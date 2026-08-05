#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ft32_usb_ep0_test_api.h"
#include "ft32f4xx_pcd_fs.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

#define FT32_USB_EP0_TEST_BUFFER_SIZE 256U
#define FT32_USB_EP0_TEST_IN_67_REQUEST 0x31U
#define FT32_USB_EP0_TEST_IN_64_REQUEST 0x32U
#define FT32_USB_EP0_TEST_OUT_70_REQUEST 0x33U
#define FT32_USB_EP0_TEST_CONFIG_DESCRIPTOR_SIZE 67U

OTG_FS_TypeDef ft32UsbEp0MockRegs;
USBD_ClassTypeDef USBD_CMPSIT;

static PCD_FS_HandleTypeDef pcd;
static USBD_HandleTypeDef device;
static USBD_ClassTypeDef testClass;
static USBD_ConfigDescTypeDef configDescriptor;
static uint8_t fullConfigDescriptor[FT32_USB_EP0_TEST_CONFIG_DESCRIPTOR_SIZE];
static USBD_CDC_HandleTypeDef cdcHandle;
static USBD_CDC_ItfTypeDef cdcInterface;
static uint8_t fifoRx[FT32_USB_EP0_TEST_BUFFER_SIZE];
static uint8_t fifoTx[FT32_USB_EP0_TEST_BUFFER_SIZE];
static uint8_t classRx[FT32_USB_EP0_TEST_BUFFER_SIZE];
static uint8_t classTx[FT32_USB_EP0_TEST_BUFFER_SIZE];
static uint32_t fifoRxLength;
static uint32_t fifoRxPosition;
static uint32_t fifoTxLength;
static uint32_t setupCallbackCount;
static uint32_t dataInCallbackCount;
static uint32_t dataOutCallbackCount;
static uint32_t classRxReadyCount;
static uint32_t classTxSentCount;
static uint32_t classSetupCount;
static uint32_t addressCallbackCount;
static uint8_t addressSeenByCallback;
static uint8_t callbackEvents[16];
static uint32_t callbackEventCount;
static uint32_t llTransmitCount;
static uint32_t llTransmitLength;
static uint32_t llPrepareReceiveCount;
static uint32_t llStallCount;
static uint32_t llClearStallCount;
static uint8_t ownerSetupResult;
static uint32_t cdcControlCount;
static uint8_t cdcControlCommand;
static uint16_t cdcControlLength;
static uint8_t cdcControlData[CDC_REQ_MAX_DATA_SIZE];

static void ft32UsbEp0TestRecordEvent(uint8_t event)
{
    if (callbackEventCount < sizeof(callbackEvents)) {
        callbackEvents[callbackEventCount++] = event;
    }
}

static uint8_t ft32UsbEp0TestClassInit(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    return (uint8_t)USBD_OK;
}

static uint8_t ft32UsbEp0TestClassDeInit(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    return (uint8_t)USBD_OK;
}

static uint8_t ft32UsbEp0TestClassSetup(USBD_HandleTypeDef *pdev,
                                       USBD_SetupReqTypeDef *request)
{
    classSetupCount++;

    if (((request->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD) &&
        ((request->bmRequest & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_ENDPOINT) &&
        (request->bRequest == USB_REQ_CLEAR_FEATURE)) {
        return ownerSetupResult;
    }

    switch (request->bRequest) {
    case 0x20U:
        (void)USBD_CtlPrepareRx(pdev, classRx, request->wLength);
        return (uint8_t)USBD_OK;

    case FT32_USB_EP0_TEST_OUT_70_REQUEST:
        (void)USBD_CtlPrepareRx(pdev, classRx, request->wLength);
        return (uint8_t)USBD_OK;

    case FT32_USB_EP0_TEST_IN_67_REQUEST:
        (void)USBD_CtlSendData(pdev, classTx, 67U);
        return (uint8_t)USBD_OK;

    case FT32_USB_EP0_TEST_IN_64_REQUEST:
        (void)USBD_CtlSendData(pdev, classTx, 64U);
        return (uint8_t)USBD_OK;

    default:
        return (uint8_t)USBD_FAIL;
    }
}

static uint8_t ft32UsbEp0TestClassTxSent(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    classTxSentCount++;
    return (uint8_t)USBD_OK;
}

static uint8_t ft32UsbEp0TestClassRxReady(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    classRxReadyCount++;
    return (uint8_t)USBD_OK;
}

static uint8_t *ft32UsbEp0TestGetFsConfigDescriptor(uint16_t *length)
{
    *length = sizeof(fullConfigDescriptor);
    return fullConfigDescriptor;
}

static int8_t ft32UsbEp0TestCdcInit(void)
{
    return 0;
}

static int8_t ft32UsbEp0TestCdcDeInit(void)
{
    return 0;
}

static int8_t ft32UsbEp0TestCdcControl(uint8_t command, uint8_t *buffer,
                                      uint16_t length)
{
    cdcControlCount++;
    cdcControlCommand = command;
    cdcControlLength = length;

    if ((command == CDC_GET_LINE_CODING) &&
        (length == CDC_REQ_MAX_DATA_SIZE)) {
        for (uint32_t i = 0U; i < CDC_REQ_MAX_DATA_SIZE; i++) {
            buffer[i] = (uint8_t)(0xA0U + i);
        }
    }

    if ((buffer != NULL) && (length <= sizeof(cdcControlData))) {
        memcpy(cdcControlData, buffer, length);
    }
    return 0;
}

static int8_t ft32UsbEp0TestCdcReceive(uint8_t *buffer, uint32_t *length)
{
    (void)buffer;
    (void)length;
    return 0;
}

static int8_t ft32UsbEp0TestCdcTransmitComplete(uint8_t *buffer,
                                                uint32_t *length,
                                                uint8_t endpoint)
{
    (void)buffer;
    (void)length;
    (void)endpoint;
    return 0;
}

uint8_t ft32UsbEp0MockFifoReadByte(uint32_t address)
{
    (void)address;
    if (fifoRxPosition >= fifoRxLength) {
        return 0U;
    }
    return fifoRx[fifoRxPosition++];
}

void ft32UsbEp0MockFifoWriteByte(uint32_t address, uint8_t value)
{
    (void)address;
    if (fifoTxLength < sizeof(fifoTx)) {
        fifoTx[fifoTxLength++] = value;
    }
}

void ft32UsbEp0TestReset(void)
{
    memset(&ft32UsbEp0MockRegs, 0, sizeof(ft32UsbEp0MockRegs));
    memset(&pcd, 0, sizeof(pcd));
    memset(&device, 0, sizeof(device));
    memset(&testClass, 0, sizeof(testClass));
    memset(&configDescriptor, 0, sizeof(configDescriptor));
    memset(fullConfigDescriptor, 0, sizeof(fullConfigDescriptor));
    memset(&cdcHandle, 0, sizeof(cdcHandle));
    memset(&cdcInterface, 0, sizeof(cdcInterface));
    memset(&USBD_CMPSIT, 0, sizeof(USBD_CMPSIT));
    memset(fifoRx, 0, sizeof(fifoRx));
    memset(fifoTx, 0, sizeof(fifoTx));
    memset(classRx, 0, sizeof(classRx));

    for (uint32_t i = 0U; i < sizeof(classTx); i++) {
        classTx[i] = (uint8_t)(0x5AU ^ i);
    }
    for (uint32_t i = 0U; i < sizeof(fullConfigDescriptor); i++) {
        fullConfigDescriptor[i] = (uint8_t)(0xC3U ^ i);
    }
    fullConfigDescriptor[0] = 9U;
    fullConfigDescriptor[1] = USB_DESC_TYPE_CONFIGURATION;
    fullConfigDescriptor[2] = sizeof(fullConfigDescriptor);
    fullConfigDescriptor[3] = 0U;
    fullConfigDescriptor[4] = 2U;
    fullConfigDescriptor[5] = 1U;
    fullConfigDescriptor[6] = 0U;
    fullConfigDescriptor[7] = 0x80U;
    fullConfigDescriptor[8] = 50U;

    fifoRxLength = 0U;
    fifoRxPosition = 0U;
    fifoTxLength = 0U;
    setupCallbackCount = 0U;
    dataInCallbackCount = 0U;
    dataOutCallbackCount = 0U;
    classRxReadyCount = 0U;
    classTxSentCount = 0U;
    classSetupCount = 0U;
    addressCallbackCount = 0U;
    addressSeenByCallback = 0U;
    memset(callbackEvents, 0, sizeof(callbackEvents));
    callbackEventCount = 0U;
    llTransmitCount = 0U;
    llTransmitLength = 0U;
    llPrepareReceiveCount = 0U;
    llStallCount = 0U;
    llClearStallCount = 0U;
    ownerSetupResult = (uint8_t)USBD_OK;
    cdcControlCount = 0U;
    cdcControlCommand = 0U;
    cdcControlLength = 0U;
    memset(cdcControlData, 0, sizeof(cdcControlData));

    testClass.Init = ft32UsbEp0TestClassInit;
    testClass.DeInit = ft32UsbEp0TestClassDeInit;
    testClass.Setup = ft32UsbEp0TestClassSetup;
    testClass.EP0_TxSent = ft32UsbEp0TestClassTxSent;
    testClass.EP0_RxReady = ft32UsbEp0TestClassRxReady;
    USBD_CMPSIT.GetFSConfigDescriptor = ft32UsbEp0TestGetFsConfigDescriptor;

    pcd.Init.endpoints = 4U;
    pcd.Init.speed = PCD_SPEED_FULL;
    pcd.Init.ep0_mps = 64U;
    pcd.ctrl_state = PCD_CTRL_SETUP;
    pcd.IN_ep[0].num = 0U;
    pcd.IN_ep[0].is_in = 1U;
    pcd.IN_ep[0].maxpacket = 64U;
    pcd.OUT_ep[0].num = 0U;
    pcd.OUT_ep[0].is_in = 0U;
    pcd.OUT_ep[0].maxpacket = 64U;
    pcd.pData = &device;

    device.dev_state = USBD_STATE_DEFAULT;
    device.dev_speed = USBD_SPEED_FULL;
    device.ep0_state = USBD_EP0_IDLE;
    device.ep_in[0].maxpacket = 64U;
    device.ep_out[0].maxpacket = 64U;
    device.pClass[0] = &testClass;
    configDescriptor.bLength = sizeof(configDescriptor);
    configDescriptor.bDescriptorType = USB_DESC_TYPE_CONFIGURATION;
    configDescriptor.wTotalLength = sizeof(configDescriptor);
    configDescriptor.bmAttributes = 0x80U;
    device.pConfDesc = &configDescriptor;
    device.pData = &pcd;
    device.NumClasses = 1U;
    device.tclasslist[0].Active = 1U;
    device.tclasslist[0].ClassId = 0U;
    device.tclasslist[0].NumIf = 1U;
    device.tclasslist[0].Ifs[0] = 0U;
    device.tclasslist[0].NumEps = 2U;
    device.tclasslist[0].Eps[0].add = 0x81U;
    device.tclasslist[0].Eps[0].is_used = 1U;
    device.tclasslist[0].Eps[1].add = 0x01U;
    device.tclasslist[0].Eps[1].is_used = 1U;
}

void ft32UsbEp0TestSetConfigured(uint8_t configured)
{
    device.dev_state = (configured != 0U) ? USBD_STATE_CONFIGURED : USBD_STATE_DEFAULT;
}

void ft32UsbEp0TestSetAddressed(void)
{
    device.dev_state = USBD_STATE_ADDRESSED;
}

void ft32UsbEp0TestQueuePacket(const uint8_t *data, uint8_t length)
{
    fifoRxLength = length;
    fifoRxPosition = 0U;
    if ((length != 0U) && (data != NULL)) {
        memcpy(fifoRx, data, length);
    }
    ft32UsbEp0MockRegs.COUNT0 = length;
    ft32UsbEp0MockRegs.CSR0 = FT32_USB_EP0_CSR_RXPKTRDY;
}

void ft32UsbEp0TestQueueSetup(uint8_t bm_request, uint8_t request,
                              uint16_t value, uint16_t index, uint16_t length)
{
    uint8_t setup[8];

    setup[0] = bm_request;
    setup[1] = request;
    setup[2] = (uint8_t)value;
    setup[3] = (uint8_t)(value >> 8U);
    setup[4] = (uint8_t)index;
    setup[5] = (uint8_t)(index >> 8U);
    setup[6] = (uint8_t)length;
    setup[7] = (uint8_t)(length >> 8U);
    ft32UsbEp0TestQueuePacket(setup, sizeof(setup));
}

void ft32UsbEp0TestSetSetupEndWithQueuedPacket(void)
{
    ft32UsbEp0MockRegs.CSR0 = FT32_USB_EP0_CSR_SETUPEND |
                              FT32_USB_EP0_CSR_RXPKTRDY;
}

void ft32UsbEp0TestPresentQueuedPacket(void)
{
    ft32UsbEp0MockRegs.COUNT0 = (uint8_t)fifoRxLength;
    ft32UsbEp0MockRegs.CSR0 = FT32_USB_EP0_CSR_RXPKTRDY;
}

void ft32UsbEp0TestSetCsr0AndCount(uint8_t csr0, uint8_t count0)
{
    ft32UsbEp0MockRegs.CSR0 = csr0;
    ft32UsbEp0MockRegs.COUNT0 = count0;
}

void ft32UsbEp0TestHandleEp0(void)
{
    PCD_FS_EP0_IRQHandler(&pcd);
}

void ft32UsbEp0TestCompleteIn(void)
{
    ft32UsbEp0MockRegs.CSR0 = 0U;
    PCD_FS_EP0_IRQHandler(&pcd);
}

void ft32UsbEp0TestSignalSentStall(void)
{
    ft32UsbEp0MockRegs.CSR0 = FT32_USB_EP0_CSR_STSTALL;
    PCD_FS_EP0_IRQHandler(&pcd);
}

void ft32UsbEp0TestSignalReset(void)
{
    ft32UsbEp0MockRegs.DEVCTL = 0U;
    ft32UsbEp0MockRegs.INTRUSB = OTG_FS_INTRUSB_RSTINT;
    ft32UsbEp0MockRegs.INTRUSBE = OTG_FS_INTRUSB_RSTINT;
    ft32UsbEp0MockRegs.INTRTX1 = 0U;
    ft32UsbEp0MockRegs.INTRRX1 = 0U;
    PCD_FS_IRQHandler(&pcd);
    ft32UsbEp0MockRegs.INTRUSB = 0U;
}

void ft32UsbEp0TestReceiveNull(uint32_t length)
{
    PCD_FS_EP_Receive(&pcd, 0U, NULL, length);
}

void ft32UsbEp0TestReceiveBuffer(uint32_t buffer_offset, uint32_t length)
{
    uint8_t *buffer = (buffer_offset < sizeof(classRx)) ? &classRx[buffer_offset] : NULL;
    PCD_FS_EP_Receive(&pcd, 0U, buffer, length);
}

void ft32UsbEp0TestRetransmit(uint32_t buffer_offset, uint32_t length)
{
    uint8_t *buffer = (buffer_offset < sizeof(classTx)) ? &classTx[buffer_offset] : NULL;
    PCD_FS_EP_Transmit(&pcd, 0U, buffer, length);
}

void ft32UsbEp0TestCorruptInCounts(uint32_t total, uint32_t completed,
                                   uint32_t packet_length, uint32_t packet_count)
{
    pcd.ctrl_state = PCD_CTRL_DATA_IN;
    pcd.ep0_data_in_total = total;
    pcd.ep0_data_in_count = completed;
    pcd.ep0_in_pending = 1U;
    pcd.IN_ep[0].xfer_buff = classTx;
    pcd.IN_ep[0].xfer_len = packet_length;
    pcd.IN_ep[0].xfer_count = packet_count;
    ft32UsbEp0MockRegs.CSR0 = 0U;
}

void ft32UsbEp0TestDirtyState(void)
{
    pcd.ctrl_state = PCD_CTRL_DATA_IN;
    pcd.ep0_data_in_total = 91U;
    pcd.ep0_data_in_count = 64U;
    pcd.ep0_data_in_zlp = 1U;
    pcd.ep0_in_pending = 1U;
    pcd.ep0_out_pending = 1U;
    pcd.address_pending = 1U;
    pcd.USB_Address = 77U;
    pcd.IN_ep[0].xfer_buff = classTx;
    pcd.IN_ep[0].xfer_len = 27U;
    pcd.IN_ep[0].xfer_count = 4U;
    pcd.IN_ep[0].is_stall = 1U;
    pcd.OUT_ep[0].xfer_buff = classRx;
    pcd.OUT_ep[0].xfer_len = 11U;
    pcd.OUT_ep[0].xfer_count = 3U;
    pcd.OUT_ep[0].is_stall = 1U;
    device.ep0_state = USBD_EP0_DATA_IN;
    device.dev_address = 77U;
    device.dev_state = USBD_STATE_ADDRESSED;
    ft32UsbEp0MockRegs.FADDR = 23U;
}

void ft32UsbEp0TestUseCdcClass(void)
{
    cdcHandle.CmdOpCode = 0xFFU;
    cdcInterface.Init = ft32UsbEp0TestCdcInit;
    cdcInterface.DeInit = ft32UsbEp0TestCdcDeInit;
    cdcInterface.Control = ft32UsbEp0TestCdcControl;
    cdcInterface.Receive = ft32UsbEp0TestCdcReceive;
    cdcInterface.TransmitCplt = ft32UsbEp0TestCdcTransmitComplete;

    device.pClass[0] = &USBD_CDC;
    device.pClassData = &cdcHandle;
    device.pClassDataCmsit[0] = &cdcHandle;
    device.pUserData[0] = &cdcInterface;
    device.classId = 0U;
    device.tclasslist[0].ClassType = CLASS_TYPE_CDC;
    device.tclasslist[0].Active = 1U;
    device.tclasslist[0].ClassId = 0U;
    device.tclasslist[0].NumIf = 2U;
    device.tclasslist[0].Ifs[0] = 0U;
    device.tclasslist[0].Ifs[1] = 1U;
    device.tclasslist[0].NumEps = 3U;
    device.tclasslist[0].Eps[0].add = 0x81U;
    device.tclasslist[0].Eps[0].type = USBD_EP_TYPE_BULK;
    device.tclasslist[0].Eps[0].is_used = 1U;
    device.tclasslist[0].Eps[1].add = 0x01U;
    device.tclasslist[0].Eps[1].type = USBD_EP_TYPE_BULK;
    device.tclasslist[0].Eps[1].is_used = 1U;
    device.tclasslist[0].Eps[2].add = 0x82U;
    device.tclasslist[0].Eps[2].type = USBD_EP_TYPE_INTR;
    device.tclasslist[0].Eps[2].is_used = 1U;
}

uint8_t ft32UsbEp0TestSubmitInterfaceRequest(uint8_t bm_request, uint8_t request,
                                             uint16_t value, uint16_t index,
                                             uint16_t length)
{
    USBD_SetupReqTypeDef setup = {
        .bmRequest = bm_request,
        .bRequest = request,
        .wValue = value,
        .wIndex = index,
        .wLength = length,
    };
    device.request = setup;
    device.ep0_data_len = length;
    return (uint8_t)USBD_StdItfReq(&device, &setup);
}

uint8_t ft32UsbEp0TestSubmitDeviceRequest(uint8_t bm_request, uint8_t request,
                                          uint16_t value, uint16_t index,
                                          uint16_t length)
{
    USBD_SetupReqTypeDef setup = {
        .bmRequest = bm_request,
        .bRequest = request,
        .wValue = value,
        .wIndex = index,
        .wLength = length,
    };
    device.request = setup;
    device.ep0_data_len = length;
    return (uint8_t)USBD_StdDevReq(&device, &setup);
}

uint8_t ft32UsbEp0TestSubmitEndpointRequest(uint8_t bm_request, uint8_t request,
                                            uint16_t value, uint16_t index,
                                            uint16_t length)
{
    USBD_SetupReqTypeDef setup = {
        .bmRequest = bm_request,
        .bRequest = request,
        .wValue = value,
        .wIndex = index,
        .wLength = length,
    };
    device.request = setup;
    device.ep0_data_len = length;
    return (uint8_t)USBD_StdEPReq(&device, &setup);
}

void ft32UsbEp0TestSetEndpointUsed(uint8_t address, uint8_t used)
{
    const uint8_t index = address & 0x0FU;

    if ((address & 0x80U) != 0U) {
        device.ep_in[index].is_used = used;
        pcd.IN_ep[index].num = index;
        pcd.IN_ep[index].is_in = 1U;
        pcd.IN_ep[index].maxpacket = 64U;
    } else {
        device.ep_out[index].is_used = used;
        pcd.OUT_ep[index].num = index;
        pcd.OUT_ep[index].is_in = 0U;
        pcd.OUT_ep[index].maxpacket = 64U;
    }
}

void ft32UsbEp0TestSetEndpointStalled(uint8_t address, uint8_t stalled)
{
    const uint8_t index = address & 0x0FU;

    if ((address & 0x80U) != 0U) {
        pcd.IN_ep[index].is_stall = stalled;
    } else {
        pcd.OUT_ep[index].is_stall = stalled;
    }
}

uint8_t ft32UsbEp0TestEndpointStalled(uint8_t address)
{
    const uint8_t index = address & 0x0FU;
    return ((address & 0x80U) != 0U) ? pcd.IN_ep[index].is_stall
                                     : pcd.OUT_ep[index].is_stall;
}

void ft32UsbEp0TestSetEndpointOwnerPresent(uint8_t present)
{
    device.tclasslist[0].NumEps = (present != 0U) ? 2U : 0U;
}

void ft32UsbEp0TestSetOwnerClassPresent(uint8_t present)
{
    device.pClass[0] = (present != 0U) ? &testClass : NULL;
}

void ft32UsbEp0TestSetOwnerSetupPresent(uint8_t present)
{
    testClass.Setup = (present != 0U) ? ft32UsbEp0TestClassSetup : NULL;
}

void ft32UsbEp0TestSetOwnerSetupResult(uint8_t result)
{
    ownerSetupResult = result;
}

void ft32UsbEp0TestSetRemoteWakeupCapability(uint8_t capable)
{
    if (capable != 0U) {
        configDescriptor.bmAttributes |= 0x20U;
    } else {
        configDescriptor.bmAttributes &= (uint8_t)~0x20U;
    }
}

void ft32UsbEp0TestSetRemoteWakeup(uint8_t enabled)
{
    device.dev_remote_wakeup = (enabled != 0U) ? 1U : 0U;
}

uint8_t ft32UsbEp0TestPcdState(void) { return (uint8_t)pcd.ctrl_state; }
uint8_t ft32UsbEp0TestCoreState(void) { return (uint8_t)device.ep0_state; }
uint8_t ft32UsbEp0TestCsr0(void) { return ft32UsbEp0MockRegs.CSR0; }
uint8_t ft32UsbEp0TestCsr02(void) { return ft32UsbEp0MockRegs.CSR02; }
uint8_t ft32UsbEp0TestFlushObserved(void)
{
    return (ft32UsbEp0MockRegs.CSR02 & OTG_FS_CSR02_FFIFO) != 0U;
}
uint8_t ft32UsbEp0TestHardwareAddress(void) { return ft32UsbEp0MockRegs.FADDR; }
uint8_t ft32UsbEp0TestPendingAddress(void) { return pcd.address_pending; }
uint8_t ft32UsbEp0TestPcdAddress(void) { return pcd.USB_Address; }
uint8_t ft32UsbEp0TestCoreAddress(void) { return device.dev_address; }
uint8_t ft32UsbEp0TestCoreDeviceState(void) { return device.dev_state; }
uint8_t ft32UsbEp0TestCoreConfiguration(void) { return (uint8_t)device.dev_config; }
uint8_t ft32UsbEp0TestRemoteWakeup(void) { return (uint8_t)device.dev_remote_wakeup; }
uint8_t ft32UsbEp0TestInPending(void) { return pcd.ep0_in_pending; }
uint8_t ft32UsbEp0TestOutPending(void) { return pcd.ep0_out_pending; }
uint8_t ft32UsbEp0TestInStall(void) { return pcd.IN_ep[0].is_stall; }
uint8_t ft32UsbEp0TestOutStall(void) { return pcd.OUT_ep[0].is_stall; }
uint32_t ft32UsbEp0TestInLength(void) { return pcd.IN_ep[0].xfer_len; }
uint32_t ft32UsbEp0TestInCount(void) { return pcd.IN_ep[0].xfer_count; }
uint32_t ft32UsbEp0TestOutLength(void) { return pcd.OUT_ep[0].xfer_len; }
uint32_t ft32UsbEp0TestOutCount(void) { return pcd.OUT_ep[0].xfer_count; }
uint8_t ft32UsbEp0TestInBufferIsNull(void) { return pcd.IN_ep[0].xfer_buff == NULL; }
uint8_t ft32UsbEp0TestOutBufferIsNull(void) { return pcd.OUT_ep[0].xfer_buff == NULL; }
uint32_t ft32UsbEp0TestInBufferOffset(void)
{
    const uintptr_t pointer = (uintptr_t)pcd.IN_ep[0].xfer_buff;
    const uintptr_t begin = (uintptr_t)classTx;
    const uintptr_t end = begin + sizeof(classTx);

    if ((pointer < begin) || (pointer > end)) {
        return UINT32_MAX;
    }
    return (uint32_t)(pointer - begin);
}
uint32_t ft32UsbEp0TestOutBufferOffset(void)
{
    const uintptr_t pointer = (uintptr_t)pcd.OUT_ep[0].xfer_buff;
    const uintptr_t begin = (uintptr_t)classRx;
    const uintptr_t end = begin + sizeof(classRx);

    if ((pointer < begin) || (pointer > end)) {
        return UINT32_MAX;
    }
    return (uint32_t)(pointer - begin);
}

uint32_t ft32UsbEp0TestTxLength(void) { return fifoTxLength; }
uint8_t ft32UsbEp0TestTxByte(uint32_t index)
{
    return (index < fifoTxLength) ? fifoTx[index] : 0U;
}
uint8_t ft32UsbEp0TestConfigurationDescriptorByte(uint32_t index)
{
    return (index < sizeof(fullConfigDescriptor)) ? fullConfigDescriptor[index] : 0U;
}
uint32_t ft32UsbEp0TestRxConsumed(void) { return fifoRxPosition; }
uint8_t ft32UsbEp0TestRxByte(uint32_t index)
{
    return (index < sizeof(classRx)) ? classRx[index] : 0U;
}
uint32_t ft32UsbEp0TestSetupCallbackCount(void) { return setupCallbackCount; }
uint32_t ft32UsbEp0TestDataInCallbackCount(void) { return dataInCallbackCount; }
uint32_t ft32UsbEp0TestDataOutCallbackCount(void) { return dataOutCallbackCount; }
uint32_t ft32UsbEp0TestClassRxReadyCount(void) { return classRxReadyCount; }
uint32_t ft32UsbEp0TestClassTxSentCount(void) { return classTxSentCount; }
uint32_t ft32UsbEp0TestAddressCallbackCount(void) { return addressCallbackCount; }
uint8_t ft32UsbEp0TestAddressSeenByCallback(void) { return addressSeenByCallback; }
void ft32UsbEp0TestClearEvents(void)
{
    memset(callbackEvents, 0, sizeof(callbackEvents));
    callbackEventCount = 0U;
}
uint32_t ft32UsbEp0TestEventCount(void) { return callbackEventCount; }
uint8_t ft32UsbEp0TestEvent(uint32_t index)
{
    return (index < callbackEventCount) ? callbackEvents[index] : 0U;
}
uint32_t ft32UsbEp0TestClassSetupCount(void) { return classSetupCount; }
uint32_t ft32UsbEp0TestLlTransmitCount(void) { return llTransmitCount; }
uint32_t ft32UsbEp0TestLlTransmitLength(void) { return llTransmitLength; }
uint32_t ft32UsbEp0TestLlPrepareReceiveCount(void) { return llPrepareReceiveCount; }
uint32_t ft32UsbEp0TestLlStallCount(void) { return llStallCount; }
uint32_t ft32UsbEp0TestLlClearStallCount(void) { return llClearStallCount; }
uint32_t ft32UsbEp0TestCdcControlCount(void) { return cdcControlCount; }
uint8_t ft32UsbEp0TestCdcControlCommand(void) { return cdcControlCommand; }
uint16_t ft32UsbEp0TestCdcControlLength(void) { return cdcControlLength; }
uint8_t ft32UsbEp0TestCdcControlByte(uint32_t index)
{
    return (index < sizeof(cdcControlData)) ? cdcControlData[index] : 0U;
}

void PCD_FS_SetupStageCallback(PCD_FS_HandleTypeDef *hpcd)
{
    setupCallbackCount++;
    ft32UsbEp0TestRecordEvent(FT32_USB_EP0_EVENT_SETUP);
    (void)USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup);
}

void PCD_FS_DataOutStageCallback(PCD_FS_HandleTypeDef *hpcd, uint8_t epnum)
{
    dataOutCallbackCount++;
    ft32UsbEp0TestRecordEvent(FT32_USB_EP0_EVENT_DATA_OUT);
    (void)USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

void PCD_FS_DataInStageCallback(PCD_FS_HandleTypeDef *hpcd, uint8_t epnum)
{
    dataInCallbackCount++;
    ft32UsbEp0TestRecordEvent(FT32_USB_EP0_EVENT_DATA_IN);
    (void)USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

void PCD_FS_AddressCallback(PCD_FS_HandleTypeDef *hpcd, uint8_t address)
{
    USBD_HandleTypeDef *pdev = (USBD_HandleTypeDef *)hpcd->pData;
    addressCallbackCount++;
    addressSeenByCallback = ft32UsbEp0MockRegs.FADDR;
    ft32UsbEp0TestRecordEvent(FT32_USB_EP0_EVENT_ADDRESS);
    pdev->dev_address = address;
    pdev->dev_state = (address != 0U) ? USBD_STATE_ADDRESSED : USBD_STATE_DEFAULT;
}

void PCD_FS_ResetCallback(PCD_FS_HandleTypeDef *hpcd)
{
    (void)USBD_LL_Reset(hpcd->pData);
    (void)USBD_LL_SetSpeed(hpcd->pData, USBD_SPEED_FULL);
}

void PCD_FS_SOFCallback(PCD_FS_HandleTypeDef *hpcd) { (void)hpcd; }
void PCD_FS_SuspendCallback(PCD_FS_HandleTypeDef *hpcd) { (void)hpcd; }
void PCD_FS_ResumeCallback(PCD_FS_HandleTypeDef *hpcd) { (void)hpcd; }
void PCD_FS_ConnectCallback(PCD_FS_HandleTypeDef *hpcd) { (void)hpcd; }
void PCD_FS_DisconnectCallback(PCD_FS_HandleTypeDef *hpcd) { (void)hpcd; }
void PCD_FS_SessionCallback(PCD_FS_HandleTypeDef *hpcd) { (void)hpcd; }
void PCD_FS_VBusErrCallback(PCD_FS_HandleTypeDef *hpcd) { (void)hpcd; }
void PCD_FS_OVERRUNCallback(PCD_FS_HandleTypeDef *hpcd) { (void)hpcd; }
void PCD_FS_UNDERRUNCallback(PCD_FS_HandleTypeDef *hpcd) { (void)hpcd; }
void PCD_FS_DERRCallback(PCD_FS_HandleTypeDef *hpcd) { (void)hpcd; }

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                  uint8_t ep_type, uint16_t ep_mps)
{
    (void)PCD_FS_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    (void)PCD_FS_EP_Close(pdev->pData, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    (void)PCD_FS_EP_Flush(pdev->pData, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    llStallCount++;
    (void)PCD_FS_EP_SetStall(pdev->pData, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    llClearStallCount++;
    (void)PCD_FS_EP_ClrStall(pdev->pData, ep_addr);
    return USBD_OK;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    PCD_FS_HandleTypeDef *hpcd = (PCD_FS_HandleTypeDef *)pdev->pData;
    if ((ep_addr & 0x80U) != 0U) {
        return hpcd->IN_ep[ep_addr & 0x7FU].is_stall;
    }
    return hpcd->OUT_ep[ep_addr & 0x7FU].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t address)
{
    (void)PCD_FS_SetAddress(pdev->pData, address);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                    uint8_t *buffer, uint32_t length)
{
    llTransmitCount++;
    llTransmitLength = length;
    PCD_FS_EP_Transmit(pdev->pData, ep_addr, buffer, length);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                          uint8_t *buffer, uint32_t length)
{
    llPrepareReceiveCount++;
    PCD_FS_EP_Receive(pdev->pData, ep_addr, buffer, length);
    return USBD_OK;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return PCD_FS_EP_GetRxCount(pdev->pData, ep_addr);
}
