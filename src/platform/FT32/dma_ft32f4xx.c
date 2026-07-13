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
#include <string.h>

#include "platform.h"

#ifdef USE_DMA

#include "drivers/nvic.h"
#include "drivers/dma_impl.h"
#include "platform/dma.h"
#include "drivers/resource.h"

/*
 * DMA channel descriptors
 *
 * FT32F4 provides 2 DMA controllers (DMA1, DMA2), each with 8 channels (0-7),
 * totalling 16 DMA channels.
 *
 * Note: flagsShift is populated for Betaflight DMA framework compatibility but
 * is not used by FT32 flag macros. FT32 uses per-channel status/clear registers
 * indexed by CalBaseAddressAndChannelIndex() instead.
 */
dmaChannelDescriptor_t dmaDescriptors[DMA_LAST_HANDLER] = {
    // DMA1 Channel 0-7
    DEFINE_DMA_CHANNEL(DMA1, 0,  0),
    DEFINE_DMA_CHANNEL(DMA1, 1,  6),
    DEFINE_DMA_CHANNEL(DMA1, 2, 16),
    DEFINE_DMA_CHANNEL(DMA1, 3, 22),
    DEFINE_DMA_CHANNEL(DMA1, 4, 32),
    DEFINE_DMA_CHANNEL(DMA1, 5, 38),
    DEFINE_DMA_CHANNEL(DMA1, 6, 48),
    DEFINE_DMA_CHANNEL(DMA1, 7, 54),

    // DMA2 Channel 0-7
    DEFINE_DMA_CHANNEL(DMA2, 0,  0),
    DEFINE_DMA_CHANNEL(DMA2, 1,  6),
    DEFINE_DMA_CHANNEL(DMA2, 2, 16),
    DEFINE_DMA_CHANNEL(DMA2, 3, 22),
    DEFINE_DMA_CHANNEL(DMA2, 4, 32),
    DEFINE_DMA_CHANNEL(DMA2, 5, 38),
    DEFINE_DMA_CHANNEL(DMA2, 6, 48),
    DEFINE_DMA_CHANNEL(DMA2, 7, 54),
};

/*
 * DMA IRQ handlers
 *
 * Each handler dispatches to the registered callback via dmaDescriptors[].
 */
DEFINE_DMA_IRQ_HANDLER(DMA1, 0, DMA1_ST0_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA1, 1, DMA1_ST1_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA1, 2, DMA1_ST2_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA1, 3, DMA1_ST3_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA1, 4, DMA1_ST4_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA1, 5, DMA1_ST5_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA1, 6, DMA1_ST6_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA1, 7, DMA1_ST7_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA2, 0, DMA2_ST0_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA2, 1, DMA2_ST1_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA2, 2, DMA2_ST2_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA2, 3, DMA2_ST3_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA2, 4, DMA2_ST4_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA2, 5, DMA2_ST5_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA2, 6, DMA2_ST6_HANDLER)
DEFINE_DMA_IRQ_HANDLER(DMA2, 7, DMA2_ST7_HANDLER)

/*
 * DMA peripheral clock enable
 *
 * DMA1/DMA2 clocks are gated on AHB1 bus (RCC_AHB1ENR bit 21/22).
 */
#define DMA_RCC(x) ((x) == DMA1 ? RCC_AHB1Periph_DMA1 : RCC_AHB1Periph_DMA2)
#define FT32_DMA_BLOCK_TS_SHIFT 45U
#define FT32_DMA_BLOCK_TS_MASK  (0xFFFFULL << FT32_DMA_BLOCK_TS_SHIFT)

static uint16_t ft32DmaBlockSize[FT32_DMA_CHANNEL_COUNT];

static DMA_BaseAddressAndChannelIndex ft32DmaAddressAndChannel(DMA_ARCH_TYPE *dmaResource)
{
    return CalBaseAddressAndChannelIndex(dmaResource);
}

static uint32_t ft32DmaLinearIndex(DMA_ARCH_TYPE *dmaResource)
{
    const DMA_BaseAddressAndChannelIndex dma = ft32DmaAddressAndChannel(dmaResource);
    const uint32_t controllerOffset = (dma.BaseAddress == DMA2) ? FT32_DMA_CHANNELS_PER_CONTROLLER : 0U;

    return controllerOffset + dma.ChannelIndex;
}

static uint64_t ft32DmaChannelMask(const DMA_BaseAddressAndChannelIndex *dma)
{
    return 1ULL << dma->ChannelIndex;
}

static void ft32DmaClearChannelStatus(DMA_ARCH_TYPE *dmaResource)
{
    const DMA_BaseAddressAndChannelIndex dma = ft32DmaAddressAndChannel(dmaResource);
    const uint64_t mask = ft32DmaChannelMask(&dma);

    dma.BaseAddress->CLEARTFR = mask;
    dma.BaseAddress->CLEARBLOCK = mask;
    dma.BaseAddress->CLEARSRCTRAN = mask;
    dma.BaseAddress->CLEARDSTTRAN = mask;
    dma.BaseAddress->CLEARERR = mask;
}

static void ft32DmaWriteBlockSize(DMA_ARCH_TYPE *dmaResource, uint16_t count)
{
    dmaResource->CTL &= ~FT32_DMA_BLOCK_TS_MASK;
    dmaResource->CTL |= (uint64_t)count << FT32_DMA_BLOCK_TS_SHIFT;
}

static uint32_t ft32DmaMasterSelectForTransfer(uint32_t transferTypeFlowCtl)
{
    switch (transferTypeFlowCtl) {
    case DMA_TRANSFERTYPE_FLOWCTL_M2M_DMA:
    case DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA:
    case DMA_TRANSFERTYPE_FLOWCTL_M2P_PRE:
        return DMA_SRCMASTER1_DSTMASTER2;

    case DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA:
    case DMA_TRANSFERTYPE_FLOWCTL_P2M_PRE:
        return DMA_SRCMASTER2_DSTMASTER1;

    case DMA_TRANSFERTYPE_FLOWCTL_P2P_DMA:
    case DMA_TRANSFERTYPE_FLOWCTL_P2P_SRCPRE:
    case DMA_TRANSFERTYPE_FLOWCTL_P2P_DSTPRE:
        return DMA_SRCMASTER2_DSTMASTER2;
    }

    return DMA_SRCMASTER1_DSTMASTER1;
}

static void ft32DmaApplyMasterSelect(DMA_InitTypeDef *init)
{
    init->SrcDstMasterSel = ft32DmaMasterSelectForTransfer(init->TransferTypeFlowCtl);
}

uint8_t ft32DmaIsChannelEnabled(DMA_ARCH_TYPE *dmaResource)
{
    const DMA_BaseAddressAndChannelIndex dma = ft32DmaAddressAndChannel(dmaResource);

    return (dma.BaseAddress->CHEN & ft32DmaChannelMask(&dma)) != 0U;
}

void ft32DmaSetCurrDataCounter(DMA_ARCH_TYPE *dmaResource, uint16_t count)
{
    ft32DmaBlockSize[ft32DmaLinearIndex(dmaResource)] = count;
    ft32DmaWriteBlockSize(dmaResource, count);
}

uint16_t ft32DmaGetCurrDataCounter(DMA_ARCH_TYPE *dmaResource)
{
    const uint16_t blockSize = ft32DmaBlockSize[ft32DmaLinearIndex(dmaResource)];
    if (blockSize == 0U) {
        return 0U;
    }

    const uint16_t completed = DMA_GetCurrDataCounter(dmaResource);
    if (completed == 0U) {
        return blockSize;
    }

    if (completed < blockSize) {
        return blockSize - completed;
    }

    if (DMA_GetFlagStatus(dmaResource, DMA_FLAG_TFR) != RESET || !ft32DmaIsChannelEnabled(dmaResource)) {
        return 0U;
    }

    return blockSize;
}

void ft32DmaCmd(DMA_ARCH_TYPE *dmaResource, FunctionalState newState)
{
    const DMA_BaseAddressAndChannelIndex dma = ft32DmaAddressAndChannel(dmaResource);

    if (newState != DISABLE) {
        DMA_ENABLE(dma.BaseAddress);
        DMA_Channel_Cmd(dmaResource, ENABLE);
        return;
    }

    DMA_Channel_Cmd(dmaResource, DISABLE);

    for (volatile uint32_t timeout = 100000U;
         (dma.BaseAddress->CHEN & ft32DmaChannelMask(&dma)) != 0U && timeout > 0U;
         timeout--) {
    }

    ft32DmaBlockSize[ft32DmaLinearIndex(dmaResource)] = 0U;

    if ((dma.BaseAddress->CHEN & ((1ULL << FT32_DMA_CHANNELS_PER_CONTROLLER) - 1ULL)) == 0U) {
        DMA_DISABLE(dma.BaseAddress);
    }
}

void ft32DmaDeInit(DMA_ARCH_TYPE *dmaResource)
{
    const DMA_BaseAddressAndChannelIndex dma = ft32DmaAddressAndChannel(dmaResource);

    ft32DmaCmd(dmaResource, DISABLE);
    ft32DmaClearChannelStatus(dmaResource);
    ft32DmaClearRequestSlot(dmaResource, dma.ChannelIndex);
    dmaResource->SAR = 0U;
    dmaResource->DAR = 0U;
    dmaResource->CTL = 0U;
    dmaResource->CFG = 0U;
}

void ft32DmaInit(DMA_ARCH_TYPE *dmaResource, DMA_InitTypeDef *init)
{
    ft32DmaCmd(dmaResource, DISABLE);
    ft32DmaClearChannelStatus(dmaResource);
    ft32DmaApplyMasterSelect(init);
    ft32DmaClearActiveRequestSlots(dmaResource, init);
    ft32DmaBlockSize[ft32DmaLinearIndex(dmaResource)] = init->BlockTransSize;
    DMA_Init(dmaResource, init);
}

/*
 * dmaEnable - Enable the AHB1 clock for the specified DMA controller
 *
 * @param identifier  DMA channel identifier (DMA_FIRST_HANDLER .. DMA_LAST_HANDLER)
 */
void dmaEnable(dmaIdentifier_e identifier)
{
    const int index = DMA_IDENTIFIER_TO_INDEX(identifier);
    RCC_AHB1PeriphClockCmd(DMA_RCC(dmaDescriptors[index].dma), ENABLE);
}

/*
 * dmaFlag_IT_TCIF - Compute the transfer-complete flag bitmask for a DMA channel
 *
 * FT32 DesignWare DMA uses unified status registers (STATUSTFR / CLEARTFR) where
 * each bit position corresponds to a channel index within the controller.
 *
 * @param stream  DMA resource pointer (DMA_Channel_TypeDef *)
 * @return        Bitmask with the channel's transfer-complete bit set
 */
static uint32_t dmaFlag_IT_TCIF(const dmaResource_t *stream)
{
    DMA_BaseAddressAndChannelIndex dma = CalBaseAddressAndChannelIndex((DMA_ARCH_TYPE *)stream);
    return (1U << dma.ChannelIndex);
}

/*
 * dmaSetHandler - Register a DMA transfer-complete callback and configure NVIC
 *
 * Enables the DMA controller clock, stores the callback and user parameter,
 * pre-computes the transfer-complete flag, and configures the NVIC for the
 * channel's IRQ line.
 *
 * completeFlag is pre-computed for Betaflight DMA framework compatibility.
 * FT32 interrupt handlers clear flags via DMA_CLEAR_FLAG directly.
 *
 * @param identifier  DMA channel identifier
 * @param callback    ISR callback function pointer
 * @param priority    Encoded NVIC priority (preemption + sub-priority)
 * @param userParam   Opaque parameter passed to callback on invocation
 */
void dmaSetHandler(dmaIdentifier_e identifier, dmaCallbackHandlerFuncPtr callback, uint32_t priority, uint32_t userParam)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    const int index = DMA_IDENTIFIER_TO_INDEX(identifier);

    RCC_AHB1PeriphClockCmd(DMA_RCC(dmaDescriptors[index].dma), ENABLE);

    dmaDescriptors[index].irqHandlerCallback = callback;
    dmaDescriptors[index].userParam = userParam;
    dmaDescriptors[index].completeFlag = dmaFlag_IT_TCIF(dmaDescriptors[index].ref);

    NVIC_InitStructure.NVIC_IRQChannel = dmaDescriptors[index].irqN;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = NVIC_PRIORITY_BASE(priority);
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = NVIC_PRIORITY_SUB(priority);
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/*
 * dmaGetHandlerCount - Return total number of DMA channel descriptors
 */
int dmaGetHandlerCount(void)
{
    return DMA_LAST_HANDLER;
}

/*
 * dmaGetDeviceNumber - Return the DMA controller number (1 or 2) for an identifier
 */
int dmaGetDeviceNumber(dmaIdentifier_e identifier)
{
    return DMA_DEVICE_NO(identifier);
}

/*
 * dmaGetDeviceIndex - Return the channel index (0-7) within the DMA controller
 */
int dmaGetDeviceIndex(dmaIdentifier_e identifier)
{
    return DMA_DEVICE_INDEX(identifier);
}

/*
 * dmaGetDisplayString - Return printf format string for DMA channel identification
 */
const char *dmaGetDisplayString(void)
{
    return DMA_OUTPUT_STRING;
}

/*
 * dmaGetDataLength - Return the remaining transfer count (NDTR) for a DMA channel
 *
 * @param ref  DMA resource pointer
 * @return     Number of data items remaining to be transferred
 */
uint32_t dmaGetDataLength(dmaResource_t *ref)
{
    return ft32DmaGetCurrDataCounter((DMA_ARCH_TYPE *)ref);
}

#endif
