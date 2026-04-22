/**
  ******************************************************************************
  * @file    			usbh_ctlreq.c
  * @author  			FMD XA
  * @brief   			This file implements the control request for device enumeration
  * @version 			V1.0.0           
  * @data		 			2025-04-10
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbh_ctlreq.h"


/**@addtogroup USBH_LIB
 * @{
 */

/**@addtogroup USBH_LIB_CORE
 * @{
 */

/**@defgroup USBH_CTLREQ
 * @brief This file implements the standard requests for device enumeration.
 * @{
 */

/**@defgroup USBH_CTLREQ_Private_Defines
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_CTLREQ_Private_TypeDefinitions
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_CTLREQ_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_CTLREQ_Private_Variables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_CTLREQ_Private_FunctionPrototypes
 * @{
 */
static USBH_StatusTypeDef USBH_HandleControl(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_ParseDevDesc(USBH_HandleTypeDef *phost, uint8_t *buf, uint16_t length);
static USBH_StatusTypeDef USBH_ParseCfgDesc(USBH_HandleTypeDef *phost, uint8_t *buf, uint16_t length);
static USBH_StatusTypeDef USBH_ParseEPDesc(USBH_HandleTypeDef *phost, USBH_EpDescTypeDef *ep_descriptor, uint8_t *buf);

static void USBH_ParseStringDesc(uint8_t *psrc, uint8_t *pdest, uint16_t length);
static void USBH_ParseInterfaceDesc(USBH_InterfaceDescTypeDef *in_descriptor, uint8_t *buf);
/**
 * @}
 */

/**@defgroup USBH_CTLREQ_Private_Functions
 * @{
 */

/**
 * @brief  USBH_Get_DevDesc
 *         Issue Get Device Descriptor command to the device. Once the response
 *         received, it parses the device descriptor and updates the status.
 * @param  phost: Host Handle
 * @param  length: Length of the descriptor
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_Get_DevDesc(USBH_HandleTypeDef *phost, uint16_t length)
{
  USBH_StatusTypeDef status;

  if (length > sizeof(phost->device.Data))
  {
    USBH_ErrLog("Control error: Get Device Descriptor failed, data buffer size issue");
    return USBH_NOT_SUPPORTED;
  }
  status = USBH_GetDescriptor(phost, (USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_STANDARD),
                              USB_DESC_DEVICE, phost->device.Data, length);
  if (status == USBH_OK)
  {
    /* Command successfully sent and Response Received */
    status = USBH_ParseDevDesc(phost, phost->device.Data, length);
  }

  return status;
}

/**
 * @brief  USBH_Get_CfgDesc
 *         Issue Configuration Descriptor to the device. Once the response received,
 *         it pares the configuration descriptor and updates the status.
 * @param  phost: Host Handle
 * @param  length: Length of the descriptor
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_Get_CfgDesc(USBH_HandleTypeDef *phost, uint16_t length)
{
  USBH_StatusTypeDef status;
  uint8_t *pData = phost->device.CfgDesc_Raw;

  if (length > sizeof(phost->device.CfgDesc_Raw))
  {
    USBH_ErrLog("Control error: Get configuration Descriptor failed, data buffer size issue");
    return USBH_NOT_SUPPORTED;
  }

  status = USBH_GetDescriptor(phost, (USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_STANDARD),
                              USB_DESC_CONFIGURATION, pData, length);
  if (status == USBH_OK)
  {
    /* Command successfully sent and Response Received */
    status = USBH_ParseCfgDesc(phost, pData, length);
  }

  return status;
}

/**
 * @brief  USBH_Get_StringDesc
 *         Issue String Descriptor command to the device. Once the response received,
 *         it pares the string descriptor and updates the status.
 * @param  phost: Host Handle
 * @param  string_index: String index for the descriptor
 * @param  buff : Buffer address for the descriptor
 * @param  length: Length of the descriptor
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_Get_StringDesc(USBH_HandleTypeDef *phost, uint8_t string_index, uint8_t *buff, uint16_t length)
{
  USBH_StatusTypeDef status;

  if ((length > sizeof(phost->device.Data)) || (buff == NULL))
  {
    USBH_ErrLog("Control error: Get String Descriptor failed, data buffer size issue");
    return USBH_NOT_SUPPORTED;
  }

  status = USBH_GetDescriptor(phost, (USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_STANDARD),
                              (USB_DESC_STRING | string_index),
                              phost->device.Data, length);
  if (status == USBH_OK)
  {
    /* Command successfully sent and Response Received */
    USBH_ParseStringDesc(phost->device.Data, buff, length);
  }

  return status;
}

/**
 * @brief  USBH_GetDescriptor
 *         Issue Descriptor command to the device. Once the response received,
 *         it pares the descriptor and updates the status.
 * @param  phost: Host Handle
 * @param  req_type: Descriptor type
 * @param  value_idx: Value for the GetDescriptor request
 * @param  buff : Buffer to store the descriptor
 * @param  length: Length of the descriptor
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_GetDescriptor(USBH_HandleTypeDef *phost, uint8_t req_type, uint16_t value_idx,
                                       uint8_t *buff, uint16_t length)
{

  if (phost->RequestState == CMD_SEND)
  {
    phost->Control.setup.b.bmRequestType = USB_D2H | req_type;
    phost->Control.setup.b.bRequest = USB_REQ_GET_DESCRIPTOR;
    phost->Control.setup.b.wValue.w = value_idx;

    if ((value_idx & 0xFF00U) == USB_DESC_STRING)
    {
      phost->Control.setup.b.wIndex.w = 0x0409U;
    }
    else
    {
      phost->Control.setup.b.wIndex.w = 0U;
    }
    phost->Control.setup.b.wLength.w = length;
  }
  return USBH_CtlReq(phost, buff, length);
}

/**
 * @brief  USBH_SetAddress
 *         This command sets the address to the connected device.
 * @param  phost: Host Handle
 * @param  DeviceAddress: Device address to assign
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_SetAddress(USBH_HandleTypeDef *phost, uint8_t DeviceAddress)
{

  if (phost->RequestState == CMD_SEND)
  {
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_STANDARD;
    phost->Control.setup.b.bRequest = USB_REQ_SET_ADDRESS;
    phost->Control.setup.b.wValue.w = (uint16_t)DeviceAddress;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = 0U;

  }

  return USBH_CtlReq(phost, NULL, 0U);
}

/**
 * @brief  USBH_SetCfg
 *         This command sets the configuration value to the connected device.
 * @param  phost: Host Handle
 * @param  cfg_idx: configuration value
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_SetCfg(USBH_HandleTypeDef *phost, uint16_t cfg_idx)
{

  if (phost->RequestState == CMD_SEND)
  {
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_STANDARD;
    phost->Control.setup.b.bRequest = USB_REQ_SET_CONFIGURATION;
    phost->Control.setup.b.wValue.w = cfg_idx;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = 0U;
  }

  return USBH_CtlReq(phost, NULL, 0U);
}

/**
 * @brief  USBH_SetInterface
 *         This command sets the Interface value to the connected device.
 * @param  phost: Host Handle
 * @param  cfg_idx: configuration value
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_SetInterface(USBH_HandleTypeDef *phost, uint8_t ep_num, uint8_t altSetting)
{

  if (phost->RequestState == CMD_SEND)
  {
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_STANDARD;
    phost->Control.setup.b.bRequest = USB_REQ_SET_INTERFACE;
    phost->Control.setup.b.wValue.w = altSetting;
    phost->Control.setup.b.wIndex.w = ep_num;
    phost->Control.setup.b.wLength.w = 0U;
  }

  return USBH_CtlReq(phost, NULL, 0U);
}

/**
 * @brief  USBH_SetFeature
 *         This command sets the device features (remote wakeup feature,...).
 * @param  pdev: Selected device
 * @param  itf_idx
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_SetFeature(USBH_HandleTypeDef *phost, uint8_t wValue)
{

  if (phost->RequestState == CMD_SEND)
  {
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_DEVICE | USB_REQ_TYPE_STANDARD;
    phost->Control.setup.b.bRequest = USB_REQ_SET_FEATURE;
    phost->Control.setup.b.wValue.w = wValue;
    phost->Control.setup.b.wIndex.w = 0U;
    phost->Control.setup.b.wLength.w = 0U;
  }

  return USBH_CtlReq(phost, NULL, 0U);
}

/**
 * @brief  USBH_ClrFeature
 *         This request is used to clear or disable a specific feature.
 * @param  phost: Host Handle
 * @param  ep_num: endpoint number
 * @param  hc_num: Host channel number
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_ClrFeature(USBH_HandleTypeDef *phost, uint8_t ep_num)
{

  if (phost->RequestState == CMD_SEND)
  {
    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_ENDPOINT | USB_REQ_TYPE_STANDARD;
    phost->Control.setup.b.bRequest = USB_REQ_CLEAR_FEATURE;
    phost->Control.setup.b.wValue.w = FEATURE_SELECTOR_ENDPOINT;
    phost->Control.setup.b.wIndex.w = ep_num;
    phost->Control.setup.b.wLength.w = 0U;
  }

  return USBH_CtlReq(phost, NULL, 0U);
}

/**
 * @brief  USBH_ParseDevDesc
 *         This function Parses the device descriptor.
 * @param  phost: Host Handle
 * @param  dev_desc: device_descriptor destination address
 * @param  buf: Buffer where the source descriptor is available
 * @param  length: Length of the descriptor
 * @retval USBH Status
 */
static USBH_StatusTypeDef  USBH_ParseDevDesc(USBH_HandleTypeDef *phost, uint8_t *buf, uint16_t length)
{
  USBH_DevDescTypeDef *dev_desc = &phost->device.DevDesc;
  USBH_StatusTypeDef status = USBH_OK;

  if (buf == NULL)
  {
    return USBH_FAIL;
  }

  dev_desc->bLength         = *(uint8_t *)(buf + 0U);
  dev_desc->bDescriptorType = *(uint8_t *)(buf + 1U);
  dev_desc->bcdUSB          = LE16(buf + 2U);
  dev_desc->bDeviceClass    = *(uint8_t *)(buf + 4U);
  dev_desc->bDeviceSubClass = *(uint8_t *)(buf + 5U);
  dev_desc->bDeviceProtocol = *(uint8_t *)(buf + 6U);
  dev_desc->bMaxPacketSize  = *(uint8_t *)(buf + 7U);

  if ((phost->device.speed == (uint8_t)USBH_SPEED_HIGH) ||
      (phost->device.speed == (uint8_t)USBH_SPEED_FULL))
  {
    /* Make sure that the max packet size is either 8, 16, 32, 64 or force it to minimum allowed value */
    switch (dev_desc->bMaxPacketSize)
    {
      case 8 :
      case 16:
      case 32:
      case 64:
        break;

      default:
        /* set the size to min allowed value in case the device has answered with incorrect size */
        dev_desc->bMaxPacketSize = 8U;
        break;
    }
  }
  else if (phost->device.speed == (uint8_t)USBH_SPEED_LOW)
  {
    if (dev_desc->bMaxPacketSize != 8U)
    {
      /* set the size to 8 in case the device has answered with incorrect size */
      dev_desc->bMaxPacketSize = 8U;
    }
  }
  else
  {
    status = USBH_NOT_SUPPORTED;
  }
  if (length > 8U)
  {
    /* for 1st time after device connection, Host may issue only 8 bytes for device descriptor length */
    dev_desc->idVendor            = LE16(buf + 8U);
    dev_desc->idProduct           = LE16(buf + 10U);
    dev_desc->bcdDevice           = LE16(buf + 12U);
    dev_desc->iManufacturer       = *(uint8_t *)(buf + 14U);
    dev_desc->iProduct            = *(uint8_t *)(buf + 15U);
    dev_desc->iSerialNumber       = *(uint8_t *)(buf + 16U);
    dev_desc->bNumConfigurations  = *(uint8_t *)(buf + 17U);
  }
  return status;
}

/**
 * @brief  USBH_ParseCfgDesc
 *         This function Parses the configuration descriptor.
 * @param  phost: Host Handle
 * @param  buf: Buffer where the source descriptor is available
 * @param  length: Length of the descriptor
 * @retval USBH Status
 */
static USBH_StatusTypeDef  USBH_ParseCfgDesc(USBH_HandleTypeDef *phost, uint8_t *buf, uint16_t length)
{
  USBH_CfgDescTypeDef *cfg_desc = &phost->device.CfgDesc;
  USBH_StatusTypeDef  status = USBH_OK;
  USBH_InterfaceDescTypeDef  *pif;
  USBH_EpDescTypeDef         *pep;
  USBH_DescHeader_t          *pdesc;
  uint16_t                   ptr;
  uint8_t                    if_ix = 0U;
  uint8_t                    ep_ix = 0U;

  if (buf == NULL)
  {
    return USBH_FAIL;
  }

  pdesc = (USBH_DescHeader_t *)(void *)buf;

  /* make sure that the configuration descriptor's bLength is euqal to USB_CONFIGURATION_DESC_SIZE */
  if (pdesc->bLength != USB_CONFIGURATION_DESC_SIZE)
  {
    pdesc->bLength = USB_CONFIGURATION_DESC_SIZE;
  }

  /* parse configuration descriptor */
  cfg_desc->bLength             = *(uint8_t *)(buf + 0U);
  cfg_desc->bDescriptorType     = *(uint8_t *)(buf + 1U);
  cfg_desc->wTotalLength        = MIN(((uint16_t) LE16(buf + 2U)), ((uint16_t)USBH_MAX_SIZE_CONFIGURATION));
  cfg_desc->bNumInterfaces      = *(uint8_t *)(buf + 4U);
  cfg_desc->bConfigurationValue = *(uint8_t *)(buf + 5U);
  cfg_desc->iConfiguration      = *(uint8_t *)(buf + 6U);
  cfg_desc->bmAttributes        = *(uint8_t *)(buf + 7U);
  cfg_desc->bMaxPower           = *(uint8_t *)(buf + 8U);

  if (length > USB_CONFIGURATION_DESC_SIZE)
  {
    ptr = USB_LEN_CFG_DESC;
    pif = (USBH_InterfaceDescTypeDef *)NULL;

    while ((if_ix < USBH_MAX_NUM_INTERFACES) && (ptr < cfg_desc->wTotalLength))
    {
      pdesc = USBH_GetNextDesc((uint8_t *)(void *)pdesc, &ptr);
      if (pdesc->bDescriptorType == USB_DESC_TYPE_INTERFACE)
      {
        /* make sure that the interface descriptor's bLength is equal to USB_INTERFACE_DESC_SIZE */
        if (pdesc->bLength != USB_INTERFACE_DESC_SIZE)
        {
          pdesc->bLength = USB_INTERFACE_DESC_SIZE;
        }
        pif = &cfg_desc->Itf_Desc[if_ix];
        USBH_ParseInterfaceDesc(pif, (uint8_t *)(void *)pdesc);

        ep_ix = 0U;
        pep = (USBH_EpDescTypeDef *)NULL;

        while ((ep_ix < pif->bNumEndpoints) && (ptr < cfg_desc->wTotalLength))
        {
          pdesc = USBH_GetNextDesc((uint8_t *)(void *)pdesc, &ptr);

          if (pdesc->bDescriptorType == USB_DESC_TYPE_ENDPOINT)
          {
            /* check if the endpoint is appartening to an audio streaming interface */
            if ((pif->bInterfaceClass == 0x01U) &&
                ((pif->bInterfaceSubClass == 0x02U) || (pif->bInterfaceSubClass == 0x03U)))
            {
              /* check if it is supporting the USB AUDIO 01 class specification */
              if ((pif->bInterfaceProtocol == 0x00U) && (pdesc->bLength != 0x09U))
              {
                pdesc->bLength = 0x09U;
              }
            }
            /* make sure that the endpoint descriptor's bLength is equal to USB_ENDPOINT_DESC_SIZE for all other endpoint types*/
            else
            {
              pdesc->bLength = USB_ENDPOINT_DESC_SIZE;
            }
            pep = &cfg_desc->Itf_Desc[if_ix].Ep_Desc[ep_ix];

            status = USBH_ParseEPDesc(phost, pep, (uint8_t *)(void *)pdesc);

            ep_ix++;
          }
        }
        /* check if the required endpoint data are parsed */
        if (ep_ix < pif->bNumEndpoints)
        {
          return USBH_NOT_SUPPORTED;
        }
        if_ix++;
      }
    }
    /* check if the required endpoint data are parsed */
    if (if_ix < MIN(cfg_desc->bNumInterfaces, (uint8_t)USBH_MAX_NUM_INTERFACES))
    {
      return USBH_NOT_SUPPORTED;
    }
  }

  return status;

}

/**
 * @brief  USBH_ParseInterfaceDesc
 *         This function Parses the interface descriptor.
 * @param  if_descriptor: interface descriptor destination
 * @param  buf: Buffer where the source descriptor is available
 * @retval none
 */
static void  USBH_ParseInterfaceDesc(USBH_InterfaceDescTypeDef *if_descriptor, uint8_t *buf)
{
  if_descriptor->bLength             = *(uint8_t *)(buf + 0U);
  if_descriptor->bDescriptorType     = *(uint8_t *)(buf + 1U);
  if_descriptor->bInterfaceNumber    = *(uint8_t *)(buf + 2U);
  if_descriptor->bAlternateSetting   = *(uint8_t *)(buf + 3U);
  if_descriptor->bNumEndpoints       = MIN(*(uint8_t *)(buf + 4U), USBH_MAX_NUM_ENDPOINTS);
  if_descriptor->bInterfaceClass     = *(uint8_t *)(buf + 5U);
  if_descriptor->bInterfaceSubClass  = *(uint8_t *)(buf + 6U);
  if_descriptor->bInterfaceProtocol  = *(uint8_t *)(buf + 7U);
  if_descriptor->iInterface          = *(uint8_t *)(buf + 8U);

}

/**
 * @brief  USBH_ParseEPDesc
 *         This function Parses the endpoint descriptor.
 * @param  phost: Host Handle
 * @param  ep_descriptor: endpoint descriptor destination address
 * @param  buf: Buffer where the parsed descriptor is available
 * @retval USBH Status
 */
static USBH_StatusTypeDef  USBH_ParseEPDesc(USBH_HandleTypeDef *phost, USBH_EpDescTypeDef *ep_descriptor, uint8_t *buf)
{
  USBH_StatusTypeDef  status = USBH_OK;

  /* parse endpoint descriptor */
  ep_descriptor->bLength             = *(uint8_t *)(buf + 0U);
  ep_descriptor->bDescriptorType     = *(uint8_t *)(buf + 1U);
  ep_descriptor->bEndpointAddress    = *(uint8_t *)(buf + 2U);
  ep_descriptor->bmAttributes        = *(uint8_t *)(buf + 3U);
  ep_descriptor->wMaxPacketSize      = LE16(buf + 4U);
  ep_descriptor->bInterval           = *(uint8_t *)(buf + 6U);

  /* make sure that wMaxPacketSize is different from 0 */
  if ((ep_descriptor->wMaxPacketSize == 0x00U) ||
      (ep_descriptor->wMaxPacketSize > USBH_MAX_EP_PACKET_SIZE) ||
      (ep_descriptor->wMaxPacketSize > USBH_MAX_DATA_BUFFER))
  {
    status = USBH_NOT_SUPPORTED;
  }

  if (phost->device.speed == (uint8_t)USBH_SPEED_HIGH)
  {
    if ((ep_descriptor->bmAttributes & EP_TYPE_MSK) == EP_TYPE_BULK)
    {
      if (ep_descriptor->wMaxPacketSize > 512U)
      {
        status = USBH_NOT_SUPPORTED;
      }
    }
    else if ((ep_descriptor->bmAttributes & EP_TYPE_MSK) == EP_TYPE_CTRL)
    {
      if (ep_descriptor->wMaxPacketSize > 64U)
      {
        status = USBH_NOT_SUPPORTED;
      }
    }
    /* for high-speed interrupt/isochronous endpoints, bInterval van vary form 1 to 16 */
    else if (((ep_descriptor->bmAttributes & EP_TYPE_MSK) == EP_TYPE_ISOC) ||
            ((ep_descriptor->bmAttributes & EP_TYPE_MSK) == EP_TYPE_INTR))
    {
      if ((ep_descriptor->bInterval == 0U) || (ep_descriptor->bInterval > 0x10U))
      {
        status = USBH_NOT_SUPPORTED;
      }
    }
    else
    {
      status = USBH_NOT_SUPPORTED;
    }
  }
  else if (phost->device.speed == (uint8_t)USBH_SPEED_FULL)
  {
    if (((ep_descriptor->bmAttributes & EP_TYPE_MSK) == EP_TYPE_BULK) ||
        ((ep_descriptor->bmAttributes & EP_TYPE_MSK) == EP_TYPE_CTRL))
    {
      if (ep_descriptor->wMaxPacketSize > 64U)
      {
        status = USBH_NOT_SUPPORTED;
      }
    }
    /* for full-speed isochronous endpoints, the value of bInterval must be in the range form 1 to 16 */
    else if ((ep_descriptor->bmAttributes & EP_TYPE_MSK) == EP_TYPE_ISOC)
    {
      if ((ep_descriptor->bInterval == 0U) || (ep_descriptor->bInterval > 0x10U) || (ep_descriptor->wMaxPacketSize > 64U))
      {
        status = USBH_NOT_SUPPORTED;
      }
    }
    /* for full-speed interrupt endpoints, the value of bInterval must be form 1 to 255 */
    else if ((ep_descriptor->bmAttributes & EP_TYPE_MSK) == EP_TYPE_INTR)
    {
      if ((ep_descriptor->bInterval == 0U) || (ep_descriptor->wMaxPacketSize > 1023U))
      {
        status = USBH_NOT_SUPPORTED;
      }
    }
    else
    {
      status = USBH_NOT_SUPPORTED;
    }
  }
  else if(phost->device.speed == (uint8_t)USBH_SPEED_LOW)
  {
    if ((ep_descriptor->bmAttributes & EP_TYPE_MSK) == EP_TYPE_CTRL)
    {
      if (ep_descriptor->wMaxPacketSize != 8U)
      {
        status = USBH_NOT_SUPPORTED;
      }
    }
    /* for low-speed interrupt endpoints, the value of bInterval must be form 1 to 255 */
    else if ((ep_descriptor->bmAttributes & EP_TYPE_MSK) == EP_TYPE_INTR)
    {
      if ((ep_descriptor->bInterval == 0U) || (ep_descriptor->wMaxPacketSize > 8U))
      {
        status = USBH_NOT_SUPPORTED;
      }
    }
    else
    {
      status = USBH_NOT_SUPPORTED;
    }
  }
  else
  {
    status = USBH_NOT_SUPPORTED;
  }
  return status;

}

/**
 * @brief  USBH_ParseStringDesc
 *         This function Parses the string descriptor.
 * @param  psrc: Source pointer containing the descriptor data
 * @param  pdest: destination address pointer
 * @param  length: Length o fthe descriptor
 * @retval None
 */
static void USBH_ParseStringDesc(uint8_t *psrc, uint8_t *pdest, uint16_t length)
{
  uint16_t strlength;
  uint16_t idx;

  /* the unicode string descriptor is not NULL-terminated. the string length is computer by subtracting two from the value of the first byte of the descriptor. */

  /* check which is lower size, the size of string or the length of bytes read from the device */
  if (psrc[1] == USB_DESC_TYPE_STRING)
  {
    /* make sure the descriptor is string type */

    /* psrc[0] contains size of descriptor, subtract 2 to get the length of string */
    strlength = ((((uint16_t)psrc[0] - 2U) <= length) ? ((uint16_t)psrc[0] - 2U) : length);

    /* adiust the offset ignoring the string len an descriptor type */
    psrc += 2U;

    for (idx = 0U; idx < strlength; idx += 2U)
    {
      /* copy only the string and ignore the unicode id, hence add the str */
      *pdest = psrc[idx];
      pdest++;
    }
    *pdest = 0U; /* mark end of string */
  }
}

/**
 * @brief  USBH_GetNextDesc
 *         This function return the next descriptor header.
 * @param  buf: Buffer where the cfg descriptor is avaliable
 * @param  ptr: data pointer inside the cfg descriptor
 * @retval next header
 */
USBH_DescHeader_t *USBH_GetNextDesc(uint8_t *pbuf, uint16_t *ptr)
{
  USBH_DescHeader_t *pnext;

  *ptr += ((USBH_DescHeader_t *)(void *)pbuf)->bLength;
  pnext = (USBH_DescHeader_t *)(void *)((uint8_t *)(void *)pbuf + ((USBH_DescHeader_t *)(void *)pbuf)->bLength);

  return(pnext);
}

/**
 * @brief  USBH_CtlReq
 *         USBH_CtlReq send a control request and prodive the status after completion of the request.
 * @param  phost: Host Handle
 * @param  req: setup request structure
 * @param  buf: data buffer address to store the response
 * @param  length: length of the response
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_CtlReq(USBH_HandleTypeDef *phost, uint8_t *buff, uint16_t length)
{
  USBH_StatusTypeDef status;
  status = USBH_BUSY;
  switch (phost->RequestState)
  {
    case CMD_SEND:
      /* strat a setup transfer */
      phost->Control.buff = buff;
      phost->Control.length = length;
      phost->Control.state = CTRL_SETUP;
      phost->RequestState = CMD_WAIT;
      status = USBH_BUSY;
      break;

    case CMD_WAIT:
      status = USBH_HandleControl(phost);
      if ((status == USBH_OK) || (status == USBH_NOT_SUPPORTED))
      {
        /* Transaction completed, move control state to idle */
        phost->RequestState = CMD_SEND;
        phost->Control.state = CTRL_IDLE;
      }
      else if (status == USBH_FAIL)
      {
        /* failure mode */
        phost->RequestState = CMD_SEND;
      }
      else
      {
        /*...*/
      }
      break;

    default:
      break;
  }
  return status;
}

/**
 * @brief  USBH_HandleControl
 *         Handles the USB control transfer state machine.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
static USBH_StatusTypeDef USBH_HandleControl(USBH_HandleTypeDef *phost)
{
  uint8_t direction;
  USBH_StatusTypeDef status = USBH_BUSY;
  USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;
  switch (phost->Control.state)
  {
    case CTRL_SETUP:
      /* send a SETUP packet */
      (void)USBH_CtlSendSetup(phost, (uint8_t *)(void *)phost->Control.setup.d8, phost->Control.pipe_out);
      phost->Control.state = CTRL_SETUP_WAIT;
      break;

    case CTRL_SETUP_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, phost->Control.pipe_out);
      /* case SETUP packet sent successfully */
      if (URB_Status == USBH_URB_DONE)
      {
        direction = (phost->Control.setup.b.bmRequestType & USB_REQ_DIR_MASK);

        /* check if there is a data stage */
        if (phost->Control.setup.b.wLength.w != 0U)
        {
          if (direction == USB_D2H)
          {
            /* Data direction is IN */
            phost->Control.state = CTRL_DATA_IN;
          }
          else
          {
            /*OUT */
            phost->Control.state = CTRL_DATA_OUT;
          }
        }
        /* no data stage */
        else
        {
          /* if there is no data transfer stage */
          if (direction == USB_D2H)
          {
            /* data direction is IN */
            phost->Control.state = CTRL_STATUS_OUT;
          }
          else
          {
            /* OUT */
            phost->Control.state = CTRL_STATUS_IN;
          }
        }
      }
      else
      {
        if ((URB_Status == USBH_URB_ERROR) || (URB_Status == USBH_URB_NOTREADY))
        {
          phost->Control.state = CTRL_ERROR;
        }
      }
      break;

    case CTRL_DATA_IN:
      /* issue an in token */
      phost->Control.timer = (uint16_t)phost->Timer;
#if defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U)
      phost->NakTimer = phost->Timer;
#endif /* defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U) */

      (void)USBH_CtlReceiveData(phost, phost->Control.buff,
                                phost->Control.length, phost->Control.pipe_in);
      phost->Control.state = CTRL_DATA_IN_WAIT;
      break;

    case CTRL_DATA_IN_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, phost->Control.pipe_in);
      /* check is DATA packet transferred successfully */
      if (URB_Status == USBH_URB_DONE)
      {
        phost->Control.state = CTRL_STATUS_OUT;
      }
#if defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U)
      else if (URB_Status == USBH_URB_NAK_WAIT)
      {
        phost->Control.state = CTRL_DATA_IN_WAIT;
        if ((phost->Timer = phost->NakTimer) > phost->NakTimeout)
        {
          phost->NakTimer = phost->Timer;
          USBH_ActivatePipe(phost, phost->Control.pipe_in);
        }
      }
#endif /* defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U) */      /* manage error cases */
      if (URB_Status == USBH_URB_STALL)
      {
        /* In stall case, return to previous machine state */
        status = USBH_NOT_SUPPORTED;
      }
      else
      {
        if (URB_Status == USBH_URB_ERROR)
        {
          /* device error */
          phost->Control.state = CTRL_ERROR;
        }
      }
      break;

    case CTRL_DATA_OUT:
    #ifdef USB_OTG_HS_CORE
      (void)USBH_CtlSendData(phost, phost->Control.buff, phost->Control.length, phost->Control.pipe_out, 1U);
    #elif USB_OTG_FS_CORE
      (void)USBH_CtlSendData(phost, phost->Control.buff, phost->Control.length,
                             phost->Control.pipe_out, CTRL_DATA);
    #else /*USB_OTG_FS_CORE*/
    #endif  /*USB_OTG_HS_CORE*/
      phost->Control.timer = (uint16_t)phost->Timer;
      phost->Control.state = CTRL_DATA_OUT_WAIT;

      break;

    case CTRL_DATA_OUT_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, phost->Control.pipe_out);

      if (URB_Status == USBH_URB_DONE)
      {
        /* if the setup pkt is sent successful, then change the state */
        phost->Control.state = CTRL_STATUS_IN;
      }
      /* handle error cases */
      else if (URB_Status == USBH_URB_STALL)
      {
        /* in stall case, return to previous machine state */
        phost->Control.state = CTRL_STALLED;
        status = USBH_NOT_SUPPORTED;
      }
      else if (URB_Status == USBH_URB_NOTREADY)
      {
        /* nack received from device */
        phost->Control.state = CTRL_DATA_OUT;
      }
      else
      {
        if (URB_Status == USBH_URB_ERROR)
        {
          /* device error */
          phost->Control.state = CTRL_ERROR;
          status = USBH_FAIL;
        }
      }
      break;

    case CTRL_STATUS_IN:
      /* send 0 bytes out packet */
      (void)USBH_CtlReceiveData(phost, NULL, 0U, phost->Control.pipe_in);

#if defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U)
      phost->NakTimer = phost->Timer;
#endif /* defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U) */

      phost->Control.timer = (uint16_t)phost->Timer;
      phost->Control.state = CTRL_STATUS_IN_WAIT;
      break;

    case CTRL_STATUS_IN_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, phost->Control.pipe_in);

      if (URB_Status == USBH_URB_DONE)
      {
        /* Control transfers completed, exit the state machine */
        phost->Control.state = CTRL_COMPLETE;
        status = USBH_OK;
      }
#if defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U)
      else if (URB_Status == USBH_URB_NAK_WAIT)
      {
        phost->Control.state = CTRL_STATUS_IN_WAIT;
        if ((phost->Timer - phost->NakTimer) > phost->NakTimeout)
        {
          phost->NakTimer = phost->Timer;
          USBH_ActivatePipe(phost, phost->Control.pipe_in);
        }
      }
#endif /* defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U) */
      else if (URB_Status == USBH_URB_ERROR)
      {
        phost->Control.state = CTRL_ERROR;
      }
      else
      {
        if (URB_Status == USBH_URB_STALL)
        {
          /* Control transfer completed, exit the state machine */
          status = USBH_NOT_SUPPORTED;
        }
      }
      break;

    case CTRL_STATUS_OUT:
    #ifdef USB_OTG_HS_CORE
      (void)USBH_CtlSendData(phost, NULL, 0U, phost->Control.pipe_out, 1U);
    #elif USB_OTG_FS_CORE
      (void)USBH_CtlSendData(phost, NULL, 0U, phost->Control.pipe_out, CTRL_STATUS);
    #else /*USB_OTG_FS_CORE*/
    #endif /*USB_OTG_HS_CORE*/
      phost->Control.timer = (uint16_t)phost->Timer;
      phost->Control.state = CTRL_STATUS_OUT_WAIT;
      break;

    case CTRL_STATUS_OUT_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, phost->Control.pipe_out);
      if (URB_Status == USBH_URB_DONE)
      {
        status = USBH_OK;
        phost->Control.state = CTRL_COMPLETE;
      }
      else if (URB_Status == USBH_URB_NOTREADY)
      {
        phost->Control.state = CTRL_STATUS_OUT;
      }
      else
      {
        if (URB_Status == USBH_URB_ERROR)
        {
          phost->Control.state = CTRL_ERROR;
        }
      }
      break;

    case CTRL_ERROR:
      /* After a halt condition is encountered or an error is detected by the host,
       * a control endpoint is allowed to recover by accepting the next setup pid;
       * i.e., recovery actions via sone other pipe are not required for control endpoints.
       * for the default control pipe, a device reset will ultimately be required to clear
       * the halt or error condition if the next setup pid id not accepted.*/
      if (++phost->Control.errorcount <= USBH_MAX_ERROR_COUNT)
      {
        /* do the transmission again, strating from setup packet */
        phost->Control.state = CTRL_SETUP;
        phost->RequestState = CMD_SEND;
      }
      else
      {
        phost->pUser(phost, HOST_USER_UNRECOVERED_ERROR);
        phost->Control.errorcount = 0U;
        USBH_ErrLog("Control error: Device not responding");

        /* free control pipes */
        (void)USBH_FreePipe(phost, phost->Control.pipe_out);
        (void)USBH_FreePipe(phost, phost->Control.pipe_in);

        phost->gState = HOST_IDLE;
        status = USBH_FAIL;
      }
      break;

    default:
      break;
  }

  return status;
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
