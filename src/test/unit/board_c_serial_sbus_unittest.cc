/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Betaflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Betaflight.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

#include <array>

extern "C" {
#include "platform.h"

#include "build/debug.h"

#include "drivers/serial.h"
#include "drivers/time.h"

#include "io/serial.h"

#include "pg/rx.h"

#include "rx/rx.h"
#include "rx/sbus.h"
#include "rx/sbus_channels.h"

#include "telemetry/telemetry.h"
}

#include "gtest/gtest.h"

namespace {

constexpr uint32_t SBUS_NORMAL_BAUD = 100000;
constexpr uint32_t SBUS_FAST_BAUD = 200000;
constexpr size_t SBUS_WIRE_FRAME_SIZE = 25;
constexpr uint8_t SBUS_SYNC_BYTE = 0x0F;
constexpr uint8_t SBUS_FLAG_CHANNEL_17_TEST = 1U << 0;
constexpr uint8_t SBUS_FLAG_CHANNEL_18_TEST = 1U << 1;
constexpr unsigned DEBUG_SBUS_FRAME_FLAGS_TEST = 0;
constexpr unsigned DEBUG_SBUS_FRAME_TIME_TEST = 2;

timeUs_t fakeNowUs;
timeUs_t nextTestEpochUs;
serialPort_t fakeSerialPort;
serialPortConfig_t fakePortConfig;
const serialPortConfig_t *findPortResult;
bool openSucceeds;
bool sharedPortResult;
unsigned findPortCalls;
unsigned openCalls;
unsigned sharedPortCalls;
serialReceiveCallbackPtr capturedRxCallback;
void *capturedRxCallbackData;
serialPortIdentifier_e capturedIdentifier;
serialPortFunction_e capturedFunction;
uint32_t capturedBaudRate;
portMode_e capturedMode;
portOptions_e capturedOptions;

std::array<uint8_t, SBUS_WIRE_FRAME_SIZE> makeSbusFrame(
    const std::array<uint16_t, 16> &analogChannels,
    uint8_t flags,
    uint8_t endByte = 0)
{
    std::array<uint8_t, SBUS_WIRE_FRAME_SIZE> frame = {};
    frame[0] = SBUS_SYNC_BYTE;

    unsigned bitOffset = 0;
    for (const uint16_t channel : analogChannels) {
        const uint16_t value = channel & 0x07FFU;
        for (unsigned bit = 0; bit < 11; bit++) {
            if (value & (1U << bit)) {
                const unsigned wireBit = bitOffset + bit;
                frame[1 + wireBit / 8] |= 1U << (wireBit % 8);
            }
        }
        bitOffset += 11;
    }

    frame[23] = flags;
    frame[24] = endByte;
    return frame;
}

std::array<uint16_t, 16> channelPattern(uint16_t first = 173)
{
    return {
        first, 992, 1812, 173,
        992, 1812, 173, 992,
        1812, 173, 992, 1812,
        173, 992, 1812, 173,
    };
}

} // namespace

extern "C" {

int16_t debug[DEBUG16_VALUE_COUNT];
uint8_t debugMode;
rssiSource_e rssiSource;
serialPort_t *telemetrySharedPort;

timeUs_t microsISR(void)
{
    return fakeNowUs;
}

const serialPortConfig_t *findSerialPortConfig(serialPortFunction_e function)
{
    findPortCalls++;
    EXPECT_EQ(FUNCTION_RX_SERIAL, function);
    return findPortResult;
}

bool telemetryCheckRxPortShared(const serialPortConfig_t *portConfig, const SerialRXType serialrxProvider)
{
    sharedPortCalls++;
    EXPECT_EQ(findPortResult, portConfig);
    EXPECT_EQ(SERIALRX_SBUS, serialrxProvider);
    return sharedPortResult;
}

serialPort_t *openSerialPort(
    serialPortIdentifier_e identifier,
    serialPortFunction_e function,
    serialReceiveCallbackPtr rxCallback,
    void *rxCallbackData,
    uint32_t baudRate,
    portMode_e mode,
    portOptions_e options)
{
    openCalls++;
    capturedIdentifier = identifier;
    capturedFunction = function;
    capturedRxCallback = rxCallback;
    capturedRxCallbackData = rxCallbackData;
    capturedBaudRate = baudRate;
    capturedMode = mode;
    capturedOptions = options;
    return openSucceeds ? &fakeSerialPort : nullptr;
}

} // extern "C"

namespace {

class BoardCSerialSbusTest : public ::testing::Test {
protected:
    rxConfig_t rxConfig = {};
    rxRuntimeState_t runtime = {};
    timeUs_t testEpochUs;

    void SetUp() override
    {
        nextTestEpochUs += 100000U;
        testEpochUs = nextTestEpochUs;
        fakeNowUs = testEpochUs;
        fakeSerialPort = {};
        fakePortConfig = {};
        fakePortConfig.identifier = SERIAL_PORT_USART2;
        fakePortConfig.functionMask = FUNCTION_RX_SERIAL;
        findPortResult = &fakePortConfig;
        openSucceeds = true;
        sharedPortResult = false;
        findPortCalls = 0;
        openCalls = 0;
        sharedPortCalls = 0;
        capturedRxCallback = nullptr;
        capturedRxCallbackData = nullptr;
        capturedIdentifier = SERIAL_PORT_NONE;
        capturedFunction = FUNCTION_NONE;
        capturedBaudRate = 0;
        capturedMode = MODE_RX;
        capturedOptions = SERIAL_NOT_INVERTED;
        telemetrySharedPort = nullptr;
        rssiSource = RSSI_SOURCE_NONE;
        debugMode = DEBUG_NONE;

        rxConfig.midrc = 1500;
        runtime.serialrxProvider = SERIALRX_SBUS;
    }

    bool initialize()
    {
        return sbusInit(&rxConfig, &runtime);
    }

    void assertOpenContract() const
    {
        ASSERT_NE(nullptr, capturedRxCallback);
        ASSERT_NE(nullptr, capturedRxCallbackData);
        EXPECT_EQ(SERIAL_PORT_USART2, capturedIdentifier);
        EXPECT_EQ(FUNCTION_RX_SERIAL, capturedFunction);
    }

    void sendFrame(
        const std::array<uint8_t, SBUS_WIRE_FRAME_SIZE> &frame,
        timeUs_t startAtUs,
        timeDelta_t finalByteDeltaUs = 0)
    {
        ASSERT_NE(nullptr, capturedRxCallback);
        fakeNowUs = startAtUs;
        for (size_t index = 0; index + 1 < frame.size(); index++) {
            capturedRxCallback(frame[index], capturedRxCallbackData);
        }
        fakeNowUs = startAtUs + finalByteDeltaUs;
        capturedRxCallback(frame.back(), capturedRxCallbackData);
    }

    void forceParserIdleBeforeClockJump()
    {
        ASSERT_NE(nullptr, capturedRxCallback);
        fakeNowUs = testEpochUs + 5000U;
        capturedRxCallback(0x55, capturedRxCallbackData);
    }
};

TEST_F(BoardCSerialSbusTest, InitSelectsNormalAndFastBaudWithBoardCSerialOptions)
{
    ASSERT_TRUE(initialize());
    assertOpenContract();
    EXPECT_EQ(1U, findPortCalls);
    EXPECT_EQ(1U, sharedPortCalls);
    EXPECT_EQ(1U, openCalls);
    EXPECT_EQ(SBUS_NORMAL_BAUD, capturedBaudRate);
    EXPECT_EQ(MODE_RX, capturedMode);
    EXPECT_EQ(static_cast<portOptions_e>(SERIAL_STOPBITS_2 | SERIAL_PARITY_EVEN | SERIAL_INVERTED), capturedOptions);
    EXPECT_EQ(nullptr, telemetrySharedPort);

    rxConfig.sbus_baud_fast = true;
    rxConfig.serialrx_inverted = true;
    rxConfig.halfDuplex = true;
    rxConfig.rssi_src_frame_errors = true;
    sharedPortResult = true;
    findPortCalls = 0;
    sharedPortCalls = 0;
    openCalls = 0;

    ASSERT_TRUE(initialize());
    assertOpenContract();
    EXPECT_EQ(1U, findPortCalls);
    EXPECT_EQ(1U, sharedPortCalls);
    EXPECT_EQ(1U, openCalls);
    EXPECT_EQ(SBUS_FAST_BAUD, capturedBaudRate);
    EXPECT_EQ(MODE_RXTX, capturedMode);
    EXPECT_EQ(static_cast<portOptions_e>(SERIAL_STOPBITS_2 | SERIAL_PARITY_EVEN | SERIAL_BIDIR), capturedOptions);
    EXPECT_EQ(&fakeSerialPort, telemetrySharedPort);
    EXPECT_EQ(RSSI_SOURCE_FRAME_ERRORS, rssiSource);
}

TEST_F(BoardCSerialSbusTest, MissingPortAndOpenFailureRemainFailClosed)
{
    findPortResult = nullptr;
    EXPECT_FALSE(initialize());
    EXPECT_EQ(1U, findPortCalls);
    EXPECT_EQ(0U, sharedPortCalls);
    EXPECT_EQ(0U, openCalls);
    EXPECT_EQ(SBUS_MAX_CHANNEL, runtime.channelCount);
    ASSERT_NE(nullptr, runtime.rcReadRawFn);
    ASSERT_NE(nullptr, runtime.rcFrameStatusFn);
    EXPECT_FLOAT_EQ(1500.0f, runtime.rcReadRawFn(&runtime, 0));

    findPortResult = &fakePortConfig;
    openSucceeds = false;
    sharedPortResult = true;
    rxConfig.rssi_src_frame_errors = true;
    findPortCalls = 0;

    EXPECT_FALSE(initialize());
    assertOpenContract();
    EXPECT_EQ(1U, findPortCalls);
    EXPECT_EQ(1U, sharedPortCalls);
    EXPECT_EQ(1U, openCalls);
    EXPECT_EQ(nullptr, telemetrySharedPort);
    EXPECT_EQ(RSSI_SOURCE_FRAME_ERRORS, rssiSource);
}

TEST_F(BoardCSerialSbusTest, UnpacksAnalogChannelsAndAllDigitalFlagCombinations)
{
    ASSERT_TRUE(initialize());
    const std::array<uint16_t, 16> analog = channelPattern();

    for (uint8_t digitalFlags = 0; digitalFlags < 4; digitalFlags++) {
        const timeUs_t frameStart = testEpochUs + 10000U + 5000U * digitalFlags;
        sendFrame(makeSbusFrame(analog, digitalFlags), frameStart);

        EXPECT_EQ(RX_FRAME_COMPLETE, runtime.rcFrameStatusFn(&runtime));
        EXPECT_EQ(RX_FRAME_PENDING, runtime.rcFrameStatusFn(&runtime));
        EXPECT_EQ(frameStart, runtime.lastRcFrameTimeUs);
        for (unsigned channel = 0; channel < analog.size(); channel++) {
            EXPECT_EQ(analog[channel], runtime.channelData[channel]);
        }
        EXPECT_EQ((digitalFlags & SBUS_FLAG_CHANNEL_17_TEST) ? 1812 : 173, runtime.channelData[16]);
        EXPECT_EQ((digitalFlags & SBUS_FLAG_CHANNEL_18_TEST) ? 1812 : 173, runtime.channelData[17]);
    }

    EXPECT_FLOAT_EQ(988.125f, runtime.rcReadRawFn(&runtime, 0));
    EXPECT_FLOAT_EQ(1500.0f, runtime.rcReadRawFn(&runtime, 1));
    EXPECT_FLOAT_EQ(2012.5f, runtime.rcReadRawFn(&runtime, 2));
}

TEST_F(BoardCSerialSbusTest, SignalLossFailsafeAndBothFlagsPreserveLastGoodFrameTime)
{
    ASSERT_TRUE(initialize());
    std::array<uint16_t, 16> analog = channelPattern();
    const timeUs_t goodFrameStart = testEpochUs + 10000U;
    sendFrame(makeSbusFrame(analog, 0), goodFrameStart);
    ASSERT_EQ(RX_FRAME_COMPLETE, runtime.rcFrameStatusFn(&runtime));
    ASSERT_EQ(goodFrameStart, runtime.lastRcFrameTimeUs);

    analog[0] = 1812;
    sendFrame(makeSbusFrame(analog, SBUS_FLAG_SIGNAL_LOSS), goodFrameStart + 5000U);
    EXPECT_EQ(RX_FRAME_COMPLETE | RX_FRAME_DROPPED, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(goodFrameStart, runtime.lastRcFrameTimeUs);
    EXPECT_EQ(1812, runtime.channelData[0]);

    analog[0] = 992;
    sendFrame(makeSbusFrame(analog, SBUS_FLAG_FAILSAFE_ACTIVE), goodFrameStart + 10000U);
    EXPECT_EQ(RX_FRAME_COMPLETE | RX_FRAME_FAILSAFE, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(goodFrameStart, runtime.lastRcFrameTimeUs);
    EXPECT_EQ(992, runtime.channelData[0]);

    analog[0] = 173;
    sendFrame(makeSbusFrame(analog, SBUS_FLAG_SIGNAL_LOSS | SBUS_FLAG_FAILSAFE_ACTIVE), goodFrameStart + 15000U);
    EXPECT_EQ(RX_FRAME_COMPLETE | RX_FRAME_FAILSAFE, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(goodFrameStart, runtime.lastRcFrameTimeUs);
    EXPECT_EQ(173, runtime.channelData[0]);
}

TEST_F(BoardCSerialSbusTest, FrameDurationBoundaryAccepts3499And3500ButResynchronizesAt3501)
{
    ASSERT_TRUE(initialize());
    const std::array<uint16_t, 16> analog = channelPattern();
    const auto frame = makeSbusFrame(analog, 0);

    const timeUs_t firstStart = testEpochUs + 10000U;
    sendFrame(frame, firstStart, 3499);
    EXPECT_EQ(RX_FRAME_COMPLETE, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(firstStart, runtime.lastRcFrameTimeUs);

    const timeUs_t secondStart = firstStart + 10000U;
    sendFrame(frame, secondStart, 3500);
    EXPECT_EQ(RX_FRAME_COMPLETE, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(secondStart, runtime.lastRcFrameTimeUs);

    const timeUs_t rejectedStart = secondStart + 10000U;
    sendFrame(frame, rejectedStart, 3501);
    EXPECT_EQ(RX_FRAME_PENDING, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(secondStart, runtime.lastRcFrameTimeUs);

    const timeUs_t recoveryStart = rejectedStart + 8000U;
    sendFrame(frame, recoveryStart);
    EXPECT_EQ(RX_FRAME_COMPLETE, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(recoveryStart, runtime.lastRcFrameTimeUs);
}

TEST_F(BoardCSerialSbusTest, NoiseAndTruncationRequireFreshSyncBeforeRecovery)
{
    ASSERT_TRUE(initialize());
    const auto frame = makeSbusFrame(channelPattern(), 0);

    fakeNowUs = testEpochUs + 5000U;
    capturedRxCallback(0x55, capturedRxCallbackData);
    capturedRxCallback(0xAA, capturedRxCallbackData);
    EXPECT_EQ(RX_FRAME_PENDING, runtime.rcFrameStatusFn(&runtime));

    const timeUs_t partialStart = testEpochUs + 10000U;
    fakeNowUs = partialStart;
    for (size_t index = 0; index < 12; index++) {
        capturedRxCallback(frame[index], capturedRxCallbackData);
    }
    EXPECT_EQ(RX_FRAME_PENDING, runtime.rcFrameStatusFn(&runtime));

    fakeNowUs = partialStart + 3501U;
    capturedRxCallback(0x55, capturedRxCallbackData);
    EXPECT_EQ(RX_FRAME_PENDING, runtime.rcFrameStatusFn(&runtime));

    const timeUs_t recoveryStart = partialStart + 8000U;
    sendFrame(frame, recoveryStart);
    EXPECT_EQ(RX_FRAME_COMPLETE, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(recoveryStart, runtime.lastRcFrameTimeUs);
}

TEST_F(BoardCSerialSbusTest, CompleteFrameIgnoresImmediateTrailingByteAndPublishesDebugValues)
{
    ASSERT_TRUE(initialize());
    debugMode = DEBUG_SBUS;
    const uint8_t flags = SBUS_FLAG_CHANNEL_17_TEST | SBUS_FLAG_CHANNEL_18_TEST;
    const auto frame = makeSbusFrame(channelPattern(), flags);
    const timeUs_t frameStart = testEpochUs + 10000U;

    sendFrame(frame, frameStart, 2500U);
    EXPECT_EQ(2500, debug[DEBUG_SBUS_FRAME_TIME_TEST]);

    fakeNowUs = frameStart + 3000U;
    capturedRxCallback(0x55, capturedRxCallbackData);

    EXPECT_EQ(RX_FRAME_COMPLETE, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(flags, debug[DEBUG_SBUS_FRAME_FLAGS_TEST]);
    EXPECT_EQ(frameStart, runtime.lastRcFrameTimeUs);
    EXPECT_EQ(RX_FRAME_PENDING, runtime.rcFrameStatusFn(&runtime));

    const timeUs_t recoveryStart = frameStart + 7000U;
    sendFrame(frame, recoveryStart);
    EXPECT_EQ(RX_FRAME_COMPLETE, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(recoveryStart, runtime.lastRcFrameTimeUs);
}

TEST_F(BoardCSerialSbusTest, FrameDurationBoundaryUsesWrapSafeTimeArithmetic)
{
    ASSERT_TRUE(initialize());
    forceParserIdleBeforeClockJump();
    const auto frame = makeSbusFrame(channelPattern(), 0);
    const timeUs_t frameStart = UINT32_MAX - 1000U;

    sendFrame(frame, frameStart, 3500);

    EXPECT_EQ(RX_FRAME_COMPLETE, runtime.rcFrameStatusFn(&runtime));
    EXPECT_EQ(frameStart, runtime.lastRcFrameTimeUs);
    EXPECT_EQ(RX_FRAME_PENDING, runtime.rcFrameStatusFn(&runtime));

    fakeNowUs = frameStart + 3501U;
    capturedRxCallback(0x55, capturedRxCallbackData);
    EXPECT_EQ(RX_FRAME_PENDING, runtime.rcFrameStatusFn(&runtime));
}

} // namespace
