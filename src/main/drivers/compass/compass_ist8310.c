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

#ifdef USE_MAG_IST8310

#include "common/axis.h"

#include "drivers/bus.h"
#include "drivers/bus_i2c.h"
#include "drivers/bus_i2c_busdev.h"
#include "drivers/sensor.h"
#include "drivers/time.h"

#include "compass.h"
#include "compass_ist8310.h"

#define IST8310_MAG_I2C_ADDRESS 0x0E

#define IST8310_REG_DATA 0x03
#define IST8310_REG_WAI 0x00
#define IST8310_REG_WAI_VALID 0x10

#define IST8310_REG_STAT1 0x02
#define IST8310_DRDY_MASK 0x01

// I2C Control Register
#define IST8310_REG_CNTRL1 0x0A
#define IST8310_REG_CNTRL2 0x0B
#define IST8310_REG_AVERAGE 0x41
#define IST8310_REG_PDCNTL 0x42

// Parameter
// ODR = Output Data Rate, we use single measure mode for getting more data.
#define IST8310_ODR_SINGLE 0x01
#define IST8310_AVG_16  0x24
#define IST8310_PULSE_DURATION_NORMAL 0xC0

#define IST8310_DRDY_MAX_RETRIES 15
#define IST8310_LSB_TO_MGAUSS 3
#define IST8310_RAW_XY_MAX 5334
#define IST8310_RAW_Z_MAX 8334
#define IST8310_DATA_SENTINEL 0x7FFF

typedef enum {
    IST8310_STATE_STATUS,
    IST8310_STATE_DATA,
    IST8310_STATE_TRIGGER,
} ist8310ReadState_e;

typedef enum {
    IST8310_PENDING_NONE,
    IST8310_PENDING_STATUS,
    IST8310_PENDING_DATA,
    IST8310_PENDING_TRIGGER,
} ist8310Pending_e;

typedef struct {
    uint8_t data[6];
    uint8_t status;
    uint8_t retries;
    ist8310ReadState_e state;
    ist8310Pending_e pending;
} ist8310ReadContext_t;

static ist8310ReadContext_t ist8310ReadContext;

static void ist8310PrepareDataBuffer(void)
{
    for (unsigned axis = 0; axis < 3; axis++) {
        ist8310ReadContext.data[2 * axis] = IST8310_DATA_SENTINEL & 0xFF;
        ist8310ReadContext.data[2 * axis + 1] = IST8310_DATA_SENTINEL >> 8;
    }
}

static void ist8310ResetReadContext(magDev_t *magDev)
{
    ist8310ReadContext = (ist8310ReadContext_t) {
        .state = IST8310_STATE_STATUS,
    };
    ist8310PrepareDataBuffer();
    magDev->busError = false;
}

static int16_t ist8310RawAxis(unsigned axis)
{
    const unsigned offset = 2 * axis;
    return (int16_t)((uint16_t)ist8310ReadContext.data[offset + 1] << 8 | ist8310ReadContext.data[offset]);
}

static bool ist8310DecodeSample(int16_t *magData)
{
    const int16_t rawX = ist8310RawAxis(X);
    const int16_t rawY = ist8310RawAxis(Y);
    const int16_t rawZ = ist8310RawAxis(Z);

    if (rawX < -IST8310_RAW_XY_MAX || rawX > IST8310_RAW_XY_MAX ||
        rawY < -IST8310_RAW_XY_MAX || rawY > IST8310_RAW_XY_MAX ||
        rawZ < -IST8310_RAW_Z_MAX || rawZ > IST8310_RAW_Z_MAX) {
        return false;
    }

    magData[X] = rawX * IST8310_LSB_TO_MGAUSS;
    magData[Y] = -rawY * IST8310_LSB_TO_MGAUSS;
    magData[Z] = rawZ * IST8310_LSB_TO_MGAUSS;
    return true;
}

static bool ist8310StartCurrentOperation(extDevice_t *dev)
{
    switch (ist8310ReadContext.state) {
    case IST8310_STATE_STATUS:
        ist8310ReadContext.status = 0;
        if (busReadRegisterBufferStart(dev, IST8310_REG_STAT1, &ist8310ReadContext.status, sizeof(ist8310ReadContext.status))) {
            ist8310ReadContext.pending = IST8310_PENDING_STATUS;
            return true;
        }
        break;

    case IST8310_STATE_DATA:
        ist8310PrepareDataBuffer();
        if (busReadRegisterBufferStart(dev, IST8310_REG_DATA, ist8310ReadContext.data, sizeof(ist8310ReadContext.data))) {
            ist8310ReadContext.pending = IST8310_PENDING_DATA;
            return true;
        }
        break;

    case IST8310_STATE_TRIGGER:
        if (busWriteRegisterStart(dev, IST8310_REG_CNTRL1, IST8310_ODR_SINGLE)) {
            ist8310ReadContext.pending = IST8310_PENDING_TRIGGER;
            return true;
        }
        break;
    }

    return false;
}

static bool ist8310CompletePending(magDev_t *magDev, int16_t *magData)
{
    bool sampleReady = false;

    switch (ist8310ReadContext.pending) {
    case IST8310_PENDING_NONE:
        break;

    case IST8310_PENDING_STATUS:
        ist8310ReadContext.pending = IST8310_PENDING_NONE;
        if (magDev->busError) {
            ist8310ReadContext.status = 0;
            ist8310ReadContext.state = IST8310_STATE_STATUS;
        } else if (ist8310ReadContext.status & IST8310_DRDY_MASK) {
            ist8310ReadContext.retries = 0;
            ist8310ReadContext.state = IST8310_STATE_DATA;
        } else if (++ist8310ReadContext.retries >= IST8310_DRDY_MAX_RETRIES) {
            ist8310ReadContext.retries = 0;
            ist8310ReadContext.state = IST8310_STATE_TRIGGER;
        } else {
            ist8310ReadContext.state = IST8310_STATE_STATUS;
        }
        break;

    case IST8310_PENDING_DATA:
        ist8310ReadContext.pending = IST8310_PENDING_NONE;
        ist8310ReadContext.retries = 0;
        ist8310ReadContext.state = IST8310_STATE_TRIGGER;
        if (!magDev->busError) {
            sampleReady = ist8310DecodeSample(magData);
        }
        break;

    case IST8310_PENDING_TRIGGER:
        ist8310ReadContext.pending = IST8310_PENDING_NONE;
        ist8310ReadContext.state = magDev->busError ? IST8310_STATE_TRIGGER : IST8310_STATE_STATUS;
        break;
    }

    return sampleReady;
}

static bool ist8310Init(magDev_t *magDev)
{
    extDevice_t *dev = &magDev->dev;

    ist8310ResetReadContext(magDev);
    magDev->magOdrHz = 0;
    busDeviceRegister(dev);

    // Init setting
    bool ack = busWriteRegister(dev, IST8310_REG_AVERAGE, IST8310_AVG_16);
    delay(6);
    ack = ack && busWriteRegister(dev, IST8310_REG_PDCNTL, IST8310_PULSE_DURATION_NORMAL);
    delay(6);
    ack = ack && busWriteRegister(dev, IST8310_REG_CNTRL1, IST8310_ODR_SINGLE);

    if (ack) {
        magDev->magOdrHz = 100;
    }

    return ack;
}

static bool ist8310Read(magDev_t * magDev, int16_t *magData)
{
    extDevice_t *dev = &magDev->dev;

    const bool sampleReady = ist8310CompletePending(magDev, magData);
    if (ist8310ReadContext.pending == IST8310_PENDING_NONE) {
        ist8310StartCurrentOperation(dev);
    }

    return sampleReady;
}

static bool deviceDetect(magDev_t * magDev)
{
    uint8_t result = 0;
    bool ack = busReadRegisterBuffer(&magDev->dev, IST8310_REG_WAI, &result, 1);

    return ack && result == IST8310_REG_WAI_VALID;
}

bool ist8310Detect(magDev_t * magDev)
{
    extDevice_t *dev = &magDev->dev;

    if (dev->bus->busType == BUS_TYPE_I2C && dev->busType_u.i2c.address == 0) {
        dev->busType_u.i2c.address = IST8310_MAG_I2C_ADDRESS;
    }

    if (deviceDetect(magDev)) {
        magDev->init = ist8310Init;
        magDev->read = ist8310Read;

        return true;
    }

    return false;
}

#endif
