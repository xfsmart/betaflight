/**
  ******************************************************************************
  * @file    ft32f4xx_i2c.c
  * @author  FT Application Team
  * @brief   I2C module driver.
  *          This file provides firmware functions to manage the following
  *          functionalities of the Inter Integrated Circuit (I2C) peripheral:
  *           + Initialization and de-initialization functions
  *           + IO operation functions
  *           + Peripheral State and Errors functions
  @verbatim
  ==============================================================================
                        ##### How to use this driver #####
  ==============================================================================
    [..]
    The I2C driver can be used as follows:

    (#) Declare a I2C_HandleTypeDef handle structure, for example:
        I2C_HandleTypeDef  hi2c;

    (#)Initialize the I2C low level resources by implementing the @ref I2C_MspInit() API:
        (##) Enable the I2Cx interface clock
        (##) I2C pins configuration
            (+++) Enable the clock for the I2C GPIOs
            (+++) Configure I2C pins as alternate function open-drain
        (##) NVIC configuration if you need to use interrupt process
            (+++) Configure the I2Cx interrupt priority
            (+++) Enable the NVIC I2C IRQ Channel
        (##) DMA Configuration if you need to use DMA process
            (+++) Declare a DMA_HandleTypeDef handle structure for the transmit or receive channel
            (+++) Enable the DMAx interface clock using
            (+++) Configure the DMA handle parameters
            (+++) Configure the DMA Tx or Rx channel
            (+++) Associate the initialized DMA handle to the hi2c DMA Tx or Rx handle
            (+++) Configure the priority and enable the NVIC for the transfer complete interrupt on
                  the DMA Tx or Rx channel

    (#) Configure the Communication Clock Timing, Own Address1, Master Addressing mode, Dual Addressing mode,
        Own Address2, Own Address2 Mask, General call and Nostretch mode in the hi2c Init structure.

    (#) Initialize the I2C registers by calling the @ref I2C_Init(), configures also the low level Hardware
        (GPIO, CLOCK, NVIC...etc) by calling the customized @ref I2C_MspInit(&hi2c) API.

    (#) To check if target device is ready for communication, use the function @ref I2C_IsDeviceReady()

    (#) For I2C IO and IO MEM operations, three operation modes are available within this driver :

    *** Polling mode IO operation ***
    =================================
    [..]
      (+) Transmit in master mode an amount of data in blocking mode using @ref I2C_Master_Transmit()
      (+) Receive in master mode an amount of data in blocking mode using @ref I2C_Master_Receive()
      (+) Transmit in slave mode an amount of data in blocking mode using @ref I2C_Slave_Transmit()
      (+) Receive in slave mode an amount of data in blocking mode using @ref I2C_Slave_Receive()

    *** Polling mode IO MEM operation ***
    =====================================
    [..]
      (+) Write an amount of data in blocking mode to a specific memory address using @ref I2C_Mem_Write()
      (+) Read an amount of data in blocking mode from a specific memory address using @ref I2C_Mem_Read()


    *** Interrupt mode IO operation ***
    ===================================
    [..]
      (+) Transmit in master mode an amount of data in non-blocking mode using @ref I2C_Master_Transmit_IT()
      (+) At transmission end of transfer, @ref I2C_MasterTxCpltCallback() is executed and user can
           add his own code by customization of function pointer @ref I2C_MasterTxCpltCallback()
      (+) Receive in master mode an amount of data in non-blocking mode using @ref I2C_Master_Receive_IT()
      (+) At reception end of transfer, @ref I2C_MasterRxCpltCallback() is executed and user can
           add his own code by customization of function pointer @ref I2C_MasterRxCpltCallback()
      (+) Transmit in slave mode an amount of data in non-blocking mode using @ref I2C_Slave_Transmit_IT()
      (+) At transmission end of transfer, @ref I2C_SlaveTxCpltCallback() is executed and user can
           add his own code by customization of function pointer @ref I2C_SlaveTxCpltCallback()
      (+) Receive in slave mode an amount of data in non-blocking mode using @ref I2C_Slave_Receive_IT()
      (+) At reception end of transfer, @ref I2C_SlaveRxCpltCallback() is executed and user can
           add his own code by customization of function pointer @ref I2C_SlaveRxCpltCallback()
      (+) In case of transfer Error, @ref I2C_ErrorCallback() function is executed and user can
           add his own code by customization of function pointer @ref I2C_ErrorCallback()
      (+) Abort a master I2C process communication with Interrupt using @ref I2C_Master_Abort_IT()
      (+) End of abort process, @ref I2C_AbortCpltCallback() is executed and user can
           add his own code by customization of function pointer @ref I2C_AbortCpltCallback()


    *** Interrupt mode or DMA mode IO sequential operation ***
    ==========================================================
    [..]
      (@) These interfaces allow to manage a sequential transfer with a repeated start condition
          when a direction change during transfer
    [..]
      (+) A specific option field manage the different steps of a sequential transfer
      (+) Option field values are defined through @ref I2C_XFEROPTIONS
      (+) Sequential transmit in master I2C mode an amount of data in non-blocking mode using @ref I2C_Master_Sequential_Transmit_IT()
            or using @ref I2C_Master_Sequential_Transmit_DMA()
      (+) Sequential receive in master I2C mode an amount of data in non-blocking mode using @ref I2C_Master_Sequential_Receive_IT()
            or using @ref I2C_Master_Sequential_Receive_DMA()
      (+) Sequential transmit in slave I2C mode an amount of data in non-blocking mode using @ref I2C_Slave_Sequential_Transmit_IT()
            or using @ref I2C_Slave_Sequential_Transmit_DMA()
      (+) Sequential receive in slave I2C mode an amount of data in non-blocking mode using @ref I2C_Slave_Sequential_Receive_IT()
            or using @ref I2C_Slave_Sequential_Receive_DMA()

  @endverbatim
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 Fremont Micro Devices.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by FT under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ft32f4xx_hal_i2c.h"

/** @addtogroup FT32F4xx_Driver
  * @{
  */

/** @defgroup I2C I2C
  * @brief I2C module driver
  * @{
  */

#ifdef I2C_MODULE_ENABLED

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/** @defgroup I2C_Private_Define I2C Private Define
  * @{
  */
#define TIMING_CLEAR_MASK         (0xF0FFFFFFU)  /*!< I2C TIMING clear register Mask */
#define I2C_TIMEOUT_ADDR          (10000U)       /*!< 10 s  */
#define I2C_TIMEOUT_BUSY          (25U)          /*!< 25 ms */
#define I2C_TIMEOUT_DIR           (25U)          /*!< 25 ms */
#define I2C_TIMEOUT_RXNE          (25U)          /*!< 25 ms */
#define I2C_TIMEOUT_STOPF         (25U)          /*!< 25 ms */
#define I2C_TIMEOUT_TC            (25U)          /*!< 25 ms */
#define I2C_TIMEOUT_TCR           (25U)          /*!< 25 ms */
#define I2C_TIMEOUT_TXIS          (25U)          /*!< 25 ms */
#define I2C_TIMEOUT_FLAG          (25U)          /*!< 25 ms */

#define MAX_NBYTE_SIZE            255U
#define SlaveAddr_SHIFT           7U
#define SlaveAddr_MSK             0x06U

/* Private define for @ref PreviousState usage */
#define I2C_STATE_MSK             ((uint32_t)((uint32_t)((uint32_t)I2C_STATE_BUSY_TX | (uint32_t)I2C_STATE_BUSY_RX) & (uint32_t)(~((uint32_t)I2C_STATE_READY))))
#define I2C_STATE_NONE            ((uint32_t)(I2C_MODE_NONE))
#define I2C_STATE_MASTER_BUSY_TX  ((uint32_t)(((uint32_t)I2C_STATE_BUSY_TX & I2C_STATE_MSK) | (uint32_t)I2C_MODE_MASTER))
#define I2C_STATE_MASTER_BUSY_RX  ((uint32_t)(((uint32_t)I2C_STATE_BUSY_RX & I2C_STATE_MSK) | (uint32_t)I2C_MODE_MASTER))
#define I2C_STATE_SLAVE_BUSY_TX   ((uint32_t)(((uint32_t)I2C_STATE_BUSY_TX & I2C_STATE_MSK) | (uint32_t)I2C_MODE_SLAVE))
#define I2C_STATE_SLAVE_BUSY_RX   ((uint32_t)(((uint32_t)I2C_STATE_BUSY_RX & I2C_STATE_MSK) | (uint32_t)I2C_MODE_SLAVE))
#define I2C_STATE_MEM_BUSY_TX     ((uint32_t)(((uint32_t)I2C_STATE_BUSY_TX & I2C_STATE_MSK) | (uint32_t)I2C_MODE_MEM))
#define I2C_STATE_MEM_BUSY_RX     ((uint32_t)(((uint32_t)I2C_STATE_BUSY_RX & I2C_STATE_MSK) | (uint32_t)I2C_MODE_MEM))


/* Private define to centralize the enable/disable of Interrupts */
#define I2C_XFER_TX_IT            (0x00000001U)
#define I2C_XFER_RX_IT            (0x00000002U)
#define I2C_XFER_LISTEN_IT        (0x00000004U)

#define I2C_XFER_ERROR_IT         (0x00000011U)
#define I2C_XFER_CPLT_IT          (0x00000012U)
#define I2C_XFER_RELOAD_IT        (0x00000012U)

/* Private define Sequential Transfer Options default/reset value */
#define I2C_NO_OPTION_FRAME       (0xFFFF0000U)
/**
  * @}
  */

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/** @defgroup I2C_Private_Functions I2C Private Functions
  * @{
  */
/* Private functions to handle DMA transfer */
static void I2C_DMAMasterTransmitCplt(DMA_HandleTypeDef *hdma);
static void I2C_DMAMasterReceiveCplt(DMA_HandleTypeDef *hdma);
static void I2C_DMASlaveTransmitCplt(DMA_HandleTypeDef *hdma);
static void I2C_DMASlaveReceiveCplt(DMA_HandleTypeDef *hdma);
static void I2C_DMAError(DMA_HandleTypeDef *hdma);
static void I2C_DMAAbort(DMA_HandleTypeDef *hdma);

/* Private functions to handle IT transfer */
static void I2C_ITAddrCplt(I2C_HandleTypeDef *hi2c, uint32_t ITFlags);
static void I2C_ITMasterSeqCplt(I2C_HandleTypeDef *hi2c);
static void I2C_ITSlaveSeqCplt(I2C_HandleTypeDef *hi2c);
static void I2C_ITMasterCplt(I2C_HandleTypeDef *hi2c, uint32_t ITFlags);
static void I2C_ITSlaveCplt(I2C_HandleTypeDef *hi2c, uint32_t ITFlags);
static void I2C_ITListenCplt(I2C_HandleTypeDef *hi2c, uint32_t ITFlags);
static void I2C_ITError(I2C_HandleTypeDef *hi2c, uint32_t ErrorCode);

/* Private functions to handle IT transfer */
static FT_StatusTypeDef I2C_RequestMemoryWrite(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint32_t Timeout, uint32_t Tickstart);
static FT_StatusTypeDef I2C_RequestMemoryRead(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint32_t Timeout, uint32_t Tickstart);

/* Private functions for I2C transfer IRQ handler */
static FT_StatusTypeDef I2C_Master_ISR_IT(struct __I2C_HandleTypeDef *hi2c, uint32_t ITFlags, uint32_t ITSources);
static FT_StatusTypeDef I2C_Slave_ISR_IT(struct __I2C_HandleTypeDef *hi2c, uint32_t ITFlags, uint32_t ITSources);
static FT_StatusTypeDef I2C_Master_ISR_DMA(struct __I2C_HandleTypeDef *hi2c, uint32_t ITFlags, uint32_t ITSources);
static FT_StatusTypeDef I2C_Slave_ISR_DMA(struct __I2C_HandleTypeDef *hi2c, uint32_t ITFlags, uint32_t ITSources);

/* Private functions to handle flags during polling transfer */
static FT_StatusTypeDef I2C_WaitOnFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Flag, FlagStatus Status, uint32_t Timeout, uint32_t Tickstart);
static FT_StatusTypeDef I2C_WaitOnTXISFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart);
static FT_StatusTypeDef I2C_WaitOnRXNEFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart);
static FT_StatusTypeDef I2C_WaitOnSTOPFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart);
static FT_StatusTypeDef I2C_IsAcknowledgeFailed(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart);

/* Private functions to centralize the enable/disable of Interrupts */
static void I2C_Enable_IRQ(I2C_HandleTypeDef *hi2c, uint16_t InterruptRequest);
static void I2C_Disable_IRQ(I2C_HandleTypeDef *hi2c, uint16_t InterruptRequest);

/* Private function to flush TXDR register */
static void I2C_Flush_TXDR(I2C_HandleTypeDef *hi2c);

/* Private function to handle  start, restart or stop a transfer */
static void I2C_TransferConfig(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t Size, uint32_t Mode, uint32_t Request);

/* Private function to Convert Specific options */
static void I2C_ConvertOtherXferOptions(I2C_HandleTypeDef *hi2c);
/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/

/** @defgroup I2C_Exported_Functions I2C Exported Functions
  * @{
  */

/** @defgroup I2C_Exported_Functions_Group1 Initialization and de-initialization functions
  * @brief    Initialization and de-initialization functions
  *
@verbatim
 ===============================================================================
              ##### Initialization and de-initialization functions #####
 ===============================================================================
    [..]  This subsection provides a set of functions allowing to initialize and
          de-initialize the I2Cx peripheral:

      (+) User must Implement I2C_MspInit() function in which he configures
          all related peripherals resources (CLOCK, GPIO, DMA, IT and NVIC).

      (+) Call the function I2C_Init() to configure the device with the declared
          configuration parameters.

      (+) Call the function I2C_DeInit() to restore the default configuration
          of the selected I2Cx peripheral.

@endverbatim
  * @{
  */

/**
  * @brief  Initialize the I2C according to the specified parameters
  *         in the I2C_InitTypeDef and initialize the associated handle.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @retval FT status
  */
FT_StatusTypeDef I2C_Init(I2C_HandleTypeDef *hi2c)
{
  /* Check the I2C handle allocation */
  if (hi2c == NULL)
  {
    return FT_ERROR;
  }

  /* Check the parameters */
  assert_param(IS_I2C_ALL_INSTANCE(hi2c->Instance));
  assert_param(IS_I2C_OWN_ADDRESS1(hi2c->Init.OwnAddress1));
  assert_param(IS_I2C_ADDRESSING_MODE(hi2c->Init.AddressingMode));
  assert_param(IS_I2C_DUAL_ADDRESS(hi2c->Init.DualAddressMode));
  assert_param(IS_I2C_OWN_ADDRESS2(hi2c->Init.OwnAddress2));
  assert_param(IS_I2C_OWN_ADDRESS2_MASK(hi2c->Init.OwnAddress2Masks));
  assert_param(IS_I2C_GENERAL_CALL(hi2c->Init.GeneralCallMode));
  assert_param(IS_I2C_NO_STRETCH(hi2c->Init.NoStretchMode));

  if (hi2c->State == I2C_STATE_RESET)
  {
    /* Allocate lock resource and initialize it */
    hi2c->Lock = FT_UNLOCKED;

#if (USE_I2C_CALLBACKS == 1)
    /* Init the I2C Callback settings */
    hi2c->MasterTxCpltCallback  = I2C_MasterTxCpltCallback;
    hi2c->MasterRxCpltCallback  = I2C_MasterRxCpltCallback;
    hi2c->SlaveTxCpltCallback   = I2C_SlaveTxCpltCallback;
    hi2c->SlaveRxCpltCallback   = I2C_SlaveRxCpltCallback;
    hi2c->ListenCpltCallback    = I2C_ListenCpltCallback;
    hi2c->MemTxCpltCallback     = I2C_MemTxCpltCallback;
    hi2c->MemRxCpltCallback     = I2C_MemRxCpltCallback;
    hi2c->ErrorCallback         = I2C_ErrorCallback;
    hi2c->AbortCpltCallback     = I2C_AbortCpltCallback;
    hi2c->AddrCallback          = I2C_AddrCallback;

    if (hi2c->MspInitCallback == NULL)
    {
      hi2c->MspInitCallback = I2C_MspInit;
    }

    /* Init the low level hardware : GPIO, CLOCK, CORTEX...etc */
    hi2c->MspInitCallback(hi2c);
#else
    /* Init the low level hardware : GPIO, CLOCK, CORTEX...etc */
    I2C_MspInit(hi2c);
#endif /* USE_I2C_CALLBACKS */
  }

  hi2c->State = I2C_STATE_BUSY;

  /* Disable the selected I2C peripheral */
  __I2C_DISABLE(hi2c);

  /*---------------------------- I2Cx TIMINGR Configuration ------------------*/
  hi2c->Instance->TIMINGR = hi2c->Init.Timing & TIMING_CLEAR_MASK;

  /*---------------------------- I2Cx OAR1 Configuration ---------------------*/
  hi2c->Instance->OAR1 &= ~I2C_OAR1_OA1EN;

  if (hi2c->Init.AddressingMode == I2C_ADDRESSINGMODE_7BIT)
  {
    hi2c->Instance->OAR1 = (I2C_OAR1_OA1EN | hi2c->Init.OwnAddress1);
  }
  else
  {
    hi2c->Instance->OAR1 = (I2C_OAR1_OA1EN | I2C_OAR1_OA1MODE | hi2c->Init.OwnAddress1);
  }

  /*---------------------------- I2Cx CR2 Configuration ----------------------*/
  if (hi2c->Init.AddressingMode == I2C_ADDRESSINGMODE_10BIT)
  {
    hi2c->Instance->CR2 = (I2C_CR2_ADD10);
  }
  hi2c->Instance->CR2 |= (I2C_CR2_AUTOEND | I2C_CR2_NACK);

  /*---------------------------- I2Cx OAR2 Configuration ---------------------*/
  hi2c->Instance->OAR2 &= ~I2C_DUALADDRESS_ENABLE;
  hi2c->Instance->OAR2 = (hi2c->Init.DualAddressMode | hi2c->Init.OwnAddress2 | (hi2c->Init.OwnAddress2Masks << 8));

  /*---------------------------- I2Cx CR1 Configuration ----------------------*/
  hi2c->Instance->CR1 = (hi2c->Init.GeneralCallMode | hi2c->Init.NoStretchMode);

  /* Enable the selected I2C peripheral */
  __I2C_ENABLE(hi2c);

  hi2c->ErrorCode = I2C_ERROR_NONE;
  hi2c->State = I2C_STATE_READY;
  hi2c->PreviousState = I2C_STATE_NONE;
  hi2c->Mode = I2C_MODE_NONE;

  return FT_OK;
}

/**
  * @brief  DeInitialize the I2C peripheral.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @retval FT status
  */
FT_StatusTypeDef I2C_DeInit(I2C_HandleTypeDef *hi2c)
{
  /* Check the I2C handle allocation */
  if (hi2c == NULL)
  {
    return FT_ERROR;
  }

  /* Check the parameters */
  assert_param(IS_I2C_ALL_INSTANCE(hi2c->Instance));

  hi2c->State = I2C_STATE_BUSY;

  /* Disable the I2C Peripheral Clock */
  __I2C_DISABLE(hi2c);

#if (USE_I2C_CALLBACKS == 1)
  if (hi2c->MspDeInitCallback == NULL)
  {
    hi2c->MspDeInitCallback = I2C_MspDeInit;
  }
  hi2c->MspDeInitCallback(hi2c);
#else
  I2C_MspDeInit(hi2c);
#endif /* USE_I2C_CALLBACKS */

  hi2c->ErrorCode = I2C_ERROR_NONE;
  hi2c->State = I2C_STATE_RESET;
  hi2c->PreviousState = I2C_STATE_NONE;
  hi2c->Mode = I2C_MODE_NONE;

  /* Release Lock */
  __FT_UNLOCK(hi2c);

  return FT_OK;
}

/**
  * @brief  Initialize the I2C MSP.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @retval None
  */
__weak void I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
  UNUSED(hi2c);
}

/**
  * @brief  DeInitialize the I2C MSP.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @retval None
  */
__weak void I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
  UNUSED(hi2c);
}

/**
  * @}
  */

/** @defgroup I2C_Exported_Functions_Group2 Callback Registration functions
  * @brief    Callback Registration functions
  *
@verbatim
 ===============================================================================
                      ##### Callback Registration functions #####
 ===============================================================================
    [..]
    The compilation flag USE_I2C_CALLBACKS when set to 1
    allows the user to configure dynamically the driver callbacks.
    Use Functions @ref I2C_RegisterCallback() or @ref I2C_RegisterAddrCallback()
    to register an interrupt callback.

    Function @ref I2C_RegisterCallback() allows to register following callbacks:
      (+) MasterTxCpltCallback : callback for Master transmission end of transfer.
      (+) MasterRxCpltCallback : callback for Master reception end of transfer.
      (+) SlaveTxCpltCallback  : callback for Slave transmission end of transfer.
      (+) SlaveRxCpltCallback  : callback for Slave reception end of transfer.
      (+) ListenCpltCallback   : callback for end of listen mode.
      (+) MemTxCpltCallback    : callback for Memory transmission end of transfer.
      (+) MemRxCpltCallback    : callback for Memory reception end of transfer.
      (+) ErrorCallback        : callback for error detection.
      (+) AbortCpltCallback    : callback for abort completion process.
      (+) MspInitCallback      : callback for Msp Init.
      (+) MspDeInitCallback    : callback for Msp DeInit.
    This function takes as parameters the HAL peripheral handle, the Callback ID
    and a pointer to the user callback function.

    For specific callback AddrCallback use dedicated register callbacks : @ref I2C_RegisterAddrCallback().

    Use function @ref I2C_UnRegisterCallback to reset a callback to the default
    weak function.
    @ref I2C_UnRegisterCallback takes as parameters the HAL peripheral handle,
    and the Callback ID.
    This function allows to reset following callbacks:
      (+) MasterTxCpltCallback : callback for Master transmission end of transfer.
      (+) MasterRxCpltCallback : callback for Master reception end of transfer.
      (+) SlaveTxCpltCallback  : callback for Slave transmission end of transfer.
      (+) SlaveRxCpltCallback  : callback for Slave reception end of transfer.
      (+) ListenCpltCallback   : callback for end of listen mode.
      (+) MemTxCpltCallback    : callback for Memory transmission end of transfer.
      (+) MemRxCpltCallback    : callback for Memory reception end of transfer.
      (+) ErrorCallback        : callback for error detection.
      (+) AbortCpltCallback    : callback for abort completion process.
      (+) MspInitCallback      : callback for Msp Init.
      (+) MspDeInitCallback    : callback for Msp DeInit.

    For callback AddrCallback use dedicated register callbacks : @ref I2C_UnRegisterAddrCallback().

    By default, after the @ref I2C_Init() and when the state is @ref I2C_STATE_RESET
    all callbacks are set to the corresponding weak functions:
    examples @ref I2C_MasterTxCpltCallback(), @ref I2C_MasterRxCpltCallback().
    Exception done for MspInit and MspDeInit functions that are
    reset to the legacy weak functions in the @ref I2C_Init()/ @ref I2C_DeInit() only when
    these callbacks are null (not registered beforehand).
    If MspInit or MspDeInit are not null, the @ref I2C_Init()/ @ref I2C_DeInit()
    keep and use the user MspInit/MspDeInit callbacks (registered beforehand) whatever the state.

    Callbacks can be registered/unregistered in @ref I2C_STATE_READY state only.
    Exception done MspInit/MspDeInit functions that can be registered/unregistered
    in @ref I2C_STATE_READY or @ref I2C_STATE_RESET state,
    thus registered (user) MspInit/DeInit callbacks can be used during the Init/DeInit.
    Then, the user first registers the MspInit/MspDeInit user callbacks
    using @ref I2C_RegisterCallback() before calling @ref I2C_DeInit()
    or @ref I2C_Init() function.

    When the compilation flag USE_I2C_CALLBACKS is set to 0 or
    not defined, the callback registration feature is not available and all callbacks
    are set to the corresponding weak functions.

@endverbatim
  * @{
  */

#if (USE_I2C_CALLBACKS == 1)
/**
  * @brief  Register a User I2C Callback
  *         To be used instead of the weak predefined callback
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  CallbackID ID of the callback to be registered
  *         This parameter can be one of the following values:
  *          @arg @ref I2C_MASTER_TX_COMPLETE_CB_ID Master Tx Transfer completed callback ID
  *          @arg @ref I2C_MASTER_RX_COMPLETE_CB_ID Master Rx Transfer completed callback ID
  *          @arg @ref I2C_SLAVE_TX_COMPLETE_CB_ID Slave Tx Transfer completed callback ID
  *          @arg @ref I2C_SLAVE_RX_COMPLETE_CB_ID Slave Rx Transfer completed callback ID
  *          @arg @ref I2C_LISTEN_COMPLETE_CB_ID Listen Complete callback ID
  *          @arg @ref I2C_MEM_TX_COMPLETE_CB_ID Memory Tx Transfer callback ID
  *          @arg @ref I2C_MEM_RX_COMPLETE_CB_ID Memory Rx Transfer completed callback ID
  *          @arg @ref I2C_ERROR_CB_ID Error callback ID
  *          @arg @ref I2C_ABORT_CB_ID Abort callback ID
  *          @arg @ref I2C_MSPINIT_CB_ID MspInit callback ID
  *          @arg @ref I2C_MSPDEINIT_CB_ID MspDeInit callback ID
  * @param  pCallback pointer to the Callback function
  * @retval FT status
  */
FT_StatusTypeDef I2C_RegisterCallback(I2C_HandleTypeDef *hi2c, I2C_CallbackIDTypeDef CallbackID, pI2C_CallbackTypeDef pCallback)
{
  FT_StatusTypeDef status = FT_OK;

  if (pCallback == NULL)
  {
    /* Update the error code */
    hi2c->ErrorCode |= I2C_ERROR_INVALID_CALLBACK;

    return FT_ERROR;
  }

  /* Process locked */
  __FT_LOCK(hi2c);

  if (I2C_STATE_READY == hi2c->State)
  {
    switch (CallbackID)
    {
      case I2C_MASTER_TX_COMPLETE_CB_ID :
        hi2c->MasterTxCpltCallback = pCallback;
        break;

      case I2C_MASTER_RX_COMPLETE_CB_ID :
        hi2c->MasterRxCpltCallback = pCallback;
        break;

      case I2C_SLAVE_TX_COMPLETE_CB_ID :
        hi2c->SlaveTxCpltCallback = pCallback;
        break;

      case I2C_SLAVE_RX_COMPLETE_CB_ID :
        hi2c->SlaveRxCpltCallback = pCallback;
        break;

      case I2C_LISTEN_COMPLETE_CB_ID :
        hi2c->ListenCpltCallback = pCallback;
        break;

      case I2C_MEM_TX_COMPLETE_CB_ID :
        hi2c->MemTxCpltCallback = pCallback;
        break;

      case I2C_MEM_RX_COMPLETE_CB_ID :
        hi2c->MemRxCpltCallback = pCallback;
        break;

      case I2C_ERROR_CB_ID :
        hi2c->ErrorCallback = pCallback;
        break;

      case I2C_ABORT_CB_ID :
        hi2c->AbortCpltCallback = pCallback;
        break;

      case I2C_MSPINIT_CB_ID :
        hi2c->MspInitCallback = pCallback;
        break;

      case I2C_MSPDEINIT_CB_ID :
        hi2c->MspDeInitCallback = pCallback;
        break;

      default :
        /* Update the error code */
        hi2c->ErrorCode |= I2C_ERROR_INVALID_CALLBACK;

        /* Return error status */
        status = FT_ERROR;
        break;
    }
  }
  else if (I2C_STATE_RESET == hi2c->State)
  {
    switch (CallbackID)
    {
      case I2C_MSPINIT_CB_ID :
        hi2c->MspInitCallback = pCallback;
        break;

      case I2C_MSPDEINIT_CB_ID :
        hi2c->MspDeInitCallback = pCallback;
        break;

      default :
        /* Update the error code */
        hi2c->ErrorCode |= I2C_ERROR_INVALID_CALLBACK;

        /* Return error status */
        status = FT_ERROR;
        break;
    }
  }
  else
  {
    /* Update the error code */
    hi2c->ErrorCode |= I2C_ERROR_INVALID_CALLBACK;

    /* Return error status */
    status = FT_ERROR;
  }

  /* Release Lock */
  __FT_UNLOCK(hi2c);

  return status;
}

/**
  * @brief  Unregister an I2C Callback
  *         I2C callback is redirected to the weak predefined callback
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  CallbackID ID of the callback to be unregistered
  *         This parameter can be one of the following values:
  *          @arg @ref I2C_MASTER_TX_COMPLETE_CB_ID Master Tx Transfer completed callback ID
  *          @arg @ref I2C_MASTER_RX_COMPLETE_CB_ID Master Rx Transfer completed callback ID
  *          @arg @ref I2C_SLAVE_TX_COMPLETE_CB_ID Slave Tx Transfer completed callback ID
  *          @arg @ref I2C_SLAVE_RX_COMPLETE_CB_ID Slave Rx Transfer completed callback ID
  *          @arg @ref I2C_LISTEN_COMPLETE_CB_ID Listen Complete callback ID
  *          @arg @ref I2C_MEM_TX_COMPLETE_CB_ID Memory Tx Transfer callback ID
  *          @arg @ref I2C_MEM_RX_COMPLETE_CB_ID Memory Rx Transfer completed callback ID
  *          @arg @ref I2C_ERROR_CB_ID Error callback ID
  *          @arg @ref I2C_ABORT_CB_ID Abort callback ID
  *          @arg @ref I2C_MSPINIT_CB_ID MspInit callback ID
  *          @arg @ref I2C_MSPDEINIT_CB_ID MspDeInit callback ID
  * @retval FT status
  */
FT_StatusTypeDef I2C_UnRegisterCallback(I2C_HandleTypeDef *hi2c, I2C_CallbackIDTypeDef CallbackID)
{
  FT_StatusTypeDef status = FT_OK;

  /* Process locked */
  __FT_LOCK(hi2c);

  if (I2C_STATE_READY == hi2c->State)
  {
    switch (CallbackID)
    {
      case I2C_MASTER_TX_COMPLETE_CB_ID :
        hi2c->MasterTxCpltCallback = I2C_MasterTxCpltCallback;
        break;

      case I2C_MASTER_RX_COMPLETE_CB_ID :
        hi2c->MasterRxCpltCallback = I2C_MasterRxCpltCallback;
        break;

      case I2C_SLAVE_TX_COMPLETE_CB_ID :
        hi2c->SlaveTxCpltCallback = I2C_SlaveTxCpltCallback;
        break;

      case I2C_SLAVE_RX_COMPLETE_CB_ID :
        hi2c->SlaveRxCpltCallback = I2C_SlaveRxCpltCallback;
        break;

      case I2C_LISTEN_COMPLETE_CB_ID :
        hi2c->ListenCpltCallback = I2C_ListenCpltCallback;
        break;

      case I2C_MEM_TX_COMPLETE_CB_ID :
        hi2c->MemTxCpltCallback = I2C_MemTxCpltCallback;
        break;

      case I2C_MEM_RX_COMPLETE_CB_ID :
        hi2c->MemRxCpltCallback = I2C_MemRxCpltCallback;
        break;

      case I2C_ERROR_CB_ID :
        hi2c->ErrorCallback = I2C_ErrorCallback;
        break;

      case I2C_ABORT_CB_ID :
        hi2c->AbortCpltCallback = I2C_AbortCpltCallback;
        break;

      case I2C_MSPINIT_CB_ID :
        hi2c->MspInitCallback = I2C_MspInit;
        break;

      case I2C_MSPDEINIT_CB_ID :
        hi2c->MspDeInitCallback = I2C_MspDeInit;
        break;

      default :
        /* Update the error code */
        hi2c->ErrorCode |= I2C_ERROR_INVALID_CALLBACK;

        /* Return error status */
        status = FT_ERROR;
        break;
    }
  }
  else if (I2C_STATE_RESET == hi2c->State)
  {
    switch (CallbackID)
    {
      case I2C_MSPINIT_CB_ID :
        hi2c->MspInitCallback = I2C_MspInit;
        break;

      case I2C_MSPDEINIT_CB_ID :
        hi2c->MspDeInitCallback = I2C_MspDeInit;
        break;

      default :
        /* Update the error code */
        hi2c->ErrorCode |= I2C_ERROR_INVALID_CALLBACK;

        /* Return error status */
        status = FT_ERROR;
        break;
    }
  }
  else
  {
    /* Update the error code */
    hi2c->ErrorCode |= I2C_ERROR_INVALID_CALLBACK;

    /* Return error status */
    status = FT_ERROR;
  }

  /* Release Lock */
  __FT_UNLOCK(hi2c);

  return status;
}

/**
  * @brief  Register the Slave Address Match I2C Callback
  *         To be used instead of the weak I2C_AddrCallback() predefined callback
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  pCallback pointer to the Address Match Callback function
  * @retval FT status
  */
FT_StatusTypeDef I2C_RegisterAddrCallback(I2C_HandleTypeDef *hi2c, pI2C_AddrCallbackTypeDef pCallback)
{
  FT_StatusTypeDef status = FT_OK;

  if (pCallback == NULL)
  {
    /* Update the error code */
    hi2c->ErrorCode |= I2C_ERROR_INVALID_CALLBACK;

    return FT_ERROR;
  }

  /* Process locked */
  __FT_LOCK(hi2c);

  if (I2C_STATE_READY == hi2c->State)
  {
    hi2c->AddrCallback = pCallback;
  }
  else
  {
    /* Update the error code */
    hi2c->ErrorCode |= I2C_ERROR_INVALID_CALLBACK;

    /* Return error status */
    status = FT_ERROR;
  }

  /* Release Lock */
  __FT_UNLOCK(hi2c);

  return status;
}

/**
  * @brief  Unregister the Slave Address Match I2C Callback
  *         Address Match I2C Callback is redirected to the weak I2C_AddrCallback() predefined callback
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @retval FT status
  */
FT_StatusTypeDef I2C_UnRegisterAddrCallback(I2C_HandleTypeDef *hi2c)
{
  FT_StatusTypeDef status = FT_OK;

  /* Process locked */
  __FT_LOCK(hi2c);

  if (I2C_STATE_READY == hi2c->State)
  {
    hi2c->AddrCallback = I2C_AddrCallback;
  }
  else
  {
    /* Update the error code */
    hi2c->ErrorCode |= I2C_ERROR_INVALID_CALLBACK;

    /* Return error status */
    status = FT_ERROR;
  }

  /* Release Lock */
  __FT_UNLOCK(hi2c);

  return status;
}

#endif /* USE_I2C_CALLBACKS */

/**
  * @}
  */

/** @defgroup I2C_Exported_Functions_Group3 Input and Output operation functions
  * @brief    Input and Output operation functions
  *
@verbatim
 ===============================================================================
                      ##### IO operation functions #####
 ===============================================================================
    [..]
    This subsection provides a set of functions allowing to manage the I2C data
    transfers.

    (#) There are two modes of transfer:
       (++) Blocking mode : The communication is performed in the polling mode.
            The status of all data processing is returned by the same function
            after finishing transfer.
       (++) No-Blocking mode : The communication is performed using Interrupts
            or DMA. These functions return the status of the transfer startup.
            The end of the data processing will be indicated through the
            dedicated I2C IRQ when using Interrupt mode or the DMA IRQ when
            using DMA mode.

    (#) Blocking mode functions are :
        (++) I2C_Master_Transmit()
        (++) I2C_Master_Receive()
        (++) I2C_Slave_Transmit()
        (++) I2C_Slave_Receive()
        (++) I2C_Mem_Write()
        (++) I2C_Mem_Read()
        (++) I2C_IsDeviceReady()

    (#) No-Blocking mode functions with Interrupt are :
        (++) I2C_Master_Transmit_IT()
        (++) I2C_Master_Receive_IT()
        (++) I2C_Slave_Transmit_IT()
        (++) I2C_Slave_Receive_IT()
        (++) I2C_Mem_Write_IT()
        (++) I2C_Mem_Read_IT()
        (++) I2C_Master_Seq_Transmit_IT()
        (++) I2C_Master_Seq_Receive_IT()
        (++) I2C_Slave_Seq_Transmit_IT()
        (++) I2C_Slave_Seq_Receive_IT()
        (++) I2C_EnableListen_IT()
        (++) I2C_DisableListen_IT()
        (++) I2C_Master_Abort_IT()

    (#) No-Blocking mode functions with DMA are :
        (++) I2C_Master_Transmit_DMA()
        (++) I2C_Master_Receive_DMA()
        (++) I2C_Slave_Transmit_DMA()
        (++) I2C_Slave_Receive_DMA()
        (++) I2C_Mem_Write_DMA()
        (++) I2C_Mem_Read_DMA()
        (++) I2C_Master_Seq_Transmit_DMA()
        (++) I2C_Master_Seq_Receive_DMA()
        (++) I2C_Slave_Seq_Transmit_DMA()
        (++) I2C_Slave_Seq_Receive_DMA()

@endverbatim
  * @{
  */

/* Blocking mode: Polling - Master Transmit */
FT_StatusTypeDef I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  uint32_t tickstart;

  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    /* Process Locked */
    __FT_LOCK(hi2c);

    /* Init tickstart for timeout management*/
    tickstart = GetTick();

    hi2c->State     = I2C_STATE_BUSY_TX;
    hi2c->Mode      = I2C_MODE_MASTER;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    /* Prepare transfer parameters */
    hi2c->pBuffPtr  = pData;
    hi2c->XferCount = Size;

    /* Send Slave Address */
    if (hi2c->XferCount > MAX_NBYTE_SIZE)
    {
      hi2c->XferSize = MAX_NBYTE_SIZE;
      I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_RELOAD_MODE, I2C_GENERATE_START_WRITE);
    }
    else
    {
      hi2c->XferSize = hi2c->XferCount;
      I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_AUTOEND_MODE, I2C_GENERATE_START_WRITE);
    }

    while (hi2c->XferCount > 0U)
    {
      /* Wait until TXIS flag is set */
      if (I2C_WaitOnTXISFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
      {
        __FT_UNLOCK(hi2c);
        return FT_ERROR;
      }

      /* Write data to TXDR */
      hi2c->Instance->TXDR = *hi2c->pBuffPtr;

      /* Increment Buffer pointer */
      hi2c->pBuffPtr++;

      hi2c->XferCount--;

      if ((hi2c->XferCount != 0U) && (hi2c->XferSize == 0U))
      {
        /* Wait until TCR flag is set */
        if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_TCR, RESET, Timeout, tickstart) != FT_OK)
        {
          __FT_UNLOCK(hi2c);
          return FT_ERROR;
        }

        if (hi2c->XferCount > MAX_NBYTE_SIZE)
        {
          hi2c->XferSize = MAX_NBYTE_SIZE;
          I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_RELOAD_MODE, I2C_NO_STARTSTOP);
        }
        else
        {
          hi2c->XferSize = hi2c->XferCount;
          I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_AUTOEND_MODE, I2C_NO_STARTSTOP);
        }
      }
    }

    /* Wait until STOPF flag is set */
    if (I2C_WaitOnSTOPFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
    {
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    /* Clear STOP Flag */
    __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);

    /* Set State and Mode */
    hi2c->State = I2C_STATE_READY;
    hi2c->Mode = I2C_MODE_NONE;

    /* Process Unlocked */
    __FT_UNLOCK(hi2c);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
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

/* Blocking mode: Polling - Master Receive */
FT_StatusTypeDef I2C_Master_Receive(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  uint32_t tickstart;

  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);
    tickstart = GetTick();

    hi2c->State     = I2C_STATE_BUSY_RX;
    hi2c->Mode      = I2C_MODE_MASTER;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr  = pData;
    hi2c->XferCount = Size;

    if (hi2c->XferCount > MAX_NBYTE_SIZE)
    {
      hi2c->XferSize = MAX_NBYTE_SIZE;
      I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_RELOAD_MODE, I2C_GENERATE_START_READ);
    }
    else
    {
      hi2c->XferSize = hi2c->XferCount;
      I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_AUTOEND_MODE, I2C_GENERATE_START_READ);
    }

    while (hi2c->XferCount > 0U)
    {
      if (I2C_WaitOnRXNEFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
      {
        __FT_UNLOCK(hi2c);
        return FT_ERROR;
      }

      *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->RXDR;
      hi2c->pBuffPtr++;
      hi2c->XferCount--;

      if ((hi2c->XferCount != 0U) && (hi2c->XferSize == 0U))
      {
        if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_TCR, RESET, Timeout, tickstart) != FT_OK)
        {
          __FT_UNLOCK(hi2c);
          return FT_ERROR;
        }

        if (hi2c->XferCount > MAX_NBYTE_SIZE)
        {
          hi2c->XferSize = MAX_NBYTE_SIZE;
          I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_RELOAD_MODE, I2C_NO_STARTSTOP);
        }
        else
        {
          hi2c->XferSize = hi2c->XferCount;
          I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_AUTOEND_MODE, I2C_NO_STARTSTOP);
        }
      }
    }

    if (I2C_WaitOnSTOPFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
    {
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);

    hi2c->State = I2C_STATE_READY;
    hi2c->Mode = I2C_MODE_NONE;

    __FT_UNLOCK(hi2c);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Blocking mode: Polling - Slave Transmit */
FT_StatusTypeDef I2C_Slave_Transmit(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  uint32_t tickstart;

  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);
    tickstart = GetTick();

    hi2c->State     = I2C_STATE_BUSY_TX;
    hi2c->Mode      = I2C_MODE_SLAVE;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr  = pData;
    hi2c->XferCount = Size;

    hi2c->Instance->CR2 &= ~I2C_CR2_NACK;

    if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, RESET, Timeout, tickstart) != FT_OK)
    {
      hi2c->Instance->CR2 |= I2C_CR2_NACK;
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_ADDR);

    if (hi2c->Init.AddressingMode == I2C_ADDRESSINGMODE_10BIT)
    {
      if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, RESET, Timeout, tickstart) != FT_OK)
      {
        hi2c->Instance->CR2 |= I2C_CR2_NACK;
        __FT_UNLOCK(hi2c);
        return FT_ERROR;
      }
      __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_ADDR);
    }

    if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_DIR, RESET, Timeout, tickstart) != FT_OK)
    {
      hi2c->Instance->CR2 |= I2C_CR2_NACK;
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    while (hi2c->XferCount > 0U)
    {
      if (I2C_WaitOnTXISFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
      {
        hi2c->Instance->CR2 |= I2C_CR2_NACK;
        __FT_UNLOCK(hi2c);
        return FT_ERROR;
      }

      hi2c->Instance->TXDR = *hi2c->pBuffPtr;
      hi2c->pBuffPtr++;
      hi2c->XferCount--;
    }

    if (I2C_WaitOnSTOPFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
    {
      hi2c->Instance->CR2 |= I2C_CR2_NACK;
      if (hi2c->ErrorCode != I2C_ERROR_AF)
      {
        __FT_UNLOCK(hi2c);
        return FT_ERROR;
      }
      hi2c->ErrorCode = I2C_ERROR_NONE;
    }

    __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);
    hi2c->Instance->CR2 |= I2C_CR2_NACK;

    hi2c->State = I2C_STATE_READY;
    hi2c->Mode = I2C_MODE_NONE;

    __FT_UNLOCK(hi2c);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Blocking mode: Polling - Slave Receive */
FT_StatusTypeDef I2C_Slave_Receive(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  uint32_t tickstart;

  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);
    tickstart = GetTick();

    hi2c->State     = I2C_STATE_BUSY_RX;
    hi2c->Mode      = I2C_MODE_SLAVE;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr  = pData;
    hi2c->XferCount = Size;

    hi2c->Instance->CR2 &= ~I2C_CR2_NACK;

    if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, RESET, Timeout, tickstart) != FT_OK)
    {
      hi2c->Instance->CR2 |= I2C_CR2_NACK;
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_ADDR);

    if (hi2c->Init.AddressingMode == I2C_ADDRESSINGMODE_10BIT)
    {
      if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_ADDR, RESET, Timeout, tickstart) != FT_OK)
      {
        hi2c->Instance->CR2 |= I2C_CR2_NACK;
        __FT_UNLOCK(hi2c);
        return FT_ERROR;
      }
      __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_ADDR);
    }

    if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_DIR, RESET, Timeout, tickstart) != FT_OK)
    {
      hi2c->Instance->CR2 |= I2C_CR2_NACK;
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    while (hi2c->XferCount > 0U)
    {
      if (I2C_WaitOnRXNEFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
      {
        hi2c->Instance->CR2 |= I2C_CR2_NACK;
        __FT_UNLOCK(hi2c);
        return FT_ERROR;
      }

      *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->RXDR;
      hi2c->pBuffPtr++;
      hi2c->XferCount--;
    }

    if (I2C_WaitOnSTOPFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
    {
      hi2c->Instance->CR2 |= I2C_CR2_NACK;
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);
    hi2c->Instance->CR2 |= I2C_CR2_NACK;

    hi2c->State = I2C_STATE_READY;
    hi2c->Mode = I2C_MODE_NONE;

    __FT_UNLOCK(hi2c);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Blocking mode: Polling - Memory Write */
FT_StatusTypeDef I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  uint32_t tickstart;

  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);
    tickstart = GetTick();

    hi2c->State     = I2C_STATE_BUSY_TX;
    hi2c->Mode      = I2C_MODE_MEM;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr  = pData;
    hi2c->XferCount = Size;

    if (I2C_RequestMemoryWrite(hi2c, DevAddress, MemAddress, MemAddSize, Timeout, tickstart) != FT_OK)
    {
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    while (hi2c->XferCount > 0U)
    {
      if (I2C_WaitOnTXISFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
      {
        __FT_UNLOCK(hi2c);
        return FT_ERROR;
      }

      hi2c->Instance->TXDR = *hi2c->pBuffPtr;
      hi2c->pBuffPtr++;
      hi2c->XferCount--;

      if ((hi2c->XferCount != 0U) && (hi2c->XferSize == 0U))
      {
        if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_TCR, RESET, Timeout, tickstart) != FT_OK)
        {
          __FT_UNLOCK(hi2c);
          return FT_ERROR;
        }

        if (hi2c->XferCount > MAX_NBYTE_SIZE)
        {
          hi2c->XferSize = MAX_NBYTE_SIZE;
          I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_RELOAD_MODE, I2C_NO_STARTSTOP);
        }
        else
        {
          hi2c->XferSize = hi2c->XferCount;
          I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_AUTOEND_MODE, I2C_NO_STARTSTOP);
        }
      }
    }

    if (I2C_WaitOnSTOPFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
    {
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);

    hi2c->State = I2C_STATE_READY;
    hi2c->Mode = I2C_MODE_NONE;

    __FT_UNLOCK(hi2c);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Blocking mode: Polling - Memory Read */
FT_StatusTypeDef I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  uint32_t tickstart;

  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);
    tickstart = GetTick();

    hi2c->State     = I2C_STATE_BUSY_RX;
    hi2c->Mode      = I2C_MODE_MEM;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr  = pData;
    hi2c->XferCount = Size;

    if (I2C_RequestMemoryRead(hi2c, DevAddress, MemAddress, MemAddSize, Timeout, tickstart) != FT_OK)
    {
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    while (hi2c->XferCount > 0U)
    {
      if (I2C_WaitOnRXNEFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
      {
        __FT_UNLOCK(hi2c);
        return FT_ERROR;
      }

      *hi2c->pBuffPtr = (uint8_t)hi2c->Instance->RXDR;
      hi2c->pBuffPtr++;
      hi2c->XferCount--;

      if ((hi2c->XferCount != 0U) && (hi2c->XferSize == 0U))
      {
        if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_TCR, RESET, Timeout, tickstart) != FT_OK)
        {
          __FT_UNLOCK(hi2c);
          return FT_ERROR;
        }

        if (hi2c->XferCount > MAX_NBYTE_SIZE)
        {
          hi2c->XferSize = MAX_NBYTE_SIZE;
          I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_RELOAD_MODE, I2C_NO_STARTSTOP);
        }
        else
        {
          hi2c->XferSize = hi2c->XferCount;
          I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_AUTOEND_MODE, I2C_NO_STARTSTOP);
        }
      }
    }

    if (I2C_WaitOnSTOPFlagUntilTimeout(hi2c, Timeout, tickstart) != FT_OK)
    {
      __FT_UNLOCK(hi2c);
      return FT_ERROR;
    }

    __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);

    hi2c->State = I2C_STATE_READY;
    hi2c->Mode = I2C_MODE_NONE;

    __FT_UNLOCK(hi2c);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Device Ready Check */
FT_StatusTypeDef I2C_IsDeviceReady(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint32_t Trials, uint32_t Timeout)
{
  uint32_t tickstart;
  __IO uint32_t I2C_Trials = 0UL;
  FlagStatus tmp1;
  FlagStatus tmp2;

  if (hi2c->State == I2C_STATE_READY)
  {
    if (__I2C_GET_FLAG(hi2c, I2C_FLAG_BUSY) == SET)
    {
      return FT_BUSY;
    }

    __FT_LOCK(hi2c);

    hi2c->State = I2C_STATE_BUSY;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    do
    {
      hi2c->Instance->CR2 = I2C_GENERATE_START(hi2c->Init.AddressingMode, DevAddress);

      tickstart = GetTick();

      tmp1 = __I2C_GET_FLAG(hi2c, I2C_FLAG_STOPF);
      tmp2 = __I2C_GET_FLAG(hi2c, I2C_FLAG_AF);

      while ((tmp1 == RESET) && (tmp2 == RESET))
      {
        if (Timeout != 0xFFFFFFFFU)
        {
          if (((GetTick() - tickstart) > Timeout) || (Timeout == 0U))
          {
            hi2c->State = I2C_STATE_READY;
            hi2c->ErrorCode |= I2C_ERROR_TIMEOUT;
            __FT_UNLOCK(hi2c);
            return FT_ERROR;
          }
        }

        tmp1 = __I2C_GET_FLAG(hi2c, I2C_FLAG_STOPF);
        tmp2 = __I2C_GET_FLAG(hi2c, I2C_FLAG_AF);
      }

      if (__I2C_GET_FLAG(hi2c, I2C_FLAG_AF) == RESET)
      {
        if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_STOPF, RESET, Timeout, tickstart) != FT_OK)
        {
          return FT_ERROR;
        }

        __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);

        hi2c->State = I2C_STATE_READY;
        __FT_UNLOCK(hi2c);

        return FT_OK;
      }
      else
      {
        if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_STOPF, RESET, Timeout, tickstart) != FT_OK)
        {
          return FT_ERROR;
        }

        __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_AF);
        __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);
      }

      if (I2C_Trials == Trials)
      {
        hi2c->Instance->CR2 |= I2C_CR2_STOP;

        if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_STOPF, RESET, Timeout, tickstart) != FT_OK)
        {
          return FT_ERROR;
        }

        __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);

        hi2c->State = I2C_STATE_READY;
        hi2c->ErrorCode |= I2C_ERROR_TIMEOUT;
        __FT_UNLOCK(hi2c);

        return FT_ERROR;
      }

      I2C_Trials++;

    } while (hi2c->State == I2C_STATE_BUSY);

    hi2c->State = I2C_STATE_READY;
    hi2c->ErrorCode |= I2C_ERROR_TIMEOUT;
    __FT_UNLOCK(hi2c);

    return FT_ERROR;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Non-Blocking mode: Interrupt - Master Transmit */
FT_StatusTypeDef I2C_Master_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);

    hi2c->State     = I2C_STATE_BUSY_TX;
    hi2c->Mode      = I2C_MODE_MASTER;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr  = pData;
    hi2c->XferCount = Size;

    if (hi2c->XferCount > MAX_NBYTE_SIZE)
    {
      hi2c->XferSize = MAX_NBYTE_SIZE;
      I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_RELOAD_MODE, I2C_GENERATE_START_WRITE);
    }
    else
    {
      hi2c->XferSize = hi2c->XferCount;
      I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_AUTOEND_MODE, I2C_GENERATE_START_WRITE);
    }

    __I2C_ENABLE_IT(hi2c, I2C_IT_TXI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ERRI);

    __FT_UNLOCK(hi2c);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Non-Blocking mode: Interrupt - Master Receive */
FT_StatusTypeDef I2C_Master_Receive_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);

    hi2c->State     = I2C_STATE_BUSY_RX;
    hi2c->Mode      = I2C_MODE_MASTER;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr  = pData;
    hi2c->XferCount = Size;

    if (hi2c->XferCount > MAX_NBYTE_SIZE)
    {
      hi2c->XferSize = MAX_NBYTE_SIZE;
      I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_RELOAD_MODE, I2C_GENERATE_START_READ);
    }
    else
    {
      hi2c->XferSize = hi2c->XferCount;
      I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, I2C_AUTOEND_MODE, I2C_GENERATE_START_READ);
    }

    __I2C_ENABLE_IT(hi2c, I2C_IT_RXI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ERRI);

    __FT_UNLOCK(hi2c);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Non-Blocking mode: Interrupt - Slave Transmit */
FT_StatusTypeDef I2C_Slave_Transmit_IT(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size)
{
  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);

    hi2c->State     = I2C_STATE_BUSY_TX;
    hi2c->Mode      = I2C_MODE_SLAVE;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr  = pData;
    hi2c->XferCount = Size;

    hi2c->Instance->CR2 &= ~I2C_CR2_NACK;

    __I2C_ENABLE_IT(hi2c, I2C_IT_TXI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ERRI);

    __FT_UNLOCK(hi2c);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Non-Blocking mode: Interrupt - Slave Receive */
FT_StatusTypeDef I2C_Slave_Receive_IT(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size)
{
  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);

    hi2c->State     = I2C_STATE_BUSY_RX;
    hi2c->Mode      = I2C_MODE_SLAVE;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr  = pData;
    hi2c->XferCount = Size;

    hi2c->Instance->CR2 &= ~I2C_CR2_NACK;

    __I2C_ENABLE_IT(hi2c, I2C_IT_RXI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ERRI);

    __FT_UNLOCK(hi2c);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Sequential Transfer - Master Sequential Transmit IT */
FT_StatusTypeDef I2C_Master_Sequential_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t XferOptions)
{
  uint32_t xfermode;
  uint32_t xferrequest = I2C_GENERATE_START_WRITE;

  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);

    hi2c->State     = I2C_STATE_BUSY_TX;
    hi2c->Mode      = I2C_MODE_MASTER;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr    = pData;
    hi2c->XferCount   = Size;
    hi2c->XferOptions = XferOptions;

    if (hi2c->XferCount > MAX_NBYTE_SIZE)
    {
      hi2c->XferSize = MAX_NBYTE_SIZE;
      xfermode = I2C_RELOAD_MODE;
    }
    else
    {
      hi2c->XferSize = hi2c->XferCount;
      xfermode = hi2c->XferOptions;
    }

    if ((hi2c->PreviousState == I2C_STATE_MASTER_BUSY_TX) && (IS_I2C_TRANSFER_OTHER_OPTIONS_REQUEST(XferOptions) == 0))
    {
      xferrequest = I2C_NO_STARTSTOP;
    }
    else
    {
      I2C_ConvertOtherXferOptions(hi2c);
      if (hi2c->XferCount < MAX_NBYTE_SIZE)
      {
        xfermode = hi2c->XferOptions;
      }
    }

    I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, xfermode, xferrequest);

    __FT_UNLOCK(hi2c);

    __I2C_ENABLE_IT(hi2c, I2C_IT_TXI | I2C_IT_TCI);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Sequential Transfer - Master Sequential Receive IT */
FT_StatusTypeDef I2C_Master_Sequential_Receive_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t XferOptions)
{
  uint32_t xfermode;
  uint32_t xferrequest = I2C_GENERATE_START_READ;

  if (hi2c->State == I2C_STATE_READY)
  {
    if ((pData == NULL) || (Size == 0U))
    {
      hi2c->ErrorCode = I2C_ERROR_INVALID_PARAM;
      return FT_ERROR;
    }

    __FT_LOCK(hi2c);

    hi2c->State     = I2C_STATE_BUSY_RX;
    hi2c->Mode      = I2C_MODE_MASTER;
    hi2c->ErrorCode = I2C_ERROR_NONE;

    hi2c->pBuffPtr    = pData;
    hi2c->XferCount   = Size;
    hi2c->XferOptions = XferOptions;

    if (hi2c->XferCount > MAX_NBYTE_SIZE)
    {
      hi2c->XferSize = MAX_NBYTE_SIZE;
      xfermode = I2C_RELOAD_MODE;
    }
    else
    {
      hi2c->XferSize = hi2c->XferCount;
      xfermode = hi2c->XferOptions;
    }

    if ((hi2c->PreviousState == I2C_STATE_MASTER_BUSY_RX) && (IS_I2C_TRANSFER_OTHER_OPTIONS_REQUEST(XferOptions) == 0))
    {
      xferrequest = I2C_NO_STARTSTOP;
    }
    else
    {
      I2C_ConvertOtherXferOptions(hi2c);
      if (hi2c->XferCount < MAX_NBYTE_SIZE)
      {
        xfermode = hi2c->XferOptions;
      }
    }

    I2C_TransferConfig(hi2c, DevAddress, (uint8_t)hi2c->XferSize, xfermode, xferrequest);

    __FT_UNLOCK(hi2c);

    __I2C_ENABLE_IT(hi2c, I2C_IT_RXI | I2C_IT_TCI);

    return FT_OK;
  }
  else
  {
    return FT_BUSY;
  }
}

/* Private functions */
static FT_StatusTypeDef I2C_WaitOnFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Flag, FlagStatus Status, uint32_t Timeout, uint32_t Tickstart)
{
  while (__I2C_GET_FLAG(hi2c, Flag) == Status)
  {
    if (Timeout != 0xFFFFFFFFU)
    {
      if (((GetTick() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        hi2c->ErrorCode |= I2C_ERROR_TIMEOUT;
        hi2c->State = I2C_STATE_READY;
        hi2c->Mode = I2C_MODE_NONE;
        return FT_ERROR;
      }
    }
  }
  return FT_OK;
}

static FT_StatusTypeDef I2C_WaitOnTXISFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  while (__I2C_GET_FLAG(hi2c, I2C_FLAG_TXIS) == RESET)
  {
    if (Timeout != 0xFFFFFFFFU)
    {
      if (((GetTick() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        hi2c->ErrorCode |= I2C_ERROR_TIMEOUT;
        hi2c->State = I2C_STATE_READY;
        return FT_ERROR;
      }
    }
  }
  return FT_OK;
}

static FT_StatusTypeDef I2C_WaitOnRXNEFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  while (__I2C_GET_FLAG(hi2c, I2C_FLAG_RXNE) == RESET)
  {
    if (Timeout != 0xFFFFFFFFU)
    {
      if (((GetTick() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        hi2c->ErrorCode |= I2C_ERROR_TIMEOUT;
        hi2c->State = I2C_STATE_READY;
        return FT_ERROR;
      }
    }
  }
  return FT_OK;
}

static FT_StatusTypeDef I2C_WaitOnSTOPFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  while (__I2C_GET_FLAG(hi2c, I2C_FLAG_STOPF) == RESET)
  {
    if (Timeout != 0xFFFFFFFFU)
    {
      if (((GetTick() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        hi2c->ErrorCode |= I2C_ERROR_TIMEOUT;
        hi2c->State = I2C_STATE_READY;
        return FT_ERROR;
      }
    }
  }
  return FT_OK;
}

static FT_StatusTypeDef I2C_IsAcknowledgeFailed(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  FT_StatusTypeDef status = FT_OK;

  if (__I2C_GET_FLAG(hi2c, I2C_FLAG_NACKF) == SET)
  {
    __I2C_CLEAR_FLAG(hi2c, I2C_FLAG_NACKF);
    hi2c->ErrorCode |= I2C_ERROR_AF;
    hi2c->State = I2C_STATE_READY;
    hi2c->Mode = I2C_MODE_NONE;
    status = FT_ERROR;
  }

  return status;
}

static FT_StatusTypeDef I2C_WaitOnTCFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  while (__I2C_GET_FLAG(hi2c, I2C_FLAG_TC) == RESET)
  {
    if (Timeout != 0xFFFFFFFFU)
    {
      if (((GetTick() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        hi2c->ErrorCode |= I2C_ERROR_TIMEOUT;
        hi2c->State = I2C_STATE_READY;
        return FT_ERROR;
      }
    }
  }
  return FT_OK;
}

static void I2C_TransferConfig(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t Size, uint32_t Mode, uint32_t Request)
{
  uint32_t tmpreg;

  tmpreg = hi2c->Instance->CR2;
  tmpreg &= ~(I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RELOAD | I2C_CR2_AUTOEND | I2C_CR2_START | I2C_CR2_STOP);

  tmpreg |= ((uint32_t)DevAddress << 1) & I2C_CR2_SADD;
  tmpreg |= ((uint32_t)Size << I2C_CR2_NBYTES_Pos) & I2C_CR2_NBYTES;
  tmpreg |= Mode | Request;

  hi2c->Instance->CR2 = tmpreg;
}

static FT_StatusTypeDef I2C_RequestMemoryWrite(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint32_t Timeout, uint32_t Tickstart)
{
  I2C_TransferConfig(hi2c, DevAddress, 0U, I2C_SOFTEND_MODE, I2C_GENERATE_START_WRITE);

  if (I2C_WaitOnTXISFlagUntilTimeout(hi2c, Timeout, Tickstart) != FT_OK)
  {
    return FT_ERROR;
  }

  if (MemAddSize == I2C_MEMADD_SIZE_8BIT)
  {
    hi2c->Instance->TXDR = (uint8_t)MemAddress;
  }
  else
  {
    hi2c->Instance->TXDR = (uint8_t)(MemAddress >> 8U);

    if (I2C_WaitOnTXISFlagUntilTimeout(hi2c, Timeout, Tickstart) != FT_OK)
    {
      return FT_ERROR;
    }

    hi2c->Instance->TXDR = (uint8_t)MemAddress;
  }

  return I2C_WaitOnTCFlagUntilTimeout(hi2c, Timeout, Tickstart);
}

static FT_StatusTypeDef I2C_RequestMemoryRead(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint32_t Timeout, uint32_t Tickstart)
{
  I2C_TransferConfig(hi2c, DevAddress, 0U, I2C_SOFTEND_MODE, I2C_GENERATE_START_WRITE);

  if (I2C_WaitOnTXISFlagUntilTimeout(hi2c, Timeout, Tickstart) != FT_OK)
  {
    return FT_ERROR;
  }

  if (MemAddSize == I2C_MEMADD_SIZE_8BIT)
  {
    hi2c->Instance->TXDR = (uint8_t)MemAddress;
  }
  else
  {
    hi2c->Instance->TXDR = (uint8_t)(MemAddress >> 8U);

    if (I2C_WaitOnTXISFlagUntilTimeout(hi2c, Timeout, Tickstart) != FT_OK)
    {
      return FT_ERROR;
    }

    hi2c->Instance->TXDR = (uint8_t)MemAddress;
  }

  if (I2C_WaitOnTCFlagUntilTimeout(hi2c, Timeout, Tickstart) != FT_OK)
  {
    return FT_ERROR;
  }

  I2C_TransferConfig(hi2c, DevAddress, 0U, I2C_SOFTEND_MODE, I2C_GENERATE_START_READ);

  return FT_OK;
}

static void I2C_ConvertOtherXferOptions(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->XferOptions == I2C_OTHER_FRAME)
  {
    hi2c->XferOptions = I2C_NEXT_FRAME;
  }
  else if (hi2c->XferOptions == I2C_OTHER_AND_LAST_FRAME)
  {
    hi2c->XferOptions = I2C_LAST_FRAME;
  }
}

/**
  * @}
  */

/**
  * @}
  */

#endif /* I2C_MODULE_ENABLED */

/**
  * @}
  */

/**
  * @}
  */
