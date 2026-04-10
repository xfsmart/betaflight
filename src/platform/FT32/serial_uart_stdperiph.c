/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Initialization part of serial_uart.c
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#ifdef USE_UART

#include "build/build_config.h"
#include "build/atomic.h"

#include "common/utils.h"
#include "drivers/inverter.h"
#include "drivers/nvic.h"
#include "platform/dma.h"
#include "platform/rcc.h"

#include "drivers/serial.h"
#include "drivers/serial_uart.h"
#include "drivers/serial_uart_impl.h"

/*
 * DMA peripheral request ID mapping:
 *   PeriphSel 2 = UART4 (DMA1 only)
 *   PeriphSel 4 = USART1, USART2, USART3, UART5
 *   PeriphSel 5 = USART6 (DMA2 only)
 */
static uint8_t uartGetDmaPeriphId(void *USARTx)
{
    uintptr_t base = (uintptr_t)USARTx;

#if defined(UART4_BASE)
    if (base == UART4_BASE) return 2;  // PeriphSel 2
#endif
#if defined(USART1_BASE)
    if (base == USART1_BASE) return 4;
#endif
#if defined(USART2_BASE)
    if (base == USART2_BASE) return 4;
#endif
#if defined(USART3_BASE)
    if (base == USART3_BASE) return 4;
#endif
#if defined(UART5_BASE)
    if (base == UART5_BASE) return 4;
#endif
#if defined(USART6_BASE)
    if (base == USART6_BASE) return 5;  // PeriphSel 5
#endif

    // Fallback: assume PeriphSel 4 (most common)
    return 4;
}

void uartReconfigure(uartPort_t *uartPort)
{
    USART_TypeDef *USARTx = (USART_TypeDef *)uartPort->USARTx;
    USART_InitTypeDef USART_InitStructure;

    USART_Cmd(USARTx, DISABLE);

    USART_StructInit(&USART_InitStructure);
    USART_InitStructure.USART_BaudRate = uartPort->port.baudRate;

    // Word length: 9 bits required for parity
    if (uartPort->port.options & SERIAL_PARITY_EVEN) {
        USART_InitStructure.USART_WordLength = USART_CHAR_LENGTH9_DISABLE;
    } else {
        USART_InitStructure.USART_WordLength = USART_CHAR_LENGTH_8BIT;
    }

    USART_InitStructure.USART_StopBits = (uartPort->port.options & SERIAL_STOPBITS_2)
                                             ? USART_STOPBITS_2
                                             : USART_STOPBITS_1;
    USART_InitStructure.USART_Parity = (uartPort->port.options & SERIAL_PARITY_EVEN)
                                           ? USART_PARITY_EVEN
                                           : USART_PARITY_NONE;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = 0;

    if (uartPort->port.mode & MODE_RX)
        USART_InitStructure.USART_Mode |= USART_MODE_RX;
    if (uartPort->port.mode & MODE_TX)
        USART_InitStructure.USART_Mode |= USART_MODE_TX;

    USART_Init(USARTx, &USART_InitStructure);

    // Config external pin inverter (no internal pin inversion available)
    uartConfigureExternalPinInversion(uartPort);

    if (uartPort->port.options & SERIAL_BIDIR) {
        // FT32 uses automatic echo mode for half-duplex (single-wire) operation
        USART_ChannelMode_Cfg(USARTx, USART_CHANNEL_MODE_AUTOMATIC);
    } else {
        USART_ChannelMode_Cfg(USARTx, USART_CHANNEL_MODE_NORMAL);
    }

    USART_Cmd(USARTx, ENABLE);

    // Receive DMA or IRQ
    if (uartPort->port.mode & MODE_RX) {
#ifdef USE_DMA
        if (uartPort->rxDMAResource) {
            DMA_InitTypeDef ft32_dma_init;
            DMA_StructInit(&ft32_dma_init);

            ft32_dma_init.SrcAddress = uartPort->rxDMAPeripheralBaseAddr;
            ft32_dma_init.DstAddress = (uint32_t)uartPort->port.rxBuffer;
            ft32_dma_init.SrcAddrMode = DMA_SRC_ADDRMODE_HOLD;
            ft32_dma_init.DstAddrMode = DMA_DST_ADDRMODE_INC;
            ft32_dma_init.SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_8BITS;
            ft32_dma_init.DstTransferWidth = DMA_DST_TRANSFERWIDTH_8BITS;
            ft32_dma_init.BlockTransSize = uartPort->port.rxBufferSize;
            ft32_dma_init.Priority = DMA_CH_PRIORITY_4;
            ft32_dma_init.TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA;
            // Circular RX: reload destination address after block completes
            ft32_dma_init.ReloadDst = ENABLE;
            ft32_dma_init.ReloadSrc = DISABLE;
            // Hardware handshaking: DMA responds to USART peripheral request signal
            // PeriphSel: 4=USART1/2/3/UART4/5, 5=USART6
            ft32_dma_init.SrcHsSel = 0;
            ft32_dma_init.SrcHsIfPeriphSel = uartGetDmaPeriphId(USARTx);
            ft32_dma_init.DstHsSel = 1; // Memory side: no peripheral request
            ft32_dma_init.DstHsIfPeriphSel = 0;

            DMA_DeInit((DMA_Channel_TypeDef *)uartPort->rxDMAResource);
            DMA_Init((DMA_Channel_TypeDef *)uartPort->rxDMAResource, &ft32_dma_init);
            DMA_Cmd((DMA_Channel_TypeDef *)uartPort->rxDMAResource, ENABLE);
            USART_DMARxEnable_Cmd(USARTx, ENABLE);

            uartPort->rxDMAPos = DMA_GetCurrDataCounter(
                (DMA_Channel_TypeDef *)uartPort->rxDMAResource);
        } else
#endif
        {
            USART_ClearFlag(USARTx, USART_FLAG_RXRDY);
            USART_ITConfig(USARTx, USART_IT_RXRDY, ENABLE);
            // Enable receiver timeout interrupt for packet end detection
            USART_ITConfig(USARTx, USART_IT_TIMEOUT, ENABLE);
        }
    }

    // Transmit DMA or IRQ
    if (uartPort->port.mode & MODE_TX) {
#ifdef USE_DMA
        if (uartPort->txDMAResource) {
            DMA_InitTypeDef ft32_dma_init;
            DMA_StructInit(&ft32_dma_init);

            ft32_dma_init.SrcAddress = (uint32_t)uartPort->port.txBuffer;
            ft32_dma_init.DstAddress = uartPort->txDMAPeripheralBaseAddr;
            ft32_dma_init.SrcAddrMode = DMA_SRC_ADDRMODE_INC;
            ft32_dma_init.DstAddrMode = DMA_DST_ADDRMODE_HOLD;
            ft32_dma_init.SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_8BITS;
            ft32_dma_init.DstTransferWidth = DMA_DST_TRANSFERWIDTH_8BITS;
            ft32_dma_init.BlockTransSize = uartPort->port.txBufferSize;
            ft32_dma_init.Priority = DMA_CH_PRIORITY_4;
            ft32_dma_init.TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA;
            // Normal mode (not circular): single-shot per chunk
            ft32_dma_init.ReloadDst = DISABLE;
            ft32_dma_init.ReloadSrc = DISABLE;
            // Hardware handshaking: DMA responds to USART TX DMA request signal
            // PeriphSel: 4=USART1/2/3/UART4/5, 5=USART6
            ft32_dma_init.SrcHsSel = 1; // Memory side: no peripheral request
            ft32_dma_init.SrcHsIfPeriphSel = 0;
            ft32_dma_init.DstHsSel = 0;
            ft32_dma_init.DstHsIfPeriphSel = uartGetDmaPeriphId(USARTx);

            DMA_DeInit((DMA_Channel_TypeDef *)uartPort->txDMAResource);
            DMA_Init((DMA_Channel_TypeDef *)uartPort->txDMAResource, &ft32_dma_init);
            USART_DMATxEnable_Cmd(USARTx, ENABLE);
            DMA_SetCurrDataCounter((DMA_Channel_TypeDef *)uartPort->txDMAResource, 0);
        } else
#endif
        {
            USART_ITConfig(USARTx, USART_IT_TXRDY, ENABLE);
        }
        USART_ITConfig(USARTx, USART_IT_TXEMPTY, ENABLE);
    }
}

#ifdef USE_DMA
void uartTryStartTxDMA(uartPort_t *s)
{
    // uartTryStartTxDMA must be protected, since it is called from
    // uartWrite and handleUsartTxDma (an ISR).

    ATOMIC_BLOCK(NVIC_PRIO_SERIALUART_TXDMA) {
        // Check if DMA channel is already enabled by reading counter
        // A non-zero counter means transfer is still in progress
        if (DMA_GetCurrDataCounter((DMA_Channel_TypeDef *)s->txDMAResource)) {
            return;
        }

        if (s->port.txBufferHead == s->port.txBufferTail) {
            // No more data to transmit
            s->txDMAEmpty = true;
            return;
        }

        // Start a new transaction.
        unsigned chunk;
        if (s->port.txBufferHead > s->port.txBufferTail) {
            chunk = s->port.txBufferHead - s->port.txBufferTail;
            s->port.txBufferTail = s->port.txBufferHead;
        } else {
            chunk = s->port.txBufferSize - s->port.txBufferTail;
            s->port.txBufferTail = 0;
        }
        s->txDMAEmpty = false;
        DMA_SetCurrDataCounter((DMA_Channel_TypeDef *)s->txDMAResource, chunk);
        DMA_Cmd((DMA_Channel_TypeDef *)s->txDMAResource, ENABLE);
    }
}
#endif

#endif // USE_UART
