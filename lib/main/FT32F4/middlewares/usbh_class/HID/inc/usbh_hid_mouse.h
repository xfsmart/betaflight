/**
  ******************************************************************************
  * @file    			usbh_hid_mouse.h
  * @author  			FMD XA
  * @brief   			This file contains all the protocoltypes for the usbh_hid_mouse.c
  * @version 			V1.0.0           
  * @data		 			2025-04-15
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBH_HID_MOUSE_H
#define __USBH_HID_MOUSE_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbh_hid.h"

/**@addtogroup USBH_LIB
 * @{
 */

/**@addtogroup USBH_CLASS
 * @{
 */

/**@addtogroup USBH_HID_CLASS
 * @{
 */

/**@defgroup USBH_HID_MOUSE
 * @brief This file is the Header file for usbh_hid_mouse.c
 * @{
 */

/**@defgroup USBH_HID_MOUSE_Exported_Types
 * @{
 */
typedef struct _HID_MOUSE_Info
{
  uint8_t   x;
  uint8_t   y;
  uint8_t   buttons[3];
}
HID_MOUSE_Info_TypeDef;

/**
 * @}
 */

/**@defgroup USBH_HID_MOUSE_Exported_defines
 * @{
 */
#ifndef USBH_HID_MOUSE_REPORT_SIZE
#define USBH_HID_MOUSE_REPORT_SIZE          0x8U
#endif  /* USBH_HID_MOUSE_REPORT_SIZE */


/**
 * @}
 */

/**@defgroup USBH_HID_MOUSE_Exported_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_HID_MOUSE_Exported_Valiables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_HID_MOUSE_Exported_FunctionsPrototype
 * @{
 */
USBH_StatusTypeDef USBH_HID_MouseInit(USBH_HandleTypeDef *phost);
HID_MOUSE_Info_TypeDef *USBH_HID_GetMouseInfo(USBH_HandleTypeDef *phost);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /*__USBH_HID_MOUSE_H*/
/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
