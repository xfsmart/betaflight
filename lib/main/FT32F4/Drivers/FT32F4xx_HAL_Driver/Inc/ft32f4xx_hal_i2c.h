/**
  ******************************************************************************
  * @file    ft32f4xx_i2c.h
  * @author  FT Application Team
  * @brief   Header file of I2C module.
  *          This file provides all the I2C firmware functions.
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

    *** I2C Callbacks registration ***
    ==================================
    [..]
    The compilation define USE_I2C_CALLBACKS in the header file can be set to 1
    to enable the callback registration feature.
    [..]
    When callback registration is enabled, user can register specific callback functions
    for various I2C events using @ref I2C_RegisterCallback() and @ref I2C_RegisterAddrCallback().
    [..]
    To unregister a callback, use the function @ref I2C_UnRegisterCallback().

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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef FT32F4xx_I2C_H
#define FT32F4xx_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ft32f4xx_def.h"
#include "ft32f4xx_hal_gpio.h"

/** @addtogroup FT32F4xx_Driver
  * @{
  */

/** @addtogroup I2C
  * @{
  */

/* Exported types ------------------------------------------------------------*/
/** @defgroup I2C_Exported_Types I2C Exported Types
  * @{
  */

/** @defgroup I2C_Configuration_Structure_definition I2C Configuration Structure definition
  * @brief  I2C Configuration Structure definition
  * @{
  */
typedef struct
{
  uint32_t Timing;              /*!< Specifies the I2C_TIMINGR_register value.
                                  This parameter calculated by referring to I2C initialization
                                         section in Reference manual */

  uint32_t OwnAddress1;         /*!< Specifies the first device own address.
                                  This parameter can be a 7-bit or 10-bit address. */

  uint32_t AddressingMode;      /*!< Specifies if 7-bit or 10-bit addressing mode is selected.
                                  This parameter can be a value of @ref I2C_ADDRESSING_MODE */

  uint32_t DualAddressMode;     /*!< Specifies if dual addressing mode is selected.
                                  This parameter can be a value of @ref I2C_DUAL_ADDRESSING_MODE */

  uint32_t OwnAddress2;         /*!< Specifies the second device own address if dual addressing mode is selected
                                  This parameter can be a 7-bit address. */

  uint32_t OwnAddress2Masks;    /*!< Specifies the acknowledge mask address second device own address if dual addressing mode is selected
                                  This parameter can be a value of @ref I2C_OWN_ADDRESS2_MASKS */

  uint32_t GeneralCallMode;     /*!< Specifies if general call mode is selected.
                                  This parameter can be a value of @ref I2C_GENERAL_CALL_ADDRESSING_MODE */

  uint32_t NoStretchMode;       /*!< Specifies if nostretch mode is selected.
                                  This parameter can be a value of @ref I2C_NOSTRETCH_MODE */

} I2C_InitTypeDef;

/**
  * @}
  */

/** @defgroup I2C_State_structure_definition I2C State structure definition
  * @brief  I2C State structure definition
  * @note   I2C State value coding follow below described bitmap :\n
  *          b7-b6  Error information\n
  *             00 : No Error\n
  *             01 : Abort (Abort user request on going)\n
  *             10 : Timeout\n
  *             11 : Error\n
  *          b5     Peripheral initialization status\n
  *             0  : Reset (peripheral not initialized)\n
  *             1  : Init done (peripheral initialized and ready to use)\n
  *          b4     (not used)\n
  *          b3\n
  *             0  : Ready or Busy (No Listen mode ongoing)\n
  *             1  : Listen (peripheral in Address Listen Mode)\n
  *          b2     Intrinsic process state\n
  *             0  : Ready\n
  *             1  : Busy (peripheral busy with some configuration or internal operations)\n
  *          b1     Rx state\n
  *             0  : Ready (no Rx operation ongoing)\n
  *             1  : Busy (Rx operation ongoing)\n
  *          b0     Tx state\n
  *             0  : Ready (no Tx operation ongoing)\n
  *             1  : Busy (Tx operation ongoing)
  * @{
  */
typedef enum
{
  I2C_STATE_RESET             = 0x00U,   /*!< Peripheral is not yet Initialized         */
  I2C_STATE_READY             = 0x20U,   /*!< Peripheral Initialized and ready for use  */
  I2C_STATE_BUSY              = 0x24U,   /*!< An internal process is ongoing            */
  I2C_STATE_BUSY_TX           = 0x21U,   /*!< Data Transmission process is ongoing      */
  I2C_STATE_BUSY_RX           = 0x22U,   /*!< Data Reception process is ongoing         */
  I2C_STATE_LISTEN            = 0x28U,   /*!< Address Listen Mode is ongoing            */
  I2C_STATE_BUSY_TX_LISTEN    = 0x29U,   /*!< Address Listen Mode and Data Transmission
                                                 process is ongoing                         */
  I2C_STATE_BUSY_RX_LISTEN    = 0x2AU,   /*!< Address Listen Mode and Data Reception
                                                 process is ongoing                         */
  I2C_STATE_ABORT             = 0x60U,   /*!< Abort user request ongoing                */
  I2C_STATE_TIMEOUT           = 0xA0U,   /*!< Timeout state                             */
  I2C_STATE_ERROR             = 0xE0U,   /*!< Error                                     */
  I2C_STATE_NONE              = 0x00U    /*!< No state                                  */

} I2C_StateTypeDef;

/**
  * @}
  */

/** @defgroup I2C_Mode_structure_definition I2C Mode structure definition
  * @brief  I2C Mode structure definition
  * @note   I2C Mode value coding follow below described bitmap :\n
  *          b7     (not used)\n
  *          b6\n
  *             0  : None\n
  *             1  : Memory (I2C communication is in Memory Mode)\n
  *          b5\n
  *             0  : None\n
  *             1  : Slave (I2C communication is in Slave Mode)\n
  *          b4\n
  *             0  : None\n
  *             1  : Master (I2C communication is in Master Mode)\n
  *          b3-b2-b1-b0  (not used)
  * @{
  */
typedef enum
{
  I2C_MODE_NONE               = 0x00U,   /*!< No I2C communication on going             */
  I2C_MODE_MASTER             = 0x10U,   /*!< I2C communication is in Master Mode       */
  I2C_MODE_SLAVE              = 0x20U,   /*!< I2C communication is in Slave Mode        */
  I2C_MODE_MEM                = 0x40U    /*!< I2C communication is in Memory Mode       */

} I2C_ModeTypeDef;

/**
  * @}
  */

/** @defgroup I2C_Error_Code_definition I2C Error Code definition
  * @{
  */
#define I2C_ERROR_NONE      (0x00000000U)    /*!< No error              */
#define I2C_ERROR_BERR      (0x00000001U)    /*!< BERR error            */
#define I2C_ERROR_ARLO      (0x00000002U)    /*!< ARLO error            */
#define I2C_ERROR_AF        (0x00000004U)    /*!< ACKF error            */
#define I2C_ERROR_OVR       (0x00000008U)    /*!< OVR error             */
#define I2C_ERROR_DMA       (0x00000010U)    /*!< DMA transfer error    */
#define I2C_ERROR_TIMEOUT   (0x00000020U)    /*!< Timeout error         */
#define I2C_ERROR_SIZE      (0x00000040U)    /*!< Size Management error */
#define I2C_ERROR_DMA_PARAM (0x00000080U)    /*!< DMA Parameter Error   */
#define I2C_ERROR_INVALID_PARAM     (0x00000200U)    /*!< Invalid Parameters error  */
/**
  * @}
  */

/** @defgroup I2C_XferOptions_definition I2C Sequential Transfer Options
  * @{
  */
#define I2C_FIRST_FRAME                 ((uint32_t)I2C_SOFTEND_MODE)
#define I2C_FIRST_AND_NEXT_FRAME        ((uint32_t)(I2C_RELOAD_MODE | I2C_SOFTEND_MODE))
#define I2C_NEXT_FRAME                  ((uint32_t)(I2C_RELOAD_MODE | I2C_SOFTEND_MODE))
#define I2C_FIRST_AND_LAST_FRAME        ((uint32_t)I2C_AUTOEND_MODE)
#define I2C_LAST_FRAME                  ((uint32_t)I2C_AUTOEND_MODE)
#define I2C_LAST_FRAME_NO_STOP          ((uint32_t)I2C_SOFTEND_MODE)
#define I2C_OTHER_FRAME                 (0x000000AAU)
#define I2C_OTHER_AND_LAST_FRAME        (0x0000AA00U)
/**
  * @}
  */

/** @defgroup I2C_handle_Structure_definition I2C handle Structure definition
  * @brief  I2C handle Structure definition
  * @{
  */
typedef struct __I2C_HandleTypeDef
{
  I2C_TypeDef                *Instance;      /*!< I2C registers base address                */

  I2C_InitTypeDef            Init;           /*!< I2C communication parameters              */

  uint8_t                    *pBuffPtr;      /*!< Pointer to I2C transfer buffer            */

  uint16_t                   XferSize;       /*!< I2C transfer size                         */

  __IO uint16_t              XferCount;      /*!< I2C transfer counter                      */

  __IO uint32_t              XferOptions;    /*!< I2C sequential transfer options, this parameter can
                                                  be a value of @ref I2C_XferOptions_definition */

  __IO uint32_t              PreviousState;  /*!< I2C communication Previous state          */

  DMA_HandleTypeDef          *hdmatx;        /*!< I2C Tx DMA handle parameters              */

  DMA_HandleTypeDef          *hdmarx;        /*!< I2C Rx DMA handle parameters              */

  FT_LockTypeDef             Lock;           /*!< I2C locking object                        */

  __IO I2C_StateTypeDef      State;          /*!< I2C communication state                   */

  __IO I2C_ModeTypeDef       Mode;           /*!< I2C communication mode                    */

  __IO uint32_t              ErrorCode;      /*!< I2C Error code                            */

  __IO uint32_t              AddrEventCount; /*!< I2C Address Event counter                 */

#if (USE_I2C_CALLBACKS == 1)
  void (* MasterTxCpltCallback)(struct __I2C_HandleTypeDef *hi2c);           /*!< I2C Master Tx Transfer completed callback */
  void (* MasterRxCpltCallback)(struct __I2C_HandleTypeDef *hi2c);           /*!< I2C Master Rx Transfer completed callback */
  void (* SlaveTxCpltCallback)(struct __I2C_HandleTypeDef *hi2c);            /*!< I2C Slave Tx Transfer completed callback  */
  void (* SlaveRxCpltCallback)(struct __I2C_HandleTypeDef *hi2c);            /*!< I2C Slave Rx Transfer completed callback  */
  void (* ListenCpltCallback)(struct __I2C_HandleTypeDef *hi2c);             /*!< I2C Listen Complete callback              */
  void (* MemTxCpltCallback)(struct __I2C_HandleTypeDef *hi2c);              /*!< I2C Memory Tx Transfer completed callback */
  void (* MemRxCpltCallback)(struct __I2C_HandleTypeDef *hi2c);              /*!< I2C Memory Rx Transfer completed callback */
  void (* ErrorCallback)(struct __I2C_HandleTypeDef *hi2c);                  /*!< I2C Error callback                        */
  void (* AbortCpltCallback)(struct __I2C_HandleTypeDef *hi2c);              /*!< I2C Abort callback                        */

  void (* AddrCallback)(struct __I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode);  /*!< I2C Slave Address Match callback */

#endif /* USE_I2C_CALLBACKS */

} I2C_HandleTypeDef;

#if (USE_I2C_CALLBACKS == 1)
/**
  * @brief  I2C Callback ID enumeration definition
  */
typedef enum
{
  I2C_MASTER_TX_COMPLETE_CB_ID      = 0x00U,    /*!< I2C Master Tx Transfer completed callback ID  */
  I2C_MASTER_RX_COMPLETE_CB_ID      = 0x01U,    /*!< I2C Master Rx Transfer completed callback ID  */
  I2C_SLAVE_TX_COMPLETE_CB_ID       = 0x02U,    /*!< I2C Slave Tx Transfer completed callback ID   */
  I2C_SLAVE_RX_COMPLETE_CB_ID       = 0x03U,    /*!< I2C Slave Rx Transfer completed callback ID   */
  I2C_LISTEN_COMPLETE_CB_ID         = 0x04U,    /*!< I2C Listen Complete callback ID               */
  I2C_MEM_TX_COMPLETE_CB_ID         = 0x05U,    /*!< I2C Memory Tx Transfer callback ID            */
  I2C_MEM_RX_COMPLETE_CB_ID         = 0x06U,    /*!< I2C Memory Rx Transfer completed callback ID  */
  I2C_ERROR_CB_ID                   = 0x07U,    /*!< I2C Error callback ID                         */
  I2C_ABORT_CB_ID                   = 0x08U,    /*!< I2C Abort callback ID                         */

  I2C_MSPINIT_CB_ID                 = 0x09U,    /*!< I2C Msp Init callback ID                      */
  I2C_MSPDEINIT_CB_ID               = 0x0AU     /*!< I2C Msp DeInit callback ID                    */

} I2C_CallbackIDTypeDef;

/**
  * @brief  I2C Callback pointer definition
  */
typedef  void (*pI2C_CallbackTypeDef)(I2C_HandleTypeDef *hi2c); /*!< pointer to an I2C callback function */
typedef  void (*pI2C_AddrCallbackTypeDef)(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode); /*!< pointer to an I2C Address Match callback function */

#endif /* USE_I2C_CALLBACKS */
/**
  * @}
  */

/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup I2C_Exported_Constants I2C Exported Constants
  * @{
  */

/** @defgroup I2C_ADDRESSING_MODE I2C Addressing Mode
  * @{
  */
#define I2C_ADDRESSINGMODE_7BIT         (0x00000001U)
#define I2C_ADDRESSINGMODE_10BIT        (0x00000002U)
/**
  * @}
  */

/** @defgroup I2C_DUAL_ADDRESSING_MODE I2C Dual Addressing Mode
  * @{
  */
#define I2C_DUALADDRESS_DISABLE         (0x00000000U)
#define I2C_DUALADDRESS_ENABLE          I2C_OAR2_OA2EN
/**
  * @}
  */

/** @defgroup I2C_OWN_ADDRESS2_MASKS I2C Own Address2 Masks
  * @{
  */
#define I2C_OA2_NOMASK                  ((uint8_t)0x00U)
#define I2C_OA2_MASK01                  ((uint8_t)0x01U)
#define I2C_OA2_MASK02                  ((uint8_t)0x02U)
#define I2C_OA2_MASK03                  ((uint8_t)0x03U)
#define I2C_OA2_MASK04                  ((uint8_t)0x04U)
#define I2C_OA2_MASK05                  ((uint8_t)0x05U)
#define I2C_OA2_MASK06                  ((uint8_t)0x06U)
#define I2C_OA2_MASK07                  ((uint8_t)0x07U)
/**
  * @}
  */

/** @defgroup I2C_GENERAL_CALL_ADDRESSING_MODE I2C General Call Addressing Mode
  * @{
  */
#define I2C_GENERALCALL_DISABLE         (0x00000000U)
#define I2C_GENERALCALL_ENABLE          I2C_CR1_GCEN
/**
  * @}
  */

/** @defgroup I2C_NOSTRETCH_MODE I2C No-Stretch Mode
  * @{
  */
#define I2C_NOSTRETCH_DISABLE           (0x00000000U)
#define I2C_NOSTRETCH_ENABLE            I2C_CR1_NOSTRETCH
/**
  * @}
  */

/** @defgroup I2C_DUTYCYCLE I2C Duty Cycle (for Fast Mode)
  * @{
  */
#define I2C_DUTYCYCLE_2                 (0x00000000U)  /*!< Tlow/Thigh = 2/1 */
#define I2C_DUTYCYCLE_16_9              (0x00000001U)  /*!< Tlow/Thigh = 16/9 */
/**
  * @}
  */

/** @defgroup I2C_MEMORY_ADDRESS_SIZE I2C Memory Address Size
  * @{
  */
#define I2C_MEMADD_SIZE_8BIT            (0x00000001U)
#define I2C_MEMADD_SIZE_16BIT           (0x00000002U)
/**
  * @}
  */

/** @defgroup I2C_XFERDIRECTION I2C Transfer Direction Master Point of View
  * @{
  */
#define I2C_DIRECTION_TRANSMIT          (0x00000000U)
#define I2C_DIRECTION_RECEIVE           (0x00000001U)
/**
  * @}
  */

/** @defgroup I2C_RELOAD_END_MODE I2C Reload End Mode
  * @{
  */
#define I2C_RELOAD_MODE                 I2C_CR2_RELOAD
#define I2C_AUTOEND_MODE                I2C_CR2_AUTOEND
#define I2C_SOFTEND_MODE                (0x00000000U)
/**
  * @}
  */

/** @defgroup I2C_START_STOP_MODE I2C Start or Stop Mode
  * @{
  */
#define I2C_NO_STARTSTOP                (0x00000000U)
#define I2C_GENERATE_STOP               (uint32_t)(0x80000000U | I2C_CR2_STOP)
#define I2C_GENERATE_START_READ         (uint32_t)(0x80000000U | I2C_CR2_START | I2C_CR2_RD_WRN)
#define I2C_GENERATE_START_WRITE        (uint32_t)(0x80000000U | I2C_CR2_START)
/**
  * @}
  */

/** @defgroup I2C_Interrupt_configuration_definition I2C Interrupt configuration definition
  * @{
  */
#define I2C_IT_ERRI                     I2C_CR1_ERRIE
#define I2C_IT_TCI                      I2C_CR1_TCIE
#define I2C_IT_STOPI                    I2C_CR1_STOPIE
#define I2C_IT_NACKI                    I2C_CR1_NACKIE
#define I2C_IT_ADDRI                    I2C_CR1_ADDRIE
#define I2C_IT_RXI                      I2C_CR1_RXIE
#define I2C_IT_TXI                      I2C_CR1_TXIE
/**
  * @}
  */

/** @defgroup I2C_Flag_definition I2C Flag definition
  * @{
  */
#define I2C_FLAG_TXE                    I2C_ISR_TXE
#define I2C_FLAG_TXIS                   I2C_ISR_TXIS
#define I2C_FLAG_RXNE                   I2C_ISR_RXNE
#define I2C_FLAG_ADDR                   I2C_ISR_ADDR
#define I2C_FLAG_NACKF                  I2C_ISR_NACKF
#define I2C_FLAG_AF                     I2C_ISR_NACKF       /* Alias for NACKF (Acknowledge Failure) - for compatibility */
#define I2C_FLAG_STOPF                  I2C_ISR_STOPF
#define I2C_FLAG_TC                     I2C_ISR_TC
#define I2C_FLAG_TCR                    I2C_ISR_TCR
#define I2C_FLAG_BERR                   I2C_ISR_BERR
#define I2C_FLAG_ARLO                   I2C_ISR_ARLO
#define I2C_FLAG_OVR                    I2C_ISR_OVR
#define I2C_FLAG_PECERR                 I2C_ISR_PECERR
#define I2C_FLAG_TIMEOUT                I2C_ISR_TIMEOUT
#define I2C_FLAG_ALERT                  I2C_ISR_ALERT
#define I2C_FLAG_BUSY                   I2C_ISR_BUSY
#define I2C_FLAG_DIR                    I2C_ISR_DIR
/**
  * @}
  */

/** @defgroup I2C_Max_Nbyte_Size I2C Maximum Nbyte Size
  * @{
  */
#define MAX_NBYTE_SIZE                  255U
/**
  * @}
  */

/** @defgroup I2C_CR2_RESET_Mask I2C CR2 Reset Mask
  * @{
  */
#define I2C_CR2_RESET_MASK              (~I2C_CR2_SADD)
/**
  * @}
  */

/** @defgroup I2C_Instance_definition I2C Instance definition
  * @{
  */
#define IS_I2C_ALL_INSTANCE(INSTANCE) (((INSTANCE) == I2C1) || \
                                       ((INSTANCE) == I2C2) || \
                                       ((INSTANCE) == I2C3))
/**
  * @}
  */

/**
  * @}
  */

/* Exported macros -----------------------------------------------------------*/
/** @defgroup I2C_Exported_Macros I2C Exported Macros
  * @{
  */

/** @brief Reset I2C handle state.
  * @param  __HANDLE__ specifies the I2C Handle.
  * @retval None
  */
#define __I2C_RESET_HANDLE_STATE(__HANDLE__)                ((__HANDLE__)->State = I2C_STATE_RESET)

/** @brief  Enable the specified I2C interrupt.
  * @param  __HANDLE__ specifies the I2C Handle.
  * @param  __INTERRUPT__ specifies the interrupt source to enable.
  *         This parameter can be one of the following values:
  *            @arg I2C_IT_ERRI: Errors interrupt enable
  *            @arg I2C_IT_TCI: Transfer complete interrupt enable
  *            @arg I2C_IT_STOPI: STOP detection interrupt enable
  *            @arg I2C_IT_NACKI: NACK received interrupt enable
  *            @arg I2C_IT_ADDRI: Address match interrupt enable
  *            @arg I2C_IT_RXI: RX interrupt enable
  *            @arg I2C_IT_TXI: TX interrupt enable
  * @retval None
  */
#define __I2C_ENABLE_IT(__HANDLE__, __INTERRUPT__)          ((__HANDLE__)->Instance->CR1 |= (__INTERRUPT__))

/** @brief  Disable the specified I2C interrupt.
  * @param  __HANDLE__ specifies the I2C Handle.
  * @param  __INTERRUPT__ specifies the interrupt source to disable.
  *         This parameter can be one of the following values:
  *            @arg I2C_IT_ERRI: Errors interrupt enable
  *            @arg I2C_IT_TCI: Transfer complete interrupt enable
  *            @arg I2C_IT_STOPI: STOP detection interrupt enable
  *            @arg I2C_IT_NACKI: NACK received interrupt enable
  *            @arg I2C_IT_ADDRI: Address match interrupt enable
  *            @arg I2C_IT_RXI: RX interrupt enable
  *            @arg I2C_IT_TXI: TX interrupt enable
  * @retval None
  */
#define __I2C_DISABLE_IT(__HANDLE__, __INTERRUPT__)         ((__HANDLE__)->Instance->CR1 &= (~(__INTERRUPT__)))

/** @brief  Check whether the specified I2C interrupt source is enabled or not.
  * @param  __HANDLE__ specifies the I2C Handle.
  * @param  __INTERRUPT__ specifies the I2C interrupt source to check.
  *          This parameter can be one of the following values:
  *            @arg I2C_IT_ERRI: Errors interrupt enable
  *            @arg I2C_IT_TCI: Transfer complete interrupt enable
  *            @arg I2C_IT_STOPI: STOP detection interrupt enable
  *            @arg I2C_IT_NACKI: NACK received interrupt enable
  *            @arg I2C_IT_ADDRI: Address match interrupt enable
  *            @arg I2C_IT_RXI: RX interrupt enable
  *            @arg I2C_IT_TXI: TX interrupt enable
  * @retval The new state of the interrupt source (SET or RESET).
  */
#define __I2C_GET_IT_SOURCE(__HANDLE__, __INTERRUPT__)      ((((__HANDLE__)->Instance->CR1 & (__INTERRUPT__)) == (__INTERRUPT__)) ? SET : RESET)

/** @brief  Check whether the specified I2C flag is set or not.
  * @param  __HANDLE__ specifies the I2C Handle.
  * @param  __FLAG__ specifies the flag to check.
  *         This parameter can be one of the following values:
  *            @arg I2C_FLAG_TXE: Transmit data register empty
  *            @arg I2C_FLAG_TXIS: Transmit interrupt status
  *            @arg I2C_FLAG_RXNE: Receive data register not empty
  *            @arg I2C_FLAG_ADDR: Address matched (slave mode)
  *            @arg I2C_FLAG_NACKF: NACK received flag
  *            @arg I2C_FLAG_STOPF: STOP detection flag
  *            @arg I2C_FLAG_TC: Transfer complete
  *            @arg I2C_FLAG_TCR: Transfer complete reload
  *            @arg I2C_FLAG_BERR: Bus error
  *            @arg I2C_FLAG_ARLO: Arbitration lost
  *            @arg I2C_FLAG_OVR: Overrun/Underrun
  *            @arg I2C_FLAG_PECERR: PEC error in reception
  *            @arg I2C_FLAG_TIMEOUT: Timeout or Tlow error
  *            @arg I2C_FLAG_ALERT: SMBus alert
  *            @arg I2C_FLAG_BUSY: Bus busy
  *            @arg I2C_FLAG_DIR: Transfer direction (slave mode)
  * @retval The new state of the flag (SET or RESET).
  */
#define __I2C_GET_FLAG(__HANDLE__, __FLAG__) (((((__HANDLE__)->Instance->ISR) & (__FLAG__)) == (__FLAG__)) ? SET : RESET)

/** @brief  Clear the I2C pending flags which are cleared by writing 1 in a specific bit.
  * @param  __HANDLE__ specifies the I2C Handle.
  * @param  __FLAG__ specifies the flag to clear.
  *         This parameter can be any combination of the following values:
  *            @arg I2C_FLAG_ADDR: Address matched (slave mode)
  *            @arg I2C_FLAG_NACKF: NACK received flag
  *            @arg I2C_FLAG_STOPF: STOP detection flag
  *            @arg I2C_FLAG_BERR: Bus error
  *            @arg I2C_FLAG_ARLO: Arbitration lost
  *            @arg I2C_FLAG_OVR: Overrun/Underrun
  *            @arg I2C_FLAG_PECERR: PEC error in reception
  *            @arg I2C_FLAG_TIMEOUT: Timeout or Tlow error
  *            @arg I2C_FLAG_ALERT: SMBus alert
  * @retval None
  */
#define __I2C_CLEAR_FLAG(__HANDLE__, __FLAG__) (((__HANDLE__)->Instance->ICR) = (__FLAG__))

/** @brief  Enable the specified I2C peripheral.
  * @param  __HANDLE__ specifies the I2C Handle.
  * @retval None
  */
#define __I2C_ENABLE(__HANDLE__)                            (SET_BIT((__HANDLE__)->Instance->CR1, I2C_CR1_PE))

/** @brief  Disable the specified I2C peripheral.
  * @param  __HANDLE__ specifies the I2C Handle.
  * @retval None
  */
#define __I2C_DISABLE(__HANDLE__)                           (CLEAR_BIT((__HANDLE__)->Instance->CR1, I2C_CR1_PE))

/** @brief  Reset I2C CR2 register.
  * @param  __HANDLE__ specifies the I2C Handle.
  * @retval None
  */
#define I2C_RESET_CR2(__HANDLE__)                           ((__HANDLE__)->Instance->CR2 &= ~I2C_CR2_RESET_MASK)

/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/
/** @addtogroup I2C_Exported_Functions
  * @{
  */

/** @addtogroup I2C_Exported_Functions_Group1 Initialization and de-initialization functions
  * @{
  */
/* Initialization and de-initialization functions  ****************************/
FT_StatusTypeDef I2C_Init(I2C_HandleTypeDef *hi2c);
FT_StatusTypeDef I2C_DeInit(I2C_HandleTypeDef *hi2c);
void I2C_MspInit(I2C_HandleTypeDef *hi2c);
void I2C_MspDeInit(I2C_HandleTypeDef *hi2c);

/* Timing calculation helper function */
uint32_t I2C_CalculateTiming(uint32_t PCLK1, uint32_t I2C_Speed, uint32_t I2C_DutyCycle);

#if (USE_I2C_CALLBACKS == 1)
FT_StatusTypeDef I2C_RegisterCallback(I2C_HandleTypeDef *hi2c, I2C_CallbackIDTypeDef CallbackID, pI2C_CallbackTypeDef pCallback);
FT_StatusTypeDef I2C_UnRegisterCallback(I2C_HandleTypeDef *hi2c, I2C_CallbackIDTypeDef CallbackID);
FT_StatusTypeDef I2C_RegisterAddrCallback(I2C_HandleTypeDef *hi2c, pI2C_AddrCallbackTypeDef pCallback);
FT_StatusTypeDef I2C_UnRegisterAddrCallback(I2C_HandleTypeDef *hi2c);
#endif /* USE_I2C_CALLBACKS */
/**
  * @}
  */

/** @addtogroup I2C_Exported_Functions_Group2 Input and Output operation functions
  * @{
  */
/* IO operation functions  ****************************************************/
/******* Blocking mode: Polling */
FT_StatusTypeDef I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout);
FT_StatusTypeDef I2C_Master_Receive(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout);
FT_StatusTypeDef I2C_Slave_Transmit(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size, uint32_t Timeout);
FT_StatusTypeDef I2C_Slave_Receive(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size, uint32_t Timeout);
FT_StatusTypeDef I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);
FT_StatusTypeDef I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);
FT_StatusTypeDef I2C_IsDeviceReady(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint32_t Trials, uint32_t Timeout);

/******* Non-Blocking mode: Interrupt */
FT_StatusTypeDef I2C_Master_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size);
FT_StatusTypeDef I2C_Master_Receive_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size);
FT_StatusTypeDef I2C_Slave_Transmit_IT(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size);
FT_StatusTypeDef I2C_Slave_Receive_IT(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size);
FT_StatusTypeDef I2C_Mem_Write_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size);
FT_StatusTypeDef I2C_Mem_Read_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size);

FT_StatusTypeDef I2C_Master_Sequential_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t XferOptions);
FT_StatusTypeDef I2C_Master_Sequential_Receive_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t XferOptions);
FT_StatusTypeDef I2C_Slave_Sequential_Transmit_IT(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size, uint32_t XferOptions);
FT_StatusTypeDef I2C_Slave_Sequential_Receive_IT(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size, uint32_t XferOptions);
FT_StatusTypeDef I2C_EnableListen_IT(I2C_HandleTypeDef *hi2c);
FT_StatusTypeDef I2C_DisableListen_IT(I2C_HandleTypeDef *hi2c);
FT_StatusTypeDef I2C_Master_Abort_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress);

/******* Non-Blocking mode: DMA */
FT_StatusTypeDef I2C_Master_Transmit_DMA(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size);
FT_StatusTypeDef I2C_Master_Receive_DMA(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size);
FT_StatusTypeDef I2C_Slave_Transmit_DMA(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size);
FT_StatusTypeDef I2C_Slave_Receive_DMA(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size);
FT_StatusTypeDef I2C_Mem_Write_DMA(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size);
FT_StatusTypeDef I2C_Mem_Read_DMA(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData, uint16_t Size);

FT_StatusTypeDef I2C_Master_Sequential_Transmit_DMA(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t XferOptions);
FT_StatusTypeDef I2C_Master_Sequential_Receive_DMA(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t XferOptions);
FT_StatusTypeDef I2C_Slave_Sequential_Transmit_DMA(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size, uint32_t XferOptions);
FT_StatusTypeDef I2C_Slave_Sequential_Receive_DMA(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size, uint32_t XferOptions);
/**
  * @}
  */

/** @addtogroup I2C_Exported_Functions_Group3 Peripheral State and Errors functions
  * @{
  */
/* Peripheral State and Errors functions  ****************************/
I2C_StateTypeDef I2C_GetState(I2C_HandleTypeDef *hi2c);
uint32_t         I2C_GetError(I2C_HandleTypeDef *hi2c);
/**
  * @}
  */

/** @addtogroup I2C_Exported_Functions_Group4 IRQ Handler and Callbacks
  * @{
  */
/* IRQ Handler and Callbacks  ****************************/
void I2C_EV_IRQHandler(I2C_HandleTypeDef *hi2c);
void I2C_ER_IRQHandler(I2C_HandleTypeDef *hi2c);

#if (USE_I2C_CALLBACKS == 1)
void I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c);
void I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c);
void I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c);
void I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c);
void I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode);
void I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c);
void I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c);
void I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c);
void I2C_ErrorCallback(I2C_HandleTypeDef *hi2c);
void I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c);
#endif /* USE_I2C_CALLBACKS */
/**
  * @}
  */

/**
  * @}
  */

/* Private macros ------------------------------------------------------------*/
/** @defgroup I2C_Private_Macros I2C Private Macros
  * @{
  */
#define IS_I2C_ADDRESSING_MODE(MODE) (((MODE) == I2C_ADDRESSINGMODE_7BIT) || \
                                      ((MODE) == I2C_ADDRESSINGMODE_10BIT))

#define IS_I2C_DUAL_ADDRESS(ADDRESS) (((ADDRESS) == I2C_DUALADDRESS_DISABLE) || \
                                      ((ADDRESS) == I2C_DUALADDRESS_ENABLE))

#define IS_I2C_OWN_ADDRESS1(ADDRESS1)             ((ADDRESS1) <= 0x000003FFU)
#define IS_I2C_OWN_ADDRESS2(ADDRESS2)             ((ADDRESS2) <= (uint16_t)0x00FFU)

#define IS_I2C_OWN_ADDRESS2_MASK(MASK) (((MASK) == I2C_OA2_NOMASK)  || \
                                        ((MASK) == I2C_OA2_MASK01) || \
                                        ((MASK) == I2C_OA2_MASK02) || \
                                        ((MASK) == I2C_OA2_MASK03) || \
                                        ((MASK) == I2C_OA2_MASK04) || \
                                        ((MASK) == I2C_OA2_MASK05) || \
                                        ((MASK) == I2C_OA2_MASK06) || \
                                        ((MASK) == I2C_OA2_MASK07))

#define IS_I2C_GENERAL_CALL(CALL) (((CALL) == I2C_GENERALCALL_DISABLE) || \
                                   ((CALL) == I2C_GENERALCALL_ENABLE))

#define IS_I2C_NO_STRETCH(STRETCH) (((STRETCH) == I2C_NOSTRETCH_DISABLE) || \
                                    ((STRETCH) == I2C_NOSTRETCH_ENABLE))

#define IS_I2C_MEMADD_SIZE(SIZE) (((SIZE) == I2C_MEMADD_SIZE_8BIT) || \
                                  ((SIZE) == I2C_MEMADD_SIZE_16BIT))

#define IS_I2C_TRANSFER_OPTIONS_REQUEST(REQUEST) (((REQUEST) == I2C_FIRST_FRAME) || \
                                                  ((REQUEST) == I2C_FIRST_AND_NEXT_FRAME) || \
                                                  ((REQUEST) == I2C_NEXT_FRAME) || \
                                                  ((REQUEST) == I2C_FIRST_AND_LAST_FRAME) || \
                                                  ((REQUEST) == I2C_LAST_FRAME) || \
                                                  ((REQUEST) == I2C_LAST_FRAME_NO_STOP) || \
                                                  ((REQUEST) == I2C_OTHER_FRAME) || \
                                                  ((REQUEST) == I2C_OTHER_AND_LAST_FRAME))

#define IS_I2C_TRANSFER_OTHER_OPTIONS_REQUEST(REQUEST) (((REQUEST) == I2C_OTHER_FRAME)     || \
                                                        ((REQUEST) == I2C_OTHER_AND_LAST_FRAME))
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* FT32F4xx_I2C_H */
