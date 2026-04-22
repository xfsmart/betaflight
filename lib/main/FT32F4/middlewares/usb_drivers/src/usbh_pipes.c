/**
  ******************************************************************************
  * @file    			usbh_pipes.c
  * @author  			FMD XA
  * @brief   			This file implements function for opening and closing pipes.
  * @version 			V1.0.0           
  * @data		 			2025-04-14
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbh_pipes.h"


/**@addtogroup USBH_LIB
 * @{
 */

/**@addtogroup USBH_LIB_CORE
 * @{
 */

/**@defgroup USBH_PIPES
 * @brief This file includes opening and closing pipes.
 * @{
 */

/**@defgroup USBH_PIPES_Private_Defines
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_PIPES_Private_TypeDefinitions
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_PIPES_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_PIPES_Private_Variables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_PIPES_Private_Functions
 * @{
 */
static uint16_t USBH_GetFreePipe(USBH_HandleTypeDef *phost);


/**
 * @brief  USBH_OpenPipe
 *         Open a pipe
 * @param  phost: Host Handle
 * @param  pipe_num: pipe number
 * @param  dev_address: USB device address allocated to attached device
 * @param  speed : USB device speed(full/low)
 * @param  ep_type: end point type(bulk/int/ctl)
 * @param  mps: max pkt size
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_OpenPipe(USBH_HandleTypeDef *phost, uint8_t pipe_num, uint8_t epnum,
                                 uint8_t dev_address, uint8_t speed, uint8_t ep_type,
                                 uint16_t mps)
{
  (void)USBH_LL_OpenPipe(phost, pipe_num, epnum, dev_address, speed, ep_type, mps);

  return USBH_OK;
}

#if defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U)
/**
 * @brief  USBH_ActivatePipe
 *         Activate a pipe
 * @param  phost: Host Handle
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_ActivatePipe(USBH_HandleTypeDef *phost, uint8_t pipe_num)
{
  USBH_LL_ActivatePipe(phost, pipe_num);

  return USBH_OK;
}

#endif /* defined (USBH_IN_NAK_PROCESS) && (USBH_IN_NAK_PROCESS == 1U) */

/**
 * @brief  USBH_ClosePipe
 *         Close a pipe
 * @param  phost: Host Handle
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_ClosePipe(USBH_HandleTypeDef *phost, uint8_t pipe_num)
{
  (void)USBH_LL_ClosePipe(phost, pipe_num);

  return USBH_OK;
}

/**
 * @brief  USBH_AllocPipe
 *         Alloc a new pipe
 * @param  phost: Host Handle
 * @param  ep_addr: end point for which the pipe to be allocated
 * @retval pipe number
 */
uint8_t USBH_AllocPipe(USBH_HandleTypeDef *phost, uint8_t ep_addr)
{
  uint16_t pipe;
  pipe = USBH_GetFreePipe(phost);

  if (pipe != 0xFFFFU)
  {
    phost->Pipes[pipe & 0xFU] = (uint32_t)(0x8000U | ep_addr);
  }

  return (uint8_t)pipe;
}

/**
 * @brief  USBH_FreePipe
 *         Free the USB pipe
 * @param  phost: Host Handle
 * @param  idx: pipe number to be freed
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_FreePipe(USBH_HandleTypeDef *phost, uint8_t idx)
{
  if (idx < USBH_MAX_PIPES_NBR)
  {
    phost->Pipes[idx] &= 0x7FFFU;
  }

  return USBH_OK;
}

/**
 * @brief  USBH_GetFreePipe
 * @param  phost: Host Handle
 *         Get a free pipe number for allocation to a device endpoint
 * @retval idx: free pipe
 */
static uint16_t USBH_GetFreePipe(USBH_HandleTypeDef *phost)
{
  uint8_t idx = 0U;
  for (idx = 0U; idx < USBH_MAX_PIPES_NBR; idx++)
  {
    if ((phost->Pipes[idx] & 0x8000U) == 0U)
    {
      return (uint16_t)idx;
    }
  }

  return 0xFFFFU;
}


/**
 * @}
 */



/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
