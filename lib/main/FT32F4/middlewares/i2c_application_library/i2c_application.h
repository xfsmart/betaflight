/**
  **************************************************************************
  * @file     i2c_application.h
  * @brief    i2c application libray header file
  **************************************************************************
  *                       Copyright notice & Disclaimer
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

#ifndef __I2C_APPLICATION_H
#define __I2C_APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

/* includes ------------------------------------------------------------------*/
#include "ft32f4xx.h"

/** @addtogroup FT32F4_middlewares_i2c_application_library
  * @{
  */

#define MAX_TRANSFER_CNT               I2C_MAX_TRANSFER_BYTES

/** @defgroup I2C_library_flags_mapping
  * @brief I2C flag mappings - AT32F43x style names to FT32F4 standard lib
  * @{
  */

#define I2C_TDBE_FLAG                  I2C_FLAG_TXE
#define I2C_TDIS_FLAG                  I2C_FLAG_TXIS
#define I2C_RDBF_FLAG                  I2C_FLAG_RXNE
#define I2C_ADDRF_FLAG                 I2C_FLAG_ADDR
#define I2C_ACKFAIL_FLAG               I2C_FLAG_NACKF
#define I2C_STOPF_FLAG                 I2C_FLAG_STOPF
#define I2C_TDC_FLAG                   I2C_FLAG_TC
#define I2C_TCRLD_FLAG                 I2C_FLAG_TCR
#define I2C_BUSERR_FLAG                I2C_FLAG_BERR
#define I2C_ARLOST_FLAG                I2C_FLAG_ARLO
#define I2C_OUF_FLAG                   I2C_FLAG_OVR
#define I2C_PECERR_FLAG                I2C_FLAG_PECERR
#define I2C_TMOUT_FLAG                 I2C_FLAG_TIMEOUT
#define I2C_ALERTF_FLAG                I2C_FLAG_ALERT
#define I2C_BUSYF_FLAG                 I2C_FLAG_BUSY

/**
  * @}
  */

/** @defgroup I2C_library_event_check_flag
  * @{
  */

#define I2C_EVENT_CHECK_NONE           ((uint32_t)0x00000000)
#define I2C_EVENT_CHECK_ACKFAIL        ((uint32_t)0x00000001)
#define I2C_EVENT_CHECK_STOP           ((uint32_t)0x00000002)

/**
  * @}
  */

/** @defgroup I2C_library_memory_address_width_mode
  * @{
  */

typedef enum
{
  I2C_MEM_ADDR_WIDIH_8                 = 0x01,
  I2C_MEM_ADDR_WIDIH_16                = 0x02,
} i2c_mem_address_width_type;

/**
  * @}
  */

/** @defgroup I2C_library_transmission_mode
  * @{
  */

typedef enum
{
  I2C_MA_TX = 0,
  I2C_MA_RX,
  I2C_INT_MA_TX,
  I2C_INT_MA_RX,
  I2C_INT_SLA_TX,
  I2C_INT_SLA_RX,
  I2C_DMA_MA_TX,
  I2C_DMA_MA_RX,
  I2C_DMA_SLA_TX,
  I2C_DMA_SLA_RX,
} i2c_mode_type;

/**
  * @}
  */

/** @defgroup I2C_library_status_code
  * @{
  */

typedef enum
{
  I2C_OK = 0,
  I2C_ERR_STEP_1,
  I2C_ERR_STEP_2,
  I2C_ERR_STEP_3,
  I2C_ERR_STEP_4,
  I2C_ERR_STEP_5,
  I2C_ERR_STEP_6,
  I2C_ERR_STEP_7,
  I2C_ERR_STEP_8,
  I2C_ERR_STEP_9,
  I2C_ERR_STEP_10,
  I2C_ERR_STEP_11,
  I2C_ERR_STEP_12,
  I2C_ERR_TCRLD,
  I2C_ERR_TDC,
  I2C_ERR_ADDR,
  I2C_ERR_STOP,
  I2C_ERR_ACKFAIL,
  I2C_ERR_TIMEOUT,
  I2C_ERR_INTERRUPT,
} i2c_status_type;

/**
  * @}
  */

/** @defgroup I2C_library_handler
  * @{
  */

typedef enum {
	I2C_START,
	I2C_END
} i2cState_t;

typedef enum {
	I2C_STEP_REG,
	I2C_STEP_DATA,
	I2C_STEP_COUNT
} i2cStep_t;

/**
  * @brief i2c handle type definition
  */
typedef struct
{
  I2C_TypeDef                            *i2cx;
  uint16_t                               reg;
  uint16_t                               addr;                    /*!< cached slave address */
  uint8_t                                *pbuff[I2C_STEP_COUNT];
  __IO uint16_t                          psize;
  __IO uint16_t                          pcount[I2C_STEP_COUNT];
  __IO uint32_t                          mode;
  __IO i2cStep_t                         step;
  __IO i2cState_t                        state;
  __IO i2c_status_type                   error_code;
  __IO uint8_t                           arbitration_lost;
  __IO uint8_t                           master_started;
  DMA_Channel_TypeDef                    *dma_tx_channel;
  DMA_Channel_TypeDef                    *dma_rx_channel;
  DMA_InitTypeDef                        dma_init_struct;
} i2c_handle_type;

/**
  * @}
  */

/** @defgroup I2C_library_exported_functions
  * @{
  */

/* Helper functions */
uint32_t        i2c_flag_get              (I2C_TypeDef* i2c_x, uint32_t flag);
void            i2c_flag_clear            (I2C_TypeDef* i2c_x, uint32_t flag);

void            i2c_config                (i2c_handle_type* hi2c);
void            i2c_lowlevel_init         (i2c_handle_type* hi2c);
void            i2c_reset_ctrl2_register  (i2c_handle_type* hi2c);
i2c_status_type i2c_wait_flag             (i2c_handle_type* hi2c, uint32_t flag, uint32_t event_check, uint32_t timeout);

i2c_status_type i2c_master_transmit       (i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_master_receive        (i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_slave_transmit        (i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_slave_receive         (i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout);

i2c_status_type i2c_master_transmit_int   (i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_master_receive_int    (i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_slave_transmit_int    (i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_slave_receive_int     (i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout);

i2c_status_type i2c_master_transmit_dma   (i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_master_receive_dma    (i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_slave_transmit_dma    (i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_slave_receive_dma     (i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout);

i2c_status_type i2c_smbus_master_transmit (i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_smbus_master_receive  (i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_smbus_slave_transmit  (i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_smbus_slave_receive   (i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout);

i2c_status_type i2c_memory_write          (i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_memory_write_int      (i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_memory_write_dma      (i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_memory_read           (i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_memory_read_int       (i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout);
i2c_status_type i2c_memory_read_dma       (i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout);

void            i2c_evt_irq_handler       (i2c_handle_type* hi2c);
void            i2c_err_irq_handler       (i2c_handle_type* hi2c);
void            i2c_dma_tx_irq_handler    (i2c_handle_type* hi2c);
void            i2c_dma_rx_irq_handler    (i2c_handle_type* hi2c);

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif
