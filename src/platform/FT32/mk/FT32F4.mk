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
        ft32f4xx_tim.c

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

# Include paths - FT32 platform must be first to ensure correct platform.h is found
INCLUDE_DIRS += \
        $(TARGET_PLATFORM_DIR)/include \
        $(TARGET_PLATFORM_DIR) \
        $(TARGET_PLATFORM_DIR)/startup \
        $(TARGET_PLATFORM_DIR)/link \
        $(PLATFORM_DIR)/common/stm32 \
        $(HAL_DRIVER_DIR)/Inc \
        $(STDPERIPH_DIR)/Inc \
        $(I2C_APP_DIR) \
        $(LIB_MAIN_DIR)/FT32F4/CMSIS/cm4/device_support \
        $(LIB_MAIN_DIR)/FT32F4/Drivers/FT32F4xx_Driver/Inc \
        $(LIB_MAIN_DIR)/CMSIS/Core/Include

# Architecture flags - Cortex-M4 with FPU
ARCH_FLAGS      = -mthumb -mcpu=cortex-m4 -march=armv7e-m -mfloat-abi=hard -mfpu=fpv4-sp-d16

DEVICE_FLAGS    = -DHSE_VALUE=$(HSE_VALUE) -DFT32F4 -Wno-unused-variable

ifeq ($(TARGET_MCU),FT32F405)
DEVICE_FLAGS    += -DFT32F405xE -DFT32F405
LD_SCRIPT       = $(LINKER_DIR)/ft32_flash_f405.ld
STARTUP_SRC     = FT32/startup/gcc/startup_ft32f405xx.s
MCU_FLASH_SIZE  := 1024

else ifeq ($(TARGET_MCU),FT32F407)
DEVICE_FLAGS    += -DFT32F407xx -DFT32F407
LD_SCRIPT       = $(LINKER_DIR)/ft32_flash_f407.ld
STARTUP_SRC     = FT32/startup/gcc/startup_ft32f407xx.s
MCU_FLASH_SIZE  := 1024

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
        FT32/drivers_stubs.c \
        FT32/io_ft32.c \
        FT32/bus_i2c_ft32.c \
        FT32/bus_i2c_ft32_init.c \
        FT32/bus_spi_ft32f4xx.c \
        FT32/adc_ft32f4xx.c \
        FT32/exti_ft32f4xx.c \
        FT32/timer_ft32f4xx.c \
        FT32/timer_ft32_stdperiph.c \
        FT32/dma_reqmap_mcu.c \
        FT32/stubs_ft32f4xx.c \
        common/stm32/adc_impl.c \
        drivers/adc.c \
        common/stm32/bus_i2c_pinconfig.c \
        common/stm32/bus_spi_pinconfig.c \
        common/stm32/bus_spi_hw.c \
        drivers/bus_i2c_timing.c \
        $(DEVICE_STDPERIPH_SRC) \
        $(DEVICE_I2C_APP_SRC)

SIZE_OPTIMISED_SRC += \
        common/stm32/bus_i2c_pinconfig.c

# FT32 standard library has unused variable warnings in adc driver
$(OBJ_DIR)/$(STDPERIPH_DIR)/Src/ft32f4xx_adc.o: CFLAGS += -Wno-unused-variable
$(OBJ_DIR)/$(STDPERIPH_DIR)/Src/ft32f4xx_tim.o: CFLAGS += -Wno-unused-but-set-variable
$(TARGET_OBJ_DIR)/ft32f4xx_tim.o: CFLAGS += -Wno-unused-but-set-variable

DSP_LIB := $(LIB_MAIN_DIR)/CMSIS/DSP
DEVICE_FLAGS += -DARM_MATH_MATRIX_CHECK -DARM_MATH_ROUNDING -DUNALIGNED_SUPPORT_DISABLE -DARM_MATH_CM4
