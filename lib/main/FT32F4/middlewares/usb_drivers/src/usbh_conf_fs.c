/**
  ******************************************************************************
  * @file    			usbh_conf_fs.c
  * @author  			FMD XA
  * @brief   			This file implements the board support package for the USB host library
  *          >>->-This template should be copied to the user folder, renamed and customized
  *          >>->-following user needs.
  * @version 			V1.0.0           
  * @data		 			2025-04-22
  ******************************************************************************
  */
#ifdef USB_OTG_FS_CORE
/* Includes ------------------------------------------------------------------*/
#include "usbh_core.h"
#include "usbh_conf.h"

HCD_FS_HandleTypeDef hhcd;


void HCD_FS_MspInit(PCD_FS_HandleTypeDef *hpcd)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
}

/**
 * @brief  HCD_FS_SOF_Callback
 * @param  hhcd
 * @retval none
 */
void HCD_FS_SOF_Callback(HCD_FS_HandleTypeDef *hhcd)
{
  USBH_LL_IncTimer(hhcd->pData);
}

/**
 * @brief  HCD_FS_Connect_Callback
 * @param  hhcd
 * @retval none
 */
void HCD_FS_Connect_Callback(HCD_FS_HandleTypeDef *hhcd)
{
  (void)USBH_LL_ResetPort();
  USBH_LL_Connect(hhcd->pData);
}

/**
 * @brief  HCD_FS_Disconnect_Callback
 * @param  hhcd
 * @retval none
 */
void HCD_FS_Disconnect_Callback(HCD_FS_HandleTypeDef *hhcd)
{
  USBH_LL_Disconnect(hhcd->pData);
}

/**
 * @brief  HCD_FS_PortEnabled_Callback
 * @param  hhcd
 * @retval none
 */
void HCD_FS_PortEnabled_Callback(HCD_FS_HandleTypeDef *hhcd)
{
  USBH_LL_PortEnabled(hhcd->pData);
}

/**
 * @brief  HCD_FS_PortDisabled_Callback
 * @param  hhcd
 * @retval none
 */
void HCD_FS_PortDisabled_Callback(HCD_FS_HandleTypeDef *hhcd)
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
void HCD_FS_EP_NotifyURBChange_Callback(HCD_FS_HandleTypeDef *hhcd, uint8_t chnum, HCD_FS_URBStateTypeDef urb_state)
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
  hhcd.Init.endpoints = 4;
  hhcd.Init.Host_eps = 8;
  hhcd.Init.speed = HCD_SPEED_FULL;

  /* link the driver to the stack */
  hhcd.pData = phost;
  phost->pData = &hhcd;

  HCD_FS_Init(&hhcd);

  USBH_LL_SetTimer(phost, HCD_FS_GetCurrentFrame());

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
  HCD_FS_DeInit(&hhcd);

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
  (void)HCD_FS_Start(phost->pData);
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
  (void)HCD_FS_Stop(phost->pData);
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

  switch (HCD_FS_GetCurrentSpeed())
  {
    case 0:
      speed = USBH_SPEED_FULL;
      break;

    case 1:
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
  HCD_FS_ResetPort();
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
  return HCD_FS_EP_GetXferCount(phost->pData, pipe);
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
  HCD_FS_EP_Init(phost->pData,
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
//  HCD_HS_HC_Halt(phost->pData, pipes);
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
  *            0: EP_PID_SETUP / 1: EP_PID_DATA1
  * @param  pbuff: pointer to URB data
  * @param  length: Length of URB data
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_LL_SubmitURB(USBH_HandleTypeDef *phost,
                                     uint8_t pipe,
                                     uint8_t direction,
                                     uint8_t ep_type,
                                     uint8_t token,
                                     uint8_t *pbuff,
                                     uint16_t length,
                                     uint8_t ctl_state)
{
  HCD_FS_EP_SubmitRequest(phost->pData,
                          pipe,
                          direction,
                          ep_type,
                          token,
                          pbuff,
                          length,
                          ctl_state);

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
  return (USBH_URBStateTypeDef)HCD_FS_EP_GetURBState(phost->pData, pipe);
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

  if (hhcd.ep[pipe].ep_is_in)
  {
    hhcd.ep[pipe].toggle_in = toggle;
  }
  else
  {
    hhcd.ep[pipe].toggle_out = toggle;
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

  if (hhcd.ep[pipe].ep_is_in)
  {
    toggle = hhcd.ep[pipe].toggle_in;
  }
  else
  {
    toggle = hhcd.ep[pipe].toggle_out;
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
#endif /* USB_OTG_FS_CORE */

/************************ (C) COPYRIGHT FMD *****END OF FILE****/
