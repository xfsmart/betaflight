/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>

#include "platform.h"

#ifdef USE_USB_CDC_HID

#include "vcpf4/usbd_cdc_vcp.h"
#include "io/usb_cdc_hid.h"

#include "usbd_hid.h"

/* USB device handle defined in vcpf4/usbd_cdc_vcp.c */
extern USBD_HandleTypeDef USBD_Device;

void sendReport(uint8_t *report, uint8_t len)
{
#ifdef USE_USBD_COMPOSITE
    /* HID is registered as the first composite class (classId 0). */
    USBD_HID_SendReport(&USBD_Device, report, len, 0U);
#else
    USBD_HID_SendReport(&USBD_Device, report, len);
#endif
}

#endif
