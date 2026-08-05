#include "ft32f4xx.h"

extern OTG_FS_TypeDef ft32UsbEp0MockRegs;
uint8_t ft32UsbEp0MockFifoReadByte(uint32_t address);
void ft32UsbEp0MockFifoWriteByte(uint32_t address, uint8_t value);

#undef USB_FS
#define USB_FS (&ft32UsbEp0MockRegs)
#define USB_FS_FIFO_READ_BYTE(address) ft32UsbEp0MockFifoReadByte(address)
#define USB_FS_FIFO_WRITE_BYTE(address, value) ft32UsbEp0MockFifoWriteByte(address, value)

#include "../../../lib/main/FT32F4/Drivers/FT32F4xx_Driver/Src/ft32f4xx_usb_fs.c"
