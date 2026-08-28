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
#include <sstream>
#include <string>

#include <gtest/gtest.h>

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
    const size_t signatureOffset = source.find(signature);
    if (signatureOffset == std::string::npos) {
        return {};
    }

    const size_t bodyStart = source.find('{', signatureOffset + signature.size());
    if (bodyStart == std::string::npos) {
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

enum TerminalFlag : unsigned {
    TFR = 1U,
    ERR = 2U,
};

struct RecoveryModel {
    bool producerEnabled = true;
    bool channelEnabled = true;
    bool stickyDisable = false;
    bool txModeEnabled = true;
    bool recoveryPending = false;
    bool txEmpty = false;
    unsigned observedAck = 0U;
    unsigned disableRequests = 0U;
    unsigned channelReads = 0U;
    unsigned counterWrites = 0U;
    unsigned rearms = 0U;
    unsigned head = 0U;
    unsigned tail = 0U;
};

void requestDisable(RecoveryModel &model)
{
    model.disableRequests++;
    if (!model.stickyDisable) {
        model.channelEnabled = false;
    }
}

bool channelEnabled(RecoveryModel &model)
{
    model.channelReads++;
    return model.channelEnabled;
}

void recoverOnce(RecoveryModel &model)
{
    model.producerEnabled = false;
    requestDisable(model);
    if (channelEnabled(model)) {
        model.recoveryPending = true;
        return;
    }

    model.counterWrites++;
    if (!model.txModeEnabled) {
        model.txEmpty = model.head == model.tail;
        model.recoveryPending = false;
        return;
    }

    if (model.head == model.tail) {
        model.txEmpty = true;
        model.recoveryPending = false;
        return;
    }

    const unsigned nextTail = model.head;
    model.channelEnabled = true;
    model.tail = nextTail;
    model.txEmpty = false;
    model.recoveryPending = false;
    model.producerEnabled = true;
    model.rearms++;
}

void writerTryStart(RecoveryModel &model)
{
    if (model.recoveryPending) {
        return;
    }

    recoverOnce(model);
}

void reconfigureTx(RecoveryModel &model)
{
    model.recoveryPending = true;
    model.producerEnabled = false;
    requestDisable(model);
    if (channelEnabled(model)) {
        return;
    }

    recoverOnce(model);
}

void terminalIrq(RecoveryModel &model, unsigned observed)
{
    if (observed == 0U) {
        return;
    }

    model.producerEnabled = false;
    requestDisable(model);
    model.recoveryPending = true;
    if (channelEnabled(model)) {
        model.observedAck |= observed;
        return;
    }

    model.observedAck |= observed;
    recoverOnce(model);
}

} // namespace

TEST(Ft32UartDmaSourceTest, IrqUsesProducerFirstBoundedStop)
{
    const std::string source = readSource("../platform/FT32/serial_uart_ft32f4xx.c");
    const std::string handler = functionBody(source, "void uartDmaIrqHandler(dmaChannelDescriptor_t *descriptor)");
    ASSERT_FALSE(handler.empty());

    expectOrdered(handler, {
        "ft32UartDMATxEnable_Cmd((USART_TypeDef *)s->USARTx, DISABLE);",
        "ft32DmaRequestDisable((DMA_ARCH_TYPE *)s->txDMAResource);",
        "s->txDMARecoveryPending = true;",
        "ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)s->txDMAResource)",
    });
    EXPECT_EQ(std::string::npos, handler.find("xDMA_Cmd("));
    EXPECT_EQ(std::string::npos, handler.find("xDMA_SetCurrDataCounter("));
    EXPECT_EQ(std::string::npos, handler.find("while ("));
    EXPECT_EQ(std::string::npos, handler.find("for ("));
    EXPECT_NE(std::string::npos, handler.find("terminalMask |= DMA_IT_TFR;"));
    EXPECT_NE(std::string::npos, handler.find("terminalMask |= DMA_IT_ERR;"));
    expectOrdered(handler, {
        "if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)s->txDMAResource))",
        "DMA_CLEAR_FLAG(descriptor, terminalMask);",
        "return;",
        "DMA_CLEAR_FLAG(descriptor, terminalMask);",
        "handleUsartTxDma(s);",
    });
}

TEST(Ft32UartDmaSourceTest, ForegroundServiceScansOnlyPendingPorts)
{
    const std::string source = readSource("../platform/FT32/serial_uart_ft32f4xx.c");
    const std::string service = functionBody(source, "void uartDmaService(void)");
    ASSERT_FALSE(service.empty());
    EXPECT_NE(std::string::npos, service.find("index < UARTDEV_COUNT"));
    EXPECT_NE(std::string::npos, service.find("s->txDMAResource && s->txDMARecoveryPending"));
    EXPECT_NE(std::string::npos, service.find("handleUsartTxDma(s);"));

    const std::string tasks = readSource("../main/fc/tasks.c");
    const std::string mainTask = functionBody(tasks, "static void taskMain(timeUs_t currentTimeUs)");
    ASSERT_FALSE(mainTask.empty());
    EXPECT_NE(std::string::npos, mainTask.find("uartDmaService();"));
    EXPECT_NE(std::string::npos, tasks.find("TASK_PERIOD_HZ(1000)"));
}

TEST(Ft32UartDmaSourceTest, RearmCommitsOwnershipAfterBoundedProgramming)
{
    const std::string source = readSource("../platform/FT32/serial_uart_stdperiph.c");
    const std::string scheduler = functionBody(source, "static void uartTryStartTxDMAInternal(uartPort_t *s, bool recoveryOwner)");
    ASSERT_FALSE(scheduler.empty());

    EXPECT_EQ(std::string::npos, scheduler.find("xDMA_SetCurrDataCounter("));
    EXPECT_EQ(std::string::npos, scheduler.find("xDMA_Cmd(s->txDMAResource, DISABLE)"));
    EXPECT_EQ(std::string::npos, scheduler.find("ft32DmaRequestDisable("));
    EXPECT_NE(std::string::npos, scheduler.find("recoveryPending != recoveryOwner"));
    EXPECT_NE(std::string::npos, scheduler.find("!recoveryOwner && !s->txDMAEmpty"));
    EXPECT_NE(std::string::npos, scheduler.find("!(s->port.mode & MODE_TX)"));
    expectOrdered(scheduler, {
        "if (!(s->port.mode & MODE_TX))",
        "ft32DmaTrySetCurrDataCounter((DMA_ARCH_TYPE *)s->txDMAResource, 0U)",
        "s->txDMAEmpty = s->port.txBufferHead == s->port.txBufferTail;",
        "s->txDMARecoveryPending = false;",
    });
    expectOrdered(scheduler, {
        "ft32UartDMATxEnable_Cmd((USART_TypeDef *)s->USARTx, DISABLE);",
        "ft32DmaTrySetCurrDataCounter((DMA_ARCH_TYPE *)s->txDMAResource, chunk)",
        "DMA_SetSrcAddress((DMA_Channel_TypeDef *)s->txDMAResource",
        "xDMA_Cmd(s->txDMAResource, ENABLE);",
        "s->port.txBufferTail = nextTail;",
        "s->txDMARecoveryPending = false;",
        "ft32UartDMATxEnable_Cmd((USART_TypeDef *)s->USARTx, ENABLE);",
    });

    const std::string portHeader = readSource("../main/drivers/serial_uart.h");
    EXPECT_NE(std::string::npos, portHeader.find("volatile bool txDMARecoveryPending;"));
}

TEST(Ft32UartDmaSourceTest, WriterAndRecoveryUseDistinctOwnershipPaths)
{
    const std::string source = readSource("../platform/FT32/serial_uart_stdperiph.c");
    const std::string writer = functionBody(source, "void uartTryStartTxDMA(uartPort_t *s)");
    const std::string recovery = functionBody(source, "void uartTryRecoverTxDMA(uartPort_t *s)");
    ASSERT_FALSE(writer.empty());
    ASSERT_FALSE(recovery.empty());
    EXPECT_NE(std::string::npos, writer.find("uartTryStartTxDMAInternal(s, false);"));
    EXPECT_NE(std::string::npos, recovery.find("uartTryStartTxDMAInternal(s, true);"));

    const std::string irqSource = readSource("../platform/FT32/serial_uart_ft32f4xx.c");
    const std::string handoff = functionBody(irqSource, "static void handleUsartTxDma(uartPort_t *s)");
    ASSERT_FALSE(handoff.empty());
    EXPECT_NE(std::string::npos, handoff.find("uartTryRecoverTxDMA(s);"));
    EXPECT_EQ(std::string::npos, handoff.find("uartTxMonitor"));
}

TEST(Ft32UartDmaSourceTest, ReconfigurePublishesRecoveryBeforeStopping)
{
    const std::string source = readSource("../platform/FT32/serial_uart_stdperiph.c");
    const std::string wrapper = functionBody(source, "void uartReconfigure(uartPort_t *uartPort)");
    const std::string reconfigure = functionBody(source, "static void uartReconfigureInternal(uartPort_t *uartPort)");
    ASSERT_FALSE(wrapper.empty());
    ASSERT_FALSE(reconfigure.empty());

    expectOrdered(wrapper, {
        "if (uartPort->txDMAResource)",
        "ATOMIC_BLOCK(NVIC_PRIO_SERIALUART_TXDMA)",
        "uartReconfigureInternal(uartPort);",
    });

    expectOrdered(reconfigure, {
        "uartPort->txDMARecoveryPending = true;",
        "ft32UartDMATxEnable_Cmd(USARTx, DISABLE);",
        "ft32DmaRequestDisable((DMA_ARCH_TYPE *)uartPort->txDMAResource);",
        "xDMA_Cmd(uartPort->txDMAResource, DISABLE);",
        "ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)uartPort->txDMAResource)",
        "xDMA_DeInit(uartPort->txDMAResource);",
        "xDMA_Init(uartPort->txDMAResource, &ft32_dma_init);",
        "ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)uartPort->txDMAResource)",
        "xDMA_ITConfig(uartPort->txDMAResource, DMA_IT_TFR | DMA_IT_ERR, ENABLE);",
        "uartTryRecoverTxDMA(uartPort);",
    });
    EXPECT_EQ(std::string::npos, reconfigure.find("uartPort->txDMARecoveryPending = false;"));
    EXPECT_EQ(std::string::npos, reconfigure.find("ft32UartDMATxEnable_Cmd(USARTx, ENABLE);"));
    EXPECT_NE(std::string::npos, reconfigure.find("uartPort->txDMAResource && !(uartPort->port.mode & MODE_TX)"));
}

TEST(Ft32UartDmaModelTest, NoTerminalFlagIsStrictNoOp)
{
    RecoveryModel model;
    terminalIrq(model, 0U);
    EXPECT_TRUE(model.producerEnabled);
    EXPECT_TRUE(model.channelEnabled);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_EQ(0U, model.disableRequests);
    EXPECT_EQ(0U, model.channelReads);
    EXPECT_EQ(0U, model.observedAck);
}

TEST(Ft32UartDmaModelTest, StickyTerminalPublishesAndAcknowledgesExactSnapshot)
{
    const unsigned terminals[] = {TFR, ERR, TFR | ERR};
    for (const unsigned terminal : terminals) {
        RecoveryModel model;
        model.stickyDisable = true;
        model.head = 7U;
        model.tail = 3U;

        terminalIrq(model, terminal);

        EXPECT_FALSE(model.producerEnabled);
        EXPECT_TRUE(model.channelEnabled);
        EXPECT_TRUE(model.recoveryPending);
        EXPECT_EQ(terminal, model.observedAck);
        EXPECT_EQ(1U, model.disableRequests);
        EXPECT_EQ(1U, model.channelReads);
        EXPECT_EQ(0U, model.counterWrites);
        EXPECT_EQ(3U, model.tail);
        EXPECT_EQ(0U, model.rearms);
    }
}

TEST(Ft32UartDmaModelTest, StickyServiceIsOneShotPerTickThenAutoResumesWithoutWrite)
{
    RecoveryModel model;
    model.stickyDisable = true;
    model.head = 9U;
    model.tail = 4U;
    terminalIrq(model, ERR);

    const unsigned disableBefore = model.disableRequests;
    const unsigned readsBefore = model.channelReads;
    recoverOnce(model);
    EXPECT_EQ(disableBefore + 1U, model.disableRequests);
    EXPECT_EQ(readsBefore + 1U, model.channelReads);
    EXPECT_TRUE(model.recoveryPending);
    EXPECT_EQ(4U, model.tail);
    EXPECT_EQ(0U, model.counterWrites);

    model.stickyDisable = false;
    recoverOnce(model);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_TRUE(model.producerEnabled);
    EXPECT_TRUE(model.channelEnabled);
    EXPECT_EQ(9U, model.tail);
    EXPECT_EQ(1U, model.counterWrites);
    EXPECT_EQ(1U, model.rearms);
}

TEST(Ft32UartDmaModelTest, EmptyErrorPublishesTruthfulStoppedState)
{
    RecoveryModel model;
    model.stickyDisable = true;
    model.head = 5U;
    model.tail = 5U;
    terminalIrq(model, ERR);

    model.stickyDisable = false;
    recoverOnce(model);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_FALSE(model.producerEnabled);
    EXPECT_FALSE(model.channelEnabled);
    EXPECT_TRUE(model.txEmpty);
    EXPECT_EQ(1U, model.counterWrites);
    EXPECT_EQ(0U, model.rearms);
}

TEST(Ft32UartDmaModelTest, PendingWriterCannotConsumeRecoveryOwnership)
{
    RecoveryModel model;
    model.channelEnabled = false;
    model.recoveryPending = true;
    model.head = 8U;
    model.tail = 2U;

    writerTryStart(model);
    EXPECT_TRUE(model.recoveryPending);
    EXPECT_EQ(2U, model.tail);
    EXPECT_TRUE(model.producerEnabled);
    EXPECT_EQ(0U, model.disableRequests);
    EXPECT_EQ(0U, model.channelReads);
    EXPECT_EQ(0U, model.counterWrites);
    EXPECT_EQ(0U, model.rearms);
}

TEST(Ft32UartDmaModelTest, ReconfigureStickyStopRetainsRecoveryOwnership)
{
    RecoveryModel model;
    model.stickyDisable = true;
    model.head = 8U;
    model.tail = 2U;

    reconfigureTx(model);
    EXPECT_TRUE(model.recoveryPending);
    EXPECT_FALSE(model.producerEnabled);
    EXPECT_TRUE(model.channelEnabled);
    EXPECT_EQ(2U, model.tail);
    EXPECT_EQ(0U, model.counterWrites);
    EXPECT_EQ(0U, model.rearms);
}

TEST(Ft32UartDmaModelTest, ReconfigureEmptyPublishesTruthfulStoppedState)
{
    RecoveryModel model;
    model.head = 5U;
    model.tail = 5U;

    reconfigureTx(model);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_FALSE(model.producerEnabled);
    EXPECT_FALSE(model.channelEnabled);
    EXPECT_TRUE(model.txEmpty);
    EXPECT_EQ(1U, model.counterWrites);
    EXPECT_EQ(0U, model.rearms);
}

TEST(Ft32UartDmaModelTest, ReconfigureQueuedSuffixCommitsNewActiveState)
{
    RecoveryModel model;
    model.head = 9U;
    model.tail = 4U;

    reconfigureTx(model);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_TRUE(model.producerEnabled);
    EXPECT_TRUE(model.channelEnabled);
    EXPECT_FALSE(model.txEmpty);
    EXPECT_EQ(9U, model.tail);
    EXPECT_EQ(1U, model.counterWrites);
    EXPECT_EQ(1U, model.rearms);
}

TEST(Ft32UartDmaModelTest, RemovingTxModeStopsWithoutConsumingQueuedSuffix)
{
    RecoveryModel model;
    model.txModeEnabled = false;
    model.head = 9U;
    model.tail = 4U;

    reconfigureTx(model);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_FALSE(model.producerEnabled);
    EXPECT_FALSE(model.channelEnabled);
    EXPECT_FALSE(model.txEmpty);
    EXPECT_EQ(4U, model.tail);
    EXPECT_EQ(1U, model.counterWrites);
    EXPECT_EQ(0U, model.rearms);
}

TEST(Ft32UartDmaModelTest, RemovingTxModeConvergesAfterStickyStop)
{
    RecoveryModel model;
    model.txModeEnabled = false;
    model.stickyDisable = true;
    model.head = 9U;
    model.tail = 4U;

    reconfigureTx(model);
    EXPECT_TRUE(model.recoveryPending);
    EXPECT_TRUE(model.channelEnabled);

    model.stickyDisable = false;
    recoverOnce(model);
    EXPECT_FALSE(model.recoveryPending);
    EXPECT_FALSE(model.producerEnabled);
    EXPECT_FALSE(model.channelEnabled);
    EXPECT_FALSE(model.txEmpty);
    EXPECT_EQ(4U, model.tail);
    EXPECT_EQ(0U, model.rearms);
}
