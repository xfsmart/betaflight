/**
  ******************************************************************************
  * @file    			usbh_pipes.h
  * @author  			FMD XA
  * @brief   			Header file for usbh_pipes.c
  * @version 			V1.0.0           
  * @data		 			2025-04-10
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBH_PIPES_H
#define __USBH_PIPES_H


#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbh_core.h"

/**@addtogroup USBH_LIB
 * @{
 */

/**@addtogroup USBH_LIB_CORE
 * @{
 */

/**@defgroup USBH_PIPES
 * @{
 */

/**@addtogroup USBH_PIPES_Exported_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBH_IOREQ_Exported_Types
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_IOREQ_Exported_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_IOREQ_Exported_Variables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_PIPES_Exported_FunctionsPrototype
 * @{
 */
USBH_StatusTypeDef USBH_OpenPipe(USBH_HandleTypeDef *phost, uint8_t pipe_num,
                                 uint8_t epnum, uint8_t dev_address,
                                 uint8_t speed, uint8_t ep_types,
                                 uint16_t mps);

USBH_StatusTypeDef USBH_ClosePipe(USBH_HandleTypeDef *phost, uint8_t pipe_num);

uint8_t            USBH_AllocPipe(USBH_HandleTypeDef *phost, uint8_t ep_addr);

USBH_StatusTypeDef USBH_FreePipe(USBH_HandleTypeDef *phost, uint8_t idx);

#if defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U)
USBH_StatusTypeDef USBH_ActivePipe(USBH_HandleTypeDef *phost, uint8_t pipe_num);
#endif /* defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U) */
/**
 * @}
 */

#ifdef  __cplusplus
}
#endif

#endif /*__USBH_PIPES_H*/


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
