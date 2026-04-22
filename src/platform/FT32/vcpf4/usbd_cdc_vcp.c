/**
 ******************************************************************************
 * @file    usbd_cdc_vcp.c
 * @author  FMD
 * @version V1.0.0
 * @date    2026-03-31
 * @brief   Generic media access Layer.
 ******************************************************************************
**/
/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>

#include "platform.h"

#include "build/atomic.h"

#include "usb_conf.h"
#include "usbd_cdc_if.h"
#include "usbd_conf.h"
#include <stdint.h>
#include "usbd_core.h"

#include "drivers/nvic.h"
#include "drivers/time.h"
#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
#pragma     data_alignment = 4
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */

#define APP_RX_DATA_SIZE  2048
#define APP_TX_DATA_SIZE  2048

#define APP_TX_BLOCK_SIZE 512
#define CDC_POLLING_INTERVAL             5 /* in ms. The max is 65 and the min is 1 */


#define TIMusb                           TIM7

#define TIMx_IRQn                        TIM7_IRQn
#define TIMx_IRQHandler                  TIM7_IRQHandler

/* USB device handle */
USBD_HandleTypeDef   USBD_Device;

uint32_t CDC_Send_DATA(const uint8_t *ptrBuffer, uint32_t sendLength);
uint32_t CDC_Send_FreeBytes(void);
uint32_t CDC_Receive_DATA(uint8_t* recvBuf, uint32_t len);
uint32_t CDC_Receive_BytesAvailable(void);
uint8_t usbIsConfigured(void);
uint8_t usbIsConnected(void);
uint32_t CDC_BaudRate(void);
void CDC_SetCtrlLineStateCb(void (*cb)(void *context, uint16_t ctrlLineState), void *context);
void CDC_SetBaudRateCb(void (*cb)(void *context, uint32_t baud), void *context);


volatile uint8_t UserRxBuffer[APP_RX_DATA_SIZE];/* Received Data over USB are stored in this buffer */
volatile uint8_t UserTxBuffer[APP_TX_DATA_SIZE];/* Received Data over UART (CDC interface) are stored in this buffer */
uint32_t BuffLength;
volatile uint32_t UserTxBufPtrIn = 0;/* Increment this pointer or roll it back to
                               start address when data are received over USART */
volatile uint32_t UserTxBufPtrOut = 0; /* Increment this pointer or roll it back to
                                 start address when data are sent over USB */

uint32_t rxAvailable = 0;
uint8_t* rxBuffPtr = NULL;

static void (*ctrlLineStateCb)(void *context, uint16_t ctrlLineState);
static void *ctrlLineStateCbContext;
static void (*baudRateCb)(void *context, uint32_t baud);
static void *baudRateCbContext;

/* CDC Line Code Information */
USBD_CDC_LineCodingTypeDef LineCoding =
{
  115200, /* baud rate*/
  0x00,   /* stop bits-1*/
  0x00,   /* parity - none*/
  0x08    /* nb. of bits 8*/
};

typedef struct __attribute__ ((packed))
{
  uint32_t bitrate;
  uint8_t  format;
  uint8_t  paritytype;
  uint8_t  datatype;
} LINE_CODING;

/* Private function prototypes -----------------------------------------------*/
static int8_t CDC_Itf_Init(void);
static int8_t CDC_Itf_DeInit(void);
static int8_t CDC_Itf_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Itf_Receive(uint8_t* pbuf, uint32_t *Len);
static int8_t CDC_Itf_TransmitCplt(uint8_t *Buf, uint32_t *Len, uint8_t epnum);


static void TIM_Config(void);
static void Error_Handler(void);
void TIM_PeriodElapsedCallback(TIM_TypeDef *tim);

USBD_CDC_ItfTypeDef USBD_CDC_fops =
{
  CDC_Itf_Init,
  CDC_Itf_DeInit,
  CDC_Itf_Control,
  CDC_Itf_Receive,
  CDC_Itf_TransmitCplt
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  CDC_Itf_Init
  *         Initializes the CDC media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Init(void)
{
  /*##-3- Configure the TIM Base generation  #################################*/
  TIM_Config();

  /*##-4- Start the TIM Base generation in interrupt mode ####################*/
  TIM_ITConfig(TIMusb, TIM_IT_Update, ENABLE);
  TIM_Cmd(TIMusb, ENABLE);

  /*##-5- Set Application Buffers ############################################*/
  USBD_CDC_SetTxBuffer(&USBD_Device, (uint8_t *)UserTxBuffer, 0);
  USBD_CDC_SetRxBuffer(&USBD_Device, (uint8_t *)UserRxBuffer);

  ctrlLineStateCb = NULL;
  baudRateCb = NULL;

  return (USBD_OK);
}

/**
  * @brief  CDC_Itf_DeInit
  *         DeInitializes the CDC media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_DeInit(void)
{

  return (USBD_OK);
}

/**
  * @brief  CDC_Itf_Control
  *         Manage the CDC class requests
  * @param  Cmd: Command code
  * @param  Buf: Buffer containing command data (request parameters)
  * @param  Len: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Control (uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  LINE_CODING* plc = (LINE_CODING*)pbuf;

  switch (cmd)
  {
  case CDC_SEND_ENCAPSULATED_COMMAND:
    /* Add your code here */
    break;

  case CDC_GET_ENCAPSULATED_RESPONSE:
    /* Add your code here */
    break;

  case CDC_SET_COMM_FEATURE:
    /* Add your code here */
    break;

  case CDC_GET_COMM_FEATURE:
    /* Add your code here */
    break;

  case CDC_CLEAR_COMM_FEATURE:
    /* Add your code here */
    break;

  case CDC_SET_LINE_CODING:
    if (pbuf && (length == sizeof(*plc))) {
        LineCoding.bitrate    = plc->bitrate;
        LineCoding.format     = plc->format;
        LineCoding.paritytype = plc->paritytype;
        LineCoding.datatype   = plc->datatype;

        // If a callback is provided, tell the upper driver of changes in baud rate
        if (baudRateCb) {
            baudRateCb(baudRateCbContext, LineCoding.bitrate);
        }
    }

    break;

  case CDC_GET_LINE_CODING:
    if (pbuf && (length == sizeof(*plc))) {
        plc->bitrate = LineCoding.bitrate;
        plc->format = LineCoding.format;
        plc->paritytype = LineCoding.paritytype;
        plc->datatype = LineCoding.datatype;
    }
    break;

  case CDC_SET_CONTROL_LINE_STATE:
    // If a callback is provided, tell the upper driver of changes in DTR/RTS state
    if (pbuf && (length == sizeof(uint16_t))) {
         if (ctrlLineStateCb) {
             ctrlLineStateCb(ctrlLineStateCbContext, *((uint16_t *)pbuf));
         }
    }
    break;

  case CDC_SEND_BREAK:
     /* Add your code here */
    break;

  default:
    break;
  }

  return (USBD_OK);
}

/**
  * @brief  TIM period elapsed callback
  * @param  htim: TIM handle
  * @retval None
  */
void TIM_PeriodElapsedCallback(TIM_TypeDef *tim)
{
    if (tim != TIMusb) {
        return;
    }

    uint32_t buffsize;
    static uint32_t lastBuffsize = 0;

    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)USBD_Device.pClassData;

    if (hcdc->TxState == 0) {
        // endpoint has finished transmitting previous block
        if (lastBuffsize) {
            bool needZeroLengthPacket = lastBuffsize % 64 == 0;

            // move the ring buffer tail based on the previous succesful transmission
            UserTxBufPtrOut += lastBuffsize;
            if (UserTxBufPtrOut == APP_TX_DATA_SIZE) {
                UserTxBufPtrOut = 0;
            }
            lastBuffsize = 0;

            if (needZeroLengthPacket) {
                USBD_CDC_SetTxBuffer(&USBD_Device, (uint8_t*)&UserTxBuffer[UserTxBufPtrOut], 0);
                return;
            }
        }
        if (UserTxBufPtrOut != UserTxBufPtrIn) {
            if (UserTxBufPtrOut > UserTxBufPtrIn) { /* Roll-back */
                buffsize = APP_TX_DATA_SIZE - UserTxBufPtrOut;
            } else {
                buffsize = UserTxBufPtrIn - UserTxBufPtrOut;
            }
            if (buffsize > APP_TX_BLOCK_SIZE) {
                buffsize = APP_TX_BLOCK_SIZE;
            }

            USBD_CDC_SetTxBuffer(&USBD_Device, (uint8_t*)&UserTxBuffer[UserTxBufPtrOut], buffsize);

            if (USBD_CDC_TransmitPacket(&USBD_Device) == USBD_OK) {
                lastBuffsize = buffsize;
            }
        }
    }
}

/**
  * @brief  CDC_Itf_DataRx
  *         Data received over USB OUT endpoint are sent over CDC interface
  *         through this function.
  * @param  Buf: Buffer of data to be transmitted
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Itf_Receive(uint8_t* Buf, uint32_t *Len)
{
    rxAvailable = *Len;
    rxBuffPtr = Buf;
    if (!rxAvailable) {
        // Received an empty packet, trigger receiving the next packet.
        // This will happen after a packet that's exactly 64 bytes is received.
        // The USB protocol requires that an empty (0 byte) packet immediately follow.
        USBD_CDC_ReceivePacket(&USBD_Device);
    }
    return (USBD_OK);
}


static int8_t CDC_Itf_TransmitCplt(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    UNUSED(Buf);
    UNUSED(Len);
    UNUSED(epnum);

    return (USBD_OK);
}


/**
  * @brief  TIM_Config: Configure TIMusb timer
  * @param  None.
  * @retval None
  */
static void TIM_Config(void)
{
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

  /* Enable TIM7 clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);

  /* Initialize TIMx peripheral as follow:
       + Period = CDC_POLLING_INTERVAL*1000 - 1 (5ms)
       + Prescaler = (SystemCoreClock / 2 / 1000000) - 1
       + ClockDivision = 0
       + Counter direction = Up
  */
  TIM_TimeBaseStructure.TIM_Period = (CDC_POLLING_INTERVAL * 1000) - 1;
  TIM_TimeBaseStructure.TIM_Prescaler = (SystemCoreClock / 2 / (1000000)) - 1;
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIMusb, &TIM_TimeBaseStructure);

  /* Configure the NVIC for TIMx */
  NVIC_SetPriority(TIMx_IRQn, NVIC_BUILD_PRIORITY(6, 0));

  /* Enable the TIMx global Interrupt */
  NVIC_EnableIRQ(TIMx_IRQn);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void) __attribute__((unused));
static void Error_Handler(void)
{
  /*  */
}


uint32_t CDC_Send_FreeBytes(void)
{
    uint32_t freeBytes;

    ATOMIC_BLOCK(NVIC_BUILD_PRIORITY(6, 0)) {
        freeBytes = ((UserTxBufPtrOut - UserTxBufPtrIn) + (-((int)(UserTxBufPtrOut <= UserTxBufPtrIn)) & APP_TX_DATA_SIZE)) - 1;
    }

    return freeBytes;
}

uint32_t CDC_Send_DATA(const uint8_t *ptrBuffer, uint32_t sendLength)
{
    for (uint32_t i = 0; i < sendLength; i++) {
        while (CDC_Send_FreeBytes() == 0) {
            // block until there is free space in the ring buffer
            delay(1);
        }
        ATOMIC_BLOCK(NVIC_BUILD_PRIORITY(6, 0)) { // Paranoia
            UserTxBuffer[UserTxBufPtrIn] = ptrBuffer[i];
            UserTxBufPtrIn = (UserTxBufPtrIn + 1) % APP_TX_DATA_SIZE;
        }
    }
    return sendLength;
}

uint32_t CDC_Receive_DATA(uint8_t* recvBuf, uint32_t len)
{
    uint32_t count = 0;
    if ( (rxBuffPtr != NULL))
    {
        while ((rxAvailable > 0) && count < len)
        {
            recvBuf[count] = rxBuffPtr[0];
            rxBuffPtr++;
            rxAvailable--;
            count++;
            if (rxAvailable < 1)
            {
                USBD_CDC_ReceivePacket(&USBD_Device);
            }
        }
    }
    return count;
}

uint32_t CDC_Receive_BytesAvailable(void)
{
    return rxAvailable;
}

uint8_t usbIsConfigured(void)
{
    return (USBD_Device.dev_state == USBD_STATE_CONFIGURED);
}

uint8_t usbIsConnected(void)
{
    return (USBD_Device.dev_state != USBD_STATE_DEFAULT);
}

uint32_t CDC_BaudRate(void)
{
    return LineCoding.bitrate;
}

void CDC_SetBaudRateCb(void (*cb)(void *context, uint32_t baud), void *context)
{
    baudRateCbContext = context;
    baudRateCb = cb;
}

void CDC_SetCtrlLineStateCb(void (*cb)(void *context, uint16_t ctrlLineState), void *context)
{
    ctrlLineStateCbContext = context;
    ctrlLineStateCb = cb;
}