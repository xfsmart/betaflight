/**
  ******************************************************************************
  * @file    usbd_cdc_desc.c
  * @author  FMD
  * @version V1.0.0
  * @date    2026-03-31
  * @brief   This file provides the USBD descriptors and string formating method.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usb_conf.h"
#include "usbd_def.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"
#include "usbd_conf.h"
#include "platform.h"
#include "build/version.h"

#include "pg/pg.h"
#include "pg/usb.h"

/** @addtogroup FT32_USB_OTG_DEVICE_LIBRARY
  * @{
  */

/** @defgroup USBD_CDC_DESC
  * @brief USBD descriptors module
  * @{
  */

/** @defgroup USBD_CDC_DESC_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */

/** @defgroup USBD_DESC_Private_Defines
  * @{
  */

#define USBD_VID                        0x0483
#define USBD_PID                        0x5740

/** @defgroup USB_String_Descriptors
  * @{
  */
#define USBD_LANGID_STRING              0x409
#define USBD_MANUFACTURER_STRING        FC_FIRMWARE_NAME

#ifdef USBD_PRODUCT_STRING
#define USBD_PRODUCT_HS_STRING          USBD_PRODUCT_STRING
#define USBD_PRODUCT_FS_STRING          USBD_PRODUCT_STRING
#else
#define USBD_PRODUCT_HS_STRING          "Betaflight Virtual ComPort in HS mode"
#define USBD_PRODUCT_FS_STRING          "Betaflight Virtual ComPort in FS Mode"
#endif /* USBD_PRODUCT_STRING */

#define USBD_CONFIGURATION_HS_STRING    "VCP Config"
#define USBD_INTERFACE_HS_STRING        "VCP Interface"

#define USBD_CONFIGURATION_FS_STRING    "VCP Config"
#define USBD_INTERFACE_FS_STRING        "VCP Interface"
/**
  * @}
  */

/** @defgroup USBD_DESC_Private_Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup USBD_DESC_Private_Variables
  * @{
  */
/* Private macro -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
uint8_t *USBD_CDC_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_CDC_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_CDC_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_CDC_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_CDC_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_CDC_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *USBD_CDC_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef USBD_CDC_Desc =
{
  USBD_CDC_DeviceDescriptor,
  USBD_CDC_LangIDStrDescriptor,
  USBD_CDC_ManufacturerStrDescriptor,
  USBD_CDC_ProductStrDescriptor,
  USBD_CDC_SerialStrDescriptor,
  USBD_CDC_ConfigStrDescriptor,
  USBD_CDC_InterfaceStrDescriptor,
  NULL,  /* GetUserStrDescriptor */
  NULL,  /* GetBOSDescriptor */
};

#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
  #if defined( __ICCARM__ ) /*!< IAR Compiler */
    #pragma data_alignment=4
  #endif
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */

/* USB Standard Device Descriptor */
__ALIGN_BEGIN uint8_t USBD_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END =
  {
    0x12,                                  /*bLength */
    USB_DESC_TYPE_DEVICE,                  /*bDescriptorType*/
    0x00, 
    0x02,                                  /*bcdUSB */
    0x02,                                  /*bDeviceClass*/
    0x00,                                  /*bDeviceSubClass*/
    0x00,                                  /*bDeviceProtocol*/
    USB_MAX_EP0_SIZE,                      /*bMaxPacketSize*/
    LOBYTE(USBD_VID), HIBYTE(USBD_VID),    /*idVendor*/
    LOBYTE(USBD_PID), HIBYTE(USBD_PID),    /*idProduct*/
    0x00,
    0x02,                                  /*bcdDevice rel. 2.00*/
    USBD_IDX_MFC_STR,                      /*Index of manufacturer  string*/
    USBD_IDX_PRODUCT_STR,                  /*Index of product string*/
    USBD_IDX_SERIAL_STR,                   /*Index of serial number string*/
    USBD_MAX_NUM_CONFIGURATION             /*bNumConfigurations*/
  } ; /* USB_DeviceDescriptor */

#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
  #if defined( __ICCARM__ ) /*!< IAR Compiler */
    #pragma data_alignment=4
  #endif
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */
/* USB Standard Device Descriptor */
__ALIGN_BEGIN uint8_t USBD_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
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

#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
  #if defined( __ICCARM__ ) /*!< IAR Compiler */
    #pragma data_alignment=4
  #endif
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */
/* USB Standard Device Descriptor */
__ALIGN_BEGIN uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
     USB_LEN_LANGID_STR_DESC,
     USB_DESC_TYPE_STRING,
     LOBYTE(USBD_LANGID_STRING),
     HIBYTE(USBD_LANGID_STRING),
};


uint8_t USBD_StringSerial[USB_SIZ_STRING_SERIAL] =
{
  USB_SIZ_STRING_SERIAL,
  USB_DESC_TYPE_STRING,
};


__ALIGN_BEGIN uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

/* Private functions ---------------------------------------------------------*/
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len);
static void Get_SerialNum(void);

/** @defgroup USBD_DESC_Private_Functions
  * @{
  */

/**
* @brief  USBD_CDC_DeviceDescriptor
*         return the device descriptor
* @param  speed : current device speed
* @param  length : pointer to data length variable
* @retval pointer to descriptor buffer
*/
uint8_t *  USBD_CDC_DeviceDescriptor( USBD_SpeedTypeDef speed , uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_DeviceDesc);
    return USBD_DeviceDesc;
}

/**
* @brief  USBD_CDC_LangIDStrDescriptor
*         return the LangID string descriptor
* @param  speed : current device speed
* @param  length : pointer to data length variable
* @retval pointer to descriptor buffer
*/
uint8_t *  USBD_CDC_LangIDStrDescriptor( USBD_SpeedTypeDef speed , uint16_t *length)
{
    (void)speed;
  *length =  sizeof(USBD_LangIDDesc);
  return USBD_LangIDDesc;
}

/**
* @brief  USBD_CDC_ProductStrDescriptor
*         return the product string descriptor
* @param  speed : current device speed
* @param  length : pointer to data length variable
* @retval pointer to descriptor buffer
*/
uint8_t *  USBD_CDC_ProductStrDescriptor( USBD_SpeedTypeDef speed , uint16_t *length)
{

  if (speed == USBD_SPEED_HIGH)
    USBD_GetString ((uint8_t*)USBD_PRODUCT_HS_STRING, USBD_StrDesc, length);
  else
    USBD_GetString ((uint8_t*)USBD_PRODUCT_FS_STRING, USBD_StrDesc, length);

  return USBD_StrDesc;
}

/**
* @brief  USBD_CDC_ManufacturerStrDescriptor
*         return the manufacturer string descriptor
* @param  speed : current device speed
* @param  length : pointer to data length variable
* @retval pointer to descriptor buffer
*/
uint8_t *  USBD_CDC_ManufacturerStrDescriptor( USBD_SpeedTypeDef speed , uint16_t *length)
{
    (void)speed;
  USBD_GetString ((uint8_t*)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
  return USBD_StrDesc;
}

/**
* @brief  USBD_CDC_SerialStrDescriptor
*         return the serial number string descriptor
* @param  speed : current device speed
* @param  length : pointer to data length variable
* @retval pointer to descriptor buffer
*/
uint8_t *  USBD_CDC_SerialStrDescriptor( USBD_SpeedTypeDef speed , uint16_t *length)
{
  (void)speed;
  *length = USB_SIZ_STRING_SERIAL;
  /* Update the serial number string descriptor with the data from the unique ID*/
  Get_SerialNum();

  return (uint8_t*)USBD_StringSerial;
}

/**
* @brief  USBD_CDC_ConfigStrDescriptor
*         return the configuration string descriptor
* @param  speed : current device speed
* @param  length : pointer to data length variable
* @retval pointer to descriptor buffer
*/
uint8_t *  USBD_CDC_ConfigStrDescriptor( USBD_SpeedTypeDef speed , uint16_t *length)
{
  if (speed  == USBD_SPEED_HIGH)
    USBD_GetString ((uint8_t*)USBD_CONFIGURATION_HS_STRING, USBD_StrDesc, length);
  else
    USBD_GetString ((uint8_t*)USBD_CONFIGURATION_FS_STRING, USBD_StrDesc, length);

  return USBD_StrDesc;
}

/**
* @brief  USBD_CDC_InterfaceStrDescriptor
*         return the interface string descriptor
* @param  speed : current device speed
* @param  length : pointer to data length variable
* @retval pointer to descriptor buffer
*/
uint8_t *  USBD_CDC_InterfaceStrDescriptor( USBD_SpeedTypeDef speed , uint16_t *length)
{
  if (speed == USBD_SPEED_HIGH)
    USBD_GetString ((uint8_t*)USBD_INTERFACE_HS_STRING, USBD_StrDesc, length);
  else
    USBD_GetString ((uint8_t*)USBD_INTERFACE_FS_STRING, USBD_StrDesc, length);

  return USBD_StrDesc;
}

/**
  * @brief  Create the serial number string descriptor
  * @param  None
  * @retval None
  */
static void Get_SerialNum(void)
{
  uint32_t deviceserial0, deviceserial1, deviceserial2;

  deviceserial0 = U_ID_0;
  deviceserial1 = U_ID_1;
  deviceserial2 = U_ID_2;

  deviceserial0 += deviceserial2;

  if (deviceserial0 != 0)
  {
    IntToUnicode (deviceserial0, &USBD_StringSerial[2] ,8);
    IntToUnicode (deviceserial1, &USBD_StringSerial[18] ,4);
  }
}

/**
  * @brief  Convert Hex 32Bits value into char
  * @param  value: value to convert
  * @param  pbuf: pointer to the buffer
  * @param  len: buffer length
  * @retval None
  */
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len)
{
  uint8_t idx = 0;

  for ( idx = 0; idx < len; idx ++)
  {
    if ( ((value >> 28)) < 0xA )
    {
      pbuf[ 2* idx] = (value >> 28) + '0';
    }
    else
    {
      pbuf[2* idx] = (value >> 28) + 'A' - 10;
    }

    value = value << 4;

    pbuf[ 2* idx + 1] = 0;
  }
}

/******************* (C) COPYRIGHT FMD *****END OF FILE****/