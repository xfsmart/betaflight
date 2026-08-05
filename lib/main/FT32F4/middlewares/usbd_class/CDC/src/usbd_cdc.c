/**
  ******************************************************************************
  * @file    usbd_cdc.c
  * @author  FMD
  * @brief   This file provides the high layer firmware functions to manage the
  *          following functionalities of the USB CDC Class:
  *           - Initialization and Configuration of high and low layer
  *           - Enumeration as CDC Device (and enumeration for each implemented memory interface)
  *           - OUT/IN data transfer
  *           - Command IN transfer (class requests management)
  *           - Error management
  * @date    26-03-2026
  ******************************************************************************
  *  @verbatim
  *
  *          ===================================================================
  *                                CDC Class Driver Description
  *          ===================================================================
  *           This driver manages the "Universal Serial Bus Class Definitions for Communications Devices
  *           Revision 1.2 November 16, 2007" and the sub-protocol specification of "Universal Serial Bus
  *           Communications Class Subclass Specification for PSTN Devices Revision 1.2 February 9, 2007"
  *           This driver implements the following aspects of the specification:
  *             - Device descriptor management
  *             - Configuration descriptor management
  *             - Enumeration as CDC device with 2 data endpoints (IN and OUT) and 1 command endpoint (IN)
  *             - Requests management (as described in section 6.2 in specification)
  *             - Abstract Control Model compliant
  *             - Union Functional collection (using 1 IN endpoint for control)
  *             - Data interface class
  *
  *           These aspects may be enriched or modified for a specific user application.
  *
  *            This driver doesn't implement the following aspects of the specification
  *            (but it is possible to manage these features with some modifications on this driver):
  *             - Any class-specific aspect relative to communication classes should be managed by user application.
  *             - All communication classes other than PSTN are not managed
  *
  *  @endverbatim
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_def.h"
#include "usbd_cdc.h"
#include "usbd_ctlreq.h"

/* External CDC interface callbacks (provided by application) */
extern USBD_CDC_ItfTypeDef USBD_CDC_fops;


/** @addtogroup FT32_USB_DEVICE_LIBRARY
  * @{
  */


/** @defgroup USBD_CDC
  * @brief usbd core module
  * @{
  */

/** @defgroup USBD_CDC_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */


/** @defgroup USBD_CDC_Private_Defines
  * @{
  */
/**
  * @}
  */


/** @defgroup USBD_CDC_Private_Macros
  * @{
  */

/**
  * @}
  */


/** @defgroup USBD_CDC_Private_FunctionPrototypes
  * @{
  */

static uint8_t USBD_CDC_Init(USBD_HandleTypeDef *pdev);
static uint8_t USBD_CDC_DeInit(USBD_HandleTypeDef *pdev);
static uint8_t USBD_CDC_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req);
static uint8_t USBD_CDC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_CDC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_CDC_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t USBD_CDC_IsValidInterfaceIndex(const USBD_HandleTypeDef *pdev,
                                              uint16_t index);
static uint8_t USBD_CDC_IsValidCommunicationInterfaceIndex(
    const USBD_HandleTypeDef *pdev, uint16_t index);
static uint8_t USBD_CDC_IsValidClassRequest(const USBD_HandleTypeDef *pdev,
                                            const USBD_SetupReqTypeDef *req);
#ifndef USE_USBD_COMPOSITE
static uint8_t *USBD_CDC_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_CDC_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_CDC_GetOtherSpeedCfgDesc(uint16_t *length);
uint8_t *USBD_CDC_GetDeviceQualifierDescriptor(uint16_t *length);
#endif /* USE_USBD_COMPOSITE  */

#ifndef USE_USBD_COMPOSITE
/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_CDC_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};
#endif /* USE_USBD_COMPOSITE  */
/**
  * @}
  */

/** @defgroup USBD_CDC_Private_Variables
  * @{
  */


/* CDC interface class callbacks structure */
USBD_ClassTypeDef  USBD_CDC =
{
  USBD_CDC_Init,
  USBD_CDC_DeInit,
  USBD_CDC_Setup,
  NULL,                 /* EP0_TxSent */
  USBD_CDC_EP0_RxReady,
  USBD_CDC_DataIn,
  USBD_CDC_DataOut,
  NULL,                 /* SOF */
  NULL,                 /* IsoINIncomplete */
  NULL,                 /* IsoOUTIncomplete */
#ifdef USE_USBD_COMPOSITE
  NULL,
  NULL,
  NULL,
  NULL,
#else
  USBD_CDC_GetHSCfgDesc,
  USBD_CDC_GetFSCfgDesc,
  USBD_CDC_GetOtherSpeedCfgDesc,
  USBD_CDC_GetDeviceQualifierDescriptor,
#endif /* USE_USBD_COMPOSITE */
#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
  NULL,                 /* GetUsrStrDescriptor */
#endif  /* USBD_SUPPORT_USER_STRING_DESC */
};

#ifndef USE_USBD_COMPOSITE
/* USB CDC device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_CDC_CfgDesc[USB_CDC_CONFIG_DESC_SIZ] __ALIGN_END =
{
  /* Configuration Descriptor */
  0x09,                                       /* bLength: Configuration Descriptor size */
  USB_DESC_TYPE_CONFIGURATION,                /* bDescriptorType: Configuration */
  USB_CDC_CONFIG_DESC_SIZ,                    /* wTotalLength */
  0x00,
  0x02,                                       /* bNumInterfaces: 2 interfaces */
  0x01,                                       /* bConfigurationValue: Configuration value */
  0x00,                                       /* iConfiguration: Index of string descriptor
                                                 describing the configuration */
#if (USBD_SELF_POWERED == 1U)
  0xC0,                                       /* bmAttributes: Bus Powered according to user configuration */
#else
  0x80,                                       /* bmAttributes: Bus Powered according to user configuration */
#endif /* USBD_SELF_POWERED */
  USBD_MAX_POWER,                             /* MaxPower (mA) */

  /*---------------------------------------------------------------------------*/

  /* Interface Descriptor */
  0x09,                                       /* bLength: Interface Descriptor size */
  USB_DESC_TYPE_INTERFACE,                    /* bDescriptorType: Interface */
  /* Interface descriptor type */
  0x00,                                       /* bInterfaceNumber: Number of Interface */
  0x00,                                       /* bAlternateSetting: Alternate setting */
  0x01,                                       /* bNumEndpoints: One endpoint used */
  USB_CLASS_CODE_CDC,                         /* bInterfaceClass: Communication Interface Class */
  0x02,                                       /* bInterfaceSubClass: Abstract Control Model */
  0x01,                                       /* bInterfaceProtocol: Common AT commands */
  0x00,                                       /* iInterface */

  /* Header Functional Descriptor */
  0x05,                                       /* bLength: Endpoint Descriptor size */
  USBD_CDC_CS_INTERFACE,                      /* bDescriptorType: CS_INTERFACE */
  0x00,                                       /* bDescriptorSubtype: Header Func Desc */
  0x10,                                       /* bcdCDC: spec release number */
  0x01,

  /* Call Management Functional Descriptor */
  0x05,                                       /* bFunctionLength */
  USBD_CDC_CS_INTERFACE,                      /* bDescriptorType: CS_INTERFACE */
  0x01,                                       /* bDescriptorSubtype: Call Management Func Desc */
  0x00,                                       /* bmCapabilities: D0+D1 */
  0x01,                                       /* bDataInterface */

  /* ACM Functional Descriptor */
  0x04,                                       /* bFunctionLength */
  USBD_CDC_CS_INTERFACE,                      /* bDescriptorType: CS_INTERFACE */
  0x02,                                       /* bDescriptorSubtype: Abstract Control Management desc */
  0x02,                                       /* bmCapabilities */

  /* Union Functional Descriptor */
  0x05,                                       /* bFunctionLength */
  USBD_CDC_CS_INTERFACE,                      /* bDescriptorType: CS_INTERFACE */
  0x06,                                       /* bDescriptorSubtype: Union func desc */
  0x00,                                       /* bMasterInterface: Communication class interface */
  0x01,                                       /* bSlaveInterface0: Data Class Interface */

  /* Endpoint 2 Descriptor */
  0x07,                                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,                     /* bDescriptorType: Endpoint */
  CDC_CMD_EP,                                 /* bEndpointAddress */
  0x03,                                       /* bmAttributes: Interrupt */
  LOBYTE(CDC_CMD_PACKET_SIZE),                /* wMaxPacketSize */
  HIBYTE(CDC_CMD_PACKET_SIZE),
  CDC_FS_BINTERVAL,                           /* bInterval */
  /*---------------------------------------------------------------------------*/

  /* Data class interface descriptor */
  0x09,                                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_INTERFACE,                    /* bDescriptorType: */
  0x01,                                       /* bInterfaceNumber: Number of Interface */
  0x00,                                       /* bAlternateSetting: Alternate setting */
  0x02,                                       /* bNumEndpoints: Two endpoints used */
  0x0A,                                       /* bInterfaceClass: CDC */
  0x00,                                       /* bInterfaceSubClass */
  0x00,                                       /* bInterfaceProtocol */
  0x00,                                       /* iInterface */

  /* Endpoint OUT Descriptor */
  0x07,                                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,                     /* bDescriptorType: Endpoint */
  CDC_OUT_EP,                                 /* bEndpointAddress */
  0x02,                                       /* bmAttributes: Bulk */
  LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),        /* wMaxPacketSize */
  HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
  0x00,                                       /* bInterval */

  /* Endpoint IN Descriptor */
  0x07,                                       /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,                     /* bDescriptorType: Endpoint */
  CDC_IN_EP,                                  /* bEndpointAddress */
  0x02,                                       /* bmAttributes: Bulk */
  LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),        /* wMaxPacketSize */
  HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
  0x00                                        /* bInterval */
};
#endif /* USE_USBD_COMPOSITE  */

static uint8_t CDCInEpAdd = CDC_IN_EP;
static uint8_t CDCOutEpAdd = CDC_OUT_EP;
static uint8_t CDCCmdEpAdd = CDC_CMD_EP;

/**
  * @}
  */

/** @defgroup USBD_CDC_Private_Functions
  * @{
  */

/**
  * @brief  USBD_CDC_Init
  *         Initialize the CDC interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_CDC_Init(USBD_HandleTypeDef *pdev)
{
  USBD_CDC_HandleTypeDef *hcdc;

  hcdc = (USBD_CDC_HandleTypeDef *)USBD_malloc(sizeof(USBD_CDC_HandleTypeDef));

  if (hcdc == NULL)
  {
#ifdef USE_USBD_COMPOSITE
    pdev->pClassDataCmsit[pdev->classId] = NULL;
#endif /* USE_USBD_COMPOSITE */
    pdev->pClassData = NULL;
    return (uint8_t)USBD_EMEM;
  }

  (void)USBD_memset(hcdc, 0, sizeof(USBD_CDC_HandleTypeDef));

  /* Store the class handle. In composite mode the per-class slot is the
   * authoritative location; pClassData is kept as a fallback alias. */
#ifdef USE_USBD_COMPOSITE
  pdev->pClassDataCmsit[pdev->classId] = (void *)hcdc;
  pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];
#else
  pdev->pClassData = (void *)hcdc;
#endif /* USE_USBD_COMPOSITE */
#ifdef USE_USBD_COMPOSITE
  /* Get the Endpoints addresses allocated for this class instance */
  CDCInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  CDCOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  CDCCmdEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

    /* Open EP IN */
    (void)USBD_LL_OpenEP(pdev, CDCInEpAdd, USBD_EP_TYPE_BULK,
                         CDC_DATA_IN_PACKET_SIZE);

    pdev->ep_in[CDCInEpAdd & 0xFU].is_used = 1U;

    /* Open EP OUT */
    (void)USBD_LL_OpenEP(pdev, CDCOutEpAdd, USBD_EP_TYPE_BULK,
                         CDC_DATA_OUT_PACKET_SIZE);

    pdev->ep_out[CDCOutEpAdd & 0xFU].is_used = 1U;

    /* Set bInterval for CDC CMD Endpoint */
    pdev->ep_in[CDCCmdEpAdd & 0xFU].bInterval = CDC_BINTERVAL;

  /* Open Command IN EP */
  (void)USBD_LL_OpenEP(pdev, CDCCmdEpAdd, USBD_EP_TYPE_INTR, CDC_CMD_PACKET_SIZE);
  pdev->ep_in[CDCCmdEpAdd & 0xFU].is_used = 1U;

  hcdc->RxBuffer = NULL;

  /* Init  physical Interface components */
#ifdef USE_USBD_COMPOSITE
  ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->Init();
#else
  USBD_CDC_fops.Init();
  /* ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->Init(); */
#endif /* USE_USBD_COMPOSITE */

  /* Init Xfer states */
  hcdc->TxState = 0U;
  hcdc->RxState = 0U;

  if (hcdc->RxBuffer == NULL)
  {
    return (uint8_t)USBD_EMEM;
  }

  /* Prepare Out endpoint to receive next packet */
  (void)USBD_LL_PrepareReceive(pdev, CDCOutEpAdd, hcdc->RxBuffer,
                                 CDC_DATA_OUT_PACKET_SIZE);


  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_CDC_DeInit
  *         DeInitialize the CDC layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t USBD_CDC_DeInit(USBD_HandleTypeDef *pdev)
{


#ifdef USE_USBD_COMPOSITE
  /* Get the Endpoints addresses allocated for this CDC class instance */
  CDCInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  CDCOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  CDCCmdEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  /* Close EP IN */
  (void)USBD_LL_CloseEP(pdev, CDCInEpAdd);
  pdev->ep_in[CDCInEpAdd & 0xFU].is_used = 0U;

  /* Close EP OUT */
  (void)USBD_LL_CloseEP(pdev, CDCOutEpAdd);
  pdev->ep_out[CDCOutEpAdd & 0xFU].is_used = 0U;

  /* Close Command IN EP */
  (void)USBD_LL_CloseEP(pdev, CDCCmdEpAdd);
  pdev->ep_in[CDCCmdEpAdd & 0xFU].is_used = 0U;
  pdev->ep_in[CDCCmdEpAdd & 0xFU].bInterval = 0U;

  /* DeInit  physical Interface components */
/*  if (pdev->pClassDataCmsit[pdev->classId] != NULL)
  {
    ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->DeInit();
    (void)USBD_free(pdev->pClassDataCmsit[pdev->classId]);
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    pdev->pClassData = NULL;
  } */

  /* DeInit  physical Interface components */
#ifdef USE_USBD_COMPOSITE
  ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->DeInit();
  (void)USBD_free(pdev->pClassDataCmsit[pdev->classId]);
  pdev->pClassDataCmsit[pdev->classId] = NULL;
  pdev->pClassData = NULL;
#else
  USBD_CDC_fops.DeInit();
  /* (void)USBD_free(pdev->pClassDataCmsit[pdev->classId]);
  pdev->pClassDataCmsit[pdev->classId] = NULL; */
  (void)USBD_free(pdev->pClassData);
  pdev->pClassData = NULL;
#endif /* USE_USBD_COMPOSITE */
 
  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_CDC_Setup
  *         Handle the CDC specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
static uint8_t USBD_CDC_Setup(USBD_HandleTypeDef *pdev,
                              USBD_SetupReqTypeDef *req)
{
  /* USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId]; */
#ifdef USE_USBD_COMPOSITE
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
#else
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
#endif /* USE_USBD_COMPOSITE */
  uint16_t len;
  uint8_t ifalt = 0U;
  uint16_t status_info = 0U;
  USBD_StatusTypeDef ret = USBD_OK;

  if (hcdc == NULL)
  {
    USBD_CtlError(pdev, req);
    return (uint8_t)USBD_FAIL;
  }

#ifdef USE_USBD_COMPOSITE
  if (pdev->pUserData[pdev->classId] == NULL)
  {
    USBD_CtlError(pdev, req);
    return (uint8_t)USBD_FAIL;
  }
#endif /* USE_USBD_COMPOSITE */

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_CLASS:
      if (USBD_CDC_IsValidClassRequest(pdev, req) == 0U)
      {
        USBD_CtlError(pdev, req);
        ret = USBD_FAIL;
        break;
      }

      if (req->wLength != 0U)
      {
        if ((req->bmRequest & 0x80U) != 0U)
        {
#ifdef USE_USBD_COMPOSITE
          ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->Control(req->bRequest,
                                                                           (uint8_t *)hcdc->data,
                                                                           req->wLength);
#else
          USBD_CDC_fops.Control(req->bRequest, (uint8_t *)hcdc->data,  req->wLength);
        /*  ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->Control(req->bRequest,
                                                                           (uint8_t *)hcdc->data,
                                                                           req->wLength); */
#endif /* USE_USBD_COMPOSITE */

          len = MIN(CDC_REQ_MAX_DATA_SIZE, req->wLength);
          (void)USBD_CtlSendData(pdev, (uint8_t *)hcdc->data, len);
        }
        else
        {
          hcdc->CmdOpCode = req->bRequest;
          hcdc->CmdLength = (uint8_t)MIN(req->wLength, USB_MAX_EP0_SIZE);

          (void)USBD_CtlPrepareRx(pdev, (uint8_t *)hcdc->data, hcdc->CmdLength);
        }
      }
      else
      {
#ifdef USE_USBD_COMPOSITE
        ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->Control(req->bRequest, (uint8_t *)req, 0U);
#else
        USBD_CDC_fops.Control(req->bRequest, (uint8_t *)req, 0U);
      /*  ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->Control(req->bRequest,
                                                                         (uint8_t *)req, 0U); */
#endif /* USE_USBD_COMPOSITE */
      }
      break;

    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_STATUS:
          if ((req->bmRequest != (USB_REQ_DIR_DTH | USB_REQ_TYPE_STANDARD |
                                  USB_REQ_RECIPIENT_INTERFACE)) ||
              (req->wValue != 0U) || (req->wLength != 2U) ||
              (USBD_CDC_IsValidInterfaceIndex(pdev, req->wIndex) == 0U))
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          else if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_GET_INTERFACE:
          if ((req->bmRequest != (USB_REQ_DIR_DTH | USB_REQ_TYPE_STANDARD |
                                  USB_REQ_RECIPIENT_INTERFACE)) ||
              (req->wValue != 0U) || (req->wLength != 1U) ||
              (USBD_CDC_IsValidInterfaceIndex(pdev, req->wIndex) == 0U))
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          else if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, &ifalt, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE:
          if ((req->bmRequest != (USB_REQ_DIR_HTD | USB_REQ_TYPE_STANDARD |
                                  USB_REQ_RECIPIENT_INTERFACE)) ||
              (req->wValue != 0U) || (req->wLength != 0U) ||
              (USBD_CDC_IsValidInterfaceIndex(pdev, req->wIndex) == 0U) ||
              (pdev->dev_state != USBD_STATE_CONFIGURED))
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          if ((req->bmRequest != (USB_REQ_DIR_HTD | USB_REQ_TYPE_STANDARD |
                                  USB_REQ_RECIPIENT_ENDPOINT)) ||
              (req->wValue != USB_FEATURE_EP_HALT) || (req->wLength != 0U) ||
              ((req->wIndex & 0xFF70U) != 0U) ||
              ((LOBYTE(req->wIndex) & 0x7FU) == 0U))
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    default:
      USBD_CtlError(pdev, req);
      ret = USBD_FAIL;
      break;
  }

  return (uint8_t)ret;
}

static uint8_t USBD_CDC_IsValidInterfaceIndex(const USBD_HandleTypeDef *pdev,
                                              uint16_t index)
{
#ifdef USE_USBD_COMPOSITE
  uint32_t interface_index;

  if ((pdev->classId >= USBD_MAX_SUPPORTED_CLASS) ||
      (pdev->tclasslist[pdev->classId].Active == 0U) ||
      (pdev->tclasslist[pdev->classId].NumIf == 0U) ||
      (pdev->tclasslist[pdev->classId].NumIf > USBD_MAX_CLASS_INTERFACES))
  {
    return 0U;
  }

  for (interface_index = 0U;
       interface_index < pdev->tclasslist[pdev->classId].NumIf;
       interface_index++)
  {
    if (index == pdev->tclasslist[pdev->classId].Ifs[interface_index])
    {
      return 1U;
    }
  }
  return 0U;
#else
  UNUSED(pdev);
  return (index <= 1U) ? 1U : 0U;
#endif /* USE_USBD_COMPOSITE */
}

static uint8_t USBD_CDC_IsValidCommunicationInterfaceIndex(
    const USBD_HandleTypeDef *pdev, uint16_t index)
{
#ifdef USE_USBD_COMPOSITE
  if ((pdev->classId >= USBD_MAX_SUPPORTED_CLASS) ||
      (pdev->tclasslist[pdev->classId].Active == 0U) ||
      (pdev->tclasslist[pdev->classId].NumIf == 0U))
  {
    return 0U;
  }
  return (index == pdev->tclasslist[pdev->classId].Ifs[0]) ? 1U : 0U;
#else
  UNUSED(pdev);
  return (index == 0U) ? 1U : 0U;
#endif /* USE_USBD_COMPOSITE */
}

static uint8_t USBD_CDC_IsValidClassRequest(const USBD_HandleTypeDef *pdev,
                                            const USBD_SetupReqTypeDef *req)
{
  if (((req->bmRequest & USB_REQ_RECIPIENT_MASK) != USB_REQ_RECIPIENT_INTERFACE) ||
      (USBD_CDC_IsValidCommunicationInterfaceIndex(pdev, req->wIndex) == 0U))
  {
    return 0U;
  }

  switch (req->bRequest)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:
      return (((req->bmRequest & USB_REQ_DIR_DTH) == USB_REQ_DIR_HTD) &&
              (req->wValue == 0U) && (req->wLength != 0U) &&
              (req->wLength <= USB_MAX_EP0_SIZE)) ? 1U : 0U;

    case CDC_GET_ENCAPSULATED_RESPONSE:
      return (((req->bmRequest & USB_REQ_DIR_DTH) == USB_REQ_DIR_DTH) &&
              (req->wValue == 0U) && (req->wLength != 0U) &&
              (req->wLength <= USB_MAX_EP0_SIZE)) ? 1U : 0U;

    case CDC_SET_COMM_FEATURE:
      return (((req->bmRequest & USB_REQ_DIR_DTH) == USB_REQ_DIR_HTD) &&
              ((req->wValue == 1U) || (req->wValue == 2U)) &&
              (req->wLength == 2U)) ? 1U : 0U;

    case CDC_GET_COMM_FEATURE:
      return (((req->bmRequest & USB_REQ_DIR_DTH) == USB_REQ_DIR_DTH) &&
              ((req->wValue == 1U) || (req->wValue == 2U)) &&
              (req->wLength == 2U)) ? 1U : 0U;

    case CDC_CLEAR_COMM_FEATURE:
      return (((req->bmRequest & USB_REQ_DIR_DTH) == USB_REQ_DIR_HTD) &&
              ((req->wValue == 1U) || (req->wValue == 2U)) &&
              (req->wLength == 0U)) ? 1U : 0U;

    case CDC_SET_LINE_CODING:
      return (((req->bmRequest & USB_REQ_DIR_DTH) == USB_REQ_DIR_HTD) &&
              (req->wValue == 0U) && (req->wLength == CDC_REQ_MAX_DATA_SIZE)) ? 1U : 0U;

    case CDC_GET_LINE_CODING:
      return (((req->bmRequest & USB_REQ_DIR_DTH) == USB_REQ_DIR_DTH) &&
              (req->wValue == 0U) && (req->wLength == CDC_REQ_MAX_DATA_SIZE)) ? 1U : 0U;

    case CDC_SET_CONTROL_LINE_STATE:
      return (((req->bmRequest & USB_REQ_DIR_DTH) == USB_REQ_DIR_HTD) &&
              ((req->wValue & 0xFFFCU) == 0U) && (req->wLength == 0U)) ? 1U : 0U;

    case CDC_SEND_BREAK:
      return (((req->bmRequest & USB_REQ_DIR_DTH) == USB_REQ_DIR_HTD) &&
              (req->wLength == 0U)) ? 1U : 0U;

    default:
      return 0U;
  }
}

/**
  * @brief  USBD_CDC_DataIn
  *         Data sent on non-control IN endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t USBD_CDC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_CDC_HandleTypeDef *hcdc;
  PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;

  /* if (pdev->pClassDataCmsit[pdev->classId] == NULL) */
#ifdef USE_USBD_COMPOSITE
  if (pdev->pClassDataCmsit[pdev->classId] == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
#else
  if (pdev->pClassData == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  /* hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId]; */
  hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
#endif /* USE_USBD_COMPOSITE */

  if ((pdev->ep_in[epnum & 0xFU].total_length > 0U) &&
      ((pdev->ep_in[epnum & 0xFU].total_length % hpcd->IN_ep[epnum & 0xFU].maxpacket) == 0U))
  {
    /* Update the packet total length */
    pdev->ep_in[epnum & 0xFU].total_length = 0U;

    /* Send ZLP */
    (void)USBD_LL_Transmit(pdev, epnum, NULL, 0U);
  }
  else
  {
    hcdc->TxState = 0U;

#ifdef USE_USBD_COMPOSITE
    if (((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->TransmitCplt != NULL)
    {
      ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->TransmitCplt(hcdc->TxBuffer, &hcdc->TxLength, epnum);
    }
#else
   /* if (((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->TransmitCplt != NULL)
    {
      ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->TransmitCplt(hcdc->TxBuffer, &hcdc->TxLength, epnum);
    } */
    if (USBD_CDC_fops.TransmitCplt != NULL)
    {
      USBD_CDC_fops.TransmitCplt(hcdc->TxBuffer, &hcdc->TxLength, epnum);
    }
#endif /* USE_USBD_COMPOSITE */
  }

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_CDC_DataOut
  *         Data received on non-control Out endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t USBD_CDC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
#ifdef USE_USBD_COMPOSITE
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (pdev->pClassDataCmsit[pdev->classId] == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }
#else
  /* USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId]; */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;

  /* if (pdev->pClassDataCmsit[pdev->classId] == NULL) */
  if (pdev->pClassData == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }
#endif /* USE_USBD_COMPOSITE */

  /* Get the received data length */
  hcdc->RxLength = USBD_LL_GetRxDataSize(pdev, epnum);

  /* USB data will be immediately processed, this allow next USB traffic being
  NAKed till the end of the application Xfer */

#ifdef USE_USBD_COMPOSITE
  ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->Receive(hcdc->RxBuffer, &hcdc->RxLength);
#else
  USBD_CDC_fops.Receive(hcdc->RxBuffer, &hcdc->RxLength);
  /* ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->Receive(hcdc->RxBuffer, &hcdc->RxLength); */
#endif /* USE_USBD_COMPOSITE */

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_CDC_EP0_RxReady
  *         Handle EP0 Rx Ready event
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t USBD_CDC_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
  /* USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId]; */
#ifdef USE_USBD_COMPOSITE
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
#else
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
#endif /* USE_USBD_COMPOSITE */
  if (hcdc == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

#ifdef USE_USBD_COMPOSITE
  if ((pdev->pUserData[pdev->classId] != NULL) && (hcdc->CmdOpCode != 0xFFU))
  {
    ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->Control(hcdc->CmdOpCode,
                                                                     (uint8_t *)hcdc->data,
                                                                     (uint16_t)hcdc->CmdLength);
    hcdc->CmdOpCode = 0xFFU;
  }
#else
 /* if ((pdev->pUserData[pdev->classId] != NULL) && (hcdc->CmdOpCode != 0xFFU))
  {
    ((USBD_CDC_ItfTypeDef *)pdev->pUserData[pdev->classId])->Control(hcdc->CmdOpCode,
                                                                     (uint8_t *)hcdc->data,
                                                                     (uint16_t)hcdc->CmdLength);
    hcdc->CmdOpCode = 0xFFU;
  } */
  if (hcdc->CmdOpCode != 0xFFU)
  {
    USBD_CDC_fops.Control(hcdc->CmdOpCode, (uint8_t *)hcdc->data, (uint16_t)hcdc->CmdLength);
    hcdc->CmdOpCode = 0xFFU;
  }
#endif /* USE_USBD_COMPOSITE */

  return (uint8_t)USBD_OK;
}
#ifndef USE_USBD_COMPOSITE
/**
  * @brief  USBD_CDC_GetFSCfgDesc
  *         Return configuration descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_CDC_GetFSCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpCmdDesc = USBD_GetEpDesc(USBD_CDC_CfgDesc, CDC_CMD_EP);
  USBD_EpDescTypeDef *pEpOutDesc = USBD_GetEpDesc(USBD_CDC_CfgDesc, CDC_OUT_EP);
  USBD_EpDescTypeDef *pEpInDesc = USBD_GetEpDesc(USBD_CDC_CfgDesc, CDC_IN_EP);

  if (pEpCmdDesc != NULL)
  {
    pEpCmdDesc->bInterval = CDC_FS_BINTERVAL;
  }

  if (pEpOutDesc != NULL)
  {
    pEpOutDesc->wMaxPacketSize = CDC_DATA_FS_MAX_PACKET_SIZE;
  }

  if (pEpInDesc != NULL)
  {
    pEpInDesc->wMaxPacketSize = CDC_DATA_FS_MAX_PACKET_SIZE;
  }

  *length = (uint16_t)sizeof(USBD_CDC_CfgDesc);
  return USBD_CDC_CfgDesc;
}

/**
  * @brief  USBD_CDC_GetHSCfgDesc
  *         Return configuration descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_CDC_GetHSCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpCmdDesc = USBD_GetEpDesc(USBD_CDC_CfgDesc, CDC_CMD_EP);
  USBD_EpDescTypeDef *pEpOutDesc = USBD_GetEpDesc(USBD_CDC_CfgDesc, CDC_OUT_EP);
  USBD_EpDescTypeDef *pEpInDesc = USBD_GetEpDesc(USBD_CDC_CfgDesc, CDC_IN_EP);

  if (pEpCmdDesc != NULL)
  {
    pEpCmdDesc->bInterval = CDC_BINTERVAL;
  }

  if (pEpOutDesc != NULL)
  {
    pEpOutDesc->wMaxPacketSize = CDC_DATA_MAX_PACKET_SIZE;
  }

  if (pEpInDesc != NULL)
  {
    pEpInDesc->wMaxPacketSize = CDC_DATA_MAX_PACKET_SIZE;
  }

  *length = (uint16_t)sizeof(USBD_CDC_CfgDesc);
  return USBD_CDC_CfgDesc;
}

/**
  * @brief  USBD_CDC_GetOtherSpeedCfgDesc
  *         Return configuration descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t *USBD_CDC_GetOtherSpeedCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpCmdDesc = USBD_GetEpDesc(USBD_CDC_CfgDesc, CDC_CMD_EP);
  USBD_EpDescTypeDef *pEpOutDesc = USBD_GetEpDesc(USBD_CDC_CfgDesc, CDC_OUT_EP);
  USBD_EpDescTypeDef *pEpInDesc = USBD_GetEpDesc(USBD_CDC_CfgDesc, CDC_IN_EP);

  if (pEpCmdDesc != NULL)
  {
    pEpCmdDesc->bInterval = CDC_BINTERVAL;
  }

  if (pEpOutDesc != NULL)
  {
    pEpOutDesc->wMaxPacketSize = CDC_DATA_MAX_PACKET_SIZE;
  }

  if (pEpInDesc != NULL)
  {
    pEpInDesc->wMaxPacketSize = CDC_DATA_MAX_PACKET_SIZE;
  }

  *length = (uint16_t)sizeof(USBD_CDC_CfgDesc);
  return USBD_CDC_CfgDesc;
}

/**
  * @brief  USBD_CDC_GetDeviceQualifierDescriptor
  *         return Device Qualifier descriptor
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
uint8_t *USBD_CDC_GetDeviceQualifierDescriptor(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_CDC_DeviceQualifierDesc);

  return USBD_CDC_DeviceQualifierDesc;
}
#endif /* USE_USBD_COMPOSITE  */
/**
  * @brief  USBD_CDC_RegisterInterface
  * @param  pdev: device instance
  * @param  fops: CD  Interface callback
  * @retval status
  */
uint8_t USBD_CDC_RegisterInterface(USBD_HandleTypeDef *pdev,
                                   USBD_CDC_ItfTypeDef *fops)
{
  if (fops == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  pdev->pUserData[pdev->classId] = fops;

  return (uint8_t)USBD_OK;
}


/**
  * @brief  USBD_CDC_SetTxBuffer
  * @param  pdev: device instance
  * @param  pbuff: Tx Buffer
  * @param  length: length of data to be sent
  * @param  ClassId: The Class ID
  * @retval status
  */
#ifdef USE_USBD_COMPOSITE
uint8_t USBD_CDC_SetTxBuffer(USBD_HandleTypeDef *pdev,
                             uint8_t *pbuff, uint32_t length, uint8_t ClassId)
{
  USBD_CDC_HandleTypeDef *hcdc;

  /* Reject out-of-range ClassId before indexing the class data table */
  if (ClassId >= USBD_MAX_SUPPORTED_CLASS)
  {
    return (uint8_t)USBD_FAIL;
  }

  hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[ClassId];
#else
uint8_t USBD_CDC_SetTxBuffer(USBD_HandleTypeDef *pdev,
                             uint8_t *pbuff, uint32_t length)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
#endif /* USE_USBD_COMPOSITE */

  if (hcdc == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  hcdc->TxBuffer = pbuff;
  hcdc->TxLength = length;

  return (uint8_t)USBD_OK;
}

/**
  * @brief  USBD_CDC_SetRxBuffer
  * @param  pdev: device instance
  * @param  pbuff: Rx Buffer
  * @param  ClassId: The Class ID
  * @retval status
  */
#ifdef USE_USBD_COMPOSITE
uint8_t USBD_CDC_SetRxBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuff,
                             uint8_t ClassId)
{
  USBD_CDC_HandleTypeDef *hcdc;

  /* Reject out-of-range ClassId before indexing the class data table */
  if (ClassId >= USBD_MAX_SUPPORTED_CLASS)
  {
    return (uint8_t)USBD_FAIL;
  }

  hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[ClassId];
#else
uint8_t USBD_CDC_SetRxBuffer(USBD_HandleTypeDef *pdev, uint8_t *pbuff)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
#endif /* USE_USBD_COMPOSITE */
  if (hcdc == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  hcdc->RxBuffer = pbuff;

  return (uint8_t)USBD_OK;
}


/**
  * @brief  USBD_CDC_TransmitPacket
  *         Transmit packet on IN endpoint
  * @param  pdev: device instance
  * @param  ClassId: The Class ID
  * @retval status
  */
#ifdef USE_USBD_COMPOSITE
uint8_t USBD_CDC_TransmitPacket(USBD_HandleTypeDef *pdev, uint8_t ClassId)
{
  USBD_CDC_HandleTypeDef *hcdc;

  /* Reject out-of-range ClassId before indexing the class data table */
  if (ClassId >= USBD_MAX_SUPPORTED_CLASS)
  {
    return (uint8_t)USBD_FAIL;
  }

  hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[ClassId];
#else
uint8_t USBD_CDC_TransmitPacket(USBD_HandleTypeDef *pdev)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
#endif  /* USE_USBD_COMPOSITE */

  USBD_StatusTypeDef ret = USBD_BUSY;

#ifdef USE_USBD_COMPOSITE
  /* Get the Endpoints addresses allocated for this class instance */
  CDCInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_BULK, ClassId);
#endif  /* USE_USBD_COMPOSITE */

  if (hcdc == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

#ifdef USE_USBD_COMPOSITE
  /* No bulk IN endpoint allocated for this class instance */
  if (CDCInEpAdd == 0xFFU)
  {
    return (uint8_t)USBD_FAIL;
  }
#endif  /* USE_USBD_COMPOSITE */

  if (hcdc->TxState == 0U)
  {
    /* Tx Transfer in progress */
    hcdc->TxState = 1U;

    /* Update the packet total length */
    pdev->ep_in[CDCInEpAdd & 0xFU].total_length = hcdc->TxLength;

    /* Transmit next packet */
    (void)USBD_LL_Transmit(pdev, CDCInEpAdd, hcdc->TxBuffer, hcdc->TxLength);

    ret = USBD_OK;
  }

  return (uint8_t)ret;
}

/**
  * @brief  USBD_CDC_ReceivePacket
  *         prepare OUT Endpoint for reception
  * @param  pdev: device instance
  * @param  ClassId: The Class ID
  * @retval status
  */
#ifdef USE_USBD_COMPOSITE
uint8_t USBD_CDC_ReceivePacket(USBD_HandleTypeDef *pdev, uint8_t ClassId)
{
  USBD_CDC_HandleTypeDef *hcdc;

  /* Reject out-of-range ClassId before indexing the class data table */
  if (ClassId >= USBD_MAX_SUPPORTED_CLASS)
  {
    return (uint8_t)USBD_FAIL;
  }

  hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassDataCmsit[ClassId];
#else
uint8_t USBD_CDC_ReceivePacket(USBD_HandleTypeDef *pdev)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)pdev->pClassData;
#endif /* USE_USBD_COMPOSITE */

  if (hcdc == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

#ifdef USE_USBD_COMPOSITE
  /* Get the Endpoints addresses allocated for this class instance */
  CDCOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, ClassId);

  /* No bulk OUT endpoint allocated for this class instance */
  if (CDCOutEpAdd == 0xFFU)
  {
    return (uint8_t)USBD_FAIL;
  }
#endif /* USE_USBD_COMPOSITE */

    /* Prepare Out endpoint to receive next packet */
    (void)USBD_LL_PrepareReceive(pdev, CDCOutEpAdd, hcdc->RxBuffer,
                                 CDC_DATA_OUT_PACKET_SIZE);

  return (uint8_t)USBD_OK;
}
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
