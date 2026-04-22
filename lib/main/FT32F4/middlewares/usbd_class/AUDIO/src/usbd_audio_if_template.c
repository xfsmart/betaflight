/**
  ******************************************************************************
  * @file    usbd_cdc_if_template.c
  * @author  FMD
  * @brief   Generic media access Layer.
  * @date    26-03-2026
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_audio_if_template.h"

/** @addtogroup FT32_USB_DEVICE_LIBRARY
  * @{
  */


/** @defgroup USBD_AUDIO
  * @brief usbd core module
  * @{
  */

/** @defgroup USBD_AUDIO_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */


/** @defgroup USBD_AUDIO_Private_Defines
  * @{
  */
/**
  * @}
  */


/** @defgroup USBD_AUDIO_Private_Macros
  * @{
  */

/**
  * @}
  */


/** @defgroup USBD_AUDIO_Private_FunctionPrototypes
  * @{
  */

static int8_t  TEMPLATE_Init(uint32_t  AudioFreq, uint32_t Volume, uint32_t options);
static int8_t  TEMPLATE_DeInit(uint32_t options);
static int8_t  TEMPLATE_AudioCmd(uint8_t *pbuf, uint32_t size, uint8_t cmd);
static int8_t  TEMPLATE_VolumeCtl(uint8_t vol);
static int8_t  TEMPLATE_MuteCtl(uint8_t cmd);
static int8_t  TEMPLATE_PeriodicTC(uint8_t *pbuf, uint32_t size, uint8_t cmd);
static int8_t  TEMPLATE_GetState(void);

USBD_AUDIO_ItfTypeDef USBD_AUDIO_Template_fops =
{
  TEMPLATE_Init,
  TEMPLATE_DeInit,
  TEMPLATE_AudioCmd,
  TEMPLATE_VolumeCtl,
  TEMPLATE_MuteCtl,
  TEMPLATE_PeriodicTC,
  TEMPLATE_GetState,
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  TEMPLATE_Init
  *         Initializes the AUDIO media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t TEMPLATE_Init(uint32_t  AudioFreq, uint32_t Volume, uint32_t options)
{
  UNUSED(AudioFreq);
  UNUSED(Volume);
  UNUSED(options);

  /*
     Add your initialization code here
  */
  return (0);
}

/**
  * @brief  TEMPLATE_DeInit
  *         DeInitializes the AUDIO media low layer
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t TEMPLATE_DeInit(uint32_t options)
{
  UNUSED(options);

  /*
     Add your deinitialization code here
  */
  return (0);
}


/**
  * @brief  TEMPLATE_AudioCmd
  *         AUDIO command handler
  * @param  Buf: Buffer of data to be sent
  * @param  size: Number of data to be sent (in bytes)
  * @param  cmd: command opcode
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t TEMPLATE_AudioCmd(uint8_t *pbuf, uint32_t size, uint8_t cmd)
{
  UNUSED(pbuf);
  UNUSED(size);
  UNUSED(cmd);

  return (0);
}

/**
  * @brief  TEMPLATE_VolumeCtl
  * @param  vol: volume level (0..100)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t TEMPLATE_VolumeCtl(uint8_t vol)
{
  UNUSED(vol);

  return (0);
}

/**
  * @brief  TEMPLATE_MuteCtl
  * @param  cmd: vmute command
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t TEMPLATE_MuteCtl(uint8_t cmd)
{
  UNUSED(cmd);

  return (0);
}

/**
  * @brief  TEMPLATE_PeriodicTC
  * @param  cmd
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t TEMPLATE_PeriodicTC(uint8_t *pbuf, uint32_t size, uint8_t cmd)
{
  UNUSED(pbuf);
  UNUSED(size);
  UNUSED(cmd);

  return (0);
}

/**
  * @brief  TEMPLATE_GetState
  * @param  None
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t TEMPLATE_GetState(void)
{
  return (0);
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

