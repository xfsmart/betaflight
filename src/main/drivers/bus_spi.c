/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#include "build/atomic.h"

#ifdef USE_SPI

#include "drivers/bus.h"
#include "drivers/bus_spi.h"
#include "drivers/bus_spi_impl.h"
#include "drivers/dma_reqmap.h"
#include "drivers/exti.h"
#include "drivers/io.h"
#include "drivers/motor.h"
#include "drivers/nvic.h"
#include "drivers/time.h"
#include "drivers/system.h"
#include "pg/bus_spi.h"

static uint8_t spiRegisteredDeviceCount = 0;

spiDevice_t spiDevice[SPIDEV_COUNT];
busDevice_t spiBusDevice[SPIDEV_COUNT];

#define SPI_RB_MAX_POLLED_LENGTH 7

typedef enum {
    SPI_SUBMIT_INVALID,
    SPI_SUBMIT_QUARANTINED,
    SPI_SUBMIT_BUSY,
    SPI_SUBMIT_QUEUED,
    SPI_SUBMIT_START,
} spiSubmitResult_e;

static bool spiTransferBlockingIfIdle(const extDevice_t *dev, busSegment_t *segments);

static spiDevice_t *spiDeviceState(const busDevice_t *bus)
{
    const spiDevice_e device = spiDeviceByInstance(bus->busType_u.spi.instance);

    return device == SPIINVALID ? NULL : &spiDevice[device];
}

static bool spiReadErrorCount(const extDevice_t *dev, uint16_t *errorCount)
{
    const spiDevice_t *state = spiDeviceState(dev->bus);

    if (!state
#ifdef FT32F4
        || state->polledRecoveryRequired
#endif
    ) {
        return false;
    }

    *errorCount = state->errorCount;
    return true;
}

static busSegment_t *spiSegmentListEnd(busSegment_t *segments)
{
    for (uint32_t count = 0; segments && segments->len > 0; count++) {
        if (count >= SPI_MAX_SEGMENTS_PER_LIST) {
            return NULL;
        }
        segments++;
    }

    return segments;
}

static bool spiSegmentListNext(busSegment_t *segments, busSegment_t **nextSegments)
{
    if (!segments || segments->len <= 0) {
        return false;
    }

    busSegment_t *endSegment = spiSegmentListEnd(segments);
    if (!endSegment || endSegment->len < 0) {
        return false;
    }
    const bool hasNextDev = endSegment->u.link.dev != NULL;
    const bool hasNextSegments = endSegment->u.link.segments != NULL;

    if (hasNextDev != hasNextSegments) {
        return false;
    }

    *nextSegments = hasNextSegments ? (busSegment_t *)endSegment->u.link.segments : NULL;
    return true;
}

static bool spiSegmentQueueAdvance(busSegment_t **segments)
{
    if (!*segments) {
        return true;
    }

    return spiSegmentListNext(*segments, segments);
}

static bool spiSegmentQueueIsValid(busSegment_t *segments)
{
    busSegment_t *slow = segments;
    busSegment_t *fast = segments;

    for (uint32_t count = 0; fast && count < SPI_MAX_QUEUE_LISTS; count++) {
        if (!spiSegmentQueueAdvance(&slow) || !spiSegmentQueueAdvance(&fast)) {
            return false;
        }
        if (fast && !spiSegmentQueueAdvance(&fast)) {
            return false;
        }
        if (fast && slow == fast) {
            return false;
        }
    }

    return fast == NULL;
}

static busSegment_t *spiSegmentQueueTail(busSegment_t *segments);

static bool spiSegmentQueuesOverlap(busSegment_t *first, busSegment_t *second)
{
    return spiSegmentQueueTail(first) == spiSegmentQueueTail(second);
}

static busSegment_t *spiSegmentQueueTail(busSegment_t *segments)
{
    busSegment_t *nextSegments;

    for (uint32_t count = 0; count < SPI_MAX_QUEUE_LISTS && spiSegmentListNext(segments, &nextSegments) && nextSegments; count++) {
        segments = nextSegments;
    }

    return spiSegmentListEnd(segments);
}

static busSegment_t *spiInvalidateFailedSegmentTail(volatile busSegment_t *segments)
{
    busSegment_t *segment = (busSegment_t *)segments;

    for (uint32_t count = 0; segment && segment->len > 0; count++) {
        if (count >= SPI_MAX_SEGMENTS_PER_LIST) {
            return NULL;
        }
        if (segment->u.buffers.rxData) {
            memset(segment->u.buffers.rxData, 0xff, segment->len);
        }
        segment++;
    }

    return segment;
}

static void spiCompleteFailedSegmentList(busDevice_t *bus, busSegment_t *endSegment, bool recovered)
{
    endSegment->u.link.dev = NULL;
    endSegment->u.link.segments = NULL;

    spiDevice_t *state = spiDeviceState(bus);
    if (state) {
        state->errorCount += recovered ? 1U : 2U;
#ifdef FT32F4
        state->polledRecoveryRequired = !recovered;
#endif
    }
    bus->curSegment = (busSegment_t *)BUS_SPI_FREE;
}

#ifdef FT32F4
static void spiInvalidateDmaFailureQueue(volatile busSegment_t *segments)
{
    while (segments) {
        busSegment_t *endSegment = spiInvalidateFailedSegmentTail(segments);
        if (!endSegment) {
            return;
        }
        volatile busSegment_t *nextSegments = endSegment->u.link.segments;

        endSegment->u.link.dev = NULL;
        endSegment->u.link.segments = NULL;
        segments = nextSegments;
    }
}

void spiHandleDmaFailure(const extDevice_t *dev, bool hardwareIsolated)
{
    if (!dev || !dev->bus) {
        return;
    }

    busDevice_t *bus = dev->bus;
    IOHi(dev->busType_u.spi.csnPin);

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    __DMB();

    volatile busSegment_t *failedSegments = bus->curSegment;
    spiDevice_t *state = spiDeviceState(bus);
    const bool releaseQueue = hardwareIsolated && state && failedSegments;

    if (state) {
        state->errorCount++;
        state->polledRecoveryRequired = true;
        state->dmaServicePending = hardwareIsolated;
    }

    if (releaseQueue) {
        if (bus->dmaTx) {
            bus->dmaTx->userParam = 0U;
        }
        if (bus->dmaRx) {
            bus->dmaRx->userParam = 0U;
        }
    }

    __DMB();
    __set_PRIMASK(primask);

    if (!releaseQueue || bus->curSegment != failedSegments) {
        return;
    }
}

static void spiInvalidateRejectedSegments(busSegment_t *segments)
{
    busSegment_t *endSegment = spiInvalidateFailedSegmentTail(segments);
    endSegment->u.link.dev = NULL;
    endSegment->u.link.segments = NULL;
}
#endif

spiDevice_e spiDeviceByInstance(const spiResource_t *instance)
{
#ifdef USE_SPI_DEVICE_0
    if (instance == (const spiResource_t *)SPI0) {
        return SPIDEV_0;
    }
#endif

#ifdef USE_SPI_DEVICE_1
    if (instance == (const spiResource_t *)SPI1) {
        return SPIDEV_1;
    }
#endif

#ifdef USE_SPI_DEVICE_2
    if (instance == (const spiResource_t *)SPI2) {
        return SPIDEV_2;
    }
#endif

#ifdef USE_SPI_DEVICE_3
    if (instance == (const spiResource_t *)SPI3) {
        return SPIDEV_3;
    }
#endif

#ifdef USE_SPI_DEVICE_4
    if (instance == (const spiResource_t *)SPI4) {
        return SPIDEV_4;
    }
#endif

#ifdef USE_SPI_DEVICE_5
    if (instance == (const spiResource_t *)SPI5) {
        return SPIDEV_5;
    }
#endif

#ifdef USE_SPI_DEVICE_6
    if (instance == (const spiResource_t *)SPI6) {
        return SPIDEV_6;
    }
#endif

    return SPIINVALID;
}

spiResource_t *spiInstanceByDevice(spiDevice_e device)
{
    if (device == SPIINVALID || device >= SPIDEV_COUNT) {
        return NULL;
    }

    return spiDevice[device].dev;
}

bool spiInit(spiDevice_e device)
{
    switch (device) {
    case SPIINVALID:

#if !defined(USE_SPI_DEVICE_1)
    case SPIDEV_1:
#endif

#if !defined(USE_SPI_DEVICE_2)
    case SPIDEV_2:
#endif

#if !defined(USE_SPI_DEVICE_3)
    case SPIDEV_3:
#endif

#if !defined(USE_SPI_DEVICE_4)
    case SPIDEV_4:
#endif

#if !defined(USE_SPI_DEVICE_5)
    case SPIDEV_5:
#endif

#if !defined(USE_SPI_DEVICE_6)
    case SPIDEV_6:
#endif
        return false;
    default:
        spiInitDevice(device);
        return true;
    }
}

// Return true if DMA engine is busy
bool spiIsBusy(const extDevice_t *dev)
{
    return (dev->bus->curSegment != (busSegment_t *)BUS_SPI_FREE);
}

// Wait for DMA completion
void spiWait(const extDevice_t *dev)
{
    // Wait for completion
    while (spiIsBusy(dev)) {
#ifdef FT32F4
        spiDmaService();
#endif
    }
}

void spiDmaService(void)
{
#ifdef FT32F4
    for (spiDevice_e device = SPIDEV_FIRST; device < SPIDEV_COUNT; device++) {
        spiDevice_t *state = &spiDevice[device];
        if (state->activeDev) {
            spiInternalServiceDMA(state->activeDev);
        }
        if (state->dmaServicePending) {
            busDevice_t *bus = state->activeDev ? state->activeDev->bus : NULL;
            if (bus && bus->curSegment != (busSegment_t *)BUS_SPI_FREE) {
                spiInvalidateDmaFailureQueue(bus->curSegment);
                const uint32_t primask = __get_PRIMASK();
                __disable_irq();
                __DMB();
                if (bus->curSegment != (busSegment_t *)BUS_SPI_FREE) {
                    bus->curSegment = (busSegment_t *)BUS_SPI_FREE;
                }
                state->activeDev = NULL;
                state->dmaServicePending = false;
                __DMB();
                __set_PRIMASK(primask);
            } else {
                state->dmaServicePending = false;
            }
        }
    }
#else
    UNUSED(spiDevice);
#endif
}

// Negate CS if held asserted after a transfer
void spiRelease(const extDevice_t *dev)
{
    // Negate Chip Select
    IOHi(dev->busType_u.spi.csnPin);
}

// Wait for bus to become free, then read/write block of data
void spiReadWriteBuf(const extDevice_t *dev, uint8_t *txData, uint8_t *rxData, int len)
{
    if (len <= 0) {
        return;
    }

    // This routine blocks so no need to use static data
    busSegment_t segments[] = {
            {.u.buffers = {txData, rxData}, len, true, NULL},
            {.u.link = {NULL, NULL}, 0, true, NULL},
    };

    spiSequence(dev, &segments[0]);

    spiWait(dev);
}

// Read/Write a block of data, returning false if the bus is busy
bool spiReadWriteBufRB(const extDevice_t *dev, uint8_t *txData, uint8_t *rxData, int length)
{
    if (length <= 0 || length > SPI_RB_MAX_POLLED_LENGTH) {
        return false;
    }

    busSegment_t segments[] = {
            {.u.buffers = {txData, rxData}, length, true, NULL},
            {.u.link = {NULL, NULL}, 0, true, NULL},
    };

    return spiTransferBlockingIfIdle(dev, &segments[0]);
}

// Wait for bus to become free, then read/write a single byte
uint8_t spiReadWrite(const extDevice_t *dev, uint8_t data)
{
    uint8_t retval;

    // This routine blocks so no need to use static data
    busSegment_t segments[] = {
            {.u.buffers = {&data, &retval}, sizeof(data), true, NULL},
            {.u.link = {NULL, NULL}, 0, true, NULL},
    };

    spiSequence(dev, &segments[0]);

    spiWait(dev);

    return retval;
}

// Wait for bus to become free, then read/write a single byte from a register
uint8_t spiReadWriteReg(const extDevice_t *dev, uint8_t reg, uint8_t data)
{
    uint8_t retval;

    // This routine blocks so no need to use static data
    busSegment_t segments[] = {
            {.u.buffers = {&reg, NULL}, sizeof(reg), false, NULL},
            {.u.buffers = {&data, &retval}, sizeof(data), true, NULL},
            {.u.link = {NULL, NULL}, 0, true, NULL},
    };

    spiSequence(dev, &segments[0]);

    spiWait(dev);

    return retval;
}

// Wait for bus to become free, then write a single byte
void spiWrite(const extDevice_t *dev, uint8_t data)
{
    // This routine blocks so no need to use static data
    busSegment_t segments[] = {
            {.u.buffers = {&data, NULL}, sizeof(data), true, NULL},
            {.u.link = {NULL, NULL}, 0, true, NULL},
    };

    spiSequence(dev, &segments[0]);

    spiWait(dev);
}

// Write data to a register
void spiWriteReg(const extDevice_t *dev, uint8_t reg, uint8_t data)
{
    // This routine blocks so no need to use static data
    busSegment_t segments[] = {
            {.u.buffers = {&reg, NULL}, sizeof(reg), false, NULL},
            {.u.buffers = {&data, NULL}, sizeof(data), true, NULL},
            {.u.link = {NULL, NULL}, 0, true, NULL},
    };

    spiSequence(dev, &segments[0]);

    spiWait(dev);
}

// Write data to a register, returning false if the bus is busy
bool spiWriteRegRB(const extDevice_t *dev, uint8_t reg, uint8_t data)
{
    // Ensure any prior DMA has completed before continuing
    if (spiIsBusy(dev)) {
        return false;
    }

    spiWriteReg(dev, reg, data);

    return true;
}

// Read a block of data from a register
void spiReadRegBuf(const extDevice_t *dev, uint8_t reg, uint8_t *data, uint8_t length)
{
    if (length == 0) {
        return;
    }

    // This routine blocks so no need to use static data
    busSegment_t segments[] = {
            {.u.buffers = {&reg, NULL}, sizeof(reg), false, NULL},
            {.u.buffers = {NULL, data}, length, true, NULL},
            {.u.link = {NULL, NULL}, 0, true, NULL},
    };

    spiSequence(dev, &segments[0]);

    spiWait(dev);
}

// Read a block of data from a register, returning false if the bus is busy
bool spiReadRegBufRB(const extDevice_t *dev, uint8_t reg, uint8_t *data, uint8_t length)
{
    // Ensure any prior DMA has completed before continuing
    if (spiIsBusy(dev)) {
        return false;
    }

    spiReadRegBuf(dev, reg, data, length);

    return true;
}

// Read a block of data where the register is ORed with 0x80, returning false if the bus is busy
bool spiReadRegMskBufRB(const extDevice_t *dev, uint8_t reg, uint8_t *data, uint8_t length)
{
    return spiReadRegBufRB(dev, reg | 0x80, data, length);
}

// Wait for bus to become free, then write a block of data to a register
void spiWriteRegBuf(const extDevice_t *dev, uint8_t reg, uint8_t *data, uint32_t length)
{
    if (length == 0 || length > INT32_MAX) {
        return;
    }

    // This routine blocks so no need to use static data
    busSegment_t segments[] = {
            {.u.buffers = {&reg, NULL}, sizeof(reg), false, NULL},
            {.u.buffers = {data, NULL}, length, true, NULL},
            {.u.link = {NULL, NULL}, 0, true, NULL},
    };

    spiSequence(dev, &segments[0]);

    spiWait(dev);
}

// Wait for bus to become free, then read a byte from a register
uint8_t spiReadReg(const extDevice_t *dev, uint8_t reg)
{
    uint8_t data;
    // This routine blocks so no need to use static data
    busSegment_t segments[] = {
            {.u.buffers = {&reg, NULL}, sizeof(reg), false, NULL},
            {.u.buffers = {NULL, &data}, sizeof(data), true, NULL},
            {.u.link = {NULL, NULL}, 0, true, NULL},
    };

    spiSequence(dev, &segments[0]);

    spiWait(dev);

    return data;
}

// Wait for bus to become free, then read a byte of data where the register is ORed with 0x80
uint8_t spiReadRegMsk(const extDevice_t *dev, uint8_t reg)
{
    return spiReadReg(dev, reg | 0x80);
}

// Mark this bus as being SPI and record the first owner to use it
bool spiSetBusInstance(extDevice_t *dev, uint32_t device)
{
    if ((device == 0) || (device > SPIDEV_COUNT)) {
        return false;
    }

    dev->bus = &spiBusDevice[SPI_CFG_TO_DEV(device)];

    // By default each device should use SPI DMA if the bus supports it
    dev->useDMA = true;

    if (dev->bus->busType == BUS_TYPE_SPI) {
        // This bus has already been initialised
        dev->bus->deviceCount++;
        return true;
    }

    busDevice_t *bus = dev->bus;

    bus->busType_u.spi.instance = spiInstanceByDevice(SPI_CFG_TO_DEV(device));

    if (bus->busType_u.spi.instance == NULL) {
        return false;
    }

    bus->busType = BUS_TYPE_SPI;
    bus->useDMA = false;
    bus->deviceCount = 1;
#ifdef USE_DMA
    bus->dmaInitTx = &dev->dmaInitTx;
    bus->dmaInitRx = &dev->dmaInitRx;
#endif

    return true;
}

void spiSetClkDivisor(const extDevice_t *dev, uint16_t divisor)
{
    ((extDevice_t *)dev)->busType_u.spi.speed = divisor;
}

// Set the clock phase/polarity to be used for accesses by the given device
void spiSetClkPhasePolarity(const extDevice_t *dev, bool leadingEdge)
{
    ((extDevice_t *)dev)->busType_u.spi.leadingEdge = leadingEdge;
}

#ifdef USE_DMA
// Enable/disable DMA on a specific device. Enabled by default.
void spiDmaEnable(const extDevice_t *dev, bool enable)
{
    ((extDevice_t *)dev)->useDMA = enable;
}

bool spiUseDMA(const extDevice_t *dev)
{
    // Full DMA only requires both transmit and receive}
    return dev->bus->useDMA && dev->bus->dmaRx && dev->useDMA;
}

bool spiUseSDO_DMA(const extDevice_t *dev)
{
    return dev->bus->useDMA && dev->useDMA;
}
#endif

void spiBusDeviceRegister(const extDevice_t *dev)
{
    UNUSED(dev);

    spiRegisteredDeviceCount++;
}

uint8_t spiGetRegisteredDeviceCount(void)
{
    return spiRegisteredDeviceCount;
}

uint8_t spiGetExtDeviceCount(const extDevice_t *dev)
{
    return dev->bus->deviceCount;
}

// Link two segment lists
// Note that there is no need to unlink segment lists as this is done automatically as they are processed
void spiLinkSegments(const extDevice_t *dev, busSegment_t *firstSegment, busSegment_t *secondSegment)
{
    if (!firstSegment || !secondSegment ||
        !spiSegmentQueueIsValid(firstSegment) || !spiSegmentQueueIsValid(secondSegment) ||
        spiSegmentQueuesOverlap(firstSegment, secondSegment)) {
        return;
    }

    busSegment_t *endSegment = spiSegmentQueueTail(firstSegment);
    endSegment->u.link.dev = dev;
    endSegment->u.link.segments = secondSegment;
}

static spiSubmitResult_e spiSubmitLocked(const extDevice_t *dev, busSegment_t *segments, bool allowQueue, uint16_t *errorSnapshot)
{
    busDevice_t *bus = dev->bus;
    spiDevice_t *state = spiDeviceState(bus);

    if (!state) {
        return SPI_SUBMIT_INVALID;
    }

#ifdef FT32F4
    if (state->polledRecoveryRequired) {
        state->errorCount++;
        return SPI_SUBMIT_QUARANTINED;
    }
#endif

    if (!spiIsBusy(dev)) {
        if (errorSnapshot) {
            *errorSnapshot = state->errorCount;
        }
        bus->curSegment = segments;
#ifdef FT32F4
        state->activeDev = dev;
        state->dmaGeneration++;
        state->dmaLastProgress = 0U;
        state->dmaLastProgressCycles = getCycleCounter();
#endif
        return SPI_SUBMIT_START;
    }

    if (!allowQueue) {
        return SPI_SUBMIT_BUSY;
    }

    busSegment_t *queuedSegments = (busSegment_t *)bus->curSegment;
    if (spiSegmentQueuesOverlap(queuedSegments, segments)) {
        return SPI_SUBMIT_INVALID;
    }

    busSegment_t *endSegment = spiSegmentQueueTail(queuedSegments);
    endSegment->u.link.dev = dev;
    endSegment->u.link.segments = segments;
    return SPI_SUBMIT_QUEUED;
}

static spiSubmitResult_e spiSubmitSegments(const extDevice_t *dev, busSegment_t *segments, bool allowQueue, uint16_t *errorSnapshot)
{
    if (!segments || !spiSegmentQueueIsValid(segments)) {
        return SPI_SUBMIT_INVALID;
    }

    spiSubmitResult_e result;
#ifdef FT32F4
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    __DMB();
    result = spiSubmitLocked(dev, segments, allowQueue, errorSnapshot);
    __DMB();
    __set_PRIMASK(primask);

    if (result == SPI_SUBMIT_QUARANTINED) {
        spiInvalidateRejectedSegments(segments);
    }
#else
    ATOMIC_BLOCK(NVIC_PRIO_MAX) {
        result = spiSubmitLocked(dev, segments, allowQueue, errorSnapshot);
    }
#endif

    if (result == SPI_SUBMIT_START) {
        spiSequenceStart(dev);
    }

    return result;
}

static bool spiTransferBlockingIfIdle(const extDevice_t *dev, busSegment_t *segments)
{
    uint16_t errorCount;
    if (spiSubmitSegments(dev, segments, false, &errorCount) != SPI_SUBMIT_START) {
        return false;
    }

    spiWait(dev);

    uint16_t completedErrorCount;
    return spiReadErrorCount(dev, &completedErrorCount) && completedErrorCount == errorCount;
}

static const extDevice_t *spiFinishSegmentListLocked(busDevice_t *bus, busSegment_t *endSegment)
{
    const extDevice_t *nextDev = endSegment->u.link.dev;
    busSegment_t *nextSegments = (busSegment_t *)endSegment->u.link.segments;

    if ((nextDev == NULL) != (nextSegments == NULL)) {
        nextDev = NULL;
        nextSegments = NULL;
    }

    bus->curSegment = nextDev ? nextSegments : (busSegment_t *)BUS_SPI_FREE;
#ifdef FT32F4
    spiDevice_t *state = spiDeviceState(bus);
    if (state) {
        state->activeDev = nextDev;
        if (nextDev) {
            state->dmaGeneration++;
            state->dmaLastProgress = 0U;
            state->dmaLastProgressCycles = getCycleCounter();
        }
    }
#endif
    endSegment->u.link.dev = NULL;
    endSegment->u.link.segments = NULL;

    return nextDev;
}

static const extDevice_t *spiFinishSegmentList(busDevice_t *bus, busSegment_t *endSegment)
{
    const extDevice_t *nextDev;
#ifdef FT32F4
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    __DMB();
    nextDev = spiFinishSegmentListLocked(bus, endSegment);
    __DMB();
    __set_PRIMASK(primask);
#else
    ATOMIC_BLOCK(NVIC_PRIO_MAX) {
        nextDev = spiFinishSegmentListLocked(bus, endSegment);
    }
#endif

    return nextDev;
}

// DMA transfer setup and start
void spiSequence(const extDevice_t *dev, busSegment_t *segments)
{
    (void)spiSubmitSegments(dev, segments, true, NULL);
}

// Process segments using DMA - expects DMA irq handler to have been set up to feed into spiIrqHandler.
FAST_CODE void spiProcessSegmentsDMA(const extDevice_t *dev)
{
    // Intialise the init structures for the first transfer
    spiInternalInitStream(dev, dev->bus->curSegment);

    // Assert Chip Select
    IOLo(dev->busType_u.spi.csnPin);

    // Start the transfers
#ifdef FT32F4
    bool hardwareIsolated = false;
    if (!spiInternalStartDMA(dev, &hardwareIsolated)) {
        spiHandleDmaFailure(dev, hardwareIsolated);
    }
#else
    spiInternalStartDMA(dev);
#endif
}

static void spiPreInitStream(const extDevice_t *dev)
{
    // Prepare the init structure for the next segment to reduce inter-segment interval
    // (if it's a "buffers" segment, not a "link" segment).
    busSegment_t *nextSegment = (busSegment_t *)dev->bus->curSegment + 1;
    if (nextSegment->len > 0) {
        spiInternalInitStream(dev, nextSegment);
    }
}

// Interrupt handler common code for SPI receive DMA completion.
// Proceed to next segment as required.
FAST_IRQ_HANDLER void spiIrqHandler(const extDevice_t *dev)
{
    busDevice_t *bus = dev->bus;
    busSegment_t *nextSegment;
    bool repeatSegment = false;

    if (bus->curSegment->callback) {
        switch(bus->curSegment->callback(dev->callbackArg)) {
        case BUS_BUSY:
            // Repeat the completed DMA segment without moving before the list start.
            repeatSegment = true;
            break;

        case BUS_ABORT:
            // Release chip select and skip to the end of the segment list.
            IOHi(dev->busType_u.spi.csnPin);
            nextSegment = (busSegment_t *)bus->curSegment + 1;
            while (nextSegment->len != 0) {
                bus->curSegment = nextSegment;
                nextSegment = (busSegment_t *)bus->curSegment + 1;
            }
            break;

        case BUS_READY:
        default:
            // Advance to the next DMA segment
            break;
        }
    }

    // Advance through the segment list
    // OK to discard the volatile qualifier here
    nextSegment = repeatSegment ?
        (busSegment_t *)bus->curSegment :
        (busSegment_t *)bus->curSegment + 1;

    if (nextSegment->len == 0) {
        const extDevice_t *nextDev = spiFinishSegmentList(bus, nextSegment);
        if (nextDev) {
            spiSequenceStart(nextDev);
        }
    } else {
        // Do as much processing as possible before asserting CS to avoid violating minimum high time
        bool negateCS = bus->curSegment->negateCS;

        bus->curSegment = nextSegment;

        // A repeated segment must replace the cached descriptor for its successor.
        if (repeatSegment || bus->initSegment) {
            spiInternalInitStream(dev, bus->curSegment);
            bus->initSegment = false;
        }

        if (negateCS) {
            // Assert Chip Select - it's costly so only do so if necessary
            IOLo(dev->busType_u.spi.csnPin);
        }

        // Launch the next transfer
#ifdef FT32F4
        bool hardwareIsolated = false;
        if (!spiInternalStartDMA(dev, &hardwareIsolated)) {
            spiHandleDmaFailure(dev, hardwareIsolated);
            return;
        }
#else
        spiInternalStartDMA(dev);
#endif

        // Prepare the init structures ready for the next segment to reduce inter-segment time
        spiPreInitStream(dev);
    }
}

FAST_CODE void spiProcessSegmentsPolled(const extDevice_t *dev)
{
    busDevice_t *bus = dev->bus;
    busSegment_t *lastSegment = NULL;
    bool segmentComplete;

    // Manually work through the segment list performing a transfer for each
    while (bus->curSegment->len) {
        if (!lastSegment || lastSegment->negateCS) {
            // Assert Chip Select if necessary - it's costly so only do so if necessary
            IOLo(dev->busType_u.spi.csnPin);
        }

        const bool transferComplete = spiInternalReadWriteBufPolled(
            bus->busType_u.spi.instance,
            bus->curSegment->u.buffers.txData,
            bus->curSegment->u.buffers.rxData,
            bus->curSegment->len);

        if (!transferComplete) {
            IOHi(dev->busType_u.spi.csnPin);
#ifdef FT32F4
            const bool recovered = spiInternalRecoverPolled(dev);

            busSegment_t *endSegment = spiInvalidateFailedSegmentTail(bus->curSegment);
            const uint32_t primask = __get_PRIMASK();
            __disable_irq();
            __DMB();
            spiCompleteFailedSegmentList(bus, endSegment, recovered);
            __DMB();
            __set_PRIMASK(primask);
#else
            busSegment_t *endSegment = spiInvalidateFailedSegmentTail(bus->curSegment);
            ATOMIC_BLOCK(NVIC_PRIO_MAX) {
                spiCompleteFailedSegmentList(bus, endSegment, true);
            }
#endif
            return;
        }

        if (bus->curSegment->negateCS) {
            // Negate Chip Select
            IOHi(dev->busType_u.spi.csnPin);
        }

        segmentComplete = true;
        if (bus->curSegment->callback) {
            switch(bus->curSegment->callback(dev->callbackArg)) {
            case BUS_BUSY:
                // Repeat with the chip-select state produced by this segment.
                lastSegment = (busSegment_t *)bus->curSegment;
                segmentComplete = false;
                break;

            case BUS_ABORT:
                // Skip this list but preserve a linked successor transaction.
                IOHi(dev->busType_u.spi.csnPin);
                while ((bus->curSegment + 1)->len != 0) {
                    bus->curSegment++;
                }
                break;

            case BUS_READY:
            default:
                // Advance to the next DMA segment
                break;
            }
        }
        if (segmentComplete) {
            lastSegment = (busSegment_t *)bus->curSegment;
            bus->curSegment++;
        }
    }

    busSegment_t *endSegment = (busSegment_t *)bus->curSegment;
    const extDevice_t *nextDev = spiFinishSegmentList(bus, endSegment);
    if (nextDev) {
        spiSequenceStart(nextDev);
    }
}

#endif
