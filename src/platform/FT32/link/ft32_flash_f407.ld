/*
*****************************************************************************
**
**  File        : ft32_flash_f407.ld
**
**  Abstract    : Linker script for FT32F407 Device
**
**  Reference   : FT32F405_407xx_DS_V1.02_cn.pdf (Official Datasheet)
**                FT32F405_407xx_RM_V1.00_cn.pdf (Official Reference Manual)
**  Location    : /mnt/d/doc/FMD/
**
**  Memory (per official datasheet):
**                - 512KByte FLASH
**                - 192KByte SRAM (including 64KB CCM)
**                - 4KByte Backup SRAM
**                - 1024 Byte OTP
**
*****************************************************************************
*/

/*
 * FT32F407 Memory Map (per official datasheet)
 * --------------------------------------------
 * 0x08000000 - 0x0807FFFF : FLASH (512KB)
 * 0x20000000 - 0x2002FFFF : SRAM (192KB total, including 64KB CCM)
 * 0x10000000 - 0x1000FFFF : CCM (64KB, part of 192KB SRAM)
 * 0x40024000 - 0x40024FFF : Backup SRAM (4KB)
 *
 * Flash Layout:
 * 0x08000000 - 0x08003FFF : 16K ISR vector, startup code
 * 0x08004000 - 0x08007FFF : 16K Config storage (FLASH_Sector_1)
 * 0x08008000 - 0x0807FFFF : 448K Firmware
 */

/* Specify the memory areas */
MEMORY
{
    FLASH (rx)        : ORIGIN = 0x08000000, LENGTH = 16K
    FLASH_CONFIG (r)  : ORIGIN = 0x08004000, LENGTH = 16K
    FLASH1 (rx)       : ORIGIN = 0x08008000, LENGTH = 448K
    SYSTEM_MEMORY (rx): ORIGIN = 0x1FFF0000, LENGTH = 29K
    RAM (rwx)         : ORIGIN = 0x20000000, LENGTH = 128K
    CCM (rwx)         : ORIGIN = 0x10000000, LENGTH = 64K
    BACKUP_SRAM (rwx) : ORIGIN = 0x40024000, LENGTH = 4K
    MEMORY_B1 (rx)    : ORIGIN = 0x60000000, LENGTH = 0K
}

REGION_ALIAS("STACKRAM", CCM)
REGION_ALIAS("FASTRAM", CCM)
REGION_ALIAS("VECTAB", RAM)

/* Put various bits and bobs of data into the main chunk of flash as we have enough of it */

REGION_ALIAS("MOVABLE_FLASH", FLASH1)

INCLUDE "../../STM32/link/stm32_flash_f4_split.ld"
