/**
  ******************************************************************************
  * @file    			usbd_conf_fs.c
  * @author  			FMD XA
  * @brief   			This file porvides all the USBD core function.
  * @version 			V1.0.0           
  * @data		 			2025-04-17
  ******************************************************************************
  */
#ifdef USB_OTG_FS_CORE 
/* Includes ------------------------------------------------------------------*/
#include "usbd_core.h"
#ifdef  USE_USBD_COMPOSITE
#include "usbd_composite_builder.h"
#endif  /* USE_USBD_COMPOSITE */

/* Private typedef ----------------------------------------------------------*/
/* Private define -----------------------------------------------------------*/
#define CURSOR_STEP

/* Private macro ------------------------------------------------------------*/
/* Private variables --------------------------------------------------------*/
PCD_FS_HandleTypeDef hpcd;
__IO uint32_t remotewakeupon = 0;
extern USBD_HandleTypeDef USBD_Device;

/* Private function prototypes ----------------------------------------------*/
/* Private functions --------------------------------------------------------*/

/**
 * @brief  PCD_FS_MspInit
 *         Initialize the PCD MSP
 * @param  hpcd: PCD Handle
 * @retval None
 */
void PCD_FS_MspInit(PCD_FS_HandleTypeDef *hpcd)
{
    UNUSED(hpcd);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /**USB GPIO Configuration
    PA11     ------> USB_DM
    PA12     ------> USB_DP
    */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_10);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, GPIO_AF_10);
    
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
 * @brief  PCD_FS_SetupStageCallback
 *         SetupStage Callback
 * @param  hpcd: PCD Handle
 * @retval None
 */
void  PCD_FS_SetupStageCallback(PCD_FS_HandleTypeDef *hpcd)
{
  USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup);
}

/**
 * @brief  PCD_FS_DataOutStageCallback
 *         DataOut Stage Callback
 * @param  hpcd: PCD Handle
 * @param  epnum: Endpoint Number
 * @retval None
 */
void PCD_FS_DataOutStageCallback(PCD_FS_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

/**
 * @brief  PCD_FS_DataInStageCallback
 *         DataIn Stage Callback
 * @param  hpcd: PCD Handle
 * @param  epnum: Endpoint Number
 * @retval None
 */
void PCD_FS_DataInStageCallback(PCD_FS_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

/**
 * @brief  PCD_FS_SOFCallback
 *         SOF Callback
 * @param  hpcd: PCD Handle
 * @retval None
 */
void PCD_FS_SOFCallback(PCD_FS_HandleTypeDef *hpcd)
{
  USBD_LL_SOF(hpcd->pData);
}

/**
 * @brief  PCD_FS_ResetCallback
 *         Reset Callback
 * @param  hpcd: PCD Handle
 * @retval None
 */
void PCD_FS_ResetCallback(PCD_FS_HandleTypeDef *hpcd)
{
  USBD_SpeedTypeDef speed = USBD_SPEED_FULL;
  /* Reset Device */
  USBD_LL_Reset(hpcd->pData);

  USBD_LL_SetSpeed(hpcd->pData, speed);
}

/**
 * @brief  PCD_FS_SuspendCallback
 *         Suspend Callback
 * @param  hpcd: PCD Handle
 * @retval None
 */
void PCD_FS_SuspendCallback(PCD_FS_HandleTypeDef *hpcd)
{
  USBD_LL_Suspend(hpcd->pData);

  /* enter in STOP mode */
//  if (hpcd->Init.low_power_mode)
//  {
//    /* set SLEEPDEEP bit and sleeponexit of cortex system control register */
//    SCB->SCR |= (uint32_t)((uint32_t)(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));
//  }
}

/**
 * @brief  PCD_FS_ResumeCallback
 *         Resume Callback
 * @param  hpcd: PCD Handle
 * @retval None
 */
void PCD_FS_ResumeCallback(PCD_FS_HandleTypeDef *hpcd)
{
//  if ((hpcd->Init.low_power_mode) && (remotewakeupon == 0U))
//  {
//    /* reset SLEEPDEEP bit and sleeponexit of cortex system control register */
//    SCB->SCR &= (uint32_t)~((uint32_t)(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));
//  }

  USBD_LL_Resume(hpcd->pData);
  remotewakeupon = 0;
}

/**
 * @brief  PCD_FS_ConnectCallback
 *         Connect Callback
 * @param  hpcd: PCD Handle
 * @retval None
 */
void PCD_FS_ConnectCallback(PCD_FS_HandleTypeDef *hpcd)
{
  USBD_LL_DevConnected(hpcd->pData);
}

/**
 * @brief  PCD_FS_DisconnectCallback
 *         Disconnect Callback
 * @param  hpcd: PCD Handle
 * @retval None
 */
void PCD_FS_DisconnectCallback(PCD_FS_HandleTypeDef *hpcd)
{
  USBD_LL_DevDisconnected(hpcd->pData);
}

/**
 * @brief  USBD_LL_Init
 *         Initialize the device driver.
 * @param  pdev: device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef  USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
  uint32_t i;
  hpcd.Init.endpoints = 4;
  hpcd.Init.speed = PCD_SPEED_FULL;
  hpcd.Init.ep0_mps = 64;

  hpcd.pData = pdev;
  pdev->pData = &hpcd;

  PCD_FS_Init(&hpcd);
  for(i = 1; i < hpcd.Init.endpoints; i++)
  {
    if (i == 2)
    {
      PCD_FS_SetRxFiFo(i, 0x200U, 1U);
      PCD_FS_SetTxFiFo(i, 0x200U, 1U);

    }
    else
    {
      PCD_FS_SetRxFiFo(i, 0x40U, 0U);
      PCD_FS_SetTxFiFo(i, 0x40U, 0U);
    }
  }
  
  /* Init endpoints structures */
  for (i = 1U; i < hpcd.Init.endpoints; i++)
  {
    /* Init ep structure */
    hpcd.IN_ep[i].is_in = 1U;
    hpcd.IN_ep[i].num = i;
    hpcd.IN_ep[i].tx_fifo_num = i;
    /* Control until ep is activated */
    hpcd.IN_ep[i].maxpacket = 64U;
    hpcd.IN_ep[i].xfer_len = 64U;
  }

  return USBD_OK;
}


/**
 * @brief  USBD_LL_DeInit
 *         De-Initialize the device driver.
 * @param  pdev: device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef  USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
  PCD_FS_DeInit(pdev->pData);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_Start
 *         Start the device driver
 * @param  pdev: device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef  USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
  PCD_FS_Start(pdev->pData);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_Stop
 *         Stop the device driver
 * @param  pdev: device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef  USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
  PCD_FS_Stop(pdev->pData);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_OpenEP
 *         opens an endpoint of the device driver
 * @param  pdev: device handle
 * @param  ep_addr: endpoint number
 * @param  ep_type: endpoint type
 * @param  ep_mps: endpoint max packet size
 * @retval USBD Status
 */
USBD_StatusTypeDef  USBD_LL_OpenEP(USBD_HandleTypeDef *pdev,
                                   uint8_t ep_addr,
                                   uint8_t ep_type,
                                   uint16_t ep_mps)
{
  PCD_FS_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_CloseEP
 *         Close an endpoint of the device driver
 * @param  pdev: device handle
 * @param  ep_addr: endpoint number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev,
                                   uint8_t ep_addr)
{
  PCD_FS_EP_Close(pdev->pData, ep_addr);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_FlushEP
 *         Flush an endpoint of the device driver
 * @param  pdev: device handle
 * @param  ep_addr: endpoint number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev,
                                   uint8_t ep_addr)
{
  PCD_FS_EP_Flush(pdev->pData, ep_addr);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_StallEP
 *         set a stall condition on an endpoint of the device driver
 * @param  pdev: device handle
 * @param  ep_addr: endpoint number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev,
                                   uint8_t ep_addr)
{
  PCD_FS_EP_SetStall(pdev->pData, ep_addr);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_ClearStallEP
 *         Clear a stall condition on an endpoint of the device driver
 * @param  pdev: device handle
 * @param  ep_addr: endpoint number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev,
                                        uint8_t ep_addr)
{
  PCD_FS_EP_ClrStall(pdev->pData, ep_addr);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_IsStallEP
 *         Return stall condition
 * @param  pdev: device handle
 * @param  ep_addr: endpoint number
 * @retval stall condition
 */
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_FS_HandleTypeDef *hpcd = pdev->pData;

  if ((ep_addr & 0x80) == 0x80)
  {
    return hpcd->IN_ep[ep_addr & 0x7F].is_stall;
  }
  else
  {
    return hpcd->OUT_ep[ep_addr & 0x7F].is_stall;
  }
}

/**
 * @brief  USBD_LL_SetUSBAddress
 *         Assign a USB address to the device
 * @param  pdev: device handle
 * @param  dev_addr: device address
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
  PCD_FS_SetAddress(pdev->pData, dev_addr);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_Transmit
 *         Transmits data over an endpoint
 * @param  pdev: device handle
 * @param  ep_addr: endpoint number
 * @param  pbuf: pointer to data to be sent
 * @param  size: Data size
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                    uint8_t *pbuf, uint32_t size)
{
  PCD_FS_EP_Transmit(pdev->pData, ep_addr, pbuf, size);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_PrepareReceive
 *         Prepares an endpoint for reception
 * @param  pdev: device handle
 * @param  ep_addr: endpoint number
 * @param  pbuf: pointer to data to be received
 * @param  size: Data size
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                          uint8_t *pbuf, uint32_t size)
{
  PCD_FS_EP_Receive(pdev->pData, ep_addr, pbuf, size);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_GetRxDataSize
 *         Return the last transferred packet size
 * @param  pdev: device handle
 * @param  ep_addr: endpoint number
 * @retval Receive data size
 */
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return PCD_FS_EP_GetRxCount(pdev->pData, ep_addr);
}

#endif // USB_OTG_FS_CORE
/************************ (C) COPYRIGHT FMD *****END OF FILE****/
