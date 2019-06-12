/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "aos.h"
#include "aos_common.h"
#include "nvrm_drf.h"
#include "t30/arclk_rst.h"
#include "cpu.h"
#include "aos_cpu.h"

NvU32 nvaosT30GetOscFreq(NvU32 *dividend, NvU32 *divisor)
{
    NvU32 freq = NV_DRF_VAL(CLK_RST_CONTROLLER, OSC_CTRL, OSC_FREQ,
        AOS_REGR(CLK_RST_PA_BASE + CLK_RST_CONTROLLER_OSC_CTRL_0));

    switch( freq ) {
        case CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC13: // 13 mhz
            *dividend = 0;
            *divisor = 12;
            break;
        case CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC16P8: // 16.8 mhz
            *dividend = 4;
            *divisor = 83;
            break;
        case CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC19P2: // 19.2 mhz
            *dividend = 4;
            *divisor = 95;
            break;
        case CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC38P4: // 38.4 mhz
            *dividend = 4;
            *divisor = 191;
            break;
        case CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC12: // 12 mhz
            *dividend = 0;
            *divisor = 11;
            break;
        case CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC48: // 48 mhz
            *dividend = 0;
            *divisor = 47;
            break;
        case CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC26: // 26 mhz
        default:
            *dividend = 0;
            *divisor = 25;
            break;
    }

    return freq;
}

