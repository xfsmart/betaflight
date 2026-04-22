/**
  ******************************************************************************
  * @file    			usbd_desc.h
  * @author  			FMD XA
  * @brief   			Header file
  * @version 			V1.0.0           
  * @data		 			2026-03-27
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_DESC_H
#define __USBD_DESC_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_def.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants---------------------------------------------------------*/
#define DEVICE_ID1      0x1FFF0A00
#define DEVICE_ID2      0x1FFF0A04
#define DEVICE_ID3      0x1FFF0A08

/* USB Billboard Class USER string desc Defines Template index should start form 0x10 to
 * avoid using the reserved device string desc indexes
 * */
#if (USBD_CLASS_USER_STRING_DESC == 1)
#define USBD_BB_IF_STRING_INDEX         0x10U
#define USBD_BB_URL_STRING_INDEX        0x11U
#define USBD_BB_ALTMODE0_STRING_INDEX   0x12U
#define USBD_BB_ALTMODE1_STRING_INDEX   0x13U
#endif

#define USB_SIZ_STRING_SERIAL           0x1AU

#if (USBD_CLASS_BOS_ENABLED == 1)
#define USB_SIZ_BOS_DESC                0x5DU
#endif  /* USBD_CLASS_BOS_ENABLED */

/* Exported macro ------------------------------------------------------------*/
/* Exported functions---------------------------------------------------------*/

/*  extern USBD_DescriptorsTypeDef HID_Desc;  */

#endif /*__USBD_DESC_H*/


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
