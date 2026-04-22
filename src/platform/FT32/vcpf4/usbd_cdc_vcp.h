/**
  ******************************************************************************
  * @file    usbd_cdc_vcp.h
  * @author  FMD
  * @brief   Header file for usbd_cdc_vcp.c on FT32F4.
  *          Equivalent to STM32 vcpf4/usbd_cdc_vcp.h
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_CDC_VCP_H
#define __USBD_CDC_VCP_H

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc.h"
#include "usbd_conf.h"
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/
typedef struct __attribute__((packed))
{
    uint32_t bitrate;
    uint8_t  format;
    uint8_t  paritytype;
    uint8_t  datatype;
} LINE_CODING;

/* Exported functions --------------------------------------------------------*/
uint32_t CDC_Send_DATA   (const uint8_t *ptrBuffer, uint32_t sendLength);
uint32_t CDC_Send_FreeBytes(void);
uint32_t CDC_Receive_DATA(uint8_t *recvBuf, uint32_t len);
uint32_t CDC_Receive_BytesAvailable(void);

uint8_t  usbIsConfigured(void);
uint8_t  usbIsConnected(void);
uint32_t CDC_BaudRate(void);

void CDC_SetCtrlLineStateCb(void (*cb)(void *context, uint16_t ctrlLineState), void *context);
void CDC_SetBaudRateCb(void (*cb)(void *context, uint32_t baud), void *context);

#endif /* __USBD_CDC_VCP_H */

/************************ (C) COPYRIGHT FMD *****END OF FILE****/
