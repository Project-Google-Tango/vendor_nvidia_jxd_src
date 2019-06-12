/* Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an
* express license agreement from NVIDIA Corporation is strictly prohibited.
*/

/** 
*
*/

#ifndef NV_ENABLE_DEBUG_PRINTS 
#define NV_ENABLE_DEBUG_PRINTS (0) 
#endif

#include "nvmm_contentpipe.h"
#include "nvrm_init.h"
#include "nvrm_memmgr.h"
#include "nvrm_power.h"
#include "nvos.h"
#include "nvmm_ulp_kpi_logger.h"
#include "nvmm_customprotocol_internal.h"
#include "nvmm_logger.h"

#define NVLOG_CLIENT NVLOG_CONTENT_PIPE // required for logging contentpipe traces

#define MAX_ROWS_READBUFFER_TABLE 100
#define MAX_COLUMNS_READBUFFER_TABLE (1 * 1024)
#define LOW_CHUNKSIZE_THRESHOLD (256 * 1024)

#define STREAMING_CHUNK_SIZE (32 * 1024)
#define STREAMING_MIN_CACHE_SIZE (10 * 1024 * 1024)
#define STREAMING_MIN_SPARE_SIZE (256 * 1024)

#define KPI_MODE_ENABLED 0
#define NVMM_CP_PROFILE_ENABLED 0
#define MAX_PROFILE_ENTRIES (40 * 1024)

static CPresult Nvmm_FileSetPosition64( CPhandle  hContent, CPint64 nOffset, CP_ORIGINTYPE eOrigin);
static CPresult Nvmm_FileGetPosition64( CPhandle hContent, CPuint64 *pPosition);
static CPresult Nvmm_InvalidateCache(CPhandle hContent);
static CPresult Nvmm_PauseCaching(CPhandle hContent, CPbool bPause);

enum FileChunkStatus
{
    INVALID = 0,
    FILLING = 1,
    FULL = 2
};

typedef struct FileChunkRec 
{
    NvU8                *pBuffer;
    NvU32               size;
    NvU32               status;
    NvBool              bInvalidate;
    NvU64               position;
}FileChunk;

typedef struct ReadBufferRec
{
    NvU8                *pBuffer;
    NvU32               size;
    NvBool              bInUse;
}ReadBuffer;

typedef struct CPProfRec
{
    NvU32   count;
    NvU64   StartTime[MAX_PROFILE_ENTRIES];
    NvU64   EndTime[MAX_PROFILE_ENTRIES];
    NvU64   SeekPosition[MAX_PROFILE_ENTRIES];
    NvU32   nBytesRead[MAX_PROFILE_ENTRIES];
} CPProf;

typedef struct NvmmFileCacheRec
{
    NvRmDeviceHandle    hRmDevice;
    NvRmMemHandle       hMem;
    NvRmPhysAddr        PhyAddress;
    void*               VirtAddress;

    NvOsSemaphoreHandle WorkerSema;
    NvOsMutexHandle     WriteLock;
    NvOsMutexHandle     AvailLock;
    NvOsMutexHandle     ChunkLock;
    NvOsThreadHandle    WorkerThread;

    NvU64               CacheSize;
    NvU64               ReadTriggerThreshold;
    NvU64               HighMark;

    NvU64               FileSize;

    FileChunk           *FileChunks;
    NvU32               TotalFileChunks;
    NvU32               NextChunkToWrite;
    NvU32               ChunkSize;

    ReadBuffer          **pReadBufferTable;
    NvU32               nRowsInReadBufferTable;

    NvU8                *pBits;
    NvU8                *pWrite;
    NvU8                *pRead;
    NvU32               nBytesAvailable;

    void                *pClientContext;
    ClientCallbackFunction ClientCallback;

    NvU8                *pSpareBits; // used when requested area wraps around end of pool
    NvU64               SpareCacheSize;
    NvBool              bSpareInUse;

    NvBool              bClientWaiting;

    NvBool              bEOS;
    NvBool              bShutDown;
    NvBool              bTrigger;
    NvBool              bInitialized;
    NvBool              bProfile;

    CPProf              *pProfData;
    NvU32               nReadBuffersInUse;

    NV_CUSTOM_PROTOCOL  *pProtocol;
    NvCPRHandle         hFile;
    NvBool              bIsStreaming;
    NvBool              bInvalidateCache;
    NvS32               nProtocolVersion;

    NvBool              bCachingPaused;
    NvBool              bStop;
} NvmmFileCache;

static NvBool CheckFileChunkInUse(NvmmFileCache *pFileCache, FileChunk *pFileChunk)
{
    NvU8 *pWriteStart, *pWriteEnd;
    NvU8 *pReadStart, *pReadEnd;
    NvU32 i = 0, j = 0;

    pWriteStart = pFileChunk->pBuffer;
    pWriteEnd = pFileChunk->pBuffer + pFileChunk->size;

    if (pFileCache->nReadBuffersInUse > 0)
    {
        for (i = 0; i < pFileCache->nRowsInReadBufferTable;i++)
        {
            for (j = 0; j < MAX_COLUMNS_READBUFFER_TABLE; j++)
            {
                if (pFileCache->pReadBufferTable[i][j].bInUse)
                {
                    pReadStart = pFileCache->pReadBufferTable[i][j].pBuffer;
                    pReadEnd = pReadStart + pFileCache->pReadBufferTable[i][j].size;
                    // do buffers intersect ?
                    if ((pReadStart < pWriteEnd) && (pWriteStart < pReadEnd))
                    {
                        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "CheckFileChunkInUse::Read and write buffers intersect pReadStart = %x\tpReadEnd = %x\tpWRiteStart %x\tpWriteEnd = %x\n", pReadStart, pReadEnd, pWriteStart, pWriteEnd));
                        return NV_TRUE;
                    }
                }
            }
        }
        if ((i >= pFileCache->nRowsInReadBufferTable) && (j >= MAX_COLUMNS_READBUFFER_TABLE))
        {
            return NV_FALSE;
        }
    }
    return NV_FALSE;
}


static NvError PrepareForProcessing(NvmmFileCache *pFileCache, FileChunk **pFileChunk)
{
    NvU32 i;
    NvU8 *pWriteStart, *pWriteEnd;

    *pFileChunk = NULL;

    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "++PrepareForProcessing \n"));

    if(pFileCache->bEOS)
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"--PrepareForProcessing::bEOS\n"));
        return NvError_ContentPipeEndOfStream;
    }

    if (pFileCache->CacheSize == pFileCache->FileSize &&
        !pFileCache->bIsStreaming)
    {
        NvOsMutexLock(pFileCache->AvailLock);
        for(i = pFileCache->NextChunkToWrite; i <pFileCache->TotalFileChunks; i++ )
        {
            if (pFileCache->FileChunks[i].status == INVALID)
            {
                pFileCache->NextChunkToWrite = i;
                break;
            }
            if ((pFileCache->FileChunks[i].status == FULL) && (pFileCache->FileChunks[i].bInvalidate))
            {
                if (!CheckFileChunkInUse(pFileCache, &pFileCache->FileChunks[i]))
                {
                    pFileCache->NextChunkToWrite = i;
                    break;
                }
                else
                {
                    NvOsMutexUnlock(pFileCache->AvailLock);
                    return NvError_ContentPipeNotInvalidated;
                }
            }
        }
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "PrepareForProcessing::NextChunkToWrite = %x\n", pFileCache->NextChunkToWrite));
        NvOsMutexUnlock(pFileCache->AvailLock);
        if (!pFileCache->bInvalidateCache)
        {
            if (i == pFileCache->TotalFileChunks)
            {
                pFileCache->bEOS = NV_TRUE;
                pFileChunk = NULL;
                if (pFileCache->bClientWaiting &&
                    pFileCache->pClientContext &&
                    pFileCache->ClientCallback)
                {
                    pFileCache->ClientCallback(pFileCache->pClientContext, CP_BytesAvailable, 0);
                }
                NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"--PrepareForProcessing::NvError_ContentPipeEndOfStream:nBytesAvailable:%x\n", pFileCache->nBytesAvailable));
                return NvError_ContentPipeEndOfStream;
            }
        }
    } 

    // check for read buffers in use in the next write buffer
    pWriteStart = pFileCache->pBits + pFileCache->NextChunkToWrite*pFileCache->ChunkSize;
    pWriteEnd = pWriteStart + pFileCache->ChunkSize;
    if (pFileCache->nBytesAvailable && 
        (pWriteStart <= pFileCache->pRead) && 
        (pFileCache->pRead < pWriteEnd)) 
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "--PrepareForProcessing::Read buffer in use in the next write pRead = %x\tpWriteStart = %x\tpWriteEnd = %x\n", pFileCache->pRead, pWriteStart, pWriteEnd));
        return NvError_ContentPipeNotReady;
    }
    // check if cache is full
    if (pFileCache->nBytesAvailable >= pFileCache->TotalFileChunks*pFileCache->ChunkSize)
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "--PrepareForProcessing::Cache is full\n"));
        return NvError_ContentPipeEndOfStream;
    }

    if (CheckFileChunkInUse(pFileCache, &pFileCache->FileChunks[pFileCache->NextChunkToWrite]))
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "--PrepareForProcessing::CheckFileChunkInUse failed"));
        return NvError_ContentPipeNotReady; // then we have to wait until read buffer is freed
    }

    // this block is ready for writing
    pFileCache->FileChunks[pFileCache->NextChunkToWrite].status = FILLING;
    *pFileChunk = &pFileCache->FileChunks[pFileCache->NextChunkToWrite];

   NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "--PrepareForProcessing::NextChunkToWrite = %x\n", pFileCache->NextChunkToWrite));

    return NvSuccess; 
}

static NvError NvmmFileCacheDoRead(NvmmFileCache *pFileCache)
{
    NvU32 nBytesRead;
    FileChunk *pFileChunk;
    NvU32 i,j;
    NvU32 index;
    CPProf *pProf = NULL;
    NvError e = NvSuccess;

#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    if ((pFileCache->CacheSize < pFileCache->FileSize) &&
        (pFileCache->nBytesAvailable > pFileCache->HighMark))
    {
        pFileCache->bTrigger = NV_FALSE;
        goto  endOfFunction;
    }

    NvOsMutexLock(pFileCache->ChunkLock);

    e = PrepareForProcessing(pFileCache, &pFileChunk);
    if (e)
    {
        pFileCache->bTrigger = NV_FALSE;
        NvOsMutexUnlock(pFileCache->ChunkLock);
        goto  endOfFunction;
    }

    if (pFileCache->CacheSize == pFileCache->FileSize &&
        !pFileCache->bIsStreaming)
    {
        e = pFileCache->pProtocol->SetPosition(pFileCache->hFile, pFileChunk->position, NvCPR_OriginBegin);
    }
    else
    {
        e = pFileCache->pProtocol->GetPosition(pFileCache->hFile, &pFileChunk->position);
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "Reading from file location %lld\n", pFileChunk->position));
        if (pFileChunk->position == pFileCache->FileSize)
        {
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_INFO, "NvMMContentPipe GetPosition EOF"));
            pFileCache->bEOS = NV_TRUE;
            pFileChunk->status = INVALID;
            if (pFileCache->bClientWaiting &&
                pFileCache->pClientContext &&
                pFileCache->ClientCallback)
            {
                pFileCache->ClientCallback(pFileCache->pClientContext, CP_BytesAvailable, 0);
            }
            NvOsMutexUnlock(pFileCache->ChunkLock);
            goto  endOfFunction;
        }
    }
    if (e != NvSuccess)
    {
        pFileChunk->status = INVALID;
        NvOsMutexUnlock(pFileCache->ChunkLock);
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_ERROR, "NvMMContentPipe Read Failed! NvError = 0x%08x", e));        
        goto  endOfFunction;
    }

    nBytesRead = 0;
    if ((pFileCache->FileSize - pFileChunk->position) < pFileCache->ChunkSize)
    {
        pFileChunk->size = (NvU32)(pFileCache->FileSize - pFileChunk->position);
    }
    else
        pFileChunk->size = pFileCache->ChunkSize;

    NvOsMutexUnlock(pFileCache->ChunkLock);

#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_ReadStart, NV_FALSE, NV_FALSE, nBytesRead);
#endif
    if (pFileCache->bProfile)
    {
        pProf = pFileCache->pProfData;
        index = pProf->count;
        pProf->SeekPosition[index] = pFileChunk->position;
        pProf->nBytesRead[index] = pFileChunk->size;
        pProf->StartTime[index] = NvOsGetTimeUS();
    }

    nBytesRead = pFileCache->pProtocol->Read(pFileCache->hFile, pFileChunk->pBuffer, pFileChunk->size);

    if (pFileChunk->size && !nBytesRead)
        e = NvError_EndOfFile;
    else 
        e = NvSuccess;

    if (pFileCache->bProfile)
    {
        pProf = pFileCache->pProfData;
        index = pProf->count;
        pProf->EndTime[index] = NvOsGetTimeUS();
        pProf->count++;
        if (pProf->count >= MAX_PROFILE_ENTRIES)
        {
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_WARN,"Profiling memory exhausted, CP profiling disabled\n"));
            pFileCache->bProfile = NV_FALSE;
        }
    }
#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_ReadEnd, NV_FALSE, NV_FALSE, nBytesRead);
#endif

    NvOsMutexLock(pFileCache->ChunkLock);

    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"Thread e = %x\tnBytesRead = %x\n", e, nBytesRead));
    if (nBytesRead)
    {
        pFileChunk->status = FULL;
        pFileChunk->bInvalidate = NV_FALSE;

        NvOsMutexLock(pFileCache->AvailLock);
        if (pFileCache->CacheSize == pFileCache->FileSize &&
            !pFileCache->bIsStreaming)
        {
            // check where the read pointer is to calculate nBytesAvailable
            for (i = 0; i<pFileCache->TotalFileChunks; i++)
            {
                if ((pFileCache->FileChunks[i].pBuffer <= pFileCache->pRead) &&
                    (pFileCache->pRead < pFileCache->FileChunks[i].pBuffer + pFileCache->FileChunks[i].size))
                {
                    break;
                }
                // if the Read pointer is at the end of the file
                if (i == pFileCache->TotalFileChunks - 1)
                {
                    if (pFileCache->pRead <= pFileCache->FileChunks[i].pBuffer + pFileCache->FileChunks[i].size)
                        break;
                }
            }
            pFileCache->nBytesAvailable = pFileCache->FileChunks[i].pBuffer + pFileCache->FileChunks[i].size - pFileCache->pRead;
            for(j = i+1; j <pFileCache->TotalFileChunks; j++ )
            {
                if ((pFileCache->FileChunks[j].status == FULL) && (!pFileCache->FileChunks[j].bInvalidate))
                {
                    pFileCache->nBytesAvailable += pFileCache->FileChunks[j].size;
                    if (j == pFileCache->TotalFileChunks-1)
                    {
                        pFileCache->bEOS = NV_TRUE;
                    }
                }
                else
                    break;
            }
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"nBytesAvailable = %x\n",pFileCache->nBytesAvailable));
        } 
        else
        {
            pFileCache->nBytesAvailable += nBytesRead;
            if (pFileChunk->size == (pFileCache->FileSize - pFileChunk->position))
                pFileCache->bEOS = NV_TRUE;
        }
            
        NvOsMutexUnlock(pFileCache->AvailLock);

        // increment write pointer
        pFileCache->pWrite = pFileChunk->pBuffer + nBytesRead;
        if (pFileCache->pWrite == (pFileCache->pBits + pFileCache->TotalFileChunks*pFileCache->ChunkSize))
            pFileCache->pWrite = pFileCache->pBits;
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"size = %x\tpWrite = %x\n", pFileChunk->size, pFileCache->pWrite));

        if (pFileCache->bClientWaiting &&
            pFileCache->pClientContext &&
            pFileCache->ClientCallback)
        {
            pFileCache->ClientCallback(pFileCache->pClientContext, CP_BytesAvailable, 0);
        }
    }
    else
    {
        pFileChunk->status = INVALID;
        if (e == NvError_EndOfFile)
        {
            pFileCache->bEOS = NV_TRUE;
            if (pFileCache->bClientWaiting &&
                pFileCache->pClientContext &&
                pFileCache->ClientCallback)
            {
                pFileCache->ClientCallback(pFileCache->pClientContext, CP_BytesAvailable, 0);
            }
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_INFO, "NvMMContentPipe Read EOF"));   
        }
        else
        {
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_ERROR, "NvMMContentPipe Read Failed! NvError = 0x%08x", e));
        }
    }

    // Signal the caching thread to continue if not EOS 
    if (!pFileCache->bEOS)
    {
        // for a full file in cache always signal
        if (pFileCache->CacheSize == pFileCache->FileSize && 
            !pFileCache->bIsStreaming)
        {
            NvOsSemaphoreSignal(pFileCache->WorkerSema);
        }
        else
        {
            // for a partially cached file check for the highmark limit
            if (pFileCache->nBytesAvailable < pFileCache->HighMark)
            {
                NvOsSemaphoreSignal(pFileCache->WorkerSema);
            }
            else
            {
               //NvOsDebugPrintf("Pause the server");
               Nvmm_PauseCaching(pFileCache, NV_TRUE);
            }
        }
    }

    pFileCache->NextChunkToWrite++;
    if (pFileCache->NextChunkToWrite == pFileCache->TotalFileChunks)
    {
        pFileCache->NextChunkToWrite = 0;
    }

    NvOsMutexUnlock(pFileCache->ChunkLock);

endOfFunction:
#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return e;

}

static void NvmmFileCacheThread(void *arg)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)arg;
    int sleeptime = pFileCache->bIsStreaming ? 5 : 1;

    while (!pFileCache->bShutDown)
    {
        NvOsSemaphoreWait(pFileCache->WorkerSema);

        NvOsMutexLock(pFileCache->WriteLock);
        if (!pFileCache->bShutDown && !pFileCache->bCachingPaused && !pFileCache->bStop)
            NvmmFileCacheDoRead(pFileCache);
        NvOsMutexUnlock(pFileCache->WriteLock);

        NvOsSleepMS(sleeptime);
        //NvOsThreadYield();

    }
}

static CPresult Nvmm_FileOpen(
                              CPhandle* hContent, 
                              CPstring szURI, 
                              CP_ACCESSTYPE eAccess)
{
    NvError e;

    NvmmFileCache *pFileCache = NvOsAlloc(sizeof(NvmmFileCache));
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_DEBUG, "++Nvmm_FileOpen")); 
    
    if (!pFileCache)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    NvOsMemset(pFileCache, 0, sizeof(NvmmFileCache));

    NvGetProtocolForFile(szURI, &(pFileCache->pProtocol));
    if(pFileCache->pProtocol == NULL)
    {
        e = NvError_FileOperationFailed;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        pFileCache->pProtocol->Open(&pFileCache->hFile, szURI, eAccess)
        );

   pFileCache->bIsStreaming = NV_FALSE;
   if(eAccess == NvCPR_AccessRead)
   {
    pFileCache->pProtocol->GetSize(pFileCache->hFile,
        &pFileCache->FileSize);
    if (pFileCache->FileSize == 0)
    {
        e = NvError_FileOperationFailed;
        goto fail;
    }

    pFileCache->bIsStreaming = pFileCache->pProtocol->IsStreaming(pFileCache->hFile);
   }
    pFileCache->nProtocolVersion = 1;
    pFileCache->pProtocol->GetVersion(&pFileCache->nProtocolVersion);

    *hContent = (CPhandle*)pFileCache;
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_INFO, "NvMMContentPipe Opened. File = %s", szURI));
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_DEBUG, "--Nvmm_FileOpen"));
    return NvSuccess;
    
fail:
    if (pFileCache)
    {
        if(pFileCache->pProtocol && pFileCache->hFile)
            pFileCache->pProtocol->Close(pFileCache->hFile);
        NvOsFree(pFileCache);
    }
    *hContent = NULL;
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_ERROR, "NvMMContentPipe Open Failed! NvError = 0x%08x,  File = %s", e, szURI));
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_DEBUG, "--Nvmm_FileOpen"));
    return e;
}

static CPresult Nvmm_FileClose(CPhandle hContent)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    NvU32 i;
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_DEBUG, "++Nvmm_FileClose")); 
#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif
    if (!pFileCache->bInitialized)
    {
        if (pFileCache->pProtocol && pFileCache->hFile)
            pFileCache->pProtocol->Close(pFileCache->hFile);

        NvOsFree(pFileCache);
        pFileCache = NULL;
    }
    else
    {
        pFileCache->bShutDown = NV_TRUE;
        NvOsSemaphoreSignal(pFileCache->WorkerSema);
        NvOsThreadJoin(pFileCache->WorkerThread);

        if (pFileCache->pProtocol && pFileCache->hFile)
            pFileCache->pProtocol->Close(pFileCache->hFile);

        if (pFileCache->hMem)
        {
            NvRmMemUnmap(pFileCache->hMem, pFileCache->VirtAddress, (NvU32)(pFileCache->CacheSize + pFileCache->SpareCacheSize));
            NvRmMemUnpin(pFileCache->hMem);
            NvRmMemHandleFree(pFileCache->hMem);
        }
        NvRmClose(pFileCache->hRmDevice);
        NvOsSemaphoreDestroy(pFileCache->WorkerSema);
        NvOsMutexDestroy(pFileCache->WriteLock);
        NvOsMutexDestroy(pFileCache->AvailLock);
        NvOsMutexDestroy(pFileCache->ChunkLock);
        if (pFileCache->pReadBufferTable)
        {
            for (i = 0; i < pFileCache->nRowsInReadBufferTable; i++)
            {
                NvOsFree(pFileCache->pReadBufferTable[i]);
                pFileCache->pReadBufferTable[i] = NULL;
            }
            NvOsFree(pFileCache->pReadBufferTable);
            pFileCache->pReadBufferTable = NULL;
        }
        NvOsFree(pFileCache->FileChunks);
        pFileCache->FileChunks = NULL;
        if (pFileCache->bProfile)
        {
            NvOsFileHandle oOut;
            NvError status;
            CPProf *pProf = pFileCache->pProfData;

            status = NvOsFopen("ContentPipe_Profile.txt",
                NVOS_OPEN_CREATE|NVOS_OPEN_WRITE,
                &oOut);

            if (NvSuccess == status)
            {
                NvU64 TotalBytesRead = 0;
                NvU64 TotalReadTime = 0;
                double ratio = 0;

                NvOsFprintf(oOut, "\n");
                NvOsFprintf(oOut, "Index\t\tStartTime\tTotalTime\tBytesRead\tFilePosition\n");
                NvOsFprintf(oOut, "\n");
                for (i = 0; i < pProf->count; i++)
                {
                    NvOsFprintf(oOut, "%d\t\t%f\t%f\t%ld\t%d\n", 
                        i, 
                        (pProf->StartTime[i] - pProf->StartTime[0])/ 1000000.0,
                        (pProf->EndTime[i] - pProf->StartTime[i]) / 1000000.0,
                        pProf->nBytesRead[i], 
                        pProf->SeekPosition[i]);
                    TotalBytesRead += pProf->nBytesRead[i];
                    TotalReadTime += (pProf->EndTime[i] - pProf->StartTime[i]);
                }
                ratio =  (double)TotalBytesRead / (double)pFileCache->FileSize;
                NvOsFprintf(oOut, "\n");
                NvOsFprintf(oOut, "FileSize = %ld\n", pFileCache->FileSize);
                NvOsFprintf(oOut, "TotalBytesRead = %ld\n", TotalBytesRead);
                NvOsFprintf(oOut, "TotalBytesRead/FileSize = %f\n", ratio);
                NvOsFprintf(oOut, "TotalReadTime(sec) = %f\n", TotalReadTime / 1000000.0);
                NvOsFprintf(oOut, "\n");
                NvOsFclose(oOut);

                NvOsDebugPrintf("\n");
                NvOsDebugPrintf("Index\tStartTime\tTotalTime\tBytesRead\tFilePosition\n");
                NvOsDebugPrintf("\n");
                for (i = 0; i < pProf->count; i++)
                {
                    NvOsDebugPrintf("%d\t\t%f\t%f\t%ld\t%d\n", 
                        i, 
                        (pProf->StartTime[i] - pProf->StartTime[0])/ 1000000.0,
                        (pProf->EndTime[i] - pProf->StartTime[i]) / 1000000.0,
                        pProf->nBytesRead[i], 
                        pProf->SeekPosition[i]);
                }
                NvOsDebugPrintf("\n");
                NvOsDebugPrintf("FileSize = %ld\n", pFileCache->FileSize);
                NvOsDebugPrintf("TotalBytesRead = %ld\n", TotalBytesRead);
                NvOsDebugPrintf("TotalBytesRead/FileSize = %f\n", ratio);
                NvOsDebugPrintf("TotalReadTime(sec) = %f\n", TotalReadTime / 1000000.0);
                NvOsDebugPrintf("\n");
            }
        }

        NvOsFree(pFileCache->pProfData);
        pFileCache->pProfData = NULL;
        NvOsFree(pFileCache);
        pFileCache = NULL;
    }
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_INFO, "NvMMContentPipe Closed"));
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_DEBUG, "--Nvmm_FileClose")); 
#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return NvSuccess;
}

static CPresult Nvmm_FileRead( CPhandle hContent, CPbyte *pData, CPuint nSize)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    NvU8 *pBitsEnd;
    NvU32 nBytesToRead, ulTailSize, nBytesRead;
    CPresult status = NvSuccess;

    Nvmm_PauseCaching(pFileCache, NV_FALSE);

    if (!pFileCache->bInitialized || pFileCache->bStop)
    {
        if (pFileCache->pProtocol && pFileCache->hFile)
        {
            NvOsMutexLock(pFileCache->WriteLock);
            nBytesRead = pFileCache->pProtocol->Read(pFileCache->hFile, (NvU8 *)pData, nSize);
            NvOsMutexUnlock(pFileCache->WriteLock);
        }
        else
            nBytesRead = 0;

        if (!nBytesRead)
            status = NvError_EndOfFile;
        else
            status = NvSuccess;

       goto CpExit;
    }

#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif
    // EOS and all bytes read?
    if (pFileCache->bEOS && !pFileCache->nBytesAvailable)
    {
#if KPI_MODE_ENABLED
        NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
        status = NvError_EndOfFile;
        goto CpExit;
    }
    // enough data?
    nBytesToRead = nSize;
    if (pFileCache->nBytesAvailable < nBytesToRead)
    {
        if (!pFileCache->bEOS)
        {
            if (pFileCache->HighMark < nBytesToRead )
            {
                status = NvError_ContentPipeNoData;
                goto CpExit;
            }
            NvOsMutexLock(pFileCache->WriteLock);
            while (pFileCache->nBytesAvailable < nBytesToRead &&
                !pFileCache->bEOS)
            {
                NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"waiting for data: %x %x\n", pFileCache->nBytesAvailable, nBytesToRead));
#if KPI_MODE_ENABLED
                NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
                status = NvmmFileCacheDoRead(pFileCache);
                if (status != NvSuccess)
                {
                    if (((status == NvError_ContentPipeNotReady) || (status == NvError_ContentPipeNotInvalidated))&& (pFileCache->nBytesAvailable == 0))
                    {
                        if (pFileCache->FileSize == pFileCache->CacheSize && 
                            !pFileCache->bIsStreaming)
                        {
                            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"Nvmm_FileRead::ContentPipeNotInvalidated NextChunkToWrite = %x\n", pFileCache->NextChunkToWrite));
                            status = pFileCache->pProtocol->SetPosition(pFileCache->hFile, (pFileCache->pRead - pFileCache->pBits), NvCPR_OriginBegin);
                            nBytesRead = pFileCache->pProtocol->Read(pFileCache->hFile, (NvU8 *)pData, nSize);
                            pFileCache->pRead += nSize;
                        }
                        else
                        {
                            nBytesRead = pFileCache->pProtocol->Read(pFileCache->hFile, (NvU8 *)pData, nSize);
                        }
                        NvOsMutexUnlock(pFileCache->WriteLock);
                        if (!nBytesRead)
                            status = NvError_EndOfFile;
                        else
                            status =  NvSuccess;
                        goto CpExit;
                    }
                    else
                    {
                        NvOsMutexUnlock(pFileCache->WriteLock);
                        goto CpExit;
                    }
                }
#if KPI_MODE_ENABLED
                NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif
            }
            NvOsMutexUnlock(pFileCache->WriteLock);

            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"done waiting for data\n"));
        }
        // After CacheDoRead ..if EOS set and still nBytesAvalible less then nBytesToRead
        if ((pFileCache->bEOS) && (pFileCache->nBytesAvailable < nBytesToRead))
        {
            nBytesToRead  = pFileCache->nBytesAvailable; // EOS: use whatever is left
        }
    }

    // copy data
    pBitsEnd = pFileCache->pBits+pFileCache->TotalFileChunks*pFileCache->ChunkSize;
    ulTailSize = (NvU32)(pBitsEnd - pFileCache->pRead);
    if (pFileCache->pRead + nBytesToRead > pBitsEnd){
        // straddles boundary, do two copies
        NvOsMemcpy(pData, pFileCache->pRead, ulTailSize);
        NvOsMemcpy(pData + ulTailSize, pFileCache->pBits, (nBytesToRead-ulTailSize));
        pFileCache->pRead = pFileCache->pBits + (nBytesToRead-ulTailSize);    // update read pointer
    } else {
        NvOsMemcpy(pData, pFileCache->pRead, nBytesToRead);
        pFileCache->pRead += nBytesToRead;
    }
    if ((nBytesToRead < nSize) && pFileCache->bEOS)
    {
        // If the requested bytes are more than avaliable bytes in the file and file reached EOF set status as EOF
        status = NvError_EndOfFile;
    }

    if (pFileCache->pRead == pBitsEnd) 
        pFileCache->pRead = pFileCache->pBits; // account for read pointer wrapping
    NvOsMutexLock(pFileCache->AvailLock);
    pFileCache->nBytesAvailable -= nBytesToRead;
    NvOsMutexUnlock(pFileCache->AvailLock);

    if (!pFileCache->bTrigger && !pFileCache->bEOS &&
        (pFileCache->CacheSize < pFileCache->FileSize) &&
        pFileCache->nBytesAvailable <= pFileCache->ReadTriggerThreshold)
    {
        pFileCache->bTrigger = NV_TRUE;
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"Trigger on nBytesAvailable = %x\n", pFileCache->nBytesAvailable));
        NvOsSemaphoreSignal(pFileCache->WorkerSema);
    }

CpExit:
#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    if (status == NvSuccess)
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "NvMMContentPipe Read Success. Size = 0x%x bytes", nSize));
    }
    else if (status == NvError_EndOfFile)
    {
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_INFO, "NvMMContentPipe Read EOF"));
    }
    else
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_ERROR, "NvMMContentPipe Read Failed! NvError = 0x%08x, Size = 0x%x bytes", status, nSize));
    }
    return status;
}

static CPresult Nvmm_FileWrite( CPhandle hContent, CPbyte *pData, CPuint nSize)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    if(pFileCache->pProtocol != NULL)
    {
        if(0 == pFileCache->pProtocol->Write(pFileCache->hFile, (NvU8*)pData, nSize))
        {
            return NvError_ContentPipeInsufficientMemory;
        }
    }
    else
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_ERROR, "NvMMContentPipe Write Failed! NvError = 0x%08x, Size = 0x%x bytes", NvError_NotImplemented, nSize));    
        return NvError_NotImplemented;
    }

    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "NvMMContentPipe Write Success. Size = 0x%x bytes", nSize));
    return NvSuccess;
}

static CPresult Nvmm_FileSetPosition( CPhandle  hContent, CPint nOffset, CP_ORIGINTYPE eOrigin)
{
    return Nvmm_FileSetPosition64(hContent, nOffset, eOrigin);
}

static CPresult Nvmm_FileGetPosition( CPhandle hContent,CPuint *pPosition)
{
    CPuint64 pos = 0;
    CPresult res;

    res = Nvmm_FileGetPosition64(hContent, &pos);
    *pPosition = (CPuint)pos; // big files = :(
    return res;
}

static CPresult Nvmm_FileCheckAvailableBytes( CPhandle hContent, CPuint nBytesRequested, CP_CHECKBYTESRESULTTYPE *eResult )
{
    NvError status = NvSuccess;
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    NvU32 i = 0, j = 0;
    CPuint64 nCurrentPosition, nEndPosition;

#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    if (!pFileCache->bInitialized || pFileCache->bStop)
    {
        status = pFileCache->pProtocol->GetPosition(pFileCache->hFile, &nCurrentPosition);
        if (status != NvSuccess)
        {
            *eResult = CP_CheckBytesInsufficientBytes;      
            return status;
        }
        status = pFileCache->pProtocol->SetPosition(pFileCache->hFile, 0, CP_OriginEnd);
        if (status != NvSuccess)
        {
            *eResult = CP_CheckBytesInsufficientBytes;      
            return status;
        }
        status = pFileCache->pProtocol->GetPosition(pFileCache->hFile, &nEndPosition);
        if (status != NvSuccess)
        {
            *eResult = CP_CheckBytesInsufficientBytes;      
            return status;
        }
        if( (nEndPosition -  nCurrentPosition) > nBytesRequested)
            *eResult = CP_CheckBytesOk;
        else
            *eResult = CP_CheckBytesInsufficientBytes;      
        status = pFileCache->pProtocol->SetPosition(pFileCache->hFile, nCurrentPosition, CP_OriginBegin);
        if (status != NvSuccess)
        {
            *eResult = CP_CheckBytesInsufficientBytes;      
            return status;
        }
        return status;
    }

    if (nBytesRequested > pFileCache->nBytesAvailable)
    {
        if (pFileCache->bEOS)
        {
            *eResult = pFileCache->nBytesAvailable ? 
CP_CheckBytesInsufficientBytes:
            CP_CheckBytesAtEndOfStream;
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "Nvmm_FileCheckAvailableBytes::EOS :available bytes:%x\n", pFileCache->nBytesAvailable));
        }
        else
        {
            *eResult = CP_CheckBytesNotReady;
            // Signal the thread to do some more caching if required
            pFileCache->bClientWaiting = NV_TRUE;
            NvOsSemaphoreSignal(pFileCache->WorkerSema);
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "Nvmm_FileCheckAvailableBytes::CP_CheckBytesNotReady:nBytesAvailable:%x\n", pFileCache->nBytesAvailable));
        }
        status = NvError_ContentPipeNotReady;
    }
    else
    {
        NvBool found = NV_FALSE;
        for (i = 0; i < pFileCache->nRowsInReadBufferTable && !found; i++)
        {
            for (j = 0; j < MAX_COLUMNS_READBUFFER_TABLE; j++)
            {
                if (!pFileCache->pReadBufferTable[i][j].bInUse)
                {
                    found = NV_TRUE;
                    break;
                }
            }
            if (found)
            {
                break;
            }
        }

        if (!found)
        {
            if (j == MAX_COLUMNS_READBUFFER_TABLE)
            {
                if (i == MAX_ROWS_READBUFFER_TABLE)
                {
                    *eResult = CP_CheckBytesOutOfBuffers;
                    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"Nvmm_FileCheckAvailableBytes::CP_CheckBytesOutOfBuffers\n"));
                }
                else
                {
                    *eResult = CP_CheckBytesOk;
                    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"Nvmm_FileCheckAvailableBytes::CP_CheckBytesOk\n"));
                }
            }
        }
        else
        {
            *eResult = CP_CheckBytesOk;
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"Nvmm_FileCheckAvailableBytes::CP_CheckBytesOk\n"));
        }
    }
#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return status;
}

static CPresult Nvmm_FileReadBuffer( CPhandle hContent, CPbyte **ppBuffer, CPuint *nSize, CPbool bForbidCopy)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    NvU32 nBytesToRead, ulTailSize;
    NvU8 *pBufferStart, *pBitsEnd;
    NvBool found = NV_FALSE;
    ReadBuffer *pReadBufferEntry = NULL;
    NvU32 i = 0, j = 0;
    CPresult status = NvSuccess;
    
#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif
    if (!pFileCache->bInitialized || pFileCache->bStop)
    {
        status = NvError_ContentPipeInNonCachingMode;
        goto CpExit;
    }

    Nvmm_PauseCaching(pFileCache, NV_FALSE);

    if (pFileCache->bEOS && !pFileCache->nBytesAvailable)
    {
#if KPI_MODE_ENABLED
        NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
        status = NvError_EndOfFile;
        goto CpExit;
    }

    nBytesToRead = *nSize;
    if (pFileCache->nBytesAvailable <nBytesToRead)
    {
        if (!pFileCache->bEOS && !pFileCache->nBytesAvailable)
        {
#if KPI_MODE_ENABLED
            NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
            status = NvError_ContentPipeNoData;
            goto CpExit;
        }
        else
            nBytesToRead  = pFileCache->nBytesAvailable;
    }

    pFileCache->bClientWaiting = NV_FALSE;
    *nSize = nBytesToRead;
    for (i = 0; i < pFileCache->nRowsInReadBufferTable && !found; i++)
    {
        for (j = 0; j < MAX_COLUMNS_READBUFFER_TABLE; j++)
        {
            if (!pFileCache->pReadBufferTable[i][j].bInUse)
            {
                pReadBufferEntry = &pFileCache->pReadBufferTable[i][j];
                found = NV_TRUE;
                break;
            }
        }
        if (found)
        {
            break;
        }
    }

    if (!found)
    {
        if (j == MAX_COLUMNS_READBUFFER_TABLE)
        {
            if (i == MAX_ROWS_READBUFFER_TABLE)
            {
                status = NvError_ContentPipeNoFreeBuffers;
                goto CpExit;            
            }
            else
            {
                pFileCache->pReadBufferTable[pFileCache->nRowsInReadBufferTable] = NvOsAlloc(sizeof(ReadBuffer) * MAX_COLUMNS_READBUFFER_TABLE);
                if (!pFileCache->pReadBufferTable[pFileCache->nRowsInReadBufferTable])
                {
                    status = NvError_ContentPipeNoFreeBuffers;
                    goto CpExit;            
                }
                NvOsMemset(pFileCache->pReadBufferTable[pFileCache->nRowsInReadBufferTable], 0, sizeof(ReadBuffer) * MAX_COLUMNS_READBUFFER_TABLE);
                pReadBufferEntry = &pFileCache->pReadBufferTable[pFileCache->nRowsInReadBufferTable][0];
                pFileCache->nRowsInReadBufferTable++;
            }
        }
    }

    pFileCache->nReadBuffersInUse++; // fixme atomic this

    if (pFileCache->CacheSize < pFileCache->FileSize)
    {
        // does requested area wrap around end of buffer?
        pBufferStart = NULL;
        pBitsEnd = pFileCache->pBits+pFileCache->TotalFileChunks*pFileCache->ChunkSize;
        ulTailSize = (NvU32)(pBitsEnd - pFileCache->pRead);
        if ((pFileCache->pRead > pFileCache->pWrite) && (ulTailSize < nBytesToRead))
        {
            // can't avoid copy - must unify data at pool tail and head into the spare bits buffer
            if (pFileCache->bSpareInUse){
#if KPI_MODE_ENABLED
                NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
                status = NvError_ContentPipeSpareAreaInUse;
                goto CpExit;            
            }
            pFileCache->bSpareInUse = NV_TRUE;
            pBufferStart = pFileCache->pSpareBits;

            // copy data to spare buffer
            NvOsMemcpy(pFileCache->pSpareBits, pFileCache->pRead, ulTailSize);
            NvOsMemcpy(pFileCache->pSpareBits + ulTailSize, pFileCache->pBits, (nBytesToRead-ulTailSize));

            pFileCache->pRead = pFileCache->pBits + (nBytesToRead-ulTailSize);    // update read pointer
        }
        else
        {
            pBufferStart = pFileCache->pRead;       // use next read area directly from pool
            pFileCache->pRead += nBytesToRead;        // update read pointer
        }
        if (pFileCache->pRead == pBitsEnd) 
            pFileCache->pRead = pFileCache->pBits; // account for read pointer wrapping
    }
    else
    {
        pBufferStart = pFileCache->pRead;       // use next read area directly from pool
        pFileCache->pRead += nBytesToRead;        // update read pointer
    }

    pFileCache->nBytesAvailable -= nBytesToRead;   // decrement bytes available for reading

    pReadBufferEntry->bInUse = NV_TRUE;
    pReadBufferEntry->pBuffer = pBufferStart;
    pReadBufferEntry->size = nBytesToRead;

    *ppBuffer = (CPbyte *)pReadBufferEntry->pBuffer;

    if (!pFileCache->bTrigger && !pFileCache->bEOS &&
        (pFileCache->CacheSize < pFileCache->FileSize) &&
        pFileCache->nBytesAvailable <= pFileCache->ReadTriggerThreshold)
    {
        pFileCache->bTrigger = NV_TRUE;
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "Trigger on nBytesAvailable = %x\n", pFileCache->nBytesAvailable));
        NvOsSemaphoreSignal(pFileCache->WorkerSema);
    }

CpExit:    
#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    if (status == NvSuccess)
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "NvMMContentPipe ReadBuffer Success. Size = 0x%x bytes", *nSize));    
    }
    else if (status == NvError_EndOfFile)
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_INFO, "NvMMContentPipe ReadBuffer EOF"));   
    }
    else
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_ERROR, "NvMMContentPipe ReadBuffer Failed! NvError = 0x%08x, Size = 0x%x bytes", status,*nSize));
    }
    return NvSuccess;
}

static CPresult Nvmm_FileReleaseReadBuffer(CPhandle hContent, CPbyte *pBuffer)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    NvU32 i,j;
    NvError status = NvError_BadParameter;
    NvBool found = NV_FALSE;

#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif
    if (!pFileCache->bInitialized)
    {
        return NvError_ContentPipeInNonCachingMode;
    }

    for (i = 0; i < pFileCache->nRowsInReadBufferTable; i++)
    {
        for (j = 0; j < MAX_COLUMNS_READBUFFER_TABLE; j++)
        {
            if (pFileCache->pReadBufferTable[i][j].pBuffer == (NvU8 *)pBuffer)
            {
                pFileCache->pReadBufferTable[i][j].bInUse = NV_FALSE;
                if (pFileCache->pReadBufferTable[i][j].pBuffer == pFileCache->pSpareBits)
                {
                    pFileCache->bSpareInUse = NV_FALSE;    // if this buffer uses spare bits then mark them available
                }
                pFileCache->pReadBufferTable[i][j].pBuffer = NULL;
                pFileCache->nReadBuffersInUse--; // fixme atomic this
                // Signal the thread to do some more caching if required
                if (!pFileCache->bInvalidateCache)
                {
                    NvOsSemaphoreSignal(pFileCache->WorkerSema);
                }
                status = NvSuccess;
                found = NV_TRUE;
                break;
            }
        }
        if (found)
        {
            break;
        }
    }
    if (pFileCache->bInvalidateCache)
    {
        FileChunk *pFileChunk;
        for (i = 0; i < pFileCache->TotalFileChunks; i++)
        {
            pFileChunk = &pFileCache->FileChunks[i];
            if ((pFileChunk->status == FULL) && (pFileChunk->bInvalidate))
            {
                if (!CheckFileChunkInUse(pFileCache, pFileChunk))
                {
                    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "Nvmm_FileReleaseReadBuffer::Chunk %x", i));
                    pFileChunk->bInvalidate = NV_FALSE;
                    pFileChunk->status = INVALID;
                }
            }
        }
        for (i = 0; i < pFileCache->TotalFileChunks; i++)
        {
            if ((pFileCache->FileChunks[i].status == FULL) && (pFileCache->FileChunks[i].bInvalidate))
            {
                break;
            }
        }
        if (i == pFileCache->TotalFileChunks)
        {
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "Nvmm_FileReleaseReadBuffer::bInvalidate = FALSE"));
            pFileCache->bInvalidateCache = NV_FALSE;
            NvOsSemaphoreSignal(pFileCache->WorkerSema);
        }
    }
    
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "FileReleaseReadBuffer")); 

#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return status;
}

static CPresult Nvmm_FileGetWriteBuffer(CPhandle hContent, CPbyte **ppBuffer, CPuint nSize)
{
    return NvError_NotImplemented;
}

static CPresult Nvmm_FileWriteBuffer(CPhandle hContent, CPbyte *pBuffer, CPuint nFilledSize)
{
    return NvError_NotImplemented;
}

static CPresult Nvmm_FileRegisterCallback ( CPhandle hContent, CPresult (*ClientCallback)(CP_EVENTTYPE eEvent, CPuint iParam))
{
    return NvError_NotImplemented;
}

static  CPresult Nvmm_FileCreate( CPhandle *hContent,CPstring szURI)
{
    return NvError_NotImplemented;
}

static void FindNextChunkToWrite(NvmmFileCache *pFileCache, NvU64 nOffset)
{
    NvBool bNextChunkInUse = NV_FALSE;
    NvU32 i, j;
    FileChunk *pCurrentChunk;

    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"cache miss, seeking bEOS = %x\n", pFileCache->bEOS));

    for (i = pFileCache->NextChunkToWrite; i < pFileCache->TotalFileChunks; i++ )
    {
        pCurrentChunk = &pFileCache->FileChunks[i];
        bNextChunkInUse = CheckFileChunkInUse(pFileCache, pCurrentChunk);
        if (!bNextChunkInUse)
        {
            pFileCache->NextChunkToWrite = i;
            break;
        }
    }

    if (bNextChunkInUse && i == pFileCache->TotalFileChunks)
    {
        for (j = 0; j < pFileCache->NextChunkToWrite; j++ )
        {
            pCurrentChunk = &pFileCache->FileChunks[j];
            bNextChunkInUse = CheckFileChunkInUse(pFileCache, pCurrentChunk);
            if (!bNextChunkInUse)
            {
                pFileCache->NextChunkToWrite = j;
                break;
            }
        }
    }

    if (!bNextChunkInUse)
    {
        pFileCache->pRead = pFileCache->pWrite = pFileCache->pBits + (pFileCache->NextChunkToWrite*pFileCache->ChunkSize);
        if (pFileCache->FileSize == nOffset)
        {
            pFileCache->bEOS = NV_TRUE;
        }
        else
        {
            pFileCache->bEOS = NV_FALSE;
        }
        pFileCache->nBytesAvailable = 0;
        pFileCache->FileChunks[pFileCache->NextChunkToWrite].position = nOffset;
        pFileCache->FileChunks[pFileCache->NextChunkToWrite].status = INVALID;
        pFileCache->FileChunks[pFileCache->NextChunkToWrite].bInvalidate = NV_FALSE;
    }
}

static CPresult Nvmm_FileSetPosition64( CPhandle  hContent, CPint64 nOffset, CP_ORIGINTYPE eOrigin)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    NvU32 i;
    NvError status = NvSuccess;
    NvU32 iChunk = 0;
    FileChunk *pFileChunk;

    if (!pFileCache->bInitialized || pFileCache->bStop)
    {
        return pFileCache->pProtocol->SetPosition(pFileCache->hFile, nOffset, eOrigin);
    }

#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"Nvmm_FileSetPosition64::nOffset = 0x%x\teOrigin = 0x%x", nOffset, eOrigin)); 

    pFileCache->bEOS = NV_FALSE;

    if (eOrigin == CP_OriginTime)
    {
        NvU32 supportsTimeSeek = 0;
        if (pFileCache->nProtocolVersion < 2)
        {
            status = NvError_BadParameter;
            goto seekend;
        }

        status = pFileCache->pProtocol->QueryConfig(pFileCache->hFile,
                                                    NvCPR_ConfigCanSeekByTime,
                                                    &supportsTimeSeek, 
                                                    sizeof(NvU32));

        if (status != NvSuccess || !supportsTimeSeek)
        {
            status = NvError_BadParameter;
            goto seekend;
        }

        // Seeking in time. Invalidate the file cache, set content position, and
        // unpause worker thread (cached data is not valid so worker thread
        // should no longer be paused)
        NvOsMutexLock(pFileCache->WriteLock);
        Nvmm_InvalidateCache(hContent);

        FindNextChunkToWrite(pFileCache, nOffset);
        status = pFileCache->pProtocol->SetPosition(pFileCache->hFile, nOffset,
                                                    NvCPR_OriginTime);
        if (status == NvSuccess)
        {
            Nvmm_PauseCaching(pFileCache, NV_FALSE);
            NvOsSemaphoreSignal(pFileCache->WorkerSema);
        }


        NvOsMutexUnlock(pFileCache->WriteLock);

        goto seekend;
    }

    if (pFileCache->FileSize == pFileCache->CacheSize && 
        !pFileCache->bIsStreaming)
    {
        switch (eOrigin)
        {
        case CP_OriginBegin:
            break;
        case CP_OriginCur:
            nOffset = pFileCache->pRead - pFileCache->pBits + nOffset;
            break;
        case CP_OriginEnd:
            nOffset = pFileCache->FileSize + nOffset;
            break;      
        case CP_OriginMax:
        default:
            break;
        }

        if ((NvU64)nOffset > pFileCache->FileSize)
        {
            return NvError_FileOperationFailed;
        }

        NvOsMutexLock(pFileCache->WriteLock);

        if ((NvU64)nOffset == pFileCache->FileSize)
        {
            iChunk = pFileCache->TotalFileChunks - 1;
        }
        else
        {
            iChunk = (NvU32)(nOffset / pFileCache->ChunkSize);
        }
        pFileChunk = &pFileCache->FileChunks[iChunk];

        pFileCache->pRead = pFileCache->pBits + nOffset;
        if ((pFileChunk->status == INVALID) || 
            ((pFileChunk->status == FULL) && pFileChunk->bInvalidate))
        {
            // Chunk is free, cache miss. Set content position, and unpause worker thread
            // (cached data is not valid so worker thread should no longer be paused)
            status = pFileCache->pProtocol->SetPosition(pFileCache->hFile, pFileChunk->position, NvCPR_OriginBegin);
            if (status != NvSuccess)
            {
                NvOsMutexUnlock(pFileCache->WriteLock);
#if KPI_MODE_ENABLED
                NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
                return status;
            }
            pFileCache->nBytesAvailable = 0;
            pFileCache->NextChunkToWrite = iChunk;
            Nvmm_PauseCaching(pFileCache, NV_FALSE);
            NvOsSemaphoreSignal(pFileCache->WorkerSema);
        }
        else if (pFileChunk->status == FULL)
        {
            pFileCache->pRead = pFileCache->pBits + nOffset;
            pFileCache->nBytesAvailable = pFileChunk->size - (NvU32)(nOffset - pFileChunk->position);
            if (iChunk == pFileCache->TotalFileChunks-1)
            {
                pFileCache->bEOS = NV_TRUE;
            }
            else
            {
                for(i = iChunk+1; i <pFileCache->TotalFileChunks; i++ )
                {
                    if ((pFileCache->FileChunks[i].status == INVALID) ||
                        ((pFileCache->FileChunks[i].status == FULL) && (pFileCache->FileChunks[i].bInvalidate)))
                    {
                        pFileCache->NextChunkToWrite = i;
                        break;
                    } 
                    else if (pFileCache->FileChunks[i].status == FULL)
                    {
                        pFileCache->nBytesAvailable += pFileCache->FileChunks[i].size;
                        if (i == pFileCache->TotalFileChunks-1)
                        {
                            pFileCache->bEOS = NV_TRUE;
                        }
                    }
                } 
            }
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"nBytesAvailable = 0x%x\n", pFileCache->nBytesAvailable));
        }

        NvOsMutexUnlock(pFileCache->WriteLock);
    }
    else
    {
        NvU32 j,k;
        NvBool found = NV_FALSE;
        FileChunk *pCurrentChunk, *pPrevChunk;

        NvOsMutexLock(pFileCache->ChunkLock);

        switch (eOrigin)
        {
        case CP_OriginBegin:
            break;
        case CP_OriginCur:
            // get the file location of pRead and then add the nOffset to calculate the exact offset
            for (i = 0; i<pFileCache->TotalFileChunks; i++)
            {
                if ((pFileCache->FileChunks[i].pBuffer <= pFileCache->pRead) &&
                    (pFileCache->pRead < pFileCache->FileChunks[i].pBuffer + pFileCache->FileChunks[i].size))
                {
                    nOffset = pFileCache->FileChunks[i].position + (pFileCache->pRead - pFileCache->FileChunks[i].pBuffer) + nOffset;
                    break;
                }
            }
            if (i == pFileCache->TotalFileChunks)
            {
                NvOsMutexUnlock(pFileCache->ChunkLock);
#if KPI_MODE_ENABLED
                NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
                return NvError_ContentPipeNoData;
            }
            break;
        case CP_OriginEnd:
            nOffset = pFileCache->FileSize + nOffset;
            if ((NvU64)nOffset == pFileCache->FileSize)
            {
                nOffset -= 1;
            }
            break;      
        case CP_OriginMax:
        default:
            break;
        }

        if ((NvU64)nOffset > pFileCache->FileSize)
        {
            NvOsMutexUnlock(pFileCache->ChunkLock);
            return NvError_FileOperationFailed;
        } 
        if (!pFileCache->bInvalidateCache)
        {
            // look for chunk containing desired position
            for (i = 0; i < pFileCache->TotalFileChunks; i++)
            {
                pCurrentChunk = &(pFileCache->FileChunks[i]);
                if (pCurrentChunk->status == FULL &&
                    pCurrentChunk->position <= (NvU64)nOffset &&
                    pCurrentChunk->position + pCurrentChunk->size > (NvU64)nOffset)
                {
                    iChunk = i;
                    pFileCache->pRead = pFileCache->FileChunks[i].pBuffer + (nOffset - pFileCache->FileChunks[i].position);
                    pFileCache->nBytesAvailable = pFileCache->FileChunks[i].size - (NvU32)(nOffset - pFileCache->FileChunks[i].position);

                    found = NV_TRUE;
                    //set EOS if the seek is to the end of file, or this is the last chunk
                    if (((NvU64)nOffset == pFileCache->FileSize) ||
                        (pFileCache->FileSize == (pFileCache->FileChunks[i].size + pFileCache->FileChunks[i].position)))
                    {
                        pFileCache->bEOS = NV_TRUE;
                    }
                    break;
                }
            }

            // calculate size of valid data in cache
            if (found)
            {
                NvS32 nextWrite = -1;
                NvU64 nextPos = 0;

                pPrevChunk = &pFileCache->FileChunks[iChunk];
                for (j = iChunk + 1; j < pFileCache->TotalFileChunks; j++ )
                {
                    pCurrentChunk = &pFileCache->FileChunks[j];
                    if ((pCurrentChunk->status == FULL) &&
                        (pCurrentChunk->position == pPrevChunk->position + pPrevChunk->size))
                    {
                        pFileCache->nBytesAvailable += pCurrentChunk->size;
                        pPrevChunk = pCurrentChunk;
                    }
                    else
                    {
                        nextWrite = j;
                        nextPos = pPrevChunk->position + pPrevChunk->size;
                        break;
                    }
                }

                if (j == pFileCache->TotalFileChunks)
                {
                    pPrevChunk = &pFileCache->FileChunks[j - 1];
                    for (k = 0; k < iChunk; k++)
                    {
                        pCurrentChunk = &pFileCache->FileChunks[k];
                        if ((pCurrentChunk->status == FULL) &&
                            (pCurrentChunk->position == pPrevChunk->position + pPrevChunk->size))
                        {
                            pFileCache->nBytesAvailable += pCurrentChunk->size;
                            pPrevChunk = pCurrentChunk;
                        }
                        else
                        {
                            nextWrite = k;
                            nextPos = pPrevChunk->position + pPrevChunk->size;
                            break;
                        }
                    }
                }

                if ((nextWrite != -1) && (pFileCache->NextChunkToWrite == (NvU32)nextWrite) && (pFileCache->FileChunks[nextWrite].position == nextPos))
                {
                    if (pFileCache->FileChunks[nextWrite].position == pFileCache->FileSize)
                    {
                        pFileCache->bEOS = NV_TRUE;
                    }
                    nextWrite = -1;
                }
                else if (nextWrite != -1)
                {
                    NvU64 curPos = 0;
                    pFileCache->pProtocol->GetPosition(pFileCache->hFile, &curPos);
                    if (curPos == nextPos && 
                        (pFileCache->NextChunkToWrite == (NvU32)nextWrite))
                    {
                        nextWrite = -1;
                    }
                }

                if (nextWrite != -1)
                {
                    NvU32 prevsize = pFileCache->nBytesAvailable;

                    NvOsMutexUnlock(pFileCache->ChunkLock);
                    NvOsMutexLock(pFileCache->WriteLock);

                    //if (pFileCache->nBytesAvailable != prevsize)
                        //NvOsDebugPrintf("fixing bytesavail: was %d, now %d\n", pFileCache->nBytesAvailable, prevsize);

                    pFileCache->nBytesAvailable = prevsize;
                    pFileCache->NextChunkToWrite = nextWrite;
                    pFileCache->bEOS = NV_FALSE;

                    if (nextPos < pFileCache->FileSize)
                    {
                        status = pFileCache->pProtocol->SetPosition(pFileCache->hFile, nextPos, NvCPR_OriginBegin);
                        if (status == NvError_NotSupported)
                        {
                            NvU64 curPos = 0;
                            pFileCache->pProtocol->GetPosition(pFileCache->hFile, &curPos);
                            if (curPos == nextPos)
                                status = NvSuccess;
                        }
                        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"SetPosition::nBytesAvailable = 0x%x\tNextChunkToWrite = 0x%x", pFileCache->nBytesAvailable, nextWrite));
                    }

                    NvOsMutexLock(pFileCache->ChunkLock);
                    NvOsMutexUnlock(pFileCache->WriteLock);
                }
            }
        }

        if (!found)
        {
            //Cache miss. Find chunk for content data, set content position, and unpause worker thread
            // (previously cached data is not valid so worker thread should no longer be paused)
            NvOsMutexUnlock(pFileCache->ChunkLock);
            NvOsMutexLock(pFileCache->WriteLock);

            FindNextChunkToWrite(pFileCache, nOffset);

            status = pFileCache->pProtocol->SetPosition(pFileCache->hFile, nOffset, NvCPR_OriginBegin);
            if (status == NvSuccess)
            {
                Nvmm_PauseCaching(pFileCache, NV_FALSE);
                NvOsSemaphoreSignal(pFileCache->WorkerSema);
            }

            NvOsMutexLock(pFileCache->ChunkLock);
            NvOsMutexUnlock(pFileCache->WriteLock);
        }

        NvOsMutexUnlock(pFileCache->ChunkLock);
    }

seekend:
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "Nvmm_FileSetPosition (NextChunkToWrite %x)\n", pFileCache->NextChunkToWrite)); 
#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return status;
}

static CPresult Nvmm_FileGetPosition64( CPhandle hContent, CPuint64 *pPosition)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    NvU32 pReadOffset, iWriteBlock;

#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif
    if (!pFileCache->bInitialized || pFileCache->bStop)
    {
        return pFileCache->pProtocol->GetPosition(pFileCache->hFile, pPosition);
    }

    // determine write block where read ptr resides then use its writepos to compute readpos
    pReadOffset = (NvU32)(pFileCache->pRead-pFileCache->pBits);
    if ((pReadOffset == (NvU32)pFileCache->FileSize) && 
        (pFileCache->CacheSize == pFileCache->FileSize) &&
        !pFileCache->bIsStreaming)
    {
        iWriteBlock = pFileCache->TotalFileChunks - 1;
    }
    else
    {
        iWriteBlock = pReadOffset/pFileCache->ChunkSize;
    }
    *pPosition = pFileCache->FileChunks[iWriteBlock].position + (pReadOffset - (pFileCache->ChunkSize*iWriteBlock));
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"Nvmm_FileGetPosition64::Position = %x\n", *pPosition));
#if KPI_MODE_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return NvSuccess;
}

static CPresult Nvmm_FileGetBaseAddress( CPhandle hContent, NvRmPhysAddr *pPhyAddress, void **pVirtAddress)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    if (!pFileCache->bInitialized)
    {
        return NvError_ContentPipeInNonCachingMode;
    }
    *pPhyAddress = pFileCache->PhyAddress;
    *pVirtAddress = pFileCache->VirtAddress;
    return NvSuccess;
}

static CPresult Nvmm_FileRegisterClientCallback( CPhandle hContent, void *pClientContext, ClientCallbackFunction ClientCallback)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;

    if (pFileCache)
    {
        pFileCache->pClientContext = pClientContext;
        pFileCache->ClientCallback = ClientCallback;
    }
    return NvSuccess;
}

static CPresult Nvmm_FileGetAvailableBytes(CPhandle hContent, CPuint64 *nBytesAvailable)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    CPuint nCurrentPosition, nEndPosition;
    NvError status = NvSuccess;

    if (!pFileCache->bInitialized || pFileCache->bStop)
    {
        status = Nvmm_FileGetPosition(hContent, &nCurrentPosition);
        if (status != NvSuccess)
        {
            *nBytesAvailable = 0;      
            return status;
        }
        status = Nvmm_FileSetPosition(hContent, 0, CP_OriginEnd);
        if (status != NvSuccess)
        {
            *nBytesAvailable = 0;      
            return status;
        }
        status = Nvmm_FileGetPosition(hContent, &nEndPosition);
        if (status != NvSuccess)
        {
            *nBytesAvailable = 0;      
            return status;
        }
        *nBytesAvailable = nEndPosition - nCurrentPosition;

        status = Nvmm_FileSetPosition(hContent, nCurrentPosition, CP_OriginBegin);
        if (status != NvSuccess)
        {
            *nBytesAvailable = 0;      
            return status;
        }
        return status;
    }

    if (nBytesAvailable)
    {
        NvOsMutexLock(pFileCache->AvailLock);
        *nBytesAvailable = pFileCache->nBytesAvailable;
        NvOsMutexUnlock(pFileCache->AvailLock);
    }
    return NvSuccess;
}

static CPresult Nvmm_FileGetSize(CPhandle hContent, CPuint64 *pFileSize)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    if (pFileSize)
    {
        *pFileSize = pFileCache->FileSize;
    }
    return NvSuccess;
}

static CPresult Nvmm_InitializeCP(CPhandle hContent, CPuint64 MinCacheSize, CPuint64 MaxCacheSize, CPuint32 SpareAreaSize, CPuint64* pActualSize)
{
    NvError e;
    NvU32 i;
    NvBool done = NV_FALSE;    

    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;

    pFileCache->bInitialized = NV_FALSE;

    if((MinCacheSize == 0) || (MaxCacheSize == 0) || (SpareAreaSize == 0))
    {
        *pActualSize = 0;
        return NvSuccess;
    }
    pFileCache->CacheSize = pFileCache->FileSize > MaxCacheSize ? MaxCacheSize : (NvU32)pFileCache->FileSize;
    pFileCache->SpareCacheSize = SpareAreaSize;

    if (pFileCache->bIsStreaming)
    {
        if (pFileCache->CacheSize < STREAMING_MIN_CACHE_SIZE)             /* In case of rtsp size is -1*/
            pFileCache->CacheSize = STREAMING_MIN_CACHE_SIZE;
        else if (pFileCache->CacheSize >= STREAMING_MIN_CACHE_SIZE) /* In case of http actual size may be > 10MB */
            pFileCache->CacheSize = STREAMING_MIN_CACHE_SIZE;

        if (pFileCache->SpareCacheSize < STREAMING_MIN_SPARE_SIZE)
            pFileCache->SpareCacheSize = STREAMING_MIN_SPARE_SIZE;
        else  if (pFileCache->SpareCacheSize >= STREAMING_MIN_SPARE_SIZE)
            pFileCache->SpareCacheSize = STREAMING_MIN_SPARE_SIZE;
    }

    pFileCache->ReadTriggerThreshold = pFileCache->CacheSize / 4;
    pFileCache->HighMark = (pFileCache->CacheSize * 3) / 4;

    NV_CHECK_ERROR_CLEANUP(
        NvRmOpen(&pFileCache->hRmDevice, 0)
        );

    while (!done)
    {
        e = NvSuccess;
        if (e == NvSuccess)
        {
            e = NvRmMemHandleAlloc(pFileCache->hRmDevice,
                            NULL,
                            0,
                            4,
                            NvOsMemAttribute_Uncached,
                            (NvU32)pFileCache->CacheSize,
                            0,
                            0,
                            &pFileCache->hMem);
            if (e == NvSuccess)
            {
                done = NV_TRUE;
                *pActualSize = pFileCache->CacheSize;
                pFileCache->bInitialized = NV_TRUE;
            }
            else if (e == NvError_InsufficientMemory)
            {
                pFileCache->hMem = NULL;
                pFileCache->CacheSize -= (1024 * 1024);
                if (pFileCache->CacheSize < MinCacheSize)
                {
                    *pActualSize = 0;
                    done = NV_TRUE;
                }
            }
            else
                goto fail;
        }
        else if (e == NvError_InsufficientMemory)
        {
            pFileCache->CacheSize -= (1024 * 1024);
            if (pFileCache->CacheSize < MinCacheSize)
            {
                *pActualSize = 0;
                done = NV_TRUE;
            }
        }
        else
            goto fail;
    }

    if (pFileCache->bInitialized)
    {
        pFileCache->PhyAddress = NvRmMemPin(pFileCache->hMem);

        NV_CHECK_ERROR_CLEANUP(
            NvRmMemMap(pFileCache->hMem, 
            0,
            (NvU32)pFileCache->CacheSize,
            NVOS_MEM_READ_WRITE,
            (void *)&pFileCache->VirtAddress)
            );

        NV_CHECK_ERROR_CLEANUP(
            NvOsSemaphoreCreate(&(pFileCache->WorkerSema), 1)
            );

        NV_CHECK_ERROR_CLEANUP(
            NvOsMutexCreate(&(pFileCache->WriteLock))
            );

        NV_CHECK_ERROR_CLEANUP(
            NvOsMutexCreate(&(pFileCache->AvailLock))
            );

        NV_CHECK_ERROR_CLEANUP(
            NvOsMutexCreate(&(pFileCache->ChunkLock))
            );

        pFileCache->pReadBufferTable = NvOsAlloc(sizeof(ReadBuffer *) * MAX_ROWS_READBUFFER_TABLE);
        if (!pFileCache->pReadBufferTable)
        {
            goto fail;
        }
        NvOsMemset(pFileCache->pReadBufferTable, 0, sizeof(ReadBuffer *) * MAX_ROWS_READBUFFER_TABLE);

        pFileCache->pReadBufferTable[0] = NvOsAlloc(sizeof(ReadBuffer) * MAX_COLUMNS_READBUFFER_TABLE);
        if (!pFileCache->pReadBufferTable[0])
        {
            goto fail;
        }
        NvOsMemset(pFileCache->pReadBufferTable[0], 0, sizeof(ReadBuffer) * MAX_COLUMNS_READBUFFER_TABLE);
        pFileCache->nRowsInReadBufferTable = 1;

        pFileCache->pBits = (NvU8 *)pFileCache->VirtAddress;
        pFileCache->pWrite = pFileCache->pRead = pFileCache->pBits;

        if ((pFileCache->CacheSize < pFileCache->FileSize) || 
            pFileCache->bIsStreaming)
        {
            pFileCache->pSpareBits = (NvU8 *)pFileCache->VirtAddress + pFileCache->CacheSize - pFileCache->SpareCacheSize;
            pFileCache->CacheSize -= pFileCache->SpareCacheSize;
        }
        else
        {
            pFileCache->SpareCacheSize = 0;
        }

        if (pFileCache->bIsStreaming)
        {
            pFileCache->pProtocol->QueryConfig(pFileCache->hFile, 
                                               NvCPR_ConfigGetChunkSize, 
                                               &(pFileCache->ChunkSize), 
                                               sizeof(NvU32));
        }
        else
        {
            pFileCache->ChunkSize = (NvU32)pFileCache->CacheSize / 32;
            if (pFileCache->ChunkSize < LOW_CHUNKSIZE_THRESHOLD)
            {
                pFileCache->ChunkSize = LOW_CHUNKSIZE_THRESHOLD;
            }
        }

        if (pFileCache->FileSize < pFileCache->ChunkSize)
        {
            pFileCache->ChunkSize = (NvU32)pFileCache->FileSize;
            pFileCache->TotalFileChunks = 1; 
        }
        else
        {
            pFileCache->TotalFileChunks = (NvU32)(pFileCache->CacheSize + pFileCache->ChunkSize - 1) / pFileCache->ChunkSize; 
        }

        pFileCache->FileChunks = NvOsAlloc(sizeof(FileChunk) * pFileCache->TotalFileChunks);
        if (!pFileCache->FileChunks)
        {
            goto fail;
        }
        NvOsMemset(pFileCache->FileChunks, 0, sizeof(FileChunk) * pFileCache->TotalFileChunks);

        for (i = 0; i < pFileCache->TotalFileChunks; i++)
        {
            pFileCache->FileChunks[i].pBuffer = pFileCache->pBits + (i*pFileCache->ChunkSize);
            pFileCache->FileChunks[i].size = pFileCache->ChunkSize;
            pFileCache->FileChunks[i].position = 0;
            if (pFileCache->CacheSize == pFileCache->FileSize &&
                !pFileCache->bIsStreaming)
            {
                pFileCache->FileChunks[i].position = (i*pFileCache->ChunkSize);
            }
            pFileCache->FileChunks[i].status = INVALID;
        }

        NV_CHECK_ERROR_CLEANUP(
            NvOsThreadCreate(NvmmFileCacheThread,
            (void *)pFileCache, 
            &(pFileCache->WorkerThread))
            );
    }
#if NVMM_CP_PROFILE_ENABLED
    pFileCache->bProfile = NV_TRUE;
#else
    pFileCache->bProfile = NV_FALSE;
#endif

    if (pFileCache->bProfile)
    {
        pFileCache->pProfData = NvOsAlloc(sizeof(CPProf));
        if (!pFileCache->pProfData)
        {
            goto fail;
        }
        NvOsMemset(pFileCache->pProfData, 0, sizeof(CPProf));
    }

    if (pFileCache->bIsStreaming && pFileCache->nProtocolVersion >= 2)
    {
        NvU32 prebuffersize = 0;

        pFileCache->pProtocol->QueryConfig(pFileCache->hFile,
                                           NvCPR_ConfigPreBufferAmount,
                                           &prebuffersize,
                                           sizeof(NvU32));

        if (prebuffersize > 0)
        {
            NvOsSemaphoreSignal(pFileCache->WorkerSema);

            while (pFileCache->nBytesAvailable < prebuffersize &&
                   !pFileCache->bEOS)
            {
                NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE, "prebuffering: %d/%d\n", 
                                pFileCache->nBytesAvailable, prebuffersize));
                NvOsSleepMS(500);
            }
        }   
    }

    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_INFO,"NvMMContentPipe Cache Initialized:TotalFileChunks = %d\tFileSize = %lld\n", pFileCache->TotalFileChunks, pFileCache->FileSize));
    return NvSuccess;

fail:

    pFileCache->bInitialized = NV_FALSE;
    if (pFileCache->hMem)
    {
        NvRmMemUnmap(pFileCache->hMem, pFileCache->VirtAddress, (NvU32)(pFileCache->CacheSize + pFileCache->SpareCacheSize));
        NvRmMemHandleFree(pFileCache->hMem);
    }
    NvRmClose(pFileCache->hRmDevice);
    NvOsSemaphoreDestroy(pFileCache->WorkerSema);
    NvOsMutexDestroy(pFileCache->WriteLock);
    NvOsMutexDestroy(pFileCache->AvailLock);
    NvOsMutexDestroy(pFileCache->ChunkLock);
    if (pFileCache->pReadBufferTable)
    {
        for (i = 0; i < pFileCache->nRowsInReadBufferTable; i++)
        {
            NvOsFree(pFileCache->pReadBufferTable[i]);
        }
        NvOsFree(pFileCache->pReadBufferTable);
    }
    NvOsFree(pFileCache->FileChunks);
    NvOsFree(pFileCache->pProfData);
    NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_ERROR,"NvMMContentPipe Cache Initialize Failed! NvError = 0x%08x", NvError_ContentPipeInsufficientMemory));
    return NvError_ContentPipeInsufficientMemory;
}

static CPbool Nvmm_IsStreaming(CPhandle hContent)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    return pFileCache->bIsStreaming;
}

static CPresult Nvmm_InvalidateCache(CPhandle hContent)
{
    NvU32 i;
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    for (i = 0; i < pFileCache->TotalFileChunks; i++)
    {
        FileChunk *pFileChunk = &pFileCache->FileChunks[i];
        if (pFileChunk->status == FULL)
        {
            if (!CheckFileChunkInUse(pFileCache, pFileChunk))
            {
                pFileChunk->status = INVALID;
            }
            else
            {
                NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_VERBOSE,"Nvmm_InvalidateCache::Chunk %x\n", i));
                pFileChunk->bInvalidate = NV_TRUE;
                pFileCache->bInvalidateCache = NV_TRUE;
            }
        }
    }
    if (pFileCache->bEOS)
    {
        pFileCache->bEOS = NV_FALSE;
    }
    return NvSuccess;
}

static CPresult Nvmm_PauseCaching(CPhandle hContent, CPbool bPause)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;

    if (pFileCache->bCachingPaused != bPause)
    {
        //NvOsDebugPrintf("Nvmm_PauseCaching %d", bPause);

        pFileCache->bCachingPaused = bPause;

        if (!pFileCache->bCachingPaused)
        {
            NvOsSemaphoreSignal(pFileCache->WorkerSema);
        }

        if (pFileCache->bIsStreaming && pFileCache->nProtocolVersion >= 2)
        {
            pFileCache->pProtocol->SetPause(pFileCache->hFile, bPause);
        }
    }

    return NvSuccess;
}

static CPresult Nvmm_GetPositionEx(CPhandle hContent, CP_PositionInfoType* pPosition)
{
    NvU32 pReadOffset, iWriteBlock;
    FileChunk *pCurrentChunk, *pPrevChunk, *pNextChunk;
    NvS32 j,k;
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;

    if (!pFileCache->bInitialized)
       return NvSuccess;
    NvOsMutexLock(pFileCache->ChunkLock);
//B
    pPosition->nDataBegin = 0;
//E
    pPosition->nDataEnd = pFileCache->FileSize;
// C
    // determine write block where read ptr resides then use its writepos to compute readpos
    pReadOffset = (NvU32)(pFileCache->pRead-pFileCache->pBits);
    if ((pReadOffset == (NvU32)pFileCache->FileSize) && 
        (pFileCache->CacheSize == pFileCache->FileSize) &&
        !pFileCache->bIsStreaming)
    {
        iWriteBlock = pFileCache->TotalFileChunks - 1;
    }
    else
    {
        iWriteBlock = pReadOffset/pFileCache->ChunkSize;
    }
    pPosition->nDataCur = pFileCache->FileChunks[iWriteBlock].position + (pReadOffset - (pFileCache->ChunkSize*iWriteBlock));
// L
    pPosition->nDataLast = pFileCache->FileChunks[iWriteBlock].position + pFileCache->FileChunks[iWriteBlock].size;
    pPrevChunk = &pFileCache->FileChunks[iWriteBlock];
    for (j = iWriteBlock + 1; j < (NvS32)pFileCache->TotalFileChunks; j++ )
    {
        pCurrentChunk = &pFileCache->FileChunks[j];
        if ((pCurrentChunk->status == FULL) &&
            (pCurrentChunk->position == pPrevChunk->position + pPrevChunk->size))
        {
            pPosition->nDataLast += pCurrentChunk->size;
            pPrevChunk = pCurrentChunk;
        }
        else
        {
            break;
        }
    }
    if (j == (NvS32)pFileCache->TotalFileChunks)
    {
        pPrevChunk = &pFileCache->FileChunks[j - 1];
        for (k = 0; k < (NvS32)iWriteBlock; k++)
        {
            pCurrentChunk = &pFileCache->FileChunks[k];
            if ((pCurrentChunk->status == FULL) &&
                (pCurrentChunk->position == pPrevChunk->position + pPrevChunk->size))
            {
                pPosition->nDataLast += pCurrentChunk->size;
                pPrevChunk = pCurrentChunk;
            }
            else
            {
                break;
            }
        }
    }
// F
    pPosition->nDataFirst = pFileCache->FileChunks[iWriteBlock].position;
    pNextChunk = &pFileCache->FileChunks[iWriteBlock];
    for (j = iWriteBlock - 1; j > 0; j--)
    {
        pCurrentChunk = &pFileCache->FileChunks[j];
        if ((pCurrentChunk->status == FULL) &&
            (pNextChunk->position == pCurrentChunk->position + pCurrentChunk->size))
        {
            pPosition->nDataFirst -= pCurrentChunk->size;
            pNextChunk = pCurrentChunk;
        }
        else
        {
            break;
        }
    }
    if (j == 0)
    {
        pNextChunk = &pFileCache->FileChunks[j];
        for (k = pFileCache->TotalFileChunks - 1; k > (NvS32)iWriteBlock; k--)
        {
            pCurrentChunk = &pFileCache->FileChunks[k];
            if ((pCurrentChunk->status == FULL) &&
                (pNextChunk->position == pCurrentChunk->position + pCurrentChunk->size))
            {
                pPosition->nDataFirst -= pCurrentChunk->size;
                pNextChunk = pCurrentChunk;
            }
            else
            {
                break;
            }
        }
    }

    if ((pFileCache->bEOS) && (pPosition->nDataEnd == (NvU64)-1))
    {
        pPosition->nDataEnd = pPosition->nDataLast;
    }

    if (pPosition->nDataLast - pPosition->nDataCur < 1536 * 1024)
    {
        //NvOsDebugPrintf("start caching again");
        NvOsSemaphoreSignal(pFileCache->WorkerSema);
    }

    NvOsMutexUnlock(pFileCache->ChunkLock);
    return NvSuccess;
}

static CPresult Nvmm_GetCPConfig(CPhandle hContent, CP_ConfigIndex nIndex, void *pConfigStructure)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    NvCPR_ConfigQueryType configType = NvCPR_ConfigMax;
    NvU32 size = 0;
    CP_ConfigProp *rtcpProp = NULL;
    switch (nIndex)
    {
        case CP_ConfigIndexCacheSize:
        {
            *(NvU32 *)pConfigStructure = (NvU32)pFileCache->CacheSize;
            break;
        }
       case CP_ConfigIndexThreshold:
       {
                NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_WARN,"Nvmm_GetCPConfig:: CP_ConfigIndexThreshold case, not handled %x\n", nIndex));
                break;
       }
       case CP_ConfigQueryMetaInterval:
        {
            pFileCache->pProtocol->QueryConfig(pFileCache->hFile, 
                                               NvCPR_ConfigGetMetaInterval, 
                                               (NvU32 *)pConfigStructure,
                                               sizeof(NvU32));
            break;
        }
        case CP_ConfigQueryActualSeekTime:
        {
            pFileCache->pProtocol->QueryConfig(pFileCache->hFile, 
                                               NvCPR_GetActualSeekTime, 
                                               (NvU64 *)pConfigStructure,
                                               sizeof(NvU64));
            break;
        }
        case CP_ConfigQueryTimeStamps:
        {
            pFileCache->pProtocol->QueryConfig(pFileCache->hFile,
                                               NvCPR_GetTimeStamps,
                                               (CP_QueryConfigTS *)pConfigStructure,
                                               sizeof(CP_QueryConfigTS));

            break;
        }
        case CP_ConfigQueryRTCPAppData:
        {
            configType = NvCPR_GetRTCPAppData;
            break;
        }
        case CP_ConfigQueryRTCPSdesCname:
        {
            configType = NvCPR_GetRTCPSdesCname;
            break;
        }
        case CP_ConfigQueryRTCPSdesName:
        {
            configType = NvCPR_GetRTCPSdesName;
            break;
        }
        case CP_ConfigQueryRTCPSdesEmail:
        {
            configType = NvCPR_GetRTCPSdesEmail;
            break;
        }
        case CP_ConfigQueryRTCPSdesPhone:
        {
            configType = NvCPR_GetRTCPSdesPhone;
            break;
        }
        case CP_ConfigQueryRTCPSdesLoc:
        {
            configType = NvCPR_GetRTCPSdesLoc;
            break;
        }
        case CP_ConfigQueryRTCPSdesTool:
        {
            configType = NvCPR_GetRTCPSdesTool;
            break;
        }
        case CP_ConfigQueryRTCPSdesNote:
        {
            configType = NvCPR_GetRTCPSdesNote;
            break;
        }
        case CP_ConfigQueryRTCPSdesPriv:
        {
            configType = NvCPR_GetRTCPSdesPriv;
            break;
        }
        default:
        break;
    }

    if (configType != NvCPR_ConfigMax)//This is specific to SDES data only
    {
        rtcpProp = (CP_ConfigProp *)pConfigStructure;
        pFileCache->pProtocol->QueryConfig(pFileCache->hFile,
                                           configType,
                                           (void *)&size,
                                           0);
        if ( rtcpProp->propSize < size)
            rtcpProp->propSize = size;
        else
        {
            pFileCache->pProtocol->QueryConfig(pFileCache->hFile,
                                           configType,
                                           (void *)rtcpProp->prop,
                                           size);
        }
    }
    return NvSuccess;
}

static CPresult Nvmm_SetCPConfig(CPhandle hContent, CP_ConfigIndex nIndex, void *pConfigStructure)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    switch (nIndex)
    {
        case CP_ConfigIndexThreshold:
        {
            CP_ConfigThreshold *pThreshold = (CP_ConfigThreshold *)pConfigStructure;
            if(pFileCache->WriteLock)
               NvOsMutexLock(pFileCache->WriteLock);
            pFileCache->HighMark = pThreshold->HighMark;
            pFileCache->ReadTriggerThreshold = pThreshold->LowMark;
            if(pFileCache->WriteLock)
               NvOsMutexUnlock(pFileCache->WriteLock);
            break;
        }
        case CP_ConfigIndexCacheSize:
        {
                NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_WARN,"Nvmm_SetCPConfig:: CP_ConfigIndexCacheSize case, not handled %x\n", nIndex));
                break;
        }
        case CP_ConfigQueryMetaInterval:
        {
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_WARN, "Nvmm_SetCPConfig:: CP_ConfigQueryMetaInterval case, not handled %x\n", nIndex));
            break;
        }
        case CP_ConfigQueryActualSeekTime:
        {
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_WARN, "Nvmm_SetCPConfig:: CP_ConfigQueryActualSeekTime case, not handled %x\n", nIndex));
            break;
        }
        default:
        break;
    }

    return NvSuccess;
}

static CPresult Nvmm_StopCaching(CPhandle hContent)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    NvOsMutexLock(pFileCache->WriteLock);
    pFileCache->bStop = NV_TRUE;
    NvOsMutexUnlock(pFileCache->WriteLock);
    return NvSuccess;
}

static CPresult Nvmm_StartCaching(CPhandle hContent)
{
    NvmmFileCache *pFileCache = (NvmmFileCache *)hContent;
    pFileCache->bStop = NV_FALSE;
    if (pFileCache->CacheSize)
    {
        NvOsSemaphoreSignal(pFileCache->WorkerSema);
    }
    return NvSuccess;
}

static CP_PIPETYPE_EXTENDED s_oNvxLocalFileContentPipe = {
    {
        Nvmm_FileOpen,
            Nvmm_FileClose,
            Nvmm_FileCreate,
            Nvmm_FileCheckAvailableBytes,
            Nvmm_FileSetPosition,
            Nvmm_FileGetPosition,
            Nvmm_FileRead,
            Nvmm_FileReadBuffer,
            Nvmm_FileReleaseReadBuffer,
            Nvmm_FileWrite,
            Nvmm_FileGetWriteBuffer,
            Nvmm_FileWriteBuffer,
            Nvmm_FileRegisterCallback
    },
    Nvmm_FileSetPosition64,
    Nvmm_FileGetPosition64,
    Nvmm_FileGetBaseAddress,
    Nvmm_FileRegisterClientCallback,
    Nvmm_FileGetAvailableBytes,
    Nvmm_FileGetSize,
    NULL,
    Nvmm_InitializeCP,
    Nvmm_IsStreaming,
    Nvmm_InvalidateCache,
    Nvmm_PauseCaching,
    Nvmm_GetPositionEx,
    Nvmm_GetCPConfig,
    Nvmm_SetCPConfig,
    Nvmm_StopCaching,
    Nvmm_StartCaching
};

void NvmmGetFileContentPipe(CP_PIPETYPE_EXTENDED **pipe)
{
    *pipe = &s_oNvxLocalFileContentPipe;
}
