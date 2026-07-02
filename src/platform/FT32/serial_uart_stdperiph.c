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
#include "platform/serial_uart.h"

#include "drivers/serial.h"
#include "drivers/serial_uart.h"
#include "drivers/serial_uart_impl.h"

// Receiver timeout window in bit periods. A sustained idle period of this
// length after the last received byte marks a packet boundary, approximating
// the idle-line detection used to drive the idle callback.
#define UART_RX_TIMEOUT_BITS 40U

void uartReconfigure(uartPort_t *uartPort)
{
    USART_TypeDef *USARTx = (USART_TypeDef *)uartPort->USARTx;

    // 8-bit character length applies to every frame format on this USART;
    // parity is selected by the PAR field and is independent of CHRL.
    const uint32_t wordLength = USART_CHAR_LENGTH_8BIT;
    const uint32_t stopBits = (uartPort->port.options & SERIAL_STOPBITS_2)
                                  ? USART_STOPBITS_2
                                  : USART_STOPBITS_1;
    const uint32_t parity = (uartPort->port.options & SERIAL_PARITY_EVEN)
                                ? USART_PARITY_EVEN
                                : USART_PARITY_NONE;
    uint32_t mode = 0;
    if (uartPort->port.mode & MODE_RX) {
        mode |= USART_MODE_RX;
    }
    if (uartPort->port.mode & MODE_TX) {
        mode |= USART_MODE_TX;
    }

    // UART4/5 are a separate peripheral family with their own init type and
    // APB1 baud divider; USART1/2/3/6 use the USART family. Both families share
    // the same MR field layout, so the configuration constants are common.
    if (USARTx == UART4 || USARTx == UART5) {
        UART_Cmd(USARTx, DISABLE);
        UART_InitTypeDef uartInit;
        UART_StructInit(&uartInit);
        uartInit.UART_BaudRate = uartPort->port.baudRate;
        uartInit.UART_WordLength = wordLength;
        uartInit.UART_StopBits = stopBits;
        uartInit.UART_Parity = parity;
        uartInit.UART_Mode = mode;
        UART_Init(USARTx, &uartInit);
    } else {
        USART_Cmd(USARTx, DISABLE);
        USART_InitTypeDef usartInit;
        USART_StructInit(&usartInit);
        usartInit.USART_BaudRate = uartPort->port.baudRate;
        usartInit.USART_WordLength = wordLength;
        usartInit.USART_StopBits = stopBits;
        usartInit.USART_Parity = parity;
        usartInit.USART_Mode = mode;
        USART_Init(USARTx, &usartInit);
    }

    // Config external pin inverter (no internal pin inversion available)
    uartConfigureExternalPinInversion(uartPort);

    // Channel mode stays normal. This USART has no asynchronous single-wire
    // half-duplex selection (CHMODE=AUTOMATIC only echoes RXD onto TXD and
    // cannot transmit independently), so SERIAL_BIDIR is refused at open in
    // serialUART rather than handled here; normal mode is the StructInit
    // default already applied by the init call above.

    if (USARTx == UART4 || USARTx == UART5) {
        UART_Cmd(USARTx, ENABLE);
    } else {
        USART_Cmd(USARTx, ENABLE);
    }

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
            ft32DmaSetSrcRequest(&ft32_dma_init, uartPort->rxDMAResource, uartPort->rxDMAChannel);

            DMA_DeInit((DMA_Channel_TypeDef *)uartPort->rxDMAResource);
            xDMA_Init(uartPort->rxDMAResource, &ft32_dma_init);
            DMA_Cmd((DMA_Channel_TypeDef *)uartPort->rxDMAResource, ENABLE);
            ft32UartDMARxEnable_Cmd(USARTx, ENABLE);

            uartPort->rxDMAPos = DMA_GetCurrDataCounter(
                (DMA_Channel_TypeDef *)uartPort->rxDMAResource);
        } else
#endif
        {
            // RXRDY is cleared by reading the receive holding register
            (void)ft32UartReceive(USARTx);
            // Program and arm the receiver timeout so the idle callback fires
            // at a packet boundary after sustained line idle.
            ft32UartReceiver_TimeOut_Cfg(USARTx, UART_RX_TIMEOUT_BITS);
            ft32UartSTTTO_After_Timeout_Cmd(USARTx, ENABLE);
            ft32UartRETTO_After_Timeout_Cmd(USARTx, ENABLE);
            ft32UartITConfig(USARTx, USART_IT_RXRDY, ENABLE);
            ft32UartITConfig(USARTx, USART_IT_TIMEOUT, ENABLE);
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
            ft32DmaSetDstRequest(&ft32_dma_init, uartPort->txDMAResource, uartPort->txDMAChannel);

            DMA_DeInit((DMA_Channel_TypeDef *)uartPort->txDMAResource);
            xDMA_Init(uartPort->txDMAResource, &ft32_dma_init);
            DMA_ITConfig((DMA_Channel_TypeDef *)uartPort->txDMAResource, DMA_IT_TFR | DMA_IT_ERR, ENABLE);
            ft32UartDMATxEnable_Cmd(USARTx, ENABLE);
            DMA_SetCurrDataCounter((DMA_Channel_TypeDef *)uartPort->txDMAResource, 0);
        } else
#endif
        {
            ft32UartITConfig(USARTx, USART_IT_TXRDY, ENABLE);
        }
        // TXEMPTY is a level flag set whenever the transmitter is idle, so its
        // interrupt is not enabled here; enabling it at configuration time would
        // assert continuously. The SERIAL_CHECK_TX completion path arms it
        // lazily after a byte is queued (see uartIrqHandler TXRDY branch); DMA
        // ports signal completion through the DMA transfer interrupt instead.
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

        // Repoint the source to the current ring tail, which is the start of
        // the next chunk, before advancing the tail. The channel is stopped at
        // this point (counter is zero) so the source address is writable.
        DMA_SetSrcAddress((DMA_Channel_TypeDef *)s->txDMAResource,
                          (uint32_t)&s->port.txBuffer[s->port.txBufferTail]);

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
