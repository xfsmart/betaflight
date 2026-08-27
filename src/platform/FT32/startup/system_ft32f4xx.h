/**
  ******************************************************************************
  * @file    system_ft32f4xx.h
  * @author  Betaflight FT32 Port
  * @brief   CMSIS Cortex-M4 Device System Header File for FT32F4xx devices.
  ******************************************************************************
  */

#ifndef __SYSTEM_FT32F4XX_H
#define __SYSTEM_FT32F4XX_H

#ifdef __cplusplus
 extern "C" {
#endif

extern uint32_t SystemCoreClock;          /*!< System Clock Frequency (Core Clock) */
extern void SystemInit(void);
extern void SystemCoreClockUpdate(void);
extern void OverclockRebootIfNecessary(uint32_t targetMhz);
extern void systemClockSetHSEValue(uint32_t frequency);
extern int SystemSYSCLKSource(void);
extern int SystemPLLSource(void);

/* FT32 private I-code/D-code cache controller support.  Restore accepts only
 * the state token returned by the immediately preceding disable call. */
#if defined(FT32_CACHE_ENABLE) && FT32_CACHE_ENABLE
extern void ft32CacheEnable(void);
extern uint32_t ft32CacheDisable(void);
extern void ft32CacheRestore(uint32_t state);
#endif

#ifdef __cplusplus
}
#endif

#endif /*__SYSTEM_FT32F4XX_H */
