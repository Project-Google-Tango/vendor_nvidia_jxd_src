/* Copyright (c) 2006-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NVContentPipe.c
 *
 *  NVIDIA's implementation of the OMX Content Pipes.
 */
 
#include <stdio.h>   /* for file handling operations */
#include "nvlocalfilecontentpipe.h"
#include "nvmm_bufmgr.h"
#include "nvassert.h"
#include "nvos.h"
#include <stdlib.h>  /* malloc, null, etc */
#include <string.h>  /* for strcpy/cmp, etc. */
#include "nvrm_hardware_access.h"
#include "nvmm_ulp_kpi_logger.h"
#define LOCAL_CP_KPI_ENABLED 0

typedef struct NvMMContentPipeRec
{
    NvOsFileHandle      hFileHandle;
    NvOsMutexHandle     hCpLock;
    NvRmDeviceHandle    hRmDevice;

    NvMMBufMgrHandle    hBufferManager;
    NvU32               CacheSize;
    NvU64               FileSize;
    NvRmMemHandle       hMemHandle;
    NvRmPhysAddr        pBufferManagerBaseAddress;
    void *              vBufferManagerBaseAddress;
    // Represents the end position of the file
    NvU64 FileEndPosition;
} NvMMContentPipe;


static CPresult UlpFileOpen(CPhandle* hContent, CPstring szURI, CP_ACCESSTYPE eAcces)
{
    NvError status = NvSuccess;
    OMX_U32 flags;
    NvOsStatType FileStats;

    NvMMContentPipe *pContentPipe = NvOsAlloc(sizeof(NvMMContentPipe));
    if(!pContentPipe)
    {
        return NvError_InsufficientMemory;
    }
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif
    NvOsMemset(pContentPipe, 0, sizeof(NvMMContentPipe));
    status = NvOsMutexCreate(&pContentPipe->hCpLock);
    if(status != NvSuccess)
    {
        NvOsMutexDestroy(pContentPipe->hCpLock);
        NvOsFree(pContentPipe);
        goto openEnd;
    }
    status = NvOsStat(szURI, &FileStats);
    if (status != NvSuccess)
    {
        NV_DEBUG_PRINTF(("Error: unable to Get File status!\n"));
    }
    pContentPipe->CacheSize = (NvU32)FileStats.size;
    pContentPipe->FileSize = FileStats.size;
    switch (eAcces)
    {
        case CP_AccessWrite:
            flags =    NVOS_OPEN_CREATE| NVOS_OPEN_WRITE;
            status = NvOsFopen(szURI, (NvU32)flags, &pContentPipe->hFileHandle);
            if (status != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Error: unable to Open the Input file-CP_AccessWrite!\n"));
            }
            break;

        case CP_AccessRead:
            status = NvOsFopen(szURI, NVOS_OPEN_READ, &pContentPipe->hFileHandle);
            if (status != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Error: unable to Open the Input file-CP_AccessRead!\n"));
            }
            break;

        case CP_AccessReadWrite:
            status = NvOsFopen(szURI, NVOS_OPEN_CREATE, &pContentPipe->hFileHandle);
            if (status != NvSuccess)
            {
                NV_DEBUG_PRINTF(("Error: unable to Open the Input file-CP_AccessReadWrite!\n"));
            }
            break;

        default:
            status = NvError_NotSupported;
    }
    *hContent = pContentPipe;

    if (status != NvSuccess)
    {
        NvOsMutexDestroy(pContentPipe->hCpLock);
        NvOsFree(pContentPipe);
        *hContent = NULL;
        goto openEnd;
    }

    //Get the position at the End of the file and save it to context
    status = NvOsFseek(pContentPipe->hFileHandle, 0, SEEK_END);
    if(status != NvSuccess)
    {
        goto openEnd;
    }
    status = NvOsFtell(pContentPipe->hFileHandle, &pContentPipe->FileEndPosition);
    if(status != NvSuccess)
    {
        goto openEnd;
    }
    //Reset the position of the file
    status = NvOsFseek(pContentPipe->hFileHandle, 0, SEEK_SET);
    if(status != NvSuccess)
    {
        goto openEnd;
    }

openEnd:

#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return status;
}


static CPresult UlpFileClose(CPhandle hContent)
{
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;

#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif
    NvOsFclose(pContentPipe->hFileHandle);
    NvOsMutexDestroy(pContentPipe->hCpLock);

    if(pContentPipe->hBufferManager)
    {
        NvMMBufMgrDeinit(pContentPipe->hBufferManager);
        if (pContentPipe->hMemHandle != NULL)
        {
            NvRmMemUnmap(pContentPipe->hMemHandle, pContentPipe->vBufferManagerBaseAddress, pContentPipe->CacheSize);
            NvRmMemUnpin(pContentPipe->hMemHandle);
            NvRmMemHandleFree(pContentPipe->hMemHandle);
        }
        NvRmClose(pContentPipe->hRmDevice);
    }
    NvOsFree(pContentPipe);

#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
   return NvSuccess;
}

static CPresult UlpFileRead(CPhandle hContent, CPbyte *pData, CPuint nSize)
{
    NvError status = NvSuccess;
    OMX_U32 readsize = 0;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis((KpiFlag_FileIOStart |KpiFlag_ReadStart), NV_FALSE, NV_FALSE, readsize);
#endif

    status = NvOsFread(pContentPipe->hFileHandle, (void*)pData, nSize, (size_t *)&readsize);
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis((KpiFlag_FileIOEnd | KpiFlag_ReadEnd), NV_FALSE, NV_FALSE, readsize);
#endif
    if(status != NvSuccess)
    {
       return status;
    }
    else
    {
        return NvSuccess;  
    }
}

static CPresult UlpFileWrite(CPhandle hContent, CPbyte *pData, CPuint nSize)
{
    NvError status = NvSuccess;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    status = NvOsFwrite(pContentPipe->hFileHandle, pData, nSize);
    //NvOsFflush(pContentPipe->hFileHandle);
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif

    return status;
}

static CPresult UlpFileSetPosition(CPhandle hContent, CPint nOffset, CP_ORIGINTYPE eOrigin)
{
    NvError status = NvSuccess;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    switch (eOrigin)
    {
        case CP_OriginBegin:
            status = NvOsFseek(pContentPipe->hFileHandle, nOffset, SEEK_SET);
            break;

        case  CP_OriginCur:
            status = NvOsFseek(pContentPipe->hFileHandle, nOffset, SEEK_CUR);
            break;

        case  CP_OriginEnd:
            status = NvOsFseek(pContentPipe->hFileHandle, nOffset, SEEK_END);
            break;

        default:
            status = NvError_NotSupported;
    }

#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return status;
}

 static CPresult UlpFileSetPosition64(CPhandle hContent, CPint64 nOffset, CP_ORIGINTYPE eOrigin)
{
    NvError status = NvSuccess;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    switch (eOrigin)
    {
        case CP_OriginBegin:
            status = NvOsFseek(pContentPipe->hFileHandle, nOffset, SEEK_SET);
            break;

        case CP_OriginCur:
            status = NvOsFseek(pContentPipe->hFileHandle, nOffset, SEEK_CUR);
            break;

        case CP_OriginEnd:
            status = NvOsFseek(pContentPipe->hFileHandle, nOffset, SEEK_END);
            break;

        default:
            status = NvError_NotSupported;
    }

#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
return status;
}
static CPresult UlpFileGetPosition(CPhandle hContent,CPuint *pPosition)
{
    NvError status = NvSuccess;
    NvU64 pPos;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    status = NvOsFtell(pContentPipe->hFileHandle, (NvU64*)&pPos);
    *pPosition = (CPuint)pPos;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif

    return status;
}

static CPresult UlpFileGetPosition64(CPhandle hContent, CPuint64 *pPosition)
{
    NvError status = NvSuccess;
    NvU64 pPos;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    status = NvOsFtell(pContentPipe->hFileHandle, &pPos);
    *pPosition = pPos;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif

    return status;
}

static CPresult UlpFileGetAvailableBytes(CPhandle hContent, CPuint64 *nBytesAvailable)
{
    NvError status = NvSuccess;
    NvU64 currentposition  = 0;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;

#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    status = NvOsFtell(pContentPipe->hFileHandle, &currentposition);

#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif

    if(status != NvSuccess)
        *nBytesAvailable = 0;
    else
        *nBytesAvailable = pContentPipe->FileEndPosition - currentposition;

    return status;
}

static CPresult UlpFileCheckAvailableBytes(CPhandle hContent, CPuint nBytesRequested, CP_CHECKBYTESRESULTTYPE *eResult)
{
    NvError status = NvSuccess;
    NvU64 NumOfBytesAvailable = 0;

    status = UlpFileGetAvailableBytes(hContent, &NumOfBytesAvailable);

    if(status != NvSuccess)
        goto ReturnStatus;
    
    if(NumOfBytesAvailable >= nBytesRequested)
    {
        *eResult = CP_CheckBytesOk;
    }
    else if(NumOfBytesAvailable == 0)
    {
        *eResult = CP_CheckBytesAtEndOfStream;
    }
    else
    {
        *eResult = CP_CheckBytesInsufficientBytes;
    }

ReturnStatus:
    
    return status;

}

static CPresult UlpFileGetSize(CPhandle hContent, CPuint64 *pFileSize)
{
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
    if (pFileSize)
    {
        *pFileSize = pContentPipe->FileSize;
    }
    return NvSuccess;
}


static CPresult UlpFileReadBuffer(CPhandle hContent, CPbyte **ppBuffer, CPuint *nSize, CPbool bForbidCopy)
{
    /** Retrieve a buffer allocated by the pipe that contains the requested number of bytes.
       Buffer contains the next block of bytes, as specified by nSize, of the content. nSize also
       returns the size of the block actually read. Content pointer advances the by the returned size.
       Note: pipe provides pointer. This function is appropriate for large reads. The client must call
       ReleaseReadBuffer when done with buffer.

       In some cases the requested block may not reside in contiguous memory within the
       pipe implementation. For instance if the pipe leverages a circular buffer then the requested
       block may straddle the boundary of the circular buffer. By default a pipe implementation
       performs a copy in this case to provide the block to the pipe client in one contiguous buffer.
       If, however, the client sets bForbidCopy, then the pipe returns only those bytes preceding the memory
       boundary. Here the client may retrieve the data in segments over successive calls. */

    NvError status = NvSuccess;
    NvU32 readsize = 0;
    NvU64 TotalBytesAvailable = 0, BytesRequested = 0;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;

    //Check Available Bytes in the file from the current file postion.
    status = UlpFileGetAvailableBytes(hContent, &TotalBytesAvailable);
    if(status != NvSuccess) 
    {
        *nSize = 0;
        return status;
    }
    // If there are no bytes available from the current position, return EOS.
    if(TotalBytesAvailable == 0) 
    {
        *nSize = 0;
        return CP_CheckBytesAtEndOfStream;
    }

    // Take a temporary variable to handle the TotalBytesAvailable (NvU64) and *nSize (NvU32) incompatabililty.
    BytesRequested = *nSize;
    // If bytes requested are more than bytes available, adjust read size to bytes available
    if(BytesRequested > TotalBytesAvailable)
    {
        *nSize = (NvU32)TotalBytesAvailable;
    }

#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    if(0 != *nSize)
    {
        CPuint freeMem = NvMMBufMgrGetTotalFreeMemoryAvailable(pContentPipe->hBufferManager);
        if(freeMem)
        {
            if(freeMem < *nSize)
            {
                *nSize = NvMMBufMgrGetLargestFreeChunkAvailable(pContentPipe->hBufferManager);
            }
            *ppBuffer = (CPbyte*)NvMMBufMgrAlloc(pContentPipe->hBufferManager, *nSize, 4);
            if(*ppBuffer == NULL)
            {
                *nSize = 0;
                status = NvError_InsufficientMemory;
                goto readBufferStatus;
            }

#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_ReadStart, NV_FALSE, NV_FALSE, readsize);
#endif
            status = NvOsFread(pContentPipe->hFileHandle, (void*)*ppBuffer, *nSize, (size_t *)&readsize);
 #if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_ReadEnd, NV_FALSE, NV_FALSE, readsize);
#endif
           
            if(readsize <= 0)
            {
                status = CP_CheckBytesAtEndOfStream;
                goto readBufferStatus;
            }
            if(*nSize != readsize)
            {
                *nSize = readsize;
                NV_DEBUG_PRINTF(("Error: unable to read the localfilereadbuffer!\n"));
            }
        }
        else
        {
            NV_DEBUG_PRINTF(("Error: Inufficient Memory!\n"));
            *ppBuffer = NULL;
            *nSize = 0;
            status = NvError_InsufficientMemory;
        }
    }
    //NvOsDebugPrintf("ReadBuffer - %x, size - %d", *ppBuffer, *nSize);
readBufferStatus:
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return status;
}

static CPresult UlpFileReleaseReadBuffer(CPhandle hContent, CPbyte *pBuffer)
{
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    if(pBuffer) 
    {
        //NvOsDebugPrintf("ReleaseReadBuffer - %x", pBuffer);
        NvMMBufMgrFree(pContentPipe->hBufferManager, (NvU32)pBuffer);    
    }
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return NvSuccess;
}

static CPresult UlpFileGetWriteBuffer(CPhandle hContent, CPbyte **ppBuffer, CPuint nSize)
{
    NvError status = NvSuccess;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    if(NvMMBufMgrGetTotalFreeMemoryAvailable(pContentPipe->hBufferManager) >= nSize)
    {
        *ppBuffer = (char *)NvMMBufMgrAlloc(pContentPipe->hBufferManager, nSize, 4);
        if(*ppBuffer == NULL)
        {
            status = NvError_InsufficientMemory; // or use NV_ASSERT
        }
    }
    else
    {
        status = NvError_InsufficientMemory;
    }
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return status;
}

static CPresult UlpFileWriteBuffer(CPhandle hContent, CPbyte *pBuffer, CPuint nFilledSize)
{
    NvError status = NvSuccess;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    status = NvOsFwrite(pContentPipe->hFileHandle, pBuffer, nFilledSize);
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return status;
}

static CPresult UlpFileCallBack(CPhandle hContent, CPresult (*ClientCallback)(CP_EVENTTYPE eEvent, CPuint iParam))
{
    return NvSuccess;
}

static CPresult UlpFileCreate(CPhandle *hContent, CPstring szURI)
{
    NvError status = NvSuccess;

    NvMMContentPipe *pContentPipe = NvOsAlloc(sizeof(NvMMContentPipe));
    if(!pContentPipe)
    {
        *hContent = NULL;
        return NvError_InsufficientMemory;
    }
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif
    NvOsMemset(pContentPipe, 0, sizeof(NvMMContentPipe));
    status = NvOsMutexCreate(&pContentPipe->hCpLock);
    if (status != NvSuccess)
    {
        NvOsFree(pContentPipe);
        *hContent = NULL;
        goto LblCreateEnd;
    }

    status = NvOsFopen(szURI, NVOS_OPEN_CREATE, &pContentPipe->hFileHandle);
    if (status != NvSuccess)
    {
        NvOsMutexDestroy(pContentPipe->hCpLock);
        NvOsFree(pContentPipe);
        NV_DEBUG_PRINTF(("Error: unable to Open the Input file-CP_AccessWrite!\n"));
        *hContent = NULL;
        goto LblCreateEnd;
    }
    *hContent = pContentPipe;
LblCreateEnd:    
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return status;
}

static CPresult UlpFileGetBaseAddress(CPhandle hContent, NvRmPhysAddr *pPhyAddress, void **pVirtAddress)
{
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;

    *pPhyAddress  = pContentPipe->pBufferManagerBaseAddress;
    *pVirtAddress = pContentPipe->vBufferManagerBaseAddress;

    return NvSuccess;
}

static CPresult UlpFileCreateBufferManager(CPhandle* hContent, CPuint nSize)
{
    NvError status = NvSuccess;
    NvMMContentPipe *pContentPipe = (NvMMContentPipe *)hContent;
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOStart, NV_FALSE, NV_FALSE, 0);
#endif

    status = NvRmOpen(&pContentPipe->hRmDevice, 0);
    if(status != NvSuccess)
    {
        goto FileCreateExit;
    }
    status = NvRmMemHandleAlloc(pContentPipe->hRmDevice, NULL, 0, 16,
            NvOsMemAttribute_Uncached, pContentPipe->CacheSize, 0, 0,
            &pContentPipe->hMemHandle);
    if(status != NvSuccess)
    {
        goto FileCreateExit;
    }
    pContentPipe->pBufferManagerBaseAddress = NvRmMemPin(pContentPipe->hMemHandle);
    status = NvRmMemMap(pContentPipe->hMemHandle, 
                        0, 
                        pContentPipe->CacheSize, 
                        NVOS_MEM_READ_WRITE, 
                        (void *)&pContentPipe->vBufferManagerBaseAddress);
    if(status != NvSuccess)
    {
        goto FileCreateExit;
    }
    if(nSize)
    {
        pContentPipe->CacheSize = nSize;
    }
    status = NvMMBufMgrInit(&pContentPipe->hBufferManager, 
                            pContentPipe->pBufferManagerBaseAddress, 
                            (NvU32)pContentPipe->vBufferManagerBaseAddress, 
                            pContentPipe->CacheSize);
    if(status != NvSuccess)
    {
        goto FileCreateExit;
    }

FileCreateExit:
#if LOCAL_CP_KPI_ENABLED
    NvmmUlpUpdateKpis(KpiFlag_FileIOEnd, NV_FALSE, NV_FALSE, 0);
#endif
    return status;
}

static CPbool UlpFileIsStreaming(CPhandle hContent)
{
    return 0;
}

static CPresult UlpPauseCaching(CPhandle hContent, CPbool bPause)
{
    return NvSuccess;
}

static CP_PIPETYPE_EXTENDED s_oNvxlocalFileExtndContentPipe = 
{
    { 
        UlpFileOpen,
        UlpFileClose,
        UlpFileCreate,
        UlpFileCheckAvailableBytes,
        UlpFileSetPosition,
        UlpFileGetPosition,
        UlpFileRead,
        UlpFileReadBuffer,
        UlpFileReleaseReadBuffer,
        UlpFileWrite,
        UlpFileGetWriteBuffer,
        UlpFileWriteBuffer,
        UlpFileCallBack 
    },

    UlpFileSetPosition64,
    UlpFileGetPosition64,
    UlpFileGetBaseAddress,
    NULL,
    UlpFileGetAvailableBytes,
    UlpFileGetSize,
    UlpFileCreateBufferManager,
    NULL,
    UlpFileIsStreaming,
    NULL,
    UlpPauseCaching,
    NULL,
    NULL,
    NULL,
    NULL
};

void NvMMGetContentPipe(CP_PIPETYPE_EXTENDED **pipe)
{
    *pipe = &s_oNvxlocalFileExtndContentPipe;
}
