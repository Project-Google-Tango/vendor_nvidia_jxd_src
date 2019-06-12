/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "pinmux.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "pinmux_soc.h"

static void NvPinmuxApplyConfig(NvPingroupConfig Pin)
{
    NvU32 RegVal = 0;

    RegVal =  (Pin.IO_Reset  << NV_PINMUX_IO_RESET_OFFSET) |
        (Pin.Rcv_sel   << NV_PINMUX_RCV_SEL_OFFSET) |
        (Pin.Lock      << NV_PINMUX_LOCK_OFFSET) |
        (Pin.E_Od      << NV_PINMUX_E_OD_OFFSET) |
        (Pin.E_Input   << NV_PINMUX_E_INPUT_OFFSET) |
        (Pin.Tristate  << NV_PINMUX_TRISTATE_OFFSET) |
        (Pin.Pupd      << NV_PINMUX_PUPD_OFFSET) |
        (Pin.Pinmux    << NV_PINMUX_PINMUX_OFFSET);

    NV_WRITE32((NV_ADDRESS_MAP_APB_MISC_BASE + Pin.RegOffset), RegVal);
}

NvError NvPinmuxConfigTable(NvPingroupConfig *config, NvU32 len)
{
    NvU32 i;

    NV_ASSERT(config);
    NV_ASSERT(len > 0);

    if (!config)
        return NvError_BadParameter;

    for (i = 0; i < len; i++)
    {
        NvPinmuxApplyConfig(config[i]);
    }
    return NvSuccess;
}
