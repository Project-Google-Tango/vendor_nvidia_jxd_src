/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvDDK Multimedia APIs</b>
 *
 * @b Description: Declares Transport Interface for NvDDK multimedia blocks (mmblocks).
 */

#ifndef INCLUDED_NVMM_TRANSPORT_H
#define INCLUDED_NVMM_TRANSPORT_H


/**
 *  @defgroup nvmm_modules Multimedia Codec APIs
 *
 *  @ingroup nvmm
 * @{
 */

#include "nvmm.h"
#include "nvrm_moduleloader.h"
#include "nvmm_debug.h"
#include "nvmm_queue.h"

#define SIMULATE_NVMM_TRANSPORT_FAILURE 0

/**
 * @brief NvMM Transport Comminication (Client side -> Block side through NvMM service) event type
 *                                                  <-
 */
typedef enum
{
    NvMMTransCommEventType_BlockOpenInfo = 0x1,
    NvMMTransCommEventType_BlockSetTBFCompleteInfo,
    NvMMTransCommEventType_Force32       = 0x7FFFFFFF

} NvMMTransCommEventType;


typedef void (*NvMMServiceCallBack)(void *pContext, void *pEventData, NvU32 EventDataSize);

/**
 * @brief NvMM Service Message enumerations.
 * Following enum are used by NvMM Service for setting the Message type.
 */
typedef struct NvMMTransportBlockOpenInfoRec
{
    NvMMTransCommEventType eEvent;
    void                   *pClientSide;
    NvMMServiceCallBack    pTransCallback;
    NvError                BlockOpenSyncStatus;

} NvMMTransportBlockOpenInfo;

/**
 * @brief NvMM Service Message info structure for SetTransferBuffer function.
 */
typedef struct NvMMTransportBlockSetTBFCompleteInfoRec
{
    NvMMTransCommEventType eEvent;
    void                   *pClientSide;
    NvMMServiceCallBack    pTransCallback;
} NvMMTransportBlockSetTBFCompleteInfo;

typedef struct BlockSideCreationParameters
{
    NvMMBlockType           eType;
    NvMMLocale              eLocale;
    NvU64                   BlockSpecific;
    NvU32                   TransportSpecific;
    NvOsThreadHandle        blockSideWorkerHandle;
    NvOsSemaphoreHandle     ClientEventSema;
    NvOsSemaphoreHandle     BlockEventSema;
    NvOsSemaphoreHandle     BlockWorkerThreadCreateSema;
    NvOsSemaphoreHandle     ScheduleBlockWorkerThreadSema;
    void                    *pClientSide;
    NvOsSemaphoreHandle     BlockInfoSema;
    NvMMServiceCallBack     pTransCallback;
    void                    *hAVPService;
    NvOsLibraryHandle       hLibraryHandle;
    char                    blockName[NVMM_BLOCKNAME_LENGTH];
    NvBool                  SetULPMode;
    NvU16                   LogLevel;
    NvU32                   StackPtr;
    NvU32                   StackSize;
    NvMMQueueHandle         ClientMsgQueue;
    NvMMQueueHandle         BlockMsgQueue;
#if NVMM_KPILOG_ENABLED
    void                    *pNvMMStat;
#endif
} BlockSideCreationParameters;

/** Constructor for the transport wrapped blocks which is used by NvMMOpenMMBlock. */
NvError NvMMTransOpenBlock(NvMMBlockHandle *phBlock, NvMMCreationParameters *pParams);
/** Constructor for the transport wrapped blocks which is used by NvMMCLoseMMBlock. */
void NvMMTransCloseBlock(NvMMBlockHandle phBlock);
/** Main thread/Block side implementation for MMBlock. */
void BlockSideWorkerThread(void *context);
/** Wrapper for AVP side MMBlock entry point */
NvError AxfModuleMain(NvRmModuleLoaderReason Reason, void *Args, NvU32 ArgsSize);


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVMM_TRANSPORT_H
