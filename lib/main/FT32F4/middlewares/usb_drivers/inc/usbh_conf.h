/**
  ******************************************************************************
  * @file    			usbh_conf.h
  * @author  			FMD XA
  * @brief   			Header file
  * @version 			V1.0.0           
  * @data		 			2025-04-10
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBH_CONF_H
#define __USBH_CONF_H

#ifdef  __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* stdio.h/stdlib.h only needed when USBH_DEBUG_LEVEL > 0 for printf/malloc */
#if (USBH_DEBUG_LEVEL > 0U)
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
/**@addtogroup FT32_USB_HOST_LIBRARY
 * @{
 */

/**@defgroup USBH_CONF
 * @brief usb host low level driver configuration file
 * @{
 */

/**@defgroup USBH_CONF_Exported_Defines
 * @{
 */


#define USBH_MAX_NUM_ENDPOINTS              2U
#define USBH_MAX_NUM_INTERFACES             2U
#define USBH_MAX_NUM_CONFIGURATION          1U
#define USBH_KEEP_CFG_DESCRIPTOR            1U
#define USBH_MAX_NUM_SUPPORTED_CLASS        1U
#define USBH_MAX_SIZE_CONFIGURATION         0x200U
#define USBH_MAX_DATA_BUFFER                0x200U
#define USBH_DEBUG_LEVEL                    0U
#define USBH_IN_NAK_PPOCESS                 0

/**
 * @}
 */

/**@defgroup USBH_Exported_Macros
 * @{
 */
/* Memory management macros */
#define USBH_malloc         malloc
#define USBH_free           free
#define USBH_memset         memset
#define USBH_memcpy         memcpy


/* DEBUG macros */
#if (USBH_DEBUG_LEVEL > 0U)
#define USBH_UsrLog(...)  do { \
                               printf(__VA_ARGS__); \
                               printf("\n"); \
                             } while(0)
#else
#define USBH_UsrLog(...)  do {} while(0)
#endif

#if (USBH_DEBUG_LEVEL > 1U)
#define USBH_ErrLog(...)  do { \
                               printf("ERROR: "); \
                               printf(__VA_ARGS__); \
                               printf("\n"); \
                             } while(0)
#else
#define USBH_ErrLog(...)  do {} while(0)
#endif

#if (USBH_DEBUG_LEVEL > 2U)
#define USBH_DbgLog(...)  do { \
                               printf("DEBUG: "); \
                               printf(__VA_ARGS__); \
                               printf("\n"); \
                             } while(0)
#else
#define USBH_DbgLog(...)  do {} while(0)
#endif

/**
 * @}
 */

/**@defgroup USBH_CONF_Exported_Types
 * @{
 */

/**
 * @}
 */

/**@defgroup USBH_CONF_Exported_Macros
 * @{
 */

/**
 * @}
 */

/**@defgroup USBH_CONF_Exported_Variables
 * @{
 */

/**
 * @}
 */

/**@defgroup USBH_CONF_Exported_FunctionsPrototype
 * @{
 */
/**
 * @}
 */

#ifdef __cplusplus
}
#endif


#endif /*__USBH_CONF_TEMPLATE_H*/


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
