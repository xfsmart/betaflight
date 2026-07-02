/**
  ******************************************************************************
  * @file    			usbd_conf.h
  * @author  			FMD XA
  * @brief   			Header file
  * @version 			V1.0.0           
  * @data		 			2026-03-27
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* memset, memcpy */

/* Define USB_OTG_FS for FT32 (required by driver headers) */
#ifndef USB_OTG_FS
#define USB_OTG_FS
#endif

/* Include PCD driver for type definitions */
#include <ft32f4xx_pcd_fs.h>

/* UNUSED macro for suppressing unused parameter warnings */
#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

/* FT32 only supports FS mode - alias types for SDK compatibility */
typedef PCD_FS_HandleTypeDef PCD_HS_HandleTypeDef;
typedef PCD_FS_HandleTypeDef PCD_HandleTypeDef;

/**@addtogroup FT32_USB_DEVICE_LIBRARY
 * @{
 */

/**@defgroup USBD_CONF
 * @brief usb device low level driver configuration file
 * @{
 */

/**@defgroup USBD_CONF_Exported_Defines
 * @{
 */

/* Number of supported interfaces. A composite device (e.g. CDC+HID) needs
 * more than one; keep the SDK default overridable by the platform/build. */
#ifndef USBD_MAX_NUM_INTERFACES
#define USBD_MAX_NUM_INTERFACES             1U
#endif /* USBD_MAX_NUM_INTERFACES */
#define USBD_MAX_NUM_CONFIGURATION          1U
#define USBD_MAX_STR_DESC_SIZ               0x100U
#define USBD_SELF_POWER                     1U
#define USBD_DEBUG_LEVEL                    0U


/* ECM, RNDIS, DFU, Class Config */
#define USBD_SUPPORT_USER_STRING_DESC       1U
/* billboard class config */
#define USBD_CLASS_USER_STRING_DESC         1U
#define USBD_CLASS_BOS_ENABLED              1U
#define USB_BB_MAX_NUM_ALT_MODE             0x2U

/* MSC class config */
#define MSC_MEDIA_PACKET                    8192U

/* CDC class config */
#define USBD_CDC_INTERVAL                   2000U

/* CDC macro aliases for SDK source file compatibility */
#define CDC_CMD_PACKET_SIZE                 CDC_CMD_PACKET_SZE
#define CDC_FS_BINTERVAL                    CDC_BINTERVAL
#define CDC_DATA_FS_MAX_PACKET_SIZE         CDC_DATA_MAX_PACKET_SIZE

/* DFU class config */
#define USBD_DFU_MAX_ITF_NUM                1U
#define USBD_DFU_XFERS_IZE                  1024U

/* AUDIO class config */
#define USBD_AUDIO_FREQ                     22100U

/* customHID class config */
#define CUSTOM_HID_HS_BINTERVAL             0x05U
#define CUSTOM_HID_FS_BINTERVAL             0x05U
#define USBD_CUSTOM_HID_OUTREPORT_BUF_SIZE  0x02U
#define USBD_CUSTOM_HID_REPORT_DESC_SIZE    163U

/* VIDEO class config */
#define UVC_1_1

/* to be used only with YUY2 and NV12 video format, shouldn't be defined for MJPEG format */
#define USBD_UVC_FORMAT_UNCOMPRESSED

#ifdef USBD_UVC_FORMAT_UNCOMPRESSED
#define UVC_BITS_PER_PIXEL                  12U
#define UVC_UNCOMPRESSED_GUID               UVC_GUID_NV12

/* refer to table 3-18 color matching descriptor video class v1.1*/
#define UVC_COLOR_PRIMARIE                  0x01U
#define UVC_TFR_CHARACTERISTICS             0x01U
#define UVC_MATRIX_COEFFICIENTS             0x04U
#endif  /* USBD_UVC_FORMAT_UNCOMPRESSED */

/* video stream frame width and height */
#define UVC_WIDTH                           176U
#define UVC_HEIGHT                          144U

/* bEndpointAddress in Endpoint Descriptor */
#define UVC_IN_EP                           0x81U

#define UVC_CAM_FPS_FS                      10U
#define UVC_CAM_FPS_HS                      5U

#define UVC_ISO_FS_MPS                      512U
#define UVC_ISO_HS_MPS                      512U

#define UVC_PACKET_SIZE                     UVC_ISO_FS_MPS

#define UVC_MAX_PACKET_SIZE                 (UVC_WIDTH * UVC_HEIGHT * 16U / 8U)

/**
 * @}
 */

/**@defgroup USBD_Exported_Macros
 * @{
 */
/* Memory management macros make sure ro use static memory allocation */
#define USBD_malloc         malloc
#define USBD_free           free
#define USBD_memset         memset
#define USBD_memcpy         memcpy

/* wait for update */
#ifdef USB_OTG_HS_CORE
  #define USBD_Delay          USB_HS_Delayms
#endif

#ifdef USB_OTG_FS_CORE
  #define USBD_Delay          USB_FS_Delayms
#endif

/* DEBUG macros */
#if (USBD_DEBUG_LEVEL > 0U)
#define USBD_UsrLog(...)  do { \
                               printf(__VA_ARGS__); \
                               printf("\n"); \
                             } while(0)
#else
#define USBD_UsrLog(...)  do {} while(0)
#endif

#if (USBD_DEBUG_LEVEL > 1U)
#define USBD_ErrLog(...)  do { \
                               printf("ERROR: "); \
                               printf(__VA_ARGS__); \
                               printf("\n"); \
                             } while(0)
#else
#define USBD_ErrLog(...)  do {} while(0)
#endif

#if (USBD_DEBUG_LEVEL > 2U)
#define USBD_DbgLog(...)  do { \
                               printf("DEBUG: "); \
                               printf(__VA_ARGS__); \
                               printf("\n"); \
                             } while(0)
#else
#define USBD_DbgLog(...)  do {} while(0)
#endif


/**
 * @}
 */

/**@defgroup USBD_CONF_Exported_Types
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_CONF_Exported_Macros
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_CONF_Exported_Variables
 * @{
 */

/**
 * @}
 */


/**
 * @}
 */

#ifdef __cplusplus
}
#endif


#endif /*__USBD_CONF_TEMPLATE_H*/


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
