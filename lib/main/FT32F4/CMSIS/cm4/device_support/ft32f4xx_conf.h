/**
  **************************************************************************
  * @file     ft32f4xx_conf.h
  * @brief    FT32F4xx config header file
  **************************************************************************
  *                       Copyright notice & Disclaimer
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Fremont Micro Devices official website is the copyrighted
  * work of FMD. FMD authorizes customers to use, copy, and distribute the
  * BSP software and its related documentation for the purpose of design and
  * development in conjunction with FMD microcontrollers. Use of the software
  * is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. FMD EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FT32F4xx_CONF_H
#define __FT32F4xx_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################### Module Selection ############################## */
/**
  * @brief This is the list of modules to be used in the driver
  * Note: Only enable modules that have been implemented. Disable others to avoid
  *       "file not found" errors in IDE and compilation.
  */
#define GPIO_MODULE_ENABLED      /* Implemented */
#define I2C_MODULE_ENABLED       /* Implemented */
#define RCC_MODULE_ENABLED       /* TODO: Implement when needed */
#define SYSCFG_MODULE_ENABLED    /* EXTI requires SYSCFG */
/* #define CORTEX_MODULE_ENABLED */
#define EXTI_MODULE_ENABLED
#define DMA_MODULE_ENABLED
#define FLASH_MODULE_ENABLED
/* #define CRC_MODULE_ENABLED */
/* #define WDT_MODULE_ENABLED */
/* #define WWDT_MODULE_ENABLED */
#define PWC_MODULE_ENABLED
#define RTC_MODULE_ENABLED
#define ADC_MODULE_ENABLED
/* #define CAN_MODULE_ENABLED */
#define USART_MODULE_ENABLED
#define UART_MODULE_ENABLED
#define SPI_MODULE_ENABLED
/* #define TMR_MODULE_ENABLED */
/* #define SDIO_MODULE_ENABLED */
/* #define USB_MODULE_ENABLED */
/* #define FSMC_MODULE_ENABLED */
/* #define DEBUG_MODULE_ENABLED */

/* ########################## HSE/HSI Values adaptation ##################### */
/**
  * @brief Adjust the value of External High Speed oscillator (HSE) used in your application.
  *        This value is used by the RCC driver to compute the system frequency
  */
#if !defined(HSE_VALUE)
  #define HSE_VALUE    ((uint32_t)8000000U)  /*!< Value of the External oscillator in Hz */
#endif /* HSE_VALUE */

#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    ((uint32_t)100U)  /*!< Time out for HSE start up, in ms */
#endif /* HSE_STARTUP_TIMEOUT */

/**
  * @brief Internal High Speed oscillator (HSI) value.
  *        This value is used by the RCC driver to compute the system frequency
  */
#if !defined(HSI_VALUE)
  #define HSI_VALUE    ((uint32_t)8000000U)  /*!< Value of the Internal oscillator in Hz*/
#endif /* HSI_VALUE */

/**
  * @brief Internal Low Speed oscillator (LSI) value.
  */
#if !defined(LSI_VALUE)
  #define LSI_VALUE  ((uint32_t)40000U)      /*!< LSI Typical Value in Hz*/
#endif /* LSI_VALUE */

/**
  * @brief External Low Speed oscillator (LSE) value.
  *        This value is used by the UART, RTC driver to compute the system frequency
  */
#if !defined(LSE_VALUE)
  #define LSE_VALUE    ((uint32_t)32768U)    /*!< Value of the External Low Speed oscillator in Hz */
#endif /* LSE_VALUE */

#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT    ((uint32_t)5000U)  /*!< Time out for LSE start up, in ms */
#endif /* LSE_STARTUP_TIMEOUT */

/* ########################### System Configuration ######################### */
/**
  * @brief This is the driver system configuration section
  */
#define  VDD_VALUE                    ((uint32_t)3300U)  /*!< Value of VDD in mv */
#define  TICK_INT_PRIORITY            ((uint32_t)0U)     /*!< tick interrupt priority */
#define  USE_RTOS                     0U
#define  USE_BSP_DRIVERS              0U

/* ########################## Assert Selection ############################## */
/**
  * @brief Uncomment the line below to expanse the "assert_param" macro in the
  *        driver code
  */
/* #define USE_FULL_ASSERT    1U */

/* Includes ------------------------------------------------------------------*/
/**
  * @brief Include module's header file
  */

#ifdef GPIO_MODULE_ENABLED
  #include "ft32f4xx_gpio.h"
#endif /* GPIO_MODULE_ENABLED */

#ifdef I2C_MODULE_ENABLED
  #include "ft32f4xx_i2c.h"
#endif /* I2C_MODULE_ENABLED */

#ifdef RCC_MODULE_ENABLED
  #include "ft32f4xx_rcc.h"
#endif /* RCC_MODULE_ENABLED */

#ifdef CORTEX_MODULE_ENABLED
  #include "ft32f4xx_cortex.h"
#endif /* CORTEX_MODULE_ENABLED */

#ifdef EXTI_MODULE_ENABLED
  #include "ft32f4xx_exti.h"
#endif /* EXTI_MODULE_ENABLED */

#ifdef SYSCFG_MODULE_ENABLED
  #include "ft32f4xx_syscfg.h"
#endif /* SYSCFG_MODULE_ENABLED */

#ifdef DMA_MODULE_ENABLED
  #include "ft32f4xx_dma.h"
#endif /* DMA_MODULE_ENABLED */

#ifdef FLASH_MODULE_ENABLED
  #include "ft32f4xx_flash.h"
#endif /* FLASH_MODULE_ENABLED */

#ifdef CRC_MODULE_ENABLED
  #include "ft32f4xx_crc.h"
#endif /* CRC_MODULE_ENABLED */

#ifdef WDT_MODULE_ENABLED
  #include "ft32f4xx_wdt.h"
#endif /* WDT_MODULE_ENABLED */

#ifdef WWDT_MODULE_ENABLED
  #include "ft32f4xx_wwdt.h"
#endif /* WWDT_MODULE_ENABLED */

#ifdef PWC_MODULE_ENABLED
  #include "ft32f4xx_pwr.h"
#endif /* PWC_MODULE_ENABLED */

#ifdef RTC_MODULE_ENABLED
  #include "ft32f4xx_rtc.h"
#endif /* RTC_MODULE_ENABLED */

#ifdef ADC_MODULE_ENABLED
  #include "ft32f4xx_adc.h"
#endif /* ADC_MODULE_ENABLED */

#ifdef CAN_MODULE_ENABLED
  #include "ft32f4xx_can.h"
#endif /* CAN_MODULE_ENABLED */

#ifdef USART_MODULE_ENABLED
  #include "ft32f4xx_usart.h"
#endif /* USART_MODULE_ENABLED */

#ifdef UART_MODULE_ENABLED
  #include "ft32f4xx_uart.h"
#endif /* UART_MODULE_ENABLED */

#ifdef SPI_MODULE_ENABLED
  #include "ft32f4xx_spi.h"
#endif /* SPI_MODULE_ENABLED */

#ifdef TMR_MODULE_ENABLED
  #include "ft32f4xx_tmr.h"
#endif /* TMR_MODULE_ENABLED */

#ifdef SDIO_MODULE_ENABLED
  #include "ft32f4xx_sdio.h"
#endif /* SDIO_MODULE_ENABLED */

#ifdef USB_MODULE_ENABLED
  #include "ft32f4xx_usb.h"
#endif /* USB_MODULE_ENABLED */

#ifdef FSMC_MODULE_ENABLED
  #include "ft32f4xx_fsmc.h"
#endif /* FSMC_MODULE_ENABLED */

#ifdef DEBUG_MODULE_ENABLED
  #include "ft32f4xx_debug.h"
#endif /* DEBUG_MODULE_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* __FT32F4xx_CONF_H */

/*****************************END OF FILE****/
