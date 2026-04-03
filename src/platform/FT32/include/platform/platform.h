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

#if defined(FT32F405) || defined(FT32F407)

// Enable modules before including ft32f4xx.h
#define ADC_MODULE_ENABLED
// Note: TMR_MODULE_ENABLED requires ft32f4xx_tmr.h which doesn't exist
// #define TMR_MODULE_ENABLED

// FT32F4xx Standard Peripheral Library header
// This includes ft32f4xx_conf.h which enables/disables modules
// and includes all standard peripheral driver headers
#include "ft32f4xx.h"

// Include TIM standard library header separately (FT32 uses ft32f4xx_tim.h, not ft32f4xx_tmr.h)
#include "ft32f4xx_tim.h"

// NVIC and misc functions (required for interrupt priority configuration)
// This must be included separately as it's not part of ft32f4xx.h
#include "ft32f4xx_misc.h"

// I2C application library (middleware - must be included separately)
#include "i2c_application.h"

// DMA platform definitions (required for DMA_ARCH_TYPE and DMA macros)
#include "platform/dma.h"

/* Chip Unique ID */
#define U_ID_0 (*(uint32_t*)0x1FFF7A10)
#define U_ID_1 (*(uint32_t*)0x1FFF7A14)
#define U_ID_2 (*(uint32_t*)0x1FFF7A18)

#ifndef FT32F4
#define FT32F4
#endif

/* RCC peripheral tag type definition */
typedef uint16_t rccPeriphTag_t;

/* GPIO Pin identifiers - NOT macros!
 * Pin names (PA0, PB3, etc.) must remain as bare identifiers for
 * Betaflight's DEFIO token-pasting system (io_def_generated.h).
 * Do NOT define them as numeric macros here.
 * The actual tag values come from common/stm32/io_def_generated.h
 * via DEFIO_TAG_E__PA0 etc.
 */

/* Register access macros */
#define SET_BIT(REG, BIT)     ((REG) |= (BIT))
#define CLEAR_BIT(REG, BIT)   ((REG) &= ~(BIT))
#define READ_BIT(REG, BIT)    ((REG) & (BIT))
#define CLEAR_REG(REG)        ((REG) = (0x0))
#define WRITE_REG(REG, VAL)   ((REG) = (VAL))
#define READ_REG(REG)         ((REG))
#define MODIFY_REG(REG, CLEARMASK, SETMASK)  WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))

#define USE_TIMER_MGMT
#define USE_TIMER_AF
#define USE_DMA_SPEC
#define USE_PERSISTENT_OBJECTS

#define USE_LATE_TASK_STATISTICS

#define TASK_GYROPID_DESIRED_PERIOD     1000 // 1000us = 1kHz
#define SCHEDULER_DELAY_LIMIT           100

#define DEFAULT_CPU_OVERCLOCK 0
#define FAST_IRQ_HANDLER FAST_CODE

// DMA function aliases for compatibility with Betaflight DMA code
// (defined in dma.h)

#define DMA_DATA_ZERO_INIT
#define DMA_DATA
#define STATIC_DMA_DATA_AUTO        static

#define DMA_RAM
#define DMA_RW_AXI
#define DMA_RAM_R
#define DMA_RAM_W

#define USE_LATE_TASK_STATISTICS

#define USE_RPM_FILTER
#define USE_DYN_IDLE
#define USE_DYN_NOTCH_FILTER

#if defined(FT32F4)

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

// Define i2cHalHandle_t structure (required for I2C_TRAIT_HANDLE)
// Must be defined after I2C_HandleTypeDef typedef
struct i2cHalHandle_s {
    I2C_HandleTypeDef hal;
};
typedef struct i2cHalHandle_s i2cHalHandle_t;

#define NVIC_PRIO_I2C           NVIC_PRIO_I2C_EV
#define SPI_TRAIT_AF_PIN        1
#define UARTHARDWARE_MAX_PINS   4

// NVIC priority configuration (FT32F4 uses priority grouping 4)
#define NVIC_PRIORITY_GROUPING  4

// NVIC priority macros
#define NVIC_BUILD_PRIORITY(base,sub) (((((base)<<(4-(7-(NVIC_PRIORITY_GROUPING))))|((sub)&(0x0f>>(7-(NVIC_PRIORITY_GROUPING)))))<<4)&0xf0)
#define NVIC_PRIORITY_BASE(prio) (((prio)>>(4-(7-(NVIC_PRIORITY_GROUPING))))>>4)
#define NVIC_PRIORITY_SUB(prio) (((prio)>>(4-(7-(NVIC_PRIORITY_GROUPING))))&0x0f)

/* ADC configuration for FT32F4 */
#define ADC_INSTANCE            ADC1
// FT32F4: Internal temperature sensor not yet calibrated
// #define USE_ADC_INTERNAL
// FT32F4: DMA not yet implemented for ADC
// #define USE_DMA_SPEC

#endif

#define FLASH_CONFIG_BUFFER_TYPE      uint32_t

#define USB_DP_PIN PA12

/* SPI clock speed for FT32F4 - assuming 210MHz SYSCLK, APB2 = 105MHz */
#define SPI_CLOCK_MHZ 105

/* Maximum SPI pin selections */
#define MAX_SPI_PIN_SEL 4

#endif

#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
