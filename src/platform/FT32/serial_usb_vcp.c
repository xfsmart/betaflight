/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Betaflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>

#include "platform.h"

#ifdef USE_VCP

#include "build/build_config.h"
#include "build/atomic.h"

#include "common/utils.h"

#include "drivers/io.h"
#include "drivers/usb_io.h"

#include "pg/usb.h"

#include "usb_conf.h"
#include "usb_core.h"
#include "usbd_def.h"
#include "usbd_cdc_if.h"

/* External CDC functions declared in vcpf4/usbd_cdc_vcp.c */
extern uint32_t CDC_Send_DATA(const uint8_t *ptrBuffer, uint32_t sendLength);
extern uint32_t CDC_Send_FreeBytes(void);
extern uint32_t CDC_Receive_DATA(uint8_t* recvBuf, uint32_t len);
extern uint32_t CDC_Receive_BytesAvailable(void);
extern uint8_t usbIsConfigured(void);
extern uint8_t usbIsConnected(void);
extern uint32_t CDC_BaudRate(void);
extern void CDC_SetBaudRateCb(void (*cb)(void *context, uint32_t baud), void *context);
extern void CDC_SetCtrlLineStateCb(void (*cb)(void *context, uint16_t ctrlLineState), void *context);

extern USBD_HandleTypeDef USBD_Device;
extern USBD_DescriptorsTypeDef USBD_CDC_Desc;

#include "drivers/time.h"
#include "drivers/serial.h"
#include "drivers/serial_usb_vcp.h"
#include "drivers/nvic.h"

/* External BSP functions from vcpf4/usb_bsp_ft32f4.c */
extern void USB_OTG_BSP_Init(void);
extern void USB_OTG_BSP_EnableInterrupt(void);

#define USB_TIMEOUT  50

static vcpPort_t vcpPort = {0};

#define APP_RX_DATA_SIZE  2048
#define APP_TX_DATA_SIZE  2048

#define APP_TX_BLOCK_SIZE 512

/* Buffers are defined in vcpf4/usbd_cdc_vcp.c */
extern volatile uint8_t UserRxBuffer[APP_RX_DATA_SIZE];
extern volatile uint8_t UserTxBuffer[APP_TX_DATA_SIZE];
extern uint32_t BuffLength;
extern volatile uint32_t UserTxBufPtrIn;
extern volatile uint32_t UserTxBufPtrOut;

#define  CDC_POLLING_INTERVAL 5

static void usbVcpSetBaudRate(serialPort_t *instance, uint32_t baudRate)
{
    UNUSED(instance);
    UNUSED(baudRate);
}

static void usbVcpSetMode(serialPort_t *instance, portMode_e mode)
{
    UNUSED(instance);
    UNUSED(mode);
}

static void usbVcpSetCtrlLineStateCb(serialPort_t *instance, void (*cb)(void *context, uint16_t ctrlLineState), void *context)
{
    UNUSED(instance);
    // Register upper driver control line state callback routine with USB driver
    CDC_SetCtrlLineStateCb((void (*)(void *context, uint16_t ctrlLineState))cb, context);
}

static void usbVcpSetBaudRateCb(serialPort_t *instance, void (*cb)(serialPort_t *context, uint32_t baud), serialPort_t *context)
{
    UNUSED(instance);

    // Register upper driver baud rate callback routine with USB driver
    CDC_SetBaudRateCb((void (*)(void *context, uint32_t baud))cb, (void *)context);
}

static bool isUsbVcpTransmitBufferEmpty(const serialPort_t *instance)
{
    UNUSED(instance);
    return true;
}

static uint32_t usbVcpAvailable(const serialPort_t *instance)
{
    UNUSED(instance);

    return CDC_Receive_BytesAvailable();
}

static uint8_t usbVcpRead(serialPort_t *instance)
{
    UNUSED(instance);

    uint8_t buf[1];

    while (true) {
        if (CDC_Receive_DATA(buf, 1))
            return buf[0];
    }
}

static void usbVcpWriteBuf(serialPort_t *instance, const void *data, int count)
{
    UNUSED(instance);

    if (!(usbIsConnected() && usbIsConfigured())) {
        return;
    }

    uint32_t start = millis();
    const uint8_t *p = data;
    while (count > 0) {
        uint32_t txed = CDC_Send_DATA(p, count);
        count -= txed;
        p += txed;

        if (millis() - start > USB_TIMEOUT) {
            break;
        }
    }
}

static bool usbVcpFlush(vcpPort_t *port)
{
    uint32_t count = port->txAt;
    port->txAt = 0;

    if (count == 0) {
        return true;
    }

    if (!usbIsConnected() || !usbIsConfigured()) {
        return false;
    }

    uint32_t start = millis();
    uint8_t *p = port->txBuf;
    while (count > 0) {
        uint32_t txed = CDC_Send_DATA(p, count);
        count -= txed;
        p += txed;

        if (millis() - start > USB_TIMEOUT) {
            break;
        }
    }
    return count == 0;
}

static void usbVcpWrite(serialPort_t *instance, uint8_t c)
{
    vcpPort_t *port = container_of(instance, vcpPort_t, port);

    port->txBuf[port->txAt++] = c;
    if (!port->buffering || port->txAt >= ARRAYLEN(port->txBuf)) {
        usbVcpFlush(port);
    }
}

static void usbVcpBeginWrite(serialPort_t *instance)
{
    vcpPort_t *port = container_of(instance, vcpPort_t, port);
    port->buffering = true;
}

static uint32_t usbTxBytesFree(const serialPort_t *instance)
{
    UNUSED(instance);
    return CDC_Send_FreeBytes();
}

static void usbVcpEndWrite(serialPort_t *instance)
{
    vcpPort_t *port = container_of(instance, vcpPort_t, port);
    port->buffering = false;
    usbVcpFlush(port);
}

static const struct serialPortVTable usbVTable[] = {
    {
        .serialWrite = usbVcpWrite,
        .serialTotalRxWaiting = usbVcpAvailable,
        .serialTotalTxFree = usbTxBytesFree,
        .serialRead = usbVcpRead,
        .serialSetBaudRate = usbVcpSetBaudRate,
        .isSerialTransmitBufferEmpty = isUsbVcpTransmitBufferEmpty,
        .setMode = usbVcpSetMode,
        .setCtrlLineStateCb = usbVcpSetCtrlLineStateCb,
        .setBaudRateCb = usbVcpSetBaudRateCb,
        .writeBuf = usbVcpWriteBuf,
        .beginWrite = usbVcpBeginWrite,
        .endWrite = usbVcpEndWrite
    }
};

void usbVcpInit(void)
{
    IOInit(IOGetByTag(IO_TAG(PA11)), OWNER_USB, 0);
    IOInit(IOGetByTag(IO_TAG(PA12)), OWNER_USB, 0);

#ifdef USB_LOW_POWER_WAKUP
    usb_low_power_wakeup_config();
#endif

    usbGenerateDisconnectPulse();

    // Initialize USB BSP (clocks, GPIO)
    USB_OTG_BSP_Init();

    // Enable USB global interrupt
    USB_OTG_BSP_EnableInterrupt();

    USBD_Init(&USBD_Device, USB_OTG_FS_CORE_ID, 0, &USBD_CDC, &USBD_CDC_Desc);
}

serialPort_t *usbVcpOpen(void)
{
    vcpPort_t *s = &vcpPort;
    s->port.vTable = usbVTable;
    return &s->port;
}

uint32_t usbVcpGetBaudRate(serialPort_t *instance)
{
    UNUSED(instance);

    return CDC_BaudRate();
}

uint8_t usbVcpIsConnected(void)
{
    return usbIsConnected();
}
#endif
