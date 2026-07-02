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

#ifdef USE_LED_STRIP

#include "build/debug.h"

#include "common/color.h"

#include "drivers/dma.h"
#include "drivers/dma_reqmap.h"
#include "platform/dma.h"
#include "drivers/io.h"
#include "drivers/nvic.h"
#include "platform/rcc.h"
#include "drivers/timer.h"

#include "platform/timer.h"

#include "drivers/light_ws2811strip.h"
#include "platform/light_ws2811strip_stm32.h"

static IO_t ws2811IO = IO_NONE;
static dmaResource_t *dmaRef = NULL;
static TIM_TypeDef *timer = NULL;

static void WS2811_DMA_IRQHandler(dmaChannelDescriptor_t *descriptor)
{
#if defined(USE_WS2811_SINGLE_COLOUR)
    static uint32_t counter = 0;
#endif

    if (DMA_GET_FLAG_STATUS(descriptor, DMA_IT_TCIF)) {
#if defined(USE_WS2811_SINGLE_COLOUR)
        counter++;
        if (counter == WS2811_LED_STRIP_LENGTH) {
            // Output low for 50us delay
            memset(ledStripDMABuffer, 0, sizeof(ledStripDMABuffer));
        } else if (counter == (WS2811_LED_STRIP_LENGTH + WS2811_DELAY_ITERATIONS)) {
            counter = 0;
            ws2811LedDataTransferInProgress = false;
            xDMA_Cmd(descriptor->ref, DISABLE);
        }
#else
        ws2811LedDataTransferInProgress = false;
        xDMA_Cmd(descriptor->ref, DISABLE);
#endif

        DMA_CLEAR_FLAG(descriptor, DMA_IT_TCIF);
    }
}

bool ws2811LedStripHardwareInit(void)
{
    if (!ledStripIoTag) {
        return false;
    }

    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;
    DMA_InitTypeDef DMA_InitStructure;

    const timerHardware_t *timerHardware = timerAllocate(ledStripIoTag, OWNER_LED_STRIP, 0);

    if (timerHardware == NULL) {
        return false;
    }

    timer = (TIM_TypeDef *)timerHardware->tim;

#if defined(USE_DMA_SPEC)
    const dmaChannelSpec_t *dmaSpec = dmaGetChannelSpecByTimer(timerHardware);

    if (dmaSpec == NULL) {
        return false;
    }

    dmaRef = dmaSpec->ref;
#else
    dmaRef = timerHardware->dmaRef;
#endif

    if (dmaRef == NULL || !dmaAllocate(dmaGetIdentifier(dmaRef), OWNER_LED_STRIP, 0)) {
        return false;
    }

    ws2811IO = IOGetByTag(ledStripIoTag);
    IOInit(ws2811IO, OWNER_LED_STRIP, 0);
    IOConfigGPIOAF(ws2811IO, IOCFG_AF_PP_UP, timerHardware->alternateFunction);

    RCC_ClockCmd(timerRCC(timerHardware->tim), ENABLE);

    // Stop timer
    TIM_Cmd(timer, DISABLE);

    /* Compute the prescaler value */
    uint16_t prescaler = timerGetPrescalerByDesiredMhz(timerHardware->tim, WS2811_TIMER_MHZ);
    uint16_t period = timerGetPeriodByPrescaler(timerHardware->tim, prescaler, WS2811_CARRIER_HZ);

    BIT_COMPARE_1 = period / 3 * 2;
    BIT_COMPARE_0 = period / 3;

    /* Time base configuration */
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = period;
    TIM_TimeBaseStructure.TIM_Prescaler = prescaler;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(timer, &TIM_TimeBaseStructure);

    /* PWM1 Mode configuration */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;

    if (timerHardware->output & TIMER_OUTPUT_N_CHANNEL) {
        TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
        TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
        TIM_OCInitStructure.TIM_OCNPolarity = (timerHardware->output & TIMER_OUTPUT_INVERTED) ? TIM_OCNPolarity_Low : TIM_OCNPolarity_High;
    } else {
        TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
        TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
        TIM_OCInitStructure.TIM_OCPolarity = (timerHardware->output & TIMER_OUTPUT_INVERTED) ? TIM_OCPolarity_Low : TIM_OCPolarity_High;
    }

    TIM_OCInitStructure.TIM_Pulse = 0;

    timerOCInit(timer, timerHardware->channel, &TIM_OCInitStructure);
    timerOCPreloadConfig(timer, timerHardware->channel, TIM_OCPreload_Enable);

    TIM_CtrlPWMOutputs(timer, ENABLE);
    TIM_ARRPreloadConfig(timer, ENABLE);

    if (timerHardware->output & TIMER_OUTPUT_N_CHANNEL) {
        TIM_CCxNCmd(timer, timerHardware->channel, TIM_CCxN_Enable);
    } else {
        TIM_CCxCmd(timer, timerHardware->channel, TIM_CCx_Enable);
    }

    TIM_Cmd(timer, ENABLE);

    dmaEnable(dmaGetIdentifier(dmaRef));
    dmaSetHandler(dmaGetIdentifier(dmaRef), WS2811_DMA_IRQHandler, NVIC_PRIO_WS2811_DMA, 0);

    xDMA_DeInit(dmaRef);

    /* configure DMA */
    xDMA_Cmd(dmaRef, DISABLE);
    xDMA_DeInit(dmaRef);
    DMA_StructInit(&DMA_InitStructure);

    // FT32 DesignWare DMA configuration
    DMA_InitStructure.SrcAddress = (uint32_t)ledStripDMABuffer;
    DMA_InitStructure.DstAddress = (uint32_t)timerCCR(timerHardware->tim, timerHardware->channel);
    DMA_InitStructure.SrcAddrMode = DMA_SRC_ADDRMODE_INC;
    DMA_InitStructure.DstAddrMode = DMA_DST_ADDRMODE_HOLD;
    DMA_InitStructure.SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_32BITS;
    DMA_InitStructure.DstTransferWidth = DMA_DST_TRANSFERWIDTH_32BITS;
    DMA_InitStructure.BlockTransSize = WS2811_DMA_BUFFER_SIZE;
    DMA_InitStructure.Priority = DMA_CH_PRIORITY_7;
    DMA_InitStructure.TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA;

#if defined(USE_DMA_SPEC)
    ft32DmaSetDstRequest(&DMA_InitStructure, dmaRef, dmaSpec->channel);
#else
    ft32DmaSetDstRequest(&DMA_InitStructure, dmaRef, timerHardware->dmaChannel);
#endif

#if defined(USE_WS2811_SINGLE_COLOUR)
    DMA_InitStructure.ReloadSrc = ENABLE;
    DMA_InitStructure.ReloadDst = DISABLE;
#else
    DMA_InitStructure.ReloadSrc = DISABLE;
    DMA_InitStructure.ReloadDst = DISABLE;
#endif

    xDMA_Init(dmaRef, &DMA_InitStructure);
    TIM_DMACmd(timer, timerDmaSource(timerHardware->channel), ENABLE);
    xDMA_ITConfig(dmaRef, DMA_IT_TFR, ENABLE);

    return true;
}

void ws2811LedStripStartTransfer(void)
{
    xDMA_SetCurrDataCounter(dmaRef, WS2811_DMA_BUFFER_SIZE);
    TIM_SetCounter(timer, 0);
    TIM_Cmd(timer, ENABLE);
    xDMA_Cmd(dmaRef, ENABLE);
}

#endif