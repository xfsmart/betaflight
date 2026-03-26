/*
 * FT32F4 driver stubs - placeholder implementations
 * These allow compilation to complete without full driver implementations
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"
#include "drivers/serial.h"
#include "drivers/pwm_output.h"
#include "drivers/accgyro/accgyro_mpu.h"
#include "drivers/timer.h"
#include "drivers/dma.h"
#include "drivers/dma_reqmap.h"

/* UART stubs */
void uartReconfigure(serialPort_t *instance)
{
    (void)instance;
    // FT32F4: Not yet implemented
}

/* MPU stubs */
bool isMPUSoftReset(void)
{
    return false;
}

/* DSHOT telemetry - this is a global variable */
bool useDshotTelemetry = false;

/* WS2811 LED strip stub */
void ws2811LedStripInit(uint8_t ioTag, uint8_t ledFormat)
{
    (void)ioTag;
    (void)ledFormat;
    // FT32F4: Not yet implemented
}

/* Persistent object stub */
void persistentObjectInit(void)
{
    // FT32F4: Not yet implemented
}

/* DSHOT bitbang stub */
bool isDshotBitbangActive(const void *motorDevConfig)
{
    (void)motorDevConfig;
    return false;
}

/* Serial UART stub */
void *serialUART(void *uart, uint32_t baudRate, uint8_t mode, uint8_t options)
{
    (void)uart;
    (void)baudRate;
    (void)mode;
    (void)options;
    return NULL;
}

/* Serial pin config PG variable */
#include "pg/pg.h"
#include "drivers/serial.h"
PG_REGISTER(serialPinConfig_t, serialPinConfig, PG_SERIAL_PIN_CONFIG, 0);

/* PWM beeper stub - correct signature */
void pwmWriteBeeper(bool on)
{
    (void)on;
    // FT32F4: Not yet implemented
}

/* WS2811 LED strip stubs */
void ws2811LedStripStartTransfer(void)
{
    // FT32F4: Not yet implemented
}

void ws2811LedStripUpdateTransferBuffer(const void *color, unsigned ledIndex)
{
    (void)color;
    (void)ledIndex;
    // FT32F4: Not yet implemented
}

void ws2811LedStripHardwareInit(void)
{
    // FT32F4: Not yet implemented
}

/* Servo stub - correct signature */
void servoWrite(uint8_t servoIndex, float value)
{
    (void)servoIndex;
    (void)value;
    // FT32F4: Not yet implemented
}

/* DSHOT bitbang timer stubs - correct signature */
const void *dshotBitbangTimerGetAllocatedByNumberAndChannel(int8_t timerNumber, uint16_t timerChannel)
{
    (void)timerNumber;
    (void)timerChannel;
    return NULL;
}

void *dshotBitbangTimerGetOwner(void *timer)
{
    (void)timer;
    return NULL;
}

/* Transponder stub - correct signature */
void transponderIrUpdateData(const uint8_t *data)
{
    (void)data;
    // FT32F4: Not yet implemented
}

// adcGetValue is now implemented in adc_impl.c
// uint16_t adcGetValue(uint8_t adcChannel) { (void)adcChannel; return 0; }

/* USB stub */
bool usbCableIsInserted(void)
{
    return false;
}

/* Timer stubs */
// timerGetTIMNumber is defined in timer_ft32f4xx.c - stub removed

// fullTimerHardware is defined in timer_ft32f4xx.c

/* Transponder stubs */
void transponderIrDisable(void)
{
    // FT32F4: Not yet implemented
}

void transponderIrTransmit(void)
{
    // FT32F4: Not yet implemented
}

/* USB VCP stubs */
void *usbVcpOpen(void)
{
    return NULL;
}

bool usbVcpIsConnected(void)
{
    return false;
}

/* DMA stubs removed — real implementations in dma_reqmap_mcu.c */
