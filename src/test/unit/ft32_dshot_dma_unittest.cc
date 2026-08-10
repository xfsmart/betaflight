#include <algorithm>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#ifdef FT32_DSHOT_NO_TELEMETRY_VARIANT

extern "C" {
#include "../../main/platform.h"
#include "drivers/dma.h"
#include "drivers/dshot.h"
#include "drivers/pwm_output.h"
#include "drivers/timer.h"
#include "platform/timer.h"
#include "dshot_dpwm.h"
#include "pwm_output_dshot_shared.h"
}

namespace {

DMA_Channel_TypeDef dmaChannel;
DMA_TypeDef dmaController;
TIM_TypeDef timerRegs;
GPIO_TypeDef gpioRegs;
timerHardware_t timerHardware;
dmaChannelSpec_t dmaSpec;
resourceOwner_t dmaOwner;
volatile timCCR_t fakeCcr;
bool dmaEnabled;
uint16_t shadowCount;
uint32_t enableCount;

uint8_t mockLoadDmaBuffer(uint32_t *buffer, int stride, uint16_t)
{
    for (unsigned i = 0; i < DSHOT_DMA_BUFFER_SIZE; i++) {
        buffer[i * stride] = i;
    }
    return DSHOT_DMA_BUFFER_SIZE;
}

class Ft32DshotNoTelemetryTest : public testing::Test {
protected:
    void SetUp() override
    {
        std::memset(&dmaChannel, 0, sizeof(dmaChannel));
        std::memset(&dmaController, 0, sizeof(dmaController));
        std::memset(&timerRegs, 0, sizeof(timerRegs));
        std::memset(&gpioRegs, 0, sizeof(gpioRegs));
        std::memset(&timerHardware, 0, sizeof(timerHardware));
        std::memset(&dmaSpec, 0, sizeof(dmaSpec));
        std::memset(&dmaOwner, 0, sizeof(dmaOwner));
        std::memset(dmaMotors, 0, sizeof(dmaMotors));
        std::memset(dmaMotorTimers, 0, sizeof(dmaMotorTimers));
        dmaMotorTimerCount = 0U;
        dmaEnabled = false;
        shadowCount = 0U;
        enableCount = 0U;
        fakeCcr = 0U;
        dmaSpec.ref = reinterpret_cast<dmaResource_t *>(&dmaChannel);
        dmaSpec.channel = 7U;
        timerHardware.tim = reinterpret_cast<timerResource_t *>(&timerRegs);
        timerHardware.channel = TIM_Channel_1;
        timerHardware.tag = 1U;
        timerHardware.alternateFunction = 3U;
    }
};

TEST_F(Ft32DshotNoTelemetryTest, ProducerPersistsCanonicalCountAndWriteEnablesOnce)
{
    ASSERT_TRUE(pwmDshotMotorHardwareConfig(
        &timerHardware, 0U, 0U, MOTOR_PROTOCOL_DSHOT600, 0U));
    motorDmaOutput_t *motor = &dmaMotors[0];
    ASSERT_TRUE(motor->configured);
    EXPECT_EQ(DSHOT_DMA_BUFFER_SIZE, motor->dmaInitStruct.BlockTransSize);
    EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(motor->dmaBuffer)),
        motor->dmaInitStruct.SrcAddress);
    EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&fakeCcr)),
        motor->dmaInitStruct.DstAddress);

    loadDmaBuffer = mockLoadDmaBuffer;
    enableCount = 0U;
    pwmWriteDshotInt(0U, 321U);
    EXPECT_EQ(DSHOT_DMA_BUFFER_SIZE, shadowCount);
    EXPECT_EQ(1U, enableCount);
    EXPECT_TRUE(dmaEnabled);
    EXPECT_EQ(motor->timerDmaSource, motor->timer->timerDmaSources);
}

} // namespace

extern "C" {

uint8_t dshotMotorCount;
DSHOT_DMA_BUFFER_ATTRIBUTE DSHOT_DMA_BUFFER_UNIT dshotDmaBuffer[MAX_SUPPORTED_MOTORS][DSHOT_DMA_BUFFER_ALLOC_SIZE];
loadDmaBufferFn *loadDmaBuffer;

uint32_t getDshotHz(motorProtocolTypes_e)
{
    return MOTOR_DSHOT600_HZ;
}

DMA_BaseAddressAndChannelIndex CalBaseAddressAndChannelIndex(DMA_Channel_TypeDef *)
{
    return DMA_BaseAddressAndChannelIndex{&dmaController, 2U};
}

const dmaChannelSpec_t *dmaGetChannelSpecByTimer(const timerHardware_t *)
{
    return &dmaSpec;
}

dmaIdentifier_e dmaGetIdentifier(const dmaResource_t *)
{
    return static_cast<dmaIdentifier_e>(DMA1_ST0_HANDLER);
}

bool dmaAllocate(dmaIdentifier_e, resourceOwner_e, uint8_t)
{
    return true;
}

const resourceOwner_t *dmaGetOwner(dmaIdentifier_e)
{
    return &dmaOwner;
}

void dmaEnable(dmaIdentifier_e)
{
}

void dmaSetHandler(dmaIdentifier_e, dmaCallbackHandlerFuncPtr, uint32_t, uint32_t)
{
}

IO_t IOGetByTag(ioTag_t)
{
    return reinterpret_cast<IO_t>(&gpioRegs);
}

void IOConfigGPIOAF(IO_t, ioConfig_t, uint8_t)
{
}

void DMA_Channel_Cmd(DMA_Channel_TypeDef *, FunctionalState state)
{
    dmaEnabled = state == ENABLE;
    if (state == ENABLE) {
        enableCount++;
    }
}

bool ft32DmaTrySetCurrDataCounter(DMA_ARCH_TYPE *resource, uint16_t count)
{
    DMA_Channel_Cmd(resource, DISABLE);
    if (dmaEnabled) {
        return false;
    }
    shadowCount = count;
    return true;
}

uint8_t ft32DmaIsChannelEnabled(DMA_ARCH_TYPE *)
{
    return dmaEnabled;
}

void ft32DmaCmd(DMA_ARCH_TYPE *resource, FunctionalState state)
{
    DMA_Channel_Cmd(resource, state);
}

void ft32DmaDeInit(DMA_ARCH_TYPE *)
{
}

void DMA_StructInit(DMA_InitTypeDef *init)
{
    std::memset(init, 0, sizeof(*init));
}

void DMA_Init(DMA_Channel_TypeDef *resource, DMA_InitTypeDef *init)
{
    resource->SAR = init->SrcAddress;
    resource->DAR = init->DstAddress;
    resource->CTL = 1U;
    resource->CFG = 1U;
}

void DMA_ITConfig(DMA_Channel_TypeDef *, uint8_t, FunctionalState)
{
}

void DMA_ClearFlagStatus(DMA_Channel_TypeDef *, uint8_t)
{
}

FlagStatus DMA_GetFlagStatus(DMA_Channel_TypeDef *, uint8_t)
{
    return RESET;
}

void TIM_DMACmd(TIM_TypeDef *, uint32_t, FunctionalState)
{
}

void TIM_TimeBaseStructInit(TIM_TimeBaseInitTypeDef *init)
{
    std::memset(init, 0, sizeof(*init));
}

void TIM_TimeBaseInit(TIM_TypeDef *, TIM_TimeBaseInitTypeDef *)
{
}

void TIM_OCStructInit(TIM_OCInitTypeDef *init)
{
    std::memset(init, 0, sizeof(*init));
}

void TIM_Cmd(TIM_TypeDef *, FunctionalState)
{
}

void TIM_CtrlPWMOutputs(TIM_TypeDef *, FunctionalState)
{
}

void TIM_CCxCmd(TIM_TypeDef *, uint32_t, uint32_t)
{
}

void TIM_CCxNCmd(TIM_TypeDef *, uint32_t, uint32_t)
{
}

void TIM_ARRPreloadConfig(TIM_TypeDef *, FunctionalState)
{
}

void timerOCPreloadConfig(TIM_TypeDef *, uint8_t, uint16_t)
{
}

void timerOCInit(TIM_TypeDef *, uint8_t, TIM_OCInitTypeDef *)
{
}

rccPeriphTag_t timerRCC(const timerResource_t *)
{
    return 0U;
}

void RCC_ClockCmd(rccPeriphTag_t, FunctionalState)
{
}

uint32_t timerClock(const timerHardware_t *)
{
    return 168000000U;
}

uint16_t timerDmaSource(uint8_t)
{
    return TIM_DMA_CC1;
}

volatile timCCR_t *timerChCCR(const timerHardware_t *)
{
    return &fakeCcr;
}

int8_t timerGetTIMNumber(const timerHardware_t *)
{
    return 8;
}

bool dshotCommandQueueEmpty(void)
{
    return true;
}

bool dshotCommandOutputIsEnabled(unsigned)
{
    return true;
}

bool dshotCommandIsProcessing(void)
{
    return false;
}

uint8_t dshotCommandGetCurrent(unsigned)
{
    return 0U;
}

uint16_t prepareDshotPacket(dshotProtocolControl_t *)
{
    return 0U;
}

} // extern "C"

#else

extern "C" {
#include "../../main/platform.h"
#include "drivers/dma.h"
#include "drivers/dma_reqmap.h"
#include "drivers/dshot.h"
#include "drivers/pwm_output.h"
#include "drivers/timer.h"
#include "platform/timer.h"
#include "dshot_dpwm.h"
#include "dshot_bitbang_impl.h"
#include "pwm_output_dshot_shared.h"

DMA_InitTypeDef ft32DshotTestOutputDescriptor(const motorDmaOutput_t *motor, const DMA_InitTypeDef *canonical);
DMA_InitTypeDef ft32DshotTestInputDescriptor(const motorDmaOutput_t *motor);
bool ft32DshotTestLoadDescriptor(motorDmaOutput_t *motor, DMA_InitTypeDef *descriptor);
bool ft32DshotTestSetDirectionOutput(motorDmaOutput_t *motor, TIM_OCInitTypeDef *ocInit, const DMA_InitTypeDef *dmaInit);
bool ft32DshotTestRearmBurst(DMA_ARCH_TYPE *dmaRef, uint16_t count, uint32_t srcAddress);
void ft32DshotTestMotorIrq(dmaChannelDescriptor_t *descriptor);
bool ft32DshotTestBitbangTryPreconfigure(bbPort_t *bbPort, uint8_t direction);
void ft32DshotTestBitbangUpdateInit(void);
void ft32DshotTestBitbangWriteInt(uint8_t motorIndex, uint16_t value);
bool ft32DshotTestBitbangDecodeTelemetry(void);
void ft32DshotTestBitbangUpdateComplete(void);
void ft32DshotTestBitbangResetState(void);
uint32_t ft32DshotTestBitbangErrorCount(void);
timeUs_t ft32DshotTestBitbangLastSendUs(void);
uint32_t ft32DshotTestBitbangDecodeCallCount(void);
}

namespace {

enum class Event : uint8_t {
    DmaDisable,
    DmaEnable,
    Counter,
    DmaInit,
    MaskDisable,
    MaskEnable,
    FlagClear,
    TimerDisable,
    TimerEnable,
};

std::vector<Event> events;
std::vector<DMA_Channel_TypeDef *> disabledResources;
std::vector<uint32_t> disabledTimerSources;
std::vector<uint32_t> enabledTimerSources;
DMA_Channel_TypeDef dmaChannel;
DMA_Channel_TypeDef dmaChannel2;
DMA_TypeDef dmaController;
DMA_InitTypeDef capturedInit;
uint16_t shadowCount;
bool dmaEnabled;
bool dmaEnabled2;
bool stickyDisable;
bool stickyDisable2;
bool enableSucceeds;
uint32_t enableCallCount;
uint32_t failEnableCall;
uint32_t disableCallCount;
uint32_t failDisableCall;
uint32_t rawInitCount;
uint32_t blockingDisableCount;
uint32_t maskDisableCount;
uint32_t maskEnableCount;
uint8_t lastDisabledMask;
uint8_t lastEnabledMask;
TIM_TypeDef timerRegs;
GPIO_TypeDef gpioRegs;
timerHardware_t timerHardware;
timerHardware_t timerHardware2;
timeUs_t mockMicros;
uint32_t mockCycles;
bool dmaEnableSawAllFlagsCleared;
bool timerEnableSawDmaAndAllFlagsCleared;
uint32_t timerCmdEnableCount;
uint32_t dmaHandlerInstallCount;
uint32_t channelOutputEnableCount;
volatile timCCR_t fakeCcr;
dmaChannelSpec_t fakeDmaSpec;
resourceOwner_t fakeDmaOwner;

bool &DmaEnabledFor(DMA_Channel_TypeDef *resource)
{
    return resource == &dmaChannel2 ? dmaEnabled2 : dmaEnabled;
}

bool DmaStickyFor(DMA_Channel_TypeDef *resource)
{
    return resource == &dmaChannel2 ? stickyDisable2 : stickyDisable;
}

void ResetMocks()
{
    events.clear();
    disabledResources.clear();
    disabledTimerSources.clear();
    enabledTimerSources.clear();
    std::memset(&dmaChannel, 0, sizeof(dmaChannel));
    std::memset(&dmaChannel2, 0, sizeof(dmaChannel2));
    std::memset(&dmaController, 0, sizeof(dmaController));
    std::memset(&capturedInit, 0, sizeof(capturedInit));
    std::memset(&timerRegs, 0, sizeof(timerRegs));
    std::memset(&gpioRegs, 0, sizeof(gpioRegs));
    std::memset(&timerHardware, 0, sizeof(timerHardware));
    std::memset(&timerHardware2, 0, sizeof(timerHardware2));
    shadowCount = 0U;
    dmaEnabled = false;
    dmaEnabled2 = false;
    stickyDisable = false;
    stickyDisable2 = false;
    enableSucceeds = true;
    enableCallCount = 0U;
    failEnableCall = 0U;
    disableCallCount = 0U;
    failDisableCall = 0U;
    rawInitCount = 0U;
    blockingDisableCount = 0U;
    maskDisableCount = 0U;
    maskEnableCount = 0U;
    lastDisabledMask = 0U;
    lastEnabledMask = 0U;
    mockMicros = 1234U;
    mockCycles = 100U;
    dmaEnableSawAllFlagsCleared = false;
    timerEnableSawDmaAndAllFlagsCleared = false;
    timerCmdEnableCount = 0U;
    dmaHandlerInstallCount = 0U;
    channelOutputEnableCount = 0U;
    fakeCcr = 0U;
    std::memset(&fakeDmaSpec, 0, sizeof(fakeDmaSpec));
    fakeDmaSpec.ref = reinterpret_cast<dmaResource_t *>(&dmaChannel);
    fakeDmaSpec.channel = 7U;
    std::memset(&fakeDmaOwner, 0, sizeof(fakeDmaOwner));
    std::memset(dmaMotors, 0, sizeof(dmaMotors));
    std::memset(dmaMotorTimers, 0, sizeof(dmaMotorTimers));
    std::memset(pwmMotors, 0, sizeof(pwmMotors));
    std::memset(&dshotTelemetryState, 0, sizeof(dshotTelemetryState));
    std::memset(dshotTelemetryQuality, 0, sizeof(dshotTelemetryQuality));
    dshotMotorCount = 0U;
    pwmMotorCount = 0U;
    dmaMotorTimerCount = 0U;
    std::memset(bbPorts, 0, sizeof(bbPorts));
    std::memset(bbPacers, 0, sizeof(bbPacers));
    std::memset(bbMotors, 0, sizeof(bbMotors));
    usedMotorPorts = 0;
    usedMotorPacers = 0;
    inputStampUs = 0U;
    useDshotTelemetry = false;
    useBurstDshot = false;
    ft32DshotTestBitbangResetState();
}

size_t CountEvent(Event event)
{
    return static_cast<size_t>(std::count(events.begin(), events.end(), event));
}

void SetTerminalFlags(bool transferComplete, bool transferError)
{
    const uint32_t bit = 1U << 2U;
    dmaController.STATUSTFR = transferComplete ? bit : 0U;
    dmaController.STATUSERR = transferError ? bit : 0U;
}

void ExpectAllTerminalFlagsCleared()
{
    const uint32_t bit = 1U << 2U;
    EXPECT_EQ(bit, dmaController.CLEARTFR);
    EXPECT_EQ(bit, dmaController.CLEARBLOCK);
    EXPECT_EQ(bit, dmaController.CLEARSRCTRAN);
    EXPECT_EQ(bit, dmaController.CLEARDSTTRAN);
    EXPECT_EQ(bit, dmaController.CLEARERR);
}

bool AllTerminalFlagsAreCleared()
{
    const uint32_t bit = 1U << 2U;
    return dmaController.CLEARTFR == bit &&
        dmaController.CLEARBLOCK == bit &&
        dmaController.CLEARSRCTRAN == bit &&
        dmaController.CLEARDSTTRAN == bit &&
        dmaController.CLEARERR == bit;
}

dmaChannelDescriptor_t MakeIrqDescriptor(uint32_t userParam)
{
    dmaChannelDescriptor_t descriptor{};
    descriptor.ref = reinterpret_cast<dmaResource_t *>(&dmaChannel);
    descriptor.userParam = userParam;
    return descriptor;
}

bbPort_t *ConfigureBitbangPort(unsigned index)
{
    bbPort_t *port = &bbPorts[index];
    timerHardware_t *hardware = index == 0U ? &timerHardware : &timerHardware2;
    hardware->tim = reinterpret_cast<timerResource_t *>(&timerRegs);
    hardware->channel = index == 0U ? TIM_Channel_1 : TIM_Channel_2;
    port->timhw = hardware;
    port->dmaResource = reinterpret_cast<dmaResource_t *>(index == 0U ? &dmaChannel : &dmaChannel2);
    port->dmaChannel = 7U;
    port->dmaSource = index == 0U ? TIM_DMA_CC1 : TIM_DMA_CC2;
    port->gpio = &gpioRegs;
    port->gpioModeMask = 0x3U;
    port->gpioModeInput = 0U;
    port->gpioModeOutput = 1U;
    port->gpioIdleBSRR = 0x10000U;
    port->outputARR = 20U;
    port->inputARR = 8U;
    port->portOutputBuffer = &bbOutputBuffer[index * MOTOR_DSHOT_BUF_CACHE_ALIGN_LENGTH];
    port->portOutputCount = MOTOR_DSHOT_BUF_LENGTH;
    port->portInputBuffer = &bbInputBuffer[index * DSHOT_BB_PORT_IP_BUF_CACHE_ALIGN_LENGTH];
    port->portInputCount = DSHOT_BB_PORT_IP_BUF_LENGTH;
    EXPECT_TRUE(ft32DshotTestBitbangTryPreconfigure(port, DSHOT_BITBANG_DIRECTION_OUTPUT));
    EXPECT_TRUE(ft32DshotTestBitbangTryPreconfigure(port, DSHOT_BITBANG_DIRECTION_INPUT));
    port->direction = DSHOT_BITBANG_DIRECTION_OUTPUT;
    return port;
}

motorDmaOutput_t MakeDirectMotor(DMA_InitTypeDef canonical)
{
    motorDmaOutput_t motor{};
    timerHardware.tim = reinterpret_cast<timerResource_t *>(&timerRegs);
    timerHardware.channel = TIM_Channel_1;
    motor.timerHardware = &timerHardware;
    motor.index = 0U;
    motor.dmaRef = reinterpret_cast<dmaResource_t *>(&dmaChannel);
    motor.timerDmaSource = TIM_DMA_CC1;
    motor.dmaInitStruct = canonical;
    return motor;
}

DMA_InitTypeDef CanonicalDirectDescriptor(uint16_t count = DSHOT_DMA_BUFFER_SIZE)
{
    DMA_InitTypeDef descriptor{};
    descriptor.SrcAddress = 0x20001000U;
    descriptor.DstAddress = 0x40010434U;
    descriptor.BlockTransSize = count;
    descriptor.DstHsIfPeriphSel = 7U;
    return descriptor;
}

void ExpectDirectOutput(const DMA_InitTypeDef &descriptor, uint16_t count)
{
    EXPECT_EQ(0x20001000U, descriptor.SrcAddress);
    EXPECT_EQ(0x40010434U, descriptor.DstAddress);
    EXPECT_EQ(count, descriptor.BlockTransSize);
    EXPECT_EQ(DMA_SRCMASTER1_DSTMASTER2, descriptor.SrcDstMasterSel);
    EXPECT_EQ(DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA, descriptor.TransferTypeFlowCtl);
    EXPECT_EQ(DMA_SRC_ADDRMODE_INC, descriptor.SrcAddrMode);
    EXPECT_EQ(DMA_DST_ADDRMODE_HOLD, descriptor.DstAddrMode);
    EXPECT_EQ(DMA_SRC_TRANSFERWIDTH_32BITS, descriptor.SrcTransferWidth);
    EXPECT_EQ(DMA_DST_TRANSFERWIDTH_32BITS, descriptor.DstTransferWidth);
    EXPECT_EQ(DMA_SRCHSSEL_SOFTWARE, descriptor.SrcHsSel);
    EXPECT_EQ(DMA_DSTHSSEL_HARDWARE, descriptor.DstHsSel);
    EXPECT_EQ(0U, descriptor.SrcHsIfPeriphSel);
    EXPECT_EQ(7U, descriptor.DstHsIfPeriphSel);
    EXPECT_EQ(2U, descriptor.SrcHardwareInterface);
    EXPECT_EQ(2U, descriptor.DstHardwareInterface);
}

class Ft32DshotDmaTest : public testing::Test {
protected:
    void SetUp() override
    {
        ResetMocks();
    }
};

TEST_F(Ft32DshotDmaTest, DirectDescriptorsAreCompleteAndCanonicalStaysOutput)
{
    const DMA_InitTypeDef canonical = CanonicalDirectDescriptor();
    motorDmaOutput_t motor = MakeDirectMotor(canonical);

    const DMA_InitTypeDef output = ft32DshotTestOutputDescriptor(&motor, &motor.dmaInitStruct);
    const DMA_InitTypeDef input = ft32DshotTestInputDescriptor(&motor);

    ExpectDirectOutput(output, DSHOT_DMA_BUFFER_SIZE);
    EXPECT_EQ(0x40010434U, input.SrcAddress);
    EXPECT_EQ(0x20001000U, input.DstAddress);
    EXPECT_EQ(GCR_TELEMETRY_INPUT_LEN, input.BlockTransSize);
    EXPECT_EQ(DMA_SRCMASTER2_DSTMASTER1, input.SrcDstMasterSel);
    EXPECT_EQ(DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA, input.TransferTypeFlowCtl);
    EXPECT_EQ(DMA_SRC_ADDRMODE_HOLD, input.SrcAddrMode);
    EXPECT_EQ(DMA_DST_ADDRMODE_INC, input.DstAddrMode);
    EXPECT_EQ(DMA_SRC_TRANSFERWIDTH_32BITS, input.SrcTransferWidth);
    EXPECT_EQ(DMA_DST_TRANSFERWIDTH_32BITS, input.DstTransferWidth);
    EXPECT_EQ(2U, input.SrcHardwareInterface);
    EXPECT_EQ(2U, input.DstHardwareInterface);
    EXPECT_EQ(DMA_SRCHSSEL_HARDWARE, input.SrcHsSel);
    EXPECT_EQ(DMA_DSTHSSEL_SOFTWARE, input.DstHsSel);
    EXPECT_EQ(7U, input.SrcHsIfPeriphSel);
    EXPECT_EQ(0U, input.DstHsIfPeriphSel);
    EXPECT_EQ(0, std::memcmp(&canonical, &motor.dmaInitStruct, sizeof(canonical)));

    for (unsigned iteration = 0; iteration < 100U; iteration++) {
        const DMA_InitTypeDef nextInput = ft32DshotTestInputDescriptor(&motor);
        const DMA_InitTypeDef nextOutput = ft32DshotTestOutputDescriptor(&motor, &motor.dmaInitStruct);
        EXPECT_EQ(input.SrcAddress, nextInput.SrcAddress);
        EXPECT_EQ(input.DstAddress, nextInput.DstAddress);
        EXPECT_EQ(output.SrcAddress, nextOutput.SrcAddress);
        EXPECT_EQ(output.DstAddress, nextOutput.DstAddress);
        EXPECT_EQ(0, std::memcmp(&canonical, &motor.dmaInitStruct, sizeof(canonical)));
    }
}

TEST_F(Ft32DshotDmaTest, DescriptorLoadUsesSameBoundaryCountAndOnlyTerminalMasks)
{
    motorDmaOutput_t motor = MakeDirectMotor(CanonicalDirectDescriptor());

    for (const uint16_t count : {uint16_t{1U}, uint16_t{UINT16_MAX}}) {
        ResetMocks();
        const DMA_InitTypeDef canonical = CanonicalDirectDescriptor(count);
        DMA_InitTypeDef descriptor = ft32DshotTestOutputDescriptor(&motor, &canonical);
        dmaController.CHSEL = (uint64_t{1U} << (2U * 3U)) | (uint64_t{3U} << (5U * 3U));
        ASSERT_TRUE(ft32DshotTestLoadDescriptor(&motor, &descriptor));
        EXPECT_EQ(count, shadowCount);
        EXPECT_EQ(count, capturedInit.BlockTransSize);
        EXPECT_EQ(1U, rawInitCount);
        EXPECT_EQ(1U, maskDisableCount);
        EXPECT_EQ(1U, maskEnableCount);
        EXPECT_EQ(DMA_IT_TFR | DMA_IT_BLOCK | DMA_IT_SRC | DMA_IT_DST | DMA_IT_ERR, lastDisabledMask);
        EXPECT_EQ(DMA_IT_TFR | DMA_IT_ERR, lastEnabledMask);
        EXPECT_EQ(7U, (dmaController.CHSEL >> (2U * 3U)) & 0x7U);
        EXPECT_EQ(3U, (dmaController.CHSEL >> (5U * 3U)) & 0x7U);
        EXPECT_EQ(0U, blockingDisableCount);
    }

    ResetMocks();
    stickyDisable = true;
    dmaEnabled = true;
    const DMA_InitTypeDef canonical = CanonicalDirectDescriptor();
    DMA_InitTypeDef descriptor = ft32DshotTestOutputDescriptor(&motor, &canonical);
    EXPECT_FALSE(ft32DshotTestLoadDescriptor(&motor, &descriptor));
    EXPECT_EQ(0U, rawInitCount);
    EXPECT_EQ(0U, maskDisableCount);
    EXPECT_EQ(0U, maskEnableCount);
    EXPECT_EQ(0U, blockingDisableCount);
}

TEST_F(Ft32DshotDmaTest, BitbangPublicPreconfigureRemainsVoidAndBuildsBothDirections)
{
    static_assert(std::is_same<decltype(&bbDMAPreconfigure), void (*)(bbPort_t *, uint8_t)>::value,
                  "bbDMAPreconfigure must keep its public void ABI");

    uint32_t outputBuffer[64]{};
    uint16_t inputBuffer[64]{};
    bbPort_t port{};
    port.dmaResource = reinterpret_cast<dmaResource_t *>(&dmaChannel);
    port.dmaChannel = 7U;
    port.gpio = &gpioRegs;
    port.portOutputBuffer = outputBuffer;
    port.portOutputCount = 51U;
    port.portInputBuffer = inputBuffer;
    port.portInputCount = 48U;

    ASSERT_TRUE(ft32DshotTestBitbangTryPreconfigure(&port, DSHOT_BITBANG_DIRECTION_OUTPUT));
    EXPECT_NE(0U, port.dmaRegOutput.CTL);
    EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(outputBuffer)), port.outputDmaInit.SrcAddress);
    EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&gpioRegs.BSRR)), port.outputDmaInit.DstAddress);
    EXPECT_EQ(DMA_SRCMASTER1_DSTMASTER2, port.outputDmaInit.SrcDstMasterSel);
    EXPECT_EQ(DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA, port.outputDmaInit.TransferTypeFlowCtl);
    EXPECT_EQ(DMA_SRC_ADDRMODE_INC, port.outputDmaInit.SrcAddrMode);
    EXPECT_EQ(DMA_DST_ADDRMODE_HOLD, port.outputDmaInit.DstAddrMode);
    EXPECT_EQ(DMA_SRCHSSEL_SOFTWARE, port.outputDmaInit.SrcHsSel);
    EXPECT_EQ(DMA_DSTHSSEL_HARDWARE, port.outputDmaInit.DstHsSel);
    EXPECT_EQ(DMA_SRC_TRANSFERWIDTH_32BITS, port.outputDmaInit.SrcTransferWidth);
    EXPECT_EQ(DMA_DST_TRANSFERWIDTH_32BITS, port.outputDmaInit.DstTransferWidth);
    EXPECT_EQ(51U, port.outputDmaInit.BlockTransSize);
    EXPECT_EQ(2U, port.outputDmaInit.SrcHardwareInterface);
    EXPECT_EQ(2U, port.outputDmaInit.DstHardwareInterface);
    EXPECT_EQ(0U, port.outputDmaInit.SrcHsIfPeriphSel);
    EXPECT_EQ(7U, port.outputDmaInit.DstHsIfPeriphSel);

    ASSERT_TRUE(ft32DshotTestBitbangTryPreconfigure(&port, DSHOT_BITBANG_DIRECTION_INPUT));
    EXPECT_NE(0U, port.dmaRegInput.CTL);
    EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&gpioRegs.IDR)), port.inputDmaInit.SrcAddress);
    EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(inputBuffer)), port.inputDmaInit.DstAddress);
    EXPECT_EQ(DMA_SRCMASTER2_DSTMASTER1, port.inputDmaInit.SrcDstMasterSel);
    EXPECT_EQ(DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA, port.inputDmaInit.TransferTypeFlowCtl);
    EXPECT_EQ(DMA_SRC_ADDRMODE_HOLD, port.inputDmaInit.SrcAddrMode);
    EXPECT_EQ(DMA_DST_ADDRMODE_INC, port.inputDmaInit.DstAddrMode);
    EXPECT_EQ(DMA_SRCHSSEL_HARDWARE, port.inputDmaInit.SrcHsSel);
    EXPECT_EQ(DMA_DSTHSSEL_SOFTWARE, port.inputDmaInit.DstHsSel);
    EXPECT_EQ(DMA_SRC_TRANSFERWIDTH_16BITS, port.inputDmaInit.SrcTransferWidth);
    EXPECT_EQ(DMA_DST_TRANSFERWIDTH_16BITS, port.inputDmaInit.DstTransferWidth);
    EXPECT_EQ(48U, port.inputDmaInit.BlockTransSize);
    EXPECT_EQ(2U, port.inputDmaInit.SrcHardwareInterface);
    EXPECT_EQ(2U, port.inputDmaInit.DstHardwareInterface);
    EXPECT_EQ(7U, port.inputDmaInit.SrcHsIfPeriphSel);
    EXPECT_EQ(0U, port.inputDmaInit.DstHsIfPeriphSel);

    stickyDisable = true;
    dmaEnabled = true;
    bbDMAPreconfigure(&port, DSHOT_BITBANG_DIRECTION_OUTPUT);
    EXPECT_EQ(0U, port.dmaRegOutput.CTL);
    EXPECT_EQ(0U, blockingDisableCount);
}

TEST_F(Ft32DshotDmaTest, DirectIrqNoFlagsReturnsWithoutMutation)
{
    dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaEnabled = true;
    const dmaChannelDescriptor_t descriptor = MakeIrqDescriptor(0U);

    ft32DshotTestMotorIrq(const_cast<dmaChannelDescriptor_t *>(&descriptor));

    EXPECT_TRUE(events.empty());
    EXPECT_TRUE(dmaEnabled);
    EXPECT_EQ(0U, dmaController.CLEARTFR);
}

TEST_F(Ft32DshotDmaTest, DirectIrqCleanOutputTransitionsToInputAndRestartsTimerLast)
{
    dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaMotors[0].isInput = false;
    useDshotTelemetry = true;
    dmaEnabled = true;
    SetTerminalFlags(true, false);
    dmaChannelDescriptor_t descriptor = MakeIrqDescriptor(0U);

    ft32DshotTestMotorIrq(&descriptor);

    EXPECT_TRUE(dmaMotors[0].isInput);
    EXPECT_EQ(GCR_TELEMETRY_INPUT_LEN, capturedInit.BlockTransSize);
    EXPECT_EQ(1U, rawInitCount);
    EXPECT_TRUE(dmaEnabled);
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(Event::TimerDisable, events.front());
    EXPECT_EQ(Event::TimerEnable, events.back());
    EXPECT_EQ(1U, CountEvent(Event::TimerEnable));
    EXPECT_EQ(0U, blockingDisableCount);
    EXPECT_EQ(mockMicros, inputStampUs);
    EXPECT_TRUE(dmaEnableSawAllFlagsCleared);
    EXPECT_TRUE(timerEnableSawDmaAndAllFlagsCleared);
    ExpectAllTerminalFlagsCleared();
}

TEST_F(Ft32DshotDmaTest, DirectMotorsShareFirstCaptureEpochStamp)
{
    dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaMotors[1] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaMotors[1].index = 1U;
    useDshotTelemetry = true;
    mockMicros = 1000U;
    dmaEnabled = true;
    SetTerminalFlags(true, false);
    dmaChannelDescriptor_t motor0Descriptor = MakeIrqDescriptor(0U);

    ft32DshotTestMotorIrq(&motor0Descriptor);
    ASSERT_TRUE(dmaMotors[0].isInput);
    ASSERT_EQ(1000U, inputStampUs);

    mockMicros = 2000U;
    dmaEnabled = true;
    SetTerminalFlags(true, false);
    dmaChannelDescriptor_t motor1Descriptor = MakeIrqDescriptor(1U);
    ft32DshotTestMotorIrq(&motor1Descriptor);

    EXPECT_TRUE(dmaMotors[1].isInput);
    EXPECT_EQ(1000U, inputStampUs);
    EXPECT_EQ(2U, CountEvent(Event::TimerEnable));
    EXPECT_EQ(0U, blockingDisableCount);
}

TEST_F(Ft32DshotDmaTest, DirectIrqOutputWithoutTelemetryAndInputCompleteStayQuiescent)
{
    for (const bool wasInput : {false, true}) {
        ResetMocks();
        dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
        dmaMotors[0].isInput = wasInput;
        useDshotTelemetry = wasInput;
        dmaEnabled = true;
        SetTerminalFlags(true, false);
        dmaChannelDescriptor_t descriptor = MakeIrqDescriptor(0U);

        ft32DshotTestMotorIrq(&descriptor);

        EXPECT_FALSE(dmaEnabled);
        EXPECT_EQ(0U, rawInitCount);
        EXPECT_EQ(0U, CountEvent(Event::TimerEnable));
        EXPECT_EQ(0U, blockingDisableCount);
        ExpectAllTerminalFlagsCleared();
    }
}

TEST_F(Ft32DshotDmaTest, DirectIrqErrorMixedStickyAndEnableFailureFailClosed)
{
    struct Scenario {
        bool tfr;
        bool error;
        bool sticky;
        bool enableOk;
        uint32_t expectedInit;
    };
    const Scenario scenarios[] = {
        {false, true, false, true, 0U},
        {true, true, false, true, 0U},
        {true, false, true, true, 0U},
        {true, false, false, false, 1U},
    };

    for (const Scenario &scenario : scenarios) {
        ResetMocks();
        dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
        useDshotTelemetry = true;
        dmaEnabled = true;
        stickyDisable = scenario.sticky;
        enableSucceeds = scenario.enableOk;
        SetTerminalFlags(scenario.tfr, scenario.error);
        dmaChannelDescriptor_t descriptor = MakeIrqDescriptor(0U);

        ft32DshotTestMotorIrq(&descriptor);

        EXPECT_EQ(scenario.expectedInit, rawInitCount);
        EXPECT_EQ(0U, CountEvent(Event::TimerEnable));
        EXPECT_FALSE(dmaMotors[0].isInput);
        EXPECT_EQ(0U, inputStampUs);
        EXPECT_EQ(0U, blockingDisableCount);
        ExpectAllTerminalFlagsCleared();
    }
}

TEST_F(Ft32DshotDmaTest, BitbangIrqNoFlagsReturnsWithoutMutation)
{
    bbPort_t *port = ConfigureBitbangPort(0U);
    events.clear();
    dmaEnabled = true;
    dmaChannelDescriptor_t descriptor = MakeIrqDescriptor(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(port)));

    bbDMAIrqHandler(&descriptor);

    EXPECT_TRUE(events.empty());
    EXPECT_TRUE(dmaEnabled);
    EXPECT_EQ(DSHOT_BITBANG_DIRECTION_OUTPUT, port->direction);
}

TEST_F(Ft32DshotDmaTest, BitbangIrqCleanOutputTransitionsAndInputCompleteDoesNotReenter)
{
    bbPort_t *port = ConfigureBitbangPort(0U);
    events.clear();
    ft32DshotTestBitbangResetState();
    useDshotTelemetry = true;
    dmaEnabled = true;
    SetTerminalFlags(true, false);
    dmaChannelDescriptor_t descriptor = MakeIrqDescriptor(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(port)));

    bbDMAIrqHandler(&descriptor);

    EXPECT_EQ(DSHOT_BITBANG_DIRECTION_INPUT, port->direction);
    EXPECT_FALSE(port->inputActive);
    EXPECT_TRUE(port->telemetryPending);
    EXPECT_TRUE(dmaEnabled);
    EXPECT_EQ(Event::TimerEnable, events.back());
    EXPECT_TRUE(dmaEnableSawAllFlagsCleared);
    EXPECT_TRUE(timerEnableSawDmaAndAllFlagsCleared);
    EXPECT_EQ(0U, ft32DshotTestBitbangErrorCount());
    ExpectAllTerminalFlagsCleared();

    events.clear();
    SetTerminalFlags(true, false);
    bbDMAIrqHandler(&descriptor);
    EXPECT_FALSE(port->telemetryPending);
    EXPECT_TRUE(port->inputActive);
    EXPECT_FALSE(dmaEnabled);
    EXPECT_EQ(0U, CountEvent(Event::TimerEnable));
    EXPECT_EQ(DSHOT_BITBANG_DIRECTION_INPUT, port->direction);

    usedMotorPorts = 1;
    dshotMotorCount = 2U;
    bbMotors[0].bbPort = port;
    bbMotors[0].pinIndex = 0U;
    bbMotors[1].bbPort = port;
    bbMotors[1].pinIndex = 1U;
    EXPECT_TRUE(ft32DshotTestBitbangDecodeTelemetry());
    EXPECT_EQ(2U, ft32DshotTestBitbangDecodeCallCount());
    EXPECT_FALSE(port->inputActive);
    EXPECT_TRUE(ft32DshotTestBitbangDecodeTelemetry());
    EXPECT_EQ(2U, ft32DshotTestBitbangDecodeCallCount());
}

TEST_F(Ft32DshotDmaTest, BitbangIrqTelemetryOffErrorsMixedAndStickyFailClosed)
{
    struct Scenario {
        bool telemetry;
        bool tfr;
        bool error;
        bool sticky;
        uint32_t expectedErrors;
    };
    const Scenario scenarios[] = {
        {false, true, false, false, 0U},
        {true, false, true, false, 1U},
        {true, true, true, false, 1U},
        {true, true, false, true, 1U},
    };

    for (const Scenario &scenario : scenarios) {
        ResetMocks();
        bbPort_t *port = ConfigureBitbangPort(0U);
        events.clear();
        ft32DshotTestBitbangResetState();
        useDshotTelemetry = scenario.telemetry;
        dmaEnabled = true;
        stickyDisable = scenario.sticky;
        SetTerminalFlags(scenario.tfr, scenario.error);
        dmaChannelDescriptor_t descriptor = MakeIrqDescriptor(
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(port)));

        bbDMAIrqHandler(&descriptor);

        EXPECT_EQ(DSHOT_BITBANG_DIRECTION_OUTPUT, port->direction);
        EXPECT_EQ(0U, CountEvent(Event::TimerEnable));
        EXPECT_EQ(scenario.expectedErrors, ft32DshotTestBitbangErrorCount());
        EXPECT_EQ(0U, blockingDisableCount);
        ExpectAllTerminalFlagsCleared();
    }
}

TEST_F(Ft32DshotDmaTest, BitbangInputErrorInvalidatesCaptureAndDecoderSkipsPort)
{
    bbPort_t *port = ConfigureBitbangPort(0U);
    usedMotorPorts = 1;
    dshotMotorCount = 1U;
    bbMotors[0].bbPort = port;
    bbMotors[0].pinIndex = 0U;
    port->direction = DSHOT_BITBANG_DIRECTION_INPUT;
    port->inputActive = true;
    port->telemetryPending = true;
    useDshotTelemetry = true;
    dmaEnabled = true;
    SetTerminalFlags(true, true);
    dmaChannelDescriptor_t descriptor = MakeIrqDescriptor(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(port)));

    bbDMAIrqHandler(&descriptor);
    EXPECT_FALSE(port->inputActive);
    EXPECT_FALSE(port->telemetryPending);
    EXPECT_TRUE(ft32DshotTestBitbangDecodeTelemetry());
    EXPECT_EQ(0U, ft32DshotTestBitbangDecodeCallCount());
    EXPECT_EQ(0U, dshotTelemetryState.readCount);
}

TEST_F(Ft32DshotDmaTest, BitbangUpdatePublishesAllConsumersBeforePacerForOneHundredFrames)
{
    bbPort_t *port0 = ConfigureBitbangPort(0U);
    const dmaRegCache_t expected0 = port0->dmaRegOutput;
    bbPort_t *port1 = ConfigureBitbangPort(1U);
    const dmaRegCache_t expected1 = port1->dmaRegOutput;
    usedMotorPorts = 2;
    usedMotorPacers = 1;
    bbPacers[0].tim = reinterpret_cast<timerResource_t *>(&timerRegs);
    bbPacers[0].dmaSources = TIM_DMA_CC1 | TIM_DMA_CC2;
    bbMotors[0].configured = true;
    bbMotors[0].bbPort = port0;
    bbMotors[0].pinIndex = 0U;
    bbMotors[1].configured = true;
    bbMotors[1].bbPort = port1;
    bbMotors[1].pinIndex = 1U;

    for (unsigned frame = 0; frame < 100U; frame++) {
        events.clear();
        enabledTimerSources.clear();
        enableCallCount = 0U;
        mockMicros = 2000U + frame;
        ft32DshotTestBitbangUpdateInit();
        ft32DshotTestBitbangWriteInt(0U, 100U + frame);
        ft32DshotTestBitbangWriteInt(1U, 200U + frame);
        ft32DshotTestBitbangUpdateComplete();

        ASSERT_GE(CountEvent(Event::TimerDisable), 1U);
        ASSERT_EQ(1U, CountEvent(Event::TimerEnable));
        ASSERT_EQ(Event::TimerDisable, events.front());
        ASSERT_EQ(Event::TimerEnable, events.back());
        const auto firstEnable = std::find(events.begin(), events.end(), Event::DmaEnable);
        ASSERT_NE(events.end(), firstEnable);
        EXPECT_EQ(2U, static_cast<size_t>(std::count(events.begin(), firstEnable, Event::Counter)));
        EXPECT_EQ(2U, enableCallCount);
        ASSERT_FALSE(enabledTimerSources.empty());
        EXPECT_EQ(TIM_DMA_CC1 | TIM_DMA_CC2, enabledTimerSources.back());
        EXPECT_EQ(100U + frame, bbMotors[0].protocolControl.value);
        EXPECT_EQ(200U + frame, bbMotors[1].protocolControl.value);
        EXPECT_EQ(mockMicros, ft32DshotTestBitbangLastSendUs());
        EXPECT_EQ(expected0.SAR, port0->dmaRegOutput.SAR);
        EXPECT_EQ(expected0.CTL, port0->dmaRegOutput.CTL);
        EXPECT_EQ(expected1.SAR, port1->dmaRegOutput.SAR);
        EXPECT_EQ(expected1.CTL, port1->dmaRegOutput.CTL);
        EXPECT_EQ(0U, blockingDisableCount);
    }
}

TEST_F(Ft32DshotDmaTest, BitbangUpdateEnableFailureStopsEveryPortAndNeverStartsPacer)
{
    bbPort_t *port0 = ConfigureBitbangPort(0U);
    bbPort_t *port1 = ConfigureBitbangPort(1U);
    usedMotorPorts = 2;
    usedMotorPacers = 1;
    bbPacers[0].tim = reinterpret_cast<timerResource_t *>(&timerRegs);
    bbPacers[0].dmaSources = TIM_DMA_CC1 | TIM_DMA_CC2;
    bbMotors[0].configured = true;
    bbMotors[0].bbPort = port0;
    bbMotors[1].configured = true;
    bbMotors[1].bbPort = port1;

    ft32DshotTestBitbangUpdateInit();
    ft32DshotTestBitbangWriteInt(0U, 300U);
    ft32DshotTestBitbangWriteInt(1U, 400U);
    events.clear();
    disabledResources.clear();
    enabledTimerSources.clear();
    enableCallCount = 0U;
    failEnableCall = 2U;
    ft32DshotTestBitbangUpdateComplete();

    EXPECT_EQ(0U, CountEvent(Event::TimerEnable));
    EXPECT_TRUE(enabledTimerSources.empty());
    EXPECT_EQ(0U, ft32DshotTestBitbangLastSendUs());
    EXPECT_EQ(2U, CountEvent(Event::DmaDisable));
    EXPECT_NE(disabledResources.end(),
        std::find(disabledResources.begin(), disabledResources.end(), &dmaChannel));
    EXPECT_NE(disabledResources.end(),
        std::find(disabledResources.begin(), disabledResources.end(), &dmaChannel2));
    EXPECT_FALSE(dmaEnabled);
    EXPECT_FALSE(dmaEnabled2);
    EXPECT_EQ(0U, blockingDisableCount);
}

TEST_F(Ft32DshotDmaTest, InitialDirectionAndBurstEnableReadbackFailuresPropagateFalse)
{
    motorDmaOutput_t motor = MakeDirectMotor(CanonicalDirectDescriptor());
    motor.isInput = true;
    TIM_OCInitTypeDef ocInit{};
    stickyDisable = true;
    dmaEnabled = true;

    EXPECT_FALSE(ft32DshotTestSetDirectionOutput(&motor, &ocInit, &motor.dmaInitStruct));
    EXPECT_TRUE(motor.isInput);
    EXPECT_EQ(0U, rawInitCount);
    EXPECT_EQ(0U, CountEvent(Event::TimerEnable));

    ResetMocks();
    enableSucceeds = false;
    EXPECT_FALSE(ft32DshotTestRearmBurst(reinterpret_cast<DMA_ARCH_TYPE *>(&dmaChannel), 17U, 0x20002000U));
    EXPECT_EQ(17U, shadowCount);
    EXPECT_EQ(1U, CountEvent(Event::DmaEnable));
    EXPECT_FALSE(dmaEnabled);
    EXPECT_EQ(0U, blockingDisableCount);
}

TEST_F(Ft32DshotDmaTest, DirectInputEnableFailureSkipsStaleDecodeAndRecoversOutput)
{
    DSHOT_DMA_BUFFER_UNIT staleBuffer[DSHOT_DMA_BUFFER_ALLOC_SIZE]{};
    dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaMotors[0].dmaBuffer = staleBuffer;
    dshotMotorCount = 1U;
    useDshotTelemetry = true;
    dmaEnabled = true;
    enableSucceeds = false;
    SetTerminalFlags(true, false);
    dmaChannelDescriptor_t descriptor = MakeIrqDescriptor(0U);

    ft32DshotTestMotorIrq(&descriptor);
    ASSERT_FALSE(dmaMotors[0].isInput);
    ASSERT_EQ(0U, inputStampUs);
    shadowCount = 0U;

    dshotTelemetryState.readCount = 7U;
    dshotTelemetryState.invalidPacketCount = 3U;
    dshotTelemetryState.motorState[0].rawValue = 0x345U;
    enableSucceeds = true;
    const uint32_t initBeforeDecode = rawInitCount;

    EXPECT_TRUE(pwmTelemetryDecode());
    EXPECT_EQ(7U, dshotTelemetryState.readCount);
    EXPECT_EQ(3U, dshotTelemetryState.invalidPacketCount);
    EXPECT_EQ(0x345U, dshotTelemetryState.motorState[0].rawValue);
    EXPECT_EQ(initBeforeDecode + 1U, rawInitCount);
    EXPECT_FALSE(dmaMotors[0].isInput);
    EXPECT_EQ(0U, inputStampUs);
}

TEST_F(Ft32DshotDmaTest, DirectInputErrorInvalidatesCaptureBeforeDecoderCanPublish)
{
    DSHOT_DMA_BUFFER_UNIT staleBuffer[DSHOT_DMA_BUFFER_ALLOC_SIZE]{};
    dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaMotors[0].dmaBuffer = staleBuffer;
    dmaMotors[0].isInput = true;
    dmaMotors[0].dshotTelemetryDeadtimeUs = 0U;
    dshotMotorCount = 1U;
    useDshotTelemetry = true;
    dmaEnabled = true;
    SetTerminalFlags(true, true);
    dmaChannelDescriptor_t descriptor = MakeIrqDescriptor(0U);

    ft32DshotTestMotorIrq(&descriptor);
    ASSERT_FALSE(dmaMotors[0].isInput);
    ASSERT_EQ(UINT8_MAX, dmaMotors[0].dmaInputLen);
    shadowCount = 0U;
    dshotTelemetryState.readCount = 9U;
    dshotTelemetryState.motorState[0].rawValue = 0x456U;

    EXPECT_TRUE(pwmTelemetryDecode());
    EXPECT_EQ(9U, dshotTelemetryState.readCount);
    EXPECT_EQ(0x456U, dshotTelemetryState.motorState[0].rawValue);
    EXPECT_EQ(0U, dmaMotors[0].dmaInputLen);
    EXPECT_FALSE(dmaMotors[0].isInput);
}

TEST_F(Ft32DshotDmaTest, PublicProducerReturnsFalseWhenInitialDirectionCannotStopDma)
{
    timerHardware.tim = reinterpret_cast<timerResource_t *>(&timerRegs);
    timerHardware.channel = TIM_Channel_1;
    timerHardware.tag = 1U;
    timerHardware.alternateFunction = 3U;
    timerHardware.output = 0U;
    stickyDisable = true;
    dmaEnabled = true;
    useDshotTelemetry = true;

    EXPECT_FALSE(pwmDshotMotorHardwareConfig(&timerHardware, 0U, 0U, MOTOR_PROTOCOL_DSHOT600, 0U));
    EXPECT_FALSE(dmaMotors[0].configured);
    EXPECT_TRUE(dmaEnabled);
    EXPECT_EQ(0U, rawInitCount);
    EXPECT_EQ(0U, dmaHandlerInstallCount);
    EXPECT_EQ(0U, timerCmdEnableCount);
}

TEST_F(Ft32DshotDmaTest, ActualCallerZerosMotorCountWhenPublicProducerFails)
{
    timerHardware.tim = reinterpret_cast<timerResource_t *>(&timerRegs);
    timerHardware.channel = TIM_Channel_1;
    timerHardware.tag = 1U;
    timerHardware.alternateFunction = 3U;
    timerHardware.output = 0U;
    stickyDisable = true;
    dmaEnabled = true;

    motorDevice_t device{};
    device.count = 1U;
    motorDevConfig_t config{};
    config.motorProtocol = MOTOR_PROTOCOL_DSHOT600;
    config.useDshotTelemetry = 1U;
    config.ioTags[0] = 1U;
    config.motorOutputReordering[0] = 0U;

    EXPECT_FALSE(dshotPwmDevInit(&device, &config));
    EXPECT_EQ(0U, dshotMotorCount);
    EXPECT_FALSE(pwmMotors[0].enabled);
    EXPECT_EQ(0U, dmaHandlerInstallCount);
}

TEST_F(Ft32DshotDmaTest, OutputRecoveryFailurePreservesBufferAndSkipsDuplicateDecode)
{
    DSHOT_DMA_BUFFER_UNIT oldTelemetry[DSHOT_DMA_BUFFER_ALLOC_SIZE];
    std::memset(oldTelemetry, 0x5a, sizeof(oldTelemetry));
    dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaMotors[0].dmaBuffer = oldTelemetry;
    dmaMotors[0].timer = &dmaMotorTimers[0];
    dmaMotors[0].configured = true;
    dmaMotors[0].isInput = true;
    dmaMotors[0].dshotTelemetryDeadtimeUs = 0;
    dmaMotorTimers[0].timer = reinterpret_cast<timerResource_t *>(&timerRegs);
    dshotMotorCount = 1U;
    useDshotTelemetry = true;
    useBurstDshot = false;
    loadDmaBuffer = loadDmaBufferDshot;
    shadowCount = GCR_TELEMETRY_INPUT_LEN;
    dmaEnabled = true;
    stickyDisable = true;

    EXPECT_TRUE(pwmTelemetryDecode());
    EXPECT_FALSE(dmaMotors[0].isInput);
    EXPECT_EQ(UINT8_MAX, dmaMotors[0].dmaInputLen);
    EXPECT_EQ(0U, dshotTelemetryState.readCount);
    EXPECT_EQ(0U, channelOutputEnableCount);

    DSHOT_DMA_BUFFER_UNIT snapshot[DSHOT_DMA_BUFFER_ALLOC_SIZE];
    std::memcpy(snapshot, oldTelemetry, sizeof(snapshot));
    pwmWriteDshotInt(0U, 321U);
    EXPECT_EQ(0, std::memcmp(snapshot, oldTelemetry, sizeof(snapshot)));
    EXPECT_EQ(0U, dmaMotors[0].protocolControl.value);

    shadowCount = 0U;
    EXPECT_TRUE(pwmTelemetryDecode());
    EXPECT_EQ(0U, dshotTelemetryState.readCount);
    EXPECT_EQ(UINT8_MAX, dmaMotors[0].dmaInputLen);
    EXPECT_EQ(0, std::memcmp(snapshot, oldTelemetry, sizeof(snapshot)));
    EXPECT_EQ(0U, channelOutputEnableCount);

    stickyDisable = false;
    dmaEnabled = false;
    EXPECT_TRUE(pwmTelemetryDecode());
    EXPECT_EQ(0U, dmaMotors[0].dmaInputLen);
    EXPECT_FALSE(dmaMotors[0].isInput);
    EXPECT_EQ(1U, channelOutputEnableCount);

    pwmWriteDshotInt(0U, 321U);
    EXPECT_EQ(321U, dmaMotors[0].protocolControl.value);
    EXPECT_NE(0, std::memcmp(snapshot, oldTelemetry, sizeof(snapshot)));
}

TEST_F(Ft32DshotDmaTest, DirectWriteStopsBeforeMutationAndPublishesSourceAfterSingleEnable)
{
    DSHOT_DMA_BUFFER_UNIT outputBuffer[DSHOT_DMA_BUFFER_ALLOC_SIZE];
    std::memset(outputBuffer, 0x5a, sizeof(outputBuffer));
    dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaMotors[0].dmaBuffer = outputBuffer;
    dmaMotors[0].timer = &dmaMotorTimers[0];
    dmaMotors[0].configured = true;
    dmaMotorTimers[0].timer = reinterpret_cast<timerResource_t *>(&timerRegs);
    dmaMotorTimers[0].timerDmaSources = TIM_DMA_CC1;
    loadDmaBuffer = loadDmaBufferDshot;

    DSHOT_DMA_BUFFER_UNIT snapshot[DSHOT_DMA_BUFFER_ALLOC_SIZE];
    std::memcpy(snapshot, outputBuffer, sizeof(snapshot));
    stickyDisable = true;
    dmaEnabled = true;
    pwmWriteDshotInt(0U, 321U);

    EXPECT_EQ(0, std::memcmp(snapshot, outputBuffer, sizeof(snapshot)));
    EXPECT_EQ(0U, dmaMotors[0].protocolControl.value);
    EXPECT_EQ(0U, dmaMotorTimers[0].timerDmaSources);
    EXPECT_EQ(0U, CountEvent(Event::DmaEnable));

    stickyDisable = false;
    dmaEnabled = false;
    events.clear();
    enableCallCount = 0U;
    pwmWriteDshotInt(0U, 321U);
    EXPECT_EQ(321U, dmaMotors[0].protocolControl.value);
    EXPECT_NE(0, std::memcmp(snapshot, outputBuffer, sizeof(snapshot)));
    EXPECT_EQ(TIM_DMA_CC1, dmaMotorTimers[0].timerDmaSources);
    EXPECT_EQ(1U, CountEvent(Event::DmaEnable));
    EXPECT_EQ(0U, blockingDisableCount);
}

TEST_F(Ft32DshotDmaTest, BurstStickyPreflightLatchesWholeTimerWithoutBufferMutation)
{
    dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaMotors[0].timer = &dmaMotorTimers[0];
    dmaMotors[0].configured = true;
    dmaMotorTimers[0].timer = reinterpret_cast<timerResource_t *>(&timerRegs);
    dmaMotorTimers[0].dmaBurstRef = reinterpret_cast<dmaResource_t *>(&dmaChannel);
    uint32_t burstBuffer[DSHOT_DMA_BUFFER_SIZE * 4];
    std::fill(std::begin(burstBuffer), std::end(burstBuffer), 0x5a5a5a5aU);
    dmaMotorTimers[0].dmaBurstBuffer = burstBuffer;
    dmaMotorTimerCount = 1U;
    useBurstDshot = true;
    loadDmaBuffer = loadDmaBufferDshot;
    uint32_t snapshot[DSHOT_DMA_BUFFER_SIZE * 4];
    std::memcpy(snapshot, burstBuffer, sizeof(snapshot));
    stickyDisable = true;
    dmaEnabled = true;

    pwmWriteDshotInt(0U, 111U);
    pwmWriteDshotInt(0U, 222U);
    EXPECT_EQ(UINT16_MAX, dmaMotorTimers[0].dmaBurstLength);
    EXPECT_EQ(0U, dmaMotors[0].protocolControl.value);
    EXPECT_EQ(0, std::memcmp(snapshot, burstBuffer, sizeof(snapshot)));

    events.clear();
    pwmCompleteDshotMotorUpdate();
    EXPECT_EQ(0U, dmaMotorTimers[0].dmaBurstLength);
    EXPECT_EQ(0U, CountEvent(Event::TimerEnable));
    EXPECT_EQ(0, std::memcmp(snapshot, burstBuffer, sizeof(snapshot)));
}

TEST_F(Ft32DshotDmaTest, BurstTwoMotorFramesRearmOnceAndSecondMotorFailureDropsWholeTimer)
{
    uint32_t burstBuffer[DSHOT_DMA_BUFFER_SIZE * 4]{};
    timerHardware.tim = reinterpret_cast<timerResource_t *>(&timerRegs);
    timerHardware.channel = TIM_Channel_1;
    timerHardware2.tim = reinterpret_cast<timerResource_t *>(&timerRegs);
    timerHardware2.channel = TIM_Channel_2;
    dmaMotors[0] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaMotors[1] = MakeDirectMotor(CanonicalDirectDescriptor());
    dmaMotors[0].timer = &dmaMotorTimers[0];
    dmaMotors[1].timer = &dmaMotorTimers[0];
    dmaMotors[1].timerHardware = &timerHardware2;
    dmaMotors[1].index = 1U;
    dmaMotors[0].configured = true;
    dmaMotors[1].configured = true;
    dmaMotorTimers[0].timer = reinterpret_cast<timerResource_t *>(&timerRegs);
    dmaMotorTimers[0].dmaBurstRef = reinterpret_cast<dmaResource_t *>(&dmaChannel);
    dmaMotorTimers[0].dmaBurstBuffer = burstBuffer;
    dmaMotorTimerCount = 1U;
    dshotMotorCount = 2U;
    useBurstDshot = true;
    loadDmaBuffer = loadDmaBufferDshot;

    for (unsigned frame = 0; frame < 2U; frame++) {
        events.clear();
        enabledTimerSources.clear();
        enableCallCount = 0U;
        disableCallCount = 0U;
        dmaEnabled = false;

        pwmWriteDshotInt(0U, 500U + frame);
        pwmWriteDshotInt(1U, 600U + frame);
        ASSERT_EQ(DSHOT_DMA_BUFFER_SIZE * 4U, dmaMotorTimers[0].dmaBurstLength);
        pwmCompleteDshotMotorUpdate();

        EXPECT_EQ(DSHOT_DMA_BUFFER_SIZE * 4U, shadowCount);
        EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(burstBuffer)), dmaChannel.SAR);
        EXPECT_EQ(1U, CountEvent(Event::DmaEnable));
        EXPECT_EQ(1U, CountEvent(Event::TimerEnable));
        ASSERT_FALSE(enabledTimerSources.empty());
        EXPECT_EQ(TIM_DMA_Update, enabledTimerSources.back());
        EXPECT_TRUE(dmaEnabled);
        EXPECT_EQ(500U + frame, dmaMotors[0].protocolControl.value);
        EXPECT_EQ(600U + frame, dmaMotors[1].protocolControl.value);
        EXPECT_EQ(0U, blockingDisableCount);
    }

    uint32_t motor1Lane[DSHOT_DMA_BUFFER_SIZE];
    for (unsigned i = 0; i < DSHOT_DMA_BUFFER_SIZE; i++) {
        motor1Lane[i] = burstBuffer[i * 4U + 1U];
    }
    const uint16_t motor1Value = dmaMotors[1].protocolControl.value;
    events.clear();
    enabledTimerSources.clear();
    enableCallCount = 0U;
    disableCallCount = 0U;
    failDisableCall = 2U;
    dmaEnabled = false;

    pwmWriteDshotInt(0U, 700U);
    pwmWriteDshotInt(1U, 800U);
    EXPECT_EQ(UINT16_MAX, dmaMotorTimers[0].dmaBurstLength);
    EXPECT_EQ(700U, dmaMotors[0].protocolControl.value);
    EXPECT_EQ(motor1Value, dmaMotors[1].protocolControl.value);
    for (unsigned i = 0; i < DSHOT_DMA_BUFFER_SIZE; i++) {
        EXPECT_EQ(motor1Lane[i], burstBuffer[i * 4U + 1U]);
    }

    pwmCompleteDshotMotorUpdate();
    EXPECT_EQ(0U, dmaMotorTimers[0].dmaBurstLength);
    EXPECT_EQ(0U, CountEvent(Event::DmaEnable));
    EXPECT_EQ(0U, CountEvent(Event::TimerEnable));
    EXPECT_TRUE(enabledTimerSources.empty());
    EXPECT_FALSE(dmaEnabled);
}

TEST_F(Ft32DshotDmaTest, BitbangUpdateInitPreservesInputSamplingAndDropsStickyOutputFrame)
{
    bbPort_t *output = ConfigureBitbangPort(0U);
    bbPort_t *input = ConfigureBitbangPort(1U);
    usedMotorPorts = 2;
    output->direction = DSHOT_BITBANG_DIRECTION_OUTPUT;
    input->direction = DSHOT_BITBANG_DIRECTION_INPUT;
    input->telemetryPending = true;
    dmaEnabled = true;
    stickyDisable = true;
    dmaEnabled2 = true;
    uint32_t outputSnapshot[MOTOR_DSHOT_BUF_CACHE_ALIGN_LENGTH];
    std::memcpy(outputSnapshot, output->portOutputBuffer, sizeof(outputSnapshot));
    events.clear();
    disabledTimerSources.clear();

    ft32DshotTestBitbangUpdateInit();

    EXPECT_EQ(UINT8_MAX, output->direction);
    EXPECT_EQ(DSHOT_BITBANG_DIRECTION_INPUT, input->direction);
    EXPECT_TRUE(input->telemetryPending);
    EXPECT_TRUE(dmaEnabled2);
    EXPECT_NE(disabledTimerSources.end(),
        std::find(disabledTimerSources.begin(), disabledTimerSources.end(), output->dmaSource));
    EXPECT_EQ(disabledTimerSources.end(),
        std::find(disabledTimerSources.begin(), disabledTimerSources.end(), input->dmaSource));
    EXPECT_EQ(0, std::memcmp(outputSnapshot, output->portOutputBuffer, sizeof(outputSnapshot)));

    bbMotors[0].configured = true;
    bbMotors[0].bbPort = output;
    ft32DshotTestBitbangWriteInt(0U, 333U);
    EXPECT_EQ(0U, bbMotors[0].protocolControl.value);
    EXPECT_EQ(0, std::memcmp(outputSnapshot, output->portOutputBuffer, sizeof(outputSnapshot)));

    ft32DshotTestBitbangUpdateComplete();
    EXPECT_EQ(0U, ft32DshotTestBitbangLastSendUs());
    EXPECT_EQ(0U, CountEvent(Event::TimerEnable));
}

} // namespace

extern "C" {

uint8_t atomic_BASEPRI;
int16_t debug[4];
uint8_t debugMode;
uint8_t dshotMotorCount;
dshotTelemetryState_t dshotTelemetryState;
dshotTelemetryQuality_t dshotTelemetryQuality[MAX_SUPPORTED_MOTORS];
pwmOutputPort_t pwmMotors[MAX_SUPPORTED_MOTORS];
uint8_t pwmMotorCount;

DMA_BaseAddressAndChannelIndex CalBaseAddressAndChannelIndex(DMA_Channel_TypeDef *resource)
{
    return DMA_BaseAddressAndChannelIndex{&dmaController, resource == &dmaChannel2 ? 3U : 2U};
}

const dmaChannelSpec_t *dmaGetChannelSpecByTimer(const timerHardware_t *)
{
    return &fakeDmaSpec;
}

const dmaChannelSpec_t *dmaGetChannelSpecByPeripheral(dmaPeripheral_e, uint8_t, int8_t)
{
    return nullptr;
}

dmaIdentifier_e dmaGetIdentifier(const dmaResource_t *)
{
    return static_cast<dmaIdentifier_e>(DMA1_ST0_HANDLER);
}

bool dmaAllocate(dmaIdentifier_e, resourceOwner_e, uint8_t)
{
    return true;
}

const resourceOwner_t *dmaGetOwner(dmaIdentifier_e)
{
    return &fakeDmaOwner;
}

void dmaEnable(dmaIdentifier_e)
{
}

void dmaSetHandler(dmaIdentifier_e, dmaCallbackHandlerFuncPtr, uint32_t, uint32_t)
{
    dmaHandlerInstallCount++;
}

IO_t IOGetByTag(ioTag_t)
{
    return reinterpret_cast<IO_t>(&gpioRegs);
}

void IOConfigGPIOAF(IO_t, ioConfig_t, uint8_t)
{
}

void IOInit(IO_t, resourceOwner_e, uint8_t)
{
}

const timerHardware_t *timerAllocate(ioTag_t, resourceOwner_e, uint8_t)
{
    return &timerHardware;
}

void DMA_Channel_Cmd(DMA_Channel_TypeDef *resource, FunctionalState state)
{
    bool &enabled = DmaEnabledFor(resource);
    if (state == DISABLE) {
        events.push_back(Event::DmaDisable);
        disabledResources.push_back(resource);
        disableCallCount++;
        if (failDisableCall != 0U && disableCallCount == failDisableCall) {
            enabled = true;
        } else if (!DmaStickyFor(resource)) {
            enabled = false;
        }
    } else {
        events.push_back(Event::DmaEnable);
        enableCallCount++;
        dmaEnableSawAllFlagsCleared = AllTerminalFlagsAreCleared();
        enabled = enableSucceeds && (failEnableCall == 0U || enableCallCount != failEnableCall);
    }
}

bool ft32DmaTrySetCurrDataCounter(DMA_ARCH_TYPE *resource, uint16_t count)
{
    events.push_back(Event::Counter);
    DMA_Channel_Cmd(resource, DISABLE);
    if (DmaEnabledFor(resource)) {
        return false;
    }
    shadowCount = count;
    return true;
}

uint8_t ft32DmaIsChannelEnabled(DMA_ARCH_TYPE *resource)
{
    return DmaEnabledFor(resource);
}

uint16_t ft32DmaGetCurrDataCounter(DMA_ARCH_TYPE *)
{
    return shadowCount;
}

void ft32DmaCmd(DMA_ARCH_TYPE *resource, FunctionalState state)
{
    if (state == DISABLE) {
        blockingDisableCount++;
    }
    DMA_Channel_Cmd(resource, state);
}

void ft32DmaDeInit(DMA_ARCH_TYPE *)
{
}

void DMA_StructInit(DMA_InitTypeDef *init)
{
    std::memset(init, 0, sizeof(*init));
}

void DMA_Init(DMA_Channel_TypeDef *resource, DMA_InitTypeDef *init)
{
    events.push_back(Event::DmaInit);
    capturedInit = *init;
    rawInitCount++;
    resource->SAR = init->SrcAddress;
    resource->DAR = init->DstAddress;
    resource->CTL = 1U | (static_cast<uint64_t>(init->BlockTransSize) << 4U);
    resource->CFG = 1U;
    dmaController.CHSEL |=
        static_cast<uint64_t>(init->DstHsIfPeriphSel) << (init->DstHardwareInterface * 3U);
    dmaController.CHSEL |=
        static_cast<uint64_t>(init->SrcHsIfPeriphSel) << (init->SrcHardwareInterface * 3U);
}

void DMA_ITConfig(DMA_Channel_TypeDef *, uint8_t mask, FunctionalState state)
{
    if (state == DISABLE) {
        events.push_back(Event::MaskDisable);
        maskDisableCount++;
        lastDisabledMask = mask;
    } else {
        events.push_back(Event::MaskEnable);
        maskEnableCount++;
        lastEnabledMask = mask;
    }
}

void DMA_ClearFlagStatus(DMA_Channel_TypeDef *, uint8_t)
{
    events.push_back(Event::FlagClear);
}

FlagStatus DMA_GetFlagStatus(DMA_Channel_TypeDef *, uint8_t)
{
    return RESET;
}

void TIM_DMACmd(TIM_TypeDef *, uint32_t source, FunctionalState state)
{
    events.push_back(state == DISABLE ? Event::TimerDisable : Event::TimerEnable);
    if (state == DISABLE) {
        disabledTimerSources.push_back(source);
    }
    if (state == ENABLE) {
        enabledTimerSources.push_back(source);
        timerEnableSawDmaAndAllFlagsCleared = dmaEnabled && AllTerminalFlagsAreCleared();
    }
}

void TIM_CCxCmd(TIM_TypeDef *, uint32_t, uint32_t)
{
    channelOutputEnableCount++;
}

void TIM_CCxNCmd(TIM_TypeDef *, uint32_t, uint32_t)
{
    channelOutputEnableCount++;
}

void TIM_TimeBaseStructInit(TIM_TimeBaseInitTypeDef *init)
{
    std::memset(init, 0, sizeof(*init));
}

void TIM_TimeBaseInit(TIM_TypeDef *, TIM_TimeBaseInitTypeDef *)
{
}

void TIM_OCStructInit(TIM_OCInitTypeDef *init)
{
    std::memset(init, 0, sizeof(*init));
}

void TIM_ICStructInit(TIM_ICInitTypeDef *init)
{
    std::memset(init, 0, sizeof(*init));
}

void TIM_Cmd(TIM_TypeDef *, FunctionalState state)
{
    if (state == ENABLE) {
        timerCmdEnableCount++;
    }
}

void TIM_CtrlPWMOutputs(TIM_TypeDef *, FunctionalState)
{
}

void TIM_DMAConfig(TIM_TypeDef *, uint32_t, uint32_t)
{
}

void TIM_SetCounter(TIM_TypeDef *, uint32_t)
{
}

timeUs_t micros(void)
{
    return mockMicros;
}

timeMs_t millis(void)
{
    return mockMicros / 1000U;
}

uint32_t getCycleCounter(void)
{
    return mockCycles++;
}

void TIM_ARRPreloadConfig(TIM_TypeDef *, FunctionalState)
{
}

void TIM_ICInit(TIM_TypeDef *, TIM_ICInitTypeDef *)
{
}

void timerOCPreloadConfig(TIM_TypeDef *, uint8_t, uint16_t)
{
}

void timerOCInit(TIM_TypeDef *, uint8_t, TIM_OCInitTypeDef *)
{
}

rccPeriphTag_t timerRCC(const timerResource_t *)
{
    return 0U;
}

void RCC_ClockCmd(rccPeriphTag_t, FunctionalState)
{
}

uint32_t timerClock(const timerHardware_t *)
{
    return 168000000U;
}

uint16_t timerDmaSource(uint8_t)
{
    return TIM_DMA_CC1;
}

volatile timCCR_t *timerChCCR(const timerHardware_t *)
{
    return &fakeCcr;
}

int8_t timerGetTIMNumber(const timerHardware_t *)
{
    return 8;
}

uint8_t timerLookupChannelIndex(const uint16_t channel)
{
    return channel == TIM_Channel_2 ? 1U : 0U;
}

void motorPostInitNull(void)
{
}

float dshotConvertFromExternal(uint16_t value)
{
    return static_cast<float>(value);
}

uint16_t dshotConvertToExternal(float value)
{
    return static_cast<uint16_t>(value);
}

bool dshotCommandQueueEmpty(void)
{
    return true;
}

bool dshotCommandOutputIsEnabled(unsigned)
{
    return true;
}

uint8_t dshotCommandGetCurrent(unsigned)
{
    return 0U;
}

bool dshotCommandIsProcessing(void)
{
    return false;
}

uint16_t prepareDshotPacket(dshotProtocolControl_t *)
{
    return 0U;
}

void updateDshotTelemetryQuality(dshotTelemetryQuality_t *, bool, timeMs_t)
{
}

void dbgPinHi(int)
{
}

void dbgPinLo(int)
{
}

uint32_t decode_bb(uint16_t *, uint32_t, uint32_t)
{
    return DSHOT_TELEMETRY_NOEDGE;
}

}

#endif // FT32_DSHOT_NO_TELEMETRY_VARIANT
