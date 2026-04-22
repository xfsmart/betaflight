/**
  ******************************************************************************
  * @file    			usbd_msc_storage.c
  * @author  			FMD XA
  * @brief   			Memory mangement layer.
  * @version 			V1.0.0           
  * @date		 			2026-03-27
  ******************************************************************************
  *
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_msc_storage.h"

/**@addtogroup USBD_MSC_CLASS
 * @{
 */

/**@defgroup USBD_MSC_STORAGE_TEMPLATE
 * @{
 */

/**@defgroup USBD_MSC_STORAGE_TEMPLATE_Private_TypeDefinitions
 * @{
 */

/**
 * @}
 */


/**@defgroup USBD_MSC_STORAGE_TEMPLATE_Private_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MSC_STORAGE_TEMPLATE_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_MSC_STORAGE_TEMPLATE_Private_Variables
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MSC_STORAGE_TEMPLATE_Private_FunctionPrototypes
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MSC_STORAGE_TEMPLATE_Private_Functions
 * @{
 */
#define STORAGE_LUN_NBR         1U
#define STORAGE_BLK_NBR         0x10000U
#define STORAGE_BLK_SIZ         0x200U

int8_t STORAGE_Init(uint8_t lun);
int8_t STORAGE_GetCapacity(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
int8_t STORAGE_IsReady(uint8_t lun);
int8_t STORAGE_IsWriteProtected(uint8_t lun);
int8_t STORAGE_Read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
int8_t STORAGE_Write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
int8_t STORAGE_GetMaxLun(void);

/* USB mass storage standard inquiry data */
int8_t STORAGE_Inquirydata[] = /* 36 */
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

USBD_StorageTypeDef USBD_MSC_Storage_fops =
{
  STORAGE_Init,
  STORAGE_GetCapacity,
  STORAGE_IsReady,
  STORAGE_IsWriteProtected,
  STORAGE_Read,
  STORAGE_Write,
  STORAGE_GetMaxLun,
  STORAGE_Inquirydata,
};

/**
 * @brief  STORAGE_Init
 *         Initializes the storage unit (medium)
 * @param  lun: Logical unit number
 * @retval Status (0:OK / -1:Error)
 */
int8_t STORAGE_Init(uint8_t lun)
{

  return (0);
}

/**
 * @brief  STORAGE_GetCapacity
 *         Return the medium capacity
 * @param  lun: Logical unit number
 * @param  block_num: Number of total block number
 * @param  block_size: Block size
 * @retval Status (0:OK / -1:Error)
 */
int8_t STORAGE_GetCapacity(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{

  *block_num = STORAGE_BLK_NBR;
  *block_size= STORAGE_BLK_SIZ;
  return (0);
}

/**
 * @brief  STORAGE_IsReady
 *         Check whether the medium is ready
 * @param  lun: Logical unit number
 * @retval Status (0:OK / -1:Error)
 */
int8_t STORAGE_IsReady(uint8_t lun)
{

  return (0);
}


/**
 * @brief  STORAGE_IsWriteProtected
 *         Check whether the medium is write protected.
 * @param  lun: Logical unit number
 * @retval Status (0: write enabled / -1:otherwise)
 */
int8_t STORAGE_IsWriteProtected(uint8_t lun)
{

  return (0);
}

/**
 * @brief  STORAGE_Read
 *         Reads data from the medium
 * @param  lun: Logical unit number
 * @param  buf: data buffer
 * @param  blk_addr: logical block address
 * @param  blk_len: block number
 * @retval Status (0: ok / -1: error)
 */
int8_t STORAGE_Read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{

  return (0);
}

/**
 * @brief  STORAGE_Write
 *         Writes data into the medium
 * @param  lun: Logical unit number
 * @param  buf: data buffer
 * @param  blk_addr: logical block address
 * @param  blk_len: block number
 * @retval Status (0: ok / -1: error)
 */
int8_t STORAGE_Write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{

  return (0);
}

/**
 * @brief  STORAGE_GetMaxLun
 *         Return the max supported luns
 * @retval Lun(s) number
 */
int8_t STORAGE_GetMaxLun(void)
{

  return (STORAGE_LUN_NBR - 1);
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


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
