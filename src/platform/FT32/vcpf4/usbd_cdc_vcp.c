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

/* Queue the largest contiguous ring block and keep it off a 64-byte multiple.
 * Smaller CDC chunks make Windows usbser expose its scheduling latency between
 * chunks; max-sized non-multiple blocks preserve the stream while avoiding the
 * FT32 CDC class ZLP path on every transfer boundary. */
#define APP_TX_BLOCK_SIZE (APP_TX_DATA_SIZE - 1U)
#define CDC_POLLING_INTERVAL             1 /* in ms. Matches USB FS SOF 1ms frame; max is 65, min is 1 */


#define TIMusb                           TIM7

#define TIMx_IRQn                        TIM7_IRQn
#define TIMx_IRQHandler                  TIM7_IRQHandler

/* USB device handle */
USBD_HandleTypeDef   USBD_Device;

/* CDC composite class id, defined in serial_usb_vcp.c. Zero for non-composite. */
extern uint8_t g_cdcClassId;

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
static uint8_t UsbRxPacketBuffer[CDC_DATA_OUT_PACKET_SIZE];
uint32_t BuffLength;
volatile uint32_t UserRxBufPtrIn = 0;
volatile uint32_t UserRxBufPtrOut = 0;
static volatile bool rxPending = false;
static volatile uint32_t rxPendingLength = 0U;
volatile uint32_t UserTxBufPtrIn = 0;/* Increment this pointer or roll it back to
	                               start address when data are received over USART */
volatile uint32_t UserTxBufPtrOut = 0; /* Increment this pointer or roll it back to
	                                 start address when data are sent over USB */

/* Pending TX length submitted to the CDC IN endpoint. Volatile and
 * file-scope so CDC_Itf_Init/CDC_Itf_DeInit can reset it from the
 * configuration context while the TIM7 callback reads it from the ISR. */
static volatile uint32_t lastBuffsize = 0U;

#define CDC_TX_ATOMIC_PRIORITY NVIC_PRIO_USB

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
static USBD_CDC_HandleTypeDef *CDC_GetHandle(void);
static void CDC_TxDrain(void);
static uint32_t CDC_RxBytesAvailable(void);
static uint32_t CDC_RxFreeBytes(void);
static bool CDC_RxCommitPacket(const uint8_t *buf, uint32_t length);
static void CDC_RxArmEndpoint(void);
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

  /* Keep the TIM update interrupt disabled and the counter stopped while the
   * transport state is reset, so the TIM7 ISR cannot race this configuration
   * path. The timer is started only after the buffers and ring state are in a
   * consistent, clean state. */
  TIM_ITConfig(TIMusb, TIM_IT_Update, DISABLE);
  TIM_Cmd(TIMusb, DISABLE);
  TIM_SetCounter(TIMusb, 0U);
  TIM_ClearITPendingBit(TIMusb, TIM_IT_Update);

  /* On bus reset/re-enumeration USBD_CDC re-invokes this Init callback. Drop
   * any in-flight TX data and reset the transport temporals so a stale
   * lastBuffsize cannot be applied to the new class instance and old ring
   * data is never resent. The application-registered baud/control-line
   * callbacks are upper-layer-owned and must survive the reset, so they are
   * intentionally NOT cleared here. */
  ATOMIC_BLOCK(CDC_TX_ATOMIC_PRIORITY) {
	    UserTxBufPtrIn = 0U;
	    UserTxBufPtrOut = 0U;
	    UserRxBufPtrIn = 0U;
	    UserRxBufPtrOut = 0U;
	    rxPending = false;
	    rxPendingLength = 0U;
	    lastBuffsize = 0U;
	  }

  /*##-5- Set Application Buffers ############################################*/
#ifdef USE_USBD_COMPOSITE
  USBD_CDC_SetTxBuffer(&USBD_Device, (uint8_t *)UserTxBuffer, 0, g_cdcClassId);
#else
  USBD_CDC_SetTxBuffer(&USBD_Device, (uint8_t *)UserTxBuffer, 0);
#endif
#ifdef USE_USBD_COMPOSITE
  USBD_CDC_SetRxBuffer(&USBD_Device, UsbRxPacketBuffer, g_cdcClassId);
#else
  USBD_CDC_SetRxBuffer(&USBD_Device, UsbRxPacketBuffer);
#endif

  /*##-4- Start the TIM Base generation in interrupt mode ####################*/
  TIM_ITConfig(TIMusb, TIM_IT_Update, ENABLE);
  TIM_Cmd(TIMusb, ENABLE);

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
  /* Stop the polling timer and disarm its update interrupt before touching the
   * transport state, so the ISR cannot process a stale transfer after the CDC
   * class instance has been released. */
  TIM_ITConfig(TIMusb, TIM_IT_Update, DISABLE);
  TIM_Cmd(TIMusb, DISABLE);
  TIM_ClearITPendingBit(TIMusb, TIM_IT_Update);

  ATOMIC_BLOCK(CDC_TX_ATOMIC_PRIORITY) {
	    UserTxBufPtrIn = 0U;
	    UserTxBufPtrOut = 0U;
	    UserRxBufPtrIn = 0U;
	    UserRxBufPtrOut = 0U;
	    rxPending = false;
	    rxPendingLength = 0U;
	    lastBuffsize = 0U;
	  }

  /* Application callbacks are upper-layer-owned; do not clear them. */

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
    if (pbuf && ctrlLineStateCb) {
        uint16_t ctrlLineState;
        if (length == sizeof(ctrlLineState)) {
            ctrlLineState = *((uint16_t *)pbuf);
        } else {
            ctrlLineState = ((USBD_SetupReqTypeDef *)pbuf)->wValue;
        }
        ctrlLineStateCb(ctrlLineStateCbContext, ctrlLineState);
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

    CDC_TxDrain();
}

static USBD_CDC_HandleTypeDef *CDC_GetHandle(void)
{
    USBD_CDC_HandleTypeDef *hcdc;
#ifdef USE_USBD_COMPOSITE
    if (g_cdcClassId >= USBD_MAX_SUPPORTED_CLASS) {
        return NULL;
    }
    hcdc = (USBD_CDC_HandleTypeDef*)USBD_Device.pClassDataCmsit[g_cdcClassId];
#else
    hcdc = (USBD_CDC_HandleTypeDef*)USBD_Device.pClassData;
#endif

    return hcdc;
}

static void CDC_TxDrain(void)
{
    uint32_t buffsize;
    USBD_CDC_HandleTypeDef *hcdc;

    ATOMIC_BLOCK(CDC_TX_ATOMIC_PRIORITY) {
        hcdc = CDC_GetHandle();
        if (!(hcdc && hcdc->TxState == 0U)) {
            return;
        }

        /* Endpoint has finished transmitting the previous block. */
        if (lastBuffsize) {
            UserTxBufPtrOut += lastBuffsize;
            if (UserTxBufPtrOut == APP_TX_DATA_SIZE) {
                UserTxBufPtrOut = 0;
            }
            lastBuffsize = 0;
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

#ifdef USE_USBD_COMPOSITE
            USBD_CDC_SetTxBuffer(&USBD_Device, (uint8_t*)&UserTxBuffer[UserTxBufPtrOut], buffsize, g_cdcClassId);

            if (USBD_CDC_TransmitPacket(&USBD_Device, g_cdcClassId) == USBD_OK) {
#else
            USBD_CDC_SetTxBuffer(&USBD_Device, (uint8_t*)&UserTxBuffer[UserTxBufPtrOut], buffsize);

            if (USBD_CDC_TransmitPacket(&USBD_Device) == USBD_OK) {
#endif
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
    const uint32_t length = *Len;
    bool armEndpoint = false;

    ATOMIC_BLOCK(CDC_TX_ATOMIC_PRIORITY) {
        if (CDC_RxCommitPacket(Buf, length)) {
            armEndpoint = true;
        } else {
            rxPending = true;
            rxPendingLength = length;
        }
    }

    if (armEndpoint) {
        CDC_RxArmEndpoint();
    }

    return (USBD_OK);
}


static int8_t CDC_Itf_TransmitCplt(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    UNUSED(Buf);
    UNUSED(Len);
    UNUSED(epnum);

    CDC_TxDrain();

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
       + Period = CDC_POLLING_INTERVAL*1000 - 1 (1ms)
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

    ATOMIC_BLOCK(CDC_TX_ATOMIC_PRIORITY) {
        freeBytes = ((UserTxBufPtrOut - UserTxBufPtrIn) + (-((int)(UserTxBufPtrOut <= UserTxBufPtrIn)) & APP_TX_DATA_SIZE)) - 1;
    }

    return freeBytes;
}

uint32_t CDC_Send_DATA(const uint8_t *ptrBuffer, uint32_t sendLength)
{
    uint32_t remaining = sendLength;

    /* Accept the complete frame into the TX ring before returning. The TIM7
     * ISR drains the ring to the USB IN endpoint, so a full ring clears as
     * the host reads. If USB drops while stalling, stop and return the
     * partial count so the upper-layer timeout and connect policy remain
     * reachable instead of hanging indefinitely. */
    while (remaining > 0U) {
        uint32_t freeBytes = CDC_Send_FreeBytes();
        if (freeBytes == 0U) {
            if (!(usbIsConnected() && usbIsConfigured())) {
                break;
            }
            delay(1);
            continue;
        }

        uint32_t toCopy = (remaining < freeBytes) ? remaining : freeBytes;

        ATOMIC_BLOCK(CDC_TX_ATOMIC_PRIORITY) {
            for (uint32_t i = 0U; i < toCopy; i++) {
                UserTxBuffer[UserTxBufPtrIn] = ptrBuffer[i];
                UserTxBufPtrIn = (UserTxBufPtrIn + 1U) % APP_TX_DATA_SIZE;
            }
        }

        ptrBuffer += toCopy;
        remaining -= toCopy;
    }

    return sendLength - remaining;
}

uint8_t CDC_Send_IsIdle(void)
{
    uint8_t isIdle;

    ATOMIC_BLOCK(CDC_TX_ATOMIC_PRIORITY) {
        USBD_CDC_HandleTypeDef *hcdc = CDC_GetHandle();
        isIdle = (UserTxBufPtrIn == UserTxBufPtrOut) &&
                 (lastBuffsize == 0U) &&
                 ((hcdc == NULL) || (hcdc->TxState == 0U));
    }

    return isIdle;
}

uint32_t CDC_Receive_DATA(uint8_t* recvBuf, uint32_t len)
{
    uint32_t count = 0;
    bool armEndpoint = false;

    ATOMIC_BLOCK(CDC_TX_ATOMIC_PRIORITY) {
        while (count < len) {
            while ((UserRxBufPtrOut != UserRxBufPtrIn) && (count < len)) {
                recvBuf[count] = UserRxBuffer[UserRxBufPtrOut];
                UserRxBufPtrOut = (UserRxBufPtrOut + 1U) % APP_RX_DATA_SIZE;
                count++;
            }

            if (rxPending && CDC_RxCommitPacket(UsbRxPacketBuffer, rxPendingLength)) {
                rxPending = false;
                rxPendingLength = 0U;
                armEndpoint = true;
                continue;
            }

            break;
        }

        if (rxPending && CDC_RxCommitPacket(UsbRxPacketBuffer, rxPendingLength)) {
            rxPending = false;
            rxPendingLength = 0U;
            armEndpoint = true;
        }
    }

    if (armEndpoint) {
        CDC_RxArmEndpoint();
    }

    return count;
}

static uint32_t CDC_RxBytesAvailable(void)
{
    return (UserRxBufPtrIn + APP_RX_DATA_SIZE - UserRxBufPtrOut) % APP_RX_DATA_SIZE;
}

static uint32_t CDC_RxFreeBytes(void)
{
    return (APP_RX_DATA_SIZE - CDC_RxBytesAvailable()) - 1U;
}

static bool CDC_RxCommitPacket(const uint8_t *buf, uint32_t length)
{
    if (CDC_RxFreeBytes() < length) {
        return false;
    }

    for (uint32_t i = 0U; i < length; i++) {
        UserRxBuffer[UserRxBufPtrIn] = buf[i];
        UserRxBufPtrIn = (UserRxBufPtrIn + 1U) % APP_RX_DATA_SIZE;
    }

    return true;
}

static void CDC_RxArmEndpoint(void)
{
#ifdef USE_USBD_COMPOSITE
    USBD_CDC_ReceivePacket(&USBD_Device, g_cdcClassId);
#else
    USBD_CDC_ReceivePacket(&USBD_Device);
#endif
}

uint32_t CDC_Receive_BytesAvailable(void)
{
    uint32_t available;

    ATOMIC_BLOCK(CDC_TX_ATOMIC_PRIORITY) {
        available = CDC_RxBytesAvailable();
    }

    return available;
}

uint8_t usbIsConfigured(void)
{
    return (USBD_Device.dev_state == USBD_STATE_CONFIGURED);
}

uint8_t usbIsConnected(void)
{
    return (USBD_Device.dev_state == USBD_STATE_ADDRESSED) ||
           (USBD_Device.dev_state == USBD_STATE_CONFIGURED) ||
           (USBD_Device.dev_state == USBD_STATE_SUSPENDED);
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
