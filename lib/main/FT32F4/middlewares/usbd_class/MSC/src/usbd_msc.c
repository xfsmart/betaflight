/**
  ******************************************************************************
  * @file    			usbd_msc.c
  * @author  			FMD XA
  * @brief   			This file provides all the MSC core functions.
  * @version 			V1.0.0           
  * @data		 			2025-04-30
  ******************************************************************************
  *
  * =========================================================
  *                     MSC Class Description
  * =========================================================
  * This module manages the MSC class v1.0 following the "Universal Serial Bus Mass
  * Storage Class (MSC) Bulk-only Transport (BOT) version 1.0 sep. 31, 1999"
  * This driver implements the following aspects of the specification:
  * - Bulk-only Transport protocol
  * - Subclass : SCSI transparent command set.
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_msc.h"


/**@addtogroup USBD_LIB
 * @{
 */

/**@addtogroup USBD_CLASS
 * @{
 */

/**@addtogroup USBD_MSC_CLASS
 * @{
 */

/**@defgroup USBD_MSC_CORE
 * @brief Mass storage core module.
 * @{
 */

/**@defgroup USBD_MSC_CORE_Private_TypeDefinitions
 * @{
 */

/**
 * @}
 */


/**@defgroup USBD_MSC_CORE_Private_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MSC_CORE_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_MSC_CORE_Private_FunctionPrototypes
 * @{
 */
uint8_t USBD_MSC_Init(USBD_HandleTypeDef *pdev);
uint8_t USBD_MSC_DeInit(USBD_HandleTypeDef *pdev);
uint8_t USBD_MSC_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req);
uint8_t USBD_MSC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
uint8_t USBD_MSC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);

#ifndef USE_USBD_COMPOSITE
uint8_t *USBD_MSC_GetHSCfgDesc(uint16_t *length);
uint8_t *USBD_MSC_GetFSCfgDesc(uint16_t *length);
uint8_t *USBD_MSC_GetOtherSpeedCfgDesc(uint16_t *length);
uint8_t *USBD_MSC_GetDeviceQualifierDescriptor(uint16_t *length);
#endif  /* USE_USBD_COMPOSITE */

/**
 * @}
 */

/**@defgroup USBD_MSC_CORE_Private_Variables
 * @{
 */
USBD_ClassTypeDef USBD_MSC =
{
  USBD_MSC_Init,
  USBD_MSC_DeInit,
  USBD_MSC_Setup,
  NULL,  /* EP0_TxSent */
  NULL,  /* EP0_RxReady*/
  USBD_MSC_DataIn,
  USBD_MSC_DataOut,
  NULL, /* SOF */
  NULL,
  NULL,
#ifdef USE_USBD_COMPOSITE
  NULL,
  NULL,
  NULL,
  NULL,
#else
  USBD_MSC_GetHSCfgDesc,
  USBD_MSC_GetFSCfgDesc,
  USBD_MSC_GetOtherSpeedCfgDesc,
  USBD_MSC_GetDeviceQualifierDescriptor,
#endif  /* USE_USBD_COMPOSITE */
#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
  NULL, /* GetUsrStrDescriptor */
#endif  /* USBD_SUPPORT_USER_STRING_DESC */
};

/* USB Mass storage device configuration descriptor */
#ifndef USE_USBD_COMPOSITE
/* USB Mass storage device configuration descirptor */
/* all descriptor (configuration, interface, endpoint, class, vendor...) */
__ALIGN_BEGIN static uint8_t USBD_MSC_CfgDesc[USB_MSC_CONFIG_DESC_SIZ] __ALIGN_END =
{
  0x09,                             /* bLength: configuration descriptor size */
  USB_DESC_TYPE_CONFIGURATION,      /* bDescriptorType: configuration */
  USB_MSC_CONFIG_DESC_SIZ,          /* wTotalLength: Bytes returned */
  0x00,
  0x01,                             /* bNumInterfaces: 1 interface */
  0x01,                             /* bConfigurationValue: configuration value */
  0x04,                             /* iConfiguration: index of string desriptor describing the configuration */
#if (USBD_SELF_POWERED == 1U)
  0xC0,                             /* bmAttributes: bus powered according to user configuration */
#else
  0x80,                             /* bmAttributes: bus powered according to user configuration */
#endif  /* USBD_SELF_POWERD */
  USBD_MAX_POWER,                   /* MaxPower (mA) */

  /**************Descriptor of mass storage interface**************************/
  0x09,                             /* bLength: interface descriptor size */
  0x04,                             /* bDescriptorType: interface descriptor type */
  0x00,                             /* bInterfaceNumber: number of interface */
  0x00,                             /* bAlternateSetting: alternate setting */
  0x02,                             /* bNumEndpoints */
  0x08,                             /* bInterfaceClass: MSC */
  0x06,                             /* bInterfaceSubClass: scsi transparent */
  0x50,                             /* nInterfaceProtocol */
  0x05,                             /* iInterface: Index of string descriptor */
  /************** mass storage endpoints  ************************************/
  0x07,                             /* bLength: endpoint Descriptor size */
  0x05,                             /* bDescriptorType: endpoint descriptor type */
  MSC_EPIN_ADDR,                    /* bEndpointAddress: endpoint address (IN) */
  0x02,                             /* bmAttributes: bulk endpoint */
  LOBYTE(MSC_MAX_FS_PACKET),
  HIBYTE(MSC_MAX_FS_PACKET),
  0x00,                             /* bInterval: polling interval */

  0x07,                             /* bLength: endpoint Descriptor size */
  0x05,                             /* bDescriptorType: endpoint descriptor type */
  MSC_EPOUT_ADDR,                   /* bEndpointAddress: endpoint address (OUT) */
  0x02,                             /* bmAttributes: bulk endpoint */
  LOBYTE(MSC_MAX_FS_PACKET),
  HIBYTE(MSC_MAX_FS_PACKET),
  0x00                              /* bInterval: polling interval */
};


/* USB Standard device descriptor */
__ALIGN_BEGIN static uint8_t USBD_MSC_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  MSC_MAX_FS_PACKET,
  0x01,
  0x00,
};

#endif /* USE_USBD_COMPOSITE */

uint8_t MSCInEpAdd  = MSC_EPIN_ADDR;
uint8_t MSCOutEpAdd = MSC_EPOUT_ADDR;

/**
 * @}
 */

/**@defgroup USBD_MSC_CORE_Private_Functions
 * @{
 */

/**
 * @brief  USBD_MSC_Init
 *         Initialize the mass storage configuration.
 * @param  pdev: device instance
 * @param  cfgidx: configuration index(unused)
 * @retval Status
 */
uint8_t USBD_MSC_Init(USBD_HandleTypeDef *pdev)
{
  USBD_MSC_BOT_HandleTypeDef *hmsc;

  hmsc = (USBD_MSC_BOT_HandleTypeDef *)USBD_malloc(sizeof(USBD_MSC_BOT_HandleTypeDef));

  if (hmsc == NULL)
  {
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    return (uint8_t)USBD_EMEM;
  }

  pdev->pClassDataCmsit[pdev->classId] = (void *)hmsc;
  pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];

#ifdef USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this class instance */
  MSCInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN,  USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  MSCOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    /* open ep out */
    (void)USBD_LL_OpenEP(pdev, MSCOutEpAdd, USBD_EP_TYPE_BULK, MSC_MAX_HS_PACKET);
    pdev->ep_out[MSCOutEpAdd & 0xFU].is_used = 1U;
    /* open ep in */
    (void)USBD_LL_OpenEP(pdev, MSCInEpAdd, USBD_EP_TYPE_BULK, MSC_MAX_HS_PACKET);
    pdev->ep_in[MSCInEpAdd & 0xFU].is_used = 1U;
  }
  else
  {
    /* open ep out */
    (void)USBD_LL_OpenEP(pdev, MSCOutEpAdd, USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);
    pdev->ep_out[MSCOutEpAdd & 0xFU].is_used = 1U;
    /* open ep in */
    (void)USBD_LL_OpenEP(pdev, MSCInEpAdd, USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);
    pdev->ep_in[MSCInEpAdd & 0xFU].is_used = 1U;
  }

  /* Init the bot layer */
  MSC_BOT_Init(pdev);

  return (uint8_t)USBD_OK;
}

/**
 * @brief  USBD_MSC_DeInit
 *         DeInitialize the mass storage configuration.
 * @param  pdev: device instance
 * @param  cfgidx: configuration index(unused)
 * @retval Status
 */
uint8_t USBD_MSC_DeInit(USBD_HandleTypeDef *pdev)
{
#ifdef USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this class instance */
  MSCInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN,  USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  MSCOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  /* Close msc eps */
  (void)USBD_LL_CloseEP(pdev, MSCOutEpAdd);
  pdev->ep_out[MSCOutEpAdd & 0xFU].is_used = 0U;
  /* Close ep in */
  (void)USBD_LL_CloseEP(pdev, MSCInEpAdd);
  pdev->ep_in[MSCInEpAdd & 0xFU].is_used = 0U;

  /* free msc class resources */
  if (pdev->pClassDataCmsit[pdev->classId] != NULL)
  {
    /* De-Init the bot layer */
    MSC_BOT_DeInit(pdev);

    (void)USBD_free(pdev->pClassDataCmsit[pdev->classId]);
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    pdev->pClassData = NULL;
  }
  return (uint8_t)USBD_OK;
}

/**
 * @brief  USBD_MSC_Setup
 *         Handle the MSC specific requests
 * @param  pdev: device instance
 * @param  req: usb request
 * @retval Status
 */
uint8_t USBD_MSC_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req)
{
  USBD_MSC_BOT_HandleTypeDef *hmsc = (USBD_MSC_BOT_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  USBD_StatusTypeDef ret = USBD_OK;
  uint16_t status_info = 0U;

#ifdef USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this class instance */
  MSCInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN,  USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  MSCOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  if (hmsc == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    /* class request */
    case USB_REQ_TYPE_CLASS:
      switch (req->bRequest)
      {
        case BOT_GET_MAX_LUN:
          if ((req->wValue == 0U) && (req->wLength == 1U) &&
              ((req->bmRequest & 0x80U) == 0x80U))
          {
            /* hmsc->max_lun = (uint32_t)((USBD_StorageTypeDef *)pdev->pUserData[pdev->classId])->GetMaxLun(); */
            hmsc->max_lun = (uint32_t)USBD_STORAGE_fops->GetMaxLun();
            (void)USBD_CtlSendData(pdev, (uint8_t *)&hmsc->max_lun, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case BOT_RESET:
          if ((req->wValue == 0U) && (req->wLength == 0U) &&
              ((req->bmRequest & 0x80U) != 0x80U))
          {
            MSC_BOT_Reset(pdev);
          }
          else
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

    /* Interface & endpoint request */
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
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_GET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&hmsc->interface, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            hmsc->interface = (uint8_t)(req->wValue);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            if (req->wValue == USB_FEATURE_EP_HALT)
            {
              /* Flush the FIFO */
              (void)USBD_LL_FlushEP(pdev, (uint8_t)req->wIndex);
              /* Handle BOT error */
              MSC_BOT_CplClrFeature(pdev, (uint8_t)req->wIndex);
            }
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

/**
 * @brief  USBD_MSC_DataIn
 *         Handle data in stage
 * @param  pdev: device instance
 * @param  epnum: endpoint index(unused)
 * @retval Status
 */
uint8_t USBD_MSC_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  UNUSED(epnum);
  MSC_BOT_DataIn(pdev);
  return (uint8_t)USBD_OK;
}

/**
 * @brief  USBD_MSC_DataOut
 *         Handle data out stage
 * @param  pdev: device instance
 * @param  epnum: endpoint index(unused)
 * @retval Status
 */
uint8_t USBD_MSC_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  MSC_BOT_DataOut(pdev);
  return (uint8_t)USBD_OK;
}

#ifndef USE_USBD_COMPOSITE
/**
 * @brief  USBD_MSC_GetHSCfgDesc
 *         return configuration descriptor
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_GetHSCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpInDesc  = USBD_GetEpDesc(USBD_MSC_CfgDesc, MSC_EPIN_ADDR);
  USBD_EpDescTypeDef *pEpOutDesc = USBD_GetEpDesc(USBD_MSC_CfgDesc, MSC_EPOUT_ADDR);

  if (pEpInDesc != NULL)
  {
    pEpInDesc->wMaxPacketSize = MSC_MAX_HS_PACKET;
  }

  if (pEpOutDesc != NULL)
  {
    pEpOutDesc->wMaxPacketSize = MSC_MAX_HS_PACKET;
  }

  *length = (uint16_t)sizeof(USBD_MSC_CfgDesc);
  return USBD_MSC_CfgDesc;

}

/**
 * @brief  USBD_MSC_GetFSCfgDesc
 *         return configuration descriptor
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_GetFSCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpInDesc  = USBD_GetEpDesc(USBD_MSC_CfgDesc, MSC_EPIN_ADDR);
  USBD_EpDescTypeDef *pEpOutDesc = USBD_GetEpDesc(USBD_MSC_CfgDesc, MSC_EPOUT_ADDR);

  if (pEpInDesc != NULL)
  {
    pEpInDesc->wMaxPacketSize = MSC_MAX_FS_PACKET;
  }

  if (pEpOutDesc != NULL)
  {
    pEpOutDesc->wMaxPacketSize = MSC_MAX_FS_PACKET;
  }

  *length = (uint16_t)sizeof(USBD_MSC_CfgDesc);
  return USBD_MSC_CfgDesc;
}

/**
 * @brief  USBD_MSC_GetOtherSpeedCfgDesc
 *         return other speed configuration descriptor
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_GetOtherSpeedCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpInDesc  = USBD_GetEpDesc(USBD_MSC_CfgDesc, MSC_EPIN_ADDR);
  USBD_EpDescTypeDef *pEpOutDesc = USBD_GetEpDesc(USBD_MSC_CfgDesc, MSC_EPOUT_ADDR);

  if (pEpInDesc != NULL)
  {
    pEpInDesc->wMaxPacketSize = MSC_MAX_FS_PACKET;
  }

  if (pEpOutDesc != NULL)
  {
    pEpOutDesc->wMaxPacketSize = MSC_MAX_FS_PACKET;
  }

  *length = (uint16_t)sizeof(USBD_MSC_CfgDesc);
  return USBD_MSC_CfgDesc;

}

/**
 * @brief  USBD_MSC_GetDeviceQualifierDescriptor
 *         return device qualifier descriptor
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_GetDeviceQualifierDescriptor(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_MSC_DeviceQualifierDesc);
  return USBD_MSC_DeviceQualifierDesc;
}

#endif /* USE_USBD_COMPOSITE */

/**
 * @brief  USBD_MSC_RegisterStorage
 * @param  fops: storage callback
 * @retval status
 */
uint8_t USBD_MSC_RegisterStorage(USBD_HandleTypeDef *pdev, USBD_StorageTypeDef *fops)
{
  if (fops == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }
  pdev->pUserData[pdev->classId] = fops;

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


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
