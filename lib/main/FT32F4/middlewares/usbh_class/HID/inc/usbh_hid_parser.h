/**
  ******************************************************************************
  * @file    			usbh_hid_parser.h
  * @author  			FMD XA
  * @brief   			This file is the header file of the usbh_hid_parser.c
  * @version 			V1.0.0           
  * @data		 			2025-04-15
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBH_HID_PARSER_H
#define __USBH_HID_PARSER_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbh_hid.h"
#include "usbh_hid_usage.h"

/**@addtogroup USBH_LIB
 * @{
 */

/**@addtogroup USBH_CLASS
 * @{
 */

/**@addtogroup USBH_HID_CLASS
 * @{
 */

/**@defgroup USBH_HID_PARSER
 * @brief This file is the Header file for usbh_hid_parser.c
 * @{
 */

/**@defgroup USBH_HID_PARSER_Exported_Types
 * @{
 */
typedef struct
{
  uint8_t   *data;
  uint32_t  size;
  uint8_t   shift;
  uint8_t   count;
  uint8_t   sign;
  uint32_t  logical_min;    /* min value device can return */
  uint32_t  logical_max;    /* max value device can return */
  uint32_t  physical_min;   /* min value read can report */
  uint32_t  physical_max;   /* max value read can report */
  uint32_t  resolution;
}
HID_Report_ItemTypeDef;

uint32_t HID_ReadItem(HID_Report_ItemTypeDef *ri, uint8_t ndx);
uint32_t HID_WriteItem(HID_Report_ItemTypeDef *ri, uint32_t value, uint8_t ndx);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /*__USBH_HID_PARSER_H*/
/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
