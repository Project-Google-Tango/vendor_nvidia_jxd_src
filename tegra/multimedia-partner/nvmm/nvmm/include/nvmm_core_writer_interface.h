/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @brief Defines the API's exposed of Mp4 Parser Block
 */

#ifndef INCLUDED_NVMM_CORE_WRITER_INT_H
#define INCLUDED_NVMM_CORE_WRITER_INT_H

#include "nvmm.h"
#include "nvos.h"
#include "nvmm_writer.h"
#include "nvmm_logger.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct NvmmWriterContextRec* NvmmWriterContext;

typedef struct NvmmCoreWriterOpsRec
{

/**
 * OpenWriter:. Opens/Inits all writer realted info
 * @param WriterContext - NvmmWriterContextHandle
 * 
 */

NvError
(*OpenWriter)(
    NvmmWriterContext WriterContext);

/**
 * CloseWriter:. closes/destroys all writer realted info.
 * @param WriterContext - NvmmWriterContextHandle
 * 
 */

NvError
(*CloseWriter)(
    NvmmWriterContext WriterContext);

/**
 * ProcessRawBuffer: Adds the media sample
 * @param WriterContext - NvmmWriterContextHandle
 * @param Stream index - Stream index to write sample for
 * @param pBuffer- pointer to NvMMBuffer
 * 
*/

NvError
(*ProcessRawBuffer)(
    NvmmWriterContext WriterContext,
    NvU32 streamIndex,
    NvMMBuffer *RawBuffer);

/**
 * GetBufferRequirements: Gets the required no. and size of buffers
 *
 * @param WriterContext - NvmmWriterContextHandle
 * @param MinBuffers,MaxBuffers,size of MinBuffers,size of MaxBuffers
 * with the file name
 * 
 */

NvError
(*GetBufferRequirements)(
    NvmmWriterContext WriterContext,
    NvU32 StreamIndex,
    NvU32 *MinBuffers,
    NvU32 *MaxBuffers,
    NvU32 *MinBufferSize,
    NvU32 *MaxBufferSize);


/**
 * GetFrameCount: Gets the audio/video frame count
 *
 * @param WriterContext - NvmmWriterContextHandle
 * @param pConfig - pointer with NvMMWriterAttrib_FileName structure
 * with the file name
 * configuration parameters.
 * 
 */

NvError
(*GetAttribute)(
    NvmmWriterContext WriterContext,
    NvU32 AttributeType,
    void *pAttribute);

/**
 * SetFileName: sets the output  file Name
 *
 * @param WriterContext - NvmmWriterContextHandle
 * @param pConfig - pointer with NvMMWriterAttrib_FileName structure or
 * NvMMWriterAttrib_TempFilePath or NvMMWriterAttrib_SplitTrack or
 * NvMMWriterStreamConfig or NvMMWriteUserDataConfig or
 * NvMMWriterAttrib_FileSize
 * with the file name
 * configuration parameters.
 * 
 */

NvError
(*SetAttribute)(
    NvmmWriterContext WriterContext,
    NvU32 AttributeType,
    void *pAttribute);

} NvmmCoreWriterOps;

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVMM_CORE_WRITER_INT_H
