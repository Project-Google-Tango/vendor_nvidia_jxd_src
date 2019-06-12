/**
 * Copyright (c) 2010 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TEGRA_ITU656_H
#define INCLUDED_TEGRA_ITU656_H

enum {
    TEGRA_ITU656_MODULE_VI = 0,
};

enum {
    TEGRA_ITU656_VI_CLK
};

#define TEGRA_ITU656_IOCTL_ENABLE       _IOWR('i', 1, uint)
#define TEGRA_ITU656_IOCTL_DISABLE      _IOWR('i', 2, uint)
#define TEGRA_ITU656_IOCTL_CLK_SET      _IOWR('i', 3, uint)
#define TEGRA_ITU656_IOCTL_RESET        _IOWR('i', 4, uint)

#endif // INCLUDED_TEGRA_ITU656_H
