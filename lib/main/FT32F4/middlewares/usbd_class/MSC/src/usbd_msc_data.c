/**
  ******************************************************************************
  * @file    			usbd_msc_data.c
  * @author  			FMD XA
  * @brief   			This file provides all the vital inquiry pages and sense data.
  * @version 			V1.0.0           
  * @date		 			2025-05-06
  ******************************************************************************
  *
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_msc_data.h"

/**@addtogroup USBD_MSC_CLASS
 * @{
 */

/**@defgroup USBD_MSC_DATA
 * @brief mass storage info/data module.
 * @{
 */

/**@defgroup USBD_MSC_DATA_Private_TypeDefinitions
 * @{
 */

/**
 * @}
 */


/**@defgroup USBD_MSC_DATA_Private_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MSC_DATA_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBD_MSC_DATA_Private_Variables
 * @{
 */

/* USB mass storage page 0 inquiry data */
uint8_t MSC_Page00_Inquiry_Data[LENGTH_INQUIRY_PAGE00] =
{
  0x00,
  0x00,
  0x00,
  (LENGTH_INQUIRY_PAGE00 - 4U),
  0x00,
  0x80
};
/* USB mass storage VPD page 0x80 inquiry data for unit serial number */
uint8_t MSC_Page80_Inquiry_Data[LENGTH_INQUIRY_PAGE80] =
{
  0x00,
  0x80,
  0x00,
  LENGTH_INQUIRY_PAGE80,
  0x20,
  0x20,
  0x20,
  0x20
};
/* USB mass storage sense 6 data */
uint8_t MSC_Mode_Sense6_data[MODE_SENSE6_LEN] =
{
  0x03,       /* MODE DATA LENGTH. The number of bytes that follow */
  0x00,       /* MEDIUM TYPE. 00h for sbc devices */
  0x00,       /* DEVICE-SPECIFIC PARAMETER. For SBC devices:
               * bit7: WP. set to 1 if the media is write-protected
               * bit6-5: reserved
               * bit4: DPOFUA. set to 1 if the device supports the DP0 and FUA bits
               * bit3-0: reserved
               */
  0x00,       /* BLOCK DESCRIPTOR LENGTH */
};

/* USB mass storage sense 10 data */
uint8_t MSC_Mode_Sense10_data[MODE_SENSE10_LEN] =
{
  0x00,       /* MODE DATA LENGTH MSB. */
  0x06,       /* MODE DATA LENGTH LSB. The number of bytes that follow */
  0x00,       /* MEDIUM TYPE. 00h for sbc devices */
  0x00,       /* DEVICE-SPECIFIC PARAMETER. For SBC devices:
               * bit7: WP. set to 1 if the media is write-protected
               * bit6-5: reserved
               * bit4: DPOFUA. set to 1 if the device supports the DP0 and FUA bits
               * bit3-0: reserved
               */
  0x00,       /* LONGLBA Set to zero */
  0x00,       /* Reserved */
  0x00,       /* BLOCK DESCRIPTOR LENGTH MSB */
  0x00,       /* BLOCK DESCRIPTOR LENGTH LSB */
};

/**
 * @}
 */

/**@defgroup USBD_MSC_DATA_Private_FunctionPrototypes
 * @{
 */

/**
 * @}
 */

/**@defgroup USBD_MSC_DATA_Private_Functions
 * @{
 */

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
