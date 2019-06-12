/* Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVX_VPP_H
#define __NVX_VPP_H


NvU32 customcookie;
//NvError NvxVPPallocVideoSurface(void *arg);
NvError VppHALInit(SNvxNvMMLiteTransformData *pData);
NvError VppHALDestroy(SNvxNvMMLiteTransformData *pData);
NvError VppHALProcess(SNvxNvMMLiteTransformData *pData, NvMMBuffer* pDecSurf, NvMMBuffer* pOutSuface);

//CPU Based interfaces
NvError NvVPPApplyCPUBasedEffect(SNvxNvMMLiteTransformData* pData, NvMMBuffer* pDecSurf, NvMMBuffer* pOutSuface, void *cookie);

//GL Based Wrappers
NvError NvVPPApplyEGLBasedEffect(SNvxNvMMLiteTransformData* pData, NvMMBuffer* pDecSurf, NvMMBuffer* output, void *cookie);
NvError NvVPPEGLBasedInit(SNvxNvMMLiteTransformData* pDatat, void *cookie);
NvError NvVPPEGLBasedDestroy(SNvxNvMMLiteTransformData* pDatat, void *cookie);


#endif // __NVX_VPP_H
