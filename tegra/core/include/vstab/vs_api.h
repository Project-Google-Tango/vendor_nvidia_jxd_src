/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef VS_API_H_
#define VS_API_H_
#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

// Struct to represent a two dimensional point.
typedef struct NvVStabPointRec
{
    NvS32 x, y;
} NvVStabPoint;

// Struct to represent any two dimensional matrix of any data type.
typedef struct NvVStabMatrixRec
{
    void * data;
    NvS32 width;
    NvS32 height;
    NvS32 pitch;
} NvVStabMatrix;

//Enumeration for the feature detector type that is used for global motion estimation.
//The optical flow tracking algorithm is run on these detected features.
typedef enum NvVStabDetectorTypeRec
{
    NV_VS_DETECTOR_TYPE_GRID = 1,
    NV_VS_DETECTOR_TYPE_FAST_GRID
} NvVStabDetectorType;

// Struct of parameters for video stabilization algorithm.
typedef struct NvVStabVSParamsRec
{
    NvVStabDetectorType detectorType;
    NvS32 nFramesForward;
    NvS32 nFramesBackward;
    NvF32 sigmaForward;
    NvF32 sigmaBackward;
    NvF32 cropMargin;
} NvVStabVSParams;

// Constructs a stabilizer object.
NvS32 NvVStabCreateStabilizer(void ** stabilizer, const NvVStabVSParams * params);

// Resets a stabilizer object to the initial state.
NvS32 NvVStabResetStabilizer(void * stabilizer);

// Safely deletes a stabilizer object, releasing the memory.
NvS32 NvVStabDeleteStabilizer(void * stabilizer);

// Initializes an object of the NvVStabVSParams class and sets its default values.
NvS32 NvVStabVSParamsInitialize(NvVStabVSParams * params);

// Returns the parameters of the stabilizer object.
NvS32 NvVStabVSParamsGet(void * stabilizer, NvVStabVSParams * params);

// Estimates global motion between the current frame and the previous one that was passed
// to this function. For the very first call, the function returns false transformation
// (identity matrix), because it does not have a previous frame for comparison.
// The current version of the function estimates only horizontal and vertical displacements of the frames.
NvS32 NvVStabGlobalMotionEstimation(void * stabilizer, const NvVStabMatrix * grayFrame,
                                    NvVStabMatrix * transform);

// Calculates the stabilizing transformation.
// Adds the current transformation matrix (transform) into the internal circular buffer
// and calculates the compensating transformation, using Gaussian convolution on all global motions in
// the buffer. Also returns the index of the frame, to which the stabilizing transformation should
// be applied (transformFrameIdx).
NvS32 NvVStabMotionSmoothing(void * stabilizer, const NvVStabMatrix * transform,
                             NvVStabMatrix * stabilizedTransform, NvS32 * transformFrameIdx);

// Gets the instantaneous clock tick count.
NvF64 NvVStabGetTickCount(void);

// Gets the clock frequency.
NvF64 NvVStabGetTickFrequency(void);

// Places text at a specified location in an image.
NvS32 NvVStabPutTextOnFrame(NvVStabMatrix *frameGray, char *text, NvVStabPoint *point);

#if defined(__cplusplus)
}
#endif
#endif //VS_API_H_
