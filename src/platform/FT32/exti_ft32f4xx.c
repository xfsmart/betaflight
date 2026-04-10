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

#ifdef USE_EXTI

#include "drivers/nvic.h"
#include "drivers/io_impl.h"
#include "drivers/exti.h"

typedef struct {
    extiCallbackRec_t* handler;
} extiChannelRec_t;

extiChannelRec_t extiChannelRecs[16];

#define EXTI_IRQ_GROUPS 7
//                                      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
static const uint8_t extiGroups[16] = { 0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6 };
static uint8_t extiGroupPriority[EXTI_IRQ_GROUPS];

static const uint8_t extiGroupIRQn[EXTI_IRQ_GROUPS] = {
    EXTI0_IRQn,
    EXTI1_IRQn,
    EXTI2_IRQn,
    EXTI3_IRQn,
    EXTI4_IRQn,
    EXTI9_5_IRQn,
    EXTI15_10_IRQn
};

static uint32_t triggerLookupTable[] = {
    [BETAFLIGHT_EXTI_TRIGGER_RISING]    = EXTI_Trigger_Rising,
    [BETAFLIGHT_EXTI_TRIGGER_FALLING]   = EXTI_Trigger_Falling,
    [BETAFLIGHT_EXTI_TRIGGER_BOTH]      = EXTI_Trigger_Rising_Falling
};

#define EXTI_REG_IMR (EXTI->IMR)
#define EXTI_REG_PR  (EXTI->PR)
#define EXTI_REG_PR_CLEAR(mask) do { EXTI_REG_PR = (mask); } while(0)

void EXTIInit(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    memset(extiChannelRecs, 0, sizeof(extiChannelRecs));
    memset(extiGroupPriority, 0xff, sizeof(extiGroupPriority));
}

void EXTIHandlerInit(extiCallbackRec_t *self, extiHandlerCallback *fn)
{
    self->fn = fn;
}

void EXTIConfig(IO_t io, extiCallbackRec_t *cb, int irqPriority, ioConfig_t config, extiTrigger_t trigger)
{
    int chIdx = IO_GPIOPinIdx(io);

    if (chIdx < 0) {
        return;
    }

    int group = extiGroups[chIdx];

    extiChannelRec_t *rec = &extiChannelRecs[chIdx];
    rec->handler = cb;

    EXTIDisable(io);

    IOConfigGPIO(io, config);

    SYSCFG_EXTILineConfig(IO_GPIO_PortSource(io), IO_GPIO_PinSource(io));

    uint32_t extiLine = IO_EXTI_Line(io);

    EXTI_ClearFlag(extiLine);

    EXTI_InitTypeDef EXTI_InitStruct;
    EXTI_InitStruct.EXTI_Line = extiLine;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = triggerLookupTable[trigger];
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    if (extiGroupPriority[group] > irqPriority) {
        extiGroupPriority[group] = irqPriority;

        NVIC_InitTypeDef NVIC_InitStruct;
        NVIC_InitStruct.NVIC_IRQChannel = extiGroupIRQn[group];
        NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = NVIC_PRIORITY_BASE(irqPriority);
        NVIC_InitStruct.NVIC_IRQChannelSubPriority = NVIC_PRIORITY_SUB(irqPriority);
        NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStruct);
    }
}

void EXTIRelease(IO_t io)
{
    EXTIDisable(io);

    const int chIdx = IO_GPIOPinIdx(io);

    if (chIdx < 0) {
        return;
    }

    extiChannelRec_t *rec = &extiChannelRecs[chIdx];
    rec->handler = NULL;
}

void EXTIEnable(IO_t io)
{
    uint32_t extiLine = IO_EXTI_Line(io);

    if (!extiLine) {
        return;
    }

    EXTI_REG_IMR |= extiLine;
}

void EXTIDisable(IO_t io)
{
    uint32_t extiLine = IO_EXTI_Line(io);

    if (!extiLine) {
        return;
    }

    EXTI_REG_IMR &= ~extiLine;
    EXTI_REG_PR_CLEAR(extiLine);
}

#define EXTI_EVENT_MASK 0xFFFF

static void EXTI_IRQHandler(uint32_t mask)
{
    uint32_t exti_active = (EXTI_REG_IMR & EXTI_REG_PR) & mask;

    EXTI_REG_PR_CLEAR(exti_active);

    while (exti_active) {
        unsigned idx = 31 - __builtin_clz(exti_active);
        uint32_t mask = 1 << idx;
        if (extiChannelRecs[idx].handler) {
            extiChannelRecs[idx].handler->fn(extiChannelRecs[idx].handler);
        }
        exti_active &= ~mask;
    }
}

#define _EXTI_IRQ_HANDLER(name, mask)            \
    void name(void) {                            \
        EXTI_IRQHandler(mask & EXTI_EVENT_MASK); \
    }                                            \
    struct dummy                                 \
    /**/

_EXTI_IRQ_HANDLER(EXTI0_IRQHandler, 0x0001);
_EXTI_IRQ_HANDLER(EXTI1_IRQHandler, 0x0002);
_EXTI_IRQ_HANDLER(EXTI2_IRQHandler, 0x0004);
_EXTI_IRQ_HANDLER(EXTI3_IRQHandler, 0x0008);
_EXTI_IRQ_HANDLER(EXTI4_IRQHandler, 0x0010);
_EXTI_IRQ_HANDLER(EXTI9_5_IRQHandler, 0x03e0);
_EXTI_IRQ_HANDLER(EXTI15_10_IRQHandler, 0xfc00);

#endif
