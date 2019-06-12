/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVCAM_FRAME_DATA_H
#define INCLUDED_NVCAM_FRAME_DATA_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvcam_frameinfo.h"
#include "nvcam_datainfo.h"
#include "nvcam_dataitemdescriptor.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Used for frame flow debugging
#define METADATA_FRAME_FLOW_DEBUG 0

#define FRAME_DS_DEBUG_TAG  "NV_CAM_FRAME_DS"
#define DS_DEBUG_NAMED_POINT    (0)
#define DS_DEBUG_FRAME_DATA     (0)
#define DS_DEBUG_DATA_ITEMS     (0)

#define NV_FRD_CHECK_ERROR_FREE(expr, ppData) \
    do \
    { \
        e = (expr); \
        if (e != NvSuccess) \
        { \
            NvOsFree(*ppData); \
            *ppData = NULL; \
        } \
    } while (0)


// A bit mask enum to describe various clients
// which hold on to FRDs.
typedef enum NvCamFRDClientRefMask
{
    NvCamFRDClientRefMask_Capture   = (1<<1),
    NvCamFRDClientRefMask_AutoAlg   = (1<<2),
    NvCamFRDClientRefMask_PNode     = (1<<3),
    NvCamFRDClientRefMask_NpList    = (1<<4),

    NvCamFRDClientRefMask_Force32 = 0x7FFFFFF,
} NvCamFRDClientRefMask;

/**
 *  NvCamFrameData is an object which contains data associated
 *  with frame(s) which make up an image in the camera pipeline.
 */
typedef struct NvCamFrameData
{
    struct NvCamFrameDataStore *hDataStore;//....A data store handle.
    NvU32 TimeCreatedMs;//.......................A time stamp when this frame data was created.
    NvBool  InUse;//.............................A flag to know when the Data store can reuse this
                                              // NvCamFrameData item. Can be changed to an enum to
                                              // indicate more than one state in the life cycle.
    NvOsMutexHandle hRefClientMaskMutex;
    NvU32 RefClientMask;//.......................A reference count mask to know if any clients are holding on
                                              // to the FRD.
    NvBool IsDummy;//.........................// Indicates if this FRD is associated with a dummy
                                              // capture in the system. Dummy capture is nothing but
                                              // a bubble to remain in sync with the sensor.
    NvU32 UniqueId;//............................A number which tracks a FRD. SyncPtId cannot be
                                              // used as they are only valid after VI acquisition.
    NvU64 ReprocessId;                        // Set when this FRD can be later used to do some
                                              // reprocessing.

    // This is an object which is returned by the
    // Metadata framework.
    NvCamFrameInfoHandle hFrameDataBundle;//.....A collection of NvCamDataItem associated with a frame.

}NvCamFrameData;

typedef NvCamFrameData* NvCamFrameData_Handle;



/**
 *  Functions which operate on the NvCamFrameData
 *  object.
 */

// Function allocates a descriptor with the meta data framework and assigns a name,
// description, copy constructor and a destructor of the data type if specified.
// Clients are expected to store these descriptors to be able to query the data
// later. Note that clients can also query the data using a name if it was specified
// during allocation of the descriptor.
NvError NvCamFrameData_CreateDataItemDescriptor(NvCamDataItemDescriptorHandle *hDescriptor,
    NvCamDataInfoHandle hDataInfo,
    NvCamCopyConstructorFunction pfnCopyFunction,
    NvCamDestructorFunction pfnDestructor,
    char *pName,
    char *pDescription);

void NvCamFrameData_ClearClientMask(
    NvCamFrameData_Handle hFrameData,
    NvCamFRDClientRefMask Mask);

void NvCamFrameData_SetClientMask(
    NvCamFrameData_Handle hFrameData,
    NvCamFRDClientRefMask Mask);

// Function can be used to print debug information
// related to a data item in frame data.
char *NvCamFrameData_GetDataItemName(
    NvCamFrameData_Handle hFrameData,
    NvU32 ItemId);

// Allocates a new FrameData object by copying the input object.
NvCamFrameData_Handle NvCamFrameData_Clone(
    NvCamFrameData_Handle hFrameData, NvBool ShallowCopy);

// Checks if a particular data item is already present in the Frame data
// object.
NvBool NvCamFrameData_CheckDataItemPresence(
    NvCamFrameData_Handle hFrameData,
    NvU32 ItemId);

// Gets the value contained in the NvCamFrameData_Handle with a given NvCamFrameDataItemID(Public/Private).
// NOTE: Client needs to call NvCamFrameData_DataItemRelease() when it no longer wants to
// use the value else the value is retained in the system forever.
void* NvCamFrameData_GetDataItemValue(
    NvCamFrameData_Handle hFrameData,
    NvU32 ItemId);

// Sets a data item in the NvCamFrameData_Handle for a specific
// NvCamFrameDataItemID(Public/Private).
NvError NvCamFrameData_SetDataItem(
    NvCamFrameData_Handle hFrameData,
    NvU32 ItemId,
    void *data);

// Indicates that a client wants to retain the data item for
// longer term use. Client needs to call a release later when
// done with the item else this would lead to item being held
// in memory forever or until program exits. Note this is similar
// to client calling a get data item without the release.
NvError NvCamFrameData_DataItemRetain(
    NvCamFrameData_Handle hFrameData,
    NvU32 ItemId);

// Decreses the ref count on the data item if a client
// previously requested a NvCamFrameData_DataItemRetain or
// called a NvCamFrameData_GetDataItemValue.
NvError NvCamFrameData_DataItemRelease(
    NvCamFrameData_Handle hFrameData,
    NvU32 ItemId);


#ifdef __cplusplus
}
#endif
#endif // INCLUDED_NVCAM_FRAME_DATA_H

