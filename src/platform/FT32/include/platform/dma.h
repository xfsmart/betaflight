/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
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

// FT32F4 uses DesignWare DMA with Channel architecture (not Stream)
#define DMA_ARCH_TYPE DMA_Channel_TypeDef

#include "drivers/dma.h"

// FT32F4 DMA handler identifiers (DMA1/DMA2 each have 8 channels: 0-7)
#define DMA1_CH0_HANDLER    (DMA_FIRST_HANDLER + 0)
#define DMA1_CH1_HANDLER    (DMA_FIRST_HANDLER + 1)
#define DMA1_CH2_HANDLER    (DMA_FIRST_HANDLER + 2)
#define DMA1_CH3_HANDLER    (DMA_FIRST_HANDLER + 3)
#define DMA1_CH4_HANDLER    (DMA_FIRST_HANDLER + 4)
#define DMA1_CH5_HANDLER    (DMA_FIRST_HANDLER + 5)
#define DMA1_CH6_HANDLER    (DMA_FIRST_HANDLER + 6)
#define DMA1_CH7_HANDLER    (DMA_FIRST_HANDLER + 7)
#define DMA2_CH0_HANDLER    (DMA_FIRST_HANDLER + 8)
#define DMA2_CH1_HANDLER    (DMA_FIRST_HANDLER + 9)
#define DMA2_CH2_HANDLER    (DMA_FIRST_HANDLER + 10)
#define DMA2_CH3_HANDLER    (DMA_FIRST_HANDLER + 11)
#define DMA2_CH4_HANDLER    (DMA_FIRST_HANDLER + 12)
#define DMA2_CH5_HANDLER    (DMA_FIRST_HANDLER + 13)
#define DMA2_CH6_HANDLER    (DMA_FIRST_HANDLER + 14)
#define DMA2_CH7_HANDLER    (DMA_FIRST_HANDLER + 15)
#define DMA_LAST_HANDLER    DMA2_CH7_HANDLER

#define DMA_DEVICE_NO(x)    ((((x)-1) / 8) + 1)
#define DMA_DEVICE_INDEX(x) ((((x)-1) % 8))
#define DMA_OUTPUT_INDEX    0
#define DMA_OUTPUT_STRING   "DMA%d Channel %d:"

// DMA interrupt flag definitions
// FT32 DesignWare DMA uses per-channel flag registers
#define DMA_IT_TCIF    DMA_FLAG_TFR    // Transfer Complete
#define DMA_IT_HTIF    0               // Half Transfer not supported by DesignWare DMA
#define DMA_IT_TEIF    DMA_FLAG_ERR    // Transfer Error

// DMA flag management macros
// FT32 DesignWare DMA: per-channel flag registers accessed via standard library
#define DMA_CLEAR_FLAG(d, flag) DMA_ClearFlagStatus((DMA_Channel_TypeDef *)(d)->ref, (flag))
#define DMA_GET_FLAG_STATUS(d, flag) (DMA_GetFlagStatus((DMA_Channel_TypeDef *)(d)->ref, (flag)) == SET)

// IS_DMA_ENABLED: check if a DMA channel is enabled
// DesignWare DMA enables channels via the CHEN register in DMA_TypeDef, not per-channel registers.
// The standard library DMA_Channel_Cmd writes CHEN. We check the CTL.INT_EN + channel enable status
// by reading the CHEN register through the base DMA_TypeDef.
// However, the simplest portable check: the standard library does not provide a direct query.
// We use the same approach as other non-stream platforms: check a control bit.
// DesignWare CTL register bit 0 = INT_EN (always set when channel is configured).
// Actually check via DMA_TypeDef->CHEN register bit.
#define IS_DMA_ENABLED(reg) (0) // TODO: implement proper check via CHEN register

// DMA channel descriptor definition macro
#define DEFINE_DMA_CHANNEL(d, s, f) { \
    .dma = d, \
    .ref = (dmaResource_t *)d ## _Channel ## s, \
    .stream = s, \
    .irqHandlerCallback = NULL, \
    .flagsShift = f, \
    .irqN = d ## _CH ## s ## _IRQn, \
    .userParam = 0, \
    .resourceOwner.owner = 0, \
    .resourceOwner.index = 0 \
    }

// DMA IRQ handler definition macro
#define DEFINE_DMA_IRQ_HANDLER(d, s, i) void DMA ## d ## _CH ## s ## _IRQHandler(void) {\
                                                                const uint8_t index = DMA_IDENTIFIER_TO_INDEX(i); \
                                                                dmaCallbackHandlerFuncPtr handler = dmaDescriptors[index].irqHandlerCallback; \
                                                                if (handler) \
                                                                    handler(&dmaDescriptors[index]); \
                                                            }

// DMA channel number definitions (CHSEL values, equivalent to STM32 DMA_Channel_x)
// These are the "peripheral number" values written to the CHSEL register
#define DMA_Channel_0    0
#define DMA_Channel_1    1
#define DMA_Channel_2    2
#define DMA_Channel_3    3
#define DMA_Channel_4    4
#define DMA_Channel_5    5
#define DMA_Channel_6    6
#define DMA_Channel_7    7

// Standard library compatibility wrappers (xDMA_ prefix used by common Betaflight code)
#define xDMA_Init(dmaResource, initStruct)          DMA_Init((DMA_ARCH_TYPE *)(dmaResource), initStruct)
#define xDMA_DeInit(dmaResource)                    DMA_DeInit((DMA_ARCH_TYPE *)(dmaResource))
#define xDMA_Cmd(dmaResource, newState)             DMA_Channel_Cmd((DMA_ARCH_TYPE *)(dmaResource), newState)
#define xDMA_ITConfig(dmaResource, flags, newState) DMA_ITConfig((DMA_ARCH_TYPE *)(dmaResource), flags, newState)
#define xDMA_GetCurrDataCounter(dmaResource)        DMA_GetCurrDataCounter((DMA_ARCH_TYPE *)(dmaResource))
#define xDMA_SetCurrDataCounter(dmaResource, count) do { /* DesignWare: set via CTL.BLOCK_TS before enabling */ } while(0)
#define xDMA_GetFlagStatus(dmaResource, flags)      DMA_GetFlagStatus((DMA_ARCH_TYPE *)(dmaResource), flags)
#define xDMA_ClearFlag(dmaResource, flags)          DMA_ClearFlagStatus((DMA_ARCH_TYPE *)(dmaResource), flags)
