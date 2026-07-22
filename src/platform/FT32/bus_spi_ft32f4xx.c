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
#include "drivers/nvic.h"
#include "drivers/system.h"
#include "drivers/time.h"
#include "platform/rcc.h"

#define SPI_DMA_THRESHOLD 8
#define SPI_DMA_FLAG_MASK (DMA_IT_TCIF | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR)
#define SPI_DMA_NON_ERROR_FLAG_MASK (DMA_IT_TCIF | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST)
#define SPI_DMA_REQUEST_MASK (SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN)
#define SPI_DMA_BLOCK_TS_SHIFT 45U
#define SPI_DMA_BLOCK_TS_MASK (0xFFFFULL << SPI_DMA_BLOCK_TS_SHIFT)
#define SPI_ERROR_FLAG_MASK (SPI_FLAG_OVR | SPI_FLAG_MODF | SPI_FLAG_FRE)
#define SPI_CR1_CLOCK_MODE_MASK (SPI_CR1_CPOL | SPI_CR1_CPHA)
#define SPI_CR1_DIRECTION_MASK (SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE | SPI_CR1_RXONLY)
#define SPI_DMA_IRQ_FENCE_CAPACITY 2U

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

static uint32_t spiDmaEnterCritical(void)
{
    const uint32_t primask = __get_PRIMASK();

    __disable_irq();
    __DMB();
    return primask;
}

static void spiDmaExitCritical(uint32_t primask)
{
    __DMB();
    __set_PRIMASK(primask);
}

typedef struct spiDmaIrqFence_s {
    IRQn_Type irqs[SPI_DMA_IRQ_FENCE_CAPACITY];
    bool wasEnabled[SPI_DMA_IRQ_FENCE_CAPACITY];
    uint8_t count;
} spiDmaIrqFence_t;

static bool spiDmaIrqNumberIsValid(int32_t irq)
{
    return irq >= 0 && irq <= UART7_IRQn;
}

static bool spiDmaIrqIsEnabled(IRQn_Type irqn)
{
    const uint32_t irq = (uint32_t)irqn;

    return (NVIC->ISER[irq >> 5U] & (1UL << (irq & 0x1FU))) != 0U;
}

static void spiDmaIrqFenceAdd(spiDmaIrqFence_t *fence, const dmaChannelDescriptor_t *descriptor)
{
    if (!descriptor || !spiDmaIrqNumberIsValid(descriptor->irqN)) {
        return;
    }

    const IRQn_Type irqn = (IRQn_Type)descriptor->irqN;
    for (uint8_t index = 0; index < fence->count; index++) {
        if (fence->irqs[index] == irqn) {
            return;
        }
    }

    if (fence->count < SPI_DMA_IRQ_FENCE_CAPACITY) {
        fence->irqs[fence->count++] = irqn;
    }
}

static spiDmaIrqFence_t spiDmaIrqFenceEnter(
    const dmaChannelDescriptor_t *dmaTx,
    const dmaChannelDescriptor_t *dmaRx)
{
    spiDmaIrqFence_t fence = { 0 };

    spiDmaIrqFenceAdd(&fence, dmaTx);
    spiDmaIrqFenceAdd(&fence, dmaRx);

    const uint32_t primask = spiDmaEnterCritical();
    for (uint8_t index = 0; index < fence.count; index++) {
        fence.wasEnabled[index] = spiDmaIrqIsEnabled(fence.irqs[index]);
        NVIC_DisableIRQ(fence.irqs[index]);
    }
    __DSB();
    __ISB();
    spiDmaExitCritical(primask);

    return fence;
}

static bool spiDmaIrqFenceWasEnabled(
    const spiDmaIrqFence_t *fence,
    const dmaChannelDescriptor_t *descriptor)
{
    if (!descriptor || !spiDmaIrqNumberIsValid(descriptor->irqN)) {
        return false;
    }

    const IRQn_Type irqn = (IRQn_Type)descriptor->irqN;
    for (uint8_t index = 0; index < fence->count; index++) {
        if (fence->irqs[index] == irqn) {
            return fence->wasEnabled[index];
        }
    }

    return false;
}

static void spiDmaIrqFenceExit(const spiDmaIrqFence_t *fence, bool restoreEnable)
{
    const uint32_t primask = spiDmaEnterCritical();
    if (restoreEnable) {
        for (uint8_t index = 0; index < fence->count; index++) {
            if (fence->wasEnabled[index]) {
                NVIC_EnableIRQ(fence->irqs[index]);
            } else {
                NVIC_DisableIRQ(fence->irqs[index]);
            }
        }
    } else {
        for (uint8_t index = 0; index < fence->count; index++) {
            NVIC_DisableIRQ(fence->irqs[index]);
        }
    }
    __DSB();
    __ISB();
    spiDmaExitCritical(primask);
}

static uint64_t spiDmaChannelMask(DMA_ARCH_TYPE *channel)
{
    const DMA_BaseAddressAndChannelIndex dma = CalBaseAddressAndChannelIndex(channel);

    return 1ULL << dma.ChannelIndex;
}

static bool spiDmaControllerIsEnabled(DMA_ARCH_TYPE *channel)
{
    const DMA_BaseAddressAndChannelIndex dma = CalBaseAddressAndChannelIndex(channel);

    return (dma.BaseAddress->DMACFG & DMA_DMACFG_DMA_EN) != 0U;
}

static void spiDmaInterruptConfig(DMA_ARCH_TYPE *channel, FunctionalState state)
{
    xDMA_Cmd(channel, DISABLE);
    if (ft32DmaIsChannelEnabled(channel)) {
        return;
    }
    xDMA_ITConfig(channel, DMA_IT_TFR, state);
    xDMA_ITConfig(channel, DMA_IT_BLOCK, state);
    xDMA_ITConfig(channel, DMA_IT_SRC, state);
    xDMA_ITConfig(channel, DMA_IT_DST, state);
    xDMA_ITConfig(channel, DMA_IT_ERR, state);
}

static bool spiDmaEnableTransferInterrupt(DMA_ARCH_TYPE *channel)
{
    xDMA_Cmd(channel, DISABLE);
    if (ft32DmaIsChannelEnabled(channel)) {
        return false;
    }

    xDMA_ITConfig(channel, DMA_IT_TFR, ENABLE);
    return true;
}

static bool spiDmaInterruptStateMatches(DMA_ARCH_TYPE *channel, bool transferEnabled)
{
    const DMA_BaseAddressAndChannelIndex dma = CalBaseAddressAndChannelIndex(channel);
    const uint64_t mask = spiDmaChannelMask(channel);
    const bool transferMaskEnabled = (dma.BaseAddress->MASKTFR & mask) != 0U;
    const uint64_t otherMasks = (dma.BaseAddress->MASKBLOCK |
        dma.BaseAddress->MASKSRCTRAN |
        dma.BaseAddress->MASKDSTTRAN |
        dma.BaseAddress->MASKERR) & mask;
    const bool globalInterruptEnabled = (channel->CTL & DMA_CTL_INT_EN) != 0U;

    return transferMaskEnabled == transferEnabled &&
        otherMasks == 0U &&
        globalInterruptEnabled == transferEnabled;
}

static bool spiDmaSelectedFlagsAreClear(dmaChannelDescriptor_t *descriptor, uint8_t flags)
{
    return (!(flags & DMA_IT_TCIF) || xDMA_GetFlagStatus(descriptor->ref, DMA_FLAG_TFR) == RESET) &&
        (!(flags & DMA_IT_BLOCK) || xDMA_GetFlagStatus(descriptor->ref, DMA_FLAG_BLOCK) == RESET) &&
        (!(flags & DMA_IT_SRC) || xDMA_GetFlagStatus(descriptor->ref, DMA_FLAG_SRC) == RESET) &&
        (!(flags & DMA_IT_DST) || xDMA_GetFlagStatus(descriptor->ref, DMA_FLAG_DST) == RESET) &&
        (!(flags & DMA_IT_ERR) || xDMA_GetFlagStatus(descriptor->ref, DMA_FLAG_ERR) == RESET);
}

static bool spiDmaClearSelectedFlagsAndPending(dmaChannelDescriptor_t *descriptor, uint8_t flags)
{
    if (!descriptor || !descriptor->ref || !spiDmaIrqNumberIsValid(descriptor->irqN)) {
        return false;
    }

    DMA_CLEAR_FLAG(descriptor, flags);
    NVIC_ClearPendingIRQ((IRQn_Type)descriptor->irqN);
    __DSB();
    __ISB();

    return spiDmaSelectedFlagsAreClear(descriptor, flags) &&
        NVIC_GetPendingIRQ((IRQn_Type)descriptor->irqN) == 0U;
}

static bool spiDmaClearFlagsAndPending(dmaChannelDescriptor_t *descriptor)
{
    return spiDmaClearSelectedFlagsAndPending(descriptor, SPI_DMA_FLAG_MASK);
}

static uint64_t spiDmaExpectedCtl(const DMA_InitTypeDef *init)
{
    return ((uint64_t)init->BlockTransSize << SPI_DMA_BLOCK_TS_SHIFT) |
        init->SrcDstMasterSel |
        init->TransferTypeFlowCtl |
        init->SrcBurstTransferLength |
        init->DstBurstTransferLength |
        init->SrcAddrMode |
        init->DstAddrMode |
        init->SrcTransferWidth |
        init->DstTransferWidth;
}

static uint64_t spiDmaExpectedCfg(const DMA_InitTypeDef *init)
{
    uint64_t cfg = ((uint64_t)(init->DstHardwareInterface << 11U |
        init->SrcHardwareInterface << 7U) << 32U) |
        (uint64_t)(init->MaxBurstLength << 20U |
        init->SrcHsIfPol << 19U |
        init->DstHsIfPol << 18U |
        init->SrcHsSel << 11U |
        init->DstHsSel << 10U |
        init->Priority << 5U);

    if (init->FIFOMode == ENABLE) {
        cfg |= DMA_CFG_FIFO_MODE;
    }
    if (init->FlowCtlMode == ENABLE) {
        cfg |= DMA_CFG_FCMODE;
    }
    if (init->ReloadDst == ENABLE) {
        cfg |= DMA_CFG_RELOAD_DST;
    }
    if (init->ReloadSrc == ENABLE) {
        cfg |= DMA_CFG_RELOAD_SRC;
    }

    return cfg;
}

static bool spiDmaRequestMatches(DMA_ARCH_TYPE *channel, const DMA_InitTypeDef *init)
{
    const DMA_BaseAddressAndChannelIndex dma = CalBaseAddressAndChannelIndex(channel);
    uint32_t hardwareInterface;
    uint32_t request;

    switch (init->TransferTypeFlowCtl) {
    case DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA:
    case DMA_TRANSFERTYPE_FLOWCTL_M2P_PRE:
        hardwareInterface = init->DstHardwareInterface;
        request = init->DstHsIfPeriphSel;
        break;

    case DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA:
    case DMA_TRANSFERTYPE_FLOWCTL_P2M_PRE:
        hardwareInterface = init->SrcHardwareInterface;
        request = init->SrcHsIfPeriphSel;
        break;

    default:
        return false;
    }

    const uint32_t shift = (hardwareInterface & 0x7U) * 3U;
    return ((dma.BaseAddress->CHSEL >> shift) & 0x7U) == request;
}

static bool spiDmaDescriptorMatches(DMA_ARCH_TYPE *channel, const DMA_InitTypeDef *init, bool transferInterruptEnabled)
{
    const uint64_t expectedCtl = spiDmaExpectedCtl(init) |
        (transferInterruptEnabled ? DMA_CTL_INT_EN : 0U);

    return channel->SAR == init->SrcAddress &&
        channel->DAR == init->DstAddress &&
        channel->CTL == expectedCtl &&
        (channel->CFG & ~(uint64_t)DMA_CFG_FIFO_EMPTY) == spiDmaExpectedCfg(init) &&
        (channel->CTL & SPI_DMA_BLOCK_TS_MASK) ==
            ((uint64_t)init->BlockTransSize << SPI_DMA_BLOCK_TS_SHIFT) &&
        spiDmaRequestMatches(channel, init) &&
        spiDmaInterruptStateMatches(channel, transferInterruptEnabled);
}

static bool spiDmaProducerMatches(const SPI_TypeDef *instance, uint16_t expected)
{
    return (instance->CR2 & SPI_DMA_REQUEST_MASK) == expected;
}

static bool spiDmaWireIdleSample(SPI_TypeDef *instance, bool hasRxDma)
{
    const uint16_t status = instance->SR;
    const uint16_t fatalMask = hasRxDma ? SPI_ERROR_FLAG_MASK : (SPI_FLAG_MODF | SPI_FLAG_FRE);

    if (status & SPI_FLAG_OVR) {
        (void)instance->DR;
        (void)instance->SR;
    }
    if (SPI_GetReceptionFIFOStatus(instance) != SPI_ReceptionFIFOStatus_Empty && hasRxDma) {
        (void)SPI_ReceiveData8(instance);
        return false;
    }
    return (status & fatalMask) == 0U &&
        SPI_GetTransmissionFIFOStatus(instance) == SPI_TransmissionFIFOStatus_Empty &&
        SPI_GetReceptionFIFOStatus(instance) == SPI_ReceptionFIFOStatus_Empty &&
        (status & SPI_FLAG_BSY) == 0U;
}

static bool spiDmaHardwareIsolated(const busDevice_t *bus)
{
    if (!bus || !bus->dmaTx || !bus->dmaTx->ref || !bus->busType_u.spi.instance ||
        (bus->dmaRx && !bus->dmaRx->ref)) {
        return false;
    }

    const SPI_TypeDef *instance = (const SPI_TypeDef *)bus->busType_u.spi.instance;
    const DMA_ARCH_TYPE *channelTx = (const DMA_ARCH_TYPE *)bus->dmaTx->ref;
    const DMA_ARCH_TYPE *channelRx = bus->dmaRx ? (const DMA_ARCH_TYPE *)bus->dmaRx->ref : NULL;

    return spiDmaProducerMatches(instance, 0U) &&
        !ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)channelTx) &&
        (!channelRx || !ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)channelRx));
}

static void spiDmaClearUserParams(busDevice_t *bus)
{
    if (bus->dmaTx) {
        bus->dmaTx->userParam = 0U;
    }
    if (bus->dmaRx) {
        bus->dmaRx->userParam = 0U;
    }
}

static bool spiDmaAbortStart(const extDevice_t *dev, bool *irqSourceClean)
{
    busDevice_t *bus = dev->bus;
    SPI_TypeDef *instance = (SPI_TypeDef *)bus->busType_u.spi.instance;

    *irqSourceClean = false;
    SPI_DMACmd(instance, SPI_DMA_REQUEST_MASK, DISABLE);

    if (bus->dmaRx && bus->dmaRx->ref) {
        ft32DmaRequestDisable((DMA_ARCH_TYPE *)bus->dmaRx->ref);
    }
    if (bus->dmaTx && bus->dmaTx->ref) {
        ft32DmaRequestDisable((DMA_ARCH_TYPE *)bus->dmaTx->ref);
    }

    const bool hardwareIsolated = spiDmaHardwareIsolated(bus);

    if (hardwareIsolated) {
        bool controlClean = true;
        bool flagsClean = true;

        if (bus->dmaRx && bus->dmaRx->ref) {
            spiDmaInterruptConfig((DMA_ARCH_TYPE *)bus->dmaRx->ref, DISABLE);
            controlClean =
                spiDmaInterruptStateMatches((DMA_ARCH_TYPE *)bus->dmaRx->ref, false) &&
                controlClean;
            flagsClean = spiDmaClearFlagsAndPending(bus->dmaRx) && flagsClean;
        }
        if (bus->dmaTx && bus->dmaTx->ref) {
            spiDmaInterruptConfig((DMA_ARCH_TYPE *)bus->dmaTx->ref, DISABLE);
            controlClean =
                spiDmaInterruptStateMatches((DMA_ARCH_TYPE *)bus->dmaTx->ref, false) &&
                controlClean;
            flagsClean = spiDmaClearFlagsAndPending(bus->dmaTx) && flagsClean;
        }
        *irqSourceClean = controlClean && flagsClean;
        spiDmaClearUserParams(bus);
    }

    return hardwareIsolated;
}

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

static bool spiWaitForFlagState(SPI_TypeDef *instance, uint16_t flag, bool set, bool failOnError)
{
    const uint32_t startCycles = getCycleCounter();
    const uint32_t timeoutCycles = clockMicrosToCycles(SPI_TIMEOUT_US);

    do {
        const uint16_t status = instance->SR;
        if (failOnError && (status & SPI_ERROR_FLAG_MASK)) {
            return false;
        }
        if (((status & flag) != 0U) == set) {
            return true;
        }
    } while (cmpTimeCycles(getCycleCounter(), startCycles) <= (int32_t)timeoutCycles);

    return false;
}

static void spiSettleBeforeReset(SPI_TypeDef *instance)
{
    (void)spiWaitForFlagState(instance, SPI_FLAG_BSY, false, false);
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

    spi->polledRecoveryRequired = false;

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
    SPI_RxFIFOThresholdConfig((SPI_TypeDef*)spi->dev, SPI_RxFIFOThreshold_QF);
    SPI_Cmd((SPI_TypeDef*)spi->dev, ENABLE);
}

/**
 * @brief  Reset SPI DMA descriptors
 * @param  bus: pointer to bus device structure
 *
 * DMA channel and peripheral request mapping:
 * - SPI1: DMA2 Channel 3/5 (Tx), Channel 0/2 (Rx), request 3
 * - SPI2: DMA2 Channel 6 (Tx), Channel 1 (Rx), request 3
 * - SPI3: DMA1 Channel 5 (Tx), Channel 0 (Rx), request 0
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
bool spiInternalResetStream(dmaChannelDescriptor_t *descriptor)
{
    if (!descriptor || !descriptor->ref || !spiDmaIrqNumberIsValid(descriptor->irqN)) {
        return false;
    }

    DMA_ARCH_TYPE *channelRegs = (DMA_ARCH_TYPE *)descriptor->ref;
    const spiDmaIrqFence_t irqFence = spiDmaIrqFenceEnter(descriptor, NULL);

    ft32DmaRequestDisable(channelRegs);
    if (ft32DmaIsChannelEnabled(channelRegs)) {
        spiDmaIrqFenceExit(&irqFence, false);
        return false;
    }

    spiDmaInterruptConfig(channelRegs, DISABLE);
    const bool reset = spiDmaInterruptStateMatches(channelRegs, false) &&
        spiDmaClearFlagsAndPending(descriptor) &&
        !ft32DmaIsChannelEnabled(channelRegs);

    if (reset) {
        descriptor->userParam = 0U;
    }
    spiDmaIrqFenceExit(&irqFence, reset);
    return reset;
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

    while (len--) {
        b = txData ? *(txData++) : 0xFF;
        if (!spiWaitForFlagState(instance, SPI_FLAG_TXE, true, true) ||
            (instance->SR & SPI_ERROR_FLAG_MASK)) {
            return false;
        }
        SPI_SendData8(instance, b);

        if (!spiWaitForFlagState(instance, SPI_FLAG_RXNE, true, true) ||
            (instance->SR & SPI_ERROR_FLAG_MASK)) {
            return false;
        }
        b = SPI_ReceiveData8(instance);
        if (rxData) {
            *(rxData++) = b;
        }
    }

    return spiWaitForFlagState(instance, SPI_FLAG_BSY, false, true) &&
        SPI_GetReceptionFIFOStatus(instance) == SPI_ReceptionFIFOStatus_Empty &&
        (instance->SR & SPI_ERROR_FLAG_MASK) == 0U;
}

bool spiInternalRecoverPolled(const extDevice_t *dev)
{
    busDevice_t *bus = dev->bus;
    SPI_TypeDef *instance = (SPI_TypeDef *)bus->busType_u.spi.instance;

    SPI_DMACmd(instance, SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN, DISABLE);
    spiSettleBeforeReset(instance);
    SPI_Cmd(instance, DISABLE);

    for (unsigned int count = 0; count < 8U &&
         SPI_GetReceptionFIFOStatus(instance) != SPI_ReceptionFIFOStatus_Empty; count++) {
        (void)SPI_ReceiveData8(instance);
    }

    if (instance->SR & SPI_FLAG_OVR) {
        (void)instance->DR;
        (void)instance->SR;
    }

    SPI_DeInit(instance);
    SPI_Init(instance, &defaultInit);
    SPI_RxFIFOThresholdConfig(instance, SPI_RxFIFOThreshold_QF);
    spiSetDivisorBRreg(instance, bus->busType_u.spi.speed);

    instance->CR1 &= ~(SPI_CPOL_High | SPI_CPHA_2Edge);
    if (!bus->busType_u.spi.leadingEdge) {
        instance->CR1 |= SPI_CPOL_High | SPI_CPHA_2Edge;
    }

    SPI_DMACmd(instance, SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN, DISABLE);
    SPI_Cmd(instance, ENABLE);

    const uint16_t cr1 = instance->CR1;
    const uint16_t expectedClockMode = bus->busType_u.spi.leadingEdge ?
        (SPI_CPOL_Low | SPI_CPHA_1Edge) : (SPI_CPOL_High | SPI_CPHA_2Edge);

    return SPI_GetReceptionFIFOStatus(instance) == SPI_ReceptionFIFOStatus_Empty &&
        SPI_GetTransmissionFIFOStatus(instance) == SPI_TransmissionFIFOStatus_Empty &&
        (instance->SR & SPI_ERROR_FLAG_MASK) == 0U &&
        (instance->SR & SPI_FLAG_BSY) == 0U &&
        (instance->SR & SPI_FLAG_TXE) != 0U &&
        (instance->CR2 & (SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN)) == 0U &&
        (instance->CR2 & SPI_CR2_DS) == SPI_DataSize_8b &&
        (instance->CR2 & SPI_CR2_FRXTH) == SPI_RxFIFOThreshold_QF &&
        (cr1 & (SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR | SPI_CR1_SPE)) ==
            (SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR | SPI_CR1_SPE) &&
        (cr1 & SPI_CR1_DIRECTION_MASK) == 0U &&
        (cr1 & SPI_CR1_BR) == spiDivisorToBRbits(instance, bus->busType_u.spi.speed) &&
        (cr1 & SPI_CR1_CLOCK_MODE_MASK) == expectedClockMode;
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
bool spiInternalStartDMA(const extDevice_t *dev, bool *hardwareIsolated)
{
    if (hardwareIsolated) {
        *hardwareIsolated = false;
    }
    if (!dev || !dev->bus) {
        return false;
    }

    busDevice_t *bus = dev->bus;
    dmaChannelDescriptor_t *dmaTx = bus->dmaTx;
    dmaChannelDescriptor_t *dmaRx = bus->dmaRx;
    SPI_TypeDef *instance = (SPI_TypeDef *)bus->busType_u.spi.instance;
    volatile busSegment_t *segment = bus->curSegment;

    if (!instance || !dmaTx || !dmaTx->ref || !spiDmaIrqNumberIsValid(dmaTx->irqN) ||
        (dmaRx && (!dmaRx->ref || !spiDmaIrqNumberIsValid(dmaRx->irqN)))) {
        return false;
    }

    DMA_ARCH_TYPE *channelTx = (DMA_ARCH_TYPE *)dmaTx->ref;
    DMA_ARCH_TYPE *channelRx = dmaRx ? (DMA_ARCH_TYPE *)dmaRx->ref : NULL;
    const uint16_t producerMask = dmaRx ? SPI_DMA_REQUEST_MASK : SPI_CR2_TXDMAEN;
    const spiDmaIrqFence_t irqFence = spiDmaIrqFenceEnter(dmaTx, dmaRx);
    bool started = false;
    bool isolated = false;
    bool irqSourceClean = false;

    if (!bus->dmaInitTx || !segment || segment->len <= 0 || segment->len > UINT16_MAX ||
        (dmaRx && !bus->dmaInitRx)) {
        goto exit;
    }

    if (!spiDmaIrqFenceWasEnabled(&irqFence, dmaRx ? dmaRx : dmaTx)) {
        goto exit;
    }

    SPI_DMACmd(instance, SPI_DMA_REQUEST_MASK, DISABLE);
    if (!spiDmaProducerMatches(instance, 0U)) {
        goto exit;
    }

    if (channelRx) {
        ft32DmaRequestDisable(channelRx);
    }
    ft32DmaRequestDisable(channelTx);
    if ((channelRx && ft32DmaIsChannelEnabled(channelRx)) ||
        ft32DmaIsChannelEnabled(channelTx)) {
        goto exit;
    }

    spiDmaInterruptConfig(channelTx, DISABLE);
    if (channelRx) {
        spiDmaInterruptConfig(channelRx, DISABLE);
    }
    if (!spiDmaInterruptStateMatches(channelTx, false) ||
        (channelRx && !spiDmaInterruptStateMatches(channelRx, false))) {
        goto exit;
    }

    if (xDMA_GetFlagStatus(channelTx, DMA_FLAG_ERR) != RESET ||
        (channelRx && xDMA_GetFlagStatus(channelRx, DMA_FLAG_ERR) != RESET)) {
        goto exit;
    }

    if (channelRx && !spiDmaClearFlagsAndPending(dmaRx)) {
        goto exit;
    }
    if (!spiDmaClearFlagsAndPending(dmaTx)) {
        goto exit;
    }

    xDMA_Init(channelTx, bus->dmaInitTx);
    if (channelRx) {
        xDMA_Init(channelRx, bus->dmaInitRx);
    }

    if (!spiDmaDescriptorMatches(channelTx, bus->dmaInitTx, false) ||
        (channelRx && !spiDmaDescriptorMatches(channelRx, bus->dmaInitRx, false))) {
        goto exit;
    }

    if (channelRx) {
        if (!spiDmaEnableTransferInterrupt(channelRx)) {
            goto exit;
        }
    } else {
        if (!spiDmaEnableTransferInterrupt(channelTx)) {
            goto exit;
        }
    }

    if (channelRx && !spiDmaClearFlagsAndPending(dmaRx)) {
        goto exit;
    }
    if (!spiDmaClearFlagsAndPending(dmaTx)) {
        goto exit;
    }
    if (!spiDmaDescriptorMatches(channelTx, bus->dmaInitTx, !channelRx) ||
        (channelRx && !spiDmaDescriptorMatches(channelRx, bus->dmaInitRx, true))) {
        goto exit;
    }

    dmaTx->userParam = (uint32_t)dev;
    if (dmaRx) {
        dmaRx->userParam = (uint32_t)dev;
        xDMA_Cmd(channelRx, ENABLE);
        if (!ft32DmaIsChannelEnabled(channelRx)) {
            goto exit;
        }
    }

    xDMA_Cmd(channelTx, ENABLE);
    if (!ft32DmaIsChannelEnabled(channelTx)) {
        goto exit;
    }

    SPI_DMACmd(instance, producerMask, ENABLE);
    if (!spiDmaProducerMatches(instance, producerMask) ||
        !spiDmaControllerIsEnabled(channelTx) ||
        (channelRx && !spiDmaControllerIsEnabled(channelRx)) ||
        !ft32DmaIsChannelEnabled(channelTx) ||
        (channelRx && !ft32DmaIsChannelEnabled(channelRx))) {
        goto exit;
    }

    started = true;

exit:
    if (!started) {
        isolated = spiDmaAbortStart(dev, &irqSourceClean);
        if (hardwareIsolated) {
            *hardwareIsolated = isolated;
        }
    }
    spiDmaIrqFenceExit(&irqFence, started || (isolated && irqSourceClean));
    return started;
}

/**
 * @brief  Stop DMA transfer for SPI
 * @param  dev: pointer to external device structure
 * 
 * Note: Disables SPI DMA requests and DMA channels
 */
bool spiInternalStopDMA(
    const extDevice_t *dev,
    const dmaChannelDescriptor_t *completionDescriptor,
    bool *hardwareIsolated)
{
    if (hardwareIsolated) {
        *hardwareIsolated = false;
    }
    if (!dev || !dev->bus) {
        return false;
    }

    busDevice_t *bus = dev->bus;
    dmaChannelDescriptor_t *dmaTx = bus->dmaTx;
    dmaChannelDescriptor_t *dmaRx = bus->dmaRx;
    SPI_TypeDef *instance = (SPI_TypeDef *)bus->busType_u.spi.instance;
    dmaChannelDescriptor_t *expectedCompletionDescriptor = dmaRx ? dmaRx : dmaTx;

    if (!instance || !dmaTx || !dmaTx->ref || !spiDmaIrqNumberIsValid(dmaTx->irqN) ||
        (dmaRx && (!dmaRx->ref || !spiDmaIrqNumberIsValid(dmaRx->irqN))) ||
        !completionDescriptor || completionDescriptor != expectedCompletionDescriptor ||
        completionDescriptor->userParam != (uint32_t)dev ||
        dmaTx->userParam != (uint32_t)dev ||
        (dmaRx && dmaRx->userParam != (uint32_t)dev) ||
        !bus->curSegment || bus->curSegment->len <= 0) {
        return false;
    }

    DMA_ARCH_TYPE *channelTx = (DMA_ARCH_TYPE *)dmaTx->ref;
    DMA_ARCH_TYPE *channelRx = dmaRx ? (DMA_ARCH_TYPE *)dmaRx->ref : NULL;
    DMA_ARCH_TYPE *completionChannel = (DMA_ARCH_TYPE *)expectedCompletionDescriptor->ref;
    const spiDmaIrqFence_t irqFence = spiDmaIrqFenceEnter(dmaTx, dmaRx);
    const bool completionObservedAtEntry =
        xDMA_GetFlagStatus(completionChannel, DMA_FLAG_TFR) != RESET;
    bool completionObserved = completionObservedAtEntry;
    bool dmaErrorObserved = xDMA_GetFlagStatus(channelTx, DMA_FLAG_ERR) != RESET ||
        (channelRx && xDMA_GetFlagStatus(channelRx, DMA_FLAG_ERR) != RESET);

    SPI_DMACmd(instance, SPI_DMA_REQUEST_MASK, DISABLE);
    const bool producerStopped = spiDmaProducerMatches(instance, 0U);

    if (channelRx) {
        ft32DmaRequestDisable(channelRx);
    }
    ft32DmaRequestDisable(channelTx);

    const bool channelsStopped =
        (!channelRx || !ft32DmaIsChannelEnabled(channelRx)) &&
        !ft32DmaIsChannelEnabled(channelTx);
    bool wireIdle = false;
    bool controlClean = false;
    bool nonErrorFlagsClean = false;
    bool errorFlagsClean = false;
    bool irqSourceClean = false;
    bool transferValidBeforeErrorCleanup = false;

    if (producerStopped && channelsStopped) {
        wireIdle = spiDmaWireIdleSample(instance, channelRx != NULL);
        completionObserved = completionObserved ||
            xDMA_GetFlagStatus(completionChannel, DMA_FLAG_TFR) != RESET;
        dmaErrorObserved = dmaErrorObserved ||
            xDMA_GetFlagStatus(channelTx, DMA_FLAG_ERR) != RESET ||
            (channelRx && xDMA_GetFlagStatus(channelRx, DMA_FLAG_ERR) != RESET);

        spiDmaInterruptConfig(channelTx, DISABLE);
        if (channelRx) {
            spiDmaInterruptConfig(channelRx, DISABLE);
        }

        controlClean = spiDmaInterruptStateMatches(channelTx, false) &&
            (!channelRx || spiDmaInterruptStateMatches(channelRx, false));
        completionObserved = completionObserved ||
            xDMA_GetFlagStatus(completionChannel, DMA_FLAG_TFR) != RESET;
        dmaErrorObserved = dmaErrorObserved ||
            xDMA_GetFlagStatus(channelTx, DMA_FLAG_ERR) != RESET ||
            (channelRx && xDMA_GetFlagStatus(channelRx, DMA_FLAG_ERR) != RESET);

        nonErrorFlagsClean = true;
        if (channelRx) {
            nonErrorFlagsClean =
                spiDmaClearSelectedFlagsAndPending(dmaRx, SPI_DMA_NON_ERROR_FLAG_MASK) &&
                nonErrorFlagsClean;
        }
        nonErrorFlagsClean =
            spiDmaClearSelectedFlagsAndPending(dmaTx, SPI_DMA_NON_ERROR_FLAG_MASK) &&
            nonErrorFlagsClean;

        dmaErrorObserved = dmaErrorObserved ||
            xDMA_GetFlagStatus(channelTx, DMA_FLAG_ERR) != RESET ||
            (channelRx && xDMA_GetFlagStatus(channelRx, DMA_FLAG_ERR) != RESET);

        transferValidBeforeErrorCleanup = producerStopped &&
            channelsStopped &&
            completionObservedAtEntry &&
            completionObserved &&
            !dmaErrorObserved &&
            wireIdle &&
            controlClean &&
            nonErrorFlagsClean;
    }

    const bool isolated = spiDmaHardwareIsolated(bus);
    if (isolated && !transferValidBeforeErrorCleanup) {
        errorFlagsClean = true;
        if (channelRx) {
            errorFlagsClean =
                spiDmaClearSelectedFlagsAndPending(dmaRx, DMA_IT_ERR) &&
                errorFlagsClean;
        }
        errorFlagsClean =
            spiDmaClearSelectedFlagsAndPending(dmaTx, DMA_IT_ERR) &&
            errorFlagsClean;
    }
    irqSourceClean = controlClean &&
        nonErrorFlagsClean &&
        (transferValidBeforeErrorCleanup ? !dmaErrorObserved : errorFlagsClean);

    const bool stopped = transferValidBeforeErrorCleanup &&
        isolated &&
        irqSourceClean &&
        spiDmaProducerMatches(instance, 0U) &&
        !ft32DmaIsChannelEnabled(channelTx) &&
        (!channelRx || !ft32DmaIsChannelEnabled(channelRx));

    if (isolated) {
        spiDmaClearUserParams(bus);
    }
    if (hardwareIsolated) {
        *hardwareIsolated = isolated;
    }
    spiDmaIrqFenceExit(&irqFence, isolated && irqSourceClean);
    return stopped;
}

void spiInternalServiceDMA(const extDevice_t *dev)
{
    if (!dev || !dev->bus || !dev->bus->dmaTx) {
        return;
    }

    busDevice_t *bus = dev->bus;
    dmaChannelDescriptor_t *completion = bus->dmaRx ? bus->dmaRx : bus->dmaTx;
    DMA_ARCH_TYPE *channel = completion ? (DMA_ARCH_TYPE *)completion->ref : NULL;
    spiDevice_e device = spiDeviceByInstance(bus->busType_u.spi.instance);
    if (!completion || !channel || device == SPIINVALID) {
        return;
    }

    spiDevice_t *state = &spiDevice[device];
    if (completion->userParam != (uint32_t)dev) {
        return;
    }

    if (xDMA_GetFlagStatus(channel, DMA_FLAG_ERR) != RESET) {
        bool isolated = false;
        const bool stopped = spiInternalStopDMA(dev, completion, &isolated);
        spiHandleDmaFailure(dev, stopped && isolated);
        return;
    }

    if (xDMA_GetFlagStatus(channel, DMA_FLAG_TFR) != RESET) {
        if (bus->dmaRx) {
            spiRxIrqHandler(completion);
        } else {
#ifdef USE_TX_IRQ_HANDLER
            spiTxIrqHandler(completion);
#else
            spiRxIrqHandler(completion);
#endif
        }
        return;
    }

    if (!ft32DmaIsChannelEnabled(channel)) {
        return;
    }

    const uint32_t remaining = xDMA_GetCurrDataCounter(channel);
    const uint32_t now = getCycleCounter();
    if (remaining != state->dmaLastProgress) {
        state->dmaLastProgress = remaining;
        state->dmaLastProgressCycles = now;
        return;
    }

    if (cmpTimeCycles(now, state->dmaLastProgressCycles) <=
        (int32_t)clockMicrosToCycles(SPI_TIMEOUT_US)) {
        return;
    }

    bool isolated = false;
    const bool stopped = spiInternalStopDMA(dev, completion, &isolated);
    spiHandleDmaFailure(dev, stopped && isolated);
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
        if (checkSegment->len > UINT16_MAX ||
            ((checkSegment->u.buffers.rxData) && (IS_CCM(checkSegment->u.buffers.rxData) || (bus->dmaRx == (dmaChannelDescriptor_t *)NULL))) ||
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
