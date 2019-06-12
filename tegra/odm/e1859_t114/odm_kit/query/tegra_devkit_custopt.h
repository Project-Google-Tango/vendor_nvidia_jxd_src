/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Definition of bitfields inside the BCT customer option</b>
 *
 * @b Description: Defines the board-specific bitfields of the
 *                 BCT customer option parameter, for NVIDIA
 *                 Tegra development platforms.
 *
 *                 This file pertains to Dalmore.
 */

#ifndef NVIDIA_TEGRA_DEVKIT_CUSTOPT_H
#define NVIDIA_TEGRA_DEVKIT_CUSTOPT_H

#if defined(__cplusplus)
extern "C"
{
#endif

/// Total RAM
#define TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_RANGE    31:28
#define TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_DEFAULT  0x0UL
#define TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_1        0x1UL // 256 MB
#define TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_2        0x2UL // 512 MB
#define TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_3        0x3UL
#define TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_4        0x4UL // 1 GB
#define TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_5        0x5UL
#define TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_6        0x6UL
#define TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_7        0x7UL
#define TEGRA_DEVKIT_BCT_SYSTEM_0_MEMORY_8        0x8UL // 2 GB

#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_OPTION_RANGE    17:15
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_OPTION_DEFAULT  0
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_OPTION_UARTA    0
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_OPTION_UARTB    1
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_OPTION_UARTC    2
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_OPTION_UARTD    3
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_OPTION_UARTE    4

#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_RANGE           19:18
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_NONE            0
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_DCC             1
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_UART            2
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_CONSOLE_AUTOMATION      3

#define TEGRA_DEVKIT_BCT_CUSTOPT_0_MODEM_RANGE             7:3
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_MODEM_NONE              0
#define TEGRA_DEVKIT_BCT_CUSTOPT_0_MODEM_NEMO              1

#if defined(__cplusplus)
}
#endif

#endif
