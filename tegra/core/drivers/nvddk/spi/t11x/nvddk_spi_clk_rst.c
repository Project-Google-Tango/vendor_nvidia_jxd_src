/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "arclk_rst.h"
#include "arapbpm.h"
#include "nvrm_drf.h"
#include "nvddk_spi.h"
#include "nvddk_spi_hw.h"
#include "nvddk_spi_debug.h"

void
SpiClockEnable(
    NvU32 Instance,
    NvBool Enable)
{
    NvU32 Reg;
    switch (Instance)
    {
        case 0:
            NV_CLK_RST_READ(CLK_OUT_ENB_H, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                             CLK_ENB_SBC1, Enable, Reg);
            NV_CLK_RST_WRITE(CLK_OUT_ENB_H, Reg);
            break;
        case 1:
            NV_CLK_RST_READ(CLK_OUT_ENB_H, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                             CLK_ENB_SBC2, Enable, Reg);
            NV_CLK_RST_WRITE(CLK_OUT_ENB_H, Reg);
            break;
        case 2:
            NV_CLK_RST_READ(CLK_OUT_ENB_H, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                             CLK_ENB_SBC3, Enable, Reg);
            NV_CLK_RST_WRITE(CLK_OUT_ENB_H, Reg);
            break;
        case 3:
            NV_CLK_RST_READ(CLK_OUT_ENB_U, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_U,
                             CLK_ENB_SBC4, Enable, Reg);
            NV_CLK_RST_WRITE(CLK_OUT_ENB_U, Reg);
            break;
        default:
            NV_ASSERT(!"Invalid Spi Instance");
            break;
     }
}

void
SpiSetClockSpeed(
    NvU32 Instance,
    NvU32 ClockSpeedInKHz)
{
    NvU32 Divisor;
    NvU32 Reg;

    Divisor = MAX_PLLP_CLOCK_FREQUENY_INKHZ / ClockSpeedInKHz;
    Divisor = Divisor << 1;

    SPI_DEBUG_PRINT("Divisor = %d, ClockSpeedInKHz = %d\n", Divisor,
        ClockSpeedInKHz);
    switch (Instance)
    {
        case 0:
            NV_CLK_RST_READ(CLK_SOURCE_SBC1, Reg);
            Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SBC1,
                      SBC1_CLK_SRC, PLLP_OUT0, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SBC1,
                      SBC1_CLK_DIVISOR, Divisor - 1, Reg);
            NV_CLK_RST_WRITE(CLK_SOURCE_SBC1, Reg);
            break;
        case 1:
            NV_CLK_RST_READ(CLK_SOURCE_SBC2, Reg);
            Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SBC2,
                      SBC2_CLK_SRC, PLLP_OUT0, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SBC2,
                      SBC2_CLK_DIVISOR, Divisor - 1, Reg);
            NV_CLK_RST_WRITE(CLK_SOURCE_SBC2, Reg);
            break;
        case 2:
            NV_CLK_RST_READ(CLK_SOURCE_SBC3, Reg);
            Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SBC3,
                      SBC3_CLK_SRC, PLLP_OUT0, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SBC3,
                      SBC3_CLK_DIVISOR, Divisor - 1, Reg);
            NV_CLK_RST_WRITE(CLK_SOURCE_SBC3, Reg);
            break;
        case 3:
            NV_CLK_RST_READ(CLK_SOURCE_SBC4, Reg);
            Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SBC4,
                      SBC4_CLK_SRC, PLLP_OUT0, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SBC4,
                      SBC4_CLK_DIVISOR, Divisor - 1, Reg);
            NV_CLK_RST_WRITE(CLK_SOURCE_SBC4, Reg);
            break;
        default:
            NV_ASSERT(!"Invalid Spi Instance");
            break;
    }
    NvOsWaitUS(HW_WAIT_TIME_IN_US);
}

void
SpiModuleReset(
    NvU32 Instance,
    NvBool Enable)
{
    NvU32 Reg;
    switch (Instance)
    {
        case 0:
            NV_CLK_RST_READ(RST_DEVICES_H, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEVICES_H,
                      SWR_SBC1_RST, Enable, Reg);
            NV_CLK_RST_WRITE(RST_DEVICES_H, Reg);
            break;
        case 1:
            NV_CLK_RST_READ(RST_DEVICES_H, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEVICES_H,
                      SWR_SBC2_RST, Enable, Reg);
            NV_CLK_RST_WRITE(RST_DEVICES_H, Reg);
            break;
        case 2:
            NV_CLK_RST_READ(RST_DEVICES_H, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEVICES_H,
                      SWR_SBC3_RST, Enable, Reg);
            NV_CLK_RST_WRITE(RST_DEVICES_H, Reg);
            break;
        case 3:
            NV_CLK_RST_READ(RST_DEVICES_U, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEVICES_U,
                      SWR_SBC4_RST, Enable, Reg);
            NV_CLK_RST_WRITE(RST_DEVICES_U, Reg);
            break;
        default:
            NV_ASSERT(!"Invalid Spi Instance");
            break;
    }
}
