/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_TRANSPORT_COMMON_H
#define INCLUDED_NVMM_TRANSPORT_COMMON_H

#include "nvmm.h"
#include "nvmm_logger.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define MSG_BUFFER_SIZE 512

#if NV_LOGGER_ENABLED
NvLoggerClient BlockTypeToLogClientType(NvMMBlockType eType);
const char *DecipherMsgCode(NvU8 *msg);
#endif /* NV_LOGGER_ENABLED */

/* message enumeration */
typedef enum {
    NVMM_MSG_SetTransferBufferFunction = 1,
    NVMM_MSG_GetTransferBufferFunction,
    NVMM_MSG_SetBufferAllocator,
    NVMM_MSG_SetSendBlockEventFunction,
    NVMM_MSG_SetState,
    NVMM_MSG_GetState,
    NVMM_MSG_AbortBuffers,
    NVMM_MSG_SetAttribute,
    NVMM_MSG_GetAttribute,
    NVMM_MSG_Extension,
    NVMM_MSG_TransferBufferFromBlock,
    NVMM_MSG_TransferBufferToBlock,
    NVMM_MSG_Close,
    NVMM_MSG_SetBlockEventHandler,
    NVMM_MSG_SendEvent,
    NVMM_MSG_GetAttributeInfo,
    NVMM_MSG_WorkerThreadCreated,
    NVMM_MSG_BlockKPI,
    NVMM_MSG_BlockAVPPROF,
    NVMM_MSG_Force32 = 0x7fffffff
} NvMMTransportMessageType;

typedef enum {
    NVMM_TUNNEL_MODE_DIRECT = 1,
    NVMM_TUNNEL_MODE_INDIRECT
} NVMM_TUNNEL_MODE;

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_TRANSPORT_COMMON_H
