/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <utility>
#include <vector>

extern "C" {
#include "platform.h"

#include "drivers/bus.h"
#include "drivers/compass/compass.h"
#include "drivers/compass/compass_ist8310.h"
#include "drivers/time.h"
}

#include "gtest/gtest.h"

namespace {

constexpr uint8_t IST8310_ADDRESS = 0x0E;
constexpr uint8_t IST8310_REG_WAI = 0x00;
constexpr uint8_t IST8310_REG_STAT1 = 0x02;
constexpr uint8_t IST8310_REG_DATA = 0x03;
constexpr uint8_t IST8310_REG_CNTRL1 = 0x0A;
constexpr uint8_t IST8310_REG_AVERAGE = 0x41;
constexpr uint8_t IST8310_REG_PDCNTL = 0x42;

struct AsyncReadResult {
    bool starts;
    uint8_t expectedRegister;
    std::vector<uint8_t> bytes;
    std::size_t copyLength;
};

bool waiReadAck;
uint8_t waiValue;
std::deque<bool> synchronousWriteResults;
std::deque<bool> asynchronousWriteResults;
std::deque<AsyncReadResult> asynchronousReadResults;
std::vector<std::pair<uint8_t, uint8_t>> synchronousWrites;
std::vector<std::pair<uint8_t, uint8_t>> asynchronousWrites;
std::vector<uint8_t> asynchronousReads;
std::vector<timeMs_t> delays;
unsigned unexpectedBusOperations;
unsigned deviceRegisterCount;
busDevice_t i2cBus;
magDev_t device;

std::array<uint8_t, 6> encodeSample(int16_t x, int16_t y, int16_t z)
{
    const std::array<int16_t, 3> raw = {x, y, z};
    std::array<uint8_t, 6> encoded = {};
    for (unsigned axis = 0; axis < raw.size(); axis++) {
        encoded[2 * axis] = static_cast<uint8_t>(raw[axis]);
        encoded[2 * axis + 1] = static_cast<uint8_t>(static_cast<uint16_t>(raw[axis]) >> 8);
    }
    return encoded;
}

void resetBusFixture()
{
    waiReadAck = true;
    waiValue = 0x10;
    synchronousWriteResults.clear();
    asynchronousWriteResults.clear();
    asynchronousReadResults.clear();
    synchronousWrites.clear();
    asynchronousWrites.clear();
    asynchronousReads.clear();
    delays.clear();
    unexpectedBusOperations = 0;
    deviceRegisterCount = 0;

    std::memset(&i2cBus, 0, sizeof(i2cBus));
    std::memset(&device, 0, sizeof(device));
    i2cBus.busType = BUS_TYPE_I2C;
    device.dev.bus = &i2cBus;
}

void detectAndInitialize()
{
    ASSERT_TRUE(ist8310Detect(&device));
    ASSERT_NE(nullptr, device.init);
    ASSERT_NE(nullptr, device.read);
    ASSERT_TRUE(device.init(&device));
    EXPECT_EQ(1U, deviceRegisterCount);
    EXPECT_FALSE(device.busError);

    synchronousWrites.clear();
    delays.clear();
}

void queueStatus(uint8_t status, bool starts = true, std::size_t copyLength = 1)
{
    asynchronousReadResults.push_back({starts, IST8310_REG_STAT1, {status}, copyLength});
}

void queueData(const std::array<uint8_t, 6> &data, bool starts = true, std::size_t copyLength = 6)
{
    asynchronousReadResults.push_back({starts, IST8310_REG_DATA, std::vector<uint8_t>(data.begin(), data.end()), copyLength});
}

bool readAfterCompletion(int16_t *sample, bool completionError = false)
{
    device.busError = completionError;
    const bool result = device.read(&device, sample);
    device.busError = false;
    return result;
}

class Ist8310Test : public testing::Test {
protected:
    void SetUp() override
    {
        resetBusFixture();
    }
};

} // namespace

TEST_F(Ist8310Test, DetectUsesOfficialDefaultAddressAndInstallsCallbacks)
{
    EXPECT_TRUE(ist8310Detect(&device));
    EXPECT_EQ(IST8310_ADDRESS, device.dev.busType_u.i2c.address);
    EXPECT_NE(nullptr, device.init);
    EXPECT_NE(nullptr, device.read);
    EXPECT_EQ(0U, unexpectedBusOperations);
}

TEST_F(Ist8310Test, DetectRejectsWrongIdentity)
{
    waiValue = 0x11;

    EXPECT_FALSE(ist8310Detect(&device));
    EXPECT_EQ(nullptr, device.init);
    EXPECT_EQ(nullptr, device.read);
}

TEST_F(Ist8310Test, InitPublishesOdrOnlyAfterAllWritesSucceed)
{
    ASSERT_TRUE(ist8310Detect(&device));
    synchronousWriteResults = {true, false};
    device.busError = true;

    EXPECT_FALSE(device.init(&device));
    EXPECT_FALSE(device.busError);
    EXPECT_EQ(0U, device.magOdrHz);
    ASSERT_EQ(2U, synchronousWrites.size());
    EXPECT_EQ(std::make_pair(IST8310_REG_AVERAGE, static_cast<uint8_t>(0x24)), synchronousWrites[0]);
    EXPECT_EQ(std::make_pair(IST8310_REG_PDCNTL, static_cast<uint8_t>(0xC0)), synchronousWrites[1]);
    EXPECT_EQ(2U, delays.size());
    EXPECT_EQ(1U, deviceRegisterCount);
}

TEST_F(Ist8310Test, InitTriggerFailureLeavesOdrUnavailable)
{
    ASSERT_TRUE(ist8310Detect(&device));
    synchronousWriteResults = {true, true, false};

    EXPECT_FALSE(device.init(&device));
    EXPECT_EQ(0U, device.magOdrHz);
    ASSERT_EQ(3U, synchronousWrites.size());
    EXPECT_EQ(std::make_pair(IST8310_REG_CNTRL1, static_cast<uint8_t>(0x01)), synchronousWrites.back());
}

TEST_F(Ist8310Test, ValidDataPublishesImmediatelyAndStartsNextTrigger)
{
    detectAndInitialize();
    queueStatus(0x01);
    queueData(encodeSample(0x0102, 0x0304, 0x0506));
    asynchronousWriteResults.push_back(true);
    int16_t sample[3] = {};

    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_TRUE(readAfterCompletion(sample));

    EXPECT_EQ(0x0102 * 3, sample[0]);
    EXPECT_EQ(-0x0304 * 3, sample[1]);
    EXPECT_EQ(0x0506 * 3, sample[2]);
    ASSERT_EQ(1U, asynchronousWrites.size());
    EXPECT_EQ(std::make_pair(IST8310_REG_CNTRL1, static_cast<uint8_t>(0x01)), asynchronousWrites.front());
    EXPECT_EQ(0U, unexpectedBusOperations);
}

TEST_F(Ist8310Test, TriggerStartFailuresNeverRepublishValidatedData)
{
    detectAndInitialize();
    queueStatus(0x01);
    queueData(encodeSample(1, 2, 3));
    asynchronousWriteResults = {false, false, true};
    int16_t sample[3] = {};

    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_TRUE(readAfterCompletion(sample));
    EXPECT_EQ(3, sample[0]);
    EXPECT_EQ(-6, sample[1]);
    EXPECT_EQ(9, sample[2]);

    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    queueStatus(0x00);
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_EQ(3U, asynchronousWrites.size());
}

TEST_F(Ist8310Test, TriggerCompletionErrorRetriesWithoutRepublishing)
{
    detectAndInitialize();
    queueStatus(0x01);
    queueData(encodeSample(1, 2, 3));
    asynchronousWriteResults = {true, true};
    int16_t sample[3] = {};

    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_TRUE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample, true));
    queueStatus(0x00);
    EXPECT_FALSE(readAfterCompletion(sample));

    EXPECT_EQ(2U, asynchronousWrites.size());
    EXPECT_EQ(3, sample[0]);
    EXPECT_EQ(-6, sample[1]);
    EXPECT_EQ(9, sample[2]);
}

TEST_F(Ist8310Test, ErasedTriggerErrorRecoversAfterBoundWithoutRepublishing)
{
    detectAndInitialize();
    queueStatus(0x01);
    queueData(encodeSample(1, 2, 3));
    asynchronousWriteResults = {true, true};
    int16_t sample[3] = {};

    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_TRUE(readAfterCompletion(sample));

    queueStatus(0x00);
    EXPECT_FALSE(readAfterCompletion(sample));
    for (unsigned poll = 0; poll < 15; poll++) {
        if (poll + 1 < 15) {
            queueStatus(0x00);
        }
        EXPECT_FALSE(readAfterCompletion(sample)) << "poll=" << poll;
    }

    EXPECT_EQ(2U, asynchronousWrites.size());
    EXPECT_EQ(17U, asynchronousReads.size());
    EXPECT_EQ(3, sample[0]);
    EXPECT_EQ(-6, sample[1]);
    EXPECT_EQ(9, sample[2]);
}

TEST_F(Ist8310Test, FailedStatusReadStartDoesNotConsumeRetryBudget)
{
    detectAndInitialize();
    for (unsigned attempt = 0; attempt < 15; attempt++) {
        queueStatus(0x01, false);
    }
    int16_t sample[3] = {};

    for (unsigned attempt = 0; attempt < 15; attempt++) {
        EXPECT_FALSE(readAfterCompletion(sample));
    }

    queueStatus(0x01);
    queueData(encodeSample(1, 2, 3));
    asynchronousWriteResults.push_back(true);
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_TRUE(readAfterCompletion(sample));
    EXPECT_EQ(3, sample[0]);
    EXPECT_EQ(-6, sample[1]);
    EXPECT_EQ(9, sample[2]);
    EXPECT_EQ(17U, asynchronousReads.size());
    EXPECT_EQ(1U, asynchronousWrites.size());
}

TEST_F(Ist8310Test, StatusCompletionErrorsDoNotConsumeRetryBudget)
{
    detectAndInitialize();
    queueStatus(0x00);
    int16_t sample[3] = {};
    EXPECT_FALSE(readAfterCompletion(sample));

    for (unsigned attempt = 0; attempt < 15; attempt++) {
        queueStatus(attempt + 1 == 15 ? 0x01 : 0x00);
        EXPECT_FALSE(readAfterCompletion(sample, true));
    }

    queueData(encodeSample(1, 2, 3));
    EXPECT_FALSE(readAfterCompletion(sample));
    asynchronousWriteResults.push_back(true);
    EXPECT_TRUE(readAfterCompletion(sample));
    EXPECT_EQ(0U, unexpectedBusOperations);
    EXPECT_EQ(1U, asynchronousWrites.size());
}

TEST_F(Ist8310Test, StatusDestinationIsClearedBeforeEveryRead)
{
    detectAndInitialize();
    queueStatus(0x01, true, 0);
    int16_t sample[3] = {};
    EXPECT_FALSE(readAfterCompletion(sample));

    queueStatus(0x00);
    EXPECT_FALSE(readAfterCompletion(sample));

    ASSERT_EQ(2U, asynchronousReads.size());
    EXPECT_EQ(IST8310_REG_STAT1, asynchronousReads[0]);
    EXPECT_EQ(IST8310_REG_STAT1, asynchronousReads[1]);
    EXPECT_TRUE(asynchronousWrites.empty());
}

TEST_F(Ist8310Test, FailedDataReadStartRetriesReadySample)
{
    detectAndInitialize();
    queueStatus(0x01);
    queueData(encodeSample(1, 2, 3), false);
    queueData(encodeSample(1, 2, 3));
    asynchronousWriteResults.push_back(true);
    int16_t sample[3] = {};

    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_TRUE(readAfterCompletion(sample));

    ASSERT_EQ(3U, asynchronousReads.size());
    EXPECT_EQ(IST8310_REG_STAT1, asynchronousReads[0]);
    EXPECT_EQ(IST8310_REG_DATA, asynchronousReads[1]);
    EXPECT_EQ(IST8310_REG_DATA, asynchronousReads[2]);
    EXPECT_EQ(3, sample[0]);
    EXPECT_EQ(-6, sample[1]);
    EXPECT_EQ(9, sample[2]);
}

TEST_F(Ist8310Test, DataCompletionErrorDropsBufferAndRetriggers)
{
    detectAndInitialize();
    queueStatus(0x01);
    queueData(encodeSample(1, 2, 3));
    asynchronousWriteResults.push_back(true);
    int16_t sample[3] = {101, 202, 303};

    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample, true));

    EXPECT_EQ(101, sample[0]);
    EXPECT_EQ(202, sample[1]);
    EXPECT_EQ(303, sample[2]);
    EXPECT_EQ(1U, asynchronousWrites.size());
}

TEST_F(Ist8310Test, EveryPartialDataWriteIsRejectedWithoutBackendError)
{
    for (std::size_t copied = 0; copied < 6; copied++) {
        SCOPED_TRACE(copied);
        resetBusFixture();
        detectAndInitialize();
        queueStatus(0x01);
        queueData(encodeSample(1, 2, 3), true, copied);
        asynchronousWriteResults.push_back(true);
        int16_t sample[3] = {101, 202, 303};

        EXPECT_FALSE(readAfterCompletion(sample));
        EXPECT_FALSE(readAfterCompletion(sample));
        EXPECT_FALSE(readAfterCompletion(sample));
        EXPECT_EQ(101, sample[0]);
        EXPECT_EQ(202, sample[1]);
        EXPECT_EQ(303, sample[2]);
        EXPECT_EQ(1U, asynchronousWrites.size());
    }
}

TEST_F(Ist8310Test, OfficialRawAxisLimitsRejectOutOfRangeData)
{
    const std::array<std::array<int16_t, 3>, 6> invalid = {{
        {{5335, 0, 0}}, {{-5335, 0, 0}},
        {{0, 5335, 0}}, {{0, -5335, 0}},
        {{0, 0, 8335}}, {{0, 0, -8335}},
    }};

    for (const auto &raw : invalid) {
        SCOPED_TRACE(testing::Message() << raw[0] << ',' << raw[1] << ',' << raw[2]);
        resetBusFixture();
        detectAndInitialize();
        queueStatus(0x01);
        queueData(encodeSample(raw[0], raw[1], raw[2]));
        asynchronousWriteResults.push_back(true);
        int16_t sample[3] = {101, 202, 303};

        EXPECT_FALSE(readAfterCompletion(sample));
        EXPECT_FALSE(readAfterCompletion(sample));
        EXPECT_FALSE(readAfterCompletion(sample));
        EXPECT_EQ(101, sample[0]);
        EXPECT_EQ(202, sample[1]);
        EXPECT_EQ(303, sample[2]);
    }
}

TEST_F(Ist8310Test, OfficialRawAxisLimitsAreInclusive)
{
    detectAndInitialize();
    queueStatus(0x01);
    queueData(encodeSample(-5334, 5334, -8334));
    asynchronousWriteResults.push_back(true);
    int16_t sample[3] = {};

    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_TRUE(readAfterCompletion(sample));
    EXPECT_EQ(-5334 * 3, sample[0]);
    EXPECT_EQ(-5334 * 3, sample[1]);
    EXPECT_EQ(-8334 * 3, sample[2]);
}

TEST_F(Ist8310Test, NoReadyTimeoutRetriggersWithoutPublishingData)
{
    detectAndInitialize();
    queueStatus(0x00);
    asynchronousWriteResults.push_back(true);
    int16_t sample[3] = {101, 202, 303};

    EXPECT_FALSE(readAfterCompletion(sample));
    for (unsigned poll = 0; poll < 15; poll++) {
        if (poll + 1 < 15) {
            queueStatus(0x00);
        }
        EXPECT_FALSE(readAfterCompletion(sample));
    }

    EXPECT_EQ(101, sample[0]);
    EXPECT_EQ(202, sample[1]);
    EXPECT_EQ(303, sample[2]);
    EXPECT_EQ(15U, asynchronousReads.size());
    EXPECT_EQ(1U, asynchronousWrites.size());
    EXPECT_EQ(0U, unexpectedBusOperations);
}

TEST_F(Ist8310Test, ReinitDiscardsPendingStateAndCompletionError)
{
    detectAndInitialize();
    queueStatus(0x01);
    queueData(encodeSample(1, 2, 3));
    int16_t sample[3] = {9, 8, 7};

    EXPECT_FALSE(readAfterCompletion(sample));
    EXPECT_FALSE(readAfterCompletion(sample));
    device.busError = true;
    ASSERT_TRUE(device.init(&device));
    EXPECT_FALSE(device.busError);
    EXPECT_EQ(2U, deviceRegisterCount);

    asynchronousReadResults.clear();
    asynchronousReads.clear();
    asynchronousWrites.clear();
    queueStatus(0x00);
    EXPECT_FALSE(readAfterCompletion(sample));
    ASSERT_EQ(1U, asynchronousReads.size());
    EXPECT_EQ(IST8310_REG_STAT1, asynchronousReads.front());
    EXPECT_TRUE(asynchronousWrites.empty());
    EXPECT_EQ(9, sample[0]);
    EXPECT_EQ(8, sample[1]);
    EXPECT_EQ(7, sample[2]);
}

extern "C" {

void delay(timeMs_t duration)
{
    delays.push_back(duration);
}

bool busReadRegisterBuffer(const extDevice_t *, uint8_t reg, uint8_t *data, uint8_t length)
{
    if (reg != IST8310_REG_WAI || length != 1) {
        unexpectedBusOperations++;
        return false;
    }

    *data = waiValue;
    return waiReadAck;
}

bool busReadRegisterBufferStart(const extDevice_t *, uint8_t reg, uint8_t *data, uint8_t length)
{
    asynchronousReads.push_back(reg);
    if (asynchronousReadResults.empty()) {
        unexpectedBusOperations++;
        return false;
    }

    AsyncReadResult result = asynchronousReadResults.front();
    asynchronousReadResults.pop_front();
    if (result.expectedRegister != reg || result.bytes.size() != length || result.copyLength > length) {
        unexpectedBusOperations++;
        return false;
    }

    if (!result.starts) {
        return false;
    }

    std::memcpy(data, result.bytes.data(), result.copyLength);
    return true;
}

bool busWriteRegister(const extDevice_t *, uint8_t reg, uint8_t value)
{
    synchronousWrites.emplace_back(reg, value);
    if (synchronousWriteResults.empty()) {
        return true;
    }

    const bool result = synchronousWriteResults.front();
    synchronousWriteResults.pop_front();
    return result;
}

bool busWriteRegisterStart(const extDevice_t *, uint8_t reg, uint8_t value)
{
    asynchronousWrites.emplace_back(reg, value);
    if (asynchronousWriteResults.empty()) {
        unexpectedBusOperations++;
        return false;
    }

    const bool result = asynchronousWriteResults.front();
    asynchronousWriteResults.pop_front();
    return result;
}

void busDeviceRegister(const extDevice_t *)
{
    deviceRegisterCount++;
}

}
