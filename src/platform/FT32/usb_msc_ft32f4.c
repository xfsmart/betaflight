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

/*
 * Author: Chris Hockuba (https://github.com/conkerkh)
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "platform.h"

#if defined(USE_USB_MSC)

#include "build/build_config.h"

#include "common/utils.h"

#include "blackbox/blackbox.h"

#include "drivers/io.h"
#include "drivers/nvic.h"
#include "drivers/sdmmc_sdio.h"
#include "drivers/system.h"
#include "drivers/time.h"
#include "drivers/usb_msc.h"

#include "msc/usbd_storage.h"

#include "pg/sdcard.h"
#include "pg/usb.h"

#include "usb_conf.h"
#include "usbd_core.h"
#include "usbd_cdc_vcp.h"
#include "usbd_msc.h"
#include "usbd_msc_desc.h"
#include "usbd_composite_builder.h"
#include "drivers/usb_io.h"

extern USBD_HandleTypeDef       USBD_Device;
extern void USB_OTG_BSP_Init(void);
extern void USB_OTG_BSP_EnableInterrupt(void);

uint8_t mscStart(void)
{
    //Start USB
    usbGenerateDisconnectPulse();

    IOInit(IOGetByTag(IO_TAG(PA11)), OWNER_USB, 0);
    IOInit(IOGetByTag(IO_TAG(PA12)), OWNER_USB, 0);

    switch (blackboxConfig()->device) {
#ifdef USE_SDCARD
    case BLACKBOX_DEVICE_SDCARD:
        switch (sdcardConfig()->mode) {
#ifdef USE_SDCARD_SDIO
        case SDCARD_MODE_SDIO:
            USBD_STORAGE_fops = &USBD_MSC_MICRO_SDIO_fops;
            break;
#endif
#ifdef USE_SDCARD_SPI
        case SDCARD_MODE_SPI:
            USBD_STORAGE_fops = &USBD_MSC_MICRO_SD_SPI_fops;
            break;
#endif
        default:
            return 1;
        }
        break;
#endif

#ifdef USE_FLASHFS
    case BLACKBOX_DEVICE_FLASH:
        USBD_STORAGE_fops = &USBD_MSC_EMFAT_fops;
        break;
#endif
    default:
        return 1;
    }
    USB_OTG_BSP_Init();
    /* Under USE_USBD_COMPOSITE MSC must also register through the composite
     * builder so the core serves the configuration descriptor from USBD_CMPSIT
     * (NumClasses > 0); a plain single-class USBD_Init would dereference a
     * NULL descriptor callback. */
    if (USBD_Init(&USBD_Device, USB_OTG_FS_CORE_ID, OTG_USB_ID, NULL, &USBD_MSC_Desc) != USBD_OK)
    {
        return 1;
    }

    /* File-scope static so the builder never holds a pointer to a returned
     * stack frame. */
    static uint8_t mscEndpoints[] = { MSC_EPIN_ADDR, MSC_EPOUT_ADDR };
    if (USBD_RegisterClassComposite(&USBD_Device, &USBD_MSC, CLASS_TYPE_MSC, mscEndpoints) != USBD_OK)
    {
        return 1;
    }

    /* Select the MSC class before wiring the storage callbacks. A 0xFF result
     * means the class was not registered; never use it as an index. */
    if (USBD_CMPSIT_SetClassID(&USBD_Device, CLASS_TYPE_MSC, 0U) == 0xFFU)
    {
        return 1;
    }

    if (USBD_MSC_RegisterStorage(&USBD_Device, USBD_STORAGE_fops) != USBD_OK)
    {
        return 1;
    }

    // Start the device core before enabling the USB global interrupt, so a
    // Start failure cannot leave an interrupt-visible partial device. On
    // failure, tear down the partially configured device and return.
    if (USBD_Start(&USBD_Device) != USBD_OK)
    {
        NVIC_DisableIRQ(OTG_IRQ);
        (void)USBD_DeInit(&USBD_Device);
        return 1;
    }

    USB_OTG_BSP_EnableInterrupt();

    // NVIC configuration for SYSTick
    NVIC_DisableIRQ(SysTick_IRQn);
    NVIC_SetPriority(SysTick_IRQn, NVIC_BUILD_PRIORITY(0, 0));
    NVIC_EnableIRQ(SysTick_IRQn);

    return 0;
}

void mscTask(void)
{
    // Nothing to do here
}
#endif  /*USE_USB_MSC*/
