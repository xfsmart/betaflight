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
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * FT32F4 GPIO Implementation
 * 
 * This implementation uses direct register access (BSRR, IDR, ODR)
 * to maintain compatibility with STM32F407 and Betaflight coding style.
 * FT32F4 GPIO registers are fully compatible with STM32F4.
 */

#include "platform.h"

#include "drivers/io.h"
#include "drivers/io_impl.h"
#include "platform/io_impl.h"
#include "platform/rcc.h"

#include "common/utils.h"

// io ports defs are stored in array by index now
struct ioPortDef_s {
    rccPeriphTag_t rcc;
};

const struct ioPortDef_s ioPortDefs[] = {
    { RCC_AHB1(GPIOA) },
    { RCC_AHB1(GPIOB) },
    { RCC_AHB1(GPIOC) },
    { RCC_AHB1(GPIOD) },
    { RCC_AHB1(GPIOE) },
    { RCC_AHB1(GPIOH) },
};

uint32_t IO_EXTI_Line(IO_t io)
{
    if (!io) {
        return 0;
    }
    return 1 << IO_GPIOPinIdx(io);
}

bool IORead(IO_t io)
{
    if (!io) {
        return false;
    }
    return (IO_GPIO(io)->IDR & IO_Pin(io));
}

void IOWrite(IO_t io, bool hi)
{
    if (!io) {
        return;
    }
    IO_GPIO(io)->BSRR = hi ? IO_Pin(io) : (IO_Pin(io) << 16);
}

void IOHi(IO_t io)
{
    if (!io) {
        return;
    }
    IO_GPIO(io)->BSRR = IO_Pin(io);
}

void IOLo(IO_t io)
{
    if (!io) {
        return;
    }
    IO_GPIO(io)->BSRR = (IO_Pin(io) << 16);
}

void IOToggle(IO_t io)
{
    if (!io) {
        return;
    }
    IO_GPIO(io)->ODR ^= IO_Pin(io);
}

void IOConfigGPIO(IO_t io, ioConfig_t cfg)
{
    if (!io) {
        return;
    }

    const rccPeriphTag_t rcc = ioPortDefs[IO_GPIOPortIdx(io)].rcc;
    RCC_ClockCmd(rcc, ENABLE);

    // Extract configuration fields from cfg
    // cfg encoding: [1:0]=Mode, [3:2]=Speed, [4]=OutputType, [6:5]=Pull
    uint32_t mode = (cfg >> 0) & 0x03;
    uint32_t speed = (cfg >> 2) & 0x03;
    uint32_t otype = (cfg >> 4) & 0x01;
    uint32_t pull = (cfg >> 5) & 0x03;

    // Map betaflight mode to FT32F4 GPIO mode
    uint32_t gpioMode;
    switch (mode) {
        case 0:  // Input
            gpioMode = GPIO_Mode_IN;
            break;
        case 1:  // Output
            gpioMode = GPIO_Mode_OUT;
            break;
        case 2:  // Alternate Function
            gpioMode = GPIO_Mode_AF;
            break;
        case 3:  // Analog
            gpioMode = GPIO_Mode_AN;
            break;
        default:
            gpioMode = GPIO_Mode_IN;
            break;
    }

    // Initialize GPIO
    GPIO_InitTypeDef init = {
        .GPIO_Pin = IO_Pin(io),
        .GPIO_Mode = gpioMode,
        .GPIO_Speed = speed,
        .GPIO_PuPd = pull,
    };

    if (otype) {
        init.GPIO_OType = GPIO_OType_OD;
    } else {
        init.GPIO_OType = GPIO_OType_PP;
    }

    GPIO_Init(IO_GPIO(io), &init);
}

void IOConfigGPIOAF(IO_t io, ioConfig_t cfg, uint8_t af)
{
    if (!io) {
        return;
    }

    const rccPeriphTag_t rcc = ioPortDefs[IO_GPIOPortIdx(io)].rcc;
    RCC_ClockCmd(rcc, ENABLE);

    // Extract configuration fields from cfg
    uint32_t mode = (cfg >> 0) & 0x03;
    uint32_t speed = (cfg >> 2) & 0x03;
    uint32_t otype = (cfg >> 4) & 0x01;
    uint32_t pull = (cfg >> 5) & 0x03;

    // Map betaflight mode to FT32F4 GPIO mode
    uint32_t gpioMode;
    switch (mode) {
        case 0:  // Input
            gpioMode = GPIO_Mode_IN;
            break;
        case 1:  // Output
            gpioMode = GPIO_Mode_OUT;
            break;
        case 2:  // Alternate Function
            gpioMode = GPIO_Mode_AF;
            break;
        case 3:  // Analog
            gpioMode = GPIO_Mode_AN;
            break;
        default:
            gpioMode = GPIO_Mode_IN;
            break;
    }

    // Initialize GPIO
    GPIO_InitTypeDef init = {
        .GPIO_Pin = IO_Pin(io),
        .GPIO_Mode = gpioMode,
        .GPIO_Speed = speed,
        .GPIO_PuPd = pull,
    };

    if (otype) {
        init.GPIO_OType = GPIO_OType_OD;
    } else {
        init.GPIO_OType = GPIO_OType_PP;
    }

    GPIO_Init(IO_GPIO(io), &init);
    
    // Configure alternate function
    if (gpioMode == GPIO_Mode_AF) {
        GPIO_PinAFConfig(IO_GPIO(io), IO_GPIOPinIdx(io), af);
    }
}
