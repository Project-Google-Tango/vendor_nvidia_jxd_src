/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_SDIO_UTILS_H
#define INCLUDED_SDIO_UTILS_H

#include "nvcommon.h"
#include "nvassert.h"
#include "nverror.h"
#include "arapb_misc.h"


#if defined(__cplusplus)
extern "C"
{
#endif

#define NV_ADDRESS_MAP_APB_MISC_BASE         0x70000000


#define NV_MISC_READ(Reg, value)                                              \
    do                                                                        \
    {                                                                         \
        value = NV_READ32(NV_ADDRESS_MAP_APB_MISC_BASE + APB_MISC_##Reg##_0); \
    } while (0)

#define NV_MISC_WRITE(Reg, value)                                             \
    do                                                                        \
    {                                                                         \
        NV_WRITE32((NV_ADDRESS_MAP_APB_MISC_BASE + APB_MISC_##Reg##_0),       \
                   value);                                                    \
    } while (0)

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_SDIO_UTILS_H
