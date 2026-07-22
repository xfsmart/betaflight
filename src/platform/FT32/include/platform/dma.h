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

// FT32F4 uses channel-based DMA architecture
// Uses DMA_Channel_TypeDef as architecture type
#define DMA_ARCH_TYPE DMA_Channel_TypeDef

#include "drivers/dma.h"

// Include FT32 DMA standard library header
#include "ft32f4xx_dma.h"

// FT32F4 DMA interrupt handler definitions
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
#define FT32_DMA_CHANNELS_PER_CONTROLLER 8
#define FT32_DMA_CHANNEL_COUNT 16

// DMA device number and index calculation macros
// FT32 has 2 DMA instances, each with 8 channels
#define DMA_DEVICE_NO(x)    ((((x) - 1) / 8) + 1)
#define DMA_DEVICE_INDEX(x) ((((x) - 1) % 8))

// DMA output string format
#define DMA_OUTPUT_INDEX    0
#define DMA_OUTPUT_STRING   "DMA%d Stream %d:"

// DMA channel macro definition
// Parameters: d = DMA instance (1 or 2), s = channel (0-7), f = flagsShift offset
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

// DMA interrupt handler macro definition
// Parameters: d = DMA instance, s = channel, i = handler identifier
#define DEFINE_DMA_IRQ_HANDLER(d, s, i) FAST_IRQ_HANDLER void d ## _Channel ## s ## _IRQHandler(void) {\
                                                                const uint8_t index = DMA_IDENTIFIER_TO_INDEX(i); \
                                                                dmaCallbackHandlerFuncPtr handler = dmaDescriptors[index].irqHandlerCallback; \
                                                                if (handler) \
                                                                    handler(&dmaDescriptors[index]); \
                                                            }

// DMA flag clear macro
// FT32 uses DMA base address clear registers (CLEARTFR, CLEARBLOCK, etc.)
// Uses CalBaseAddressAndChannelIndex to get channel index
#define DMA_CLEAR_FLAG(d, flag) \
    do { \
        DMA_BaseAddressAndChannelIndex _dma = CalBaseAddressAndChannelIndex((DMA_Channel_TypeDef*)(d)->ref); \
        if ((flag) & DMA_IT_TCIF) _dma.BaseAddress->CLEARTFR = (1U << _dma.ChannelIndex); \
        if ((flag) & DMA_IT_BLOCK) _dma.BaseAddress->CLEARBLOCK = (1U << _dma.ChannelIndex); \
        if ((flag) & DMA_IT_SRC) _dma.BaseAddress->CLEARSRCTRAN = (1U << _dma.ChannelIndex); \
        if ((flag) & DMA_IT_DST) _dma.BaseAddress->CLEARDSTTRAN = (1U << _dma.ChannelIndex); \
        if ((flag) & DMA_IT_ERR) _dma.BaseAddress->CLEARERR = (1U << _dma.ChannelIndex); \
    } while(0)

// DMA flag status macro
// FT32 uses DMA base address status registers (STATUSTFR, STATUSBLOCK, etc.)
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

// DMA interrupt flag definitions
// Uses FT32 standard library definitions to avoid value conflicts
// FT32 standard library (ft32f4xx_dma.h):
//   DMA_IT_TFR   = 0x01U (transfer complete) - maps to Betaflight DMA_IT_TCIF
//   DMA_IT_BLOCK = 0x02U (block transfer complete)
//   DMA_IT_SRC   = 0x04U (source transfer complete)
//   DMA_IT_DST   = 0x08U (destination transfer complete)
//   DMA_IT_ERR   = 0x10U (transfer error) - maps to Betaflight DMA_IT_TEIF
#define DMA_IT_TCIF         DMA_IT_TFR    // Transfer complete interrupt
// FT32 channel architecture does not support HTIF/DMEIF/FEIF flags

void ft32DmaDeInit(DMA_ARCH_TYPE *dmaResource);
void ft32DmaInit(DMA_ARCH_TYPE *dmaResource, DMA_InitTypeDef *init);
void ft32DmaCmd(DMA_ARCH_TYPE *dmaResource, FunctionalState newState);
uint16_t ft32DmaGetCurrDataCounter(DMA_ARCH_TYPE *dmaResource);
void ft32DmaSetCurrDataCounter(DMA_ARCH_TYPE *dmaResource, uint16_t count);
uint8_t ft32DmaIsChannelEnabled(DMA_ARCH_TYPE *dmaResource);

// DMA common macro definitions
#define xDMA_DeInit(dmaResource) ft32DmaDeInit((DMA_ARCH_TYPE *)(dmaResource))
#define xDMA_Cmd(dmaResource, newState) ft32DmaCmd((DMA_ARCH_TYPE *)(dmaResource), newState)

// Issue the one-shot channel disable command without waiting for CHEN convergence.
// ISR callers must use this primitive and defer convergence to foreground service.
static inline void ft32DmaRequestDisable(DMA_ARCH_TYPE *dmaResource)
{
    DMA_Channel_Cmd(dmaResource, DISABLE);
}
#define xDMA_ITConfig(dmaResource, flags, newState) DMA_ITConfig((DMA_ARCH_TYPE *)(dmaResource), flags, newState)
#define xDMA_GetCurrDataCounter(dmaResource) ft32DmaGetCurrDataCounter((DMA_ARCH_TYPE *)(dmaResource))
#define xDMA_SetCurrDataCounter(dmaResource, count) ft32DmaSetCurrDataCounter((DMA_ARCH_TYPE *)(dmaResource), count)
#define xDMA_GetFlagStatus(dmaResource, flags) DMA_GetFlagStatus((DMA_ARCH_TYPE *)(dmaResource), flags)
#define xDMA_ClearFlag(dmaResource, flags) DMA_ClearFlagStatus((DMA_ARCH_TYPE *)(dmaResource), flags)

static inline uint32_t ft32DmaGetHardwareInterface(const dmaResource_t *dmaResource)
{
    DMA_BaseAddressAndChannelIndex dma = CalBaseAddressAndChannelIndex((DMA_ARCH_TYPE *)dmaResource);
    return dma.ChannelIndex;
}

static inline void ft32DmaSetSrcRequest(DMA_InitTypeDef *init, const dmaResource_t *dmaResource, uint32_t request)
{
    init->SrcHardwareInterface = ft32DmaGetHardwareInterface(dmaResource);
    init->SrcHsIfPeriphSel = request;
}

static inline void ft32DmaSetDstRequest(DMA_InitTypeDef *init, const dmaResource_t *dmaResource, uint32_t request)
{
    init->DstHardwareInterface = ft32DmaGetHardwareInterface(dmaResource);
    init->DstHsIfPeriphSel = request;
}

static inline void ft32DmaClearRequestSlot(DMA_ARCH_TYPE *dmaResource, uint32_t hardwareInterface)
{
    DMA_BaseAddressAndChannelIndex dma = CalBaseAddressAndChannelIndex(dmaResource);
    const uint32_t shift = (hardwareInterface & 0x7U) * 3U;

    dma.BaseAddress->CHSEL &= ~((uint64_t)0x7U << shift);
}

static inline void ft32DmaClearActiveRequestSlots(DMA_ARCH_TYPE *dmaResource, const DMA_InitTypeDef *init)
{
    switch (init->TransferTypeFlowCtl) {
    case DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA:
    case DMA_TRANSFERTYPE_FLOWCTL_M2P_PRE:
        ft32DmaClearRequestSlot(dmaResource, init->DstHardwareInterface);
        break;

    case DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA:
    case DMA_TRANSFERTYPE_FLOWCTL_P2M_PRE:
        ft32DmaClearRequestSlot(dmaResource, init->SrcHardwareInterface);
        break;

    case DMA_TRANSFERTYPE_FLOWCTL_P2P_DMA:
    case DMA_TRANSFERTYPE_FLOWCTL_P2P_SRCPRE:
    case DMA_TRANSFERTYPE_FLOWCTL_P2P_DSTPRE:
        ft32DmaClearRequestSlot(dmaResource, init->SrcHardwareInterface);
        ft32DmaClearRequestSlot(dmaResource, init->DstHardwareInterface);
        break;
    }
}

#define xDMA_Init(dmaResource, initStruct) ft32DmaInit((DMA_ARCH_TYPE *)(dmaResource), (initStruct))

// FT32 DMA standard library lacks a source-address setter, provide inline implementation.
// The channel must be stopped before calling this: SAR is only writable while the
// channel is disabled, the same precondition the block-size setter relies on.
static inline void DMA_SetSrcAddress(DMA_Channel_TypeDef* DMAy_Channelx, uint32_t srcAddress)
{
    DMAy_Channelx->SAR = (uint64_t)srcAddress;
}
