/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all NvS32ellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef FD_API_H_
#define FD_API_H_
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _FaceLocation
{
    NvS32 x;
    NvS32 y;
    NvS32 width;
    NvS32 height;
} FaceLocation;

typedef struct _FaceSpeed
{
    NvF32 x;
    NvF32 y;
} FaceSpeed;

typedef struct _FaceOrientation
{
    NvF32 azimuth;
    NvF32 roll;
} FaceOrientation;

typedef struct _Face
{
    FaceLocation location;

    NvF32 confidence;

    FaceSpeed speed;
    FaceOrientation orientation;
    NvS32   lifetime;
    NvS32   numFramesNotDetected;
    NvS32   id;
} Face;

typedef struct _DeviceMotion
{
    NvF32 x;
    NvF32 y;
    NvF32 z;
    NvF32 azimuth;
    NvF32 pitch;
    NvF32 roll;
} DeviceMotion;

typedef struct _FDInitializationParameters
{
    NvS32 imageWidth;
    NvS32 imageHeight;
    NvS32 imageStride;

    NvS32 maxNumFacesEstimation;

    const NvS8* FDcascadefilename;

} FDInitializationParameters;

typedef enum _FDImageFormat
{
    FDImageFormat_Grayscale = 1,
    FDImageFormat_YUV420,
    FDImageFormat_YUV422,
    FDImageFormat_BGR,
    FDImageFormat_RGB,

    FDImageFormat_Force32 = 0x7FFFFFFF,
} FDImageFormat;

typedef struct _FDParameters
{
    NvS32 minFaceSize;
    NvS32 maxFaceSize;
    NvF64 scaleFactor;

    NvS32 maxTrackLifetime;
    NvS32 minNeighbors;

    NvS32 skinPrefiltering;
    NvS32 texturePrefiltering;
    NvS32 cameraOrientation;
    NvS32 minDetectionPeriod;
    FDImageFormat imageFormat;
} FDParameters;

typedef enum _costType
{
    SAD = 0,
    SATD,
    MAD
} costType;

typedef struct _MotionVectors
{
    NvS8 blockWidth;
    NvS8 blockHeight;
    NvS16 * vectors;
    costType frameCostType;
    NvS32* costs;

} MotionVectors;

typedef enum _FDStatusError
{
    // FIXME: should start from 1
    FDStatusError_OK = 0,

    /* error codes */
    FDStatusError_CannotAllocateInternalData,
    FDStatusError_SmallBuffer,
    FDStatusError_IndexNotInRange,
    FDStatusError_WrongParameter,

    /* exceptions */
    FDStatusError_OpencvException = 252,
    FDStatusError_StdException,
    FDStatusError_UnknownException,
    FDStatusError_UnknownError,

    FDStatusError_Force32 = 0xFFFFFFFF
} FDStatusError;

NvS32 FDgetRequiredInternalDataSize(const FDInitializationParameters* fdInitParameters,
                                  const FDParameters* fdParameters);
FDStatusError FDinitInternalData(const FDInitializationParameters* fdInitParameters,
                       const FDParameters* fdParameters,
                       NvS8* NvS32ernalDataBuffer,
                       NvS32 NvS32ernalDataBufferSize);

void FDsetDefaultParametersValues(FDParameters* params);

//This function decides if it should detect or track itself
//It contains multithreading algorithm inside.
//
//This algorithm runs face detection process for the whole image in a separate thread
// and runs the face detection process on small subimages (as tracking) while this
// separate process is not finished.
//Note that the results of the detection process on the whole image (received from the thread)
//  are NOT STORED NvS32o the array <Face* faces>, since these results are based on obsolete image,
//  which was 8-10 frames ago.
//So, the results of the thread are used as addition "hNvS32s" for tracking (i.e. small detecors)
// for future frames.
//
//ATTENTION: This algorithm is based on the idea, that the frames are passed to this function
// one by one, continiously, and there is NO big lag between the frames.
// (otherwise tracking presumptions won't work)
// --- see the function FDstopFindFaces and FDresetFindFaces below.
NvS32 FDfindFaces(NvS8* NvS32ernalDataBuffer,
                NvS8* grayImage,
                const MotionVectors* motionVectors,
                const DeviceMotion* motion,
                Face* faces, NvS32 numFaces);

//This function is required to change parameters of the face detection.
//Note that these parameters are initialized in the function FDinitInternalData.
//And note that the parameters from the structure FDInitializationParameters cannot be updated
// without deallocating the inner structures (by FDdeallocateInnerStructures)
// and reinitialization the structures (by FDinitInternalData)
FDStatusError FDupdateParameters(const FDParameters* fdParameters, NvS8* NvS32ernalDataBuffer);

//This function stops the separate thread with detection process
//This is very important to call this function before deallocation of the buffer <NvS8* NvS32ernalDataBuffer>,
//since the thread uses the data stored in the buffer,
//so without calling this function the application will be crashed after the deallocation.
//
//Note that this function may require some time to stop the separate thread with detection process,
// since there is no method in Android to cancel pthread quickly, so we have to signal pthread and
// wait its reaction.
FDStatusError FDstopFindFaces(NvS8* NvS32ernalDataBuffer);


//This function is less important that the previous one, it does not stops the separate detecting thread,
// but it just clears the inner structures storing tracking data.
//This function may be used if campera module knows, that the was a big lag between frames,
// so the tracking data are outdated.
FDStatusError FDresetFindFaces(NvS8* NvS32ernalDataBuffer);

//This function is required to deallocate all the inner data structures correctly.
//Without calling this function memory leaks will occur.
//This function calls the function FDstopFindFaces (see above).
FDStatusError FDdeallocateInnerStructures(NvS8* NvS32ernalDataBuffer);

#ifdef __cplusplus
} //extern "C"
#endif
#endif
