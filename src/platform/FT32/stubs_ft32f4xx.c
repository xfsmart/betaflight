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

#include "drivers/motor.h"
#include "drivers/serial.h"
#include "drivers/sound_beeper.h"
#include "drivers/transponder_ir.h"
#include "config/config_streamer.h"
#include "drivers/servo_impl.h"

// FT32F4: System stubs

void systemInit(void)
{
    // FT32F4: System initialization stub
}

void uartPinConfigure(const serialPinConfig_t *pSerialPinConfig)
{
    // FT32F4: UART pin configuration stub
    (void)pSerialPinConfig;
}

void spiPreinit(void)
{
    // FT32F4: SPI pre-initialization stub
    // FT32F4: SD card, flash, RX SPI, MAX7456 not yet implemented
}

void beeperPwmInit(const ioTag_t tag, uint16_t frequency)
{
    // FT32F4: Beeper PWM initialization stub
    (void)tag;
    (void)frequency;
}

void servoDevInit(const servoDevConfig_t *servoDevConfig)
{
    // FT32F4: Servo device initialization stub
    (void)servoDevConfig;
}

// FT32F4: Configuration stubs
void configUnlock(void)
{
    // FT32F4: Configuration unlock stub
}

void configLock(void)
{
    // FT32F4: Configuration lock stub
}

configStreamerResult_e configWriteWord(uintptr_t address, config_streamer_buffer_type_t *buffer)
{
    (void)address;
    (void)buffer;
    return (configStreamerResult_e)0;  // Return error code
}


void configClearFlags(void)
{
    // FT32F4: Configuration clear flags stub
}

// FT32F4: DMA stubs - defined in dma_ft32f4xx.c

// FT32F4: System reset - defined in system_ft32f4xx.c

// FT32F4: Motor PWM stubs
bool motorPwmDevInit(motorDevice_t *device, const motorDevConfig_t *motorDevConfig, uint16_t idlePulse)
{
    (void)device;
    (void)motorDevConfig;
    (void)idlePulse;
    return false;
}

bool dshotPwmDevInit(motorDevice_t *device, const motorDevConfig_t *motorConfig)
{
    (void)device;
    (void)motorConfig;
    return false;
}

bool transponderIrIsReady(void)
{
    // FT32F4: Transponder stub
    return false;
}

// FT32F4: USB VCP not yet implemented
void usbVcpInit(void)
{
    // FT32F4: USB VCP initialization stub
}
