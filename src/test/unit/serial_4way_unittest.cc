/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software and/or
 * modify this software under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Betaflight is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this software.
 */

#include <stdint.h>

#include <array>
#include <deque>
#include <initializer_list>
#include <vector>

extern "C" {
#include "platform.h"

#include "drivers/io.h"
#include "drivers/motor.h"
#include "drivers/serial.h"
#include "drivers/time.h"

#include "io/serial_4way.h"
#include "io/serial_4way_avrootloader.h"
#include "io/serial_4way_stk500v2.h"

extern uint8_32_u DeviceInfo;
}

#include "gtest/gtest.h"

namespace {

constexpr uint8_t LOCAL_ESCAPE = 0x2F;
constexpr uint8_t REMOTE_ESCAPE = 0x2E;

constexpr uint8_t CMD_TEST_ALIVE = 0x30;
constexpr uint8_t CMD_EXIT = 0x34;
#if defined(USE_SERIAL_4WAY_BLHELI_BOOTLOADER) && defined(USE_SERIAL_4WAY_SK_BOOTLOADER)
constexpr uint8_t CMD_RESET = 0x35;
#endif
constexpr uint8_t CMD_INIT_FLASH = 0x37;
constexpr uint8_t CMD_READ = 0x3A;
constexpr uint8_t CMD_WRITE = 0x3B;
#if defined(USE_SERIAL_4WAY_BLHELI_BOOTLOADER) && defined(USE_SERIAL_4WAY_SK_BOOTLOADER)
constexpr uint8_t CMD_SET_MODE = 0x3F;
#endif

constexpr uint8_t ACK_OK = 0x00;
constexpr uint8_t ACK_INVALID_CMD = 0x02;
constexpr uint8_t ACK_INVALID_CRC = 0x03;
constexpr uint8_t ACK_GENERAL_ERROR = 0x0F;

struct ProbeResult {
    bool success;
    std::array<uint8_t, 4> deviceInfo;
};

struct Response {
    uint8_t command;
    uint8_t addressHigh;
    uint8_t addressLow;
    std::vector<uint8_t> payload;
    uint8_t ack;
};

serialPort_t testPort;
std::deque<uint8_t> serialInput;
std::vector<uint8_t> serialOutput;
std::deque<ProbeResult> blProbeResults;
std::deque<ProbeResult> stkProbeResults;
std::deque<bool> blKeepAliveResults;
std::deque<bool> stkKeepAliveResults;
std::deque<bool> blReadResults;
std::deque<bool> stkReadResults;
std::deque<bool> blWriteResults;
std::deque<bool> stkWriteResults;
std::vector<char> probeOrder;
bool allProbeEntriesWereClear;
bool injectExitAfterFirstResponse;
unsigned responseCount;
unsigned blReadCalls;
unsigned stkReadCalls;
unsigned blWriteCalls;
unsigned stkWriteCalls;
unsigned blRestartCalls;
timeUs_t testTimeUs;
timeUs_t testTimeStepUs;
unsigned emptyPollsBeforeNextByte;
unsigned emptyPollsBetweenBytes;

uint16_t crcXmodemUpdate(uint16_t crc, uint8_t data)
{
    crc ^= static_cast<uint16_t>(data) << 8;
    for (unsigned i = 0; i < 8; i++) {
        crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

void appendRequest(uint8_t command, std::initializer_list<uint8_t> payload = { 0 }, uint8_t addressHigh = 0, uint8_t addressLow = 0)
{
    std::vector<uint8_t> frame = {
        LOCAL_ESCAPE,
        command,
        addressHigh,
        addressLow,
        static_cast<uint8_t>(payload.size()),
    };
    frame.insert(frame.end(), payload.begin(), payload.end());

    uint16_t crc = 0;
    for (const uint8_t byte : frame) {
        crc = crcXmodemUpdate(crc, byte);
        serialInput.push_back(byte);
    }
    serialInput.push_back(static_cast<uint8_t>(crc >> 8));
    serialInput.push_back(static_cast<uint8_t>(crc));
}

std::vector<Response> parseResponses()
{
    std::vector<Response> responses;
    size_t offset = 0;

    while (offset < serialOutput.size()) {
        const size_t frameStart = offset;
        if (serialOutput.size() - offset < 8) {
            ADD_FAILURE() << "truncated 4-way response";
            break;
        }
        if (serialOutput[offset++] != REMOTE_ESCAPE) {
            ADD_FAILURE() << "invalid 4-way response prefix";
            break;
        }

        Response response = {};
        response.command = serialOutput[offset++];
        response.addressHigh = serialOutput[offset++];
        response.addressLow = serialOutput[offset++];
        const uint8_t encodedLength = serialOutput[offset++];
        const size_t payloadLength = encodedLength ? encodedLength : 256;
        if (serialOutput.size() - offset < payloadLength + 3) {
            ADD_FAILURE() << "truncated 4-way response payload";
            break;
        }
        response.payload.assign(serialOutput.begin() + offset, serialOutput.begin() + offset + payloadLength);
        offset += payloadLength;
        response.ack = serialOutput[offset++];

        uint16_t expectedCrc = 0;
        for (size_t i = frameStart; i < offset; i++) {
            expectedCrc = crcXmodemUpdate(expectedCrc, serialOutput[i]);
        }
        const uint16_t receivedCrc = static_cast<uint16_t>(serialOutput[offset] << 8) | serialOutput[offset + 1];
        EXPECT_EQ(expectedCrc, receivedCrc);
        offset += 2;
        responses.push_back(response);
    }

    return responses;
}

ProbeResult dirtyFailure()
{
    return { false, { 0xA5, 0x5A, 0xC3, 0x3C } };
}

#if defined(USE_SERIAL_4WAY_BLHELI_BOOTLOADER) && defined(USE_SERIAL_4WAY_SK_BOOTLOADER)
ProbeResult unknownSuccess()
{
    return { true, { 0x55, 0x66, 0x77, 0x88 } };
}
#endif

#ifdef USE_SERIAL_4WAY_BLHELI_BOOTLOADER
ProbeResult silabsSuccess()
{
    return { true, { 0xB2, 0xE8, 0x64, 0xA5 } };
}

ProbeResult blAtmelSuccess()
{
    return { true, { 0x07, 0x93, 0x00, 0xA5 } };
}
#endif

#if defined(USE_SERIAL_4WAY_BLHELI_BOOTLOADER) && defined(USE_SERIAL_4WAY_SK_BOOTLOADER)
ProbeResult armSuccess()
{
    return { true, { 0x06, 0x12, 0x34, 0xA5 } };
}
#endif

#ifdef USE_SERIAL_4WAY_SK_BOOTLOADER
ProbeResult stkSuccess()
{
    return { true, { 0x07, 0x93, 0x00, 0xA5 } };
}
#endif

bool popBool(std::deque<bool> &results, bool fallback = false)
{
    if (results.empty()) {
        return fallback;
    }
    const bool result = results.front();
    results.pop_front();
    return result;
}

uint8_t applyProbe(std::deque<ProbeResult> &results, uint8_32_u *deviceInfo, char loader)
{
    probeOrder.push_back(loader);
    if (deviceInfo->dword != 0) {
        allProbeEntriesWereClear = false;
    }
    if (results.empty()) {
        return false;
    }

    const ProbeResult result = results.front();
    results.pop_front();
    for (unsigned i = 0; i < result.deviceInfo.size(); i++) {
        deviceInfo->bytes[i] = result.deviceInfo[i];
    }
    return result.success;
}

void resetHarness()
{
    testPort = {};
    serialInput.clear();
    serialOutput.clear();
    blProbeResults.clear();
    stkProbeResults.clear();
    blKeepAliveResults.clear();
    stkKeepAliveResults.clear();
    blReadResults.clear();
    stkReadResults.clear();
    blWriteResults.clear();
    stkWriteResults.clear();
    probeOrder.clear();
    allProbeEntriesWereClear = true;
    injectExitAfterFirstResponse = false;
    responseCount = 0;
    blReadCalls = 0;
    stkReadCalls = 0;
    blWriteCalls = 0;
    stkWriteCalls = 0;
    blRestartCalls = 0;
    testTimeUs = 0;
    testTimeStepUs = 1000;
    emptyPollsBeforeNextByte = 0;
    emptyPollsBetweenBytes = 0;
    DeviceInfo.dword = 0;
    selected_esc = UINT8_MAX;
}

#if defined(USE_SERIAL_4WAY_BLHELI_BOOTLOADER) && defined(USE_SERIAL_4WAY_SK_BOOTLOADER)
void clearSessionTransport()
{
    serialInput.clear();
    serialOutput.clear();
    probeOrder.clear();
    responseCount = 0;
}
#endif

class Serial4WayTest : public testing::Test {
protected:
    void SetUp() override
    {
        resetHarness();
    }

    void beginSession()
    {
        ASSERT_EQ(4, esc4wayInit());
        EXPECT_FALSE(isMcuConnected());
        EXPECT_EQ(UINT8_MAX, selected_esc);
    }

    std::vector<Response> runSession()
    {
        esc4wayProcess(&testPort);
        return parseResponses();
    }
};

TEST_F(Serial4WayTest, FailedInitClearsAllStateAndGatesDeviceCommands)
{
    beginSession();
    for (unsigned attempt = 0; attempt < 3; attempt++) {
        stkProbeResults.push_back(dirtyFailure());
        blProbeResults.push_back(dirtyFailure());
    }

    appendRequest(CMD_INIT_FLASH, { 0 });
    appendRequest(CMD_TEST_ALIVE);
    appendRequest(CMD_READ, { 1 }, 0x12, 0x34);
    appendRequest(CMD_WRITE, { 0x5A }, 0x12, 0x34);
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(5U, responses.size());
    EXPECT_EQ(ACK_GENERAL_ERROR, responses[0].ack);
    EXPECT_EQ((std::vector<uint8_t>{ 0, 0, 0, 0 }), responses[0].payload);
    EXPECT_EQ(ACK_GENERAL_ERROR, responses[1].ack);
    EXPECT_EQ(ACK_GENERAL_ERROR, responses[2].ack);
    EXPECT_EQ(ACK_GENERAL_ERROR, responses[3].ack);
    EXPECT_EQ(ACK_OK, responses[4].ack);
    EXPECT_EQ(0U, blReadCalls);
    EXPECT_EQ(0U, stkReadCalls);
    EXPECT_EQ(0U, blWriteCalls);
    EXPECT_EQ(0U, stkWriteCalls);
    EXPECT_TRUE(allProbeEntriesWereClear);
    EXPECT_EQ(0U, DeviceInfo.dword);
    EXPECT_EQ(UINT8_MAX, selected_esc);
}

#ifdef USE_SERIAL_4WAY_BLHELI_BOOTLOADER
TEST_F(Serial4WayTest, BlAtmelSignatureSelectsAtmelModeAndDispatchesCommands)
{
    beginSession();
#ifdef USE_SERIAL_4WAY_SK_BOOTLOADER
    stkProbeResults.push_back(dirtyFailure());
#endif
    blProbeResults.push_back(blAtmelSuccess());
    blKeepAliveResults.push_back(true);
    blReadResults.push_back(true);
    blWriteResults.push_back(true);

    appendRequest(CMD_INIT_FLASH, { 0 });
    appendRequest(CMD_TEST_ALIVE);
    appendRequest(CMD_READ, { 2 }, 0x12, 0x34);
    appendRequest(CMD_WRITE, { 0x5A }, 0x12, 0x34);
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(5U, responses.size());
    ASSERT_EQ(4U, responses[0].payload.size());
    EXPECT_EQ(ACK_OK, responses[0].ack);
    EXPECT_EQ(imATM_BLB, responses[0].payload[3]);
    EXPECT_EQ(ACK_OK, responses[1].ack);
    EXPECT_EQ(ACK_OK, responses[2].ack);
    EXPECT_EQ((std::vector<uint8_t>{ 0xA0, 0xA1 }), responses[2].payload);
    EXPECT_EQ(ACK_OK, responses[3].ack);
    EXPECT_EQ(1U, blReadCalls);
    EXPECT_EQ(1U, blWriteCalls);
    EXPECT_TRUE(allProbeEntriesWereClear);
}
#endif

#if defined(USE_SERIAL_4WAY_BLHELI_BOOTLOADER) && defined(USE_SERIAL_4WAY_SK_BOOTLOADER)
TEST_F(Serial4WayTest, UnknownBlSignatureIsClearedBeforeStkFallback)
{
    beginSession();
    stkProbeResults.push_back(dirtyFailure());
    blProbeResults.push_back(unknownSuccess());
    stkProbeResults.push_back(stkSuccess());

    appendRequest(CMD_INIT_FLASH, { 1 });
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(2U, responses.size());
    ASSERT_EQ(4U, responses[0].payload.size());
    EXPECT_EQ(ACK_OK, responses[0].ack);
    EXPECT_EQ(imSK, responses[0].payload[3]);
    EXPECT_EQ((std::vector<char>{ 'S', 'B', 'S' }), probeOrder);
    EXPECT_TRUE(allProbeEntriesWereClear);
}

TEST_F(Serial4WayTest, MixedBootloadersFallBackInBothDirections)
{
    beginSession();
    stkProbeResults.push_back(unknownSuccess());
    stkProbeResults.push_back(stkSuccess());
    stkProbeResults.push_back(dirtyFailure());
    blProbeResults.push_back(armSuccess());
    blProbeResults.push_back(dirtyFailure());
    blProbeResults.push_back(armSuccess());

    appendRequest(CMD_INIT_FLASH, { 0 });
    appendRequest(CMD_INIT_FLASH, { 1 });
    appendRequest(CMD_INIT_FLASH, { 2 });
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(4U, responses.size());
    ASSERT_EQ(4U, responses[0].payload.size());
    ASSERT_EQ(4U, responses[1].payload.size());
    ASSERT_EQ(4U, responses[2].payload.size());
    EXPECT_EQ(imARM_BLB, responses[0].payload[3]);
    EXPECT_EQ(imSK, responses[1].payload[3]);
    EXPECT_EQ(imARM_BLB, responses[2].payload[3]);
    EXPECT_EQ((std::vector<char>{ 'S', 'B', 'B', 'S', 'S', 'B' }), probeOrder);
    EXPECT_TRUE(allProbeEntriesWereClear);
}

TEST_F(Serial4WayTest, RepeatedSessionsResetModeSelectionAndDeviceInfo)
{
    beginSession();
    stkProbeResults.push_back(dirtyFailure());
    blProbeResults.push_back(armSuccess());
    appendRequest(CMD_INIT_FLASH, { 0 });
    appendRequest(CMD_EXIT);
    std::vector<Response> responses = runSession();
    ASSERT_EQ(2U, responses.size());
    EXPECT_EQ(imARM_BLB, responses[0].payload[3]);
    EXPECT_EQ(0U, DeviceInfo.dword);
    EXPECT_EQ(UINT8_MAX, selected_esc);

    clearSessionTransport();
    DeviceInfo.dword = UINT32_MAX;
    selected_esc = 2;
    stkProbeResults.push_back(stkSuccess());
    beginSession();
    appendRequest(CMD_INIT_FLASH, { 1 });
    appendRequest(CMD_EXIT);
    responses = runSession();

    ASSERT_EQ(2U, responses.size());
    EXPECT_EQ(imSK, responses[0].payload[3]);
    EXPECT_EQ((std::vector<char>{ 'S' }), probeOrder);
    EXPECT_EQ(0U, DeviceInfo.dword);
    EXPECT_EQ(UINT8_MAX, selected_esc);
}

TEST_F(Serial4WayTest, KeepAliveFailureGatesUntilReinitialization)
{
    beginSession();
    stkProbeResults.push_back(dirtyFailure());
    stkProbeResults.push_back(dirtyFailure());
    blProbeResults.push_back(silabsSuccess());
    blProbeResults.push_back(silabsSuccess());
    blKeepAliveResults.push_back(false);
    blReadResults.push_back(true);

    appendRequest(CMD_INIT_FLASH, { 0 });
    appendRequest(CMD_TEST_ALIVE);
    appendRequest(CMD_READ, { 1 }, 0x01, 0x20);
    appendRequest(CMD_INIT_FLASH, { 0 });
    appendRequest(CMD_READ, { 1 }, 0x01, 0x20);
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(6U, responses.size());
    EXPECT_EQ(ACK_OK, responses[0].ack);
    EXPECT_EQ(ACK_GENERAL_ERROR, responses[1].ack);
    EXPECT_EQ(ACK_GENERAL_ERROR, responses[2].ack);
    EXPECT_EQ(ACK_OK, responses[3].ack);
    EXPECT_EQ(ACK_OK, responses[4].ack);
    EXPECT_EQ((std::vector<uint8_t>{ 0xA0 }), responses[4].payload);
    EXPECT_EQ(1U, blReadCalls);
    EXPECT_EQ(0U, stkReadCalls);
}

TEST_F(Serial4WayTest, ResetRemainsStandaloneButDisconnectsMemoryCommands)
{
    beginSession();
    emptyPollsBeforeNextByte = 2;
    emptyPollsBetweenBytes = 2;
    appendRequest(CMD_RESET, { 0 });
    appendRequest(CMD_SET_MODE, { imSIL_BLB });
    appendRequest(CMD_RESET, { 2 });
    appendRequest(CMD_READ, { 1 }, 0x01, 0x20);
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(5U, responses.size());
    EXPECT_EQ(ACK_INVALID_CMD, responses[0].ack);
    EXPECT_EQ(ACK_OK, responses[1].ack);
    EXPECT_EQ(ACK_OK, responses[2].ack);
    EXPECT_EQ(ACK_GENERAL_ERROR, responses[3].ack);
    EXPECT_EQ(1U, blRestartCalls);
    EXPECT_EQ(0U, blReadCalls);
}

TEST_F(Serial4WayTest, DeviceReadFailureCanBeRetriedWithoutReconnect)
{
    beginSession();
    stkProbeResults.push_back(dirtyFailure());
    blProbeResults.push_back(silabsSuccess());
    blReadResults.push_back(false);
    blReadResults.push_back(true);

    appendRequest(CMD_INIT_FLASH, { 0 });
    appendRequest(CMD_READ, { 2 }, 0x12, 0x34);
    appendRequest(CMD_READ, { 2 }, 0x12, 0x34);
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(4U, responses.size());
    EXPECT_EQ(ACK_GENERAL_ERROR, responses[1].ack);
    EXPECT_EQ(ACK_OK, responses[2].ack);
    EXPECT_EQ((std::vector<uint8_t>{ 0xA0, 0xA1 }), responses[2].payload);
    EXPECT_EQ(2U, blReadCalls);
}
#endif

#if defined(USE_SERIAL_4WAY_BLHELI_BOOTLOADER) && !defined(USE_SERIAL_4WAY_SK_BOOTLOADER)
TEST_F(Serial4WayTest, BlOnlyBuildConnectsAndReads)
{
    beginSession();
    blProbeResults.push_back(silabsSuccess());
    blReadResults.push_back(true);

    appendRequest(CMD_INIT_FLASH, { 0 });
    appendRequest(CMD_READ, { 2 }, 0x12, 0x34);
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(3U, responses.size());
    EXPECT_EQ(ACK_OK, responses[0].ack);
    ASSERT_EQ(4U, responses[0].payload.size());
    EXPECT_EQ(imSIL_BLB, responses[0].payload[3]);
    EXPECT_EQ(ACK_OK, responses[1].ack);
    EXPECT_EQ((std::vector<uint8_t>{ 0xA0, 0xA1 }), responses[1].payload);
    EXPECT_EQ(1U, blReadCalls);
    EXPECT_EQ(0U, stkReadCalls);
}
#endif

#if defined(USE_SERIAL_4WAY_SK_BOOTLOADER) && !defined(USE_SERIAL_4WAY_BLHELI_BOOTLOADER)
TEST_F(Serial4WayTest, StkOnlyBuildConnectsAndReads)
{
    beginSession();
    stkProbeResults.push_back(stkSuccess());
    stkReadResults.push_back(true);

    appendRequest(CMD_INIT_FLASH, { 0 });
    appendRequest(CMD_READ, { 2 }, 0x12, 0x34);
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(3U, responses.size());
    EXPECT_EQ(ACK_OK, responses[0].ack);
    ASSERT_EQ(4U, responses[0].payload.size());
    EXPECT_EQ(imSK, responses[0].payload[3]);
    EXPECT_EQ(ACK_OK, responses[1].ack);
    EXPECT_EQ((std::vector<uint8_t>{ 0xB0, 0xB1 }), responses[1].payload);
    EXPECT_EQ(0U, blReadCalls);
    EXPECT_EQ(1U, stkReadCalls);
}
#endif

TEST_F(Serial4WayTest, CompleteFrameCrcMismatchFailsClosedAndRecovers)
{
    beginSession();
    appendRequest(CMD_INIT_FLASH, { 0 }, 0x12, 0x34);
    serialInput.back() ^= 0x01;
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(2U, responses.size());
    EXPECT_EQ(CMD_INIT_FLASH, responses[0].command);
    EXPECT_EQ(0x12, responses[0].addressHigh);
    EXPECT_EQ(0x34, responses[0].addressLow);
    EXPECT_EQ(ACK_INVALID_CRC, responses[0].ack);
    EXPECT_EQ(ACK_OK, responses[1].ack);
    EXPECT_FALSE(isMcuConnected());
}

TEST_F(Serial4WayTest, ConnectedMarkerWithDefaultModeRejectsWriteWithoutLoaderCall)
{
    beginSession();
    DeviceInfo.bytes[0] = 1;

    appendRequest(CMD_WRITE, { 0x5A }, 0x12, 0x34);
    appendRequest(CMD_EXIT);
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(2U, responses.size());
    EXPECT_EQ(ACK_INVALID_CMD, responses[0].ack);
    EXPECT_EQ(0U, blWriteCalls);
    EXPECT_EQ(0U, stkWriteCalls);
    EXPECT_EQ(ACK_OK, responses[1].ack);
}

TEST_F(Serial4WayTest, TimeoutResponseHasDeterministicCommandAndAddress)
{
    beginSession();
    serialInput.push_back(LOCAL_ESCAPE);
    injectExitAfterFirstResponse = true;
    const std::vector<Response> responses = runSession();

    ASSERT_EQ(2U, responses.size());
    EXPECT_EQ(0U, responses[0].command);
    EXPECT_EQ(0U, responses[0].addressHigh);
    EXPECT_EQ(0U, responses[0].addressLow);
    EXPECT_EQ(ACK_INVALID_CRC, responses[0].ack);
    EXPECT_EQ(CMD_EXIT, responses[1].command);
    EXPECT_EQ(ACK_OK, responses[1].ack);
}

} // namespace

extern "C" {

bool IORead(IO_t)
{
    return true;
}

void IOHi(IO_t)
{
}

void IOLo(IO_t)
{
}

void IOConfigGPIO(IO_t, ioConfig_t)
{
}

void motorDisable(void)
{
}

void motorEnable(void)
{
}

void beeperSilence(void)
{
}

bool motorIsMotorEnabled(unsigned index)
{
    return index < 4;
}

IO_t motorGetIo(unsigned index)
{
    return reinterpret_cast<IO_t>(static_cast<uintptr_t>(index + 1));
}

uint32_t serialRxBytesWaiting(const serialPort_t *)
{
    if (!serialInput.empty() && emptyPollsBeforeNextByte) {
        emptyPollsBeforeNextByte--;
        return 0;
    }
    return serialInput.size();
}

uint8_t serialRead(serialPort_t *)
{
    const uint8_t byte = serialInput.front();
    serialInput.pop_front();
    emptyPollsBeforeNextByte = emptyPollsBetweenBytes;
    return byte;
}

uint32_t serialTxBytesFree(const serialPort_t *)
{
    return 1;
}

void serialWrite(serialPort_t *, uint8_t byte)
{
    serialOutput.push_back(byte);
}

void serialBeginWrite(serialPort_t *)
{
}

void serialEndWrite(serialPort_t *)
{
    responseCount++;
    if (injectExitAfterFirstResponse && responseCount == 1) {
        appendRequest(CMD_EXIT);
    }
}

timeUs_t micros(void)
{
    testTimeUs += testTimeStepUs;
    return testTimeUs;
}

timeMs_t millis(void)
{
    return testTimeUs / 1000;
}

uint8_t BL_ConnectEx(uint8_32_u *deviceInfo)
{
    return applyProbe(blProbeResults, deviceInfo, 'B');
}

uint8_t BL_SendCMDKeepAlive(void)
{
    return popBool(blKeepAliveResults);
}

void BL_SendCMDRunRestartBootloader(uint8_32_u *deviceInfo)
{
    blRestartCalls++;
    deviceInfo->bytes[0] = 1;
}

uint8_t BL_PageErase(ioMem_t *)
{
    return false;
}

uint8_t BL_ReadEEprom(ioMem_t *)
{
    return false;
}

uint8_t BL_WriteEEprom(ioMem_t *)
{
    return false;
}

uint8_t BL_WriteFlash(ioMem_t *)
{
    blWriteCalls++;
    return popBool(blWriteResults);
}

uint8_t BL_ReadFlash(uint8_t, ioMem_t *memory)
{
    blReadCalls++;
    if (!popBool(blReadResults)) {
        return false;
    }
    const unsigned length = memory->D_NUM_BYTES ? memory->D_NUM_BYTES : 256;
    for (unsigned i = 0; i < length; i++) {
        memory->D_PTR_I[i] = 0xA0 + i;
    }
    return true;
}

uint8_t BL_VerifyFlash(ioMem_t *)
{
    return brERRORCOMMAND;
}

uint8_t Stk_SignOn(void)
{
    return popBool(stkKeepAliveResults);
}

uint8_t Stk_ConnectEx(uint8_32_u *deviceInfo)
{
    return applyProbe(stkProbeResults, deviceInfo, 'S');
}

uint8_t Stk_ReadEEprom(ioMem_t *)
{
    return false;
}

uint8_t Stk_WriteEEprom(ioMem_t *)
{
    return false;
}

uint8_t Stk_ReadFlash(ioMem_t *memory)
{
    stkReadCalls++;
    if (!popBool(stkReadResults)) {
        return false;
    }
    const unsigned length = memory->D_NUM_BYTES ? memory->D_NUM_BYTES : 256;
    for (unsigned i = 0; i < length; i++) {
        memory->D_PTR_I[i] = 0xB0 + i;
    }
    return true;
}

uint8_t Stk_WriteFlash(ioMem_t *)
{
    stkWriteCalls++;
    return popBool(stkWriteResults);
}

uint8_t Stk_Chip_Erase(void)
{
    return false;
}

} // extern "C"
