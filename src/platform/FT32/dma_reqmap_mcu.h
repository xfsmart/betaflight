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

#pragma once

#include "platform.h"

// FT32F4 使用基于 Channel 的 DMA 架构
// 每个通道可以配置外设请求映射
// Citation: spec.json -> channel_mapping

// 外设 DMA 选项最大值
#define MAX_PERIPHERAL_DMA_OPTIONS 2

// 定时器 DMA 选项最大值
#define MAX_TIMER_DMA_OPTIONS 3
