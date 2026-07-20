/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Betaflight is free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
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

#include <stdint.h>
#include <math.h>
#include <string.h>

#include "platform.h"

#ifdef USE_DSHOT_BITBANG

#include "build/atomic.h"
#include "build/debug.h"
#include "build/debug_pin.h"

#include "drivers/io.h"
#include "drivers/io_impl.h"
#include "drivers/dma.h"
#include "drivers/dma_reqmap.h"
#include "platform/dma.h"
#include "drivers/dshot.h"
#include "dshot_bitbang_impl.h"
#include "drivers/dshot_command.h"
#include "drivers/motor.h"
#include "drivers/nvic.h"
#include "drivers/time.h"
#include "drivers/timer.h"

#include "platform/timer.h"

#include "pg/motor.h"

void bbGpioSetup(bbMotor_t *bbMotor)
{
    bbPort_t *bbPort = bbMotor->bbPort;
    int pinIndex = bbMotor->pinIndex;

    bbPort->gpioModeMask |= (GPIO_MODER_MODER0 << (pinIndex * 2));
    bbPort->gpioModeInput |= (GPIO_Mode_IN << (pinIndex * 2));
    bbPort->gpioModeOutput |= (GPIO_Mode_OUT << (pinIndex * 2));

#ifdef USE_DSHOT_TELEMETRY
    if (useDshotTelemetry) {
        bbPort->gpioIdleBSRR |= (1 << pinIndex);         // BS (lower half)
    } else
#endif
    {
        bbPort->gpioIdleBSRR |= (1 << (pinIndex + 16));  // BR (higher half)
    }

#ifdef USE_DSHOT_TELEMETRY
    if (useDshotTelemetry) {
        IOWrite(bbMotor->io, 1);
    } else
#endif
    {
        IOWrite(bbMotor->io, 0);
    }
}

void bbTimerChannelInit(bbPort_t *bbPort)
{
    const timerHardware_t *timhw = bbPort->timhw;

    TIM_OCInitTypeDef TIM_OCStruct;

    TIM_OCStructInit(&TIM_OCStruct);
    TIM_OCStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCStruct.TIM_OCIdleState = TIM_OCIdleState_Set;
    TIM_OCStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCStruct.TIM_OCPolarity = TIM_OCPolarity_Low;

    TIM_OCStruct.TIM_Pulse = 10;

    TIM_Cmd((TIM_TypeDef *)bbPort->timhw->tim, DISABLE);

    timerOCInit((TIM_TypeDef *)timhw->tim, timhw->channel, &TIM_OCStruct);

#ifdef DEBUG_MONITOR_PACER
    if (timhw->tag) {
        IO_t io = IOGetByTag(timhw->tag);
        IOConfigGPIOAF(io, IOCFG_AF_PP, timhw->alternateFunction);
        IOInit(io, OWNER_DSHOT_BITBANG, 0);
        TIM_CtrlPWMOutputs((TIM_TypeDef *)timhw->tim, ENABLE);
    }
#endif

    TIM_Cmd((TIM_TypeDef *)bbPort->timhw->tim, ENABLE);
}

#ifdef USE_DMA_REGISTER_CACHE

static void bbLoadDMARegs(dmaResource_t *dmaResource, dmaRegCache_t *dmaRegCache)
{
    DMA_Channel_TypeDef *ch = (DMA_Channel_TypeDef *)dmaResource;
    xDMA_Cmd(ch, DISABLE);
    if (ft32DmaIsChannelEnabled(ch)) {
        return;
    }
    ch->SAR = dmaRegCache->SAR;
    ch->DAR = dmaRegCache->DAR;
    ch->CTL = dmaRegCache->CTL;
}

static void bbSaveDMARegs(dmaResource_t *dmaResource, dmaRegCache_t *dmaRegCache)
{
    DMA_Channel_TypeDef *ch = (DMA_Channel_TypeDef *)dmaResource;
    xDMA_Cmd(ch, DISABLE);
    if (ft32DmaIsChannelEnabled(ch)) {
        return;
    }
    const dmaRegCache_t snapshot = {
        .SAR = ch->SAR,
        .DAR = ch->DAR,
        .CTL = ch->CTL,
    };
    *dmaRegCache = snapshot;
}
#endif

void bbSwitchToOutput(bbPort_t * bbPort)
{
    dbgPinHi(1);

    // Output idle level before switching to output
    // Use BSRR register for this
    // Normal: Use BR (higher half)
    // Inverted: Use BS (lower half)

    bbPort->gpio->BSRR = bbPort->gpioIdleBSRR;

    // Set GPIO to output
    ATOMIC_BLOCK(NVIC_PRIO_TIMER) {
        MODIFY_REG(bbPort->gpio->MODER, bbPort->gpioModeMask, bbPort->gpioModeOutput);
    }

    // Reinitialize port group DMA for output

    dmaResource_t *dmaResource = bbPort->dmaResource;
    xDMA_Cmd(dmaResource, DISABLE);
    if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)dmaResource)) {
        return;
    }
#ifdef USE_DMA_REGISTER_CACHE
    bbLoadDMARegs(dmaResource, &bbPort->dmaRegOutput);
#else
    xDMA_DeInit(dmaResource);
    xDMA_Init(dmaResource, &bbPort->outputDmaInit);
    xDMA_ITConfig(dmaResource, DMA_IT_TFR, ENABLE);
#endif

    // Reinitialize pacer timer for output

    ((TIM_TypeDef *)bbPort->timhw->tim)->ARR = bbPort->outputARR;

    bbPort->direction = DSHOT_BITBANG_DIRECTION_OUTPUT;

    dbgPinLo(1);
}

#ifdef USE_DSHOT_TELEMETRY
void bbSwitchToInput(bbPort_t *bbPort)
{
    dbgPinHi(1);

    // Set GPIO to input

    ATOMIC_BLOCK(NVIC_PRIO_TIMER) {
        MODIFY_REG(bbPort->gpio->MODER, bbPort->gpioModeMask, bbPort->gpioModeInput);
    }

    // Reinitialize port group DMA for input

    dmaResource_t *dmaResource = bbPort->dmaResource;
    xDMA_Cmd(dmaResource, DISABLE);
    if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)dmaResource)) {
        return;
    }
#ifdef USE_DMA_REGISTER_CACHE
    bbLoadDMARegs(dmaResource, &bbPort->dmaRegInput);
#else
    xDMA_DeInit(dmaResource);
    xDMA_Init(dmaResource, &bbPort->inputDmaInit);
    xDMA_ITConfig(dmaResource, DMA_IT_TFR, ENABLE);
#endif

    // Reinitialize pacer timer for input

    ((TIM_TypeDef *)bbPort->timhw->tim)->CNT = 0;
    ((TIM_TypeDef *)bbPort->timhw->tim)->ARR = bbPort->inputARR;

    bbDMA_Cmd(bbPort, ENABLE);

    bbPort->direction = DSHOT_BITBANG_DIRECTION_INPUT;

    dbgPinLo(1);
}
#endif

void bbDMAPreconfigure(bbPort_t *bbPort, uint8_t direction)
{
    DMA_InitTypeDef *dmainit = (direction == DSHOT_BITBANG_DIRECTION_OUTPUT) ? &bbPort->outputDmaInit : &bbPort->inputDmaInit;

    xDMA_Cmd(bbPort->dmaResource, DISABLE);
    if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)bbPort->dmaResource)) {
        return;
    }
    DMA_StructInit(dmainit);

    dmainit->SrcAddrMode = DMA_SRC_ADDRMODE_INC;
    dmainit->DstAddrMode = DMA_DST_ADDRMODE_HOLD;
    dmainit->FIFOMode = ENABLE;
    dmainit->ReloadDst = DISABLE;
    dmainit->ReloadSrc = DISABLE;

    if (direction == DSHOT_BITBANG_DIRECTION_OUTPUT) {
        dmainit->Priority = DMA_CH_PRIORITY_6;
        dmainit->SrcAddress = (uint32_t)bbPort->portOutputBuffer;
        dmainit->DstAddress = (uint32_t)&bbPort->gpio->BSRR;
        dmainit->BlockTransSize = bbPort->portOutputCount;
        dmainit->SrcDstMasterSel = DMA_SRCMASTER1_DSTMASTER2;
        dmainit->TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA;
        dmainit->SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_32BITS;
        dmainit->DstTransferWidth = DMA_DST_TRANSFERWIDTH_32BITS;
        ft32DmaSetDstRequest(dmainit, bbPort->dmaResource, bbPort->dmaChannel);

#ifdef USE_DMA_REGISTER_CACHE
        xDMA_Init(bbPort->dmaResource, dmainit);
        bbSaveDMARegs(bbPort->dmaResource, &bbPort->dmaRegOutput);
#endif
    } else {
        dmainit->Priority = DMA_CH_PRIORITY_7;
        dmainit->SrcAddress = (uint32_t)&bbPort->gpio->IDR;
        dmainit->DstAddress = (uint32_t)bbPort->portInputBuffer;
        dmainit->BlockTransSize = bbPort->portInputCount;
        dmainit->SrcDstMasterSel = DMA_SRCMASTER1_DSTMASTER2;
        dmainit->TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA;
        dmainit->SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_16BITS;
        dmainit->DstTransferWidth = DMA_DST_TRANSFERWIDTH_16BITS;
        ft32DmaSetSrcRequest(dmainit, bbPort->dmaResource, bbPort->dmaChannel);

#ifdef USE_DMA_REGISTER_CACHE
        xDMA_Init(bbPort->dmaResource, dmainit);
        bbSaveDMARegs(bbPort->dmaResource, &bbPort->dmaRegInput);
#endif
    }
}

void bbTIM_TimeBaseInit(bbPort_t *bbPort, uint16_t period)
{
    TIM_TimeBaseInitTypeDef *init = &bbPort->timeBaseInit;

    init->TIM_Prescaler = 0;
    init->TIM_ClockDivision = TIM_CKD_DIV1;
    init->TIM_CounterMode = TIM_CounterMode_Up;
    init->TIM_Period = period;
    TIM_TimeBaseInit((TIM_TypeDef *)bbPort->timhw->tim, init);
    TIM_ARRPreloadConfig((TIM_TypeDef *)bbPort->timhw->tim, ENABLE);
}

void bbTIM_DMACmd(void *TIMx, uint16_t TIM_DMASource, FunctionalState NewState)
{
    TIM_DMACmd((TIM_TypeDef *)TIMx, TIM_DMASource, NewState);
}

void bbDMA_ITConfig(bbPort_t *bbPort)
{
    xDMA_Cmd(bbPort->dmaResource, DISABLE);
    if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)bbPort->dmaResource)) {
        return;
    }
    xDMA_ITConfig(bbPort->dmaResource, DMA_IT_TFR, ENABLE);
}

void bbDMA_Cmd(bbPort_t *bbPort, FunctionalState NewState)
{
    xDMA_Cmd(bbPort->dmaResource, NewState);
}

int bbDMA_Count(bbPort_t *bbPort)
{
    return xDMA_GetCurrDataCounter(bbPort->dmaResource);
}

#endif // USE_DSHOT_BB
