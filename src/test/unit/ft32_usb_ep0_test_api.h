#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FT32_USB_EP0_PCD_SETUP = 0,
    FT32_USB_EP0_PCD_DATA_IN,
    FT32_USB_EP0_PCD_DATA_OUT,
    FT32_USB_EP0_PCD_STATUS_IN,
    FT32_USB_EP0_PCD_STATUS_OUT,
    FT32_USB_EP0_PCD_STALL,
};

enum {
    FT32_USB_EP0_CORE_IDLE = 0,
    FT32_USB_EP0_CORE_SETUP,
    FT32_USB_EP0_CORE_DATA_IN,
    FT32_USB_EP0_CORE_DATA_OUT,
    FT32_USB_EP0_CORE_STATUS_IN,
    FT32_USB_EP0_CORE_STATUS_OUT,
    FT32_USB_EP0_CORE_STALL,
};

enum {
    FT32_USB_EP0_CSR_RXPKTRDY = 0x01,
    FT32_USB_EP0_CSR_TXPKTRDY = 0x02,
    FT32_USB_EP0_CSR_STSTALL = 0x04,
    FT32_USB_EP0_CSR_DATAEND = 0x08,
    FT32_USB_EP0_CSR_SETUPEND = 0x10,
    FT32_USB_EP0_CSR_SDSTALL = 0x20,
    FT32_USB_EP0_CSR_SRXPKTRDY = 0x40,
    FT32_USB_EP0_CSR_SSETUPEND = 0x80,
};

enum {
    FT32_USB_EP0_EVENT_SETUP = 1,
    FT32_USB_EP0_EVENT_DATA_IN,
    FT32_USB_EP0_EVENT_DATA_OUT,
    FT32_USB_EP0_EVENT_ADDRESS,
};

enum {
    FT32_USB_EP0_USBD_OK = 0,
    FT32_USB_EP0_USBD_FAIL = 3,
};

void ft32UsbEp0TestReset(void);
void ft32UsbEp0TestSetConfigured(uint8_t configured);
void ft32UsbEp0TestSetAddressed(void);
void ft32UsbEp0TestQueueSetup(uint8_t bm_request, uint8_t request,
                              uint16_t value, uint16_t index, uint16_t length);
void ft32UsbEp0TestQueuePacket(const uint8_t *data, uint8_t length);
void ft32UsbEp0TestSetSetupEndWithQueuedPacket(void);
void ft32UsbEp0TestPresentQueuedPacket(void);
void ft32UsbEp0TestSetCsr0AndCount(uint8_t csr0, uint8_t count0);
void ft32UsbEp0TestHandleEp0(void);
void ft32UsbEp0TestCompleteIn(void);
void ft32UsbEp0TestSignalSentStall(void);
void ft32UsbEp0TestSignalReset(void);
void ft32UsbEp0TestReceiveNull(uint32_t length);
void ft32UsbEp0TestReceiveBuffer(uint32_t buffer_offset, uint32_t length);
void ft32UsbEp0TestRetransmit(uint32_t buffer_offset, uint32_t length);
void ft32UsbEp0TestCorruptInCounts(uint32_t total, uint32_t completed,
                                   uint32_t packet_length, uint32_t packet_count);
void ft32UsbEp0TestDirtyState(void);
void ft32UsbEp0TestUseCdcClass(void);
uint8_t ft32UsbEp0TestSubmitInterfaceRequest(uint8_t bm_request, uint8_t request,
                                             uint16_t value, uint16_t index,
                                             uint16_t length);
uint8_t ft32UsbEp0TestSubmitDeviceRequest(uint8_t bm_request, uint8_t request,
                                          uint16_t value, uint16_t index,
                                          uint16_t length);
uint8_t ft32UsbEp0TestSubmitEndpointRequest(uint8_t bm_request, uint8_t request,
                                            uint16_t value, uint16_t index,
                                            uint16_t length);
void ft32UsbEp0TestSetEndpointUsed(uint8_t address, uint8_t used);
void ft32UsbEp0TestSetEndpointStalled(uint8_t address, uint8_t stalled);
uint8_t ft32UsbEp0TestEndpointStalled(uint8_t address);
void ft32UsbEp0TestSetEndpointOwnerPresent(uint8_t present);
void ft32UsbEp0TestSetOwnerClassPresent(uint8_t present);
void ft32UsbEp0TestSetOwnerSetupPresent(uint8_t present);
void ft32UsbEp0TestSetOwnerSetupResult(uint8_t result);
void ft32UsbEp0TestSetRemoteWakeupCapability(uint8_t capable);
void ft32UsbEp0TestSetRemoteWakeup(uint8_t enabled);

uint8_t ft32UsbEp0TestPcdState(void);
uint8_t ft32UsbEp0TestCoreState(void);
uint8_t ft32UsbEp0TestCsr0(void);
uint8_t ft32UsbEp0TestCsr02(void);
uint8_t ft32UsbEp0TestFlushObserved(void);
uint8_t ft32UsbEp0TestHardwareAddress(void);
uint8_t ft32UsbEp0TestPendingAddress(void);
uint8_t ft32UsbEp0TestPcdAddress(void);
uint8_t ft32UsbEp0TestCoreAddress(void);
uint8_t ft32UsbEp0TestCoreDeviceState(void);
uint8_t ft32UsbEp0TestCoreConfiguration(void);
uint8_t ft32UsbEp0TestRemoteWakeup(void);
uint8_t ft32UsbEp0TestInPending(void);
uint8_t ft32UsbEp0TestOutPending(void);
uint8_t ft32UsbEp0TestInStall(void);
uint8_t ft32UsbEp0TestOutStall(void);
uint32_t ft32UsbEp0TestInLength(void);
uint32_t ft32UsbEp0TestInCount(void);
uint32_t ft32UsbEp0TestOutLength(void);
uint32_t ft32UsbEp0TestOutCount(void);
uint8_t ft32UsbEp0TestInBufferIsNull(void);
uint8_t ft32UsbEp0TestOutBufferIsNull(void);
uint32_t ft32UsbEp0TestInBufferOffset(void);
uint32_t ft32UsbEp0TestOutBufferOffset(void);

uint32_t ft32UsbEp0TestTxLength(void);
uint8_t ft32UsbEp0TestTxByte(uint32_t index);
uint8_t ft32UsbEp0TestConfigurationDescriptorByte(uint32_t index);
uint32_t ft32UsbEp0TestRxConsumed(void);
uint8_t ft32UsbEp0TestRxByte(uint32_t index);
uint32_t ft32UsbEp0TestSetupCallbackCount(void);
uint32_t ft32UsbEp0TestDataInCallbackCount(void);
uint32_t ft32UsbEp0TestDataOutCallbackCount(void);
uint32_t ft32UsbEp0TestClassRxReadyCount(void);
uint32_t ft32UsbEp0TestClassTxSentCount(void);
uint32_t ft32UsbEp0TestAddressCallbackCount(void);
uint8_t ft32UsbEp0TestAddressSeenByCallback(void);
void ft32UsbEp0TestClearEvents(void);
uint32_t ft32UsbEp0TestEventCount(void);
uint8_t ft32UsbEp0TestEvent(uint32_t index);
uint32_t ft32UsbEp0TestClassSetupCount(void);
uint32_t ft32UsbEp0TestLlTransmitCount(void);
uint32_t ft32UsbEp0TestLlTransmitLength(void);
uint32_t ft32UsbEp0TestLlPrepareReceiveCount(void);
uint32_t ft32UsbEp0TestLlStallCount(void);
uint32_t ft32UsbEp0TestLlClearStallCount(void);
uint32_t ft32UsbEp0TestCdcControlCount(void);
uint8_t ft32UsbEp0TestCdcControlCommand(void);
uint16_t ft32UsbEp0TestCdcControlLength(void);
uint8_t ft32UsbEp0TestCdcControlByte(uint32_t index);

#ifdef __cplusplus
}
#endif
