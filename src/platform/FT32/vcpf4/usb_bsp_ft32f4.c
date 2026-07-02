/**
  ******************************************************************************
  * @file    usb_bsp_ft32f4.c
  * @author  FMD
  * @brief   This file provides board support package for USB on FT32F4.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"

#ifdef USE_VCP

#include "usb_conf.h"
#include "drivers/nvic.h"

/**
  * @brief  USB_OTG_BSP_Init
  *         Initializes USB BSP (clocks and NVIC)
  * @param  None
  * @retval None
  */
void USB_OTG_BSP_Init(void)
{
#ifdef USB_OTG_HS_CORE
    RCC_HSEConfig(RCC_HSE_ON);
    RCC_WaitForHSEStartUp();
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_USBOTGHS, ENABLE);
#elif defined(USB_OTG_FS_CORE)
    // FT32: HSI48 as USB clock source
    RCC_HSI48Cmd(ENABLE);
    RCC_WaitForHSI48StartUp();
    RCC_48MCLKConfig(RCC_48MCLK_HSI48);
    RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_USBOTGFS, ENABLE);
#endif
}

/**
  * @brief  USB_OTG_BSP_EnableInterrupt
  *         Enable USB global interrupt
  * @param  None
  * @retval None
  */
void USB_OTG_BSP_EnableInterrupt(void)
{
    NVIC_SetPriority(OTG_IRQ, NVIC_PRIO_USB);
    NVIC_EnableIRQ(OTG_IRQ);
}

#endif /* USE_VCP */
