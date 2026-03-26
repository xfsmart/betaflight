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

/*
 * This file implements the FT32F405/407 RCC clock enable/reset functions
 * using FT32F4xx Standard Peripheral Library
 * 
 * Note: FT32F4 uses register-based clock control
 * - AHBxENR for AHB bus clock enable
 * - APBxENR for APB bus clock enable
 * - AHBxRSTR for AHB bus reset
 * - APBxRSTR for APB bus reset
 */

#include "platform.h"
#include "platform/rcc.h"

/**
 * @brief  Enables or disables the peripheral clock
 * @param  periphTag: RCC peripheral tag (encoded register and bit position)
 *         - Upper 5 bits: bus type (RCC_AHB1, RCC_APB1, etc.)
 *         - Lower 5 bits: bit position in enable register
 * @param  NewState: ENABLE or DISABLE
 * @retval None
 * 
 * Note: After enabling a peripheral clock, a dummy read is performed
 *       to ensure the clock is stable before the peripheral is accessed.
 *       This follows the same pattern as AT32F43x and STM32F4.
 */
void RCC_ClockCmd(rccPeriphTag_t periphTag, FunctionalState NewState)
{
    int tag = periphTag >> 5;
    uint32_t mask = 1 << (periphTag & 0x1f);

    // Macro definitions for clock enable/disable
    // FT32F4 uses uppercase register names: AHB1ENR, APB1ENR, etc.
#define NOSUFFIX // Empty suffix for FT32F4

    // Clock enable macro with dummy read for stability
    // This ensures the clock is stable before peripheral access
#define __FT_RCC_CLK_ENABLE(reg, enbit)   do {      \
        __IO uint32_t tmpreg;                       \
        SET_BIT(RCC->reg, enbit);                   \
        /* Delay after an RCC peripheral clock enabling */  \
        /* Read back to ensure clock is stable */           \
        tmpreg = READ_BIT(RCC->reg, enbit);         \
        UNUSED(tmpreg);                             \
    } while(0)

    // Clock disable macro (no dummy read needed)
#define __FT_RCC_CLK_DISABLE(reg, enbit) (RCC->reg &= ~(enbit))

    // Combined clock control macro
#define __FT_RCC_CLK(reg, enbit, newState) \
    if (newState == ENABLE) {              \
        __FT_RCC_CLK_ENABLE(reg, enbit);   \
    } else {                               \
        __FT_RCC_CLK_DISABLE(reg, enbit);  \
    }

    // Process based on bus type
    switch (tag) {
        case RCC_AHB1_VALUE:
            // AHB1 peripherals: DMA1, DMA2, GPIOA-GPIOH, etc.
            __FT_RCC_CLK(AHB1ENR, mask, NewState);
            break;

        case RCC_APB1_VALUE:
            // APB1 peripherals: TIM2-TIM7, I2C1-3, USART2-3, etc.
            __FT_RCC_CLK(APB1ENR, mask, NewState);
            break;

        case RCC_APB2_VALUE:
            // APB2 peripherals: TIM1, TIM8, USART1, SPI1, etc.
            __FT_RCC_CLK(APB2ENR, mask, NewState);
            break;

        default:
            // Unknown bus type - should not happen
            break;
    }

#undef NOSUFFIX
#undef __FT_RCC_CLK_ENABLE
#undef __FT_RCC_CLK_DISABLE
#undef __FT_RCC_CLK
}

/**
 * @brief  Forces or releases peripheral reset
 * @param  periphTag: RCC peripheral tag (encoded register and bit position)
 *         - Upper 5 bits: bus type (RCC_AHB1, RCC_APB1, etc.)
 *         - Lower 5 bits: bit position in reset register
 * @param  NewState: ENABLE (release reset) or DISABLE (force reset)
 * @retval None
 * 
 * Note: Reset control follows the same bit positions as clock enable.
 *       This is consistent across FT32F4, AT32F43x, and STM32F4.
 */
void RCC_ResetCmd(rccPeriphTag_t periphTag, FunctionalState NewState)
{
    int tag = periphTag >> 5;
    uint32_t mask = 1 << (periphTag & 0x1f);

    // Macro definitions for reset control
    // FT32F4 uses uppercase register names: AHB1RSTR, APB1RSTR, etc.

#define __FT_RCC_FORCE_RESET(reg, enbit) (RCC->reg |= (enbit))
#define __FT_RCC_RELEASE_RESET(reg, enbit) (RCC->reg &= ~(enbit))

    // Combined reset control macro
    // ENABLE = release reset, DISABLE = force reset
#define __FT_RCC_RESET(reg, enbit, newState) \
    if (newState == ENABLE) {                \
        __FT_RCC_RELEASE_RESET(reg, enbit);  \
    } else {                                 \
        __FT_RCC_FORCE_RESET(reg, enbit);    \
    }

    // Process based on bus type
    switch (tag) {
        case RCC_AHB1_VALUE:
            // AHB1 peripherals reset
            __FT_RCC_RESET(AHB1RSTR, mask, NewState);
            break;

        case RCC_APB1_VALUE:
            // APB1 peripherals reset
            __FT_RCC_RESET(APB1RSTR, mask, NewState);
            break;

        case RCC_APB2_VALUE:
            // APB2 peripherals reset
            __FT_RCC_RESET(APB2RSTR, mask, NewState);
            break;

        default:
            // Unknown bus type - should not happen
            break;
    }

#undef __FT_RCC_FORCE_RESET
#undef __FT_RCC_RELEASE_RESET
#undef __FT_RCC_RESET
}
