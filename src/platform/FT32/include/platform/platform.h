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

#pragma once

// ============================================================
// Chip identification
// ============================================================
#if defined(FT32F405) || defined(FT32F407)

#define ADC_MODULE_ENABLED

#include "ft32f4xx.h"
#include "ft32f4xx_misc.h"
#include "system_ft32f4xx.h"
#include "i2c_application.h"
#include "platform/dma.h"

/* Chip Unique ID (FT32F4: 0x1FFF0A00-0x1FFF0A0B) */
#define U_ID_0 (*(uint32_t*)0x1FFF0A00)
#define U_ID_1 (*(uint32_t*)0x1FFF0A04)
#define U_ID_2 (*(uint32_t*)0x1FFF0A08)

/* RCC peripheral tag type definition */
typedef uint16_t rccPeriphTag_t;

#ifndef FT32F4
#define FT32F4
#endif

#endif /* FT32F405 || FT32F407 */

// ============================================================
// Platform feature macros (per-series)
// ============================================================
#ifdef FT32F4

#define USE_TIMER_MGMT
#define USE_TIMER_AF
#define USE_DMA_SPEC
#define USE_PERSISTENT_OBJECTS
#define USE_USB_MSC
#define USE_USB_CDC_HID
#define USE_LATE_TASK_STATISTICS

#define USE_RPM_FILTER
#define USE_DYN_IDLE
#define USE_DYN_NOTCH_FILTER

#define USE_ADC_INTERNAL

#endif /* FT32F4 */

// ============================================================
// Scheduler defaults
// ============================================================
#define TASK_GYROPID_DESIRED_PERIOD     1000 // 1000us = 1kHz
#define SCHEDULER_DELAY_LIMIT           100

#define DEFAULT_CPU_OVERCLOCK 0
#define PLATFORM_TRAIT_CONFIG_HSE 1
#define FAST_IRQ_HANDLER FAST_CODE

// ============================================================
// DMA memory attributes
// ============================================================
#define DMA_DATA_ZERO_INIT
#define DMA_DATA
#define STATIC_DMA_DATA_AUTO        static

#define DMA_RAM
#define DMA_RW_AXI
#define DMA_RAM_R
#define DMA_RAM_W

// ============================================================
// IO configs and peripheral traits
// ============================================================
#ifdef FT32F4

/* Register access macros */
#define SET_BIT(REG, BIT)     ((REG) |= (BIT))
#define CLEAR_BIT(REG, BIT)   ((REG) &= ~(BIT))
#define READ_BIT(REG, BIT)    ((REG) & (BIT))
#define CLEAR_REG(REG)        ((REG) = (0x0))
#define WRITE_REG(REG, VAL)   ((REG) = (VAL))
#define READ_REG(REG)         ((REG))
#define MODIFY_REG(REG, CLEARMASK, SETMASK)  WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))

/* GPIO configuration macros */
#define IO_CONFIG(mode, speed, otype, pupd) ((mode) | ((speed) << 2) | ((otype) << 4) | ((pupd) << 5))

#define IOCFG_OUT_PP         IO_CONFIG(GPIO_Mode_OUT, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL)
#define IOCFG_OUT_PP_UP      IO_CONFIG(GPIO_Mode_OUT, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_UP)
#define IOCFG_OUT_PP_25      IO_CONFIG(GPIO_Mode_OUT, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL)
#define IOCFG_OUT_OD         IO_CONFIG(GPIO_Mode_OUT, GPIO_Speed_50MHz, GPIO_OType_OD, GPIO_PuPd_NOPULL)
#define IOCFG_AF_PP          IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL)
#define IOCFG_AF_PP_PD       IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_DOWN)
#define IOCFG_AF_PP_UP       IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_UP)
#define IOCFG_AF_OD          IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_OD, GPIO_PuPd_NOPULL)
#define IOCFG_IPD            IO_CONFIG(GPIO_Mode_IN, GPIO_Speed_50MHz, 0, GPIO_PuPd_DOWN)
#define IOCFG_IPU            IO_CONFIG(GPIO_Mode_IN, GPIO_Speed_50MHz, 0, GPIO_PuPd_UP)
#define IOCFG_IN_FLOATING    IO_CONFIG(GPIO_Mode_IN, GPIO_Speed_50MHz, 0, GPIO_PuPd_NOPULL)
#define IOCFG_IPU_25         IO_CONFIG(GPIO_Mode_IN, GPIO_Speed_50MHz, 0, GPIO_PuPd_UP)

#define SPI_IO_AF_CFG           IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL)
#define SPI_IO_AF_SCK_CFG_HIGH  IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_UP)
#define SPI_IO_AF_SCK_CFG_LOW   IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_DOWN)
#define SPI_IO_AF_SDI_CFG       IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_UP)
#define SPI_IO_CS_CFG           IO_CONFIG(GPIO_Mode_OUT, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL)
#define SPI_IO_CS_HIGH_CFG      IO_CONFIG(GPIO_Mode_IN, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_UP)

#define SPIDEV_COUNT       4

#define UART_TX_BUFFER_ATTRIBUTE
#define UART_RX_BUFFER_ATTRIBUTE

#define PLATFORM_TRAIT_RCC      1
#define PLATFORM_TRAIT_ADC_DEVICE  1
#define UART_TRAIT_AF_PIN       1
#define SERIAL_TRAIT_PIN_CONFIG 1
#define I2CDEV_COUNT            3
#define I2C_TRAIT_AF_PIN        1
#define I2C_TRAIT_HANDLE        1
#define I2C_HandleTypeDef       i2c_handle_type

struct i2cHalHandle_s {
    I2C_HandleTypeDef hal;
};
typedef struct i2cHalHandle_s i2cHalHandle_t;

#define NVIC_PRIO_I2C           NVIC_PRIO_I2C_EV
#define SPI_TRAIT_AF_PIN        1
#define UARTHARDWARE_MAX_PINS   4

/* USART data register addresses for DMA */
#define UART_REG_TXD(base)      (((USART_TypeDef *)(base))->THR)
#define UART_REG_RXD(base)      (((USART_TypeDef *)(base))->RHR)

/* NVIC priority configuration (FT32F4 uses priority grouping 4) */
#define NVIC_PRIORITY_GROUPING  4

#define NVIC_BUILD_PRIORITY(base,sub) (((((base)<<(4-(7-(NVIC_PRIORITY_GROUPING))))|((sub)&(0x0f>>(7-(NVIC_PRIORITY_GROUPING)))))<<4)&0xf0)
#define NVIC_PRIORITY_BASE(prio) (((prio)>>(4-(7-(NVIC_PRIORITY_GROUPING))))>>4)
#define NVIC_PRIORITY_SUB(prio) (((prio)>>(4-(7-(NVIC_PRIORITY_GROUPING))))&0x0f)
#define NVIC_PRIORITY_TO_CMSIS(prio) ((uint32_t)(prio) >> (8U - __NVIC_PRIO_BITS))

/* ADC configuration for FT32F4 */
#define ADC_INSTANCE            ADC1

#endif /* FT32F4 */

// ============================================================
// Common definitions
// ============================================================
#define FLASH_CONFIG_BUFFER_TYPE      uint32_t

#define USB_DP_PIN PA12

/* SPI clock speed for FT32F4 - 210MHz SYSCLK, APB2 = 105MHz */
#define SPI_CLOCK_MHZ 105

/* Maximum SPI pin selections */
#define MAX_SPI_PIN_SEL 4

#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
