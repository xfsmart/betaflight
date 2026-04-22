/**
  ******************************************************************************
  * @file    			usbh_hid_usage.h
  * @author  			FMD XA
  * @brief   			This file contains the USAGE page codes
  * @version 			V1.0.0           
  * @data		 			2025-04-15
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBH_HID_USAGE_H
#define __USBH_HID_USAGE_H

#ifdef  __cplusplus
extern "c" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbh_core.h"
#include "usbh_hid_mouse.h"
#include "usbh_hid_keybd.h"

/**@addtogroup USBH_LIB
 * @{
 */

/**@addtogroup USBH_CLASS
 * @{
 */

/**@addtogroup USBH_HID_CLASS
 * @{
 */

/**@defgroup USBH_HID_USAGE
 * @brief This file is the Header file for usbh_hid_usage.c
 * @{
 */

/**@defgroup USBH_HID_USAGE_Exported_Types
 * @{
 */
/********************************************************************/
/******************HID 1.11 usage pages******************************/
/********************************************************************/

#define HID_USAGE_PAGE_UNDEFINED      uint16_t (0x00)   /* undefined */
/***Top level pages***/
#define HID_USAGE_PAGE_GEN_DES        uint16_t (0x01)   /* Generic Desktop Controls */
#define HID_USAGE_PAGE_SIM_CTR        uint16_t (0x02)   /* Simulation Controls */
#define HID_USAGE_PAGE_VR_CTR         uint16_t (0x03)   /* VR Controls */
#define HID_USAGE_PAGE_SPORT_CTR      uint16_t (0x04)   /* Sport controls */
#define HID_USAGE_PAGE_GAME_CTR       uint16_t (0x05)   /* Game Controls */
#define HID_USAGE_PAGE_GEN_DEV        uint16_t (0x06)   /* Generic Device Controls */
#define HID_USAGE_PAGE_KEYB           uint16_t (0x07)   /* Keyboard/keypad */
#define HID_USAGE_PAGE_LED            uint16_t (0x08)   /* LEDs */
#define HID_USAGE_PAGE_BUTTON         uint16_t (0x09)   /* Button */
#define HID_USAGE_PAGE_ORDINAL        uint16_t (0x0A)   /* Ordinal */
#define HID_USAGE_PAGE_PHONE          uint16_t (0x0B)   /* Telephony */
#define HID_USAGE_PAGE_CONSUMER       uint16_t (0x0C)   /* Consumer */
#define HID_USAGE_PAGE_DEGITIZER      uint16_t (0x0D)   /* Digitizer */
#define HID_USAGE_PAGE_HAPTICS        uint16_t (0x0E)   /* Haptics */
#define HID_USAGE_PAGE_PID            uint16_t (0x0F)   /* PID Page (force feedback and related devices) */
#define HID_USAGE_PAGE_UNICODE        uint16_t (0x10)   /* Unicode */
/* 11~13 Reserved */
#define HID_USAGE_PAGE_ALNUM_DISP     uint16_t (0x14)   /* Alphanumeric display */
/* 15~1f Reserved */
/***END of Top level pages***/
/* 25~3f Reserved */
#define HID_USAGE_PAGE_MEDICAL        uint16_t (0x40)   /* Medical Instruments */
/* 41~7f Reserved */
/* 80~83 Monitor pages USB Device Class Definition for Monitor Devices
 * 84~87 Power pages USB Device Class Definityion for Power Devices */
/* 88~8B Reserved */
#define HID_USAGE_PAGE_BARCODE        uint16_t (0x8C)   /* Bar code scanner page */
#define HID_USAGE_PAGE_SCALE          uint16_t (0x8D)   /* Scale page */
#define HID_USAGE_PAGE_MSR            uint16_t (0x8E)   /* Magnetic Stripe Reading(MSR) Devices */
#define HID_USAGE_PAGE_POS            uint16_t (0x8F)   /* Reserved Point of Sale page */
#define HID_USAGE_PAGE_CAMERA_CTR     uint16_t (0x90)   /* camera control page */
#define HID_USAGE_PAGE_ARCADE         uint16_t (0x91)   /* Arcade page */

/********************************************************************/
/*****Usage definitions for the "Generic Desktop" page***************/
/********************************************************************/
#define HID_USAGE_UNDEFINED           uint16_t (0x00)   /* undefined */
#define HID_USAGE_POINTER             uint16_t (0x01)   /* Pointer (physical collection) */
#define HID_USAGE_MOUSE               uint16_t (0x02)   /* Mouse (application collection) */
/* 03 Reserved */
#define HID_USAGE_JOYSTICK            uint16_t (0x04)   /* Joystick (application collection) */
#define HID_USAGE_GAMEPAD             uint16_t (0x05)   /* Game pad (application collection) */
#define HID_USAGE_KBD                 uint16_t (0x06)   /* keyboard (application collection) */
#define HID_USAGE_KEYPAD              uint16_t (0x07)   /* keypad (application collection) */
#define HID_USAGE_MAX_CTR             uint16_t (0x08)   /* Multi-axis controller (application collection) */
/* 09~2f Reserved */
#define HID_USAGE_X                   uint16_t (0x30)   /* X (Dynamic value) */
#define HID_USAGE_Y                   uint16_t (0x31)   /* Y (Dynamic value) */
#define HID_USAGE_Z                   uint16_t (0x32)   /* Z (Dynamic value) */
#define HID_USAGE_RX                  uint16_t (0x33)   /* RX (Dynamic value) */
#define HID_USAGE_RY                  uint16_t (0x34)   /* RY (Dynamic value) */
#define HID_USAGE_RZ                  uint16_t (0x35)   /* RZ (Dynamic value) */
#define HID_USAGE_SLIDER              uint16_t (0x36)   /* Slider (Dynamic value) */
#define HID_USAGE_DIAL                uint16_t (0x37)   /* Dial (Dynamic value) */
#define HID_USAGE_WHEEL               uint16_t (0x38)   /* Wheel (Dynamic value) */
#define HID_USAGE_HATSW               uint16_t (0x39)   /* Hat switch (Dynamic value) */
#define HID_USAGE_COUNTEDBUF          uint16_t (0x3A)   /* Counted buffer (Logical collection) */
#define HID_USAGE_BYTECOUNT           uint16_t (0x3B)   /* Byte Counted (Dynamic value) */
#define HID_USAGE_MOTTONWAKE          uint16_t (0x3C)   /* Motion wakeup (one shot control) */
#define HID_USAGE_START               uint16_t (0x3D)   /* Strat (on/off control) */
#define HID_USAGE_SELECT              uint16_t (0x3E)   /* Select (on/off control) */
/* 3f Reserved */
#define HID_USAGE_VX                  uint16_t (0x40)   /* VX (Dynamic value) */
#define HID_USAGE_VY                  uint16_t (0x41)   /* VY (Dynamic value) */
#define HID_USAGE_VZ                  uint16_t (0x42)   /* VZ (Dynamic value) */
#define HID_USAGE_VBRX                uint16_t (0x43)   /* Vbrx (Dynamic value) */
#define HID_USAGE_VBRY                uint16_t (0x44)   /* Vbry (Dynamic value) */
#define HID_USAGE_VBRZ                uint16_t (0x45)   /* Vbrz (Dynamic value) */
#define HID_USAGE_VNO                 uint16_t (0x46)   /* Vno (Dynamic value) */
#define HID_USAGE_FEATNOTIF           uint16_t (0x47)   /* Feature Notification (Dynamic value), (Dynamic Flag) */
/* 48~7f Reserved */
#define HID_USAGE_SYSCTL              uint16_t (0x80)   /* System control (Application collection) */
#define HID_USAGE_PWDOWN              uint16_t (0x81)   /* System power down (one shot control) */
#define HID_USAGE_SLEEP               uint16_t (0x82)   /* System sleep (one shot control) */
#define HID_USAGE_WAKEUP              uint16_t (0x83)   /* System wake up (one shot control) */
#define HID_USAGE_CONTEXTM            uint16_t (0x84)   /* System context menu (one shot control) */
#define HID_USAGE_MAINM               uint16_t (0x85)   /* System main menu (one shot control) */
#define HID_USAGE_APPM                uint16_t (0x86)   /* System app menu (one shot control) */
#define HID_USAGE_MENUHELP            uint16_t (0x87)   /* System menu help (one shot control) */
#define HID_USAGE_MENUEXIT            uint16_t (0x88)   /* System menu exit (one shot control) */
#define HID_USAGE_MENUSELECT          uint16_t (0x89)   /* System menu select (one shot control) */
#define HID_USAGE_SYSM_RIGHT          uint16_t (0x8A)   /* System menu right (Re-Trigger control) */
#define HID_USAGE_SYSM_LEFT           uint16_t (0x8B)   /* System menu left (Re-Trigger control) */
#define HID_USAGE_SYSM_UP             uint16_t (0x8C)   /* System menu up (Re-Trigger control) */
#define HID_USAGE_SYSM_DOWN           uint16_t (0x8D)   /* System menu down (Re-Trigger control) */
#define HID_USAGE_COLDRESET           uint16_t (0x8E)   /* System cold restart (one shot control) */
#define HID_USAGE_WARMRESET           uint16_t (0x8F)   /* System warm restart (one shot control) */
#define HID_USAGE_DUP                 uint16_t (0x90)   /* D-pad up (on/off control) */
#define HID_USAGE_DDOWN               uint16_t (0x91)   /* D-pad down (on/off control) */
#define HID_USAGE_DRIGHT              uint16_t (0x92)   /* D-pad right (on/off control) */
#define HID_USAGE_DLEFT               uint16_t (0x93)   /* D-pad left (on/off control) */
/* 94~9f Reserved */
#define HID_USAGE_SYS_DOCK            uint16_t (0xA0)   /* System Dock (one shot control) */
#define HID_USAGE_SYS_UNDOCK          uint16_t (0xA1)   /* System Undock (one shot control) */
#define HID_USAGE_SYS_SETUP           uint16_t (0xA2)   /* System Setup (one shot control) */
#define HID_USAGE_SYS_BREAK           uint16_t (0xA3)   /* System Break (one shot control) */
#define HID_USAGE_SYS_DBGBRK          uint16_t (0xA4)   /* System Debugger Break (one shot control) */
#define HID_USAGE_APP_BRK             uint16_t (0xA5)   /* Application Break (one shot control) */
#define HID_USAGE_APP_DBGBRK          uint16_t (0xA6)   /* Application debugger Break (one shot control) */
#define HID_USAGE_SYS_SPKMUTE         uint16_t (0xA7)   /* System Speaker Mute (one shot control) */
#define HID_USAGE_SYS_HIBERN          uint16_t (0xA8)   /* System Hibernate (one shot control) */
/* a9~af Reserved */
#define HID_USAGE_SYS_SIDPINV         uint16_t (0xB0)   /* System Display Invert (one shot control) */
#define HID_USAGE_SYS_DISPINT         uint16_t (0xB1)   /* System Display Internal (one shot control) */
#define HID_USAGE_SYS_DISPEXT         uint16_t (0xB2)   /* System Display External (one shot control) */
#define HID_USAGE_SYS_DISPBOTH        uint16_t (0xB3)   /* System Display Both (one shot control) */
#define HID_USAGE_SYS_DISPDUAL        uint16_t (0xB4)   /* System Display Dual (one shot control) */
#define HID_USAGE_SYS_DISPTGLIE       uint16_t (0xB5)   /* System Display toggle int/ext (one shot control) */
#define HID_USAGE_SYS_DISP_SWAP       uint16_t (0xB6)   /* System Display swap primary/secondary (one shot control) */
#define HID_USAGE_SYS_DISP_LCDA       uint16_t (0xB7)   /* System Display LCD Autoscale(one shot control) */
/* b8~ffff Reserved */



/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /*__USBH_HID_USAGE_H*/
/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
