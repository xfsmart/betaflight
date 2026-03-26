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

#pragma once

#include "platform.h"
#include "common/utils.h"
#include "platform/dma.h"

// allow conditional definition of DMA related members
#if defined(USE_TIMER_DMA)
# define DEF_TIM_DMA_COND(...) __VA_ARGS__
#else
# define DEF_TIM_DMA_COND(...)
#endif

#if defined(USE_TIMER_MGMT)
#define TIMER_GET_IO_TAG(pin) DEFIO_TAG_E(pin)
#else
#define TIMER_GET_IO_TAG(pin) DEFIO_TAG(pin)
#endif

// map to base channel (strip N from channel)
#define DEF_TIM_TCH2BTCH(timch) CONCAT(B, timch)
#define BTCH_TIM1_CH1N BTCH_TIM1_CH1
#define BTCH_TIM1_CH2N BTCH_TIM1_CH2
#define BTCH_TIM1_CH3N BTCH_TIM1_CH3

#define BTCH_TIM8_CH1N BTCH_TIM8_CH1
#define BTCH_TIM8_CH2N BTCH_TIM8_CH2
#define BTCH_TIM8_CH3N BTCH_TIM8_CH3

#define BTCH_TIM13_CH1N BTCH_TIM13_CH1
#define BTCH_TIM14_CH1N BTCH_TIM14_CH1

// channel table D(chan_n, n_type)
#define DEF_TIM_CH_GET(ch) CONCAT2(DEF_TIM_CH__, ch)
#define DEF_TIM_CH__CH_CH1  D(1, 0)
#define DEF_TIM_CH__CH_CH2  D(2, 0)
#define DEF_TIM_CH__CH_CH3  D(3, 0)
#define DEF_TIM_CH__CH_CH4  D(4, 0)
#define DEF_TIM_CH__CH_CH1N D(1, 1)
#define DEF_TIM_CH__CH_CH2N D(2, 1)
#define DEF_TIM_CH__CH_CH3N D(3, 1)

// timer table D(tim_n)
#define DEF_TIM_TIM_GET(tim) CONCAT2(DEF_TIM_TIM__, tim)
#define DEF_TIM_TIM__TIM_TIM1  D(1)
#define DEF_TIM_TIM__TIM_TIM2  D(2)
#define DEF_TIM_TIM__TIM_TIM3  D(3)
#define DEF_TIM_TIM__TIM_TIM4  D(4)
#define DEF_TIM_TIM__TIM_TIM5  D(5)
#define DEF_TIM_TIM__TIM_TIM6  D(6)
#define DEF_TIM_TIM__TIM_TIM7  D(7)
#define DEF_TIM_TIM__TIM_TIM8  D(8)
#define DEF_TIM_TIM__TIM_TIM9  D(9)
#define DEF_TIM_TIM__TIM_TIM10 D(10)
#define DEF_TIM_TIM__TIM_TIM11 D(11)
#define DEF_TIM_TIM__TIM_TIM12 D(12)
#define DEF_TIM_TIM__TIM_TIM13 D(13)
#define DEF_TIM_TIM__TIM_TIM14 D(14)

// define output type (N-channel)
#define DEF_TIM_OUTPUT(ch)                    CONCAT(DEF_TIM_OUTPUT__, DEF_TIM_CH_GET(ch))
#define DEF_TIM_OUTPUT__D(chan_n, n_channel)   PP_IIF(n_channel, TIMER_OUTPUT_N_CHANNEL, TIMER_OUTPUT_NONE)

// GPIO AF definitions for timer peripheral mapping
// FT32F4 AF assignments identical to STM32F4
// Source: ft32f4xx_gpio.h
#define GPIO_AF_TIM1   GPIO_AF_1    // AF1: TIM1/TIM2
#define GPIO_AF_TIM2   GPIO_AF_1    // AF1: TIM1/TIM2
#define GPIO_AF_TIM3   GPIO_AF_2    // AF2: TIM3/TIM4/TIM5
#define GPIO_AF_TIM4   GPIO_AF_2    // AF2: TIM3/TIM4/TIM5
#define GPIO_AF_TIM5   GPIO_AF_2    // AF2: TIM3/TIM4/TIM5
#define GPIO_AF_TIM8   GPIO_AF_3    // AF3: TIM8/TIM9/TIM10/TIM11
#define GPIO_AF_TIM9   GPIO_AF_3    // AF3: TIM8/TIM9/TIM10/TIM11
#define GPIO_AF_TIM10  GPIO_AF_3    // AF3: TIM8/TIM9/TIM10/TIM11
#define GPIO_AF_TIM11  GPIO_AF_3    // AF3: TIM8/TIM9/TIM10/TIM11
#define GPIO_AF_TIM12  GPIO_AF_9    // AF9: TIM12/TIM13/TIM14
#define GPIO_AF_TIM13  GPIO_AF_9    // AF9: TIM12/TIM13/TIM14
#define GPIO_AF_TIM14  GPIO_AF_9    // AF9: TIM12/TIM13/TIM14

#define DEF_TIM(tim, chan, pin, out, dmaopt) {                  \
    tim,                                                        \
    TIMER_GET_IO_TAG(pin),                                      \
    DEF_TIM_CHANNEL(CH_ ## chan),                               \
    (DEF_TIM_OUTPUT(CH_ ## chan) | out),                        \
    DEF_TIM_AF(TIM_ ## tim)                                     \
    DEF_TIM_DMA_COND(/* add comma */ ,                          \
        NULL,                                                   \
        0                                                       \
    )                                                           \
    DEF_TIM_DMA_COND(/* add comma */ ,                          \
        NULL,                                                   \
        0,                                                      \
        0                                                       \
    )                                                           \
}                                                               \
/**/

#define DEF_TIM_CHANNEL(ch)                   CONCAT(DEF_TIM_CHANNEL__, DEF_TIM_CH_GET(ch))
#define DEF_TIM_CHANNEL__D(chan_n, n_channel)  TIM_Channel_ ## chan_n

#define DEF_TIM_AF(tim)                       CONCAT(DEF_TIM_AF__, DEF_TIM_TIM_GET(tim))
#define DEF_TIM_AF__D(tim_n)                  GPIO_AF_TIM ## tim_n

#define FULL_TIMER_CHANNEL_COUNT 78
#define USED_TIMERS ( TIM_N(1) | TIM_N(2) | TIM_N(3) | TIM_N(4) | TIM_N(5) | TIM_N(6) | TIM_N(7) | TIM_N(8) | TIM_N(9) | TIM_N(10) | TIM_N(11) | TIM_N(12) | TIM_N(13) | TIM_N(14) )
#define HARDWARE_TIMER_DEFINITION_COUNT BITCOUNT(USED_TIMERS)
