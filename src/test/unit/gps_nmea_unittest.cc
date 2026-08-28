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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdint.h>

#include <string>

extern "C" {
    #include "platform.h"

    #include "build/debug.h"

    #include "common/maths.h"

    #include "config/feature.h"

    #include "drivers/serial.h"
    #include "drivers/time.h"

    #include "fc/runtime_config.h"

    #include "flight/gps_rescue.h"

    #include "io/beeper.h"
    #include "io/dashboard.h"
    #include "io/gps.h"
    #include "io/serial.h"

    #include "pg/gps.h"
    #include "pg/pg.h"
    #include "pg/pg_ids.h"

    #include "scheduler/scheduler.h"

    #include "sensors/sensors.h"
}

#include "gtest/gtest.h"

extern "C" {
PG_REGISTER(gpsRescueConfig_t, gpsRescueConfig, PG_GPS_RESCUE, 0);

extern const gpsConfig_t pgResetTemplate_gpsConfig;

uint8_t stateFlags;
uint8_t armingFlags;
int16_t debug[DEBUG16_VALUE_COUNT];
uint8_t debugMode;

const uint32_t baudRates[BAUD_COUNT] = {
    0, 9600, 19200, 38400, 57600, 115200, 230400, 250000,
    400000, 460800, 500000, 921600, 1000000, 1500000, 2000000, 2470000
};
}

namespace {

struct FakeSerialState {
    std::string rx;
    size_t rxOffset;
    std::string tx;
    unsigned openCount;
    unsigned baudChangeCount;
    serialPortIdentifier_e openedIdentifier;
    serialPortFunction_e openedFunction;
    uint32_t openedBaudRate;
    portMode_e openedMode;
    portOptions_e openedOptions;
};

FakeSerialState fakeSerial;
serialPort_t fakeGpsPort;
timeMs_t fakeNowMs;
uint32_t fakeSensorMask;
unsigned fakeBeeperCount;

void fakeSerialWrite(serialPort_t *, uint8_t value)
{
    fakeSerial.tx += static_cast<char>(value);
}

uint32_t fakeSerialRxWaiting(const serialPort_t *)
{
    return static_cast<uint32_t>(fakeSerial.rx.size() - fakeSerial.rxOffset);
}

uint32_t fakeSerialTxFree(const serialPort_t *)
{
    return 1024;
}

uint8_t fakeSerialRead(serialPort_t *)
{
    if (fakeSerial.rxOffset >= fakeSerial.rx.size()) {
        return 0;
    }
    return static_cast<uint8_t>(fakeSerial.rx[fakeSerial.rxOffset++]);
}

void fakeSerialSetBaudRate(serialPort_t *instance, uint32_t baudRate)
{
    instance->baudRate = baudRate;
    fakeSerial.baudChangeCount++;
}

bool fakeSerialTxEmpty(const serialPort_t *)
{
    return true;
}

void fakeSerialSetMode(serialPort_t *instance, portMode_e mode)
{
    instance->mode = mode;
}

void fakeSerialWriteBuf(serialPort_t *, const void *data, int count)
{
    fakeSerial.tx.append(static_cast<const char *>(data), count);
}

const serialPortVTable fakeSerialVTable = {
    fakeSerialWrite,
    fakeSerialRxWaiting,
    fakeSerialTxFree,
    fakeSerialRead,
    fakeSerialSetBaudRate,
    fakeSerialTxEmpty,
    fakeSerialSetMode,
    nullptr,
    nullptr,
    fakeSerialWriteBuf,
    nullptr,
    nullptr,
};

void resetFakeSerial()
{
    fakeSerial = {};
    fakeSerial.openedIdentifier = SERIAL_PORT_NONE;
    fakeSerial.openedFunction = FUNCTION_NONE;
    fakeGpsPort = {};
    fakeGpsPort.vTable = &fakeSerialVTable;
    fakeGpsPort.identifier = SERIAL_PORT_UART5;
}

void queueSerialRx(const std::string &data)
{
    if (fakeSerial.rxOffset == fakeSerial.rx.size()) {
        fakeSerial.rx.clear();
        fakeSerial.rxOffset = 0;
    }
    fakeSerial.rx += data;
}

std::string makeNmeaSentence(const std::string &payload, uint8_t checksumAdjustment = 0)
{
    uint8_t checksum = 0;
    for (const char c : payload) {
        checksum ^= static_cast<uint8_t>(c);
    }
    checksum ^= checksumAdjustment;

    static const char hex[] = "0123456789ABCDEF";
    std::string sentence = "$" + payload + "*";
    sentence += hex[checksum >> 4];
    sentence += hex[checksum & 0x0f];
    sentence += "\r\n";
    return sentence;
}

} // namespace

extern "C" {

timeUs_t micros(void)
{
    return fakeNowMs * 1000U;
}

timeMs_t millis(void)
{
    return fakeNowMs;
}

uint32_t getCycleCounter(void)
{
    return 0;
}

uint32_t clockMicrosToCycles(uint32_t micros)
{
    return micros;
}

serialType_e serialType(serialPortIdentifier_e identifier)
{
    return identifier == SERIAL_PORT_UART5 ? SERIALTYPE_UART : SERIALTYPE_INVALID;
}

serialPort_t *openSerialPort(
    serialPortIdentifier_e identifier,
    serialPortFunction_e function,
    serialReceiveCallbackPtr,
    void *,
    uint32_t baudRate,
    portMode_e mode,
    portOptions_e options)
{
    fakeSerial.openCount++;
    fakeSerial.openedIdentifier = identifier;
    fakeSerial.openedFunction = function;
    fakeSerial.openedBaudRate = baudRate;
    fakeSerial.openedMode = mode;
    fakeSerial.openedOptions = options;
    fakeGpsPort.baudRate = baudRate;
    fakeGpsPort.mode = mode;
    fakeGpsPort.options = options;
    return &fakeGpsPort;
}

bool sensors(uint32_t mask)
{
    return (fakeSensorMask & mask) != 0;
}

void sensorsSet(uint32_t mask)
{
    fakeSensorMask |= mask;
}

void sensorsClear(uint32_t mask)
{
    fakeSensorMask &= ~mask;
}

uint32_t sensorsMask(void)
{
    return fakeSensorMask;
}

void rescheduleTask(taskId_e, timeDelta_t) {}
void schedulerSetNextStateTime(timeDelta_t) {}

void beeper(beeperMode_e)
{
    fakeBeeperCount++;
}

float atan2_approx(float, float)
{
    return 0.0f;
}

float cos_approx(float)
{
    return 1.0f;
}

bool featureIsEnabled(uint32_t)
{
    return false;
}

void dashboardUpdate(timeUs_t) {}
void dashboardShowFixedPage(pageId_e) {}
void waitForSerialPortToFinishTransmitting(serialPort_t *) {}
void serialPassthrough(serialPort_t *, serialPort_t *, serialConsumer *, serialConsumer *) {}

}

namespace {

bool feedNmeaPayload(const std::string &payload, uint8_t checksumAdjustment = 0)
{
    const std::string sentence = makeNmeaSentence(payload, checksumAdjustment);

    bool completed = false;
    for (const char c : sentence) {
        completed = gpsNewFrame(static_cast<uint8_t>(c)) || completed;
    }
    return completed;
}

void updateGps()
{
    gpsUpdate(fakeNowMs * 1000U);
}

void advanceGpsBy(timeMs_t deltaMs)
{
    fakeNowMs += deltaMs;
    updateGps();
}

class GpsNmeaTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        stateFlags = 0;
        armingFlags = 0;
        debugMode = DEBUG_NONE;
        fakeNowMs = 0;
        fakeSensorMask = 0;
        fakeBeeperCount = 0;
        resetFakeSerial();

        *gpsConfigMutable() = pgResetTemplate_gpsConfig;
        gpsConfigMutable()->provider = GPS_NMEA;
        gpsConfigMutable()->autoConfig = GPS_AUTOCONFIG_OFF;
        gpsConfigMutable()->autoBaud = GPS_AUTOBAUD_OFF;
        gpsConfigMutable()->gps_uart = SERIAL_PORT_UART5;
        gpsConfigMutable()->gps_baud = BAUD_38400;

        *gpsRescueConfigMutable() = {};
        gpsRescueConfigMutable()->minSats = UINT8_MAX;

        gpsSol = {};
        gpsData = {};
        GPS_update = 0;
    }
};

} // namespace

TEST_F(GpsNmeaTest, AcceptsValidGgaFix)
{
    const bool completed = feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");

    EXPECT_TRUE(completed);
    EXPECT_NE(0, STATE(GPS_FIX));
    EXPECT_NE(0, STATE(GPS_FIX_EVER));
    EXPECT_EQ(481173000, gpsSol.llh.lat);
    EXPECT_EQ(115166666, gpsSol.llh.lon);
    EXPECT_EQ(8, gpsSol.numSat);
    EXPECT_EQ(54540, gpsSol.llh.altCm);
}

TEST_F(GpsNmeaTest, RejectsBadChecksumWithoutPublishingSolution)
{
    gpsSol.llh.lat = 123456789;
    gpsSol.llh.lon = -234567890;
    gpsSol.numSat = 4;

    const bool completed = feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", 1);

    EXPECT_FALSE(completed);
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);
    EXPECT_EQ(4, gpsSol.numSat);
    // Fix-state isolation is tracked separately in nmea-checksum-fix-state-residual.json.
}

TEST_F(GpsNmeaTest, ClearsFixWithoutOverwritingLastPositionOnValidNoFixGga)
{
    ENABLE_STATE(GPS_FIX | GPS_FIX_EVER);
    gpsSol.llh.lat = 123456789;
    gpsSol.llh.lon = -234567890;
    gpsSol.numSat = 7;

    const bool completed = feedNmeaPayload("GPGGA,123520,,,,,0,00,99.9,,,,,,");

    EXPECT_TRUE(completed);
    EXPECT_EQ(0, STATE(GPS_FIX));
    EXPECT_NE(0, STATE(GPS_FIX_EVER));
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);
    EXPECT_EQ(7, gpsSol.numSat);
}

TEST_F(GpsNmeaTest, ParsesValidRmcMotionWithoutPublishingNavigationTick)
{
    const uint8_t updateBefore = GPS_update;

    const bool completed = feedNmeaPayload("GNRMC,123519.00,A,4807.038,N,01131.000,E,12.3,84.4,230394,,,A");

    EXPECT_FALSE(completed);
    EXPECT_EQ(632, gpsSol.groundSpeed);
    EXPECT_EQ(844, gpsSol.groundCourse);
    EXPECT_EQ(updateBefore, GPS_update);
}

TEST_F(GpsNmeaTest, ParsesValidGsaDilutionWithoutPublishingNavigationTick)
{
    const uint8_t updateBefore = GPS_update;

    const bool completed = feedNmeaPayload("GNGSA,A,3,04,05,09,12,24,25,29,31,32,21,22,18,1.23,0.90,0.80");

    EXPECT_FALSE(completed);
    EXPECT_EQ(123, gpsSol.dop.pdop);
    EXPECT_EQ(90, gpsSol.dop.hdop);
    EXPECT_EQ(80, gpsSol.dop.vdop);
    EXPECT_EQ(updateBefore, GPS_update);
}

TEST_F(GpsNmeaTest, ParsesValidGsvSatelliteInfoWithinLegacyBounds)
{
    GPS_numCh = 0;
    memset(GPS_svinfo, 0, sizeof(GPS_svinfo));
    const uint8_t updateBefore = GPS_update;

    const bool completed = feedNmeaPayload("GNGSV,1,1,04,01,40,083,41,02,17,308,42,03,13,172,43,04,09,301,44");

    EXPECT_FALSE(completed);
    EXPECT_EQ(4, GPS_numCh);
    for (unsigned i = 0; i < 4; i++) {
        EXPECT_EQ(i + 1, GPS_svinfo[i].chn);
        EXPECT_EQ(i + 1, GPS_svinfo[i].svid);
        EXPECT_EQ(41 + i, GPS_svinfo[i].cno);
        EXPECT_EQ(0, GPS_svinfo[i].quality);
    }
    EXPECT_EQ(updateBefore, GPS_update);
}

TEST_F(GpsNmeaTest, TimesOutReinitializesAndRecoversAfterPartialInput)
{
    gpsInit();

    ASSERT_EQ(GPS_STATE_DETECT_BAUD, gpsData.state);
    ASSERT_EQ(1U, fakeSerial.openCount);
    EXPECT_EQ(SERIAL_PORT_UART5, fakeSerial.openedIdentifier);
    EXPECT_EQ(FUNCTION_GPS, fakeSerial.openedFunction);
    EXPECT_EQ(38400U, fakeSerial.openedBaudRate);
    EXPECT_EQ(MODE_RXTX, fakeSerial.openedMode);
    EXPECT_NE(0, fakeSerial.openedOptions & SERIAL_CHECK_TX);

    advanceGpsBy(501);
    ASSERT_EQ(GPS_STATE_CHANGE_BAUD, gpsData.state);
    advanceGpsBy(501);
    ASSERT_EQ(GPS_STATE_CHANGE_BAUD, gpsData.state);
    EXPECT_EQ(1U, fakeSerial.baudChangeCount);
    EXPECT_EQ(38400U, fakeGpsPort.baudRate);
    advanceGpsBy(501);
    ASSERT_EQ(GPS_STATE_RECEIVING_DATA, gpsData.state);

    const timeMs_t lastNavBeforeNoise = gpsData.lastNavMessage;
    queueSerialRx("$GPGGA,123519,4807");
    advanceGpsBy(100);
    EXPECT_EQ(GPS_STATE_RECEIVING_DATA, gpsData.state);
    EXPECT_FALSE(sensors(SENSOR_GPS));
    EXPECT_EQ(0, STATE(GPS_FIX));
    EXPECT_EQ(lastNavBeforeNoise, gpsData.lastNavMessage);
    ASSERT_EQ(fakeSerial.rx.size(), fakeSerial.rxOffset);

    gpsSol.groundSpeed = 4321;
    queueSerialRx(makeNmeaSentence("GPRMC,123519,A,4807.038,N,01131.000,E,12.3,84.4,230394,,", 1));
    advanceGpsBy(100);
    EXPECT_EQ(GPS_STATE_RECEIVING_DATA, gpsData.state);
    EXPECT_FALSE(sensors(SENSOR_GPS));
    EXPECT_EQ(0, STATE(GPS_FIX));
    EXPECT_EQ(4321, gpsSol.groundSpeed);
    EXPECT_EQ(lastNavBeforeNoise, gpsData.lastNavMessage);
    ASSERT_EQ(fakeSerial.rx.size(), fakeSerial.rxOffset);

    const uint8_t updateBeforeFirstFix = GPS_update;
    queueSerialRx(makeNmeaSentence("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    advanceGpsBy(100);

    ASSERT_EQ(GPS_STATE_RECEIVING_DATA, gpsData.state);
    ASSERT_TRUE(sensors(SENSOR_GPS));
    ASSERT_NE(0, STATE(GPS_FIX));
    EXPECT_NE(0, STATE(GPS_FIX_EVER));
    EXPECT_EQ(481173000, gpsSol.llh.lat);
    EXPECT_EQ(115166666, gpsSol.llh.lon);
    EXPECT_EQ(8, gpsSol.numSat);
    EXPECT_EQ(fakeNowMs, gpsData.lastNavMessage);
    EXPECT_EQ(static_cast<uint8_t>(updateBeforeFirstFix ^ GPS_DIRECT_TICK), GPS_update);
    ASSERT_EQ(fakeSerial.rx.size(), fakeSerial.rxOffset);

    advanceGpsBy(2500);
    ASSERT_EQ(GPS_STATE_RECEIVING_DATA, gpsData.state);
    EXPECT_TRUE(sensors(SENSOR_GPS));
    EXPECT_NE(0, STATE(GPS_FIX));

    advanceGpsBy(1);
    ASSERT_EQ(GPS_STATE_LOST_COMMUNICATION, gpsData.state);
    EXPECT_FALSE(sensors(SENSOR_GPS));
    EXPECT_NE(0, STATE(GPS_FIX));
    EXPECT_EQ(8, gpsSol.numSat);
    EXPECT_EQ(0U, gpsData.timeouts);
    EXPECT_EQ(0, gpsData.state_position);
    EXPECT_EQ(fakeNowMs, gpsData.state_ts);

    updateGps();
    ASSERT_EQ(GPS_STATE_DETECT_BAUD, gpsData.state);
    EXPECT_FALSE(sensors(SENSOR_GPS));
    EXPECT_EQ(0, STATE(GPS_FIX));
    EXPECT_NE(0, STATE(GPS_FIX_EVER));
    EXPECT_EQ(0, gpsSol.numSat);
    EXPECT_EQ(1U, gpsData.timeouts);
    EXPECT_EQ(0, gpsData.state_position);
    EXPECT_EQ(fakeNowMs, gpsData.state_ts);

    advanceGpsBy(501);
    ASSERT_EQ(GPS_STATE_CHANGE_BAUD, gpsData.state);
    advanceGpsBy(501);
    ASSERT_EQ(GPS_STATE_CHANGE_BAUD, gpsData.state);
    advanceGpsBy(501);
    ASSERT_EQ(GPS_STATE_RECEIVING_DATA, gpsData.state);

    const uint8_t updateBeforeRecovery = GPS_update;
    queueSerialRx(makeNmeaSentence("GPGGA,123520,3723.2475,N,12158.3416,W,1,09,0.8,10.0,M,0.0,M,,"));
    advanceGpsBy(100);

    EXPECT_EQ(GPS_STATE_RECEIVING_DATA, gpsData.state);
    EXPECT_TRUE(sensors(SENSOR_GPS));
    EXPECT_NE(0, STATE(GPS_FIX));
    EXPECT_NE(0, STATE(GPS_FIX_EVER));
    EXPECT_NE(481173000, gpsSol.llh.lat);
    EXPECT_LT(gpsSol.llh.lon, 0);
    EXPECT_EQ(9, gpsSol.numSat);
    EXPECT_EQ(1000, gpsSol.llh.altCm);
    EXPECT_EQ(fakeNowMs, gpsData.lastNavMessage);
    EXPECT_EQ(static_cast<uint8_t>(updateBeforeRecovery ^ GPS_DIRECT_TICK), GPS_update);
    EXPECT_EQ(fakeSerial.rx.size(), fakeSerial.rxOffset);
    EXPECT_EQ(1U, gpsData.timeouts);
    EXPECT_EQ(1U, fakeSerial.openCount);
    EXPECT_EQ(2U, fakeSerial.baudChangeCount);
    EXPECT_EQ(0U, fakeBeeperCount);
}

TEST_F(GpsNmeaTest, FragmentedSerialFramePublishesOnceAfterFinalChecksumByte)
{
    gpsInit();
    advanceGpsBy(501);
    advanceGpsBy(501);
    advanceGpsBy(501);
    ASSERT_EQ(GPS_STATE_RECEIVING_DATA, gpsData.state);
    ASSERT_TRUE(gpsIsHealthy());
    ASSERT_EQ(BAUD_38400, getGpsPortActualBaudRateIndex());

    uint16_t dataStamp = UINT16_MAX;
    (void)gpsHasNewData(&dataStamp);
    EXPECT_FALSE(gpsHasNewData(&dataStamp));

    const std::string sentence = makeNmeaSentence("GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    const size_t split = sentence.size() / 2;
    const uint8_t updateBefore = GPS_update;
    const timeMs_t lastNavBefore = gpsData.lastNavMessage;

    queueSerialRx(sentence.substr(0, split));
    advanceGpsBy(100);
    EXPECT_FALSE(sensors(SENSOR_GPS));
    EXPECT_EQ(updateBefore, GPS_update);
    EXPECT_EQ(lastNavBefore, gpsData.lastNavMessage);
    EXPECT_FALSE(gpsHasNewData(&dataStamp));

    queueSerialRx(sentence.substr(split));
    advanceGpsBy(100);
    EXPECT_TRUE(sensors(SENSOR_GPS));
    EXPECT_EQ(static_cast<uint8_t>(updateBefore ^ GPS_DIRECT_TICK), GPS_update);
    EXPECT_EQ(fakeNowMs, gpsData.lastNavMessage);
    EXPECT_TRUE(gpsHasNewData(&dataStamp));
    EXPECT_FALSE(gpsHasNewData(&dataStamp));
    EXPECT_EQ(481173000, gpsSol.llh.lat);
    EXPECT_EQ(115166666, gpsSol.llh.lon);
    EXPECT_EQ(8, gpsSol.numSat);
    EXPECT_GE(gpsSol.navIntervalMs, 50U);
    EXPECT_LE(gpsSol.navIntervalMs, 2500U);
    EXPECT_FLOAT_EQ(gpsSol.navIntervalMs * 0.001f, getGpsDataIntervalSeconds());
    EXPECT_FLOAT_EQ(1.0f / getGpsDataIntervalSeconds(), getGpsDataFrequencyHz());
}

TEST_F(GpsNmeaTest, TimeoutBoundaryRemainsHealthyAtThresholdAcrossTimeWrap)
{
    gpsData.state = GPS_STATE_RECEIVING_DATA;
    gpsData.lastNavMessage = UINT32_MAX - 1000U;
    fakeNowMs = 1499U;
    sensorsSet(SENSOR_GPS);
    ENABLE_STATE(GPS_FIX | GPS_FIX_EVER);
    gpsSol.numSat = 8;

    updateGps();
    EXPECT_EQ(GPS_STATE_RECEIVING_DATA, gpsData.state);
    EXPECT_TRUE(gpsIsHealthy());
    EXPECT_TRUE(sensors(SENSOR_GPS));
    EXPECT_NE(0, STATE(GPS_FIX));

    fakeNowMs = 1500U;
    updateGps();
    EXPECT_EQ(GPS_STATE_LOST_COMMUNICATION, gpsData.state);
    EXPECT_FALSE(gpsIsHealthy());
    EXPECT_FALSE(sensors(SENSOR_GPS));
    EXPECT_NE(0, STATE(GPS_FIX));

    updateGps();
    EXPECT_EQ(GPS_STATE_DETECT_BAUD, gpsData.state);
    EXPECT_EQ(0, STATE(GPS_FIX));
    EXPECT_NE(0, STATE(GPS_FIX_EVER));
    EXPECT_EQ(0, gpsSol.numSat);
    EXPECT_EQ(1U, gpsData.timeouts);
}

TEST_F(GpsNmeaTest, GsvOutOfOrderDuplicateAndLegacyCountClippingAreDeterministic)
{
    GPS_numCh = 0;
    memset(GPS_svinfo, 0, sizeof(GPS_svinfo));
    const uint8_t updateBefore = GPS_update;

    EXPECT_FALSE(feedNmeaPayload("GNGSV,5,2,20,05,40,083,31,06,17,308,32,07,13,172,33,08,09,301,34"));
    EXPECT_EQ(GPS_SV_MAXSATS_LEGACY, GPS_numCh);
    for (unsigned i = 4; i < 8; i++) {
        EXPECT_EQ(i + 1, GPS_svinfo[i].chn);
        EXPECT_EQ(i + 1, GPS_svinfo[i].svid);
        EXPECT_EQ(27 + i, GPS_svinfo[i].cno);
    }

    EXPECT_FALSE(feedNmeaPayload("GNGSV,5,2,20,05,40,083,41,06,17,308,42,07,13,172,43,08,09,301,44"));
    for (unsigned i = 4; i < 8; i++) {
        EXPECT_EQ(37 + i, GPS_svinfo[i].cno);
    }

    EXPECT_FALSE(feedNmeaPayload("GNGSV,5,5,20,17,40,083,91,18,17,308,92,19,13,172,93,20,09,301,94"));
    for (unsigned i = 4; i < 8; i++) {
        EXPECT_EQ(37 + i, GPS_svinfo[i].cno);
    }
    EXPECT_EQ(updateBefore, GPS_update);
}

TEST_F(GpsNmeaTest, EmptyGsaDilutionFieldsClearValuesWithoutNavigationTick)
{
    gpsSol.dop.pdop = 123;
    gpsSol.dop.hdop = 90;
    gpsSol.dop.vdop = 80;
    const uint8_t updateBefore = GPS_update;

    EXPECT_FALSE(feedNmeaPayload("GPGSA,A,1,,,,,,,,,,,,,,,"));
    EXPECT_EQ(0, gpsSol.dop.pdop);
    EXPECT_EQ(0, gpsSol.dop.hdop);
    EXPECT_EQ(0, gpsSol.dop.vdop);
    EXPECT_EQ(updateBefore, GPS_update);
}

TEST_F(GpsNmeaTest, VoidRmcDoesNotPublishMotionAndNextValidRmcRecovers)
{
    gpsSol.groundSpeed = 4321;
    gpsSol.groundCourse = 987;
    const uint8_t updateBefore = GPS_update;

    EXPECT_FALSE(feedNmeaPayload("GPRMC,123519,V,4807.038,N,01131.000,E,12.3,84.4,230394,,"));
    EXPECT_EQ(4321, gpsSol.groundSpeed);
    EXPECT_EQ(987, gpsSol.groundCourse);
    EXPECT_EQ(updateBefore, GPS_update);

    EXPECT_FALSE(feedNmeaPayload("GPRMC,123520,A,4807.038,N,01131.000,E,12.3,84.4,230394,,"));
    EXPECT_EQ(632, gpsSol.groundSpeed);
    EXPECT_EQ(844, gpsSol.groundCourse);
    EXPECT_EQ(updateBefore, GPS_update);
}

TEST_F(GpsNmeaTest, FifteenByteStoredFieldIsMemorySafeAndParserRecovers)
{
    gpsSol.llh.lat = 123456789;
    gpsSol.llh.lon = -234567890;

    EXPECT_FALSE(feedNmeaPayload("GPXXX,123456789012345,noise"));
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);

    EXPECT_TRUE(feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    EXPECT_EQ(481173000, gpsSol.llh.lat);
    EXPECT_EQ(115166666, gpsSol.llh.lon);
}

extern "C" baudRate_e lookupBaudRateIndex(uint32_t baudRate)
{
    for (unsigned index = 0; index < BAUD_COUNT; index++) {
        if (baudRates[index] == baudRate) {
            return static_cast<baudRate_e>(index);
        }
    }
    return BAUD_AUTO;
}
