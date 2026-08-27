TARGET_MCU        := FT32F405
MCU_FLASH_SIZE    := 512
DEVICE_FLAGS       = -D$(TARGET_MCU) -DFT32F405xE
TARGET_MCU_FAMILY := FT32F4

# The canonical FT32F405 target uses the production profile in this directory.
# Keep its provenance tied to the exact profile that completed integration testing.
ifeq ($(CONFIG),)
TARGET_FLAGS += -DUSE_CONFIG
CONFIG_REVISION_DEFINE := -D'__CONFIG_REVISION__="92fd7ec"'
endif
