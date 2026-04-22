/**
  ******************************************************************************
  * @file    			usbh_ioreq.c
  * @author  			FMD XA
  * @brief   			This file handles the issuing of the USB transactions
  * @version 			V1.0.0           
  * @data		 			2025-04-14
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbh_ioreq.h"


/**@addtogroup USBH_LIB
 * @{
 */

/**@addtogroup USBH_LIB_CORE
 * @{
 */

/**@defgroup USBH_IOREQ
 * @brief This file handles the standard protocol processing.
 * @{
 */

/**@defgroup USBH_IOREQ_Private_Defines
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_IOREQ_Private_TypeDefinitions
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_IOREQ_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_IOREQ_Private_Variables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_IOREQ_Private_FunctionPrototypes
 * @{
 */

/**
 * @}
 */

/**@defgroup USBH_IOREQ_Private_Functions
 * @{
 */

/**
 * @brief  USBH_CtlSendSetup
 *         Send the Setup Packet to the device
 * @param  phost: Host Handle
 * @param  buff: Buffer pointer from which the data will be send to device
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_CtlSendSetup(USBH_HandleTypeDef *phost, uint8_t *buff, uint8_t pipe_num)
{
  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          0U,                     /* Direction : OUT */
                          USBH_EP_CONTROL,        /* EP type */
                          USBH_PID_SETUP,         /* Type setup */
                          buff,                   /* data buffer */
                          USBH_SETUP_PKT_SIZE,    /* data length */
                          0U);

  return USBH_OK;
}

/**
 * @brief  USBH_CtlSendData
 *         Send the data Packet to the device
 * @param  phost: Host Handle
 * @param  buff: Buffer pointer from which the data will be send to device
 * @param  length: Length of the data to be sent
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
#ifdef USB_OTG_HS_CORE
USBH_StatusTypeDef  USBH_CtlSendData(USBH_HandleTypeDef *phost, uint8_t *buff, uint16_t length,
                                     uint8_t pipe_num, uint8_t do_ping)
{
  if (phost->device.speed != USBH_SPEED_HIGH)
  {
    do_ping = 0U;
  }
  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          0U,                     /* Direction : OUT */
                          USBH_EP_CONTROL,        /* EP type */
                          USBH_PID_DATA,          /* Type data */
                          buff,                   /* data buffer */
                          length,                 /* data length */
                          do_ping);               /* do ping */

  return USBH_OK;
}
#elif USB_OTG_FS_CORE
USBH_StatusTypeDef  USBH_CtlSendData(USBH_HandleTypeDef *phost, uint8_t *buff, uint16_t length,
                                     uint8_t pipe_num, uint8_t ctl_phase)
{
  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          0U,                     /* Direction : OUT */
                          USBH_EP_CONTROL,        /* EP type */
                          USBH_PID_DATA,          /* Type data */
                          buff,                   /* data buffer */
                          length,                 /* data length */
                          ctl_phase);

  return USBH_OK;
}
#else /*USB_OTG_FS_CORE*/
#endif /*USB_OTG_HS_CORE*/
/**
 * @brief  USBH_CtlReceiveData
 *         Receives the device response to the setup packet
 * @param  phost: Host Handle
 * @param  buff: Buffer pointer in which the response needs to be copied
 * @param  length: Length of the data to be received
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
#ifdef USB_OTG_HS_CORE
USBH_StatusTypeDef  USBH_CtlReceiveData(USBH_HandleTypeDef *phost, uint8_t *buff, uint16_t length,
                                        uint8_t pipe_num)
{
  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          1U,                     /* Direction : IN */
                          USBH_EP_CONTROL,        /* EP type */
                          USBH_PID_DATA,          /* Type data */
                          buff,                   /* data buffer */
                          length,                 /* data length */
                          0U);                    /* do ping */

  return USBH_OK;
}
#elif USB_OTG_FS_CORE
USBH_StatusTypeDef  USBH_CtlReceiveData(USBH_HandleTypeDef *phost, uint8_t *buff, uint16_t length,
                                        uint8_t pipe_num, uint8_t ctl_phase)
{
  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          1U,                     /* Direction : IN */
                          USBH_EP_CONTROL,        /* EP type */
                          USBH_PID_DATA,          /* Type data */
                          buff,                   /* data buffer */
                          length,                 /* data length */
                          ctl_phase);

  return USBH_OK;
}
#else /*USB_OTG_FS_CORE*/
#endif /*USB_OTG_HS_CORE*/
/**
 * @brief  USBH_BulkSendData
 *         Sends the BULK packet to the device
 * @param  phost: Host Handle
 * @param  buff: Buffer pointer from which the data will be sent to the device
 * @param  length: Length of the data to be sent
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_BulkSendData(USBH_HandleTypeDef *phost, uint8_t *buff, uint16_t length,
                                      uint8_t pipe_num, uint8_t do_ping)
{
  if (phost->device.speed != USBH_SPEED_HIGH)
  {
    do_ping = 0U;
  }
  #ifdef USB_OTG_FS_CORE
    do_ping = 0U;
  #endif /*USB_OTG_FS_CORE*/
  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          0U,                     /* Direction : OUT */
                          USBH_EP_BULK,           /* EP type */
                          USBH_PID_DATA,          /* Type data */
                          buff,                   /* data buffer */
                          length,                 /* data length */
                          do_ping);               /* do ping */

  return USBH_OK;
}

/**
 * @brief  USBH_BulkReceiveData
 *         Receives IN BULK packet from device
 * @param  phost: Host Handle
 * @param  buff: Buffer pointer in which received data packet to be copied
 * @param  length: Length of the data to be received
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_BulkReceiveData(USBH_HandleTypeDef *phost, uint8_t *buff, uint16_t length,
                                         uint8_t pipe_num)
{

  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          1U,                     /* Direction : IN */
                          USBH_EP_BULK,           /* EP type */
                          USBH_PID_DATA,          /* Type data */
                          buff,                   /* data buffer */
                          length,                 /* data length */
                          0U);                    /* do ping */

  return USBH_OK;
}

/**
 * @brief  USBH_InterruptSendData
 *         Sends the data on interrupt OUT endpoint
 * @param  phost: Host Handle
 * @param  buff: Buffer pointer from which the data needs to be copied
 * @param  length: Length of the data to be sent
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_InterruptSendData(USBH_HandleTypeDef *phost, uint8_t *buff, uint8_t length,
                                           uint8_t pipe_num)
{

  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          0U,                     /* Direction : OUT */
                          USBH_EP_INTERRUPT,      /* EP type */
                          USBH_PID_DATA,          /* Type data */
                          buff,                   /* data buffer */
                          (uint16_t)length,       /* data length */
                          0U);                    /* do ping */

  return USBH_OK;
}

/**
 * @brief  USBH_InterruptReceiveData
 *         Receives the device response to the interrupt in token
 * @param  phost: Host Handle
 * @param  buff: Buffer pointer in which the response needs to be copied
 * @param  length: Length of the data to be received
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_InterruptReceiveData(USBH_HandleTypeDef *phost, uint8_t *buff, uint8_t length,
                                              uint8_t pipe_num)
{

  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          1U,                     /* Direction : IN */
                          USBH_EP_INTERRUPT,      /* EP type */
                          USBH_PID_DATA,          /* Type data */
                          buff,                   /* data buffer */
                          (uint16_t)length,       /* data length */
                          0U);                    /* do ping */

  return USBH_OK;
}

/**
 * @brief  USBH_IsocSendData
 *         Sends the data on isochronous OUT endpoint
 * @param  phost: Host Handle
 * @param  buff: Buffer pointer from which the data needs to be copied
 * @param  length: Length of the data to be sent
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_IsocSendData(USBH_HandleTypeDef *phost, uint8_t *buff, uint32_t length,
                                      uint8_t pipe_num)
{

  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          0U,                     /* Direction : OUT */
                          USBH_EP_ISO,            /* EP type */
                          USBH_PID_DATA,          /* Type data */
                          buff,                   /* data buffer */
                          (uint16_t)length,       /* data length */
                          0U);                    /* do ping */

  return USBH_OK;
}

/**
 * @brief  USBH_IsocReceiveData
 *         Receives the device response to the isochronous in token
 * @param  phost: Host Handle
 * @param  buff: Buffer pointer in which the response needs to be copied
 * @param  length: Length of the data to be received
 * @param  pipe_num: pipe number
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_IsocReceiveData(USBH_HandleTypeDef *phost, uint8_t *buff, uint32_t length,
                                        uint8_t pipe_num)
{

  (void)USBH_LL_SubmitURB(phost,                  /* Driver handle */
                          pipe_num,               /* Pipe index */
                          1U,                     /* Direction : IN */
                          USBH_EP_ISO,            /* EP type */
                          USBH_PID_DATA,          /* Type data */
                          buff,                   /* data buffer */
                          (uint16_t)length,       /* data length */
                          0U);                    /* do ping */

  return USBH_OK;
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
