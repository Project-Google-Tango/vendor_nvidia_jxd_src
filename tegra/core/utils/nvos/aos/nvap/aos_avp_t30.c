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
#include "avp.h"
#include "aos_avp.h"
#include "t30/arictlr.h"
#include "t30/artimer.h"
#include "t30/artimerus.h"
#include "t30/arclk_rst.h"
#include "nvrm_drf.h"

extern NvBool s_FpgaEmulation;
extern NvBool s_QuickTurnEmul;

NvU32 measureT30OscFreq(NvU32* dividend, NvU32* divisor)
{
    NvU32 cnt;
    NvU32 reg;
    NvU32 result;
#if NV_DEBUG
    NvU32 PllRefDiv;
#endif

    if (s_FpgaEmulation || s_QuickTurnEmul)
    {
        *dividend = 0; // 13mhz
        *divisor = 12;
        result = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC13;
        return result;
    }

#if NV_DEBUG
    PllRefDiv = NV_DRF_VAL( CLK_RST_CONTROLLER, OSC_CTRL, PLL_REF_DIV,
        AOS_REGR( CLK_RST_PA_BASE + CLK_RST_CONTROLLER_OSC_CTRL_0 ) );
#endif
    reg = NV_DRF_DEF(CLK_RST_CONTROLLER, OSC_FREQ_DET, OSC_FREQ_DET_TRIG,
            ENABLE)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, OSC_FREQ_DET, REF_CLK_WIN_CFG,
            (1-1));
    AOS_REGW( CLK_RST_PA_BASE + CLK_RST_CONTROLLER_OSC_FREQ_DET_0, reg );

    do {
        reg = AOS_REGR( CLK_RST_PA_BASE + CLK_RST_CONTROLLER_OSC_FREQ_DET_STATUS_0 );
    } while  ( NV_DRF_VAL( CLK_RST_CONTROLLER, OSC_FREQ_DET_STATUS,
        OSC_FREQ_DET_BUSY, reg ) );
    cnt = NV_DRF_VAL(CLK_RST_CONTROLLER, OSC_FREQ_DET_STATUS,
        OSC_FREQ_DET_CNT, reg) ;

    if ( cnt < (NvU32)(12000000/32768) + 5 ) // 12 mhz
    {
        *dividend = 0;
        *divisor = 11;
        result = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC12;
    }
    else if ( cnt < (NvU32)(13000000/32768) + 5 ) // 13 mhz
    {
        *dividend = 0;
        *divisor = 12;
        result = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC13;
    }
    else if ( cnt < (NvU32)(16800000/32768) + 5 ) // 16.8 mhz
    {
        *dividend = 4;
        *divisor = 83;
        result = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC16P8;
    }
    else if ( cnt < (NvU32)(19200000/32768) + 5 ) // 19.2 mhz
    {
        *dividend = 4;
        *divisor = 95;
        result = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC19P2;
    }
    else if ( cnt < (NvU32)(26000000/32768) + 5 ) // 26 mhz
    {
        *dividend = 0;
        *divisor = 25;
        result = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC26;
    }
    else if ( cnt < (NvU32)(38400000/32768) + 5 ) // 38.4 mhz
    {
        NV_ASSERT(PllRefDiv == CLK_RST_CONTROLLER_OSC_CTRL_0_PLL_REF_DIV_DIV2);
        *dividend = 4;
        *divisor = 191;
        result = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC38P4;
    }
    else if ( cnt < (NvU32)(48000000/32768) + 5 ) // 48 mhz
    {
        NV_ASSERT(PllRefDiv == CLK_RST_CONTROLLER_OSC_CTRL_0_PLL_REF_DIV_DIV4);
        *dividend = 0;
        *divisor = 47;
        result = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC48;
    }
    else // assume 26 mhz
    {
        *dividend = 0;
        *divisor = 25;
        result = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC26;
    }

    return result;
}
