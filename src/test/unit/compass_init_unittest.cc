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

#include "flight/imu.h"

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
timeUs_t testTimeUs;

struct BusCompletion {
    bool busy;
    bool error;
};

std::deque<BusCompletion> busCompletions;

struct MagReadResult {
    bool available;
    int16_t data[XYZ_AXIS_COUNT];
};

std::deque<MagReadResult> magReadResults;

bool testMagInit(magDev_t *dev)
{
    dev->magOdrHz = magInitResult ? 100 : 0;
    return magInitResult;
}

bool testMagRead(magDev_t *dev, int16_t *data)
{
    observedReadErrors.push_back(dev->busError);
    if (magReadResults.empty()) {
        return false;
    }

    const MagReadResult result = magReadResults.front();
    magReadResults.pop_front();
    if (result.available) {
        std::memcpy(data, result.data, sizeof(result.data));
    }
    return result.available;
}

void publishMagSample(timeUs_t timestamp, int16_t x, int16_t y, int16_t z)
{
    testTimeUs = timestamp;
    magReadResults.push_back({true, {x, y, z}});
    compassUpdate(timestamp);
}

void configureCompass()
{
    std::memset(&magDev, 0, sizeof(magDev));
    std::memset(&mag, 0, sizeof(mag));
    std::memset(compassConfigMutable(), 0, sizeof(*compassConfigMutable()));
    std::memset(imuConfigMutable(), 0, sizeof(*imuConfigMutable()));

    compassConfigMutable()->mag_alignment = ALIGN_DEFAULT;
    compassConfigMutable()->mag_hardware = MAG_IST8310;
    compassConfigMutable()->mag_busType = BUS_TYPE_I2C;
    compassConfigMutable()->mag_i2c_device = I2C_DEV_TO_CFG(I2CDEV_1);
    compassConfigMutable()->mag_i2c_address = 0;
    imuConfigMutable()->trust_mag = true;
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
    magReadResults.clear();
    testTimeUs = 0;
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

TEST_F(CompassInitTest, NoPublishedSampleIsUnhealthy)
{
    ASSERT_TRUE(compassInit());

    EXPECT_FALSE(compassEnabledAndCalibrated());
}

TEST_F(CompassInitTest, SampleExpiresAtExactFreshnessBoundary)
{
    ASSERT_TRUE(compassInit());
    publishMagSample(1000, 11, 22, 33);

    testTimeUs = 500999;
    EXPECT_TRUE(compassEnabledAndCalibrated());

    testTimeUs = 501000;
    EXPECT_FALSE(compassEnabledAndCalibrated());
}

#if !defined(USE_64BIT_TIME)
TEST_F(CompassInitTest, FreshnessAgeIsWrapSafeForUint32Time)
{
    ASSERT_TRUE(compassInit());
    const timeUs_t publishedAt = UINT32_MAX - 250000U;
    publishMagSample(publishedAt, 11, 22, 33);

    testTimeUs = publishedAt + 499999U;
    EXPECT_TRUE(compassEnabledAndCalibrated());

    testTimeUs = publishedAt + 500000U;
    EXPECT_FALSE(compassEnabledAndCalibrated());
}

TEST_F(CompassInitTest, HalfRangeOldSampleCannotAppearFresh)
{
    ASSERT_TRUE(compassInit());
    publishMagSample(1000, 11, 22, 33);

    testTimeUs = 1000U + 0x80000000U;
    EXPECT_FALSE(compassEnabledAndCalibrated());
}

TEST_F(CompassInitTest, ExpiredSampleCannotResurrectAfterFullUint32Wrap)
{
    ASSERT_TRUE(compassInit());
    publishMagSample(1000, 11, 22, 33);

    testTimeUs = 501000;
    ASSERT_FALSE(compassEnabledAndCalibrated());

    testTimeUs = 1001;
    EXPECT_FALSE(compassEnabledAndCalibrated());

    publishMagSample(2000, 44, 55, 66);
    EXPECT_TRUE(compassEnabledAndCalibrated());
}
#endif

TEST_F(CompassInitTest, ReinitializationClearsPreviousFreshnessEvenOnFailure)
{
    ASSERT_TRUE(compassInit());
    publishMagSample(1000, 11, 22, 33);
    testTimeUs = 1001;
    ASSERT_TRUE(compassEnabledAndCalibrated());

    magInitResult = false;
    EXPECT_FALSE(compassInit());
    EXPECT_FALSE(compassEnabledAndCalibrated());
}

TEST_F(CompassInitTest, BusyAndCompletionErrorDoNotRefreshFreshness)
{
    ASSERT_TRUE(compassInit());
    publishMagSample(1000, 11, 22, 33);
    busCompletions = {{true, false}, {false, true}};

    testTimeUs = 400000;
    EXPECT_EQ(500U, compassUpdate(testTimeUs));
    EXPECT_TRUE(compassEnabledAndCalibrated());

    testTimeUs = 450000;
    EXPECT_EQ(1000U, compassUpdate(testTimeUs));
    ASSERT_FALSE(observedReadErrors.empty());
    EXPECT_TRUE(observedReadErrors.back());

    testTimeUs = 501000;
    EXPECT_FALSE(compassEnabledAndCalibrated());
}

TEST_F(CompassInitTest, FirstSuccessfulSampleRestoresHealthAfterStaleness)
{
    ASSERT_TRUE(compassInit());
    publishMagSample(1000, 11, 22, 33);
    testTimeUs = 501000;
    ASSERT_FALSE(compassEnabledAndCalibrated());

    publishMagSample(700000, 44, 55, 66);
    EXPECT_TRUE(compassEnabledAndCalibrated());
}

TEST_F(CompassInitTest, FreshSampleWithZeroAxisRetainsLegacyUnhealthyResult)
{
    ASSERT_TRUE(compassInit());
    publishMagSample(1000, 0, 22, -33);

    EXPECT_FALSE(compassEnabledAndCalibrated());
}

TEST_F(CompassInitTest, FreshSampleRequiresOfficialTrustMagGate)
{
    ASSERT_TRUE(compassInit());
    publishMagSample(1000, 11, 22, 33);
    ASSERT_TRUE(compassEnabledAndCalibrated());

    imuConfigMutable()->trust_mag = false;
    EXPECT_FALSE(compassEnabledAndCalibrated());

    imuConfigMutable()->trust_mag = true;
    EXPECT_TRUE(compassEnabledAndCalibrated());
}

TEST_F(CompassInitTest, FreshSampleRequiresEnabledMagSensor)
{
    ASSERT_TRUE(compassInit());
    publishMagSample(1000, 11, 22, 33);
    ASSERT_TRUE(compassEnabledAndCalibrated());

    sensorsClear(SENSOR_MAG);
    EXPECT_FALSE(compassEnabledAndCalibrated());

    sensorsSet(SENSOR_MAG);
    EXPECT_TRUE(compassEnabledAndCalibrated());
}

#if defined(USE_64BIT_TIME)
TEST_F(CompassInitTest, Simulator64FreshnessRetainsTimestampUpperBits)
{
    ASSERT_EQ(8U, sizeof(timeUs_t));
    ASSERT_TRUE(compassInit());
    const timeUs_t publishedAt = (static_cast<timeUs_t>(1) << 40) + 1234;
    publishMagSample(publishedAt, 11, 22, 33);

    testTimeUs = publishedAt + 499999;
    EXPECT_TRUE(compassEnabledAndCalibrated());

    testTimeUs = publishedAt + 500000;
    EXPECT_FALSE(compassEnabledAndCalibrated());
}
#endif

extern "C" {

gyro_t gyro;
imuConfig_t imuConfig_System;
imuConfig_t imuConfig_Copy;
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

bool sensors(uint32_t mask)
{
    return (sensorMask & mask) != 0;
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
    return testTimeUs;
}

void saveConfigAndNotify(void)
{
}

void schedulerIgnoreTaskExecRate(void)
{
    schedulerIgnoreCount++;
}

}

#if !defined(USE_64BIT_TIME)
TEST_F(CompassInitTest, Exact499999And500000FailureDoesNotRefreshAndNextSampleRecoversAcrossWrap)
{
    ASSERT_EQ(4U, sizeof(timeUs_t));
    ASSERT_TRUE(compassInit());
    const timeUs_t publishedAt = UINT32_MAX - 100000U;
    publishMagSample(publishedAt, 11, 22, 33);

    testTimeUs = publishedAt + 499999U;
    ASSERT_TRUE(compassEnabledAndCalibrated());

    magReadResults.push_back({false, {0, 0, 0}});
    testTimeUs = publishedAt + 500000U;
    EXPECT_EQ(1000U, compassUpdate(testTimeUs));
    EXPECT_FALSE(compassEnabledAndCalibrated());

    publishMagSample(publishedAt + 700000U, 44, 55, 66);
    EXPECT_TRUE(compassEnabledAndCalibrated());
}
#endif
