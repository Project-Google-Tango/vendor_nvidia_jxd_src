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
                             CLK_ENB_SPI1, Enable, Reg);
            NV_CLK_RST_WRITE(CLK_OUT_ENB_H, Reg);
            break;
        case 1:
            NV_CLK_RST_READ(CLK_OUT_ENB_H, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                             CLK_ENB_SPI2, Enable, Reg);
            NV_CLK_RST_WRITE(CLK_OUT_ENB_H, Reg);
            break;
        case 2:
            NV_CLK_RST_READ(CLK_OUT_ENB_H, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_H,
                             CLK_ENB_SPI3, Enable, Reg);
            NV_CLK_RST_WRITE(CLK_OUT_ENB_H, Reg);
            break;
        case 3:
            NV_CLK_RST_READ(CLK_OUT_ENB_U, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_U,
                             CLK_ENB_SPI4, Enable, Reg);
            NV_CLK_RST_WRITE(CLK_OUT_ENB_U, Reg);
            break;
        case 4:
            NV_CLK_RST_READ(CLK_OUT_ENB_V, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_V,
                             CLK_ENB_SPI5, Enable, Reg);
            NV_CLK_RST_WRITE(CLK_OUT_ENB_V, Reg);
            break;
        case 5:
            NV_CLK_RST_READ(CLK_OUT_ENB_V, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_V,
                             CLK_ENB_SPI6, Enable, Reg);
            NV_CLK_RST_WRITE(CLK_OUT_ENB_V, Reg);
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
            NV_CLK_RST_READ(CLK_SOURCE_SPI1, Reg);
            Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SPI1,
                      SPI1_CLK_SRC, PLLP_OUT0, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SPI1,
                      SPI1_CLK_DIVISOR, Divisor - 1, Reg);
            NV_CLK_RST_WRITE(CLK_SOURCE_SPI1, Reg);
            break;
        case 1:
            NV_CLK_RST_READ(CLK_SOURCE_SPI2, Reg);
            Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SPI2,
                      SPI2_CLK_SRC, PLLP_OUT0, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SPI2,
                      SPI2_CLK_DIVISOR, Divisor - 1, Reg);
            NV_CLK_RST_WRITE(CLK_SOURCE_SPI2, Reg);
            break;
        case 2:
            NV_CLK_RST_READ(CLK_SOURCE_SPI3, Reg);
            Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SPI3,
                      SPI3_CLK_SRC, PLLP_OUT0, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SPI3,
                      SPI3_CLK_DIVISOR, Divisor - 1, Reg);
            NV_CLK_RST_WRITE(CLK_SOURCE_SPI3, Reg);
            break;
        case 3:
            NV_CLK_RST_READ(CLK_SOURCE_SPI4, Reg);
            Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SPI4,
                      SPI4_CLK_SRC, PLLP_OUT0, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SPI4,
                      SPI4_CLK_DIVISOR, Divisor - 1, Reg);
            NV_CLK_RST_WRITE(CLK_SOURCE_SPI4, Reg);
            break;
        case 4:
            NV_CLK_RST_READ(CLK_SOURCE_SPI5, Reg);
            Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SPI5,
                      SPI5_CLK_SRC, PLLP_OUT0, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SPI5,
                      SPI5_CLK_DIVISOR, Divisor - 1, Reg);
            NV_CLK_RST_WRITE(CLK_SOURCE_SPI5, Reg);
            break;
        case 5:
            NV_CLK_RST_READ(CLK_SOURCE_SPI6, Reg);
            Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_SPI6,
                      SPI6_CLK_SRC, PLLP_OUT0, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_SPI6,
                      SPI6_CLK_DIVISOR, Divisor - 1, Reg);
            NV_CLK_RST_WRITE(CLK_SOURCE_SPI6, Reg);
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
                      SWR_SPI1_RST, Enable, Reg);
            NV_CLK_RST_WRITE(RST_DEVICES_H, Reg);
            break;
        case 1:
            NV_CLK_RST_READ(RST_DEVICES_H, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEVICES_H,
                      SWR_SPI2_RST, Enable, Reg);
            NV_CLK_RST_WRITE(RST_DEVICES_H, Reg);
            break;
        case 2:
            NV_CLK_RST_READ(RST_DEVICES_H, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEVICES_H,
                      SWR_SPI3_RST, Enable, Reg);
            NV_CLK_RST_WRITE(RST_DEVICES_H, Reg);
            break;
        case 3:
            NV_CLK_RST_READ(RST_DEVICES_U, Reg);
            Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEVICES_U,
                      SWR_SPI4_RST, Enable, Reg);
            NV_CLK_RST_WRITE(RST_DEVICES_U, Reg);
            break;
        default:
            NV_ASSERT(!"Invalid Spi Instance");
            break;
    }
}
