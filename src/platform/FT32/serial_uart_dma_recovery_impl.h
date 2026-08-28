/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 */

#pragma once

#if defined(UNIT_TEST)
#define FT32_UART_DMA_RECOVERY_UNIT_TEST
#define USE_UART
#define USE_DMA
#endif

#if !defined(UNIT_TEST) || defined(FT32_UART_DMA_WRITE_HELPERS)
static inline bool ft32UartDmaWriteBufferReady(uartPort_t *uartPort)
{
    uartDevice_t *uart = container_of(uartPort, uartDevice_t, port);

    if (uart->txPinState != TX_PIN_MONITOR) {
        return true;
    }

    return uartPort->checkUsartTxOutput && uartPort->checkUsartTxOutput(uartPort);
}

static inline void ft32UartDmaFinishWriteBuffer(uartPort_t *uartPort)
{
    uartTryStartTxDMA(uartPort);
}
#endif
