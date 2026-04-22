/**
  ******************************************************************************
  * @file    			usb_conf.h
  * @author  			FMD XA
  * @brief   			This file is header file for USB_HS or USB_FS
  * @version 			V1.0.0           
  * @data		 			2026-03-27
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_CONF_H
#define __USB_CONF_H

#ifdef  __cplusplus
extern "c" {
#endif

#include <stdint.h>
#define     __IO    volatile             /*!< Defines 'read / write' permissions */
#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif
/**
  * @brief enable usb device mode
  */
#define USE_OTG_DEVICE_MODE

/**
  * @brief enable usb host mode
  */
/* #define USE_OTG_HOST_MODE */


/**
  * @brief select otghs or otgfs define
  */

#define  USB_OTG_HS_CORE_ID  0
#define  USB_OTG_FS_CORE_ID  1

/* use otgfs */
#define OTG_USB_ID                       1

#if (OTG_USB_ID == USB_OTG_HS_CORE_ID)

#define  USB_OTG_HS

#ifdef USE_OTG_DEVICE_MODE
  #define PCD_HS_MODULE_ENABLED
#endif
#ifdef USE_OTG_DEVICE_MODE
  #define HCD_HS_MODULE_ENABLED
#endif

#include "ft32f4xx_usb_hs.h"
#include "ft32f4xx_pcd_hs.h"
#include "ft32f4xx_pcd_ex_hs.h"
#include "ft32f4xx_hcd_hs.h"

#define USB_OTG_HS_CORE
#define OTG_IRQ                          OTG_HS_IRQn
#define OTG_IRQ_HANDLER                  OTG_HS_Handler
#define PCD_IRQHandler                   PCD_HS_IRQHandler
#define HCD_IRQHandler                   HCD_HS_IRQHandler
#define OTG_WKUP_IRQ                     OTG_HS_WKUP_IRQn
#define OTG_WKUP_HANDLER                 OTG_HS_WKUP_IRQHandler
#define OTG_WKUP_EXINT_LINE              EXINT_LINE_20


#endif

#if (OTG_USB_ID == USB_OTG_FS_CORE_ID)

#ifndef USB_OTG_FS
#define USB_OTG_FS
#endif

#ifdef USE_OTG_DEVICE_MODE
  #ifndef PCD_FS_MODULE_ENABLED
  #define PCD_FS_MODULE_ENABLED
  #endif
#endif
#ifdef USE_OTG_DEVICE_MODE
  #ifndef HCD_FS_MODULE_ENABLED
  #define HCD_FS_MODULE_ENABLED
  #endif
#endif

#include "ft32f4xx_usb_fs.h"
#include "ft32f4xx_pcd_fs.h"
#include "ft32f4xx_hcd_fs.h"

#ifndef USB_OTG_FS_CORE
#define USB_OTG_FS_CORE
#endif

#define OTG_IRQ                          OTG_FS_IRQn
#define OTG_IRQ_HANDLER                  OTG_FS_Handler
#define PCD_IRQHandler                   PCD_FS_IRQHandler
#define HCD_IRQHandler                   HCD_FS_IRQHandler
#define OTG_WKUP_IRQ                     OTG_FS_WKUP_IRQn
#define OTG_WKUP_HANDLER                 OTG_FS_WKUP_IRQHandler
#define OTG_WKUP_EXINT_LINE              EXINT_LINE_18

#endif


/* __ALIGN_BEGIN/__ALIGN_END defined in usbd_def.h (FT32 USB middleware) */


#endif
/************************ (C) COPYRIGHT FMD *****END OF FILE****/
