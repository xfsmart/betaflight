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

bool feedNmeaPayload(const std::string &payload, uint8_t checksumAdjustment = 0)
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
