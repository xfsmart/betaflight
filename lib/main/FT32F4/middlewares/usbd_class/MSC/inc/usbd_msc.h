/**
  ******************************************************************************
  * @file    			usbd_msc.h
  * @author  			FMD XA
  * @brief   			This file contains all the prototypes for the usbd_msc.c
  * @version 			V1.0.0           
  * @data		 			2025-04-30
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_MSC_H
#define __USBD_MSC_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_msc_bot.h"
#include "usbd_msc_scsi.h"
#include "usbd_ioreq.h"

/**@addtogroup USBD_LIB
 * @{
 */

/**@addtogroup USBD_CLASS
 * @{
 */

/**@addtogroup USBD_MSC_CLASS
 * @{
 */

/**@defgroup USBD_MSC_CORE
 * @brief This file is Header file for usbd_msc.c
 * @{
 */


/**@defgroup USBD_MSC_CORE_Exported_Defines
 * @{
 */

/* MSC Class config */
#ifndef MSC_MEDIA_PACKET
#define MSC_MEDIA_PACKET        512U
#endif  /* MSC_MEDIA_PACKET */

#define MSC_MAX_FS_PACKET       0x40U
#define MSC_MAX_HS_PACKET       0x200U

#define BOT_GET_MAX_LUN         0xFE
#define BOT_RESET               0xFF
#define USB_MSC_CONFIG_DESC_SIZ 32

#ifndef MSC_EPIN_ADDR
#define MSC_EPIN_ADDR           0x81U
#endif  /* MSC_EPIN_ADDR */

#ifndef MSC_EPOUT_ADDR
#define MSC_EPOUT_ADDR          0x01U
#endif  /* MSC_EPOUT_ADDR */

/**
 * @}
 */


/**@defgroup USBD_MSC_CORE_Exported_Types
 * @{
 */

typedef struct _USBD_STORAGE
{
  int8_t (* Init)(uint8_t lun);
  int8_t (* GetCapacity)(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
  int8_t (* IsReady)(uint8_t lun);
  int8_t (* IsWriteProtected)(uint8_t lun);
  int8_t (* Read)(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
  int8_t (* Write)(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
  int8_t (* GetMaxLun)(void);
  int8_t *pInquiry;
} USBD_StorageTypeDef;

typedef struct
{
  uint32_t                max_lun;
  uint32_t                interface;
  uint8_t                 bot_state;
  uint8_t                 bot_status;
  uint32_t                bot_data_length;
  uint8_t                 bot_data[MSC_MEDIA_PACKET];
  USBD_MSC_BOT_CBWTypeDef cbw;
  USBD_MSC_BOT_CSWTypeDef csw;
  USBD_SCSI_SenseTypeDef  scsi_sense[SENSE_LIST_DEEPTH];
  uint8_t                 scsi_sense_head;
  uint8_t                 scsi_sense_tail;
  uint8_t                 scsi_medium_state;
  uint16_t                scsi_blk_size;
  uint32_t                scsi_blk_nbr;
  uint32_t                scsi_blk_addr;
  uint32_t                scsi_blk_len;
} USBD_MSC_BOT_HandleTypeDef;

/**
 * @}
 */


/**@defgroup USBD_MSC_CORE_Exported_Macros
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MSC_CORE_Exported_Variables
 * @{
 */
extern USBD_ClassTypeDef        USBD_MSC;
#define USBD_MSC_CLASS          &USBD_MSC

/* Global storage function pointer (set by platform code via USBD_MSC_RegisterStorage) */
extern USBD_StorageTypeDef     *USBD_STORAGE_fops;

/**
 * @}
 */

/**@defgroup USBD_MSC_CORE_Exported_FunctionsPrototype
 * @{
 */

uint8_t USBD_MSC_RegisterStorage(USBD_HandleTypeDef *pdev, USBD_StorageTypeDef *fops);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /*__USBD_MSC_H*/
/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
