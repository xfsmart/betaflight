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

// FT32F4 使用基于 Channel 的 DMA 架构
// 使用 DMA_Channel_TypeDef 作为架构类型
#define DMA_ARCH_TYPE DMA_Channel_TypeDef

#include "drivers/dma.h"

// 包含 FT32 DMA 标准库头文件，用于 DMA_BaseAddressAndChannelIndex 和寄存器定义
#include "ft32f4xx_dma.h"

// FT32F4 DMA 中断处理程序定义
// 基于 spec.json: irq_map 字段 (RM V1.00 第 258-259 页)
#define DMA1_ST0_HANDLER    (DMA_FIRST_HANDLER + 0)
#define DMA1_ST1_HANDLER    (DMA_FIRST_HANDLER + 1)
#define DMA1_ST2_HANDLER    (DMA_FIRST_HANDLER + 2)
#define DMA1_ST3_HANDLER    (DMA_FIRST_HANDLER + 3)
#define DMA1_ST4_HANDLER    (DMA_FIRST_HANDLER + 4)
#define DMA1_ST5_HANDLER    (DMA_FIRST_HANDLER + 5)
#define DMA1_ST6_HANDLER    (DMA_FIRST_HANDLER + 6)
#define DMA1_ST7_HANDLER    (DMA_FIRST_HANDLER + 7)
#define DMA2_ST0_HANDLER    (DMA_FIRST_HANDLER + 8)
#define DMA2_ST1_HANDLER    (DMA_FIRST_HANDLER + 9)
#define DMA2_ST2_HANDLER    (DMA_FIRST_HANDLER + 10)
#define DMA2_ST3_HANDLER    (DMA_FIRST_HANDLER + 11)
#define DMA2_ST4_HANDLER    (DMA_FIRST_HANDLER + 12)
#define DMA2_ST5_HANDLER    (DMA_FIRST_HANDLER + 13)
#define DMA2_ST6_HANDLER    (DMA_FIRST_HANDLER + 14)
#define DMA2_ST7_HANDLER    (DMA_FIRST_HANDLER + 15)
#define DMA_LAST_HANDLER    DMA2_ST7_HANDLER

// DMA 设备编号和索引计算宏
// FT32 有 2 个 DMA 实例，每个 8 个通道
#define DMA_DEVICE_NO(x)    ((((x) - 1) / 8) + 1)
#define DMA_DEVICE_INDEX(x) ((((x) - 1) % 8))

// DMA 输出字符串格式
#define DMA_OUTPUT_INDEX    0
#define DMA_OUTPUT_STRING   "DMA%d Stream %d:"

// 定义 DMA 通道宏
// 参数：d = DMA 实例号 (1 或 2), s = 通道号 (0-7), f = flagsShift 偏移量
// 基于 spec.json: channel_offset = 0x58
// Citation: ft32f407xe.h -> IRQn 定义 (DMA1_CH0_IRQn, etc.)
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

// 定义 DMA 中断处理程序宏
// 参数：d = DMA 实例号，s = 通道号，i = 中断处理程序标识符
#define DEFINE_DMA_IRQ_HANDLER(d, s, i) FAST_IRQ_HANDLER void d ## _Channel ## s ## _IRQHandler(void) {\
                                                                const uint8_t index = DMA_IDENTIFIER_TO_INDEX(i); \
                                                                dmaCallbackHandlerFuncPtr handler = dmaDescriptors[index].irqHandlerCallback; \
                                                                if (handler) \
                                                                    handler(&dmaDescriptors[index]); \
                                                            }

// DMA 标志清除宏
// FT32 使用 DMA 基地址的清除寄存器（CLEARTFR, CLEARBLOCK 等）
// 需要通过 CalBaseAddressAndChannelIndex 获取通道索引
// Citation: ft32f4xx_dma.c -> DMA_ClearFlagStatus()
#define DMA_CLEAR_FLAG(d, flag) \
    do { \
        DMA_BaseAddressAndChannelIndex _dma = CalBaseAddressAndChannelIndex((DMA_Channel_TypeDef*)(d)->ref); \
        if ((flag) & DMA_IT_TCIF) _dma.BaseAddress->CLEARTFR = (1U << _dma.ChannelIndex); \
        if ((flag) & DMA_IT_BLOCK) _dma.BaseAddress->CLEARBLOCK = (1U << _dma.ChannelIndex); \
        if ((flag) & DMA_IT_SRC) _dma.BaseAddress->CLEARSRCTRAN = (1U << _dma.ChannelIndex); \
        if ((flag) & DMA_IT_DST) _dma.BaseAddress->CLEARDSTTRAN = (1U << _dma.ChannelIndex); \
        if ((flag) & DMA_IT_ERR) _dma.BaseAddress->CLEARERR = (1U << _dma.ChannelIndex); \
    } while(0)

// DMA 标志状态获取宏
// FT32 使用 DMA 基地址的状态寄存器（STATUSTFR, STATUSBLOCK 等）
// Citation: ft32f4xx_dma.c -> DMA_GetITStatus()
#define DMA_GET_FLAG_STATUS(d, flag) \
    ({ \
        uint32_t _status = 0; \
        DMA_BaseAddressAndChannelIndex _dma = CalBaseAddressAndChannelIndex((DMA_Channel_TypeDef*)(d)->ref); \
        uint32_t _ch_mask = (1U << _dma.ChannelIndex); \
        if ((flag) & DMA_IT_TCIF) _status |= (_dma.BaseAddress->STATUSTFR & _ch_mask); \
        if ((flag) & DMA_IT_BLOCK) _status |= (_dma.BaseAddress->STATUSBLOCK & _ch_mask); \
        if ((flag) & DMA_IT_SRC) _status |= (_dma.BaseAddress->STATUSSRCTRAN & _ch_mask); \
        if ((flag) & DMA_IT_DST) _status |= (_dma.BaseAddress->STATUSDSTTRAN & _ch_mask); \
        if ((flag) & DMA_IT_ERR) _status |= (_dma.BaseAddress->STATUSERR & _ch_mask); \
        _status; \
    })

// DMA 中断标志定义
// 使用 FT32 标准库定义，避免值冲突
// FT32 标准库定义 (ft32f4xx_dma.h):
//   DMA_IT_TFR   = 0x01U (传输完成) - 对应 Betaflight DMA_IT_TCIF
//   DMA_IT_BLOCK = 0x02U (块传输完成)
//   DMA_IT_SRC   = 0x04U (源传输完成)
//   DMA_IT_DST   = 0x08U (目标传输完成)
//   DMA_IT_ERR   = 0x10U (传输错误) - 对应 Betaflight DMA_IT_TEIF
#define DMA_IT_TCIF         DMA_IT_TFR    // 传输完成中断
// FT32 使用 Channel 架构，不支持 HTIF/DMEIF/FEIF 这些标志，故不定义

// DMA 通用宏定义
#define xDMA_Init(dmaResource, initStruct) DMA_Init((DMA_ARCH_TYPE *)(dmaResource), initStruct)
#define xDMA_DeInit(dmaResource) DMA_DeInit((DMA_ARCH_TYPE *)(dmaResource))
#define xDMA_Cmd(dmaResource, newState) DMA_Cmd((DMA_ARCH_TYPE *)(dmaResource), newState)
#define xDMA_ITConfig(dmaResource, flags, newState) DMA_ITConfig((DMA_ARCH_TYPE *)(dmaResource), flags, newState)
#define xDMA_GetCurrDataCounter(dmaResource) DMA_GetCurrDataCounter((DMA_ARCH_TYPE *)(dmaResource))
// CTL register bits [60:45] contain BLOCK_TS (16-bit block transfer count)
// Source: FT32F405_407xx_RM_V1.00_cn.pdf page 209
#define xDMA_SetCurrDataCounter(dmaResource, count) DMA_SetCurrDataCounter((DMA_ARCH_TYPE *)(dmaResource), count)
#define xDMA_GetFlagStatus(dmaResource, flags) DMA_GetFlagStatus((DMA_ARCH_TYPE *)(dmaResource), flags)
#define xDMA_ClearFlag(dmaResource, flags) DMA_ClearFlagStatus((DMA_ARCH_TYPE *)(dmaResource), flags)

// FT32 DMA 标准库缺少 DMA_SetCurrDataCounter，提供内联实现
// CTL register layout: bits [60:45] = BLOCK_TS (16-bit)
// Source: FT32F405_407xx_RM_V1.00_cn.pdf page 209
static inline void DMA_SetCurrDataCounter(DMA_Channel_TypeDef* DMAy_Channelx, uint16_t count)
{
    // Clear bits [60:45] (16-bit BLOCK_TS field) then set new value
    DMAy_Channelx->CTL &= ~(0xFFFFULL << 45U);
    DMAy_Channelx->CTL |= ((uint64_t)count << 45U);
}
