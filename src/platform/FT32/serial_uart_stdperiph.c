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

#ifndef FT32_UART_DMA_RECOVERY_UNIT_TEST
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

static uint32_t ft32UartDmaControlMasks[UARTDEV_COUNT];

static int ft32UartIndex(USART_TypeDef *USARTx)
{
    for (int index = 0; index < UARTDEV_COUNT; index++) {
        if ((USART_TypeDef *)uartHardware[index].reg == USARTx) {
            return index;
        }
    }

    return -1;
}

uint32_t ft32UartDmaControlMask(USART_TypeDef *USARTx)
{
    const int index = ft32UartIndex(USARTx);

    if (index < 0) {
        return 0U;
    }

    return ft32UartDmaControlMasks[index] & (USART_CR_DMAT_EN | USART_CR_DMAR_EN);
}

void ft32UartSetDmaControlMask(USART_TypeDef *USARTx, uint32_t mask)
{
    const int index = ft32UartIndex(USARTx);

    if (index < 0) {
        return;
    }

    ft32UartDmaControlMasks[index] = mask & (USART_CR_DMAT_EN | USART_CR_DMAR_EN);
}

static uint32_t ft32UartBaudDivider(uint32_t apbclock, uint32_t baudRate)
{
    if (baudRate == 0U) {
        return 0U;
    }

    const uint64_t divisor = (uint64_t)baudRate * 16U;
    const uint64_t scaledDivider = (((uint64_t)apbclock * 8U) + (divisor / 2U)) / divisor;
    const uint32_t clockDivider = (uint32_t)(scaledDivider / 8U);
    const uint32_t fracDivider = (uint32_t)(scaledDivider % 8U);

    return ((clockDivider << USART_BRGR_CD_Pos) & USART_BRGR_CD) |
           ((fracDivider << USART_BRGR_FP_Pos) & USART_BRGR_FP);
}

static void ft32UartInitAsyncDisabled(USART_TypeDef *USARTx, uint32_t baudRate, uint32_t wordLength, uint32_t stopBits, uint32_t parity)
{
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);

    const uint32_t apbclock = (USARTx == USART1 || USARTx == USART6)
                                  ? clocks.P2CLK_Frequency
                                  : clocks.PCLK_Frequency;

    ft32UartSetDmaControlMask(USARTx, 0U);
    USARTx->IDR = 0xffffffffU;
    USARTx->CR = USART_CR_TXDIS | USART_CR_RXDIS;
    USARTx->MR = wordLength |
                 stopBits |
                 parity |
                 USART_CLOCK_OUTPUT_DISABLE |
                 USART_CLOCK_SELECT_MCK |
                 USART_MODE_OPERATION_NORMAL |
                 USART_SYNC_MODE_ASYNC |
                 USART_BIT_ORDER_LSBF |
                 USART_CHANNEL_MODE_NORMAL |
                 USART_OVERSAMPLING_16 |
                 USART_INVDATA_DISABLE;
    USARTx->BRGR = ft32UartBaudDivider(apbclock, baudRate);
}

static void ft32UartResetAndEnableTx(USART_TypeDef *USARTx)
{
    USARTx->CR = USART_CR_RSTTX;
    USARTx->CR = USART_CR_TXEN;
}

static void ft32UartRestoreTxStateAfterReset(uartPort_t *uartPort);
static void ft32UartPauseCheckedTxForMode(uartPort_t *uartPort);

static void uartReconfigureInternal(uartPort_t *uartPort)
{
    USART_TypeDef *USARTx = (USART_TypeDef *)uartPort->USARTx;

#ifdef USE_DMA
    if (uartPort->txDMAResource) {
        uartPort->txDMARecoveryPending = true;
        __DMB();
        ft32UartDMATxEnable_Cmd(USARTx, DISABLE);
        __DMB();
        ft32DmaRequestDisable((DMA_ARCH_TYPE *)uartPort->txDMAResource);
        __DMB();
        ft32UartPauseCheckedTxForMode(uartPort);
    }
#endif

    // 8-bit character length applies to every frame format on this USART;
    // parity is selected by the PAR field and is independent of CHRL.
    const uint32_t wordLength = USART_CHAR_LENGTH_8BIT;
    const uint32_t stopBits = (uartPort->port.options & SERIAL_STOPBITS_2)
                                  ? USART_STOPBITS_2
                                  : USART_STOPBITS_1;
    const uint32_t parity = (uartPort->port.options & SERIAL_PARITY_EVEN)
                                ? USART_PARITY_EVEN
                                : USART_PARITY_NONE;

    // UART4/5 are a separate peripheral family with their own init type and
    // APB1 baud divider; USART1/2/3/6 use the USART family. Both families share
    // the same MR field layout, so the configuration constants are common.
    // Keep TX/RX disabled during format and baud setup; directions are enabled
    // after MR/BRGR and DMA/IRQ setup below.
    if (USARTx == UART4 || USARTx == UART5) {
        // Ensure the APB clock is present while applying the peripheral reset.
        UART_Cmd(USARTx, ENABLE);
        ft32UartDeInit(USARTx);
    } else {
        // Ensure the APB clock is present while applying the peripheral reset.
        USART_Cmd(USARTx, ENABLE);
        ft32UartDeInit(USARTx);
    }
    ft32UartInitAsyncDisabled(USARTx, uartPort->port.baudRate, wordLength, stopBits, parity);

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

    ft32UartRestoreTxStateAfterReset(uartPort);

    // Receive DMA or IRQ
    if (uartPort->port.mode & MODE_RX) {
        ft32UartRXEN_Cmd(USARTx, ENABLE);
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

            xDMA_DeInit(uartPort->rxDMAResource);
            xDMA_Init(uartPort->rxDMAResource, &ft32_dma_init);
            xDMA_Cmd(uartPort->rxDMAResource, ENABLE);
            ft32UartDMARxEnable_Cmd(USARTx, ENABLE);

            uartPort->rxDMAPos = xDMA_GetCurrDataCounter(uartPort->rxDMAResource);
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

            xDMA_Cmd(uartPort->txDMAResource, DISABLE);
            if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)uartPort->txDMAResource)) {
                return;
            }
            xDMA_DeInit(uartPort->txDMAResource);
            xDMA_Init(uartPort->txDMAResource, &ft32_dma_init);
            if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)uartPort->txDMAResource)) {
                return;
            }
            xDMA_ITConfig(uartPort->txDMAResource, DMA_IT_TFR | DMA_IT_ERR, ENABLE);
            uartTryRecoverTxDMA(uartPort);
        } else
#endif
        {
            // TXRDY is armed when data is queued; arming it here lets an empty
            // TX buffer consume the initial ready condition before the first
            // write reaches the port.
        }
        // TXEMPTY is a level flag set whenever the transmitter is idle, so its
        // interrupt is not enabled here; enabling it at configuration time would
        // assert continuously. The SERIAL_CHECK_TX completion path arms it
        // lazily after a byte is queued (see uartIrqHandler TXRDY branch); DMA
        // ports signal completion through the DMA transfer interrupt instead.
        if (!(uartPort->port.options & SERIAL_CHECK_TX)) {
            ft32UartTXEN_Cmd(USARTx, ENABLE);
        }
    }

#ifdef USE_DMA
    if (uartPort->txDMAResource && !(uartPort->port.mode & MODE_TX)) {
        uartTryRecoverTxDMA(uartPort);
    }
#endif
}

void uartReconfigure(uartPort_t *uartPort)
{
#ifdef USE_DMA
    if (uartPort->txDMAResource) {
        ATOMIC_BLOCK(NVIC_PRIO_SERIALUART_TXDMA) {
            uartReconfigureInternal(uartPort);
        }
        return;
    }
#endif

    uartReconfigureInternal(uartPort);
}

#endif // USE_UART
#endif // FT32_UART_DMA_RECOVERY_UNIT_TEST

#ifdef USE_UART

static void ft32UartPauseCheckedTxForMode(uartPort_t *uartPort)
{
    if (!(uartPort->port.mode & MODE_TX) &&
        (uartPort->port.options & SERIAL_CHECK_TX)) {
        // Discard the software-active state as soon as the producer is off.
        // A later MODE_TX restore must sample the shared line again, even when
        // a sticky DMA channel delays completion of the stop transaction.
        uartTxMonitor(uartPort);
    }
}

static void ft32UartRestoreTxStateAfterReset(uartPort_t *uartPort)
{
    if (!(uartPort->port.mode & MODE_TX)) {
        return;
    }

    const uartDevice_t *uartDevice = container_of(uartPort, uartDevice_t, port);

    // Reconfiguration resets the hardware transmitter. Keep an already active
    // checked-TX port synchronized with its software pin state; monitored ports
    // stay disabled until the line is sampled high.
    if (!(uartPort->port.options & SERIAL_CHECK_TX) || uartDevice->txPinState == TX_PIN_ACTIVE) {
        ft32UartResetAndEnableTx((USART_TypeDef *)uartPort->USARTx);
    }
}

#ifdef USE_DMA
static void uartTryStartTxDMAInternal(uartPort_t *s, bool recoveryOwner)
{
    ATOMIC_BLOCK(NVIC_PRIO_SERIALUART_TXDMA) {
        const bool recoveryPending = s->txDMARecoveryPending;

        if (recoveryPending != recoveryOwner) {
            return;
        }

        if (!recoveryOwner && !s->txDMAEmpty) {
            return;
        }

        if (!recoveryPending && ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)s->txDMAResource)) {
            // The active block owns the channel until its completion or error
            // handler schedules the queued suffix.
            return;
        }

        ft32UartDMATxEnable_Cmd((USART_TypeDef *)s->USARTx, DISABLE);
        __DMB();

        if (!(s->port.mode & MODE_TX)) {
            if (!ft32DmaTrySetCurrDataCounter((DMA_ARCH_TYPE *)s->txDMAResource, 0U)) {
                s->txDMARecoveryPending = true;
                __DMB();
                return;
            }

            s->txDMAEmpty = s->port.txBufferHead == s->port.txBufferTail;
            __DMB();
            uartTxMonitor(s);
            s->txDMARecoveryPending = false;
            __DMB();
            return;
        }

        if (s->port.txBufferHead == s->port.txBufferTail) {
            if (!ft32DmaTrySetCurrDataCounter((DMA_ARCH_TYPE *)s->txDMAResource, 0U)) {
                s->txDMARecoveryPending = true;
                __DMB();
                return;
            }

            // No more data to transmit. Publish the stopped state before
            // releasing recovery ownership.
            s->txDMAEmpty = true;
            __DMB();
            uartTxMonitor(s);
            s->txDMARecoveryPending = false;
            __DMB();
            return;
        }

        const uint16_t currentTail = s->port.txBufferTail;
        uint16_t nextTail;
        unsigned chunk;
        if (s->port.txBufferHead > currentTail) {
            chunk = s->port.txBufferHead - currentTail;
            nextTail = s->port.txBufferHead;
        } else {
            chunk = s->port.txBufferSize - currentTail;
            nextTail = 0U;
        }

        if (!ft32DmaTrySetCurrDataCounter((DMA_ARCH_TYPE *)s->txDMAResource, chunk)) {
            s->txDMARecoveryPending = true;
            __DMB();
            return;
        }

        if (s->port.options & SERIAL_CHECK_TX) {
            const uartDevice_t *uartDevice = container_of(s, uartDevice_t, port);

            // A monitored line must be sampled by thread mode before DMA owns
            // the suffix. ISR recovery leaves one bounded retry to TASK_MAIN
            // or to a polling caller. TX_PIN_IGNORE is not a configured TX
            // output and must never arm DMA with the transmitter disabled.
            if (uartDevice->txPinState != TX_PIN_ACTIVE &&
                (__get_IPSR() != 0U || uartDevice->txPinState != TX_PIN_MONITOR ||
                 !s->checkUsartTxOutput || !s->checkUsartTxOutput(s))) {
                s->txDMAEmpty = false;
                __DMB();
                s->txDMARecoveryPending = true;
                __DMB();
                return;
            }
        }

        DMA_SetSrcAddress((DMA_Channel_TypeDef *)s->txDMAResource,
                          (uint32_t)&s->port.txBuffer[currentTail]);
        xDMA_Cmd(s->txDMAResource, ENABLE);
        __DMB();
        if (!ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)s->txDMAResource)) {
            s->txDMAEmpty = false;
            __DMB();
            s->txDMARecoveryPending = true;
            __DMB();
            return;
        }

        s->port.txBufferTail = nextTail;
        s->txDMAEmpty = false;
        __DMB();
        s->txDMARecoveryPending = false;
        __DMB();
        ft32UartDMATxEnable_Cmd((USART_TypeDef *)s->USARTx, ENABLE);
    }
}

void uartTryStartTxDMA(uartPort_t *s)
{
    // Ordinary writers cannot consume a pending terminal recovery.
    uartTryStartTxDMAInternal(s, false);
}

void uartTryRecoverTxDMA(uartPort_t *s)
{
    uartTryStartTxDMAInternal(s, true);
}
#endif

#endif // USE_UART
