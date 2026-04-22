/**
  ******************************************************************************
  * @file    			usbh_mtp.c
  * @author  			FMD XA
  * @brief   			This file is the MTP layer handlers for USB host mtp class.
  * @version 			V1.0.0           
  * @data		 			2025-05-08
  ******************************************************************************
  *
  * =========================================================
  *                     MTP Class Description
  * =========================================================
  * This module manages the MTP class following the "Media Transfer Protocol (MTP)
  * specification Version 1.11 Aprial 6th, 2011". the implementation is compatible
  * with the PTP model as an extension of the existing Picture Transfer Protocol
  * defined by the ISO 15740 specification.
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "usbh_mtp.h"

/**@addtogroup USBH_LIB
 * @{
 */

/**@addtogroup USBH_CLASS
 * @{
 */

/**@addtogroup USBH_MTP_CLASS
 * @{
 */

/**@defgroup USBH_MTP_CORE
 * @brief This file includes the MTP layer handlers for USB hots mtp class.
 * @{
 */

/**@defgroup USBH_MTP_CORE_Private_TypeDefinitions
 * @{
 */

/**
 * @}
 */


/**@defgroup USBH_MTP_CORE_Private_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBH_MTP_CORE_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_MTP_CORE_Private_Variables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_MTP_CORE_Private_FunctionPrototypes
 * @{
 */
static USBH_StatusTypeDef USBH_MTP_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_MTP_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_MTP_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_MTP_Process(USBH_HandleTypeDef *phost);

static uint8_t MTP_FindCtlEndpoint(USBH_HandleTypeDef *phost);
static uint8_t MTP_FindDataOutEndpoint(USBH_HandleTypeDef *phost);
static uint8_t MTP_FindDataInEndpoint(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_MTP_SOFProcess(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_MTP_Events(USBH_HandleTypeDef *phost);

static void MTP_DecodeEvent(USBH_HandleTypeDef *phost);

USBH_ClassTypeDef MTP_Class =
{
  "MTP",
  USB_MTP_CLASS,
  USBH_MTP_InterfaceInit,
  USBH_MTP_InterfaceDeInit,
  USBH_MTP_ClassRequest,
  USBH_MTP_Process,
  USBH_MTP_SOFProcess,
  NULL,
};

/**
 * @}
 */

/**@defgroup USBH_MTP_CORE_Private_Functions
 * @{
 */

/**
 * @brief  USBH_MTP_InterfaceInit
 *         Initialize the MTP class.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
static USBH_StatusTypeDef  USBH_MTP_InterfaceInit(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status;
  uint8_t interface, endpoint;
  MTP_HandleTypeDef *MTP_Handle;

  interface = USBH_FindInterface(phost, USB_MTP_CLASS, 1U, 1U);

  if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_INTERFACES))   /* no valid interface */
  {
    USBH_DbgLog("Cannot Find the interface for still Image Class.");
    return USBH_FAIL;
  }

  (void)USBH_SelectInterface(phost, interface);
  status = USBH_SelectInterface(phost, interface);

  if (status != USBH_OK)
  {
    return USBH_FAIL;
  }

  endpoint = MTP_FindCtlEndpoint(phost);
  if ((endpoint == 0xFFU) || (endpoint >= USBH_MAX_NUM_ENDPOINTS))
  {
    USBH_DbgLog("Invalid Control endpoint number");
    return USBH_FAIL;
  }

  phost->pActiveClass->pData = (MTP_HandleTypeDef *)USBH_malloc(sizeof(MTP_HandleTypeDef));
  MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;

  if (MTP_Handle == NULL)
  {
    USBH_DbgLog("Cannot allocate memory for MTP Handle");
    return USBH_FAIL;
  }

  /* Initialize mtp handler */
  (void)USBH_memset(MTP_Handle, 0, sizeof(MTP_HandleTypeDef));

  /* Collect the control endpoint address and length */
  MTP_Handle->NotificationEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].bEndpointAddress;
  MTP_Handle->NotificationEpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].wMaxPacketSize;
  MTP_Handle->NotificationPipe = USBH_AllocPipe(phost, MTP_Handle->NotificationEp);
  MTP_Handle->events.poll = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].bInterval;

  /* open pipe for notification endpoint */
  (void)USBH_OpenPipe(phost, MTP_Handle->NotificationPipe, MTP_Handle->NotificationEp,
                      phost->device.address, phost->device.speed,
                      USB_EP_TYPE_INTR, MTP_Handle->NotificationEpSize);
  (void)USBH_LL_SetToggle(phost, MTP_Handle->NotificationPipe, 0U);

  endpoint = MTP_FindDataInEndpoint(phost);
  if ((endpoint == 0xFFU) || (endpoint >= USBH_MAX_NUM_ENDPOINTS))
  {
    USBH_DbgLog("Invalid Data IN endpoint number");
    return USBH_FAIL;
  }

  /* Collect the data in endpoint address and length */
  MTP_Handle->DataInEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].bEndpointAddress;
  MTP_Handle->DataInEpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].wMaxPacketSize;
  MTP_Handle->DataInPipe = USBH_AllocPipe(phost, MTP_Handle->DataInEp);

  /* open pipe for data in endpoint */
  (void)USBH_OpenPipe(phost, MTP_Handle->DataInPipe, MTP_Handle->DataInEp,
                      phost->device.address, phost->device.speed,
                      USB_EP_TYPE_BULK, MTP_Handle->DataInEpSize);
  (void)USBH_LL_SetToggle(phost, MTP_Handle->DataInPipe, 0U);

  endpoint = MTP_FindDataOutEndpoint(phost);
  if ((endpoint == 0xFFU) || (endpoint >= USBH_MAX_NUM_ENDPOINTS))
  {
    USBH_DbgLog("Invalid Data OUT endpoint number");
    return USBH_FAIL;
  }

  /* Collect the data out endpoint address and length */
  MTP_Handle->DataOutEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].bEndpointAddress;
  MTP_Handle->DataOutEpSize = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].wMaxPacketSize;
  MTP_Handle->DataOutPipe = USBH_AllocPipe(phost, MTP_Handle->DataOutEp);

  /* open pipe for data out endpoint */
  (void)USBH_OpenPipe(phost, MTP_Handle->DataOutPipe, MTP_Handle->DataOutEp,
                      phost->device.address, phost->device.speed,
                      USB_EP_TYPE_BULK, MTP_Handle->DataOutEpSize);
  (void)USBH_LL_SetToggle(phost, MTP_Handle->DataOutPipe, 0U);

  MTP_Handle->state = MTP_OPENSESSION;
  MTP_Handle->is_ready = 0U;
  MTP_Handle->events.state = MTP_EVENTS_INIT;

  return USBH_PTP_Init(phost);
}

/**
 * @brief  USBH_MTP_InterfaceDeInit
 *         DeInitialize the Pipes used for the MTP class.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
static USBH_StatusTypeDef  USBH_MTP_InterfaceDeInit(USBH_HandleTypeDef *phost)
{
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;

  if (MTP_Handle->DataOutPipe != 0U)
  {
    (void)USBH_ClosePipe(phost, MTP_Handle->DataOutPipe);
    (void)USBH_FreePipe(phost, MTP_Handle->DataOutPipe);
    MTP_Handle->DataOutPipe = 0U;    /* Reset the pipe as Free */
  }

  if (MTP_Handle->DataInPipe != 0U)
  {
    (void)USBH_ClosePipe(phost, MTP_Handle->DataInPipe);
    (void)USBH_FreePipe(phost, MTP_Handle->DataInPipe);
    MTP_Handle->DataInPipe = 0U;    /* Reset the pipe as Free */
  }

  if (MTP_Handle->NotificationPipe != 0U)
  {
    (void)USBH_ClosePipe(phost, MTP_Handle->NotificationPipe);
    (void)USBH_FreePipe(phost, MTP_Handle->NotificationPipe);
    MTP_Handle->NotificationPipe = 0U;    /* Reset the pipe as Free */
  }

  if ((phost->pActiveClass->pData) != NULL)
  {
    USBH_free(phost->pActiveClass->pData);
    phost->pActiveClass->pData = 0U;
  }
  return USBH_OK;
}

/**
 * @brief  MTP_FindCtlEndpoint
 *         Find MTP control endpoints.
 * @param  phost: Host Handle
 * @retval endpoint number
 */
static uint8_t MTP_FindCtlEndpoint(USBH_HandleTypeDef *phost)
{
  uint8_t interface, endpoint;
  for (interface = 0U; interface < USBH_MAX_NUM_INTERFACES; interface++)
  {
    if (phost->device.CfgDesc.Itf_Desc[interface].bInterfaceClass == USB_MTP_CLASS)
    {
      for (endpoint = 0U; endpoint < USBH_MAX_NUM_ENDPOINTS; endpoint++)
      {
        if (((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].bEndpointAddress & 0x80U) != 0U) &&
            (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].wMaxPacketSize > 0U) &&
            ((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].bmAttributes & USBH_EP_INTERRUPT) == USBH_EP_INTERRUPT))
        {
          return endpoint;
        }
      }
    }
  }
  return 0xFFU; /* invalid endpoint */
}

/**
 * @brief  MTP_FindDataOutEndpoint
 *         Find MTP data out endpoints.
 * @param  phost: Host Handle
 * @retval endpoint number
 */
static uint8_t MTP_FindDataOutEndpoint(USBH_HandleTypeDef *phost)
{
  uint8_t interface, endpoint;
  for (interface = 0U; interface < USBH_MAX_NUM_INTERFACES; interface++)
  {
    if (phost->device.CfgDesc.Itf_Desc[interface].bInterfaceClass == USB_MTP_CLASS)
    {
      for (endpoint = 0U; endpoint < USBH_MAX_NUM_ENDPOINTS; endpoint++)
      {
        if (((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].bEndpointAddress & 0x80U) == 0U) &&
            (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].wMaxPacketSize > 0U) &&
            ((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].bmAttributes & USBH_EP_BULK) == USBH_EP_BULK))
        {
          return endpoint;
        }
      }
    }
  }
  return 0xFFU; /* invalid endpoint */
}

/**
 * @brief  MTP_FindDataInEndpoint
 *         Find MTP data in endpoints.
 * @param  phost: Host Handle
 * @retval endpoint number
 */
static uint8_t MTP_FindDataInEndpoint(USBH_HandleTypeDef *phost)
{
  uint8_t interface, endpoint;
  for (interface = 0U; interface < USBH_MAX_NUM_INTERFACES; interface++)
  {
    if (phost->device.CfgDesc.Itf_Desc[interface].bInterfaceClass == USB_MTP_CLASS)
    {
      for (endpoint = 0U; endpoint < USBH_MAX_NUM_ENDPOINTS; endpoint++)
      {
        if (((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].bEndpointAddress & 0x80U) != 0U) &&
            (phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].wMaxPacketSize > 0U) &&
            ((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[endpoint].bmAttributes & USBH_EP_BULK) == USBH_EP_BULK))
        {
          return endpoint;
        }
      }
    }
  }
  return 0xFFU; /* invalid endpoint */
}

/**
 * @brief  USBH_MTP_ClassRequest
 *         The function is responsible for handling Standard requests for MTP class.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
static USBH_StatusTypeDef USBH_MTP_ClassRequest(USBH_HandleTypeDef *phost)
{
  return USBH_OK;
}

/**
 * @brief  USBH_MTP_Process
 *         The function is for managing state machine for MTP data transfers.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
static USBH_StatusTypeDef USBH_MTP_Process(USBH_HandleTypeDef *phost)
{
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  USBH_StatusTypeDef status = USBH_BUSY;
  uint32_t idx = 0U;

  switch (MTP_Handle->state)
  {
    case MTP_OPENSESSION:
      status = USBH_PTP_OpenSession(phost, 1U);  /* session '0' is not valid */
      if (status == USBH_OK)
      {
        USBH_UsrLog("MTP session #0 opened");
        MTP_Handle->state = MTP_GETDEVICEINFO;
      }
      break;

    case MTP_GETDEVICEINFO:
      status = USBH_PTP_GetDeviceInfo(phost, &(MTP_Handle->info.devinfo));
      if (status == USBH_OK)
      {
        USBH_DbgLog(">>>>> MTP Device Information");
        USBH_DbgLog("Standard version : %x", MTP_Handle->info.devinfo.StandardVersion);
        USBH_DbgLog("Vendor ExtID : %s", (MTP_Handle->info.devinfo.VendorExtensionID == 6) ? "MTP" : "NOT SUPPORTED");
        USBH_DbgLog("Functional mode : %s", (MTP_Handle->info.devinfo.FunctionalMode == 0U) ? "Standard" : "Vendor");
        USBH_DbgLog("Number of Supported Operation(s) : %d", MTP_Handle->info.devinfo.OperationsSupported_len);
        USBH_DbgLog("Number of Supported Events(s) : %d", MTP_Handle->info.devinfo.EventsSupported_len);
        USBH_DbgLog("Number of Supported Proprieties : %d", MTP_Handle->info.devinfo.DevicePropertiesSupported_len);
        USBH_DbgLog("Manufacturer : %s", MTP_Handle->info.devinfo.Manufacturer);
        USBH_DbgLog("Model : %s", MTP_Handle->info.devinfo.Model);
        USBH_DbgLog("Device version : %s", MTP_Handle->info.devinfo.DeviceVersion);
        USBH_DbgLog("Serial number : %s", MTP_Handle->info.devinfo.SerialNumber);

        MTP_Handle->state = MTP_GETSTORAGEIDS;
      }
      break;

    case MTP_GETSTORAGEIDS:
      status = USBH_PTP_GetStorageIds(phost, &(MTP_Handle->info.storids));
      if (status == USBH_OK)
      {
        USBH_DbgLog("Number of storage ID items : %d", MTP_Handle->info.storids.n);
        for (idx = 0U; idx < MTP_Handle->info.storids.n; idx++)
        {
          USBH_DbgLog("Storage#%d ID : %x", idx, MTP_Handle->info.storids.Storage[idx]);
        }
        MTP_Handle->current_storage_unit = 0U;
        MTP_Handle->state = MTP_GETSTORAGEINFO;
      }
      break;

    case MTP_GETSTORAGEINFO:
      status = USBH_PTP_GetStorageInfo(phost, MTP_Handle->info.storids.Storage[MTP_Handle->current_storage_unit],
                                       &((MTP_Handle->info.storinfo)[MTP_Handle->current_storage_unit]));

      if (status == USBH_OK)
      {
        USBH_DbgLog("Volume#%1u : %s  [%s]", MTP_Handle->current_storage_unit,
                    MTP_Handle->info.storinfo[MTP_Handle->current_storage_unit].StorageDescription,
                    MTP_Handle->info.storinfo[MTP_Handle->current_storage_unit].VolumeLabel);
        if (++MTP_Handle->current_storage_unit >= MTP_Handle->info.storids.n)
        {
          MTP_Handle->state = MTP_IDLE;
          MTP_Handle->is_ready = 1U;
          MTP_Handle->current_storage_unit = 0U;
          MTP_Handle->params.CurrentStorageId = MTP_Handle->info.storids.Storage[0];

          USBH_DbgLog("MTP Class initialized.");
          USBH_DbgLog("%s is default storage unit", MTP_Handle->info.storinfo[0].StorageDescription);
          phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
        }
      }
      break;

    case MTP_IDLE:
      (void)USBH_MTP_Events(phost);
      status = USBH_OK;
      break;

    default:
      break;
  }
  return status;
}

/**
 * @brief  USBH_MTP_SOFProcess
 *         The function is for managing the SOF Process.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
static USBH_StatusTypeDef USBH_MTP_SOFProcess(USBH_HandleTypeDef *phost)
{
  return USBH_OK;
}

/**
 * @brief  USBH_MTP_IsReady
 *         Select the storage unit to be used
 * @param  phost: Host Handle
 * @retval USBH Status
 */
uint8_t USBH_MTP_IsReady(USBH_HandleTypeDef *phost)
{
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *) phost->pActiveClass->pData;

  return ((uint8_t)MTP_Handle->is_ready);
}


/**
 * @brief  USBH_MTP_GetNumStorage
 *         Get storage unit number
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetNumStorage(USBH_HandleTypeDef *phost, uint8_t *storage_num)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;

  if (MTP_Handle->is_ready > 0U)
  {
    *storage_num = (uint8_t)MTP_Handle->info.storids.n;
    status == USBH_OK;
  }

  return status;
}

/**
 * @brief  USBH_MTP_SelectStorage
 *         Select the storage unit to be used
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_SelectStorage(USBH_HandleTypeDef *phost, uint8_t storage_idx)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;

  if ((storage_idx < MTP_Handle->info.storids.n) && (MTP_Handle->is_ready == 1U))
  {
    MTP_Handle->params.CurrentStorageId = MTP_Handle->info.storids.Storage[storage_idx];
    status == USBH_OK;
  }

  return status;
}

/**
 * @brief  USBH_MTP_GetStorageInfo
 *         Get the storage unit info
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetStorageInfo(USBH_HandleTypeDef *phost, uint8_t storage_idx, MTP_StorageInfoTypedef *info)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;

  if ((storage_idx < MTP_Handle->info.storids.n) && (MTP_Handle->is_ready == 1U))
  {
    *info = MTP_Handle->info.storinfo[storage_idx];
    status == USBH_OK;
  }

  return status;
}

/**
 * @brief  USBH_MTP_GetNumObjects
 *         Get the Objects number
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetNumObjects(USBH_HandleTypeDef *phost, uint32_t storage_idx,
                                          uint32_t objectformatcode, uint32_t associationOH,
                                          uint32_t *numobs)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((storage_idx < MTP_Handle->info.storids.n) && (MTP_Handle->is_ready == 1U))
  {
    while ((status = USBH_PTP_GetNumObjects(phost, MTP_Handle->info.storids.Storage[storage_idx],
                                            objectformatcode, associationOH, numobs))
                                            == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_GetObjectHandles
 *         Get the Object Handle
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetObjectHandles(USBH_HandleTypeDef *phost, uint32_t storage_idx,
                                             uint32_t objectformatcode, uint32_t associationOH,
                                             PTP_ObjectHandlesTypedef *objecthandles)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((storage_idx < MTP_Handle->info.storids.n) && (MTP_Handle->is_ready == 1U))
  {
    while ((status = USBH_PTP_GetObjectHandles(phost, MTP_Handle->info.storids.Storage[storage_idx],
                                               objectformatcode, associationOH, objecthandles))
                                               == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_GetObjectInfo
 *         Get the Object info
 * @param  phost: Host Handle
 * @parma  handle: object handle
 * @param  dev_info: Device info structure
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetObjectInfo(USBH_HandleTypeDef *phost, uint32_t handle,
                                          PTP_ObjectInfoTypedef *objectinfo)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((MTP_Handle->is_ready) != 0U)
  {
    while ((status = USBH_PTP_GetObjectInfo(phost, handle, objectinfo)) == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_DeleteObject
 *         Delete an Object
 * @param  phost: Host Handle
 * @param  handle: object handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_DeleteObject(USBH_HandleTypeDef *phost, uint32_t handle,
                                         uint32_t objectformatcode)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((MTP_Handle->is_ready) != 0U)
  {
    while ((status = USBH_PTP_DeleteObject(phost, handle, objectformatcode)) == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_GetObject
 *         Get an Object
 * @param  phost: Host Handle
 * @param  handle: object handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetObject(USBH_HandleTypeDef *phost, uint32_t handle,
                                      uint8_t *object)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((MTP_Handle->is_ready) != 0U)
  {
    while ((status = USBH_PTP_GetObject(phost, handle, object)) == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_GetPartialObject
 *         Get Object partially
 * @param  phost: Host Handle
 * @param  handle: object handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetPartialObject(USBH_HandleTypeDef *phost, uint32_t handle,
                                             uint32_t offset, uint32_t maxbytes,
                                             uint8_t *object, uint32_t *len)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((MTP_Handle->is_ready) != 0U)
  {
    while ((status = USBH_PTP_GetPartialObject(phost, handle, offset, maxbytes, object,
                                               len)) == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_GetObjectPropsSupported
 *         Get Object Prop Supported
 * @param  phost: Host Handle
 * @param  handle: object handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetObjectPropsSupported(USBH_HandleTypeDef *phost, uint16_t ofc,
                                                    uint32_t *propnum, uint16_t *props)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((MTP_Handle->is_ready) != 0U)
  {
    while ((status = USBH_PTP_GetObjectPropsSupported(phost, ofc, propnum,
                                                      props)) == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_GetObjectPropDesc
 *         Get Object Prop description
 * @param  phost: Host Handle
 * @param  handle: object handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetObjectPropDesc(USBH_HandleTypeDef *phost, uint16_t opc,
                                              uint16_t ofc,
                                              PTP_ObjectPropDescTypeDef *opd)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((MTP_Handle->is_ready) != 0U)
  {
    while ((status = USBH_PTP_GetObjectPropDesc(phost, opc, ofc, opd)) == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_GetObjectPropList
 *         Get Object Prop list
 * @param  phost: Host Handle
 * @param  handle: object handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetObjectPropList(USBH_HandleTypeDef *phost, uint32_t handle,
                                              MTP_PropertiesTypedef *pprops, uint32_t *nrofprops)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((MTP_Handle->is_ready) != 0U)
  {
    while ((status = USBH_PTP_GetObjectPropList(phost, handle, pprops, nrofprops)) == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_SendObject
 *         Send an Object
 * @param  phost: Host Handle
 * @param  handle: object handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_SendObject(USBH_HandleTypeDef *phost, uint32_t handle,
                                       uint8_t *object, uint32_t size)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((MTP_Handle->is_ready) != 0U)
  {
    while ((status = USBH_PTP_SendObject(phost, handle, object, size)) == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_Events
 *         Handle MTP events
 * @param  phost: Host Handle
 * @retval USBH Status
 */
static USBH_StatusTypeDef USBH_MTP_Events(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;

  switch (MTP_Handle->events.state)
  {
    case MTP_EVENTS_INIT:
      if ((phost->Timer & 1U) == 0U)
      {
        MTP_Handle->events.timer = phost->Timer;
        (void)USBH_InterruptReceiveData(phost, ((uint8_t *)(void *) & (MTP_Handle->events.container)),
                                        (uint8_t)MTP_Handle->NotificationEpSize,
                                        MTP_Handle->NotificationPipe);

        MTP_Handle->events.state = MTP_EVENTS_GETDATA;
      }
      break;

    case MTP_EVENTS_GETDATA:
      if (USBH_LL_GetURBState(phost, MTP_Handle->NotificationPipe) == USBH_URB_DONE)
      {
        MTP_DecodeEvent(phost);
      }

      if ((phost->Timer - MTP_Handle->events.timer) >= MTP_Handle->events.poll)
      {
        MTP_Handle->events.timer = phost->Timer;
        (void)USBH_InterruptReceiveData(phost, ((uint8_t *)(void *) & (MTP_Handle->events.container)),
                                        (uint8_t)MTP_Handle->NotificationEpSize,
                                        MTP_Handle->NotificationPipe);

        MTP_Handle->events.state = MTP_EVENTS_GETDATA;
      }
      break;

    default:
      break;
  }

  return status;
}

/**
 * @brief  MTP_DecodeEvent
 *         Decode device event sent by responder
 * @param  phost: Host Handle
 * @retval USBH Status
 */
static void MTP_DecodeEvent(USBH_HandleTypeDef *phost)
{
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint16_t code;
  uint32_t param1;

  /* process the event */
  code = MTP_Handle->events.container.code;
  param1 = MTP_Handle->events.container.param1;

  switch (code)
  {
    case PTP_EC_Undefined:
      USBH_DbgLog("EVT: PTP_EC_Undefined in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_CancelTransaction:
      USBH_DbgLog("EVT: PTP_EC_CancelTransaction in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_ObjectAdded:
      USBH_DbgLog("EVT: PTP_EC_ObjectAdded in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_ObjectRemoved:
      USBH_DbgLog("EVT: PTP_EC_ObjectRemoved in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_StoreAdded:
      USBH_DbgLog("EVT: PTP_EC_StoreAdded in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_StoreRemoved:
      USBH_DbgLog("EVT: PTP_EC_StoreRemoved in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_DevicePropChanged:
      USBH_DbgLog("EVT: PTP_EC_DevicePropChanged in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_ObjectInfoChanged:
      USBH_DbgLog("EVT: PTP_EC_ObjectInfoChanged in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_DeviceInfoChanged:
      USBH_DbgLog("EVT: PTP_EC_DeviceInfoChanged in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_RequestObjectTransfer:
      USBH_DbgLog("EVT: PTP_EC_RequestObjectTransfer in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_StoreFull:
      USBH_DbgLog("EVT: PTP_EC_StoreFull in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_DeviceReset:
      USBH_DbgLog("EVT: PTP_EC_DeviceReset in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_StorageInfoChanged:
      USBH_DbgLog("EVT: PTP_EC_StorageInfoChanged in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_CaptureComplete:
      USBH_DbgLog("EVT: PTP_EC_CaptureComplete in session %u", MTP_Handle->ptp.session_id);
      break;

    case PTP_EC_UnreportedStatus:
      USBH_DbgLog("EVT: PTP_EC_UnreportedStatus in session %u", MTP_Handle->ptp.session_id);
      break;

    default:
      USBH_DbgLog("Received unkonwn event in session %u", MTP_Handle->ptp.session_id);
      break;
  }

  USBH_MTP_EventsCallback(phost, (uint32_t)code, param1);
}

/**
 * @brief  USBH_MTP_GetDevicePropDesc
 *         Get device prop description
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_MTP_GetDevicePropDesc(USBH_HandleTypeDef *phost, uint16_t propcode,
                                              PTP_DevicePropDescTypedef *devicepropertydesc)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MTP_HandleTypeDef *MTP_Handle = (MTP_HandleTypeDef *)phost->pActiveClass->pData;
  uint32_t timeout = phost->Timer;

  if ((MTP_Handle->is_ready) != 0U)
  {
    while ((status = USBH_PTP_GetDevicePropDesc(phost, propcode, devicepropertydesc)) == USBH_BUSY)
    {
      if (((phost->Timer - timeout) > 5000U) || (phost->device.is_connected == 0U))
      {
        return USBH_FAIL;
      }
    }
  }

  return status;
}

/**
 * @brief  USBH_MTP_EventsCallback
 *         Informs that host has received an event
 * @param  phost: Host Handle
 * @retval none
 */
void __attribute__((weak)) USBH_MTP_EventsCallback(USBH_HandleTypeDef *phost, uint32_t event,
                                                   uint32_t param)
{
  UNUSED(phost);
  UNUSED(event);
  UNUSED(param);
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
