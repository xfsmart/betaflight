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
#include "drivers/motor.h"
#include "drivers/dshot.h"
#include "drivers/timer.h"
#include "drivers/resource.h"

/* DSHOT telemetry - global variable */
bool useDshotTelemetry = false;

/* DSHOT DMA handler cycle counters - for CLI debugging */
FAST_DATA_ZERO_INIT dshotTelemetryCycleCounters_t dshotDMAHandlerCycleCounters;

/* Serial pin config is now in drivers/serial_pinconfig.c */

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
#include "drivers/transponder_ir.h"
bool transponderIrInit(const ioTag_t ioTag, const transponderProvider_e provider)
{
    (void)ioTag;
    (void)provider;
    return false;
}

void transponderIrDisable(void)
{
    // FT32F4: Not yet implemented
}

void transponderIrTransmit(void)
{
}

bool transponderIrIsReady(void)
{
    return false;
}

/* USB VCP stubs */
void usbCableDetectInit(void)
{
    // FT32F4: Not yet implemented
}

void *usbVcpOpen(void)
{
    return NULL;
}

bool usbVcpIsConnected(void)
{
    return false;
}

void usbVcpInit(void)
{
}

/* DSHOT PWM stub - requires DMA and timer support */
bool dshotPwmDevInit(motorDevice_t *device, const motorDevConfig_t *motorConfig)
{
    (void)device;
    (void)motorConfig;
    return false;
}

/* DMA stubs removed — real implementations in dma_reqmap_mcu.c */
