/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

#include "platform.h"

#ifdef USE_DMA_SPEC

#include "timer_def.h"
#include "platform/adc_impl.h"
#include "drivers/bus_spi.h"
#include "drivers/dma_reqmap.h"
#include "platform/dma.h"
#include "drivers/serial.h"
#include "drivers/serial_uart.h"
#include "drivers/serial_uart_impl.h"

#include "pg/timerio.h"

/*
 * FT32F4 DMA 外设映射
 *
 * Citation: spec.json -> channel_mapping
 * - DMA1: RM V1.00 表 10-1
 * - DMA2: RM V1.00 表 10-2
 */

// DMA 通道定义宏（本地版本，只在当前文件有效）
// FT32 使用 DMA1_Channel0..DMA1_Channel7 格式，channel 字段存储通道号 (0-7)
#define DMA(d, c) { DMA_CODE(d, c, 0), (dmaResource_t *)DMA ## d ## _Channel ## c, 0 }

typedef struct dmaPeripheralMapping_s {
    dmaPeripheral_e device;
    uint8_t index;
    dmaChannelSpec_t channelSpec[MAX_PERIPHERAL_DMA_OPTIONS];
} dmaPeripheralMapping_t;

typedef struct dmaTimerMapping_s {
    timerResource_t *tim;
    uint8_t channel;
    dmaChannelSpec_t channelSpec[MAX_TIMER_DMA_OPTIONS];
} dmaTimerMapping_t;

// DMA 通道定义宏（本地版本，使用不同名称避免与 dma_reqmap_mcu.h 冲突）
// FT32 使用 DMA1_Channel0..DMA1_Channel7 格式，channel 字段存储通道号 (0-7)
#define FT32_DMA(d, s, c) { DMA_CODE(d, s, c), (dmaResource_t *)DMA ## d ## _Channel ## s, (c) }

/*
 * 外设 DMA 映射表
 * 
 * 基于 spec.json channel_mapping 字段：
 * - DMA1 Channel_4: UART5_RX, USART3_RX, TIM3_CH4, TIM3_CH1
 * - DMA1 Channel_5: UART7_TX, UART7_RX, TIM3_CH2, TIM3_UP, TIM3_TRIG, TIM5_CH3, TIM5_CH4
 * - DMA1 Channel_6: TIM5_CH1, TIM5_CH2, TIM5_UP, TIM5_TRIG
 * - DMA1 Channel_7: TIM6_UP, I2C2_RX, USART3_TX, DAC1, DAC2, I2C2_TX
 * - DMA2 Channel_0: ADC1, AC97_TX0, TIM8_CH1, TIM1_CH1, TIM8_CH2, AC97_RX0, TIM1_CH2
 * - DMA2 Channel_1: ADC2, AC97_RX1, QSPI_TX, QSPI_RX
 * - DMA2 Channel_2: ADC3
 * - DMA2 Channel_3: SPI1_RX, SPI2_RX, SPI1_TX, SPI2_TX
 * - DMA2 Channel_4: USART1_RX, SDIO
 * - DMA2 Channel_5: SPDIF_CB, USART6_RX, USART6_TX
 * - DMA2 Channel_6: TIM1_TRIG, TIM1_CH1, TIM1_CH2, TIM1_UP, TIM1_CH3, TIM1_COM, USART1_TX
 * - DMA2 Channel_7: TIM8_UP, TIM8_CH1, TIM8_CH2, TIM8_CH3, TIM8_TRIG, TIM8_COM
 */
static const dmaPeripheralMapping_t dmaPeripheralMapping[] = {
#ifdef USE_SPI
    // SPI1: DMA2 Channel_3 (SPI1_RX), DMA2 Channel_3 (SPI1_TX)
    // Citation: spec.json -> channel_mapping.DMA2.Channel_3
    { DMA_PERIPH_SPI_SDO,  SPIDEV_1,  { FT32_DMA(2, 3, 3), FT32_DMA(2, 3, 3) } },
    { DMA_PERIPH_SPI_SDI,  SPIDEV_1,  { FT32_DMA(2, 3, 3), FT32_DMA(2, 3, 3) } },
    
    // SPI2: DMA1 Channel 6 (TX), Channel 1 (RX) - RM V1.00 表 10-1, 页码 189
    // Citation: spi_dma_mapping.json -> mapping.SPI2
    { DMA_PERIPH_SPI_SDO,  SPIDEV_2,  { FT32_DMA(1, 6, 3) } },
    { DMA_PERIPH_SPI_SDI,  SPIDEV_2,  { FT32_DMA(1, 1, 3) } },
    
    // SPI3: DMA1 Channel 5 (TX), Channel 0 (RX) - RM V1.00 表 10-1, 页码 189
    // Citation: spi_dma_mapping.json -> mapping.SPI3
    { DMA_PERIPH_SPI_SDO,  SPIDEV_3,  { FT32_DMA(1, 5, 0) } },
    { DMA_PERIPH_SPI_SDI,  SPIDEV_3,  { FT32_DMA(1, 0, 0) } },
#endif // USE_SPI

#ifdef USE_ADC
    // ADC1: DMA2 Channel_0
    // Citation: spec.json -> channel_mapping.DMA2.Channel_0
    { DMA_PERIPH_ADC,     ADCDEV_1,  { FT32_DMA(2, 0, 0), FT32_DMA(2, 0, 0) } },
    
    // ADC2: DMA2 Channel_1
    // Citation: spec.json -> channel_mapping.DMA2.Channel_1
    { DMA_PERIPH_ADC,     ADCDEV_2,  { FT32_DMA(2, 1, 1), FT32_DMA(2, 1, 1) } },
    
    // ADC3: DMA2 Channel_2
    // Citation: spec.json -> channel_mapping.DMA2.Channel_2
    { DMA_PERIPH_ADC,     ADCDEV_3,  { FT32_DMA(2, 2, 2), FT32_DMA(2, 2, 2) } },
#endif

#ifdef USE_SDCARD_SDIO
    // SDIO: DMA2 Channel_4
    // Citation: spec.json -> channel_mapping.DMA2.Channel_4
    { DMA_PERIPH_SDIO,    0,         { FT32_DMA(2, 4, 4), FT32_DMA(2, 4, 4) } },
#endif

#ifdef USE_UART1
    // USART1_TX: DMA2 Channel_6
    // USART1_RX: DMA2 Channel_4
    // Citation: spec.json -> channel_mapping.DMA2.Channel_6, Channel_4
    { DMA_PERIPH_UART_TX, UARTDEV_1, { FT32_DMA(2, 6, 4) } },
    { DMA_PERIPH_UART_RX, UARTDEV_1, { FT32_DMA(2, 4, 4), FT32_DMA(2, 4, 4) } },
#endif

#ifdef USE_UART2
    // USART2 需要根据 FT32 手册确认映射
    // 这里使用常见映射（需要验证）
    { DMA_PERIPH_UART_TX, UARTDEV_2, { FT32_DMA(1, 6, 4) } },
    { DMA_PERIPH_UART_RX, UARTDEV_2, { FT32_DMA(1, 5, 4) } },
#endif

#ifdef USE_UART3
    // USART3_TX: DMA1 Channel_7
    // USART3_RX: DMA1 Channel_4
    // Citation: spec.json -> channel_mapping.DMA1.Channel_7, Channel_4
    { DMA_PERIPH_UART_TX, UARTDEV_3, { FT32_DMA(1, 7, 4) } },
    { DMA_PERIPH_UART_RX, UARTDEV_3, { FT32_DMA(1, 4, 4) } },
#endif

#ifdef USE_UART4
    // UART4_TX: DMA1 Channel_4 (需要验证)
    // UART4_RX: DMA1 Channel_2
    // Citation: spec.json -> channel_mapping.DMA1.Channel_2
    { DMA_PERIPH_UART_TX, UARTDEV_4, { FT32_DMA(1, 4, 4) } },
    { DMA_PERIPH_UART_RX, UARTDEV_4, { FT32_DMA(1, 2, 4) } },
#endif

#ifdef USE_UART5
    // UART5_TX: DMA1 Channel_7 (需要验证)
    // UART5_RX: DMA1 Channel_4
    // Citation: spec.json -> channel_mapping.DMA1.Channel_4, Channel_7
    { DMA_PERIPH_UART_TX, UARTDEV_5, { FT32_DMA(1, 7, 4) } },
    { DMA_PERIPH_UART_RX, UARTDEV_5, { FT32_DMA(1, 4, 4) } },
#endif

#ifdef USE_UART6
    // USART6_TX: DMA2 Channel_5
    // USART6_RX: DMA2 Channel_5
    // Citation: spec.json -> channel_mapping.DMA2.Channel_5
    { DMA_PERIPH_UART_TX, UARTDEV_6, { FT32_DMA(2, 5, 5), FT32_DMA(2, 5, 5) } },
    { DMA_PERIPH_UART_RX, UARTDEV_6, { FT32_DMA(2, 5, 5), FT32_DMA(2, 5, 5) } },
#endif

#ifdef USE_UART7
    // UART7_TX: DMA1 Channel_5
    // UART7_RX: DMA1 Channel_5
    // Citation: spec.json -> channel_mapping.DMA1.Channel_5
    { DMA_PERIPH_UART_TX, UARTDEV_7, { FT32_DMA(1, 5, 5) } },
    { DMA_PERIPH_UART_RX, UARTDEV_7, { FT32_DMA(1, 5, 5) } },
#endif

// I2C DMA 映射暂不支持 - DMA_PERIPH_I2C_RX/TX 未在 dma_reqmap.h 中定义
// 需要在 dma_reqmap.h 中添加 DMA_PERIPH_I2C_RX 和 DMA_PERIPH_I2C_TX 枚举
#ifdef USE_I2C
    // I2C2_RX: DMA1 Channel_7
    // I2C2_TX: DMA1 Channel_7
    // Citation: spec.json -> channel_mapping.DMA1.Channel_7
    // { DMA_PERIPH_I2C_RX,  I2CDEV_2,  { FT32_DMA(1, 7, 4) } },
    // { DMA_PERIPH_I2C_TX,  I2CDEV_2,  { FT32_DMA(1, 7, 4) } },
    
    // I2C3_RX: DMA1 Channel_3
    // I2C3_TX: DMA1 Channel_3
    // Citation: spec.json -> channel_mapping.DMA1.Channel_3
    // { DMA_PERIPH_I2C_RX,  I2CDEV_3,  { FT32_DMA(1, 3, 4) } },
    // { DMA_PERIPH_I2C_TX,  I2CDEV_3,  { FT32_DMA(1, 3, 4) } },
#endif
};

#undef DMA

#define TC(chan) DEF_TIM_CHANNEL(CH_ ## chan)

/*
 * 定时器 DMA 映射表
 *
 * Citation: spec.json -> channel_mapping
 */
static const dmaTimerMapping_t dmaTimerMapping[] = {
    // TIM1: DMA2 Channel_6 (CH1, CH2, CH3), DMA2 Channel_3 (CH4)
    // Citation: RM V1.00 表 10-2 页 190 - 外设 0 通道 6 (CH1/CH2/CH3), 外设 6 通道 3 (CH4)
    { (timerResource_t *)TIM1, TC(CH1), { FT32_DMA(2, 6, 0) } },
    { (timerResource_t *)TIM1, TC(CH2), { FT32_DMA(2, 6, 0) } },
    { (timerResource_t *)TIM1, TC(CH3), { FT32_DMA(2, 6, 0) } },
    { (timerResource_t *)TIM1, TC(CH4), { FT32_DMA(2, 3, 6) } },

    // TIM2: DMA1 Channel_4 (CH1/CH2), Channel_0 (CH3), Channel_5/6 (CH4)
    // Citation: RM V1.00 表 10-1 页 189 - 外设 3 通道 4 (CH1/CH2), 通道 0 (CH3), 通道 5/6 (CH4)
    { (timerResource_t *)TIM2, TC(CH1), { FT32_DMA(1, 4, 3) } },
    { (timerResource_t *)TIM2, TC(CH2), { FT32_DMA(1, 4, 3) } },
    { (timerResource_t *)TIM2, TC(CH3), { FT32_DMA(1, 0, 3) } },
    { (timerResource_t *)TIM2, TC(CH4), { FT32_DMA(1, 5, 3), FT32_DMA(1, 6, 3) } },

    // TIM3: DMA1 Channel_1 (CH1), Channel_2 (CH2), Channel_4 (CH3), Channel_0 (CH4)
    // Citation: RM V1.00 表 10-1 页 189 - 外设 5 通道 1 (CH1), 通道 2 (CH2), 通道 4 (CH3), 通道 0 (CH4)
    { (timerResource_t *)TIM3, TC(CH1), { FT32_DMA(1, 1, 5) } },
    { (timerResource_t *)TIM3, TC(CH2), { FT32_DMA(1, 2, 5) } },
    { (timerResource_t *)TIM3, TC(CH3), { FT32_DMA(1, 4, 5) } },
    { (timerResource_t *)TIM3, TC(CH4), { FT32_DMA(1, 0, 5) } },

    // TIM4: DMA1 Channel_0 (CH1), DMA1 Channel_3 (CH2), DMA1 Channel_7 (CH3)
    // Citation: spec.json -> channel_mapping.DMA1.Channel_0, Channel_3
    { (timerResource_t *)TIM4, TC(CH1), { FT32_DMA(1, 0, 2) } },
    { (timerResource_t *)TIM4, TC(CH2), { FT32_DMA(1, 3, 2) } },
    { (timerResource_t *)TIM4, TC(CH3), { FT32_DMA(1, 7, 2) } },

    // TIM5: DMA1 Channel_2 (CH1), Channel_3 (CH2), Channel_0 (CH3), Channel_1 (CH4)
    // Citation: RM V1.00 表 10-1 页 189 - 外设 6 通道 2 (CH1), 通道 3 (CH2), 通道 0 (CH3), 通道 1 (CH4)
    { (timerResource_t *)TIM5, TC(CH1), { FT32_DMA(1, 2, 6) } },
    { (timerResource_t *)TIM5, TC(CH2), { FT32_DMA(1, 3, 6) } },
    { (timerResource_t *)TIM5, TC(CH3), { FT32_DMA(1, 0, 6) } },
    { (timerResource_t *)TIM5, TC(CH4), { FT32_DMA(1, 1, 6) } },

    // TIM8: DMA2 Channel_2 (CH1/CH2/CH3), Channel_3 (CH2 备选), Channel_4 (CH3 备选), Channel_7 (CH4)
    // Citation: RM V1.00 表 10-2 页 190 - 外设 0 通道 2 (CH1/CH2/CH3), 外设 7 通道 3/4/7 (CH2/CH3/CH4)
    { (timerResource_t *)TIM8, TC(CH1), { FT32_DMA(2, 2, 0) } },
    { (timerResource_t *)TIM8, TC(CH2), { FT32_DMA(2, 2, 0), FT32_DMA(2, 3, 7) } },
    { (timerResource_t *)TIM8, TC(CH3), { FT32_DMA(2, 2, 0), FT32_DMA(2, 4, 7) } },
    { (timerResource_t *)TIM8, TC(CH4), { FT32_DMA(2, 7, 7) } },
};

#undef TC
#undef DMA

/*
 * 根据外设获取 DMA 通道规格
 *
 * @param device 外设类型
 * @param index 外设索引
 * @param opt DMA 选项索引
 * @return DMA 通道规格指针，失败返回 NULL
 */
const dmaChannelSpec_t *dmaGetChannelSpecByPeripheral(dmaPeripheral_e device, uint8_t index, int8_t opt)
{
    if (opt < 0 || opt >= MAX_PERIPHERAL_DMA_OPTIONS) {
        return NULL;
    }

    for (const dmaPeripheralMapping_t *periph = dmaPeripheralMapping; periph < ARRAYEND(dmaPeripheralMapping); periph++) {
        if (periph->device == device && periph->index == index && periph->channelSpec[opt].ref) {
            return &periph->channelSpec[opt];
        }
    }

    return NULL;
}

/*
 * 根据 IO 标签获取 DMA 选项
 *
 * @param ioTag IO 标签
 * @return DMA 选项值
 */
dmaoptValue_t dmaoptByTag(ioTag_t ioTag)
{
#ifdef USE_TIMER_MGMT
    for (unsigned i = 0; i < MAX_TIMER_PINMAP_COUNT; i++) {
        if (timerIOConfig(i)->ioTag == ioTag) {
            return timerIOConfig(i)->dmaopt;
        }
    }
#else
    UNUSED(ioTag);
#endif

    return DMA_OPT_UNUSED;
}

/*
 * 根据定时器值获取 DMA 通道规格
 *
 * @param tim 定时器资源指针
 * @param channel 定时器通道
 * @param dmaopt DMA 选项
 * @return DMA 通道规格指针，失败返回 NULL
 */
const dmaChannelSpec_t *dmaGetChannelSpecByTimerValue(timerResource_t *tim, uint8_t channel, dmaoptValue_t dmaopt)
{
    if (dmaopt < 0 || dmaopt >= MAX_TIMER_DMA_OPTIONS) {
        return NULL;
    }

    for (unsigned i = 0; i < ARRAYLEN(dmaTimerMapping); i++) {
        const dmaTimerMapping_t *timerMapping = &dmaTimerMapping[i];
        if (timerMapping->tim == tim && timerMapping->channel == channel && timerMapping->channelSpec[dmaopt].ref) {
            return &timerMapping->channelSpec[dmaopt];
        }
    }

    return NULL;
}

/*
 * 根据定时器硬件获取 DMA 通道规格
 *
 * @param timer 定时器硬件指针
 * @return DMA 通道规格指针，失败返回 NULL
 */
const dmaChannelSpec_t *dmaGetChannelSpecByTimer(const timerHardware_t *timer)
{
    if (!timer) {
        return NULL;
    }

    dmaoptValue_t dmaopt = dmaoptByTag(timer->tag);
    return dmaGetChannelSpecByTimerValue(timer->tim, timer->channel, dmaopt);
}

/*
 * 根据定时器获取 DMA 选项
 *
 * @param timer 定时器硬件指针
 * @return DMA 选项值，失败返回 DMA_OPT_UNUSED
 */
dmaoptValue_t dmaGetOptionByTimer(const timerHardware_t *timer)
{
    for (unsigned i = 0; i < ARRAYLEN(dmaTimerMapping); i++) {
        const dmaTimerMapping_t *timerMapping = &dmaTimerMapping[i];
        if (timerMapping->tim == timer->tim && timerMapping->channel == timer->channel) {
            for (unsigned j = 0; j < MAX_TIMER_DMA_OPTIONS; j++) {
                const dmaChannelSpec_t *dma = &timerMapping->channelSpec[j];
                if (dma->ref == timer->dmaRefConfigured
                    && dma->channel == timer->dmaChannelConfigured) {
                    return j;
                }
            }
        }
    }

    return DMA_OPT_UNUSED;
}

#endif // USE_DMA_SPEC
