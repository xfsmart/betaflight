/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Betaflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#ifdef USE_ADC

#include "build/debug.h"

#include "drivers/dma_reqmap.h"
#include "platform/dma.h"
#include "drivers/io.h"
#include "drivers/io_impl.h"
#include "platform/rcc.h"
#include "drivers/dma.h"
#include "drivers/sensor.h"
#include "drivers/adc.h"
#include "drivers/time.h"
#include "platform/adc_impl.h"
#include "pg/adc.h"

// FT32F4 calibration data addresses
#define VREFINT_CAL_ADDR  0x1FFF0A18  // VREFINT calibration at 25 C, VDDA = 3.3V
#define TS_CAL1_ADDR      0x1FFF0A1C  // Temperature sensor calibration at 25 C (+/-5 C)

#define ADC_ENABLE_TIMEOUT_US  1000
#define ADC_INTERNAL_CONVERSION_TIMEOUT_US  1000
#define ADC_REGULATOR_STARTUP_US  20

const adcDevice_t adcHardware[] = {
    {
        .ADCx = ADC1,
        .rccADC = RCC_APB2Periph_ADC,
    },
    {
        .ADCx = ADC2,
        .rccADC = RCC_APB2Periph_ADC,
    },
    {
        .ADCx = ADC3,
        .rccADC = RCC_APB2Periph_ADC,
    },
};

const adcTagMap_t adcTagMap[] = {
    { DEFIO_TAG_E__PC0, ADC_DEVICES_123, ADC_CHANNEL_1  },
    { DEFIO_TAG_E__PC1, ADC_DEVICES_123, ADC_CHANNEL_2  },
    { DEFIO_TAG_E__PC2, ADC_DEVICES_123, ADC_CHANNEL_3  },
    { DEFIO_TAG_E__PC3, ADC_DEVICES_123, ADC_CHANNEL_4  },
    { DEFIO_TAG_E__PA0, ADC_DEVICES_12,  ADC_CHANNEL_5  },
    { DEFIO_TAG_E__PA1, ADC_DEVICES_12,  ADC_CHANNEL_6  },
    { DEFIO_TAG_E__PA2, ADC_DEVICES_12,  ADC_CHANNEL_7  },
    { DEFIO_TAG_E__PA3, ADC_DEVICES_12,  ADC_CHANNEL_8  },
    { DEFIO_TAG_E__PA4, ADC_DEVICES_12,  ADC_CHANNEL_9  },
    { DEFIO_TAG_E__PA5, ADC_DEVICES_12,  ADC_CHANNEL_10 },
    { DEFIO_TAG_E__PA6, ADC_DEVICES_12,  ADC_CHANNEL_11 },
    { DEFIO_TAG_E__PA7, ADC_DEVICES_12,  ADC_CHANNEL_12 },
    { DEFIO_TAG_E__PC4, ADC_DEVICES_12,  ADC_CHANNEL_13 },
    { DEFIO_TAG_E__PC5, ADC_DEVICES_12,  ADC_CHANNEL_14 },
    { DEFIO_TAG_E__PB0, ADC_DEVICES_12,  ADC_CHANNEL_15 },
    { DEFIO_TAG_E__PB1, ADC_DEVICES_12,  ADC_CHANNEL_16 },
};

static void adcInitDevice(ADC_TypeDef *adcdev, int channelCount)
{
    ADC_InitTypeDef ADC_InitStructure = { 0 };

    ADC_StructInit(&ADC_InitStructure);

    ADC_InitStructure.Resolution               = ADC_RESOLUTION_12B;
    ADC_InitStructure.ContinuousConvMode       = ENABLE;
    ADC_InitStructure.ExternalTrigConv         = ADC12_EXTERNALTRIG_TIM1_CC1;
    ADC_InitStructure.ExternalTrigConvEdge     = ADC_SOFTWARE_START;
    ADC_InitStructure.DataAlign                = ADC_DATAALIGN_RIGHT;
    ADC_InitStructure.NbrOfConversion          = channelCount;
    ADC_InitStructure.GainCompensation         = 0;
    ADC_InitStructure.DMAMode                  = ADC_DMAMODE_CIRCULAR;

    ADC_Init(adcdev, &ADC_InitStructure);
}

static void adcPrepare(ADC_TypeDef *adcdev)
{
    ADC_DeepPWDModeCmd(adcdev, DISABLE);
    ADC_InternalRegulatorCmd(adcdev, ENABLE);
    delayMicroseconds(ADC_REGULATOR_STARTUP_US);
    ADC_StartSingleCalibration(adcdev);
}

static void adcEnableReady(ADC_TypeDef *adcdev)
{
    ADC_ClearFlag(adcdev, ADC_FLAG_ADRDY);
    ADC_Cmd(adcdev, ENABLE);

    const timeUs_t startTime = microsISR();
    while (ADC_GetFlagStatus(adcdev, ADC_FLAG_ADRDY) == RESET) {
        if (cmpTimeUs(microsISR(), startTime) > ADC_ENABLE_TIMEOUT_US) {
            break;
        }
    }
}

#ifdef USE_ADC_INTERNAL
static void adcInitInternalInjected(const adcConfig_t *config)
{
    ADC_TempSensorCmd(ENABLE);
    ADC_VrefintCmd(ENABLE);
    ADC_INJ_DiscModeCmd(ADC1, DISABLE);
    ADC_AutoInjectedModeCmd(ADC1, DISABLE);

    ADC_InjectedConfTypeDef InjectedConfig = {
        .InjectedNbrOfConversion = 2,
        .InjectedSamplingTime = ADC_SAMPLETIME_640_5CYCLES,
        .InjectedSingleDiff = ADC_SINGLE_ENDED,
        .InjectedOffsetNumber = ADC_OFFSET_NONE,
        .InjectedOffset = 0,
        .InjectedOffsetSign = 0,
        .InjectedOffsetSaturation = DISABLE,
        .ExternalTrigInjecConvEdge = 0,
        .ExternalTrigInjecConv = ADC_INJECTED_SOFTWARE_START,
    };

    InjectedConfig.InjectedChannel = ADC1_CHANNEL_VREFINT;
    InjectedConfig.InjectedRank = ADC_INJECTED_RANK_1;
    ADC_InjectedChannelConfig(ADC1, &InjectedConfig);

    InjectedConfig.InjectedChannel = ADC13_CHANNEL_TENOSENSOR;
    InjectedConfig.InjectedRank = ADC_INJECTED_RANK_2;
    ADC_InjectedChannelConfig(ADC1, &InjectedConfig);

    adcVREFINTCAL = config->vrefIntCalibration ? config->vrefIntCalibration : *(uint16_t *)VREFINT_CAL_ADDR;
    adcTSCAL1 = config->tempSensorCalibration1 ? config->tempSensorCalibration1 : *(uint16_t *)TS_CAL1_ADDR;

    // Temperature sensor parameters: V25 = 0.76V, Avg_Slope = 2.6mV/C
    // Negative tempco: VSENSE decreases as temperature increases
    // adcTSSlopeK unit: 0.001 C/count
    // Calculation: -1000 / (2.6 / (3300/4096)) = -1000 / 3.227 = -309.9 ~= -310
    adcTSSlopeK = -310;  // -0.310 C per ADC count

    adcTSCAL2 = adcTSCAL1;  // Single calibration point at 25 C
}

// Sampling time for temperature sensor and vrefint:
// Both sources require minimum sample time of 10us.
// HCLK = 210MHz, ADC clock = 26.25MHz (prescaler = 8)
// tcycle = 1/26.25MHz = 0.038us, 10us = 262 cycles
// Max sample time: 640.5 cycles = 24.4us (meets 10us minimum requirement)

static bool adcInternalConversionInProgress = false;
static timeUs_t adcInternalConversionStartUs;

bool adcInternalIsBusy(void)
{
    if (adcInternalConversionInProgress) {
        if (ADC_GetFlagStatus(ADC1, ADC_FLAG_JEOS) != RESET) {
            adcInternalConversionInProgress = false;
        } else if (cmpTimeUs(microsISR(), adcInternalConversionStartUs) > ADC_INTERNAL_CONVERSION_TIMEOUT_US) {
            ADC_INJ_StopOfConversion(ADC1);
            adcInternalConversionInProgress = false;
        }
    }

    return adcInternalConversionInProgress;
}

void adcInternalStartConversion(void)
{
    ADC_ClearFlag(ADC1, ADC_FLAG_JEOC | ADC_FLAG_JEOS);
    ADC_INJ_StartOfConversion(ADC1);

    adcInternalConversionStartUs = microsISR();
    adcInternalConversionInProgress = true;
}

uint16_t adcInternalRead(adcSource_e source)
{
    switch (source) {
    case ADC_VREFINT:
        return ADC_INJ_GetConversionValue(ADC1, ADC_INJECTED_RANK_1);
    case ADC_TEMPSENSOR:
        return ADC_INJ_GetConversionValue(ADC1, ADC_INJECTED_RANK_2);
    default:
        return 0;
    }
}
#endif

void adcInit(const adcConfig_t *config)
{
    uint8_t i;
    uint8_t configuredAdcChannels = 0;

    memset(adcOperatingConfig, 0, sizeof(adcOperatingConfig));

    if (config->vbat.enabled) {
        adcOperatingConfig[ADC_BATTERY].tag = config->vbat.ioTag;
    }

    if (config->rssi.enabled) {
        adcOperatingConfig[ADC_RSSI].tag = config->rssi.ioTag;
    }

    if (config->external1.enabled) {
        adcOperatingConfig[ADC_EXTERNAL1].tag = config->external1.ioTag;
    }

    if (config->current.enabled) {
        adcOperatingConfig[ADC_CURRENT].tag = config->current.ioTag;
    }

    adcDevice_e device = ADC_CFG_TO_DEV(config->device);

    if (device == ADCINVALID) {
        return;
    }

    adcDevice_t adc = adcHardware[device];

    bool adcActive = false;
    for (int i = 0; i < ADC_SOURCE_COUNT; i++) {
        if (!adcVerifyPin(adcOperatingConfig[i].tag, device)) {
            continue;
        }

        adcActive = true;
        IOInit(IOGetByTag(adcOperatingConfig[i].tag), OWNER_ADC_BATT + i, 0);
        IOConfigGPIO(IOGetByTag(adcOperatingConfig[i].tag), IO_CONFIG(GPIO_Mode_AN, 0, GPIO_OType_OD, GPIO_PuPd_NOPULL));
        adcOperatingConfig[i].adcChannel = adcChannelByTag(adcOperatingConfig[i].tag);
        adcOperatingConfig[i].dmaIndex = configuredAdcChannels++;
        adcOperatingConfig[i].sampleTime = ADC_SAMPLETIME_640_5CYCLES;  // 640.5 cycles = 24.4us @ 26.25MHz (meets 10us minimum for temp sensor)
        adcOperatingConfig[i].enabled = true;
    }

#ifndef USE_ADC_INTERNAL
    if (!adcActive) {
        return;
    }
#endif

    RCC_APB2PeriphClockCmd(adc.rccADC, ENABLE);

    // Configure ADC common parameters
    // ADC clock: HCLK = 210MHz, DIV8 = 26.25MHz
    RCC_ADCCLKConfig(RCC_ADCCLK_SYSCLK);
    ADC_ClockModeConfig(ADC_CLOCK_ASYNC_DIV8);

    // Multi-ADC mode: independent mode, DMA disabled, 5 cycles sampling delay
    ADC_MultiModeTypeDef multiModeConfig = {
        .Mode = ADC_MODE_INDEPENDENT,
        .DMAAccessMode = ADC_DMAACCESSMODE_DISABLED,
        .DMAMode = ADC_DMAMODE_ONESHOT,
        .TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_5CYCLES,
    };
    ADC_MultiModeConfig(&multiModeConfig);

#ifdef USE_ADC_INTERNAL
    if (device != ADCDEV_1 || !adcActive) {
        RCC_APB2PeriphClockCmd(adcHardware[ADCDEV_1].rccADC, ENABLE);
        adcPrepare(ADC1);
        adcInitDevice(ADC1, 2);
        adcInitInternalInjected(config);
        adcEnableReady(ADC1);
    }

    adcOperatingConfig[ADC_VREFINT].enabled = true;
    adcOperatingConfig[ADC_TEMPSENSOR].enabled = true;

    if (!adcActive) {
        return;
    }
#endif

    adcPrepare(adc.ADCx);
    adcInitDevice(adc.ADCx, configuredAdcChannels);
    ADC_DMACmd(adc.ADCx, ENABLE);

#ifdef USE_ADC_INTERNAL
    if (device == ADCDEV_1) {
        adcInitInternalInjected(config);
    }
#endif

    uint32_t rank = ADC_REGULAR_RANK_1;
    for (i = 0; i < ADC_EXTERNAL_COUNT; i++) {
        if (!adcOperatingConfig[i].enabled) {
            continue;
        }
        
        ADC_ChannelConfTypeDef channelConfig = {
            .Channel = adcOperatingConfig[i].adcChannel,
            .Rank = rank++,
            .SamplingTime = adcOperatingConfig[i].sampleTime,
            .SingleDiff = ADC_SINGLE_ENDED,
            .OffsetNumber = ADC_OFFSET_NONE,
            .Offset = 0,
            .OffsetSign = 0,
            .OffsetSaturation = DISABLE,
        };
        
        ADC_RegularChannelConfig(adc.ADCx, &channelConfig);
    }

    const dmaChannelSpec_t *dmaSpec = dmaGetChannelSpecByPeripheral(DMA_PERIPH_ADC, device, config->dmaopt[device]);

    if (!dmaSpec || !dmaAllocate(dmaGetIdentifier(dmaSpec->ref), OWNER_ADC, RESOURCE_INDEX(device))) {
        return;
    }

    dmaEnable(dmaGetIdentifier(dmaSpec->ref));

    xDMA_DeInit(dmaSpec->ref);

    DMA_InitTypeDef DMA_InitStructure;

    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA;
    DMA_InitStructure.SrcDstMasterSel = DMA_SRCMASTER1_DSTMASTER2;
    DMA_InitStructure.SrcAddress = (uint32_t)&adc.ADCx->DR;
    DMA_InitStructure.DstAddress = (uint32_t)adcValues;
    DMA_InitStructure.BlockTransSize = configuredAdcChannels;
    DMA_InitStructure.SrcAddrMode = DMA_SRC_ADDRMODE_HOLD;
    DMA_InitStructure.DstAddrMode = DMA_DST_ADDRMODE_INC;
    DMA_InitStructure.ReloadDst = ENABLE;
    DMA_InitStructure.SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_16BITS;
    DMA_InitStructure.DstTransferWidth = DMA_DST_TRANSFERWIDTH_16BITS;

    // Configure hardware handshaking interface for ADC peripheral
    // SrcHardwareInterface: DMA channel number (0-7)
    // SrcHsIfPeriphSel: Peripheral request number within the channel
    DMA_InitStructure.SrcHardwareInterface = DMA_CODE_STREAM(dmaSpec->code);
    DMA_InitStructure.SrcHsIfPeriphSel = dmaSpec->channel;
    DMA_InitStructure.Priority = DMA_CH_PRIORITY_7;  // Highest priority

    xDMA_Init(dmaSpec->ref, &DMA_InitStructure);
    xDMA_Cmd(dmaSpec->ref, ENABLE);

    adcEnableReady(adc.ADCx);

    // Start conversions
    ADC_REG_StartOfConversion(adc.ADCx);

#ifdef USE_ADC_INTERNAL
    adcInternalStartConversion();
#endif
}

void adcGetChannelValues(void)
{
}
#endif
