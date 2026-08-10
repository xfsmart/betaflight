/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 */

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "platform.h"
#undef DEFAULT_AUX_CHANNEL_COUNT
#include "target/common_post.h"

#include "pg/bus_i2c.h"

void pgResetFn_i2cConfig(i2cConfig_t *i2cConfig);
}

#if !defined(USE_MAG_HMC5883) \
    || !defined(USE_MAG_SPI_HMC5883) \
    || !defined(USE_MAG_QMC5883) \
    || !defined(USE_MAG_LIS2MDL) \
    || !defined(USE_MAG_LIS3MDL) \
    || !defined(USE_MAG_AK8963) \
    || !defined(USE_MAG_MPU925X_AK8963) \
    || !defined(USE_MAG_SPI_AK8963) \
    || !defined(USE_MAG_AK8975) \
    || !defined(USE_MAG_IST8310) \
    || !defined(USE_MAG_MMC560X)
#error Generic USE_MAG builds must retain the complete historical driver fallback
#endif

#include "gtest/gtest.h"

namespace {

std::string readBoardCConfig()
{
    std::ifstream input("../platform/FT32/config/configs/FT32F405_FT32/config.h");
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

std::vector<std::string> selectedMagDriverMacros(const std::string &source)
{
    std::vector<std::string> selected;
    std::istringstream lines(source);
    std::string line;

    while (std::getline(lines, line)) {
        static const std::string prefix = "#define USE_MAG_";
        if (line.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }

        std::istringstream tokens(line);
        std::string directive;
        std::string macro;
        tokens >> directive >> macro;
        if (macro != "USE_MAG_EXPLICIT_DRIVERS") {
            selected.push_back(macro);
        }
    }

    return selected;
}

} // namespace

TEST(BoardCIst8310ConfigTest, GenericMagFallbackRemainsCompatible)
{
    EXPECT_TRUE(true);
}

TEST(BoardCIst8310ConfigTest, BoardCSelectsOnlyIst8310OnI2c1)
{
    const std::string config = readBoardCConfig();

    ASSERT_FALSE(config.empty());
    EXPECT_NE(std::string::npos, config.find("#define USE_MAG\n"));
    EXPECT_NE(std::string::npos, config.find("#define USE_MAG_EXPLICIT_DRIVERS\n"));
    EXPECT_NE(std::string::npos, config.find("#define USE_MAG_IST8310\n"));
    EXPECT_NE(std::string::npos, config.find("#define I2C1_SCL_PIN               PB6"));
    EXPECT_NE(std::string::npos, config.find("#define I2C1_SDA_PIN               PB7"));
    EXPECT_NE(std::string::npos, config.find("#define I2C1_CLOCKSPEED            400"));
    EXPECT_NE(std::string::npos, config.find("#define MAG_I2C_INSTANCE               I2CDEV_1"));
    EXPECT_EQ(std::string::npos, config.find("#define MAG_ALIGN"));

    const std::vector<std::string> selected = selectedMagDriverMacros(config);
    ASSERT_EQ(1U, selected.size());
    EXPECT_EQ("USE_MAG_IST8310", selected.front());
}

TEST(BoardCIst8310ConfigTest, CompiledI2c1ResetDefaultIs400Khz)
{
    i2cConfig_t config[I2CDEV_COUNT] = {};

    pgResetFn_i2cConfig(config);

    EXPECT_EQ(400U, config[I2CDEV_1].clockSpeed);
}
