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
#include "usbd_msc_desc.h"
#include "drivers/usb_io.h"

extern USBD_HandleTypeDef       USBD_Device;

#ifdef USB_OTG_FS_CORE
    extern PCD_FS_HandleTypeDef hpcd;
#endif

static void msc_usb_clock48m_select(uint32_t clk48_sel)
{
    if(clk48_sel == RCC_48MCLK_HSI48)
    {
        RCC_HSI48Cmd(ENABLE);
        RCC_WaitForHSI48StartUp();
        RCC_48MCLKConfig(RCC_48MCLK_HSI48);
    }
    else if(clk48_sel == RCC_48MCLK_PLLQ)
    {
        /* system_init ensures that the pllq_clock has been properly configured."*/
    }
}


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
#ifdef USB_OTG_HS_CORE
    RCC_HSEConfig(RCC_HSE_ON); /*ensure HSE12M on board*/
    RCC_WaitForHSEStartUp();
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_USBOTGHS, ENABLE);
#elif USB_OTG_FS_CORE
    msc_usb_clock48m_select(RCC_48MCLK_HSI48); /* HSI48 as USB_OTG_FS clock */
    RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_USBOTGFS, ENABLE);
#else
/* */
#endif
    NVIC_EnableIRQ(OTG_IRQ);
    USBD_Init(&USBD_Device, USB_OTG_FS_CORE_ID, OTG_USB_ID, &USBD_MSC, &USBD_MSC_Desc);
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
