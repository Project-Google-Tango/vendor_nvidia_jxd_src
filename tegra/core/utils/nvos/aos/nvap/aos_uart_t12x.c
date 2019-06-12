/**
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
.* NVIDIA Corporation and its licensors retain all intellectual property and
.* proprietary rights in and to this software and related documentation.  Any
.* use, reproduction, disclosure or distribution of this software and related
.* documentation without an express license agreement from NVIDIA Corporation
.* is strictly prohibited.
*/

#include "aos_common.h"
#include "aos_uart.h"
#include "aos.h"
#include "nvodm_query_pinmux.h"
#include "nvrm_drf.h"
#include "t12x/arapb_misc.h"
#include "t12x/arclk_rst.h"
#include "t12x/nvboot_osc.h"
#include "t12x/arflow_ctlr.h"
#include "t12x/aruart.h"

#define TRISTATE_UART(reg) \
        TristateReg = NV_READ32(MISC_PA_BASE + PINMUX_AUX_##reg##_0); \
        TristateReg = NV_FLD_SET_DRF_DEF(PINMUX, AUX_##reg, \
        TRISTATE, NORMAL, TristateReg); \
        NV_WRITE32(MISC_PA_BASE + PINMUX_AUX_##reg##_0, TristateReg);


#define PINMUX_UART(reg, uart) \
        PinMuxReg = NV_READ32(MISC_PA_BASE + PINMUX_AUX_##reg##_0); \
        PinMuxReg = NV_FLD_SET_DRF_DEF(PINMUX, AUX_##reg, PM, \
                    uart, PinMuxReg); \
        NV_WRITE32(MISC_PA_BASE + PINMUX_AUX_##reg##_0, PinMuxReg);


NvError aos_EnableUartA(NvU32 configuration)
{
    //To do
    return NvError_BadParameter;
}

NvError aos_EnableUartB(NvU32 configuration)
{
    //To do
    return NvError_BadParameter;
}

NvError aos_EnableUartC(NvU32 configuration)
{
    //To do
    return NvError_BadParameter;
}

NvError aos_EnableUartD(NvU32 configuration)
{
    //To do
    return NvError_BadParameter;
}

NvError aos_EnableUartE(NvU32 configuration)
{
    return NvError_BadParameter;
}
