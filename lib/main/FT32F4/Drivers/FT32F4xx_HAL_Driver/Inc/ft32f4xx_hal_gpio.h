/**
  ******************************************************************************
  * @file    ft32f4xx_gpio.h
  * @author  FT Application Team
  * @brief   Header file of GPIO module.
  *          This file provides all the GPIO firmware functions.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 Fremont Micro Devices.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by FT under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef FT32F4xx_GPIO_H
#define FT32F4xx_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ft32f4xx_def.h"

/** @addtogroup FT32F4xx_Driver
  * @{
  */

/** @addtogroup GPIO
  * @{
  */

/* Exported types ------------------------------------------------------------*/
/** @defgroup GPIO_Exported_Types GPIO Exported Types
  * @{
  */

/**
  * @brief GPIO Init structure definition
  */
typedef struct
{
  uint32_t Pin;       /*!< Specifies the GPIO pins to be configured.
                           This parameter can be any value of @ref GPIO_pins_define */

  uint32_t Mode;      /*!< Specifies the operating mode for the selected pins.
                           This parameter can be a value of @ref GPIO_mode_define */

  uint32_t Pull;      /*!< Specifies the Pull-up or Pull-Down activation for the selected pins.
                           This parameter can be a value of @ref GPIO_pull_define */

  uint32_t Speed;     /*!< Specifies the speed for the selected pins.
                           This parameter can be a value of @ref GPIO_speed_define */

  uint32_t Alternate;  /*!< Peripheral to be connected to the selected pins.
                            This parameter can be a value of @ref GPIO_Alternate_function_selection */
} GPIO_InitTypeDef;

/**
  * @brief  GPIO Bit SET and Bit RESET enumeration
  */
typedef enum
{
  GPIO_PIN_RESET = 0,
  GPIO_PIN_SET
} GPIO_PinState;
/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup GPIO_Exported_Constants GPIO Exported Constants
  * @{
  */

/** @defgroup GPIO_pins_define GPIO pins define
  * @{
  */
#define GPIO_PIN_0                 ((uint16_t)0x0001)  /* Pin 0 selected    */
#define GPIO_PIN_1                 ((uint16_t)0x0002)  /* Pin 1 selected    */
#define GPIO_PIN_2                 ((uint16_t)0x0004)  /* Pin 2 selected    */
#define GPIO_PIN_3                 ((uint16_t)0x0008)  /* Pin 3 selected    */
#define GPIO_PIN_4                 ((uint16_t)0x0010)  /* Pin 4 selected    */
#define GPIO_PIN_5                 ((uint16_t)0x0020)  /* Pin 5 selected    */
#define GPIO_PIN_6                 ((uint16_t)0x0040)  /* Pin 6 selected    */
#define GPIO_PIN_7                 ((uint16_t)0x0080)  /* Pin 7 selected    */
#define GPIO_PIN_8                 ((uint16_t)0x0100)  /* Pin 8 selected    */
#define GPIO_PIN_9                 ((uint16_t)0x0200)  /* Pin 9 selected    */
#define GPIO_PIN_10                ((uint16_t)0x0400)  /* Pin 10 selected   */
#define GPIO_PIN_11                ((uint16_t)0x0800)  /* Pin 11 selected   */
#define GPIO_PIN_12                ((uint16_t)0x1000)  /* Pin 12 selected   */
#define GPIO_PIN_13                ((uint16_t)0x2000)  /* Pin 13 selected   */
#define GPIO_PIN_14                ((uint16_t)0x4000)  /* Pin 14 selected   */
#define GPIO_PIN_15                ((uint16_t)0x8000)  /* Pin 15 selected   */
#define GPIO_PIN_All               ((uint16_t)0xFFFF)  /* All pins selected */

#define GPIO_PIN_MASK              0x0000FFFFU  /* PIN mask for assert test */
/**
  * @}
  */

/** @defgroup GPIO_mode_define GPIO mode define
  * @brief GPIO Configuration Mode
  *        Elements values convention: 0xX0yz00YZ
  *           - X  : GPIO mode or EXTI Mode
  *           - y  : External IT or Event trigger detection
  *           - z  : IO configuration on External IT or Event
  *           - Y  : Output type (Push Pull or Open Drain)
  *           - Z  : IO Direction mode (Input, Output, Alternate or Analog)
  * @{
  */
#define  GPIO_MODE_INPUT                        0x00000000U   /*!< Input Floating Mode                   */
#define  GPIO_MODE_OUTPUT_PP                    0x00000001U   /*!< Output Push Pull Mode                 */
#define  GPIO_MODE_OUTPUT_OD                    0x00000011U   /*!< Output Open Drain Mode                */
#define  GPIO_MODE_AF_PP                        0x00000002U   /*!< Alternate Function Push Pull Mode     */
#define  GPIO_MODE_AF_OD                        0x00000012U   /*!< Alternate Function Open Drain Mode    */
#define  GPIO_MODE_ANALOG                       0x00000003U   /*!< Analog Mode  */

#define  GPIO_MODE_IT_RISING                    0x10110000U   /*!< External Interrupt Mode with Rising edge trigger detection          */
#define  GPIO_MODE_IT_FALLING                   0x10210000U   /*!< External Interrupt Mode with Falling edge trigger detection         */
#define  GPIO_MODE_IT_RISING_FALLING            0x10310000U   /*!< External Interrupt Mode with Rising/Falling edge trigger detection  */

#define  GPIO_MODE_EVT_RISING                   0x10120000U   /*!< External Event Mode with Rising edge trigger detection               */
#define  GPIO_MODE_EVT_FALLING                  0x10220000U   /*!< External Event Mode with Falling edge trigger detection              */
#define  GPIO_MODE_EVT_RISING_FALLING           0x10320000U   /*!< External Event Mode with Rising/Falling edge trigger detection       */
/**
  * @}
  */

/** @defgroup GPIO_speed_define  GPIO speed define
  * @brief GPIO Output Maximum frequency
  * @{
  */
#define  GPIO_SPEED_FREQ_LOW         0x00000000U  /*!< IO works at 2 MHz, please refer to the product datasheet */
#define  GPIO_SPEED_FREQ_MEDIUM      0x00000001U  /*!< range 12,5 MHz to 50 MHz, please refer to the product datasheet */
#define  GPIO_SPEED_FREQ_HIGH        0x00000002U  /*!< range 25 MHz to 100 MHz, please refer to the product datasheet  */
#define  GPIO_SPEED_FREQ_VERY_HIGH   0x00000003U  /*!< range 50 MHz to 200 MHz, please refer to the product datasheet  */
/**
  * @}
  */

/** @defgroup GPIO_pull_define GPIO pull define
  * @brief GPIO Pull-Up or Pull-Down Activation
  * @{
  */
#define  GPIO_NOPULL        0x00000000U   /*!< No Pull-up or Pull-down activation  */
#define  GPIO_PULLUP        0x00000001U   /*!< Pull-up activation                  */
#define  GPIO_PULLDOWN      0x00000002U   /*!< Pull-down activation                */
/**
  * @}
  */

/**
  * @}
  */

/* Compatibility macros for StdPeriph-style code (Betaflight compatibility) */
/** @addtogroup GPIO_Compatibility_Macros GPIO Compatibility Macros
  * @brief Compatibility macros for code written in StdPeriph style
  * @{
  */
#define GPIO_Mode_IN          GPIO_MODE_INPUT           /*!< Input Mode */
#define GPIO_Mode_OUT         GPIO_MODE_OUTPUT_PP       /*!< Output Mode (Push-Pull) */
#define GPIO_Mode_AF          GPIO_MODE_AF_PP           /*!< Alternate Function Mode */
#define GPIO_Mode_AN          GPIO_MODE_ANALOG          /*!< Analog Mode */

#define GPIO_Speed_2MHz       GPIO_SPEED_FREQ_LOW       /*!< Low Speed */
#define GPIO_Speed_25MHz      GPIO_SPEED_FREQ_MEDIUM    /*!< Medium Speed */
#define GPIO_Speed_50MHz      GPIO_SPEED_FREQ_HIGH      /*!< High Speed */
#define GPIO_Speed_100MHz     GPIO_SPEED_FREQ_VERY_HIGH /*!< Very High Speed */

#define GPIO_OType_PP         (0U << 4)                 /*!< Push Pull Output Type */
#define GPIO_OType_OD         (1U << 4)                 /*!< Open Drain Output Type */

#define GPIO_PuPd_NOPULL      GPIO_NOPULL               /*!< No Pull-up or Pull-down */
#define GPIO_PuPd_UP          GPIO_PULLUP               /*!< Pull-up */
#define GPIO_PuPd_DOWN        GPIO_PULLDOWN             /*!< Pull-down */

/* Alternate Function definitions for I2C and other peripherals */
#define GPIO_AF_I2C1          4U   /*!< I2C1 Alternate Function mapping */
#define GPIO_AF_I2C2          4U   /*!< I2C2 Alternate Function mapping */
#define GPIO_AF_I2C3          4U   /*!< I2C3 Alternate Function mapping */
#define GPIO_AF4_I2C1         4U   /*!< I2C1 Alternate Function mapping (alternate name) */
#define GPIO_AF4_I2C2         4U   /*!< I2C2 Alternate Function mapping (alternate name) */
#define GPIO_AF4_I2C3         4U   /*!< I2C3 Alternate Function mapping (alternate name) */
#define GPIO_AF9_I2C2         9U   /*!< I2C2 Alternate Function mapping (AF9) */
#define GPIO_AF9_I2C3         9U   /*!< I2C3 Alternate Function mapping (AF9) */

#define GPIO_AF_SPI1          5U   /*!< SPI1 Alternate Function mapping */
#define GPIO_AF_SPI2          5U   /*!< SPI2 Alternate Function mapping */
#define GPIO_AF_SPI3          6U   /*!< SPI3 Alternate Function mapping */

#define GPIO_AF_USART1        7U   /*!< USART1 Alternate Function mapping */
#define GPIO_AF_USART2        7U   /*!< USART2 Alternate Function mapping */
#define GPIO_AF_USART3        7U   /*!< USART3 Alternate Function mapping */

#define GPIO_AF_TIM1          1U   /*!< TIM1 Alternate Function mapping */
#define GPIO_AF_TIM2          1U   /*!< TIM2 Alternate Function mapping */
#define GPIO_AF_TIM3          2U   /*!< TIM3 Alternate Function mapping */
#define GPIO_AF_TIM4          2U   /*!< TIM4 Alternate Function mapping */

#define GPIO_AF5_SPI1         5U   /*!< SPI1 Alternate Function mapping (AF5) */
#define GPIO_AF5_SPI2         5U   /*!< SPI2 Alternate Function mapping (AF5) */
#define GPIO_AF6_SPI3         6U   /*!< SPI3 Alternate Function mapping (AF6) */

#define GPIO_AF7_USART1       7U   /*!< USART1 Alternate Function mapping (AF7) */
#define GPIO_AF7_USART2       7U   /*!< USART2 Alternate Function mapping (AF7) */
#define GPIO_AF7_USART3       7U   /*!< USART3 Alternate Function mapping (AF7) */

/**
  * @}
  */

/* Exported macro ------------------------------------------------------------*/
/** @defgroup GPIO_Exported_Macros GPIO Exported Macros
  * @{
  */

/**
  * @brief  Checks whether the specified EXTI line flag is set or not.
  * @param  __EXTI_LINE__: specifies the EXTI line flag to check.
  *         This parameter can be GPIO_PIN_x where x can be(0..15)
  * @retval The new state of __EXTI_LINE__ (SET or RESET).
  */
#define __GPIO_EXTI_GET_FLAG(__EXTI_LINE__) (EXTI->PR & (__EXTI_LINE__))

/**
  * @brief  Clears the EXTI's line pending flags.
  * @param  __EXTI_LINE__: specifies the EXTI lines flags to clear.
  *         This parameter can be any combination of GPIO_PIN_x where x can be (0..15)
  * @retval None
  */
#define __GPIO_EXTI_CLEAR_FLAG(__EXTI_LINE__) (EXTI->PR = (__EXTI_LINE__))

/**
  * @brief  Checks whether the specified EXTI line is asserted or not.
  * @param  __EXTI_LINE__: specifies the EXTI line to check.
  *          This parameter can be GPIO_PIN_x where x can be(0..15)
  * @retval The new state of __EXTI_LINE__ (SET or RESET).
  */
#define __GPIO_EXTI_GET_IT(__EXTI_LINE__) (EXTI->PR & (__EXTI_LINE__))

/**
  * @brief  Clears the EXTI's line pending bits.
  * @param  __EXTI_LINE__: specifies the EXTI lines to clear.
  *          This parameter can be any combination of GPIO_PIN_x where x can be (0..15)
  * @retval None
  */
#define __GPIO_EXTI_CLEAR_IT(__EXTI_LINE__) (EXTI->PR = (__EXTI_LINE__))

/**
  * @brief  Generates a Software interrupt on selected EXTI line.
  * @param  __EXTI_LINE__: specifies the EXTI line to check.
  *          This parameter can be GPIO_PIN_x where x can be(0..15)
  * @retval None
  */
#define __GPIO_EXTI_GENERATE_SWIT(__EXTI_LINE__) (EXTI->SWIER |= (__EXTI_LINE__))
/**
  * @}
  */

/* Exported functions --------------------------------------------------------*/
/** @addtogroup GPIO_Exported_Functions
  * @{
  */

/** @addtogroup GPIO_Exported_Functions_Group1
  * @{
  */
/* Initialization and de-initialization functions *****************************/
void  GPIO_Init(GPIO_TypeDef  *GPIOx, GPIO_InitTypeDef *GPIO_Init);
void  GPIO_DeInit(GPIO_TypeDef  *GPIOx, uint32_t GPIO_Pin);
/**
  * @}
  */

/** @addtogroup GPIO_Exported_Functions_Group2
  * @{
  */
/* IO operation functions *****************************************************/
GPIO_PinState GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
void GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
FT_StatusTypeDef GPIO_LockPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void GPIO_EXTI_IRQHandler(uint16_t GPIO_Pin);
void GPIO_EXTI_Callback(uint16_t GPIO_Pin);

/**
  * @}
  */

/**
  * @}
  */

/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/** @defgroup GPIO_Private_Constants GPIO Private Constants
  * @{
  */
#define GPIO_MODE             0x00000003U
#define EXTI_MODE             0x10000000U
#define GPIO_MODE_IT          0x00010000U
#define GPIO_MODE_EVT         0x00020000U
#define RISING_EDGE           0x00100000U
#define FALLING_EDGE          0x00200000U
#define GPIO_OUTPUT_TYPE      0x00000010U

#define GPIO_NUMBER           16U
/**
  * @}
  */

/* Private macros ------------------------------------------------------------*/
/** @defgroup GPIO_Private_Macros GPIO Private Macros
  * @{
  */
#define GPIO_GET_INDEX(__GPIOx__)    (((__GPIOx__) == (GPIOA))? 0U :\
                                      ((__GPIOx__) == (GPIOB))? 1U :\
                                      ((__GPIOx__) == (GPIOC))? 2U :\
                                      ((__GPIOx__) == (GPIOD))? 3U :\
                                      ((__GPIOx__) == (GPIOE))? 4U :\
                                      ((__GPIOx__) == (GPIOH))? 5U : 6U)

#define IS_GPIO_PIN_ACTION(ACTION)  (((ACTION) == GPIO_PIN_RESET) || ((ACTION) == GPIO_PIN_SET))
#define IS_GPIO_PIN(PIN)            ((((PIN) & GPIO_PIN_MASK) != 0x00U) && (((PIN) & ~GPIO_PIN_MASK) == 0x00U))
#define IS_GPIO_MODE(MODE)          (((MODE) == GPIO_MODE_INPUT)              ||\
                                     ((MODE) == GPIO_MODE_OUTPUT_PP)          ||\
                                     ((MODE) == GPIO_MODE_OUTPUT_OD)          ||\
                                     ((MODE) == GPIO_MODE_AF_PP)              ||\
                                     ((MODE) == GPIO_MODE_AF_OD)              ||\
                                     ((MODE) == GPIO_MODE_IT_RISING)          ||\
                                     ((MODE) == GPIO_MODE_IT_FALLING)         ||\
                                     ((MODE) == GPIO_MODE_IT_RISING_FALLING)  ||\
                                     ((MODE) == GPIO_MODE_EVT_RISING)         ||\
                                     ((MODE) == GPIO_MODE_EVT_FALLING)        ||\
                                     ((MODE) == GPIO_MODE_EVT_RISING_FALLING) ||\
                                     ((MODE) == GPIO_MODE_ANALOG))
#define IS_GPIO_SPEED(SPEED)        (((SPEED) == GPIO_SPEED_FREQ_LOW)  || ((SPEED) == GPIO_SPEED_FREQ_MEDIUM) || \
                                     ((SPEED) == GPIO_SPEED_FREQ_HIGH) || ((SPEED) == GPIO_SPEED_FREQ_VERY_HIGH))
#define IS_GPIO_PULL(PULL)          (((PULL) == GPIO_NOPULL) || ((PULL) == GPIO_PULLUP) || \
                                     ((PULL) == GPIO_PULLDOWN))
#define IS_GPIO_AF(AF)              ((AF) <= 15U)

/** @defgroup GPIO_Instance_Definitions GPIO Instance Definitions
  * @{
  */
#define IS_GPIO_ALL_INSTANCE(INSTANCE) (((INSTANCE) == GPIOA) || \
                                        ((INSTANCE) == GPIOB) || \
                                        ((INSTANCE) == GPIOC) || \
                                        ((INSTANCE) == GPIOD) || \
                                        ((INSTANCE) == GPIOE) || \
                                        ((INSTANCE) == GPIOH))
/**
  * @}
  */

/**
  * @}
  */

/* Private functions ---------------------------------------------------------*/
/** @defgroup GPIO_Private_Functions GPIO Private Functions
  * @{
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* FT32F4xx_GPIO_H */

/************************ (C) COPYRIGHT Fremont Micro *****END OF FILE****/
