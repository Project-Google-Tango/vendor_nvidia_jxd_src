/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PINMUX_T30_H
#define PINMUX_T30_H

#include "arapb_misc.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* These entries are not present in arapb_misc.h
 * This is a hack to force them to default values
 * to avoid compilation errors.
 */
#define PINMUX_AUX_DDC_SCL_0_OD_DEFAULT                 NV_PINMUX_E_OD_DEFAULT
#define PINMUX_AUX_DDC_SDA_0_OD_DEFAULT                 NV_PINMUX_E_OD_DEFAULT
#define PINMUX_AUX_CLK1_REQ_0_PM_RSVD                   NV_PINMUX_PM_RSVD
#define PINMUX_AUX_CAM_MCLK_0_IO_RESET_DEFAULT          NV_PINMUX_IO_RESET_VALUE_DEFAULT
#define PINMUX_AUX_GPIO_PBB0_0_IO_RESET_DEFAULT         NV_PINMUX_IO_RESET_VALUE_DEFAULT
#define PINMUX_AUX_SPDIF_IN_0_OD_ENABLE                 NV_PINMUX_E_OD_ENABLE
#define PINMUX_AUX_USB_VBUS_EN1_0_PM_RSVD               NV_PINMUX_PM_RSVD

#define PINMUX_AUX_CAM1_MCLK_0_IO_RESET_DISABLE         NV_PINMUX_IO_RESET_VALUE_DEFAULT
#define PINMUX_AUX_CAM2_MCLK_0_IO_RESET_DISABLE         NV_PINMUX_IO_RESET_VALUE_DEFAULT
#define PINMUX_AUX_DDC_SCL_0_OD_DISABLE                 NV_PINMUX_E_OD_DISABLE
#define PINMUX_AUX_DDC_SDA_0_OD_DISABLE                 NV_PINMUX_E_OD_DISABLE

#define DEFAULT_DRIVE(_name)                    \
{                                               \
    .pingroup = TEGRA_DRIVE_PINGROUP_##_name,   \
    .hsm = TEGRA_HSM_DISABLE,                   \
    .schmitt = TEGRA_SCHMITT_ENABLE,            \
    .drive = TEGRA_DRIVE_DIV_1,                 \
    .pull_down = TEGRA_PULL_31,                 \
    .pull_up = TEGRA_PULL_31,                   \
    .slew_rising = TEGRA_SLEW_SLOWEST,          \
    .slew_falling = TEGRA_SLEW_SLOWEST,         \
}
/* Setting the drive strength of pins
 * hsm: Enable High speed mode (ENABLE/DISABLE)
 * Schimit: Enable/disable schimit (ENABLE/DISABLE)
 * drive: low power mode (DIV_1, DIV_2, DIV_4, DIV_8)
 * pulldn_drive - drive down (falling edge) - Driver Output Pull-Down drive
 *                strength code. Value from 0 to 31.
 * pullup_drive - drive up (rising edge)  - Driver Output Pull-Up drive
 *                strength code. Value from 0 to 31.
 * pulldn_slew -  Driver Output Pull-Up slew control code  - 2bit code
 *                code 11 is least slewing of signal. code 00 is highest
 *                slewing of the signal.
 *                Value - FASTEST, FAST, SLOW, SLOWEST
 * pullup_slew -  Driver Output Pull-Down slew control code -
 *                code 11 is least slewing of signal. code 00 is highest
 *                slewing of the signal.
 *                Value - FASTEST, FAST, SLOW, SLOWEST
 */
#define SET_DRIVE(_name, _hsm, _schmitt, _drive, _pulldn_drive, _pullup_drive, _pulldn_slew, _pullup_slew) \
{                                                               \
        TEGRA_DRIVE_PINGROUP_##_name,                           \
        (((TEGRA_HSM_##_hsm) & 1UL) << 2) |                     \
         (((TEGRA_SCHMITT_##_schmitt) & 1UL)  << 3) |           \
         (((TEGRA_DRIVE_##_drive) & 3UL) << 4) |                \
         (((TEGRA_PULL_##_pulldn_drive) & 31UL) << 12) |        \
         (((TEGRA_PULL_##_pullup_drive) & 31UL) << 20) |        \
         (((TEGRA_SLEW_##_pulldn_slew) & 3UL) << 28) |          \
         (((TEGRA_SLEW_##_pullup_slew) & 3UL) << 30)            \
}

#define SET_DRIVE_GMA(_name, _hsm, _schmitt, _drive_type, _pulldn_drive, _pullup_drive, _pulldn_slew, _pullup_slew) \
{                                                               \
        TEGRA_DRIVE_PINGROUP_##_name,                           \
        (((TEGRA_HSM_##_hsm) & 1UL) << 2) |                     \
         (((TEGRA_SCHMITT_##_schmitt) & 1UL)  << 3) |           \
         (((TEGRA_DRIVE_##_drive_type) & 3UL) << 6) |           \
         (((TEGRA_PULL_##_pulldn_drive) & 31UL) << 14) |        \
         (((TEGRA_PULL_##_pullup_drive) & 31UL) << 20) |        \
         (((TEGRA_SLEW_##_pulldn_slew) & 3UL) << 28) |          \
         (((TEGRA_SLEW_##_pullup_slew) & 3UL) << 30)            \
}

#define VI_PINMUX(_Pingroup, _Pinmux, _Pupd, _Tristate, _E_Input, _Lock, _IO_Reset) \
{                                                                     \
        PINMUX_AUX_##_Pingroup##_0,                                   \
        0,                                                            \
        PINMUX_AUX_##_Pingroup##_0_IO_RESET_##_IO_Reset,              \
        PINMUX_AUX_##_Pingroup##_0_LOCK_DEFAULT,                      \
        NV_PINMUX_E_OD_DEFAULT,                                       \
        PINMUX_AUX_##_Pingroup##_0_E_INPUT_##_E_Input,                \
        PINMUX_AUX_##_Pingroup##_0_TRISTATE_##_Tristate,              \
        PINMUX_AUX_##_Pingroup##_0_PUPD_##_Pupd,                      \
        PINMUX_AUX_##_Pingroup##_0_PM_##_Pinmux                       \
}

/* For LV(Low voltage) pad groups which has IO_RESET bit */
#define LVPAD_PINMUX(_Pingroup, _Pinmux, _Pupd, _Tristate, _E_Input, _Lock, _IO_Reset) \
{                                                                       \
        PINMUX_AUX_##_Pingroup##_0,                                     \
        0,                                                              \
        PINMUX_AUX_##_Pingroup##_0_IO_RESET_##_IO_Reset,                \
        PINMUX_AUX_##_Pingroup##_0_LOCK_##_Lock,                        \
        NV_PINMUX_E_OD_DEFAULT,                                         \
        PINMUX_AUX_##_Pingroup##_0_E_INPUT_##_E_Input,                  \
        PINMUX_AUX_##_Pingroup##_0_TRISTATE_##_Tristate,                \
        PINMUX_AUX_##_Pingroup##_0_PUPD_##_Pupd,                        \
        PINMUX_AUX_##_Pingroup##_0_PM_##_Pinmux                         \
}

#define DEFAULT_PINMUX(_Pingroup, _Pinmux, _Pupd, _Tristate, _E_Input) \
{                                                                      \
        PINMUX_AUX_##_Pingroup##_0,                                    \
        0,                                                             \
        NV_PINMUX_IO_RESET_VALUE_DEFAULT,                              \
        PINMUX_AUX_##_Pingroup##_0_LOCK_DEFAULT,                       \
        NV_PINMUX_E_OD_DEFAULT,                                        \
        PINMUX_AUX_##_Pingroup##_0_E_INPUT_##_E_Input,                 \
        PINMUX_AUX_##_Pingroup##_0_TRISTATE_##_Tristate,               \
        PINMUX_AUX_##_Pingroup##_0_PUPD_##_Pupd,                       \
        PINMUX_AUX_##_Pingroup##_0_PM_##_Pinmux                        \
}

#define I2C_PINMUX(_Pingroup, _Pinmux, _Pupd, _Tristate, _E_Input, _Lock, _E_Od) \
{                                                                      \
        PINMUX_AUX_##_Pingroup##_0,                                    \
        0,                                                             \
        NV_PINMUX_IO_RESET_VALUE_DEFAULT,                              \
        PINMUX_AUX_##_Pingroup##_0_LOCK_##_Lock,                       \
        PINMUX_AUX_##_Pingroup##_0_OD_##_E_Od,                         \
        PINMUX_AUX_##_Pingroup##_0_E_INPUT_##_E_Input,                 \
        PINMUX_AUX_##_Pingroup##_0_TRISTATE_##_Tristate,               \
        PINMUX_AUX_##_Pingroup##_0_PUPD_##_Pupd,                       \
        PINMUX_AUX_##_Pingroup##_0_PM_##_Pinmux                        \
}

#define DDC_PINMUX(_Pingroup, _Pinmux, _Pupd, _Tristate, _E_Input, _Lock, _Rcv_sel) \
{                                                                      \
        PINMUX_AUX_##_Pingroup##_0,                                    \
        0,                                                             \
        NV_PINMUX_IO_RESET_VALUE_DEFAULT,                              \
        PINMUX_AUX_##_Pingroup##_0_LOCK_##_Lock,                       \
        PINMUX_AUX_##_Pingroup##_0_RCV_SEL_##_Rcv_sel,                 \
        PINMUX_AUX_##_Pingroup##_0_E_INPUT_##_E_Input,                 \
        PINMUX_AUX_##_Pingroup##_0_TRISTATE_##_Tristate,               \
        PINMUX_AUX_##_Pingroup##_0_PUPD_##_Pupd,                       \
        PINMUX_AUX_##_Pingroup##_0_PM_##_Pinmux                        \
}

#define CEC_PINMUX(_Pingroup, _Pinmux, _Pupd, _Tristate, _E_Input, _Lock, _E_Od) \
{                                                                      \
        PINMUX_AUX_##_Pingroup##_0,                                    \
        0,                                                             \
        NV_PINMUX_IO_RESET_VALUE_DEFAULT,                              \
        PINMUX_AUX_##_Pingroup##_0_LOCK_##_Lock,                       \
        PINMUX_AUX_##_Pingroup##_0_OD_##_E_Od,                         \
        PINMUX_AUX_##_Pingroup##_0_E_INPUT_##_E_Input,                 \
        PINMUX_AUX_##_Pingroup##_0_TRISTATE_##_Tristate,               \
        PINMUX_AUX_##_Pingroup##_0_PUPD_##_Pupd,                       \
        PINMUX_AUX_##_Pingroup##_0_PM_##_Pinmux                        \
}

#define USB_PINMUX CEC_PINMUX

enum NvPinDrivePingroup {
    TEGRA_DRIVE_PINGROUP_AO1   = 0x868,
    TEGRA_DRIVE_PINGROUP_AO2   = 0x86c,
    TEGRA_DRIVE_PINGROUP_AT1   = 0x870,
    TEGRA_DRIVE_PINGROUP_AT2   = 0x874,
    TEGRA_DRIVE_PINGROUP_AT3   = 0x878,
    TEGRA_DRIVE_PINGROUP_AT4   = 0x87c,
    TEGRA_DRIVE_PINGROUP_AT5   = 0x880,
    TEGRA_DRIVE_PINGROUP_CDEV1 = 0x884,
    TEGRA_DRIVE_PINGROUP_CDEV2 = 0x888,
    TEGRA_DRIVE_PINGROUP_CSUS  = 0x88c,
    TEGRA_DRIVE_PINGROUP_DAP1  = 0x890,
    TEGRA_DRIVE_PINGROUP_DAP2  = 0x894,
    TEGRA_DRIVE_PINGROUP_DAP3  = 0x898,
    TEGRA_DRIVE_PINGROUP_DAP4  = 0x89c,
    TEGRA_DRIVE_PINGROUP_DBG   = 0x8a0,
    TEGRA_DRIVE_PINGROUP_LCD1  = 0x8a4,
    TEGRA_DRIVE_PINGROUP_LCD2  = 0x8a8,
    TEGRA_DRIVE_PINGROUP_SDIO2 = 0x8ac,
    TEGRA_DRIVE_PINGROUP_SDIO3 = 0x8b0,
    TEGRA_DRIVE_PINGROUP_SPI   = 0x8b4,
    TEGRA_DRIVE_PINGROUP_UAA   = 0x8b8,
    TEGRA_DRIVE_PINGROUP_UAB   = 0x8bc,
    TEGRA_DRIVE_PINGROUP_UART2 = 0x8c0,
    TEGRA_DRIVE_PINGROUP_UART3 = 0x8c4,
    TEGRA_DRIVE_PINGROUP_VI1   = 0x8c8,
    TEGRA_DRIVE_PINGROUP_SDIO1 = 0x8ec,
    TEGRA_DRIVE_PINGROUP_CRT   = 0x8f8,
    TEGRA_DRIVE_PINGROUP_DDC   = 0x8fc,
    TEGRA_DRIVE_PINGROUP_GMA   = 0x900,
    TEGRA_DRIVE_PINGROUP_GMB   = 0x904,
    TEGRA_DRIVE_PINGROUP_GMC   = 0x908,
    TEGRA_DRIVE_PINGROUP_GMD   = 0x90c,
    TEGRA_DRIVE_PINGROUP_GME   = 0x910,
    TEGRA_DRIVE_PINGROUP_GMF   = 0x914,
    TEGRA_DRIVE_PINGROUP_GMG   = 0x918,
    TEGRA_DRIVE_PINGROUP_GMH   = 0x91c,
    TEGRA_DRIVE_PINGROUP_OWR   = 0x920,
    TEGRA_DRIVE_PINGROUP_UAD   = 0x924,
    TEGRA_DRIVE_PINGROUP_GPV   = 0x928,
    TEGRA_DRIVE_PINGROUP_DEV3  = 0x92c,
    TEGRA_DRIVE_PINGROUP_CEC   = 0x938,
    TEGRA_DRIVE_PINGROUP_DUMMY = -1,
};

/*
 * Data structures required for setting up pinmux.
 */
typedef struct NvPingroupConfigRec
{
    //NvOdmIoModule Module;   // Module used.
    NvU32 RegOffset;        // Offset of the pin register.
    NvU8  Rcv_sel;          // Bit 9: I/O Reset.
    NvU8  IO_Reset;         // Bit 8: I/O Reset.
    NvU8  Lock;             // Bit 7: Lock.
    NvU8  E_Od;             // Bit 6: Mode of output driver.
    NvU8  E_Input;          // Bit 5: Enable/Disable input receiver.
    NvU8  Tristate;         // Bit 4: Enable/Disable output driver.
    NvU8  Pupd;             // Bits 3:2 : State of pull-up and pull-down registers.
    NvU8  Pinmux;           // Bits 1:0 : Specify the function for this pin.
}NvPingroupConfig;

NvError NvPinmuxConfigTable(NvPingroupConfig *config, NvU32 len);

#if defined(__cplusplus)
}
#endif

#endif
