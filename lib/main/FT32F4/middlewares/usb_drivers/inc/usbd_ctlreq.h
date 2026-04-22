/**
  ******************************************************************************
  * @file    			usbd_ctlreq.h
  * @author  			FMD XA
  * @brief   			Header file for usbd_ctlreq.c
  * @version 			V1.0.0           
  * @data		 			2025-04-17
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_CTLREQ_H
#define __USBD_CTLREQ_H


#ifdef __cplusplus
 extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_def.h"


/**@defgroup USBD_CTLREQ
 * @brief header file for the usbd_ctlreq.c
 * @{
 */

/**@addtogroup USBD_CTLREQ_Exported_Defines
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_CTLREQ_Exported_Types
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_CTLREQ_Exported_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_CTLREQ_Exported_Variables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_CTLREQ_Exported_FunctionsPrototype
 * @{
 */
USBD_StatusTypeDef USBD_StdDevReq(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req);
USBD_StatusTypeDef USBD_StdItfReq(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req);
USBD_StatusTypeDef USBD_StdEPReq(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req);

void USBD_CtlError(USBD_HandleTypeDef *pdev, USBD_SetupReqTypeDef *req);
void USBD_ParseSetupRequest(USBD_SetupReqTypeDef *req, uint8_t *pdata);
void USBD_GetString(uint8_t *desc, uint8_t *unicode, uint16_t *len);

/**
 * @}
 */

#ifdef  __cplusplus
}
#endif

#endif /*__USBD_CTLREQ_H*/


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
