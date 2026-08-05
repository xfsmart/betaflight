#include <array>
#include <cstdint>

#include <gtest/gtest.h>

extern "C" {
#include "ft32_usb_ep0_test_api.h"
}

namespace {

constexpr uint8_t kClassOutInterface = 0x21U;
constexpr uint8_t kClassInInterface = 0xA1U;
constexpr uint8_t kSetLineCoding = 0x20U;
constexpr uint8_t kGetLineCoding = 0x21U;

class Ft32UsbEp0AdjacentTest : public testing::Test {
protected:
    void SetUp() override
    {
        ft32UsbEp0TestReset();
        ft32UsbEp0TestUseCdcClass();
        ft32UsbEp0TestSetConfigured(1U);
    }

    static void ArmStatusOut()
    {
        ASSERT_EQ(FT32_USB_EP0_USBD_OK,
                  ft32UsbEp0TestSubmitInterfaceRequest(
                      kClassInInterface, kGetLineCoding, 0U, 0U, 7U));
        ASSERT_EQ(1U, ft32UsbEp0TestInPending());

        ft32UsbEp0TestCompleteIn();

        ASSERT_EQ(1U, ft32UsbEp0TestDataInCallbackCount());
        ASSERT_EQ(1U, ft32UsbEp0TestOutPending());
        ASSERT_EQ(0U, ft32UsbEp0TestOutLength());
        ASSERT_EQ(FT32_USB_EP0_PCD_STATUS_OUT, ft32UsbEp0TestPcdState());
        ASSERT_EQ(FT32_USB_EP0_CORE_STATUS_OUT, ft32UsbEp0TestCoreState());
    }
};

TEST_F(Ft32UsbEp0AdjacentTest,
       CompletedStatusOutDispatchesQueuedSetupWithoutFlushOrStall)
{
    ArmStatusOut();
    ft32UsbEp0TestClearEvents();

    ft32UsbEp0TestQueueSetup(
        kClassOutInterface, kSetLineCoding, 0U, 0U, 7U);
    ft32UsbEp0TestHandleEp0();

    ASSERT_EQ(2U, ft32UsbEp0TestEventCount());
    EXPECT_EQ(FT32_USB_EP0_EVENT_DATA_OUT, ft32UsbEp0TestEvent(0U));
    EXPECT_EQ(FT32_USB_EP0_EVENT_SETUP, ft32UsbEp0TestEvent(1U));
    EXPECT_EQ(1U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(1U, ft32UsbEp0TestSetupCallbackCount());
    EXPECT_EQ(8U, ft32UsbEp0TestRxConsumed());
    EXPECT_EQ(0U, ft32UsbEp0TestFlushObserved());
    EXPECT_EQ(0U, ft32UsbEp0TestInStall());
    EXPECT_EQ(0U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(0U, ft32UsbEp0TestLlStallCount());
    EXPECT_EQ(FT32_USB_EP0_CSR_SRXPKTRDY, ft32UsbEp0TestCsr0());
    EXPECT_EQ(1U, ft32UsbEp0TestOutPending());
    EXPECT_EQ(7U, ft32UsbEp0TestOutLength());
    EXPECT_EQ(FT32_USB_EP0_PCD_DATA_OUT, ft32UsbEp0TestPcdState());
    EXPECT_EQ(FT32_USB_EP0_CORE_DATA_OUT, ft32UsbEp0TestCoreState());
}

TEST_F(Ft32UsbEp0AdjacentTest, MalformedStatusOutPayloadStillFlushesAndStalls)
{
    const std::array<uint8_t, 7> malformed = {};
    ArmStatusOut();
    ft32UsbEp0TestClearEvents();

    ft32UsbEp0TestQueuePacket(malformed.data(), malformed.size());
    ft32UsbEp0TestHandleEp0();

    EXPECT_EQ(0U, ft32UsbEp0TestEventCount());
    EXPECT_EQ(0U, ft32UsbEp0TestDataOutCallbackCount());
    EXPECT_EQ(0U, ft32UsbEp0TestSetupCallbackCount());
    EXPECT_EQ(0U, ft32UsbEp0TestRxConsumed());
    EXPECT_EQ(1U, ft32UsbEp0TestFlushObserved());
    EXPECT_EQ(1U, ft32UsbEp0TestInStall());
    EXPECT_EQ(1U, ft32UsbEp0TestOutStall());
    EXPECT_EQ(FT32_USB_EP0_PCD_STALL, ft32UsbEp0TestPcdState());
}

} // namespace
