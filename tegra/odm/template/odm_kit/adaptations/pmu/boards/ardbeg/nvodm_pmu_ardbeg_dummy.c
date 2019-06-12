/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_ardbeg.h"
#include "nvassert.h"

NvBool ArdbegPmuReadRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 *Count)
{
    // NOTE: Currently a stub. Will be implemented soon.
    *Count = 0;
    NV_ASSERT(0);
    return NV_FALSE;
}

NvBool ArdbegPmuWriteRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 Count)
{
    // NOTE: Currently a stub. Will be implemented soon.
    NV_ASSERT(0);
    return NV_FALSE;
}

NvBool ArdbegPmuIsRtcInitialized( NvOdmPmuDeviceHandle  hDevice)
{
    // NOTE: Currently a stub. Will be implemented soon.
    NV_ASSERT(0);
    return NV_FALSE;
}
