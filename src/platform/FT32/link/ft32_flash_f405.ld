/*
*****************************************************************************
**
**  File        : ft32_flash_f405.ld
**
**  Abstract    : Linker script for FT32F405 Device with
**                512KByte FLASH, 128KByte RAM, 64KByte CCM
**
**  Note        : FT32F405 512KB Flash variant
**                Flash: 1024 pages × 512 bytes = 512KB
**
*****************************************************************************
*/

/*
 * FT32F405 512KB Flash Layout
 * --------------------------------------------
 * 0x08000000 - 0x08003FFF : 16KB ISR vector, startup code (Pages 0-31)
 * 0x08004000 - 0x08007FFF : 16KB Config storage (Pages 32-63)
 * 0x08008000 - 0x0807FFFF : 480KB Firmware (Pages 64-1023)
 *
 * Total: 16 + 16 + 480 = 512KB
 */

/* Specify the memory areas */
MEMORY
{
    FLASH (rx)        : ORIGIN = 0x08000000, LENGTH = 16K
    FLASH_CONFIG (r)  : ORIGIN = 0x08004000, LENGTH = 16K
    FLASH1 (rx)       : ORIGIN = 0x08008000, LENGTH = 480K

    SYSTEM_MEMORY (rx): ORIGIN = 0x1FFF1000, LENGTH = 30K

    RAM (rwx)         : ORIGIN = 0x20000000, LENGTH = 128K
    CCM (rwx)         : ORIGIN = 0x10000000, LENGTH = 64K
    BACKUP_SRAM (rwx) : ORIGIN = 0x40024000, LENGTH = 4K
    MEMORY_B1 (rx)    : ORIGIN = 0x60000000, LENGTH = 0K
}

REGION_ALIAS("STACKRAM", RAM)
REGION_ALIAS("FASTRAM", RAM)
REGION_ALIAS("VECTAB", RAM)

/* Use FLASH for movable data to maximize FLASH1 for main code */
REGION_ALIAS("MOVABLE_FLASH", FLASH)

INCLUDE "../../STM32/link/stm32_flash_f4_split.ld"