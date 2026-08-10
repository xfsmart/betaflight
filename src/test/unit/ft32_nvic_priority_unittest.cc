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
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

typedef int i2c_handle_type;
typedef struct USART_TypeDef_s USART_TypeDef;

#define FT32F4
#define __NVIC_PRIO_BITS 4U
#include "../../platform/FT32/include/platform/platform.h"

#include <fstream>
#include <sstream>
#include <string>

#include "gtest/gtest.h"

namespace {

constexpr uint32_t cmsisEncodePriority(uint32_t grouping, uint32_t base, uint32_t sub)
{
    const uint32_t preemptionBits = ((7U - grouping) > __NVIC_PRIO_BITS) ? __NVIC_PRIO_BITS : (7U - grouping);
    const uint32_t subpriorityBits = ((grouping + __NVIC_PRIO_BITS) < 7U) ? 0U : (grouping - 7U + __NVIC_PRIO_BITS);
    return ((base & ((1U << preemptionBits) - 1U)) << subpriorityBits)
        | (sub & ((1U << subpriorityBits) - 1U));
}

constexpr uint8_t sdkPriorityByte(uint32_t grouping, uint32_t base, uint32_t sub)
{
    const uint32_t priorityBits = 7U - grouping;
    const uint32_t preemptionShift = 4U - priorityBits;
    const uint32_t subpriorityMask = 0x0fU >> priorityBits;
    return static_cast<uint8_t>(((base << preemptionShift) | (sub & subpriorityMask)) << 4U);
}

constexpr uint32_t legacyBrokenSubpriority(uint32_t packed)
{
    return (packed >> (4U - (7U - NVIC_PRIORITY_GROUPING))) & 0x0fU;
}

std::string readSource(const char *path)
{
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

size_t countOccurrences(const std::string &source, const std::string &needle)
{
    size_t count = 0;
    size_t offset = 0;
    while ((offset = source.find(needle, offset)) != std::string::npos) {
        count++;
        offset += needle.size();
    }
    return count;
}

} // namespace

TEST(Ft32NvicPriorityTest, UsesTwoPreemptionAndTwoSubpriorityBits)
{
    EXPECT_EQ(5U, NVIC_PRIORITY_GROUPING);

    for (uint32_t base = 0; base < 4U; base++) {
        for (uint32_t sub = 0; sub < 4U; sub++) {
            const uint32_t packed = NVIC_BUILD_PRIORITY(base, sub);
            EXPECT_EQ(cmsisEncodePriority(NVIC_PRIORITY_GROUPING, base, sub) << 4U, packed)
                << "base=" << base << " sub=" << sub;
            EXPECT_EQ(base, NVIC_PRIORITY_BASE(packed)) << "base=" << base << " sub=" << sub;
            EXPECT_EQ(sub, NVIC_PRIORITY_SUB(packed)) << "base=" << base << " sub=" << sub;
            EXPECT_EQ(packed, sdkPriorityByte(NVIC_PRIORITY_GROUPING, base, sub))
                << "base=" << base << " sub=" << sub;
            EXPECT_EQ(0U, packed & 0x0fU);
            EXPECT_EQ(packed, NVIC_PRIORITY_TO_CMSIS(packed) << (8U - __NVIC_PRIO_BITS));
        }
    }
}

TEST(Ft32NvicPriorityTest, PreservesNamedPriorityRelations)
{
    constexpr uint32_t i2c = NVIC_BUILD_PRIORITY(0, 0);
    constexpr uint32_t timer = NVIC_BUILD_PRIORITY(1, 1);
    constexpr uint32_t uart = NVIC_BUILD_PRIORITY(1, 2);
    constexpr uint32_t usb = NVIC_BUILD_PRIORITY(2, 0);
    constexpr uint32_t dshot = NVIC_BUILD_PRIORITY(2, 1);
    constexpr uint32_t cdcTimer = NVIC_BUILD_PRIORITY(3, 0);

    EXPECT_EQ(0x00U, i2c);
    EXPECT_EQ(0x50U, timer);
    EXPECT_EQ(0x60U, uart);
    EXPECT_EQ(0x80U, usb);
    EXPECT_EQ(0x90U, dshot);
    EXPECT_EQ(0xc0U, cdcTimer);

    EXPECT_EQ(NVIC_PRIORITY_BASE(usb), NVIC_PRIORITY_BASE(dshot));
    EXPECT_LT(NVIC_PRIORITY_SUB(usb), NVIC_PRIORITY_SUB(dshot));
    EXPECT_LT(NVIC_PRIORITY_BASE(usb), NVIC_PRIORITY_BASE(cdcTimer));
    EXPECT_EQ(0x80U, cmsisEncodePriority(NVIC_PRIORITY_GROUPING, 6, 0) << 4U);
}

TEST(Ft32NvicPriorityTest, RejectsKnownPriorityEncodingMutations)
{
    constexpr uint32_t usb = NVIC_BUILD_PRIORITY(2, 0);
    constexpr uint32_t dshot = NVIC_BUILD_PRIORITY(2, 1);
    constexpr uint32_t cdcTimer = NVIC_BUILD_PRIORITY(3, 0);

    EXPECT_EQ(4U, legacyBrokenSubpriority(dshot));
    EXPECT_NE(NVIC_PRIORITY_SUB(dshot), legacyBrokenSubpriority(dshot));
    EXPECT_NE(dshot, cmsisEncodePriority(4U, 2U, 1U) << 4U);
    EXPECT_EQ(usb, cmsisEncodePriority(NVIC_PRIORITY_GROUPING, 6U, 0U) << 4U);
    EXPECT_NE(cdcTimer, cmsisEncodePriority(NVIC_PRIORITY_GROUPING, 6U, 0U) << 4U);
    EXPECT_EQ(8U, NVIC_PRIORITY_TO_CMSIS(usb));
    EXPECT_NE(usb, NVIC_PRIORITY_TO_CMSIS(usb));
}

TEST(Ft32NvicPriorityTest, ConfiguresGroupingOnceBeforePeripheralInitialization)
{
    const std::string source = readSource("../platform/FT32/system_ft32f4xx.c");
    ASSERT_FALSE(source.empty());
    EXPECT_EQ(1U, countOccurrences(source, "NVIC_SetPriorityGrouping(NVIC_PRIORITY_GROUPING);"));
    EXPECT_EQ(std::string::npos, source.find("NVIC_PriorityGroupConfig("));
}

TEST(Ft32NvicPriorityTest, KeepsDirectCmsisPrioritiesInTheValidDomain)
{
    const std::string cdcSource = readSource("../platform/FT32/vcpf4/usbd_cdc_vcp.c");
    const std::string usbSource = readSource("../platform/FT32/vcpf4/usb_bsp_ft32f4.c");
    ASSERT_FALSE(cdcSource.empty());
    ASSERT_FALSE(usbSource.empty());

    EXPECT_EQ(1U, countOccurrences(cdcSource, "NVIC_PRIORITY_TO_CMSIS(NVIC_BUILD_PRIORITY(3, 0))"));
    EXPECT_EQ(std::string::npos, cdcSource.find("NVIC_PRIORITY_TO_CMSIS(NVIC_BUILD_PRIORITY(6, 0))"));
    EXPECT_EQ(1U, countOccurrences(usbSource, "NVIC_PRIORITY_TO_CMSIS(NVIC_PRIO_USB)"));
}
