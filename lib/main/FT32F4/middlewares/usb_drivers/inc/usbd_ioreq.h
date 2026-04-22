/**
  ******************************************************************************
  * @file    			usbd_ioreq.h
  * @author  			FMD XA
  * @brief   			Header file for usbd_ioreq.c
  * @version 			V1.0.0           
  * @data		 			2025-04-10
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_IOREQ_H
#define __USBD_IOREQ_H


#ifdef __cplusplus
 extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_def.h"
#include "usbd_core.h"

/**@defgroup USBD_IOREQ
 * @brief header file for the usbd_ioreq.c file
 * @{
 */

/**@addtogroup USBD_IOREQ_Exported_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_IOREQ_Exported_Types
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_IOREQ_Exported_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_IOREQ_Exported_Variables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_IOREQ_Exported_FunctionsPrototype
 * @{
 */
USBD_StatusTypeDef USBD_CtlSendData(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len);

USBD_StatusTypeDef USBD_CtlContinueSendData(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len);

USBD_StatusTypeDef USBD_CtlPrepareRx(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len);

USBD_StatusTypeDef USBD_CtlContinueRx(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len);

USBD_StatusTypeDef USBD_CtlSendStatus(USBD_HandleTypeDef *pdev);

USBD_StatusTypeDef USBD_CtlReceiveStatus(USBD_HandleTypeDef *pdev);

uint32_t  USBD_GetRxCount(USBD_HandleTypeDef *pdev, uint8_t ep_addr);

/**
 * @}
 */

#ifdef  __cplusplus
}
#endif

#endif /*__USBD_IOREQ_H*/


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
