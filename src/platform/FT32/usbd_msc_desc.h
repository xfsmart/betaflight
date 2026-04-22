/**
  ******************************************************************************
  * @file    			usbd_msc_desc.h
  * @author  			FMD XA
  * @brief   			This file contains all the prototypes for the usbd_msc_desc.c
  * @version 			V1.0.0           
  * @data		 			2025-03-26
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_MSC_DESC_H
#define __USBD_MSC_DESC_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/


#include "usbd_ioreq.h"
#include "usbd_ctlreq.h"

/**@defgroup USBD_MSC_CORE
 * @brief This file is Header file for usbd_msc.c
 * @{
 */


/**@defgroup USBD_MSC_CORE_Exported_Defines
 * @{
 */

/* MSC Class config */
#ifndef MSC_MEDIA_PACKET
#define MSC_MEDIA_PACKET        512U
#endif  /* MSC_MEDIA_PACKET */

#define MSC_MAX_FS_PACKET       0x40U
#define MSC_MAX_HS_PACKET       0x200U

#define BOT_GET_MAX_LUN         0xFE
#define BOT_RESET               0xFF
#define USB_MSC_CONFIG_DESC_SIZ 32

#ifndef MSC_EPIN_ADDR
#define MSC_EPIN_ADDR           0x81U
#endif  /* MSC_EPIN_ADDR */

#ifndef MSC_EPOUT_ADDR
#define MSC_EPOUT_ADDR          0x01U
#endif  /* MSC_EPOUT_ADDR */


#define USB_DEVICE_DESCRIPTOR_TYPE              0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE       0x02
#define USB_STRING_DESCRIPTOR_TYPE              0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE           0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE            0x05
#define USB_SIZ_DEVICE_DESC                     18




/* DEVICE_ID1/2/3 are defined in usbd_desc.h (FT32 SDK: 0x1FFF0A00/0A04/0A08) */
/**
 * @}
 */
#define USBD_VID                                                   0x34D3
#define USBD_PID                                                   0x0002
#define USBD_LANGID_STRING                                         0x409
#define USBD_MSC_SIZ_STRING_LANGID                                 4
#define USBD_MSC_SIZ_STRING_SERIAL                                 0x1A
#define USBD_MANUFACTURER_STRING                                   "FMD"
#define USBD_PRODUCT_FS_STRING                                     "Betaflight FC Mass Storage (FS Mode)"
#define USBD_CONFIGURATION_FS_STRING                               "MSC Config"
#define USBD_INTERFACE_FS_STRING                                   "MSC Interface"

/**
 * @brief  MSC descriptor handle
 */
extern USBD_DescriptorsTypeDef USBD_MSC_Desc;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /*__USBD_MSC_H*/
/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
