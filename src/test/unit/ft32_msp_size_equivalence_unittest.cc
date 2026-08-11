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

/*
 * Host semantic-equivalence gate for the FT32-only
 * msp/msp.c -O2 -> -Os build-policy change.
 *
 * The unit-test build compiles this source and src/main/msp/msp.c into two
 * expanded executables.  The final per-variant flag is -O2 for `o2` and -Os
 * for `os`; both link src/main/common/streambuf.c and use
 * -ffunction-sections -fdata-sections -Wl,--gc-sections -no-pie.
 *
 * This file deliberately does NOT define, wrap, macro-rename, weak-override,
 * or stub mspFcProcessCommand.  The feasibility gate must additionally prove
 * with each executable's link map and `nm` output that the one strong symbol
 * comes from that variant's compiled src/main/msp/msp.c object.
 */

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
#include "platform.h"
#include "common/streambuf.h"
#include "msp/msp.h"
#include "msp/msp_protocol.h"
#include "msp/msp_serial.h"
}

#include "gtest/gtest.h"

namespace {

constexpr size_t kMaxInput = MSP_PORT_INBUF_SIZE;
constexpr size_t kMaxOutput = MSP_PORT_OUTBUF_SIZE_MIN;
constexpr uint8_t kArmedFlag = 1U;

uint32_t schedulerIgnoreCount;
uint32_t writeEepromCount;
uint32_t readEepromCount;
uint32_t setArmingDisabledCount;
uint32_t unsetArmingDisabledCount;
uint32_t disarmCount;

struct Case {
    int16_t command = 0;
    std::array<uint8_t, kMaxInput> input{};
    size_t inputSize = 0;
    size_t outputCapacity = kMaxOutput;
    bool armed = false;
    bool capturePostProcess = true;
};

struct Trace {
    int16_t command;
    int16_t result;
    int16_t replyCommand;
    uint16_t inputConsumed;
    uint16_t outputSize;
    bool postProcessSet;
    uint8_t finalArmingFlags;
    uint16_t schedulerIgnores;
    uint16_t writes;
    uint16_t reads;
    uint16_t setArmingCalls;
    uint16_t unsetArmingCalls;
    uint16_t disarms;
    std::vector<uint8_t> response;
};

uint64_t fnv1aByte(uint64_t digest, uint8_t value)
{
    return (digest ^ value) * UINT64_C(1099511628211);
}

uint64_t fnv1aU16(uint64_t digest, uint16_t value)
{
    digest = fnv1aByte(digest, value & 0xffU);
    return fnv1aByte(digest, value >> 8);
}

uint64_t appendTrace(uint64_t digest, const Trace &trace)
{
    digest = fnv1aU16(digest, static_cast<uint16_t>(trace.command));
    digest = fnv1aU16(digest, static_cast<uint16_t>(trace.result));
    digest = fnv1aU16(digest, static_cast<uint16_t>(trace.replyCommand));
    digest = fnv1aU16(digest, trace.inputConsumed);
    digest = fnv1aU16(digest, trace.outputSize);
    digest = fnv1aByte(digest, trace.postProcessSet);
    digest = fnv1aByte(digest, trace.finalArmingFlags);
    digest = fnv1aU16(digest, trace.schedulerIgnores);
    digest = fnv1aU16(digest, trace.writes);
    digest = fnv1aU16(digest, trace.reads);
    digest = fnv1aU16(digest, trace.setArmingCalls);
    digest = fnv1aU16(digest, trace.unsetArmingCalls);
    digest = fnv1aU16(digest, trace.disarms);
    for (const uint8_t byte : trace.response) {
        digest = fnv1aByte(digest, byte);
    }
    return digest;
}

Case oneByteCase(int16_t command, uint8_t value)
{
    Case testCase;
    testCase.command = command;
    testCase.input[0] = value;
    testCase.inputSize = 1;
    return testCase;
}

Trace runCase(const Case &testCase);

} // namespace

/*
 * Stateful link seams used by exercised corpus paths.  Their counters are
 * part of the oracle, so an optimization-induced change in state behavior is
 * visible even when reply bytes happen to match.
 */
extern "C" {

uint8_t armingFlags = 0;

void schedulerIgnoreTaskStateTime(void)
{
    schedulerIgnoreCount++;
}

void writeEEPROM(void)
{
    writeEepromCount++;
}

bool readEEPROM(void)
{
    readEepromCount++;
    return true;
}

void ft32MspSetArmingDisabled(uint32_t) asm("setArmingDisabled");
void ft32MspSetArmingDisabled(uint32_t)
{
    setArmingDisabledCount++;
}

void ft32MspUnsetArmingDisabled(uint32_t) asm("unsetArmingDisabled");
void ft32MspUnsetArmingDisabled(uint32_t)
{
    unsetArmingDisabledCount++;
}

void ft32MspDisarm(uint32_t) asm("disarm");
void ft32MspDisarm(uint32_t)
{
    disarmCount++;
    armingFlags &= ~kArmedFlag;
}

/*
 * msp.c's full dispatcher references many flight-controller services even
 * though this bounded host corpus does not execute those cases.  One inert
 * link seam satisfies each such relocation.  Executed dependencies are
 * excluded from this list and implemented above (or by real streambuf.c).
 * This keeps the harness honest about the dispatcher TU while preventing any
 * accidental host hardware access.
 */
uintptr_t ft32MspUnusedDependency(...)
{
    return 0;
}

} // extern "C"

#define FT32_MSP_UNUSED_LINK_SEAM(symbol) \
    asm(".globl " #symbol "\n.set " #symbol ", ft32MspUnusedDependency")

FT32_MSP_UNUSED_LINK_SEAM(GPS_directionToHome);
FT32_MSP_UNUSED_LINK_SEAM(GPS_distanceToHome);
FT32_MSP_UNUSED_LINK_SEAM(GPS_numCh);
FT32_MSP_UNUSED_LINK_SEAM(GPS_svinfo);
FT32_MSP_UNUSED_LINK_SEAM(GPS_update);
FT32_MSP_UNUSED_LINK_SEAM(acc);
FT32_MSP_UNUSED_LINK_SEAM(accHasBeenCalibrated);
FT32_MSP_UNUSED_LINK_SEAM(accStartCalibration);
FT32_MSP_UNUSED_LINK_SEAM(accelerometerConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(activeAdjustmentRangeReset);
FT32_MSP_UNUSED_LINK_SEAM(adjustmentRanges_SystemArray);
FT32_MSP_UNUSED_LINK_SEAM(armingConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(attitude);
FT32_MSP_UNUSED_LINK_SEAM(barometerConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(batteryConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(batteryProfiles_SystemArray);
FT32_MSP_UNUSED_LINK_SEAM(beeperConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(blackboxCalculatePDenom);
FT32_MSP_UNUSED_LINK_SEAM(blackboxCalculateSampleRate);
FT32_MSP_UNUSED_LINK_SEAM(blackboxConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(blackboxGetPRatio);
FT32_MSP_UNUSED_LINK_SEAM(blackboxGetRateDenom);
FT32_MSP_UNUSED_LINK_SEAM(blackboxMayEditConfig);
FT32_MSP_UNUSED_LINK_SEAM(boardAlignment_System);
FT32_MSP_UNUSED_LINK_SEAM(buildDate);
FT32_MSP_UNUSED_LINK_SEAM(buildKey);
FT32_MSP_UNUSED_LINK_SEAM(buildTime);
FT32_MSP_UNUSED_LINK_SEAM(changeBatteryProfile);
FT32_MSP_UNUSED_LINK_SEAM(changeControlRateProfile);
FT32_MSP_UNUSED_LINK_SEAM(changePidProfile);
FT32_MSP_UNUSED_LINK_SEAM(checkMotorProtocolEnabled);
FT32_MSP_UNUSED_LINK_SEAM(compassConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(compassStartCalibration);
FT32_MSP_UNUSED_LINK_SEAM(copyControlRateProfile);
FT32_MSP_UNUSED_LINK_SEAM(currentBatteryProfile);
FT32_MSP_UNUSED_LINK_SEAM(currentControlRateProfile);
FT32_MSP_UNUSED_LINK_SEAM(currentMeterIds);
FT32_MSP_UNUSED_LINK_SEAM(currentMeterRead);
FT32_MSP_UNUSED_LINK_SEAM(currentPidProfile);
FT32_MSP_UNUSED_LINK_SEAM(currentSensorADCConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(customServoMixers_SystemArray);
FT32_MSP_UNUSED_LINK_SEAM(debug);
FT32_MSP_UNUSED_LINK_SEAM(detectedSensors);
FT32_MSP_UNUSED_LINK_SEAM(failsafeConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(featureConfigReplace);
FT32_MSP_UNUSED_LINK_SEAM(featureConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(findBoxByBoxId);
FT32_MSP_UNUSED_LINK_SEAM(findBoxByPermanentId);
FT32_MSP_UNUSED_LINK_SEAM(firstEnabledGyro);
FT32_MSP_UNUSED_LINK_SEAM(flight3DConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(getAmperage);
FT32_MSP_UNUSED_LINK_SEAM(getArmingDisableFlags);
FT32_MSP_UNUSED_LINK_SEAM(getAverageSystemLoadPercent);
FT32_MSP_UNUSED_LINK_SEAM(getBatteryCellCount);
FT32_MSP_UNUSED_LINK_SEAM(getBatteryState);
FT32_MSP_UNUSED_LINK_SEAM(getBatteryVoltage);
FT32_MSP_UNUSED_LINK_SEAM(getCurrentBatteryProfileIndex);
FT32_MSP_UNUSED_LINK_SEAM(getCurrentControlRateProfileIndex);
FT32_MSP_UNUSED_LINK_SEAM(getCurrentPidProfileIndex);
FT32_MSP_UNUSED_LINK_SEAM(getEstimatedAltitudeCm);
FT32_MSP_UNUSED_LINK_SEAM(getGyroDetectedFlags);
FT32_MSP_UNUSED_LINK_SEAM(getLegacyBatteryVoltage);
FT32_MSP_UNUSED_LINK_SEAM(getMAhDrawn);
FT32_MSP_UNUSED_LINK_SEAM(getMcuTypeId);
FT32_MSP_UNUSED_LINK_SEAM(getMcuTypeName);
FT32_MSP_UNUSED_LINK_SEAM(getMotorCount);
FT32_MSP_UNUSED_LINK_SEAM(getRebootRequired);
FT32_MSP_UNUSED_LINK_SEAM(getRssi);
FT32_MSP_UNUSED_LINK_SEAM(getTaskDeltaTimeUs);
FT32_MSP_UNUSED_LINK_SEAM(gpsConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(gpsSetFixState);
FT32_MSP_UNUSED_LINK_SEAM(gpsSol);
FT32_MSP_UNUSED_LINK_SEAM(gyro);
FT32_MSP_UNUSED_LINK_SEAM(gyroConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(gyroDeviceConfig_SystemArray);
FT32_MSP_UNUSED_LINK_SEAM(gyroInitFilters);
FT32_MSP_UNUSED_LINK_SEAM(gyroRateDps);
FT32_MSP_UNUSED_LINK_SEAM(imuAttitudeQuaternion);
FT32_MSP_UNUSED_LINK_SEAM(imuConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(initActiveBoxIds);
FT32_MSP_UNUSED_LINK_SEAM(initEscEndpoints);
FT32_MSP_UNUSED_LINK_SEAM(initRcProcessing);
FT32_MSP_UNUSED_LINK_SEAM(ledStripConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(ledStripStatusModeConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(loadCustomServoMixer);
FT32_MSP_UNUSED_LINK_SEAM(mag);
FT32_MSP_UNUSED_LINK_SEAM(magHold);
FT32_MSP_UNUSED_LINK_SEAM(mixerConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(mixerInitProfile);
FT32_MSP_UNUSED_LINK_SEAM(modeActivationConditions_SystemArray);
FT32_MSP_UNUSED_LINK_SEAM(motorConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(motorConvertFromExternal);
FT32_MSP_UNUSED_LINK_SEAM(motorShutdown);
FT32_MSP_UNUSED_LINK_SEAM(motor_disarmed);
FT32_MSP_UNUSED_LINK_SEAM(packFlightModeFlags);
FT32_MSP_UNUSED_LINK_SEAM(pidConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(pidCopyProfile);
FT32_MSP_UNUSED_LINK_SEAM(pidInitConfig);
FT32_MSP_UNUSED_LINK_SEAM(pidInitFilters);
FT32_MSP_UNUSED_LINK_SEAM(pidNames);
FT32_MSP_UNUSED_LINK_SEAM(pilotConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(rcControlsConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(rcControlsInit);
FT32_MSP_UNUSED_LINK_SEAM(rcData);
FT32_MSP_UNUSED_LINK_SEAM(reevaluateLedConfig);
FT32_MSP_UNUSED_LINK_SEAM(releaseName);
FT32_MSP_UNUSED_LINK_SEAM(resetEEPROM);
FT32_MSP_UNUSED_LINK_SEAM(resetPidProfile);
FT32_MSP_UNUSED_LINK_SEAM(rssiSource);
FT32_MSP_UNUSED_LINK_SEAM(rxConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(rxFailsafeChannelConfigs_SystemArray);
FT32_MSP_UNUSED_LINK_SEAM(rxMspFrameReceive);
FT32_MSP_UNUSED_LINK_SEAM(rxRuntimeState);
FT32_MSP_UNUSED_LINK_SEAM(sbufWriteBuildInfoFlags);
FT32_MSP_UNUSED_LINK_SEAM(sensors);
FT32_MSP_UNUSED_LINK_SEAM(serialConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(serialFindPortConfigurationMutable);
FT32_MSP_UNUSED_LINK_SEAM(serialIsPortAvailable);
FT32_MSP_UNUSED_LINK_SEAM(serializeBoxNameFn);
FT32_MSP_UNUSED_LINK_SEAM(serializeBoxPermanentIdFn);
FT32_MSP_UNUSED_LINK_SEAM(serializeBoxReply);
FT32_MSP_UNUSED_LINK_SEAM(servo);
FT32_MSP_UNUSED_LINK_SEAM(servoParams_SystemArray);
FT32_MSP_UNUSED_LINK_SEAM(setModeColor);
FT32_MSP_UNUSED_LINK_SEAM(setRssiMsp);
FT32_MSP_UNUSED_LINK_SEAM(shortGitRevision);
FT32_MSP_UNUSED_LINK_SEAM(stateFlags);
FT32_MSP_UNUSED_LINK_SEAM(supportedCurrentMeterCount);
FT32_MSP_UNUSED_LINK_SEAM(supportedVoltageMeterCount);
FT32_MSP_UNUSED_LINK_SEAM(systemConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(systemReset);
FT32_MSP_UNUSED_LINK_SEAM(systemResetToBootloader);
FT32_MSP_UNUSED_LINK_SEAM(targetName);
FT32_MSP_UNUSED_LINK_SEAM(transponderConfig_System);
FT32_MSP_UNUSED_LINK_SEAM(transponderRequirements);
FT32_MSP_UNUSED_LINK_SEAM(transponderStopRepeating);
FT32_MSP_UNUSED_LINK_SEAM(transponderUpdateData);
FT32_MSP_UNUSED_LINK_SEAM(validateAndFixGyroConfig);
FT32_MSP_UNUSED_LINK_SEAM(voltageMeterADCtoIDMap);
FT32_MSP_UNUSED_LINK_SEAM(voltageMeterIds);
FT32_MSP_UNUSED_LINK_SEAM(voltageMeterRead);
FT32_MSP_UNUSED_LINK_SEAM(voltageSensorADCConfig_SystemArray);

#undef FT32_MSP_UNUSED_LINK_SEAM

namespace {

Trace runCase(const Case &testCase)
{
    std::array<uint8_t, kMaxInput> input = testCase.input;
    std::array<uint8_t, kMaxOutput + 16> output;
    output.fill(0xa5);

    mspPacket_t command{};
    mspPacket_t reply{};
    sbufInit(&command.buf, input.data(), input.data() + testCase.inputSize);
    sbufInit(&reply.buf, output.data(), output.data() + testCase.outputCapacity);
    command.cmd = testCase.command;
    reply.cmd = static_cast<int16_t>(0x5a5a);
    reply.result = static_cast<int16_t>(0x5a5a);

    schedulerIgnoreCount = 0;
    writeEepromCount = 0;
    readEepromCount = 0;
    setArmingDisabledCount = 0;
    unsetArmingDisabledCount = 0;
    disarmCount = 0;
    armingFlags = testCase.armed ? kArmedFlag : 0;

    mspPostProcessFnPtr postProcess = nullptr;
    mspPostProcessFnPtr *postProcessOut = testCase.capturePostProcess ? &postProcess : nullptr;
    const mspResult_e result = mspFcProcessCommand(0, &command, &reply, postProcessOut);

    const size_t outputSize = static_cast<size_t>(reply.buf.ptr - output.data());
    const size_t inputConsumed = static_cast<size_t>(command.buf.ptr - input.data());
    EXPECT_LE(outputSize, testCase.outputCapacity);
    EXPECT_LE(inputConsumed, testCase.inputSize);
    EXPECT_EQ(result, reply.result);
    for (size_t i = testCase.outputCapacity; i < output.size(); i++) {
        EXPECT_EQ(0xa5, output[i]) << "output canary offset " << i;
    }

    const size_t safeOutputSize = std::min(outputSize, output.size());
    return {
        testCase.command,
        static_cast<int16_t>(result),
        reply.cmd,
        static_cast<uint16_t>(inputConsumed),
        static_cast<uint16_t>(outputSize),
        postProcess != nullptr,
        armingFlags,
        static_cast<uint16_t>(schedulerIgnoreCount),
        static_cast<uint16_t>(writeEepromCount),
        static_cast<uint16_t>(readEepromCount),
        static_cast<uint16_t>(setArmingDisabledCount),
        static_cast<uint16_t>(unsetArmingDisabledCount),
        static_cast<uint16_t>(disarmCount),
        std::vector<uint8_t>(output.begin(), output.begin() + safeOutputSize),
    };
}

TEST(FT32MspSizeEquivalence, RealDispatcherResponseAndStateTrace)
{
    uint64_t digest = UINT64_C(14695981039346656037);
    unsigned caseCount = 0;

    // Exhaust the one-byte reboot mode and alternate post-process capture.
    for (unsigned mode = 0; mode <= UINT8_MAX; mode++) {
        Case testCase = oneByteCase(MSP_REBOOT, static_cast<uint8_t>(mode));
        testCase.capturePostProcess = (mode & 1U) == 0;
        const Trace trace = runCase(testCase);
        const bool validMode = mode == 0 || mode == 1 || mode == 4;
        EXPECT_EQ(validMode ? MSP_RESULT_ACK : MSP_RESULT_ERROR, trace.result);
        EXPECT_EQ(validMode ? 1U : 0U, trace.outputSize);
        EXPECT_EQ(validMode && testCase.capturePostProcess, trace.postProcessSet);
        if (validMode) {
            ASSERT_EQ(1U, trace.response.size());
            EXPECT_EQ(mode, trace.response[0]);
        }
        digest = appendTrace(digest, trace);
        caseCount++;
    }

    // Cover every request length from empty through the real 192-byte maximum.
    for (size_t inputSize = 0; inputSize <= kMaxInput; inputSize++) {
        Case testCase;
        testCase.command = static_cast<int16_t>(0x6000 + inputSize);
        testCase.inputSize = inputSize;
        testCase.capturePostProcess = (inputSize & 1U) != 0;
        for (size_t i = 0; i < inputSize; i++) {
            testCase.input[i] = static_cast<uint8_t>((inputSize * 17U + i * 29U) & 0xffU);
        }
        const Trace trace = runCase(testCase);
        EXPECT_EQ(MSP_RESULT_ERROR, trace.result);
        EXPECT_EQ(0U, trace.inputConsumed);
        EXPECT_EQ(0U, trace.outputSize);
        EXPECT_FALSE(trace.postProcessSet);
        digest = appendTrace(digest, trace);
        caseCount++;
    }

    {
        Case testCase;
        testCase.command = MSP_API_VERSION;
        const Trace trace = runCase(testCase);
        ASSERT_EQ(3U, trace.response.size());
        EXPECT_EQ(MSP_PROTOCOL_VERSION, trace.response[0]);
        EXPECT_EQ(API_VERSION_MAJOR, trace.response[1]);
        EXPECT_EQ(API_VERSION_MINOR, trace.response[2]);
        EXPECT_EQ(MSP_RESULT_ACK, trace.result);
        digest = appendTrace(digest, trace);
        caseCount++;
    }

    {
        Case testCase;
        testCase.command = MSP_UID;
        const Trace trace = runCase(testCase);
        const std::array<uint8_t, 12> expected = {
            0, 0, 0, 0,
            1, 0, 0, 0,
            2, 0, 0, 0,
        };
        EXPECT_EQ(std::vector<uint8_t>(expected.begin(), expected.end()), trace.response);
        EXPECT_EQ(MSP_RESULT_ACK, trace.result);
        digest = appendTrace(digest, trace);
        caseCount++;
    }

    // The legacy battery setter rejects all 0..6-byte short inputs before read.
    for (size_t inputSize = 0; inputSize < 7; inputSize++) {
        Case testCase;
        testCase.command = MSP_SET_BATTERY_CONFIG;
        testCase.inputSize = inputSize;
        const Trace trace = runCase(testCase);
        EXPECT_EQ(MSP_RESULT_ERROR, trace.result);
        EXPECT_EQ(0U, trace.inputConsumed);
        EXPECT_EQ(0U, trace.outputSize);
        digest = appendTrace(digest, trace);
        caseCount++;
    }

    // 128 nested API requests produce the exact 512-byte MSP output maximum.
    {
        Case testCase;
        testCase.command = MSP_MULTIPLE_MSP;
        testCase.inputSize = 128;
        testCase.input.fill(MSP_API_VERSION);
        const Trace trace = runCase(testCase);
        ASSERT_EQ(kMaxOutput, trace.response.size());
        for (size_t i = 0; i < trace.response.size(); i += 4) {
            EXPECT_EQ(3, trace.response[i + 0]);
            EXPECT_EQ(MSP_PROTOCOL_VERSION, trace.response[i + 1]);
            EXPECT_EQ(API_VERSION_MAJOR, trace.response[i + 2]);
            EXPECT_EQ(API_VERSION_MINOR, trace.response[i + 3]);
        }
        EXPECT_EQ(MSP_RESULT_ACK, trace.result);
        EXPECT_EQ(128U, trace.inputConsumed);
        digest = appendTrace(digest, trace);
        caseCount++;
    }

    // A zero-capacity reply is a safe short-output boundary for MULTIPLE_MSP.
    {
        Case testCase = oneByteCase(MSP_MULTIPLE_MSP, MSP_API_VERSION);
        testCase.outputCapacity = 0;
        const Trace trace = runCase(testCase);
        EXPECT_EQ(MSP_RESULT_ACK, trace.result);
        EXPECT_EQ(0U, trace.inputConsumed);
        EXPECT_EQ(0U, trace.outputSize);
        digest = appendTrace(digest, trace);
        caseCount++;
    }

    // EEPROM write is forbidden while armed and fully traced while unarmed.
    for (const bool armed : {true, false}) {
        Case testCase;
        testCase.command = MSP_EEPROM_WRITE;
        testCase.armed = armed;
        const Trace trace = runCase(testCase);
        EXPECT_EQ(armed ? MSP_RESULT_ERROR : MSP_RESULT_ACK, trace.result);
        EXPECT_EQ(armed ? 0 : 1, trace.schedulerIgnores);
        EXPECT_EQ(armed ? 0 : 1, trace.writes);
        EXPECT_EQ(armed ? 0 : 1, trace.reads);
        digest = appendTrace(digest, trace);
        caseCount++;
    }

    // Exercise descriptor-local disable, armed disarm, and matching re-enable.
    {
        Case disable = oneByteCase(MSP_SET_ARMING_DISABLED, 1);
        disable.armed = true;
        const Trace trace = runCase(disable);
        EXPECT_EQ(MSP_RESULT_ACK, trace.result);
        EXPECT_EQ(1, trace.setArmingCalls);
        EXPECT_EQ(1, trace.disarms);
        EXPECT_EQ(0, trace.finalArmingFlags);
        digest = appendTrace(digest, trace);
        caseCount++;
    }
    {
        const Case enable = oneByteCase(MSP_SET_ARMING_DISABLED, 0);
        const Trace trace = runCase(enable);
        EXPECT_EQ(MSP_RESULT_ACK, trace.result);
        EXPECT_EQ(1, trace.unsetArmingCalls);
        digest = appendTrace(digest, trace);
        caseCount++;
    }

    ASSERT_EQ(464U, caseCount);

    std::printf("FT32_MSP_TRACE cases=%u digest=%" PRIu64 "\n", caseCount, digest);

    // Frozen from the first reviewed -O2 run.  The identical constant must
    // pass unchanged in both the -O2 and -Os executables.
    EXPECT_EQ(UINT64_C(13245422755985777579), digest)
        << "candidate trace digest=" << digest;
}

} // namespace
