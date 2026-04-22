/**
  ******************************************************************************
  * @file    usb_it_ft32f4.c
  * @author  FMD
  * @brief   This file provides USB and TIM7 interrupt handlers for FT32F4.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"

#ifdef USE_VCP

#include "usb_conf.h"
#include "usbd_def.h"
#include "usbd_core.h"
#include "usbd_cdc_if.h"

#ifdef USB_OTG_FS_CORE
extern PCD_FS_HandleTypeDef hpcd;
#endif
#ifdef USB_OTG_HS_CORE
extern PCD_HS_HandleTypeDef hpcd;
#endif

/* External CDC functions declared in vcpf4/usbd_cdc_vcp.c */
extern USBD_HandleTypeDef USBD_Device;
extern void TIM_PeriodElapsedCallback(TIM_TypeDef *tim);

#define TIMusb      TIM7
#define TIMx_IRQn   TIM7_IRQn

/**
  * @brief  This function handles USB OTG global interrupt.
  * @param  None
  * @retval None
  */
void OTG_IRQ_HANDLER(void)
{
    PCD_IRQHandler(&hpcd);
}

/**
  * @brief  This function handles TIM7 global interrupt.
  *         Used for CDC TX polling.
  * @param  None
  * @retval None
  */
void TIM7_IRQHandler(void)
{
    if (TIM_GetITStatus(TIMusb, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIMusb, TIM_IT_Update);
        TIM_PeriodElapsedCallback(TIMusb);
    }
}

#endif /* USE_VCP */
