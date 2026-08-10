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

    #include "io/gps.h"
    #include "io/serial.h"

    #include "pg/gps.h"
    #include "pg/pg.h"
    #include "pg/pg_ids.h"
    #include "pg/rx.h"

    #include "rx/rx.h"

    extern const pgRegistry_t serialConfig_Registry;
    extern const pgRegistry_t rxConfig_Registry;
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

const serialPortConfig_t *portConfig(serialPortIdentifier_e identifier)
{
    return serialFindPortConfiguration(identifier);
}

} // namespace

TEST(BoardCGpsDefaultsTest, PreservesGlobalGpsFallbacksOrAppliesBoardOverrides)
{
#ifdef BOARD_C_DEFAULTS_VARIANT
    EXPECT_EQ(GPS_NMEA, pgResetTemplate_gpsConfig.provider);
    EXPECT_EQ(GPS_AUTOCONFIG_OFF, pgResetTemplate_gpsConfig.autoConfig);
#else
    EXPECT_EQ(GPS_UBLOX, pgResetTemplate_gpsConfig.provider);
    EXPECT_EQ(GPS_AUTOCONFIG_ON, pgResetTemplate_gpsConfig.autoConfig);
#endif
    EXPECT_EQ(GPS_AUTOBAUD_OFF, pgResetTemplate_gpsConfig.autoBaud);
}

TEST(BoardCGpsDefaultsTest, ResetsSerialPortsWithoutTupleCollisions)
{
    resetWithFunction(serialConfig_Registry);

    for (int index = 0; index < SERIAL_PORT_COUNT; index++) {
        const serialPortConfig_t &config = serialConfig()->portConfigs[index];
        EXPECT_EQ(serialPortIdentifiers[index], config.identifier);
        EXPECT_EQ(BAUD_115200, config.msp_baudrateIndex);
        baudRate_e expectedGpsBaudrate = BAUD_57600;
#ifdef BOARD_C_DEFAULTS_VARIANT
        if (config.identifier == SERIAL_PORT_UART5) {
            expectedGpsBaudrate = BAUD_38400;
        }
#endif
        EXPECT_EQ(expectedGpsBaudrate, config.gps_baudrateIndex);
        EXPECT_EQ(BAUD_AUTO, config.telemetry_baudrateIndex);
        EXPECT_EQ(BAUD_115200, config.blackbox_baudrateIndex);

        serialPortFunction_e expectedFunction = FUNCTION_NONE;
        if (config.identifier == SERIAL_PORT_USB_VCP) {
            expectedFunction = FUNCTION_MSP;
        }
#ifdef BOARD_C_DEFAULTS_VARIANT
        else if (config.identifier == SERIAL_PORT_UART5) {
            expectedFunction = FUNCTION_GPS;
        } else if (config.identifier == SERIAL_PORT_USART2) {
            expectedFunction = FUNCTION_RX_SERIAL;
        } else if (config.identifier == SERIAL_PORT_USART3) {
            expectedFunction = FUNCTION_ESC_SENSOR;
        }
#endif
        EXPECT_EQ(expectedFunction, config.functionMask);
    }

    EXPECT_EQ('R', serialConfig()->reboot_character);
    EXPECT_EQ(100, serialConfig()->serial_update_rate_hz);

    ASSERT_NE(nullptr, portConfig(SERIAL_PORT_USB_VCP));
    EXPECT_EQ(FUNCTION_MSP, portConfig(SERIAL_PORT_USB_VCP)->functionMask);

#ifdef BOARD_C_DEFAULTS_VARIANT
    ASSERT_NE(nullptr, portConfig(SERIAL_PORT_UART5));
    EXPECT_EQ(FUNCTION_GPS, portConfig(SERIAL_PORT_UART5)->functionMask);

    ASSERT_NE(nullptr, portConfig(SERIAL_PORT_USART2));
    EXPECT_EQ(FUNCTION_RX_SERIAL, portConfig(SERIAL_PORT_USART2)->functionMask);

    ASSERT_NE(nullptr, portConfig(SERIAL_PORT_USART3));
    EXPECT_EQ(FUNCTION_ESC_SENSOR, portConfig(SERIAL_PORT_USART3)->functionMask);
#else
    EXPECT_EQ(nullptr, findSerialPortConfig(FUNCTION_GPS));
    EXPECT_EQ(nullptr, findSerialPortConfig(FUNCTION_RX_SERIAL));
    EXPECT_EQ(nullptr, findSerialPortConfig(FUNCTION_ESC_SENSOR));
#endif
}

TEST(BoardCGpsDefaultsTest, ResetsReceiverAndFeatureDefaults)
{
    resetWithFunction(rxConfig_Registry);

#ifdef BOARD_C_DEFAULTS_VARIANT
    EXPECT_EQ(SERIALRX_SBUS, rxConfig()->serialrx_provider);
    const uint32_t expectedFeatures = FEATURE_RX_SERIAL | FEATURE_ANTI_GRAVITY | FEATURE_AIRMODE | FEATURE_GPS;
#else
    EXPECT_EQ(SERIALRX_CRSF, rxConfig()->serialrx_provider);
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
void systemResetToBootloader(void) {}
bool telemetryCheckRxPortShared(const serialPortConfig_t *) { return false; }
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
