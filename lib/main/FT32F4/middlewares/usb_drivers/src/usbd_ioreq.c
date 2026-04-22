/**
  ******************************************************************************
  * @file    			usbd_ioreq.c
  * @author  			FMD XA
  * @brief   			This file provides the IO requests APIs for control endpoint.
  * @version 			V1.0.0           
  * @data		 			2025-04-21
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_ioreq.h"


/**@defgroup USBD_IOREQ
 * @brief control I/O request.
 * @{
 */

/**@defgroup USBD_IOREQ_Private_Defines
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_IOREQ_Private_TypeDefinitions
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_IOREQ_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_IOREQ_Private_Variables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_IOREQ_Private_FunctionPrototypes
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_IOREQ_Private_Functions
 * @{
 */

/**
 * @brief  USBD_CtlSendData
 *         Send data on the ctl pipe
 * @param  pdev: device instance
 * @param  buff: pointer to data buffer
 * @param  len: length of data to be sent
 * @retval Status
 */
USBD_StatusTypeDef  USBD_CtlSendData(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len)
{
  /* Set EP0 state */
  pdev->ep0_state = USBD_EP0_DATA_IN;
  pdev->ep_in[0].total_length = len;

#ifdef USBD_AVOID_PACKET_SPLIT_MPS
  pdev->ep_in[0].rem_length = 0U;
#else
  pdev->ep_in[0].rem_length = len;
#endif /* USBD_AVOID_PACKET_SPLIT_MPS */

  /* start the transfer */
  (void)USBD_LL_Transmit(pdev, 0x00U, pbuf, len);

  return USBD_OK;
}

/**
 * @brief  USBD_CtlContinueSendData
 *         continue sending data on the ctl pipe
 * @param  pdev: device instance
 * @param  buff: pointer to data buffer
 * @param  len: length of data to be sent
 * @retval Status
 */
USBD_StatusTypeDef  USBD_CtlContinueSendData(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len)
{
  /* Start the next transfer */
  (void)USBD_LL_Transmit(pdev, 0x00U, pbuf, len);

  return USBD_OK;
}

/**
 * @brief  USBD_CtlPrepareRx
 *         receive data on the ctl pipe
 * @param  pdev: device instance
 * @param  buff: pointer to data buffer
 * @param  len: length of data to be received
 * @retval Status
 */
USBD_StatusTypeDef  USBD_CtlPrepareRx(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len)
{
  /* Set EP0 state */
  pdev->ep0_state = USBD_EP0_DATA_OUT;
  pdev->ep_out[0].total_length = len;

#ifdef USBD_AVOID_PACKET_SPLIT_MPS
  pdev->ep_out[0].rem_length = 0U;
#else
  pdev->ep_out[0].rem_length = len;
#endif /* USBD_AVOID_PACKET_SPLIT_MPS */

  /* start the transfer */
  (void)USBD_LL_PrepareReceive(pdev, 0x00U, pbuf, len);

  return USBD_OK;

}

/**
 * @brief  USBD_CtlContinueRx
 *         continue receive data on the ctl pipe
 * @param  pdev: device instance
 * @param  buff: pointer to data buffer
 * @param  len: length of data to be received
 * @retval Status
 */
USBD_StatusTypeDef USBD_CtlContinueRx(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len)
{
  (void)USBD_LL_PrepareReceive(pdev, 0x00U, pbuf, len);

  return USBD_OK;
}

/**
 * @brief  USBD_CtlSendStatus
 *         send zero length packet on the ctl pipe
 * @param  pdev: device instance
 * @retval Status
 */
USBD_StatusTypeDef  USBD_CtlSendStatus(USBD_HandleTypeDef *pdev)
{
  /* Set EP0 state */
  pdev->ep0_state = USBD_EP0_STATUS_IN;
  /* start the transfer */
  (void)USBD_LL_Transmit(pdev, 0x00U, NULL, 0U);

  return USBD_OK;
}

/**
 * @brief  USBD_CtlReceiveStatus
 *         receive zero length packet on the ctl pipe
 * @param  pdev: device instance
 * @retval Status
 */
USBD_StatusTypeDef  USBD_CtlReceiveStatus(USBD_HandleTypeDef *pdev)
{
  /* Set EP0 state */
  pdev->ep0_state = USBD_EP0_STATUS_OUT;
  /* start the transfer */
  (void)USBD_LL_PrepareReceive(pdev, 0x00U, NULL, 0U);

  return USBD_OK;
}

/**
 * @brief  USBD_GetRxCount
 *         return the received data length
 * @param  pdev: device instance
 * @retval Status
 */
uint32_t USBD_GetRxCount(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return USBD_LL_GetRxDataSize(pdev, ep_addr);
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
