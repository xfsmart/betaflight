#include <array>
#include <cstdint>

#include <gtest/gtest.h>

extern "C" {
#include "ft32_usb_ep0_test_api.h"
}

namespace {

constexpr uint8_t kClassOutInterface = 0x21U;
constexpr uint8_t kClassInInterface = 0xA1U;
constexpr uint8_t kStandardOutDevice = 0x00U;
constexpr uint8_t kStandardInDevice = 0x80U;
constexpr uint8_t kSetLineCoding = 0x20U;
constexpr uint8_t kGetLineCoding = 0x21U;
constexpr uint8_t kDataIn67 = 0x31U;
constexpr uint8_t kDataIn64 = 0x32U;
constexpr uint8_t kDataOut70 = 0x33U;
constexpr uint8_t kSetAddress = 0x05U;
constexpr uint8_t kGetDescriptor = 0x06U;
constexpr uint8_t kGetConfiguration = 0x08U;
constexpr uint8_t kSetConfiguration = 0x09U;
constexpr uint8_t kStandardOutEndpoint = 0x02U;
constexpr uint8_t kStandardInEndpoint = 0x82U;
constexpr uint8_t kGetStatus = 0x00U;
constexpr uint8_t kClearFeature = 0x01U;
constexpr uint8_t kSetFeature = 0x03U;
constexpr uint8_t kEndpointHalt = 0x00U;

class Ft32UsbEp0Test : public testing::Test {
protected:
    void SetUp() override
    {
        ft32UsbEp0TestReset();
    }

    static void QueueClassSetup(uint8_t directionAndRecipient, uint8_t request,
                                uint16_t length)
    {
        ft32UsbEp0TestQueueSetup(directionAndRecipient, request, 0U, 0U, length);
        ft32UsbEp0TestHandleEp0();
    }

    static void ExpectControlErrorWithoutTransfer()
    {
        EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
        EXPECT_EQ(2U, ft32UsbEp0TestLlStallCount());
        EXPECT_EQ(1U, ft32UsbEp0TestInStall());
        EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
        EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());
        EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
    }
};

TEST_F(Ft32UsbEp0Test, SevenByteOutCompletesOnceAndUsesDataEndOnlyStatus)
{
    constexpr std::array<uint8_t, 7> payload = {0x00U, 0xC2U, 0x01U, 0x00U,
                                                 0x00U, 0x00U, 0x08U};
    ft32UsbEp0TestSetConfigured(1U);

    QueueClassSetup(kClassOutInterface, kSetLineCoding, payload.size());
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_OUT, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_DATA_OUT, ft32UsbEp0TestCoreState());
    EXPECT_EQ(1U, ft32UsbEp0TestOutPending());
    EXPECT_EQ(payload.size(), ft32UsbEp0TestOutLength());

    ft32UsbEp0TestQueuePacket(payload.data(), payload.size());
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(1U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestClassRxReadyCount());
    for (uint32_t i = 0U; i < payload.size(); i++) {
        EXPECT_EQ(payload[i], ft32UsbEp0TestRxByte(i));
    }
    EXPECT_EQ(0U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(FT32_USB_EP0_CSR_DATAEND, ft32UsbEp0TestCsr0());
    EXPECT_EQ(FT32_USB_EP0_PCD_STATUS_IN, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_STATUS_IN, ft32UsbEp0TestCoreState());

    ft32UsbEp0TestCompleteIn();
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
    EXPECT_EQ(1U, ft32UsbEp0TestDataInCallbackCount());
}

TEST_F(Ft32UsbEp0Test, SixtySevenByteInUsesExactlySixtyFourThenThree)
{
    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassInInterface, kDataIn67, 67U);

    ASSERT_EQ(64U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(FT32_USB_EP0_CSR_TXPKTRDY, ft32UsbEp0TestCsr0());
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_IN, ft32UsbEp0TestPcdState());
    for (uint32_t i = 0U; i < 64U; i++) {
        EXPECT_EQ(static_cast<uint8_t>(0x5AU ^ i), ft32UsbEp0TestTxByte(i));
    }

    ft32UsbEp0TestCompleteIn();
    ASSERT_EQ(67U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(FT32_USB_EP0_CSR_TXPKTRDY | FT32_USB_EP0_CSR_DATAEND,
              ft32UsbEp0TestCsr0());
    for (uint32_t i = 64U; i < 67U; i++) {
        EXPECT_EQ(static_cast<uint8_t>(0x5AU ^ i), ft32UsbEp0TestTxByte(i));
    }

    ft32UsbEp0TestCompleteIn();
    EXPECT_EQ(67U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(FT32_USB_EP0_PCD_STATUS_OUT, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_STATUS_OUT, ft32UsbEp0TestCoreState());
    EXPECT_EQ(2U, ft32UsbEp0TestDataInCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestClassTxSentCount());

    ft32UsbEp0TestQueuePacket(nullptr, 0U);
    ft32UsbEp0TestHandleEp0();
    EXPECT_EQ(FT32_USB_EP0_CSR_SRXPKTRDY | FT32_USB_EP0_CSR_DATAEND,
              ft32UsbEp0TestCsr0());
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, FinalShortInAndStatusOutCoalesceInOneInterrupt)
{
    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassInInterface, kDataIn67, 67U);
    ft32UsbEp0TestCompleteIn();
    ASSERT_EQ(67U, ft32UsbEp0TestTxLength());
    ASSERT_EQ(1U, ft32UsbEp0TestDataInCallbackCount());
    ASSERT_EQ(1U, ft32UsbEp0TestInPending());

    ft32UsbEp0TestQueuePacket(nullptr, 0U);
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(67U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(2U, ft32UsbEp0TestDataInCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestClassTxSentCount());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestInPending());
    EXPECT_EQ(FT32_USB_EP0_CSR_SRXPKTRDY | FT32_USB_EP0_CSR_DATAEND,
              ft32UsbEp0TestCsr0());
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, EarlyStatusOutSuppressesNextInPacket)
{
    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassInInterface, kDataIn67, 67U);
    ASSERT_EQ(64U, ft32UsbEp0TestTxLength());
    ASSERT_EQ(1U, ft32UsbEp0TestInPending());

    ft32UsbEp0TestQueuePacket(nullptr, 0U);
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(64U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(0U, ft32UsbEp0TestDataInCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(0U, ft32UsbEp0TestClassTxSentCount());
    EXPECT_EQ(0U, ft32UsbEp0TestInPending());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, EarlyStatusOutFlushesTailAlreadyQueuedByMiddleware)
{
    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassInInterface, kDataIn67, 67U);
    ft32UsbEp0TestCompleteIn();
    ASSERT_EQ(67U, ft32UsbEp0TestTxLength());
    ASSERT_EQ(1U, ft32UsbEp0TestDataInCallbackCount());
    ASSERT_EQ(1U, ft32UsbEp0TestInPending());
    ASSERT_EQ(67U, ft32UsbEp0TestInBufferOffset());
    ASSERT_EQ(0U, ft32UsbEp0TestFlushObserved());

    ft32UsbEp0TestSetCsr0AndCount(FT32_USB_EP0_CSR_TXPKTRDY |
                                 FT32_USB_EP0_CSR_RXPKTRDY, 0U);
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(67U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(1U, ft32UsbEp0TestDataInCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(0U, ft32UsbEp0TestClassTxSentCount());
    EXPECT_EQ(1U, ft32UsbEp0TestFlushObserved());
    EXPECT_EQ(0U, ft32UsbEp0TestInPending());
    EXPECT_EQ(1U, ft32UsbEp0TestInBufferIsNull());
    EXPECT_EQ(0U, ft32UsbEp0TestInLength());
    EXPECT_EQ(0U, ft32UsbEp0TestInCount());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, OutstandingInRetransmitStallsWithoutOverwritingFifoOrPointer)
{
    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassInInterface, kDataIn67, 67U);
    ASSERT_EQ(64U, ft32UsbEp0TestTxLength());
    ASSERT_EQ(64U, ft32UsbEp0TestInBufferOffset());
    ASSERT_EQ(1U, ft32UsbEp0TestInPending());

    ft32UsbEp0TestRetransmit(128U, 1U);

    EXPECT_EQ(64U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(UINT32_MAX, ft32UsbEp0TestInBufferOffset());
    EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CSR_SDSTALL, ft32UsbEp0TestCsr0());
    EXPECT_EQ(1U, ft32UsbEp0TestInStall());
    EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestDataInCallbackCount());
}

TEST_F(Ft32UsbEp0Test, MaxPacketAlignedDataUsesTxReadyDataEndZlp)
{
    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassInInterface, kDataIn64, 67U);
    ASSERT_EQ(64U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(FT32_USB_EP0_CSR_TXPKTRDY, ft32UsbEp0TestCsr0());

    ft32UsbEp0TestCompleteIn();
    EXPECT_EQ(64U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(FT32_USB_EP0_CSR_TXPKTRDY | FT32_USB_EP0_CSR_DATAEND,
              ft32UsbEp0TestCsr0());
    EXPECT_EQ(1U, ft32UsbEp0TestInPending());

    ft32UsbEp0TestCompleteIn();
    EXPECT_EQ(FT32_USB_EP0_PCD_STATUS_OUT, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_STATUS_OUT, ft32UsbEp0TestCoreState());
    EXPECT_EQ(2U, ft32UsbEp0TestDataInCallbackCount());
}

TEST_F(Ft32UsbEp0Test, DataInZlpAndStatusOutCoalesceInOneInterrupt)
{
    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassInInterface, kDataIn64, 67U);
    ft32UsbEp0TestCompleteIn();
    ASSERT_EQ(64U, ft32UsbEp0TestTxLength());
    ASSERT_EQ(1U, ft32UsbEp0TestDataInCallbackCount());
    ASSERT_EQ(1U, ft32UsbEp0TestInPending());
    ASSERT_EQ(FT32_USB_EP0_CSR_TXPKTRDY | FT32_USB_EP0_CSR_DATAEND,
              ft32UsbEp0TestCsr0());

    ft32UsbEp0TestQueuePacket(nullptr, 0U);
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(64U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(2U, ft32UsbEp0TestDataInCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestClassTxSentCount());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, SetAddressCommitsOnlyAfterStatusInCompletion)
{
    ft32UsbEp0TestQueueSetup(0x00U, kSetAddress, 9U, 0U, 0U);
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(1U, ft32UsbEp0TestPendingAddress());
    EXPECT_EQ(9U, ft32UsbEp0TestPcdAddress());
    EXPECT_EQ(0U, ft32UsbEp0TestHardwareAddress());
    EXPECT_EQ(0U, ft32UsbEp0TestCoreAddress());
    EXPECT_EQ(1U, ft32UsbEp0TestCoreDeviceState());
    EXPECT_EQ(FT32_USB_EP0_CSR_DATAEND, ft32UsbEp0TestCsr0());
    EXPECT_EQ(FT32_USB_EP0_PCD_STATUS_IN, ft32UsbEp0TestPcdState());

    ft32UsbEp0TestCompleteIn();
    EXPECT_EQ(0U, ft32UsbEp0TestPendingAddress());
    EXPECT_EQ(9U, ft32UsbEp0TestHardwareAddress());
    EXPECT_EQ(9U, ft32UsbEp0TestCoreAddress());
    EXPECT_EQ(2U, ft32UsbEp0TestCoreDeviceState());
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, StatusInCompletionPrecedesCoalescedNextSetup)
{
    ft32UsbEp0TestQueueSetup(0x00U, kSetAddress, 9U, 0U, 0U);
    ft32UsbEp0TestHandleEp0();
    ASSERT_EQ(1U, ft32UsbEp0TestPendingAddress());
    ASSERT_EQ(FT32_USB_EP0_PCD_STATUS_IN, ft32UsbEp0TestPcdState());
    ASSERT_EQ(FT32_USB_EP0_CSR_DATAEND, ft32UsbEp0TestCsr0());

    ft32UsbEp0TestClearEvents();
    ft32UsbEp0TestQueueSetup(kClassOutInterface, kSetLineCoding, 0U, 0U, 7U);
    ft32UsbEp0TestHandleEp0();

    ASSERT_EQ(3U, ft32UsbEp0TestEventCount());
    EXPECT_EQ(FT32_USB_EP0_EVENT_ADDRESS, ft32UsbEp0TestEvent(0U));
    EXPECT_EQ(FT32_USB_EP0_EVENT_DATA_IN, ft32UsbEp0TestEvent(1U));
    EXPECT_EQ(FT32_USB_EP0_EVENT_SETUP, ft32UsbEp0TestEvent(2U));
    EXPECT_EQ(1U, ft32UsbEp0TestAddressCallbackCount());
    EXPECT_EQ(9U, ft32UsbEp0TestAddressSeenByCallback());
    EXPECT_EQ(1U, ft32UsbEp0TestDataInCallbackCount());
    EXPECT_EQ(2U, ft32UsbEp0TestSetupCallbackCount());
    EXPECT_EQ(8U, ft32UsbEp0TestRxConsumed());
    EXPECT_EQ(9U, ft32UsbEp0TestHardwareAddress());
    EXPECT_EQ(9U, ft32UsbEp0TestCoreAddress());
    EXPECT_EQ(2U, ft32UsbEp0TestCoreDeviceState());
    EXPECT_EQ(0U, ft32UsbEp0TestPendingAddress());
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_OUT, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_DATA_OUT, ft32UsbEp0TestCoreState());
    EXPECT_EQ(1U, ft32UsbEp0TestOutPending());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestFlushObserved());
}

TEST_F(Ft32UsbEp0Test, DataEndStillSetDoesNotMiscompleteStatusIn)
{
    ft32UsbEp0TestQueueSetup(0x00U, kSetAddress, 9U, 0U, 0U);
    ft32UsbEp0TestHandleEp0();
    ASSERT_EQ(FT32_USB_EP0_PCD_STATUS_IN, ft32UsbEp0TestPcdState());

    ft32UsbEp0TestClearEvents();
    ft32UsbEp0TestQueueSetup(kClassOutInterface, kSetLineCoding, 0U, 0U, 7U);
    ft32UsbEp0TestSetCsr0AndCount(FT32_USB_EP0_CSR_DATAEND |
                                 FT32_USB_EP0_CSR_RXPKTRDY, 8U);
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(0U, ft32UsbEp0TestEventCount());
    EXPECT_EQ(0U, ft32UsbEp0TestAddressCallbackCount());
    EXPECT_EQ(0U, ft32UsbEp0TestDataInCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestSetupCallbackCount());
    EXPECT_EQ(0U, ft32UsbEp0TestHardwareAddress());
    EXPECT_EQ(0U, ft32UsbEp0TestCoreAddress());
}

TEST_F(Ft32UsbEp0Test, SetupEndAbortsOldTransferWithoutConsumingQueuedSetup)
{
    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassInInterface, kDataIn67, 67U);
    ASSERT_EQ(FT32_USB_EP0_PCD_DATA_IN, ft32UsbEp0TestPcdState());

    ft32UsbEp0TestQueueSetup(kClassOutInterface, kSetLineCoding, 0U, 0U, 7U);
    ft32UsbEp0TestSetSetupEndWithQueuedPacket();
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(0U, ft32UsbEp0TestRxConsumed());
    EXPECT_EQ(0U, ft32UsbEp0TestCsr02());
    EXPECT_EQ(FT32_USB_EP0_CSR_SSETUPEND, ft32UsbEp0TestCsr0());
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(0U, ft32UsbEp0TestInPending());

    // The host model exposes the RXPKTRDY bit again after the RC_W0
    // SETUPEND acknowledgement, as the peripheral does on the board.
    ft32UsbEp0TestPresentQueuedPacket();
    ft32UsbEp0TestHandleEp0();
    EXPECT_EQ(8U, ft32UsbEp0TestRxConsumed());
    EXPECT_EQ(2U, ft32UsbEp0TestSetupCallbackCount());
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_OUT, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_DATA_OUT, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, SentStallClearsSoftwareStateAndNextSetupRecovers)
{
    const std::array<uint8_t, 7> malformed = {};
    ft32UsbEp0TestSetConfigured(1U);
    ft32UsbEp0TestQueuePacket(malformed.data(), malformed.size());
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
    EXPECT_EQ(1U, ft32UsbEp0TestInStall());
    EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(FT32_USB_EP0_CSR_SDSTALL, ft32UsbEp0TestCsr0());
    EXPECT_EQ(0U, ft32UsbEp0TestSetupCallbackCount());

    ft32UsbEp0TestSignalSentStall();
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());

    QueueClassSetup(kClassOutInterface, kSetLineCoding, 7U);
    EXPECT_EQ(1U, ft32UsbEp0TestSetupCallbackCount());
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_OUT, ft32UsbEp0TestPcdState());
}

TEST_F(Ft32UsbEp0Test, BusResetClearsControlTransferAndAddressState)
{
    ft32UsbEp0TestDirtyState();
    ft32UsbEp0TestSignalReset();

    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
    EXPECT_EQ(1U, ft32UsbEp0TestCoreDeviceState());
    EXPECT_EQ(0U, ft32UsbEp0TestPcdAddress());
    EXPECT_EQ(0U, ft32UsbEp0TestHardwareAddress());
    EXPECT_EQ(0U, ft32UsbEp0TestPendingAddress());
    EXPECT_EQ(0U, ft32UsbEp0TestInPending());
    EXPECT_EQ(0U, ft32UsbEp0TestOutPending());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestInLength());
    EXPECT_EQ(0U, ft32UsbEp0TestInCount());
    EXPECT_EQ(0U, ft32UsbEp0TestOutLength());
    EXPECT_EQ(0U, ft32UsbEp0TestOutCount());
    EXPECT_EQ(1U, ft32UsbEp0TestInBufferIsNull());
    EXPECT_EQ(1U, ft32UsbEp0TestOutBufferIsNull());
}

TEST_F(Ft32UsbEp0Test, RejectsSetupPacketsThatAreNotExactlyEightBytes)
{
    std::array<uint8_t, 9> malformed = {};

    for (const uint8_t length : {7U, 9U}) {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestQueuePacket(malformed.data(), length);
        ft32UsbEp0TestHandleEp0();
        EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
        EXPECT_EQ(0U, ft32UsbEp0TestSetupCallbackCount());
        EXPECT_EQ(0U, ft32UsbEp0TestRxConsumed());
        EXPECT_NE(0U, ft32UsbEp0TestCsr02());
    }
}

TEST_F(Ft32UsbEp0Test, RejectsNullReceiveBufferForNonzeroData)
{
    ft32UsbEp0TestReceiveNull(7U);
    EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
    EXPECT_EQ(1U, ft32UsbEp0TestInStall());
    EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
}

TEST_F(Ft32UsbEp0Test, OutstandingOutRearmStallsWithoutReplacingBuffer)
{
    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassOutInterface, kSetLineCoding, 7U);
    ASSERT_EQ(1U, ft32UsbEp0TestOutPending());
    ASSERT_EQ(7U, ft32UsbEp0TestOutLength());
    ASSERT_EQ(0U, ft32UsbEp0TestOutBufferOffset());

    ft32UsbEp0TestReceiveBuffer(32U, 3U);

    EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
    EXPECT_EQ(1U, ft32UsbEp0TestInStall());
    EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutPending());
    EXPECT_EQ(0U, ft32UsbEp0TestOutLength());
    EXPECT_EQ(0U, ft32UsbEp0TestOutCount());
    EXPECT_EQ(1U, ft32UsbEp0TestOutBufferIsNull());
    EXPECT_EQ(UINT32_MAX, ft32UsbEp0TestOutBufferOffset());
    EXPECT_EQ(0U, ft32UsbEp0TestDataOutCallbackCount());
}

TEST_F(Ft32UsbEp0Test, RejectsNonNullBufferForZeroLengthEp0Transfer)
{
    ft32UsbEp0TestReceiveBuffer(0U, 0U);
    EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
    EXPECT_EQ(1U, ft32UsbEp0TestInStall());
    EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutPending());
    EXPECT_EQ(1U, ft32UsbEp0TestOutBufferIsNull());

    ft32UsbEp0TestReset();
    ft32UsbEp0TestRetransmit(0U, 0U);
    EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
    EXPECT_EQ(1U, ft32UsbEp0TestInStall());
    EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestInPending());
    EXPECT_EQ(1U, ft32UsbEp0TestInBufferIsNull());
    EXPECT_EQ(0U, ft32UsbEp0TestTxLength());
}

TEST_F(Ft32UsbEp0Test, RepeatedPrematureStatusOutArmRemainsLegal)
{
    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassInInterface, kDataIn67, 67U);
    ft32UsbEp0TestCompleteIn();
    ASSERT_EQ(FT32_USB_EP0_PCD_DATA_IN, ft32UsbEp0TestPcdState());
    ASSERT_EQ(1U, ft32UsbEp0TestInPending());
    ASSERT_EQ(1U, ft32UsbEp0TestOutPending());
    ASSERT_EQ(0U, ft32UsbEp0TestOutLength());
    ASSERT_EQ(1U, ft32UsbEp0TestOutBufferIsNull());

    ft32UsbEp0TestReceiveNull(0U);

    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_IN, ft32UsbEp0TestPcdState());
    EXPECT_EQ(1U, ft32UsbEp0TestInPending());
    EXPECT_EQ(1U, ft32UsbEp0TestOutPending());
    EXPECT_EQ(0U, ft32UsbEp0TestOutLength());
    EXPECT_EQ(1U, ft32UsbEp0TestOutBufferIsNull());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());
}

TEST_F(Ft32UsbEp0Test, RejectsShortAndOversizedOutPackets)
{
    std::array<uint8_t, 8> payload = {};

    for (const uint8_t length : {6U, 8U}) {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestSetConfigured(1U);
        QueueClassSetup(kClassOutInterface, kSetLineCoding, 7U);
        ft32UsbEp0TestQueuePacket(payload.data(), length);
        ft32UsbEp0TestHandleEp0();
        EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
        EXPECT_EQ(0U, ft32UsbEp0TestDataOutCallbackCount());
        EXPECT_EQ(0U, ft32UsbEp0TestClassRxReadyCount());
    }
}

TEST_F(Ft32UsbEp0Test, RejectsImpossibleInTransferCounters)
{
    ft32UsbEp0TestCorruptInCounts(4U, 5U, 1U, 1U);
    ft32UsbEp0TestHandleEp0();
    EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());

    ft32UsbEp0TestReset();
    ft32UsbEp0TestCorruptInCounts(10U, 0U, 3U, 4U);
    ft32UsbEp0TestHandleEp0();
    EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
}

TEST_F(Ft32UsbEp0Test, MultiPacketOutUsesSixtyFourThenRemainingBytes)
{
    std::array<uint8_t, 64> first = {};
    std::array<uint8_t, 6> second = {};
    for (uint32_t i = 0U; i < first.size(); i++) {
        first[i] = static_cast<uint8_t>(i);
    }
    for (uint32_t i = 0U; i < second.size(); i++) {
        second[i] = static_cast<uint8_t>(0xA0U + i);
    }

    ft32UsbEp0TestSetConfigured(1U);
    QueueClassSetup(kClassOutInterface, kDataOut70, 70U);
    EXPECT_EQ(64U, ft32UsbEp0TestOutLength());

    ft32UsbEp0TestQueuePacket(first.data(), first.size());
    ft32UsbEp0TestHandleEp0();
    EXPECT_EQ(1U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(0U, ft32UsbEp0TestClassRxReadyCount());
    EXPECT_EQ(6U, ft32UsbEp0TestOutLength());
    EXPECT_EQ(1U, ft32UsbEp0TestOutPending());

    ft32UsbEp0TestQueuePacket(second.data(), second.size());
    ft32UsbEp0TestHandleEp0();
    EXPECT_EQ(2U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestClassRxReadyCount());
    EXPECT_EQ(FT32_USB_EP0_PCD_STATUS_IN, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CSR_DATAEND, ft32UsbEp0TestCsr0());
    for (uint32_t i = 0U; i < first.size(); i++) {
        EXPECT_EQ(first[i], ft32UsbEp0TestRxByte(i));
    }
    for (uint32_t i = 0U; i < second.size(); i++) {
        EXPECT_EQ(second[i], ft32UsbEp0TestRxByte(first.size() + i));
    }
}

TEST_F(Ft32UsbEp0Test, InterfaceRequestsRejectUnownedAndStandardHighByteIndexes)
{
    struct RequestShape {
        uint8_t bmRequest;
        uint16_t index;
    };
    constexpr std::array<RequestShape, 2> invalidShapes = {{
        {kClassOutInterface, 0x0001U},
        {0x01U, 0x0100U},
    }};

    for (const RequestShape &shape : invalidShapes) {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestSetConfigured(1U);

        EXPECT_EQ(FT32_USB_EP0_USBD_FAIL,
                  ft32UsbEp0TestSubmitInterfaceRequest(
                      shape.bmRequest, kSetLineCoding, 0U,
                      shape.index, 0U));
        EXPECT_EQ(2U, ft32UsbEp0TestLlStallCount());
        EXPECT_EQ(1U, ft32UsbEp0TestInStall());
        EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
        EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());
        EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
        EXPECT_EQ(0U, ft32UsbEp0TestClassSetupCount());
    }
}

TEST_F(Ft32UsbEp0Test, GenericClassRequestRoutesByLowInterfaceByte)
{
    ft32UsbEp0TestSetConfigured(1U);

    EXPECT_EQ(FT32_USB_EP0_USBD_OK,
              ft32UsbEp0TestSubmitInterfaceRequest(
                  kClassOutInterface, kSetLineCoding,
                  0U, 0x0100U, 7U));
    EXPECT_EQ(1U, ft32UsbEp0TestClassSetupCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlPrepareReceiveCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_OUT, ft32UsbEp0TestPcdState());
}

TEST_F(Ft32UsbEp0Test, EndpointFeatureRequestsRejectMalformedAndUnavailableTargets)
{
    struct RequestShape {
        uint8_t bmRequest;
        uint16_t value;
        uint16_t index;
        uint16_t length;
    };
    constexpr std::array<RequestShape, 6> invalidShapes = {{
        {kStandardInEndpoint, kEndpointHalt, 0x81U, 0U},
        {kStandardOutEndpoint, 1U, 0x81U, 0U},
        {kStandardOutEndpoint, kEndpointHalt, 0x81U, 1U},
        {kStandardOutEndpoint, kEndpointHalt, 0x00U, 0U},
        {kStandardOutEndpoint, kEndpointHalt, 0x80U, 0U},
        {kStandardOutEndpoint, kEndpointHalt, 0x82U, 0U},
    }};

    for (const uint8_t request : {kSetFeature, kClearFeature}) {
        for (const RequestShape &shape : invalidShapes) {
            ft32UsbEp0TestReset();
            ft32UsbEp0TestSetConfigured(1U);
            ft32UsbEp0TestSetEndpointUsed(0x81U, 1U);

            EXPECT_EQ(FT32_USB_EP0_USBD_FAIL,
                      ft32UsbEp0TestSubmitEndpointRequest(
                          shape.bmRequest, request, shape.value,
                          shape.index, shape.length));
            EXPECT_EQ(2U, ft32UsbEp0TestLlStallCount());
            EXPECT_EQ(1U, ft32UsbEp0TestInStall());
            EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
            EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());
            EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
            EXPECT_EQ(0U, ft32UsbEp0TestLlClearStallCount());
            EXPECT_EQ(0U, ft32UsbEp0TestClassSetupCount());
        }
    }
}

TEST_F(Ft32UsbEp0Test, ConfiguredNonzeroEndpointAcceptsSetClearAndGetStatus)
{
    ft32UsbEp0TestSetConfigured(1U);
    ft32UsbEp0TestSetEndpointUsed(0x81U, 1U);

    EXPECT_EQ(FT32_USB_EP0_USBD_OK,
              ft32UsbEp0TestSubmitEndpointRequest(
                  kStandardOutEndpoint, kSetFeature, kEndpointHalt, 0x81U, 0U));
    EXPECT_EQ(1U, ft32UsbEp0TestEndpointStalled(0x81U));
    EXPECT_EQ(1U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitLength());
    EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());

    ft32UsbEp0TestReset();
    ft32UsbEp0TestSetConfigured(1U);
    ft32UsbEp0TestSetEndpointUsed(0x81U, 1U);
    ft32UsbEp0TestSetEndpointStalled(0x81U, 1U);

    EXPECT_EQ(FT32_USB_EP0_USBD_OK,
              ft32UsbEp0TestSubmitEndpointRequest(
                  kStandardOutEndpoint, kClearFeature, kEndpointHalt, 0x81U, 0U));
    EXPECT_EQ(0U, ft32UsbEp0TestEndpointStalled(0x81U));
    EXPECT_EQ(0U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlClearStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitLength());
    EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
    EXPECT_EQ(1U, ft32UsbEp0TestClassSetupCount());

    ft32UsbEp0TestReset();
    ft32UsbEp0TestSetConfigured(1U);
    ft32UsbEp0TestSetEndpointUsed(0x81U, 1U);
    ft32UsbEp0TestSetEndpointStalled(0x81U, 1U);

    EXPECT_EQ(FT32_USB_EP0_USBD_OK,
              ft32UsbEp0TestSubmitEndpointRequest(
                  kStandardInEndpoint, kGetStatus, 0U, 0x81U, 2U));
    EXPECT_EQ(0U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(2U, ft32UsbEp0TestLlTransmitLength());
    ASSERT_EQ(2U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(1U, ft32UsbEp0TestTxByte(0U));
    EXPECT_EQ(0U, ft32UsbEp0TestTxByte(1U));
    EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
}

TEST_F(Ft32UsbEp0Test, ClearFeatureRejectsMissingOrNullEndpointOwnerBeforeStatus)
{
    for (const bool nullOwnerClass : {false, true}) {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestSetConfigured(1U);
        ft32UsbEp0TestSetEndpointUsed(0x81U, 1U);
        ft32UsbEp0TestSetEndpointStalled(0x81U, 1U);
        if (nullOwnerClass) {
            ft32UsbEp0TestSetOwnerClassPresent(0U);
        } else {
            ft32UsbEp0TestSetEndpointOwnerPresent(0U);
        }

        EXPECT_EQ(FT32_USB_EP0_USBD_FAIL,
                  ft32UsbEp0TestSubmitEndpointRequest(
                      kStandardOutEndpoint, kClearFeature,
                      kEndpointHalt, 0x81U, 0U));
        EXPECT_EQ(2U, ft32UsbEp0TestLlStallCount());
        EXPECT_EQ(1U, ft32UsbEp0TestInStall());
        EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
        EXPECT_EQ(1U, ft32UsbEp0TestEndpointStalled(0x81U));
        EXPECT_EQ(0U, ft32UsbEp0TestLlClearStallCount());
        EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());
        EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
        EXPECT_EQ(0U, ft32UsbEp0TestClassSetupCount());
    }
}

TEST_F(Ft32UsbEp0Test, ClearFeatureRestoresHaltWhenOwnerSetupRejectsRequest)
{
    ft32UsbEp0TestSetConfigured(1U);
    ft32UsbEp0TestSetEndpointUsed(0x81U, 1U);
    ft32UsbEp0TestSetEndpointStalled(0x81U, 1U);
    ft32UsbEp0TestSetOwnerSetupResult(FT32_USB_EP0_USBD_FAIL);

    EXPECT_EQ(FT32_USB_EP0_USBD_FAIL,
              ft32UsbEp0TestSubmitEndpointRequest(
                  kStandardOutEndpoint, kClearFeature,
                  kEndpointHalt, 0x81U, 0U));
    EXPECT_EQ(1U, ft32UsbEp0TestLlClearStallCount());
    EXPECT_EQ(3U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestEndpointStalled(0x81U));
    EXPECT_EQ(1U, ft32UsbEp0TestInStall());
    EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
    EXPECT_EQ(1U, ft32UsbEp0TestClassSetupCount());
}

TEST_F(Ft32UsbEp0Test, ClearFeatureAcceptsOwnerWithoutOptionalSetupCallback)
{
    ft32UsbEp0TestSetConfigured(1U);
    ft32UsbEp0TestSetEndpointUsed(0x81U, 1U);
    ft32UsbEp0TestSetEndpointStalled(0x81U, 1U);
    ft32UsbEp0TestSetOwnerSetupPresent(0U);

    EXPECT_EQ(FT32_USB_EP0_USBD_OK,
              ft32UsbEp0TestSubmitEndpointRequest(
                  kStandardOutEndpoint, kClearFeature,
                  kEndpointHalt, 0x81U, 0U));
    EXPECT_EQ(0U, ft32UsbEp0TestEndpointStalled(0x81U));
    EXPECT_EQ(0U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlClearStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitLength());
    EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
    EXPECT_EQ(0U, ft32UsbEp0TestClassSetupCount());
}

TEST_F(Ft32UsbEp0Test, EndpointGetStatusRejectsMalformedAndUnusedTargets)
{
    struct RequestShape {
        uint8_t bmRequest;
        uint16_t value;
        uint16_t index;
        uint16_t length;
    };
    constexpr std::array<RequestShape, 5> invalidShapes = {{
        {kStandardOutEndpoint, 0U, 0x81U, 2U},
        {kStandardInEndpoint, 0U, 0x81U, 1U},
        {kStandardInEndpoint, 0U, 0x0181U, 2U},
        {kStandardInEndpoint, 1U, 0x81U, 2U},
        {kStandardInEndpoint, 0U, 0x82U, 2U},
    }};

    for (const RequestShape &shape : invalidShapes) {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestSetConfigured(1U);
        ft32UsbEp0TestSetEndpointUsed(0x81U, 1U);

        EXPECT_EQ(FT32_USB_EP0_USBD_FAIL,
                  ft32UsbEp0TestSubmitEndpointRequest(
                      shape.bmRequest, kGetStatus, shape.value,
                      shape.index, shape.length));
        EXPECT_EQ(2U, ft32UsbEp0TestLlStallCount());
        EXPECT_EQ(1U, ft32UsbEp0TestInStall());
        EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
        EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());
        EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
        EXPECT_EQ(0U, ft32UsbEp0TestClassSetupCount());
    }
}

TEST_F(Ft32UsbEp0Test, CdcSetLineCodingConsumesExactlySevenBytes)
{
    constexpr std::array<uint8_t, 7> lineCoding = {
        0x00U, 0xC2U, 0x01U, 0x00U, 0x00U, 0x00U, 0x08U,
    };
    ft32UsbEp0TestUseCdcClass();
    ft32UsbEp0TestSetConfigured(1U);

    EXPECT_EQ(FT32_USB_EP0_USBD_OK,
              ft32UsbEp0TestSubmitInterfaceRequest(
                  kClassOutInterface, kSetLineCoding, 0U, 0U,
                  lineCoding.size()));
    EXPECT_EQ(1U, ft32UsbEp0TestLlPrepareReceiveCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestCdcControlCount());
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_OUT, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_DATA_OUT, ft32UsbEp0TestCoreState());

    ft32UsbEp0TestQueuePacket(lineCoding.data(), lineCoding.size());
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(1U, ft32UsbEp0TestCdcControlCount());
    EXPECT_EQ(kSetLineCoding, ft32UsbEp0TestCdcControlCommand());
    EXPECT_EQ(lineCoding.size(), ft32UsbEp0TestCdcControlLength());
    for (uint32_t i = 0U; i < lineCoding.size(); i++) {
        EXPECT_EQ(lineCoding[i], ft32UsbEp0TestCdcControlByte(i));
    }
    EXPECT_EQ(1U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitLength());
    EXPECT_EQ(0U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(FT32_USB_EP0_PCD_STATUS_IN, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_STATUS_IN, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, CdcGetLineCodingReturnsExactlySevenBytes)
{
    ft32UsbEp0TestUseCdcClass();
    ft32UsbEp0TestSetConfigured(1U);

    EXPECT_EQ(FT32_USB_EP0_USBD_OK,
              ft32UsbEp0TestSubmitInterfaceRequest(
                  kClassInInterface, kGetLineCoding, 0U, 0U, 7U));
    EXPECT_EQ(1U, ft32UsbEp0TestCdcControlCount());
    EXPECT_EQ(kGetLineCoding, ft32UsbEp0TestCdcControlCommand());
    EXPECT_EQ(7U, ft32UsbEp0TestCdcControlLength());
    EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(7U, ft32UsbEp0TestLlTransmitLength());
    EXPECT_EQ(0U, ft32UsbEp0TestLlStallCount());
    ASSERT_EQ(7U, ft32UsbEp0TestTxLength());
    for (uint32_t i = 0U; i < 7U; i++) {
        EXPECT_EQ(static_cast<uint8_t>(0xA0U + i),
                  ft32UsbEp0TestTxByte(i));
    }
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_IN, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_DATA_IN, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, CdcGetLineCodingCompletesExactlyOneStatusOut)
{
    ft32UsbEp0TestUseCdcClass();
    ft32UsbEp0TestSetConfigured(1U);

    EXPECT_EQ(FT32_USB_EP0_USBD_OK,
              ft32UsbEp0TestSubmitInterfaceRequest(
                  kClassInInterface, kGetLineCoding, 0U, 0U, 7U));
    ASSERT_EQ(7U, ft32UsbEp0TestTxLength());
    ASSERT_EQ(1U, ft32UsbEp0TestInPending());

    ft32UsbEp0TestCompleteIn();

    EXPECT_EQ(1U, ft32UsbEp0TestDataInCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlPrepareReceiveCount());
    EXPECT_EQ(1U, ft32UsbEp0TestOutPending());
    EXPECT_EQ(FT32_USB_EP0_PCD_STATUS_OUT, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_STATUS_OUT, ft32UsbEp0TestCoreState());

    ft32UsbEp0TestQueuePacket(nullptr, 0U);
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(1U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestCdcControlCount());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, CdcLineCodingRejectsWrongRecipient)
{
    constexpr uint8_t classOutDevice = 0x20U;
    ft32UsbEp0TestUseCdcClass();
    ft32UsbEp0TestSetConfigured(1U);

    EXPECT_EQ(FT32_USB_EP0_USBD_FAIL,
              ft32UsbEp0TestSubmitInterfaceRequest(
                  classOutDevice, kSetLineCoding, 0U, 0U, 7U));
    EXPECT_EQ(2U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestInStall());
    EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
    EXPECT_EQ(0U, ft32UsbEp0TestCdcControlCount());
}

TEST_F(Ft32UsbEp0Test, CdcLineCodingRejectsWrongInterfaceDirectionAndLength)
{
    struct RequestShape {
        uint8_t bmRequest;
        uint8_t request;
        uint16_t index;
        uint16_t length;
    };
    constexpr std::array<RequestShape, 6> invalidShapes = {{
        {kClassOutInterface, kSetLineCoding, 1U, 7U},
        {kClassOutInterface, kSetLineCoding, 0x0100U, 7U},
        {kClassInInterface, kSetLineCoding, 0U, 7U},
        {kClassOutInterface, kSetLineCoding, 0U, 6U},
        {kClassOutInterface, kGetLineCoding, 0U, 7U},
        {kClassInInterface, kGetLineCoding, 0U, 6U},
    }};

    for (const RequestShape &shape : invalidShapes) {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestUseCdcClass();
        ft32UsbEp0TestSetConfigured(1U);

        EXPECT_EQ(FT32_USB_EP0_USBD_FAIL,
                  ft32UsbEp0TestSubmitInterfaceRequest(
                      shape.bmRequest, shape.request, 0U,
                      shape.index, shape.length));
        EXPECT_EQ(2U, ft32UsbEp0TestLlStallCount());
        EXPECT_EQ(1U, ft32UsbEp0TestInStall());
        EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
        EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());
        EXPECT_EQ(0U, ft32UsbEp0TestLlPrepareReceiveCount());
        EXPECT_EQ(0U, ft32UsbEp0TestCdcControlCount());
    }
}

TEST_F(Ft32UsbEp0Test, SetAddressRejectsWrongDirectionWithoutCachingAddress)
{
    (void)ft32UsbEp0TestSubmitDeviceRequest(
        kStandardInDevice, kSetAddress, 9U, 0U, 0U);

    ExpectControlErrorWithoutTransfer();
    EXPECT_EQ(0U, ft32UsbEp0TestPendingAddress());
    EXPECT_EQ(0U, ft32UsbEp0TestPcdAddress());
    EXPECT_EQ(0U, ft32UsbEp0TestHardwareAddress());
    EXPECT_EQ(0U, ft32UsbEp0TestCoreAddress());
}

TEST_F(Ft32UsbEp0Test, SetConfigurationRejectsMalformedShape)
{
    struct RequestShape {
        uint8_t bmRequest;
        uint16_t value;
        uint16_t index;
        uint16_t length;
    };
    constexpr std::array<RequestShape, 4> invalidShapes = {{
        {kStandardInDevice, 1U, 0U, 0U},
        {kStandardOutDevice, 1U, 1U, 0U},
        {kStandardOutDevice, 1U, 0U, 1U},
        {kStandardOutDevice, 0x0101U, 0U, 0U},
    }};

    for (const RequestShape &shape : invalidShapes) {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestSetAddressed();

        EXPECT_EQ(FT32_USB_EP0_USBD_FAIL,
                  ft32UsbEp0TestSubmitDeviceRequest(
                      shape.bmRequest, kSetConfiguration,
                      shape.value, shape.index, shape.length));
        ExpectControlErrorWithoutTransfer();
        EXPECT_EQ(0U, ft32UsbEp0TestCoreConfiguration());
    }
}

TEST_F(Ft32UsbEp0Test, GetConfigurationRejectsDefaultStateAndMalformedShape)
{
    (void)ft32UsbEp0TestSubmitDeviceRequest(
        kStandardInDevice, kGetConfiguration, 0U, 0U, 1U);
    ExpectControlErrorWithoutTransfer();

    struct RequestShape {
        uint8_t bmRequest;
        uint16_t value;
        uint16_t index;
        uint16_t length;
    };
    constexpr std::array<RequestShape, 4> invalidShapes = {{
        {kStandardOutDevice, 0U, 0U, 1U},
        {kStandardInDevice, 1U, 0U, 1U},
        {kStandardInDevice, 0U, 1U, 1U},
        {kStandardInDevice, 0U, 0U, 2U},
    }};

    for (const RequestShape &shape : invalidShapes) {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestSetConfigured(1U);

        (void)ft32UsbEp0TestSubmitDeviceRequest(
            shape.bmRequest, kGetConfiguration, shape.value,
            shape.index, shape.length);
        ExpectControlErrorWithoutTransfer();
    }
}

TEST_F(Ft32UsbEp0Test, DeviceGetStatusRejectsMalformedShape)
{
    struct RequestShape {
        uint8_t bmRequest;
        uint16_t value;
        uint16_t index;
        uint16_t length;
    };
    constexpr std::array<RequestShape, 4> invalidShapes = {{
        {kStandardOutDevice, 0U, 0U, 2U},
        {kStandardInDevice, 1U, 0U, 2U},
        {kStandardInDevice, 0U, 1U, 2U},
        {kStandardInDevice, 0U, 0U, 1U},
    }};

    for (const RequestShape &shape : invalidShapes) {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestSetConfigured(1U);

        (void)ft32UsbEp0TestSubmitDeviceRequest(
            shape.bmRequest, kGetStatus, shape.value,
            shape.index, shape.length);
        ExpectControlErrorWithoutTransfer();
    }
}

TEST_F(Ft32UsbEp0Test, RemoteWakeupRequiresCapabilityAndConfiguredState)
{
    for (const uint8_t request : {kSetFeature, kClearFeature}) {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestSetConfigured(1U);
        ft32UsbEp0TestSetRemoteWakeupCapability(0U);
        ft32UsbEp0TestSetRemoteWakeup(request == kClearFeature ? 1U : 0U);

        (void)ft32UsbEp0TestSubmitDeviceRequest(
            kStandardOutDevice, request, 1U, 0U, 0U);
        ExpectControlErrorWithoutTransfer();
        EXPECT_EQ(request == kClearFeature ? 1U : 0U,
                  ft32UsbEp0TestRemoteWakeup());
    }

    for (const bool addressed : {false, true}) {
        for (const uint8_t request : {kSetFeature, kClearFeature}) {
            ft32UsbEp0TestReset();
            if (addressed) {
                ft32UsbEp0TestSetAddressed();
            }
            ft32UsbEp0TestSetRemoteWakeupCapability(1U);
            ft32UsbEp0TestSetRemoteWakeup(request == kClearFeature ? 1U : 0U);

            (void)ft32UsbEp0TestSubmitDeviceRequest(
                kStandardOutDevice, request, 1U, 0U, 0U);
            ExpectControlErrorWithoutTransfer();
            EXPECT_EQ(request == kClearFeature ? 1U : 0U,
                      ft32UsbEp0TestRemoteWakeup());
        }
    }

    ft32UsbEp0TestReset();
    ft32UsbEp0TestSetConfigured(1U);
    ft32UsbEp0TestSetRemoteWakeupCapability(1U);

    (void)ft32UsbEp0TestSubmitDeviceRequest(
        kStandardOutDevice, kSetFeature, 1U, 0U, 0U);
    EXPECT_EQ(1U, ft32UsbEp0TestRemoteWakeup());
    EXPECT_EQ(0U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitLength());

    ft32UsbEp0TestReset();
    ft32UsbEp0TestSetConfigured(1U);
    ft32UsbEp0TestSetRemoteWakeupCapability(1U);
    ft32UsbEp0TestSetRemoteWakeup(1U);

    (void)ft32UsbEp0TestSubmitDeviceRequest(
        kStandardOutDevice, kClearFeature, 1U, 0U, 0U);
    EXPECT_EQ(0U, ft32UsbEp0TestRemoteWakeup());
    EXPECT_EQ(0U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitLength());
}

TEST_F(Ft32UsbEp0Test, FullSpeedRejectsTestModeAndUnknownClearFeature)
{
    (void)ft32UsbEp0TestSubmitDeviceRequest(
        kStandardOutDevice, kSetFeature, 2U, 0x0100U, 0U);
    ExpectControlErrorWithoutTransfer();

    ft32UsbEp0TestReset();
    ft32UsbEp0TestSetConfigured(1U);
    ft32UsbEp0TestSetRemoteWakeupCapability(1U);
    ft32UsbEp0TestSetRemoteWakeup(1U);

    (void)ft32UsbEp0TestSubmitDeviceRequest(
        kStandardOutDevice, kClearFeature, 0x7FU, 0U, 0U);
    ExpectControlErrorWithoutTransfer();
    EXPECT_EQ(1U, ft32UsbEp0TestRemoteWakeup());
}

TEST_F(Ft32UsbEp0Test, DeviceAndConfigurationDescriptorsRejectInvalidIndexes)
{
    struct RequestShape {
        uint16_t value;
        uint16_t index;
        uint16_t length;
    };
    constexpr std::array<RequestShape, 4> invalidShapes = {{
        {0x0101U, 0U, 18U},
        {0x0100U, 1U, 18U},
        {0x0201U, 0U, 9U},
        {0x0200U, 1U, 9U},
    }};

    for (const RequestShape &shape : invalidShapes) {
        ft32UsbEp0TestReset();

        (void)ft32UsbEp0TestSubmitDeviceRequest(
            kStandardInDevice, kGetDescriptor,
            shape.value, shape.index, shape.length);
        ExpectControlErrorWithoutTransfer();
    }
}

TEST_F(Ft32UsbEp0Test, FullSpeedDeviceQualifierStallsThenNextSetupRecovers)
{
    ft32UsbEp0TestQueueSetup(
        kStandardInDevice, kGetDescriptor, 0x0600U, 0U, 10U);
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(1U, ft32UsbEp0TestSetupCallbackCount());
    EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
    EXPECT_EQ(2U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(1U, ft32UsbEp0TestInStall());
    EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitCount());

    ft32UsbEp0TestSignalSentStall();
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());

    ft32UsbEp0TestQueueSetup(
        kStandardOutDevice, kSetAddress, 9U, 0U, 0U);
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(2U, ft32UsbEp0TestSetupCallbackCount());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(1U, ft32UsbEp0TestPendingAddress());
    EXPECT_EQ(1U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(0U, ft32UsbEp0TestLlTransmitLength());
    EXPECT_EQ(FT32_USB_EP0_PCD_STATUS_IN, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_STATUS_IN, ft32UsbEp0TestCoreState());

    ft32UsbEp0TestCompleteIn();
    EXPECT_EQ(0U, ft32UsbEp0TestPendingAddress());
    EXPECT_EQ(9U, ft32UsbEp0TestHardwareAddress());
    EXPECT_EQ(9U, ft32UsbEp0TestCoreAddress());
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0Test, ConfigurationDescriptorReadUsesSixtyFourThenThreeAndStatusOut)
{
    EXPECT_EQ(FT32_USB_EP0_USBD_OK,
              ft32UsbEp0TestSubmitDeviceRequest(
                  kStandardInDevice, kGetDescriptor,
                  0x0200U, 0U, 67U));

    EXPECT_EQ(1U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(67U, ft32UsbEp0TestLlTransmitLength());
    ASSERT_EQ(64U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_IN, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_DATA_IN, ft32UsbEp0TestCoreState());
    for (uint32_t i = 0U; i < 64U; i++) {
        EXPECT_EQ(ft32UsbEp0TestConfigurationDescriptorByte(i),
                  ft32UsbEp0TestTxByte(i));
    }

    ft32UsbEp0TestCompleteIn();

    EXPECT_EQ(2U, ft32UsbEp0TestLlTransmitCount());
    EXPECT_EQ(3U, ft32UsbEp0TestLlTransmitLength());
    ASSERT_EQ(67U, ft32UsbEp0TestTxLength());
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_IN, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_DATA_IN, ft32UsbEp0TestCoreState());
    for (uint32_t i = 64U; i < 67U; i++) {
        EXPECT_EQ(ft32UsbEp0TestConfigurationDescriptorByte(i),
                  ft32UsbEp0TestTxByte(i));
    }

    ft32UsbEp0TestCompleteIn();

    EXPECT_EQ(2U, ft32UsbEp0TestDataInCallbackCount());
    EXPECT_EQ(2U, ft32UsbEp0TestLlPrepareReceiveCount());
    EXPECT_EQ(FT32_USB_EP0_PCD_STATUS_OUT, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_STATUS_OUT, ft32UsbEp0TestCoreState());

    ft32UsbEp0TestQueuePacket(nullptr, 0U);
    ft32UsbEp0TestHandleEp0();
    EXPECT_EQ(1U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(FT32_USB_EP0_PCD_SETUP, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_IDLE, ft32UsbEp0TestCoreState());
}

} // namespace
