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
#include <string.h>

#include <limits.h>

extern "C" {
    #include "platform.h"

    #include "drivers/serial.h"
    #include "drivers/serial_softserial.h"
    #include "drivers/serial_uart.h"

    #include "io/serial.h"

    #include "rx/rx.h"

    #include "pg/pg.h"
    #include "pg/pg_ids.h"
    #include "pg/rx.h"
    #include "pg/msp.h"

    void serialInit(bool softserialEnabled);

    PG_REGISTER(rxConfig_t, rxConfig, PG_RX_CONFIG, 0);
    PG_REGISTER(mspConfig_t, mspConfig, PG_MSP_CONFIG, 0);
    PG_REGISTER(serialPinConfig_t, serialPinConfig, PG_SERIAL_PIN_CONFIG, 0);
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

static uint32_t stubbedFunctionMask[SERIAL_PORT_COUNT];

static void setStubbedFunctionMask(serialPortIdentifier_e identifier, uint32_t mask)
{
    stubbedFunctionMask[findSerialPortIndexByIdentifier(identifier)] = mask;
}

TEST(IoSerialTest, TestPortSharing)
{
    // given
    memset(stubbedFunctionMask, 0, sizeof(stubbedFunctionMask));
    serialInit(false);

    // then nothing claims a port, so no function is in use
    EXPECT_EQ(PORTSHARING_UNUSED, determinePortSharing(SERIAL_PORT_UART1, FUNCTION_MSP));
    EXPECT_FALSE(isSerialPortShared(SERIAL_PORT_UART1, FUNCTION_MSP, FUNCTION_BLACKBOX));

    // when a single function claims the port
    setStubbedFunctionMask(SERIAL_PORT_UART1, FUNCTION_MSP);

    // then
    EXPECT_EQ(PORTSHARING_NOT_SHARED, determinePortSharing(SERIAL_PORT_UART1, FUNCTION_MSP));
    EXPECT_EQ(PORTSHARING_UNUSED, determinePortSharing(SERIAL_PORT_UART1, FUNCTION_BLACKBOX));
    EXPECT_EQ(PORTSHARING_UNUSED, determinePortSharing(SERIAL_PORT_UART2, FUNCTION_MSP));
    EXPECT_FALSE(isSerialPortShared(SERIAL_PORT_UART1, FUNCTION_MSP, FUNCTION_BLACKBOX));

    // when a second function joins it
    setStubbedFunctionMask(SERIAL_PORT_UART1, FUNCTION_MSP | FUNCTION_BLACKBOX);

    // then
    EXPECT_EQ(PORTSHARING_SHARED, determinePortSharing(SERIAL_PORT_UART1, FUNCTION_MSP));
    EXPECT_EQ(PORTSHARING_SHARED, determinePortSharing(SERIAL_PORT_UART1, FUNCTION_BLACKBOX));
    EXPECT_TRUE(isSerialPortShared(SERIAL_PORT_UART1, FUNCTION_MSP, FUNCTION_BLACKBOX));

    // and SERIAL_PORT_NONE is never in use
    EXPECT_EQ(PORTSHARING_UNUSED, determinePortSharing(SERIAL_PORT_NONE, FUNCTION_MSP));
    EXPECT_FALSE(isSerialPortShared(SERIAL_PORT_NONE, FUNCTION_MSP, FUNCTION_BLACKBOX));
}


struct ResetCalled {};
static const serialPort_t *hostPort = NULL;
static uint32_t fakeMillis = 0;
static int plusToSend = 0;

// STUBS
extern "C" {
    void delay(uint32_t) {}

    bool isSerialTransmitBufferEmpty(const serialPort_t *) { return true; }

    void systemResetToBootloader(void) {}

    bool telemetryCheckRxPortShared(serialPortIdentifier_e, SerialRXType) { return false; }

    uint32_t serialRxBytesWaiting(const serialPort_t *p) { return p == hostPort ? plusToSend : 0; }
    uint8_t serialRead(serialPort_t *) { plusToSend--; return '+'; }
    void serialWrite(serialPort_t *, uint8_t) {}

    uint32_t millis(void) { return fakeMillis += 1000; }  // advance so the "+++" idle guard always passes
    void systemReset(void) { throw ResetCalled(); }

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

    // serial_feature_map is exercised by its own unit test; stub here so
    // determinePortSharing/isSerialPortShared can be driven directly.
    uint32_t serialSynthesizeFunctionMask(serialPortIdentifier_e identifier) {
        const int index = findSerialPortIndexByIdentifier(identifier);
        return index < 0 ? 0 : stubbedFunctionMask[index];
    }
}

TEST(IoSerialTest, TestFunctionsConflict)
{
    memset(stubbedFunctionMask, 0, sizeof(stubbedFunctionMask));

    // a lone function never clashes with itself
    setStubbedFunctionMask(SERIAL_PORT_UART1, FUNCTION_MSP);
    EXPECT_FALSE(serialPortFunctionsConflict(SERIAL_PORT_UART1));

    // MSP sharing with what it is allowed to share with
    setStubbedFunctionMask(SERIAL_PORT_UART1, FUNCTION_MSP | FUNCTION_BLACKBOX);
    EXPECT_FALSE(serialPortFunctionsConflict(SERIAL_PORT_UART1));

    // an MT rangefinder is heard over MSP on the port it declares
    setStubbedFunctionMask(SERIAL_PORT_UART1, FUNCTION_MSP | FUNCTION_LIDAR);
    EXPECT_FALSE(serialPortFunctionsConflict(SERIAL_PORT_UART1));

    // MSP cannot share with serial RX
    setStubbedFunctionMask(SERIAL_PORT_UART1, FUNCTION_MSP | FUNCTION_RX_SERIAL);
    EXPECT_TRUE(serialPortFunctionsConflict(SERIAL_PORT_UART1));

    // nor does an allowed pairing excuse a function outside the set
    setStubbedFunctionMask(SERIAL_PORT_UART1, FUNCTION_MSP | FUNCTION_BLACKBOX | FUNCTION_RX_SERIAL);
    EXPECT_TRUE(serialPortFunctionsConflict(SERIAL_PORT_UART1));

    setStubbedFunctionMask(SERIAL_PORT_UART1, FUNCTION_MSP | FUNCTION_LIDAR | FUNCTION_GPS);
    EXPECT_TRUE(serialPortFunctionsConflict(SERIAL_PORT_UART1));

    memset(stubbedFunctionMask, 0, sizeof(stubbedFunctionMask));
}

TEST(IoSerialTest, TestPassthroughEscape)
{
    // given
    serialPort_t left = {}, right = {};
    right.identifier = SERIAL_PORT_UART1;   // non-USB host -> "+++" escape enabled
    hostPort = &right;
    fakeMillis = 0;
    plusToSend = 3;
    // when "+++" arrives after an idle gap, then it must reboot out of passthrough
    EXPECT_THROW(serialPassthrough(&left, &right, NULL, NULL), ResetCalled);
}

TEST(IoSerialTest, ClassifiesSynthesizedFunctionsAndConfigurationUse)
{
    memset(stubbedFunctionMask, 0, sizeof(stubbedFunctionMask));
    setStubbedFunctionMask(SERIAL_PORT_USART1, FUNCTION_GPS | FUNCTION_BLACKBOX);
    setStubbedFunctionMask(SERIAL_PORT_USART2, FUNCTION_GPS);

    EXPECT_EQ(PORTSHARING_SHARED, determinePortSharing(SERIAL_PORT_USART1, FUNCTION_GPS));
    EXPECT_TRUE(isSerialPortShared(SERIAL_PORT_USART1, FUNCTION_BLACKBOX, FUNCTION_GPS));
    EXPECT_FALSE(isSerialPortShared(SERIAL_PORT_USART1, FUNCTION_RX_SERIAL, FUNCTION_GPS));
    EXPECT_EQ(PORTSHARING_NOT_SHARED, determinePortSharing(SERIAL_PORT_USART2, FUNCTION_GPS));
    EXPECT_EQ(PORTSHARING_UNUSED, determinePortSharing(SERIAL_PORT_NONE, FUNCTION_GPS));
    EXPECT_EQ(PORTSHARING_UNUSED, determinePortSharing(SERIAL_PORT_USART2, FUNCTION_MSP));
    EXPECT_TRUE(doesConfigurationUsePort(SERIAL_PORT_USART1));
    EXPECT_FALSE(doesConfigurationUsePort(SERIAL_PORT_USART3));
}

TEST(IoSerialTest, ValidationRejectsMissingMspUsbAndIllegalSharingCombinations)
{
    memset(stubbedFunctionMask, 0, sizeof(stubbedFunctionMask));
    memset(mspConfigMutable(), 0, sizeof(*mspConfigMutable()));
    for (unsigned slot = 0; slot < MAX_MSP_PORT_COUNT; slot++) {
        mspConfigMutable()->msp_uart[slot] = SERIAL_PORT_NONE;
    }

    mspConfigMutable()->msp_uart[0] = SERIAL_PORT_USB_VCP;
    setStubbedFunctionMask(SERIAL_PORT_USB_VCP, FUNCTION_MSP);
    EXPECT_TRUE(isSerialConfigValid());

    setStubbedFunctionMask(SERIAL_PORT_USB_VCP, FUNCTION_MSP | FUNCTION_BLACKBOX);
    EXPECT_TRUE(isSerialConfigValid());

    setStubbedFunctionMask(SERIAL_PORT_USB_VCP, FUNCTION_NONE);
    EXPECT_FALSE(isSerialConfigValid());

    setStubbedFunctionMask(SERIAL_PORT_USB_VCP, FUNCTION_GPS);
    EXPECT_FALSE(isSerialConfigValid());

    setStubbedFunctionMask(SERIAL_PORT_USB_VCP, FUNCTION_MSP);
    setStubbedFunctionMask(SERIAL_PORT_USART1, FUNCTION_GPS | FUNCTION_RX_SERIAL);
    EXPECT_FALSE(isSerialConfigValid());
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
