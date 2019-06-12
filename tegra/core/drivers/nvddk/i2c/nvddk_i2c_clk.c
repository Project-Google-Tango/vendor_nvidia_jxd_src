/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_i2c.h"
#include "nvddk_i2c_debug.h"
#include "nvddk_i2c_hw.h"
#include "nvddk_i2c_clk.h"
#include "nvddk_i2c_controller.h"

#define RESET_CONTROLLER(Clock, Instance)                                      \
    do                                                                         \
    {                                                                          \
        NvU32 Reg;                                                             \
        /* Assert the reset */                                                 \
        NV_CLK_RST_REG_READ(RST_DEVICES_##Clock, Reg);                         \
        Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEVICES_##Clock,      \
                                 SWR_I2C##Instance##_RST, 1, Reg);             \
        NV_CLK_RST_REG_WRITE(RST_DEVICES_##Clock, Reg);                        \
        /* Enable Clock */                                                     \
        NV_CLK_RST_REG_READ(CLK_OUT_ENB_##Clock, Reg);                         \
        Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_##Clock,      \
                                 CLK_ENB_I2C##Instance, 1, Reg);               \
        NV_CLK_RST_REG_WRITE(CLK_OUT_ENB_##Clock, Reg);                        \
        /* Set the clock source to clock M */                                  \
        Reg = NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_I2C##Instance,         \
                         I2C##Instance##_CLK_SRC, CLK_M);                      \
        Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_I2C##Instance, \
                                 I2C##Instance##_CLK_DIVISOR, 0, Reg);         \
        NV_CLK_RST_REG_WRITE(CLK_SOURCE_I2C##Instance, Reg);                   \
        NvDdkI2cStallMs(1);                                                    \
        /* Deassert the reset */                                               \
        NV_CLK_RST_REG_READ(RST_DEVICES_##Clock, Reg);                         \
        Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, RST_DEVICES_##Clock,      \
                                 SWR_I2C##Instance##_RST, 0, Reg);             \
        NV_CLK_RST_REG_WRITE(RST_DEVICES_##Clock, Reg);                        \
        NvDdkI2cStallMs(1);                                                    \
    } while (0)

#define SET_DIVISOR(Instance, Divisor)                                         \
    do                                                                         \
    {                                                                          \
        NvU32 Reg;                                                             \
        NV_CLK_RST_REG_READ(CLK_SOURCE_I2C##Instance, Reg);                    \
        Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_I2C##Instance, \
                               I2C##Instance##_CLK_DIVISOR, Divisor - 1, Reg); \
        NV_CLK_RST_REG_WRITE(CLK_SOURCE_I2C##Instance, Reg);                   \
    } while (0)

NvU32 NvDdkI2cGetTimeMS(void)
{
    NvU32 timeUs = NV_READ32(NV_ADDRESS_MAP_TMRUS_BASE + TIMERUS_CNTR_1US_0);
    /* avoid using division */
    return timeUs >> 10;
}

NvU32 NvDdkI2cGetTimeUS(void)
{
    NvU32 timeUs = NV_READ32(NV_ADDRESS_MAP_TMRUS_BASE + TIMERUS_CNTR_1US_0);
    return timeUs;
}

void NvDdkI2cStallMs(NvU64 MiliSec)
{
    NvU64 TimeStart;

    TimeStart = NvDdkI2cGetTimeMS();
    while (NvDdkI2cGetTimeMS() < (TimeStart + MiliSec))
        ;
}

void NvDdkI2cStallUs(NvU64 USec)
{
    NvU64 TimeStart;

    TimeStart = NvDdkI2cGetTimeUS();
    while (NvDdkI2cGetTimeUS() < (TimeStart + USec))
        ;
}

void NvDdkI2cEnableClDvfs(void)
{
    NvU32 Reg = 0;

    DDK_I2C_INFO_PRINT("%s: entry\n", __func__);

    NV_CLK_RST_REG_READ(CLK_OUT_ENB_W, Reg);

    if (Reg & CLK_RST_CONTROLLER_CLK_OUT_ENB_W_0_CLK_ENB_DVFS_FIELD)
        return;

    /* Assert Reset */
    NV_CLK_RST_REG_READ(RST_DEVICES_W, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W,
                             SWR_DVFS_RST, ENABLE, Reg);
    NV_CLK_RST_REG_WRITE(RST_DEVICES_W, Reg);

    /* Enable clock */
    NV_CLK_RST_REG_READ(CLK_OUT_ENB_W, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_W,
                             CLK_ENB_DVFS, ENABLE, Reg);
    NV_CLK_RST_REG_WRITE(CLK_OUT_ENB_W, Reg);

    /* Deassert Reset */
    NV_CLK_RST_REG_READ(RST_DEVICES_W, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, RST_DEVICES_W,
                             SWR_DVFS_RST, DISABLE, Reg);
    NV_CLK_RST_REG_WRITE(RST_DEVICES_W, Reg);
}

void NvDdkI2cResetController(NvU32 Instance)
{
    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, Instance);

    /* Reset I2c controller */
    switch (Instance)
    {
        case NvDdkI2c1:
            RESET_CONTROLLER(L, 1);
            break;
        case NvDdkI2c2:
            RESET_CONTROLLER(H, 2);
            break;
        case NvDdkI2c3:
            RESET_CONTROLLER(U, 3);
            break;
        case NvDdkI2c4:
            RESET_CONTROLLER(V, 4);
            break;
        case NvDdkI2c5:
            RESET_CONTROLLER(H, 5);
            break;
        case NvDdkI2c6:
            RESET_CONTROLLER(X, 6);
            break;
        default:
            NV_ASSERT(!"Invalid I2C Controller");
    }
}

void NvDdkI2cSetDivisor(NvU32 Instance, NvU32 Divisor)
{
    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d Divisor %d\n",
                       __func__, Instance, Divisor);
    switch (Instance)
    {
        case NvDdkI2c1:
            SET_DIVISOR(1, Divisor);
            break;
        case NvDdkI2c2:
            SET_DIVISOR(2, Divisor);
            break;
        case NvDdkI2c3:
            SET_DIVISOR(3, Divisor);
            break;
        case NvDdkI2c4:
            SET_DIVISOR(4, Divisor);
            break;
        case NvDdkI2c5:
            SET_DIVISOR(5, Divisor);
            break;
        case NvDdkI2c6:
            SET_DIVISOR(6, Divisor);
            break;
        default:
            NV_ASSERT(!"Invalid I2C Controller");
    }
}

NvU32 NvDdkIntDiv(NvU32 Dividend, NvU32 Divisor)
{
    NvU32 i = 0;
    NvU32 OldVal = 0;
    NvU32 NewVal = 0;

    NV_ASSERT(Divisor > 0);

    DDK_I2C_INFO_PRINT("%s: entry Dividend %d Divisor %d\n",
                       __func__, Dividend, Divisor);

    NewVal = Divisor;
    while ((NewVal <= Dividend) && (OldVal < NewVal))
    {
        OldVal = NewVal;
        NewVal += Divisor;
        i++;
    }

    return i;
}

void NvDdkI2cSetFrequency(const NvDdkI2cHandle hI2c, NvU32 Frequency)
{
    NvU32 Reg = 0;
    NvU32 OscFreq = 0;
    NvU32 OscFreqKHz = 0;
    NvU32 Divisor = 0;
    NvU32 Chip = 0;
    static NvU32 s_OscFreq2KHz[] = {13000, 16800, 0, 0, 19200, 38400, 0, 0,
                                    12000, 48000, 0, 0, 26000, 0, 0, 0};

    NV_ASSERT(hI2c != NULL);

    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, hI2c->Instance);

    NV_CLK_RST_REG_READ(OSC_CTRL, Reg);
    OscFreq = NV_DRF_VAL(CLK_RST_CONTROLLER, OSC_CTRL, OSC_FREQ, Reg);

    OscFreqKHz = s_OscFreq2KHz[OscFreq];
    Divisor = NvDdkIntDiv(OscFreqKHz >> 3, Frequency);

    Chip = NvDdkI2cGetChipId();

    if (Chip != 0x30)
    {
        if (Chip == 0x35)
            Divisor = ((Divisor > 7) ? Divisor : 8);
        else
            Divisor = ((Divisor > 1) ? Divisor : 2);

        Reg = 0;
        Reg = NV_DRF_NUM(I2C, I2C_CLK_DIVISOR_REGISTER,
                              I2C_CLK_DIVISOR_STD_FAST_MODE, Divisor - 1);
        Reg = NV_FLD_SET_DRF_NUM(I2C, I2C_CLK_DIVISOR_REGISTER,
                              I2C_CLK_DIVISOR_HSMODE, 1, Reg);
        NV_DDK_I2C_REG_WRITE(hI2c->Offset, I2C_CLK_DIVISOR_REGISTER, Reg);
    }
    else
    {
        Divisor = ((Divisor > 0) ? Divisor : 1);
        NvDdkI2cSetDivisor(hI2c->Instance, Divisor);
    }
}

