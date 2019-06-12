/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvodm_pmu.h"
#include "nvodm_pmu_pluto.h"
#include "nvassert.h"

void PlutoPmuInterruptHandler( NvOdmPmuDeviceHandle  hDevice)
{
    // NOTE: Not implemented completely as it is only for bootloader.
}

NvBool PlutoPmuReadRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 *Count)
{
    // NOTE: Not implemented completely as it is only for bootloader.
    *Count = 0;
    return NV_FALSE;
}

NvBool PlutoPmuWriteRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 Count)
{
    // NOTE: Not implemented completely as it is only for bootloader.
    return NV_FALSE;
}

NvBool PlutoPmuIsRtcInitialized( NvOdmPmuDeviceHandle  hDevice)
{
    // NOTE: Not implemented completely as it is only for bootloader.
    return NV_FALSE;
}
