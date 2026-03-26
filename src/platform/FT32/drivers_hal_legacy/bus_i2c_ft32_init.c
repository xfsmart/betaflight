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

#include "ft32f4xx_hal_i2c.h"
#include "ft32f4xx_hal_gpio.h"
#include "ft32f4xx_rcc.h"

/* FT32F4 I2C pin configuration - Open-drain with AF */
#define IOCFG_I2C_PU IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_OD, GPIO_PuPd_UP)
#define IOCFG_I2C    IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_OD, GPIO_PuPd_NOPULL)

const i2cHardware_t i2cHardware[I2CDEV_COUNT] = {
#ifdef USE_I2C_DEVICE_1
    {
        .device = I2CDEV_1,
        .reg = I2C1,
        .sclPins = {
            I2CPINDEF(PB6, GPIO_AF_I2C1),
            I2CPINDEF(PB8, GPIO_AF_I2C1),
        },
        .sdaPins = {
            I2CPINDEF(PB7, GPIO_AF_I2C1),
            I2CPINDEF(PB9, GPIO_AF_I2C1),
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
            I2CPINDEF(PB10, GPIO_AF_I2C2),
        },
        .sdaPins = {
            I2CPINDEF(PB11, GPIO_AF_I2C2),
#if defined(FT32F405) || defined(FT32F407)
            I2CPINDEF(PB3,  GPIO_AF9_I2C2),
            I2CPINDEF(PB9,  GPIO_AF9_I2C2),
#endif
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
            I2CPINDEF(PA8, GPIO_AF_I2C3),
        },
        .sdaPins = {
            I2CPINDEF(PC9, GPIO_AF_I2C3),
#if defined(FT32F405) || defined(FT32F407)
            I2CPINDEF(PB4, GPIO_AF9_I2C3),
            I2CPINDEF(PB8, GPIO_AF9_I2C3),
#endif
        },
        .rcc = RCC_APB1(I2C3),
        .irqn = I2C3_IRQn,
    },
#endif
};

i2cDevice_t i2cDevice[I2CDEV_COUNT];

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

    /* Enable RCC clock for I2C peripheral */
    RCC_ClockCmd(hardware->rcc, ENABLE);

    /* Unstick I2C bus before reconfiguring pins */
    i2cUnstick(scl, sda);

    /* Configure GPIO pins as AF open-drain */
    IOConfigGPIOAF(scl, pDev->pullUp ? IOCFG_I2C_PU : IOCFG_I2C, pDev->sclAF);
    IOConfigGPIOAF(sda, pDev->pullUp ? IOCFG_I2C_PU : IOCFG_I2C, pDev->sdaAF);

    /* Initialize I2C handle */
    I2C_HandleTypeDef *pHandle = &pDev->handle;
    memset(pHandle, 0, sizeof(*pHandle));
    pHandle->Instance = hardware->reg;

    /*
     * FT32F4 I2C clock source is PCLK1.
     * Use standard library function to get PCLK frequency.
     */
    RCC_ClocksTypeDef rccClocks;
    RCC_GetClocksFreq(&rccClocks);
    uint32_t i2cPclk = rccClocks.PCLK_Frequency;

    pHandle->Init.Timing           = i2cClockTIMINGR(i2cPclk, pDev->clockSpeed, 0);
    pHandle->Init.OwnAddress1      = 0x0;
    pHandle->Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    pHandle->Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    pHandle->Init.OwnAddress2      = 0x0;
    pHandle->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    pHandle->Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    pHandle->Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;

    /* Initialize I2C peripheral using FMD driver (FMD style, no HAL keyword) */
    I2C_Init(pHandle);

    /*
     * Note: FT32F4 I2C peripheral does not support analog/digital noise filters.
     * This is a hardware difference compared to STM32G4/F7/H7.
     * No analog filter configuration call is needed here.
     */

    /* Configure NVIC for I2C interrupt (FT32F4 uses single IRQ for both events and errors) */
    NVIC_InitTypeDef nvic;

    nvic.NVIC_IRQChannel                   = hardware->irqn;
    nvic.NVIC_IRQChannelPreemptionPriority = NVIC_PRIORITY_BASE(NVIC_PRIO_I2C_EV);
    nvic.NVIC_IRQChannelSubPriority        = NVIC_PRIORITY_SUB(NVIC_PRIO_I2C_EV);
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

#endif
