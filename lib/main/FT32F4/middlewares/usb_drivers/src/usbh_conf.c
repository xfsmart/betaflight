/**
  ******************************************************************************
  * @file    			usbh_conf.c
  * @author  			FMD XA
  * @brief   			This file implements the board support package for the USB host library
  *          >>->-This template should be copied to the user folder, renamed and customized
  *          >>->-following user needs.
  * @version 			V1.0.0           
  * @data		 			2025-04-22
  ******************************************************************************
  */
#ifdef USB_OTG_HS_CORE
/* Includes ------------------------------------------------------------------*/
#include "usbh_core.h"
#include "usbh_conf.h"


HCD_HS_HandleTypeDef hhcd;
/**
 * @brief  HCD_HS_SOF_Callback
 * @param  hhcd
 * @retval none
 */
void HCD_HS_SOF_Callback(HCD_HS_HandleTypeDef *hhcd)
{
  USBH_LL_IncTimer(hhcd->pData);
}

/**
 * @brief  HCD_HS_Connect_Callback
 * @param  hhcd
 * @retval none
 */
void HCD_HS_Connect_Callback(HCD_HS_HandleTypeDef *hhcd)
{
  USBH_LL_Connect(hhcd->pData);
}

/**
 * @brief  HCD_HS_Disconnect_Callback
 * @param  hhcd
 * @retval none
 */
void HCD_HS_Disconnect_Callback(HCD_HS_HandleTypeDef *hhcd)
{
  USBH_LL_Disconnect(hhcd->pData);
}

/**
 * @brief  HCD_HS_PortEnabled_Callback
 * @param  hhcd
 * @retval none
 */
void HCD_HS_PortEnabled_Callback(HCD_HS_HandleTypeDef *hhcd)
{
  USBH_LL_PortEnabled(hhcd->pData);
}

/**
 * @brief  HCD_HS_PortDisabled_Callback
 * @param  hhcd
 * @retval none
 */
void HCD_HS_PortDisabled_Callback(HCD_HS_HandleTypeDef *hhcd)
{
  USBH_LL_PortDisabled(hhcd->pData);
}

/**
  * @brief  Notify URB state change callback.
  * @param  hhcd HCD handle
  * @param  chnum Channel number.
  *         This parameter can be a value from 1 to 15
  * @param  urb_state:
  *          This parameter can be one of these values:
  *            URB_IDLE/
  *            URB_DONE/
  *            URB_NOTREADY/
  *            URB_NYET/
  *            URB_ERROR/
  *            URB_STALL/
  * @retval None
  */
void HCD_HS_HC_NotifyURBChange_Callback(HCD_HS_HandleTypeDef *hhcd, uint8_t chnum, HCD_HS_URBStateTypeDef urb_state)
{
  
}


/**
 * @brief  USBH_LL_Init
 *         Initialize the low level portion of the host driver.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_LL_Init(USBH_HandleTypeDef *phost)
{
  /* set ll driver parameters */
  hhcd.Init.Host_channels = 8;
  hhcd.Init.dma_enable = 0;
  hhcd.Init.low_power_enable = 0;
  hhcd.Init.speed = HCD_SPEED_HIGH;

  /* link the driver to the stack */
  hhcd.pData = phost;
  phost->pData = &hhcd;

  HCD_HS_Init(&hhcd);

  USBH_LL_SetTimer(phost, HCD_HS_GetCurrentFrame());

  return USBH_OK;
}

/**
 * @brief  USBH_LL_DeInit
 *         Deinitialize the low level portion of the host driver.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_LL_DeInit(USBH_HandleTypeDef *phost)
{
  HCD_HS_DeInit(&hhcd);

  return USBH_OK;
}

/**
 * @brief  USBH_LL_Start
 *         Starts the low level portion of the host driver.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_LL_Start(USBH_HandleTypeDef *phost)
{
  HCD_HS_Start(phost->pData);
  return USBH_OK;
}


/**
 * @brief  USBH_LL_Stop
 *         Stops the low level portion of the host driver.
 * @param  phost: Host Handle
 * @retval USBH Status
 */
USBH_StatusTypeDef  USBH_LL_Stop(USBH_HandleTypeDef *phost)
{
  HCD_HS_Stop(phost->pData);
  return USBH_OK;
}

/**
 * @brief  USBH_LL_GetSpeed
 *         Return the USB Host Speed from the low level Driver.
 * @param  phost: Host Handle
 * @retval USBH speed
 */
USBH_SpeedTypeDef USBH_LL_GetSpeed(void)
{
  USBH_SpeedTypeDef speed = USBH_SPEED_FULL;

  switch (HCD_HS_GetCurrentSpeed())
  {
    case 0:
      speed = USBH_SPEED_HIGH;
      break;

    case 1:
      speed = USBH_SPEED_FULL;
      break;

    case 2:
      speed = USBH_SPEED_LOW;
      break;

    default:
      speed = USBH_SPEED_FULL;
      break;
  }
  return speed;
}

/**
 * @brief  USBH_LL_ResetPort
 *         Resets the host port of the low level driver.
 * @param  phost: Host Handle
 * @retval USBH status
 */
USBH_StatusTypeDef USBH_LL_ResetPort(void)
{
  HCD_HS_ResetPort();
  return USBH_OK;
}


/**
 * @brief  USBH_LL_GetLastXferSize
 *         Return the last transferred packet size.
 * @param  phost: Host Handle
 * @param  pipe: pipe index
 * @retval packet size
 */
uint32_t USBH_LL_GetLastXferSize(USBH_HandleTypeDef *phost, uint8_t pipe)
{
  return HCD_HS_HC_GetXferCount(phost->pData, pipe);
}

/**
  * @brief  USBH_LL_OpenPipe
  *         Open a pipe of the low level driver.
  * @param  phost: host handle
  * @param  pipe: pipe index
  * @param  epnum: Endpoint number.
  * @param  dev_address: Current device address
  * @param  speed:Current device speed.
  * @param  ep_type: Endpoint Type.
  * @param  mps: endpoint Max Packet Size.
  * @retval USBH status
  */
USBH_StatusTypeDef USBH_LL_OpenPipe(USBH_HandleTypeDef *phost,
                                    uint8_t pipe,
                                    uint8_t epnum,
                                    uint8_t dev_address,
                                    uint8_t speed,
                                    uint8_t ep_type,
                                    uint16_t mps)
{
  HCD_HS_HC_Init(phost->pData,
                 pipe,
                 epnum,
                 dev_address,
                 speed,
                 ep_type,
                 mps);
  return USBH_OK;

}

/**
  * @brief  USBH_LL_Activate
  *         Activate a pipe of the low level driver.
  * @param  phost: host handle
  * @param  pipe: pipe index
  * @retval USBH status
  */
USBH_StatusTypeDef USBH_LL_Activate(USBH_HandleTypeDef *phost, uint8_t pipe)
{
  UNUSED(phost);
  UNUSED(pipe);
  return USBH_OK;
}

/**
  * @brief  USBH_LL_ClosePipe
  *         Close a pipe of the low level driver.
  * @param  phost: host handle
  * @param  pipe: pipe index
  * @retval USBH status
  */
USBH_StatusTypeDef USBH_LL_ClosePipe(USBH_HandleTypeDef *phost, uint8_t pipes)
{
  HCD_HS_HC_Halt(phost->pData, pipes);
  return USBH_OK;
}

/**
  * @brief  USBH_LL_SubmitURB
  *         Submit a new URB to the low level driver.
  * @param  phost: host handle
  * @param  pipe: pipe index
  *         This parameter can be a value from 1 to 15
  * @param  direction: Channel number.
  *          This parameter can be one of these values:
  *           0 : Output / 1 : Input
  * @param  ep_type:Endpoint Type.
  *          This parameter can be one of these values:
  *            EP_TYPE_CTRL: Control type/
  *            EP_TYPE_ISOC: Isochronous type/
  *            EP_TYPE_BULK: Bulk type/
  *            EP_TYPE_INTR: Interrupt type/
  * @param  token:Endpoint Type.
  *          This parameter can be one of these values:
  *            0: HC_PID_SETUP / 1: HC_PID_DATA1
  * @param  pbuff: pointer to URB data
  * @param  length: Length of URB data
  * @param  do_ping: activate do ping protocol (for high speed only).
  *          This parameter can be one of these values:
  *           0 : do ping inactive / 1 : do ping active
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_SubmitURB(USBH_HandleTypeDef *phost,
                                     uint8_t pipe,
                                     uint8_t direction,
                                     uint8_t ep_type,
                                     uint8_t token,
                                     uint8_t *pbuff,
                                     uint16_t length,
                                     uint8_t do_ping)
{
  HCD_HS_HC_SubmitRequest(phost->pData,
                          pipe,
                          direction,
                          ep_type,
                          token,
                          pbuff,
                          length,
                          do_ping);

  return USBH_OK;

}

/**
  * @brief  USBH_LL_GetURBState
  *         get a URB state from the low level driver.
  * @param  phost: host handle
  * @param  pipe: pipe index
  *         This parameter can be a value from 1 to 15
  * @retval URB state.
  *          This parameter can be one of these values:
  *            URB_IDLE
  *            URB_DONE
  *            URB_NOTREADY
  *            URB_NYET
  *            URB_ERROR
  *            URB_STALL
  */
USBH_URBStateTypeDef USBH_LL_GetURBState(USBH_HandleTypeDef *phost, uint8_t pipe)
{
  return (USBH_URBStateTypeDef)HCD_HS_HC_GetURBState(phost->pData, pipe);
}

/**
  * @brief  USBH_LL_SetToggle
  * @param  phost: host handle
  * @param  pipe: pipe index
  * @param  toggle: toggle(0/1)
  * @retval USBH state.
  */
USBH_StatusTypeDef USBH_LL_SetToggle(USBH_HandleTypeDef *phost, uint8_t pipe, uint8_t toggle)
{

  if (hhcd.hc[pipe].ep_is_in)
  {
    hhcd.hc[pipe].toggle_in = toggle;
  }
  else
  {
    hhcd.hc[pipe].toggle_out = toggle;
  }
  return USBH_OK;
}

/**
  * @brief  USBH_LL_GetToggle
  *         return the current toggle of a pipe
  * @param  phost: host handle
  * @param  pipe: pipe index
  * @retval toggle (0/1)
  */
uint8_t USBH_LL_GetToggle(USBH_HandleTypeDef *phost, uint8_t pipe)
{
  uint8_t toggle = 0;

  if (hhcd.hc[pipe].ep_is_in)
  {
    toggle = hhcd.hc[pipe].toggle_in;
  }
  else
  {
    toggle = hhcd.hc[pipe].toggle_out;
  }
  return toggle;
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

#endif /* USB_OTG_HS_CORE*/
/************************ (C) COPYRIGHT FMD *****END OF FILE****/
