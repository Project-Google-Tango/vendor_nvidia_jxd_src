/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvrm_drf.h"
#include "nvrm_clocks.h"
#include "t30/arclk_rst.h"
#include "nvrm_moduleids.h"
#include "t30/project_relocation_table.h"

#define NV_COMMON_CLK_RST_FIELDS_INFO(MODULE, H_L) \
        CLK_RST_CONTROLLER_CLK_SOURCE_##MODULE##_0, \
        CLK_RST_CONTROLLER_CLK_SOURCE_##MODULE##_0_##MODULE##_CLK_SRC_DEFAULT_MASK, \
        CLK_RST_CONTROLLER_CLK_SOURCE_##MODULE##_0_##MODULE##_CLK_SRC_SHIFT, \
        CLK_RST_CONTROLLER_CLK_SOURCE_##MODULE##_0_##MODULE##_CLK_DIVISOR_DEFAULT_MASK, \
        CLK_RST_CONTROLLER_CLK_SOURCE_##MODULE##_0_##MODULE##_CLK_DIVISOR_SHIFT, \
        CLK_RST_CONTROLLER_CLK_OUT_ENB_##H_L##_0, \
        CLK_RST_CONTROLLER_CLK_OUT_ENB_##H_L##_0_CLK_ENB_##MODULE##_FIELD, \
        CLK_RST_CONTROLLER_RST_DEVICES_##H_L##_0, \
        CLK_RST_CONTROLLER_RST_DEVICES_##H_L##_0_SWR_##MODULE##_RST_FIELD

const NvRmModuleClockInfo g_ModuleClockTable[] =
{
    {   /* Invalid module */
        NvRmPrivModuleID_System, 0, 0,
        {
            NvRmClockSource_SystemBus
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        0,0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_TRIG_SYS_RST_FIELD,
        NvRmDiagModuleID_SystemReset
    },
    {   /* VI controller module - VI clock */
        NvRmModuleID_Vi, 0 , 0,
        {
            NvRmClockSource_PllM0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllP0,
            NvRmClockSource_PllA0
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_VI_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_VI_0_VI_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_VI_0_VI_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_VI_0_VI_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_VI_0_VI_CLK_DIVISOR_SHIFT,

        // Combined VI and VI sensor reset and clock enable controls
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_VI_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_VI_RST_FIELD,
        NvRmDiagModuleID_Vi
    },
    {   /* VI controller module - VI sensor clock
         * Module sub clock must immediately follow main clock
         */
        NvRmModuleID_Vi, 0 , 1,
        {
            NvRmClockSource_PllM0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllP0,
            NvRmClockSource_PllA0
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_VI_SENSOR_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_VI_SENSOR_0_VI_SENSOR_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_VI_SENSOR_0_VI_SENSOR_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_VI_SENSOR_0_VI_SENSOR_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_VI_SENSOR_0_VI_SENSOR_CLK_DIVISOR_SHIFT,

        // Combined VI and VI sensor reset and clock enable controls
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_VI_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_VI_RST_FIELD,
        NvRmDiagModuleID_ViSensor
    },

    {   /* I2S0 controller module */
        NvRmModuleID_I2s, 0, 0,
        {
            NvRmClockSource_PllA0,
            NvRmClockSource_AudioSync,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(I2S0, L),
        NvRmDiagModuleID_I2s
    },
    {   /* I2S1 controller module */
        NvRmModuleID_I2s, 1, 0,
        {
            NvRmClockSource_PllA0,
            NvRmClockSource_AudioSync,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(I2S1, L),
        NvRmDiagModuleID_I2s
    },
    {   /* I2S2 controller module */
        NvRmModuleID_I2s, 2, 0,
        {
            NvRmClockSource_PllA0,
            NvRmClockSource_AudioSync,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(I2S2, L),
        NvRmDiagModuleID_I2s
    },
    {   /* I2S3 controller module */
        NvRmModuleID_I2s, 3, 0,
        {
            NvRmClockSource_PllA0,
            NvRmClockSource_AudioSync,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(I2S3, V),
        NvRmDiagModuleID_I2s
    },
    {   /* I2S4 controller module */
        NvRmModuleID_I2s, 4, 0,
        {
            NvRmClockSource_PllA0,
            NvRmClockSource_AudioSync,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(I2S4, V),
        NvRmDiagModuleID_I2s
    },

    {   /* I2C1 controller module */
        NvRmModuleID_I2c, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Integer_1,
        NV_COMMON_CLK_RST_FIELDS_INFO(I2C1, L),
        NvRmDiagModuleID_I2c
    },

    {   /* I2C2 controller module */
        NvRmModuleID_I2c, 1, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Integer_1,
        NV_COMMON_CLK_RST_FIELDS_INFO(I2C2, H),
        NvRmDiagModuleID_I2c
    },
    {   /* I2C3 controller module */
        NvRmModuleID_I2c, 2, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Integer_1,
        NV_COMMON_CLK_RST_FIELDS_INFO(I2C3, U),
        NvRmDiagModuleID_I2c
    },

    {   /* I2C4 controller module */
        NvRmModuleID_I2c, 3, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Integer_1,
        NV_COMMON_CLK_RST_FIELDS_INFO(I2C4, V),
        NvRmDiagModuleID_I2c
    },
    {   /* DVC_I2C controller module */
        NvRmModuleID_I2c, 4, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Integer_1,
        NV_COMMON_CLK_RST_FIELDS_INFO(DVC_I2C, H),
        NvRmDiagModuleID_I2c
    },
    {   /* I2C_SLOW controller module */
        NvRmModuleID_I2c, 5, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_ClkS,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(I2C_SLOW, U),
        NvRmDiagModuleID_I2c
    },

    {   /* S/PDIF controller module - S/PDIF OUT clock */
        NvRmModuleID_Spdif, 0, 0,
        {
            NvRmClockSource_PllA0,
            NvRmClockSource_AudioSync,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_OUT_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_OUT_0_SPDIFOUT_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_OUT_0_SPDIFOUT_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_OUT_0_SPDIFOUT_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_OUT_0_SPDIFOUT_CLK_DIVISOR_SHIFT,

        // Combined SPDIF reset and and clock enable controls
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_SPDIF_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_SPDIF_RST_FIELD,
        NvRmDiagModuleID_Spdif
    },
    {   /* S/PDIF controller module - S/PDIF IN clock
         * Module sub clock must immediately follow main clock
         */
        NvRmModuleID_Spdif, 0, 1,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_IN_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_IN_0_SPDIFIN_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_IN_0_SPDIFIN_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_IN_0_SPDIFIN_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_SPDIF_IN_0_SPDIFIN_CLK_DIVISOR_SHIFT,

        // Combined SPDIF reset and and clock enable controls
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_SPDIF_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_SPDIF_RST_FIELD,
        NvRmDiagModuleID_SpdifIn
    },

    {   /* PWM controller module */
        NvRmModuleID_Pwm, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_AudioSync,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(PWM, L),
        NvRmDiagModuleID_Pwm
    },

    {   /* SBC1 controller module */
        NvRmModuleID_Slink, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SBC1, H),
        NvRmDiagModuleID_Sbc
    },

    {   /* SBC2 controller module */
        NvRmModuleID_Slink, 1, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SBC2, H),
        NvRmDiagModuleID_Sbc
    },

    {   /* SBC3 controller module */
        NvRmModuleID_Slink, 2, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SBC3, H),
        NvRmDiagModuleID_Sbc
    },

    {   /* SBC4 controller module */
        NvRmModuleID_Slink, 3, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SBC4, U),
        NvRmDiagModuleID_Sbc
    },
    {   /* SBC5 controller module */
        NvRmModuleID_Slink, 4, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SBC5, V),
        NvRmDiagModuleID_Sbc
    },
    {   /* SBC6 controller module */
        NvRmModuleID_Slink, 5, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SBC6, V),
        NvRmDiagModuleID_Sbc
    },

    {   /* TWC controller module */
        NvRmModuleID_Twc, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(TWC, L),
        NvRmDiagModuleID_Twc
    },

    {   /* XIO controller module */
        NvRmModuleID_Xio, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(XIO, H),
        NvRmDiagModuleID_Xio
    },

    {   /* SDIO1 controller module */
        NvRmModuleID_Sdio, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SDMMC1, L),
        NvRmDiagModuleID_Sdio
    },

    {   /* SDIO2 controller module */
        NvRmModuleID_Sdio, 1, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SDMMC2, L),
        NvRmDiagModuleID_Sdio
    },

    {   /* SDIO3 controller module */
        NvRmModuleID_Sdio, 2, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SDMMC3, U),
        NvRmDiagModuleID_Sdio
    },

    {   /* SDIO4 controller module */
        NvRmModuleID_Sdio, 3, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SDMMC4, L),
        NvRmDiagModuleID_Sdio
    },

    {   /* NAND Flash controller module */
        NvRmModuleID_Nand, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(NDFLASH, L),
        NvRmDiagModuleID_NandFlash
    },
    {   /* NAND Speed controller module
         * Module sub clocks must immediately follow main clock
         */
        NvRmModuleID_Nand, 0, 1,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(NAND_SPEED, U),
        NvRmDiagModuleID_NandFlash
    },

    {   /*  MIPI BB controller module */
        NvRmModuleID_Mipi, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(HSI, H),
        NvRmDiagModuleID_MipiBaseband
    },

    {   /* UARTA controller module */
        NvRmModuleID_Uart, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_None,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTA_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTA_0_UARTA_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTA_0_UARTA_CLK_SRC_SHIFT,
        0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_UARTA_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_UARTA_RST_FIELD,
        NvRmDiagModuleID_Uart
    },

    {   /* UARTB controller module */
        NvRmModuleID_Uart, 1, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_None,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTB_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTB_0_UARTB_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTB_0_UARTB_CLK_SRC_SHIFT,
        0, 0,

        // Combined UARTB and VFIR reset and clock enable controls
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_UARTB_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_UARTB_RST_FIELD,
        NvRmDiagModuleID_Uart
    },

    {   /* UARTC controller module */
        NvRmModuleID_Uart, 2, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_None,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTC_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTC_0_UARTC_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTC_0_UARTC_CLK_SRC_SHIFT,
        0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_UARTC_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_UARTC_RST_FIELD,
        NvRmDiagModuleID_Uart
    },

    {   /* UARTD controller module */
        NvRmModuleID_Uart, 3, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_None,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTD_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTD_0_UARTD_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTD_0_UARTD_CLK_SRC_SHIFT,
        0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0_CLK_ENB_UARTD_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_U_0,
        CLK_RST_CONTROLLER_RST_DEVICES_U_0_SWR_UARTD_RST_FIELD,
        NvRmDiagModuleID_Uart
    },

    {   /* UARTE controller module */
        NvRmModuleID_Uart, 4, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_None,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTE_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTE_0_UARTE_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_UARTE_0_UARTE_CLK_SRC_SHIFT,
        0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0_CLK_ENB_UARTE_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_U_0,
        CLK_RST_CONTROLLER_RST_DEVICES_U_0_SWR_UARTE_RST_FIELD,
        NvRmDiagModuleID_Uart
    },

    {   /* VFIR controller module */
        NvRmModuleID_Vfir, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_VFIR_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_VFIR_0_VFIR_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_VFIR_0_VFIR_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_VFIR_0_VFIR_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_VFIR_0_VFIR_CLK_DIVISOR_SHIFT,

        // Combined UARTB and VFIR reset and clock enable controls
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_UARTB_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_UARTB_RST_FIELD,
        NvRmDiagModuleID_Vfir
    },

    {   /* Host1x module */
        NvRmModuleID_GraphicsHost, 0, 0,
        {
            NvRmClockSource_PllM0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllP0,
            NvRmClockSource_PllA0
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(HOST1X, L),
        NvRmDiagModuleID_Host1x
    },

    {   /* EPP controller module */
        NvRmModuleID_Epp, 0, 0,
        {
            NvRmClockSource_PllM0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllP0,
            NvRmClockSource_PllA0
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(EPP, L),
        NvRmDiagModuleID_Epp
    },

    {   /* MPE controller module */
        NvRmModuleID_Mpe, 0, 0,
        {
            NvRmClockSource_PllM0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllP0,
            NvRmClockSource_PllA0
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(MPE, H),
        NvRmDiagModuleID_Mpe
    },

    {   /* 2D controller module */
        NvRmModuleID_2D, 0, 0,
        {
            NvRmClockSource_PllM0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllP0,
            NvRmClockSource_PllA0
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_G2D_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_G2D_0_G2D_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_G2D_0_G2D_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_G2D_0_G2D_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_G2D_0_G2D_CLK_DIVISOR_SHIFT,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_2D_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_2D_RST_FIELD,
        NvRmDiagModuleID_2d
    },

    {   /* 3D controller module */
        NvRmModuleID_3D, 0, 0,
        {
            NvRmClockSource_PllM0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllP0,
            NvRmClockSource_PllA0
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_G3D_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_G3D_0_G3D_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_G3D_0_G3D_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_G3D_0_G3D_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_G3D_0_G3D_CLK_DIVISOR_SHIFT,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_3D_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_3D_RST_FIELD,
        NvRmDiagModuleID_3d
    },

    {   /* 3D2 controller module */
        NvRmModuleID_3D, 1, 0,
        {
            NvRmClockSource_PllM0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllP0,
            NvRmClockSource_PllA0
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_G3D2_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_G3D2_0_G3D2_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_G3D2_0_G3D2_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_G3D2_0_G3D2_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_G3D2_0_G3D2_CLK_DIVISOR_SHIFT,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0_CLK_ENB_3D2_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_V_0,
        CLK_RST_CONTROLLER_RST_DEVICES_V_0_SWR_3D2_RST_FIELD,
        NvRmDiagModuleID_3d
    },

    {   /* Display 1 controller module */
        NvRmModuleID_Display, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllM0,
            NvRmClockSource_PllD0,
            NvRmClockSource_PllA0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllC1,   // FIXME: placeholder for PLLD2
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_None,
        CLK_RST_CONTROLLER_CLK_SOURCE_DISP1_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_DISP1_0_DISP1_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_DISP1_0_DISP1_CLK_SRC_SHIFT,
        0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_DISP1_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_DISP1_RST_FIELD,
        NvRmDiagModuleID_Display
    },

    {   /* Display 2 controller module */
        NvRmModuleID_Display, 1, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllM0,
            NvRmClockSource_PllD0,
            NvRmClockSource_PllA0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllC1,   // FIXME: placeholder for PLLD2
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_None,
        CLK_RST_CONTROLLER_CLK_SOURCE_DISP2_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_DISP2_0_DISP2_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_DISP2_0_DISP2_CLK_SRC_SHIFT,
        0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_DISP2_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_DISP2_RST_FIELD,
        NvRmDiagModuleID_Display
    },

    {   /* TVO controller module - TVO clock */
        NvRmModuleID_Tvo, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllD0,
            NvRmClockSource_PllC0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_TVO_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_TVO_0_TVO_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_TVO_0_TVO_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_TVO_0_TVO_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_TVO_0_TVO_CLK_DIVISOR_SHIFT,

        // Combined TVO, and CVE reset and and clock enable controls
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_TVO_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_TVO_RST_FIELD,
        NvRmDiagModuleID_Tvo
    },
    {   /* TVO controller module - CVE clock
         * Module sub clocks must immediately follow main clock
         */
        NvRmModuleID_Tvo, 0, 1,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllD0,
            NvRmClockSource_PllC0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_CVE_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_CVE_0_CVE_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_CVE_0_CVE_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_CVE_0_CVE_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_CVE_0_CVE_CLK_DIVISOR_SHIFT,

        // Combined TVO, and CVE reset and and clock enable controls
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_TVO_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_TVO_RST_FIELD,
        NvRmDiagModuleID_Cve
    },
    {   /* TVO controller module - TVDAC clock
         * Module sub clocks must immediately follow main clock
         */
        NvRmModuleID_Tvo, 0, 2,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllD0,
            NvRmClockSource_PllC0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_TVDAC_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_TVDAC_0_TVDAC_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_TVDAC_0_TVDAC_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_TVDAC_0_TVDAC_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_TVDAC_0_TVDAC_CLK_DIVISOR_SHIFT,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_TVDAC_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_TVDAC_RST_FIELD,
        NvRmDiagModuleID_Tvdac
    },

    {   /* HDMI controller module */
        NvRmModuleID_Hdmi, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllM0,
            NvRmClockSource_PllD0,
            NvRmClockSource_PllA0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllC1,   // FIXME: placeholder for PLLD2
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(HDMI, H),
        NvRmDiagModuleID_Hdmi
    },

    {   /* VDE controller module (VDE and BSEV clocks)
         * These clocks should always be enabled/reset in sync. Threfore,
         * no need for separate VDE and BSEV subclock descriptors
         */
        NvRmModuleID_Vde, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_VDE_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_VDE_0_VDE_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_VDE_0_VDE_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_VDE_0_VDE_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_VDE_0_VDE_CLK_DIVISOR_SHIFT,

       // Combined VDE and BSEV reset and and clock enable controls
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        (CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_VDE_FIELD |
         CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_BSEV_FIELD),
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        (CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_VDE_RST_FIELD |
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_BSEV_RST_FIELD),
        NvRmDiagModuleID_Vde
    },

    {   /*  BSEA controller module */
        NvRmModuleID_BseA, 0, 0,
        {
            NvRmClockSource_SystemBus
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_BSEA_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_BSEA_RST_FIELD,
        NvRmDiagModuleID_Bsea
    },

    {   /* VCP controller module */
        NvRmModuleID_Vcp, 0, 0,
        {
            NvRmClockSource_SystemBus
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_VCP_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_VCP_RST_FIELD,
        NvRmDiagModuleID_Vcp
    },

    {   /* Timer controller module */
        NvRmModuleID_Timer, 0, 0,
        {
            NvRmClockSource_SystemBus
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_TMR_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_TMR_RST_FIELD,
        NvRmDiagModuleID_Timer
    },

    {   /*  System Monitor controller module */
        NvRmModuleID_SysStatMonitor, 0, 0,
        {
            NvRmClockSource_SystemBus
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_STAT_MON_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_STAT_MON_RST_FIELD,
        NvRmDiagModuleID_StatMon
    },

    {   /* GPIO controller module */
        NvRmPrivModuleID_Gpio, 0, 0,
        {
            NvRmClockSource_SystemBus
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_GPIO_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_GPIO_RST_FIELD,
        NvRmDiagModuleID_Gpio
    },

    {   /* USB controller module */
        NvRmModuleID_Usb2Otg, 0, 0,
        {
            NvRmClockSource_PllU0
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_USBD_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_USBD_RST_FIELD,
        NvRmDiagModuleID_Usb
    },

    {   /* USB2 controller module */
        NvRmModuleID_Usb2Otg, 1, 0,
        {
            NvRmClockSource_PllU0
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_USB2_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_USB2_RST_FIELD,
        NvRmDiagModuleID_Usb
    },

    {   /* USB3 controller module */
        NvRmModuleID_Usb2Otg, 2, 0,
        {
            NvRmClockSource_PllU0
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_USB3_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_USB3_RST_FIELD,
        NvRmDiagModuleID_Usb
    },

    {   /*  APB DMA controller module */
        NvRmPrivModuleID_ApbDma, 0, 0,
        {
            NvRmClockSource_Apb
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_APBDMA_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_APBDMA_RST_FIELD,
        NvRmDiagModuleID_ApbDma
    },

    {   /* APB IF controller module */
        NvRmModuleID_Apbif, 0, 0,
        {
            NvRmClockSource_Apb
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0_CLK_ENB_APBIF_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_V_0,
        CLK_RST_CONTROLLER_RST_DEVICES_V_0_SWR_APBIF_RST_FIELD,
        NvRmDiagModuleID_Apbif
    },

    {   /*  Keyboard controller module */
        NvRmModuleID_Kbc, 0, 0,
        {
            NvRmClockSource_Apb
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_KBC_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        0,
        NvRmDiagModuleID_Kbc
    },

    {   /* RTC  controller module */
        NvRmModuleID_Rtc, 0, 0,
        {
            NvRmClockSource_Apb
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_RTC_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        0,
        NvRmDiagModuleID_Rtc
    },

    {   /*  Fuse controller module */
        NvRmModuleID_Fuse, 0, 0,
        {
            NvRmClockSource_Apb
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_FUSE_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_FUSE_RST_FIELD,
        NvRmDiagModuleID_Fuse
    },

    {   /*  KFuse controller module */
        NvRmModuleID_KFuse, 0, 0,
        {
            NvRmClockSource_Apb
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_KFUSE_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_KFUSE_RST_FIELD,
        NvRmDiagModuleID_KFuse
    },

    {   /*  Power Management controller module */
        NvRmModuleID_Pmif, 0, 0,
        {
            NvRmClockSource_Apb
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_PMC_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        0,
        NvRmDiagModuleID_Pmc
    },

    {   /* COP (AVP) cache controller module */
        NvRmModuleID_CacheMemCtrl, 0, 0,
        {
            NvRmClockSource_SystemBus
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_CACHE2_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_CACHE2_RST_FIELD,
        NvRmDiagModuleID_Cache
    },

    {   /* DSI controller module */
        NvRmModuleID_Dsi, 0, 0,
        {
            NvRmClockSource_PllD0
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_DSI_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_DSI_RST_FIELD,
        NvRmDiagModuleID_Dsi
    },

    {   /* CSI  controller module */
        NvRmModuleID_Csi, 0, 0,
        {
            NvRmClockSource_SystemBus // TODO: find a proper clock source
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_CSI_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_CSI_RST_FIELD,
        NvRmDiagModuleID_Csi
    },

    {   /* ISP controller module */
        NvRmModuleID_Isp, 0, 0,
        {
            NvRmClockSource_SystemBus // TODO: find a proper clock source
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_ISP_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_ISP_RST_FIELD,
        NvRmDiagModuleID_Isp
    },

    {   /*  CPU module */
        NvRmModuleID_Cpu, 0, 0,
        {
            NvRmClockSource_CpuBus
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_CPU_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_CPU_RST_FIELD,
        NvRmDiagModuleID_Cpu
    },

    {   /*  COP (AVP) module */
        NvRmModuleID_Avp, 0, 0,
        {
            NvRmClockSource_SystemBus // TODO: Add COP skipper source?
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        0, 0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0,
        CLK_RST_CONTROLLER_RST_DEVICES_L_0_SWR_COP_RST_FIELD,
        NvRmDiagModuleID_Coprocessor
    },

    {
        NvRmModuleID_OneWire, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(OWR,U),
        NvRmDiagModuleID_OneWire
    },

    {
        NvRmModuleID_SyncNor, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM,
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_NOR_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_NOR_0_SNOR_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_NOR_0_SNOR_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_NOR_0_SNOR_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_NOR_0_SNOR_CLK_DIVISOR_SHIFT,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_SNOR_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_SNOR_RST_FIELD,
        NvRmDiagModuleID_SyncNor
    },

    {
        NvRmModuleID_AvpUcq, 0, 0,
        {
            NvRmClockSource_SystemBus // TODO: Add COP skipper source?
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0_CLK_ENB_AVPUCQ_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_U_0,
        CLK_RST_CONTROLLER_RST_DEVICES_U_0_SWR_AVPUCQ_RST_FIELD,
        NvRmDiagModuleID_AvpUcq
    },

    {
        NvRmPrivModuleID_Pcie, 0, 0,
        {
            NvRmClockSource_CpuBridge
        },
        NvRmClockDivider_None,
        0, 0, 0, 0, 0,

        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0_CLK_ENB_PCIE_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_U_0,
        CLK_RST_CONTROLLER_RST_DEVICES_U_0_SWR_PCIE_RST_FIELD,
        NvRmDiagModuleID_Pcie
    },

    {   /*  Memory controller module */
        NvRmPrivModuleID_MemoryController, 0, 0,
        {
            NvRmClockSource_PllM0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM,
        },
        // MC clock is the same as EMC1x domain clock
        NvRmClockDivider_Integer_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_DIVISOR_SHIFT,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_MEM_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_MEM_RST_FIELD,
        NvRmDiagModuleID_Mc
    },

    {   /*  External Memory controller module */
        NvRmPrivModuleID_ExternalMemoryController, 0, 0,
        {
            NvRmClockSource_PllM0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM,
        },
        NvRmClockDivider_Fractional_2,
        CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0,
        CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_SRC_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_SRC_SHIFT,
        CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SOURCE_EMC_0_EMC_2X_CLK_DIVISOR_SHIFT,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0_CLK_ENB_EMC_FIELD,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0,
        CLK_RST_CONTROLLER_RST_DEVICES_H_0_SWR_EMC_RST_FIELD,
        NvRmDiagModuleID_Emc
    },

    {   /* HDA controller module */
        NvRmModuleID_HDA, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(HDA, V),
        NvRmDiagModuleID_HDA
    },

    {   /* HDA to Codec X2 controller module
         * Module sub clocks must immediately follow main clock
         */
        NvRmModuleID_HDA, 0, 1,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(HDA2CODEC_2X, V),
        NvRmDiagModuleID_HDA
    },

    {   /* Memory configuration module */
        NvRmPrivModuleID_Mselect, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(MSELECT, V),
        NvRmDiagModuleID_Mselect
    },

    {   /* SATA controller module */
        NvRmModuleID_Sata, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SATA, V),
        NvRmDiagModuleID_Sata
    },
    {   /* SATA OOB controller module
         * Module sub clocks must immediately follow main clock
         */
        NvRmModuleID_Sata, 0, 1,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SATA_OOB, V),
        NvRmDiagModuleID_Sata
    },

    {   /* Tsensor controller module */
        NvRmModuleID_Tsensor, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_ClkM,
            NvRmClockSource_ClkS,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(TSENSOR, V),
        NvRmDiagModuleID_Tsensor
    },

    {   /* ACTMON controller module */
        NvRmModuleID_Actmon, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_ClkS,
            NvRmClockSource_ClkM,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(ACTMON, V),
        NvRmDiagModuleID_Actmon
    },

    {   /* SE controller module */
        NvRmModuleID_Se, 0, 0,
        {
            NvRmClockSource_PllP0,
            NvRmClockSource_PllC0,
            NvRmClockSource_PllM0,
            NvRmClockSource_ClkM,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(SE, V),
        NvRmDiagModuleID_Se
    },
    {   /*  External Peripheral  module */
        NvRmModuleID_ExtPeriphClk, 0, 0,
        {
            NvRmClockSource_PllA0,
            NvRmClockSource_ClkS,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM,
            NvRmClockSource_PllE0,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(EXTPERIPH1, V),
        NvRmDiagModuleID_ExtPeriphClk
    },
    {   /*  External Peripheral  module */
        NvRmModuleID_ExtPeriphClk, 1, 0,
        {
            NvRmClockSource_PllA0,
            NvRmClockSource_ClkS,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM,
            NvRmClockSource_PllE0,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(EXTPERIPH2, V),
        NvRmDiagModuleID_ExtPeriphClk
    },
    {   /*  External Peripheral  module */
        NvRmModuleID_ExtPeriphClk, 2, 0,
        {
            NvRmClockSource_PllA0,
            NvRmClockSource_ClkS,
            NvRmClockSource_PllP0,
            NvRmClockSource_ClkM,
            NvRmClockSource_PllE0,
        },
        NvRmClockDivider_Fractional_2,
        NV_COMMON_CLK_RST_FIELDS_INFO(EXTPERIPH3, V),
        NvRmDiagModuleID_ExtPeriphClk
    },
};

NvU32 const g_ModuleClockTableSize = NV_ARRAY_SIZE(g_ModuleClockTable);

/*****************************************************************************/
/*****************************************************************************/
// Clock sources

static const NvRmFixedClockInfo s_T30FixedClockTable[] =
{
    {
        NvRmClockSource_ClkS,
        NvRmClockSource_Invalid,
        0, 0
    },
    {
        NvRmClockSource_ClkM,
        NvRmClockSource_Invalid,
        0, 0
    },
    {
        NvRmClockSource_ClkD,
        NvRmClockSource_ClkM,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0,
        CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0_CLK_M_DOUBLER_ENB_FIELD
    },

    {
        NvRmClockSource_ExtSpdf,
        NvRmClockSource_Invalid,
        0, 0
    },
    {
        NvRmClockSource_ExtI2s1,
        NvRmClockSource_Invalid,
        0, 0
    },
    {
        NvRmClockSource_ExtI2s2,
        NvRmClockSource_Invalid,
        0, 0
    },
    {
        NvRmClockSource_ExtAc97,
        NvRmClockSource_Invalid,
        0, 0
    },
    {
        NvRmClockSource_ExtAudio1,
        NvRmClockSource_Invalid,
        0, 0
    },
    {
        NvRmClockSource_ExtAudio2,
        NvRmClockSource_Invalid,
        0, 0
    },
    {
        NvRmClockSource_ExtVi,
        NvRmClockSource_Invalid,
        0, 0
    }
};

static const NvU32 s_T30FixedClockTableSize = NV_ARRAY_SIZE(s_T30FixedClockTable);

/*****************************************************************************/

// TODO: Specify PLL ref divider in OSC control reg as PLL C, D, M, P, U source

/*
 * Notation clarification: in h/w documentation PLL base outputs (except PLLA
 * output) are denoted as PllX_OUT0, and the seconadry PLL outputs (if any)
 * after fractional dividers are denoted as PllX_OUT1, PllX_OUT2, .... However,
 * no h/w name is defined for the base PLLA output, and the output of the PLLA
 * secondary divider is marked as PllA_OUT0 (not PllA_OUT1). Threfore, we use
 * PllA1 (not PllA0) to denote base PLLA clock.
 */
static const NvRmPllClockInfo s_T30PllClockTable[] =
{
    {   /* PLLA base output */
        NvRmClockSource_PllA1,
        NvRmClockSource_PllP1,
        NvRmPllType_LP,
        CLK_RST_CONTROLLER_PLLA_BASE_0,
        CLK_RST_CONTROLLER_PLLA_MISC_0,
        50000,
        1400000
    },

    {   /* PLLC base output */
        NvRmClockSource_PllC0,
        NvRmClockSource_PllRef,
        NvRmPllType_LP,
        CLK_RST_CONTROLLER_PLLC_BASE_0,
        CLK_RST_CONTROLLER_PLLC_MISC_0,
        100000,
        1400000
    },

    {   /* PLLM base output */
        NvRmClockSource_PllM0,
        NvRmClockSource_PllRef,
        NvRmPllType_LP,
        CLK_RST_CONTROLLER_PLLM_BASE_0,
        CLK_RST_CONTROLLER_PLLM_MISC_0,
        100000,
        1200000
    },

    {   /* PLLX base output */
        NvRmClockSource_PllX0,
        NvRmClockSource_PllRef,
        NvRmPllType_LP,
        CLK_RST_CONTROLLER_PLLX_BASE_0,
        CLK_RST_CONTROLLER_PLLX_MISC_0,
        100000,
        1400000
    },

    {   /* PLLP base output */
        NvRmClockSource_PllP0,
        NvRmClockSource_PllRef,
        NvRmPllType_LP,
        CLK_RST_CONTROLLER_PLLP_BASE_0,
        CLK_RST_CONTROLLER_PLLP_MISC_0,
        100000,
        1400000
    },

    {   /* PLLD base output */
        NvRmClockSource_PllD0,
        NvRmClockSource_PllRef,
        NvRmPllType_MIPI,
        CLK_RST_CONTROLLER_PLLD_BASE_0,
        CLK_RST_CONTROLLER_PLLD_MISC_0,
        100000,
        1000000
    },

    {   /* PLLU base output */
        NvRmClockSource_PllU0,
        NvRmClockSource_PllRef,
        NvRmPllType_UHS,
        CLK_RST_CONTROLLER_PLLU_BASE_0,
        CLK_RST_CONTROLLER_PLLU_MISC_0,
        480000,
        960000
    }
};

static const NvU32 s_T30PllClockTableSize = NV_ARRAY_SIZE(s_T30PllClockTable);

/*****************************************************************************/

static const NvRmDividerClockInfo s_T30DividerClockTable[] =
{
    {   /* PLLA0 - PLLA secondary output */
        NvRmClockSource_PllA0,
        NvRmClockSource_PllA1,
        NvRmClockDivider_Fractional_2,

        CLK_RST_CONTROLLER_PLLA_OUT_0,
        CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_RATIO_DEFAULT_MASK,
        CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_RATIO_SHIFT,

        CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_CLKEN_FIELD |
         CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_RSTN_FIELD,

        ((CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_CLKEN_ENABLE <<
          CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_RSTN_SHIFT)),

        ((CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_CLKEN_DISABLE <<
          CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLA_OUT_0_PLLA_OUT0_RSTN_SHIFT)),

        NVRM_VARIABLE_DIVIDER
    },

    {   /* PLLC1 - PLLC secondary output */
        NvRmClockSource_PllC1,
        NvRmClockSource_PllC0,
        NvRmClockDivider_Fractional_2,

        CLK_RST_CONTROLLER_PLLC_OUT_0,
        CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_RATIO_DEFAULT_MASK,
        CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_RATIO_SHIFT,

        CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_CLKEN_FIELD |
         CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_RSTN_FIELD,

        ((CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_CLKEN_ENABLE <<
          CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_RSTN_SHIFT)),

        ((CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_CLKEN_DISABLE <<
          CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLC_OUT_0_PLLC_OUT1_RSTN_SHIFT)),

        NVRM_VARIABLE_DIVIDER
    },

    {   /* PLLM1 - PLLM secondary ouput */
        NvRmClockSource_PllM1,
        NvRmClockSource_PllM0,
        NvRmClockDivider_Fractional_2,

        CLK_RST_CONTROLLER_PLLM_OUT_0,
        CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_RATIO_DEFAULT_MASK,
        CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_RATIO_SHIFT,

        CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_CLKEN_FIELD |
         CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_RSTN_FIELD,

        ((CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_CLKEN_ENABLE <<
          CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_RSTN_SHIFT)),

        ((CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_CLKEN_DISABLE <<
          CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLM_OUT_0_PLLM_OUT1_RSTN_SHIFT)),

        NVRM_VARIABLE_DIVIDER
    },

    {   /* PLLP1 - PLLP secondary output (overridden) */
        NvRmClockSource_PllP1,
        NvRmClockSource_PllP0,
        NvRmClockDivider_Fractional_2,

        CLK_RST_CONTROLLER_PLLP_OUTA_0,
        CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_RATIO_DEFAULT_MASK,
        CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_RATIO_SHIFT,

        CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_OVRRIDE_FIELD |
         CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_CLKEN_FIELD |
         CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_RSTN_FIELD,

        ((CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_OVRRIDE_ENABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_OVRRIDE_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_CLKEN_ENABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_RSTN_SHIFT)),

        ((CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_OVRRIDE_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_OVRRIDE_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_CLKEN_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT1_RSTN_SHIFT)),

        NVRM_VARIABLE_DIVIDER
    },

    {   /* PLLP2 - PLLP secondary output (overridden) */
        NvRmClockSource_PllP2,
        NvRmClockSource_PllP0,
        NvRmClockDivider_Fractional_2,

        CLK_RST_CONTROLLER_PLLP_OUTA_0,
        CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_RATIO_DEFAULT_MASK,
        CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_RATIO_SHIFT,

        CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_OVRRIDE_FIELD |
         CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_CLKEN_FIELD |
         CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_RSTN_FIELD,

        ((CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_OVRRIDE_ENABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_OVRRIDE_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_CLKEN_ENABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_RSTN_SHIFT)),

        ((CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_OVRRIDE_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_OVRRIDE_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_CLKEN_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTA_0_PLLP_OUT2_RSTN_SHIFT)),

        NVRM_VARIABLE_DIVIDER
    },

    {   /* PLLP3 - PLLP secondary output (overridden)  */
        NvRmClockSource_PllP3,
        NvRmClockSource_PllP0,
        NvRmClockDivider_Fractional_2,

        CLK_RST_CONTROLLER_PLLP_OUTB_0,
        CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_RATIO_DEFAULT_MASK,
        CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_RATIO_SHIFT,

        CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_OVRRIDE_FIELD |
         CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_CLKEN_FIELD |
         CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_RSTN_FIELD,

        ((CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_OVRRIDE_ENABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_OVRRIDE_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_CLKEN_ENABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_RSTN_SHIFT)),

        ((CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_OVRRIDE_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_OVRRIDE_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_CLKEN_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT3_RSTN_SHIFT)),

        NVRM_VARIABLE_DIVIDER
    },

    {   /* PLLP4 - PLLP secondary output (overridden) */
        NvRmClockSource_PllP4,
        NvRmClockSource_PllP0,
        NvRmClockDivider_Fractional_2,

        CLK_RST_CONTROLLER_PLLP_OUTB_0,
        CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_RATIO_DEFAULT_MASK,
        CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_RATIO_SHIFT,

        CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_OVRRIDE_FIELD |
         CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_CLKEN_FIELD |
         CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_RSTN_FIELD,

        ((CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_OVRRIDE_ENABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_OVRRIDE_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_CLKEN_ENABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_RSTN_SHIFT)),

        ((CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_OVRRIDE_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_OVRRIDE_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_CLKEN_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_CLKEN_SHIFT) |
         (CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_RSTN_RESET_DISABLE <<
          CLK_RST_CONTROLLER_PLLP_OUTB_0_PLLP_OUT4_RSTN_SHIFT)),

        NVRM_VARIABLE_DIVIDER
    },

    {   /* AHB bus clock divider */
        NvRmClockSource_Ahb,
        NvRmClockSource_SystemBus,
        NvRmClockDivider_Integer_1,

        CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0,
        CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0_AHB_RATE_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0_AHB_RATE_SHIFT,

        CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0_HCLK_DIS_FIELD,
        (0x0 << CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0_HCLK_DIS_SHIFT),
        (0x1 << CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0_HCLK_DIS_SHIFT),
         NVRM_VARIABLE_DIVIDER
    },

    {   /* APB bus clock divider */
        NvRmClockSource_Apb,
        NvRmClockSource_Ahb,
        NvRmClockDivider_Integer_1,

        CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0,
        CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0_APB_RATE_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0_APB_RATE_SHIFT,

        CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0_PCLK_DIS_FIELD,
        (0x0 << CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0_PCLK_DIS_SHIFT),
        (0x1 << CLK_RST_CONTROLLER_CLK_SYSTEM_RATE_0_PCLK_DIS_SHIFT),
        NVRM_VARIABLE_DIVIDER
    },

    // TODO: PLL ref divider, CLK_M input divider
};

static const NvU32 s_T30DividerClockTableSize = NV_ARRAY_SIZE(s_T30DividerClockTable);

/*****************************************************************************/

static const NvRmCoreClockInfo s_T30CoreClockTable[] =
{
    {
        NvRmClockSource_CpuBus,
        {
            NvRmClockSource_ClkM,
            NvRmClockSource_PllC0,
            NvRmClockSource_ClkS,
            NvRmClockSource_PllM0,
            NvRmClockSource_PllP0,
            NvRmClockSource_PllP4,
            NvRmClockSource_PllP3,
            NvRmClockSource_ClkD,
            NvRmClockSource_PllX0
        },

        CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0,
        CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0_CPU_STATE_DEFAULT_MASK,
        CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0_CPU_STATE_SHIFT,
        {
            0,
            CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0_CWAKEUP_IDLE_SOURCE_DEFAULT_MASK,
            CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0_CWAKEUP_RUN_SOURCE_DEFAULT_MASK,
            CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0_CWAKEUP_IRQ_SOURCE_DEFAULT_MASK,
            CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0_CWAKEUP_FIQ_SOURCE_DEFAULT_MASK

        },
        {
            0,
            CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0_CWAKEUP_IDLE_SOURCE_SHIFT,
            CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0_CWAKEUP_RUN_SOURCE_SHIFT,
            CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0_CWAKEUP_IRQ_SOURCE_SHIFT,
            CLK_RST_CONTROLLER_CCLK_BURST_POLICY_0_CWAKEUP_FIQ_SOURCE_SHIFT
        },

        CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER_0,
        CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER_0_SUPER_CDIV_ENB_DEFAULT_MASK,
        CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER_0_SUPER_CDIV_ENB_SHIFT,
        CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER_0_SUPER_CDIV_DIVIDEND_DEFAULT_MASK,
        CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER_0_SUPER_CDIV_DIVIDEND_SHIFT,
        NV_FIELD_SIZE(CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER_0_SUPER_CDIV_DIVIDEND_RANGE),
        CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER_0_SUPER_CDIV_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER_0_SUPER_CDIV_DIVISOR_SHIFT,
        NV_FIELD_SIZE(CLK_RST_CONTROLLER_SUPER_CCLK_DIVIDER_0_SUPER_CDIV_DIVISOR_RANGE)
    },
    {
        NvRmClockSource_SystemBus,
        {
            NvRmClockSource_ClkM,
            NvRmClockSource_PllC1,
            NvRmClockSource_PllP4,
            NvRmClockSource_PllP3,
            NvRmClockSource_PllP2,
            NvRmClockSource_ClkD,
            NvRmClockSource_ClkS,
            NvRmClockSource_PllM1,
        },

        CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0,
        CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0_SYS_STATE_DEFAULT_MASK,
        CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0_SYS_STATE_SHIFT,
        {
            0,
            CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0_SWAKEUP_IDLE_SOURCE_DEFAULT_MASK,
            CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0_SWAKEUP_RUN_SOURCE_DEFAULT_MASK,
            CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0_SWAKEUP_IRQ_SOURCE_DEFAULT_MASK,
            CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0_SWAKEUP_FIQ_SOURCE_DEFAULT_MASK

        },
        {
            0,
            CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0_SWAKEUP_IDLE_SOURCE_SHIFT,
            CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0_SWAKEUP_RUN_SOURCE_SHIFT,
            CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0_SWAKEUP_IRQ_SOURCE_SHIFT,
            CLK_RST_CONTROLLER_SCLK_BURST_POLICY_0_SWAKEUP_FIQ_SOURCE_SHIFT
        },

        CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER_0,
        CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER_0_SUPER_SDIV_ENB_DEFAULT_MASK,
        CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER_0_SUPER_SDIV_ENB_SHIFT,
        CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER_0_SUPER_SDIV_DIVIDEND_DEFAULT_MASK,
        CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER_0_SUPER_SDIV_DIVIDEND_SHIFT,
        NV_FIELD_SIZE(CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER_0_SUPER_SDIV_DIVIDEND_RANGE),
        CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER_0_SUPER_SDIV_DIVISOR_DEFAULT_MASK,
        CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER_0_SUPER_SDIV_DIVISOR_SHIFT,
        NV_FIELD_SIZE(CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER_0_SUPER_SDIV_DIVISOR_RANGE)
    }
};

static const NvU32 s_T30CoreClockTableSize = NV_ARRAY_SIZE(s_T30CoreClockTable);

/*****************************************************************************/

static const NvRmSelectorClockInfo s_T30SelectorClockTable[] =
{
    {0},
};

static const NvU32 s_T30SelectorClockTableSize = NV_ARRAY_SIZE(s_T30SelectorClockTable);

/*****************************************************************************/
/*****************************************************************************/

static NvRmClockSourceInfo s_T30ClockSourceTable[NvRmClockSource_Num] = {{0}};

NvRmClockSourceInfo* NvRmClockSourceTableInit(void)
{
    NvRmClockSourceInfoPtr Src;

#define PARSE_SOURCE_TABLE(type) \
do\
{\
    Src.p##type = (NvRm##type##ClockInfo*)s_T30##type##ClockTable;\
    NvRmPrivParseClockSources( \
        s_T30ClockSourceTable, NvRmClockSource_Num, \
        Src, s_T30##type##ClockTableSize, NvRmClockSourceType_##type); \
} while(0)

        NvOsMemset(s_T30ClockSourceTable, 0, sizeof(s_T30ClockSourceTable));

        PARSE_SOURCE_TABLE(Fixed);
        PARSE_SOURCE_TABLE(Pll);
        PARSE_SOURCE_TABLE(Divider);
        PARSE_SOURCE_TABLE(Core);
        PARSE_SOURCE_TABLE(Selector);

#undef PARSE_SOURCE_TABLE

        return &s_T30ClockSourceTable[0];
}

/*****************************************************************************/

static NvBool s_T30PllM0Clocks[NV_ARRAY_SIZE(g_ModuleClockTable)] = {0};
static NvBool s_T30PllC0Clocks[NV_ARRAY_SIZE(g_ModuleClockTable)] = {0};
static NvBool s_T30PllP0Clocks[NV_ARRAY_SIZE(g_ModuleClockTable)] = {0};
static NvBool s_T30PllA0Clocks[NV_ARRAY_SIZE(g_ModuleClockTable)] = {0};
static NvBool s_T30PllD0Clocks[NV_ARRAY_SIZE(g_ModuleClockTable)] = {0};
static NvBool s_T30PllX0Clocks[NV_ARRAY_SIZE(g_ModuleClockTable)] = {0};

static NvRmPllReference s_T30PllReferencesTable[] =
{
    { NvRmClockSource_PllM0, NvRmDfsStatusFlags_StopPllM0, 0, s_T30PllM0Clocks, 0 },
    { NvRmClockSource_PllC0, NvRmDfsStatusFlags_StopPllC0, 0, s_T30PllC0Clocks, 0 },
    { NvRmClockSource_PllP0, NvRmDfsStatusFlags_StopPllP0, 0, s_T30PllP0Clocks, 0 },
    { NvRmClockSource_PllA0, NvRmDfsStatusFlags_StopPllA0, 0, s_T30PllA0Clocks, 0 },
    { NvRmClockSource_PllD0, NvRmDfsStatusFlags_StopPllD0, 0, s_T30PllD0Clocks, 0 },
    { NvRmClockSource_PllX0, NvRmDfsStatusFlags_StopPllX0, 0, s_T30PllX0Clocks, 0 },
};
static const NvU32 s_T30PllReferencesTableSize =
    NV_ARRAY_SIZE(s_T30PllReferencesTable);

void
NvRmPllReferenceTableInit(
    NvRmPllReference** pPllReferencesTable,
    NvU32* pPllReferencesTableSize)
{
    NvU32 i;
    for (i = 0; i < s_T30PllReferencesTableSize; i++)
    {
        NvOsMemset(s_T30PllReferencesTable[i].AttachedModules, 0,
                   sizeof(NvBool) * g_ModuleClockTableSize);
        s_T30PllReferencesTable[i].ReferenceCnt = 0;
        s_T30PllReferencesTable[i].ExternalClockRefCnt = 0;
    }
    *pPllReferencesTable = s_T30PllReferencesTable;
    *pPllReferencesTableSize = s_T30PllReferencesTableSize;
}

/*****************************************************************************/

// Power Gating Ids for each Power Group specified in re-location table header
static const NvU32 s_T30PowerGroupIds[] = { NV_POWERGROUP_ENUM_TABLE };

void
NvRmPowerGroupTableInit(
    const NvU32** pPowerGroupIdsTable,
    NvU32* pPowerGroupIdsTableSize)
{
    *pPowerGroupIdsTable = s_T30PowerGroupIds;
    *pPowerGroupIdsTableSize = NV_ARRAY_SIZE(s_T30PowerGroupIds);
}

/*****************************************************************************/

