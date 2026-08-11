/*
 * Board C final resource integration test.
 *
 * This host test deliberately compiles as two variants.  The target-defaults
 * variant links production reset code.  The reqmap variant also links the
 * production FT32 ADC, timer, UART and DMA request-map translation units via
 * the accompanying Makefile stanza.
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

extern "C" {

// This is an actual preprocessor include, not a file-content probe.
#include "../../platform/FT32/config/configs/FT32F405_FT32/config.h"
#include "platform.h"

#include "config/feature.h"
#include "drivers/io.h"
#include "drivers/motor_types.h"
#include "drivers/serial.h"
#include "io/gps.h"
#include "io/serial.h"
#include "pg/gps.h"
#include "pg/motor.h"

extern const pgRegistry_t motorConfig_Registry;

#if defined(BOARD_C_TARGET_DEFAULTS_VARIANT)
#include "pg/adc.h"
#include "pg/bus_i2c.h"
#include "pg/pg.h"
#include "pg/pg_ids.h"
#include "pg/rx.h"
#include "pg/timerio.h"
#include "rx/rx.h"

extern const pgRegistry_t adcConfig_Registry;
extern const pgRegistry_t i2cConfig_Registry;
extern const pgRegistry_t rxConfig_Registry;
extern const pgRegistry_t serialConfig_Registry;
extern const pgRegistry_t serialPinConfig_Registry;
extern const pgRegistry_t timerIOConfig_Registry;
extern const featureConfig_t pgResetTemplate_featureConfig;
extern const gpsConfig_t pgResetTemplate_gpsConfig;
#endif

#if defined(BOARD_C_REQMAP_VARIANT)
#include "drivers/adc.h"
#include "drivers/dma_reqmap.h"
#include "drivers/serial_uart.h"
#include "drivers/serial_uart_impl.h"
#include "drivers/timer.h"
#include "platform/adc_impl.h"

// These storage symbols are referenced by production uartHardware[].
volatile uint8_t uart1RxBuffer[UART_RX_BUFFER_SIZE];
volatile uint8_t uart1TxBuffer[UART_TX_BUFFER_SIZE];
volatile uint8_t uart2RxBuffer[UART_RX_BUFFER_SIZE];
volatile uint8_t uart2TxBuffer[UART_TX_BUFFER_SIZE];
volatile uint8_t uart3RxBuffer[UART_RX_BUFFER_SIZE];
volatile uint8_t uart3TxBuffer[UART_TX_BUFFER_SIZE];
volatile uint8_t uart4RxBuffer[UART_RX_BUFFER_SIZE];
volatile uint8_t uart4TxBuffer[UART_TX_BUFFER_SIZE];
volatile uint8_t uart5RxBuffer[UART_RX_BUFFER_SIZE];
volatile uint8_t uart5TxBuffer[UART_TX_BUFFER_SIZE];
volatile uint8_t uart6RxBuffer[UART_RX_BUFFER_SIZE];
volatile uint8_t uart6TxBuffer[UART_TX_BUFFER_SIZE];
#endif

} // extern "C"

#include "gtest/gtest.h"

#if !defined(BOARD_C_TARGET_DEFAULTS_VARIANT) && !defined(BOARD_C_REQMAP_VARIANT)
#error Select BOARD_C_TARGET_DEFAULTS_VARIANT or BOARD_C_REQMAP_VARIANT
#endif

namespace {

void resetInto(const pgRegistry_t &registry, void *destination)
{
    ASSERT_NE(nullptr, registry.reset.fn);
    registry.reset.fn(destination);
}

#if defined(BOARD_C_TARGET_DEFAULTS_VARIANT)

struct PinOwner {
    ioTag_t tag;
    const char *owner;
};

std::vector<std::string> unexpectedPinAliases(const std::vector<PinOwner> &pins)
{
    std::map<ioTag_t, std::vector<std::string> > ownersByPin;
    for (const PinOwner &pin : pins) {
        if (pin.tag != IO_TAG(NONE)) {
            ownersByPin[pin.tag].push_back(pin.owner);
        }
    }

    std::vector<std::string> errors;
    for (const auto &entry : ownersByPin) {
        if (entry.second.size() < 2U) {
            continue;
        }

        std::vector<std::string> owners = entry.second;
        std::sort(owners.begin(), owners.end());
        const std::vector<std::string> allowedPa3Alias = {
            "RX_PPM_INACTIVE",
            "UART2_RX",
        };
        if (entry.first == IO_TAG(PA3) && owners == allowedPa3Alias) {
            continue;
        }

        std::string error = "pin:" + std::to_string(entry.first);
        for (const std::string &owner : owners) {
            error += ":" + owner;
        }
        errors.push_back(error);
    }
    return errors;
}

void resetWithFunction(const pgRegistry_t &registry)
{
    resetInto(registry, registry.address);
}

const serialPortConfig_t *findPort(const serialConfig_t &config, serialPortIdentifier_e identifier)
{
    for (const serialPortConfig_t &port : config.portConfigs) {
        if (port.identifier == identifier) {
            return &port;
        }
    }
    return nullptr;
}

std::vector<PinOwner> declaredBoardCPins()
{
    return {
        { IO_TAG(BEEPER_PIN), "BEEPER" },
        { IO_TAG(MOTOR1_PIN), "MOTOR1" },
        { IO_TAG(MOTOR2_PIN), "MOTOR2" },
        { IO_TAG(MOTOR3_PIN), "MOTOR3" },
        { IO_TAG(MOTOR4_PIN), "MOTOR4" },
        { IO_TAG(SERVO1_PIN), "SERVO1" },
        { IO_TAG(SERVO2_PIN), "SERVO2" },
        { IO_TAG(RX_PPM_PIN), "RX_PPM_INACTIVE" },
        { IO_TAG(LED_STRIP_PIN), "LED_STRIP" },
        { IO_TAG(LED0_PIN), "LED0" },
        { IO_TAG(UART1_TX_PIN), "UART1_TX" },
        { IO_TAG(UART1_RX_PIN), "UART1_RX" },
        { IO_TAG(UART2_TX_PIN), "UART2_TX" },
        { IO_TAG(UART2_RX_PIN), "UART2_RX" },
        { IO_TAG(UART3_TX_PIN), "UART3_TX" },
        { IO_TAG(UART3_RX_PIN), "UART3_RX" },
        { IO_TAG(UART4_TX_PIN), "UART4_TX" },
        { IO_TAG(UART4_RX_PIN), "UART4_RX" },
        { IO_TAG(UART5_TX_PIN), "UART5_TX" },
        { IO_TAG(UART5_RX_PIN), "UART5_RX" },
        { IO_TAG(I2C1_SCL_PIN), "I2C1_SCL" },
        { IO_TAG(I2C1_SDA_PIN), "I2C1_SDA" },
        { IO_TAG(SPI1_SCK_PIN), "SPI1_SCK" },
        { IO_TAG(SPI1_SDI_PIN), "SPI1_SDI" },
        { IO_TAG(SPI1_SDO_PIN), "SPI1_SDO" },
        { IO_TAG(SPI2_SCK_PIN), "SPI2_SCK" },
        { IO_TAG(SPI2_SDI_PIN), "SPI2_SDI" },
        { IO_TAG(SPI2_SDO_PIN), "SPI2_SDO" },
        { IO_TAG(SPI3_SCK_PIN), "SPI3_SCK" },
        { IO_TAG(SPI3_SDI_PIN), "SPI3_SDI" },
        { IO_TAG(SPI3_SDO_PIN), "SPI3_SDO" },
        { IO_TAG(ADC_VBAT_PIN), "ADC_VBAT" },
        { IO_TAG(ADC_CURR_PIN), "ADC_CURRENT" },
        { IO_TAG(ADC_RSSI_PIN), "ADC_RSSI" },
        { IO_TAG(FLASH_CS_PIN), "FLASH_CS" },
        { IO_TAG(MAX7456_SPI_CS_PIN), "MAX7456_CS" },
        { IO_TAG(GYRO_1_EXTI_PIN), "GYRO1_EXTI" },
        { IO_TAG(GYRO_1_CS_PIN), "GYRO1_CS" },
        { IO_TAG(PINIO1_PIN), "PINIO1" },
    };
}

struct TargetDefaultsSnapshot {
    std::array<int8_t, 4> motorDmaopt;
    uint8_t dshotBurst;
    uint16_t i2cClockKhz;
    bool ppmDefault;
    bool sdioEnabled;
};

std::vector<std::string> targetDefaultErrors(const TargetDefaultsSnapshot &snapshot)
{
    std::vector<std::string> errors;
    const std::array<int8_t, 4> expectedDmaopt = {{ 0, 1, 1, 0 }};
    if (snapshot.motorDmaopt != expectedDmaopt) {
        errors.push_back("motor-dmaopt");
    }
    if (snapshot.dshotBurst != DSHOT_DMAR_OFF) {
        errors.push_back("dshot-burst");
    }
    if (snapshot.i2cClockKhz != 400U) {
        errors.push_back("i2c1-clock");
    }
    if (snapshot.ppmDefault) {
        errors.push_back("ppm-default");
    }
    if (snapshot.sdioEnabled) {
        errors.push_back("sdio-enabled");
    }
    return errors;
}

TEST(BoardCResourceTargetDefaultsTest, Adc3PinsAndSelectionComeFromTargetAndProductionReset)
{
    adcConfig_t adc = {};
    resetInto(adcConfig_Registry, &adc);

    EXPECT_TRUE(adc.vbat.enabled);
    EXPECT_EQ(IO_TAG(PC2), adc.vbat.ioTag);
    EXPECT_TRUE(adc.current.enabled);
    EXPECT_EQ(IO_TAG(PC1), adc.current.ioTag);
    EXPECT_TRUE(adc.rssi.enabled);
    EXPECT_EQ(IO_TAG(PC3), adc.rssi.ioTag);

#define BOARD_C_STRINGIFY_IMPL(value) #value
#define BOARD_C_STRINGIFY(value) BOARD_C_STRINGIFY_IMPL(value)
    EXPECT_STREQ("ADC3", BOARD_C_STRINGIFY(TARGET_ADC_INSTANCE));
#undef BOARD_C_STRINGIFY
#undef BOARD_C_STRINGIFY_IMPL
    EXPECT_EQ(1, ADC3_DMA_OPT);
}

TEST(BoardCResourceTargetDefaultsTest, TimerIoResetPinsAndDmaoptsAreExact)
{
    timerIOConfig_t timerConfig[MAX_TIMER_PINMAP_COUNT] = {};
    resetInto(timerIOConfig_Registry, timerConfig);

    const std::array<ioTag_t, 4> expectedPins = {{
        IO_TAG(PC6), IO_TAG(PC7), IO_TAG(PC8), IO_TAG(PC9),
    }};
    const std::array<int8_t, 4> expectedDmaopt = {{ 0, 1, 1, 0 }};
    for (size_t motor = 0; motor < expectedPins.size(); ++motor) {
        EXPECT_EQ(expectedPins[motor], timerConfig[motor].ioTag) << motor;
        EXPECT_EQ(2, timerConfig[motor].index) << motor;
        EXPECT_EQ(expectedDmaopt[motor], timerConfig[motor].dmaopt) << motor;
    }
}

TEST(BoardCResourceTargetDefaultsTest, MotorResetIsDirectDshot600WithAllOptionalModesOff)
{
    motorConfig_t motor = {};
    resetInto(motorConfig_Registry, &motor);

    EXPECT_EQ(MOTOR_PROTOCOL_DSHOT600, motor.dev.motorProtocol);
    EXPECT_EQ(DSHOT_BITBANG_OFF, motor.dev.useDshotBitbang);
    EXPECT_EQ(DSHOT_DMAR_OFF, motor.dev.useBurstDshot);
    EXPECT_EQ(DSHOT_TELEMETRY_OFF, motor.dev.useDshotTelemetry);
    EXPECT_EQ(IO_TAG(PC6), motor.dev.ioTags[0]);
    EXPECT_EQ(IO_TAG(PC7), motor.dev.ioTags[1]);
    EXPECT_EQ(IO_TAG(PC8), motor.dev.ioTags[2]);
    EXPECT_EQ(IO_TAG(PC9), motor.dev.ioTags[3]);
}

TEST(BoardCResourceTargetDefaultsTest, SerialPinsFunctionsRxAndGpsDefaultsAreExact)
{
    serialPinConfig_t pins = {};
    resetInto(serialPinConfig_Registry, &pins);

    EXPECT_EQ(IO_TAG(PA9), pins.ioTagTx[serialResourceIndex(SERIAL_PORT_USART1)]);
    EXPECT_EQ(IO_TAG(PA10), pins.ioTagRx[serialResourceIndex(SERIAL_PORT_USART1)]);
    EXPECT_EQ(IO_TAG(PA2), pins.ioTagTx[serialResourceIndex(SERIAL_PORT_USART2)]);
    EXPECT_EQ(IO_TAG(PA3), pins.ioTagRx[serialResourceIndex(SERIAL_PORT_USART2)]);
    EXPECT_EQ(IO_TAG(PC12), pins.ioTagTx[serialResourceIndex(SERIAL_PORT_UART5)]);
    EXPECT_EQ(IO_TAG(PD2), pins.ioTagRx[serialResourceIndex(SERIAL_PORT_UART5)]);

    resetWithFunction(serialConfig_Registry);
    const serialConfig_t &serial = *serialConfig();
    const serialPortConfig_t *uart2 = findPort(serial, SERIAL_PORT_USART2);
    const serialPortConfig_t *uart5 = findPort(serial, SERIAL_PORT_UART5);
    ASSERT_NE(nullptr, uart2);
    ASSERT_NE(nullptr, uart5);
    EXPECT_EQ(FUNCTION_RX_SERIAL, uart2->functionMask);
    EXPECT_EQ(FUNCTION_GPS, uart5->functionMask);
    EXPECT_EQ(BAUD_38400, uart5->gps_baudrateIndex);

    resetWithFunction(rxConfig_Registry);
    EXPECT_EQ(SERIALRX_SBUS, rxConfig()->serialrx_provider);
    EXPECT_EQ(0, rxConfig()->halfDuplex);
    EXPECT_NE(0U, pgResetTemplate_featureConfig.enabledFeatures & FEATURE_RX_SERIAL);
    EXPECT_EQ(0U, pgResetTemplate_featureConfig.enabledFeatures & FEATURE_RX_PPM);
    EXPECT_NE(0U, pgResetTemplate_featureConfig.enabledFeatures & FEATURE_GPS);

    EXPECT_EQ(GPS_NMEA, pgResetTemplate_gpsConfig.provider);
    EXPECT_EQ(GPS_AUTOCONFIG_OFF, pgResetTemplate_gpsConfig.autoConfig);
    EXPECT_EQ(IO_TAG(PA3), IO_TAG(RX_PPM_PIN));
    EXPECT_EQ(IO_TAG(UART2_RX_PIN), IO_TAG(RX_PPM_PIN));
}

TEST(BoardCResourceTargetDefaultsTest, I2c1ResetAndMagSelectionAre400KhzOnPb6Pb7)
{
    // The unit-test platform has a zero-sized I2C PG projection.  The reset
    // function accepts the production-sized caller storage used here.
    std::array<i2cConfig_t, 5> i2c = {};
    resetInto(i2cConfig_Registry, i2c.data());

    EXPECT_EQ(IO_TAG(PB6), i2c[I2CDEV_1].ioTagScl);
    EXPECT_EQ(IO_TAG(PB7), i2c[I2CDEV_1].ioTagSda);
    EXPECT_EQ(400U, i2c[I2CDEV_1].clockSpeed);
    EXPECT_EQ(400000U, static_cast<uint32_t>(i2c[I2CDEV_1].clockSpeed) * 1000U);
    EXPECT_EQ(I2CDEV_1, MAG_I2C_INSTANCE);
    EXPECT_NE(IO_TAG(PB6), IO_TAG(UART1_TX_PIN));
    EXPECT_NE(IO_TAG(PB7), IO_TAG(UART1_RX_PIN));
}

TEST(BoardCResourceTargetDefaultsTest, DeclaredPinsAreUniqueExceptInactivePpmAlias)
{
    EXPECT_TRUE(unexpectedPinAliases(declaredBoardCPins()).empty());
}

TEST(BoardCResourceTargetDefaultsTest, SdioIsAbsentAndItsKnownPinoutConflictsIfMutatedOn)
{
#ifdef USE_SDCARD_SDIO
    constexpr bool sdioEnabled = true;
#else
    constexpr bool sdioEnabled = false;
#endif
    EXPECT_FALSE(sdioEnabled);

    std::vector<PinOwner> mutatedPins = declaredBoardCPins();
    mutatedPins.push_back({ IO_TAG(PC12), "SDIO_CK" });
    mutatedPins.push_back({ IO_TAG(PD2), "SDIO_CMD" });
    mutatedPins.push_back({ IO_TAG(PC8), "SDIO_D0" });
    mutatedPins.push_back({ IO_TAG(PC9), "SDIO_D1" });
    mutatedPins.push_back({ IO_TAG(PC11), "SDIO_D3" });

    const std::vector<std::string> conflicts = unexpectedPinAliases(mutatedPins);
    EXPECT_EQ(5U, conflicts.size());
}

TEST(BoardCResourceTargetDefaultsTest, NegativeMutationsAreRejected)
{
    timerIOConfig_t timerConfig[MAX_TIMER_PINMAP_COUNT] = {};
    motorConfig_t motor = {};
    std::array<i2cConfig_t, 5> i2c = {};
    resetInto(timerIOConfig_Registry, timerConfig);
    resetInto(motorConfig_Registry, &motor);
    resetInto(i2cConfig_Registry, i2c.data());

#ifdef USE_SDCARD_SDIO
    constexpr bool sdioEnabled = true;
#else
    constexpr bool sdioEnabled = false;
#endif
    TargetDefaultsSnapshot good = {
        {{ timerConfig[0].dmaopt, timerConfig[1].dmaopt,
           timerConfig[2].dmaopt, timerConfig[3].dmaopt }},
        motor.dev.useBurstDshot,
        i2c[I2CDEV_1].clockSpeed,
        (pgResetTemplate_featureConfig.enabledFeatures & FEATURE_RX_PPM) != 0,
        sdioEnabled,
    };
    EXPECT_TRUE(targetDefaultErrors(good).empty());

    for (const uint8_t burst : { DSHOT_DMAR_ON, DSHOT_DMAR_AUTO }) {
        TargetDefaultsSnapshot mutation = good;
        mutation.dshotBurst = burst;
        EXPECT_FALSE(targetDefaultErrors(mutation).empty());
    }

    TargetDefaultsSnapshot ppm = good;
    ppm.ppmDefault = true;
    EXPECT_FALSE(targetDefaultErrors(ppm).empty());

    TargetDefaultsSnapshot sdio = good;
    sdio.sdioEnabled = true;
    EXPECT_FALSE(targetDefaultErrors(sdio).empty());

    TargetDefaultsSnapshot i2c800 = good;
    i2c800.i2cClockKhz = 800;
    EXPECT_FALSE(targetDefaultErrors(i2c800).empty());

    for (size_t motorIndex = 0; motorIndex < good.motorDmaopt.size(); ++motorIndex) {
        TargetDefaultsSnapshot dmaopt = good;
        dmaopt.motorDmaopt[motorIndex] = dmaopt.motorDmaopt[motorIndex] == 0 ? 1 : 0;
        EXPECT_FALSE(targetDefaultErrors(dmaopt).empty()) << motorIndex;
    }
}

extern "C" void parseRcChannels(const char *, rxConfig_t *) {}

#endif // BOARD_C_TARGET_DEFAULTS_VARIANT

#if defined(BOARD_C_REQMAP_VARIANT)

struct TargetTimerRoute {
    unsigned index;
    ioTag_t tag;
    uint8_t occurrence;
    dmaoptValue_t dmaopt;
};

#define TIMER_PIN_MAP(index_, pin_, occurrence_, dmaopt_) \
    { index_, IO_TAG(pin_), occurrence_, dmaopt_ },
const TargetTimerRoute targetTimerRoutes[] = {
    TIMER_PIN_MAPPING
};
#undef TIMER_PIN_MAP

struct DmaIdentity {
    unsigned controller;
    unsigned handshake;
    unsigned request;
};

uintptr_t expectedDmaChannelAddress(unsigned controller, unsigned handshake)
{
    const uintptr_t base = controller == 1U ? 0x40026000UL : 0x40026400UL;
    return base + handshake * 0x58UL;
}

void expectTuple(const dmaChannelSpec_t *spec, unsigned controller,
                 unsigned handshake, unsigned request)
{
    ASSERT_NE(nullptr, spec);
    EXPECT_EQ(controller, DMA_CODE_CONTROLLER(spec->code));
    EXPECT_EQ(handshake, DMA_CODE_STREAM(spec->code));
    EXPECT_EQ(request, DMA_CODE_REQUEST(spec->code));
    EXPECT_EQ(request, spec->channel);
    EXPECT_EQ(expectedDmaChannelAddress(controller, handshake),
              reinterpret_cast<uintptr_t>(spec->ref));
}

const uartHardware_t *findUartHardware(serialPortIdentifier_e identifier)
{
    for (int index = 0; index < UARTDEV_COUNT; ++index) {
        if (uartHardware[index].identifier == identifier) {
            return &uartHardware[index];
        }
    }
    return nullptr;
}

bool uartSupportsPin(const uartPinDef_t *pins, ioTag_t tag)
{
    for (int index = 0; index < UARTHARDWARE_MAX_PINS; ++index) {
        if (pins[index].pin == tag) {
            return true;
        }
    }
    return false;
}

const adcTagMap_t *findAdcTag(ioTag_t tag)
{
    for (int index = 0; index < ADC_TAG_MAP_COUNT; ++index) {
        if (adcTagMap[index].tag == tag) {
            return &adcTagMap[index];
        }
    }
    return nullptr;
}

TEST(BoardCResourceReqmapTest, TargetTimerOccurrencesResolveThroughProductionTim8TableAndReqmap)
{
    ASSERT_GE(sizeof(targetTimerRoutes) / sizeof(targetTimerRoutes[0]), 4U);
    const std::array<ioTag_t, 4> expectedPins = {{
        IO_TAG(PC6), IO_TAG(PC7), IO_TAG(PC8), IO_TAG(PC9),
    }};
    const std::array<uint8_t, 4> expectedChannels = {{ 0x00, 0x04, 0x08, 0x0c }};
    const std::array<int8_t, 4> expectedDmaopt = {{ 0, 1, 1, 0 }};
    const std::array<DmaIdentity, 4> expectedDma = {{
        { 2, 2, 0 },
        { 2, 3, 7 },
        { 2, 4, 7 },
        { 2, 7, 7 },
    }};

    std::set<std::pair<unsigned, unsigned> > controllerHandshakePairs;
    for (size_t motor = 0; motor < expectedPins.size(); ++motor) {
        const TargetTimerRoute &route = targetTimerRoutes[motor];
        EXPECT_EQ(motor, route.index);
        EXPECT_EQ(expectedPins[motor], route.tag);
        EXPECT_EQ(2, route.occurrence);
        EXPECT_EQ(expectedDmaopt[motor], route.dmaopt);

        const timerHardware_t *timer = timerGetByTagAndIndex(route.tag, route.occurrence);
        ASSERT_NE(nullptr, timer) << motor;
        EXPECT_EQ(0x40010400UL, reinterpret_cast<uintptr_t>(timer->tim)) << motor;
        EXPECT_EQ(expectedChannels[motor], timer->channel) << motor;

        const dmaChannelSpec_t *spec = dmaGetChannelSpecByTimerValue(
            timer->tim, timer->channel, route.dmaopt);
        expectTuple(spec, expectedDma[motor].controller,
                    expectedDma[motor].handshake, expectedDma[motor].request);
        ASSERT_NE(nullptr, spec);
        controllerHandshakePairs.insert({
            DMA_CODE_CONTROLLER(spec->code), DMA_CODE_STREAM(spec->code),
        });
    }
    EXPECT_EQ(4U, controllerHandshakePairs.size());
}

TEST(BoardCResourceReqmapTest, Adc3PinsPlatformMapAndDmaTupleAreExact)
{
    EXPECT_EQ(ADC3, TARGET_ADC_INSTANCE);
    EXPECT_EQ(1, ADC3_DMA_OPT);
    EXPECT_EQ(ADCDEV_3, adcDeviceByInstance(reinterpret_cast<const ADC_TypeDef *>(0x40012200UL)));
    EXPECT_EQ(0x40012200UL, reinterpret_cast<uintptr_t>(adcHardware[ADCDEV_3].ADCx));

    const std::array<std::pair<ioTag_t, uint32_t>, 3> channels = {{
        { IO_TAG(PC2), 3U },
        { IO_TAG(PC1), 2U },
        { IO_TAG(PC3), 4U },
    }};
    for (const auto &channel : channels) {
        const adcTagMap_t *mapping = findAdcTag(channel.first);
        ASSERT_NE(nullptr, mapping);
        EXPECT_NE(0U, mapping->devices & (1U << ADCDEV_3));
        EXPECT_EQ(channel.second, mapping->channel);
        EXPECT_TRUE(adcVerifyPin(channel.first, ADCDEV_3));
    }

    const dmaChannelSpec_t *adc = dmaGetChannelSpecByPeripheral(
        DMA_PERIPH_ADC, ADCDEV_3, ADC3_DMA_OPT);
    expectTuple(adc, 2, 1, 2);
}

TEST(BoardCResourceReqmapTest, MotorAndAdcHandshakeIdentitiesAreGloballyUnique)
{
    const dmaChannelSpec_t *adc = dmaGetChannelSpecByPeripheral(
        DMA_PERIPH_ADC, ADCDEV_3, ADC3_DMA_OPT);
    ASSERT_NE(nullptr, adc);

    std::set<std::pair<unsigned, unsigned> > identities;
    identities.insert({ DMA_CODE_CONTROLLER(adc->code), DMA_CODE_STREAM(adc->code) });
    for (size_t motor = 0; motor < 4; ++motor) {
        const TargetTimerRoute &route = targetTimerRoutes[motor];
        const timerHardware_t *timer = timerGetByTagAndIndex(route.tag, route.occurrence);
        ASSERT_NE(nullptr, timer);
        const dmaChannelSpec_t *spec = dmaGetChannelSpecByTimerValue(
            timer->tim, timer->channel, route.dmaopt);
        ASSERT_NE(nullptr, spec);
        const bool inserted = identities.insert({
            DMA_CODE_CONTROLLER(spec->code), DMA_CODE_STREAM(spec->code),
        }).second;
        EXPECT_TRUE(inserted) << motor;
    }
    EXPECT_EQ(5U, identities.size());
}

TEST(BoardCResourceReqmapTest, BurstDefaultRejectsTim8UpCollisionWithAdc3Hs1)
{
    motorConfig_t motor = {};
    resetInto(motorConfig_Registry, &motor);
    EXPECT_EQ(MOTOR_PROTOCOL_DSHOT600, motor.dev.motorProtocol);
    EXPECT_EQ(DSHOT_BITBANG_OFF, motor.dev.useDshotBitbang);
    EXPECT_EQ(DSHOT_DMAR_OFF, motor.dev.useBurstDshot);
    EXPECT_EQ(DSHOT_TELEMETRY_OFF, motor.dev.useDshotTelemetry);

    const dmaChannelSpec_t *adc = dmaGetChannelSpecByPeripheral(
        DMA_PERIPH_ADC, ADCDEV_3, ADC3_DMA_OPT);
    const dmaChannelSpec_t *tim8Up = dmaGetChannelSpecByPeripheral(
        DMA_PERIPH_TIMUP, 8, 0);
    expectTuple(adc, 2, 1, 2);
    expectTuple(tim8Up, 2, 1, 7);
    ASSERT_NE(nullptr, adc);
    ASSERT_NE(nullptr, tim8Up);
    EXPECT_EQ(DMA_CODE_CONTROLLER(adc->code), DMA_CODE_CONTROLLER(tim8Up->code));
    EXPECT_EQ(DMA_CODE_STREAM(adc->code), DMA_CODE_STREAM(tim8Up->code));

    for (const uint8_t mutation : { DSHOT_DMAR_ON, DSHOT_DMAR_AUTO }) {
        EXPECT_NE(DSHOT_DMAR_OFF, mutation);
        EXPECT_TRUE(DMA_CODE_CONTROLLER(adc->code) == DMA_CODE_CONTROLLER(tim8Up->code)
            && DMA_CODE_STREAM(adc->code) == DMA_CODE_STREAM(tim8Up->code));
    }
}

TEST(BoardCResourceReqmapTest, UartPinsAreSupportedAndUart5DmaIsDisjointFromDma2Set)
{
    const uartHardware_t *uart1 = findUartHardware(SERIAL_PORT_USART1);
    const uartHardware_t *uart2 = findUartHardware(SERIAL_PORT_USART2);
    const uartHardware_t *uart5 = findUartHardware(SERIAL_PORT_UART5);
    ASSERT_NE(nullptr, uart1);
    ASSERT_NE(nullptr, uart2);
    ASSERT_NE(nullptr, uart5);

    EXPECT_TRUE(uartSupportsPin(uart1->txPins, IO_TAG(UART1_TX_PIN)));
    EXPECT_TRUE(uartSupportsPin(uart1->rxPins, IO_TAG(UART1_RX_PIN)));
    EXPECT_TRUE(uartSupportsPin(uart2->txPins, IO_TAG(UART2_TX_PIN)));
    EXPECT_TRUE(uartSupportsPin(uart2->rxPins, IO_TAG(UART2_RX_PIN)));
    EXPECT_TRUE(uartSupportsPin(uart5->txPins, IO_TAG(UART5_TX_PIN)));
    EXPECT_TRUE(uartSupportsPin(uart5->rxPins, IO_TAG(UART5_RX_PIN)));

    const dmaChannelSpec_t *uart5Rx = dmaGetChannelSpecByPeripheral(
        DMA_PERIPH_UART_RX, UARTDEV_5, 0);
    const dmaChannelSpec_t *uart5Tx = dmaGetChannelSpecByPeripheral(
        DMA_PERIPH_UART_TX, UARTDEV_5, 0);
    expectTuple(uart5Rx, 1, 0, 4);
    expectTuple(uart5Tx, 1, 7, 4);
    ASSERT_NE(nullptr, uart5Rx);
    ASSERT_NE(nullptr, uart5Tx);
    EXPECT_NE(2U, DMA_CODE_CONTROLLER(uart5Rx->code));
    EXPECT_NE(2U, DMA_CODE_CONTROLLER(uart5Tx->code));
}

#endif // BOARD_C_REQMAP_VARIANT

} // namespace
