#include "ft32f4xx.h"

extern OTG_FS_TypeDef ft32UsbEp0MockRegs;

#undef USB_FS
#define USB_FS (&ft32UsbEp0MockRegs)

#include "../../../lib/main/FT32F4/Drivers/FT32F4xx_Driver/Src/ft32f4xx_pcd_fs.c"
