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

static void bbCommitDMADescriptor(DMA_ARCH_TYPE *dmaRef, DMA_InitTypeDef *descriptor)
{
#ifdef UNIT_TEST
    __asm__ volatile ("" ::: "memory");
    ft32DmaClearActiveRequestSlots(dmaRef, descriptor);
    DMA_Init(dmaRef, descriptor);
    __asm__ volatile ("" ::: "memory");
#else
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ft32DmaClearActiveRequestSlots(dmaRef, descriptor);
    DMA_Init(dmaRef, descriptor);
    __DMB();
    __set_PRIMASK(primask);
#endif
}

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

static bool bbLoadDMARegs(dmaResource_t *dmaResource, const dmaRegCache_t *dmaRegCache, uint16_t count)
{
    DMA_Channel_TypeDef *ch = (DMA_Channel_TypeDef *)dmaResource;
    if (dmaRegCache->CTL == 0U || !ft32DmaTrySetCurrDataCounter(ch, count)) {
        return false;
    }
    ch->SAR = dmaRegCache->SAR;
    ch->DAR = dmaRegCache->DAR;
    ch->CTL = dmaRegCache->CTL;
    ch->CFG = dmaRegCache->CFG;
    xDMA_ITConfig(ch, DMA_IT_TFR | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR, DISABLE);
    xDMA_ITConfig(ch, DMA_IT_TFR | DMA_IT_ERR, ENABLE);
    return true;
}

static bool bbSaveDMARegs(dmaResource_t *dmaResource, dmaRegCache_t *dmaRegCache)
{
    DMA_Channel_TypeDef *ch = (DMA_Channel_TypeDef *)dmaResource;
    if (ft32DmaIsChannelEnabled(ch)) {
        return false;
    }
    const dmaRegCache_t snapshot = {
        .SAR = ch->SAR,
        .DAR = ch->DAR,
        .CTL = ch->CTL,
        .CFG = ch->CFG,
    };
    *dmaRegCache = snapshot;
    return snapshot.CTL != 0U;
}
#endif

static inline void bbPublishDirection(bbPort_t *bbPort, uint8_t direction)
{
    *(volatile uint8_t *)&bbPort->direction = direction;
}

void bbSwitchToOutput(bbPort_t *bbPort)
{
    dbgPinHi(1);
    bbPublishDirection(bbPort, UINT8_MAX);

    dmaResource_t *dmaResource = bbPort->dmaResource;
#ifdef USE_DMA_REGISTER_CACHE
    if (!bbLoadDMARegs(dmaResource, &bbPort->dmaRegOutput, bbPort->portOutputCount)) {
        dbgPinLo(1);
        return;
    }
#else
    DMA_ARCH_TYPE *dmaRef = (DMA_ARCH_TYPE *)dmaResource;
    if (!ft32DmaTrySetCurrDataCounter(dmaRef, bbPort->outputDmaInit.BlockTransSize)) {
        dbgPinLo(1);
        return;
    }
    bbCommitDMADescriptor(dmaRef, &bbPort->outputDmaInit);
    xDMA_ITConfig(dmaRef, DMA_IT_TFR | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR, DISABLE);
    xDMA_ITConfig(dmaRef, DMA_IT_TFR | DMA_IT_ERR, ENABLE);
#endif

    bbPort->gpio->BSRR = bbPort->gpioIdleBSRR;
    ATOMIC_BLOCK(NVIC_PRIO_TIMER) {
        MODIFY_REG(bbPort->gpio->MODER, bbPort->gpioModeMask, bbPort->gpioModeOutput);
    }
    ((TIM_TypeDef *)bbPort->timhw->tim)->ARR = bbPort->outputARR;
    bbPublishDirection(bbPort, DSHOT_BITBANG_DIRECTION_OUTPUT);

    dbgPinLo(1);
}

#ifdef USE_DSHOT_TELEMETRY
void bbSwitchToInput(bbPort_t *bbPort)
{
    dbgPinHi(1);
    bbPublishDirection(bbPort, UINT8_MAX);

    dmaResource_t *dmaResource = bbPort->dmaResource;
#ifdef USE_DMA_REGISTER_CACHE
    if (!bbLoadDMARegs(dmaResource, &bbPort->dmaRegInput, bbPort->portInputCount)) {
        dbgPinLo(1);
        return;
    }
#else
    DMA_ARCH_TYPE *dmaRef = (DMA_ARCH_TYPE *)dmaResource;
    if (!ft32DmaTrySetCurrDataCounter(dmaRef, bbPort->inputDmaInit.BlockTransSize)) {
        dbgPinLo(1);
        return;
    }
    bbCommitDMADescriptor(dmaRef, &bbPort->inputDmaInit);
    xDMA_ITConfig(dmaRef, DMA_IT_TFR | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR, DISABLE);
    xDMA_ITConfig(dmaRef, DMA_IT_TFR | DMA_IT_ERR, ENABLE);
#endif

    ((TIM_TypeDef *)bbPort->timhw->tim)->CNT = 0;
    ((TIM_TypeDef *)bbPort->timhw->tim)->ARR = bbPort->inputARR;

    bbDMA_Cmd(bbPort, ENABLE);
    if (!ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)dmaResource)) {
        ft32DmaRequestDisable((DMA_ARCH_TYPE *)dmaResource);
        dbgPinLo(1);
        return;
    }

    ATOMIC_BLOCK(NVIC_PRIO_TIMER) {
        MODIFY_REG(bbPort->gpio->MODER, bbPort->gpioModeMask, bbPort->gpioModeInput);
    }
    bbPublishDirection(bbPort, DSHOT_BITBANG_DIRECTION_INPUT);
    dbgPinLo(1);
}
#endif

static bool bbTryDMAPreconfigure(bbPort_t *bbPort, uint8_t direction)
{
    DMA_InitTypeDef *dmainit = (direction == DSHOT_BITBANG_DIRECTION_OUTPUT) ? &bbPort->outputDmaInit : &bbPort->inputDmaInit;
    DMA_StructInit(dmainit);
    dmainit->FIFOMode = ENABLE;
    dmainit->ReloadDst = DISABLE;
    dmainit->ReloadSrc = DISABLE;

    const uint32_t hardwareInterface = ft32DmaGetHardwareInterface(bbPort->dmaResource);

    if (direction == DSHOT_BITBANG_DIRECTION_OUTPUT) {
        dmainit->SrcHardwareInterface = DMA_SRC_HARDWARE_INTERFACE_0;
        dmainit->DstHardwareInterface = hardwareInterface;
        dmainit->Priority = DMA_CH_PRIORITY_6;
        dmainit->SrcAddress = (uint32_t)bbPort->portOutputBuffer;
        dmainit->DstAddress = (uint32_t)&bbPort->gpio->BSRR;
        dmainit->BlockTransSize = bbPort->portOutputCount;
        dmainit->SrcDstMasterSel = DMA_SRCMASTER1_DSTMASTER2;
        dmainit->TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA;
        dmainit->SrcAddrMode = DMA_SRC_ADDRMODE_INC;
        dmainit->DstAddrMode = DMA_DST_ADDRMODE_HOLD;
        dmainit->SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_32BITS;
        dmainit->DstTransferWidth = DMA_DST_TRANSFERWIDTH_32BITS;
        dmainit->SrcHsIfPol = DMA_SRCHSIFPOL_HIGH;
        dmainit->DstHsIfPol = DMA_DSTHSIFPOL_LOW;
        dmainit->SrcHsSel = DMA_SRCHSSEL_SOFTWARE;
        dmainit->DstHsSel = DMA_DSTHSSEL_HARDWARE;
        dmainit->SrcHsIfPeriphSel = 0U;
        dmainit->DstHsIfPeriphSel = bbPort->dmaChannel;
    } else {
        dmainit->SrcHardwareInterface = hardwareInterface;
        dmainit->DstHardwareInterface = DMA_DST_HARDWARE_INTERFACE_0;
        dmainit->Priority = DMA_CH_PRIORITY_7;
        dmainit->SrcAddress = (uint32_t)&bbPort->gpio->IDR;
        dmainit->DstAddress = (uint32_t)bbPort->portInputBuffer;
        dmainit->BlockTransSize = bbPort->portInputCount;
        dmainit->SrcDstMasterSel = DMA_SRCMASTER2_DSTMASTER1;
        dmainit->TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA;
        dmainit->SrcAddrMode = DMA_SRC_ADDRMODE_HOLD;
        dmainit->DstAddrMode = DMA_DST_ADDRMODE_INC;
        dmainit->SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_16BITS;
        dmainit->DstTransferWidth = DMA_DST_TRANSFERWIDTH_16BITS;
        dmainit->SrcHsIfPol = DMA_SRCHSIFPOL_LOW;
        dmainit->DstHsIfPol = DMA_DSTHSIFPOL_HIGH;
        dmainit->SrcHsSel = DMA_SRCHSSEL_HARDWARE;
        dmainit->DstHsSel = DMA_DSTHSSEL_SOFTWARE;
        dmainit->SrcHsIfPeriphSel = bbPort->dmaChannel;
        dmainit->DstHsIfPeriphSel = 0U;
    }

    DMA_ARCH_TYPE *dmaRef = (DMA_ARCH_TYPE *)bbPort->dmaResource;
    if (!ft32DmaTrySetCurrDataCounter(dmaRef, dmainit->BlockTransSize)) {
        return false;
    }

    bbCommitDMADescriptor(dmaRef, dmainit);
    xDMA_ITConfig(dmaRef, DMA_IT_TFR | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR, DISABLE);
    xDMA_ITConfig(dmaRef, DMA_IT_TFR | DMA_IT_ERR, ENABLE);

#ifdef USE_DMA_REGISTER_CACHE
    dmaRegCache_t *dmaRegCache = (direction == DSHOT_BITBANG_DIRECTION_OUTPUT) ? &bbPort->dmaRegOutput : &bbPort->dmaRegInput;
    if (!bbSaveDMARegs(bbPort->dmaResource, dmaRegCache)) {
        memset(dmaRegCache, 0, sizeof(*dmaRegCache));
        return false;
    }
#endif
    return true;
}

void bbDMAPreconfigure(bbPort_t *bbPort, uint8_t direction)
{
#ifdef USE_DMA_REGISTER_CACHE
    dmaRegCache_t *dmaRegCache = (direction == DSHOT_BITBANG_DIRECTION_OUTPUT) ? &bbPort->dmaRegOutput : &bbPort->dmaRegInput;
    memset(dmaRegCache, 0, sizeof(*dmaRegCache));
#endif
    (void)bbTryDMAPreconfigure(bbPort, direction);
}

#ifdef UNIT_TEST
bool ft32DshotTestBitbangTryPreconfigure(bbPort_t *bbPort, uint8_t direction)
{
    return bbTryDMAPreconfigure(bbPort, direction);
}
#endif

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
    if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)bbPort->dmaResource)) {
        return;
    }
    xDMA_ITConfig(bbPort->dmaResource, DMA_IT_TFR | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR, DISABLE);
    xDMA_ITConfig(bbPort->dmaResource, DMA_IT_TFR | DMA_IT_ERR, ENABLE);
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
