/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any
 * version.
 *
 * Betaflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "platform.h"

// Include the FT32 standard library USART and UART driver headers.
#include "ft32f4xx_usart.h"
#include "ft32f4xx_uart.h"

// FT32 exposes two SDK families over the same USART_TypeDef register block:
// USART_* for USART1/2/3/6 and UART_* for UART4/5. Both families take the
// same base pointer type and access identical register offsets, so the only
// behavioural difference is the per-instance assert_param gate
// (IS_USART_ALL_PERIPH versus IS_UART_ALL_PERIPH). USE_FULL_ASSERT is off in
// the shipped configuration, so a wrong-family call happens to work today,
// but it violates the SDK contract and would assert-fail in a debug build.
// Route every runtime USART/UART call through these dispatch wrappers so
// UART4/5 stay inside the UART_* family contract and USART1/2/3/6 stay inside
// the USART_* family. Init/enable dispatch (UART_Init/UART_Cmd versus
// USART_Init/USART_Cmd) also selects the correct APB baud clock and remains
// handled directly in uartReconfigure.

uint32_t ft32UartDmaControlMask(USART_TypeDef *USARTx);
void ft32UartSetDmaControlMask(USART_TypeDef *USARTx, uint32_t mask);

static inline void ft32UartWriteControl(USART_TypeDef *USARTx, uint32_t command)
{
    USARTx->CR = command | ft32UartDmaControlMask(USARTx);
}

static inline void ft32UartDeInit(USART_TypeDef *USARTx)
{
    if (USARTx == UART4 || USARTx == UART5) {
        UART_DeInit(USARTx);
    } else {
        USART_DeInit(USARTx);
    }
}

static inline FlagStatus ft32UartGetFlagStatus(USART_TypeDef *USARTx, uint32_t flag)
{
    if (USARTx == UART4 || USARTx == UART5) {
        return UART_GetFlagStatus(USARTx, flag);
    }
    return USART_GetFlagStatus(USARTx, flag);
}

static inline ITStatus ft32UartGetITStatus(USART_TypeDef *USARTx, uint32_t it)
{
    if (USARTx == UART4 || USARTx == UART5) {
        return UART_GetITStatus(USARTx, it);
    }
    return USART_GetITStatus(USARTx, it);
}

static inline uint16_t ft32UartReceive(USART_TypeDef *USARTx)
{
    if (USARTx == UART4 || USARTx == UART5) {
        return UART_Receive(USARTx);
    }
    return USART_Receive(USARTx);
}

static inline void ft32UartTransmit(USART_TypeDef *USARTx, uint16_t data)
{
    if (USARTx == UART4 || USARTx == UART5) {
        UART_Transmit(USARTx, data);
    } else {
        USART_Transmit(USARTx, data);
    }
}

static inline void ft32UartITConfig(USART_TypeDef *USARTx, uint32_t it, FunctionalState state)
{
    if (USARTx == UART4 || USARTx == UART5) {
        UART_ITConfig(USARTx, it, state);
    } else {
        USART_ITConfig(USARTx, it, state);
    }
}

static inline void ft32UartITDisableConfig(USART_TypeDef *USARTx, uint32_t it, FunctionalState state)
{
    if (USARTx == UART4 || USARTx == UART5) {
        UART_ITDisableConfig(USARTx, it, state);
    } else {
        USART_ITDisableConfig(USARTx, it, state);
    }
}

static inline void ft32UartClearFlag(USART_TypeDef *USARTx, uint32_t flag)
{
    ft32UartWriteControl(USARTx, flag);
}

static inline void ft32UartTXEN_Cmd(USART_TypeDef *USARTx, FunctionalState state)
{
    if (state != DISABLE) {
        ft32UartWriteControl(USARTx, USART_CR_TXEN);
    }
}

static inline void ft32UartRXEN_Cmd(USART_TypeDef *USARTx, FunctionalState state)
{
    if (state != DISABLE) {
        ft32UartWriteControl(USARTx, USART_CR_RXEN);
    }
}

static inline void ft32UartTXDIS_Cmd(USART_TypeDef *USARTx, FunctionalState state)
{
    if (state != DISABLE) {
        ft32UartWriteControl(USARTx, USART_CR_TXDIS);
    }
}

static inline void ft32UartDMATxEnable_Cmd(USART_TypeDef *USARTx, FunctionalState state)
{
    uint32_t mask = ft32UartDmaControlMask(USARTx);

    if (state != DISABLE) {
        mask |= USART_CR_DMAT_EN;
    } else {
        mask &= ~USART_CR_DMAT_EN;
    }

    ft32UartSetDmaControlMask(USARTx, mask);
    USARTx->CR = mask;
}

static inline void ft32UartDMARxEnable_Cmd(USART_TypeDef *USARTx, FunctionalState state)
{
    uint32_t mask = ft32UartDmaControlMask(USARTx);

    if (state != DISABLE) {
        mask |= USART_CR_DMAR_EN;
    } else {
        mask &= ~USART_CR_DMAR_EN;
    }

    ft32UartSetDmaControlMask(USARTx, mask);
    USARTx->CR = mask;
}

static inline void ft32UartReceiver_TimeOut_Cfg(USART_TypeDef *USARTx, uint32_t timeout)
{
    if (USARTx == UART4 || USARTx == UART5) {
        UART_Receiver_TimeOut_Cfg(USARTx, timeout);
    } else {
        USART_Receiver_TimeOut_Cfg(USARTx, timeout);
    }
}

static inline void ft32UartSTTTO_After_Timeout_Cmd(USART_TypeDef *USARTx, FunctionalState state)
{
    if (USARTx == UART4 || USARTx == UART5) {
        UART_STTTO_After_Timeout_Cmd(USARTx, state);
    } else {
        USART_STTTO_After_Timeout_Cmd(USARTx, state);
    }
}

static inline void ft32UartRETTO_After_Timeout_Cmd(USART_TypeDef *USARTx, FunctionalState state)
{
    if (USARTx == UART4 || USARTx == UART5) {
        UART_RETTO_After_Timeout_Cmd(USARTx, state);
    } else {
        USART_RETTO_After_Timeout_Cmd(USARTx, state);
    }
}
