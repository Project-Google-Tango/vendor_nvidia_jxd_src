/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef VS_API_2_H_
#define VS_API_2_H_

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef unsigned long long ULL;

typedef struct NvVStab2MatrixRec
{
    void* data;
    NvS32 width;
    NvS32 height;
    NvS32 pitch;
    ULL timeStamp;
} NvVStab2Matrix;

typedef enum NvVStab2DetectorTypeRec
{
    NV_VS2_DETECTOR_TYPE_GRID = 1,
    NV_VS2_DETECTOR_TYPE_FAST_GRID,
    NV_VS2_DETECTOR_TYPE_MAX = 0x7FFFFFFF
} NvVStab2DetectorType;

typedef struct NvVStab2VSParamsRec
{
    NvVStab2DetectorType detectorType;
} NvVStab2VSParams;

typedef struct NvVStab2SmoothingParamsRec
{
    NvS32 nFramesForward;
    NvS32 nFramesBackward;
    NvF32 sigmaForward;
    NvF32 sigmaBackward;
    NvF32 cropMargin;
} NvVStab2SmoothingParams;

NvS32 NvVStab2CreateStabilizer(void** stabilizer, const NvVStab2VSParams* params);

NvS32 NvVStab2ResetStabilizer(void* stabilizer);

NvS32 NvVStab2DeleteStabilizer(void* stabilizer);

NvS32 NvVStab2VSParamsInitialize(NvVStab2VSParams* params);

NvS32 NvVStab2SmoothingParamsInitialize(NvVStab2SmoothingParams* params);

NvS32 NvVStab2VSParamsGet(void* stabilizer, NvVStab2VSParams* params);

NvS32 NvVStab2Feed(void* stabilizer, const NvVStab2Matrix* grayFrame);

NvS32 NvVStab2FeedYUV422(void* stabilizer, const NvVStab2Matrix* YUV422Frame);

NvS32
NvVStab2CalculateSmoothingTransform(
    void* stabilizer,
    const ULL timeStamp,
    NvVStab2SmoothingParams* smoothingParams,
    NvVStab2Matrix* stabilizedTransform);

NvS32 NvVStab2PutTextOnFrame(NvVStab2Matrix *frameGray, char *text, NvPoint *point);

NvF64 NvVStabGetTickCount(void);

NvF64 NvVStabGetTickFrequency(void);

#if defined(__cplusplus)
}
#endif

#endif //VS_API_2_H_
