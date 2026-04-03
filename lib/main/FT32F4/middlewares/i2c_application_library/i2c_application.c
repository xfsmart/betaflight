/**
  **************************************************************************
  * @file     i2c_application.c
  * @brief    the driver library of the i2c peripheral
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

#include "i2c_application.h"
#include "drivers/time.h"

/** @addtogroup FT32F4_middlewares_i2c_application_library
  * @{
  */

/* Forward declarations for internal functions */
static void i2c_refresh_txdt_register(i2c_handle_type* hi2c);
static void i2c_set_buffer(i2c_handle_type* hi2c, i2cStep_t step, uint8_t *buf, uint16_t len);
static void i2c_dma_config(i2c_handle_type* hi2c, DMA_Channel_TypeDef* dma_channel, uint8_t* pdata, uint16_t size);
static void i2c_start_transfer(i2c_handle_type* hi2c, uint16_t address, uint32_t start);
static void i2c_start_transfer_dma(i2c_handle_type* hi2c, DMA_Channel_TypeDef* dma_channelx, uint16_t address, uint32_t start);
static i2c_status_type i2c_memory_address_send(i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t mem_address, int32_t timeout);
static i2c_status_type i2c_master_irq_handler_int(i2c_handle_type* hi2c);
static i2c_status_type i2c_slave_irq_handler_int(i2c_handle_type* hi2c);
static i2c_status_type i2c_master_irq_handler_dma(i2c_handle_type* hi2c);
static i2c_status_type i2c_slave_irq_handler_dma(i2c_handle_type* hi2c);
static void i2c_dma_tx_rx_irq_handler(i2c_handle_type* hi2c, DMA_Channel_TypeDef* dma_channel);

/* FT32F4 I2C standard library API mappings */
/* Flags - use FT32F4 native definitions directly */
#define I2C_TDBE_FLAG                    I2C_FLAG_TXE
#define I2C_TDIS_FLAG                    I2C_FLAG_TXIS
#define I2C_RDBF_FLAG                    I2C_FLAG_RXNE
#define I2C_ADDRF_FLAG                   I2C_FLAG_ADDR
#define I2C_ACKFAIL_FLAG                 I2C_FLAG_NACKF
#define I2C_STOPF_FLAG                   I2C_FLAG_STOPF
#define I2C_TDC_FLAG                     I2C_FLAG_TC
#define I2C_TCRLD_FLAG                   I2C_FLAG_TCR
#define I2C_BUSERR_FLAG                  I2C_FLAG_BERR
#define I2C_ARLOST_FLAG                  I2C_FLAG_ARLO
#define I2C_OUF_FLAG                     I2C_FLAG_OVR
#define I2C_PECERR_FLAG                  I2C_FLAG_PECERR
#define I2C_TMOUT_FLAG                   I2C_FLAG_TIMEOUT
#define I2C_ALERTF_FLAG                  I2C_FLAG_ALERT
#define I2C_BUSYF_FLAG                   I2C_FLAG_BUSY

/* Interrupts - use FT32F4 native definitions */
#define I2C_ERR_INT                      I2C_IT_ERRI
#define I2C_TDC_INT                      I2C_IT_TCI
#define I2C_STOP_INT                     I2C_IT_STOPI
#define I2C_ACKFIAL_INT                  I2C_IT_NACKI
#define I2C_TD_INT                       I2C_IT_TXI
#define I2C_ADDR_INT                     I2C_IT_ADDRI
#define I2C_RD_INT                       I2C_IT_RXI

/* Start modes - use FT32F4 native definitions */
#define I2C_WITHOUT_START                I2C_No_StartStop
#define I2C_GEN_START_READ               I2C_Generate_Start_Read
#define I2C_GEN_START_WRITE              I2C_Generate_Start_Write

/* Reload/End modes - use FT32F4 native definitions */
#define I2C_AUTO_STOP_MODE               I2C_AutoEnd_Mode
#define I2C_SOFT_STOP_MODE               I2C_SoftEnd_Mode
#define I2C_RELOAD_MODE                  I2C_Reload_Mode

/* DMA requests - use FT32F4 native definitions */
#define I2C_DMA_REQUEST_TX               I2C_DMAReq_Tx
#define I2C_DMA_REQUEST_RX               I2C_DMAReq_Rx

/**
  * @brief  Get I2C flag status.
  * @param  i2c_x: I2C peripheral
  * @param  flag: flag to check
  * @retval Flag status (SET or RESET)
  */
uint32_t i2c_flag_get(I2C_TypeDef* i2c_x, uint32_t flag)
{
  return (I2C_GetFlagStatus(i2c_x, flag) == SET) ? 1 : 0;
}

/**
  * @brief  Clear I2C flag.
  * @param  i2c_x: I2C peripheral
  * @param  flag: flag to clear
  * @retval None
  */
void i2c_flag_clear(I2C_TypeDef* i2c_x, uint32_t flag)
{
  I2C_ClearFlag(i2c_x, flag);
}

/**
  * @brief  initializes peripherals used by the i2c.
  * @param  none
  * @retval none
  */
void i2c_lowlevel_init(i2c_handle_type* hi2c) __attribute__((weak));
void i2c_lowlevel_init(i2c_handle_type* hi2c)
{
  UNUSED(hi2c);
}

/**
  * @brief  i2c peripheral initialization.
  * @param  hi2c: the handle points to the operation information.
  * @retval none.
  */
void i2c_config(i2c_handle_type* hi2c)
{
  /* reset i2c peripheral */
  I2C_SoftwareResetCmd(hi2c->i2cx);

  /* i2c peripheral initialization */
  i2c_lowlevel_init(hi2c);

  /* i2c peripheral enable */
  I2C_Cmd(hi2c->i2cx, ENABLE);
}

/**
  * @brief  refresh i2c register.
  */
static void i2c_refresh_txdt_register(i2c_handle_type* hi2c)
{
  UNUSED(hi2c);
}

/**
  * @brief  reset ctrl2 register.
  */
void i2c_reset_ctrl2_register(i2c_handle_type* hi2c)
{
  hi2c->i2cx->CR2 &= ~I2C_CR2_SADD;
  hi2c->i2cx->CR2 &= ~I2C_CR2_HEAD10R;
  hi2c->i2cx->CR2 &= ~I2C_CR2_NBYTES;
  hi2c->i2cx->CR2 &= ~I2C_CR2_RELOAD;
  hi2c->i2cx->CR2 &= ~I2C_CR2_RD_WRN;
}

/**
  * @brief  wait for the flag to be set or reset.
  */
i2c_status_type i2c_wait_flag(i2c_handle_type* hi2c, uint32_t flag, uint32_t event_check, uint32_t timeout)
{
  hi2c->error_code = I2C_OK;
  uint32_t startTick = microsISR();
  
  if(flag == I2C_FLAG_BUSY)
  {
    while(I2C_GetFlagStatus(hi2c->i2cx, flag) != RESET)
    {
      if ((int32_t)cmpTimeUs(microsISR(), startTick) >= (int32_t)timeout)
      {
        hi2c->error_code = I2C_ERR_TIMEOUT;
        return hi2c->error_code;
      }
    }
  }
  else
  {
    while(I2C_GetFlagStatus(hi2c->i2cx, flag) == RESET)
    {
      UNUSED(event_check);
      
      if ((int32_t)cmpTimeUs(microsISR(), startTick) >= (int32_t)timeout)
      {
        hi2c->error_code = I2C_ERR_TIMEOUT;
        return hi2c->error_code;
      }
    }
  }

  return hi2c->error_code;
}

static void i2c_set_buffer(i2c_handle_type* hi2c, i2cStep_t step, uint8_t *buf, uint16_t len)
{
  hi2c->step = step;
  hi2c->pbuff[step] = buf;
  hi2c->pcount[step] = len;
}

/**
  * @brief  dma transfer configuration.
  */
static void i2c_dma_config(i2c_handle_type* hi2c, DMA_Channel_TypeDef* dma_channel, uint8_t* pdata, uint16_t size)
{
  DMA_InitTypeDef ft32_dma_init;
  uint32_t direction;
  
  DMA_Cmd(dma_channel, DISABLE);
  DMA_ITConfig(dma_channel, DMA_IT_TFR, DISABLE);
  DMA_StructInit(&ft32_dma_init);
  
  direction = (dma_channel == hi2c->dma_tx_channel) ? 1 : 0;
  
  if (direction == 1) {
    ft32_dma_init.SrcAddress = (uint32_t)pdata;
    ft32_dma_init.DstAddress = (uint32_t)&hi2c->i2cx->TXDR;
    ft32_dma_init.SrcAddrMode = DMA_SRC_ADDRMODE_INC;
    ft32_dma_init.DstAddrMode = DMA_DST_ADDRMODE_HOLD;
  } else {
    ft32_dma_init.SrcAddress = (uint32_t)&hi2c->i2cx->RXDR;
    ft32_dma_init.DstAddress = (uint32_t)pdata;
    ft32_dma_init.SrcAddrMode = DMA_SRC_ADDRMODE_HOLD;
    ft32_dma_init.DstAddrMode = DMA_DST_ADDRMODE_INC;
  }
  
  ft32_dma_init.BlockTransSize = size;
  ft32_dma_init.SrcDstMasterSel = 0;
  ft32_dma_init.TransferTypeFlowCtl = 0;
  ft32_dma_init.SrcBurstTransferLength = 0;
  ft32_dma_init.DstBurstTransferLength = 0;
  ft32_dma_init.SrcTransferWidth = 0;
  ft32_dma_init.DstTransferWidth = 0;
  ft32_dma_init.SrcHardwareInterface = 0;
  ft32_dma_init.DstHardwareInterface = 0;
  ft32_dma_init.FIFOMode = DISABLE;
  ft32_dma_init.FlowCtlMode = DISABLE;
  ft32_dma_init.ReloadDst = DISABLE;
  ft32_dma_init.ReloadSrc = DISABLE;
  ft32_dma_init.MaxBurstLength = 0;
  ft32_dma_init.SrcHsIfPol = 0;
  ft32_dma_init.DstHsIfPol = 0;
  ft32_dma_init.SrcHsSel = 0;
  ft32_dma_init.DstHsSel = 0;
  ft32_dma_init.Priority = 0;
  ft32_dma_init.SrcHsIfPeriphSel = 0;
  ft32_dma_init.DstHsIfPeriphSel = 0;
  
  DMA_Init(dma_channel, &ft32_dma_init);
  DMA_ITConfig(dma_channel, DMA_IT_TFR, ENABLE);
  DMA_Cmd(dma_channel, ENABLE);
}

static void i2c_start_transfer(i2c_handle_type* hi2c, uint16_t address, uint32_t start)
{
  uint16_t totalLen = hi2c->pcount[hi2c->step];

  /* Cache the address for restart operations */
  hi2c->addr = address;

  if ((hi2c->step == I2C_STEP_REG) && (hi2c->mode == I2C_INT_MA_TX)) {
    totalLen = hi2c->pcount[I2C_STEP_REG] + hi2c->pcount[I2C_STEP_DATA];
  }

  if (totalLen > MAX_TRANSFER_CNT)
  {
    hi2c->psize = MAX_TRANSFER_CNT;
    I2C_TransferHandling(hi2c->i2cx, address, hi2c->psize, I2C_RELOAD_MODE, start);
  }
  else
  {
    hi2c->psize = totalLen;

    if ((hi2c->step == I2C_STEP_DATA) && (hi2c->mode == I2C_INT_MA_TX)) {
      I2C_TransferHandling(hi2c->i2cx, address, hi2c->psize, I2C_RELOAD_MODE, start);
    } else {
      if ((hi2c->mode == I2C_INT_MA_RX) || (hi2c->mode == I2C_INT_MA_TX)) {
        I2C_TransferHandling(hi2c->i2cx, address, hi2c->psize, I2C_SOFT_STOP_MODE, start);
      } else {
        I2C_TransferHandling(hi2c->i2cx, address, hi2c->psize, I2C_AUTO_STOP_MODE, start);
      }
    }
  }
}

static void i2c_start_transfer_dma(i2c_handle_type* hi2c, DMA_Channel_TypeDef* dma_channelx, uint16_t address, uint32_t start)
{
  if (hi2c->pcount[hi2c->step] > MAX_TRANSFER_CNT)
  {
    hi2c->psize = MAX_TRANSFER_CNT;
    i2c_dma_config(hi2c, dma_channelx, hi2c->pbuff[hi2c->step], hi2c->psize);
    I2C_TransferHandling(hi2c->i2cx, address, hi2c->psize, I2C_RELOAD_MODE, start);
  }
  else
  {
    hi2c->psize = hi2c->pcount[hi2c->step];
    i2c_dma_config(hi2c, dma_channelx, hi2c->pbuff[hi2c->step], hi2c->psize);
    I2C_TransferHandling(hi2c->i2cx, address, hi2c->psize, I2C_AUTO_STOP_MODE, start);
  }
}

/**
  * @brief  the master transmits data through polling mode.
  */
i2c_status_type i2c_master_transmit(i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  i2c_start_transfer(hi2c, address, I2C_GEN_START_WRITE);

  while (hi2c->pcount[hi2c->step] > 0)
  {
    if(i2c_wait_flag(hi2c, I2C_FLAG_TXIS, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
    {
      hi2c->error_code = I2C_ERR_STEP_2;
      return hi2c->error_code;
    }

    I2C_SendData(hi2c->i2cx, *hi2c->pbuff[hi2c->step]++);
    hi2c->psize--;
    hi2c->pcount[hi2c->step]--;

    if ((hi2c->psize == 0) && (hi2c->pcount[hi2c->step] != 0))
    {
      if (i2c_wait_flag(hi2c, I2C_FLAG_TCR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
      {
        hi2c->error_code = I2C_ERR_STEP_3;
        return hi2c->error_code;
      }
      i2c_start_transfer(hi2c, address, I2C_WITHOUT_START);
    }
  }

  if(i2c_wait_flag(hi2c, I2C_FLAG_STOPF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_4;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);
  i2c_reset_ctrl2_register(hi2c);

  return hi2c->error_code;
}

/**
  * @brief  the slave receive data through polling mode.
  */
i2c_status_type i2c_slave_receive(i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  I2C_AcknowledgeConfig(hi2c->i2cx, ENABLE);

  if (i2c_wait_flag(hi2c, I2C_FLAG_ADDR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_2;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_ADDR);

  while (hi2c->pcount[hi2c->step] > 0)
  {
    if(i2c_wait_flag(hi2c, I2C_FLAG_RXNE, I2C_EVENT_CHECK_STOP, timeout) != I2C_OK)
    {
      I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
      hi2c->error_code = I2C_ERR_STEP_4;
      return hi2c->error_code;
    }

    (*hi2c->pbuff[hi2c->step]++) = I2C_ReceiveData(hi2c->i2cx);
    hi2c->pcount[hi2c->step]--;
  }

  if(i2c_wait_flag(hi2c, I2C_FLAG_STOPF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
    hi2c->error_code = I2C_ERR_STEP_5;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
    hi2c->error_code = I2C_ERR_STEP_6;
    return hi2c->error_code;
  }

  return hi2c->error_code;
}

/**
  * @brief  the master receive data through polling mode.
  */
i2c_status_type i2c_master_receive(i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  i2c_start_transfer(hi2c, address, I2C_GEN_START_READ);

  while (hi2c->pcount[hi2c->step] > 0)
  {
    if(i2c_wait_flag(hi2c, I2C_FLAG_RXNE, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
    {
      hi2c->error_code = I2C_ERR_STEP_2;
      return hi2c->error_code;
    }

    (*hi2c->pbuff[hi2c->step]++) = I2C_ReceiveData(hi2c->i2cx);
    hi2c->pcount[hi2c->step]--;
    hi2c->psize--;

    if ((hi2c->psize == 0) && (hi2c->pcount[hi2c->step] != 0))
    {
      if (i2c_wait_flag(hi2c, I2C_FLAG_TCR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
      {
        hi2c->error_code = I2C_ERR_STEP_3;
        return hi2c->error_code;
      }
      i2c_start_transfer(hi2c, address, I2C_WITHOUT_START);
    }
  }

  if(i2c_wait_flag(hi2c, I2C_FLAG_STOPF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_4;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);
  i2c_reset_ctrl2_register(hi2c);

  return hi2c->error_code;
}

/**
  * @brief  the slave transmits data through polling mode.
  */
i2c_status_type i2c_slave_transmit(i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  I2C_AcknowledgeConfig(hi2c->i2cx, ENABLE);

  if (i2c_wait_flag(hi2c, I2C_FLAG_ADDR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
    hi2c->error_code = I2C_ERR_STEP_2;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_ADDR);

  if ((hi2c->i2cx->CR2 & I2C_CR2_ADD10) != RESET)
  {
    if (i2c_wait_flag(hi2c, I2C_FLAG_ADDR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
    {
      I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
      hi2c->error_code = I2C_ERR_STEP_3;
      return hi2c->error_code;
    }
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_ADDR);
  }

  while (hi2c->pcount[hi2c->step] > 0)
  {
    if(i2c_wait_flag(hi2c, I2C_FLAG_TXIS, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
    {
      I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
      hi2c->error_code = I2C_ERR_STEP_5;
      return hi2c->error_code;
    }

    I2C_SendData(hi2c->i2cx, *hi2c->pbuff[hi2c->step]++);
    hi2c->pcount[hi2c->step]--;
  }

  if(i2c_wait_flag(hi2c, I2C_FLAG_NACKF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_6;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_NACKF);

  if(i2c_wait_flag(hi2c, I2C_FLAG_STOPF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
    hi2c->error_code = I2C_ERR_STEP_7;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
    hi2c->error_code = I2C_ERR_STEP_8;
    return hi2c->error_code;
  }

  i2c_refresh_txdt_register(hi2c);

  return hi2c->error_code;
}

/**
  * @brief  the master transmits data through interrupt mode.
  */
i2c_status_type i2c_master_transmit_int(i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_INT_MA_TX;
  hi2c->state = I2C_START;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  i2c_start_transfer(hi2c, address, I2C_GEN_START_WRITE);
  I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_TXI, ENABLE);

  return hi2c->error_code;
}

/**
  * @brief  the slave receive data through interrupt mode.
  */
i2c_status_type i2c_slave_receive_int(i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_INT_SLA_RX;
  hi2c->state = I2C_START;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
  } else {
    I2C_AcknowledgeConfig(hi2c->i2cx, ENABLE);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ADDRI | I2C_IT_RXI, ENABLE);
  }

  return hi2c->error_code;
}

/**
  * @brief  the master receive data through interrupt mode.
  */
i2c_status_type i2c_master_receive_int(i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_INT_MA_RX;
  hi2c->state = I2C_START;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
  } else {
    i2c_start_transfer(hi2c, address, I2C_GEN_START_READ);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_RXI, ENABLE);
  }

  return hi2c->error_code;
}

/**
  * @brief  the slave transmits data through interrupt mode.
  */
i2c_status_type i2c_slave_transmit_int(i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_INT_SLA_TX;
  hi2c->state = I2C_START;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
  } else {
    I2C_AcknowledgeConfig(hi2c->i2cx, ENABLE);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ADDRI | I2C_IT_TXI, ENABLE);
    i2c_refresh_txdt_register(hi2c);
  }

  return hi2c->error_code;
}

/**
  * @brief  the master transmits data through dma mode.
  */
i2c_status_type i2c_master_transmit_dma(i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_DMA_MA_TX;
  hi2c->state = I2C_START;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
  } else {
    I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_TX, DISABLE);
    i2c_start_transfer_dma(hi2c, hi2c->dma_tx_channel, address, I2C_GEN_START_WRITE);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_NACKI, ENABLE);
    I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_TX, ENABLE);
  }

  return hi2c->error_code;
}

/**
  * @brief  the slave receive data through dma mode.
  */
i2c_status_type i2c_slave_receive_dma(i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_DMA_SLA_RX;
  hi2c->state = I2C_START;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
  } else {
    I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_RX, DISABLE);
    i2c_dma_config(hi2c, hi2c->dma_rx_channel, hi2c->pbuff[hi2c->step], size);
    I2C_AcknowledgeConfig(hi2c->i2cx, ENABLE);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ADDRI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ERRI, ENABLE);
    I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_RX, ENABLE);
  }

  return hi2c->error_code;
}

/**
  * @brief  the master receive data through dma mode.
  */
i2c_status_type i2c_master_receive_dma(i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_DMA_MA_RX;
  hi2c->state = I2C_START;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
  } else {
    I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_RX, DISABLE);
    i2c_start_transfer_dma(hi2c, hi2c->dma_rx_channel, address, I2C_GEN_START_READ);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_NACKI, ENABLE);
    I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_RX, ENABLE);
  }

  return hi2c->error_code;
}

/**
  * @brief  the slave transmits data through dma mode.
  */
i2c_status_type i2c_slave_transmit_dma(i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_DMA_SLA_TX;
  hi2c->state = I2C_START;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
  } else {
    I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_TX, DISABLE);
    i2c_dma_config(hi2c, hi2c->dma_tx_channel, hi2c->pbuff[hi2c->step], size);
    I2C_AcknowledgeConfig(hi2c->i2cx, ENABLE);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ADDRI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ERRI, ENABLE);
    I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_TX, ENABLE);
  }

  return hi2c->error_code;
}

/**
  * @brief  send memory address.
  */
static i2c_status_type i2c_memory_address_send(i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t mem_address, int32_t timeout)
{
  i2c_status_type err_code;
  
  if(mem_address_width == I2C_MEM_ADDR_WIDIH_8)
  {
    I2C_SendData(hi2c->i2cx, mem_address & 0xFF);
  }
  else
  {
    I2C_SendData(hi2c->i2cx, (mem_address >> 8) & 0xFF);
    
    err_code = i2c_wait_flag(hi2c, I2C_FLAG_TXIS, I2C_EVENT_CHECK_ACKFAIL, timeout);
    
    if(err_code != I2C_OK)
    {
      return err_code;
    }
    
    I2C_SendData(hi2c->i2cx, mem_address & 0xFF);
  }
  
  return hi2c->error_code;
}

/**
  * @brief  write data to the memory device through polling mode.
  */
i2c_status_type i2c_memory_write(i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_MA_TX;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size + mem_address_width);
  hi2c->error_code = I2C_OK;

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  i2c_start_transfer(hi2c, address, I2C_GEN_START_WRITE);

  if(i2c_wait_flag(hi2c, I2C_FLAG_TXIS, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_2;
    return hi2c->error_code;
  }

  if(i2c_memory_address_send(hi2c, mem_address_width, mem_address, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_3;
    return hi2c->error_code;
  }

  hi2c->psize -= mem_address_width;
  hi2c->pcount[hi2c->step] -= mem_address_width;

  while (hi2c->pcount[hi2c->step] > 0)
  {
    if(i2c_wait_flag(hi2c, I2C_FLAG_TXIS, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
    {
      hi2c->error_code = I2C_ERR_STEP_4;
      return hi2c->error_code;
    }

    I2C_SendData(hi2c->i2cx, *hi2c->pbuff[hi2c->step]++);
    hi2c->psize--;
    hi2c->pcount[hi2c->step]--;

    if ((hi2c->psize == 0) && (hi2c->pcount[hi2c->step] != 0))
    {
      if (i2c_wait_flag(hi2c, I2C_FLAG_TCR, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
      {
        hi2c->error_code = I2C_ERR_STEP_5;
        return hi2c->error_code;
      }
      i2c_start_transfer(hi2c, address, I2C_WITHOUT_START);
    }
  }

  if(i2c_wait_flag(hi2c, I2C_FLAG_STOPF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_6;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);
  i2c_reset_ctrl2_register(hi2c);

  return hi2c->error_code;
}

/**
  * @brief  read data from memory device through polling mode.
  */
i2c_status_type i2c_memory_read(i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_MA_RX;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  I2C_TransferHandling(hi2c->i2cx, address, mem_address_width, I2C_SOFT_STOP_MODE, I2C_GEN_START_WRITE);

  if(i2c_wait_flag(hi2c, I2C_FLAG_TXIS, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_2;
    return hi2c->error_code;
  }

  if(i2c_memory_address_send(hi2c, mem_address_width, mem_address, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_3;
    return hi2c->error_code;
  }

  if (i2c_wait_flag(hi2c, I2C_FLAG_TC, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_4;
    return hi2c->error_code;
  }

  i2c_start_transfer(hi2c, address, I2C_GEN_START_READ);

  while (hi2c->pcount[hi2c->step] > 0)
  {
    if (i2c_wait_flag(hi2c, I2C_FLAG_RXNE, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
    {
      hi2c->error_code = I2C_ERR_STEP_5;
      return hi2c->error_code;
    }

    (*hi2c->pbuff[hi2c->step]++) = I2C_ReceiveData(hi2c->i2cx);
    hi2c->pcount[hi2c->step]--;
    hi2c->psize--;

    if ((hi2c->psize == 0) && (hi2c->pcount[hi2c->step] != 0))
    {
      if (i2c_wait_flag(hi2c, I2C_FLAG_TCR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
      {
        hi2c->error_code = I2C_ERR_STEP_6;
        return hi2c->error_code;
      }
      i2c_start_transfer(hi2c, address, I2C_WITHOUT_START);
    }
  }

  if (i2c_wait_flag(hi2c, I2C_FLAG_STOPF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_7;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);
  i2c_reset_ctrl2_register(hi2c);

  return hi2c->error_code;
}

/**
  * @brief  write data to the memory device through interrupt mode.
  */
i2c_status_type i2c_memory_write_int(i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  if (mem_address_width == I2C_MEM_ADDR_WIDIH_8) {
    hi2c->pcount[I2C_STEP_REG] = 1;
    hi2c->reg = mem_address;
  } else {
    hi2c->pcount[I2C_STEP_REG] = 2;
    hi2c->reg = ((mem_address >> 8) & 0xff) | ((mem_address & 0xff) << 8);
  }

  hi2c->step = I2C_STEP_REG;
  hi2c->pbuff[I2C_STEP_REG] = (uint8_t *)&hi2c->reg;

  hi2c->pbuff[I2C_STEP_DATA] = pdata;
  hi2c->pcount[I2C_STEP_DATA] = size;

  hi2c->error_code = I2C_OK;

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  hi2c->state = I2C_START;
  hi2c->mode = I2C_INT_MA_TX;

  i2c_start_transfer(hi2c, address, I2C_GEN_START_WRITE);
  I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_TXI, ENABLE);

  return hi2c->error_code;
}

/**
  * @brief  read data from memory device through interrupt mode.
  */
i2c_status_type i2c_memory_read_int(i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  if (mem_address_width == I2C_MEM_ADDR_WIDIH_8) {
    hi2c->pcount[I2C_STEP_REG] = 1;
    hi2c->reg = mem_address;
  } else {
    hi2c->pcount[I2C_STEP_REG] = 2;
    hi2c->reg = ((mem_address >> 8) & 0xff) | ((mem_address & 0xff) << 8);
  }

  hi2c->step = I2C_STEP_REG;
  hi2c->pbuff[I2C_STEP_REG] = (uint8_t *)&hi2c->reg;

  hi2c->pbuff[I2C_STEP_DATA] = pdata;
  hi2c->pcount[I2C_STEP_DATA] = size;

  hi2c->error_code = I2C_OK;

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  hi2c->state = I2C_START;
  hi2c->mode = I2C_INT_MA_RX;

  i2c_start_transfer(hi2c, address, I2C_GEN_START_WRITE);
  I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_TXI | I2C_IT_RXI, ENABLE);

  return hi2c->error_code;
}

/**
  * @brief  write data to the memory device through dma mode.
  */
i2c_status_type i2c_memory_write_dma(i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_DMA_MA_TX;
  hi2c->state = I2C_START;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_TX, DISABLE);

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    return hi2c->error_code;
    hi2c->error_code = I2C_ERR_STEP_1;
  }

  I2C_TransferHandling(hi2c->i2cx, address, mem_address_width, I2C_RELOAD_MODE, I2C_GEN_START_WRITE);

  if(i2c_wait_flag(hi2c, I2C_FLAG_TXIS, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
  {
    return hi2c->error_code;
    hi2c->error_code = I2C_ERR_STEP_2;
  }

  if(i2c_memory_address_send(hi2c, mem_address_width, mem_address, timeout) != I2C_OK)
  {
    return hi2c->error_code;
    hi2c->error_code = I2C_ERR_STEP_3;
  }

  if (i2c_wait_flag(hi2c, I2C_FLAG_TCR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    return hi2c->error_code;
    hi2c->error_code = I2C_ERR_STEP_4;
  }

  i2c_start_transfer_dma(hi2c, hi2c->dma_tx_channel, address, I2C_WITHOUT_START);
  I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_NACKI, ENABLE);
  I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_TX, ENABLE);

  return hi2c->error_code;
}

/**
  * @brief  read data from memory device through dma mode.
  */
i2c_status_type i2c_memory_read_dma(i2c_handle_type* hi2c, i2c_mem_address_width_type mem_address_width, uint16_t address, uint16_t mem_address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  hi2c->mode = I2C_DMA_MA_RX;
  hi2c->state = I2C_START;

  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if(i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  I2C_TransferHandling(hi2c->i2cx, address, mem_address_width, I2C_SOFT_STOP_MODE, I2C_GEN_START_WRITE);

  if(i2c_wait_flag(hi2c, I2C_FLAG_TXIS, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_2;
    return hi2c->error_code;
  }

  if(i2c_memory_address_send(hi2c, mem_address_width, mem_address, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_3;
    return hi2c->error_code;
  }

  if (i2c_wait_flag(hi2c, I2C_FLAG_TC, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_4;
    return hi2c->error_code;
  }

  I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_RX, DISABLE);
  i2c_start_transfer_dma(hi2c, hi2c->dma_rx_channel, address, I2C_GEN_START_READ);
  I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_NACKI, ENABLE);
  I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_RX, ENABLE);

  return hi2c->error_code;
}

/**
  * @brief  the master transmits data through SMBus mode.
  */
i2c_status_type i2c_smbus_master_transmit(i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  I2C_CalculatePEC(hi2c->i2cx, ENABLE);
  I2C_PECRequestCmd(hi2c->i2cx, ENABLE);

  i2c_start_transfer(hi2c, address, I2C_GEN_START_WRITE);

  hi2c->pcount[hi2c->step]--;

  while (hi2c->pcount[hi2c->step] > 0)
  {
    if(i2c_wait_flag(hi2c, I2C_FLAG_TXIS, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
    {
      hi2c->error_code = I2C_ERR_STEP_2;
      return hi2c->error_code;
    }

    I2C_SendData(hi2c->i2cx, *hi2c->pbuff[hi2c->step]++);
    hi2c->psize--;
    hi2c->pcount[hi2c->step]--;

    if ((hi2c->psize == 0) && (hi2c->pcount[hi2c->step] != 0))
    {
      if (i2c_wait_flag(hi2c, I2C_FLAG_TCR, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
      {
        hi2c->error_code = I2C_ERR_STEP_3;
        return hi2c->error_code;
      }
      i2c_start_transfer(hi2c, address, I2C_WITHOUT_START);
    }
  }

  if(i2c_wait_flag(hi2c, I2C_FLAG_STOPF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_4;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);
  i2c_reset_ctrl2_register(hi2c);

  return hi2c->error_code;
}

/**
  * @brief  the slave receive data through SMBus mode.
  */
i2c_status_type i2c_smbus_slave_receive(i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  I2C_CalculatePEC(hi2c->i2cx, ENABLE);
  I2C_SlaveByteControlCmd(hi2c->i2cx, ENABLE);
  I2C_AcknowledgeConfig(hi2c->i2cx, ENABLE);

  if (i2c_wait_flag(hi2c, I2C_FLAG_ADDR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  I2C_PECRequestCmd(hi2c->i2cx, ENABLE);
  I2C_NumberOfBytesConfig(hi2c->i2cx, hi2c->pcount[hi2c->step]);

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_ADDR);

  while (hi2c->pcount[hi2c->step] > 0)
  {
    if(i2c_wait_flag(hi2c, I2C_FLAG_RXNE, I2C_EVENT_CHECK_STOP, timeout) != I2C_OK)
    {
      I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
      hi2c->error_code = I2C_ERR_STEP_3;
      return hi2c->error_code;
    }

    (*hi2c->pbuff[hi2c->step]++) = I2C_ReceiveData(hi2c->i2cx);
    hi2c->pcount[hi2c->step]--;
  }

  if(i2c_wait_flag(hi2c, I2C_FLAG_STOPF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
    hi2c->error_code = I2C_ERR_STEP_4;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
    hi2c->error_code = I2C_ERR_STEP_5;
    return hi2c->error_code;
  }

  I2C_SlaveByteControlCmd(hi2c->i2cx, DISABLE);

  return hi2c->error_code;
}

/**
  * @brief  the master receive data through SMBus mode.
  */
i2c_status_type i2c_smbus_master_receive(i2c_handle_type* hi2c, uint16_t address, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  I2C_CalculatePEC(hi2c->i2cx, ENABLE);
  I2C_PECRequestCmd(hi2c->i2cx, ENABLE);

  i2c_start_transfer(hi2c, address, I2C_GEN_START_READ);

  while (hi2c->pcount[hi2c->step] > 0)
  {
    if(i2c_wait_flag(hi2c, I2C_FLAG_RXNE, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
    {
      hi2c->error_code = I2C_ERR_STEP_2;
      return hi2c->error_code;
    }

    (*hi2c->pbuff[hi2c->step]++) = I2C_ReceiveData(hi2c->i2cx);
    hi2c->pcount[hi2c->step]--;
    hi2c->psize--;

    if ((hi2c->psize == 0) && (hi2c->pcount[hi2c->step] != 0))
    {
      if (i2c_wait_flag(hi2c, I2C_FLAG_TCR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
      {
        hi2c->error_code = I2C_ERR_STEP_3;
        return hi2c->error_code;
      }
      i2c_start_transfer(hi2c, address, I2C_WITHOUT_START);
    }
  }

  if(i2c_wait_flag(hi2c, I2C_FLAG_STOPF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_4;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);
  i2c_reset_ctrl2_register(hi2c);

  return hi2c->error_code;
}

/**
  * @brief  the slave transmits data through SMBus mode.
  */
i2c_status_type i2c_smbus_slave_transmit(i2c_handle_type* hi2c, uint8_t* pdata, uint16_t size, uint32_t timeout)
{
  i2c_set_buffer(hi2c, I2C_STEP_DATA, pdata, size);
  hi2c->error_code = I2C_OK;

  I2C_CalculatePEC(hi2c->i2cx, ENABLE);
  I2C_SlaveByteControlCmd(hi2c->i2cx, ENABLE);
  I2C_AcknowledgeConfig(hi2c->i2cx, ENABLE);

  if (i2c_wait_flag(hi2c, I2C_FLAG_ADDR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
    hi2c->error_code = I2C_ERR_STEP_1;
    return hi2c->error_code;
  }

  if ((hi2c->i2cx->CR2 & I2C_CR2_ADD10) == 0)
  {
    I2C_PECRequestCmd(hi2c->i2cx, ENABLE);
    I2C_NumberOfBytesConfig(hi2c->i2cx, hi2c->pcount[hi2c->step]);
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_ADDR);

  if ((hi2c->i2cx->CR2 & I2C_CR2_ADD10) != RESET)
  {
    if (i2c_wait_flag(hi2c, I2C_FLAG_ADDR, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
    {
      I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
      hi2c->error_code = I2C_ERR_STEP_2;
      return hi2c->error_code;
    }

    I2C_PECRequestCmd(hi2c->i2cx, ENABLE);
    I2C_NumberOfBytesConfig(hi2c->i2cx, hi2c->pcount[hi2c->step]);
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_ADDR);
  }

  hi2c->pcount[hi2c->step]--;

  while (hi2c->pcount[hi2c->step] > 0)
  {
    if(i2c_wait_flag(hi2c, I2C_FLAG_TXIS, I2C_EVENT_CHECK_ACKFAIL, timeout) != I2C_OK)
    {
      I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
      hi2c->error_code = I2C_ERR_STEP_4;
      return hi2c->error_code;
    }

    I2C_SendData(hi2c->i2cx, *hi2c->pbuff[hi2c->step]++);
    hi2c->pcount[hi2c->step]--;
  }

  if(i2c_wait_flag(hi2c, I2C_FLAG_NACKF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    hi2c->error_code = I2C_ERR_STEP_5;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_NACKF);

  if(i2c_wait_flag(hi2c, I2C_FLAG_STOPF, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
    hi2c->error_code = I2C_ERR_STEP_6;
    return hi2c->error_code;
  }

  I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);

  if (i2c_wait_flag(hi2c, I2C_FLAG_BUSY, I2C_EVENT_CHECK_NONE, timeout) != I2C_OK)
  {
    I2C_AcknowledgeConfig(hi2c->i2cx, DISABLE);
    hi2c->error_code = I2C_ERR_STEP_7;
    return hi2c->error_code;
  }

  i2c_reset_ctrl2_register(hi2c);
  i2c_refresh_txdt_register(hi2c);
  I2C_SlaveByteControlCmd(hi2c->i2cx, DISABLE);

  return hi2c->error_code;
}

/**
  * @brief  master interrupt processing function in interrupt mode.
  */
static i2c_status_type i2c_master_irq_handler_int(i2c_handle_type* hi2c)
{
  if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_NACKF) != RESET)
  {
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_NACKF);
    i2c_refresh_txdt_register(hi2c);

    if(hi2c->pcount[hi2c->step] != 0)
    {
      hi2c->error_code = I2C_ERR_ACKFAIL;
    }
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_TXIS) != RESET)
  {
    I2C_SendData(hi2c->i2cx, *hi2c->pbuff[hi2c->step]++);
    hi2c->pcount[hi2c->step]--;
    hi2c->psize--;
    if ((hi2c->pcount[hi2c->step] == 0) && (hi2c->step == I2C_STEP_REG) && (hi2c->mode == I2C_INT_MA_TX)) {
      hi2c->step = I2C_STEP_DATA;
    }
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_TCR) != RESET)
  {
    if ((hi2c->psize == 0) && (hi2c->pcount[hi2c->step] != 0))
    {
      i2c_start_transfer(hi2c, hi2c->addr, I2C_WITHOUT_START);
    } else {
      hi2c->error_code = I2C_ERR_TCRLD;
    }
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_RXNE) != RESET)
  {
    (*hi2c->pbuff[hi2c->step]++) = I2C_ReceiveData(hi2c->i2cx);
    hi2c->pcount[hi2c->step]--;
    hi2c->psize--;
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_TC) != RESET)
  {
    if (hi2c->pcount[hi2c->step] == 0)
    {
      if ((hi2c->step == I2C_STEP_REG) && (hi2c->mode == I2C_INT_MA_RX)) {
        hi2c->step = I2C_STEP_DATA;
        i2c_start_transfer(hi2c, hi2c->addr, I2C_GEN_START_READ);
      } else {
        if ((hi2c->i2cx->CR2 & I2C_CR2_AUTOEND) == 0)
        {
          I2C_GenerateSTOP(hi2c->i2cx, ENABLE);
        }
      }
    }
    else
    {
      hi2c->error_code = I2C_ERR_TDC;
    }
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_STOPF) != RESET)
  {
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);
    i2c_reset_ctrl2_register(hi2c);

    if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_NACKF) != RESET)
    {
      I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_NACKF);
    }

    i2c_refresh_txdt_register(hi2c);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_TXI | I2C_IT_RXI, DISABLE);
    hi2c->state = I2C_END;
  }

  return hi2c->error_code;
}

/**
  * @brief  slave interrupt processing function in interrupt mode.
  */
static i2c_status_type i2c_slave_irq_handler_int(i2c_handle_type* hi2c)
{
  if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_NACKF) != RESET)
  {
    if (hi2c->pcount[hi2c->step] == 0)
    {
      i2c_refresh_txdt_register(hi2c);
      I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_NACKF);
    }
    else
    {
      I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_NACKF);
    }
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_ADDR) != RESET)
  {
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_ADDR);
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_TXIS) != RESET)
  {
    if (hi2c->pcount[hi2c->step] > 0)
    {
      hi2c->i2cx->TXDR = (*(hi2c->pbuff[hi2c->step]++));
      hi2c->psize--;
      hi2c->pcount[hi2c->step]--;
    }
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_RXNE) != RESET)
  {
    if (hi2c->pcount[hi2c->step] > 0)
    {
      (*hi2c->pbuff[hi2c->step]++) = I2C_ReceiveData(hi2c->i2cx);
      hi2c->pcount[hi2c->step]--;
      hi2c->psize--;
    }
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_STOPF) != RESET)
  {
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ADDRI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_TXI | I2C_IT_RXI, DISABLE);
    i2c_reset_ctrl2_register(hi2c);
    i2c_refresh_txdt_register(hi2c);

    if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_RXNE) != RESET)
    {
      (*hi2c->pbuff[hi2c->step]++) = I2C_ReceiveData(hi2c->i2cx);
      if ((hi2c->psize > 0))
      {
        hi2c->pcount[hi2c->step]--;
        hi2c->psize--;
      }
    }

    hi2c->state = I2C_END;
  }

  return hi2c->error_code;
}

/**
  * @brief  master interrupt processing function in dma mode.
  */
static i2c_status_type i2c_master_irq_handler_dma(i2c_handle_type* hi2c)
{
  if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_NACKF) != RESET)
  {
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_NACKF);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_STOPI, ENABLE);
    i2c_refresh_txdt_register(hi2c);

    if(hi2c->pcount[hi2c->step] != 0)
    {
      hi2c->error_code = I2C_ERR_ACKFAIL;
      return hi2c->error_code;
    }
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_TCR) != RESET)
  {
    I2C_ITConfig(hi2c->i2cx, I2C_IT_TCI, DISABLE);

    if (hi2c->pcount[hi2c->step] != 0)
    {
      i2c_start_transfer(hi2c, hi2c->addr, I2C_WITHOUT_START);

      if (hi2c->mode == I2C_DMA_MA_TX)
      {
        I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_TX, ENABLE);
      }
      else
      {
        I2C_DMACmd(hi2c->i2cx, I2C_DMA_REQUEST_RX, ENABLE);
      }
    }
    else
    {
      hi2c->error_code = I2C_ERR_TCRLD;
      return hi2c->error_code;
    }
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_STOPF) != RESET)
  {
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);
    i2c_reset_ctrl2_register(hi2c);

    if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_NACKF) != RESET)
    {
      I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_NACKF);
    }

    i2c_refresh_txdt_register(hi2c);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_TXI | I2C_IT_RXI, DISABLE);
    hi2c->state = I2C_END;
  }

  return hi2c->error_code;
}

/**
  * @brief  slave interrupt processing function in dma mode.
  */
static i2c_status_type i2c_slave_irq_handler_dma(i2c_handle_type* hi2c)
{
  if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_NACKF) != RESET)
  {
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_NACKF);
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_ADDR) != RESET)
  {
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_ADDR);
  }
  else if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_STOPF) != RESET)
  {
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_STOPF);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ADDRI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_TXI | I2C_IT_RXI, DISABLE);
    i2c_reset_ctrl2_register(hi2c);
    i2c_refresh_txdt_register(hi2c);

    if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_RXNE) != RESET)
    {
      (*hi2c->pbuff[hi2c->step]++) = I2C_ReceiveData(hi2c->i2cx);
      if ((hi2c->psize > 0))
      {
        hi2c->pcount[hi2c->step]--;
        hi2c->psize--;
      }
    }

    hi2c->state = I2C_END;
  }

  return hi2c->error_code;
}

/**
  * @brief  dma processing function.
  */
static void i2c_dma_tx_rx_irq_handler(i2c_handle_type* hi2c, DMA_Channel_TypeDef* dma_channel)
{
  if (DMA_GetFlagStatus(dma_channel, DMA_FLAG_TFR) != RESET)
  {
    DMA_ITConfig(dma_channel, DMA_IT_TFR, DISABLE);
    DMA_ClearFlagStatus(dma_channel, DMA_IT_TFR);
    I2C_DMACmd(hi2c->i2cx, (dma_channel == hi2c->dma_tx_channel) ? I2C_DMA_REQUEST_TX : I2C_DMA_REQUEST_RX, DISABLE);
    DMA_Cmd(dma_channel, DISABLE);

    switch(hi2c->mode)
    {
      case I2C_DMA_MA_TX:
      case I2C_DMA_MA_RX:
      {
        hi2c->pcount[hi2c->step] -= hi2c->psize;

        if (hi2c->pcount[hi2c->step] == 0)
        {
          I2C_ITConfig(hi2c->i2cx, I2C_IT_STOPI, ENABLE);
        }
        else
        {
          hi2c->pbuff[hi2c->step] += hi2c->psize;

          if (hi2c->pcount[hi2c->step] > MAX_TRANSFER_CNT)
          {
            hi2c->psize = MAX_TRANSFER_CNT;
          }
          else
          {
            hi2c->psize = hi2c->pcount[hi2c->step];
          }

          i2c_dma_config(hi2c, dma_channel, hi2c->pbuff[hi2c->step], hi2c->psize);
          I2C_ITConfig(hi2c->i2cx, I2C_IT_TCI, ENABLE);
        }
      }break;
      case I2C_DMA_SLA_TX:
      case I2C_DMA_SLA_RX:
      {
      }break;
      default:break;
    }
  }
}

/**
  * @brief  dma transmission complete interrupt function.
  */
void i2c_dma_tx_irq_handler(i2c_handle_type* hi2c)
{
  i2c_dma_tx_rx_irq_handler(hi2c, hi2c->dma_tx_channel);
}

/**
  * @brief  dma receive complete interrupt function.
  */
void i2c_dma_rx_irq_handler(i2c_handle_type* hi2c)
{
  i2c_dma_tx_rx_irq_handler(hi2c, hi2c->dma_rx_channel);
}

/**
  * @brief  interrupt procession function.
  */
void i2c_evt_irq_handler(i2c_handle_type* hi2c)
{
  switch(hi2c->mode)
  {
    case I2C_INT_MA_TX:
    case I2C_INT_MA_RX:
    {
      i2c_master_irq_handler_int(hi2c);
    }break;
    case I2C_INT_SLA_TX:
    case I2C_INT_SLA_RX:
    {
      i2c_slave_irq_handler_int(hi2c);
    }break;
    case I2C_DMA_MA_TX:
    case I2C_DMA_MA_RX:
    {
      i2c_master_irq_handler_dma(hi2c);
    }break;
    case I2C_DMA_SLA_TX:
    case I2C_DMA_SLA_RX:
    {
      i2c_slave_irq_handler_dma(hi2c);
    }break;
    default:break;
  }
}

/**
  * @brief  error interrupt function.
  */
void i2c_err_irq_handler(i2c_handle_type* hi2c)
{
  if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_BERR) != RESET)
  {
    hi2c->error_code = I2C_ERR_INTERRUPT;
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_BERR);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI, DISABLE);
  }

  if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_ARLO) != RESET)
  {
    hi2c->error_code = I2C_ERR_INTERRUPT;
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_ARLO);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI, DISABLE);
  }

  if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_OVR) != RESET)
  {
    hi2c->error_code = I2C_ERR_INTERRUPT;
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_OVR);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI, DISABLE);
  }

  if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_PECERR) != RESET)
  {
    hi2c->error_code = I2C_ERR_INTERRUPT;
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_PECERR);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI, DISABLE);
  }

  if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_TIMEOUT) != RESET)
  {
    hi2c->error_code = I2C_ERR_INTERRUPT;
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_TIMEOUT);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI, DISABLE);
  }

  if (I2C_GetFlagStatus(hi2c->i2cx, I2C_FLAG_ALERT) != RESET)
  {
    hi2c->error_code = I2C_ERR_INTERRUPT;
    I2C_ClearFlag(hi2c->i2cx, I2C_FLAG_ALERT);
    I2C_ITConfig(hi2c->i2cx, I2C_IT_ERRI, DISABLE);
  }
}

/**
  * @}
  */
