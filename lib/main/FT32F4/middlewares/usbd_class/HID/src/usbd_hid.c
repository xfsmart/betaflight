/**
  ******************************************************************************
  * @file    			usbd_hid.c
  * @author  			FMD XA
  * @brief   			This file provides the HID core functions.
  * @version 			V1.0.0           
  * @data		 			2025-04-21
  ******************************************************************************
  *
  * =========================================================
  *                     HID Class Description
  * =========================================================
  * This module manages the HID class v1.11 following the "deivce class definition
  * for human interface device (HID) version 1.11 Yun 27, 2001".
  * This driver implements the following aspects of the specification:
  * - the boot interface subclass
  * - the Mouse protocols
  * - usage page : generic desktop
  * - usage : joystick
  * - collection : application
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_hid.h"
#include "usbd_ctlreq.h"


/**@defgroup USBD_HID_CORE
 * @brief usbd core module.
 * @{
 */

/**@defgroup USBD_HID_CORE_Private_TypeDefinitions
 * @{
 */

/**
 * @}
 */


/**@defgroup USBD_HID_CORE_Private_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_HID_CORE_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_HID_CORE_Private_Variables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_HID_CORE_Private_FunctionPrototypes
 * @{
 */
static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev);
static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev);
static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req);
static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev);
#ifndef USE_USBD_COMPOSITE
static uint8_t *USBD_HID_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length);
#endif  /* USE_USBD_COMPOSITE */

/**
 * @}
 */

/**@defgroup USBD_HID_CORE_Private_Variables
 * @{
 */
USBD_ClassTypeDef USBD_HID =
{
  USBD_HID_Init,
  USBD_HID_DeInit,
  USBD_HID_Setup,
  NULL,               /* EP0_TxSent */
  NULL,               /* EP0_RxReady */
  USBD_HID_DataIn,    /* DataIn */
  NULL,               /* DataOut */
  NULL,               /* SOF */
  NULL,
  NULL,
#ifdef USE_USBD_COMPOSITE
  NULL,
  NULL,
  NULL,
  NULL,
#else
  USBD_HID_GetHSCfgDesc,
  USBD_HID_GetFSCfgDesc,
  USBD_HID_GetOtherSpeedCfgDesc,
  USBD_HID_GetDeviceQualifierDesc,
#endif  /* USE_USBD_COMPOSITE */
};

#ifndef USE_USBD_COMPOSITE
/* USB HID device FS Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_HID_CfgDesc[USB_HID_CONFIG_DESC_SIZ] __ALIGN_END =
{
  0x09,                             /* bLength: configuration descriptor size */
  USB_DESC_TYPE_CONFIGURATION,      /* bDescriptorType: configuration */
  USB_HID_CONFIG_DESC_SIZ,          /* wTotalLength: Bytes returned */
  0x00,
  0x01,                             /* bNumInterfaces: 1 interface */
  0x01,                             /* bConfigurationValue: configuration value */
  0x00,                             /* iConfiguration: index of string desriptor describing the configuration */
#if (USBD_SELF_POWERED == 1U)
  0xE0,                             /* bmAttributes: bus powered according to user configuration */
#else
  0xA0,                             /* bmAttributes: bus powered according to user configuration */
#endif  /* USBD_SELF_POWERD */
  USBD_MAX_POWER,                   /* MaxPower (mA) */

  /**************Descriptor of joystick mouse interface**************************/
  /* 09 */
  0x09,                             /* bLength: interface descriptor size */
  USB_DESC_TYPE_INTERFACE,          /* bDescriptorType: interface descriptor type */
  0x00,                             /* bInterfaceNumber: number of interface */
  0x00,                             /* bAlternateSetting: alternate setting */
  0x01,                             /* bNumEndpoints */
  0x03,                             /* bInterfaceClass: HID */
  0x01,                             /* bInterfaceSubClass: 1=BOOT, 0=no boot */
  0x02,                             /* nInterfaceProtocol: 0=none, 1=keyboard, 2=mouse */
  0,                                /* iInterface: Index of string descriptor */
  /**************Descriptor of joystick mouse HID********************************/
  /* 18 */
  0x09,                             /* bLength: HID Descriptor size */
  HID_DESCRIPTOR_TYPE,              /* bDescriptorType: HID */
  0x11,                             /* bcdHID: HID class spec release number */
  0x01,
  0x00,                             /* bCountryCode: Hardware traget country */
  0x01,                             /* bNumDescriptors: Number of HID class descriptors to follow */
  0x22,                             /* bDescriptorType */
  HID_MOUSE_REPORT_DESC_SIZE,       /* wItemLength: Total length of report descriptor */
  0x00,
  /**************Descriptor of mouse endpoint************************************/
  /* 27 */
  0x07,                             /* bLength: endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,           /* bDescriptorType: endpoint descriptor type */
  HID_EPIN_ADDR,                    /* bEndpointAddress: endpoint address (IN) */
  0x03,                             /* bmAttributes: Interrupt endpoint */
  HID_EPIN_SIZE,                    /* wMaxPacketSize: 4 bytes max */
  0x00,
  HID_FS_BINTERVAL,                 /* bInterval: polling interval */
  /* 34 */
};
#endif  /* USE_USBD_COMPOSITE */

/* USB HID device configuration descriptor */
__ALIGN_BEGIN static uint8_t USBD_HID_Desc[USB_HID_DESC_SIZ] __ALIGN_END =
{
  /* 18 */
  0x09,                             /* bLength: HID descriptor size */
  HID_DESCRIPTOR_TYPE,              /* bDescriptorType: HID */
  0x11,                             /* bcdHID: HID class spec release number */
  0x01,
  0x00,                             /* bCountryCode: hardware target country */
  0x01,                             /* bNumDescriptors: number of HID class descriptors to follow */
  0x22,                             /* bDescriptorType */
  HID_MOUSE_REPORT_DESC_SIZE,       /* wItemLength: total length of report descriptor */
  0x00,                             /* MaxPower (mA) */
};

#ifndef USE_USBD_COMPOSITE
/* USB standard device descriptor */
__ALIGN_BEGIN static uint8_t USBD_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
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
#endif /* USE_USBD_COMPOSITE */

__ALIGN_BEGIN static uint8_t HID_MOUSE_ReportDesc[HID_MOUSE_REPORT_DESC_SIZE] __ALIGN_END =
{
  0x05, 0x01,         /* usage page (generic desktop ctrls)     */
  0x09, 0x02,         /* usage(mouse)                           */
  0xA1, 0x01,         /* collection (application)               */
  0x09, 0x01,         /*  usage (pointer)                       */
  0xA1, 0x00,         /*  collection (physical)                 */
  0x05, 0x09,         /*    usage page (button)                 */
  0x19, 0x01,         /*    usage minimum (0x01)                */
  0x29, 0x03,         /*    usage maximum (0x03)                */
  0x15, 0x00,         /*    logical minimum (0)                 */
  0x25, 0x01,         /*    logical maximum (1)                 */
  0x95, 0x03,         /*    report count (3)                    */
  0x75, 0x01,         /*    report size (1)                     */
  0x81, 0x02,         /*    input (data, var, abs)              */
  0x95, 0x01,         /*    report count (1)                    */
  0x75, 0x05,         /*    report size (5)                     */
  0x81, 0x01,         /*    input (const, var, abs)             */
  0x05, 0x01,         /*    usage page (generic desktop ctrls)  */
  0x09, 0x30,         /*    usage (X)                           */
  0x09, 0x31,         /*    usage (Y)                           */
  0x09, 0x38,         /*    usage (wheel)                       */
  0x15, 0x81,         /*    logical minimum (-127)              */
  0x25, 0x7F,         /*    logical maximum (127)               */
  0x75, 0x08,         /*    report size (8)                     */
  0x95, 0x03,         /*    report count (3)                    */
  0x81, 0x06,         /*    input (data, var, rel)              */
  0xC0,               /*  end collection                        */
  0x09, 0x3C,         /*  usage (motion wakeup)                 */
  0x05, 0xFF,         /*  usage page (reserved 0xFF)            */
  0x09, 0x01,         /*  usage (0x01)                          */
  0x15, 0x00,         /*  logical minimum (0)                   */
  0x25, 0x01,         /*  logical maximum (1)                   */
  0x75, 0x01,         /*  report size (1)                       */
  0x95, 0x02,         /*  report count (2)                      */
  0xB1, 0x22,         /*  feature (data, var, abs, nowrp)       */
  0x75, 0x06,         /*  report size (6)                       */
  0x95, 0x01,         /*  report count (1)                      */
  0xB1, 0x01,         /*  feature (const, array, abs, nowrp)    */
  0xC0                /* end collection                         */
};

static uint8_t HIDInEpAdd = HID_EPIN_ADDR;

/**
 * @}
 */
/**@defgroup USBD_HID_CORE_Private_Functions
 * @{
 */

/**
 * @brief  USBD_HID_Init
 *         Initialize the HID interface.
 * @param  pdev: device instance
 * @param  cfgidx: configuration index(unused)
 * @retval USBD Status
 */
static uint8_t  USBD_HID_Init(USBD_HandleTypeDef *pdev)
{
  USBD_HID_HandleTypeDef *hhid;
  hhid = (USBD_HID_HandleTypeDef *)USBD_malloc(sizeof(USBD_HID_HandleTypeDef));

  if (hhid == NULL)
  {
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    return (uint8_t)USBD_EMEM;
  }

  pdev->pClassDataCmsit[pdev->classId] = (void *)hhid;
  pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];

#ifdef  USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this class instance */
  HIDInEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    pdev->ep_in[HIDInEpAdd & 0xFU].bInterval = HID_HS_BINTERVAL;
  }
  else  /* low and full-speed endpoints */
  {
    pdev->ep_in[HIDInEpAdd & 0xFU].bInterval = HID_FS_BINTERVAL;
  }

  /* open ep in */
  (void)USBD_LL_OpenEP(pdev, HIDInEpAdd, USBD_EP_TYPE_INTR, HID_EPIN_SIZE);
  pdev->ep_in[HIDInEpAdd & 0xFU].is_used = 1U;
  hhid->state = USBD_HID_IDLE;

  return (uint8_t)USBD_OK;
}

/**
 * @brief  USBD_HID_DeInit
 *         DeInitialize the HID layer.
 * @param  pdev: device instance
 * @param  cfgidx: configuration index(unused)
 * @retval USBD Status
 */
static uint8_t  USBD_HID_DeInit(USBD_HandleTypeDef *pdev)
{

#ifdef  USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this class instance */
  HIDInEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  /* close HID eps */
  (void)USBD_LL_CloseEP(pdev, HIDInEpAdd);
  pdev->ep_in[HIDInEpAdd & 0xFU].is_used = 0U;
  pdev->ep_in[HIDInEpAdd & 0xFU].bInterval = 0U;

  /* free allocated memory */
  if (pdev->pClassDataCmsit[pdev->classId] != NULL)
  {
    (void)USBD_free(pdev->pClassDataCmsit[pdev->classId]);
    pdev->pClassDataCmsit[pdev->classId] = NULL;
  }

  return (uint8_t)USBD_OK;

}

/**
 * @brief  USBD_HID_Setup
 *         handle the HID specific requests.
 * @param  pdev: device instance
 * @param  req: usb requests
 * @retval USBD Status
 */
static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  USBD_StatusTypeDef ret = USBD_OK;
  uint16_t len;
  uint8_t  *pbuf;
  uint16_t status_info = 0U;

  if (hhid == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_CLASS:
      switch (req->bRequest)
      {
        case USBD_HID_REQ_SET_PROTOCOL:
          hhid->Protocol = (uint8_t)(req->wValue);
          break;

        case USBD_HID_REQ_GET_PROTOCOL:
          (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->Protocol, 1U);
          break;

        case USBD_HID_REQ_SET_IDLE:
          hhid->IdleState = (uint8_t)(req->wValue >> 8);
          break;

        case USBD_HID_REQ_GET_IDLE:
          (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->IdleState, 1U);
          break;

        default:
          USBD_CtlError(pdev);
          ret = USBD_FAIL;
          break;
      }
      break;

    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_STATUS:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
          }
          else
          {
            USBD_CtlError(pdev);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_GET_DESCRIPTOR:
          if ((req->wValue >> 8) == HID_REPORT_DESC)
          {
            len = MIN(HID_MOUSE_REPORT_DESC_SIZE, req->wLength);
            pbuf = HID_MOUSE_ReportDesc;
          }
          else if ((req->wValue >> 8) == HID_DESCRIPTOR_TYPE)
          {
            pbuf = USBD_HID_Desc;
            len = MIN(USB_HID_DESC_SIZ, req->wLength);
          }
          else
          {
            USBD_CtlError(pdev);
            ret = USBD_FAIL;
            break;
          }
          (void)USBD_CtlSendData(pdev, pbuf, len);
          break;

        case USB_REQ_GET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->AltSetting, 1U);
          }
          else
          {
            USBD_CtlError(pdev);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            hhid->AltSetting = (uint8_t)(req->wValue);
          }
          else
          {
            USBD_CtlError(pdev);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          break;

        default:
          USBD_CtlError(pdev);
          ret = USBD_FAIL;
          break;
      }
      break;

    default:
      USBD_CtlError(pdev);
      ret = USBD_FAIL;
      break;
  }
  return (uint8_t)ret;
}

/**
 * @brief  USBD_HID_SendReport
 *         send HID report
 * @param  pdev: device instance
 * @param  ClassId: the class id
 * @retval USBD Status
 */
#ifdef USE_USBD_COMPOSITE
uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len, uint8_t ClassId)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[ClassId];
#else
uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
#endif /* USE_USBD_COMPOSITE */

  if (hhid == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

#ifdef USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this class instance */
  HIDInEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_INTR, ClassId);
#endif /* USE_USBD_COMPOSITE */

  if (pdev->dev_state == USBD_STATE_CONFIGURED)
  {
    if (hhid->state == USBD_HID_IDLE)
    {
      hhid->state = USBD_HID_BUSY;
      (void)USBD_LL_Transmit(pdev, HIDInEpAdd, report, len);
    }
  }

  return (uint8_t)USBD_OK;
}

/**
 * @brief  USBD_HID_GetPollingInterval
 *         return polling interval from endpoint descriptor
 * @param  pdev: device instance
 * @retval polling interval
 */
uint32_t USBD_HID_GetPollingInterval(USBD_HandleTypeDef *pdev)
{
  uint32_t polling_interval;
  /* high-speed endpoints */
  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    /* set the data transfer polling interval for high speed transfers
     * value between 1..16 are allowed. Value correspond to interval
     * of 2 ^ (bInterval-1). this option (8ms) correspond to HID_HS_BINTERVAL)*/
    polling_interval = (((1U << (HID_HS_BINTERVAL - 1U))) / 8U);
  }
  else  /* low and full-speed endpoints */
  {
    /* sets the data transfer polling interval for low and full speed transfers */
    polling_interval = HID_FS_BINTERVAL;
  }
  return ((uint32_t)(polling_interval));
}

#ifndef USE_USBD_COMPOSITE
/**
 * @brief  USBD_HID_GetFSCfgDesc
 *         return fs configuration descriptor
 * @param  speed: current device speed
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t *USBD_HID_GetFSCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpDesc = USBD_GetEpDesc(USBD_HID_CfgDesc, HID_EPIN_ADDR);

  if (pEpDesc != NULL)
  {
    pEpDesc->bInterval = HID_FS_BINTERVAL;
  }
  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}

/**
 * @brief  USBD_HID_GetHSCfgDesc
 *         return hs configuration descriptor
 * @param  speed: current device speed
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t *USBD_HID_GetHSCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpDesc = USBD_GetEpDesc(USBD_HID_CfgDesc, HID_EPIN_ADDR);

  if (pEpDesc != NULL)
  {
    pEpDesc->bInterval = HID_HS_BINTERVAL;
  }
  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}

/**
 * @brief  USBD_HID_GetOtherSpeedCfgDesc
 *         return other speed configuration descriptor
 * @param  speed: current device speed
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t *USBD_HID_GetOtherSpeedCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpDesc = USBD_GetEpDesc(USBD_HID_CfgDesc, HID_EPIN_ADDR);

  if (pEpDesc != NULL)
  {
    pEpDesc->bInterval = HID_FS_BINTERVAL;
  }
  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}
#endif /* USE_USBD_COMPOSITE */

/**
 * @brief  USBD_HID_DataIn
 *         handle data IN stage
 * @param  pdev: device instance
 * @param  epnum: endpoint index(unused)
 * @retval status
 */
static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev)
{
  /* ensure that the fifo is empty before a new transfer, this condition could be caused by
   * a new transfer before the end of the previous transfer */
  ((USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId])->state = USBD_HID_IDLE;
  return (uint8_t)USBD_OK;
}

#ifndef USE_USBD_COMPOSITE
/**
 * @brief  USBD_HID_GetDeviceQualifierDesc
 *         return device qualifier descriptor
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_HID_DeviceQualifierDesc);
  return USBD_HID_DeviceQualifierDesc;
}
#endif  /* USE_USBD_COMPOSITE */

/**
 * @}
 */



/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
