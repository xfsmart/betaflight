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

#include "platform.h"

#include "drivers/exti.h"
#include "drivers/nvic.h"
#include "drivers/system.h"
#include "drivers/persistent.h"

// External declaration from common system code
extern uint32_t cachedRccCsrValue;
extern void cycleCounterInit(void);

// Clock configuration from startup/system_ft32f4xx.c
void SetSysClock(void);

#define FT32_SYSTEM_MEMORY_SIZE_BYTES  (30U * 1024U)
#define FT32_SRAM_BASE                 0x20000000U
#define FT32_SRAM_END                  0x20020000U
#define FT32_CCM_BASE                  0x10000000U
#define FT32_CCM_END                   0x10010000U

void systemReset(void)
{
    __disable_irq();
    NVIC_SystemReset();
}

void systemResetToBootloader(bootloaderRequestType_e requestType)
{
    switch (requestType) {
    case BOOTLOADER_REQUEST_ROM:
    default:
        persistentObjectWrite(PERSISTENT_OBJECT_RESET_REASON, RESET_BOOTLOADER_REQUEST_ROM);
        break;
    }

    __disable_irq();
    NVIC_SystemReset();
}

typedef void resetHandler_t(void);

typedef struct isrVector_s {
    __I uint32_t    stackEnd;
    resetHandler_t *resetHandler;
} isrVector_t;

static bool isStackAddressInRange(uint32_t address, uint32_t base, uint32_t end)
{
    return address > base && address <= end;
}

static bool isAddressInRange(uint32_t address, uint32_t base, uint32_t end)
{
    return address >= base && address < end;
}

static bool isValidBootloaderStack(uint32_t stackEnd)
{
    if ((stackEnd & 0x3U) != 0U) {
        return false;
    }

    return isStackAddressInRange(stackEnd, FT32_SRAM_BASE, FT32_SRAM_END) ||
           isStackAddressInRange(stackEnd, FT32_CCM_BASE, FT32_CCM_END);
}

static bool isValidBootloaderResetHandler(uint32_t resetHandler, uint32_t systemMemoryBase)
{
    if ((resetHandler == 0U) || (resetHandler == 0xffffffffU) || ((resetHandler & 0x1U) == 0U)) {
        return false;
    }

    return isAddressInRange(resetHandler & ~0x1U, systemMemoryBase, systemMemoryBase + FT32_SYSTEM_MEMORY_SIZE_BYTES);
}

void checkForBootLoaderRequest(void)
{
    volatile uint32_t bootloaderRequest = persistentObjectRead(PERSISTENT_OBJECT_RESET_REASON);

    if (bootloaderRequest != RESET_BOOTLOADER_REQUEST_ROM) {
        return;
    }
    persistentObjectWrite(PERSISTENT_OBJECT_RESET_REASON, RESET_NONE);

    extern isrVector_t system_isr_vector_table_base;

    const uint32_t bootloaderStackEnd = system_isr_vector_table_base.stackEnd;
    const uint32_t bootloaderResetHandler = (uint32_t)(uintptr_t)system_isr_vector_table_base.resetHandler;
    const uint32_t systemMemoryBase = (uint32_t)(uintptr_t)&system_isr_vector_table_base;

    if (!isValidBootloaderStack(bootloaderStackEnd) ||
        !isValidBootloaderResetHandler(bootloaderResetHandler, systemMemoryBase)) {
        return;
    }

    __disable_irq();
    __set_MSP(bootloaderStackEnd);
    ((resetHandler_t *)(uintptr_t)bootloaderResetHandler)();
    __builtin_unreachable();
}

void enableGPIOPowerUsageAndNoiseReductions(void)
{
    // Pre-enable clocks for common peripherals to prevent
    // floating input GPIOs from drawing excess current on unclocked blocks.

    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_CCMDATARAM |
        RCC_AHB1Periph_BKPSRAM |
        RCC_AHB1Periph_DMA1 |
        RCC_AHB1Periph_DMA2 |
        0, ENABLE
    );

    RCC_AHB2PeriphClockCmd(0, ENABLE);
    RCC_AHB3PeriphClockCmd(0, ENABLE);

    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_TIM2 |
        RCC_APB1Periph_TIM3 |
        RCC_APB1Periph_TIM4 |
        RCC_APB1Periph_TIM5 |
        RCC_APB1Periph_TIM6 |
        RCC_APB1Periph_TIM7 |
        RCC_APB1Periph_TIM12 |
        RCC_APB1Periph_TIM13 |
        RCC_APB1Periph_TIM14 |
        RCC_APB1Periph_WWDG |
        RCC_APB1Periph_SPI2 |
        RCC_APB1Periph_SPI3 |
        RCC_APB1Periph_UART2 |
        RCC_APB1Periph_UART3 |
        RCC_APB1Periph_UART4 |
        RCC_APB1Periph_UART5 |
        RCC_APB1Periph_I2C1 |
        RCC_APB1Periph_I2C2 |
        RCC_APB1Periph_I2C3 |
        RCC_APB1Periph_CAN1 |
        RCC_APB1Periph_CAN2 |
        RCC_APB1Periph_CAN3 |
        RCC_APB1Periph_CAN4 |
        RCC_APB1Periph_PWR |
        RCC_APB1Periph_DAC |
        0, ENABLE);

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_TIM1 |
        RCC_APB2Periph_TIM8 |
        RCC_APB2Periph_TIM9 |
        RCC_APB2Periph_TIM10 |
        RCC_APB2Periph_TIM11 |
        RCC_APB2Periph_USART1 |
        RCC_APB2Periph_USART6 |
        RCC_APB2Periph_ADC |
        RCC_APB2Periph_SDIO |
        RCC_APB2Periph_SPI1 |
        RCC_APB2Periph_SYSCFG |
        0, ENABLE);
}

bool isMPUSoftReset(void)
{
    if (cachedRccCsrValue & RCC_CSR_SFTRSTF)
        return true;
    else
        return false;
}

void systemInit(void)
{
    // Initialize persistent objects (RTC backup domain)
    persistentObjectInit();

    // Check if we should jump to bootloader
    checkForBootLoaderRequest();

    // Configure system clock (HSE/HSI -> PLL -> 210MHz default)
    SetSysClock();

    // Configure NVIC preempt/priority groups. CMSIS shifts the raw FT32
    // priority-group value before updating AIRCR; the FT32 StdPeriph helper
    // expects a pre-shifted value and would treat the raw platform constant as
    // a reset request bit.
    NVIC_SetPriorityGrouping(NVIC_PRIORITY_GROUPING);

    // Cache RCC->CSR value for isMPUSoftReset()
    cachedRccCsrValue = RCC->CSR;

    // Clear reset flags by setting RMVF bit
    RCC->CSR |= RCC_CSR_RMVF;

    // Set vector table location
    extern uint8_t isr_vector_table_base;
    NVIC_SetVectorTable((uint32_t)&isr_vector_table_base, 0x0);

    RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_USBOTGFS, DISABLE);

    // Enable GPIO power optimizations
    enableGPIOPowerUsageAndNoiseReductions();

    // Initialize cycle counter for micros()
    cycleCounterInit();

    // Configure SysTick for 1ms interrupts
    SysTick_Config(SystemCoreClock / 1000);
}
