/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>

#include <limits.h>

extern "C" {
    #include "platform.h"

    #include "drivers/serial.h"
    #include "drivers/serial_softserial.h"
    #include "drivers/serial_uart.h"

    #include "io/serial.h"

    #include "pg/pg.h"
    #include "pg/pg_ids.h"
    #include "pg/rx.h"

    void serialInit(bool softserialEnabled);

    PG_REGISTER(rxConfig_t, rxConfig, PG_RX_CONFIG, 0);
    PG_REGISTER(serialPinConfig_t, serialPinConfig, PG_SERIAL_PIN_CONFIG, 0);
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

TEST(IoSerialTest, TestFindPortConfig)
{
    // given
    serialInit(false);

    // when
    const serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_MSP);

    // then
    EXPECT_EQ(NULL, portConfig);
}


// STUBS
extern "C" {
    void delay(uint32_t) {}

    bool isSerialTransmitBufferEmpty(const serialPort_t *) { return true; }

    void systemResetToBootloader(void) {}

    bool telemetryCheckRxPortShared(const serialPortConfig_t *) { return false; }

    uint32_t serialRxBytesWaiting(const serialPort_t *) { return 0; }
    uint8_t serialRead(serialPort_t *) { return 0; }
    void serialWrite(serialPort_t *, uint8_t) {}

    serialPort_t *usbVcpOpen(void) { return NULL; }

    serialPort_t *uartOpen(serialPortIdentifier_e, serialReceiveCallbackPtr, void *, uint32_t, portMode_e, portOptions_e) {
      return NULL;
    }

    serialPort_t *softSerialOpen(serialPortIdentifier_e, serialReceiveCallbackPtr, void *, uint32_t, portMode_e, portOptions_e) {
      return NULL;
    }

    void serialSetCtrlLineStateCb(serialPort_t *, void (*)(void *, uint16_t ), void *) {}
    void serialSetCtrlLineState(serialPort_t *, uint16_t ) {}
    uint32_t serialTxBytesFree(const serialPort_t *) {return 1;}

    void serialSetBaudRateCb(serialPort_t *, void (*)(serialPort_t *context, uint32_t baud), serialPort_t *) {}

    void pinioSet(int, bool) {}
}

TEST(IoSerialTest, EnumeratesDuplicateFunctionsAndClassifiesSharingBoundaries)
{
    *serialConfigMutable() = {};
    serialConfigMutable()->portConfigs[0].identifier = SERIAL_PORT_USB_VCP;
    serialConfigMutable()->portConfigs[0].functionMask = FUNCTION_MSP;
    serialConfigMutable()->portConfigs[1].identifier = SERIAL_PORT_USART1;
    serialConfigMutable()->portConfigs[1].functionMask = FUNCTION_GPS | FUNCTION_BLACKBOX;
    serialConfigMutable()->portConfigs[2].identifier = SERIAL_PORT_USART2;
    serialConfigMutable()->portConfigs[2].functionMask = FUNCTION_GPS;

    const serialPortConfig_t *firstGps = findSerialPortConfig(FUNCTION_GPS);
    ASSERT_EQ(&serialConfig()->portConfigs[1], firstGps);
    EXPECT_EQ(PORTSHARING_SHARED, determinePortSharing(firstGps, FUNCTION_GPS));
    EXPECT_TRUE(isSerialPortShared(firstGps, FUNCTION_BLACKBOX, FUNCTION_GPS));
    EXPECT_FALSE(isSerialPortShared(firstGps, FUNCTION_RX_SERIAL, FUNCTION_GPS));

    const serialPortConfig_t *secondGps = findNextSerialPortConfig(FUNCTION_GPS);
    ASSERT_EQ(&serialConfig()->portConfigs[2], secondGps);
    EXPECT_EQ(PORTSHARING_NOT_SHARED, determinePortSharing(secondGps, FUNCTION_GPS));
    EXPECT_EQ(nullptr, findNextSerialPortConfig(FUNCTION_GPS));
    EXPECT_EQ(PORTSHARING_UNUSED, determinePortSharing(nullptr, FUNCTION_GPS));
    EXPECT_EQ(PORTSHARING_UNUSED, determinePortSharing(secondGps, FUNCTION_MSP));
    EXPECT_TRUE(doesConfigurationUsePort(SERIAL_PORT_USART1));
    EXPECT_FALSE(doesConfigurationUsePort(SERIAL_PORT_USART3));
}

TEST(IoSerialTest, ValidationRejectsMissingMspUsbAndIllegalSharingCombinations)
{
    serialConfig_t *config = serialConfigMutable();

    *config = {};
    config->portConfigs[0].identifier = SERIAL_PORT_USB_VCP;
    config->portConfigs[0].functionMask = FUNCTION_MSP;
    EXPECT_TRUE(isSerialConfigValid(config));

    config->portConfigs[0].functionMask = FUNCTION_MSP | FUNCTION_BLACKBOX;
    EXPECT_TRUE(isSerialConfigValid(config));

    config->portConfigs[0].functionMask = FUNCTION_NONE;
    EXPECT_FALSE(isSerialConfigValid(config));

    config->portConfigs[0].functionMask = FUNCTION_GPS;
    EXPECT_FALSE(isSerialConfigValid(config));

    *config = {};
    config->portConfigs[0].identifier = SERIAL_PORT_USB_VCP;
    config->portConfigs[0].functionMask = FUNCTION_MSP;
    config->portConfigs[1].identifier = SERIAL_PORT_USART1;
    config->portConfigs[1].functionMask = FUNCTION_GPS | FUNCTION_RX_SERIAL;
    EXPECT_FALSE(isSerialConfigValid(config));

    *config = {};
    for (unsigned i = 0; i < 4; i++) {
        config->portConfigs[i].identifier = serialPortIdentifiers[i];
        config->portConfigs[i].functionMask = FUNCTION_MSP;
    }
    EXPECT_FALSE(isSerialConfigValid(config));
}

TEST(IoSerialTest, FailedOpenLeavesUsageUnclaimedAndExistingOwnerBlocksDuplicates)
{
    serialInit(false);
    serialPortUsage_t *usage = findSerialPortUsageByIdentifier(SERIAL_PORT_USART1);
    ASSERT_NE(nullptr, usage);
    ASSERT_EQ(FUNCTION_NONE, usage->function);
    ASSERT_EQ(nullptr, usage->serialPort);

    EXPECT_EQ(nullptr, openSerialPort(SERIAL_PORT_USART1, FUNCTION_GPS, nullptr, nullptr, 38400, MODE_RXTX, SERIAL_NOT_INVERTED));
    EXPECT_EQ(FUNCTION_NONE, usage->function);
    EXPECT_EQ(nullptr, usage->serialPort);

    serialPort_t ownedPort = {};
    ownedPort.identifier = SERIAL_PORT_USART1;
    ownedPort.rxCallback = +[](uint16_t, void *) {};
    usage->function = FUNCTION_GPS;
    usage->serialPort = &ownedPort;

    EXPECT_EQ(nullptr, openSerialPort(SERIAL_PORT_USART1, FUNCTION_MSP, nullptr, nullptr, 115200, MODE_RXTX, SERIAL_NOT_INVERTED));
    EXPECT_EQ(FUNCTION_GPS, usage->function);
    EXPECT_EQ(&ownedPort, usage->serialPort);

    closeSerialPort(&ownedPort);
    EXPECT_EQ(nullptr, ownedPort.rxCallback);
    EXPECT_EQ(FUNCTION_NONE, usage->function);
    EXPECT_EQ(nullptr, usage->serialPort);

    closeSerialPort(&ownedPort);
    closeSerialPort(nullptr);
    EXPECT_EQ(FUNCTION_NONE, usage->function);
    EXPECT_EQ(nullptr, usage->serialPort);
}

TEST(IoSerialTest, RemovingPortAfterClosePreventsLaterOwnership)
{
    serialInit(false);
    ASSERT_TRUE(serialIsPortAvailable(SERIAL_PORT_USART2));

    serialRemovePort(SERIAL_PORT_USART2);

    EXPECT_FALSE(serialIsPortAvailable(SERIAL_PORT_USART2));
    EXPECT_EQ(nullptr, findSerialPortUsageByIdentifier(SERIAL_PORT_USART2));
    EXPECT_EQ(nullptr, openSerialPort(SERIAL_PORT_USART2, FUNCTION_GPS, nullptr, nullptr, 38400, MODE_RXTX, SERIAL_NOT_INVERTED));
}
