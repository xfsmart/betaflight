/**
  ******************************************************************************
  * @file    			usbd_msc_bot.h
  * @author  			FMD XA
  * @brief   			header file for the usbd_msc_bot.c
  * @version 			V1.0.0           
  * @data		 			2025-04-30
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_MSC_BOT_H
#define __USBD_MSC_BOT_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_core.h"

/**@addtogroup USBD_LIB
 * @{
 */

/**@addtogroup USBD_CLASS
 * @{
 */

/**@addtogroup USBD_MSC_CLASS
 * @{
 */

/**@defgroup USBD_MSC_BOT
 * @brief This file is Header file for usbd_msc_bot.c
 * @{
 */


/**@defgroup USBD_MSC_BOT_Exported_Defines
 * @{
 */
#define USBD_BOT_IDLE               0U      /* idle state */
#define USBD_BOT_DATA_OUT           1U      /* data out state */
#define USBD_BOT_DATA_IN            2U      /* data in state */
#define USBD_BOT_LAST_DATA_IN       3U      /* last data in last */
#define USBD_BOT_SEND_DATA          4U      /* send immediate data */
#define USBD_BOT_NO_DATA            5U      /* no data stage */

#define USBD_BOT_CBW_SIGNATURE      0x43425355U
#define USBD_BOT_CSW_SIGNATURE      0x53425355U
#define USBD_BOT_CBW_LENGTH         31U
#define USBD_BOT_CSW_LENGTH         13U
#define USBD_BOT_MAX_DATA           256U

/* CSW Status Definitions */
#define USBD_CSW_CMD_PASSED         0x00U
#define USBD_CSW_CMD_FAILED         0x01U
#define USBD_CSW_PHASE_ERROR        0x02U

/* BOT Status */
#define USBD_BOT_STATUS_NORMAL      0U
#define USBD_BOT_STATUS_RECOVERY    1U
#define USBD_BOT_STATUS_ERROR       2U


#define USBD_DIR_IN                 0U
#define USBD_DIR_OUT                1U
#define USBD_BOTH_DIR               2U

/**
 * @}
 */

/**@defgroup USBD_MSC_BOT_Exported_TypesDefinitions
 * @{
 */
typedef struct
{
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataLength;
  uint8_t  bmFlags;
  uint8_t  bLUN;
  uint8_t  bCBLength;
  uint8_t  CB[16];
  uint8_t  ReservedForAlign;
} USBD_MSC_BOT_CBWTypeDef;

typedef struct
{
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataResidue;
  uint8_t  bStatus;
  uint8_t  ReservedForAlign[3];
} USBD_MSC_BOT_CSWTypeDef;

/**
 * @}
 */

/**@defgroup USBD_MSC_BOT_Exported_Macros
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MSC_BOT_Exported_Variables
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MSC_BOT_Exported_FunctionsPrototype
 * @{
 */
void  MSC_BOT_Init(USBD_HandleTypeDef *pdev);
void  MSC_BOT_Reset(USBD_HandleTypeDef *pdev);
void  MSC_BOT_DeInit(USBD_HandleTypeDef *pdev);
void  MSC_BOT_DataIn(USBD_HandleTypeDef *pdev);
void  MSC_BOT_DataOut(USBD_HandleTypeDef *pdev);
void  MSC_BOT_SendCSW(USBD_HandleTypeDef *pdev, uint8_t CSW_Status);
void  MSC_BOT_CplClrFeature(USBD_HandleTypeDef *pdev, uint8_t epnum);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /*__USBD_MSC_BOT_H*/
/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
