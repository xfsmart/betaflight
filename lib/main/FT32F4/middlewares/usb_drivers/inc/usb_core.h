/**
  **************************************************************************
  * @file     usb_core.h
  * @brief    usb core header file for USB_OTG_HS and USB_OTG_FS
  **************************************************************************
  */

/* define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_CORE_H
#define __USB_CORE_H
#include <stdlib.h>
#include <string.h>

#ifdef USE_OTG_DEVICE_MODE
#include "usbd_core.h"
#include "usbd_conf.h"
#include "usbd_def.h"
#include "usbd_desc.h"
#endif
#ifdef USE_OTG_HOST_MODE
#include "usbh_core.h"
#include "usbh_conf.h"
#include "usbh_def.h"
#endif



#endif

