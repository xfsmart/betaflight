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
        .reg = I2C1,
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
        .reg = I2C2,
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
        .reg = I2C3,
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

// Initialize I2C peripheral
// device: I2C device identifier (I2CDEV_1, I2CDEV_2, I2CDEV_3)
// Configures GPIO pins, enables clock, initializes peripheral and NVIC
void i2cInit(i2cDevice_e device)
{
    if (device == I2CINVALID) {
        return;
    }

    i2cDevice_t *pDev = &i2cDevice[device];

    const i2cHardware_t *hardware = pDev->hardware;
    const IO_t scl = pDev->scl;
    const IO_t sda = pDev->sda;

    if (!hardware || IOGetOwner(scl) || IOGetOwner(sda)) {
        return;
    }

    IOInit(scl, OWNER_I2C_SCL, RESOURCE_INDEX(device));
    IOInit(sda, OWNER_I2C_SDA, RESOURCE_INDEX(device));

    // Enable i2c RCC
    RCC_ClockCmd(hardware->rcc, ENABLE);

    i2cUnstick(scl, sda);

    // Init pins

    IOConfigGPIOAF(scl, pDev->pullUp ? IOCFG_I2C_PU : IOCFG_I2C, pDev->sclAF);
    IOConfigGPIOAF(sda, pDev->pullUp ? IOCFG_I2C_PU : IOCFG_I2C, pDev->sdaAF);

    // Init I2C peripheral
    i2c_handle_type  *pHandle = &pDev->handle;
    memset(pHandle, 0, sizeof(*pHandle));

    I2C_TypeDef *i2cx = (I2C_TypeDef *)pDev->hardware->reg;
    pHandle->i2cx = i2cx;

    // Get PCLK1 frequency using FT32F4 standard library
    RCC_ClocksTypeDef rcc_clocks;
    RCC_GetClocksFreq(&rcc_clocks);
    uint32_t i2cPclk = rcc_clocks.PCLK_Frequency;

    uint32_t I2Cx_Timing = i2cClockTIMINGR(i2cPclk, pDev->clockSpeed, 0);

    i2c_config(pHandle);

    // Initialize I2C using FT32F4 standard library
    I2C_InitTypeDef I2C_InitStruct;
    I2C_InitStruct.I2C_Timing = I2Cx_Timing;
    I2C_InitStruct.I2C_AnalogFilter = I2C_AnalogFilter_Enable;
    I2C_InitStruct.I2C_DigitalFilter = 0;
    I2C_InitStruct.I2C_NoStretchMode = I2C_NoStretch_Disable;
    I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStruct.I2C_OwnAddress1 = 0x00;
    I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    
    I2C_Init(i2cx, &I2C_InitStruct);

    // NVIC init
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = hardware->irqn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = NVIC_PRIORITY_BASE(NVIC_PRIO_I2C);
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = NVIC_PRIORITY_SUB(NVIC_PRIO_I2C);
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    I2C_Cmd(i2cx, ENABLE);
}

#endif
