/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
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

#include <stdbool.h>
#include <stdint.h>

extern "C" {
    #include "platform.h"

    #include "config/feature.h"

    #include "drivers/serial.h"
    #include "drivers/serial_softserial.h"
    #include "drivers/serial_uart.h"
    #include "drivers/system.h"

    #include "io/gps.h"
    #include "io/serial.h"

    #include "pg/gps.h"
    #include "pg/msp.h"
    #include "pg/pg.h"
    #include "pg/pg_ids.h"
    #include "pg/rx.h"

    #include "rx/rx.h"
    #include "sensors/esc_sensor.h"

    extern const pgRegistry_t mspConfig_Registry;
    extern const pgRegistry_t serialConfig_Registry;
    extern const pgRegistry_t rxConfig_Registry;
    extern const escSensorConfig_t pgResetTemplate_escSensorConfig;
    extern const gpsConfig_t pgResetTemplate_gpsConfig;
    extern const featureConfig_t pgResetTemplate_featureConfig;

    PG_REGISTER(serialPinConfig_t, serialPinConfig, PG_SERIAL_PIN_CONFIG, 0);
}

#include "gtest/gtest.h"

namespace {

void resetWithFunction(const pgRegistry_t &registry)
{
    ASSERT_NE(nullptr, registry.reset.fn);
    registry.reset.fn(registry.address);
}

} // namespace

TEST(BoardCGpsDefaultsTest, PreservesGlobalGpsFallbacksOrAppliesBoardOverrides)
{
#ifdef BOARD_C_DEFAULTS_VARIANT
    EXPECT_EQ(GPS_NMEA, pgResetTemplate_gpsConfig.provider);
    EXPECT_EQ(GPS_AUTOCONFIG_OFF, pgResetTemplate_gpsConfig.autoConfig);
    EXPECT_EQ(SERIAL_PORT_UART5, pgResetTemplate_gpsConfig.gps_uart);
    EXPECT_EQ(BAUD_38400, pgResetTemplate_gpsConfig.gps_baud);
#else
    EXPECT_EQ(GPS_UBLOX, pgResetTemplate_gpsConfig.provider);
    EXPECT_EQ(GPS_AUTOCONFIG_ON, pgResetTemplate_gpsConfig.autoConfig);
    EXPECT_EQ(SERIAL_PORT_NONE, pgResetTemplate_gpsConfig.gps_uart);
    EXPECT_EQ(BAUD_57600, pgResetTemplate_gpsConfig.gps_baud);
#endif
    EXPECT_EQ(GPS_AUTOBAUD_OFF, pgResetTemplate_gpsConfig.autoBaud);
}

TEST(BoardCGpsDefaultsTest, ResetsFeatureOwnedSerialAssignmentsWithoutCollisions)
{
    resetWithFunction(serialConfig_Registry);
    EXPECT_EQ('R', serialConfig()->reboot_character);
    EXPECT_EQ(100, serialConfig()->serial_update_rate_hz);

    resetWithFunction(mspConfig_Registry);
    EXPECT_EQ(SERIAL_PORT_USB_VCP, mspConfig()->msp_uart[0]);
    EXPECT_EQ(BAUD_115200, mspConfig()->msp_baud[0]);
    for (unsigned slot = 1; slot < MAX_MSP_PORT_COUNT; slot++) {
        EXPECT_EQ(SERIAL_PORT_NONE, mspConfig()->msp_uart[slot]);
        EXPECT_EQ(BAUD_115200, mspConfig()->msp_baud[slot]);
    }

#ifdef BOARD_C_DEFAULTS_VARIANT
    EXPECT_EQ(SERIAL_PORT_UART5, pgResetTemplate_gpsConfig.gps_uart);
    EXPECT_EQ(BAUD_38400, pgResetTemplate_gpsConfig.gps_baud);
    EXPECT_EQ(SERIAL_PORT_USART3, pgResetTemplate_escSensorConfig.esc_sensor_uart);
#else
    EXPECT_EQ(SERIAL_PORT_NONE, pgResetTemplate_gpsConfig.gps_uart);
    EXPECT_EQ(BAUD_57600, pgResetTemplate_gpsConfig.gps_baud);
    EXPECT_EQ(SERIAL_PORT_NONE, pgResetTemplate_escSensorConfig.esc_sensor_uart);
#endif
}

TEST(BoardCGpsDefaultsTest, ResetsReceiverAndFeatureDefaults)
{
    resetWithFunction(rxConfig_Registry);

#ifdef BOARD_C_DEFAULTS_VARIANT
    EXPECT_EQ(SERIALRX_SBUS, rxConfig()->serialrx_provider);
    EXPECT_EQ(SERIAL_PORT_USART2, rxConfig()->rx_uart);
    const uint32_t expectedFeatures = FEATURE_RX_SERIAL | FEATURE_ANTI_GRAVITY | FEATURE_AIRMODE | FEATURE_GPS;
#else
    EXPECT_EQ(SERIALRX_CRSF, rxConfig()->serialrx_provider);
    EXPECT_EQ(SERIAL_PORT_NONE, rxConfig()->rx_uart);
    const uint32_t expectedFeatures = FEATURE_RX_SERIAL | FEATURE_ANTI_GRAVITY | FEATURE_AIRMODE;
#endif
    EXPECT_EQ(0, rxConfig()->serialrx_inverted);
    EXPECT_EQ(0, rxConfig()->halfDuplex);
    EXPECT_FALSE(rxConfig()->sbus_baud_fast);
    EXPECT_EQ(expectedFeatures, pgResetTemplate_featureConfig.enabledFeatures);
}

extern "C" {

void parseRcChannels(const char *, rxConfig_t *) {}

void delay(uint32_t) {}
bool isSerialTransmitBufferEmpty(const serialPort_t *) { return true; }
void systemResetToBootloader(bootloaderRequestType_e) {}
bool telemetryCheckRxPortShared(serialPortIdentifier_e, SerialRXType) { return false; }
uint32_t serialRxBytesWaiting(const serialPort_t *) { return 0; }
uint8_t serialRead(serialPort_t *) { return 0; }
void serialWrite(serialPort_t *, uint8_t) {}
serialPort_t *usbVcpOpen(void) { return nullptr; }
serialPort_t *uartOpen(serialPortIdentifier_e, serialReceiveCallbackPtr, void *, uint32_t, portMode_e, portOptions_e) { return nullptr; }
serialPort_t *softSerialOpen(serialPortIdentifier_e, serialReceiveCallbackPtr, void *, uint32_t, portMode_e, portOptions_e) { return nullptr; }
void serialSetCtrlLineStateCb(serialPort_t *, void (*)(void *, uint16_t), void *) {}
void serialSetCtrlLineState(serialPort_t *, uint16_t) {}
uint32_t serialTxBytesFree(const serialPort_t *) { return 1; }
void serialSetBaudRateCb(serialPort_t *, void (*)(serialPort_t *, uint32_t), serialPort_t *) {}
void pinioSet(int, bool) {}

}
