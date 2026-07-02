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

#include "platform.h"

#include "drivers/io.h"
#include "drivers/io_impl.h"
#include "platform/io_impl.h"
#include "platform/rcc.h"

#include "common/utils.h"

#include "ft32f4xx_hal_gpio.h"

// IO ports definitions - FT32F4 uses AHB1 for GPIO clocks
struct ioPortDef_s {
    rccPeriphTag_t rcc;
};

#if defined(FT32F4)
const struct ioPortDef_s ioPortDefs[] = {
    { RCC_AHB1(GPIOA) },
    { RCC_AHB1(GPIOB) },
    { RCC_AHB1(GPIOC) },
    { RCC_AHB1(GPIOD) },
    { RCC_AHB1(GPIOE) },
    { RCC_EMPTY },
    { RCC_EMPTY },
    { RCC_AHB1(GPIOH) },
};
#else
# error "IO PortDefs not defined for MCU"
#endif

// EXTI line mapping for FT32F4
uint32_t IO_EXTI_Line(IO_t io)
{
    if (!io) {
        return 0;
    }
    return 1 << IO_GPIOPinIdx(io);
}

// Read pin state
bool IORead(IO_t io)
{
    if (!io) {
        return false;
    }
    return (IO_GPIO(io)->IDR & IO_Pin(io)) != 0;
}

// Write pin state
void IOWrite(IO_t io, bool hi)
{
    if (!io) {
        return;
    }
    if (hi) {
        IO_GPIO(io)->BSRR = IO_Pin(io);
    } else {
        IO_GPIO(io)->BSRR = (uint32_t)IO_Pin(io) << 16U;
    }
}

// Set pin high
void IOHi(IO_t io)
{
    if (!io) {
        return;
    }
    IO_GPIO(io)->BSRR = IO_Pin(io);
}

// Set pin low
void IOLo(IO_t io)
{
    if (!io) {
        return;
    }
    IO_GPIO(io)->BSRR = (uint32_t)IO_Pin(io) << 16U;
}

// Toggle pin
void IOToggle(IO_t io)
{
    if (!io) {
        return;
    }
    IO_GPIO(io)->ODR ^= IO_Pin(io);
}

// Configure GPIO without alternate function
void IOConfigGPIO(IO_t io, ioConfig_t cfg)
{
    IOConfigGPIOAF(io, cfg, 0);
}

// Configure GPIO with alternate function
void IOConfigGPIOAF(IO_t io, ioConfig_t cfg, uint8_t af)
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
    // Betaflight mode: 0=IN, 1=OUT, 2=AF, 3=ANALOG
    // FT32F4 mode: GPIO_MODE_INPUT, GPIO_MODE_OUTPUT_PP, GPIO_MODE_AF_PP, GPIO_MODE_ANALOG
    uint32_t gpioMode;
    switch (mode) {
        case 0:  // Input
            gpioMode = GPIO_MODE_INPUT;
            break;
        case 1:  // Output
            gpioMode = otype ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT_PP;
            break;
        case 2:  // Alternate Function
            gpioMode = otype ? GPIO_MODE_AF_OD : GPIO_MODE_AF_PP;
            break;
        case 3:  // Analog
            gpioMode = GPIO_MODE_ANALOG;
            break;
        default:
            gpioMode = GPIO_MODE_INPUT;
            break;
    }

    // Initialize GPIO
    GPIO_InitTypeDef init = {
        .Pin = IO_Pin(io),
        .Mode = gpioMode,
        .Speed = speed,
        .Pull = pull,
        .Alternate = af,
    };

    GPIO_Init(IO_GPIO(io), &init);
}
