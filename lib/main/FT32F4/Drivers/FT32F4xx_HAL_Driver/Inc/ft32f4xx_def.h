/**
  ******************************************************************************
  * @file    ft32f4xx_def.h
  * @author  FT Application Team
  * @brief   This file contains FT common defines, enumeration, macros and
  *          structures definitions.
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
#ifndef FT32F4xx_DEF_H
#define FT32F4xx_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ft32f4xx.h"
#include <stddef.h>

/* Exported types ------------------------------------------------------------*/

/** @defgroup FT_Status_structure_definition FT Status structure definition
  * @brief  FT Status structure definition
  * @{
  */
typedef enum
{
  FT_OK       = 0x00U,
  FT_ERROR    = 0x01U,
  FT_BUSY     = 0x02U,
  FT_TIMEOUT  = 0x03U
} FT_StatusTypeDef;

/**
  * @}
  */

/** @defgroup FT_Lock_status_definition FT Lock status definition
  * @brief  FT Lock status structure definition
  * @{
  */
typedef enum
{
  FT_UNLOCKED = 0x00U,
  FT_LOCKED   = 0x01U
} FT_LockTypeDef;

/**
  * @}
  */

/* Exported macros -----------------------------------------------------------*/

/** @defgroup FT_Macros FT Macros
  * @brief  FT Macros
  * @{
  */

/* Compiler abstraction for weak symbols */
#if defined(__CC_ARM)
  #define __weak    __weak
#elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #define __weak    __attribute__((weak))
#elif defined(__GNUC__)
  #define __weak    __attribute__((weak))
#elif defined(__ICCARM__)
  #define __weak    __weak
#else
  #define __weak
#endif

#define UNUSED(X) (void)X      /* To avoid gcc/g++ warnings */

#define FT_MAX_DELAY      0xFFFFFFFFU

#define FT_IS_BIT_SET(REG, BIT)         (((REG) & (BIT)) == (BIT))
#define FT_IS_BIT_CLR(REG, BIT)         (((REG) & (BIT)) == 0U)

#define __FT_LINKDMA(__HANDLE__, __PPP_DMA_FIELD__, __DMA_HANDLE__)               \
                        do{                                                      \
                              (__HANDLE__)->__PPP_DMA_FIELD__ = &(__DMA_HANDLE__); \
                              (__DMA_HANDLE__).Parent = (__HANDLE__);             \
                          } while(0)

/**
  * @}
  */

/* Lock/Unlock macros --------------------------------------------------------*/
/** @defgroup FT_Lock_Unlock FT Lock and Unlock
  * @brief  FT Lock and Unlock macros
  * @{
  */
#define __FT_LOCK(__HANDLE__)                                           \
                                do{                                        \
                                    if((__HANDLE__)->Lock == FT_LOCKED)   \
                                    {                                      \
                                       return FT_BUSY;                     \
                                    }                                      \
                                    else                                   \
                                    {                                      \
                                       (__HANDLE__)->Lock = FT_LOCKED;     \
                                    }                                      \
                                  }while (0)

#define __FT_UNLOCK(__HANDLE__)                                          \
                                  do{                                       \
                                      (__HANDLE__)->Lock = FT_UNLOCKED;     \
                                    }while (0)
/**
  * @}
  */

/* Assert macros -----------------------------------------------------------*/
/** @defgroup FT_Assert FT Assert
  * @brief  FT Assert macros
  * @{
  */
#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t* file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif /* USE_FULL_ASSERT */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* FT32F4xx_DEF_H */
