/**************************************************************************//**
 * @file     core_cm4.h
 * @brief    CMSIS Cortex-M4 Core Peripheral Access Layer Header File
 * @version  V3.20
 * @date     25. February 2013
 *
 * @note
 *
 ******************************************************************************/
/* Copyright (c) 2009 - 2013 ARM LIMITED

   All rights reserved.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   - Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
   - Neither the name of ARM nor the names of its contributors may be used
     to endorse or promote products derived from this software without
     specific prior written permission.
   *
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
   ---------------------------------------------------------------------------*/


#if defined ( __ICCARM__ )
 #pragma system_include  /* treat file as system include file for MISRA check */
#endif

#ifdef __cplusplus
 extern "C" {
#endif

#ifndef __CORE_CM4_H_GENERIC
#define __CORE_CM4_H_GENERIC

/** \page CMSIS_MISRA_Exceptions  MISRA-C:2004 Compliance Exceptions
  CMSIS violates the following MISRA-C:2004 rules:

   \li Required Rule 8.5, object/function definition in header file.<br>
     Function definitions in header files are used to allow 'inlining'.

   \li Required Rule 18.4, declaration of union type or object of union type: '{...}'.<br>
     Unions are used for effective representation of core registers.

   \li Advisory Rule 19.7, Function-like macro defined.<br>
     Function-like macros are used to allow more efficient code.
 */


/*******************************************************************************
 *                 CMSIS definitions
 ******************************************************************************/
/** \ingroup Cortex_M4
  @{
 */

/*  CMSIS CM4 definitions */
#define __CM4_CMSIS_VERSION_MAIN  (0x03)                                   /*!< [31:16] CMSIS HAL main version   */
#define __CM4_CMSIS_VERSION_SUB   (0x20)                                   /*!< [15:0]  CMSIS HAL sub version    */
#define __CM4_CMSIS_VERSION       ((__CM4_CMSIS_VERSION_MAIN << 16) | \
                                    __CM4_CMSIS_VERSION_SUB          )     /*!< CMSIS HAL version number         */

#define __CORTEX_M                (0x04)                                   /*!< Cortex-M Core                    */


#if   defined ( __CC_ARM )
  #define __ASM            __asm                                      /*!< asm keyword for ARM Compiler          */
  #define __INLINE         __inline                                   /*!< inline keyword for ARM Compiler       */
  #define __STATIC_INLINE  static __inline

#elif defined ( __ICCARM__ )
  #define __ASM            __asm                                      /*!< asm keyword for IAR Compiler          */
  #define __INLINE         inline                                     /*!< inline keyword for IAR Compiler. Only available in High optimization mode! */
  #define __STATIC_INLINE  static inline

#elif defined ( __TMS470__ )
  #define __ASM            __asm                                      /*!< asm keyword for TI CCS Compiler       */
  #define __STATIC_INLINE  static inline

#elif defined ( __GNUC__ )
  #define __ASM            __asm                                      /*!< asm keyword for GNU Compiler          */
  #define __INLINE         inline                                     /*!< inline keyword for GNU Compiler       */
  #define __STATIC_INLINE  static inline

#elif defined ( __TASKING__ )
  #define __ASM            __asm                                      /*!< asm keyword for TASKING Compiler      */
  #define __INLINE         inline                                     /*!< inline keyword for TASKING Compiler   */
  #define __STATIC_INLINE  static inline

#endif

/** __FPU_USED indicates whether an FPU is used or not. For this, __FPU_PRESENT has to be checked prior to making use of FPU specific registers and functions.
*/

#define __FPU_PRESENT 1
   
#if defined ( __CC_ARM )
  #if defined __TARGET_FPU_VFP
    #if (__FPU_PRESENT == 1)
      #define __FPU_USED       1
    #else
      #warning "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
      #define __FPU_USED       0
    #endif
  #else
    #define __FPU_USED         0
  #endif

#elif defined ( __ICCARM__ )
  #if defined __ARMVFP__
    #if (__FPU_PRESENT == 1)
      #define __FPU_USED       1
    #else
      #warning "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
      #define __FPU_USED       0
    #endif
  #else
    #define __FPU_USED         0
  #endif

#elif defined ( __TMS470__ )
  #if defined __TI_VFP_SUPPORT__
    #if (__FPU_PRESENT == 1)
      #define __FPU_USED       1
    #else
      #warning "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
      #define __FPU_USED       0
    #endif
  #else
    #define __FPU_USED         0
  #endif

#elif defined ( __GNUC__ )
  #if defined (__VFP_FP__) && !defined(__SOFTFP__)
    #if (__FPU_PRESENT == 1)
      #define __FPU_USED       1
    #else
      #warning "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
      #define __FPU_USED       0
    #endif
  #else
    #define __FPU_USED         0
  #endif

#elif defined ( __TASKING__ )
  #if defined __FPU_VFP__
    #if (__FPU_PRESENT == 1)
      #define __FPU_USED       1
    #else
      #error "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"
      #define __FPU_USED       0
    #endif
  #else
    #define __FPU_USED         0
  #endif
#endif

#include <stdint.h>                      /* standard types definitions                      */
#include <core_cmInstr.h>                /* Core Instruction Access                         */
#include <core_cmFunc.h>                 /* Core Function Access                            */
#include <core_cm4_simd.h>               /* Compiler specific SIMD Intrinsics               */

#endif /* __CORE_CM4_H_GENERIC */

#ifndef __CMSIS_GENERIC

#ifndef __CORE_CM4_H_DEPENDANT
#define __CORE_CM4_H_DEPENDANT

/* check device defines and use defaults */
#if defined __CHECK_DEVICE_DEFINES
  #ifndef __CM4_REV
    #define __CM4_REV               0x0000
    #warning "__CM4_REV not defined in device header file; using default!"
  #endif

  #ifndef __FPU_PRESENT
    #define __FPU_PRESENT             0
    #warning "__FPU_PRESENT not defined in device header file; using default!"
  #endif

  #ifndef __MPU_PRESENT
    #define __MPU_PRESENT             0
    #warning "__MPU_PRESENT not defined in device header file; using default!"
  #endif

  #ifndef __NVIC_PRIO_BITS
    #define __NVIC_PRIO_BITS          4
    #warning "__NVIC_PRIO_BITS not defined in device header file; using default!"
  #endif

  #ifndef __Vendor_SysTickConfig
    #define __Vendor_SysTickConfig    0
    #warning "__Vendor_SysTickConfig not defined in device header file; using default!"
  #endif
#endif

/* IO definitions (access restrictions to peripheral registers) */
/**
    \defgroup CMSIS_glob_defs CMSIS Global Defines

    <strong>IO Type Qualifiers</strong> are used
    \li to specify the access to peripheral variables.
    \li for automatic generation of peripheral register debug information.
*/
#ifdef __cplusplus
  #define   __I     volatile             /*!< Defines 'read only' permissions                 */
#else
  #define   __I     volatile const       /*!< Defines 'read only' permissions                 */
#endif
#define     __O     volatile             /*!< Defines 'write only' permissions                */
#define     __IO    volatile             /*!< Defines 'read / write' permissions              */

/*@} end of group Cortex_M4 */



/*******************************************************************************
 *                 Register Abstraction
  Core Register contain:
  - Core Register
  - Core NVIC Register
  - Core SCB Register
  - Core SysTick Register
  - Core Debug Register
  - Core MPU Register
  - Core FPU Register
 ******************************************************************************/
/** \defgroup CMSIS_core_register Defines and Type Definitions
    \brief Type definitions and defines for Cortex-M processor based devices.
*/

/** \ingroup    CMSIS_core_register
    \defgroup   CMSIS_CORE  Status and Control Registers
    \brief  Core Register type definitions.
  @{
 */


/* System Reset */
#define NVIC_VECTRESET              0         /*!< Vector Reset Bit             */
#define NVIC_SYSRESETREQ            2         /*!< System Reset Request         */
#define NVIC_AIRCR_VECTKEY    (0x5FA << 16)   /*!< AIRCR Key for write access   */
#define NVIC_AIRCR_ENDIANESS        15        /*!< Endianess                    */
#define NVIC_MEMFAULTENA          (1 << 16)   /*!< MEMFAULTENA bit              */

/*SCB */
#define SCB_CPACR_FPU           (0xF << 20)   /*!< cp10 and cp11 set in CPACR  */


/*MPU */
#define MPU_FULL_ACC_NON_CACHEABLE    (0x320 << 16)   /*!< Attribute registerin MPU   */
#define MPU_READ_ONLY_NON_CACHEABLE   (0x620 << 16)   /*!< Attribute registerin MPU   */
#define MPU_NO_ACC_SO                 (0x0000 << 16)  /*!< Attribute registerin MPU   */

#define MPU_SIZE_512MB                (0x1C << 1)  /*!< MPU region size configuration in MPU    */
#define MPU_SIZE_2GB                  (0x1E << 1)  /*!< MPU region size configuration in MPU    */
#define MPU_SIZE_32BYTES               (0x4 << 1)  /*!< MPU region size configuration in MPU    */


#define MMFSR_IACCVIOL                  (1 << 0)   /*!< Fault status register information   */
#define MMFSR_DACCVIOL                  (1 << 1)   /*!< Fault status register information   */
#define MMFSR_MUNSTKERR                 (1 << 3)   /*!< Fault status register information   */
#define MMFSR_MSTKERR                   (1 << 4)   /*!< Fault status register information   */
#define MMFSR_MLSPERR                   (1 << 5)   /*!< Fault status register information   */
#define MMFSR_MMARVALID                 (1 << 7)   /*!< Fault status register information   */

/* Core Debug */
#define CoreDebug_DEMCR_TRCENA  (1 << 24)     /*!< DEMCR TRCENA enable          */
#define ITM_TCR_BUSY            (1 << 23)     /*!< ITM Busy                     */
#define ITM_TCR_TBUSID          16            /*!< ITM TraceBusID offset        */
#define ITM_TCR_TS_GLOBAL_128   (0x01 << 10)  /*!< Timestamp every 128 cycles   */
#define ITM_TCR_TS_GLOBAL_8192  (0x10 << 10)  /*!< Timestamp every 8192 cycles  */
#define ITM_TCR_TS_GLOBAL_ALL   (0x11 << 10)  /*!< Timestamp all                */
#define ITM_TCR_SWOENA          (1 << 4)      /*!< ITM Enables asynchronous-specific usage model*/
#define ITM_TCR_TXENA           (1 << 3)      /*!< ITM Enable hardware event packet 
                                                    emission to the TPIU from the DWT */
#define ITM_TCR_SYNCENA         (1 << 2)      /*!< ITM Enable synchronization packet
                                                   transmission for a synchronous TPIU*/
#define ITM_TCR_TSENA           (1 << 1)      /*!< ITM Enable differential timestamps */
#define ITM_TCR_ITMENA          (1 << 0)      /*!< ITM enable                   */
#define ITM_TER_STIM0           (1 << 0)      /*!< ITM stimulus0 enable         */
#define ITM_TER_STIM1           (1 << 1)      /*!< ITM stimulus1 enable         */
#define ITM_TER_STIM2           (1 << 2)      /*!< ITM stimulus2 enable         */
   
#define DWT_CTRL_CYCEVTENA      (1 << 22)     /*!< DWT Periodic packet enable   */
#define DWT_CTRL_FOLDEVTENA     (1 << 21)     /*!< DWT fold event enable        */
#define DWT_CTRL_LSUEVTENA      (1 << 20)     /*!< DWT lsu event enable         */
#define DWT_CTRL_SLEEPEVTENA    (1 << 19)     /*!< DWT sleep event enabel       */
#define DWT_CTRL_EXCEVTENA      (1 << 18)     /*!< DWT exception event enable   */
#define DWT_CTRL_CPIEVTENA      (1 << 17)     /*!< DWT CPI event enable         */
#define DWT_CTRL_EXCTRCENA      (1 << 16)     /*!< DWT Exception trace enable   */
#define DWT_CTRL_PCSAMPLENA     (1 << 12)     /*!< DWT Periodic event PC select */
#define DWT_CTRL_SYNCTAP24      (1 << 10)     /*!< DWT Synch packet tap at 24   */
#define DWT_CTRL_SYNCTAP26      (2 << 10)     /*!< DWT Synch packet tap at 26   */
#define DWT_CTRL_SYNCTAP28      (3 << 10)     /*!< DWT Synch packet tap at 28   */
#define DWT_CTRL_CYCTAP         (1 << 9)      /*!< DWT POSTCNT tap select       */
#define DWT_CTRL_POSTPRESET_BITS     1        /*!< DWT POSTCNT reload offset    */
#define DWT_CTRL_CYCCNTENA           1        /*!< DWT Cycle counter enable     */

/* DWT Function, EMITRANGE=0, CYCMATCH = 0 */
#define DWT_FUNC_SAMP_PC        0x1           /*!< DWT Func: PC Sample Packet   */
#define DWT_FUNC_SAMP_DATA      0x2           /*!< DWT Func: Data Value Packet  */
#define DWT_FUNC_SAMP_PC_DATA   0x3           /*!< DWT Func: PC and Data Packets*/
#define DWT_FUNC_PC_WPT         0x4           /*!< DWT Func: PC Watchpoint      */
#define DWT_FUNC_TRIG_PC        0x8           /*!< DWT Func: PC to CMPMATCH(ETM)*/
#define DWT_FUNC_TRIG_RD        0x9           /*!< DWT Func: Daddr(R)to CMPMATCH*/
#define DWT_FUNC_TRIG_WR        0xA           /*!< DWT Func: Daddr(W)to CMPMATCH*/
#define DWT_FUNC_TRIG_RW        0xB           /*!< DWT Func: Daddr to CMPMATCH  */

#define DWT_CTRL_POSTPRESET_10  0xA
   
/* ETM */
#define ETM_CR_PWRDN                1         /*!< ETM Power Down                */
#define ETM_CR_STALLPROC        (1 << 7)      /*!< ETM Stall Processor           */
#define ETM_CR_BRANCH_OUTPUT    (1 << 8)      /*!< ETM Branch Broadcast          */
#define ETM_CR_DEBUG            (1 << 9)      /*!< ETM Debug Request control     */
#define ETM_CR_PROGBIT          (1 << 10)     /*!< ETM ProgBit                   */
#define ETM_CR_ETMEN            (1 << 11)     /*!< ETM ProgBit                   */
#define ETM_CR_TSEN             (1 << 28)     /*!< ETM Timestamp Enable          */
#define ETM_SR_OVERFLOW             1         /*!< ETM Overflow Status           */
#define ETM_SR_PROGBIT          (1 << 1)      /*!< ETM Progbit Status            */
#define ETM_SR_SSTOP            (1 << 2)      /*!< ETM Start/Stop Status         */
#define ETM_SR_TRIGGER          (1 << 3)      /*!< ETM Triggered Status          */
#define ETM_SCR_FIFOFULL        (1 << 8)      /*!< ETM System FIFOFILL Support   */
#define ETM_EVT_A                   0         /*!< ETM Event A offset            */
#define ETM_EVT_NOTA            (1 << 14)     /*!< ETM Event fn Not(A)           */
#define ETM_EVT_AANDB           (2 << 14)     /*!< ETM Event fn (A)and(B)        */
#define ETM_EVT_NOTAANDB        (3 << 14)     /*!< ETM Event fn (A)and(not(B))   */
#define ETM_EVT_NOTAANDNOTB     (4 << 14)     /*!< ETM Event fn (not(A))and(not(B))*/
#define ETM_EVT_AORB            (5 << 14)     /*!< ETM Event fn (A)or(B)         */
#define ETM_EVT_NOTAORB         (6 << 14)     /*!< ETM Event fn (not(A))or(B)    */
#define ETM_EVT_NOTAORNOTB      (7 << 14)     /*!< ETM Event fn (not(A))or(not(B))*/
#define ETM_EVT_RESB                   7     /*!< ETM Event B offset            */
#define ETM_EVT_DWT0               0x20      /*!< ETM Event select DWT0         */
#define ETM_EVT_DWT1               0x21      /*!< ETM Event select DWT1         */
#define ETM_EVT_DWT2               0x22      /*!< ETM Event select DWT2         */
#define ETM_EVT_DWT3               0x23      /*!< ETM Event select DWT3         */
#define ETM_EVT_COUNT1             0x40      /*!< ETM Event select Counter1 at zero */
#define ETM_EVT_SSTOP              0x5F      /*!< ETM Event select Start/Stop   */
#define ETM_EVT_EXTIN0             0x60      /*!< ETM Event select ExtIn0       */
#define ETM_EVT_EXTIN1             0x61      /*!< ETM Event select ExtIn1       */
#define ETM_EVT_TRUE               0x6F      /*!< ETM Event select Always True  */
#define ETM_TECR1_USE_SS           (1 << 25) /*!< ETM TraceEnable Start/Stop enable */
#define CS_UNLOCK                  0xC5ACCE55UL

#define ETM_TESSEICR_ICE0          0x1       /*!< ETM Start/Stop from DWT0      */
#define ETM_TESSEICR_ICE1          0x2       /*!< ETM Start/Stop from DWT1      */
#define ETM_TESSEICR_ICE2          0x4       /*!< ETM Start/Stop from DWT2      */
#define ETM_TESSEICR_ICE3          0x8       /*!< ETM Start/Stop from DWT3      */
   
#define ETM_TESSEICR_STOP          16        /*!< ETM Start/Stop Stop offset    */

/* TPIU */
#define TPIU_PIN_TRACEPORT         0       /*!< TPIU Selected Pin Protocol Trace Port */
#define TPIU_PIN_MANCHESTER        1       /*!< TPIU Selected Pin Protocol Manchester */
#define TPIU_PIN_NRZ               2       /*!< TPIU Selected Pin Protocol NRZ (uart) */

/* memory mapping struct for Nested Vectored Interrupt Controller (NVIC) */
typedef struct
{
  __IO uint32_t ISER[8];                      /*!< Interrupt Set Enable Register            */
       uint32_t RESERVED0[24];
  __IO uint32_t ICER[8];                      /*!< Interrupt Clear Enable Register          */
       uint32_t RSERVED1[24];
  __IO uint32_t ISPR[8];                      /*!< Interrupt Set Pending Register           */
       uint32_t RESERVED2[24];
  __IO uint32_t ICPR[8];                      /*!< Interrupt Clear Pending Register         */
       uint32_t RESERVED3[24];
  __IO uint32_t IABR[8];                      /*!< Interrupt Active bit Register            */
       uint32_t RESERVED4[56];
  __IO uint8_t  IP[240];                      /*!< Interrupt Priority Register, 8Bit wide   */
       uint32_t RESERVED5[644];
  __O  uint32_t STIR;                         /*!< Software Trigger Interrupt Register      */
}  NVIC_Type;


/* memory mapping struct for System Control Block */
typedef struct
{
  __I  uint32_t CPUID;                        /*!< CPU ID Base Register                                     */
  __IO uint32_t ICSR;                         /*!< Interrupt Control State Register                         */
  __IO uint32_t VTOR;                         /*!< Vector Table Offset Register                             */
  __IO uint32_t AIRCR;                        /*!< Application Interrupt / Reset Control Register           */
  __IO uint32_t SCR;                          /*!< System Control Register                                  */
  __IO uint32_t CCR;                          /*!< Configuration Control Register                           */
  __IO uint8_t  SHP[12];                      /*!< System Handlers Priority Registers (4-7, 8-11, 12-15)    */
  __IO uint32_t SHCSR;                        /*!< System Handler Control and State Register                */
  __IO uint32_t CFSR;                         /*!< Configurable Fault Status Register                       */
  __IO uint32_t HFSR;                         /*!< Hard Fault Status Register                               */
  __IO uint32_t DFSR;                         /*!< Debug Fault Status Register                              */
  __IO uint32_t MMFAR;                        /*!< Mem Manage Address Register                              */
  __IO uint32_t BFAR;                         /*!< Bus Fault Address Register                               */
  __IO uint32_t AFSR;                         /*!< Auxiliary Fault Status Register                          */
  __I  uint32_t PFR[2];                       /*!< Processor Feature Register                               */
  __I  uint32_t DFR;                          /*!< Debug Feature Register                                   */
  __I  uint32_t ADR;                          /*!< Auxiliary Feature Register                               */
  __I  uint32_t MMFR[4];                      /*!< Memory Model Feature Register                            */
  __I  uint32_t ISAR[5];                      /*!< ISA Feature Register                                     */
       uint32_t RESERVED0[5];
  __IO uint32_t CPACR;                        /*!< Coprocessor access register                              */
       uint32_t RESERVED1[106];
  __IO uint32_t FPCCR;                        /*!< Floating point context control register                              */
  __IO uint32_t FPCAR;                        /*!< Floating point context address register                             */
  __IO uint32_t FPDSCR;                        /*!< Floating point default status control register                     */
  __IO uint32_t MVFR0;                        /*!< Media and VFP feature register                              */
  __IO uint32_t MVFR1;                        /*!< Media and VFP feature register                              */
       uint32_t RESERVED2[34];
  __I  uint32_t PID4;                        /*!< CoreSight register  */
  __I  uint32_t PID5;                        /*!< CoreSight register  */
  __I  uint32_t PID6;                        /*!< CoreSight register  */
  __I  uint32_t PID7;                        /*!< CoreSight register  */
  __I  uint32_t PID0;                        /*!< CoreSight register  */
  __I  uint32_t PID1;                        /*!< CoreSight register  */
  __I  uint32_t PID2;                        /*!< CoreSight register  */
  __I  uint32_t PID3;                        /*!< CoreSight register  */
  __I  uint32_t CID0;                        /*!< CoreSight register  */
  __I  uint32_t CID1;                        /*!< CoreSight register  */
  __I  uint32_t CID2;                        /*!< CoreSight register  */
  __I  uint32_t CID3;                        /*!< CoreSight register  */
} SCB_Type;


/* memory mapping struct for SysTick */
typedef struct
{
  __IO uint32_t CTRL;                         /*!< SysTick Control and Status Register */
  __IO uint32_t LOAD;                         /*!< SysTick Reload Value Register       */
  __IO uint32_t VAL;                          /*!< SysTick Current Value Register      */
  __I  uint32_t CALIB;                        /*!< SysTick Calibration Register        */
} SysTick_Type;


/* memory mapping structur for ITM */
typedef struct
{
  __O  union  
  {
    __O  uint8_t    u8;                       /*!< ITM Stimulus Port 8-bit               */
    __O  uint16_t   u16;                      /*!< ITM Stimulus Port 16-bit              */
    __O  uint32_t   u32;                      /*!< ITM Stimulus Port 32-bit              */
  }  PORT [32];                               /*!< ITM Stimulus Port Registers           */
       uint32_t RESERVED0[864];
  __IO uint32_t TER;                          /*!< ITM Trace Enable Register             */
       uint32_t RESERVED1[15];
  __IO uint32_t TPR;                          /*!< ITM Trace Privilege Register          */
       uint32_t RESERVED2[15];
  __IO uint32_t TCR;                          /*!< ITM Trace Control Register            */
       uint32_t RESERVED3[29];
  __IO uint32_t IWR;                          /*!< ITM Integration Write Register        */
  __IO uint32_t IRR;                          /*!< ITM Integration Read Register         */
  __IO uint32_t IMCR;                         /*!< ITM Integration Mode Control Register */
       uint32_t RESERVED4[43];
  __IO uint32_t LAR;                          /*!< ITM Lock Access Register              */
  __IO uint32_t LSR;                          /*!< ITM Lock Status Register              */
       uint32_t RESERVED5[6];
  __I  uint32_t PID4;                        /*!< CoreSight register  */
  __I  uint32_t PID5;                        /*!< CoreSight register  */
  __I  uint32_t PID6;                        /*!< CoreSight register  */
  __I  uint32_t PID7;                        /*!< CoreSight register  */
  __I  uint32_t PID0;                        /*!< CoreSight register  */
  __I  uint32_t PID1;                        /*!< CoreSight register  */
  __I  uint32_t PID2;                        /*!< CoreSight register  */
  __I  uint32_t PID3;                        /*!< CoreSight register  */
  __I  uint32_t CID0;                        /*!< CoreSight register  */
  __I  uint32_t CID1;                        /*!< CoreSight register  */
  __I  uint32_t CID2;                        /*!< CoreSight register  */
  __I  uint32_t CID3;                        /*!< CoreSight register  */
} ITM_Type;

/* memory mapping structur for DWT */
typedef struct
{
  __IO uint32_t CTRL;                         /*!< DWT Control Register        */
  __IO uint32_t CYCCNT;                       /*!< DWT Cycle Count             */
  __IO uint32_t CPICNT;                       /*!< DWT CPI Count               */
  __IO uint32_t EXCCNT;                       /*!< DWT Exception Count         */
  __IO uint32_t SLEEPCNT;                     /*!< DWT Sleep Count             */
  __IO uint32_t LSUCNT;                       /*!< DWT LSU Count               */
  __IO uint32_t FOLDCNT;                      /*!< DWT Cold Count              */
  __I  uint32_t PCSR;                         /*!< DWT PC Sample Register      */
  __IO uint32_t COMP0;                        /*!< DWT Comparator0 Value       */
  __IO uint32_t MASK0;                        /*!< DWT Comparator0 Mask        */
  __IO uint32_t FUNCTION0;                    /*!< DWT Comparator0 Function    */
       uint32_t RESERVED0;
  __IO uint32_t COMP1;                        /*!< DWT Comparator1 Value       */
  __IO uint32_t MASK1;                        /*!< DWT Comparator1 Mask        */
  __IO uint32_t FUNCTION1;                    /*!< DWT Comparator1 Function    */
       uint32_t RESERVED1;
  __IO uint32_t COMP2;                        /*!< DWT Comparator2 Value       */
  __IO uint32_t MASK2;                        /*!< DWT Comparator2 Mask        */
  __IO uint32_t FUNCTION2;                    /*!< DWT Comparator2 Function    */
       uint32_t RESERVED2;
  __IO uint32_t COMP3;                        /*!< DWT Comparator3 Value       */
  __IO uint32_t MASK3;                        /*!< DWT Comparator3 Mask        */
  __IO uint32_t FUNCTION3;                    /*!< DWT Comparator3 Function    */
       uint32_t RESERVED3[989];
  __I  uint32_t PID4;                        /*!< CoreSight register  */
  __I  uint32_t PID5;                        /*!< CoreSight register  */
  __I  uint32_t PID6;                        /*!< CoreSight register  */
  __I  uint32_t PID7;                        /*!< CoreSight register  */
  __I  uint32_t PID0;                        /*!< CoreSight register  */
  __I  uint32_t PID1;                        /*!< CoreSight register  */
  __I  uint32_t PID2;                        /*!< CoreSight register  */
  __I  uint32_t PID3;                        /*!< CoreSight register  */
  __I  uint32_t CID0;                        /*!< CoreSight register  */
  __I  uint32_t CID1;                        /*!< CoreSight register  */
  __I  uint32_t CID2;                        /*!< CoreSight register  */
  __I  uint32_t CID3;                        /*!< CoreSight register  */
} DWT_Type;

/* memory mapping structur for FPB */
typedef struct
{
  __IO uint32_t CTRL;                         /*!< FPB Control Registers                */
  __IO uint32_t REMAP;                        /*!< FPB Control Registers                */
  __IO uint32_t COMP0;                        /*!< FPB Control Registers                */
  __IO uint32_t COMP1;                        /*!< FPB Control Registers                */
  __IO uint32_t COMP2;                        /*!< FPB Control Registers                */
  __IO uint32_t COMP3;                        /*!< FPB Control Registers                */
  __IO uint32_t COMP4;                        /*!< FPB Control Registers                */
  __IO uint32_t COMP5;                        /*!< FPB Control Registers                */
  __IO uint32_t COMP6;                        /*!< FPB Control Registers                */
  __IO uint32_t COMP7;                        /*!< FPB Control Registers                */
       uint32_t RESERVED3[1002];
  __I  uint32_t PID4;                        /*!< CoreSight register  */
  __I  uint32_t PID5;                        /*!< CoreSight register  */
  __I  uint32_t PID6;                        /*!< CoreSight register  */
  __I  uint32_t PID7;                        /*!< CoreSight register  */
  __I  uint32_t PID0;                        /*!< CoreSight register  */
  __I  uint32_t PID1;                        /*!< CoreSight register  */
  __I  uint32_t PID2;                        /*!< CoreSight register  */
  __I  uint32_t PID3;                        /*!< CoreSight register  */
  __I  uint32_t CID0;                        /*!< CoreSight register  */
  __I  uint32_t CID1;                        /*!< CoreSight register  */
  __I  uint32_t CID2;                        /*!< CoreSight register  */
  __I  uint32_t CID3;                        /*!< CoreSight register  */
} FPB_Type;


typedef struct
{
  __I  uint32_t SSPSR;                        /*!< TPIU Supported Synchronous Port Size register */
  __IO uint32_t CSPSR;                        /*!< TPIU Current Synchronous Port Size register  */
       uint32_t RESERVED0[2];
  __IO uint32_t ACPR;                         /*!< TPIU Asynchronous Clock Prescale Register    */
       uint32_t RESERVED1[55];
  __IO uint32_t SPPR;                         /*!< TPIU Selected Pin Protocol Register          */
       uint32_t RESERVED2[132];
  __I  uint32_t FFPR;                         /*!< TPIU Formatter and Flush Status Register     */
       uint32_t RESERVED3[759];
  __I  uint32_t TRIGGER;                      /*!< TPIU Trigger Integration Register            */
  __I  uint32_t ITETMDATA;                    /*!< TPIU  Integration ETM Data Register          */
  __I  uint32_t ITATBCR2;                     /*!< TPIU  Integration ATB Control2 Register      */
  __I  uint32_t ITITMDATA;                    /*!< TPIU  Integration ITM Data Register          */
  __I  uint32_t ITATBCR0;                     /*!< TPIU  Integration ATB Control0 Register      */
       uint32_t RESERVED4;
  __IO uint32_t ITCTRL;                       /*!< TPIU  Integration Control Register  */
       uint32_t RESERVED5[40];
  __IO uint32_t CLAIMSET;                     /*!< TPIU  Claim Tag Set Register        */
  __IO uint32_t CLAIMCLR;                     /*!< TPIU  Claim Tag Clear Register      */
       uint32_t RESERVED6[8];
  __IO uint32_t DEVID;                        /*!< TPIU  Device ID Register            */
       uint32_t RESERVED7;
  __I  uint32_t PID4;                         /*!< CoreSight register  */
  __I  uint32_t PID5;                         /*!< CoreSight register  */
  __I  uint32_t PID6;                         /*!< CoreSight register  */
  __I  uint32_t PID7;                         /*!< CoreSight register  */
  __I  uint32_t PID0;                         /*!< CoreSight register  */
  __I  uint32_t PID1;                         /*!< CoreSight register  */
  __I  uint32_t PID2;                         /*!< CoreSight register  */
  __I  uint32_t PID3;                         /*!< CoreSight register  */
  __I  uint32_t CID0;                         /*!< CoreSight register  */
  __I  uint32_t CID1;                         /*!< CoreSight register  */
  __I  uint32_t CID2;                         /*!< CoreSight register  */
  __I  uint32_t CID3;                         /*!< CoreSight register  */
} TPIU_Type;

/* memory mapping structure for ETM */
typedef struct
{
  __IO uint32_t CR;                          /*!< ETM Control Register                      */
  __I  uint32_t CCR;                         /*!< ETM Configuration Code Register           */
  __IO uint32_t TRIGGER;                     /*!< ETM Trigger Event Register                */
       uint32_t RESERVED0;
  __IO uint32_t SR;                          /*!< ETM Status Register                       */
  __I  uint32_t SCR;                         /*!< ETM System Configuration Register         */
       uint32_t RESERVED1[2];
  __IO uint32_t TEEVR;                       /*!< ETM Trace Enable Event Register           */
  __IO uint32_t TECR1;                       /*!< ETM Trace Enable Control 1 Register       */
       uint32_t RESERVED2X;
  __IO uint32_t FFLR;                        /*!< ETM Fifo Full Level Register              */
       uint32_t RESERVED2[68];
  __IO uint32_t CNTRLDVR1;                   /*!< ETM Counter1 Reload Register              */
       uint32_t RESERVED3[39];
  __I  uint32_t SYNCFR;                      /*!< ETM Sync Frequency Register               */
  __I  uint32_t IDR;                         /*!< ETM ID Register                           */
  __I  uint32_t CCER;                        /*!< ETM Configuration Code Extention Register */
       uint32_t RESERVED4;
  __IO uint32_t TESSEICR;                    /*!< ETM Trace Enable Start Stop EICE Register */
       uint32_t RESERVED5;
  __IO uint32_t TSEVR;                       /*!< ETM Timestamp Event Register              */
       uint32_t RESERVED6;
  __IO uint32_t TRACEIDR;                    /*!< ETM Trace ID Register                     */
       uint32_t RESERVED7[68];
  __I  uint32_t PDSR;                        /*!< ETM Power Down Status Register            */
       uint32_t RESERVED8[754];
  __I  uint32_t ITMISCIN;                    /*!< ETM Integration Misc In Register          */
       uint32_t RESERVED9;
  __IO uint32_t ITTRIGOUT;                   /*!< ETM Integration Trigger Register          */
       uint32_t RESERVED10;
  __I  uint32_t ITATBCR2;                    /*!< ETM Integration ATB2 Register             */
       uint32_t RESERVED11;
  __IO uint32_t ITATBCR0;                    /*!< ETM Integration ATB0 Register             */
       uint32_t RESERVED12;
  __IO uint32_t ITCTRL;                      /*!< ETM Integration Mode Control Register     */
       uint32_t RESERVED13[39];
  __IO uint32_t CLAIMSET;                    /*!< ETM Claim Set Register                    */
  __IO uint32_t CLAIMCLR;                    /*!< ETM Claim Clear Register                  */
       uint32_t RESERVED14[2];
  __IO uint32_t LAR;                         /*!< ETM Lock Access Register                  */
  __I  uint32_t LSR;                         /*!< ETM Lock Status Register                  */
  __I  uint32_t AUTHSTATUS;                  /*!< ETM Authentication Status Register        */
       uint32_t RESERVED15[4]; 
  __I  uint32_t DEVTYPE;                     /*!< ETM Device Type Register                  */
  __I  uint32_t PID4;                        /*!< CoreSight register  */
  __I  uint32_t PID5;                        /*!< CoreSight register  */
  __I  uint32_t PID6;                        /*!< CoreSight register  */
  __I  uint32_t PID7;                        /*!< CoreSight register  */
  __I  uint32_t PID0;                        /*!< CoreSight register  */
  __I  uint32_t PID1;                        /*!< CoreSight register  */
  __I  uint32_t PID2;                        /*!< CoreSight register  */
  __I  uint32_t PID3;                        /*!< CoreSight register  */
  __I  uint32_t CID0;                        /*!< CoreSight register  */
  __I  uint32_t CID1;                        /*!< CoreSight register  */
  __I  uint32_t CID2;                        /*!< CoreSight register  */
  __I  uint32_t CID3;                        /*!< CoreSight register  */
} ETM_Type;
     
/* memory mapped struct for Interrupt Type */
typedef struct
{
       uint32_t RESERVED0;
  __I  uint32_t ICTR;                         /*!< Interrupt Control Type Register  */
  __IO uint32_t ACTLR;                        /*!< Auxiliary Control Register       */
} InterruptType_Type;


/* Memory Protection Unit */
#if defined (__MPU_PRESENT) && (__MPU_PRESENT == 1)
typedef struct
{
  __I  uint32_t TYPE;                         /*!< MPU Type Register                               */
  __IO uint32_t CTRL;                         /*!< MPU Control Register                            */
  __IO uint32_t RNR;                          /*!< MPU Region RNRber Register                      */
  __IO uint32_t RBAR;                         /*!< MPU Region Base Address Register                */
  __IO uint32_t RASR;                         /*!< MPU Region Attribute and Size Register          */
  __IO uint32_t RBAR_A1;                      /*!< MPU Alias 1 Region Base Address Register        */
  __IO uint32_t RASR_A1;                      /*!< MPU Alias 1 Region Attribute and Size Register  */
  __IO uint32_t RBAR_A2;                      /*!< MPU Alias 2 Region Base Address Register        */
  __IO uint32_t RASR_A2;                      /*!< MPU Alias 2 Region Attribute and Size Register  */
  __IO uint32_t RBAR_A3;                      /*!< MPU Alias 3 Region Base Address Register        */
  __IO uint32_t RASR_A3;                      /*!< MPU Alias 3 Region Attribute and Size Register  */
} MPU_Type;
#endif


/* Core Debug Register */
typedef struct
{
  __IO uint32_t DHCSR;                        /*!< Debug Halting Control and Status Register       */
  __O  uint32_t DCRSR;                        /*!< Debug Core Register Selector Register           */
  __IO uint32_t DCRDR;                        /*!< Debug Core Register Data Register               */
  __IO uint32_t DEMCR;                        /*!< Debug Exception and Monitor Control Register    */
} CoreDebug_Type;


/* Memory mapping of Cortex-M4 Hardware */

#define SCS_BASE            (0xE000E000)                              /*!< System Control Space Base Address    */
#define ITM_BASE            (0xE0000000)                              /*!< ITM Base Address                     */
#define DWT_BASE            (0xE0001000)                              /*!< DWT Base Address                     */
#define FPB_BASE            (0xE0002000)                              /*!< FPB Base Address                     */
#define TPIU_BASE           (0xE0040000)                              /*!< TPIU Base Address                    */
#define ETM_BASE            (0xE0041000)                              /*!< ETM Base Address                     */
#define CoreDebug_BASE      (0xE000EDF0)                              /*!< Core Debug Base Address              */
#define SysTick_BASE        (SCS_BASE +  0x0010)                      /*!< SysTick Base Address                 */
#define NVIC_BASE           (SCS_BASE +  0x0100)                      /*!< NVIC Base Address                    */
#define SCB_BASE            (SCS_BASE +  0x0D00)                      /*!< System Control Block Base Address    */

#define InterruptType       ((InterruptType_Type *) SCS_BASE)         /*!< Interrupt Type Register              */
#define SCB                 ((SCB_Type *)           SCB_BASE)         /*!< SCB configuration struct             */
#define SysTick             ((SysTick_Type *)       SysTick_BASE)     /*!< SysTick configuration struct         */
#define NVIC                ((NVIC_Type *)          NVIC_BASE)        /*!< NVIC configuration struct            */
#define ITM                 ((ITM_Type *)           ITM_BASE)         /*!< ITM configuration struct             */
#define DWT                 ((DWT_Type *)           DWT_BASE)         /*!< DWT configuration struct             */
#define FPB                 ((FPB_Type *)           FPB_BASE)         /*!< DWT configuration struct             */
#define TPIU                ((TPIU_Type *)          TPIU_BASE)         /*!< TPIU configuration struct           */
#define ETM                 ((ETM_Type *)           ETM_BASE)         /*!< ETM configuration struct             */
#define CoreDebug           ((CoreDebug_Type *)     CoreDebug_BASE)   /*!< Core Debug configuration struct      */

#if defined (__MPU_PRESENT) && (__MPU_PRESENT == 1)
  #define MPU_BASE          (SCS_BASE +  0x0D90)                      /*!< Memory Protection Unit               */
  #define MPU               ((MPU_Type*)            MPU_BASE)         /*!< Memory Protection Unit               */
#endif




/*******************************************************************************
 *                Hardware Abstraction Layer
  Core Function Interface contains:
  - Core NVIC Functions
  - Core SysTick Functions
  - Core Debug Functions
  - Core Register Access Functions
 ******************************************************************************/
/** \defgroup CMSIS_Core_FunctionInterface Functions and Instructions Reference
*/



/* ##########################   NVIC functions  #################################### */


/**
 * @brief  Set the Priority Grouping in NVIC Interrupt Controller
 *
 * @param  uint32_t priority_grouping is priority grouping field
 * @return none 
 *
 * Set the priority grouping field using the required unlock sequence.
 * The parameter priority_grouping is assigned to the field 
 * SCB->AIRCR [10:8] PRIGROUP field. Only values from 0..7 are used.
 * In case of a conflict between priority grouping and available
 * priority bits (__NVIC_PRIO_BITS) the smallest possible priority group is set.
 */
static __INLINE void NVIC_SetPriorityGrouping(uint32_t PriorityGroup)
{
  uint32_t reg_value;
  uint32_t PriorityGroupTmp = (PriorityGroup & 0x07);                         /* only values 0..7 are used          */
  
  reg_value  = SCB->AIRCR;                                                    /* read old register configuration    */
  reg_value &= ~((0xFFFFU << 16) | (0x0F << 8));                              /* clear bits to change               */
  reg_value  = ((reg_value | NVIC_AIRCR_VECTKEY | (PriorityGroupTmp << 8)));  /* Insert write key and priorty group */
  SCB->AIRCR = reg_value;
}

/**
 * @brief  Get the Priority Grouping from NVIC Interrupt Controller
 *
 * @param  none
 * @return uint32_t   priority grouping field 
 *
 * Get the priority grouping from NVIC Interrupt Controller.
 * priority grouping is SCB->AIRCR [10:8] PRIGROUP field.
 */
static __INLINE uint32_t NVIC_GetPriorityGrouping(void)
{
  return ((SCB->AIRCR >> 8) & 0x07);                                          /* read priority grouping field */
}

/**
 * @brief  Enable Interrupt in NVIC Interrupt Controller
 *
 * @param  IRQn_Type IRQn specifies the interrupt number
 * @return none 
 *
 * Enable a device specific interupt in the NVIC interrupt controller.
 * The interrupt number cannot be a negative value.
 */
static __INLINE void NVIC_EnableIRQ(IRQn_Type IRQn)
{
  NVIC->ISER[((uint32_t)(IRQn) >> 5)] = (1 << ((uint32_t)(IRQn) & 0x1F)); /* enable interrupt */
}

/**
 * @brief  Disable the interrupt line for external interrupt specified
 * 
 * @param  IRQn_Type IRQn is the positive number of the external interrupt
 * @return none
 * 
 * Disable a device specific interupt in the NVIC interrupt controller.
 * The interrupt number cannot be a negative value.
 */
static __INLINE void NVIC_DisableIRQ(IRQn_Type IRQn)
{
  NVIC->ICER[((uint32_t)(IRQn) >> 5)] = (1 << ((uint32_t)(IRQn) & 0x1F)); /* disable interrupt */
}

/**
 * @brief  Read the interrupt pending bit for a device specific interrupt source
 * 
 * @param  IRQn_Type IRQn is the number of the device specifc interrupt
 * @return uint32_t 1 if pending interrupt else 0
 *
 * Read the pending register in NVIC and return 1 if its status is pending, 
 * otherwise it returns 0
 */
static __INLINE uint32_t NVIC_GetPendingIRQ(IRQn_Type IRQn)
{
  return((uint32_t) ((NVIC->ISPR[(uint32_t)(IRQn) >> 5] & (1 << ((uint32_t)(IRQn) & 0x1F)))?1:0)); /* Return 1 if pending else 0 */
}

/**
 * @brief  Set the pending bit for an external interrupt
 * 
 * @param  IRQn_Type IRQn is the Number of the interrupt
 * @return none
 *
 * Set the pending bit for the specified interrupt.
 * The interrupt number cannot be a negative value.
 */
static __INLINE void NVIC_SetPendingIRQ(IRQn_Type IRQn)
{
  NVIC->ISPR[((uint32_t)(IRQn) >> 5)] = (1 << ((uint32_t)(IRQn) & 0x1F)); /* set interrupt pending */
}

/**
 * @brief  Clear the pending bit for an external interrupt
 *
 * @param  IRQn_Type IRQn is the Number of the interrupt
 * @return none
 *
 * Clear the pending bit for the specified interrupt. 
 * The interrupt number cannot be a negative value.
 */
static __INLINE void NVIC_ClearPendingIRQ(IRQn_Type IRQn)
{
  NVIC->ICPR[((uint32_t)(IRQn) >> 5)] = (1 << ((uint32_t)(IRQn) & 0x1F)); /* Clear pending interrupt */
}

/**
 * @brief  Read the active bit for an external interrupt
 *
 * @param  IRQn_Type  IRQn is the Number of the interrupt
 * @return uint32_t   1 if active else 0
 *
 * Read the active register in NVIC and returns 1 if its status is active, 
 * otherwise it returns 0.
 */
static __INLINE uint32_t NVIC_GetActive(IRQn_Type IRQn)
{
  return((uint32_t)((NVIC->IABR[(uint32_t)(IRQn) >> 5] & (1 << ((uint32_t)(IRQn) & 0x1F)))?1:0)); /* Return 1 if active else 0 */
}

/**
 * @brief  Set the priority for an interrupt
 *
 * @param  IRQn_Type IRQn is the Number of the interrupt
 * @param  priority is the priority for the interrupt
 * @return none
 *
 * Set the priority for the specified interrupt. The interrupt 
 * number can be positive to specify an external (device specific) 
 * interrupt, or negative to specify an internal (core) interrupt. \n
 *
 * Note: The priority cannot be set for every core interrupt.
 */
static __INLINE void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority)
{
  if(IRQn < 0) {
    SCB->SHP[((uint32_t)(IRQn) & 0xF)-4] = ((priority << (8 - __NVIC_PRIO_BITS)) & 0xff); } /* set Priority for Cortex-M4 System Interrupts */
  else {
    NVIC->IP[(uint32_t)(IRQn)] = ((priority << (8 - __NVIC_PRIO_BITS)) & 0xff);    }        /* set Priority for device specific Interrupts      */
}

/**
 * @brief  Read the priority for an interrupt
 *
 * @param  IRQn_Type IRQn is the Number of the interrupt
 * @return uint32_t  priority is the priority for the interrupt
 *
 * Read the priority for the specified interrupt. The interrupt 
 * number can be positive to specify an external (device specific) 
 * interrupt, or negative to specify an internal (core) interrupt.
 *
 * The returned priority value is automatically aligned to the implemented
 * priority bits of the microcontroller.
 *
 * Note: The priority cannot be set for every core interrupt.
 */
static __INLINE uint32_t NVIC_GetPriority(IRQn_Type IRQn)
{

  if(IRQn < 0) {
    return((uint32_t)(SCB->SHP[((uint32_t)(IRQn) & 0xF)-4] >> (8 - __NVIC_PRIO_BITS)));  } /* get priority for Cortex-M4 system interrupts */
  else {
    return((uint32_t)(NVIC->IP[(uint32_t)(IRQn)]           >> (8 - __NVIC_PRIO_BITS)));  } /* get priority for device specific interrupts  */
}


/**
 * @brief  Encode the priority for an interrupt
 *
 * @param  uint32_t PriorityGroup   is the used priority group
 * @param  uint32_t PreemptPriority is the preemptive priority value (starting from 0)
 * @param  uint32_t SubPriority     is the sub priority value (starting from 0)
 * @return uint32_t                    the priority for the interrupt
 *
 * Encode the priority for an interrupt with the given priority group,
 * preemptive priority value and sub priority value.
 * In case of a conflict between priority grouping and available
 * priority bits (__NVIC_PRIO_BITS) the samllest possible priority group is set.
 *
 * The returned priority value can be used for NVIC_SetPriority(...) function
 */
static __INLINE uint32_t NVIC_EncodePriority (uint32_t PriorityGroup, uint32_t PreemptPriority, uint32_t SubPriority)
{
  uint32_t PriorityGroupTmp = (PriorityGroup & 0x07);                         /* only values 0..7 are used          */
  uint32_t PreemptPriorityBits;
  uint32_t SubPriorityBits;

  PreemptPriorityBits = ((7 - PriorityGroupTmp) > __NVIC_PRIO_BITS) ? __NVIC_PRIO_BITS : 7 - PriorityGroupTmp;
  SubPriorityBits     = ((PriorityGroupTmp + __NVIC_PRIO_BITS) < 7) ? 0 : PriorityGroupTmp - 7 + __NVIC_PRIO_BITS;
 
  return (
           ((PreemptPriority & ((1 << (PreemptPriorityBits)) - 1)) << SubPriorityBits) |
           ((SubPriority     & ((1 << (SubPriorityBits    )) - 1)))
         );
}


/**
 * @brief  Decode the priority of an interrupt
 *
 * @param  uint32_t   Priority       the priority for the interrupt
 * @param  uint32_t   PrioGroup   is the used priority group
 * @param  uint32_t* pPreemptPrio is the preemptive priority value (starting from 0)
 * @param  uint32_t* pSubPrio     is the sub priority value (starting from 0)
 * @return none
 *
 * Decode an interrupt priority value with the given priority group to 
 * preemptive priority value and sub priority value.
 * In case of a conflict between priority grouping and available
 * priority bits (__NVIC_PRIO_BITS) the samllest possible priority group is set.
 *
 * The priority value can be retrieved with NVIC_GetPriority(...) function
 */
static __INLINE void NVIC_DecodePriority (uint32_t Priority, uint32_t PriorityGroup, uint32_t* pPreemptPriority, uint32_t* pSubPriority)
{
  uint32_t PriorityGroupTmp = (PriorityGroup & 0x07);                         /* only values 0..7 are used          */
  uint32_t PreemptPriorityBits;
  uint32_t SubPriorityBits;

  PreemptPriorityBits = ((7 - PriorityGroupTmp) > __NVIC_PRIO_BITS) ? __NVIC_PRIO_BITS : 7 - PriorityGroupTmp;
  SubPriorityBits     = ((PriorityGroupTmp + __NVIC_PRIO_BITS) < 7) ? 0 : PriorityGroupTmp - 7 + __NVIC_PRIO_BITS;
  
  *pPreemptPriority = (Priority >> SubPriorityBits) & ((1 << (PreemptPriorityBits)) - 1);
  *pSubPriority     = (Priority                   ) & ((1 << (SubPriorityBits    )) - 1);
}



/* ##################################    SysTick function  ############################################ */

#if (!defined (__Vendor_SysTickConfig)) || (__Vendor_SysTickConfig == 0)

/* SysTick constants */
#define SYSTICK_ENABLE              0                                          /* Config-Bit to start or stop the SysTick Timer                         */
#define SYSTICK_TICKINT             1                                          /* Config-Bit to enable or disable the SysTick interrupt                 */
#define SYSTICK_CLKSOURCE           2                                          /* Clocksource has the offset 2 in SysTick Control and Status Register   */
#define SYSTICK_MAXCOUNT       ((1<<24) -1)                                    /* SysTick MaxCount                                                      */

/**
 * @brief  Initialize and start the SysTick counter and its interrupt.
 *
 * @param  uint32_t ticks is the number of ticks between two interrupts
 * @return  none
 *
 * Initialise the system tick timer and its interrupt and start the
 * system tick timer / counter in free running mode to generate 
 * periodical interrupts.
 */
static __INLINE uint32_t SysTick_Config(uint32_t ticks)
{ 
  if (ticks > SYSTICK_MAXCOUNT)  return (1);                                             /* Reload value impossible */

  SysTick->LOAD  =  (ticks & SYSTICK_MAXCOUNT) - 1;                                      /* set reload register */
  NVIC_SetPriority (SysTick_IRQn, (1<<__NVIC_PRIO_BITS) - 1);                            /* set Priority for Cortex-M0 System Interrupts */
  SysTick->VAL   =  (0x00);                                                              /* Load the SysTick Counter Value */
  SysTick->CTRL = (1 << SYSTICK_CLKSOURCE) | (1<<SYSTICK_ENABLE) | (1<<SYSTICK_TICKINT); /* Enable SysTick IRQ and SysTick Timer */
  return (0);                                                                            /* Function successful */
}

#endif





/* ##################################    Reset function  ############################################ */

/**
 * @brief  Initiate a system reset request.
 *
 * @param   none
 * @return  none
 *
 * Initialize a system reset request to reset the MCU
 */
static __INLINE void NVIC_SystemReset(void)
{
  SCB->AIRCR  = (NVIC_AIRCR_VECTKEY | (SCB->AIRCR & (0x700)) | (1<<NVIC_SYSRESETREQ)); /* Keep priority group unchanged */
  __DSB();                                                                             /* Ensure completion of memory access */              
  while(1);                                                                            /* wait until reset */
}


/* ##################################    Debug Output  function  ############################################ */


/**
 * @brief  Outputs a character via the ITM channel 0
 *
 * @param   uint32_t character to output
 * @return  uint32_t input character
 *
 * The function outputs a character via the ITM channel 0. 
 * The function returns when no debugger is connected that has booked the output.  
 * It is blocking when a debugger is connected, but the previous character send is not transmitted. 
 */
static __INLINE uint32_t ITM_SendChar (uint32_t ch)
{
  if (ch == '\n') ITM_SendChar('\r');
  
  if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA)  &&
      (ITM->TCR & ITM_TCR_ITMENA)                  &&
      (ITM->TER & (1UL << 0))  ) 
  {
    while (ITM->PORT[0].u32 == 0);
    ITM->PORT[0].u8 = (uint8_t) ch;
  }  
  return (ch);
}



/*@} end of CMSIS_core_DebugFunctions */

#endif /* __CORE_CM4_H_DEPENDANT */

#endif /* __CMSIS_GENERIC */

#ifdef __cplusplus
}
#endif
