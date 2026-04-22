/**
  ******************************************************************************
  * @file    			usbh_msc_bot.c
  * @author  			FMD XA
  * @brief   			This file includes the BOT protocol related functions.
  * @version 			V1.0.0           
  * @data		 			2025-04-29
  ******************************************************************************
  *
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "usbh_msc_bot.h"

/**@addtogroup USBH_LIB
 * @{
 */

/**@addtogroup USBH_CLASS
 * @{
 */

/**@addtogroup USBH_MSC_CLASS
 * @{
 */

/**@defgroup USBH_MSC_BOT
 * @brief This file includes the mass storage related functions.
 * @{
 */

/**@defgroup USBH_MSC_BOT_Private_TypeDefinitions
 * @{
 */

/**
 * @}
 */


/**@defgroup USBH_MSC_BOT_Private_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBH_MSC_BOT_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_MSC_BOT_Private_Variables
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_MSC_BOT_Private_FunctionPrototypes
 * @{
 */
static USBH_StatusTypeDef USBH_MSC_BOT_Abort(USBH_HandleTypeDef *phost, uint8_t dir);
static BOT_CSWStatusTypeDef USBH_MSC_DecodeCSW(USBH_HandleTypeDef *phost);



/**
 * @}
 */

/**@defgroup USBH_MSC_BOT_Private_Functions
 * @{
 */

/**
 * @brief  USBH_MSC_BOT_REQ_Reset
 *         The function the MSC BOT Reset request.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_MSC_BOT_REQ_Reset(USBH_HandleTypeDef *phost)
{
  phost->Control.setup.b.bmRequestType = USB_H2D |
                                         USB_REQ_TYPE_CLASS |
                                         USB_REQ_RECIPIENT_INTERFACE;
  phost->Control.setup.b.bRequest  = USB_REQ_BOT_RESET;
  phost->Control.setup.b.wValue.w  = 0U;
  phost->Control.setup.b.wIndex.w  = 0U;
  phost->Control.setup.b.wLength.w = 0U;

  return USBH_CtlReq(phost, NULL, 0U);
}

/**
 * @brief  USBH_MSC_BOT_REQ_GetMaxLUN
 *         The function the MSC BOT GetMaxLUN request.
 * @param  phost: Host Handle
 * @parma  Maxln: pointer to maxlun variable
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_MSC_BOT_REQ_GetMaxLUN(USBH_HandleTypeDef *phost, uint8_t *Maxlun)
{
  phost->Control.setup.b.bmRequestType = USB_D2H |
                                         USB_REQ_TYPE_CLASS |
                                         USB_REQ_RECIPIENT_INTERFACE;
  phost->Control.setup.b.bRequest  = USB_REQ_GET_MAX_LUN;
  phost->Control.setup.b.wValue.w  = 0U;
  phost->Control.setup.b.wIndex.w  = 0U;
  phost->Control.setup.b.wLength.w = 1U;

  return USBH_CtlReq(phost, Maxlun, 1U);
}

/**
 * @brief  USBH_MSC_BOT_Init
 *         The function initializes the MSC BOT protocol.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_MSC_BOT_Init(USBH_HandleTypeDef *phost)
{
  MSC_HandleTypeDef *MSC_Handle = (MSC_HandleTypeDef *)phost->pActiveClass->pData;

  MSC_Handle->hbot.cbw.field.Signature = BOT_CBW_SIGNATURE;
  MSC_Handle->hbot.cbw.field.Tag       = BOT_CBW_TAG;
  MSC_Handle->hbot.state               = BOT_SEND_CBW;
  MSC_Handle->hbot.cmd_state           = BOT_CMD_SEND;

  return USBH_OK;
}

/**
 * @brief  USBH_MSC_BOT_Process
 *         The function handle the BOT protocol.
 * @param  phost: Host Handle
 * @param  lun : logical unit number
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_MSC_BOT_Process(USBH_HandleTypeDef *phost, uint8_t lun)
{
  USBH_StatusTypeDef    status     = USBH_BUSY;
  USBH_StatusTypeDef    error      = USBH_BUSY;
  BOT_CSWStatusTypeDef  CSW_Status = BOT_CSW_CMD_FAILED;
  USBH_URBStateTypeDef  URB_Status = USBH_URB_IDLE;
  MSC_HandleTypeDef     *MSC_Handle= (MSC_HandleTypeDef *) phost->pActiveClass->pData;
  uint8_t toggle = 0U;

  switch (MSC_Handle->hbot.state)
  {
    case BOT_SEND_CBW:
      MSC_Handle->hbot.cbw.field.LUN = lun;
      MSC_Handle->hbot.state = BOT_SEND_CBW_WAIT;
      (void)USBH_BulkSendData(phost, MSC_Handle->hbot.cbw.data,
                              BOT_CBW_LENGTH, MSC_Handle->OutPipe, 1U);

      break;

    case BOT_SEND_CBW_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, MSC_Handle->OutPipe);

      if (URB_Status == USBH_URB_DONE)
      {
        if (MSC_Handle->hbot.cbw.field.DataTransferLength != 0U)
        {
          /* if there is data transfer stage */
          if (((MSC_Handle->hbot.cbw.field.Flags) & USB_REQ_DIR_MASK) == USB_D2H)
          {
            /* data direction is in */
            MSC_Handle->hbot.state = BOT_DATA_IN;
          }
          else
          {
            /* data direction is out */
            MSC_Handle->hbot.state = BOT_DATA_OUT;
          }
        }
        else
        {
          /* if there is no data transfer stage */
          MSC_Handle->hbot.state = BOT_RECEIVE_CSW;
        }
      }
      else if (URB_Status == USBH_URB_NOTREADY)
      {
        /* Re-send CBW */
        MSC_Handle->hbot.state = BOT_SEND_CBW;
      }
      else
      {
        if (URB_Status == USBH_URB_STALL)
        {
          MSC_Handle->hbot.state = BOT_ERROR_OUT;
        }
      }
      break;

    case BOT_DATA_IN:
      /* send first packet */
      (void)USBH_BulkReceiveData(phost, MSC_Handle->hbot.pbuf, MSC_Handle->InEpSize,
                                 MSC_Handle->InPipe);
      MSC_Handle->hbot.state = BOT_DATA_IN_WAIT;

      break;

    case BOT_DATA_IN_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, MSC_Handle->InPipe);

      if (URB_Status == USBH_URB_DONE)
      {
        /* adjust data pointer and data length */
        if (MSC_Handle->hbot.cbw.field.DataTransferLength > MSC_Handle->InEpSize)
        {
          MSC_Handle->hbot.pbuf += MSC_Handle->InEpSize;
          MSC_Handle->hbot.cbw.field.DataTransferLength -= MSC_Handle->InEpSize;
        }
        else
        {
          MSC_Handle->hbot.cbw.field.DataTransferLength = 0U;
        }
        /* more data to be received */
        if (MSC_Handle->hbot.cbw.field.DataTransferLength > 0U)
        {
          /* Send next packet */
          (void)USBH_BulkReceiveData(phost, MSC_Handle->hbot.pbuf, MSC_Handle->InEpSize,
                                     MSC_Handle->InPipe);
        }
        else
        {
          /* if value was 0, and successful transfer, then change the state */
          MSC_Handle->hbot.state = BOT_RECEIVE_CSW;
        }
      }
      else if (URB_Status == USBH_URB_STALL)
      {
        /* this is data in stage stall condition */
        MSC_Handle->hbot.state = BOT_ERROR_IN;
      }
      else
      {
        /*...*/
      }
      break;

    case BOT_DATA_OUT:
      (void)USBH_BulkSendData(phost, MSC_Handle->hbot.pbuf, MSC_Handle->OutEpSize,
                              MSC_Handle->OutPipe, 1U);
      MSC_Handle->hbot.state = BOT_DATA_OUT_WAIT;
      break;

    case BOT_DATA_OUT_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, MSC_Handle->OutPipe);

      if (URB_Status == USBH_URB_DONE)
      {
        /* adjust data pointer and data length */
        if (MSC_Handle->hbot.cbw.field.DataTransferLength > MSC_Handle->OutEpSize)
        {
          MSC_Handle->hbot.pbuf += MSC_Handle->OutEpSize;
          MSC_Handle->hbot.cbw.field.DataTransferLength -= MSC_Handle->OutEpSize;
        }
        else
        {
          MSC_Handle->hbot.cbw.field.DataTransferLength = 0U;
        }
        /* more data to be send */
        if (MSC_Handle->hbot.cbw.field.DataTransferLength > 0U)
        {
          /* Send next packet */
          (void)USBH_BulkSendData(phost, MSC_Handle->hbot.pbuf, MSC_Handle->OutEpSize,
                                  MSC_Handle->OutPipe, 1U);
        }
        else
        {
          /* if value was 0, and successful transfer, then change the state */
          MSC_Handle->hbot.state = BOT_RECEIVE_CSW;
        }
      }
      else if (URB_Status == USBH_URB_NOTREADY)
      {
        /* resend same data */
        MSC_Handle->hbot.state = BOT_DATA_OUT;
      }
      else if (URB_Status == USBH_URB_STALL)
      {
        /* this is data in stage stall condition */
        MSC_Handle->hbot.state = BOT_ERROR_OUT;
      }
      else
      {
        /*...*/
      }
      break;

    case BOT_RECEIVE_CSW:
      (void)USBH_BulkReceiveData(phost, MSC_Handle->hbot.csw.data, BOT_CSW_LENGTH,
                                 MSC_Handle->InPipe);
      MSC_Handle->hbot.state = BOT_RECEIVE_CSW_WAIT;
      break;

    case BOT_RECEIVE_CSW_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, MSC_Handle->InPipe);

      /* decode csw */
      if (URB_Status == USBH_URB_DONE)
      {
        MSC_Handle->hbot.state = BOT_SEND_CBW;
        MSC_Handle->hbot.cmd_state = BOT_CMD_SEND;
        CSW_Status = USBH_MSC_DecodeCSW(phost);

        if (CSW_Status == BOT_CSW_CMD_PASSED)
        {
          status = USBH_OK;
        }
        else
        {
          status = USBH_FAIL;
        }
      }
      else if (URB_Status == USBH_URB_STALL)
      {
        MSC_Handle->hbot.state = BOT_ERROR_IN;
      }
      else
      {
        /*...*/
      }
      break;

    case BOT_ERROR_IN:
      error = USBH_MSC_BOT_Abort(phost, BOT_DIR_IN);

      if (error == USBH_OK)
      {
        MSC_Handle->hbot.state = BOT_RECEIVE_CSW;
      }
      else if (error == USBH_UNRECOVERED_ERROR)
      {
        /* this means that there is a stall error limit, do reset recovery */
        MSC_Handle->hbot.state = BOT_UNRECOVERED_ERROR;
      }
      else
      {
        /*...*/
      }
      break;

    case BOT_ERROR_OUT:
      error = USBH_MSC_BOT_Abort(phost, BOT_DIR_OUT);

      if (error == USBH_OK)
      {
        toggle = USBH_LL_GetToggle(phost, MSC_Handle->OutPipe);
        (void)USBH_LL_SetToggle(phost, MSC_Handle->OutPipe, (1U - toggle));
        (void)USBH_LL_SetToggle(phost, MSC_Handle->InPipe, 0U);
        MSC_Handle->hbot.state = BOT_ERROR_IN;
      }
      else
      {
        if (error == USBH_UNRECOVERED_ERROR)
        {
          MSC_Handle->hbot.state = BOT_UNRECOVERED_ERROR;
        }
      }
      break;

    case BOT_UNRECOVERED_ERROR:
      status = USBH_MSC_BOT_REQ_Reset(phost);
      if (status == USBH_OK)
      {
        MSC_Handle->hbot.state = BOT_SEND_CBW;
      }
      break;

    default:
      break;
  }

  return status;
}

/**
 * @brief  USBH_MSC_BOT_Abort
 *         The function handle the BOT abort process.
 * @param  phost: Host Handle
 * @param  lun : logical unit number(unused)
 * @param  dir : direction (0: out / 1: in)
 * @retval USBH Status
 */
static USBH_StatusTypeDef USBH_MSC_BOT_Abort(USBH_HandleTypeDef *phost, uint8_t dir)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MSC_HandleTypeDef *MSC_Handle = (MSC_HandleTypeDef *) phost->pActiveClass->pData;

  switch (dir)
  {
    case BOT_DIR_IN:
      /* send ClrFeature on bulk in endpoint */
      status = USBH_ClrFeature(phost, MSC_Handle->InEp);
      break;

    case BOT_DIR_OUT:
      /* send ClrFeature on bulk out endpoint */
      status = USBH_ClrFeature(phost, MSC_Handle->OutEp);
      break;

    default:
      break;
  }
  return status;
}

/**
 * @brief  USBH_MSC_DecodeCSW
 *         The function decode the csw received by the device and
 *         update the same to upper layer.
 * @param  phost: Host Handle
 * @retval USBH Status
 * @notes
 *        refer ro USB Mass-Storage Class : BOT
 *        6.3.1 valid cse conditions:
 *        the host shall consider the csw valid when:
 *        1. dCSWSignature is euqal to 53425355h
 *        2. the CSW is 13 (Dh) bytes in length
 *        3. dCSWTag matches the dCBWTag from the corresponding CBW.
 */
static BOT_CSWStatusTypeDef USBH_MSC_DecodeCSW(USBH_HandleTypeDef *phost)
{
  MSC_HandleTypeDef *MSC_Handle = (MSC_HandleTypeDef *) phost->pActiveClass->pData;
  BOT_CSWStatusTypeDef status = BOT_CSW_CMD_FAILED;

  /* checking if the transfer length is different than 13 */
  if (USBH_LL_GetLastXferSize(phost, MSC_Handle->InPipe) != BOT_CSW_LENGTH)
  {
    /* (4) Hi > Dn (host expects to receive data from the device,
     * device intends to transfer no data)
     * (5) Hi > Di (host expects to receive data from the device,
     * device intends to send data to the host)
     * (9) Ho > Dn (host expects to send data to the device,
     * device intends to transfer no data)
     * (11) Ho > Do (host expects to send data to the device,
     * device intends to receive data from the host)
     */
    status = BOT_CSW_PHASE_ERROR;
  }
  else
  {
    /* csw length is correct */
    /* check validity of the csw signature and cswstatus */
    if (MSC_Handle->hbot.csw.field.Signature == BOT_CSW_SIGNATURE)
    {
      /* check condition 1.dCSWSignature is equal to 53425355h */
      if (MSC_Handle->hbot.csw.field.Tag == MSC_Handle->hbot.cbw.field.Tag)
      {
        /* check condition 3.dCSWTag match the dCBWTag from the corresponding CBW */
        if (MSC_Handle->hbot.csw.field.Status == 0U)
        {
          /* (1) Hn = Dn (host expects no data transfers,
           * device intends to transfer no data)
           * (6) Hi = Di (host expects to receive data from the device,
           * device intends to send data to the host)
           * (12)Ho = Do (host expects to send data to the device,
           * device intends to receive data from the host)
           */
          status = BOT_CSW_CMD_PASSED;
        }
        else if (MSC_Handle->hbot.csw.field.Status == 1U)
        {
          status = BOT_CSW_CMD_FAILED;
        }
        else if (MSC_Handle->hbot.csw.field.Status == 2U)
        {
          /* (2) Hn < Di (host expects no data transfers,
           * device intends to send data to the host)
           * (3) Hn < Do (host expects no data transfers,
           * device intends to receive data from the host)
           * (7) Hi < Di (host expects to receive data from the device,
           * device intends to send data to the host)
           * (8) Hi <> Do (host expects to receive data from the device,
           * device intends to receive data from the host)
           * (10)Ho <> Di (host expects to send data to the device,
           * device intends to send data to the host)
           * (13)Ho < Do (host expects to send data to the device,
           * device intends to receive data from the host)
           */

          status = BOT_CSW_PHASE_ERROR;
        }
        else
        {
          /*...*/
        }
      } /* CSW Tag matching is checked */
    } /* CSW Signature cprrect checking */
    else
    {
      /* if the CSW Signature is not valid, we shall return the phase error
       * to upper layers for reset recovery */
      status = BOT_CSW_PHASE_ERROR;
    }
  } /* CSW Length check */

  return status;

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
