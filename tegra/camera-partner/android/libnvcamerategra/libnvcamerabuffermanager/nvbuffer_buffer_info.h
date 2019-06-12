/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVBUFFER_BUFFER_INFO_H
#define NVBUFFER_BUFFER_INFO_H

#include "nvbuffer_manager_common.h"



typedef struct NvOuputPortBuffers_
{
    NvBool bufferInUse;
    NvBool bufferAllocated;
    NvMMBuffer *pBuffer;
}NvOuputPortBuffers;

typedef struct NvOuputPortConfig_
{
    NvBool                         used;
    NvMMNewBufferConfigurationInfo originalBufCfg;
    NvMMNewBufferConfigurationInfo currentBufCfg;
    NvMMNewBufferRequirementsInfo  bufReq;
    NvOuputPortBuffers             buffer[MAX_OUTPUT_BUFFERS_PER_PORT];
    NvU32                          totalBuffersAllocated;
}NvOuputPortConfig;

typedef struct NvInputPortConfig_
{
    NvBool                         used;
    NvMMNewBufferConfigurationInfo originalBufCfg;
    NvMMNewBufferConfigurationInfo currentBufCfg;
    NvMMNewBufferRequirementsInfo  bufReq;
}NvInputPortConfig;

typedef struct NvComponentBufferConfig_
{
    NvOuputPortConfig outputPort[MAX_PORTS];
    NvInputPortConfig inputPort[MAX_PORTS];
}NvComponentBufferConfig;

typedef struct NvStreamBufferConfig_
{
    NvComponentBufferConfig component[MAX_COMPONENTS];
}NvStreamBufferConfig;

// Helpers
NvBool BufferConfigurationsEqual(NvOuputPortConfig *pOldPortCfg, NvOuputPortConfig *pNewPortCfg);
NvBool BufferConfigurationsEqual(NvMMNewBufferConfigurationInfo *pOldCfg,
                                 NvMMNewBufferConfigurationInfo *pNewCfg);







#endif



