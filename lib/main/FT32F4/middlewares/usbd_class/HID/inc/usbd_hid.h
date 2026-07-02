/**
  ******************************************************************************
  * @file    			usbd_hid.h
  * @author  			FMD XA
  * @brief   			This file for the usbd_hid.c
  * @version 			V1.0.0           
  * @data		 			2025-04-15
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_HID_H
#define __USBD_HID_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_ioreq.h"
#include "usbd_def.h"
/**@defgroup USBD_HID
 * @brief This file is Header file for usbd_hid.c
 * @{
 */

/**@defgroup USBD_HID_CORE_Exported_Defines
 * @{
 */
#ifndef HID_EPIN_ADDR
#define HID_EPIN_ADDR                             0x81U
#endif /* HID_EPIN_ADDR */
#define HID_EPIN_SIZE                             9U

#define USB_HID_CONFIG_DESC_SIZ                   34U
#define USB_HID_DESC_SIZ                          9U
#define HID_MOUSE_REPORT_DESC_SIZE                54U

#define HID_DESCRIPTOR_TYPE                       0x21U
#define HID_REPORT_DESC                           0x22U

#ifndef HID_HS_BINTERVAL
#define HID_HS_BINTERVAL                          0x07U
#endif /* HID_HS_BINTERVAL */

#ifndef HID_FS_BINTERVAL
#define HID_FS_BINTERVAL                          0x0AU
#endif /* HID_FS_BINTERVAL */

#define USBD_HID_REQ_SET_PROTOCOL                 0x0BU
#define USBD_HID_REQ_GET_PROTOCOL                 0x03U

#define USBD_HID_REQ_SET_IDLE                     0x0AU
#define USBD_HID_REQ_GET_IDLE                     0x02U

#define USBD_HID_REQ_SET_REPORT                   0x09U
#define USBD_HID_REQ_GET_REPORT                   0x01U

/**
 * @}
 */

/**@defgroup USBD_HID_CORE_Exported_TypeDefinitions
 * @{
 */
typedef enum
{
  USBD_HID_IDLE = 0,
  USBD_HID_BUSY,
} USBD_HID_StateTypeDef;

typedef struct
{
  uint32_t Protocol;
  uint32_t IdleState;
  uint32_t AltSetting;
  USBD_HID_StateTypeDef state;
} USBD_HID_HandleTypeDef;

/* HID class specification version 1.1
 * 6.2.1 HID Descriptor */
typedef struct
{
  uint8_t   blength;
  uint8_t   bDescriptorType;
  uint16_t  bcdHID;
  uint8_t   bCountryCode;
  uint8_t   bNumDescriptors;
  uint8_t   bHIDDescriptorType;
  uint16_t  wItemLength;
} __attribute__((packed)) USBD_HIDDescTypeDef;

/**
 * @}
 */

/**@defgroup USBD_HID_CORE_Exported_Macros
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_HID_CORE_Exported_Variables
 * @{
 */
extern USBD_ClassTypeDef USBD_HID;
#define USBD_HID_CLASS &USBD_HID

/**
 * @}
 */

/**@defgroup USBD_HID_CORE_Exported_Functions
 * @{
 */
#ifdef USE_USBD_COMPOSITE
uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len, uint8_t ClassId);
#else
uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len);
#endif  /* USE_USBD_COMPOSITE */

uint32_t USBD_HID_GetPollingInterval(USBD_HandleTypeDef *pdev);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /*__USBD_HID_H*/
/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
