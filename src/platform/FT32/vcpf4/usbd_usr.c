/**
  ******************************************************************************
  * @file    usbd_usr.c
  * @author  FMD
  * @brief   This file provides the user application layer for USB on FT32F4.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"

#ifdef USE_VCP

#include "usbd_usr.h"
#include "usbd_ioreq.h"

/**
  * @}
  */

/** @defgroup USBD_USR_Private_FunctionPrototypes
  * @{
  */
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  USBD_USR_Init
  *         Displays the message on LCD for host lib initialization
  * @param  None
  * @retval None
  */
void USBD_USR_Init(void)
{
}

/**
  * @brief  USBD_USR_DeviceReset
  *         Displays the message on LCD on device Reset Event
  * @param  speed : device speed
  * @retval None
  */
void USBD_USR_DeviceReset(uint8_t speed)
{
    (void)speed;
}

/**
  * @brief  USBD_USR_DeviceConfigured
  *         Displays the message on LCD on device configuration Event
  * @param  None
  * @retval Status
  */
void USBD_USR_DeviceConfigured(void)
{
}

/**
  * @brief  USBD_USR_DeviceConnected
  *         Displays the message on LCD on device connection Event
  * @param  None
  * @retval Status
  */
void USBD_USR_DeviceConnected(void)
{
}

/**
  * @brief  USBD_USR_DeviceDisconnected
  *         Displays the message on LCD on device disconnection Event
  * @param  None
  * @retval Status
  */
void USBD_USR_DeviceDisconnected(void)
{
}

/**
  * @brief  USBD_USR_DeviceSuspended
  *         Displays the message on LCD on device suspend Event
  * @param  None
  * @retval None
  */
void USBD_USR_DeviceSuspended(void)
{
}

/**
  * @brief  USBD_USR_DeviceResumed
  *         Displays the message on LCD on device resume Event
  * @param  None
  * @retval None
  */
void USBD_USR_DeviceResumed(void)
{
}

/**
  * @brief  USBD device user callback structure
  */
USBD_Usr_cb_TypeDef USR_cb =
{
    USBD_USR_Init,
    USBD_USR_DeviceReset,
    USBD_USR_DeviceConfigured,
    USBD_USR_DeviceSuspended,
    USBD_USR_DeviceResumed,
    USBD_USR_DeviceConnected,
    USBD_USR_DeviceDisconnected,
};

#endif /* USE_VCP */

/************************ (C) COPYRIGHT FMD *****END OF FILE****/
