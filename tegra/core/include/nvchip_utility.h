/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_CHIP_UTILITY_H
#define INCLUDED_CHIP_UTILITY_H
#include "nvcommon.h"
#define MISC_PA_BASE        0x70000000UL
/**
 * Checks for forward-looking condition to find out whether the chip is release
 * later or not than particular chip.
 *
 * @Param: The chip id to be compared against the current chip
 *
 * @return NV_TRUE if the currect chip is relased later than the
 * chip to be compared. Otherwise returns NV_FALSE.
*/

NvBool  NvIsCurrentChipLaterThan(NvU32 Chip_id);

#endif // INCLUDED_CHIP_UTILITY_H
