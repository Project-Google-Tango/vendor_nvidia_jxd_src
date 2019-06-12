/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PINMUX_H
#define PINMUX_H

#include "nvos.h"
#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define NV_CHECK_PINMUX_ERROR(expr) \
    do \
        { \
            if (expr) \
                goto fail; \
        }while(0)

#define NV_ADDRESS_MAP_APB_MISC_BASE        0x70000000
#define NV_WRITE32(a,d)     *((volatile NvU32 *)(a)) = (d)
#define NV_READ32(a)        *((const volatile NvU32 *)(a))

/*
 * Offsets corresponding to various fields of pinmux register.
 */

#define NV_PINMUX_RCV_SEL_OFFSET            9
#define NV_PINMUX_IO_RESET_OFFSET           8
#define NV_PINMUX_LOCK_OFFSET               7
#define NV_PINMUX_E_OD_OFFSET               6
#define NV_PINMUX_E_INPUT_OFFSET            5
#define NV_PINMUX_TRISTATE_OFFSET           4
#define NV_PINMUX_PUPD_OFFSET               2
#define NV_PINMUX_PINMUX_OFFSET             0

/*
 * I/O Reset
 */

#define NV_PINMUX_IO_RESET_VALUE_DEFAULT    0
#define NV_PINMUX_IO_RESET_VALUE_NORMAL     0
#define NV_PINMUX_IO_RESET_VALUE_IORESET    1

/*
 * RCV_SEL
 */

#define NV_PINMUX_RCV_SEL_VALUE_DEFAULT     0
#define NV_PINMUX_RCV_SEL_VALUE_NORMAL      0
#define NV_PINMUX_RCV_SEL_VALUE_HIGH        1

/*
 * E_Od
 */
#define NV_PINMUX_E_OD_DEFAULT              0
#define NV_PINMUX_E_OD_DISABLE              0
#define NV_PINMUX_E_OD_ENABLE               1

/*
 * PM
 */
#define NV_PINMUX_PM_RSVD                   0x80U
#define NV_PINMUX_PM_RSVD0                  NV_PINMUX_PM_RSVD
#define NV_PINMUX_PM_RSVD1                  0X81U
#define NV_PINMUX_PM_RSVD2                  0X82U
#define NV_PINMUX_PM_RSVD3                  0X83U
#define NV_PINMUX_PM_RSVD4                  0X84U

enum NvPinHsm {
    TEGRA_HSM_DISABLE = 0,
    TEGRA_HSM_ENABLE,
};

enum NvPinSchmitt {
    TEGRA_SCHMITT_DISABLE = 0,
    TEGRA_SCHMITT_ENABLE,
};

enum NvPinSlew {
    TEGRA_SLEW_FASTEST = 0,
    TEGRA_SLEW_FAST,
    TEGRA_SLEW_SLOW,
    TEGRA_SLEW_SLOWEST,
    TEGRA_MAX_SLEW,
    TEGRA_SLEW_DUMMY = -1,
};

enum NvPinPullStrength {
    TEGRA_PULL_0 = 0,
    TEGRA_PULL_1,
    TEGRA_PULL_2,
    TEGRA_PULL_3,
    TEGRA_PULL_4,
    TEGRA_PULL_5,
    TEGRA_PULL_6,
    TEGRA_PULL_7,
    TEGRA_PULL_8,
    TEGRA_PULL_9,
    TEGRA_PULL_10,
    TEGRA_PULL_11,
    TEGRA_PULL_12,
    TEGRA_PULL_13,
    TEGRA_PULL_14,
    TEGRA_PULL_15,
    TEGRA_PULL_16,
    TEGRA_PULL_17,
    TEGRA_PULL_18,
    TEGRA_PULL_19,
    TEGRA_PULL_20,
    TEGRA_PULL_21,
    TEGRA_PULL_22,
    TEGRA_PULL_23,
    TEGRA_PULL_24,
    TEGRA_PULL_25,
    TEGRA_PULL_26,
    TEGRA_PULL_27,
    TEGRA_PULL_28,
    TEGRA_PULL_29,
    TEGRA_PULL_30,
    TEGRA_PULL_31,
    TEGRA_PULL_32,
    TEGRA_PULL_33,
    TEGRA_PULL_34,
    TEGRA_PULL_35,
    TEGRA_PULL_36,
    TEGRA_PULL_37,
    TEGRA_PULL_38,
    TEGRA_PULL_39,
    TEGRA_PULL_40,
    TEGRA_PULL_41,
    TEGRA_PULL_42,
    TEGRA_PULL_43,
    TEGRA_PULL_44,
    TEGRA_PULL_45,
    TEGRA_PULL_46,
    TEGRA_MAX_PULL,
    TEGRA_PULL_DUMMY = -1,
};

enum NvPinDrive {
    TEGRA_DRIVE_DIV_8 = 0,
    TEGRA_DRIVE_DIV_4,
    TEGRA_DRIVE_DIV_2,
    TEGRA_DRIVE_DIV_1,
    TEGRA_MAX_DRIVE,
    TEGRA_DRIVE_DUMMY = -1,
};

enum NvPinDriveType {
    TEGRA_DRIVE_1 = 1,
    TEGRA_DRIVE_2,
    TEGRA_DRIVE_3,
    TEGRA_DRIVE_4,
    TEGRA_MAX_DRIVETYPE,
    TEGRA_DRIVETYPE_DUMMY = -1,
};

typedef struct NvPinDrivePingroupConfigRec
{
    NvU32 PingroupDriveReg;
    NvU32 Value;
}NvPinDrivePingroupConfig;

NvError NvPinmuxDriveConfigTable(NvPinDrivePingroupConfig *config, NvU32 len);

#if defined(__cplusplus)
}
#endif

#endif
