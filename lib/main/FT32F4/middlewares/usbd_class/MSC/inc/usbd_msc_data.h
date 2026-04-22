/**
  ******************************************************************************
  * @file    			usbd_msc_data.h
  * @author  			FMD XA
  * @brief   			header file for the usbd_msc_data.c
  * @version 			V1.0.0           
  * @data		 			2025-04-28
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_MSC_DATA_H
#define __USBD_MSC_DATA_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_def.h"

/**@addtogroup USBD_LIB
 * @{
 */

/**@addtogroup USBD_CLASS
 * @{
 */

/**@addtogroup USBD_MSC_CLASS
 * @{
 */

/**@defgroup USBD_MSC_DATA
 * @brief This file is Header file for usbd_msc_data.c
 * @{
 */


/**@defgroup USBD_MSC_DATA_Exported_Defines
 * @{
 */

#define MODE_SENSE6_LEN                     0x04U
#define MODE_SENSE10_LEN                    0x08U
#define LENGTH_INQUIRY_PAGE00               0x06U
#define LENGTH_INQUIRY_PAGE80               0x08U
#define LENGTH_FORMAT_CAPACITIES            0x14U

/**
 * @}
 */

/**@defgroup USBD_MSC_DATA_Exported_TypesDefinitions
 * @{
 */

/**
 * @}
 */


/**@defgroup USBD_MSC_DATA_Exported_Macros
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MSC_DATA_Exported_Variables
 * @{
 */
extern uint8_t MSC_Page00_Inquiry_Data[LENGTH_INQUIRY_PAGE00];
extern uint8_t MSC_Page80_Inquiry_Data[LENGTH_INQUIRY_PAGE80];
extern uint8_t MSC_Mode_Sense6_data[MODE_SENSE6_LEN];
extern uint8_t MSC_Mode_Sense10_data[MODE_SENSE10_LEN];


/**
 * @}
 */

/**@defgroup USBD_MSC_DATA_Exported_FunctionsPrototype
 * @{
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /*__USBD_MSC_DATA_H*/
/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
