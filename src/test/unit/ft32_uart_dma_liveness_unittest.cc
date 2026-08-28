/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * Betaflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "ft32_uart_dma_recovery_test_api.h"

namespace {

std::string readSource(const char *path)
{
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

std::string functionBody(const std::string &source, const std::string &signature)
{
    size_t signatureOffset = source.find(signature);
    size_t bodyStart = std::string::npos;
    while (signatureOffset != std::string::npos) {
        bodyStart = source.find('{', signatureOffset + signature.size());
        const size_t declarationEnd = source.find(';', signatureOffset + signature.size());
        if (bodyStart != std::string::npos &&
            (declarationEnd == std::string::npos || bodyStart < declarationEnd)) {
            break;
        }
        signatureOffset = source.find(signature, signatureOffset + signature.size());
    }
    if (signatureOffset == std::string::npos || bodyStart == std::string::npos) {
        return {};
    }

    unsigned depth = 0U;
    for (size_t offset = bodyStart; offset < source.size(); offset++) {
        if (source[offset] == '{') {
            depth++;
        } else if (source[offset] == '}' && --depth == 0U) {
            return source.substr(bodyStart, offset - bodyStart + 1U);
        }
    }

    return {};
}

void expectOrdered(const std::string &source, const std::initializer_list<const char *> &needles)
{
    size_t previous = 0U;
    bool first = true;
    for (const char *needle : needles) {
        const size_t offset = source.find(needle, first ? 0U : previous + 1U);
        ASSERT_NE(std::string::npos, offset) << needle;
        if (!first) {
            EXPECT_LT(previous, offset) << needle;
        }
        previous = offset;
        first = false;
    }
}

struct RecoveryModel {
    bool threadMode = true;
    bool txMode = true;
    bool resourceOwned = true;
    bool stopSticky = false;
    bool enableAccepted = true;
    bool channelEnabled = false;
    bool producerEnabled = false;
    bool recoveryPending = true;
    bool txEmpty = false;
    bool monitorActive = true;
    unsigned bufferSize = 16U;
    unsigned head = 0U;
    unsigned tail = 0U;
    unsigned stagedTail = 0U;
    unsigned stagedCount = 0U;
    unsigned recoveryAttempts = 0U;
    unsigned writerAttempts = 0U;
    unsigned enableCommands = 0U;
    unsigned commits = 0U;
    unsigned monitorCalls = 0U;
};

void scheduleOnce(RecoveryModel &model, bool recoveryOwner)
{
    if (model.recoveryPending != recoveryOwner) {
        return;
    }
    if (!recoveryOwner && !model.txEmpty) {
        return;
    }

    model.producerEnabled = false;

    if (model.stopSticky) {
        model.channelEnabled = true;
        model.recoveryPending = true;
        return;
    }

    model.channelEnabled = false;
    if (!model.txMode) {
        model.stagedCount = 0U;
        model.txEmpty = model.head == model.tail;
        if (model.monitorActive) {
            model.monitorCalls++;
            model.monitorActive = false;
        }
        model.recoveryPending = false;
        return;
    }

    if (model.head == model.tail) {
        model.stagedCount = 0U;
        model.txEmpty = true;
        if (model.monitorActive) {
            model.monitorCalls++;
            model.monitorActive = false;
        }
        model.recoveryPending = false;
        return;
    }

    const unsigned currentTail = model.tail;
    const unsigned nextTail = model.head > currentTail ? model.head : 0U;
    const unsigned chunk = model.head > currentTail
        ? model.head - currentTail
        : model.bufferSize - currentTail;

    model.stagedTail = currentTail;
    model.stagedCount = chunk;
    model.enableCommands++;
    if (!model.enableAccepted) {
        model.channelEnabled = false;
        model.txEmpty = false;
        model.recoveryPending = true;
        return;
    }

    model.channelEnabled = true;
    model.tail = nextTail;
    model.txEmpty = false;
    model.recoveryPending = false;
    model.producerEnabled = true;
    model.commits++;
}

void recoverOnce(RecoveryModel &model)
{
    model.recoveryAttempts++;
    scheduleOnce(model, true);
}

void pollRecovery(RecoveryModel &model)
{
    if (!model.threadMode || !model.resourceOwned) {
        return;
    }

    if (model.recoveryPending) {
        recoverOnce(model);
    }
}

bool queryEmpty(RecoveryModel &model)
{
    pollRecovery(model);
    return !model.recoveryPending && model.txEmpty;
}

unsigned queryFree(RecoveryModel &model)
{
    pollRecovery(model);
    if (model.recoveryPending) {
        return 0U;
    }

    const unsigned queued = model.head >= model.tail
        ? model.head - model.tail
        : model.bufferSize + model.head - model.tail;
    const unsigned active = model.channelEnabled ? model.stagedCount : 0U;
    const unsigned used = queued + active;
    return used >= model.bufferSize - 1U ? 0U : model.bufferSize - 1U - used;
}

void writerTryStart(RecoveryModel &model)
{
    model.writerAttempts++;
    scheduleOnce(model, false);
}

} // namespace

TEST(Ft32UartDmaLivenessSourceTest, EnableReadbackDominatesOwnershipCommit)
{
    const std::string source = readSource("../platform/FT32/serial_uart_stdperiph.c");
    const std::string scheduler = functionBody(source, "static void uartTryStartTxDMAInternal(uartPort_t *s, bool recoveryOwner)");
    ASSERT_FALSE(scheduler.empty());

    expectOrdered(scheduler, {
        "ft32DmaTrySetCurrDataCounter((DMA_ARCH_TYPE *)s->txDMAResource, chunk)",
        "DMA_SetSrcAddress((DMA_Channel_TypeDef *)s->txDMAResource",
        "xDMA_Cmd(s->txDMAResource, ENABLE);",
        "__DMB();",
        "if (!ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)s->txDMAResource))",
        "s->txDMAEmpty = false;",
        "s->txDMARecoveryPending = true;",
        "return;",
        "s->port.txBufferTail = nextTail;",
        "s->txDMARecoveryPending = false;",
        "ft32UartDMATxEnable_Cmd((USART_TypeDef *)s->USARTx, ENABLE);",
    });

    const std::string rejected = functionBody(
        scheduler,
        "if (!ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)s->txDMAResource))");
    ASSERT_FALSE(rejected.empty());
    EXPECT_EQ(std::string::npos, rejected.find("txBufferTail ="));
    EXPECT_EQ(std::string::npos, rejected.find("DMATxEnable_Cmd"));
    EXPECT_EQ(std::string::npos, rejected.find("DMA_CLEAR_FLAG"));
    EXPECT_EQ(std::string::npos, rejected.find("while ("));
    EXPECT_EQ(std::string::npos, rejected.find("for ("));
}

TEST(Ft32UartDmaLivenessSourceTest, PollIsThreadOnlyAtomicAndDelegatesOneRecovery)
{
    const std::string source = readSource("../main/drivers/serial_uart.c");
    const std::string poll = functionBody(source, "static void uartPollTxDMARecovery(uartPort_t *uartPort)");
    ASSERT_FALSE(poll.empty());

    expectOrdered(poll, {
        "__get_IPSR() != 0U",
        "ATOMIC_BLOCK(NVIC_PRIO_SERIALUART_TXDMA)",
        "uartPort->txDMARecoveryPending",
        "uartTryRecoverTxDMA(uartPort);",
    });
    EXPECT_EQ(std::string::npos, poll.find("uartTxMonitor"));
    EXPECT_EQ(std::string::npos, poll.find("while ("));
    EXPECT_EQ(std::string::npos, poll.find("for ("));
}

TEST(Ft32UartDmaLivenessSourceTest, RecoveryOwnsIdentityAndMonitorInsideAtomicFence)
{
    const std::string source = readSource("../platform/FT32/serial_uart_stdperiph.c");
    const std::string scheduler = functionBody(source, "static void uartTryStartTxDMAInternal(uartPort_t *s, bool recoveryOwner)");
    const std::string recovery = functionBody(source, "void uartTryRecoverTxDMA(uartPort_t *s)");
    ASSERT_FALSE(scheduler.empty());
    ASSERT_FALSE(recovery.empty());

    expectOrdered(scheduler, {
        "ATOMIC_BLOCK(NVIC_PRIO_SERIALUART_TXDMA)",
        "const bool recoveryPending = s->txDMARecoveryPending;",
        "if (recoveryPending != recoveryOwner)",
        "if (!recoveryOwner && !s->txDMAEmpty)",
        "ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)s->txDMAResource)",
    });
    expectOrdered(scheduler, {
        "s->txDMAEmpty = s->port.txBufferHead == s->port.txBufferTail;",
        "uartTxMonitor(s);",
        "s->txDMARecoveryPending = false;",
    });
    EXPECT_EQ(std::string::npos, recovery.find("uartTxMonitor"));

    const std::string irqSource = readSource("../platform/FT32/serial_uart_ft32f4xx.c");
    const std::string handoff = functionBody(irqSource, "static void handleUsartTxDma(uartPort_t *s)");
    ASSERT_FALSE(handoff.empty());
    EXPECT_NE(std::string::npos, handoff.find("uartTryRecoverTxDMA(s);"));
    EXPECT_EQ(std::string::npos, handoff.find("uartTxMonitor"));
}

TEST(Ft32UartDmaLivenessSourceTest, BufferedWriteResamplesAndAlwaysClosesFt32Dma)
{
    const std::string source = readSource("../main/drivers/serial_uart.c");
    const std::string writeBuffer = functionBody(source, "static void uartWriteBuf(serialPort_t *instance, const void *data, int count)");
    const std::string endWrite = functionBody(source, "static void uartEndWrite(serialPort_t *instance)");
    ASSERT_FALSE(writeBuffer.empty());
    ASSERT_FALSE(endWrite.empty());

    expectOrdered(writeBuffer, {
        "uart->txPinState == TX_PIN_MONITOR",
        "uartPort->txDMAResource && ft32UartDmaWriteBufferReady(uartPort)",
        "while (count > 0)",
    });
    expectOrdered(endWrite, {
        "if (uartPort->txDMAResource)",
        "ft32UartDmaFinishWriteBuffer(uartPort);",
        "return;",
        "if (uart->txPinState == TX_PIN_MONITOR)",
    });
}

TEST(Ft32UartDmaLivenessSourceTest, QueriesUseConservativePendingResults)
{
    const std::string source = readSource("../main/drivers/serial_uart.c");
    const std::string freeQuery = functionBody(source, "static uint32_t uartTotalTxBytesFree(const serialPort_t *instance)");
    const std::string emptyQuery = functionBody(source, "static bool isUartTransmitBufferEmpty(const serialPort_t *instance)");
    ASSERT_FALSE(freeQuery.empty());
    ASSERT_FALSE(emptyQuery.empty());

    expectOrdered(freeQuery, {
        "uartPollTxDMARecovery((uartPort_t *)uartPort);",
        "if (uartPort->txDMARecoveryPending)",
        "return 0U;",
        "const uint32_t bytesFree",
        "if (uartPort->txDMARecoveryPending)",
        "return bytesFree;",
    });
    expectOrdered(emptyQuery, {
        "uartPollTxDMARecovery((uartPort_t *)uartPort);",
        "if (uartPort->txDMARecoveryPending)",
        "return false;",
        "return !uartPort->txDMARecoveryPending && empty;",
    });
}

TEST(Ft32UartDmaLivenessSourceTest, SettersFencePublicationThroughReconfigure)
{
    const std::string source = readSource("../main/drivers/serial_uart.c");
    const std::string setMode = functionBody(source, "static void uartSetMode(serialPort_t *instance, portMode_e mode)");
    const std::string setBaud = functionBody(source, "static void uartSetBaudRate(serialPort_t *instance, uint32_t baudRate)");
    ASSERT_FALSE(setMode.empty());
    ASSERT_FALSE(setBaud.empty());

    expectOrdered(setMode, {
        "if (uartPort->txDMAResource)",
        "ATOMIC_BLOCK(NVIC_PRIO_SERIALUART_TXDMA)",
        "uartPort->port.mode = uartSanitizeMode",
        "uartReconfigure(uartPort);",
    });
    expectOrdered(setBaud, {
        "if (uartPort->txDMAResource)",
        "ATOMIC_BLOCK(NVIC_PRIO_SERIALUART_TXDMA)",
        "uartPort->port.mode = uartSanitizeMode",
        "uartPort->port.baudRate = baudRate;",
        "uartReconfigure(uartPort);",
    });
}

TEST(Ft32UartDmaLivenessSourceTest, ModeRemovalDropsCheckedTxActiveStateBeforeDmaConvergence)
{
    const std::string source = readSource("../platform/FT32/serial_uart_stdperiph.c");
    const std::string reconfigure = functionBody(source, "static void uartReconfigureInternal(uartPort_t *uartPort)");
    const std::string pause = functionBody(source, "static void ft32UartPauseCheckedTxForMode(uartPort_t *uartPort)");
    ASSERT_FALSE(reconfigure.empty());
    ASSERT_FALSE(pause.empty());

    expectOrdered(reconfigure, {
        "uartPort->txDMARecoveryPending = true;",
        "ft32UartDMATxEnable_Cmd(USARTx, DISABLE);",
        "ft32DmaRequestDisable((DMA_ARCH_TYPE *)uartPort->txDMAResource);",
        "ft32UartPauseCheckedTxForMode(uartPort);",
    });
    expectOrdered(pause, {
        "!(uartPort->port.mode & MODE_TX)",
        "uartPort->port.options & SERIAL_CHECK_TX",
        "uartTxMonitor(uartPort);",
    });
    EXPECT_EQ(std::string::npos, pause.find("ft32DmaIsChannelEnabled"));
}

TEST(Ft32UartDmaLivenessModelTest, RejectedEnableRetainsSuffixUntilOneSuccessfulCommit)
{
    RecoveryModel model;
    model.head = 9U;
    model.tail = 4U;
    model.enableAccepted = false;

    recoverOnce(model);
    EXPECT_TRUE(model.recoveryPending);
    EXPECT_FALSE(model.channelEnabled);
    EXPECT_FALSE(model.producerEnabled);
    EXPECT_FALSE(model.txEmpty);
    EXPECT_EQ(4U, model.tail);
    EXPECT_EQ(4U, model.stagedTail);
    EXPECT_EQ(5U, model.stagedCount);
    EXPECT_EQ(0U, model.commits);

    model.enableAccepted = true;
    EXPECT_FALSE(queryEmpty(model));
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_TRUE(model.channelEnabled);
    EXPECT_TRUE(model.producerEnabled);
    EXPECT_EQ(9U, model.tail);
    EXPECT_EQ(1U, model.commits);
}

TEST(Ft32UartDmaLivenessModelTest, EmptyPollingConvergesWithoutTaskOrWriteAndRunsMonitor)
{
    RecoveryModel model;
    model.head = 5U;
    model.tail = 5U;
    model.stopSticky = true;

    EXPECT_FALSE(queryEmpty(model));
    EXPECT_TRUE(model.recoveryPending);
    EXPECT_EQ(1U, model.recoveryAttempts);
    EXPECT_EQ(0U, model.monitorCalls);

    model.stopSticky = false;
    EXPECT_TRUE(queryEmpty(model));
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_FALSE(model.channelEnabled);
    EXPECT_FALSE(model.producerEnabled);
    EXPECT_EQ(2U, model.recoveryAttempts);
    EXPECT_EQ(1U, model.monitorCalls);
    EXPECT_EQ(0U, model.writerAttempts);
}

TEST(Ft32UartDmaLivenessModelTest, FreePollingConvergesQueuedSuffixWithoutTaskOrWrite)
{
    RecoveryModel model;
    model.head = 10U;
    model.tail = 6U;
    model.stopSticky = true;

    EXPECT_EQ(0U, queryFree(model));
    EXPECT_TRUE(model.recoveryPending);
    model.stopSticky = false;
    EXPECT_EQ(11U, queryFree(model));
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_EQ(10U, model.tail);
    EXPECT_EQ(1U, model.commits);
    EXPECT_EQ(0U, model.writerAttempts);
}

TEST(Ft32UartDmaLivenessModelTest, ExceptionQueryNeverConsumesRecoveryOwner)
{
    RecoveryModel model;
    model.threadMode = false;
    model.head = 9U;
    model.tail = 3U;

    EXPECT_FALSE(queryEmpty(model));
    EXPECT_EQ(0U, queryFree(model));
    EXPECT_TRUE(model.recoveryPending);
    EXPECT_EQ(3U, model.tail);
    EXPECT_EQ(0U, model.recoveryAttempts);
    EXPECT_EQ(0U, model.enableCommands);
}

TEST(Ft32UartDmaLivenessModelTest, PendingWriterCannotStealRecovery)
{
    RecoveryModel model;
    model.head = 8U;
    model.tail = 2U;

    writerTryStart(model);
    EXPECT_TRUE(model.recoveryPending);
    EXPECT_EQ(2U, model.tail);
    EXPECT_EQ(0U, model.recoveryAttempts);
    EXPECT_EQ(0U, model.enableCommands);
    EXPECT_EQ(1U, model.writerAttempts);
}

TEST(Ft32UartDmaLivenessModelTest, StaleServiceOwnerCannotOverwriteIrqCommit)
{
    RecoveryModel model;
    model.head = 8U;
    model.tail = 2U;
    const bool staleServiceSnapshot = model.recoveryPending;

    recoverOnce(model);
    ASSERT_TRUE(staleServiceSnapshot);
    ASSERT_FALSE(model.recoveryPending);
    ASSERT_TRUE(model.channelEnabled);
    ASSERT_EQ(8U, model.tail);
    ASSERT_EQ(1U, model.commits);

    model.head = 11U;
    recoverOnce(model);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_TRUE(model.channelEnabled);
    EXPECT_EQ(8U, model.tail);
    EXPECT_EQ(1U, model.commits);
}

TEST(Ft32UartDmaLivenessModelTest, NonIdleWriterWaitsForTerminalOwner)
{
    RecoveryModel model;
    model.recoveryPending = false;
    model.txEmpty = false;
    model.channelEnabled = false;
    model.head = 11U;
    model.tail = 8U;

    writerTryStart(model);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_FALSE(model.channelEnabled);
    EXPECT_EQ(8U, model.tail);
    EXPECT_EQ(0U, model.enableCommands);
    EXPECT_EQ(0U, model.commits);
}

TEST(Ft32UartDmaLivenessModelTest, BufferedCommitKeepsFreeAndEmptyAccountingExact)
{
    RecoveryModel nonWrapped;
    nonWrapped.recoveryPending = false;
    nonWrapped.txEmpty = true;
    nonWrapped.head = 5U;
    nonWrapped.tail = 0U;

    writerTryStart(nonWrapped);
    EXPECT_FALSE(queryEmpty(nonWrapped));
    EXPECT_EQ(10U, queryFree(nonWrapped));
    EXPECT_EQ(5U, nonWrapped.tail);
    EXPECT_EQ(5U, nonWrapped.stagedCount);

    RecoveryModel wrapped;
    wrapped.recoveryPending = false;
    wrapped.txEmpty = true;
    wrapped.head = 3U;
    wrapped.tail = 13U;

    writerTryStart(wrapped);
    EXPECT_FALSE(queryEmpty(wrapped));
    EXPECT_EQ(9U, queryFree(wrapped));
    EXPECT_EQ(0U, wrapped.tail);
    EXPECT_EQ(3U, wrapped.stagedCount);

    wrapped.recoveryPending = true;
    wrapped.channelEnabled = false;
    wrapped.producerEnabled = false;
    EXPECT_FALSE(queryEmpty(wrapped));
    EXPECT_EQ(12U, queryFree(wrapped));
    EXPECT_EQ(3U, wrapped.tail);
    EXPECT_EQ(2U, wrapped.commits);
}

TEST(Ft32UartDmaLivenessModelTest, ModeRemoveThenRestoreCommitsRetainedSuffixOnce)
{
    RecoveryModel model;
    model.head = 9U;
    model.tail = 4U;
    model.txMode = false;

    recoverOnce(model);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_FALSE(model.channelEnabled);
    EXPECT_FALSE(model.producerEnabled);
    EXPECT_FALSE(model.txEmpty);
    EXPECT_EQ(4U, model.tail);
    EXPECT_EQ(0U, model.commits);

    model.txMode = true;
    model.recoveryPending = true;
    recoverOnce(model);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_TRUE(model.channelEnabled);
    EXPECT_TRUE(model.producerEnabled);
    EXPECT_EQ(9U, model.tail);
    EXPECT_EQ(1U, model.commits);
}

TEST(Ft32UartDmaLivenessModelTest, RejectedRestoreLaterConvergesThroughQuery)
{
    RecoveryModel model;
    model.head = 9U;
    model.tail = 4U;
    model.txMode = false;
    recoverOnce(model);

    model.txMode = true;
    model.recoveryPending = true;
    model.enableAccepted = false;
    recoverOnce(model);
    EXPECT_TRUE(model.recoveryPending);
    EXPECT_EQ(4U, model.tail);
    EXPECT_EQ(0U, model.commits);

    model.enableAccepted = true;
    EXPECT_FALSE(queryEmpty(model));
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_EQ(9U, model.tail);
    EXPECT_EQ(1U, model.commits);
}

TEST(Ft32UartDmaLivenessModelTest, StickyRemoveFollowedByRestoreUsesCurrentMode)
{
    RecoveryModel model;
    model.head = 3U;
    model.tail = 13U;
    model.txMode = false;
    model.stopSticky = true;
    recoverOnce(model);
    EXPECT_TRUE(model.recoveryPending);
    EXPECT_EQ(13U, model.tail);

    model.txMode = true;
    model.stopSticky = false;
    EXPECT_FALSE(queryEmpty(model));
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_EQ(0U, model.tail);
    EXPECT_EQ(13U, model.stagedTail);
    EXPECT_EQ(3U, model.stagedCount);
    EXPECT_EQ(1U, model.commits);
}

class Ft32UartDmaProductionRecoveryTest : public testing::Test {
protected:
    void SetUp() override
    {
        ft32UartDmaTestReset();
    }
};

TEST_F(Ft32UartDmaProductionRecoveryTest, ActiveResetEmptyRecoveryRestoresThenMonitorsTransmitter)
{
    ft32UartDmaTestApplyPeripheralResetState();
    EXPECT_TRUE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_TX_RESET));
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_TX_ENABLE));

    ft32UartDmaTestRecover();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_TRUE(ft32UartDmaTestEmpty());
    EXPECT_FALSE(ft32UartDmaTestProducerEnabled());
    EXPECT_FALSE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(FT32_UART_DMA_TEST_PIN_MONITOR, ft32UartDmaTestPinState());
    EXPECT_LT(ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_TX_RESET, 0U),
              ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_TX_ENABLE, 0U));
    EXPECT_LT(ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_TRY_SET_COUNT, 0U),
              ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_MONITOR, 0U));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, ModeOffRetainsSuffixAndLineLowRestoreDefersCommit)
{
    ft32UartDmaTestSetRing(9U, 4U);
    ft32UartDmaTestSetModeTx(false);
    ft32UartDmaTestPauseCheckedTxForMode();
    ft32UartDmaTestRecover();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_EQ(4U, ft32UartDmaTestTail());
    EXPECT_EQ(FT32_UART_DMA_TEST_PIN_MONITOR, ft32UartDmaTestPinState());
    EXPECT_FALSE(ft32UartDmaTestTransmitterEnabled());

    ft32UartDmaTestSetLineHigh(false);
    ft32UartDmaTestSetModeTx(true);
    ft32UartDmaTestSetPending(true);
    ft32UartDmaTestApplyPeripheralResetState();
    ft32UartDmaTestRecover();

    EXPECT_TRUE(ft32UartDmaTestPending());
    EXPECT_EQ(4U, ft32UartDmaTestTail());
    EXPECT_FALSE(ft32UartDmaTestChannelEnabled());
    EXPECT_FALSE(ft32UartDmaTestProducerEnabled());
    EXPECT_FALSE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_SET_SOURCE));

    ft32UartDmaTestSetLineHigh(true);
    ft32UartDmaTestRecover();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_TRUE(ft32UartDmaTestProducerEnabled());
    EXPECT_TRUE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(9U, ft32UartDmaTestTail());
    EXPECT_LT(ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE, 1U),
              ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_SET_SOURCE, 0U));
    EXPECT_LT(ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_CHEN_READ, 0U),
              ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_DMAT_ON, 0U));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, StickyModeRemoveForcesLineRecheckBeforeImmediateRestore)
{
    ft32UartDmaTestSetRing(3U, 13U);
    ft32UartDmaTestSetChannelEnabled(true);
    ft32UartDmaTestSetStopSticky(true);
    ft32UartDmaTestSetModeTx(false);
    ft32UartDmaTestPauseCheckedTxForMode();
    ft32UartDmaTestRecover();

    EXPECT_TRUE(ft32UartDmaTestPending());
    EXPECT_EQ(FT32_UART_DMA_TEST_PIN_MONITOR, ft32UartDmaTestPinState());
    EXPECT_FALSE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(13U, ft32UartDmaTestTail());

    ft32UartDmaTestSetLineHigh(false);
    ft32UartDmaTestSetModeTx(true);
    ft32UartDmaTestApplyPeripheralResetState();
    ft32UartDmaTestSetStopSticky(false);
    ft32UartDmaTestRecover();

    EXPECT_TRUE(ft32UartDmaTestPending());
    EXPECT_EQ(FT32_UART_DMA_TEST_PIN_MONITOR, ft32UartDmaTestPinState());
    EXPECT_FALSE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_FALSE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(13U, ft32UartDmaTestTail());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_SET_SOURCE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, ExceptionDefersMonitoredLineToForeground)
{
    ft32UartDmaTestSetRing(8U, 2U);
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_MONITOR);
    ft32UartDmaTestSetTransmitterEnabled(false);
    ft32UartDmaTestSetExceptionContext(true);

    ft32UartDmaTestRecover();

    EXPECT_TRUE(ft32UartDmaTestPending());
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_SET_SOURCE));
    EXPECT_EQ(2U, ft32UartDmaTestTail());

    ft32UartDmaTestSetExceptionContext(false);
    ft32UartDmaTestRecover();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_TRUE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(8U, ft32UartDmaTestTail());
    EXPECT_LT(ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE, 0U),
              ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_SET_SOURCE, 0U));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, RejectedEnableKeepsTailAndProducerOffUntilOneCommit)
{
    ft32UartDmaTestSetRing(9U, 4U);
    ft32UartDmaTestSetEnableAccepted(false);

    ft32UartDmaTestRecover();

    EXPECT_TRUE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestChannelEnabled());
    EXPECT_FALSE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(4U, ft32UartDmaTestTail());
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));

    ft32UartDmaTestSetEnableAccepted(true);
    ft32UartDmaTestRecover();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_TRUE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(9U, ft32UartDmaTestTail());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));
    EXPECT_EQ(2U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMA_ENABLE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, StickyStopPreventsLineAndDescriptorMutation)
{
    ft32UartDmaTestSetRing(10U, 6U);
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_MONITOR);
    ft32UartDmaTestSetTransmitterEnabled(false);
    ft32UartDmaTestSetChannelEnabled(true);
    ft32UartDmaTestSetStopSticky(true);

    ft32UartDmaTestRecover();

    EXPECT_TRUE(ft32UartDmaTestPending());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_FALSE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(6U, ft32UartDmaTestTail());
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_SET_SOURCE));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMA_ENABLE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, IgnoreStateNeverArmsCheckedTxDma)
{
    ft32UartDmaTestSetRing(12U, 7U);
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_IGNORE);
    ft32UartDmaTestSetTransmitterEnabled(false);

    ft32UartDmaTestRecover();

    EXPECT_TRUE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestChannelEnabled());
    EXPECT_FALSE(ft32UartDmaTestProducerEnabled());
    EXPECT_FALSE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(7U, ft32UartDmaTestTail());
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_SET_SOURCE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, PendingWriterIsNoOpAndAtomicRestoresBasepri)
{
    ft32UartDmaTestSetRing(8U, 2U);
    ft32UartDmaTestSetBasepri(96U);

    ft32UartDmaTestWriterStart();

    EXPECT_TRUE(ft32UartDmaTestPending());
    EXPECT_EQ(2U, ft32UartDmaTestTail());
    EXPECT_EQ(96U, ft32UartDmaTestBasepri());
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_OFF));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_TRY_SET_COUNT));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, NonPendingRecoveryOwnerIsNoOp)
{
    ft32UartDmaTestSetRing(11U, 8U);
    ft32UartDmaTestSetPending(false);
    ft32UartDmaTestSetEmpty(false);
    ft32UartDmaTestSetChannelEnabled(false);
    ft32UartDmaTestClearEvents();

    ft32UartDmaTestRecover();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_EQ(8U, ft32UartDmaTestTail());
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_OFF));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_CHEN_READ));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_TRY_SET_COUNT));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMA_ENABLE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, WriterCannotConsumeTerminalBeforePendingPublication)
{
    ft32UartDmaTestSetRing(11U, 8U);
    ft32UartDmaTestSetPending(false);
    ft32UartDmaTestSetEmpty(false);
    ft32UartDmaTestSetChannelEnabled(false);
    ft32UartDmaTestClearEvents();

    ft32UartDmaTestWriterStart();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_EQ(8U, ft32UartDmaTestTail());
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_OFF));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_CHEN_READ));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_TRY_SET_COUNT));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMA_ENABLE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, IdleWriterCanCommitQueuedBlock)
{
    ft32UartDmaTestSetRing(11U, 8U);
    ft32UartDmaTestSetPending(false);
    ft32UartDmaTestSetEmpty(true);
    ft32UartDmaTestSetChannelEnabled(false);

    ft32UartDmaTestWriterStart();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_TRUE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(11U, ft32UartDmaTestTail());
}

TEST_F(Ft32UartDmaProductionRecoveryTest, BufferedMonitorCheckPreservesLowAndReactivatesHigh)
{
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_MONITOR);
    ft32UartDmaTestSetTransmitterEnabled(false);
    ft32UartDmaTestSetLineHigh(false);
    ft32UartDmaTestClearEvents();

    EXPECT_EQ(0U, ft32UartDmaTestPrepareWriteBuffer());
    EXPECT_EQ(FT32_UART_DMA_TEST_PIN_MONITOR, ft32UartDmaTestPinState());
    EXPECT_FALSE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_TX_ENABLE));

    ft32UartDmaTestReset();
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_MONITOR);
    ft32UartDmaTestSetTransmitterEnabled(false);
    ft32UartDmaTestSetLineHigh(true);
    ft32UartDmaTestClearEvents();

    EXPECT_EQ(1U, ft32UartDmaTestPrepareWriteBuffer());
    EXPECT_EQ(FT32_UART_DMA_TEST_PIN_ACTIVE, ft32UartDmaTestPinState());
    EXPECT_TRUE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE));
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_TX_ENABLE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, CompletionBeforeNonWrappedHeadPublishClosesHighAndLow)
{
    ft32UartDmaTestSetPending(false);
    ft32UartDmaTestSetEmpty(true);
    ft32UartDmaTestSetChannelEnabled(false);
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_MONITOR);
    ft32UartDmaTestSetTransmitterEnabled(false);
    ft32UartDmaTestSetLineHigh(true);
    ft32UartDmaTestSetRing(5U, 0U);
    ft32UartDmaTestClearEvents();

    ft32UartDmaTestFinishWriteBuffer();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_EQ(5U, ft32UartDmaTestHead());
    EXPECT_EQ(5U, ft32UartDmaTestTail());
    EXPECT_EQ(5U, ft32UartDmaTestProgrammedCount());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_TRUE(ft32UartDmaTestProducerEnabled());
    EXPECT_TRUE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));
    EXPECT_LT(ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE, 0U),
              ft32UartDmaTestEventIndex(FT32_UART_DMA_TEST_EVENT_SET_SOURCE, 0U));

    ft32UartDmaTestReset();
    ft32UartDmaTestSetPending(false);
    ft32UartDmaTestSetEmpty(true);
    ft32UartDmaTestSetChannelEnabled(false);
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_MONITOR);
    ft32UartDmaTestSetTransmitterEnabled(false);
    ft32UartDmaTestSetLineHigh(false);
    ft32UartDmaTestSetRing(5U, 0U);
    ft32UartDmaTestClearEvents();

    ft32UartDmaTestFinishWriteBuffer();

    EXPECT_TRUE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_EQ(5U, ft32UartDmaTestHead());
    EXPECT_EQ(0U, ft32UartDmaTestTail());
    EXPECT_EQ(5U, ft32UartDmaTestProgrammedCount());
    EXPECT_FALSE(ft32UartDmaTestChannelEnabled());
    EXPECT_FALSE(ft32UartDmaTestProducerEnabled());
    EXPECT_FALSE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_OFF));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_SET_SOURCE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, CompletionAfterNonWrappedHeadPublishKeepsActiveOwner)
{
    ft32UartDmaTestSetRing(5U, 0U);
    ft32UartDmaTestSetPending(true);
    ft32UartDmaTestSetEmpty(false);
    ft32UartDmaTestSetChannelEnabled(false);
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_ACTIVE);
    ft32UartDmaTestSetTransmitterEnabled(true);
    ft32UartDmaTestSetLineHigh(false);
    ft32UartDmaTestClearEvents();

    ft32UartDmaTestRecover();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_EQ(5U, ft32UartDmaTestHead());
    EXPECT_EQ(5U, ft32UartDmaTestTail());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_TRUE(ft32UartDmaTestProducerEnabled());
    EXPECT_TRUE(ft32UartDmaTestTransmitterEnabled());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE));

    ft32UartDmaTestClearEvents();
    ft32UartDmaTestFinishWriteBuffer();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_EQ(5U, ft32UartDmaTestHead());
    EXPECT_EQ(5U, ft32UartDmaTestTail());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_TRUE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_OFF));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_SET_SOURCE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, CompletionBeforeWrappedHeadPublishClosesHighAndLow)
{
    ft32UartDmaTestSetPending(false);
    ft32UartDmaTestSetEmpty(true);
    ft32UartDmaTestSetChannelEnabled(false);
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_MONITOR);
    ft32UartDmaTestSetTransmitterEnabled(false);
    ft32UartDmaTestSetLineHigh(true);
    ft32UartDmaTestSetRing(3U, 13U);
    ft32UartDmaTestClearEvents();

    ft32UartDmaTestFinishWriteBuffer();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_EQ(3U, ft32UartDmaTestHead());
    EXPECT_EQ(0U, ft32UartDmaTestTail());
    EXPECT_EQ(3U, ft32UartDmaTestProgrammedCount());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_TRUE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));

    ft32UartDmaTestReset();
    ft32UartDmaTestSetPending(false);
    ft32UartDmaTestSetEmpty(true);
    ft32UartDmaTestSetChannelEnabled(false);
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_MONITOR);
    ft32UartDmaTestSetTransmitterEnabled(false);
    ft32UartDmaTestSetLineHigh(false);
    ft32UartDmaTestSetRing(3U, 13U);
    ft32UartDmaTestClearEvents();

    ft32UartDmaTestFinishWriteBuffer();

    EXPECT_TRUE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_EQ(3U, ft32UartDmaTestHead());
    EXPECT_EQ(13U, ft32UartDmaTestTail());
    EXPECT_EQ(3U, ft32UartDmaTestProgrammedCount());
    EXPECT_FALSE(ft32UartDmaTestChannelEnabled());
    EXPECT_FALSE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_SET_SOURCE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, CompletionDuringWrappedPublishCommitsTwoDisjointChunks)
{
    // The first memcpy chunk publishes head=0 just before the old transfer
    // completes; recovery owns only the end-of-ring bytes.
    ft32UartDmaTestSetRing(0U, 13U);
    ft32UartDmaTestSetPending(true);
    ft32UartDmaTestSetEmpty(false);
    ft32UartDmaTestSetChannelEnabled(false);
    ft32UartDmaTestSetPinState(FT32_UART_DMA_TEST_PIN_ACTIVE);
    ft32UartDmaTestSetTransmitterEnabled(true);
    ft32UartDmaTestSetLineHigh(false);
    ft32UartDmaTestClearEvents();

    ft32UartDmaTestRecover();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_EQ(0U, ft32UartDmaTestHead());
    EXPECT_EQ(0U, ft32UartDmaTestTail());
    EXPECT_EQ(3U, ft32UartDmaTestProgrammedCount());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_TRUE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE));

    // The writer then publishes the second chunk. endWrite must not steal the
    // active block; the next terminal owner commits exactly [0, 3).
    ft32UartDmaTestSetRing(3U, 0U);
    ft32UartDmaTestClearEvents();
    ft32UartDmaTestFinishWriteBuffer();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_EQ(3U, ft32UartDmaTestHead());
    EXPECT_EQ(0U, ft32UartDmaTestTail());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_OFF));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_SET_SOURCE));

    ft32UartDmaTestSetChannelEnabled(false);
    ft32UartDmaTestSetPending(true);
    ft32UartDmaTestClearEvents();
    ft32UartDmaTestRecover();

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_FALSE(ft32UartDmaTestEmpty());
    EXPECT_EQ(3U, ft32UartDmaTestHead());
    EXPECT_EQ(3U, ft32UartDmaTestTail());
    EXPECT_EQ(3U, ft32UartDmaTestProgrammedCount());
    EXPECT_TRUE(ft32UartDmaTestChannelEnabled());
    EXPECT_TRUE(ft32UartDmaTestProducerEnabled());
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_DMAT_ON));
    EXPECT_EQ(1U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_SET_SOURCE));
    EXPECT_EQ(0U, ft32UartDmaTestEventCount(FT32_UART_DMA_TEST_EVENT_LINE_SAMPLE));
}

TEST_F(Ft32UartDmaProductionRecoveryTest, MonitorRunsInsideDmaFenceAndNestedBasepriRestores)
{
    ft32UartDmaTestRecoverNested(96U, 80U);

    EXPECT_FALSE(ft32UartDmaTestPending());
    EXPECT_TRUE(ft32UartDmaTestEmpty());
    EXPECT_EQ(64U, ft32UartDmaTestBasepriAtMonitor());
    EXPECT_EQ(0U, ft32UartDmaTestBasepri());
    EXPECT_EQ(1U, ft32UartDmaTestMonitorTransitions());

    ft32UartDmaTestReset();
    ft32UartDmaTestSetBasepri(32U);
    ft32UartDmaTestRecover();

    EXPECT_EQ(32U, ft32UartDmaTestBasepriAtMonitor());
    EXPECT_EQ(32U, ft32UartDmaTestBasepri());
    EXPECT_EQ(1U, ft32UartDmaTestMonitorTransitions());
}
