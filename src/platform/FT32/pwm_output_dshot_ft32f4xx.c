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
#include <math.h>

#include "platform.h"

#ifdef USE_DSHOT

#include "build/debug.h"

#include "drivers/dma.h"
#include "drivers/dma_reqmap.h"
#include "drivers/io.h"
#include "drivers/nvic.h"
#include "platform/rcc.h"
#include "drivers/time.h"
#include "drivers/timer.h"
#include "platform/timer.h"
#include "drivers/system.h"

#include "drivers/pwm_output.h"
#include "drivers/dshot.h"
#include "dshot_dpwm.h"
#include "drivers/dshot_command.h"
#include "pwm_output_dshot_shared.h"

#ifdef USE_DSHOT_TELEMETRY

void dshotEnableChannels(unsigned motorCount)
{
    for (unsigned i = 0; i < motorCount; i++) {
        if (dmaMotors[i].output & TIMER_OUTPUT_N_CHANNEL) {
            TIM_CCxNCmd((TIM_TypeDef *)dmaMotors[i].timerHardware->tim, dmaMotors[i].timerHardware->channel, TIM_CCxN_Enable);
        } else {
            TIM_CCxCmd((TIM_TypeDef *)dmaMotors[i].timerHardware->tim, dmaMotors[i].timerHardware->channel, TIM_CCx_Enable);
        }
    }
}

#endif

FAST_CODE void pwmDshotSetDirectionOutput(
    motorDmaOutput_t * const motor
#ifndef USE_DSHOT_TELEMETRY
    ,TIM_OCInitTypeDef *pOcInit, DMA_InitTypeDef* pDmaInit
#endif
)
{
#ifdef USE_DSHOT_TELEMETRY
    TIM_OCInitTypeDef* pOcInit = &motor->ocInitStruct;
    DMA_InitTypeDef* pDmaInit = &motor->dmaInitStruct;
#endif

    const timerHardware_t * const timerHardware = motor->timerHardware;
    TIM_TypeDef *timer = (TIM_TypeDef *)timerHardware->tim;

    dmaResource_t *dmaRef = motor->dmaRef;

    TIM_DMACmd(timer, motor->timerDmaSource, DISABLE);
    xDMA_Cmd(dmaRef, DISABLE);
    if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)dmaRef)) {
        return;
    }
    xDMA_DeInit(dmaRef);

#ifdef USE_DSHOT_TELEMETRY
    motor->isInput = false;
#endif
    timerOCPreloadConfig(timer, timerHardware->channel, TIM_OCPreload_Disable);
    timerOCInit(timer, timerHardware->channel, pOcInit);
    timerOCPreloadConfig(timer, timerHardware->channel, TIM_OCPreload_Enable);

#ifdef USE_DSHOT_DMAR
    if (useBurstDshot) {
        pDmaInit->TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA;
    } else
#endif
    {
#if defined(FT32F4)
        pDmaInit->TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA;
#endif
        ft32DmaSetDstRequest(pDmaInit, dmaRef, pDmaInit->DstHsIfPeriphSel);
    }

    xDMA_Init(dmaRef, pDmaInit);
    xDMA_ITConfig(dmaRef, DMA_IT_TFR, ENABLE);
    xDMA_ITConfig(dmaRef, DMA_IT_ERR, ENABLE);
}

#ifdef USE_DSHOT_TELEMETRY
FAST_CODE
static void pwmDshotSetDirectionInput(
    motorDmaOutput_t * const motor
)
{
    DMA_InitTypeDef* pDmaInit = &motor->dmaInitStruct;

    const timerHardware_t * const timerHardware = motor->timerHardware;
    TIM_TypeDef *timer = (TIM_TypeDef *)timerHardware->tim;

    dmaResource_t *dmaRef = motor->dmaRef;

    TIM_DMACmd(timer, motor->timerDmaSource, DISABLE);
    xDMA_Cmd(dmaRef, DISABLE);
    if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)dmaRef)) {
        return;
    }
    xDMA_DeInit(dmaRef);

    motor->isInput = true;
    if (!inputStampUs) {
        inputStampUs = micros();
    }
    TIM_ARRPreloadConfig(timer, ENABLE);
    timer->ARR = 0xffffffff;

    TIM_ICInit(timer, &motor->icInitStruct);

#if defined(FT32F4)
    const uint32_t dmaRequest = pDmaInit->DstHsIfPeriphSel;
    motor->dmaInitStruct.TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA;
    ft32DmaSetSrcRequest(pDmaInit, dmaRef, dmaRequest);
#endif

    xDMA_Init(dmaRef, pDmaInit);
}
#endif

void pwmCompleteDshotMotorUpdate(void)
{
    /* If there is a dshot command loaded up, time it correctly with motor update*/
    if (!dshotCommandQueueEmpty()) {
        if (!dshotCommandOutputIsEnabled(pwmMotorCount)) {
            return;
        }
    }

    for (int i = 0; i < dmaMotorTimerCount; i++) {
#ifdef USE_DSHOT_DMAR
        if (useBurstDshot) {
            TIM_TypeDef *tim = (TIM_TypeDef *)dmaMotorTimers[i].timer;
            TIM_DMACmd(tim, TIM_DMA_Update, DISABLE);
            if (!ft32DmaTrySetCurrDataCounter((DMA_ARCH_TYPE *)dmaMotorTimers[i].dmaBurstRef,
                    dmaMotorTimers[i].dmaBurstLength)) {
                continue;
            }
            xDMA_Cmd(dmaMotorTimers[i].dmaBurstRef, ENABLE);
            TIM_DMAConfig(tim, TIM_DMABase_CCR1, TIM_DMABurstLength_4Transfers);
            TIM_DMACmd(tim, TIM_DMA_Update, ENABLE);
        } else
#endif
        {
            TIM_TypeDef *tim = (TIM_TypeDef *)dmaMotorTimers[i].timer;
            TIM_ARRPreloadConfig(tim, DISABLE);
            tim->ARR = dmaMotorTimers[i].outputPeriod;
            TIM_ARRPreloadConfig(tim, ENABLE);
            TIM_SetCounter(tim, 0);
            TIM_DMACmd(tim, dmaMotorTimers[i].timerDmaSources, ENABLE);
            dmaMotorTimers[i].timerDmaSources = 0;
        }
    }
}

FAST_CODE static void motor_DMA_IRQHandler(dmaChannelDescriptor_t *descriptor)
{
    const bool transferComplete = DMA_GET_FLAG_STATUS(descriptor, DMA_IT_TFR) != RESET;
    const bool transferError = DMA_GET_FLAG_STATUS(descriptor, DMA_IT_ERR) != RESET;

    if (transferComplete || transferError) {
        motorDmaOutput_t * const motor = &dmaMotors[descriptor->userParam];
#ifdef USE_DSHOT_TELEMETRY
        dshotDMAHandlerCycleCounters.irqAt = getCycleCounter();
#endif
#ifdef USE_DSHOT_DMAR
        if (useBurstDshot) {
            TIM_DMACmd((TIM_TypeDef *)motor->timerHardware->tim, TIM_DMA_Update, DISABLE);
        } else
#endif
        {
            TIM_DMACmd((TIM_TypeDef *)motor->timerHardware->tim, motor->timerDmaSource, DISABLE);
        }

        ft32DmaRequestDisable((DMA_ARCH_TYPE *)motor->dmaRef);
        DMA_CLEAR_FLAG(descriptor, DMA_IT_TFR | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR);

#ifdef USE_DSHOT_TELEMETRY
        if (transferComplete && !transferError && useDshotTelemetry &&
            !ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)motor->dmaRef)) {
            pwmDshotSetDirectionInput(motor);
            if (ft32DmaTrySetCurrDataCounter((DMA_ARCH_TYPE *)motor->dmaRef, GCR_TELEMETRY_INPUT_LEN)) {
                xDMA_Cmd(motor->dmaRef, ENABLE);
                TIM_DMACmd((TIM_TypeDef *)motor->timerHardware->tim, motor->timerDmaSource, ENABLE);
                dshotDMAHandlerCycleCounters.changeDirectionCompletedAt = getCycleCounter();
            }
        }
#endif
    }
}

bool pwmDshotMotorHardwareConfig(const timerHardware_t *timerHardware, uint8_t motorIndex, uint8_t reorderedMotorIndex, motorProtocolTypes_e pwmProtocolType, uint8_t output)
{
#ifdef USE_DSHOT_TELEMETRY
#define OCINIT motor->ocInitStruct
#define DMAINIT motor->dmaInitStruct
#else
    TIM_OCInitTypeDef ocInitStruct;
    DMA_InitTypeDef   dmaInitStruct;
#define OCINIT ocInitStruct
#define DMAINIT dmaInitStruct
#endif

    dmaResource_t *dmaRef = NULL;
#if defined(FT32F4)
    __attribute__((unused)) uint32_t dmaChannel = 0;
#endif
#if defined(USE_DMA_SPEC)
    const dmaChannelSpec_t *dmaSpec = dmaGetChannelSpecByTimer(timerHardware);

    if (dmaSpec != NULL) {
        dmaRef = dmaSpec->ref;
#if defined(FT32F4)
        dmaChannel = dmaSpec->channel;
#endif
    }
#else
    dmaRef = timerHardware->dmaRef;
#if defined(FT32F4)
    dmaChannel = timerHardware->dmaChannel;
#endif
#endif

#ifdef USE_DSHOT_DMAR
    if (useBurstDshot) {
        const dmaChannelSpec_t *dmaTimUpSpec = dmaGetChannelSpecByPeripheral(DMA_PERIPH_TIMUP, timerGetTIMNumber(timerHardware), 0);
        if (dmaTimUpSpec) {
            dmaRef = dmaTimUpSpec->ref;
#if defined(FT32F4)
            dmaChannel = dmaTimUpSpec->channel;
#endif
        } else {
            dmaRef = NULL;
        }
    }
#endif

    if (dmaRef == NULL) {
        return false;
    }

    dmaIdentifier_e dmaIdentifier = dmaGetIdentifier(dmaRef);

    bool dmaIsConfigured = false;
#ifdef USE_DSHOT_DMAR
    if (useBurstDshot) {
        const resourceOwner_t *owner = dmaGetOwner(dmaIdentifier);
        if (owner->owner == OWNER_TIMUP && owner->index == timerGetTIMNumber(timerHardware)) {
            dmaIsConfigured = true;
        } else if (!dmaAllocate(dmaIdentifier, OWNER_TIMUP, timerGetTIMNumber(timerHardware))) {
            return false;
        }
    } else
#endif
    {
        if (!dmaAllocate(dmaIdentifier, OWNER_MOTOR, RESOURCE_INDEX(reorderedMotorIndex))) {
            return false;
        }
    }

    motorDmaOutput_t * const motor = &dmaMotors[motorIndex];
    TIM_TypeDef *timer = (TIM_TypeDef *)timerHardware->tim;

    // Boolean configureTimer is always true when different channels of the same timer are processed in sequence,
    // causing the timer and the associated DMA initialized more than once.
    // To fix this, getTimerIndex must be expanded to return if a new timer has been requested.
    // However, since the initialization is idempotent, it is left as is in a favor of flash space (for now).
    const uint8_t timerIndex = getTimerIndex(timer);
    const bool configureTimer = (timerIndex == dmaMotorTimerCount-1);

    motor->timer = &dmaMotorTimers[timerIndex];
    motor->index = motorIndex;
    motor->timerHardware = timerHardware;

    const IO_t motorIO = IOGetByTag(timerHardware->tag);

    uint8_t pupMode = 0;
    pupMode = (output & TIMER_OUTPUT_INVERTED) ? GPIO_PuPd_DOWN : GPIO_PuPd_UP;
#ifdef USE_DSHOT_TELEMETRY
    if (useDshotTelemetry) {
        output ^= TIMER_OUTPUT_INVERTED;
    }
#endif

    motor->iocfg = IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP, pupMode);
    IOConfigGPIOAF(motorIO, motor->iocfg, timerHardware->alternateFunction);

    if (configureTimer) {
        TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
        TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);

        RCC_ClockCmd(timerRCC(timerHardware->tim), ENABLE);
        TIM_Cmd(timer, DISABLE);

        TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)(lrintf((float) timerClock(timerHardware) / getDshotHz(pwmProtocolType) + 0.01f) - 1);
        TIM_TimeBaseStructure.TIM_Period = (pwmProtocolType == MOTOR_PROTOCOL_PROSHOT1000 ? (MOTOR_NIBBLE_LENGTH_PROSHOT) : MOTOR_BITLENGTH) - 1;
        TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
        TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
        TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
        TIM_TimeBaseInit(timer, &TIM_TimeBaseStructure);
    }

    TIM_OCStructInit(&OCINIT);
    OCINIT.TIM_OCMode = TIM_OCMode_PWM1;
    if (output & TIMER_OUTPUT_N_CHANNEL) {
        OCINIT.TIM_OutputNState = TIM_OutputNState_Enable;
        OCINIT.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
        OCINIT.TIM_OCNPolarity = (output & TIMER_OUTPUT_INVERTED) ? TIM_OCNPolarity_Low : TIM_OCNPolarity_High;
    } else {
        OCINIT.TIM_OutputState = TIM_OutputState_Enable;
        OCINIT.TIM_OCIdleState = TIM_OCIdleState_Set;
        OCINIT.TIM_OCPolarity =  (output & TIMER_OUTPUT_INVERTED) ? TIM_OCPolarity_Low : TIM_OCPolarity_High;
    }
    OCINIT.TIM_Pulse = 0;

#ifdef USE_DSHOT_TELEMETRY
    TIM_ICStructInit(&motor->icInitStruct);
    motor->icInitStruct.TIM_ICSelection = TIM_ICSelection_DirectTI;
    motor->icInitStruct.TIM_ICPolarity = TIM_ICPolarity_BothEdge;
    motor->icInitStruct.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    motor->icInitStruct.TIM_Channel = timerHardware->channel;
    motor->icInitStruct.TIM_ICFilter = 2;
#endif

#ifdef USE_DSHOT_DMAR
    if (useBurstDshot) {
        motor->timer->dmaBurstRef = dmaRef;
    } else
#endif
    {
        motor->timerDmaSource = timerDmaSource(timerHardware->channel);
        motor->timer->timerDmaSources &= ~motor->timerDmaSource;
    }

    xDMA_Cmd(dmaRef, DISABLE);
    xDMA_DeInit(dmaRef);

    if (!dmaIsConfigured) {
        dmaEnable(dmaIdentifier);
    }

    DMA_StructInit(&DMAINIT);

#ifdef USE_DSHOT_DMAR
    if (useBurstDshot) {
        motor->timer->dmaBurstBuffer = &dshotBurstDmaBuffer[timerIndex][0];
        DMAINIT.SrcAddress = (uint32_t)motor->timer->dmaBurstBuffer;
        DMAINIT.DstAddress = (uint32_t)&timer->DMAR;
    } else
#endif
    {
        motor->dmaBuffer = &dshotDmaBuffer[motorIndex][0];
        DMAINIT.SrcAddress = (uint32_t)motor->dmaBuffer;
        DMAINIT.DstAddress = (uint32_t)timerChCCR(timerHardware);
    }
    DMAINIT.BlockTransSize = (pwmProtocolType == MOTOR_PROTOCOL_PROSHOT1000) ? PROSHOT_DMA_BUFFER_SIZE : DSHOT_DMA_BUFFER_SIZE;
    DMAINIT.SrcDstMasterSel = DMA_SRCMASTER1_DSTMASTER2;
    DMAINIT.TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA;

    DMAINIT.SrcAddrMode = DMA_SRC_ADDRMODE_INC;
    DMAINIT.DstAddrMode = DMA_DST_ADDRMODE_HOLD;
    DMAINIT.SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_32BITS;
    DMAINIT.DstTransferWidth = DMA_DST_TRANSFERWIDTH_32BITS;
    ft32DmaSetDstRequest(&DMAINIT, dmaRef, dmaChannel);
    DMAINIT.Priority = DMA_CH_PRIORITY_6;
    DMAINIT.FIFOMode = ENABLE;
    DMAINIT.ReloadDst = DISABLE;
    DMAINIT.ReloadSrc = DISABLE;

    // XXX Consolidate common settings in the next refactor

    motor->dmaRef = dmaRef;

#ifdef USE_DSHOT_TELEMETRY
    motor->dshotTelemetryDeadtimeUs = DSHOT_TELEMETRY_DEADTIME_US + 1000000 *
        (16 * MOTOR_BITLENGTH) / getDshotHz(pwmProtocolType);
    motor->timer->outputPeriod = (pwmProtocolType == MOTOR_PROTOCOL_PROSHOT1000 ? (MOTOR_NIBBLE_LENGTH_PROSHOT) : MOTOR_BITLENGTH) - 1;
    pwmDshotSetDirectionOutput(motor);
#else
    pwmDshotSetDirectionOutput(motor, &OCINIT, &DMAINIT);
#endif

#ifdef USE_DSHOT_DMAR
    if (useBurstDshot) {
        if (!dmaIsConfigured) {
            dmaSetHandler(dmaIdentifier, motor_DMA_IRQHandler, NVIC_PRIO_DSHOT_DMA, motor->index);
        }
    } else
#endif
    {
        dmaSetHandler(dmaIdentifier, motor_DMA_IRQHandler, NVIC_PRIO_DSHOT_DMA, motor->index);
    }

    TIM_Cmd(timer, ENABLE);
    if (output & TIMER_OUTPUT_N_CHANNEL) {
        TIM_CCxNCmd(timer, timerHardware->channel, TIM_CCxN_Enable);
    } else {
        TIM_CCxCmd(timer, timerHardware->channel, TIM_CCx_Enable);
    }
    if (configureTimer) {
        TIM_ARRPreloadConfig(timer, ENABLE);
        TIM_CtrlPWMOutputs(timer, ENABLE);
        TIM_Cmd(timer, ENABLE);
    }
#ifdef USE_DSHOT_TELEMETRY
    if (useDshotTelemetry) {
        // avoid high line during startup to prevent bootloader activation
        *timerChCCR(timerHardware) = 0xffff;
    }
#endif
    motor->configured = true;

    return true;
}

#endif
