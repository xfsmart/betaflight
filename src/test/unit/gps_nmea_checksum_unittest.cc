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

#include <string>

extern "C" {
    #include "platform.h"

    #include "drivers/serial.h"

    #include "fc/runtime_config.h"

    #include "io/gps.h"

    #include "pg/gps.h"
}

#include "gtest/gtest.h"

extern "C" {
uint8_t stateFlags;
void serialWrite(serialPort_t *, uint8_t) {}
}

namespace {

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

bool feedNmeaPayload(const std::string &payload, uint8_t checksumAdjustment = 0)
{
    const std::string sentence = makeNmeaSentence(payload, checksumAdjustment);
    bool completed = false;
    for (const char c : sentence) {
        completed = gpsNewFrame(static_cast<uint8_t>(c)) || completed;
    }
    return completed;
}

bool feedRawNmea(const std::string &sentence)
{
    bool completed = false;
    for (const char c : sentence) {
        completed = gpsNewFrame(static_cast<uint8_t>(c)) || completed;
    }
    return completed;
}

class GpsNmeaChecksumTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        stateFlags = 0;
        gpsConfigMutable()->provider = GPS_NMEA;
        gpsSol = {};
        gpsData = {};
    }
};

} // namespace

TEST_F(GpsNmeaChecksumTest, CommitsValidGgaFix)
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

TEST_F(GpsNmeaChecksumTest, CommitsValidGgaNoFixWithoutOverwritingPosition)
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

TEST_F(GpsNmeaChecksumTest, RejectsBadChecksumsWithoutChangingEitherFixState)
{
    gpsSol.llh.lat = 123456789;
    gpsSol.llh.lon = -234567890;
    gpsSol.numSat = 4;

    EXPECT_FALSE(feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", 1));
    EXPECT_EQ(0, STATE(GPS_FIX | GPS_FIX_EVER));
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);
    EXPECT_EQ(4, gpsSol.numSat);

    ENABLE_STATE(GPS_FIX | GPS_FIX_EVER);

    EXPECT_FALSE(feedNmeaPayload("GPGGA,123520,,,,,0,00,99.9,,,,,,", 1));
    EXPECT_NE(0, STATE(GPS_FIX));
    EXPECT_NE(0, STATE(GPS_FIX_EVER));
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);
    EXPECT_EQ(4, gpsSol.numSat);
}

TEST_F(GpsNmeaChecksumTest, RecoversFromCorruptGgaAtTheNextValidSentence)
{
    ASSERT_TRUE(feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));

    EXPECT_FALSE(feedNmeaPayload("GPGGA,123520,,,,,0,00,99.9,,,,,,", 1));
    EXPECT_NE(0, STATE(GPS_FIX));
    EXPECT_NE(0, STATE(GPS_FIX_EVER));
    EXPECT_EQ(481173000, gpsSol.llh.lat);
    EXPECT_EQ(115166666, gpsSol.llh.lon);
    EXPECT_EQ(8, gpsSol.numSat);

    EXPECT_TRUE(feedNmeaPayload("GPGGA,123521,4907.038,N,01231.000,E,1,09,0.9,100.0,M,46.9,M,,"));
    EXPECT_NE(0, STATE(GPS_FIX));
    EXPECT_NE(0, STATE(GPS_FIX_EVER));
    EXPECT_EQ(491173000, gpsSol.llh.lat);
    EXPECT_EQ(125166666, gpsSol.llh.lon);
    EXPECT_EQ(9, gpsSol.numSat);
    EXPECT_EQ(10000, gpsSol.llh.altCm);
}

TEST_F(GpsNmeaChecksumTest, StartsEachGgaWithFailClosedStagedFix)
{
    ENABLE_STATE(GPS_FIX | GPS_FIX_EVER);
    gpsSol.llh.lat = 123456789;
    gpsSol.llh.lon = -234567890;

    const bool completed = feedNmeaPayload("GPGGA,123520,4807.038,N,01131.000,E");

    EXPECT_TRUE(completed);
    EXPECT_EQ(0, STATE(GPS_FIX));
    EXPECT_NE(0, STATE(GPS_FIX_EVER));
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);
}

TEST_F(GpsNmeaChecksumTest, NoiseAndRestartDelimiterResynchronizeBeforePublishing)
{
    gpsSol.llh.lat = 123456789;
    gpsSol.llh.lon = -234567890;
    gpsSol.numSat = 4;

    const std::string noise = "noise,$GPGGA,123519,4807.038,N,01131";
    bool completed = false;
    for (const char c : noise) {
        completed = gpsNewFrame(static_cast<uint8_t>(c)) || completed;
    }

    EXPECT_FALSE(completed);
    EXPECT_EQ(0, STATE(GPS_FIX | GPS_FIX_EVER));
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);
    EXPECT_EQ(4, gpsSol.numSat);

    EXPECT_TRUE(feedNmeaPayload("GNGGA,123520,3723.2475,N,12158.3416,W,1,09,0.8,10.0,M,0.0,M,,"));
    EXPECT_NE(0, STATE(GPS_FIX | GPS_FIX_EVER));
    EXPECT_EQ(373874583, gpsSol.llh.lat);
    EXPECT_EQ(-1219723600, gpsSol.llh.lon);
    EXPECT_EQ(9, gpsSol.numSat);
    EXPECT_EQ(1000, gpsSol.llh.altCm);
}

TEST_F(GpsNmeaChecksumTest, MissingNonHexAndShortChecksumsFailClosedThenRecover)
{
    ENABLE_STATE(GPS_FIX | GPS_FIX_EVER);
    gpsSol.llh.lat = 123456789;
    gpsSol.llh.lon = -234567890;
    gpsSol.numSat = 7;

    const std::string payload = "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,";

    EXPECT_FALSE(feedRawNmea("$" + payload + "\r\n"));
    EXPECT_FALSE(feedRawNmea("$" + payload + "*ZZ\r\n"));
    EXPECT_FALSE(feedRawNmea("$" + payload + "*0\r\n"));
    EXPECT_NE(0, STATE(GPS_FIX | GPS_FIX_EVER));
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);
    EXPECT_EQ(7, gpsSol.numSat);

    EXPECT_TRUE(feedNmeaPayload("GPGGA,123521,4907.038,N,01231.000,E,1,09,0.9,100.0,M,46.9,M,,"));
    EXPECT_EQ(491173000, gpsSol.llh.lat);
    EXPECT_EQ(125166666, gpsSol.llh.lon);
    EXPECT_EQ(9, gpsSol.numSat);
}

TEST_F(GpsNmeaChecksumTest, BackToBackGpAndGnFramesCommitOnlyAtSentenceBoundaries)
{
    EXPECT_TRUE(feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    EXPECT_EQ(481173000, gpsSol.llh.lat);
    EXPECT_EQ(115166666, gpsSol.llh.lon);
    EXPECT_EQ(8, gpsSol.numSat);

    EXPECT_TRUE(feedNmeaPayload("GNGGA,123520,3723.2475,S,12158.3416,E,1,10,0.8,-1.2,M,0.0,M,,"));
    EXPECT_EQ(-373874583, gpsSol.llh.lat);
    EXPECT_EQ(1219723600, gpsSol.llh.lon);
    EXPECT_EQ(10, gpsSol.numSat);
    EXPECT_EQ(-120, gpsSol.llh.altCm);
}

TEST_F(GpsNmeaChecksumTest, FourteenByteStoredFieldBoundaryDoesNotBlockNextFix)
{
    gpsSol.llh.lat = 123456789;
    gpsSol.llh.lon = -234567890;

    EXPECT_FALSE(feedNmeaPayload("GPXXX,12345678901234,noise"));
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);

    EXPECT_TRUE(feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    EXPECT_EQ(481173000, gpsSol.llh.lat);
    EXPECT_EQ(115166666, gpsSol.llh.lon);
}

TEST_F(GpsNmeaChecksumTest, RejectsChecksumWithTrailingThirdDigit)
{
    gpsSol.llh.lat = 123456789;
    gpsSol.llh.lon = -234567890;
    gpsSol.numSat = 4;

    const std::string sentence =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*470\r\n";
    EXPECT_FALSE(feedRawNmea(sentence));
    EXPECT_EQ(0, STATE(GPS_FIX | GPS_FIX_EVER));
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);
    EXPECT_EQ(4, gpsSol.numSat);
}

TEST_F(GpsNmeaChecksumTest, FifteenByteStoredFieldBoundaryIsMemorySafe)
{
    const std::string boundary = "$GPXXX,123456789012345,\r\n";
    bool completed = false;
    for (const char c : boundary) {
        completed = gpsNewFrame(static_cast<uint8_t>(c)) || completed;
    }

    EXPECT_FALSE(completed);
    EXPECT_TRUE(feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    EXPECT_EQ(481173000, gpsSol.llh.lat);
    EXPECT_EQ(115166666, gpsSol.llh.lon);
}

TEST_F(GpsNmeaChecksumTest, FifteenByteNumericFieldActuallyExercisesGrabFieldsSafely)
{
    EXPECT_TRUE(feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,12345678901234.,M,46.9,M,,"));

    EXPECT_TRUE(feedNmeaPayload("GPGGA,123520,4907.038,N,01231.000,E,1,09,0.9,100.0,M,46.9,M,,"));
    EXPECT_EQ(491173000, gpsSol.llh.lat);
    EXPECT_EQ(125166666, gpsSol.llh.lon);
    EXPECT_EQ(9, gpsSol.numSat);
    EXPECT_EQ(10000, gpsSol.llh.altCm);
}

TEST_F(GpsNmeaChecksumTest, DollarlessFragmentAfterValidSentenceCannotReuseParserState)
{
    ASSERT_TRUE(feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    const gpsLocation_t acceptedLocation = gpsSol.llh;
    const uint8_t acceptedSatellites = gpsSol.numSat;

    std::string fragment = makeNmeaSentence("GPGGA,123520,4907.038,N,01231.000,E,1,09,0.9,100.0,M,46.9,M,,");
    fragment.erase(0, 1);
    EXPECT_FALSE(feedRawNmea(fragment));
    EXPECT_EQ(acceptedLocation.lat, gpsSol.llh.lat);
    EXPECT_EQ(acceptedLocation.lon, gpsSol.llh.lon);
    EXPECT_EQ(acceptedLocation.altCm, gpsSol.llh.altCm);
    EXPECT_EQ(acceptedSatellites, gpsSol.numSat);

    EXPECT_TRUE(feedNmeaPayload("GPGGA,123521,5007.038,N,01331.000,E,1,10,0.9,200.0,M,46.9,M,,"));
    EXPECT_EQ(501173000, gpsSol.llh.lat);
    EXPECT_EQ(135166666, gpsSol.llh.lon);
    EXPECT_EQ(10, gpsSol.numSat);
    EXPECT_EQ(20000, gpsSol.llh.altCm);
}

TEST_F(GpsNmeaChecksumTest, OverlongStoredFieldFailsClosedThenNextFixRecovers)
{
    gpsSol.llh.lat = 123456789;
    gpsSol.llh.lon = -234567890;
    gpsSol.numSat = 4;

    EXPECT_FALSE(feedNmeaPayload("GPGGA,1234567890123456,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    EXPECT_EQ(0, STATE(GPS_FIX | GPS_FIX_EVER));
    EXPECT_EQ(123456789, gpsSol.llh.lat);
    EXPECT_EQ(-234567890, gpsSol.llh.lon);
    EXPECT_EQ(4, gpsSol.numSat);

    EXPECT_TRUE(feedNmeaPayload("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"));
    EXPECT_EQ(481173000, gpsSol.llh.lat);
    EXPECT_EQ(115166666, gpsSol.llh.lon);
    EXPECT_EQ(8, gpsSol.numSat);
}
