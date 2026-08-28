#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ft32_uart_dma_recovery_test_api.h"
#include "build/atomic.h"

#define MODE_TX (1U << 0)
#define SERIAL_CHECK_TX (1U << 0)
#define NVIC_PRIO_SERIALUART_TXDMA 64U
#define __DMB() __asm__ volatile ("" ::: "memory")
#define container_of(ptr, type, member) ((type *)((uint8_t *)(ptr) - offsetof(type, member)))

typedef enum {
    DISABLE = 0,
    ENABLE = 1,
} FunctionalState;

typedef enum {
    TX_PIN_ACTIVE,
    TX_PIN_MONITOR,
    TX_PIN_IGNORE,
} txPinState_t;

typedef struct {
    bool placeholder;
} USART_TypeDef;

typedef struct {
    bool placeholder;
} DMA_Channel_TypeDef;

typedef DMA_Channel_TypeDef DMA_ARCH_TYPE;

typedef struct uartPort_s uartPort_t;

typedef struct {
    uint8_t *txBuffer;
    uint16_t txBufferSize;
    volatile uint16_t txBufferHead;
    volatile uint16_t txBufferTail;
    uint32_t mode;
    uint32_t options;
} ft32UartDmaTestPort_t;

struct uartPort_s {
    ft32UartDmaTestPort_t port;
    void *USARTx;
    void *txDMAResource;
    bool txDMAEmpty;
    volatile bool txDMARecoveryPending;
    bool (*checkUsartTxOutput)(uartPort_t *s);
};

typedef struct {
    uartPort_t port;
    volatile txPinState_t txPinState;
} uartDevice_t;

void uartTryStartTxDMA(uartPort_t *s);

#define FT32_UART_DMA_WRITE_HELPERS
#include "../../platform/FT32/serial_uart_dma_recovery_impl.h"

#define FT32_UART_DMA_TEST_BUFFER_SIZE 16U
#define FT32_UART_DMA_TEST_MAX_EVENTS 128U

uint8_t atomic_BASEPRI;

static uartDevice_t testDevice;
static USART_TypeDef testUsart;
static DMA_Channel_TypeDef testDma;
static uint8_t testBuffer[FT32_UART_DMA_TEST_BUFFER_SIZE];
static ft32UartDmaTestEvent_e testEvents[FT32_UART_DMA_TEST_MAX_EVENTS];
static uint32_t testEventCount;
static bool testLineHigh;
static bool testExceptionContext;
static bool testChannelEnabled;
static bool testEnableAccepted;
static bool testStopSticky;
static bool testProducerEnabled;
static bool testTransmitterEnabled;
static uint32_t testProgrammedCount;
static uint32_t testMonitorTransitions;
static uint8_t testBasepriAtMonitor;

static void ft32UartDmaTestRecord(ft32UartDmaTestEvent_e event)
{
    if (testEventCount < FT32_UART_DMA_TEST_MAX_EVENTS) {
        testEvents[testEventCount++] = event;
    }
}

static uint32_t __get_IPSR(void)
{
    return testExceptionContext ? 1U : 0U;
}

static void ft32UartResetAndEnableTx(USART_TypeDef *USARTx)
{
    (void)USARTx;
    ft32UartDmaTestRecord(FT32_UART_DMA_TEST_EVENT_TX_RESET);
    testTransmitterEnabled = true;
    ft32UartDmaTestRecord(FT32_UART_DMA_TEST_EVENT_TX_ENABLE);
}

static void ft32UartDMATxEnable_Cmd(USART_TypeDef *USARTx, FunctionalState state)
{
    (void)USARTx;
    testProducerEnabled = state == ENABLE;
    ft32UartDmaTestRecord(state == ENABLE
        ? FT32_UART_DMA_TEST_EVENT_DMAT_ON
        : FT32_UART_DMA_TEST_EVENT_DMAT_OFF);
}

static bool ft32DmaTrySetCurrDataCounter(DMA_ARCH_TYPE *resource, uint32_t count)
{
    (void)resource;
    ft32UartDmaTestRecord(FT32_UART_DMA_TEST_EVENT_TRY_SET_COUNT);
    if (testStopSticky) {
        testChannelEnabled = true;
        return false;
    }

    testChannelEnabled = false;
    testProgrammedCount = count;
    return true;
}

static bool ft32DmaIsChannelEnabled(const DMA_ARCH_TYPE *resource)
{
    (void)resource;
    ft32UartDmaTestRecord(FT32_UART_DMA_TEST_EVENT_CHEN_READ);
    return testChannelEnabled;
}

static void DMA_SetSrcAddress(DMA_Channel_TypeDef *resource, uint32_t address)
{
    (void)resource;
    (void)address;
    ft32UartDmaTestRecord(FT32_UART_DMA_TEST_EVENT_SET_SOURCE);
}

static void xDMA_Cmd(void *resource, FunctionalState state)
{
    (void)resource;
    if (state == ENABLE) {
        ft32UartDmaTestRecord(FT32_UART_DMA_TEST_EVENT_DMA_ENABLE);
        testChannelEnabled = testEnableAccepted;
    } else {
        testChannelEnabled = testStopSticky;
    }
}

static bool ft32UartDmaTestCheckOutput(uartPort_t *s)
{
    uartDevice_t *uartDevice = container_of(s, uartDevice_t, port);
    ft32UartDmaTestRecord(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE);

    if (uartDevice->txPinState != TX_PIN_MONITOR) {
        return true;
    }
    if (!testLineHigh) {
        return false;
    }

    uartDevice->txPinState = TX_PIN_ACTIVE;
    testTransmitterEnabled = true;
    ft32UartDmaTestRecord(FT32_UART_DMA_TEST_EVENT_TX_ENABLE);
    return true;
}

static void uartTxMonitor(uartPort_t *s)
{
    uartDevice_t *uartDevice = container_of(s, uartDevice_t, port);

    if (uartDevice->txPinState == TX_PIN_ACTIVE) {
        testBasepriAtMonitor = atomic_BASEPRI;
        testTransmitterEnabled = false;
        ft32UartDmaTestRecord(FT32_UART_DMA_TEST_EVENT_TX_DISABLE);
        uartDevice->txPinState = TX_PIN_MONITOR;
        testMonitorTransitions++;
        ft32UartDmaTestRecord(FT32_UART_DMA_TEST_EVENT_MONITOR);
    }
}

#include "../../platform/FT32/serial_uart_stdperiph.c"

void ft32UartDmaTestReset(void)
{
    memset(&testDevice, 0, sizeof(testDevice));
    memset(testBuffer, 0, sizeof(testBuffer));
    memset(testEvents, 0, sizeof(testEvents));
    testDevice.port.port.txBuffer = testBuffer;
    testDevice.port.port.txBufferSize = FT32_UART_DMA_TEST_BUFFER_SIZE;
    testDevice.port.port.mode = MODE_TX;
    testDevice.port.port.options = SERIAL_CHECK_TX;
    testDevice.port.USARTx = &testUsart;
    testDevice.port.txDMAResource = &testDma;
    testDevice.port.txDMAEmpty = false;
    testDevice.port.txDMARecoveryPending = true;
    testDevice.port.checkUsartTxOutput = ft32UartDmaTestCheckOutput;
    testDevice.txPinState = TX_PIN_ACTIVE;
    testEventCount = 0U;
    testLineHigh = true;
    testExceptionContext = false;
    testChannelEnabled = false;
    testEnableAccepted = true;
    testStopSticky = false;
    testProducerEnabled = false;
    testTransmitterEnabled = true;
    testProgrammedCount = 0U;
    testMonitorTransitions = 0U;
    testBasepriAtMonitor = 0U;
    atomic_BASEPRI = 0U;
}

void ft32UartDmaTestClearEvents(void)
{
    memset(testEvents, 0, sizeof(testEvents));
    testEventCount = 0U;
}

void ft32UartDmaTestSetModeTx(bool enabled)
{
    if (enabled) {
        testDevice.port.port.mode |= MODE_TX;
    } else {
        testDevice.port.port.mode &= ~MODE_TX;
    }
}

void ft32UartDmaTestSetCheckedTx(bool enabled)
{
    if (enabled) {
        testDevice.port.port.options |= SERIAL_CHECK_TX;
    } else {
        testDevice.port.port.options &= ~SERIAL_CHECK_TX;
    }
}

void ft32UartDmaTestSetPinState(ft32UartDmaTestPinState_e state)
{
    testDevice.txPinState = (txPinState_t)state;
}

void ft32UartDmaTestSetLineHigh(bool high)
{
    testLineHigh = high;
}

void ft32UartDmaTestSetExceptionContext(bool exceptionContext)
{
    testExceptionContext = exceptionContext;
}

void ft32UartDmaTestSetRing(uint16_t head, uint16_t tail)
{
    testDevice.port.port.txBufferHead = head;
    testDevice.port.port.txBufferTail = tail;
}

void ft32UartDmaTestSetPending(bool pending)
{
    testDevice.port.txDMARecoveryPending = pending;
}

void ft32UartDmaTestSetEmpty(bool empty)
{
    testDevice.port.txDMAEmpty = empty;
}

void ft32UartDmaTestSetChannelEnabled(bool enabled)
{
    testChannelEnabled = enabled;
}

void ft32UartDmaTestSetEnableAccepted(bool accepted)
{
    testEnableAccepted = accepted;
}

void ft32UartDmaTestSetStopSticky(bool sticky)
{
    testStopSticky = sticky;
}

void ft32UartDmaTestSetTransmitterEnabled(bool enabled)
{
    testTransmitterEnabled = enabled;
}

void ft32UartDmaTestSetBasepri(uint8_t basepri)
{
    atomic_BASEPRI = basepri;
}

void ft32UartDmaTestApplyPeripheralResetState(void)
{
    testTransmitterEnabled = false;
    ft32UartRestoreTxStateAfterReset(&testDevice.port);
}

void ft32UartDmaTestPauseCheckedTxForMode(void)
{
    ft32UartPauseCheckedTxForMode(&testDevice.port);
}

void ft32UartDmaTestRecover(void)
{
    uartTryRecoverTxDMA(&testDevice.port);
}

void ft32UartDmaTestRecoverNested(uint8_t outerBasepri, uint8_t innerBasepri)
{
    ATOMIC_BLOCK(outerBasepri) {
        ATOMIC_BLOCK(innerBasepri) {
            uartTryRecoverTxDMA(&testDevice.port);
        }
    }
}

void ft32UartDmaTestWriterStart(void)
{
    uartTryStartTxDMA(&testDevice.port);
}

uint32_t ft32UartDmaTestPrepareWriteBuffer(void)
{
    return ft32UartDmaWriteBufferReady(&testDevice.port);
}

void ft32UartDmaTestFinishWriteBuffer(void)
{
    ft32UartDmaFinishWriteBuffer(&testDevice.port);
}

uint32_t ft32UartDmaTestPending(void)
{
    return testDevice.port.txDMARecoveryPending;
}

uint32_t ft32UartDmaTestEmpty(void)
{
    return testDevice.port.txDMAEmpty;
}

uint32_t ft32UartDmaTestChannelEnabled(void)
{
    return testChannelEnabled;
}

uint32_t ft32UartDmaTestProducerEnabled(void)
{
    return testProducerEnabled;
}

uint32_t ft32UartDmaTestTransmitterEnabled(void)
{
    return testTransmitterEnabled;
}

uint16_t ft32UartDmaTestHead(void)
{
    return testDevice.port.port.txBufferHead;
}

uint16_t ft32UartDmaTestTail(void)
{
    return testDevice.port.port.txBufferTail;
}

ft32UartDmaTestPinState_e ft32UartDmaTestPinState(void)
{
    return (ft32UartDmaTestPinState_e)testDevice.txPinState;
}

uint32_t ft32UartDmaTestProgrammedCount(void)
{
    return testProgrammedCount;
}

uint32_t ft32UartDmaTestMonitorTransitions(void)
{
    return testMonitorTransitions;
}

uint8_t ft32UartDmaTestBasepriAtMonitor(void)
{
    return testBasepriAtMonitor;
}

uint8_t ft32UartDmaTestBasepri(void)
{
    return atomic_BASEPRI;
}

uint32_t ft32UartDmaTestEventCount(ft32UartDmaTestEvent_e event)
{
    uint32_t count = 0U;
    for (uint32_t index = 0U; index < testEventCount; index++) {
        if (testEvents[index] == event) {
            count++;
        }
    }
    return count;
}

int32_t ft32UartDmaTestEventIndex(ft32UartDmaTestEvent_e event, uint32_t occurrence)
{
    uint32_t seen = 0U;
    for (uint32_t index = 0U; index < testEventCount; index++) {
        if (testEvents[index] == event && seen++ == occurrence) {
            return (int32_t)index;
        }
    }
    return -1;
}
