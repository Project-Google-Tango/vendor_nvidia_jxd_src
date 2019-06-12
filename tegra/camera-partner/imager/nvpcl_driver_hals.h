/**
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_DRIVER_HALS_H
#define PCL_DRIVER_HALS_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef NvError (*pfnGetPclDriverHal)(NvPclDriverHandle hPclDriver);
typedef struct NvPclDevNameHAL
{
    const char *dev_name;
    pfnGetPclDriverHal pfnGetHal;
} NvPclDevNameHal;

NvPclDevNameHal g_PclDevNameHALTable[] =
{
    //{"<dev_name>", NvPclDriver_<DEVNAME>_GetFunctions},
    //{"ar0833", NvPclDriver_AR0833_GetFunctions},
};

#if defined(__cplusplus)
}
#endif

#endif  //PCL_DRIVER_HALS_H

