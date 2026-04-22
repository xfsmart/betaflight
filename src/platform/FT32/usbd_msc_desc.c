/**
  ******************************************************************************
  * @file    			usbd_msc_desc.c
  * @author  			FMD XA
  * @brief   			This file porvides the USBD descriptors and string formating method.
  * @version 			V1.0.0           
  * @data		 			2026-03-30
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/


#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usb_conf.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"
#include "usbd_conf.h"
#include "usbd_msc_desc.h"
#include "platform.h"

#ifdef  USE_USBD_COMPOSITE
#include "usbd_composite_builder.h"
#endif  /* USE_USBD_COMPOSITE */
/* Private typedef ----------------------------------------------------------*/
/* Private define -----------------------------------------------------------*/

/* Private macro ------------------------------------------------------------*/
/* Private function prototypes ----------------------------------------------*/
uint8_t *USBD_MSC_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MSC_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MSC_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MSC_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MSC_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MSC_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_MSC_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
#if (USBD_CLASS_USER_STRING_DESC == 1)
uint8_t *USBD_MSC_UserStrDescriptor(USBD_SpeedTypeDef speed, uint8_t idx, uint16_t *length);
#endif /* USBD_CLASS_USER_STRING_DESC */

/* Private variables --------------------------------------------------------*/
USBD_DescriptorsTypeDef USBD_MSC_Desc =
{
  USBD_MSC_DeviceDescriptor,
  USBD_MSC_LangIDStrDescriptor,
  USBD_MSC_ManufacturerStrDescriptor,
  USBD_MSC_ProductStrDescriptor,
  USBD_MSC_SerialStrDescriptor,
  USBD_MSC_ConfigStrDescriptor,
  USBD_MSC_InterfaceStrDescriptor,
#if (USBD_CLASS_USER_STRING_DESC == 1)
  USBD_MSC_UserStrDescriptor,
#endif /* USBD_CLASS_USER_STRING_DESC */
#if (USBD_CLASS_BOS_ENABLED == 1)
  NULL,  /* GetBOSDescriptor not implemented */
#endif /* USBD_CLASS_BOS_ENABLED */
};

__ALIGN_BEGIN uint8_t USBD_DeviceDesc_MSC[USB_SIZ_DEVICE_DESC] __ALIGN_END =
{
  0x12,                             /* bLength */
  USB_DEVICE_DESCRIPTOR_TYPE,       /* bDescriptorType */
  0x00,                             /* bcdUSB */
  0x02,
  0x00,                             /* bDeviceClass */
  0x00,                             /* bDeviceSubClass */
  0x00,                             /* bDeviceProtocol */
  USB_MAX_EP0_SIZE,                 /* bMaxPacketSize */
  LOBYTE(USBD_VID),                 /* idVendor */
  HIBYTE(USBD_VID),                 /* idVendor */
  LOBYTE(USBD_PID),                 /* idVendor */
  HIBYTE(USBD_PID),                 /* idVendor */
  0x00,                             /* bcdDevice rel. 2.00 */
  0x02,
  USBD_IDX_MFC_STR,                 /* Index of manufacturer string */
  USBD_IDX_PRODUCT_STR,             /* Index of product string */
  USBD_IDX_SERIAL_STR,              /* Index of serial number string */
  USBD_MAX_NUM_CONFIGURATION        /* bNumConfigurations */
}; /* USBD_DeviceDesc_MSC */

__ALIGN_BEGIN uint8_t USBD_LangIDDesc_MSC[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
  USB_LEN_LANGID_STR_DESC,
  USB_DESC_TYPE_STRING,
  LOBYTE(USBD_LANGID_STRING),
  HIBYTE(USBD_LANGID_STRING),
};

__ALIGN_BEGIN uint8_t USBD_StringSerial_MSC[USB_SIZ_STRING_SERIAL] =
{
  USB_SIZ_STRING_SERIAL,
  USB_DESC_TYPE_STRING,
};

__ALIGN_BEGIN uint8_t USBD_StrDesc_MSC[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

/* Private functions -------------------------------------------------------- */
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len);
static void Get_SerialNum(void);

/**
 * @brief  USBD_Class_DeviceDescriptor
 *         return the device descriptor
 * @param  speed: current device speed
 * @param  length: pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);

  *length = sizeof(USBD_DeviceDesc_MSC);
  return (uint8_t *)USBD_DeviceDesc_MSC;
}

/**
 * @brief  USBD_Class_LangIDStrDescriptor
 *         return the LangID string descriptor
 * @param  speed: current device speed
 * @param  length: pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);

  *length = sizeof(USBD_LangIDDesc_MSC);
  return (uint8_t *)USBD_LangIDDesc_MSC;
}

/**
 * @brief  USBD_Class_ProductStrDescriptor
 *         return the Product string descriptor
 * @param  speed: current device speed
 * @param  length: pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);

  USBD_GetString((uint8_t *)USBD_PRODUCT_FS_STRING, USBD_StrDesc_MSC, length);
  return USBD_StrDesc_MSC;
}

/**
 * @brief  USBD_Class_ManufacturerStrDescriptor
 *         return the Manufacturer string descriptor
 * @param  speed: current device speed
 * @param  length: pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);

  USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc_MSC, length);
  return USBD_StrDesc_MSC;
}

/**
 * @brief  USBD_Class_SerialStrDescriptor
 *         return the serial number string descriptor
 * @param  speed: current device speed
 * @param  length: pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = USB_SIZ_STRING_SERIAL;
  /* update the serial number string descriptor with the data from the unique ID */
  Get_SerialNum();

  return (uint8_t *)USBD_StringSerial_MSC;
}

/**
 * @brief  USBD_Class_ConfigStrDescriptor
 *         return the configuration string descriptor
 * @param  speed: current device speed
 * @param  length: pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);

  USBD_GetString((uint8_t *)USBD_CONFIGURATION_FS_STRING, USBD_StrDesc_MSC, length);

  return USBD_StrDesc_MSC;
}

/**
 * @brief  USBD_Class_InterfaceStrDescriptor
 *         return the Interface string descriptor
 * @param  speed: current device speed
 * @param  length: pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);

  USBD_GetString((uint8_t *)USBD_INTERFACE_FS_STRING, USBD_StrDesc_MSC, length);

  return USBD_StrDesc_MSC;
}

/**
 * @brief  Get_SerialNum
 *         Create the serial number string descriptor
 * @param  none
 * @retval none
 */
static void Get_SerialNum(void)
{
  uint32_t deviceserial0;
  uint32_t deviceserial1;
  uint32_t deviceserial2;

  deviceserial0 = *(uint32_t *)DEVICE_ID1;
  deviceserial1 = *(uint32_t *)DEVICE_ID2;
  deviceserial2 = *(uint32_t *)DEVICE_ID3;

  deviceserial0 += deviceserial2;

  if (deviceserial0 != 0U)
  {
    IntToUnicode(deviceserial0, &USBD_StringSerial_MSC[2], 8U);
    IntToUnicode(deviceserial1, &USBD_StringSerial_MSC[18], 4U);
  }
}

#if (USBD_CLASS_USER_STRING_DESC == 1)

/**
 * @brief  USBD_Class_UserStrDescriptor
 *         return the class user string descriptor
 * @param  speed: current device speed
 * @param  idx: index of string descriptor
 * @param  length: pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *USBD_MSC_UserStrDescriptor(USBD_SpeedTypeDef speed, uint8_t idx, uint16_t *length)
{
  UNUSED(speed);
  UNUSED(idx);
  UNUSED(length);
  static uint8_t USBD_UserStrDesc_MSC[255];

  return USBD_UserStrDesc_MSC;
}

#endif /* USBD_CLASS_USER_STRING_DESC */

/**
 * @brief  IntToUnicode
 *         Convert hex 32bits value into char
 * @param  value: value to convert
 * @param  pbuf:  pointer to the buffer
 * @parma  len:   buffer length
 * @retval None
 */
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
  uint8_t idx = 0U;
  for(idx = 0U; idx < len; idx++)
  {
    if (((value >> 28)) < 0xAU)
    {
      pbuf[2U * idx] = (value >> 28) + '0';
    }
    else
    {
      pbuf[2U * idx] = (value >> 28) + 'A' - 10U;
    }

    value = value << 4;
    pbuf[2U * idx + 1] = 0U;
  }
}



/**
 * @}
 */

extern USBD_DescriptorsTypeDef  USBD_MSC_Desc;
/**
 * @}
 */
#ifdef __cplusplus
}
#endif

/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/