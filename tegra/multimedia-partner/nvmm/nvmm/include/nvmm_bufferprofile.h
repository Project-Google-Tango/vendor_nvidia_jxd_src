/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_BUFFERPROFILE_H
#define INCLUDED_NVMM_BUFFERPROFILE_H


#if defined(__cplusplus)
extern "C"
{
#endif

#define NVMM_BUFFER_PROFILING_ENABLED (0)
#define MAX_BUFFER_PROFILING_ENTRIES (6000)


typedef enum
{
    // TODO: Remove this when 3GP becomes an nvmm block.
    NvMMBlockTypeForBufferProfiling_3gpAudio = 0x1000,
    NvMMBlockTypeForBufferProfiling_3gpVideo
}NvMMBlockTypeForBufferProfiling;


typedef enum
{
    NvMMBufferProfilingEvent_ReceiveBuffer = 1,
    NvMMBufferProfilingEvent_SendBuffer,
    NvMMBufferProfilingEvent_StartProcessing,

    NvMMBufferProfilingEvent_Force32 = 0x7FFFFFFF
}NvMMBufferProfilingEvent;

typedef struct NvMMBufferProfilingEntryRec
{
    NvMMBufferProfilingEvent Event;
    NvU32 StreamIndex;
    NvU32 FrameId;
    NvU64 TimeStamp; // in 100 nanosecond

} NvMMBufferProfilingEntry;

typedef struct NvMMBufferProfilingDataRec
{
    NvMMBlockType BlockType;
    NvMMBufferProfilingEntry Entries[MAX_BUFFER_PROFILING_ENTRIES];
    NvU32 NumEntries;
} NvMMBufferProfilingData;


NvError
NvMMUtilAddBufferProfilingEntry(
    NvMMBufferProfilingData* pProfilingData,
    NvMMBufferProfilingEvent Event,
    NvU32 StreamIndex,
    NvU32 FrameId);

NvError
NvMMUtilDumpBufferProfilingData(
    NvMMBufferProfilingData* pProfilingData,
    NvOsFileHandle hFile);


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_BUFFERPROFILE_H
