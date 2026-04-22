/**
  ******************************************************************************
  * @file    			usbd_mtp_if_template.h
  * @author  			FMD XA
  * @brief   			Header file for the usbd_mtp_if_template.c file
  * @version 			V1.0.0           
  * @date		 			2025-05-12
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_MTP_IF_TEMPLATE_H
#define __USBD_MTP_IF_TEMPLATE_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_mtp.h"

/* Exported Define -----------------------------------------------------------*/
#define USBD_MTP_DEVICE_PROP_SUPPORTED                  1U
#define USBD_MTP_CAPTURE_FORMAT_SUPPORTED               1U
#define USBD_MTP_VEND_EXT_DESC_SUPPORTED                1U
#define USBD_MTP_EVENTS_SUPPORTED                       1U

#if USBD_MTP_EVENTS_SUPPORTED == 1
#define SUPP_EVENTS_LEN                                 (uint8_t)((uint8_t)sizeof(SuppEvents) / 2U)
#else
#define SUPP_EVENTS_LEN                                 0U
#endif /* USBD_MTP_EVENTS_SUPPORTED */

#if USBD_MTP_VEND_EXT_DESC_SUPPORTED == 1
#define VEND_EXT_DESC_LEN                               (sizeof(VendExtDesc) / 2U)
#else
#define VEND_EXT_DESC_LEN                               0U
#endif /* USBD_MTP_VEND_EXT_DESC_SUPPORTED */

#if USBD_MTP_CAPTURE_FORMAT_SUPPORTED == 1
#define SUPP_CAPT_FORMAT_LEN                            (uint8_t)((uint8_t)sizeof(SuppCaptFormat) / 2U)
#else
#define SUPP_CAPT_FORMAT_LEN                            0U
#endif /* USBD_MTP_CAPTURE_FORMAT_SUPPORTED */

#if USBD_MTP_DEVICE_PROP_SUPPORTED == 1
#define SUPP_DEVICE_PROP_LEN                            (uint8_t)((uint8_t)sizeof(DevicePropSupp) / 2U)
#else
#define SUPP_DEVICE_PROP_LEN                            0U
#endif /* USBD_MTP_DEVICE_PROP_SUPPORTED */

#define MTP_IF_SCRATCH_BUFF_SZE                         1024U

/* Exported types ------------------------------------------------------------*/
extern USBD_MTP_ItfTypeDef USBD_MTP_fops;

/* Exported macros -----------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

static const uint16_t Manuf[] = {'F', 'M', 'D', 0}; /* last 2 bytes must be 0*/
static const uint16_t Model[] = {'F', 'M', 'D', '3', '2', 0}; /* last 2 bytes must be 0*/
static const uint16_t VendExtDesc[] = {'m', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', '.',
                                       'c', 'o', 'm', ':', ' ', '1', '.', '0', ';', ' ',
                                       0}; /* last 2 bytes must be 0*/
/* serialNbr shall be 32 character hexadecimal string for legacy compatiblity reasons */
static const uint16_t SerialNbr[] = {'0', '0', '0', '0', '1', '0', '0', '0', '0', '1',
                                     '0', '0', '0', '0', '1', '0', '0', '0', '0', '1',
                                     '0', '0', '0', '0', '1', '0', '0', '0', '0', '1',
                                     '0', '0', 0}; /* last 2 bytes must be 0*/

static const uint16_t DeviceVers[] = {'V', '1', '.', '0', '0', 0}; /* last 2 bytes must be 0*/

static const uint16_t DefaultFileName[] = {'N', 'e', 'w', ' ', 'F', 'o', 'l', 'd', 'e', 'r', 0}; /* last 2 bytes must be 0*/

static const uint16_t DevicePropDefVal[] = {'F', 'M', 'D', '3', '2', 0}; /* last 2 bytes must be 0*/

static const uint16_t DevicePropCurDefVal[] = {'F', 'M', 'D', '3', '2', ' ', 'V', '1', '.', '0', 0}; /* last 2 bytes must be 0*/

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /*__USBD_MTP_IF_TEMPLATE_H*/
/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
