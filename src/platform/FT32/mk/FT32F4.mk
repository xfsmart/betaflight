#
# FT32F4 Make file include
# FMD (Fremont Micro Devices) FT32F4xx series
#

# CMSIS
CMSIS_DIR       := $(LIB_MAIN_DIR)/FT32F4/Drivers/CMSIS

# FT32F4 FMD Standard Peripheral Driver
STDPERIPH_DIR   = $(LIB_MAIN_DIR)/FT32F4/Drivers/FT32F4xx_Driver
HAL_DRIVER_DIR  = $(LIB_MAIN_DIR)/FT32F4/Drivers/FT32F4xx_HAL_Driver
STDPERIPH_SRC   = \
        ft32f4xx_i2c.c \
        ft32f4xx_gpio.c \
        ft32f4xx_rcc.c \
        ft32f4xx_exti.c \
        ft32f4xx_syscfg.c \
        ft32f4xx_misc.c \
        ft32f4xx_spi.c \
        ft32f4xx_dma.c \
        ft32f4xx_adc.c \
        ft32f4xx_tim.c \
        ft32f4xx_usart.c \
        ft32f4xx_uart.c \
        ft32f4xx_i2s.c \
        ft32f4xx_rtc.c \
        ft32f4xx_pwr.c \
        ft32f4xx_sdio.c \
        ft32f4xx_iwdg.c \
        ft32f4xx_wwdg.c \
        ft32f4xx_dac.c \
        ft32f4xx_flash.c

VPATH   := $(VPATH):$(STDPERIPH_DIR)/Src

DEVICE_STDPERIPH_SRC := \
        $(STDPERIPH_SRC)

# FT32F4 I2C Application Library (middlewares)
I2C_APP_DIR   = $(LIB_MAIN_DIR)/FT32F4/middlewares/i2c_application_library
I2C_APP_SRC   = \
        i2c_application.c

VPATH   := $(VPATH):$(I2C_APP_DIR)

DEVICE_I2C_APP_SRC := \
        $(I2C_APP_SRC)

# USB Platform Source (参考 STM32 结构)
# 平台层 USB 文件位于 src/platform/FT32/
# vcpf4 目录: src/platform/FT32/vcpf4/ (对应 STM32 的 vcpf4/)
# include 目录: src/platform/FT32/include/platform/

# USB Driver (SDK 底层驱动)
# 使用官方 SDK 的 Drivers/FT32F4xx_Driver，不重复拷贝
USB_DRIVER_DIR := $(STDPERIPH_DIR)
USB_DRIVER_SRC = \
        $(STDPERIPH_DIR)/Src/ft32f4xx_pcd_fs.c \
        $(STDPERIPH_DIR)/Src/ft32f4xx_usb_fs.c

# USB Middleware - Core
USB_CORE_DIR := $(LIB_MAIN_DIR)/FT32F4/middlewares/usb_drivers
USB_CORE_SRC = \
        usbd_core.c \
        usbd_ctlreq.c \
        usbd_ioreq.c \
        usbd_conf.c \
        usbd_conf_fs.c

VPATH := $(VPATH):$(USB_CORE_DIR)/src

# USB Middleware - CDC Class
USB_CDC_DIR := $(LIB_MAIN_DIR)/FT32F4/middlewares/usbd_class/CDC
USB_CDC_SRC = \
        usbd_cdc.c

VPATH := $(VPATH):$(USB_CDC_DIR)/src

# USB Middleware - MSC Class
USB_MSC_DIR := $(LIB_MAIN_DIR)/FT32F4/middlewares/usbd_class/MSC
USB_MSC_SRC = \
        usbd_msc.c \
        usbd_msc_bot.c \
        usbd_msc_data.c \
        usbd_msc_scsi.c

VPATH := $(VPATH):$(USB_MSC_DIR)/src

# USB Middleware - HID Class (present but not yet ported - API incompatibilities with FT32 USB core)
# USB_HID_DIR := $(LIB_MAIN_DIR)/FT32F4/middlewares/usbd_class/HID
# USB_HID_SRC = \
#         usbd_hid.c
# VPATH := $(VPATH):$(USB_HID_DIR)/src

# Include paths - FT32 platform must be FIRST to ensure correct platform.h is found
# The := assignment places these before $(SRC_DIR), overriding the default order
INCLUDE_DIRS := \
        $(TARGET_PLATFORM_DIR)/include \
        $(TARGET_PLATFORM_DIR) \
        $(INCLUDE_DIRS) \
        $(TARGET_PLATFORM_DIR)/startup \
        $(TARGET_PLATFORM_DIR)/link \
        $(PLATFORM_DIR)/common/stm32 \
        $(TARGET_PLATFORM_DIR)/vcpf4 \
        $(HAL_DRIVER_DIR)/Inc \
        $(STDPERIPH_DIR)/Inc \
        $(I2C_APP_DIR) \
        $(LIB_MAIN_DIR)/FT32F4/CMSIS/cm4/device_support \
        $(LIB_MAIN_DIR)/FT32F4/Drivers/FT32F4xx_Driver/Inc \
        $(LIB_MAIN_DIR)/CMSIS/Core/Include \
        $(USB_DRIVER_DIR)/Inc \
        $(USB_CORE_DIR)/inc \
        $(USB_CDC_DIR)/inc \
        $(USB_MSC_DIR)/inc

# Architecture flags - Cortex-M4 with FPU
ARCH_FLAGS      = -mthumb -mcpu=cortex-m4 -march=armv7e-m -mfloat-abi=hard -mfpu=fpv4-sp-d16

DEVICE_FLAGS    = -DHSE_VALUE=$(HSE_VALUE) -DFT32F4 -Wno-unused-variable -Wno-unused-parameter

# USB device mode (FS core)
DEVICE_FLAGS    += -DUSB_OTG_FS_CORE -DUSB_OTG_FS -DPCD_FS_MODULE_ENABLED

ifeq ($(TARGET_MCU),FT32F405)
DEVICE_FLAGS    += -DFT32F405xE -DFT32F405
LD_SCRIPT       = $(LINKER_DIR)/ft32_flash_f405.ld
STARTUP_SRC     = FT32/startup/gcc/startup_ft32f405xx.s
MCU_FLASH_SIZE  := 512
# Inline limit for 512KB constrained target (same as STM32F411)
DEVICE_FLAGS    += -finline-limit=20

else ifeq ($(TARGET_MCU),FT32F407)
DEVICE_FLAGS    += -DFT32F407xx -DFT32F407
LD_SCRIPT       = $(LINKER_DIR)/ft32_flash_f407.ld
STARTUP_SRC     = FT32/startup/gcc/startup_ft32f407xx.s
MCU_FLASH_SIZE  := 512

else
$(error TARGET_MCU [$(TARGET_MCU)] is not supported)
endif

# FT32F4xx Standard Peripheral Library I2C Driver
FT32_I2C_STD_SRC = \
        ft32f4xx_i2c.c \
        ft32f4xx_gpio.c

VPATH := $(VPATH):$(STDPERIPH_DIR)/Src

MCU_COMMON_SRC = \
        common/stm32/system.c \
        common/stm32/io_impl.c \
        common/stm32/mco.c \
        $(LIB_MAIN_DIR)/FT32F4/CMSIS/cm4/device_support/system_ft32f4xx.c \
        FT32/system_ft32f4xx.c \
        FT32/rcc_ft32f4xx.c \
        FT32/dma_ft32f4xx.c \
        FT32/persistent_ft32.c \
        FT32/io_ft32.c \
        FT32/bus_i2c_ft32.c \
        FT32/bus_i2c_ft32_init.c \
        FT32/bus_spi_ft32f4xx.c \
        FT32/adc_ft32f4xx.c \
        FT32/exti_ft32f4xx.c \
        FT32/timer_ft32f4xx.c \
        FT32/timer_ft32_stdperiph.c \
        FT32/dma_reqmap_mcu.c \
        FT32/serial_uart_stdperiph.c \
        FT32/serial_uart_ft32f4xx.c \
        FT32/sdio_ft32f4xx.c \
        FT32/light_ws2811strip_ft32f4xx.c \
        FT32/pwm_output_hw_ft32f4xx.c \
        FT32/pwm_output_dshot_ft32f4xx.c \
        FT32/dshot_bitbang.c \
        FT32/dshot_bitbang_stdperiph.c \
        common/stm32/dshot_dpwm.c \
        common/stm32/pwm_output_dshot_shared.c \
        common/stm32/dshot_bitbang_shared.c \
        common/stm32/config_flash.c \
        common/stm32/debug_pin.c \
        FT32/debug.c \
        common/stm32/serial_uart_hw.c \
        common/stm32/serial_uart_pinconfig.c \
        common/stm32/adc_impl.c \
        common/stm32/ledstrip_ws2811_stm32.c \
        common/stm32/pwm_output_beeper.c \
        common/stm32/rx_pwm_hw.c \
        drivers/adc.c \
        drivers/bus_spi_config.c \
        drivers/serial_pinconfig.c \
        drivers/inverter.c \
        drivers/serial_escserial.c \
        common/stm32/bus_i2c_pinconfig.c \
        common/stm32/bus_spi_pinconfig.c \
        common/stm32/bus_spi_hw.c \
        drivers/bus_i2c_timing.c \
        drivers/dshot_bitbang_decode.c \
        $(DEVICE_STDPERIPH_SRC) \
        $(DEVICE_I2C_APP_SRC) \
        $(USB_DRIVER_SRC) \
        $(USB_CORE_SRC) \
        $(USB_CDC_SRC) \
        $(MSC_SRC)

# Size optimization for non-critical paths (same pattern as STM32F411)

# Speed optimization for critical paths (same pattern as STM32F411)
SPEED_OPTIMISED_SRC += \
        common/stm32/system.c \
        common/stm32/bus_spi_hw.c \
        FT32/pwm_output_hw_ft32f4xx.c \
        common/stm32/dshot_bitbang_shared.c \
        common/stm32/io_impl.c

# FT32 standard library has unused variable warnings in adc driver
$(OBJ_DIR)/$(STDPERIPH_DIR)/Src/ft32f4xx_adc.o: CFLAGS += -Wno-unused-variable
$(OBJ_DIR)/$(STDPERIPH_DIR)/Src/ft32f4xx_tim.o: CFLAGS += -Wno-unused-but-set-variable
$(TARGET_OBJ_DIR)/ft32f4xx_tim.o: CFLAGS += -Wno-unused-but-set-variable

DSP_LIB := $(LIB_MAIN_DIR)/CMSIS/DSP
DEVICE_FLAGS += -DARM_MATH_MATRIX_CHECK -DARM_MATH_ROUNDING -DUNALIGNED_SUPPORT_DISABLE -DARM_MATH_CM4

# USB VCP Source (参考 STM32 vcpf4 结构)
# Note: usbd_usr.c and usb_cdc_hid_ft32f4.c are incomplete ports (missing USBD_Usr_cb_TypeDef
# and USBD_HID_SendReport from FT32 USB middleware). Excluded until properly ported.
VCP_SRC = \
        FT32/vcpf4/usb_it_ft32f4.c \
        FT32/vcpf4/usb_bsp_ft32f4.c \
        FT32/vcpf4/usbd_desc.c \
        FT32/vcpf4/usbd_cdc_vcp.c \
        FT32/serial_usb_vcp.c \
        drivers/usb_io.c

# Size optimization for non-critical paths
SIZE_OPTIMISED_SRC += \
        drivers/bus_spi_config.c \
        drivers/serial_pinconfig.c \
        common/stm32/bus_i2c_pinconfig.c \
        common/stm32/config_flash.c \
        common/stm32/bus_spi_pinconfig.c \
        common/stm32/pwm_output_beeper.c \
        common/stm32/serial_uart_pinconfig.c

# USB MSC Source (参考 STM32 结构)
MSC_SRC = \
        $(USB_MSC_SRC) \
        drivers/usb_msc_common.c \
        FT32/usb_msc_ft32f4.c \
        FT32/usbd_msc_desc.c \
        FT32/usbd_msc_storage.c \
        msc/usbd_storage.c \
        msc/usbd_storage_emfat.c \
        msc/usbd_storage_sdio.c \
        msc/emfat.c \
        msc/emfat_file.c \
        common/stm32/msc_sdio_storage.c