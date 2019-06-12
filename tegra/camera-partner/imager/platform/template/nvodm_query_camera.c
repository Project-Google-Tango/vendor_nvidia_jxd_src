/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvodm_imager_guid.h"
#include "nvodm_query_camera.h"

//#include "nvodm_query_camera_modules.h"

/*****************************************************************************/

const void *NvOdmCameraQuery(NvU32 *NumModules)
{
// Steps to support PCL camera device scripts:
//    - create the script header file: ./nvodm_query_camera_modules.h
//          sample: ../ceres/nvodm_query_camera_modules.h
//    - un-comment the header files above:
//          subboards/nvodm_query_camera_modules.h
//    - remove below #if 0 line and un-comment the next line: #if (BUILD_FOR_AOS...
#if 0
//#if (BUILD_FOR_AOS == 0)
    if (!NumModules) {
        return NULL;
    }

    *NumModules = NV_ARRAY_SIZE(s_CameraDevices);
    return (void *)s_CameraDevices;
#else
    *NumModules = 0;
    return NULL;
#endif
}
