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

#define FT32_DSHOT_DIRECTION_RECOVERY_PENDING UINT8_MAX

static void pwmDshotDirectionRecoveryBarrier(void)
{
#ifdef UNIT_TEST
    __asm__ volatile ("" ::: "memory");
#else
    __DMB();
#endif
}

static bool pwmDshotDirectionRecoveryIsPending(const motorDmaOutput_t *motor)
{
    return *(const volatile uint8_t *)&motor->dmaInputLen == FT32_DSHOT_DIRECTION_RECOVERY_PENDING;
}

static void pwmDshotDirectionRecoveryMarkPending(motorDmaOutput_t *motor)
{
    *(volatile uint8_t *)&motor->dmaInputLen = FT32_DSHOT_DIRECTION_RECOVERY_PENDING;
    pwmDshotDirectionRecoveryBarrier();
    motor->isInput = false;
}

static void pwmDshotDirectionRecoveryClear(motorDmaOutput_t *motor)
{
    pwmDshotDirectionRecoveryBarrier();
    *(volatile uint8_t *)&motor->dmaInputLen = 0U;
}

void dshotEnableChannels(unsigned motorCount)
{
    for (unsigned i = 0; i < motorCount; i++) {
        if (dmaMotors[i].isInput || pwmDshotDirectionRecoveryIsPending(&dmaMotors[i])) {
            continue;
        }
        if (dmaMotors[i].output & TIMER_OUTPUT_N_CHANNEL) {
            TIM_CCxNCmd((TIM_TypeDef *)dmaMotors[i].timerHardware->tim, dmaMotors[i].timerHardware->channel, TIM_CCxN_Enable);
        } else {
            TIM_CCxCmd((TIM_TypeDef *)dmaMotors[i].timerHardware->tim, dmaMotors[i].timerHardware->channel, TIM_CCx_Enable);
        }
    }
}

#endif

static void pwmDshotCommitDmaDescriptor(DMA_ARCH_TYPE *dmaRef, DMA_InitTypeDef *descriptor)
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

static DMA_InitTypeDef pwmDshotOutputDmaDescriptor(const motorDmaOutput_t *motor, const DMA_InitTypeDef *canonical)
{
    DMA_InitTypeDef descriptor = *canonical;
    const uint32_t hardwareInterface = ft32DmaGetHardwareInterface(motor->dmaRef);

    descriptor.SrcDstMasterSel = DMA_SRCMASTER1_DSTMASTER2;
    descriptor.TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA;
    descriptor.SrcAddrMode = DMA_SRC_ADDRMODE_INC;
    descriptor.DstAddrMode = DMA_DST_ADDRMODE_HOLD;
    descriptor.SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_32BITS;
    descriptor.DstTransferWidth = DMA_DST_TRANSFERWIDTH_32BITS;
    descriptor.SrcHardwareInterface = 0U;
    descriptor.DstHardwareInterface = hardwareInterface;
    descriptor.SrcHsSel = DMA_SRCHSSEL_SOFTWARE;
    descriptor.DstHsSel = DMA_DSTHSSEL_HARDWARE;
    descriptor.SrcHsIfPol = DMA_SRCHSIFPOL_HIGH;
    descriptor.DstHsIfPol = DMA_DSTHSIFPOL_LOW;
    descriptor.SrcHsIfPeriphSel = 0U;
    descriptor.DstHsIfPeriphSel = canonical->DstHsIfPeriphSel;

    return descriptor;
}

static bool pwmDshotTryLoadDmaDescriptor(motorDmaOutput_t *motor, DMA_InitTypeDef *descriptor)
{
    DMA_ARCH_TYPE *dmaRef = (DMA_ARCH_TYPE *)motor->dmaRef;

    if (!ft32DmaTrySetCurrDataCounter(dmaRef, descriptor->BlockTransSize)) {
        return false;
    }

    pwmDshotCommitDmaDescriptor(dmaRef, descriptor);
    xDMA_ITConfig(dmaRef, DMA_IT_TFR | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR, DISABLE);
    xDMA_ITConfig(dmaRef, DMA_IT_TFR | DMA_IT_ERR, ENABLE);
    return true;
}

static bool pwmDshotTrySetDirectionOutputInternal(
    motorDmaOutput_t * const motor,
    TIM_OCInitTypeDef *pOcInit,
    const DMA_InitTypeDef *pDmaInit)
{
    const timerHardware_t * const timerHardware = motor->timerHardware;
    TIM_TypeDef *timer = (TIM_TypeDef *)timerHardware->tim;

#ifdef USE_DSHOT_DMAR
    if (useBurstDshot) {
        TIM_DMACmd(timer, TIM_DMA_Update, DISABLE);
    } else
#endif
    {
        TIM_DMACmd(timer, motor->timerDmaSource, DISABLE);
    }

    DMA_InitTypeDef descriptor = pwmDshotOutputDmaDescriptor(motor, pDmaInit);
    if (!pwmDshotTryLoadDmaDescriptor(motor, &descriptor)) {
        return false;
    }

    timerOCPreloadConfig(timer, timerHardware->channel, TIM_OCPreload_Disable);
    timerOCInit(timer, timerHardware->channel, pOcInit);
    timerOCPreloadConfig(timer, timerHardware->channel, TIM_OCPreload_Enable);

#ifdef USE_DSHOT_TELEMETRY
    motor->isInput = false;
    pwmDshotDirectionRecoveryClear(motor);
#endif
    return true;
}

FAST_CODE void pwmDshotSetDirectionOutput(
    motorDmaOutput_t * const motor
#ifndef USE_DSHOT_TELEMETRY
    ,TIM_OCInitTypeDef *pOcInit, DMA_InitTypeDef* pDmaInit
#endif
)
{
#ifdef USE_DSHOT_TELEMETRY
    const bool wasInput = motor->isInput;
    if (!pwmDshotTrySetDirectionOutputInternal(motor, &motor->ocInitStruct, &motor->dmaInitStruct) && wasInput) {
        pwmDshotDirectionRecoveryMarkPending(motor);
    }
#else
    (void)pwmDshotTrySetDirectionOutputInternal(motor, pOcInit, pDmaInit);
#endif
}

#ifdef USE_DSHOT_TELEMETRY
static DMA_InitTypeDef pwmDshotInputDmaDescriptor(const motorDmaOutput_t *motor)
{
    const DMA_InitTypeDef *output = &motor->dmaInitStruct;
    DMA_InitTypeDef descriptor = *output;
    const uint32_t hardwareInterface = ft32DmaGetHardwareInterface(motor->dmaRef);

    descriptor.SrcAddress = output->DstAddress;
    descriptor.DstAddress = output->SrcAddress;
    descriptor.BlockTransSize = GCR_TELEMETRY_INPUT_LEN;
    descriptor.SrcDstMasterSel = DMA_SRCMASTER2_DSTMASTER1;
    descriptor.TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA;
    descriptor.SrcAddrMode = DMA_SRC_ADDRMODE_HOLD;
    descriptor.DstAddrMode = DMA_DST_ADDRMODE_INC;
    descriptor.SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_32BITS;
    descriptor.DstTransferWidth = DMA_DST_TRANSFERWIDTH_32BITS;
    descriptor.SrcHardwareInterface = hardwareInterface;
    descriptor.DstHardwareInterface = 0U;
    descriptor.SrcHsSel = DMA_SRCHSSEL_HARDWARE;
    descriptor.DstHsSel = DMA_DSTHSSEL_SOFTWARE;
    descriptor.SrcHsIfPol = DMA_SRCHSIFPOL_LOW;
    descriptor.DstHsIfPol = DMA_DSTHSIFPOL_HIGH;
    descriptor.SrcHsIfPeriphSel = output->DstHsIfPeriphSel;
    descriptor.DstHsIfPeriphSel = 0U;

    return descriptor;
}

FAST_CODE
static bool pwmDshotTrySetDirectionInputStopped(motorDmaOutput_t * const motor)
{
    const timerHardware_t * const timerHardware = motor->timerHardware;
    TIM_TypeDef *timer = (TIM_TypeDef *)timerHardware->tim;
    DMA_InitTypeDef descriptor = pwmDshotInputDmaDescriptor(motor);

    if (!pwmDshotTryLoadDmaDescriptor(motor, &descriptor)) {
        return false;
    }

    TIM_ARRPreloadConfig(timer, ENABLE);
    timer->ARR = 0xffffffff;
    TIM_ICInit(timer, &motor->icInitStruct);
    return true;
}
#endif

static bool pwmDshotTryRearmBurst(DMA_ARCH_TYPE *dmaRef, uint16_t count, uint32_t srcAddress)
{
    if (!ft32DmaTrySetCurrDataCounter(dmaRef, count)) {
        return false;
    }
    DMA_SetSrcAddress(dmaRef, srcAddress);
    xDMA_Cmd(dmaRef, ENABLE);
    if (!ft32DmaIsChannelEnabled(dmaRef)) {
        ft32DmaRequestDisable(dmaRef);
        (void)ft32DmaIsChannelEnabled(dmaRef);
        return false;
    }
    return true;
}

static void pwmDshotDmaArmBarrier(void)
{
#ifdef UNIT_TEST
    __asm__ volatile ("" ::: "memory");
#else
    __DMB();
#endif
}

static void pwmDshotDisableTimerChannels(const motorDmaTimer_t *motorTimer, uint16_t timerDmaSources)
{
    for (unsigned i = 0; i < dshotMotorCount; i++) {
        motorDmaOutput_t *motor = &dmaMotors[i];
        if (!motor->configured || motor->timer != motorTimer || !(timerDmaSources & motor->timerDmaSource)) {
            continue;
        }

        DMA_ARCH_TYPE *dmaRef = (DMA_ARCH_TYPE *)motor->dmaRef;
        ft32DmaRequestDisable(dmaRef);
        (void)ft32DmaIsChannelEnabled(dmaRef);
    }
}

static uint16_t pwmDshotConfiguredTimerSources(const motorDmaTimer_t *motorTimer)
{
    uint16_t timerDmaSources = 0U;

    for (unsigned i = 0; i < dshotMotorCount; i++) {
        const motorDmaOutput_t *motor = &dmaMotors[i];
        if (motor->configured && motor->timer == motorTimer) {
            timerDmaSources |= motor->timerDmaSource;
        }
    }
    return timerDmaSources;
}

static bool pwmDshotTryArmTimerChannels(const motorDmaTimer_t *motorTimer, uint16_t timerDmaSources)
{
    uint16_t armedSources = 0U;
    bool channelsReady = true;

    pwmDshotDmaArmBarrier();
    for (unsigned i = 0; i < dshotMotorCount; i++) {
        motorDmaOutput_t *motor = &dmaMotors[i];
        if (!motor->configured || motor->timer != motorTimer || !(timerDmaSources & motor->timerDmaSource)) {
            continue;
        }

#ifdef USE_DSHOT_TELEMETRY
        if (motor->isInput || pwmDshotDirectionRecoveryIsPending(motor)) {
            channelsReady = false;
            continue;
        }
#endif

        DMA_ARCH_TYPE *dmaRef = (DMA_ARCH_TYPE *)motor->dmaRef;
        if (ft32DmaIsChannelEnabled(dmaRef)) {
            channelsReady = false;
            continue;
        }

        xDMA_Cmd(dmaRef, ENABLE);
        if (!ft32DmaIsChannelEnabled(dmaRef)) {
            channelsReady = false;
            continue;
        }
        armedSources |= motor->timerDmaSource;
    }

    if (!channelsReady || armedSources != timerDmaSources) {
        pwmDshotDisableTimerChannels(motorTimer, timerDmaSources);
        return false;
    }
    return true;
}

#if defined(USE_DSHOT_DMAR) && defined(USE_DSHOT_TELEMETRY)
static bool pwmDshotBurstTimerRecoveryIsPending(const motorDmaTimer_t *motorTimer)
{
    for (unsigned i = 0; i < dshotMotorCount; i++) {
        if (dmaMotors[i].timer == motorTimer &&
            (dmaMotors[i].isInput || pwmDshotDirectionRecoveryIsPending(&dmaMotors[i]))) {
            return true;
        }
    }
    return false;
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
            DMA_ARCH_TYPE *dmaRef = (DMA_ARCH_TYPE *)dmaMotorTimers[i].dmaBurstRef;
            TIM_DMACmd(tim, TIM_DMA_Update, DISABLE);
            if (dmaMotorTimers[i].dmaBurstLength == UINT16_MAX) {
                ft32DmaRequestDisable(dmaRef);
                (void)ft32DmaIsChannelEnabled(dmaRef);
                dmaMotorTimers[i].dmaBurstLength = 0U;
                continue;
            }
#ifdef USE_DSHOT_TELEMETRY
            if (pwmDshotBurstTimerRecoveryIsPending(&dmaMotorTimers[i])) {
                ft32DmaRequestDisable(dmaRef);
                (void)ft32DmaIsChannelEnabled(dmaRef);
                dmaMotorTimers[i].dmaBurstLength = 0U;
                continue;
            }
#endif
            if (!pwmDshotTryRearmBurst(dmaRef,
                    dmaMotorTimers[i].dmaBurstLength,
                    (uint32_t)dmaMotorTimers[i].dmaBurstBuffer)) {
                dmaMotorTimers[i].dmaBurstLength = 0U;
                continue;
            }
            TIM_DMAConfig(tim, TIM_DMABase_CCR1, TIM_DMABurstLength_4Transfers);
            TIM_DMACmd(tim, TIM_DMA_Update, ENABLE);
        } else
#endif
        {
            TIM_TypeDef *tim = (TIM_TypeDef *)dmaMotorTimers[i].timer;
            const uint16_t timerDmaSources = dmaMotorTimers[i].timerDmaSources;
            dmaMotorTimers[i].timerDmaSources = 0U;
            if (!timerDmaSources) {
                continue;
            }
            if (timerDmaSources != pwmDshotConfiguredTimerSources(&dmaMotorTimers[i])) {
                pwmDshotDisableTimerChannels(&dmaMotorTimers[i], timerDmaSources);
                continue;
            }
            if (!pwmDshotTryArmTimerChannels(&dmaMotorTimers[i], timerDmaSources)) {
                continue;
            }
            TIM_ARRPreloadConfig(tim, DISABLE);
            tim->ARR = dmaMotorTimers[i].outputPeriod;
            TIM_ARRPreloadConfig(tim, ENABLE);
            TIM_SetCounter(tim, 0);
            TIM_DMACmd(tim, timerDmaSources, ENABLE);
        }
    }
}

FAST_CODE static void motor_DMA_IRQHandler(dmaChannelDescriptor_t *descriptor)
{
    const bool transferComplete = DMA_GET_FLAG_STATUS(descriptor, DMA_IT_TFR) != RESET;
    const bool transferError = DMA_GET_FLAG_STATUS(descriptor, DMA_IT_ERR) != RESET;

    if (!transferComplete && !transferError) {
        return;
    }

    motorDmaOutput_t * const motor = &dmaMotors[descriptor->userParam];
#ifdef USE_DSHOT_TELEMETRY
    const bool wasInput = motor->isInput;
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

    DMA_ARCH_TYPE *dmaRef = (DMA_ARCH_TYPE *)motor->dmaRef;
    ft32DmaRequestDisable(dmaRef);
    const bool channelStopped = !ft32DmaIsChannelEnabled(dmaRef);
    if (!channelStopped) {
        const uint8_t observedTerminalFlags =
            (transferComplete ? DMA_IT_TFR : 0U) |
            (transferError ? DMA_IT_ERR : 0U);
        DMA_CLEAR_FLAG(descriptor, observedTerminalFlags);
#ifdef USE_DSHOT_TELEMETRY
        if (wasInput || useDshotTelemetry) {
            pwmDshotDirectionRecoveryMarkPending(motor);
        }
#endif
        return;
    }

    DMA_CLEAR_FLAG(descriptor, DMA_IT_TFR | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR);

#ifdef USE_DSHOT_TELEMETRY
    bool inputReady = false;
    if (!wasInput && !pwmDshotDirectionRecoveryIsPending(motor) && transferComplete && !transferError && useDshotTelemetry) {
        inputReady = pwmDshotTrySetDirectionInputStopped(motor);
    }

    if (wasInput && transferError) {
        pwmDshotDirectionRecoveryMarkPending(motor);
    }

    if (inputReady) {
        xDMA_Cmd(motor->dmaRef, ENABLE);
        if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)motor->dmaRef)) {
            if (!inputStampUs) {
                inputStampUs = micros();
            }
            motor->isInput = true;
            TIM_DMACmd((TIM_TypeDef *)motor->timerHardware->tim, motor->timerDmaSource, ENABLE);
            dshotDMAHandlerCycleCounters.changeDirectionCompletedAt = getCycleCounter();
        } else {
            ft32DmaRequestDisable((DMA_ARCH_TYPE *)motor->dmaRef);
            pwmDshotDirectionRecoveryMarkPending(motor);
        }
    }
#endif
}

#ifdef UNIT_TEST
DMA_InitTypeDef ft32DshotTestOutputDescriptor(const motorDmaOutput_t *motor, const DMA_InitTypeDef *canonical)
{
    return pwmDshotOutputDmaDescriptor(motor, canonical);
}

#ifdef USE_DSHOT_TELEMETRY
DMA_InitTypeDef ft32DshotTestInputDescriptor(const motorDmaOutput_t *motor)
{
    return pwmDshotInputDmaDescriptor(motor);
}
#endif

bool ft32DshotTestLoadDescriptor(motorDmaOutput_t *motor, DMA_InitTypeDef *descriptor)
{
    return pwmDshotTryLoadDmaDescriptor(motor, descriptor);
}

bool ft32DshotTestSetDirectionOutput(motorDmaOutput_t *motor, TIM_OCInitTypeDef *ocInit, const DMA_InitTypeDef *dmaInit)
{
    return pwmDshotTrySetDirectionOutputInternal(motor, ocInit, dmaInit);
}

bool ft32DshotTestRearmBurst(DMA_ARCH_TYPE *dmaRef, uint16_t count, uint32_t srcAddress)
{
    return pwmDshotTryRearmBurst(dmaRef, count, srcAddress);
}

void ft32DshotTestMotorIrq(dmaChannelDescriptor_t *descriptor)
{
    motor_DMA_IRQHandler(descriptor);
}
#endif

bool pwmDshotMotorHardwareConfig(const timerHardware_t *timerHardware, uint8_t motorIndex, uint8_t reorderedMotorIndex, motorProtocolTypes_e pwmProtocolType, uint8_t output)
{
#ifdef USE_DSHOT_TELEMETRY
#define OCINIT motor->ocInitStruct
#else
    TIM_OCInitTypeDef ocInitStruct;
#define OCINIT ocInitStruct
#endif
#define DMAINIT motor->dmaInitStruct

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
    pwmDshotDirectionRecoveryClear(motor);
    motor->dshotTelemetryDeadtimeUs = DSHOT_TELEMETRY_DEADTIME_US + 1000000 *
        (16 * MOTOR_BITLENGTH) / getDshotHz(pwmProtocolType);
    motor->timer->outputPeriod = (pwmProtocolType == MOTOR_PROTOCOL_PROSHOT1000 ? (MOTOR_NIBBLE_LENGTH_PROSHOT) : MOTOR_BITLENGTH) - 1;
    if (!pwmDshotTrySetDirectionOutputInternal(motor, &OCINIT, &DMAINIT)) {
        return false;
    }
#else
    if (!pwmDshotTrySetDirectionOutputInternal(motor, &OCINIT, &DMAINIT)) {
        return false;
    }
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
