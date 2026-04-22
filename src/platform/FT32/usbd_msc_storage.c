/**
  ******************************************************************************
  * @file    			usbd_msc_storage.c
  * @author  			FMD XA
  * @brief   			Memory mangement layer.
  * @version 			V1.0.0           
  * @date		 			2026-03-30
  ******************************************************************************
  *
  *
  */
#include "usb_conf.h"
#include "usbd_msc.h"
/* Includes ------------------------------------------------------------------*/
#define STORAGE_LUN_NBR         1U
#define STORAGE_BLK_NBR         0x10000U
#define STORAGE_BLK_SIZ         0x200U

uint8_t MSC_STORAGE_Init(uint8_t lun);
uint8_t MSC_STORAGE_GetCapacity(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
uint8_t MSC_STORAGE_IsReady(uint8_t lun);
uint8_t MSC_STORAGE_IsWriteProtected(uint8_t lun);
uint8_t MSC_STORAGE_Read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
uint8_t MSC_STORAGE_Write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
uint8_t MSC_STORAGE_GetMaxLun(void);
uint8_t *MSC_STORAGE_Inquiry(uint8_t lun);

/* USB mass storage standard inquiry data */
int8_t MSC_STORAGE_Inquirydata[] = /* 36 */
{
  /* LUN 0 */
  0x00,
  0x80,
  0x02,
  0x02,
  (STANDARD_INQUIRY_DATA_LEN - 5),
  0x00,
  0x00,
  0x00,
  'F', 'M', 'D', ' ', ' ', ' ', ' ', ' ', /* manufacture : 8bytes */
  'P', 'r', 'o', 'd', 'u', 'c', 't', ' ', /* product     : 16bytes */
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  '0', '.', '0', '1',                     /* version     : 4bytes */
};


/** @defgroup USBD_MEM_Exported_FunctionsPrototype
  * @{
  */
/* USBD_STORAGE_fops is defined in usbd_storage.c (main/msc/) */



/**
 * @brief  MSC_STORAGE_Init
 *         Initializes the storage unit (medium)
 * @param  lun: Logical unit number
 * @retval Status (0:OK / -1:Error)
 */
uint8_t MSC_STORAGE_Init(uint8_t lun)
{
  return USBD_STORAGE_fops->Init(lun);
}

/**
 * @brief  MSC_STORAGE_GetCapacity
 *         Return the medium capacity
 * @param  lun: Logical unit number
 * @param  block_num: Number of total block number
 * @param  block_size: Block size
 * @retval Status (0:OK / -1:Error)
 */
uint8_t  MSC_STORAGE_GetCapacity(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{
  return USBD_STORAGE_fops->GetCapacity(lun, block_num, block_size);
}

/**
 * @brief  MSC_STORAGE_IsReady
 *         Check whether the medium is ready
 * @param  lun: Logical unit number
 * @retval Status (0:OK / -1:Error)
 */
uint8_t MSC_STORAGE_IsReady(uint8_t lun)
{
  return USBD_STORAGE_fops->IsReady(lun);
}


/**
 * @brief  MSC_STORAGE_IsWriteProtected
 *         Check whether the medium is write protected.
 * @param  lun: Logical unit number
 * @retval Status (0: write enabled / -1:otherwise)
 */
uint8_t MSC_STORAGE_IsWriteProtected(uint8_t lun)
{
  return USBD_STORAGE_fops->IsWriteProtected(lun);
}

/**
 * @brief  MSC_STORAGE_Read
 *         Reads data from the medium
 * @param  lun: Logical unit number
 * @param  buf: data buffer
 * @param  blk_addr: logical block address
 * @param  blk_len: block number
 * @retval Status (0: ok / -1: error)
 */
uint8_t MSC_STORAGE_Read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
  return USBD_STORAGE_fops->Read(lun, buf, blk_addr, blk_len);
}

/**
 * @brief  MSC_STORAGE_Write
 *         Writes data into the medium
 * @param  lun: Logical unit number
 * @param  buf: data buffer
 * @param  blk_addr: logical block address
 * @param  blk_len: block number
 * @retval Status (0: ok / -1: error)
 */
uint8_t MSC_STORAGE_Write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
  return USBD_STORAGE_fops->Write(lun, buf, blk_addr, blk_len);
}

/**
 * @brief  MSC_STORAGE_GetMaxLun
 *         Return the max supported luns
 * @retval Lun(s) number
 */
uint8_t MSC_STORAGE_GetMaxLun(void)
{
  return USBD_STORAGE_fops->GetMaxLun();
}

/**
 * @brief  MSC_STORAGE_Inquiry
 *         Return the inquiry data
 * @param  lun: Logical unit number
 * @retval Inquiry data pointer
 */
uint8_t *MSC_STORAGE_Inquiry(uint8_t lun)
{
  UNUSED(lun);
  return (uint8_t *)USBD_STORAGE_fops->pInquiry;
}





/************************ (C) COPYRIGHT FMD *****END OF FILE****/
