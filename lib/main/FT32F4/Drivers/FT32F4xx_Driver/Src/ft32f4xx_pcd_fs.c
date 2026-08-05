/**
  ******************************************************************************
  * @file    			ft32f4xx_pcd_fs.c
  * @author  			FMD XA
  * @brief   			This file provides firmware functions to manage the following 
  *          			functionalities of the USB Peripheral Controller:
  *           		+ Initialization/de-initialization functions
  *           		+ Peripheral Control functions
  *           		+ Peripheral State functions
  * @version 			V1.0.0           
  * @data		 			2025-05-30
  @verbatim
  ==============================================================================
                    ##### How to use this driver #####
  ==============================================================================
  [..]
    (#)Declare a PCD_FS_HandleTypeDef handle structure, for example:
       PCD_FS_HandleTypeDef  hpcd;

    (#)Fill parameters of Init structure in PCD handle

    (#)Call PCD_FS_Init() API to initialize the PCD peripheral (Core, Host core, ...)

    (#)Initialize the PCD low level resources through the PCD_FS_MspInit() API:
        (##) Enable the PCD/USB Low Level interface clock using the following macros
        (##) Initialize the related GPIO clocks
        (##) Configure PCD NVIC interrupt

    (#)Associate the Upper USB Host stack to the PCD Driver:
        (##) hpcd.pData = pdev;

    (#)Enable PCD transmission and reception:
        (##) PCD_FS_Start();

  @endverbatim
  */
/* Includes ------------------------------------------------------------------*/
#include "ft32f4xx.h"
#include "ft32f4xx_pcd_fs.h"
#include "ft32f4xx_rcc.h"

static __IO uint8_t report;
/** @addtogroup FT32F4xx_DRIVER
  * @{
  */

#ifdef PCD_FS_MODULE_ENABLED
#if defined (USB_OTG_FS)
/** @defgroup PCD_FS PCD
  * @brief PCD FS module driver
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void PCD_FS_EP0ResetTransferState(PCD_FS_HandleTypeDef *hpcd);
static void PCD_FS_EP0AbortTransfer(PCD_FS_HandleTypeDef *hpcd);
static void PCD_FS_EP0Stall(PCD_FS_HandleTypeDef *hpcd);
static void PCD_FS_EP0CompleteStatusIn(PCD_FS_HandleTypeDef *hpcd);
static uint16_t PCD_FS_EP0GetSetupLength(const PCD_FS_HandleTypeDef *hpcd);

/** @defgroup PCD_FS_Private_Macros PCD Private Functions
  * @{
  */
#define PCD_MIN(a,b) (((a) < (b)) ? (a) : (b))
#define PCD_MAX(a,b) (((a) > (b)) ? (a) : (b))
/**
  * @{
  */

/** @defgroup PCD_FS_Private_Functions PCD Private Functions
  * @{
  */

static void PCD_FS_EP0ResetTransferState(PCD_FS_HandleTypeDef *hpcd)
{
  PCD_FS_EPTypeDef *in_ep = &hpcd->IN_ep[0U];
  PCD_FS_EPTypeDef *out_ep = &hpcd->OUT_ep[0U];

  in_ep->xfer_buff = NULL;
  in_ep->xfer_len = 0U;
  in_ep->xfer_count = 0U;
  in_ep->is_stall = 0U;

  out_ep->xfer_buff = NULL;
  out_ep->xfer_len = 0U;
  out_ep->xfer_count = 0U;
  out_ep->is_stall = 0U;

  hpcd->ctrl_state = PCD_CTRL_SETUP;
  hpcd->ep0_data_in_total = 0U;
  hpcd->ep0_data_in_count = 0U;
  hpcd->ep0_data_in_zlp = 0U;
  hpcd->ep0_in_pending = 0U;
  hpcd->ep0_out_pending = 0U;
}

static void PCD_FS_EP0AbortTransfer(PCD_FS_HandleTypeDef *hpcd)
{
  if (hpcd->address_pending != 0U)
  {
    hpcd->USB_Address = USB_FS_GetAddress();
    hpcd->address_pending = 0U;
  }

  PCD_FS_EP0ResetTransferState(hpcd);
}

static void PCD_FS_EP0Stall(PCD_FS_HandleTypeDef *hpcd)
{
  PCD_FS_EP0AbortTransfer(hpcd);
  hpcd->ctrl_state = PCD_CTRL_STALL;
  hpcd->IN_ep[0U].is_stall = 1U;
  hpcd->OUT_ep[0U].is_stall = 1U;
  (void)USB_FS_SendStall(&hpcd->IN_ep[0U]);
}

static void PCD_FS_EP0CompleteStatusIn(PCD_FS_HandleTypeDef *hpcd)
{
  hpcd->ep0_in_pending = 0U;
  if (hpcd->address_pending != 0U)
  {
    USB_FS_SetAddress(hpcd->USB_Address);
    hpcd->address_pending = 0U;
    PCD_FS_AddressCallback(hpcd, hpcd->USB_Address);
  }

  PCD_FS_EP0ResetTransferState(hpcd);
  PCD_FS_DataInStageCallback(hpcd, 0U);
}

static uint16_t PCD_FS_EP0GetSetupLength(const PCD_FS_HandleTypeDef *hpcd)
{
  const uint8_t *setup = (const uint8_t *)hpcd->Setup;

  return (uint16_t)setup[6] | ((uint16_t)setup[7] << 8U);
}

/**
  * @{
  */

/* Exported functions --------------------------------------------------------*/
/** @defgroup PCD_FS_Exported_Functions PCD Exported Functions
  * @{
  */

/** @defgroup PCD_FS_Exported_Functions_Group1 Initialization and de-initialization functions
  *  @brief    Initialization and Configuration functions
  *
@verbatim
 ===============================================================================
          ##### Initialization and de-initialization functions #####
 ===============================================================================
    [..]  This section provides functions allowing to:

@endverbatim
  * @{
  */

/**
  * @brief  Initializes the PCD according to the specified
  *         parameters in the PCD_InitTypeDef and initialize the associated handle.
  * @param  hpcd PCD handle
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_Init(PCD_FS_HandleTypeDef *hpcd)
{

  uint8_t i;

  /* Check the PCD handle allocation */
  if (hpcd == NULL)
  {
    return USB_FS_ERROR;
  }

  if (hpcd->State == PCD_FS_STATE_RESET)
  {
    /* Allocate lock resource and initialize it */
    hpcd->Lock = USB_FS_UNLOCKED;

    /* Init the low level hardware : GPIO, CLOCK, NVIC... */
    PCD_FS_MspInit(hpcd);
  }

  hpcd->State = PCD_FS_STATE_BUSY;

  /* Disable the Interrupts */
  USB_FS_SetUSBInt(0U);

  /*Init the Core (common init.) */
  if (USB_FS_CoreInit() != USB_FS_OK)
  {
    hpcd->State = PCD_FS_STATE_ERROR;
    return USB_FS_ERROR;
  }

  /* Init endpoints structures */
  for (i = 0U; i < hpcd->Init.endpoints; i++)
  {
    /* Init ep structure */
    hpcd->IN_ep[i].is_in = 1U;
    hpcd->IN_ep[i].num = i;
    hpcd->IN_ep[i].tx_fifo_num = i;
    /* Control until ep is activated */
    hpcd->IN_ep[i].type = EP_TYPE_CTRL;
    hpcd->IN_ep[i].maxpacket = 0U;
    hpcd->IN_ep[i].xfer_buff = 0U;
    hpcd->IN_ep[i].xfer_len = 0U;
    hpcd->IN_ep[i].xfer_count = 0U;
    hpcd->IN_ep[i].is_stall = 0U;
  }

  for (i = 0U; i < hpcd->Init.endpoints; i++)
  {
    hpcd->OUT_ep[i].is_in = 0U;
    hpcd->OUT_ep[i].num = i;
    /* Control until ep is activated */
    hpcd->OUT_ep[i].type = EP_TYPE_CTRL;
    hpcd->OUT_ep[i].maxpacket = 0U;
    hpcd->OUT_ep[i].xfer_buff = 0U;
    hpcd->OUT_ep[i].xfer_len = 0U;
    hpcd->OUT_ep[i].xfer_count = 0U;
    hpcd->OUT_ep[i].is_stall = 0U;
  }

  /* Init Device */
  if (USB_FS_DevInit(hpcd->Init) != USB_FS_OK)
  {
    hpcd->State = PCD_FS_STATE_ERROR;
    return USB_FS_ERROR;
  }

  hpcd->USB_Address = 0U;
  hpcd->address_pending = 0U;
  hpcd->State = PCD_FS_STATE_READY;
  PCD_FS_EP0ResetTransferState(hpcd);
  USB_FS_DrvSess(1U);

  return USB_FS_OK;
}

/**
  * @brief  DeInitializes the PCD peripheral.
  * @param  hpcd PCD handle
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_DeInit(PCD_FS_HandleTypeDef *hpcd)
{
  uint32_t i;
  /* Check the PCD handle allocation */
  if (hpcd == NULL)
  {
    return USB_FS_ERROR;
  }

  hpcd->State = PCD_FS_STATE_BUSY;

  /* Stop Device */
  USB_FS_SetUSBInt(0U);
  USB_FS_SetEPInt(0U);

  if (USB_FS_RstEP0Regs() != USB_FS_OK)
  {
    return USB_FS_ERROR;
  }
  for (i = 1U; i < hpcd->Init.endpoints; i++)
  {
    if (USB_FS_RstEPRegs(i) != USB_FS_OK)
    {
      return USB_FS_ERROR;
    }
  }
  /* DeInit the low level hardware: CLOCK, NVIC.*/
  PCD_FS_MspDeInit(hpcd);

  hpcd->State = PCD_FS_STATE_RESET;

  return USB_FS_OK;
}

/**
  * @brief  set iso mode.
  * @param  hpcd PCD handle
  * @retval none
  */
void PCD_FS_SetISO(uint8_t ep_num, uint8_t state)
{
  USB_FS_IndexSel(ep_num);
  if (state != 0U)
  {
    USB_FS->TXCSR2 |= OTG_FS_TXCSR2_ISO ;
    USB_FS->RXCSR2 |= OTG_FS_RXCSR2_ISO ;
  }
  else
  {
    USB_FS->TXCSR2 &= ~OTG_FS_TXCSR2_ISO ;
    USB_FS->RXCSR2 &= ~OTG_FS_RXCSR2_ISO ;
  }
}

/**
  * @brief  set maxpacket.
  * @param  hpcd PCD handle
  * @retval none
  */
void PCD_FS_SetMaxPkt(uint8_t ep_num, uint16_t size)
{
  uint16_t maxpkt;
  maxpkt = (size + 7U) / 8U ;
  USB_FS_IndexSel(ep_num);

  USB_FS->TXMAXP = maxpkt;
  USB_FS->RXMAXP = maxpkt;
}

/**
  * @brief  Initializes the PCD MSP.
  * @param  hpcd PCD handle
  * @retval None
  */
void __attribute__((weak)) PCD_FS_MspInit(PCD_FS_HandleTypeDef *hpcd)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hpcd);

  /* NOTE : This function should not be modified, when the callback is needed,
            the PCD_FS_MspInit could be implemented in the user file
   */
}

/**
  * @brief  DeInitializes PCD MSP.
  * @param  hpcd PCD handle
  * @retval None
  */
void __attribute__((weak)) PCD_FS_MspDeInit(PCD_FS_HandleTypeDef *hpcd)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hpcd);

  /* NOTE : This function should not be modified, when the callback is needed,
            the PCD_FS_MspDeInit could be implemented in the user file
   */
}


/**
  * @}
  */

/** @defgroup PCD_Exported_Functions_Group2 Input and Output operation functions
  *  @brief   Data transfers functions
  *
@verbatim
 ===============================================================================
                      ##### IO operation functions #####
 ===============================================================================
    [..]
    This subsection provides a set of functions allowing to manage the PCD data
    transfers.

@endverbatim
  * @{
  */

/**
  * @brief  Start the USB device
  * @param  hpcd PCD handle
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_Start(PCD_FS_HandleTypeDef *hpcd)
{
  __USB_FS_LOCK(hpcd);

  USB_FS_SetUSBInt(OTG_FS_INTRUSBE_SOFINTE  | OTG_FS_INTRUSBE_RSTINTE  |
                   OTG_FS_INTRUSBE_DISCINTE | OTG_FS_INTRUSBE_SREQINTE |
                   OTG_FS_INTRUSBE_VERRINTE);

  USB_FS_DrvSess(1U);

  __USB_FS_UNLOCK(hpcd);

  return USB_FS_OK;
}

/**
  * @brief  Stop the USB device.
  * @param  hpcd PCD handle
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_Stop(PCD_FS_HandleTypeDef *hpcd)
{
  uint32_t i;
  __USB_FS_LOCK(hpcd);

  USB_FS_SetUSBInt(0U);

  USB_FS_DrvSess(0U);
  if (USB_FS_FlushEp0Fifo() != USB_FS_OK)
  {
    return USB_FS_ERROR;
  }
  for (i = 1U; i < hpcd->Init.endpoints; i++)
  {
    if (USB_FS_FlushTxFifo(i) != USB_FS_OK)
    {
      return USB_FS_ERROR;
    }
    if (USB_FS_FlushRxFifo(i) != USB_FS_OK)
    {
      return USB_FS_ERROR;
    }
  }
  __USB_FS_UNLOCK(hpcd);

  return USB_FS_OK;
}

#if defined (USB_OTG_FS)
/**
  * @brief  Handles PCD interrupt request.
  * @param  hpcd PCD handle
  * @retval none
  */
void PCD_FS_IRQHandler(PCD_FS_HandleTypeDef *hpcd)
{
  uint32_t i;
  uint32_t ep_intr;
  uint32_t epint;
  uint32_t epnum;
  uint32_t fifoemptymsk;
  uint32_t RegVal;
  uint32_t reg_int;
  uint32_t tx_int;
  uint32_t rx_int;
	  uint8_t  saved_index;
	  uint8_t  ep0_csr;
	  uint8_t  reg_power;

	  reg_int = USB_FS_ReadInterrupts();
  /* ensure that we are in device mode */
  if ((USB_FS_GetMode() & USB_OTG_MODE_DEVICE) == USB_OTG_MODE_DEVICE)
  {
    /* avoid spurious interrupt */
    if (reg_int == 0U)
	    {
	      return;
	    }

	    /* store current frame number */
	    hpcd->FrameNumber = USB_FS_GetCurrentFrame();

    /* Handle vbus error Interrupts */
    if ((reg_int & 0xFFU) != 0U)
    {
      if ((reg_int & OTG_FS_INTRUSB_VERRINT) == OTG_FS_INTRUSB_VERRINT)
      {
        PCD_FS_VBusErrCallback(hpcd);
      }

      /* Handle session request Interrupts */
      if ((reg_int & OTG_FS_INTRUSB_SREQINT) == OTG_FS_INTRUSB_SREQINT)
      {
        PCD_FS_SessionCallback(hpcd);
      }

      /* Handle Host Disconnect Interrupts */
      if ((reg_int & OTG_FS_INTRUSB_DISCINT) == OTG_FS_INTRUSB_DISCINT)
      {
        /* Handle Host Port Disconnect Interrupt */
        PCD_FS_DisconnectCallback(hpcd);
      }

      /* Handle Host Connect Interrupts */
      if ((reg_int & OTG_FS_INTRUSB_CONNINT) == OTG_FS_INTRUSB_CONNINT)
      {
        /* Handle Host Port Connect Interrupt */
        PCD_FS_ConnectCallback(hpcd);
      }

      /* Handle Host SOF Interrupt */
      if ((reg_int & OTG_FS_INTRUSB_SOFINT) == OTG_FS_INTRUSB_SOFINT)
      {
        PCD_FS_SOFCallback(hpcd);
      }
	      /* Handle Host reset Interrupt */
	      if ((reg_int & OTG_FS_INTRUSB_RSTINT) == OTG_FS_INTRUSB_RSTINT)
	      {
	        USB_FS->CSR0 = OTG_FS_CSR0_SSETUPEND | OTG_FS_CSR0_SRXPKTRDY;
	        USB_FS_RstEP0Regs();
	        for (i = 1U; i < hpcd->Init.endpoints; i++)
        {
          (void)USB_FS_IndexSel((uint8_t)i);
          USB_FS->TXCSR1 = 0U;
          USB_FS->RXCSR1 = 0U;
          (void)USB_FS_ReadInterrupts();
          USB_FS_RstEPRegs(i);
        }

        USB_FS_SetEPInt(0x0FU);

        hpcd->USB_Address = 0U;
	        hpcd->address_pending = 0U;
	        USB_FS_SetAddress(0U);
	        PCD_FS_EP0ResetTransferState(hpcd);

	        PCD_FS_ResetCallback(hpcd);
	      }
      /* Handle resume Interrupt */
      if ((reg_int & OTG_FS_INTRUSB_RESINT) == OTG_FS_INTRUSB_RESINT)
      {
        PCD_FS_ResumeCallback(hpcd);
      }
      /* Handle suspend Interrupt */
      if ((reg_int & OTG_FS_INTRUSB_SUSPINT) == OTG_FS_INTRUSB_SUSPINT)
      {
        reg_power = USB_FS_GetPower();
        if ((reg_power & OTG_FS_POWER_SUSPEND) != 0U)
        {
          PCD_FS_SuspendCallback(hpcd);
        }
      }
    }
    /* Handle EP0 endpoint Interrupt */
    tx_int = ((reg_int >> 8) & 0xFU);
    if ((tx_int & OTG_FS_INTRTX1_EP0INF) == 0U)
    {
      saved_index = USB_FS->INDEX;
      (void)USB_FS_IndexSel(0U);
      ep0_csr = USB_FS->CSR0;
      (void)USB_FS_IndexSel(saved_index);
	      if ((ep0_csr & (OTG_FS_CSR0_RXPKTRDY | OTG_FS_CSR0_SETUPEND | OTG_FS_CSR0_STSTALL)) != 0U)
	      {
	        tx_int |= OTG_FS_INTRTX1_EP0INF;
	      }
	    }
    if ((tx_int & OTG_FS_INTRTX1_EP0INF) == OTG_FS_INTRTX1_EP0INF)
    {
      (void)USB_FS_IndexSel(0U);
      PCD_FS_EP0_IRQHandler(hpcd);
    }
    
    /* Handle Tx endpoint Interrupt */
    if (((reg_int >> 8) & 0xEU) != 0U)
    {
      for (i = 1U; i < hpcd->Init.endpoints; i++)
      {
        if (((tx_int >> i) & 0x01U) != 0U)
        {
          (void)USB_FS_IndexSel((uint8_t)i);
          PCD_FS_TXEP_IRQHandler(hpcd, (uint8_t)i);
        }
      }
    }

    /* Handle Rx endpoint Interrupt */
    if (((reg_int >> 16) & 0xFU) != 0U)
    {
      report = 1;
      rx_int = ((reg_int >> 16) & 0xFU);
      for (i = 1U; i < hpcd->Init.endpoints; i++)
      {
        if (((rx_int >> i) & 0x01U) != 0U)
        {
          (void)USB_FS_IndexSel((uint8_t)i);
          PCD_FS_RXEP_IRQHandler(hpcd, (uint8_t)i);
        }
      }
    }
  }
}

/**
  * @brief  Handles PCD Wakeup interrupt request.
  * @param  hpcd PCD handle
  * @retval none status
  */
void PCD_FS_WKUP_IRQHandler(void)
{
  /* Clear EXTI pending Bit */
  __USB_OTG_FS_WAKEUP_EXTI_CLEAR_FLAG();
}
#endif /* defined (USB_OTG_FS) */


/**
  * @brief  Data OUT stage callback.
  * @param  hpcd PCD handle
  * @param  epnum endpoint number
  * @retval None
  */
/**
  * @brief  USB Start Of Frame callback.
  * @param  hpcd PCD handle
  * @retval None
  */
void __attribute__((weak)) PCD_FS_SessionCallback(PCD_FS_HandleTypeDef *hpcd)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hpcd);

  /* NOTE : This function should not be modified, when the callback is needed,
            the PCD_FS_SOFCallback could be implemented in the user file
   */
}

/**
  * @brief  USB Start Of Frame callback.
  * @param  hpcd PCD handle
  * @retval None
  */
void __attribute__((weak)) PCD_FS_VBusErrCallback(PCD_FS_HandleTypeDef *hpcd)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hpcd);

  /* NOTE : This function should not be modified, when the callback is needed,
            the PCD_FS_SOFCallback could be implemented in the user file
   */
}

/**
  * @brief  USB Start Of Frame callback.
  * @param  hpcd PCD handle
  * @retval None
  */
void __attribute__((weak)) PCD_FS_OVERRUNCallback(PCD_FS_HandleTypeDef *hpcd)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hpcd);

  /* NOTE : This function should not be modified, when the callback is needed,
            the PCD_FS_SOFCallback could be implemented in the user file
   */
}

/**
  * @brief  USB Start Of Frame callback.
  * @param  hpcd PCD handle
  * @retval None
  */
void __attribute__((weak)) PCD_FS_UNDERRUNCallback(PCD_FS_HandleTypeDef *hpcd)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hpcd);

  /* NOTE : This function should not be modified, when the callback is needed,
            the PCD_FS_SOFCallback could be implemented in the user file
   */
}

/**
  * @brief  USB Start Of Frame callback.
  * @param  hpcd PCD handle
  * @retval None
  */
void __attribute__((weak)) PCD_FS_DERRCallback(PCD_FS_HandleTypeDef *hpcd)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(hpcd);

  /* NOTE : This function should not be modified, when the callback is needed,
            the PCD_FS_SOFCallback could be implemented in the user file
   */
}
/**
  * @brief  USB Reset callback.
  * @param  hpcd PCD handle
  * @retval None
  */
/**
  * @}
  */

/** @defgroup PCD_Exported_Functions_Group3 Peripheral Control functions
  *  @brief   management functions
  *
@verbatim
 ===============================================================================
                      ##### Peripheral Control functions #####
 ===============================================================================
    [..]
    This subsection provides a set of functions allowing to control the PCD data
    transfers.

@endverbatim
  * @{
  */

///**
//  * @brief  Connect the USB device
//  * @param  hpcd PCD handle
//  * @retval USB_FS status
//  */
//USB_FS_StatusTypeDef PCD_FS_DevConnect(PCD_FS_HandleTypeDef *hpcd)
//{
//  __USB_FS_LOCK(hpcd);
//
//  USB_FS_DevConnect();
//  __USB_FS_UNLOCK(hpcd);
//
//  return USB_FS_OK;
//}

/**
  * @brief  Disconnect the USB device.
  * @param  hpcd PCD handle
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_DevDisconnect(PCD_FS_HandleTypeDef *hpcd)
{
  __USB_FS_LOCK(hpcd);

  USB_FS_DrvSess(0U);
  __USB_FS_UNLOCK(hpcd);

  return USB_FS_OK;
}

/**
  * @brief  Set the USB Device address.
  * @param  hpcd PCD handle
  * @param  address new device address
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_SetAddress(PCD_FS_HandleTypeDef *hpcd, uint8_t address)
{
  __USB_FS_LOCK(hpcd);
  hpcd->USB_Address = address;
  hpcd->address_pending = 1U;
  __USB_FS_UNLOCK(hpcd);

  return USB_FS_OK;
}
/**
  * @brief  Open and configure an endpoint.
  * @param  hpcd PCD handle
  * @param  ep_addr endpoint address
  * @param  ep_mps endpoint max packet size
  * @param  ep_type endpoint type
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_EP_Open(PCD_FS_HandleTypeDef *hpcd, uint8_t ep_addr,
                                    uint16_t ep_mps, uint8_t ep_type)
{
  USB_FS_StatusTypeDef  ret = USB_FS_OK;
  PCD_FS_EPTypeDef *ep;

  if ((ep_addr & 0x80U) == 0x80U)
  {
    ep = &hpcd->IN_ep[ep_addr & EP_ADDR_MSK];
    ep->is_in = 1U;
  }
  else
  {
    ep = &hpcd->OUT_ep[ep_addr & EP_ADDR_MSK];
    ep->is_in = 0U;
  }

  ep->num = ep_addr & EP_ADDR_MSK;
  ep->maxpacket = ep_mps;
  ep->type = ep_type;

  if (ep->is_in != 0U)
  {
    /* Assign a Tx FIFO */
    ep->tx_fifo_num = ep->num;
  }
  /* Set initial data PID. */
  if (ep_type == EP_TYPE_BULK)
  {
    ep->data_pid_start = 0U;
  }

  __USB_FS_LOCK(hpcd);
  if (ep->num != 0U)
  {
    USB_FS_Enable_DEP(ep);
  }
  __USB_FS_UNLOCK(hpcd);

  return ret;
}

/**
  * @brief  Deactivate an endpoint.
  * @param  hpcd PCD handle
  * @param  ep_addr endpoint address
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_EP_Close(PCD_FS_HandleTypeDef *hpcd, uint8_t ep_addr)
{
  PCD_FS_EPTypeDef *ep;

  if ((ep_addr & 0x80U) == 0x80U)
  {
    ep = &hpcd->IN_ep[ep_addr & EP_ADDR_MSK];
    ep->is_in = 1U;
  }
  else
  {
    ep = &hpcd->OUT_ep[ep_addr & EP_ADDR_MSK];
    ep->is_in = 0U;
  }
  ep->num = ep_addr & EP_ADDR_MSK;

  __USB_FS_LOCK(hpcd);
  if (USB_FS_SendStall(ep) != USB_FS_OK)
  {
    return USB_FS_ERROR;
  }
  __USB_FS_UNLOCK(hpcd);
  return USB_FS_OK;
}


/**
  * @brief  Receive an amount of data.
  * @param  hpcd PCD handle
  * @param  ep_addr endpoint address
  * @param  pBuf pointer to the reception buffer
  * @param  len amount of data to be received
  * @retval none
  */
void PCD_FS_EP_Receive(PCD_FS_HandleTypeDef *hpcd, uint8_t ep_addr, uint8_t *pBuf, uint32_t len)
{
  PCD_FS_EPTypeDef *ep;
  uint32_t pkt_len;

  ep = &hpcd->OUT_ep[ep_addr & EP_ADDR_MSK];

  /*setup and start the Xfer */
  ep->is_in = 0U;
  ep->num = ep_addr & EP_ADDR_MSK;

  if (ep->num == 0U)
  {
    if (((len != 0U) && ((pBuf == NULL) || (ep->maxpacket == 0U))) ||
        ((len == 0U) && (pBuf != NULL)))
    {
      PCD_FS_EP0Stall(hpcd);
      return;
    }

    if ((len != 0U) && (hpcd->ep0_out_pending != 0U))
    {
      PCD_FS_EP0Stall(hpcd);
      return;
    }

    if ((len != 0U) && (hpcd->ctrl_state == PCD_CTRL_DATA_OUT) &&
        (hpcd->ep0_out_pending == 0U) && (ep->xfer_buff != pBuf))
    {
      PCD_FS_EP0Stall(hpcd);
      return;
    }

    pkt_len = (len > ep->maxpacket) ? ep->maxpacket : len;
    ep->xfer_buff = pBuf;
    ep->xfer_len = pkt_len;
    ep->xfer_count = 0U;
    hpcd->ep0_out_pending = 1U;

    if (len != 0U)
    {
      hpcd->ctrl_state = PCD_CTRL_DATA_OUT;
    }
    else if (!((hpcd->ctrl_state == PCD_CTRL_DATA_IN) &&
               (hpcd->ep0_in_pending != 0U)))
    {
      hpcd->ctrl_state = PCD_CTRL_STATUS_OUT;
    }
    return;
  }

  ep->xfer_buff = pBuf;
  ep->xfer_len = len;
  ep->xfer_count = 0U;
  USB_FS_DEPStartXfer(ep);
}

/**
  * @brief  Get Received Data Size
  * @param  hpcd PCD handle
  * @param  ep_addr endpoint address
  * @retval Data Size
  */
uint32_t PCD_FS_EP_GetRxCount(const PCD_FS_HandleTypeDef *hpcd, uint8_t ep_addr)
{
  return hpcd->OUT_ep[ep_addr & EP_ADDR_MSK].xfer_count;
}
/**
  * @brief  Send an amount of data
  * @param  hpcd PCD handle
  * @param  ep_addr endpoint address
  * @param  pBuf pointer to the transmission buffer
  * @param  len amount of data to be sent
  * @retval none
  */
void PCD_FS_EP_Transmit(PCD_FS_HandleTypeDef *hpcd, uint8_t ep_addr, uint8_t *pBuf, uint32_t len)
{
  PCD_FS_EPTypeDef *ep;
  uint32_t pkt_len;
  uint32_t remaining;
  uint8_t data_end;

  ep = &hpcd->IN_ep[ep_addr & EP_ADDR_MSK];

  /*setup and start the Xfer */
  ep->is_in = 1U;
  ep->num = ep_addr & EP_ADDR_MSK;

  if (ep->num != 0U)
  {
    ep->xfer_buff = pBuf;
    ep->xfer_len = len;
    ep->xfer_count = 0U;
    ep->is_stall = 0U;
    (void)USB_FS_IndexSel(ep->num);
    USB_FS->TXCSR1 &= (~OTG_FS_TXCSR1_STSTALL);
    USB_FS_DEPStartXfer(ep);
    return;
  }

  if (((len != 0U) && ((pBuf == NULL) || (ep->maxpacket == 0U))) ||
      ((len == 0U) && (pBuf != NULL)))
  {
    PCD_FS_EP0Stall(hpcd);
    return;
  }

  /* The middleware may submit the next EP0 IN packet only from the previous
   * packet's completion callback. Re-arming an outstanding packet would lose
   * both the FIFO ownership token and the transfer accounting. */
  if (hpcd->ep0_in_pending != 0U)
  {
    PCD_FS_EP0Stall(hpcd);
    return;
  }

  data_end = 1U;
  if ((hpcd->ctrl_state == PCD_CTRL_DATA_IN) &&
      (hpcd->ep0_data_in_count <= hpcd->ep0_data_in_total))
  {
    remaining = hpcd->ep0_data_in_total - hpcd->ep0_data_in_count;
    if ((len != remaining) || ((len != 0U) && (pBuf != ep->xfer_buff)))
    {
      PCD_FS_EP0Stall(hpcd);
      return;
    }
  }
  else if (hpcd->ctrl_state == PCD_CTRL_DATA_IN)
  {
    PCD_FS_EP0Stall(hpcd);
    return;
  }
  else
  {
    if (len == 0U)
    {
      hpcd->ctrl_state = PCD_CTRL_STATUS_IN;
      hpcd->ep0_data_in_total = 0U;
      hpcd->ep0_data_in_count = 0U;
      hpcd->ep0_data_in_zlp = 0U;
    }
    else
    {
      hpcd->ctrl_state = PCD_CTRL_DATA_IN;
      hpcd->ep0_data_in_total = len;
      hpcd->ep0_data_in_count = 0U;
      hpcd->ep0_data_in_zlp = (((len % ep->maxpacket) == 0U) &&
                               (len < PCD_FS_EP0GetSetupLength(hpcd))) ? 1U : 0U;
    }
  }

  pkt_len = (len > ep->maxpacket) ? ep->maxpacket : len;
  if ((hpcd->ctrl_state == PCD_CTRL_DATA_IN) && (len > ep->maxpacket))
  {
    data_end = 0U;
  }
  else if ((hpcd->ctrl_state == PCD_CTRL_DATA_IN) && (len != 0U) &&
           (hpcd->ep0_data_in_zlp != 0U))
  {
    data_end = 0U;
  }

  ep->xfer_buff = pBuf;
  ep->xfer_len = pkt_len;
  ep->xfer_count = 0U;
  ep->is_stall = 0U;
  hpcd->ep0_in_pending = 1U;
  USB_FS_DEP0StartXfer(ep, hpcd->ctrl_state, data_end);
}

/**
  * @brief  Set a STALL condition over an endpoint
  * @param  hpcd PCD handle
  * @param  ep_addr endpoint address
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_EP_SetStall(PCD_FS_HandleTypeDef *hpcd, uint8_t ep_addr)
{
  PCD_FS_EPTypeDef *ep;

  if (((uint32_t)ep_addr & EP_ADDR_MSK) >= hpcd->Init.endpoints)
  {
    return USB_FS_ERROR;
  }

  if ((0x80U & ep_addr) == 0x80U)
  {
    ep = &hpcd->IN_ep[ep_addr & EP_ADDR_MSK];
    ep->is_in = 1U;
  }
  else
  {
    ep = &hpcd->OUT_ep[ep_addr];
    ep->is_in = 0U;
  }

  ep->is_stall = 1U;
  ep->num = ep_addr & EP_ADDR_MSK;

  __USB_FS_LOCK(hpcd);

  if ((ep_addr & EP_ADDR_MSK) == 0U)
  {
    PCD_FS_EP0Stall(hpcd);
  }
  else
  {
    (void)USB_FS_SendStall(ep);
  }

  __USB_FS_UNLOCK(hpcd);

  return USB_FS_OK;
}

/**
  * @brief  Clear a STALL condition over in an endpoint
  * @param  hpcd PCD handle
  * @param  ep_addr endpoint address
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_EP_ClrStall(PCD_FS_HandleTypeDef *hpcd, uint8_t ep_addr)
{
  PCD_FS_EPTypeDef *ep;

  if (((uint32_t)ep_addr & 0x0FU) >= hpcd->Init.endpoints)
  {
    return USB_FS_ERROR;
  }

  if ((0x80U & ep_addr) == 0x80U)
  {
    ep = &hpcd->IN_ep[ep_addr & EP_ADDR_MSK];
    ep->is_in = 1U;
  }
  else
  {
    ep = &hpcd->OUT_ep[ep_addr & EP_ADDR_MSK];
    ep->is_in = 0U;
  }

  ep->is_stall = 0U;
  ep->num = ep_addr & EP_ADDR_MSK;

  __USB_FS_LOCK(hpcd);
  if (ep->num == 0U)
  {
    PCD_FS_EP0AbortTransfer(hpcd);
  }
  (void)USB_FS_ClrStall(ep);
  __USB_FS_UNLOCK(hpcd);

  return USB_FS_OK;
}

/**
  *
  * @brief  Abort an USB EP transaction
  * @param  hpcd PCD handle
  * @param  ep_addr endpoint address
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_EP_Abort(PCD_FS_HandleTypeDef *hpcd, uint8_t ep_addr)
{
  USB_FS_StatusTypeDef ret;
  PCD_FS_EPTypeDef *ep;

  if ((0x80U & ep_addr) == 0x80U)
  {
    ep = &hpcd->IN_ep[ep_addr & EP_ADDR_MSK];
  }
  else
  {
    ep = &hpcd->OUT_ep[ep_addr & EP_ADDR_MSK];
  }

  /* Stop Xfer */
  if (ep->num == 0U)
  {
    ret = USB_FS_RstEP0Regs();
    PCD_FS_EP0AbortTransfer(hpcd);
  }
  else
  {
    ret = USB_FS_RstEPRegs(ep->num);
  }

  return ret;
}

/**
  * @brief  Flush an endpoint
  * @param  hpcd PCD handle
  * @param  ep_addr endpoint address
  * @retval USB_FS status
  */
USB_FS_StatusTypeDef PCD_FS_EP_Flush(PCD_FS_HandleTypeDef *hpcd, uint8_t ep_addr)
{
  __USB_FS_LOCK(hpcd);

  if ((ep_addr & 0x80U) == 0x80U)
  {
    (void)USB_FS_FlushTxFifo((uint32_t)ep_addr & EP_ADDR_MSK);
  }
  else
  {
    (void)USB_FS_FlushRxFifo((uint32_t)ep_addr & EP_ADDR_MSK);
  }

  __USB_FS_UNLOCK(hpcd);

  return USB_FS_OK;
}

/**
  * @brief  Activate remote wakeup signalling
  * @param  hpcd PCD handle
  * @retval none
  */
void PCD_FS_ActivateRemoteWakeup(void)
{
  USB_FS_Activate_Resume();
}

/**
  * @brief  De-activate remote wakeup signalling.
  * @param  hpcd PCD handle
  * @retval none
  */
void PCD_FS_DeActivateRemoteWakeup(void)
{
  USB_FS_DeActivate_Resume();
}

/**
  * @}
  */

/** @defgroup PCD_Exported_Functions_Group4 Peripheral State functions
  *  @brief   Peripheral State functions
  *
@verbatim
 ===============================================================================
                      ##### Peripheral State functions #####
 ===============================================================================
    [..]
    This subsection permits to get in run-time the status of the peripheral
    and the data flow.

@endverbatim
  * @{
  */

/**
  * @brief  Return the PCD handle state.
  * @param  hpcd PCD handle
  * @retval hpcd state
  */
PCD_FS_StateTypeDef PCD_FS_GetState(PCD_FS_HandleTypeDef const *hpcd)
{
  return hpcd->State;
}

/**
  * @}
  */

/**
  * @}
  */

/* Private functions ---------------------------------------------------------*/
/** @addtogroup PCD_Private_Functions
  * @{
  */

#if defined (USB_OTG_FS)


/**
  * @brief  process EP OUT transfer complete interrupt.
  * @param  hpcd PCD handle
  * @param  epnum endpoint number
  * @retval none
  * */
void PCD_FS_EP0_IRQHandler(PCD_FS_HandleTypeDef *hpcd)
{
  USB_OTG_FS_DEPTypeDef *ep;
  uint8_t tmpreg;
  uint8_t bytecount;

  tmpreg = USB_FS->CSR0;

  /* SETUPEND terminates the old control transfer. A new SETUP can already
   * be waiting in FIFO0, so acknowledge the old transfer before re-reading
   * CSR0 and never flush FIFO0 on this path. */
  if ((tmpreg & OTG_FS_CSR0_SETUPEND) != 0U)
  {
    USB_FS->CSR0 = OTG_FS_CSR0_SSETUPEND;
    PCD_FS_EP0AbortTransfer(hpcd);
    tmpreg = USB_FS->CSR0;
  }

  if ((tmpreg & OTG_FS_CSR0_STSTALL) != 0U)
  {
    /* STSTALL is RC_W0. A direct zero avoids replaying CSR0 command bits. */
    USB_FS->CSR0 = 0U;
    PCD_FS_EP0AbortTransfer(hpcd);
    tmpreg = USB_FS->CSR0;
  }

  /* The ACK for the last IN packet and its zero-length STATUS OUT can be
   * reported by one endpoint interrupt. Complete a genuinely final data
   * packet first so the device core can arm STATUS OUT before it is consumed.
   * For an early host termination, leave the data callback suppressed and let
   * the STATUS OUT path below abort the outstanding IN transfer. */
  if (((tmpreg & OTG_FS_CSR0_RXPKTRDY) != 0U) &&
      ((tmpreg & (OTG_FS_CSR0_TXPKTRDY | OTG_FS_CSR0_DATAEND)) == 0U) &&
      (hpcd->ep0_in_pending != 0U) &&
      (hpcd->ctrl_state == PCD_CTRL_DATA_IN))
  {
    uint32_t completed_count;

    ep = &hpcd->IN_ep[0U];
    if ((hpcd->ep0_data_in_count > hpcd->ep0_data_in_total) ||
        (ep->xfer_count > ep->xfer_len) ||
        (ep->xfer_count > (hpcd->ep0_data_in_total - hpcd->ep0_data_in_count)))
    {
      PCD_FS_EP0Stall(hpcd);
      return;
    }

    completed_count = hpcd->ep0_data_in_count + ep->xfer_count;
    if ((completed_count == hpcd->ep0_data_in_total) &&
        ((hpcd->ep0_data_in_zlp == 0U) || (ep->xfer_len == 0U)))
    {
      hpcd->ep0_in_pending = 0U;
      hpcd->ep0_data_in_count = completed_count;
      PCD_FS_DataInStageCallback(hpcd, 0U);
      tmpreg = USB_FS->CSR0;
    }
  }

  /* A new SETUP can share the interrupt that reports completion of the
   * previous STATUS IN. Commit deferred side effects before parsing the new
   * request; reset here is software-only and intentionally preserves FIFO0. */
  if (((tmpreg & OTG_FS_CSR0_RXPKTRDY) != 0U) &&
      ((tmpreg & (OTG_FS_CSR0_TXPKTRDY | OTG_FS_CSR0_DATAEND)) == 0U) &&
      (hpcd->ep0_in_pending != 0U) &&
      (hpcd->ctrl_state == PCD_CTRL_STATUS_IN))
  {
    PCD_FS_EP0CompleteStatusIn(hpcd);
    tmpreg = USB_FS->CSR0;
  }

  if ((tmpreg & OTG_FS_CSR0_RXPKTRDY) != 0U)
  {
    bytecount = USB_FS_Read_Count0();
    ep = &hpcd->OUT_ep[0U];

    if ((hpcd->ctrl_state == PCD_CTRL_SETUP) ||
        (hpcd->ctrl_state == PCD_CTRL_STALL))
    {
      if (bytecount != 8U)
      {
        (void)USB_FS_FlushEp0Fifo();
        PCD_FS_EP0Stall(hpcd);
        return;
      }

      PCD_FS_EP0AbortTransfer(hpcd);
      USB_FS_FIFORead((uint8_t *)hpcd->Setup, 0U, bytecount);
      USB_FS->CSR0 = OTG_FS_CSR0_SRXPKTRDY;
      PCD_FS_SetupStageCallback(hpcd);
      return;
    }

    if (hpcd->ctrl_state == PCD_CTRL_DATA_OUT)
    {
      if ((hpcd->ep0_out_pending == 0U) || (ep->xfer_buff == NULL) ||
          (ep->xfer_count != 0U) || (bytecount != ep->xfer_len))
      {
        (void)USB_FS_FlushEp0Fifo();
        PCD_FS_EP0Stall(hpcd);
        return;
      }

      USB_FS_FIFORead(ep->xfer_buff, 0U, bytecount);
      ep->xfer_buff += bytecount;
      ep->xfer_count = bytecount;
      hpcd->ep0_out_pending = 0U;
      USB_FS->CSR0 = OTG_FS_CSR0_SRXPKTRDY;
      PCD_FS_DataOutStageCallback(hpcd, 0U);
      return;
    }

    if ((((hpcd->ctrl_state == PCD_CTRL_STATUS_OUT) &&
          (hpcd->ep0_out_pending != 0U) && (ep->xfer_len == 0U)) ||
         (hpcd->ctrl_state == PCD_CTRL_DATA_IN)) &&
        (bytecount == 0U))
    {
      uint8_t early_status_out =
          (hpcd->ctrl_state == PCD_CTRL_DATA_IN) ? 1U : 0U;

      USB_FS->CSR0 = OTG_FS_CSR0_SRXPKTRDY | OTG_FS_CSR0_DATAEND;
      if (early_status_out != 0U)
      {
        /* The middleware can already have queued the next IN packet while
         * STATUS OUT was armed for early host termination. DATAEND completes
         * the received status handshake; FFIFO then cancels stale TX/RX ready
         * state before software returns to SETUP ownership. */
        (void)USB_FS_FlushEp0Fifo();
      }
      PCD_FS_EP0ResetTransferState(hpcd);
      PCD_FS_DataOutStageCallback(hpcd, 0U);
      return;
    }

    (void)USB_FS_FlushEp0Fifo();
    PCD_FS_EP0Stall(hpcd);
    return;
  }

  if ((tmpreg != 0U) || (hpcd->ep0_in_pending == 0U))
  {
    return;
  }

  ep = &hpcd->IN_ep[0U];
  hpcd->ep0_in_pending = 0U;

  if (hpcd->ctrl_state == PCD_CTRL_DATA_IN)
  {
    if ((hpcd->ep0_data_in_count > hpcd->ep0_data_in_total) ||
        (ep->xfer_count > ep->xfer_len) ||
        (ep->xfer_count > (hpcd->ep0_data_in_total - hpcd->ep0_data_in_count)))
    {
      PCD_FS_EP0Stall(hpcd);
      return;
    }

    hpcd->ep0_data_in_count += ep->xfer_count;
    PCD_FS_DataInStageCallback(hpcd, 0U);
    return;
  }

  if (hpcd->ctrl_state == PCD_CTRL_STATUS_IN)
  {
    PCD_FS_EP0CompleteStatusIn(hpcd);
  }
}

/**
  * @brief  process EP IN transfer complete interrupt.
  * @param  hpcd PCD handle
  * @param  epnum endpoint number
  * @retval none
  * */
void PCD_FS_TXEP_IRQHandler(PCD_FS_HandleTypeDef *hpcd, uint32_t epnum)
{
  uint8_t tmpreg;
  PCD_FS_EPTypeDef *ep;
  uint32_t remaining;
  uint16_t pkt_len;

  tmpreg = USB_FS->TXCSR1;

  if ((tmpreg & OTG_FS_TXCSR1_STSTALL) == OTG_FS_TXCSR1_STSTALL)
  {
    (void)USB_FS_FlushTxFifo(epnum);
    USB_FS->TXCSR1 &= (~(OTG_FS_TXCSR1_STSTALL | OTG_FS_TXCSR1_SDSTALL));
    ep = &hpcd->IN_ep[epnum];
    ep->xfer_count = ep->xfer_len;
  }
  else if ((tmpreg & OTG_FS_TXCSR1_UNDERRUN) == OTG_FS_TXCSR1_UNDERRUN)
  {
    USB_FS->TXCSR1 &= (~OTG_FS_TXCSR1_UNDERRUN);
    PCD_FS_UNDERRUNCallback(hpcd);
    return;
  }
  else
  {
    ep = &hpcd->IN_ep[epnum];
  }

  remaining = ep->xfer_len - ep->xfer_count;

  if (remaining > 0U)
  {
    /* previous packet sent and more data remains: submit exactly one packet,
     * then return without raising the completion callback */
    pkt_len = (remaining > ep->maxpacket) ? (uint16_t)ep->maxpacket
                                          : (uint16_t)remaining;
    USB_FS_FIFOWrite(ep->xfer_buff, (uint8_t)epnum, pkt_len);
    ep->xfer_buff += pkt_len;
    ep->xfer_count += pkt_len;
    USB_FS->TXCSR1 = OTG_FS_TXCSR1_TXPKTRDY;
    return;
  }

  /* all packets (including any zero-length terminator) have been sent */
  PCD_FS_DataInStageCallback(hpcd, (uint8_t)epnum);
}

/**
  * @brief  process EP OUT transfer complete interrupt.
  * @param  hpcd PCD handle
  * @param  epnum endpoint number
  * @retval none
  * */
void PCD_FS_RXEP_IRQHandler(PCD_FS_HandleTypeDef *hpcd, uint32_t epnum)
{
  uint8_t tmpreg;
  uint16_t byte_count;
  uint32_t remaining;
  PCD_FS_EPTypeDef *ep;

  if (epnum >= hpcd->Init.endpoints)
  {
    return;
  }

  /* bind the endpoint before any dereference */
  ep = &hpcd->OUT_ep[epnum];
  tmpreg = USB_FS->RXCSR1;

  if ((tmpreg & OTG_FS_RXCSR1_STSTALL) == OTG_FS_RXCSR1_STSTALL)
  {
    (void)USB_FS_FlushRxFifo(epnum);   /* flush fifo to halt transaction */
    USB_FS->RXCSR1 &= (~OTG_FS_RXCSR1_STSTALL);
    return;
  }

  /* Overrun: a packet arrived while the FIFO was full. The latched packet
   * must be drained and RXPKTRDY cleared, otherwise the endpoint locks up
   * and the IRQ re-fires forever. Do not write the application buffer and
   * do not advance xfer_count; report the fault and leave the endpoint
   * ready to receive the next OUT. */
  if ((tmpreg & OTG_FS_RXCSR1_OVERRUN) == OTG_FS_RXCSR1_OVERRUN)
  {
    (void)USB_FS_FlushRxFifo(epnum);
    USB_FS->RXCSR1 &= (~OTG_FS_RXCSR1_OVERRUN);
    USB_FS->RXCSR1 &= (~OTG_FS_RXCSR1_RXPKTRDY);
    PCD_FS_OVERRUNCallback(hpcd);
    return;
  }

  if ((tmpreg & OTG_FS_RXCSR1_RXPKTRDY) == 0U)
  {
    return;
  }

  /* Data error: the packet is present but corrupted. Drain it, clear
   * RXPKTRDY, report the fault and drop the packet without delivering it
   * to the class layer. */
  if ((tmpreg & OTG_FS_RXCSR1_DERR) == OTG_FS_RXCSR1_DERR)
  {
    (void)USB_FS_FlushRxFifo(epnum);
    USB_FS->RXCSR1 &= (~OTG_FS_RXCSR1_RXPKTRDY);
    PCD_FS_DERRCallback(hpcd);
    return;
  }

  byte_count = USB_FS_Read_RxCount();

  /* A zero-length packet is a valid OUT completion (e.g. the short-packet
   * terminator required by the USB protocol). It must reach the core even
   * when no application buffer is currently armed, exactly once. */
  if (byte_count == 0U)
  {
    USB_FS->RXCSR1 &= (~OTG_FS_RXCSR1_RXPKTRDY);
    PCD_FS_DataOutStageCallback(hpcd, (uint8_t)epnum);
    return;
  }

  /* bound-check before touching memory: a missing buffer or an oversized
   * packet would overflow the receive window. Drop the packet, flush the
   * FIFO and clear RXPKTRDY so the endpoint can keep receiving instead of
   * locking up forever. */
  if ((ep->xfer_buff == NULL) || (ep->xfer_count > ep->xfer_len))
  {
    (void)USB_FS_FlushRxFifo(epnum);
    USB_FS->RXCSR1 &= (~OTG_FS_RXCSR1_RXPKTRDY);
    return;
  }

  remaining = ep->xfer_len - ep->xfer_count;
  if ((uint32_t)byte_count > remaining)
  {
    (void)USB_FS_FlushRxFifo(epnum);
    USB_FS->RXCSR1 &= (~OTG_FS_RXCSR1_RXPKTRDY);
    return;
  }

  USB_FS_FIFORead(ep->xfer_buff, epnum, byte_count);
  ep->xfer_count += byte_count;

  /* release the packet and notify the upper stack (zero-length too) */
  USB_FS->RXCSR1 &= (~OTG_FS_RXCSR1_RXPKTRDY);
  PCD_FS_DataOutStageCallback(hpcd, (uint8_t)epnum);
}


#endif /* defined (USB_OTG_FS) */


/**
  * @brief  Set Tx FIFO
  * @param  fifo The number of Tx fifo
  * @param  size Fifo size
  * @retval none
  */
void PCD_FS_SetTxFiFo(uint8_t fifo, uint16_t size, uint8_t dpb)
{
  uint8_t i;
  uint32_t Tx_Offset;
  uint8_t fifo_size;
  uint8_t dpb_cfg;

  dpb_cfg = dpb << 4 ;
  /*  TXn min size = 16 words. (n  : Transmit FIFO index)
      When a TxFIFO is not used, the Configuration should be as follows:
          case 1 :  n > m    and Txn is not used    (n,m  : Transmit FIFO indexes)
         --> Txm can use the space allocated for Txn.
         case2  :  n < m    and Txn is not used    (n,m  : Transmit FIFO indexes)
         --> Txn should be configured with the minimum space of 16 words
     The FIFO is used optimally when used TxFIFOs are allocated in the top
         of the FIFO.Ex: use EP1 and EP2 as IN instead of EP1 and EP3 as IN ones.
 */

  Tx_Offset = 0x08;
  if (fifo == 0U)
  {
    return;
  }
  else
  {
    for (i = 1U; i < fifo; i++)
    {
      USB_FS_IndexSel(i);
      fifo_size = USB_FS->TXFIFO2 >> 5 ;
      Tx_Offset += (0x01U << fifo_size);
    }
    USB_FS_IndexSel(fifo);
    USB_FS->TXFIFO1 = Tx_Offset;
    USB_FS->TXFIFO2 = ((usb_log2((size + 7U) / 8U) << 5) | dpb_cfg);
  }
}

/**
  * @brief  Set Rx FIFO
  * @param  size Size of Rx fifo
  * @retval none
  */
void PCD_FS_SetRxFiFo(uint8_t fifo, uint16_t size, uint8_t dpb)
{
  uint8_t i;
  uint32_t Rx_Offset;
  uint8_t fifo_size;
  uint8_t dpb_cfg;

  dpb_cfg = dpb << 4 ;
  Rx_Offset = 0x08;

  if (fifo == 0U)
  {
    return;
  }
  else
  {
    for (i = 1U; i < fifo; i++)
    {
      USB_FS_IndexSel(i);
      fifo_size = USB_FS->RXFIFO2 >> 5 ;
      Rx_Offset += (0x01U << fifo_size);
    }
    USB_FS_IndexSel(fifo);
    USB_FS->RXFIFO1 = Rx_Offset;
    USB_FS->RXFIFO2 = ((usb_log2((size + 7U) / 8U) << 5) | dpb_cfg);
  }
}

/**
  * @}
  */


#endif /* defined (USB_OTG_FS) */
#endif /* PCD_FS_MODULE_ENABLED */
