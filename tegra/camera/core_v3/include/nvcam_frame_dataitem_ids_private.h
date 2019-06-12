/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVCAM_FRAME_DATA_ITEM_IDS_PRIVATE_H
#define INCLUDED_NVCAM_FRAME_DATA_ITEM_IDS_PRIVATE_H

#include "nvmm.h"
#include "nvcam_dataitemdescriptor.h"
#include "nvcam_frame_dataitem_ids_public.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Options dependent on still styles
typedef struct NvCamFrameStillStyleOptionsRec
{
    NvU32 currentStyleIndex;
    NvBool suppressIspGlobalTonemapping;
    NvBool suppressIspSharpening;
    NvBool suppressIspSaturation;
    NvBool debugMessages;
} NvCamFrameStillStyleOptions;

/**
 * List of most used data item identifiers internal to
 * the camera system. When adding new identifiers please
 * add to this Enum. This indexes into a array of Ids in
 * NvCamFrameDataStore. NvCamFrameDataItemID_SIZE should
 * be the last item in the enumeration as it is used to
 * reference the array.
 */
typedef enum
{
    NvCamFrameDataItemID_RAWFrameBuffer = NvCamFrameDataItemIDPub_SIZE,
    NvCamFrameDataItemID_AEState,
    NvCamFrameDataItemID_AEOutputs,
    NvCamFrameDataItemID_AECondition,
    NvCamFrameDataItemID_IspMergeInputState,
    NvCamFrameDataItemID_OpticalBlack,
    NvCamFrameDataItemID_AOHDREnable,
    NvCamFrameDataItemID_AWBOutputs,
    NvCamFrameDataItemID_AWBState,
    NvCamFrameDataItemID_AFOutputs,
    NvCamFrameDataItemID_AFState,
    NvCamFrameDataItemID_ISPOutBuffer,
    NvCamFrameDataItemID_AeAOHDRSettings,
    NvCamFrameDataItemID_M3StatsF32,
    NvCamFrameDataItemID_AeTargetError,
    NvCamFrameDataItemID_FlashOutput,
    NvCamFrameDataItemID_StatsSurfaces,
    NvCamFrameDataItemID_SensorActiveArraySize,
    NvCamFrameDataItemID_M2Stats,
    NvCamFrameDataItemID_FMStats,
    NvCamFrameDataItemID_Lux,
    NvCamFrameDataItemID_CameraState,
    NvCamFrameDataItemID_StillStyleOptions,
    NvCamFrameDataItemID_CaptureDoneTimeMS,
    NvCamFrameDataItemID_FlashControl,
    NvCamFrameDataItemID_CaptureType,
    NvCamFrameDataItemID_PclFrameInfo,
    NvCamFrameDataItemID_PclVersion,
    NvCamFrameDataItemID_PclExposure,
    NvCamFrameDataItemID_PclSensorMode,
    NvCamFrameDataItemID_PclClockProfile,
    NvCamFrameDataItemID_PclFrameRate,
    NvCamFrameDataItemID_PclFocuserSetting,
    NvCamFrameDataItemID_PclFlashSetting,
    NvCamFrameDataItemID_PclSettingsOutput,
    NvCamFrameDataItemID_StatsFrameId,
    NvCamFrameDataItemID_PreProcessEnable,
    NvCamFrameDataItemID_FBStats,
    NvCamFrameDataItemID_IsYUVSensor,
    NvCamFrameDataItemID_ALSState,
    NvCamFrameDataItemID_TonemapState,
    NvCamFrameDataItemID_CCMatrix,
    NvCamFrameDataItemID_IspConfigData,
    NvCamFrameDataItemID_RunIspControl,
    NvCamFrameDataItemID_ScalerRequest,
    NvCamFrameDataItemID_PclFrameRateScheme,
    NvCamFrameDataItemID_SIZE,
    NvCamFrameDataItemIDPrv_Force32 = 0x7FFFFFF,
} NvCamFrameDataItemIDPrivate;


/**
 * Keeps a mapping between dynamically assigned descriptors and
 * statically allocated Ids at compile time. When new
 * data item descriptors are registered with the nvcamdata
 * framework an ID is assigned for the data item dynamically.
 * This map keeps these identifiers easily accessible by the
 * the internal camera system.
 */

typedef struct NvCamFrameDataItemIDMapEntry
{
    NvCamDataItemDescriptorHandle hItemDescriptor;
    // Descriptor also holds Name and Description and these are
    // duplicated but they help in avoiding extra burden on clients
    // to do the registration of most common descriptors in the camera.
    // Frame data store would create these descriptors for the clients.
    char *pStrName;
    char *pStrDescription;
    NvCamCopyConstructorFunction pfnCopyFunction;
    NvCamDestructorFunction pfnDestructor;

}NvCamFrameDataItemIDMapEntry;

typedef struct NvCamFrameDataItemIDMap
{
    NvCamFrameDataItemIDMapEntry Entry[NvCamFrameDataItemID_SIZE];
}NvCamFrameDataItemIDMap;

extern NvCamFrameDataItemIDMap DATA_ITEM_TABLE;

void NvCamFrameDataItem_StandardDestroy(void *pValue);

#ifdef __cplusplus
}
#endif
#endif // INCLUDED_NVCAM_FRAME_DATA_ITEM_IDS_PRIVATE_H

