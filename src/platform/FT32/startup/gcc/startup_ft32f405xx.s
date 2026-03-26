/**
  ******************************************************************************
  * @file      startup_ft32f405xx.s
  * @author    FMD AE
  * @version   V1.0.0
  * @date      18-March-2026
  * @brief     FT32F405xx Devices vector table for GCC toolchain.
  *            This file is based on FT32F4xx CMSIS startup file for MDK.
  ******************************************************************************
  * @attention
  *
  *  Copyright (C) 2026 Fremont Micro Devices. All rights reserved.
  *
  *  This software is provided "as is" without warranty of any kind.
  *  FMD shall not be liable for any damages arising from the use of this software.
  *
  ******************************************************************************
  */
    
  .syntax unified
  .cpu cortex-m4
  .fpu softvfp
  .thumb

.global  g_pfnVectors
.global  Default_Handler

/* start address for the initialization values of the .data section. 
defined in linker script */
.word  _sidata
/* start address for the .data section. defined in linker script */  
.word  _sdata
/* end address for the .data section. defined in linker script */
.word  _edata
/* start address for the .bss section. defined in linker script */
.word  _sbss
/* end address for the .bss section. defined in linker script */
.word  _ebss
/* start address for the .fastram_bss section. defined in linker script */
.word  __fastram_bss_start__
/* end address for the .fastram_bss section. defined in linker script */
.word  __fastram_bss_end__

/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event.
*/

  .section  .text.Reset_Handler
  .weak  Reset_Handler
  .type  Reset_Handler, %function
Reset_Handler:
  // Enable CCM
  ldr     r0, =0x40023800       // RCC_BASE
  ldr     r1, [r0, #0x30]       // AHB1ENR
  orr     r1, r1, 0x00100000    // RCC_AHB1ENR_CCMDATARAMEN
  str     r1, [r0, #0x30]
  dsb

  // Defined in C code
  bl persistentObjectInit
  bl checkForBootLoaderRequest

/* Copy the data segment initializers from flash to SRAM */  
  movs  r1, #0
  b  LoopCopyDataInit

CopyDataInit:
  ldr  r3, =_sidata
  ldr  r3, [r3, r1]
  str  r3, [r0, r1]
  adds  r1, r1, #4
    
LoopCopyDataInit:
  ldr  r0, =_sdata
  ldr  r3, =_edata
  adds  r2, r0, r1
  cmp  r2, r3
  bcc  CopyDataInit

  ldr  r2, =_sbss
  b  LoopFillZerobss

FillZerobss:
  movs  r3, #0
  str  r3, [r2], #4
    
LoopFillZerobss:
  ldr  r3, = _ebss
  cmp  r2, r3
  bcc  FillZerobss

  ldr  r2, =__fastram_bss_start__
  b  LoopFillZerofastram_bss

FillZerofastram_bss:
  movs  r3, #0
  str  r3, [r2], #4
    
LoopFillZerofastram_bss:
  ldr  r3, = __fastram_bss_end__
  cmp  r2, r3
  bcc  FillZerofastram_bss

/*FPU settings*/
 ldr     r0, =0xE000ED88           /* Enable CP10,CP11 */
 ldr     r1,[r0]
 orr     r1,r1,#(0xF << 20)
 str     r1,[r0]

/* Call the clock system intitialization function.*/
 bl  SystemInit

/* Call the application's entry point.*/
  bl  main
  bx  lr    

LoopForever:
  b LoopForever

.size  Reset_Handler, .-Reset_Handler

    .section  .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b  Infinite_Loop
  .size  Default_Handler, .-Default_Handler

  .section  .isr_vector,"a",%progbits
  .type  g_pfnVectors, %object
  .size  g_pfnVectors, .-g_pfnVectors
    
g_pfnVectors:
  .word  _estack
  .word  Reset_Handler
  .word  NMI_Handler
  .word  HardFault_Handler
  .word  MemManage_Handler
  .word  BusFault_Handler
  .word  UsageFault_Handler
  .word  0
  .word  0
  .word  0
  .word  0
  .word  SVC_Handler
  .word  DebugMon_Handler
  .word  0
  .word  PendSV_Handler
  .word  SysTick_Handler
  
  /* External Interrupts - FT32F4xx standard vector names */
  .word     WWDG_Handler                    /* 0: Window Watchdog              */
  .word     PVD_Handler                     /* 1: PVD PROG VDDIO               */
  .word     TAMP_STAMP_Handler              /* 2: TAMP and STAMP               */
  .word     RTC_Handler                     /* 3: RTC                          */
  .word     FLASH_Handler                   /* 4: FLASH                        */
  .word     RCC_Handler                     /* 5: RCC                          */
  .word     EXTI0_Handler                   /* 6: EXTI 0                       */
  .word     EXTI1_Handler                   /* 7: EXTI 1                       */
  .word     EXTI2_Handler                   /* 8: EXTI 2                       */
  .word     EXTI3_Handler                   /* 9: EXTI 3                       */
  .word     EXTI4_Handler                   /* 10: EXTI 4                      */
  .word     DMA1_CH0_Handler                /* 11: DMA1 CH0                    */
  .word     DMA1_CH1_Handler                /* 12: DMA1 CH1                    */
  .word     DMA1_CH2_Handler                /* 13: DMA1 CH2                    */
  .word     DMA1_CH3_Handler                /* 14: DMA1 CH3                    */
  .word     DMA1_CH4_Handler                /* 15: DMA1 CH4                    */
  .word     DMA1_CH5_Handler                /* 16: DMA1 CH5                    */
  .word     DMA1_CH6_Handler                /* 17: DMA1 CH6                    */
  .word     ADC_Handler                     /* 18: ADC                         */
  .word     CAN1_Handler                    /* 19: FDxCAN1                     */
  .word     CAN2_Handler                    /* 20: FDxCAN2                     */
  .word     CAN3_Handler                    /* 21: FDxCAN3                     */
  .word     CAN4_Handler                    /* 22: FDxCAN4                     */
  .word     EXTI9_5_Handler                 /* 23: EXTI[9:5]                   */
  .word     TIM1_BRK_TIM9_Handler           /* 24: TIM1 Break and TIM9         */
  .word     TIM1_UP_TIM1O_Handler           /* 25: TIM1 Update and TIM10       */
  .word     TIM1_TRG_COM_TIM11_Handler      /* 26: TIM1 Trigger and TIM11      */
  .word     TIM1_CC_Handler                 /* 27: TIM1 Capture Compare        */
  .word     TIM2_Handler                    /* 28: TIM2                        */
  .word     TIM3_Handler                    /* 29: TIM3                        */
  .word     TIM4_Handler                    /* 30: TIM4                        */
  .word     I2C1_Handler                    /* 31: I2C1                        */
  .word     I2C2_Handler                    /* 32: I2C2                        */
  .word     QSPI_Handler                    /* 33: QSPI                        */
  .word     SPI1_Handler                    /* 34: SPI1                        */
  .word     SPI2_Handler                    /* 35: SPI2                        */
  .word     USART1_Handler                  /* 36: USART1                      */
  .word     USART2_Handler                  /* 37: USART2                      */
  .word     USART3_Handler                  /* 38: USART3                      */
  .word     EXTI15_10_Handler               /* 39: EXTI[15:10]                 */
  .word     RTCAlarm_Handler                /* 40: RTC Alarm                   */
  .word     OTG_FS_WKUP_Handler             /* 41: USB OTG FS Wakeup           */
  .word     TIM8_BRK_TIM12_Handler          /* 42: TIM8 Break and TIM12        */
  .word     TIM8_UP_TIM13_Handler           /* 43: TIM8 Update and TIM13       */
  .word     TIM8_TRG_COM_TIM14_Handler      /* 44: TIM8 Trigger and TIM14      */
  .word     TIM8_CC_Handler                 /* 45: TIM8 Capture Compare        */
  .word     DMA1_CH7_Handler                /* 46: DMA1 CH7                    */
  .word     FMC_Handler                     /* 47: FMC                         */
  .word     SDIO_Handler                    /* 48: SDIO                        */
  .word     TIM5_Handler                    /* 49: TIM5                        */
  .word     SPI3_Handler                    /* 50: SPI3                        */
  .word     UART4_Handler                   /* 51: UART4                       */
  .word     UART5_Handler                   /* 52: UART5                       */
  .word     TIM6_DAC_Handler                /* 53: TIM6 DAC                    */
  .word     TIM7_Handler                    /* 54: TIM7                        */
  .word     DMA2_CH0_Handler                /* 55: DMA2 CH0                    */
  .word     DMA2_CH1_Handler                /* 56: DMA2 CH1                    */
  .word     DMA2_CH2_Handler                /* 57: DMA2 CH2                    */
  .word     DMA2_CH3_Handler                /* 58: DMA2 CH3                    */
  .word     DMA2_CH4_Handler                /* 59: DMA2 CH4                    */
  .word     OTG_FS_Handler                  /* 60: OTG FS                      */
  .word     DMA2_CH5_Handler                /* 61: DMA2 CH5                    */
  .word     DMA2_CH6_Handler                /* 62: DMA2 CH6                    */
  .word     DMA2_CH7_Handler                /* 63: DMA2 CH7                    */
  .word     USART6_Handler                  /* 64: USART6                      */
  .word     I2C3_Handler                    /* 65: I2C3                        */
  .word     OTG_HS_EP1_OUT_Handler          /* 66: OTG HS EP1OUT               */
  .word     OTG_HS_EP1_IN_Handler           /* 67: OTG HS EP1IN                */
  .word     OTG_HS_WKUP_Handler             /* 68: OTG HS WKUP                 */
  .word     OTG_HS_Handler                  /* 69: OTG HS                      */
  .word     RNG_Handler                     /* 70: RNG                         */
  .word     FPU_Handler                     /* 71: FPU                         */
  .word     CRS_Handler                     /* 72: CRS                         */
  .word     SPDIF_Handler                   /* 73: SPDIF                       */
  .word     SSI_AC97_Handler                /* 74: SSI_AC97                    */
  .word     ETH_WKUP_Handler                /* 75: ETH WKUP                    */
  .word     LPUART_Handler                  /* 76: LPUART                      */
  .word     LPTIM_Handler                   /* 77: LPTIM                       */
  .word     ETH_SBD_Handler                 /* 78: ETH SBD                     */
  .word     ETH_PERCHTX_Handler             /* 79: ETH PERCHTX                 */
  .word     ETH_PERCHRX_Handler             /* 80: ETH PERCHRX                 */
  .word     EPWM1_Handler                   /* 81: EPWM1                       */
  .word     EPWM1_TZ_Handler                /* 82: EPWM1 TZ                    */
  .word     EPWM2_Handler                   /* 83: EPWM2                       */
  .word     EPWM2_TZ_Handler                /* 84: EPWM2 TZ                    */
  .word     EPWM3_Handler                   /* 85: EPWM3                       */
  .word     EPWM3_TZ_Handler                /* 86: EPWM3 TZ                    */
  .word     EPWM4_Handler                   /* 87: EPWM4                       */
  .word     EPWM4_TZ_Handler                /* 88: EPWM4 TZ                    */
  .word     ECAP_Handler                    /* 89: ECAP                        */
  .word     EQEP_Handler                    /* 90: EQEP                        */
  .word     DLL_CAL_Handler                 /* 91: DLL CAL                     */
  .word     COMP1_Handler                   /* 92: COMP1                       */
  .word     COMP2_Handler                   /* 93: COMP2                       */
  .word     COMP3_Handler                   /* 94: COMP3                       */
  .word     COMP4_Handler                   /* 95: COMP4                       */
  .word     COMP5_Handler                   /* 96: COMP5                       */
  .word     COMP6_Handler                   /* 97: COMP6                       */
  .word     ICACHE_Handler                  /* 98: ICACHE                      */
  .word     DCACHE_Handler                  /* 99: DCACHE                      */
  .word     UART7_Handler                   /* 100: UART7                      */

/*******************************************************************************
*
* Provide weak aliases for each Exception handler to the Default_Handler.
*******************************************************************************/
   .weak      NMI_Handler
   .thumb_set NMI_Handler,Default_Handler
  
   .weak      HardFault_Handler
   .thumb_set HardFault_Handler,Default_Handler
  
   .weak      MemManage_Handler
   .thumb_set MemManage_Handler,Default_Handler
  
   .weak      BusFault_Handler
   .thumb_set BusFault_Handler,Default_Handler

   .weak      UsageFault_Handler
   .thumb_set UsageFault_Handler,Default_Handler

   .weak      SVC_Handler
   .thumb_set SVC_Handler,Default_Handler

   .weak      DebugMon_Handler
   .thumb_set DebugMon_Handler,Default_Handler

   .weak      PendSV_Handler
   .thumb_set PendSV_Handler,Default_Handler

   .weak      SysTick_Handler
   .thumb_set SysTick_Handler,Default_Handler              
  
   .weak      WWDG_Handler
   .thumb_set WWDG_Handler,Default_Handler
      
   .weak      PVD_Handler
   .thumb_set PVD_Handler,Default_Handler
               
   .weak      TAMP_STAMP_Handler
   .thumb_set TAMP_STAMP_Handler,Default_Handler
            
   .weak      RTC_Handler
   .thumb_set RTC_Handler,Default_Handler
            
   .weak      FLASH_Handler
   .thumb_set FLASH_Handler,Default_Handler
                  
   .weak      RCC_Handler
   .thumb_set RCC_Handler,Default_Handler
                  
   .weak      EXTI0_Handler
   .thumb_set EXTI0_Handler,Default_Handler
                  
   .weak      EXTI1_Handler
   .thumb_set EXTI1_Handler,Default_Handler
                     
   .weak      EXTI2_Handler
   .thumb_set EXTI2_Handler,Default_Handler 
                 
   .weak      EXTI3_Handler
   .thumb_set EXTI3_Handler,Default_Handler
                        
   .weak      EXTI4_Handler
   .thumb_set EXTI4_Handler,Default_Handler
                  
   .weak      DMA1_CH0_Handler
   .thumb_set DMA1_CH0_Handler,Default_Handler
         
   .weak      DMA1_CH1_Handler
   .thumb_set DMA1_CH1_Handler,Default_Handler
                  
   .weak      DMA1_CH2_Handler
   .thumb_set DMA1_CH2_Handler,Default_Handler
                  
   .weak      DMA1_CH3_Handler
   .thumb_set DMA1_CH3_Handler,Default_Handler 
                 
   .weak      DMA1_CH4_Handler
   .thumb_set DMA1_CH4_Handler,Default_Handler
                  
   .weak      DMA1_CH5_Handler
   .thumb_set DMA1_CH5_Handler,Default_Handler
                  
   .weak      DMA1_CH6_Handler
   .thumb_set DMA1_CH6_Handler,Default_Handler
                  
   .weak      ADC_Handler
   .thumb_set ADC_Handler,Default_Handler
               
   .weak      CAN1_Handler
   .thumb_set CAN1_Handler,Default_Handler
            
   .weak      CAN2_Handler
   .thumb_set CAN2_Handler,Default_Handler
                           
   .weak      CAN3_Handler
   .thumb_set CAN3_Handler,Default_Handler
            
   .weak      CAN4_Handler
   .thumb_set CAN4_Handler,Default_Handler
            
   .weak      EXTI9_5_Handler
   .thumb_set EXTI9_5_Handler,Default_Handler
            
   .weak      TIM1_BRK_TIM9_Handler
   .thumb_set TIM1_BRK_TIM9_Handler,Default_Handler
            
   .weak      TIM1_UP_TIM1O_Handler
   .thumb_set TIM1_UP_TIM1O_Handler,Default_Handler
      
   .weak      TIM1_TRG_COM_TIM11_Handler
   .thumb_set TIM1_TRG_COM_TIM11_Handler,Default_Handler
      
   .weak      TIM1_CC_Handler
   .thumb_set TIM1_CC_Handler,Default_Handler
                  
   .weak      TIM2_Handler
   .thumb_set TIM2_Handler,Default_Handler
                  
   .weak      TIM3_Handler
   .thumb_set TIM3_Handler,Default_Handler
                  
   .weak      TIM4_Handler
   .thumb_set TIM4_Handler,Default_Handler
                  
   .weak      I2C1_Handler
   .thumb_set I2C1_Handler,Default_Handler
                     
   .weak      I2C2_Handler
   .thumb_set I2C2_Handler,Default_Handler
                     
   .weak      QSPI_Handler
   .thumb_set QSPI_Handler,Default_Handler
                  
   .weak      SPI1_Handler
   .thumb_set SPI1_Handler,Default_Handler
                           
   .weak      SPI2_Handler
   .thumb_set SPI2_Handler,Default_Handler
                  
   .weak      USART1_Handler
   .thumb_set USART1_Handler,Default_Handler
                     
   .weak      USART2_Handler
   .thumb_set USART2_Handler,Default_Handler
                     
   .weak      USART3_Handler
   .thumb_set USART3_Handler,Default_Handler
                  
   .weak      EXTI15_10_Handler
   .thumb_set EXTI15_10_Handler,Default_Handler
               
   .weak      RTCAlarm_Handler
   .thumb_set RTCAlarm_Handler,Default_Handler
            
   .weak      OTG_FS_WKUP_Handler
   .thumb_set OTG_FS_WKUP_Handler,Default_Handler
            
   .weak      TIM8_BRK_TIM12_Handler
   .thumb_set TIM8_BRK_TIM12_Handler,Default_Handler
         
   .weak      TIM8_UP_TIM13_Handler
   .thumb_set TIM8_UP_TIM13_Handler,Default_Handler
         
   .weak      TIM8_TRG_COM_TIM14_Handler
   .thumb_set TIM8_TRG_COM_TIM14_Handler,Default_Handler
      
   .weak      TIM8_CC_Handler
   .thumb_set TIM8_CC_Handler,Default_Handler
                  
   .weak      DMA1_CH7_Handler
   .thumb_set DMA1_CH7_Handler,Default_Handler
                     
   .weak      FMC_Handler
   .thumb_set FMC_Handler,Default_Handler
                     
   .weak      SDIO_Handler
   .thumb_set SDIO_Handler,Default_Handler
                     
   .weak      TIM5_Handler
   .thumb_set TIM5_Handler,Default_Handler
                     
   .weak      SPI3_Handler
   .thumb_set SPI3_Handler,Default_Handler
                     
   .weak      UART4_Handler
   .thumb_set UART4_Handler,Default_Handler
                  
   .weak      UART5_Handler
   .thumb_set UART5_Handler,Default_Handler
                  
   .weak      TIM6_DAC_Handler
   .thumb_set TIM6_DAC_Handler,Default_Handler
               
   .weak      TIM7_Handler
   .thumb_set TIM7_Handler,Default_Handler
         
   .weak      DMA2_CH0_Handler
   .thumb_set DMA2_CH0_Handler,Default_Handler
               
   .weak      DMA2_CH1_Handler
   .thumb_set DMA2_CH1_Handler,Default_Handler
                  
   .weak      DMA2_CH2_Handler
   .thumb_set DMA2_CH2_Handler,Default_Handler
            
   .weak      DMA2_CH3_Handler
   .thumb_set DMA2_CH3_Handler,Default_Handler
            
   .weak      DMA2_CH4_Handler
   .thumb_set DMA2_CH4_Handler,Default_Handler
            
   .weak      OTG_FS_Handler
   .thumb_set OTG_FS_Handler,Default_Handler
            
   .weak      DMA2_CH5_Handler
   .thumb_set DMA2_CH5_Handler,Default_Handler
                  
   .weak      DMA2_CH6_Handler
   .thumb_set DMA2_CH6_Handler,Default_Handler
                  
   .weak      DMA2_CH7_Handler
   .thumb_set DMA2_CH7_Handler,Default_Handler
                  
   .weak      USART6_Handler
   .thumb_set USART6_Handler,Default_Handler
                        
   .weak      I2C3_Handler
   .thumb_set I2C3_Handler,Default_Handler
                        
   .weak      OTG_HS_EP1_OUT_Handler
   .thumb_set OTG_HS_EP1_OUT_Handler,Default_Handler
               
   .weak      OTG_HS_EP1_IN_Handler
   .thumb_set OTG_HS_EP1_IN_Handler,Default_Handler
               
   .weak      OTG_HS_WKUP_Handler
   .thumb_set OTG_HS_WKUP_Handler,Default_Handler
            
   .weak      OTG_HS_Handler
   .thumb_set OTG_HS_Handler,Default_Handler
                  
   .weak      RNG_Handler
   .thumb_set RNG_Handler,Default_Handler

   .weak      FPU_Handler
   .thumb_set FPU_Handler,Default_Handler

   .weak      CRS_Handler
   .thumb_set CRS_Handler,Default_Handler

   .weak      SPDIF_Handler
   .thumb_set SPDIF_Handler,Default_Handler

   .weak      SSI_AC97_Handler
   .thumb_set SSI_AC97_Handler,Default_Handler

   .weak      ETH_WKUP_Handler
   .thumb_set ETH_WKUP_Handler,Default_Handler

   .weak      LPUART_Handler
   .thumb_set LPUART_Handler,Default_Handler

   .weak      LPTIM_Handler
   .thumb_set LPTIM_Handler,Default_Handler

   .weak      ETH_SBD_Handler
   .thumb_set ETH_SBD_Handler,Default_Handler

   .weak      ETH_PERCHTX_Handler
   .thumb_set ETH_PERCHTX_Handler,Default_Handler

   .weak      ETH_PERCHRX_Handler
   .thumb_set ETH_PERCHRX_Handler,Default_Handler

   .weak      EPWM1_Handler
   .thumb_set EPWM1_Handler,Default_Handler

   .weak      EPWM1_TZ_Handler
   .thumb_set EPWM1_TZ_Handler,Default_Handler

   .weak      EPWM2_Handler
   .thumb_set EPWM2_Handler,Default_Handler

   .weak      EPWM2_TZ_Handler
   .thumb_set EPWM2_TZ_Handler,Default_Handler

   .weak      EPWM3_Handler
   .thumb_set EPWM3_Handler,Default_Handler

   .weak      EPWM3_TZ_Handler
   .thumb_set EPWM3_TZ_Handler,Default_Handler

   .weak      EPWM4_Handler
   .thumb_set EPWM4_Handler,Default_Handler

   .weak      EPWM4_TZ_Handler
   .thumb_set EPWM4_TZ_Handler,Default_Handler

   .weak      ECAP_Handler
   .thumb_set ECAP_Handler,Default_Handler

   .weak      EQEP_Handler
   .thumb_set EQEP_Handler,Default_Handler

   .weak      DLL_CAL_Handler
   .thumb_set DLL_CAL_Handler,Default_Handler

   .weak      COMP1_Handler
   .thumb_set COMP1_Handler,Default_Handler

   .weak      COMP2_Handler
   .thumb_set COMP2_Handler,Default_Handler

   .weak      COMP3_Handler
   .thumb_set COMP3_Handler,Default_Handler

   .weak      COMP4_Handler
   .thumb_set COMP4_Handler,Default_Handler

   .weak      COMP5_Handler
   .thumb_set COMP5_Handler,Default_Handler

   .weak      COMP6_Handler
   .thumb_set COMP6_Handler,Default_Handler

   .weak      ICACHE_Handler
   .thumb_set ICACHE_Handler,Default_Handler

   .weak      DCACHE_Handler
   .thumb_set DCACHE_Handler,Default_Handler

   .weak      UART7_Handler
   .thumb_set UART7_Handler,Default_Handler
