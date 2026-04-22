/**
  ******************************************************************************
  * @file    			usbd_mtp.c
  * @author  			FMD XA
  * @brief   			This file provides the high layer firmware functions to manage the following
  *               functionalities of the USB MTP Class:
  *               - Initialization and Configuration of high and low layer
  *               - Enumeration as MTP Device (and enumeration for each implemented memory interface)
  *               - OUT/IN data transfer
  *               - Command IN transfer (class requests management)
  *               - Error mangement
  * @version 			V1.0.0           
  * @data		 			2025-05-08
  ******************************************************************************
  *
  * =========================================================
  *                     MTP Class Description
  * =========================================================
  * This module manages the MTP class following the "Media Transfer Protocol (MTP)
  * specification Version 1.11 Aprial 6th, 2011". the driver implements the following aspects
  * of the specification:
  * - Device descriptor management
  * - Configuration descriptor management
  * - Enumeration as MTP device with 2 data endpoints(IN and OUT) and 1 command endpoint (IN)
  * - Requests management
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_mtp.h"
#include "usbd_mtp_storage.h"

/**@addtogroup USBD_LIB
 * @{
 */

/**@addtogroup USBD_CLASS
 * @{
 */

/**@addtogroup USBD_MTP_CLASS
 * @{
 */

/**@defgroup USBD_MTP_CORE
 * @brief usbd core module.
 * @{
 */

/**@defgroup USBD_MTP_CORE_Private_TypeDefinitions
 * @{
 */

/**
 * @}
 */


/**@defgroup USBD_MTP_CORE_Private_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MTP_CORE_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_MTP_CORE_Private_FunctionPrototypes
 * @{
 */
static uint8_t USBD_MTP_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_MTP_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_MTP_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req);
static uint8_t USBD_MTP_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_MTP_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);

#ifndef USE_USBD_COMPOSITE
static uint8_t *USBD_MTP_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_MTP_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_MTP_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_MTP_GetDeviceQualifierDescriptor(uint16_t *length);

#endif /* USE_USBD_COMPOSITE */

/**
 * @}
 */

/**@defgroup USBD_MTP_CORE_Private_Variables
 * @{
 */

/* MTP interface class callbacks structure */
USBD_ClassTypeDef USBD_MTP =
{
  USBD_MTP_Init,
  USBD_MTP_DeInit,
  USBD_MTP_Setup,
  NULL,  /* EP0_TxSent */
  NULL,  /* EP0_RxReady */
  USBD_MTP_DataIn,
  USBD_MTP_DataOut,
  NULL,  /* SOF */
  NULL,  /* ISOIn */
  NULL,  /* ISOOut */
#ifdef USE_USBD_COMPOSITE
  NULL,
  NULL,
  NULL,
  NULL,
#else
  USBD_MTP_GetHSCfgDesc,
  USBD_MTP_GetFSCfgDesc,
  USBD_MTP_GetOtherSpeedCfgDesc,
  USBD_MTP_GetDeviceQualifierDescriptor,
#endif /* USE_USBD_COMPOSITE */
};



#ifndef USE_USBD_COMPOSITE
/* USB MTP device configuration descriptor */
__ALIGN_BEGIN static uint8_t USBD_MTP_CfgDesc[MTP_CONFIG_DESC_SIZ] __ALIGN_END =
{
  /* Configuration descriptor */
  0x09,                             /* bLength: configuration descriptor size */
  USB_DESC_TYPE_CONFIGURATION,      /* bDescriptorType: configuration */
  LOBYTE(MTP_CONFIG_DESC_SIZ),      /* wTotalLength: Bytes returned */
  HIBYTE(MTP_CONFIG_DESC_SIZ),
  0x01,                             /* bNumInterfaces: 1 interface */
  0x01,                             /* bConfigurationValue: configuration value */
  0x00,                             /* iConfiguration: index of string desriptor describing the configuration */
#if (USBD_SELF_POWERED == 1U)
  0xC0,                             /* bmAttributes: bus powered according to user configuration */
#else
  0x80,                             /* bmAttributes: bus powered according to user configuration */
#endif  /* USBD_SELF_POWERD */
  USBD_MAX_POWER,                   /* MaxPower (mA) */

  /**************MTP interface ************************************************/
  MTP_INTERFACE_DESC_SIZE,          /* bLength: interface descriptor size */
  USB_DESC_TYPE_INTERFACE,          /* bDescriptorType: interface descriptor type */
  MTP_CMD_ITF_NBR,                  /* bInterfaceNumber: number of interface */
  0x00,                             /* bAlternateSetting: alternate setting */
  0x03,                             /* bNumEndpoints */
  USB_MTP_INTERFACE_CLASS,          /* bInterfaceClass: MTP */
  USB_MTP_INTERFACE_SUB_CLASS,      /* bInterfaceSubClass: Abstract control model */
  USB_MTP_INTERFACE_PROTOCOL,       /* nInterfaceProtocol: common AT commands */
  0x00,                             /* iInterface: Index of string descriptor */
  /************** MTP Endpoints **********************************************/
  MTP_ENDPOINT_DESC_SIZE,           /* bLength: endpoint Descriptor length = 7 */
  USB_DESC_TYPE_ENDPOINT,           /* bDescriptorType: endpoint descriptor type */
  MTP_IN_EP,                        /* bEndpointAddress: endpoint address (IN) */
  USBD_EP_TYPE_BULK,                /* bmAttributes: bulk endpoint */
  LOBYTE(MTP_DATA_MAX_FS_PACKET_SIZE),
  HIBYTE(MTP_DATA_MAX_FS_PACKET_SIZE),
  0x00,                             /* bInterval: polling interval in milliseconds */

  MTP_ENDPOINT_DESC_SIZE,           /* bLength: endpoint Descriptor length = 7 */
  USB_DESC_TYPE_ENDPOINT,           /* bDescriptorType: endpoint descriptor type */
  MTP_OUT_EP,                       /* bEndpointAddress: endpoint address (OUT) */
  USBD_EP_TYPE_BULK,                /* bmAttributes: bulk endpoint */
  LOBYTE(MTP_DATA_MAX_FS_PACKET_SIZE),
  HIBYTE(MTP_DATA_MAX_FS_PACKET_SIZE),
  0x00,                             /* bInterval: polling interval in milliseconds */

  MTP_ENDPOINT_DESC_SIZE,           /* bLength: endpoint Descriptor length = 7 */
  USB_DESC_TYPE_ENDPOINT,           /* bDescriptorType: endpoint descriptor type */
  MTP_CMD_EP,                       /* bEndpointAddress: endpoint address (IN) */
  USBD_EP_TYPE_INTR,                /* bmAttributes: bulk endpoint */
  LOBYTE(MTP_CMD_PACKET_SIZE),
  HIBYTE(MTP_CMD_PACKET_SIZE),
  MTP_FS_BINTERVAL                  /* bInterval: polling interval in milliseconds */
};


/* USB Standard device descriptor */
__ALIGN_BEGIN static uint8_t USBD_MTP_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
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

uint8_t MTPInEpAdd = MTP_IN_EP;
uint8_t MTPOutEpAdd = MTP_OUT_EP;
uint8_t MTPCmdEpAdd = MTP_CMD_EP;



/**
 * @}
 */

/**@defgroup USBD_MTP_CORE_Private_Functions
 * @{
 */

/**
 * @brief  USBD_MTP_Init
 *         Initialize the MTP class.
 * @param  pdev: device instance
 * @param  cfgidx: Configuration index
 * @retval Status
 */
static uint8_t USBD_MTP_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  USBD_MTP_HandleTypeDef *hmtp;

  hmtp = (USBD_MTP_HandleTypeDef *)USBD_malloc(sizeof(USBD_MTP_HandleTypeDef));

  if (hmtp == NULL)
  {
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    return (uint8_t)USBD_EMEM;
  }

  /* setup the pClassData pointer */
  pdev->pClassDataCmsit[pdev->classId] = (void *)hmtp;
  pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];

#ifdef USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this class instance */
  MTPInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN,  USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  MTPOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  MTPCmdEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_IN,  USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  /* initialize all variables */
  (void)USBD_memset(hmtp, 0, sizeof(USBD_MTP_HandleTypeDef));

  /* setup the max packet size according to selected speed */
  if (pdev->dev_speed == USBD_SPEED_HIGH)
  {
    hmtp->MaxPcktLen = MTP_DATA_MAX_HS_PACKET_SIZE;
  }
  else
  {
    hmtp->MaxPcktLen = MTP_DATA_MAX_FS_PACKET_SIZE;
  }

  /* open ep in */
  (void)USBD_LL_OpenEP(pdev, MTPInEpAdd, USBD_EP_TYPE_BULK, hmtp->MaxPcktLen);
  pdev->ep_in[MTPInEpAdd & 0xFU].is_used = 1U;

  /* open ep out */
  (void)USBD_LL_OpenEP(pdev, MTPOutEpAdd, USBD_EP_TYPE_BULK, hmtp->MaxPcktLen);
  pdev->ep_out[MTPOutEpAdd & 0xFU].is_used = 1U;

  /* open intr ep in */
  (void)USBD_LL_OpenEP(pdev, MTPCmdEpAdd, USBD_EP_TYPE_INTR, MTP_CMD_PACKET_SIZE);
  pdev->ep_in[MTPCmdEpAdd & 0xFU].is_used = 1U;

  /* Init the MTP layer */
  (void)USBD_MTP_STORAGE_Init(pdev);

  return (uint8_t)USBD_OK;
}

/**
 * @brief  USBD_MTP_DeInit
 *         DeInitialize the MTP class.
 * @param  pdev: device instance
 * @param  cfgidx: Configuration index
 * @retval Status
 */
static uint8_t USBD_MTP_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{

#ifdef USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this class instance */
  MTPInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN,  USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  MTPOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  MTPCmdEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_IN,  USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  /* close ep in */
  (void)USBD_LL_CloseEP(pdev, MTPInEpAdd);
  pdev->ep_in[MTPInEpAdd & 0xFU].is_used = 0U;

  /* close ep out */
  (void)USBD_LL_CloseEP(pdev, MTPOutEpAdd);
  pdev->ep_out[MTPOutEpAdd & 0xFU].is_used = 0U;

  /* close intr ep in */
  (void)USBD_LL_CloseEP(pdev, MTPCmdEpAdd);
  pdev->ep_in[MTPCmdEpAdd & 0xFU].is_used = 0U;

  /* free mtp class resources */
  if (pdev->pClassDataCmsit[pdev->classId] != NULL)
  {
    /* de-init the MTP layer */
    (void)USBD_MTP_STORAGE_DeInit(pdev);

    (void)USBD_free(pdev->pClassDataCmsit[pdev->classId]);
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    pdev->pClassData = NULL;
  }

  return (uint8_t)USBD_OK;
}

/**
 * @brief  USBD_MTP_Setup
 *         Handle the MTP specific requests
 * @param  pdev: device instance
 * @param  req: usb requests
 * @retval Status
 */
static uint8_t USBD_MTP_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req)
{
  USBD_MTP_HandleTypeDef *hmtp = (USBD_MTP_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  USBD_StatusTypeDef ret = USBD_OK;
  uint16_t len = 0U;

#ifdef USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this MTP class instance */
  MTPOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  if (hmtp == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    /* class request */
    case USB_REQ_TYPE_CLASS:
      switch (req->bRequest)
      {
        case MTP_REQ_CANCEL:
          len = MIN(hmtp->MaxPcktLen, req->wLength);
          (void)USBD_CtlPrepareRx(pdev, (uint8_t *)(hmtp->rx_buff), len);
          break;

        case MTP_REQ_GET_EXT_EVENT_DATA:
          break;

        case MTP_REQ_RESET:
          /* stop low layer file system operations if any */
          USBD_MTP_STORAGE_Cancel(pdev, MTP_PHASE_IDLE);
          (void)USBD_LL_PrepareReceive(pdev, MTPOutEpAdd, (uint8_t *)&hmtp->rx_buff, hmtp->MaxPcktLen);
          break;

        case MTP_REQ_GET_DEVICE_STATUS:
          switch (hmtp->MTP_ResponsePhase)
          {
            case MTP_READ_DATA:
              len = 4U;
              hmtp->dev_status = ((uint32_t)MTP_RESPONSE_DEVICE_BUSY << 16) | len;
              break;

            case MTP_RECEIVE_DATA:
              len = 4U;
              hmtp->dev_status = ((uint32_t)MTP_RESPONSE_TRANSACTION_CANCELLED << 16) | len;
              break;

            case MTP_PHASE_IDLE:
              len = 4U;
              hmtp->dev_status = ((uint32_t)MTP_RESPONSE_OK << 16) | len;
              break;

            default:
              break;
          }
          (void)USBD_CtlSendData(pdev, (uint8_t *)&hmtp->dev_status, len);
          break;

        default:
          USBD_CtlError(pdev);
          ret = USBD_FAIL;
          break;
      }
      break;

    /* interface & endpoint request */
    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            hmtp->alt_setting = 0U;
            (void)USBD_CtlSendData(pdev, (uint8_t *)&hmtp->alt_setting, 1U);
          }
          break;

        case USB_REQ_SET_INTERFACE:
          if (pdev->dev_state != USBD_STATE_CONFIGURED)
          {
            USBD_CtlError(pdev);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          /* re-active the ep */
          (void)USBD_LL_CloseEP(pdev, (uint8_t)req->wIndex);

          if ((((uint8_t)req->wIndex) & 0x80U) == 0x80U)
          {
            (void)USBD_LL_OpenEP(pdev, ((uint8_t)req->wIndex), USBD_EP_TYPE_BULK, hmtp->MaxPcktLen);
          }
          else
          {
            (void)USBD_LL_OpenEP(pdev, ((uint8_t)req->wIndex), USBD_EP_TYPE_BULK, hmtp->MaxPcktLen);
          }
          break;


        default:
          break;
      }
      break;

    default:
      break;
  }

  return (uint8_t)ret;
}

/**
 * @brief  USBD_MTP_DataIn
 *         Data sent on non-control IN endpoint
 * @param  pdev: device instance
 * @param  epnum: endpoint number
 * @retval Status
 */
static uint8_t USBD_MTP_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_MTP_HandleTypeDef *hmtp = (USBD_MTP_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  uint16_t len;

#ifdef USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this MTP class instance */
  MTPInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN,  USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
  MTPOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  if (epnum == (MTPInEpAdd & 0x7FU))
  {
    switch (hmtp->MTP_ResponsePhase)
    {
      case MTP_RESPONSE_PHASE:
        (void)USBD_MTP_STORAGE_SendContainer(pdev, REP_TYPE);
        /* prepare to receive next operation */
        len = MIN(hmtp->MaxPcktLen, pdev->request.wLength);
        (void)USBD_LL_PrepareReceive(pdev, MTPOutEpAdd, (uint8_t *)&hmtp->rx_buff, len);
        hmtp->MTP_ResponsePhase = MTP_PHASE_IDLE;
        break;

      case MTP_READ_DATA:
        (void)USBD_MTP_STORAGE_ReadData(pdev);
        /* prepare to receive next operation */
        len = MIN(hmtp->MaxPcktLen, pdev->request.wLength);
        (void)USBD_LL_PrepareReceive(pdev, MTPInEpAdd, (uint8_t *)&hmtp->rx_buff, len);
        break;

      case MTP_PHASE_IDLE:
        /* prepare to receive next operation */
        len = MIN(hmtp->MaxPcktLen, pdev->request.wLength);
        (void)USBD_LL_PrepareReceive(pdev, MTPOutEpAdd, (uint8_t *)&hmtp->rx_buff, len);
        break;

      default:
        break;
    }
  }

  return (uint8_t)USBD_OK;
}

/**
 * @brief  USBD_MTP_DataOut
 *         Data received on non-control Out endpoint
 * @param  pdev: device instance
 * @param  epnum: endpoint number
 * @retval Status
 */
static uint8_t USBD_MTP_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_MTP_HandleTypeDef *hmtp = (USBD_MTP_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  uint16_t len;

#ifdef USE_USBD_COMPOSITE
  /* get the endpoints address allocated for this MTP class instance */
  MTPOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_BULK, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  (void)USBD_MTP_STORAGE_ReceiveOpt(pdev);

  switch (hmtp->MTP_ResponsePhase)
  {
    case MTP_RESPONSE_PHASE:
      if (hmtp->ResponseLength == MTP_CONT_HEADER_SIZE)
      {
        (void)USBD_MTP_STORAGE_SendContainer(pdev, REP_TYPE);
        hmtp->MTP_ResponsePhase = MTP_PHASE_IDLE;
      }
      else
      {
        (void)USBD_MTP_STORAGE_SendContainer(pdev, DATA_TYPE);
      }
      break;

    case MTP_READ_DATA:
      (void)USBD_MTP_STORAGE_ReadData(pdev);
      break;

    case MTP_RECEIVE_DATA:
      (void)USBD_MTP_STORAGE_ReceiveData(pdev);
      /* prepare to receive operation */
      len = MIN(hmtp->MaxPcktLen, pdev->request.wLength);
      (void)USBD_LL_PrepareReceive(pdev, MTPOutEpAdd, (uint8_t *)&hmtp->rx_buff, len);
      break;

    case MTP_PHASE_IDLE:
      /* prepare to receive next operation */
      len = MIN(hmtp->MaxPcktLen, pdev->request.wLength);
      (void)USBD_LL_PrepareReceive(pdev, MTPOutEpAdd, (uint8_t *)&hmtp->rx_buff, len);
      break;

    default:
      break;
  }

  return (uint8_t)USBD_OK;
}

#ifndef USE_USBD_COMPOSITE
/**
 * @brief  USBD_MTP_GetHSCfgDesc
 *         return configuration descriptor
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t *USBD_MTP_GetHSCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpInDesc  = USBD_GetEpDesc(USBD_MTP_CfgDesc, MTP_IN_EP);
  USBD_EpDescTypeDef *pEpOutDesc = USBD_GetEpDesc(USBD_MTP_CfgDesc, MTP_OUT_EP);
  USBD_EpDescTypeDef *pEpCmdDesc = USBD_GetEpDesc(USBD_MTP_CfgDesc, MTP_CMD_EP);

  if (pEpInDesc != NULL)
  {
    pEpInDesc->wMaxPacketSize = MTP_DATA_MAX_HS_PACKET_SIZE;
  }

  if (pEpOutDesc != NULL)
  {
    pEpOutDesc->wMaxPacketSize = MTP_DATA_MAX_HS_PACKET_SIZE;
  }

  if (pEpCmdDesc != NULL)
  {
    pEpCmdDesc->bInterval = MTP_HS_BINTERVAL;
  }

  *length = (uint16_t)sizeof(USBD_MTP_CfgDesc);
  return USBD_MTP_CfgDesc;
}

/**
 * @brief  USBD_MTP_GetFSCfgDesc
 *         return configuration descriptor
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t *USBD_MTP_GetFSCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpInDesc  = USBD_GetEpDesc(USBD_MTP_CfgDesc, MTP_IN_EP);
  USBD_EpDescTypeDef *pEpOutDesc = USBD_GetEpDesc(USBD_MTP_CfgDesc, MTP_OUT_EP);
  USBD_EpDescTypeDef *pEpCmdDesc = USBD_GetEpDesc(USBD_MTP_CfgDesc, MTP_CMD_EP);

  if (pEpInDesc != NULL)
  {
    pEpInDesc->wMaxPacketSize = MTP_DATA_MAX_FS_PACKET_SIZE;
  }

  if (pEpOutDesc != NULL)
  {
    pEpOutDesc->wMaxPacketSize = MTP_DATA_MAX_FS_PACKET_SIZE;
  }

  if (pEpCmdDesc != NULL)
  {
    pEpCmdDesc->bInterval = MTP_FS_BINTERVAL;
  }

  *length = (uint16_t)sizeof(USBD_MTP_CfgDesc);
  return USBD_MTP_CfgDesc;
}

/**
 * @brief  USBD_MTP_GetOtherSpeedCfgDesc
 *         return configuration descriptor
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t *USBD_MTP_GetOtherSpeedCfgDesc(uint16_t *length)
{
  USBD_EpDescTypeDef *pEpInDesc  = USBD_GetEpDesc(USBD_MTP_CfgDesc, MTP_IN_EP);
  USBD_EpDescTypeDef *pEpOutDesc = USBD_GetEpDesc(USBD_MTP_CfgDesc, MTP_OUT_EP);
  USBD_EpDescTypeDef *pEpCmdDesc = USBD_GetEpDesc(USBD_MTP_CfgDesc, MTP_CMD_EP);

  if (pEpInDesc != NULL)
  {
    pEpInDesc->wMaxPacketSize = MTP_DATA_MAX_FS_PACKET_SIZE;
  }

  if (pEpOutDesc != NULL)
  {
    pEpOutDesc->wMaxPacketSize = MTP_DATA_MAX_FS_PACKET_SIZE;
  }

  if (pEpCmdDesc != NULL)
  {
    pEpCmdDesc->bInterval = MTP_FS_BINTERVAL;
  }

  *length = (uint16_t)sizeof(USBD_MTP_CfgDesc);
  return USBD_MTP_CfgDesc;
}

/**
 * @brief  USBD_MTP_GetDeviceQualifierDescriptor
 *         return device qualifier descriptor
 * @param  length: pointer data length
 * @retval pointer to descriptor buffer
 */
static uint8_t *USBD_MTP_GetDeviceQualifierDescriptor(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_MTP_DeviceQualifierDesc);
  return USBD_MTP_DeviceQualifierDesc;
}

#endif /* USE_USBD_COMPOSITE */

/**
 * @brief  USBD_MTP_RegisterInterface
 * @param  pdev: device instance
 * @param  fops: CD interface callback
 * @retval status
 */
uint8_t USBD_MTP_RegisterInterface(USBD_HandleTypeDef *pdev, USBD_MTP_ItfTypeDef *fops)
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
