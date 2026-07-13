/**
  ******************************************************************************
  * @file    system_ft32f4xx.c
  * @author  Betaflight FT32 Port
  * @brief   CMSIS Cortex-M4 Device System Source File for FT32F4xx devices.
  *
  * Clock tree initialization: HSE/HSE-bypass/HSI -> PLL -> 210MHz SYSCLK.
  * FT32 PLL register layout uses separate output enable bits (PLLPEN/PLLQEN/PLLREN)
  * and a 3-bit P divider field. The 48MHz USB clock comes from the dedicated HSI48
  * oscillator. There is no PWR over-drive or PLLSAI on this device.
  ******************************************************************************
  */

#include <string.h>

#include "platform.h"
#include "drivers/system.h"
#include "drivers/persistent.h"

#define VECT_TAB_SRAM

// Convenience aliases for PLL output enable bits
#define PLL_OUTPUT_ENABLE_P    RCC_PLLCFGR_PLLPEN
#define PLL_OUTPUT_ENABLE_Q    RCC_PLLCFGR_PLLQEN
#define PLL_OUTPUT_ENABLE_R    RCC_PLLCFGR_PLLREN

// AHB prescaler table: index = HPRE[3:0], value = log2(divisor)
__I uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

// Global clock variables
uint32_t SystemCoreClock;
uint32_t pll_src, pll_input, pll_m, pll_p, pll_n, pll_q;

// SystemSYSCLKSource
// 0: HSI
// 1: HSE
// 2: PLLP

int SystemSYSCLKSource(void)
{
    return (RCC->CFGR & RCC_CFGR_SWS) >> 2;
}

// SystemPLLSource
// 0: HSI
// 1: HSE

int SystemPLLSource(void)
{
    return (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) >> 14;
}

// Overclocking configuration table.
// PLLN values assume pll_input = 1 MHz.
// When pll_input = 2 MHz, pll_n = table_n / 2.
//
// VCO must stay within 96-500 MHz.
// PLLP register field: 001 = /2, 010 = /3, ..., 111 = /8.
// (000 is reserved; 001 is the explicit /2 encoding.)
// PLLQ = 0 means "use HSI48 for USB 48 MHz instead of PLLQ".

typedef struct pllConfig_s {
    uint16_t mhz;   // target SYSCLK in MHz
    uint16_t n;     // PLLN for 1 MHz input (VCO = 1 MHz * n)
    uint16_t p;     // PLLP actual divider (2..8)
    uint16_t q;     // PLLQ actual divider, 0 = disabled
} pllConfig_t;

static const pllConfig_t overclockLevels[] = {
    { 210, 420, 2,  0 },  // Default/reset-cleared level, USB from HSI48
    { 180, 360, 2,  0 },  // 180 MHz, USB from HSI48
    { 192, 384, 2,  8 },  // 192 MHz, USB = 48 MHz from PLLQ (384/8)
    { 210, 420, 2,  0 },  // Legacy persisted default index, USB from HSI48
    { 216, 432, 2,  9 },  // 216 MHz, USB = 48 MHz from PLLQ (432/9)
    { 240, 480, 2, 10 },  // 240 MHz, USB = 48 MHz from PLLQ (480/10)
    { 168, 336, 2,  7 },  // 168 MHz, USB = 48 MHz from PLLQ (336/7)
};

#define DEFAULT_OVERCLOCK_LEVEL 0

// Encode PLLP divider to register field value.
// Register encoding: 001 = /2, 010 = /3, 011 = /4, ..., 111 = /8.
// (000 is reserved per the datasheet, so use 001 for /2.)
static uint32_t encodePllP(uint32_t div)
{
    if (div <= 2)
        return 1;
    return div - 1;
}

// Encode PLLQ/PLLR divider to register field value.
// Register encoding: 0001 = /2, 0010 = /3, ..., 1111 = /16.
// (0000 is reserved per the datasheet, so use 0001 for /2.)
static uint32_t encodePllQ(uint32_t div)
{
    if (div <= 2)
        return 1;
    return div - 1;
}

// Get Flash wait states for a given SYSCLK frequency in MHz.
static uint32_t getFlashLatency(uint32_t freqMhz)
{
    if (freqMhz <= 30)  return 0;
    if (freqMhz <= 60)  return 1;
    if (freqMhz <= 90)  return 2;
    if (freqMhz <= 120) return 3;
    if (freqMhz <= 150) return 4;
    if (freqMhz <= 180) return 5;
    if (freqMhz <= 210) return 6;
    return 7;
}

static void SystemInitPLLParameters(void)
{
    uint32_t currentOverclockLevel = persistentObjectRead(PERSISTENT_OBJECT_OVERCLOCK_LEVEL);

    if (currentOverclockLevel >= ARRAYLEN(overclockLevels)) {
        currentOverclockLevel = DEFAULT_OVERCLOCK_LEVEL;
    }

    const pllConfig_t * const pll = overclockLevels + currentOverclockLevel;

    pll_n = pll->n / pll_input;
    pll_p = pll->p;
    pll_q = pll->q;
}

void OverclockRebootIfNecessary(uint32_t targetMhz)
{
    if (targetMhz == 0) {
        targetMhz = overclockLevels[DEFAULT_OVERCLOCK_LEVEL].mhz;
    }

    for (unsigned i = 0; i < ARRAYLEN(overclockLevels); i++) {
        if (overclockLevels[i].mhz == targetMhz) {
            if (SystemCoreClock != targetMhz * 1000000U) {
                persistentObjectWrite(PERSISTENT_OBJECT_OVERCLOCK_LEVEL, i);
                __disable_irq();
                NVIC_SystemReset();
            }
            return;
        }
    }
}

void systemClockSetHSEValue(uint32_t frequency)
{
    uint32_t hse_value = persistentObjectRead(PERSISTENT_OBJECT_HSE_VALUE);

    if (hse_value != frequency) {
        persistentObjectWrite(PERSISTENT_OBJECT_HSE_VALUE, frequency);
        __disable_irq();
        NVIC_SystemReset();
    }
}

void SystemInit(void)
{
    initialiseMemorySections();

    // FPU settings
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));
#endif

    // Reset RCC to default state
    RCC->CR |= (uint32_t)0x00000001;     // HSION

    // Reset CFGR: clear SW, HPRE, PPRE1, PPRE2, MCO
    RCC->CFGR = 0x00000000;

    // Reset HSEON, CSSON and PLLON
    RCC->CR &= (uint32_t)0xFEF6FFFF;

    // Reset PLLCFGR and PLL2CFGR
    RCC->PLLCFGR = 0x00000000;
    RCC->PLL2CFGR = 0x00000000;

    // Reset HSEBYP
    RCC->CR &= (uint32_t)0xFFFBFFFF;

    // Disable all interrupts
    RCC->CIR = 0x00000000;

    // Vector table relocation
#ifdef VECT_TAB_SRAM
    extern uint8_t isr_vector_table_flash_base;
    extern uint8_t isr_vector_table_base;
    extern uint8_t isr_vector_table_end;

    memcpy(&isr_vector_table_base, &isr_vector_table_flash_base,
           &isr_vector_table_end - &isr_vector_table_base);
    SCB->VTOR = (uint32_t)&isr_vector_table_base;
#else
    extern uint8_t isr_vector_table_flash_base;
    SCB->VTOR = (uint32_t)&isr_vector_table_flash_base;
#endif

#ifdef USE_HAL_DRIVER
    HAL_Init();
#endif

    // Set SystemCoreClock to HSI (16 MHz default after reset).
    // Full clock configuration (SetSysClock) is deferred to systemInit()
    // so persistent objects (HSE_VALUE, OVERCLOCK_LEVEL) are available.
    SystemCoreClockUpdate();
}

void SystemCoreClockUpdate(void)
{
    uint32_t hse_value = persistentObjectRead(PERSISTENT_OBJECT_HSE_VALUE);
    uint32_t tmp = 0, pllvco = 0, pllsource = 0, pllm = 0;
    uint32_t pllp_val = 0, pllp_div = 2;

    // Get SYSCLK source from SWS[1:0]
    tmp = RCC->CFGR & RCC_CFGR_SWS;

    switch (tmp) {
    case 0x00:  // HSI
        SystemCoreClock = HSI_VALUE;
        break;
    case 0x04:  // HSE
        SystemCoreClock = hse_value;
        break;
    case 0x08:  // PLLP
        // VCO = (PLL_input / PLLM) * PLLN
        // SYSCLK = VCO / PLLP_div
        pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) >> 14;
        pllm = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;

        if (pllsource != 0) {
            pllvco = (hse_value / pllm) * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6);
        } else {
            pllvco = (HSI_VALUE / pllm) * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 6);
        }

        // PLLP register field: 000/001 = /2, 010 = /3, ..., 111 = /8
        pllp_val = (RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >> 17;
        if (pllp_val <= 1) {
            pllp_div = 2;
        } else {
            pllp_div = pllp_val + 1;
        }

        SystemCoreClock = pllvco / pllp_div;
        break;
    default:
        SystemCoreClock = HSI_VALUE;
        break;
    }

    // Apply AHB prescaler
    tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4)];
    SystemCoreClock >>= tmp;
}

// Start HSI/HSE oscillator and wait for ready.
// Returns 1 on success, 0 on timeout.
static int StartHSx(uint32_t onBit, uint32_t readyBit, int maxWaitCount)
{
    RCC->CR |= onBit;
    for (int waitCounter = 0; waitCounter < maxWaitCount; waitCounter++) {
        if (RCC->CR & readyBit) {
            return 1;
        }
    }
    return 0;
}

// Configure system clock: HSE/HSI -> PLL -> SYSCLK.
// Called from systemInit() after persistentObjectInit() is available.
void SetSysClock(void)
{
    uint32_t hse_value = persistentObjectRead(PERSISTENT_OBJECT_HSE_VALUE);
    uint32_t hse_mhz = hse_value / 1000000;

    // Switch to HSI during configuration
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);

    // Determine PLL source and input frequency.
    // Target: pll_input = 2 MHz for best flexibility (or 1 MHz for odd HSE values).
    if (hse_value == 0) {
        // HSE unknown or not present; use HSI as PLL source
        if (!StartHSx(RCC_CR_HSION, RCC_CR_HSIRDY, 5000)) {
            return;
        }

        pll_src = RCC_PLLCFGR_PLLSRC_HSI;
        // HSI = 16 MHz, PLLM = 8 -> pll_input = 2 MHz
        pll_m = 8;
        pll_input = 2;
    } else {
        // HSE is available
        if (!StartHSx(RCC_CR_HSEON, RCC_CR_HSERDY, 5000)) {
            // HSE failed; fall back to HSI
            pll_src = RCC_PLLCFGR_PLLSRC_HSI;
            pll_m = 8;
            pll_input = 2;
        } else {
            pll_src = RCC_PLLCFGR_PLLSRC_HSE;

            // Target 2 MHz VCO input. PLLM must be 1-24 and
            // VCO input must be 2-16 MHz. Fall back to HSI
            // for HSE frequencies that cannot meet both constraints.
            pll_m = hse_mhz / 2;
            if (pll_m * 2 != hse_mhz || pll_m > 24) {
                pll_src = RCC_PLLCFGR_PLLSRC_HSI;
                pll_m = 8;
                pll_input = 2;
            } else {
                pll_input = hse_mhz / pll_m;
            }
        }
    }

    SystemInitPLLParameters();

    // Bus dividers:
    // HCLK = SYSCLK / 1
    // PCLK2 = HCLK / 2    (APB2, max = SYSCLK/2)
    // PCLK1 = HCLK / 4    (APB1, max = SYSCLK/4)

    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;

    // Configure main PLL.
    // PLLCFGR layout:
    //   bits[4:0]   = PLLM
    //   bits[13:6]  = PLLN
    //   bit  14     = PLLSRC
    //   bit  16     = PLLPEN
    //   bits[19:17] = PLLP
    //   bit  20     = PLLQEN
    //   bits[24:21] = PLLQ
    //   bit  25     = PLLREN
    //   bits[28:26] = PLLR
    {
        uint32_t pllcfgr = pll_m
                         | (pll_n << 6)
                         | pll_src
                         | PLL_OUTPUT_ENABLE_P
                         | (encodePllP(pll_p) << 17);

        if (pll_q > 0) {
            pllcfgr |= PLL_OUTPUT_ENABLE_Q | (encodePllQ(pll_q) << 21);
        }

        RCC->PLLCFGR = pllcfgr;
    }

    // Enable main PLL
    RCC->CR |= RCC_CR_PLLON;

    // Wait till PLL is ready
    while ((RCC->CR & RCC_CR_PLLRDY) == 0);

    // Configure Flash wait states and prefetch buffer.
    // Prefetch: FLASH_RDC_PRFTBE (bit 4).
    // Wait states: FLASH_RDC_LATENCY (bits [3:0]).
    FLASH->RDC = FLASH_RDC_PRFTBE | getFlashLatency((pll_n * pll_input) / pll_p);

    // Select PLL as system clock
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    // Wait until PLL is used as system clock
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    // Enable HSI48 for USB OTG FS / RNG / SDIO 48 MHz clock.
    // The FT32 has a dedicated internal 48 MHz RC oscillator (HSI48).
    RCC->CR2 |= RCC_CR2_HSI48ON;
    while ((RCC->CR2 & RCC_CR2_HSI48RDY) == 0);

    // Select HSI48 as the 48 MHz clock source for OTG_FS / SDIO / RNG
    // (CLK48SEL = 0 selects HSI48; = 1 would select PLLQ)
    RCC->CCIPR &= ~RCC_CCIPR_CLK48SEL;

    // Configure PLL2 for I2S audio clock.
    // PLL2 is the secondary PLL (no P output, only Q and R).
    // I2S target: ~108 MHz VCO for flexible audio clock generation.
#define PLLI2S_TARGET_FREQ_MHZ (27 * 4)
#define PLLI2S_R               2

    uint32_t plli2s_n = (PLLI2S_TARGET_FREQ_MHZ * PLLI2S_R) / pll_input;

    // PLL2CFGR has the same layout as PLLCFGR but bits[19:15] are reserved.
    {
        uint32_t pll2cfgr = pll_m                       // same input divider
                          | (plli2s_n << 6)              // PLL2N
                          | pll_src                      // same source
                          | PLL_OUTPUT_ENABLE_R          // enable PLL2R
                          | (encodePllP(PLLI2S_R) << 26); // PLL2R divider

        RCC->PLL2CFGR = pll2cfgr;
    }

    // Enable PLL2
    RCC->CR |= RCC_CR_PLL2ON;

    // Wait till PLL2 is ready
    while ((RCC->CR & RCC_CR_PLL2RDY) == 0);

    SystemCoreClockUpdate();
}
