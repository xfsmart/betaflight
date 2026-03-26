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

#pragma once

/*
 * FT32F4 DMA request mapping
 * Reference: FT32F405_407xx_RM_V1.00_cn.pdf Table 10-1 (p189), Table 10-2 (p190)
 *
 * FT32F4 DesignWare DMA uses the same DMA(d, channel, periph) triple as STM32F4's
 * DMA(d, stream, channel). The "periph" is the CHSEL value (0-7) that selects
 * which peripheral request is routed to a given DMA channel.
 */

#define MAX_PERIPHERAL_DMA_OPTIONS  2
#define MAX_TIMER_DMA_OPTIONS       3
