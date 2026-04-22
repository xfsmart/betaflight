/**
  ******************************************************************************
  * @file    			usbd_mtp_storage.h
  * @author  			FMD XA
  * @brief   			Header file for the usbd_mtp_storage.c file
  * @version 			V1.0.0           
  * @date		 			2025-05-12
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_MTP_STORAGE_H
#define __USBD_MTP_STORAGE_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_ctlreq.h"
#include "usbd_mtp_opt.h"

/**@addtogroup USBD_LIB
 * @{
 */

/**@addtogroup USBD_CLASS
 * @{
 */

/**@addtogroup USBD_MTP_CLASS
 * @{
 */

/**@defgroup USBD_MTP_STORAGE
 * @brief This file is Header file for usbd_mtp_storage.c
 * @{
 */

/**@defgroup USBD_MTP_STORAGE_Exported_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MTP_STORAGE_Exported_Macros
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MTP_STORAGE_Exported_TypesDefinitions
 * @{
 */

typedef enum
{
  DATA_TYPE = 0x00,
  REP_TYPE  = 0x01,
} MTP_CONTAINER_TYPE;

typedef enum
{
  READ_FIRST_DATA = 0x00,
  READ_REST_OF_DATA = 0x01,
} MTP_READ_DATA_STATUS;

/**
 * @}
 */

/**@defgroup USBD_MTP_STORAGE_Exported_Variables
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MTP_STORAGE_Exported_Functions
 * @{
 */
uint8_t USBD_MTP_STORAGE_Init(USBD_HandleTypeDef *pdev);
uint8_t USBD_MTP_STORAGE_DeInit(USBD_HandleTypeDef *pdev);
void USBD_MTP_STORAGE_Cancel(USBD_HandleTypeDef *pdev, MTP_ResponsePhaseTypeDef MTP_ResponsePhase);
uint8_t USBD_MTP_STORAGE_ReadData(USBD_HandleTypeDef *pdev);
uint8_t USBD_MTP_STORAGE_SendContainer(USBD_HandleTypeDef *pdev, MTP_CONTAINER_TYPE CONT_TYPE);
uint8_t USBD_MTP_STORAGE_ReceiveOpt(USBD_HandleTypeDef *pdev);
uint8_t USBD_MTP_STORAGE_ReceiveData(USBD_HandleTypeDef *pdev);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /*__USBD_MTP_STORAGE_H*/
/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
