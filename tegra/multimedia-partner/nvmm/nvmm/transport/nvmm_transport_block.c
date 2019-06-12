/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvmm/transport/nvmm_transport.c
 *  This file defines the transportAPI wrappers around the NvMMblock methods and
 * callbacks.
 *  The wrappers are composed of two parts:
 *   1) ClientSide wrapper: resides on the client side. Translates client calls
 *      to the NvMMBlock handle to transport API calls and sends them to the
 *      block side wrapper. Also receives block callback messages from the
 *      BlockSide wrapper and translates them to callback calls on the client's
 *      callback functions.
 *   2) BlockSide wrapper: resides on the block side. Receives client messages,
 *      converts them to NvMMBlock calls. Translates callbacks from the block
 *      to messages
 */
#ifndef NV_ENABLE_DEBUG_PRINTS
#define NV_ENABLE_DEBUG_PRINTS (0)
#endif

#include "nvmm_transport.h"
#include "nvmm_transport_private.h"
#include "nvrm_memmgr.h"
#include "nvmm_queue.h"
#include "nvmm_amrnbdec.h"
#include "nvmm_amrwbenc.h"
#include "nvmm_amrnbenc.h"
#include "nvmm_wavdec.h"
#include "nvmm_wavenc.h"
#include "nvmm_aacdec.h"
#include "nvmm_adtsdec.h"
#include "nvassert.h"
#include "nvmm_mp3dec.h"
#include "nvmm_mp2dec.h"
#include "nvmm_aacplusenc.h"
#if NV_IS_RINGTONE_ENABLED
#include "nvmm_ringtoneblock.h"
#endif
#if NV_IS_DECWMA_ENABLED
#include "nvmm_wmadec.h"
#endif
#if NV_IS_DECWMAPRO_ENABLED
#include "nvmm_wmaprodec.h"
#endif
#if NV_IS_DECWMASUPER_ENABLED
#include "nvmm_wma_super_dec.h"
#endif
#if NV_IS_DECOGG_ENABLED
#include "nvmm_oggdec.h"
#endif
#if NV_IS_SUPERPARSER_ENABLED
#include "nvmm_super_parserblock.h"
#endif

#include "nvmm_basewriterblock.h"
#include "nvmm_mixer.h"
#include "nvmm_mixer_int.h"
#include "nvmm_audio_private.h"
#include "nvmm_camera_int.h"
#include "nvmm_amrwbdec.h"
#include "nvmm_digitalzoomblock.h"
#include "nvmm_superaudiodec.h"
#include "nvrm_memmgr.h"
#include "nvrm_moduleloader.h"
#include "nvmm_util.h"

#if NV_LOGGER_ENABLED
NvLoggerClient BlockTypeToLogClientType(NvMMBlockType eType)
{
    switch (eType)
    {
        case NvMMBlockType_DecAAC:
        case NvMMBlockType_DecEAACPLUS: return NVLOG_AAC_DEC;
        case NvMMBlockType_DecMP3:
        case NvMMBlockType_DecMP3SW: return NVLOG_MP3_DEC;
        case NvMMBlockType_VideoRenderer: return NVLOG_VIDEO_RENDERER;
        case NvMMBlockType_AudioMixer: return NVLOG_MIXER;
        case NvMMBlockType_DecWMA:
        case NvMMBlockType_DecWMAPRO:
        case NvMMBlockType_DecWMALSL: return NVLOG_WMA_DEC;
        case NvMMBlockType_DecWAV: return NVLOG_WAV_DEC;
        default:
            break;
    }

    return NVLOG_MAX_CLIENTS;
}
#endif

/** NvMMTransport's blockside context */
typedef struct BlockSideInfo_
{
    NvU32                       TunnelledMode;

    NvMMBlockHandle             hBlock;
    SendBlockEventFunction      pEventHandler;
    TransferBufferFunction      transferBuffer;
    NvMMDoWorkFunction          pDoWork;
    NvOsMutexHandle             TransportApiLock;

    NvU8                        workBuffer[MSG_BUFFER_SIZE];
    NvU8                        sendMsgBuffer[MSG_BUFFER_SIZE];
    NvU8                        rcvMsgBuffer[MSG_BUFFER_SIZE];

    BlockSideCreationParameters Param;

    NvBool                      bShutDown;
    NvBool                      bAbnormalTermination;
} BlockSideInfo;

static NvError SendEventHandler(void *pContext, NvU32 EventType, NvU32 EventInfoSize, void *pEventInfo);
static NvError TransferBufferEventHandler(void *pContext, NvU32 StreamIndex, NvMMBufferType BufferType, NvU32 BufferSize, void *pBuffer);
static NvError BlockSideSendMsg(BlockSideInfo *pBlockSide, NvU8* msg, NvU32 size) ;
static NvError BlockSideRcvMsg(BlockSideInfo *pBlockSideInfo, NvU8* msg);
static NvError ConvertMsgToNvMMBlockCall(BlockSideInfo *pBlockSideInfo, NvU8* msg);
static NvError OpenBlock(BlockSideInfo* pBlockSide);
static void    CloseBlock(BlockSideInfo *pBlockSide);
static NvError BlockSideShutDown(BlockSideInfo *pBlockSideInfo);


void BlockSideWorkerThread(void *context)
{
    BlockSideInfo *pBlockSide = NULL;
    NvBool bMoreWork;
    NvU8* msg;
    NvU32 size = 0;
    NvBool bPortInitialized = NV_FALSE;
    NvBool bBlockOpenSyncDone = NV_FALSE;
    NvError status = NvSuccess;
    NvMMTransportBlockOpenInfo TransportBlockOpenInfo;
    NvU32 SignalSent = 0;

    NV_DEBUG_PRINTF(("In BlockSideWOrkerThread \n"));

    pBlockSide = (BlockSideInfo *)NvOsAlloc(sizeof(BlockSideInfo));
    if (!pBlockSide)
    {
        NV_DEBUG_PRINTF(("BlockSideWorkerThread: pBlockSide alloc failed\n"));
        status = NvError_InsufficientMemory;
        goto cleanup;
    }

    NvOsMemset(pBlockSide, 0, sizeof(BlockSideInfo));
    NvOsMemcpy(&pBlockSide->Param, context, sizeof(BlockSideCreationParameters));
    NV_ASSERT(sizeof(NvMMBuffer) <= MSG_BUFFER_SIZE); // pBlockSide->workBuffer will be casted to NvMMBuffer

    /* Populate block open sync struct */
    TransportBlockOpenInfo.eEvent         = NvMMTransCommEventType_BlockOpenInfo;
    TransportBlockOpenInfo.pClientSide    = pBlockSide->Param.pClientSide;
    TransportBlockOpenInfo.pTransCallback = pBlockSide->Param.pTransCallback;

    status = NvOsMutexCreate(&pBlockSide->TransportApiLock);
    if (status != NvSuccess)
    {
        NV_DEBUG_PRINTF(("BlockSideWorkerThread: Mutex alloc failed\n"));
        goto cleanup;
    }

    pBlockSide->pEventHandler = SendEventHandler;
    pBlockSide->transferBuffer = TransferBufferEventHandler;

    /* Fill block open sync status */
    TransportBlockOpenInfo.BlockOpenSyncStatus = status;

    if (pBlockSide->Param.ScheduleBlockWorkerThreadSema)
    {
        NvOsSemaphoreSignal(pBlockSide->Param.ScheduleBlockWorkerThreadSema);
        SignalSent = 1;
    }

    TransportBlockOpenInfo.pTransCallback(TransportBlockOpenInfo.pClientSide,
                                          &TransportBlockOpenInfo,
                                          sizeof(NvMMTransportBlockOpenInfo));
    bBlockOpenSyncDone = NV_TRUE;
    bPortInitialized = NV_TRUE;

    /* set the log level */
    NV_LOGGER_SETLEVEL(BlockTypeToLogClientType(pBlockSide->Param.eType), (NvLoggerLevel)pBlockSide->Param.LogLevel);

    if ((status = OpenBlock(pBlockSide)) != NvSuccess)
    {
        NV_DEBUG_PRINTF(("BlockSideWorkerThread: OpenBlock type %u failed\n", pBlockSide->Param.eType));
        goto cleanup;
    }

    if(pBlockSide->hBlock->BlockSemaphoreTimeout == 0)
    {
        pBlockSide->hBlock->BlockSemaphoreTimeout = NV_WAIT_INFINITE;
    }
    size = 0;
    msg = pBlockSide->sendMsgBuffer;
    *((NvU32 *)&msg[size]) = NVMM_MSG_WorkerThreadCreated;
    size += sizeof(NvU32);
    *((NvU32 *)&msg[size]) = NvSuccess;
    size += sizeof(NvU32);
    *(NvOsThreadHandle*)(void*)&msg[size] = pBlockSide->Param.blockSideWorkerHandle;
    size += sizeof(NvOsThreadHandle);
    *(NvMMBlockHandle*)(void*)&msg[size] = pBlockSide->hBlock;
    size += sizeof(NvMMBlockHandle);
    status = BlockSideSendMsg(pBlockSide, msg, size);
    if(status != NvSuccess)
        pBlockSide->bShutDown = NV_TRUE;

    while (!pBlockSide->bShutDown)
    {
        /* Wait for some trigger via the block semaphore */
        /* (e.g. new messages, hardware interupts, etc)  */
        NvError StatusTimeout = NvOsSemaphoreWaitTimeout(pBlockSide->Param.BlockEventSema, pBlockSide->hBlock->BlockSemaphoreTimeout);

        bMoreWork = NV_FALSE;
        do
        {
            if (StatusTimeout == NvError_Timeout)
            {
                if (!pBlockSide->bShutDown)
                    pBlockSide->pDoWork(pBlockSide->hBlock, NvMMDoWorkCondition_Timeout, &bMoreWork);
                StatusTimeout = NvSuccess;
            }
            else
            {
                /* Check for messages */
                status = BlockSideRcvMsg(pBlockSide, pBlockSide->rcvMsgBuffer);
                if( NvSuccess == status)
                {
                    status = ConvertMsgToNvMMBlockCall(pBlockSide, pBlockSide->rcvMsgBuffer);
                    if (!pBlockSide->bShutDown)
                        pBlockSide->pDoWork(pBlockSide->hBlock, NvMMDoWorkCondition_Critical, &bMoreWork);
                }
                else
                {
                    /* then try to do non-critical work */
                    if (!pBlockSide->bShutDown)
                        pBlockSide->pDoWork(pBlockSide->hBlock, NvMMDoWorkCondition_SomeWork, &bMoreWork);
                }
            }
        } while (bMoreWork && !pBlockSide->bShutDown);
    }

    NV_ASSERT(pBlockSide->bShutDown);
    BlockSideShutDown(pBlockSide);
    return;

cleanup:
    if (!SignalSent)
    {
        if (pBlockSide->Param.ScheduleBlockWorkerThreadSema)
            NvOsSemaphoreSignal(pBlockSide->Param.ScheduleBlockWorkerThreadSema);
    }

    if (bPortInitialized)
    {
        size = 0;
        msg = pBlockSide->sendMsgBuffer;
        *((NvU32 *)&msg[size]) = NVMM_MSG_WorkerThreadCreated;
        size += sizeof(NvU32);
        *((NvU32 *)&msg[size]) = status;
        size += sizeof(NvU32);
        BlockSideSendMsg(pBlockSide, msg, size);
    }
    else
    {
        if(bBlockOpenSyncDone == NV_FALSE)
        {
            if(pBlockSide)
            {
                TransportBlockOpenInfo.BlockOpenSyncStatus = status;
                if(TransportBlockOpenInfo.pTransCallback)
                {
                    TransportBlockOpenInfo.pTransCallback(TransportBlockOpenInfo.pClientSide,
                                                          &TransportBlockOpenInfo,
                                                          sizeof(NvMMTransportBlockOpenInfo));
                }
            }
        }
    }
    if (pBlockSide)
    {
#if NV_IS_AVP
        pBlockSide->pNvmmManagerFxnTable->NvmmManagerUnregisterBlock(
                                          pBlockSide->hBlock,
                                          pBlockSide->Param.hAVPService);
#endif
        NvOsSemaphoreDestroy(pBlockSide->Param.BlockEventSema);
        NvOsMutexDestroy(pBlockSide->TransportApiLock);
        NvOsFree(pBlockSide);
    }
}

NvError ConvertMsgToNvMMBlockCall(BlockSideInfo *pBlockSideInfo, NvU8* msg)
{
    NvError status = NvSuccess;
    NvU32 size;
    NvU32 message;
    NvU32 streamIndex, streamIndexForTransferBuffer;
    NvU32 AttributeType, AttributeSize, AttributeFlag;
    void *pAttribute;
    NvMMState eState;
    NvBool bAllocateBuffers;
    NvU32 BufferSize;
    NvMMBufferType BufferType;
    void *pBuffer;
    TransferBufferFunction TransferBufferToBlock = NULL;
    NvMMBlockHandle hBlock = pBlockSideInfo->hBlock;
    NvU8* sendMsg;
    size = 0;
    message = *((NvU32 *)&msg[size]); size += sizeof(NvU32);

    switch (message)
    {
        case NVMM_MSG_SetTransferBufferFunction:
            streamIndex = *((NvU32 *)&msg[size]);
            size += sizeof(NvU32);
            streamIndexForTransferBuffer = *((NvU32 *)&msg[size]);
            size += sizeof(NvU32);
            pBlockSideInfo->TunnelledMode = *((NvU32 *)&msg[size]);
            size += sizeof(NvU32);
            {
                NvMMBlockHandle phOutBlock = NULL;
                NvMMBlockHandle pContextForTranferBuffer = NULL;
                NvMMTransportBlockSetTBFCompleteInfo BlockSetTBFCompleteInfo;
                BlockSetTBFCompleteInfo.eEvent         = NvMMTransCommEventType_BlockSetTBFCompleteInfo;
                BlockSetTBFCompleteInfo.pClientSide    = pBlockSideInfo->Param.pClientSide;
                BlockSetTBFCompleteInfo.pTransCallback = pBlockSideInfo->Param.pTransCallback;

                if(pBlockSideInfo->TunnelledMode == NVMM_TUNNEL_MODE_DIRECT)
                {
                    phOutBlock = *((NvMMBlockHandle *)&msg[size]);
                    size += sizeof(NvMMBlockHandle);
                }
                else if (pBlockSideInfo->TunnelledMode == NVMM_TUNNEL_MODE_INDIRECT)
                {
                    TransferBufferToBlock = *(TransferBufferFunction *)&msg[size];
                    size += sizeof(TransferBufferFunction);
                    pContextForTranferBuffer = *((NvMMBlockHandle *)&msg[size]);
                    size += sizeof(NvMMBlockHandle);
                }
                if(pBlockSideInfo->TunnelledMode == NVMM_TUNNEL_MODE_DIRECT)
                {
                    NvMMAttrib_SetTunnelModeInfo SetTunnelModeInfo;
                    SetTunnelModeInfo.StreamIndex    = streamIndex;
                    SetTunnelModeInfo.bDirectTunnel = NV_TRUE;

                    hBlock->SetAttribute(hBlock,
                                    NvMMAttribute_SetTunnelMode,
                                    NV_FALSE,
                                    sizeof(NvMMAttrib_SetTunnelModeInfo),
                                    &SetTunnelModeInfo);

                    TransferBufferToBlock = phOutBlock->GetTransferBufferFunction(
                                                        phOutBlock,
                                                        0,
                                                        NULL);
                    status = hBlock->SetTransferBufferFunction(
                                     hBlock,
                                     streamIndex,
                                     TransferBufferToBlock,
                                     phOutBlock,
                                     streamIndexForTransferBuffer);
                }
                else if (pBlockSideInfo->TunnelledMode == NVMM_TUNNEL_MODE_INDIRECT)
                {
                    status = hBlock->SetTransferBufferFunction(
                                     hBlock,
                                     streamIndex,
                                     TransferBufferToBlock,
                                     pContextForTranferBuffer,
                                     streamIndexForTransferBuffer);

                }
                else
                {
                    status = hBlock->SetTransferBufferFunction(
                        hBlock,
                        streamIndex,
                        pBlockSideInfo->transferBuffer,
                        pBlockSideInfo,
                        streamIndexForTransferBuffer);
                }

                if(BlockSetTBFCompleteInfo.pTransCallback)
                {
                    BlockSetTBFCompleteInfo.pTransCallback(BlockSetTBFCompleteInfo.pClientSide,
                                                           &BlockSetTBFCompleteInfo,
                                                           sizeof(NvMMTransportBlockSetTBFCompleteInfo));
                }
            }
            break;

        case NVMM_MSG_GetTransferBufferFunction:
            break;

        case NVMM_MSG_SetBufferAllocator:
            streamIndex = *((NvU32 *)&msg[size]);
            size += sizeof(NvU32);
            bAllocateBuffers = *((NvBool *)&msg[size]);
            size += sizeof(NvBool);
            status = hBlock->SetBufferAllocator(hBlock,
                                                streamIndex,
                                                bAllocateBuffers);

            break;

        case NVMM_MSG_SetSendBlockEventFunction:
            hBlock->SetSendBlockEventFunction(hBlock,
                                              pBlockSideInfo,
                                              pBlockSideInfo->pEventHandler);
            break;

        case NVMM_MSG_SetState:
            eState = *((NvMMState *)&msg[size]);
            size += sizeof(NvMMState);
            size += sizeof(NvBool);
            status = hBlock->SetState(hBlock, eState);
            break;

        case NVMM_MSG_AbortBuffers:
            streamIndex = *((NvU32 *)&msg[size]);
            size += sizeof(NvU32);
            size += sizeof(NvBool);
            status = hBlock->AbortBuffers(hBlock, streamIndex);
            break;

        case NVMM_MSG_SetAttribute:
            AttributeType = *((NvU32 *)&msg[size]);
            size += sizeof(NvU32);
            AttributeFlag = *((NvU32 *)&msg[size]);
            size += sizeof(NvU32);
            AttributeSize = *((NvU32 *)&msg[size]);
            size += sizeof(NvU32);
            if (AttributeSize)
            {
                pAttribute = pBlockSideInfo->workBuffer;
                NvOsMemcpy(pAttribute, &msg[size], AttributeSize);
                size+= AttributeSize;
                size += sizeof(NvBool);
                status = hBlock->SetAttribute(hBlock,
                                              AttributeType,
                                              AttributeFlag,
                                              AttributeSize,
                                              pAttribute);

                /* Send a SetAttributeError rather than a BlockError in this case */
                if (status != NvSuccess)
                {
                    NvMMSetAttributeErrorInfo ErrorInfo;
                    ErrorInfo.structSize = sizeof(NvMMSetAttributeErrorInfo);
                    ErrorInfo.event = NvMMEvent_SetAttributeError;
                    ErrorInfo.error = status;
                    ErrorInfo.AttributeType = AttributeType;
                    ErrorInfo.AttributeSize = AttributeSize;
                    NV_DEBUG_PRINTF(("Send Set Attribute Error %x\n", status));
                    pBlockSideInfo->pEventHandler(pBlockSideInfo,
                        ErrorInfo.event,
                        ErrorInfo.structSize,
                        &ErrorInfo);
                    status = NvSuccess;
                }
            }
            else
            {
                size += sizeof(NvBool);
            }

            break;

        case NVMM_MSG_GetAttribute:
           {
               NvU32 nAnswer = 0;

               NvOsMutexLock(pBlockSideInfo->TransportApiLock);
               AttributeType = *((NvU32 *)&msg[size]);
               size += sizeof(NvU32);
               AttributeSize = *((NvU32 *)&msg[size]);
               size += sizeof(NvU32);

               nAnswer = hBlock->GetAttribute(hBlock,
                                              AttributeType,
                                              AttributeSize,
                                              &msg[sizeof(NvU32)*3]);

               sendMsg = pBlockSideInfo->sendMsgBuffer;
               *((NvU32 *)&sendMsg[sizeof(NvU32)*0]) = NVMM_MSG_GetAttributeInfo;
               *((NvU32 *)&sendMsg[sizeof(NvU32)*1]) = nAnswer;
               *((NvU32 *)&sendMsg[sizeof(NvU32)*2]) = AttributeSize;
               NvOsMemcpy(&sendMsg[sizeof(NvU32)*3], &msg[sizeof(NvU32)*3], AttributeSize);
               status = BlockSideSendMsg(pBlockSideInfo, sendMsg, AttributeSize + sizeof(NvU32)*3);
               NvOsMutexUnlock(pBlockSideInfo->TransportApiLock);
           break;
           }
        case NVMM_MSG_Extension:
           {
               NvU32 ExtensionIndex;
               NvU32 InputSize;
               NvU32 OutputSize;
               NvU32 nAnswer;
               void *pInputData = NULL;

               // Retrieve input args to extension
               ExtensionIndex = *((NvU32 *)&msg[size]);
               size += sizeof(NvU32);
               InputSize = *((NvU32 *)&msg[size]);
               size += sizeof(NvU32);
               OutputSize = *((NvU32 *)&msg[size]);
               size += sizeof(NvU32);
               if (InputSize)
               {
                   pInputData = pBlockSideInfo->workBuffer;
                   NvOsMemcpy(pInputData, &msg[size], InputSize);
                   size += InputSize;
               }
               // Prepare return packet (output args to extension)
               nAnswer = hBlock->Extension(hBlock,
                                           ExtensionIndex,
                                           InputSize,
                                           pInputData,
                                           OutputSize,
                                           &msg[sizeof(NvU32)*3]);
               NvOsMutexLock(pBlockSideInfo->TransportApiLock);
               sendMsg = pBlockSideInfo->sendMsgBuffer;
               *((NvU32 *)&sendMsg[sizeof(NvU32)*0]) = NVMM_MSG_GetAttributeInfo;
               *((NvU32 *)&sendMsg[sizeof(NvU32)*1]) = nAnswer;
               *((NvU32 *)&sendMsg[sizeof(NvU32)*2]) = OutputSize;
               NvOsMemcpy(&sendMsg[sizeof(NvU32)*3], &msg[sizeof(NvU32)*3], OutputSize);

               status = BlockSideSendMsg(pBlockSideInfo, sendMsg, OutputSize + sizeof(NvU32)*3);
               NvOsMutexUnlock(pBlockSideInfo->TransportApiLock);
           }
           break;

        case NVMM_MSG_TransferBufferToBlock:
            pBuffer= NULL;
            streamIndex = *((NvU32 *)&msg[size]);
            size += sizeof(NvU32);
            BufferType = *((NvMMBufferType *)&msg[size]);
            size += sizeof(NvMMBufferType);
            BufferSize = *((NvU32 *)&msg[size]);
            size += sizeof(NvU32);

            if (BufferSize)
            {
                pBuffer = (NvMMBuffer *)pBlockSideInfo->workBuffer;
                NvOsMemcpy(pBuffer, &msg[size], BufferSize);
                size+= BufferSize;

                TransferBufferToBlock = hBlock->GetTransferBufferFunction(hBlock,
                                                                          streamIndex,
                                                                          NULL);
                status = TransferBufferToBlock(hBlock,
                                               streamIndex,
                                               BufferType,
                                               BufferSize,
                                               pBuffer);
                pBuffer= NULL;
            }
            else
            {
                pBuffer = NULL;
            }

            break;

        case NVMM_MSG_Close:
            {
                size += sizeof(NvBool);
                CloseBlock(pBlockSideInfo);
                pBlockSideInfo->bShutDown = NV_TRUE;
                pBlockSideInfo->pDoWork = NULL;

                status = NvSuccess;
            }
            break;

        default:
            NV_DEBUG_PRINTF((" NvMM_TRANSPORT BLOCK SIDE NvError_BadValue\n"));
            status = NvError_BadValue;
    }

    if (status != NvSuccess)
    {
        NvMMBlockErrorInfo BlockErrorInfo;
        BlockErrorInfo.structSize = sizeof(NvMMBlockErrorInfo);
        BlockErrorInfo.event = NvMMEvent_BlockError;
        BlockErrorInfo.error = status;
        NV_DEBUG_PRINTF(("Send Block Error %x\n", BlockErrorInfo.error));
        pBlockSideInfo->pEventHandler(pBlockSideInfo,
                                NvMMEvent_BlockError,
                                BlockErrorInfo.structSize,
                                &BlockErrorInfo);
    }
    return status;
}

static NvError OpenBlock(BlockSideInfo* pBlockSide)
{
    NvError status = NvError_BadParameter;

    NvError(*open)(NvMMBlockHandle *phBlock,
                   NvMMInternalCreationParameters *pParams,
                   NvOsSemaphoreHandle semaphore,
                   NvMMDoWorkFunction *pDoWorkFunction) = NULL;

#if (NVMM_BUILT_DYNAMIC && !NV_IS_AVP)
    if(pBlockSide->Param.eLocale == NvMMLocale_CPU)
    {
        switch (pBlockSide->Param.eType)
        {
            case NvMMBlockType_DecAAC:
            case NvMMBlockType_DecEAACPLUS:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAacDecOpen");
                break;
            case NvMMBlockType_DecADTS:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAdtsDecOpen");
                break;
            case NvMMBlockType_EncAMRNB:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAmrNBEncOpen");
                break;
            case NvMMBlockType_DecAMRNB:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAmrNBDecOpen");
                break;
            case NvMMBlockType_DecMP3:
            case NvMMBlockType_DecMP3SW:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMMP3DecOpen");
                break;
            case NvMMBlockType_DecMP2:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMMP2DecOpen");
                break;
            case NvMMBlockType_EncAMRWB:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAmrWBEncOpen");
                break;
            case NvMMBlockType_DecAMRWB:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAmrWBDecOpen");
                break;
            case NvMMBlockType_DecWAV:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWavDecOpen");
                break;
            case NvMMBlockType_EncWAV:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWavEncOpen");
                break;
            case NvMMBlockType_AudioMixer:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMMixerOpen");
                break;
            case NvMMBlockType_SinkAudio:
            case NvMMBlockType_SourceAudio:
                break;
            case NvMMBlockType_SrcCamera:
#if NV_IS_SRCCAMERA_ENABLED
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMCameraOpen");
#endif
                break;
            case NvMMBlockType_SrcUSBCamera:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMUsbCameraOpen");
                break;
            case NvMMBlockType_RingTone:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMRingToneOpen");
                break;
            case NvMMBlockType_DigitalZoom:
#if NV_IS_DIGITALZOOM_ENABLED
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMDigitalZoomOpen");
#endif
                break;
            case NvMMBlockType_EncAAC:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAacPlusEncOpen");
                break;
            case NvMMBlockType_DecWMA:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWmaDecOpen");
                break;
            case NvMMBlockType_DecWMAPRO:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWmaproDecOpen");
                break;
            case NvMMBlockType_DecWMALSL:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWmalslDecOpen");
                break;
            case NvMMBlockType_DecWMASUPER:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWmaSuperDecOpen");
                break;
            case NvMMBlockType_DecSUPERAUDIO:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMSuperAudioDecOpen");
                break;
            case NvMMBlockType_DecOGG:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMOGGDecOpen");
                break;
            case NvMMBlockType_SuperParser:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMSuperParserBlockOpen");
                break;
            case NvMMBlockType_3gppWriter:
                open = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMBaseWriterBlockOpen");
                break;
            default:
                break;
        }
        if(open == NULL)
            return status;
    }
#else
/* Clean this: AVP does'nt support getting symbols from library. See Bug: */
/* Clean this: AOS does'nt support dynamic loading/getting symbols from library. See Bug: */

    switch (pBlockSide->Param.eType)
    {
        case NvMMBlockType_DecAAC:
        #if NV_IS_DECAAC_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECAAC_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMAacDecOpen;
            #elif (NV_IS_AVP && (NV_IS_DECAAC_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMAacDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECAAC_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMAacDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECAAC_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMAacDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecADTS:
        #if NV_IS_DECADTS_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECADTS_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMAdtsDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECAAC_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMAdtsDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecEAACPLUS:
        #if NV_IS_DECAACPLUS_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECAACPLUS_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMAacDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECAACPLUS_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMAacDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_EncAMRNB:
        #if NV_IS_ENCAMRNB_ENABLED
            #if (NV_IS_AVP && (NV_IS_ENCAMRNB_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMAmrNBEncOpen;
            #elif (!NV_IS_AVP && (NV_IS_ENCAMRNB_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMAmrNBEncOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecAMRNB:
        #if NV_IS_DECAMRNB_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECAMRNB_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMAmrNBDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECAMRNB_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMAmrNBDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecMP2:
        #if NV_IS_DECMP2_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECMP2_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMMP2DecOpen;
            #elif (NV_IS_AVP && (NV_IS_DECMP2_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMMP2DecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECMP2_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMMP2DecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECMP2_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMMP3DecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecMP3:
        #if NV_IS_DECMP3_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECMP3_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMMP3DecOpen;
            #elif (NV_IS_AVP && (NV_IS_DECMP3_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMMP3DecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECMP3_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMMP3DecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECMP3_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMMP3DecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecMP3SW:
        #if NV_IS_DECMP3SW_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECMP3SW_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMMP3DecOpen;
            #elif (NV_IS_AVP && (NV_IS_DECMP3SW_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMMP3DecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECMP3SW_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMMP3DecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECMP3SW_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMMP3DecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_EncAMRWB:
        #if NV_IS_ENCAMRWB_ENABLED
            #if (NV_IS_AVP && (NV_IS_ENCAMRWB_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMAmrWBEncOpen;
            #elif (!NV_IS_AVP && (NV_IS_ENCAMRWB_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMAmrWBEncOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecAMRWB:
        #if NV_IS_DECAMRWB_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECAMRWB_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMAmrWBDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECAMRWB_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMAmrWBDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecWAV:
        #if NV_IS_DECWAV_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECWAV_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMWavDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECWAV_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMWavDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_EncWAV:
        #if NV_IS_ENCWAV_ENABLED
            #if (NV_IS_AVP && (NV_IS_ENCWAV_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMWavEncOpen;
            #elif (!NV_IS_AVP && (NV_IS_ENCWAV_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMWavEncOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_AudioMixer:
        #if NV_IS_AUDIOMIXER_ENABLED
            #if (NV_IS_AVP && (NV_IS_AUDIOMIXER_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMMixerOpen;
            #elif (!NV_IS_AVP && (NV_IS_AUDIOMIXER_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMMixerOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_SinkAudio:
        case NvMMBlockType_SourceAudio:
            break;
        case NvMMBlockType_SrcCamera:
        #if NV_IS_SRCCAMERA_ENABLED
            #if (NV_IS_AVP && (NV_IS_SRCCAMERA_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMCameraOpen;
            #elif (!NV_IS_AVP && (NV_IS_SRCCAMERA_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMCameraOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_SrcUSBCamera:
        #if NV_IS_SRCUSBCAMERA_ENABLED
            #if (NV_IS_AVP && (NV_IS_SRCUSBCAMERA_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMUsbCameraOpen;
            #elif (!NV_IS_AVP && (NV_IS_SRCUSBCAMERA_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMUsbCameraOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_RingTone:
        #if NV_IS_RINGTONE_ENABLED
            #if (NV_IS_AVP && (NV_IS_RINGTONE_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMRingToneOpen;
            #elif (!NV_IS_AVP && (NV_IS_RINGTONE_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMRingToneOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DigitalZoom:
        #if NV_IS_DIGITALZOOM_ENABLED
            #if (NV_IS_AVP && (NV_IS_DIGITALZOOM_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMDigitalZoomOpen;
            #elif (!NV_IS_AVP && (NV_IS_DIGITALZOOM_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMDigitalZoomOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_EncAAC:
        #if NV_IS_ENCAAC_ENABLED
            #if (NV_IS_AVP && (NV_IS_ENCAAC_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMAacPlusEncOpen;
            #elif (!NV_IS_AVP && (NV_IS_ENCAAC_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMAacPlusEncOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecWMA:
        #if NV_IS_DECWMA_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECWMA_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMWmaDecOpen;
            #elif (NV_IS_AVP && (NV_IS_DECWMA_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMWmaDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECWMA_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMWmaDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECWMA_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMWmaDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecWMAPRO:
        #if NV_IS_DECWMAPRO_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECWMAPRO_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMWmaproDecOpen;
            #elif (NV_IS_AVP && (NV_IS_DECWMAPRO_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMWmaproDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECWMAPRO_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMWmaproDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECWMAPRO_LOCALE == NV_BLOCK_LOCALE_ALL))
            open = NvMMWmaproDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecWMALSL:
        #if NV_IS_DECWMALSL_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECWMALSL_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMWmalslDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECWMALSL_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMWmalslDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecWMASUPER:
        #if NV_IS_DECWMASUPER_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECWMASUPER_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMWmaSuperDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECWMASUPER_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMWmaSuperDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecSUPERAUDIO:
        #if NV_IS_DECSUPERAUDIO_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECSUPERAUDIO_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMSuperAudioDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECSUPERAUDIO_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMSuperAudioDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_DecOGG:
        #if NV_IS_DECOGG_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECOGG_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMOGGDecOpen;
            #elif (!NV_IS_AVP && (NV_IS_DECOGG_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMOGGDecOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_SuperParser:
        #if NV_IS_SUPERPARSER_ENABLED
            #if (NV_IS_AVP && (NV_IS_SUPERPARSER_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMSuperParserBlockOpen;
            #elif (!NV_IS_AVP && (NV_IS_SUPERPARSER_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMSuperParserBlockOpen;
            #endif
        #endif
            break;
        case NvMMBlockType_3gppWriter:
        #if NV_IS_3GPWRITER_ENABLED
            #if (NV_IS_AVP && (NV_IS_3GPWRITER_LOCALE == NV_BLOCK_LOCALE_AVP))
            open = NvMMBaseWriterBlockOpen;
            #elif (!NV_IS_AVP && (NV_IS_3GPWRITER_LOCALE == NV_BLOCK_LOCALE_CPU))
            open = NvMMBaseWriterBlockOpen;
            #endif
        #endif
            break;
        default:
            break;
    }
#endif
    if (open)
    {
        NvMMInternalCreationParameters internalParams;
        NvOsMemset(&internalParams, 0, sizeof(internalParams));
        NvOsStrncpy(internalParams.InstanceName, pBlockSide->Param.blockName, NVMM_BLOCKNAME_LENGTH);
        internalParams.Locale = pBlockSide->Param.eLocale;
        internalParams.hService = pBlockSide->Param.hAVPService;
        internalParams.BlockSpecific = pBlockSide->Param.BlockSpecific;
        internalParams.SetULPMode = pBlockSide->Param.SetULPMode;
        status = open(&pBlockSide->hBlock, &internalParams, pBlockSide->Param.BlockEventSema, &pBlockSide->pDoWork);
    }

    return status;
}

static void CloseBlock(BlockSideInfo *pBlockSide)
{

    void(*close)(NvMMBlockHandle hBlock,
                  NvMMInternalDestructionParameters *pParams)= NULL;

#if (NVMM_BUILT_DYNAMIC && !NV_IS_AVP)
    if(pBlockSide->Param.eLocale == NvMMLocale_CPU)
    {
        switch (pBlockSide->Param.eType)
        {
            case NvMMBlockType_DecAAC:
            case NvMMBlockType_DecEAACPLUS:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAacDecClose");
                break;
            case NvMMBlockType_DecADTS:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAdtsDecClose");
                break;
            case NvMMBlockType_EncAMRNB:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAmrNBEncClose");
                break;
            case NvMMBlockType_DecAMRNB:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAmrNBDecClose");
                break;
            case NvMMBlockType_DecMP2:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMMP2DecClose");
                break;
            case NvMMBlockType_DecMP3:
            case NvMMBlockType_DecMP3SW:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMMP3DecClose");
                break;
            case NvMMBlockType_DecJPEG:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMJpegDecBlockClose");
                break;
            case NvMMBlockType_EncAMRWB:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAmrWBEncClose");
                break;
            case NvMMBlockType_DecAMRWB:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAmrWBDecClose");
                break;
            case NvMMBlockType_DecWAV:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWavDecClose");
                break;
            case NvMMBlockType_EncWAV:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWavEncClose");
                break;
            case NvMMBlockType_SrcCamera:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMCameraClose");
                break;
            case NvMMBlockType_SrcUSBCamera:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMUsbCameraClose");
                break;
            case NvMMBlockType_AudioMixer:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMMixerClose");
                break;
            case NvMMBlockType_SinkAudio:
            case NvMMBlockType_SourceAudio:
                break;
            case NvMMBlockType_RingTone:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMRingToneClose");
                break;
            case NvMMBlockType_DigitalZoom:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMDigitalZoomClose");
                break;
            case NvMMBlockType_EncAAC:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMAacPlusEncClose");
                break;
            case NvMMBlockType_DecWMA:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWmaDecClose");
                break;
            case NvMMBlockType_DecWMAPRO:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWmaproDecClose");
                break;
            case NvMMBlockType_DecWMALSL:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWmalslDecClose");
                break;
            case NvMMBlockType_DecWMASUPER:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMWmaSuperDecClose");
                break;
            case NvMMBlockType_DecSUPERAUDIO:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMSuperAudioDecClose");
                break;
            case NvMMBlockType_DecOGG:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMOGGDecClose");
                break;
            case NvMMBlockType_SuperParser:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMSuperParserBlockClose");
                break;
            case NvMMBlockType_3gppWriter:
                close = NvOsLibraryGetSymbol(pBlockSide->Param.hLibraryHandle, "NvMMBaseWriterBlockClose");
                break;
            default:
                break;
        }
        if(close == NULL)
            return;
    }

#else
/* Clean this: AVP does'nt support getting symbols from library. See Bug: */
/* Clean this: AOS does'nt support dynamic loading/getting symbols from library. See Bug: */
    switch (pBlockSide->Param.eType)
    {
        case NvMMBlockType_DecAAC:
        #if NV_IS_DECAAC_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECAAC_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMAacDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECAAC_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMAacDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecADTS:
        #if NV_IS_DECADTS_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECADTS_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMAdtsDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECADTS_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMAdtsDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecEAACPLUS:
        #if NV_IS_DECAACPLUS_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECAACPLUS_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMAacDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECAACPLUS_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMAacDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_EncAMRNB:
        #if NV_IS_ENCAMRNB_ENABLED
            #if (NV_IS_AVP && (NV_IS_ENCAMRNB_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMAmrNBEncClose;
            #elif (!NV_IS_AVP && (NV_IS_ENCAMRNB_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMAmrNBEncClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecAMRNB:
        #if NV_IS_DECAMRNB_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECAMRNB_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMAmrNBDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECAMRNB_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMAmrNBDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecMP2:
        #if NV_IS_DECMP2_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECMP2_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMMP2DecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECMP2_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMMP2DecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecMP3:
        #if NV_IS_DECMP3_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECMP3_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMMP3DecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECMP3_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMMP3DecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecMP3SW:
        #if NV_IS_DECMP3SW_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECMP3SW_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMMP3DecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECMP3SW_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMMP3DecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_EncAMRWB:
        #if NV_IS_ENCAMRWB_ENABLED
            #if (NV_IS_AVP && (NV_IS_ENCAMRWB_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMAmrWBEncClose;
            #elif (!NV_IS_AVP && (NV_IS_ENCAMRWB_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMAmrWBEncClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecAMRWB:
        #if NV_IS_DECAMRWB_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECAMRWB_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMAmrWBDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECAMRWB_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMAmrWBDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecWAV:
        #if NV_IS_DECWAV_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECWAV_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMWavDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECWAV_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMWavDecClose;
            #endif
        #endif
        case NvMMBlockType_EncWAV:
        #if NV_IS_ENCWAV_ENABLED
            #if (NV_IS_AVP && (NV_IS_ENCWAV_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMWavEncClose;
            #elif (!NV_IS_AVP && (NV_IS_ENCWAV_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMWavEncClose;
            #endif
        #endif
            break;
        case NvMMBlockType_AudioMixer:
        #if NV_IS_AUDIOMIXER_ENABLED
            #if (NV_IS_AVP && (NV_IS_AUDIOMIXER_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMMixerClose;
            #elif (!NV_IS_AVP && (NV_IS_AUDIOMIXER_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMMixerClose;
            #endif
        #endif
        case NvMMBlockType_SinkAudio:
        case NvMMBlockType_SourceAudio:
            break;
        case NvMMBlockType_SrcCamera:
        #if NV_IS_SRCCAMERA_ENABLED
            #if (NV_IS_AVP && (NV_IS_SRCCAMERA_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMCameraClose;
            #elif (!NV_IS_AVP && (NV_IS_SRCCAMERA_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMCameraClose;
            #endif
        #endif
            break;
        case NvMMBlockType_SrcUSBCamera:
        #if NV_IS_SRCUSBCAMERA_ENABLED
            #if (NV_IS_AVP && (NV_IS_SRCUSBCAMERA_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMUsbCameraClose;
            #elif (!NV_IS_AVP && (NV_IS_SRCUSBCAMERA_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMUsbCameraClose;
            #endif
        #endif
            break;
        case NvMMBlockType_RingTone:
        #if NV_IS_RINGTONE_ENABLED
            #if (NV_IS_AVP && (NV_IS_RINGTONE_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMRingToneClose;
            #elif (!NV_IS_AVP && (NV_IS_RINGTONE_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMRingToneClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DigitalZoom:
        #if NV_IS_DIGITALZOOM_ENABLED
            #if (NV_IS_AVP && (NV_IS_DIGITALZOOM_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMDigitalZoomClose;
            #elif (!NV_IS_AVP && (NV_IS_DIGITALZOOM_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMDigitalZoomClose;
            #endif
        #endif
            break;
        case NvMMBlockType_EncAAC:
        #if NV_IS_ENCAAC_ENABLED
            #if (NV_IS_AVP && (NV_IS_ENCAAC_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMAacPlusEncClose;
            #elif (!NV_IS_AVP && (NV_IS_ENCAAC_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMAacPlusEncClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecWMA:
        #if NV_IS_DECWMA_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECWMA_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMWmaDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECWMA_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMWmaDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecWMAPRO:
        #if NV_IS_DECWMAPRO_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECWMAPRO_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMWmaproDecClose;
            #elif (NV_IS_AVP && (NV_IS_DECWMAPRO_LOCALE == NV_BLOCK_LOCALE_ALL))
            close = NvMMWmaproDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECWMAPRO_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMWmaproDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECWMAPRO_LOCALE == NV_BLOCK_LOCALE_ALL))
            close = NvMMWmaproDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecWMALSL:
        #if NV_IS_DECWMALSL_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECWMALSL_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMWmalslDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECWMALSL_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMWmalslDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecWMASUPER:
        #if NV_IS_DECWMASUPER_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECWMASUPER_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMWmaSuperDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECWMASUPER_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMWmaSuperDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecSUPERAUDIO:
        #if NV_IS_DECSUPERAUDIO_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECSUPERAUDIO_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMSuperAudioDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECSUPERAUDIO_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMSuperAudioDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_DecOGG:
        #if NV_IS_DECOGG_ENABLED
            #if (NV_IS_AVP && (NV_IS_DECOGG_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMOGGDecClose;
            #elif (!NV_IS_AVP && (NV_IS_DECOGG_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMOGGDecClose;
            #endif
        #endif
            break;
        case NvMMBlockType_SuperParser:
        #if NV_IS_SUPERPARSER_ENABLED
            #if (NV_IS_AVP && (NV_IS_SUPERPARSER_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMSuperParserBlockClose;
            #elif (!NV_IS_AVP && (NV_IS_SUPERPARSER_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMSuperParserBlockClose;
            #endif
        #endif
            break;
        case NvMMBlockType_3gppWriter:
        #if NV_IS_3GPWRITER_ENABLED
            #if (NV_IS_AVP && (NV_IS_3GPWRITER_LOCALE == NV_BLOCK_LOCALE_AVP))
            close = NvMMBaseWriterBlockClose;
            #elif (!NV_IS_AVP && (NV_IS_3GPWRITER_LOCALE == NV_BLOCK_LOCALE_CPU))
            close = NvMMBaseWriterBlockClose;
            #endif
        #endif
            break;
        default:
            break;
    }
#endif
    if(close)
    {
        close(pBlockSide->hBlock, NULL);
    }
}

NvError SendEventHandler(
    void *pContext,
    NvU32 EventType,
    NvU32 EventInfoSize,
    void *pEventInfo)
{
    NvU8* msg;
    NvError status = NvSuccess;
    NvU32 size = 0;
    BlockSideInfo *pBlockSide = (BlockSideInfo *)pContext;

    NvOsMutexLock(pBlockSide->TransportApiLock);

    msg = pBlockSide->sendMsgBuffer;
    *((NvU32 *)&msg[size]) = NVMM_MSG_SendEvent;
    size += sizeof(NvU32);
    *((NvU32 *)&msg[size]) = EventType;
    size += sizeof(NvU32);
    *((NvU32 *)&msg[size]) = EventInfoSize;
    size += sizeof(NvU32);

    if (pEventInfo)
    {
        NvOsMemcpy(&msg[size], pEventInfo, EventInfoSize);
        size+= EventInfoSize;
    }
    else
    {
        *((NvU32 *)&msg[size]) = 0;
        size += sizeof(NvU32);
    }

    *((NvBool *)&msg[size]) = 0;
    size += sizeof(NvBool);
    status = BlockSideSendMsg(pBlockSide, msg, size);

    NvOsMutexUnlock(pBlockSide->TransportApiLock);
    return status;
}

NvError
TransferBufferEventHandler(
    void *pContext,
    NvU32 StreamIndex,
    NvMMBufferType BufferType,
    NvU32 BufferSize,
    void *pBuffer)
{
    NvU8* msg;
    NvError status = NvSuccess;
    NvU32 size = 0;
    BlockSideInfo *pBlockSide = (BlockSideInfo *)pContext;

    NvOsMutexLock(pBlockSide->TransportApiLock);

    msg = pBlockSide->sendMsgBuffer;
    size = 0;

    *((NvU32 *)&msg[size]) = NVMM_MSG_TransferBufferFromBlock;
    size += sizeof(NvU32);
    *((NvU32 *)&msg[size]) = StreamIndex;
    size += sizeof(NvU32);
    *((NvU32 *)&msg[size]) = BufferType;
    size += sizeof(NvU32);
    *((NvU32 *)&msg[size]) = BufferSize;
    size += sizeof(NvU32);
    // this is to ensure the buffer size send should not be more than 256 bytes
    NV_ASSERT(BufferSize <= MSG_BUFFER_SIZE);
    if (pBuffer)
    {
        NvOsMemcpy(&msg[size], pBuffer, BufferSize);
        size += BufferSize;
    }
    else
    {
        *((NvU32 *)&msg[size]) = 0;
        size += sizeof(NvU32);
    }
    status = BlockSideSendMsg(pBlockSide, msg, size);

    NvOsMutexUnlock(pBlockSide->TransportApiLock);
    return status;
}


NvError BlockSideSendMsg(BlockSideInfo *pBlockSide, NvU8* msg, NvU32 size)
{
    NvError status = NvSuccess;

    NV_LOGGER_PRINT((NVLOG_NVMM_TRANSPORT, NVLOG_VERBOSE,
        "Block %s send msg: %p of %s\n", pBlockSide->Param.blockName, msg, DecipherMsgCode(msg)));

    status = NvMMQueueEnQ(pBlockSide->Param.ClientMsgQueue, (void *)msg, 0);
    if (status == NvSuccess)
        NvOsSemaphoreSignal(pBlockSide->Param.ClientEventSema);

    if (status != NvSuccess)
    {
        NV_LOGGER_PRINT((NVLOG_NVMM_TRANSPORT, NVLOG_ERROR,
            "BlockSideSendMsg - Message dropped. NvMMQueue likely overflowed."));

        NV_ASSERT(!"Message dropped. NvMMQueue likely overflowed.\n");
    }

    return status;
}

NvError BlockSideRcvMsg(BlockSideInfo *pBlockSide, NvU8* msg)
{
    NvError status = NvError_Timeout;

    if (pBlockSide->Param.BlockMsgQueue &&
        NvMMQueueGetNumEntries(pBlockSide->Param.BlockMsgQueue))
    {
        status = NvMMQueueDeQ(pBlockSide->Param.BlockMsgQueue, (void *)msg);
    }

    if (status == NvSuccess)
        NV_LOGGER_PRINT((NVLOG_NVMM_TRANSPORT, NVLOG_VERBOSE,
            "Block %s received msg: %p of %s\n", pBlockSide->Param.blockName, msg, DecipherMsgCode(msg)));

    return status;
}

NvError BlockSideShutDown(BlockSideInfo *pBlockSideInfo)
{
    NvError status = NvSuccess;
    NvMMBlockCloseInfo BlockCloseInfo;

    NvOsMemset(&BlockCloseInfo, 0, sizeof(NvMMBlockCloseInfo));
    NV_DEBUG_PRINTF(("In BlockSideShutDown \n"));

    if(pBlockSideInfo->bAbnormalTermination == NV_FALSE)
    {
        BlockCloseInfo.structSize = sizeof(NvMMBlockCloseInfo);
        BlockCloseInfo.blockClosed = NV_TRUE;
        pBlockSideInfo->pEventHandler(pBlockSideInfo,
                                NvMMEvent_BlockClose,
                                BlockCloseInfo.structSize,
                                &BlockCloseInfo);
    }

#if NVMM_KPILOG_ENABLED
    if (pBlockSideInfo->pNvMMStat &&
        pBlockSideInfo->profileStatus & NvMMProfile_BlockTransport)
    {
        NvMMStatsDestroy(pBlockSideInfo->pNvMMStat);
    }

    if (pBlockSideInfo->pNvMMAVPProfile &&
        pBlockSideInfo->profileStatus & NvMMProfile_AVPKernel)
    {
        NvMMAVPProfileDestroy(pBlockSideInfo->pNvMMAVPProfile);
    }
#endif
    NvOsSemaphoreDestroy(pBlockSideInfo->Param.BlockEventSema);
    NvOsMutexDestroy(pBlockSideInfo->TransportApiLock);
    NvOsFree(pBlockSideInfo);

    return status;
}

