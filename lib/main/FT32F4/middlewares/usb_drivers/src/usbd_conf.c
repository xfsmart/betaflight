/**
  ******************************************************************************
  * @file    			usbd_conf.c
  * @author  			FMD XA
  * @brief   			This file porvides all the USBD core function.
  * @version 			V1.0.0           
  * @data		 			2025-08-08
  ******************************************************************************
  */
#ifdef USB_OTG_HS_COER
/* Includes ------------------------------------------------------------------*/
#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_def.h"
#ifdef  USE_USBD_COMPOSITE
#include "usbd_composite_builder.h"
#endif  /* USE_USBD_COMPOSITE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define CURSOR_STEP     5
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
PCD_HS_HandleTypeDef hpcd;
uint8_t HID_Buffer[4];
__IO uint32_t remotewakeupon = 0;
extern USBD_HandleTypeDef USBD_Device;
void GetPointerData(uint8_t *pbuf);
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
                       PCD BSP Routines
*******************************************************************************/

/**
  * @brief  Initializes the PCD MSP.
  * @param  hpcd: PCD handle
  * @retval None
  */
void PCD_HS_MspInit(PCD_HS_HandleTypeDef *hpcd)
{

}

/**
  * @brief  De-Initializes the PCD MSP.
  * @param  hpcd: PCD handle
  * @retval None
  */
void PCD_HS_MspDeInit(PCD_HS_HandleTypeDef *hpcd)
{
  
}

/*******************************************************************************
                       LL Driver Callbacks (PCD -> USB Device Library)
*******************************************************************************/

/**
  * @brief  SetupStage callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void PCD_HS_SetupStageCallback(PCD_HS_HandleTypeDef *hpcd)
{
  USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup);
}

/**
  * @brief  DataOut Stage callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number
  * @retval None
  */
void PCD_HS_DataOutStageCallback(PCD_HS_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

/**
  * @brief  DataIn Stage callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number
  * @retval None
  */
void PCD_HS_DataInStageCallback(PCD_HS_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

/**
  * @brief  SOF callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void PCD_HS_SOFCallback(PCD_HS_HandleTypeDef *hpcd)
{
  USBD_LL_SOF(hpcd->pData);
}

/**
  * @brief  Reset callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void PCD_HS_ResetCallback(PCD_HS_HandleTypeDef *hpcd)
{
  USBD_SpeedTypeDef speed = USBD_SPEED_FULL;

  /* Set USB Current Speed */
  switch(hpcd->Init.speed)
  {
  case PCD_SPEED_HIGH:
    speed = USBD_SPEED_HIGH;
    break;

  case PCD_SPEED_FULL:
    speed = USBD_SPEED_FULL;
    break;

  default:
    speed = USBD_SPEED_FULL;
    break;
  }

  /* Reset Device */
  USBD_LL_Reset(hpcd->pData);

  USBD_LL_SetSpeed(hpcd->pData, speed);
}

/**
  * @brief  Suspend callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void PCD_HS_SuspendCallback(PCD_HS_HandleTypeDef *hpcd)
{
    USBD_LL_Suspend(hpcd->pData);
    __PCD_HS_GATE_PHYCLOCK();

    /* Enter in STOP mode */
    if (hpcd->Init.low_power_enable)
    {
      /* Set SLEEPDEEP bit and SleepOnExit of Cortex System Control Register */
      /* SLEEPDEEP bit2           SKEEPONEXIT bit1 */
      SCB->SCR |= (uint32_t)((uint32_t)(0x00000004 | 0x00000002));
    }
}

/**
  * @brief  Resume callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void PCD_HS_ResumeCallback(PCD_HS_HandleTypeDef *hpcd)
{
  if ((hpcd->Init.low_power_enable)&&(remotewakeupon == 0))
  {
    SystemClockConfig_STOP();

    /* Reset SLEEPDEEP bit of Cortex System Control Register */
    /* SLEEPDEEP bit2           SKEEPONEXIT bit1 */
    SCB->SCR &= (uint32_t)~((uint32_t)(0x00000004 | 0x00000002));
  }

  __PCD_HS_UNGATE_PHYCLOCK();
  USBD_LL_Resume(hpcd->pData);
  remotewakeupon = 0;
}

/**
  * @brief  ISOOUTIncomplete callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number
  * @retval None
  */
void PCD_HS_ISOOUTIncompleteCallback(PCD_HS_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_IsoOUTIncomplete(hpcd->pData, epnum);
}

/**
  * @brief  ISOINIncomplete callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number
  * @retval None
  */
void PCD_HS_ISOINIncompleteCallback(PCD_HS_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_IsoINIncomplete(hpcd->pData, epnum);
}

/**
  * @brief  ConnectCallback callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void PCD_HS_ConnectCallback(PCD_HS_HandleTypeDef *hpcd)
{
  USBD_LL_DevConnected(hpcd->pData);
}

/**
  * @brief  Disconnect callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void PCD_HS_DisconnectCallback(PCD_HS_HandleTypeDef *hpcd)
{
  USBD_LL_DevDisconnected(hpcd->pData);
}

/**
  * @brief  Initializes the Low Level portion of the Device driver.
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
  /* Set LL Driver parameters */
  hpcd.Init.dev_endpoints = 6;
  hpcd.Init.use_dedicated_ep1 = 0;

  /* Be aware that enabling DMA mode will result in data being sent only by
  multiple of 4 packet sizes. This is due to the fact that USB DMA does
  not allow sending data from non word-aligned addresses.
  For this specific application, it is advised to not enable this option
  unless required. */
  hpcd.Init.dma_enable = 0;
  hpcd.Init.low_power_enable = 1;
  hpcd.Init.speed = PCD_SPEED_HIGH;

  /* Link The driver to the stack */
  hpcd.pData = pdev;
  pdev->pData = &hpcd;

  /* Initialize LL Driver */
  PCD_HS_Init(&hpcd);

  PCDEx_HS_SetRxFiFo(0x200);
  PCDEx_HS_SetTxFiFo(0, 0x40);
  PCDEx_HS_SetTxFiFo(1, 0x100);

  return USBD_OK;
}

/**
  * @brief  De-Initializes the Low Level portion of the Device driver.
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
  PCD_HS_DeInit(pdev->pData);
  return USBD_OK;
}

/**
  * @brief  Starts the Low Level portion of the Device driver.
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
  PCD_HS_Start(pdev->pData);
  return USBD_OK;
}

/**
  * @brief  Stops the Low Level portion of the Device driver.
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
  PCD_HS_Stop(pdev->pData);
  return USBD_OK;
}

/**
  * @brief  Opens an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  ep_type: Endpoint Type
  * @param  ep_mps: Endpoint Max Packet Size
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev,
                                  uint8_t ep_addr,
                                  uint8_t ep_type,
                                  uint16_t ep_mps)
{
  PCD_HS_EP_Open(pdev->pData,
                  ep_addr,
                  ep_mps,
                  ep_type);

  return USBD_OK;
}

/**
  * @brief  Closes an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_HS_EP_Close(pdev->pData, ep_addr);
  return USBD_OK;
}

/**
  * @brief  Flushes an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_HS_EP_Flush(pdev->pData, ep_addr);
  return USBD_OK;
}

/**
  * @brief  Sets a Stall condition on an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_HS_EP_SetStall(pdev->pData, ep_addr);
  return USBD_OK;
}

/**
  * @brief  Clears a Stall condition on an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_HS_EP_ClrStall(pdev->pData, ep_addr);
  return USBD_OK;
}

/**
  * @brief  Returns Stall condition.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval Stall (1: Yes, 0: No)
  */
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_HS_HandleTypeDef *hpcd = pdev->pData;

  if((ep_addr & 0x80) == 0x80)
  {
    return hpcd->IN_ep[ep_addr & 0x7F].is_stall;
  }
  else
  {
    return hpcd->OUT_ep[ep_addr & 0x7F].is_stall;
  }
}

/**
  * @brief  Assigns a USB address to the device.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
  PCD_HS_SetAddress(pdev->pData, dev_addr);
  return USBD_OK;
}

/**
  * @brief  Transmits data over an endpoint.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  pbuf: Pointer to data to be sent
  * @param  size: Data size
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev,
                                    uint8_t ep_addr,
                                    uint8_t *pbuf,
                                    uint32_t size)
{
  PCD_HS_EP_Transmit(pdev->pData, ep_addr, pbuf, size);
  return USBD_OK;
}

/**
  * @brief  Prepares an endpoint for reception.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  pbuf: Pointer to data to be received
  * @param  size: Data size
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev,
                                          uint8_t ep_addr,
                                          uint8_t *pbuf,
                                          uint32_t size)
{
  PCD_HS_EP_Receive(pdev->pData, ep_addr, pbuf, size);
  return USBD_OK;
}

/**
  * @brief  Returns the last transferred packet size.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval Received Data Size
  */
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return PCD_HS_EP_GetRxCount(pdev->pData, ep_addr);
}

/**
  * @brief  Configures system clock after wakeup from STOP mode.
  * @param  None
  * @retval None
  */
void SystemClockConfig_STOP(void)
{
  /* Enable HSE Oscillator and activate PLL with HSE as source */

  RCC_HSEConfig(RCC_HSE_ON);  // HSE 12MHz to USB_HS PHY
  RCC_WaitForHSEStartUp();
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_USBOTGHS, ENABLE);

  // 200MHz
  RCC_PLLVcoOutputConfig(RCC_PLLSource_HSI, 0x19U, 0x01U);
  RCC_PLLCmd(RCC_SEL_PLL, ENABLE);
  RCC_PLLPOutputConfig(RCC_PLLP_ON, 0x0U);
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
  RCC_HCLKConfig(RCC_SYSCLK_DIV1);
  RCC_PCLK1Config(RCC_HCLK_DIV4);
  RCC_PCLK2Config(RCC_HCLK_DIV2);

  FLASH_SetLatency(FLASH_Latency_6);
}

/**
  * @brief  GPIO EXTI Callback function
  *         Handle remote-wakeup through Tamper button
  * @param  GPIO_Pin
  * @retval None
  */
void GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_Pin_0)
  {
    if ((((USBD_HandleTypeDef *)hpcd.pData)->dev_remote_wakeup == 1)&&
        (((USBD_HandleTypeDef *)hpcd.pData)->dev_state == USBD_STATE_SUSPENDED))
    {
      if ((&hpcd)->Init.low_power_enable)
      {
        /* Reset SLEEPDEEP bit of Cortex System Control Register */
        /* SLEEPDEEP bit2           SKEEPONEXIT bit1 */
        SCB->SCR &= (uint32_t)~((uint32_t)(0x00000004 | 0x00000002));
        SystemClockConfig_STOP();
      }

      /* Ungate PHY clock */
      __PCD_HS_UNGATE_PHYCLOCK();

      /* Activate Remote wakeup */
      PCD_HS_ActivateRemoteWakeup();

      /* Remote wakeup delay */
      USBD_Delay(10);

      /* Disable Remote wakeup */
      PCD_HS_DeActivateRemoteWakeup();

      /* change state to configured */
      ((USBD_HandleTypeDef *)hpcd.pData)->dev_state = USBD_STATE_CONFIGURED;

      /* Change remote_wakeup feature to 0*/
      ((USBD_HandleTypeDef *)hpcd.pData)->dev_remote_wakeup=0;
      remotewakeupon = 1;
    }
    else
    {
      GetPointerData(HID_Buffer);
      USBD_HID_SendReport(&USBD_Device, HID_Buffer, 4);
    }
  }
}

/**
  * @brief  Gets Pointer Data.
  * @param  pbuf: Pointer to report
  * @retval None
  */
void GetPointerData(uint8_t *pbuf)
{
  static int8_t cnt = 0;
  int8_t  x = 0, y = 0 ;

  if(cnt++ > 0)
  {
    x = CURSOR_STEP;
  }
  else
  {
    x = -CURSOR_STEP;
  }
  pbuf[0] = 0;
  pbuf[1] = x;
  pbuf[2] = y;
  pbuf[3] = 0;
}

/**
  * @}
  */

/**
  * @}
  */
#endif // USB_OTG_HS_COER

/************************ (C) COPYRIGHT FMD *****END OF FILE****/
