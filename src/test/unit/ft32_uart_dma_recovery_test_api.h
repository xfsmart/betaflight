#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FT32_UART_DMA_TEST_PIN_ACTIVE,
    FT32_UART_DMA_TEST_PIN_MONITOR,
    FT32_UART_DMA_TEST_PIN_IGNORE,
} ft32UartDmaTestPinState_e;

typedef enum {
    FT32_UART_DMA_TEST_EVENT_DMAT_OFF = 1,
    FT32_UART_DMA_TEST_EVENT_TRY_SET_COUNT,
    FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE,
    FT32_UART_DMA_TEST_EVENT_TX_RESET,
    FT32_UART_DMA_TEST_EVENT_TX_ENABLE,
    FT32_UART_DMA_TEST_EVENT_SET_SOURCE,
    FT32_UART_DMA_TEST_EVENT_DMA_ENABLE,
    FT32_UART_DMA_TEST_EVENT_CHEN_READ,
    FT32_UART_DMA_TEST_EVENT_DMAT_ON,
    FT32_UART_DMA_TEST_EVENT_TX_DISABLE,
    FT32_UART_DMA_TEST_EVENT_MONITOR,
} ft32UartDmaTestEvent_e;

void ft32UartDmaTestReset(void);
void ft32UartDmaTestClearEvents(void);
void ft32UartDmaTestSetModeTx(bool enabled);
void ft32UartDmaTestSetCheckedTx(bool enabled);
void ft32UartDmaTestSetPinState(ft32UartDmaTestPinState_e state);
void ft32UartDmaTestSetLineHigh(bool high);
void ft32UartDmaTestSetExceptionContext(bool exceptionContext);
void ft32UartDmaTestSetRing(uint16_t head, uint16_t tail);
void ft32UartDmaTestSetPending(bool pending);
void ft32UartDmaTestSetEmpty(bool empty);
void ft32UartDmaTestSetChannelEnabled(bool enabled);
void ft32UartDmaTestSetEnableAccepted(bool accepted);
void ft32UartDmaTestSetStopSticky(bool sticky);
void ft32UartDmaTestSetTransmitterEnabled(bool enabled);
void ft32UartDmaTestSetBasepri(uint8_t basepri);

void ft32UartDmaTestApplyPeripheralResetState(void);
void ft32UartDmaTestPauseCheckedTxForMode(void);
void ft32UartDmaTestRecover(void);
void ft32UartDmaTestRecoverNested(uint8_t outerBasepri, uint8_t innerBasepri);
void ft32UartDmaTestWriterStart(void);
uint32_t ft32UartDmaTestPrepareWriteBuffer(void);
void ft32UartDmaTestFinishWriteBuffer(void);

uint32_t ft32UartDmaTestPending(void);
uint32_t ft32UartDmaTestEmpty(void);
uint32_t ft32UartDmaTestChannelEnabled(void);
uint32_t ft32UartDmaTestProducerEnabled(void);
uint32_t ft32UartDmaTestTransmitterEnabled(void);
uint16_t ft32UartDmaTestHead(void);
uint16_t ft32UartDmaTestTail(void);
ft32UartDmaTestPinState_e ft32UartDmaTestPinState(void);
uint32_t ft32UartDmaTestProgrammedCount(void);
uint32_t ft32UartDmaTestMonitorTransitions(void);
uint8_t ft32UartDmaTestBasepriAtMonitor(void);
uint8_t ft32UartDmaTestBasepri(void);
uint32_t ft32UartDmaTestEventCount(ft32UartDmaTestEvent_e event);
int32_t ft32UartDmaTestEventIndex(ft32UartDmaTestEvent_e event, uint32_t occurrence);

#ifdef __cplusplus
}
#endif
