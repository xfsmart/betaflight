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

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#ifdef USE_UART

#include "build/debug.h"

#include "drivers/system.h"
#include "drivers/io.h"
#include "drivers/dma.h"
#include "platform/dma.h"
#include "drivers/nvic.h"
#include "platform/rcc.h"
#include "platform/serial_uart.h"

#include "drivers/serial.h"
#include "drivers/serial_uart.h"
#include "drivers/serial_uart_impl.h"

/*
 * GPIO AF mapping:
 *   AF7 = USART1/2/3, UART7
 *   AF8 = UART4/5, LPUART, USART6
 */

#define PIN_AF(tag, af) { DEFIO_TAG_E(tag), af }

const uartHardware_t uartHardware[UARTDEV_COUNT] = {
#ifdef USE_UART1
    {
        .identifier = SERIAL_PORT_USART1,
        .reg = (usartResource_t *)USART1,
#ifdef USE_DMA
        .rxDMAResource = (dmaResource_t *)DMA2_Channel5,
        .txDMAResource = (dmaResource_t *)DMA2_Channel7,
#endif
        .rxPins = { PIN_AF(PA10, GPIO_AF_7), PIN_AF(PB7, GPIO_AF_7) },
        .txPins = { PIN_AF(PA9, GPIO_AF_7), PIN_AF(PB6, GPIO_AF_7) },
        .rcc = RCC_APB2(USART1),
        .irqn = USART1_IRQn,
        .txPriority = NVIC_PRIO_SERIALUART1_TXDMA,
        .rxPriority = NVIC_PRIO_SERIALUART1,
        .txBuffer = uart1TxBuffer,
        .rxBuffer = uart1RxBuffer,
        .txBufferSize = sizeof(uart1TxBuffer),
        .rxBufferSize = sizeof(uart1RxBuffer),
    },
#endif

#ifdef USE_UART2
    {
        .identifier = SERIAL_PORT_USART2,
        .reg = (usartResource_t *)USART2,
#ifdef USE_DMA
        .rxDMAResource = (dmaResource_t *)DMA1_Channel5,
        .txDMAResource = (dmaResource_t *)DMA1_Channel6,
#endif
        .rxPins = { PIN_AF(PA3, GPIO_AF_7), PIN_AF(PD6, GPIO_AF_7) },
        .txPins = { PIN_AF(PA2, GPIO_AF_7), PIN_AF(PD5, GPIO_AF_7) },
        .rcc = RCC_APB1(UART2),
        .irqn = USART2_IRQn,
        .txPriority = NVIC_PRIO_SERIALUART2_TXDMA,
        .rxPriority = NVIC_PRIO_SERIALUART2,
        .txBuffer = uart2TxBuffer,
        .rxBuffer = uart2RxBuffer,
        .txBufferSize = sizeof(uart2TxBuffer),
        .rxBufferSize = sizeof(uart2RxBuffer),
    },
#endif

#ifdef USE_UART3
    {
        .identifier = SERIAL_PORT_USART3,
        .reg = (usartResource_t *)USART3,
#ifdef USE_DMA
        // DMA1 PeriphSel=4: CH1=USART3_RX, CH3=USART3_TX
        .rxDMAResource = (dmaResource_t *)DMA1_Channel1,
        .txDMAResource = (dmaResource_t *)DMA1_Channel3,
#endif
        .rxPins = { PIN_AF(PB11, GPIO_AF_7), PIN_AF(PC11, GPIO_AF_7), PIN_AF(PD9, GPIO_AF_7) },
        .txPins = { PIN_AF(PB10, GPIO_AF_7), PIN_AF(PC10, GPIO_AF_7), PIN_AF(PD8, GPIO_AF_7) },
        .rcc = RCC_APB1(UART3),
        .irqn = USART3_IRQn,
        .txPriority = NVIC_PRIO_SERIALUART3_TXDMA,
        .rxPriority = NVIC_PRIO_SERIALUART3,
        .txBuffer = uart3TxBuffer,
        .rxBuffer = uart3RxBuffer,
        .txBufferSize = sizeof(uart3TxBuffer),
        .rxBufferSize = sizeof(uart3RxBuffer),
    },
#endif

#ifdef USE_UART4
    {
        .identifier = SERIAL_PORT_UART4,
        .reg = (usartResource_t *)UART4,
#ifdef USE_DMA
        .rxDMAResource = (dmaResource_t *)DMA1_Channel2,
        .txDMAResource = (dmaResource_t *)DMA1_Channel4,
#endif
        .rxPins = { PIN_AF(PA1, GPIO_AF_8), PIN_AF(PC11, GPIO_AF_8) },
        .txPins = { PIN_AF(PA0, GPIO_AF_8), PIN_AF(PC10, GPIO_AF_8) },
        .rcc = RCC_APB1(UART4),
        .irqn = UART4_IRQn,
        .txPriority = NVIC_PRIO_SERIALUART4_TXDMA,
        .rxPriority = NVIC_PRIO_SERIALUART4,
        .txBuffer = uart4TxBuffer,
        .rxBuffer = uart4RxBuffer,
        .txBufferSize = sizeof(uart4TxBuffer),
        .rxBufferSize = sizeof(uart4RxBuffer),
    },
#endif

#ifdef USE_UART5
    {
        .identifier = SERIAL_PORT_UART5,
        .reg = (usartResource_t *)UART5,
#ifdef USE_DMA
        // DMA1 PeriphSel=4: CH0=UART5_RX, CH7=UART5_TX
        .rxDMAResource = (dmaResource_t *)DMA1_Channel0,
        .txDMAResource = (dmaResource_t *)DMA1_Channel7,
#endif
        .rxPins = { PIN_AF(PD2, GPIO_AF_8) },
        .txPins = { PIN_AF(PC12, GPIO_AF_8) },
        .rcc = RCC_APB1(UART5),
        .irqn = UART5_IRQn,
        .txPriority = NVIC_PRIO_SERIALUART5_TXDMA,
        .rxPriority = NVIC_PRIO_SERIALUART5,
        .txBuffer = uart5TxBuffer,
        .rxBuffer = uart5RxBuffer,
        .txBufferSize = sizeof(uart5TxBuffer),
        .rxBufferSize = sizeof(uart5RxBuffer),
    },
#endif

#ifdef USE_UART6
    {
        .identifier = SERIAL_PORT_USART6,
        .reg = (usartResource_t *)USART6,
#ifdef USE_DMA
        // DMA2 PeriphSel=5: CH2=USART6_RX, CH6=USART6_TX
        .rxDMAResource = (dmaResource_t *)DMA2_Channel2,
        .txDMAResource = (dmaResource_t *)DMA2_Channel6,
#endif
        .rxPins = { PIN_AF(PC7, GPIO_AF_8) },
        .txPins = { PIN_AF(PC6, GPIO_AF_8) },
        .rcc = RCC_APB2(USART6),
        .irqn = USART6_IRQn,
        .txPriority = NVIC_PRIO_SERIALUART6_TXDMA,
        .rxPriority = NVIC_PRIO_SERIALUART6,
        .txBuffer = uart6TxBuffer,
        .rxBuffer = uart6RxBuffer,
        .txBufferSize = sizeof(uart6TxBuffer),
        .rxBufferSize = sizeof(uart6RxBuffer),
    },
#endif
};

bool checkUsartTxOutput(uartPort_t *s)
{
    uartDevice_t *uart = container_of(s, uartDevice_t, port);
    IO_t txIO = IOGetByTag(uart->tx.pin);

    if ((uart->txPinState == TX_PIN_MONITOR) && txIO) {
        if (IORead(txIO)) {
            uart->txPinState = TX_PIN_ACTIVE;
            IOConfigGPIOAF(txIO, IOCFG_AF_PP, uart->tx.af);
            ft32UartTXEN_Cmd((USART_TypeDef *)s->USARTx, ENABLE);
            return true;
        } else {
            return false;
        }
    }

    return true;
}

void uartTxMonitor(uartPort_t *s)
{
    uartDevice_t *uart = container_of(s, uartDevice_t, port);

    if (uart->txPinState == TX_PIN_ACTIVE) {
        IO_t txIO = IOGetByTag(uart->tx.pin);

        // Disable the transmitter through the TXDIS command so the TXD line is
        // released for monitoring; the TXEN command with DISABLE has no effect.
        ft32UartTXDIS_Cmd((USART_TypeDef *)s->USARTx, ENABLE);

        // Mask the TXEMPTY interrupt through IDR. TXEMPTY is a level flag that
        // stays set while the transmitter is idle, so it must be masked until
        // the next transmit to avoid a pending interrupt storm.
        ft32UartITDisableConfig((USART_TypeDef *)s->USARTx, USART_DIS_TXEMPTY, ENABLE);

        uart->txPinState = TX_PIN_MONITOR;
        IOConfigGPIO(txIO, IOCFG_IPU);
    }
}

static void handleUsartTxDma(uartPort_t *s)
{
    uartDevice_t *uart = container_of(s, uartDevice_t, port);

    uartTryStartTxDMA(s);

    if (s->txDMAEmpty && (uart->txPinState != TX_PIN_IGNORE)) {
        uartTxMonitor(s);
    }
}

void uartDmaIrqHandler(dmaChannelDescriptor_t *descriptor)
{
    uartPort_t *s = &(((uartDevice_t *)(descriptor->userParam))->port);

    if (DMA_GET_FLAG_STATUS(descriptor, DMA_IT_TFR)) {
        DMA_CLEAR_FLAG(descriptor, DMA_IT_TFR);
        handleUsartTxDma(s);
    }

    if (DMA_GET_FLAG_STATUS(descriptor, DMA_IT_ERR)) {
        DMA_CLEAR_FLAG(descriptor, DMA_IT_ERR);
        // Recover the TX channel to a restartable state without disturbing
        // other channels: stop only this channel, clear every transfer and
        // error flag, and reset the block count so the next write retries the
        // remaining queued data from the current ring tail.
        DMA_Channel_Cmd((DMA_Channel_TypeDef *)s->txDMAResource, DISABLE);
        DMA_CLEAR_FLAG(descriptor, DMA_IT_TFR | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR);
        DMA_SetCurrDataCounter((DMA_Channel_TypeDef *)s->txDMAResource, 0);
    }
}

void uartIrqHandler(uartPort_t *s)
{
    USART_TypeDef *USARTx = (USART_TypeDef *)s->USARTx;

    // Gate every source on the CSR pending flag together with the IMR enable
    // bit so an enabled-but-idle source cannot inject phantom work.
    if (!s->rxDMAResource &&
        (ft32UartGetFlagStatus(USARTx, USART_FLAG_RXRDY) != RESET) &&
        (ft32UartGetITStatus(USARTx, USART_IT_RXRDY) != RESET)) {
        if (s->port.rxCallback) {
            s->port.rxCallback(ft32UartReceive(USARTx), s->port.rxCallbackData);
        } else {
            s->port.rxBuffer[s->port.rxBufferHead] = ft32UartReceive(USARTx);
            s->port.rxBufferHead = (s->port.rxBufferHead + 1) % s->port.rxBufferSize;
        }
    }

    // Transmission completion: TXEMPTY is set once the shift register has
    // drained. Check both the pending flag and the enable bit.
    if ((ft32UartGetFlagStatus(USARTx, USART_FLAG_TXEMPTY) != RESET) &&
        (ft32UartGetITStatus(USARTx, USART_IT_TXEMPTY) != RESET)) {
        uartTxMonitor(s);
    }

    if (!s->txDMAResource &&
        (ft32UartGetFlagStatus(USARTx, USART_FLAG_TXRDY) != RESET) &&
        (ft32UartGetITStatus(USARTx, USART_IT_TXRDY) != RESET)) {
        if (s->port.txBufferTail != s->port.txBufferHead) {
            ft32UartTransmit(USARTx, s->port.txBuffer[s->port.txBufferTail]);
            s->port.txBufferTail = (s->port.txBufferTail + 1) % s->port.txBufferSize;
            // Writing THR clears TXEMPTY. Only SERIAL_CHECK_TX ports use the
            // TXEMPTY completion interrupt to release the TX line, and only
            // then is it armed, so an idle level flag cannot storm. DMA ports
            // never reach this branch and signal completion via the DMA
            // transfer interrupt instead.
            if (s->port.options & SERIAL_CHECK_TX) {
                ft32UartITConfig(USARTx, USART_IT_TXEMPTY, ENABLE);
            }
        } else {
            // No more data: mask TXRDY through IDR; writing IER cannot clear it.
            ft32UartITDisableConfig(USARTx, USART_DIS_TXRDY, ENABLE);
        }
    }

    if (ft32UartGetFlagStatus(USARTx, USART_FLAG_OVER) != RESET) {
        ft32UartClearFlag(USARTx, USART_CLEAR_OVER);
    }

    // Receiver timeout marks a packet boundary after sustained line idle.
    if ((ft32UartGetFlagStatus(USARTx, USART_FLAG_TIMEOUT) != RESET) &&
        (ft32UartGetITStatus(USARTx, USART_IT_TIMEOUT) != RESET)) {
        if (s->port.idleCallback) {
            s->port.idleCallback();
        }
        // Clearing via STTTO also restarts the timeout counter for the next packet.
        ft32UartClearFlag(USARTx, USART_CLEAR_TIMEOUT);
    }
}

#endif // USE_UART
