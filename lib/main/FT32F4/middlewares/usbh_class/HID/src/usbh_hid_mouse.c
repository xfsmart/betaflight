/**
  ******************************************************************************
  * @file    			usbh_hid_mouse.c
  * @author  			FMD XA
  * @brief   			This file is the HID layer handlers for usb host hid class.
  * @version 			V1.0.0           
  * @data		 			2025-04-16
  ******************************************************************************
  *
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "usbh_hid_mouse.h"
#include "usbh_hid_parser.h"

/**@addtogroup USBH_LIB
 * @{
 */

/**@addtogroup USBH_CLASS
 * @{
 */

/**@addtogroup USBH_HID_CLASS
 * @{
 */

/**@defgroup USBH_HID_MOUSE
 * @brief This file includes HID Layer Handlers for USB Host HID class.
 * @{
 */

/**@defgroup USBH_HID_MOUSE_Private_TypeDefinitions
 * @{
 */

/**
 * @}
 */


/**@defgroup USBH_HID_MOUSE_Private_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup USBH_HID_MOUSE_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup USBH_HID_MOUSE_Private_FunctionPrototypes
 * @{
 */
static USBH_StatusTypeDef USBH_HID_MouseDecode(USBH_HandleTypeDef *phost);

/**
 * @}
 */


/**@defgroup USBH_HID_MOUSE_Private_Variables
 * @{
 */
HID_MOUSE_Info_TypeDef  mouse_info;
uint8_t                 mouse_report_data[USBH_HID_MOUSE_REPORT_SIZE];
uint8_t                 mouse_rx_report_buf[USBH_HID_MOUSE_REPORT_SIZE];

/* structures defining how to access item in a HID mouse report */
/* access button 1 state */
static const HID_Report_ItemTypeDef prop_b1 =
{
  mouse_report_data,  /* data */
  1,                  /* size */
  0,                  /* shift */
  0,                  /* count (only for array items) */
  0,                  /* signed */
  0,                  /* min value read can return */
  1,                  /* max value read can return */
  0,                  /* min value device can report */
  1,                  /* max value device can report */
  1,                  /* resolution */
};

/* access button 2 state */
static const HID_Report_ItemTypeDef prop_b2 =
{
  mouse_report_data,  /* data */
  1,                  /* size */
  1,                  /* shift */
  0,                  /* count (only for array items) */
  0,                  /* signed */
  0,                  /* min value read can return */
  1,                  /* max value read can return */
  0,                  /* min value device can report */
  1,                  /* max value device can report */
  1,                  /* resolution */
};

/* access button 3 state */
static const HID_Report_ItemTypeDef prop_b3 =
{
  mouse_report_data,  /* data */
  1,                  /* size */
  2,                  /* shift */
  0,                  /* count (only for array items) */
  0,                  /* signed */
  0,                  /* min value read can return */
  1,                  /* max value read can return */
  0,                  /* min value device can report */
  1,                  /* max value device can report */
  1,                  /* resolution */
};

/* access x coordinate change. */
static const HID_Report_ItemTypeDef prop_x =
{
  mouse_report_data + 1U, /* data */
  8,                      /* size */
  0,                      /* shift */
  0,                      /* count (only for array items) */
  1,                      /* signed */
  0,                      /* min value read can return */
  0xFFFF,                 /* max value read can return */
  0,                      /* min value device can report */
  0xFFFF,                 /* max value device can report */
  1,                      /* resolution */
};

/* access y coordinate change */
static const HID_Report_ItemTypeDef prop_y =
{
  mouse_report_data + 2U, /* data */
  8,                      /* size */
  0,                      /* shift */
  0,                      /* count (only for array items) */
  1,                      /* signed */
  0,                      /* min value read can return */
  0xFFFF,                 /* max value read can return */
  0,                      /* min value device can report */
  0xFFFF,                 /* max value device can report */
  1,                      /* resolution */
};



/**
 * @}
 */



/**@defgroup USBH_HID_CORE_Private_Functions
 * @{
 */

/**
 * @brief  USBH_HID_MouseInit
 *         The function init the HID mouse.
 * @param  phost: host handle
 * @retval USBH Status
 */
USBH_StatusTypeDef USBH_HID_MouseInit(USBH_HandleTypeDef *phost)
{
  uint32_t i;
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  mouse_info.x = 0U;
  mouse_info.y = 0U;
  mouse_info.buttons[0] = 0U;
  mouse_info.buttons[1] = 0U;
  mouse_info.buttons[2] = 0U;

  for (i = 0U; i < sizeof(mouse_report_data); i++)
  {
    mouse_report_data[i] = 0U;
    mouse_rx_report_buf[i] = 0U;
  }

  if (HID_Handle->length > sizeof(mouse_report_data))
  {
    HID_Handle->length = (uint16_t)sizeof(mouse_report_data);
  }
  HID_Handle->pData = mouse_rx_report_buf;

  if ((HID_QUEUE_SIZE * sizeof(mouse_report_data)) > sizeof(phost->device.Data))
  {
    return USBH_FAIL;
  }
  else
  {
    USBH_HID_FifoInit(&HID_Handle->fifo, phost->device.Data,
                      (uint16_t)(HID_QUEUE_SIZE * sizeof(mouse_report_data)));
  }

  return USBH_OK;
}

/**
 * @brief  USBH_HID_GetMouseInfo
 *         The function return mouse information.
 * @param  phost: host handle
 * @retval USBH Status
 */
HID_MOUSE_Info_TypeDef *USBH_HID_GetMouseInfo(USBH_HandleTypeDef *phost)
{
  if (USBH_HID_MouseDecode(phost) == USBH_OK)
  {
    return &mouse_info;
  }
  else
  {
    return NULL;
  }
}


/**
 * @brief  USBH_HID_MouseDecode
 *         The function decode mouse data.
 * @param  phost: host handle
 * @retval USBH Status
 */
static USBH_StatusTypeDef USBH_HID_MouseDecode(USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle = (HID_HandleTypeDef *) phost->pActiveClass->pData;

  if ((HID_Handle->length == 0U) || (HID_Handle->fifo.buf = NULL))
  {
    return USBH_FAIL;
  }
  /* fill report */
  if (USBH_HID_FifoRead(&HID_Handle->fifo, &mouse_report_data, HID_Handle->length) == HID_Handle->length)
  {
    /* decode report */
    mouse_info.x = (uint8_t)HID_ReadItem((HID_Report_ItemTypeDef *) &prop_x, 0U);
    mouse_info.y = (uint8_t)HID_ReadItem((HID_Report_ItemTypeDef *) &prop_y, 0U);
    mouse_info.buttons[0] = (uint8_t)HID_ReadItem((HID_Report_ItemTypeDef *) &prop_b1, 0U);
    mouse_info.buttons[1] = (uint8_t)HID_ReadItem((HID_Report_ItemTypeDef *) &prop_b2, 0U);
    mouse_info.buttons[2] = (uint8_t)HID_ReadItem((HID_Report_ItemTypeDef *) &prop_b3, 0U);

    return USBH_OK;
  }

  return USBH_FAIL;
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
