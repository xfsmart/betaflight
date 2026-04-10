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

#ifdef USE_SPI

#define IS_CCM(p) (((uint32_t)p & 0xffff0000) == 0x10000000)

#include "common/maths.h"
#include "drivers/bus.h"
#include "drivers/bus_spi.h"
#include "drivers/bus_spi_impl.h"
#include "drivers/exti.h"
#include "drivers/io.h"
#include "drivers/time.h"
#include "platform/rcc.h"

#define SPI_DMA_THRESHOLD 8

static SPI_InitTypeDef defaultInit = {
    .SPI_Mode = SPI_Mode_Master,
    .SPI_Direction = SPI_Direction_2Lines_FullDuplex,
    .SPI_DataSize = SPI_DataSize_8b,
    .SPI_CPOL = SPI_CPOL_High,
    .SPI_CPHA = SPI_CPHA_2Edge,
    .SPI_NSS = SPI_NSS_Soft,
    .SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8,
    .SPI_FirstBit = SPI_FirstBit_MSB,
    .SPI_CRCPolynomial = 7
};

/**
 * @brief  Convert SPI baud rate divisor to BR register bits
 * @param  instance: SPI instance (used to determine APB bus)
 * @param  divisor: Clock divisor (2-256)
 * @retval BR register bits
 * BR[2:0] = f(FFS(divisor) - 2) << 3
 */
static uint16_t spiDivisorToBRbits(const SPI_TypeDef *instance, uint16_t divisor)
{
    // SPI2 and SPI3 are on APB1 which PCLK is half that of APB2
    // APB1 = 52.5MHz, APB2 = 105MHz (FT32F405: 210MHz, APB1=/4, APB2=/2)
    if (instance == SPI2 || instance == SPI3) {
        divisor /= 2; // Safe for divisor == 0 or 1
    }

    divisor = constrain(divisor, 2, 256);

    return (ffs(divisor) - 2) << 3; // SPI_CR1_BR_Pos
}

/**
 * @brief  Set SPI baud rate divisor in BR register
 * @param  instance: SPI instance
 * @param  divisor: Clock divisor (2-256)
 * 
 * Note: This function preserves other CR1 bits while updating BR[2:0]
 */
static void spiSetDivisorBRreg(SPI_TypeDef *instance, uint16_t divisor)
{
#define BR_BITS ((BIT(5) | BIT(4) | BIT(3)))
    const uint16_t tempRegister = (instance->CR1 & ~BR_BITS);
    instance->CR1 = tempRegister | spiDivisorToBRbits(instance, divisor);
#undef BR_BITS
}

/**
 * @brief  Initialize SPI device
 * @param  device: SPI device enumeration
 */
void spiInitDevice(spiDevice_e device)
{
    spiDevice_t *spi = &(spiDevice[device]);

    if (!spi->dev) {
        return;
    }

    // Enable SPI clock
    RCC_ClockCmd(spi->rcc, ENABLE);
    RCC_ResetCmd(spi->rcc, ENABLE);

    IOInit(IOGetByTag(spi->sck),  OWNER_SPI_SCK,  RESOURCE_INDEX(device));
    IOInit(IOGetByTag(spi->miso), OWNER_SPI_SDI, RESOURCE_INDEX(device));
    IOInit(IOGetByTag(spi->mosi), OWNER_SPI_SDO, RESOURCE_INDEX(device));

    IOConfigGPIOAF(IOGetByTag(spi->sck),  SPI_IO_AF_SCK_CFG_HIGH, spi->sckAF);
    IOConfigGPIOAF(IOGetByTag(spi->miso), SPI_IO_AF_SDI_CFG, spi->misoAF);
    IOConfigGPIOAF(IOGetByTag(spi->mosi), SPI_IO_AF_CFG, spi->mosiAF);

    // Init SPI hardware
    SPI_DeInit((SPI_TypeDef*)spi->dev);

    // Disable SPI DMA requests by default
    // DMA will be enabled in spiInternalStartDMA() when needed
    SPI_DMACmd((SPI_TypeDef*)spi->dev, SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN, DISABLE);
    
    SPI_Init((SPI_TypeDef*)spi->dev, &defaultInit);
    SPI_Cmd((SPI_TypeDef*)spi->dev, ENABLE);
}

/**
 * @brief  Reset SPI DMA descriptors
 * @param  bus: pointer to bus device structure
 *
 * DMA channel mapping:
 * - SPI1: DMA2 Channel 3 (Tx and Rx share same channel)
 * - SPI2: DMA1 Channel 6 (Tx), Channel 1 (Rx)
 * - SPI3: DMA1 Channel 5 (Tx), Channel 0 (Rx)
 */
void spiInternalResetDescriptors(busDevice_t *bus)
{
    // Convert opaque spiResource_t* to SPI_TypeDef*
    SPI_TypeDef *instance = (SPI_TypeDef *)bus->busType_u.spi.instance;
    DMA_InitTypeDef *dmaInitTx = bus->dmaInitTx;

    DMA_StructInit(dmaInitTx);
    dmaInitTx->TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA;
    dmaInitTx->DstAddress = (uint32_t)&instance->DR;
    dmaInitTx->SrcAddrMode = DMA_SRC_ADDRMODE_INC;
    dmaInitTx->DstAddrMode = DMA_DST_ADDRMODE_HOLD;
    dmaInitTx->SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_8BITS;
    dmaInitTx->DstTransferWidth = DMA_DST_TRANSFERWIDTH_8BITS;

    // Configure hardware handshaking interface for SPI Tx (Memory to Peripheral)
    // DstHardwareInterface: DMA channel number (0-7) - selects which CHSEL field to write
    // DstHsIfPeriphSel: Peripheral request ID - value to write to CHSEL
    dmaInitTx->DstHardwareInterface = bus->dmaTx->stream;
    dmaInitTx->DstHsIfPeriphSel = bus->dmaTx->channel;

    if (bus->dmaRx) {
        DMA_InitTypeDef *dmaInitRx = bus->dmaInitRx;

        DMA_StructInit(dmaInitRx);
        dmaInitRx->TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA;
        dmaInitRx->SrcAddress = (uint32_t)&instance->DR;
        dmaInitRx->SrcAddrMode = DMA_SRC_ADDRMODE_HOLD;
        dmaInitRx->DstAddrMode = DMA_DST_ADDRMODE_INC;
        dmaInitRx->SrcTransferWidth = DMA_SRC_TRANSFERWIDTH_8BITS;
        dmaInitRx->DstTransferWidth = DMA_DST_TRANSFERWIDTH_8BITS;

        // Configure hardware handshaking interface for SPI Rx (Peripheral to Memory)
        // SrcHardwareInterface: DMA channel number (0-7) - selects which CHSEL field to write
        // SrcHsIfPeriphSel: Peripheral request ID - value to write to CHSEL
        dmaInitRx->SrcHardwareInterface = bus->dmaRx->stream;
        dmaInitRx->SrcHsIfPeriphSel = bus->dmaRx->channel;
    }
}

/**
 * @brief  Reset DMA stream
 * @param  descriptor: pointer to DMA channel descriptor
 */
void spiInternalResetStream(dmaChannelDescriptor_t *descriptor)
{
    DMA_ARCH_TYPE *channelRegs = (DMA_ARCH_TYPE *)descriptor->ref;

    // Disable the channel
    DMA_Cmd(channelRegs, DISABLE);

    // Clear any pending interrupt flags
    DMA_CLEAR_FLAG(descriptor, DMA_IT_TCIF | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR);
}

/**
 * @brief  SPI polled read/write transfer
 * @param  spiInstance: opaque SPI instance handle (spiResource_t*)
 * @param  txData: transmit buffer (NULL for dummy bytes)
 * @param  rxData: receive buffer (NULL to discard received data)
 * @param  len: number of bytes to transfer
 * @retval true on success
 * 
 * Note: Performs byte-by-byte SPI transfer using polling method.
 * Uses FT32 standard library functions (SPI_GetFlagStatus, SPI_SendData8, etc.)
 */
bool spiInternalReadWriteBufPolled(spiResource_t *spiInstance, const uint8_t *txData, uint8_t *rxData, int len)
{
    // Convert opaque spiResource_t* to SPI_TypeDef*
    SPI_TypeDef *instance = (SPI_TypeDef *)spiInstance;
    uint8_t b;
    timeUs_t startTime;

    while (len--) {
        b = txData ? *(txData++) : 0xFF;
        startTime = microsISR();
        while (SPI_GetFlagStatus(instance, SPI_FLAG_TXE) == RESET) {
            if (cmpTimeUs(microsISR(), startTime) > SPI_TIMEOUT_US) {
                return false;  // Timeout
            }
        }
        SPI_SendData8(instance, b);

        startTime = microsISR();
        while (SPI_GetFlagStatus(instance, SPI_FLAG_RXNE) == RESET) {
            if (cmpTimeUs(microsISR(), startTime) > SPI_TIMEOUT_US) {
                return false;  // Timeout
            }
        }
        b = SPI_ReceiveData8(instance);
        if (rxData) {
            *(rxData++) = b;
        }
    }

    return true;
}

/**
 * @brief  Initialize DMA stream for SPI transfer
 * @param  dev: pointer to external device structure
 * @param  segment: pointer to bus segment structure
 * 
 * Note: Configures DMA buffer addresses and transfer size
 */
void spiInternalInitStream(const extDevice_t *dev, volatile busSegment_t *segment)
{
    STATIC_DMA_DATA_AUTO uint8_t dummyTxByte = 0xff;
    STATIC_DMA_DATA_AUTO uint8_t dummyRxByte;
    busDevice_t *bus = dev->bus;
    int len = segment->len;

    uint8_t *txData = segment->u.buffers.txData;
    DMA_InitTypeDef *dmaInitTx = bus->dmaInitTx;

    if (txData) {
        dmaInitTx->SrcAddress = (uint32_t)txData;
        dmaInitTx->SrcAddrMode = DMA_SRC_ADDRMODE_INC;
    } else {
        dummyTxByte = 0xff;
        dmaInitTx->SrcAddress = (uint32_t)&dummyTxByte;
        dmaInitTx->SrcAddrMode = DMA_SRC_ADDRMODE_HOLD;
    }
    dmaInitTx->BlockTransSize = len;

    if (dev->bus->dmaRx) {
        uint8_t *rxData = segment->u.buffers.rxData;
        DMA_InitTypeDef *dmaInitRx = bus->dmaInitRx;

        if (rxData) {
            dmaInitRx->DstAddress = (uint32_t)rxData;
            dmaInitRx->DstAddrMode = DMA_DST_ADDRMODE_INC;
        } else {
            dmaInitRx->DstAddress = (uint32_t)&dummyRxByte;
            dmaInitRx->DstAddrMode = DMA_DST_ADDRMODE_HOLD;
        }
        dmaInitRx->BlockTransSize = len;
    }
}

/**
 * @brief  Start DMA transfer for SPI
 * @param  dev: pointer to external device structure
 * 
 * Note: Enables SPI DMA requests and DMA channels
 */
void spiInternalStartDMA(const extDevice_t *dev)
{
    dmaChannelDescriptor_t *dmaTx = dev->bus->dmaTx;
    dmaChannelDescriptor_t *dmaRx = dev->bus->dmaRx;
    DMA_ARCH_TYPE *channelTx = (DMA_ARCH_TYPE *)dmaTx->ref;
    
    // Convert opaque spiResource_t* to SPI_TypeDef*
    SPI_TypeDef *instance = (SPI_TypeDef *)dev->bus->busType_u.spi.instance;
    
    if (dmaRx) {
        DMA_ARCH_TYPE *channelRx = (DMA_ARCH_TYPE *)dmaRx->ref;

        // Set callback parameter
        dmaRx->userParam = (uint32_t)dev;

        // Clear transfer flags
        DMA_CLEAR_FLAG(dmaTx, DMA_IT_ERR | DMA_IT_TCIF);
        DMA_CLEAR_FLAG(dmaRx, DMA_IT_ERR | DMA_IT_TCIF);

        // Disable channels to enable update
        DMA_Cmd(channelTx, DISABLE);
        DMA_Cmd(channelRx, DISABLE);

        // Use Rx interrupt to detect transfer completion
        DMA_ITConfig(channelRx, DMA_IT_TFR, ENABLE);

        // Initialize DMA channels
        DMA_Init(channelTx, dev->bus->dmaInitTx);
        DMA_Init(channelRx, dev->bus->dmaInitRx);

        // Enable channels
        DMA_Cmd(channelTx, ENABLE);
        DMA_Cmd(channelRx, ENABLE);

        // Enable SPI DMA requests
        SPI_DMACmd(instance, SPI_CR2_TXDMAEN, ENABLE);
        SPI_DMACmd(instance, SPI_CR2_RXDMAEN, ENABLE);
    } else {
        // Set callback parameter
        dmaTx->userParam = (uint32_t)dev;

        // Clear transfer flags
        DMA_CLEAR_FLAG(dmaTx, DMA_IT_ERR | DMA_IT_TCIF);

        // Disable channel to enable update
        DMA_Cmd(channelTx, DISABLE);

        DMA_ITConfig(channelTx, DMA_IT_TFR, ENABLE);

        // Initialize DMA channel
        DMA_Init(channelTx, dev->bus->dmaInitTx);

        // Enable channel
        DMA_Cmd(channelTx, ENABLE);

        // Enable SPI DMA Tx request
        SPI_DMACmd(instance, SPI_CR2_TXDMAEN, ENABLE);
    }
}

/**
 * @brief  Stop DMA transfer for SPI
 * @param  dev: pointer to external device structure
 * 
 * Note: Disables SPI DMA requests and DMA channels
 */
void spiInternalStopDMA(const extDevice_t *dev)
{
    dmaChannelDescriptor_t *dmaTx = dev->bus->dmaTx;
    dmaChannelDescriptor_t *dmaRx = dev->bus->dmaRx;
    // Convert opaque spiResource_t* to SPI_TypeDef*
    SPI_TypeDef *instance = (SPI_TypeDef *)dev->bus->busType_u.spi.instance;
    DMA_ARCH_TYPE *channelTx = (DMA_ARCH_TYPE *)dmaTx->ref;

    if (dmaRx) {
        DMA_ARCH_TYPE *channelRx = (DMA_ARCH_TYPE *)dmaRx->ref;

        // Disable channels
        DMA_Cmd(channelTx, DISABLE);
        DMA_Cmd(channelRx, DISABLE);

        // Disable SPI DMA requests
        SPI_DMACmd(instance, SPI_CR2_TXDMAEN, DISABLE);
        SPI_DMACmd(instance, SPI_CR2_RXDMAEN, DISABLE);

        // Clear flags
        DMA_CLEAR_FLAG(dmaTx, DMA_IT_ERR | DMA_IT_TCIF);
        DMA_CLEAR_FLAG(dmaRx, DMA_IT_ERR | DMA_IT_TCIF);
    } else {
        // Ensure current transmission is complete
        while (SPI_GetFlagStatus(instance, SPI_FLAG_BSY));

        // Drain RX buffer
        while (SPI_GetFlagStatus(instance, SPI_FLAG_RXNE)) {
            instance->DR;
        }

        // Disable channel
        DMA_Cmd(channelTx, DISABLE);

        // Disable SPI DMA Tx request
        SPI_DMACmd(instance, SPI_CR2_TXDMAEN, DISABLE);

        // Clear flags
        DMA_CLEAR_FLAG(dmaTx, DMA_IT_ERR | DMA_IT_TCIF);
    }
}

// DMA transfer setup and start
void spiSequenceStart(const extDevice_t *dev)
{
    busDevice_t *bus = dev->bus;
    // Convert opaque spiResource_t* to SPI_TypeDef*
    SPI_TypeDef *instance = (SPI_TypeDef *)bus->busType_u.spi.instance;
    bool dmaSafe = dev->useDMA;
    uint32_t xferLen = 0;
    uint32_t segmentCount = 0;

    dev->bus->initSegment = true;

    SPI_Cmd(instance, DISABLE);

    // Switch bus speed
    if (dev->busType_u.spi.speed != bus->busType_u.spi.speed) {
        spiSetDivisorBRreg(instance, dev->busType_u.spi.speed);
        bus->busType_u.spi.speed = dev->busType_u.spi.speed;
    }

    if (dev->busType_u.spi.leadingEdge != bus->busType_u.spi.leadingEdge) {
        // Switch SPI clock polarity/phase
        instance->CR1 &= ~(SPI_CPOL_High | SPI_CPHA_2Edge);

        // Apply setting
        if (dev->busType_u.spi.leadingEdge) {
            instance->CR1 |= SPI_CPOL_Low | SPI_CPHA_1Edge;
        } else {
            instance->CR1 |= SPI_CPOL_High | SPI_CPHA_2Edge;
        }
        bus->busType_u.spi.leadingEdge = dev->busType_u.spi.leadingEdge;
    }

    SPI_Cmd(instance, ENABLE);

    // Check that there are no attempts to DMA to/from CCM SRAM
    for (busSegment_t *checkSegment = (busSegment_t *)bus->curSegment; checkSegment->len; checkSegment++) {
        // Check there is no receive data as only transmit DMA is available
        if (((checkSegment->u.buffers.rxData) && (IS_CCM(checkSegment->u.buffers.rxData) || (bus->dmaRx == (dmaChannelDescriptor_t *)NULL))) ||
            ((checkSegment->u.buffers.txData) && IS_CCM(checkSegment->u.buffers.txData))) {
            dmaSafe = false;
            break;
        }
        segmentCount++;
        xferLen += checkSegment->len;
    }

    // Use DMA if possible
    // If there are more than one segments, or a single segment with negateCS negated in the list terminator then force DMA irrespective of length
    if (bus->useDMA && dmaSafe && ((segmentCount > 1) ||
                                   (xferLen >= SPI_DMA_THRESHOLD) ||
                                   !bus->curSegment[segmentCount].negateCS)) {
        spiProcessSegmentsDMA(dev);
    } else {
        spiProcessSegmentsPolled(dev);
    }
}

#endif
