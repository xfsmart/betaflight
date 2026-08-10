/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 */

#include <cstring>
#include <deque>
#include <vector>

extern "C" {
#include "platform.h"

#include "common/sensor_alignment.h"

#include "build/debug.h"

#include "drivers/bus.h"
#include "drivers/bus_i2c.h"
#include "drivers/compass/compass.h"
#include "drivers/time.h"

#include "fc/runtime_config.h"

#include "io/beeper.h"

#include "sensors/acceleration.h"
#include "sensors/compass.h"
#include "sensors/gyro.h"
#include "sensors/initialisation.h"
#include "sensors/sensors.h"

extern magDev_t magDev;
}

#include "gtest/gtest.h"

namespace {

bool gyroInitResult;
bool magDetectResult;
bool magInitResult;
uint32_t sensorMask;
unsigned accelerometerInitCount;
unsigned rotationBuildCount;
unsigned barometerInitCount;
unsigned schedulerIgnoreCount;
std::vector<bool> observedReadErrors;

struct BusCompletion {
    bool busy;
    bool error;
};

std::deque<BusCompletion> busCompletions;

bool testMagInit(magDev_t *dev)
{
    dev->magOdrHz = magInitResult ? 100 : 0;
    return magInitResult;
}

bool testMagRead(magDev_t *dev, int16_t *)
{
    observedReadErrors.push_back(dev->busError);
    return false;
}

void configureCompass()
{
    std::memset(&magDev, 0, sizeof(magDev));
    std::memset(compassConfigMutable(), 0, sizeof(*compassConfigMutable()));

    compassConfigMutable()->mag_alignment = ALIGN_DEFAULT;
    compassConfigMutable()->mag_hardware = MAG_IST8310;
    compassConfigMutable()->mag_busType = BUS_TYPE_I2C;
    compassConfigMutable()->mag_i2c_device = I2C_DEV_TO_CFG(I2CDEV_1);
    compassConfigMutable()->mag_i2c_address = 0;
    detectedSensors[SENSOR_INDEX_MAG] = MAG_NONE;

    gyroInitResult = true;
    magDetectResult = true;
    magInitResult = true;
    sensorMask = 0;
    accelerometerInitCount = 0;
    rotationBuildCount = 0;
    barometerInitCount = 0;
    schedulerIgnoreCount = 0;
    observedReadErrors.clear();
    busCompletions.clear();
}

class CompassInitTest : public testing::Test {
protected:
    void SetUp() override
    {
        configureCompass();
    }
};

} // namespace

TEST_F(CompassInitTest, DriverInitFailurePropagatesAndClearsMagState)
{
    magInitResult = false;

    EXPECT_TRUE(sensorsAutodetect());
    EXPECT_EQ(MAG_NONE, detectedSensors[SENSOR_INDEX_MAG]);
    EXPECT_EQ(0U, sensorMask & SENSOR_MAG);
    EXPECT_EQ(1U, accelerometerInitCount);
    EXPECT_EQ(0U, rotationBuildCount);
    EXPECT_EQ(1U, barometerInitCount);
}

TEST_F(CompassInitTest, SuccessfulInitRetainsDetectedMagState)
{
    EXPECT_TRUE(sensorsAutodetect());
    EXPECT_EQ(MAG_IST8310, detectedSensors[SENSOR_INDEX_MAG]);
    EXPECT_NE(0U, sensorMask & SENSOR_MAG);
    EXPECT_EQ(ALIGN_DEFAULT, magDev.magAlignment);
    EXPECT_EQ(1U, rotationBuildCount);
    EXPECT_EQ(1U, barometerInitCount);
}

TEST_F(CompassInitTest, CompassInitReturnsFalseWhenDriverInitFails)
{
    magInitResult = false;

    EXPECT_FALSE(compassInit());
    EXPECT_EQ(0U, rotationBuildCount);
}

TEST_F(CompassInitTest, BusyCompletionErrorIsLatchedUntilOneIdleRead)
{
    ASSERT_TRUE(compassInit());
    busCompletions = {{true, true}, {false, false}, {false, false}};

    EXPECT_EQ(500U, compassUpdate(1000));
    EXPECT_TRUE(magDev.busError);
    EXPECT_TRUE(observedReadErrors.empty());

    EXPECT_EQ(1000U, compassUpdate(1500));
    ASSERT_EQ(1U, observedReadErrors.size());
    EXPECT_TRUE(observedReadErrors[0]);
    EXPECT_FALSE(magDev.busError);

    EXPECT_EQ(1000U, compassUpdate(2500));
    ASSERT_EQ(2U, observedReadErrors.size());
    EXPECT_FALSE(observedReadErrors[1]);
    EXPECT_FALSE(magDev.busError);
    EXPECT_EQ(3U, schedulerIgnoreCount);
}

extern "C" {

gyro_t gyro;
int16_t debug[DEBUG16_VALUE_COUNT];
uint8_t debugMode;

bool gyroInit(void)
{
    return gyroInitResult;
}

bool accInit(uint16_t)
{
    accelerometerInitCount++;
    return true;
}

void baroInit(void)
{
    barometerInitCount++;
}

bool i2cBusSetInstance(extDevice_t *dev, uint32_t device)
{
    dev->bus->busType = BUS_TYPE_I2C;
    dev->bus->busType_u.i2c.device = static_cast<i2cDevice_e>(I2C_CFG_TO_DEV(device));
    return true;
}

bool busBusy(const extDevice_t *, bool *error)
{
    if (busCompletions.empty()) {
        if (error) {
            *error = false;
        }
        return false;
    }

    const BusCompletion completion = busCompletions.front();
    busCompletions.pop_front();
    if (error) {
        *error = completion.error;
    }
    return completion.busy;
}

bool ist8310Detect(magDev_t *dev)
{
    if (!magDetectResult) {
        return false;
    }

    dev->init = testMagInit;
    dev->read = testMagRead;
    return true;
}

void sensorsSet(uint32_t mask)
{
    sensorMask |= mask;
}

void sensorsClear(uint32_t mask)
{
    sensorMask &= ~mask;
}

uint32_t sensorsMask(void)
{
    return sensorMask;
}

void buildRotationMatrixFromAngles(matrix33_t *, const sensorAlignment_t *)
{
    rotationBuildCount++;
}

void alignSensorViaMatrix(vector3_t *, matrix33_t *)
{
}

void alignSensorViaRotation(vector3_t *, sensor_align_e)
{
}

float vector3Norm(const vector3_t *)
{
    return 0.0f;
}

float gyroGetFilteredDownsampled(int)
{
    return 0.0f;
}

void beeper(beeperMode_e)
{
}

timeUs_t micros(void)
{
    return 0;
}

void saveConfigAndNotify(void)
{
}

void schedulerIgnoreTaskExecRate(void)
{
    schedulerIgnoreCount++;
}

}
