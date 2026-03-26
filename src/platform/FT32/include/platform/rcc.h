#pragma once
#include "ft32f4xx_rcc.h"
// RCC bus type tags for RCC_ClockCmd/RCC_ResetCmd
#define RCC_AHB1_VALUE    0
#define RCC_APB1_VALUE    1
#define RCC_APB2_VALUE    2
#define RCC_ENCODE(bus, mask) ((rccPeriphTag_t)(((bus) << 5) | LOG2_32BIT(mask)))
void RCC_ClockCmd(rccPeriphTag_t periphTag, FunctionalState NewState);
void RCC_ResetCmd(rccPeriphTag_t periphTag, FunctionalState NewState);
