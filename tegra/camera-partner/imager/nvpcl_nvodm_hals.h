/**
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef PCL_NVODM_HALS_H
#define PCL_NVODM_HALS_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvodm_query_discovery_imager.h"
#include "nvpcl_driver.h"

// NOTE: this version is not encouraged moving forward
NvOdmGUIDNameEntry g_PclNvOdmSensorGUIDTable[] =
{
// NOTE: this version is not encouraged moving forward
    {"nvodm_nvc_sensor", IMAGER_NVC_GUID},
// NOTE: this version is not encouraged moving forward
};


// NOTE: this version is not encouraged moving forward
NvOdmGUIDNameEntry g_PclNvOdmFocuserGUIDTable[] =
{
// NOTE: this version is not encouraged moving forward
    {"nvodm_nvc_focuser", FOCUSER_NVC_GUID},
// NOTE: this version is not encouraged moving forward
};


// NOTE: this version is not encouraged moving forward
NvOdmGUIDNameEntry g_PclNvOdmFlashGUIDTable[] =
{
// NOTE: this version is not encouraged moving forward
    {"nvodm_nvc_torch", TORCH_NVC_GUID},
// NOTE: this version is not encouraged moving forward
};

#if defined(__cplusplus)
}
#endif

#endif  //PCL_NVODM_HALS_H

