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

#if defined(USE_I2C) && !defined(USE_SOFT_I2C)

#include "drivers/io.h"
#include "drivers/nvic.h"
#include "drivers/time.h"
#include "platform/rcc.h"

#include "drivers/bus_i2c.h"
#include "drivers/bus_i2c_impl.h"
#include "drivers/bus_i2c_timing.h"
#include "drivers/bus_i2c_utils.h"

#include "pg/pinio.h"

#define IOCFG_I2C_PU IO_CONFIG(GPIO_Mode_AF , GPIO_Speed_50MHz, GPIO_OType_OD, GPIO_PuPd_UP)
#define IOCFG_I2C    IO_CONFIG(GPIO_Mode_AF , GPIO_Speed_50MHz, GPIO_OType_OD, GPIO_PuPd_NOPULL)

const i2cHardware_t i2cHardware[I2CDEV_COUNT] = {
#ifdef USE_I2C_DEVICE_1
    {
        .device = I2CDEV_1,
        .reg = (i2cResource_t *)I2C1,
        .sclPins = {
            I2CPINDEF(PB6,  GPIO_AF_4),
            I2CPINDEF(PB8,  GPIO_AF_4),
        },
        .sdaPins = {
            I2CPINDEF(PB7,  GPIO_AF_4),
            I2CPINDEF(PB9,  GPIO_AF_4),
        },
        .rcc = RCC_APB1(I2C1),
        .irqn = I2C1_IRQn,
    },
#endif
#ifdef USE_I2C_DEVICE_2
    {
        .device = I2CDEV_2,
        .reg = (i2cResource_t *)I2C2,
        .sclPins = {
            I2CPINDEF(PB10, GPIO_AF_4),
        },
        .sdaPins = {
            I2CPINDEF(PB11, GPIO_AF_4),
        },
        .rcc = RCC_APB1(I2C2),
        .irqn = I2C2_IRQn,
    },
#endif
#ifdef USE_I2C_DEVICE_3
    {
        .device = I2CDEV_3,
        .reg = (i2cResource_t *)I2C3,
        .sclPins = {
            I2CPINDEF(PA8,  GPIO_AF_4),
        },
        .sdaPins = {
            I2CPINDEF(PC9,  GPIO_AF_4),
        },
        .rcc = RCC_APB1(I2C3),
        .irqn = I2C3_IRQn,
    },
#endif
};

i2cDevice_t i2cDevice[I2CDEV_COUNT];

// Static HAL handle storage for I2C devices
// Required because i2cDevice[].halHandle is a pointer (see bus_i2c_impl.h)
static struct i2cHalHandle_s i2cHalHandles[I2CDEV_COUNT];

static uint32_t i2cPclkFrequency(void)
{
    const uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    const uint32_t shift = ppre1 < 4 ? 0 : ppre1 - 3;
    return SystemCoreClock >> shift;
}

static void i2cSelectPclk(i2cDevice_e device)
{
    switch (device) {
    case I2CDEV_1:
        RCC_I2C1CLKConfig(RCC_I2C1CLK_PCLK);
        break;
    case I2CDEV_2:
        RCC_I2C2CLKConfig(RCC_I2C2CLK_PCLK);
        break;
    case I2CDEV_3:
        RCC_I2C3CLKConfig(RCC_I2C3CLK_PCLK);
        break;
    default:
        break;
    }
}

static bool i2cWaitBusReleased(IO_t scl, IO_t sda)
{
    const uint32_t start = microsISR();
    do {
        if (IORead(scl) && IORead(sda)) {
            return true;
        }
    } while ((int32_t)cmpTimeUs(microsISR(), start) < (int32_t)I2C_TIMEOUT_US);

    return false;
}

static bool i2cQuiesce(const i2cHardware_t *hardware, i2c_handle_type *pHandle, bool passiveRecovery)
{
    I2C_TypeDef *i2cx = pHandle->i2cx;

    NVIC_DisableIRQ((IRQn_Type)hardware->irqn);
    NVIC_ClearPendingIRQ((IRQn_Type)hardware->irqn);
    I2C_ITConfig(i2cx, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ADDRI | I2C_IT_TXI | I2C_IT_RXI, DISABLE);
    I2C_DMACmd(i2cx, I2C_DMAReq_Tx | I2C_DMAReq_Rx, DISABLE);

    if (passiveRecovery) {
        const uint32_t start = microsISR();
        while (I2C_GetFlagStatus(i2cx, I2C_FLAG_BUSY) == SET &&
            (int32_t)cmpTimeUs(microsISR(), start) < (int32_t)I2C_TIMEOUT_US) {
        }

        if (I2C_GetFlagStatus(i2cx, I2C_FLAG_BUSY) == SET) {
            return false;
        }
    } else if (I2C_GetFlagStatus(i2cx, I2C_FLAG_BUSY) == SET) {
        I2C_GenerateSTOP(i2cx, ENABLE);

        const uint32_t start = microsISR();
        while (I2C_GetFlagStatus(i2cx, I2C_FLAG_BUSY) == SET &&
            (int32_t)cmpTimeUs(microsISR(), start) < (int32_t)I2C_TIMEOUT_US) {
        }
    }

    I2C_ClearFlag(i2cx, I2C_FLAG_NACKF | I2C_FLAG_STOPF | I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_OVR | I2C_FLAG_PECERR | I2C_FLAG_TIMEOUT | I2C_FLAG_ALERT);
    I2C_Cmd(i2cx, DISABLE);
    I2C_DeInit(i2cx);
    return true;
}

static void i2cSetUnavailable(const i2cHardware_t *hardware, i2c_handle_type *pHandle,
    i2c_status_type terminalError, bool arbitrationLost)
{
    NVIC_DisableIRQ((IRQn_Type)hardware->irqn);
    NVIC_ClearPendingIRQ((IRQn_Type)hardware->irqn);
    I2C_Cmd((I2C_TypeDef *)hardware->reg, DISABLE);
    pHandle->i2cx = NULL;
    pHandle->state = I2C_END;
    pHandle->error_code = terminalError == I2C_OK ? I2C_ERR_TIMEOUT : terminalError;
    pHandle->arbitration_lost = arbitrationLost;
}

static bool i2cConfigure(i2cDevice_e device, bool recovery)
{
    if ((unsigned)device >= I2CDEV_COUNT) {
        return false;
    }

    i2cDevice_t *pDev = &i2cDevice[device];

    const i2cHardware_t *hardware = pDev->hardware;
    const IO_t scl = pDev->scl;
    const IO_t sda = pDev->sda;

    if (!hardware || hardware->device != device || !hardware->reg || pDev->reg != hardware->reg || !scl || !sda) {
        return false;
    }

    i2c_status_type terminalError = I2C_OK;
    bool arbitrationLost = false;
    bool passiveRecovery = false;
    i2c_handle_type *pHandle;
    I2C_TypeDef *i2cx = (I2C_TypeDef *)hardware->reg;

    if (recovery) {
        if (IOGetOwner(scl) != OWNER_I2C_SCL || IOGetOwner(sda) != OWNER_I2C_SDA || !pDev->halHandle) {
            return false;
        }

        pHandle = &pDev->halHandle->hal;
        if (pHandle->i2cx && pHandle->i2cx != i2cx) {
            return false;
        }

        RCC_ClockCmd(hardware->rcc, ENABLE);
        terminalError = pHandle->error_code;
        arbitrationLost = pHandle->arbitration_lost ||
            (pHandle->i2cx && I2C_GetFlagStatus(pHandle->i2cx, I2C_FLAG_ARLO) == SET);
        pHandle->arbitration_lost = arbitrationLost;
        passiveRecovery = arbitrationLost || !pHandle->master_started;

        if (pHandle->i2cx) {
            if (!i2cQuiesce(hardware, pHandle, passiveRecovery)) {
                i2cSetUnavailable(hardware, pHandle, terminalError, arbitrationLost);
                return false;
            }
        } else {
            NVIC_DisableIRQ((IRQn_Type)hardware->irqn);
            NVIC_ClearPendingIRQ((IRQn_Type)hardware->irqn);
        }
    } else {
        if (IOGetOwner(scl) || IOGetOwner(sda)) {
            return false;
        }

        RCC_ClockCmd(hardware->rcc, ENABLE);
        IOInit(scl, OWNER_I2C_SCL, RESOURCE_INDEX(device));
        IOInit(sda, OWNER_I2C_SDA, RESOURCE_INDEX(device));
        pDev->halHandle = &i2cHalHandles[device];
        pHandle = &pDev->halHandle->hal;
        memset(pHandle, 0, sizeof(*pHandle));
        pHandle->state = I2C_END;
        NVIC_DisableIRQ((IRQn_Type)hardware->irqn);
        NVIC_ClearPendingIRQ((IRQn_Type)hardware->irqn);
    }

    const bool busReleased = passiveRecovery ? i2cWaitBusReleased(scl, sda) : i2cUnstick(scl, sda);
    if (!busReleased) {
        i2cSetUnavailable(hardware, pHandle, terminalError, arbitrationLost);
        return false;
    }

    IOConfigGPIOAF(scl, pDev->pullUp ? IOCFG_I2C_PU : IOCFG_I2C, pDev->sclAF);
    IOConfigGPIOAF(sda, pDev->pullUp ? IOCFG_I2C_PU : IOCFG_I2C, pDev->sdaAF);

    memset(pHandle, 0, sizeof(*pHandle));
    pHandle->i2cx = i2cx;
    pHandle->state = I2C_END;
    pHandle->error_code = terminalError;
    pHandle->arbitration_lost = false;
    pHandle->master_started = false;

    i2cSelectPclk(device);
    const uint32_t i2cTiming = i2cClockTIMINGR(i2cPclkFrequency(), pDev->clockSpeed, 0);
    if (!i2cTiming) {
        i2cSetUnavailable(hardware, pHandle, terminalError, false);
        return false;
    }

    I2C_InitTypeDef I2C_InitStruct;
    I2C_InitStruct.I2C_Timing = i2cTiming;
    I2C_InitStruct.I2C_AnalogFilter = I2C_AnalogFilter_Enable;
    I2C_InitStruct.I2C_DigitalFilter = 0;
    I2C_InitStruct.I2C_NoStretchMode = I2C_NoStretch_Disable;
    I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStruct.I2C_OwnAddress1 = 0x00;
    I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(i2cx, &I2C_InitStruct);
    I2C_ClearFlag(i2cx, I2C_FLAG_NACKF | I2C_FLAG_STOPF | I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_OVR | I2C_FLAG_PECERR | I2C_FLAG_TIMEOUT | I2C_FLAG_ALERT);
    i2c_reset_ctrl2_register(pHandle);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = hardware->irqn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = NVIC_PRIORITY_BASE(NVIC_PRIO_I2C);
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = NVIC_PRIORITY_SUB(NVIC_PRIO_I2C);
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_ClearPendingIRQ((IRQn_Type)hardware->irqn);
    NVIC_Init(&NVIC_InitStructure);

    I2C_Cmd(i2cx, ENABLE);
    return true;
}

bool i2cReinit(i2cDevice_e device)
{
    return i2cConfigure(device, true);
}

void i2cInit(i2cDevice_e device)
{
    i2cConfigure(device, false);
}

#endif
