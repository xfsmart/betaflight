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
 * DMA descriptors.
 * FT32F4: DMA1/DMA2 each have 8 channels (Channel0-7)
 * flagsShift values match STM32F4 pattern for compatibility
 */
dmaChannelDescriptor_t dmaDescriptors[DMA_LAST_HANDLER] = {
    DEFINE_DMA_CHANNEL(DMA1, 0,  0),
    DEFINE_DMA_CHANNEL(DMA1, 1,  6),
    DEFINE_DMA_CHANNEL(DMA1, 2, 16),
    DEFINE_DMA_CHANNEL(DMA1, 3, 22),
    DEFINE_DMA_CHANNEL(DMA1, 4, 32),
    DEFINE_DMA_CHANNEL(DMA1, 5, 38),
    DEFINE_DMA_CHANNEL(DMA1, 6, 48),
    DEFINE_DMA_CHANNEL(DMA1, 7, 54),

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
 * DMA IRQ Handlers
 */
DEFINE_DMA_IRQ_HANDLER(1, 0, DMA1_CH0_HANDLER)
DEFINE_DMA_IRQ_HANDLER(1, 1, DMA1_CH1_HANDLER)
DEFINE_DMA_IRQ_HANDLER(1, 2, DMA1_CH2_HANDLER)
DEFINE_DMA_IRQ_HANDLER(1, 3, DMA1_CH3_HANDLER)
DEFINE_DMA_IRQ_HANDLER(1, 4, DMA1_CH4_HANDLER)
DEFINE_DMA_IRQ_HANDLER(1, 5, DMA1_CH5_HANDLER)
DEFINE_DMA_IRQ_HANDLER(1, 6, DMA1_CH6_HANDLER)
DEFINE_DMA_IRQ_HANDLER(1, 7, DMA1_CH7_HANDLER)
DEFINE_DMA_IRQ_HANDLER(2, 0, DMA2_CH0_HANDLER)
DEFINE_DMA_IRQ_HANDLER(2, 1, DMA2_CH1_HANDLER)
DEFINE_DMA_IRQ_HANDLER(2, 2, DMA2_CH2_HANDLER)
DEFINE_DMA_IRQ_HANDLER(2, 3, DMA2_CH3_HANDLER)
DEFINE_DMA_IRQ_HANDLER(2, 4, DMA2_CH4_HANDLER)
DEFINE_DMA_IRQ_HANDLER(2, 5, DMA2_CH5_HANDLER)
DEFINE_DMA_IRQ_HANDLER(2, 6, DMA2_CH6_HANDLER)
DEFINE_DMA_IRQ_HANDLER(2, 7, DMA2_CH7_HANDLER)

void dmaEnable(dmaIdentifier_e identifier)
{
    const int index = DMA_IDENTIFIER_TO_INDEX(identifier);
    if (dmaDescriptors[index].dma == DMA1) {
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    } else {
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
    }
}

void dmaSetHandler(dmaIdentifier_e identifier, dmaCallbackHandlerFuncPtr callback, uint32_t priority, uint32_t userParam)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    const int index = DMA_IDENTIFIER_TO_INDEX(identifier);

    // Enable DMA clock
    dmaEnable(identifier);

    dmaDescriptors[index].irqHandlerCallback = callback;
    dmaDescriptors[index].userParam = userParam;
    dmaDescriptors[index].completeFlag = DMA_IT_TCIF;

    NVIC_InitStructure.NVIC_IRQChannel = dmaDescriptors[index].irqN;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = NVIC_PRIORITY_BASE(priority);
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = NVIC_PRIORITY_SUB(priority);
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

int dmaGetHandlerCount(void)
{
    return DMA_LAST_HANDLER;
}

int dmaGetDeviceNumber(dmaIdentifier_e identifier)
{
    return DMA_DEVICE_NO(identifier);
}

int dmaGetDeviceIndex(dmaIdentifier_e identifier)
{
    return DMA_DEVICE_INDEX(identifier);
}

const char *dmaGetDisplayString(void)
{
    return DMA_OUTPUT_STRING;
}

uint32_t dmaGetDataLength(dmaResource_t *ref)
{
    return DMA_GetCurrDataCounter((DMA_ARCH_TYPE *)ref);
}

#endif
