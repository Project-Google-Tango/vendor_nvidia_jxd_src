/* Copyright (c) 2011-2012, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvparserhal_content_pipe.h"

namespace android {

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************************/
static CPresult NvParserHal_FileOpen(CPhandle* hContent, CPstring szURI, CP_ACCESSTYPE eAccess)
{
    NvError e;
    char *s = NULL;
    int fd;

    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)NvOsAlloc(sizeof(NvParserHalFileCache));
    if(!pFileCache)
    {
        e = NvError_InsufficientMemory;
        goto CleanUp;
    }
    NvOsMemset(pFileCache, 0, sizeof(NvParserHalFileCache));

    // strip protocol off
    s = strchr(szURI, ':');
    if(s)
    {
        int count = 0;
        s++;
        while (*s == '/' && count < 2)
        {
            s++;
            count++;
        }
    }
    else
        s = szURI;

    if(1 != sscanf(s, "%x", &fd))
    {
        e = NvError_FileNotFound;
        goto CleanUp;
    }

    // Got the data source handle
    pFileCache->mDataSource = (DataSource *)fd;
    pFileCache->nFileOffset = 0;

    if(!(pFileCache->mDataSource->flags() & DataSource::kIsLocalDataSource))
        pFileCache->bIsStreaming = NV_TRUE;

    // Get the file size
    pFileCache->mDataSource->getSize(&pFileCache->FileSize);

    *hContent = (CPhandle*)pFileCache;
    return NvSuccess;
CleanUp:
    if(pFileCache)
        NvOsFree(pFileCache);
    *hContent = NULL;
    return e;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileClose(CPhandle hContent)
{
    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)hContent;

    NvOsFree(pFileCache);
    pFileCache = NULL;
    return NvSuccess;
}

/*********************************************************************************************************/
static  CPresult NvParserHal_FileCreate( CPhandle *hContent,CPstring szURI)
{
    return NvError_NotImplemented;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileCheckAvailableBytes( CPhandle hContent, CPuint nBytesRequested, CP_CHECKBYTESRESULTTYPE *eResult )
{
    NvError status = NvSuccess;
    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)hContent;

    if((pFileCache->FileSize - pFileCache->nFileOffset) > nBytesRequested)
        *eResult = CP_CheckBytesOk;
    else
        *eResult = CP_CheckBytesInsufficientBytes;

    return status;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileSetPosition( CPhandle  hContent, CPint nOffset, CP_ORIGINTYPE eOrigin)
{
    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)hContent;

    if (eOrigin == CP_OriginCur)
    {
         pFileCache->nFileOffset +=  (NvU64)nOffset;
    }
    else if (eOrigin == CP_OriginBegin)
    {
        pFileCache->nFileOffset = nOffset;
    }
    else if(eOrigin == CP_OriginEnd)
    {
        pFileCache->nFileOffset = pFileCache->FileSize + nOffset;
    }
    else
    {
        return NvError_NotSupported;
    }

    if(pFileCache->nFileOffset >= (NvU64)pFileCache->FileSize)
    {
        pFileCache->nFileOffset = (NvU64)pFileCache->FileSize;
        return NvError_EndOfFile;
    }
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileGetPosition( CPhandle hContent,CPuint *pPosition)
{
    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)hContent;

    *pPosition = pFileCache->nFileOffset;
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileRead( CPhandle hContent, CPbyte *pData, CPuint nSize)
{
    ssize_t nBytesRead;
    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)hContent;

    nBytesRead = pFileCache->mDataSource->readAt(pFileCache->nFileOffset,pData,nSize);
    pFileCache->nFileOffset += nBytesRead;

    if(nBytesRead < (ssize_t)nSize)
    {
        return NvError_EndOfFile;
    }
    else
        return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileReadBuffer( CPhandle hContent, CPbyte **ppBuffer, CPuint *nSize, CPbool bForbidCopy)
{
    return NvError_NotImplemented;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileReleaseReadBuffer(CPhandle hContent, CPbyte *pBuffer)
{
    return NvError_NotImplemented;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileWrite( CPhandle hContent, CPbyte *pData, CPuint nSize)
{
    //Data source does not support writing
    return NvError_FileNotFound;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileGetWriteBuffer(CPhandle hContent, CPbyte **ppBuffer, CPuint nSize)
{
    return NvError_NotImplemented;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileWriteBuffer(CPhandle hContent, CPbyte *pBuffer, CPuint nFilledSize)
{
    return NvError_NotImplemented;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileRegisterCallback(CPhandle hContent, CPresult (*ClientCallback)(CP_EVENTTYPE eEvent, CPuint iParam))
{
    return NvError_NotImplemented;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileSetPosition64(CPhandle  hContent, CPint64 nOffset, CP_ORIGINTYPE eOrigin)
{
    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)hContent;

    if (eOrigin == CP_OriginCur)
    {
         pFileCache->nFileOffset +=  (NvU64)nOffset;
    }
    else if (eOrigin == CP_OriginBegin)
    {
        pFileCache->nFileOffset = nOffset;
    }
    else if(eOrigin == CP_OriginEnd)
    {
        pFileCache->nFileOffset = pFileCache->FileSize + nOffset;
    }
    else
    {
        return NvError_NotSupported;
    }

    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileGetPosition64( CPhandle hContent, CPuint64 *pPosition)
{
    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)hContent;

    *pPosition = pFileCache->nFileOffset;
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileGetBaseAddress(CPhandle hContent, NvRmPhysAddr *pPhyAddress, void **pVirtAddress)
{
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileRegisterClientCallback( CPhandle hContent, void *pClientContext, ClientCallbackFunction ClientCallback)
{
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileGetAvailableBytes(CPhandle hContent, CPuint64 *nBytesAvailable)
{
    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)hContent;
    *nBytesAvailable = (pFileCache->FileSize - pFileCache->nFileOffset);
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_FileGetSize(CPhandle hContent, CPuint64 *pFileSize)
{
    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)hContent;
    *pFileSize = pFileCache->FileSize;
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_InitializeCP(CPhandle hContent, CPuint64 MinCacheSize, CPuint64 MaxCacheSize, CPuint32 SpareAreaSize, CPuint64* pActualSize)
{
    return NvSuccess;
}

/*********************************************************************************************************/
static CPbool NvParserHal_IsStreaming(CPhandle hContent)
{
    NvParserHalFileCache *pFileCache = (NvParserHalFileCache *)hContent;
    return (CPbool) pFileCache->bIsStreaming;
}

/*********************************************************************************************************/
static CPresult NvParserHal_InvalidateCache(CPhandle hContent)
{
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_PauseCaching(CPhandle hContent, CPbool bPause)
{
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_GetPositionEx(CPhandle hContent, CP_PositionInfoType* pPosition)
{
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_GetCPConfig(CPhandle hContent, CP_ConfigIndex nIndex, void *pConfigStructure)
{
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_SetCPConfig(CPhandle hContent, CP_ConfigIndex nIndex, void *pConfigStructure)
{
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_StopCaching(CPhandle hContent)
{
    return NvSuccess;
}

/*********************************************************************************************************/
static CPresult NvParserHal_StartCaching(CPhandle hContent)
{
    return NvSuccess;
}

/*********************************************************************************************************/
static CP_PIPETYPE_EXTENDED s_oNVHALLocalFileContentPipe = {
    {
        NvParserHal_FileOpen,
        NvParserHal_FileClose,
        NvParserHal_FileCreate,
        NvParserHal_FileCheckAvailableBytes,
        NvParserHal_FileSetPosition,
        NvParserHal_FileGetPosition,
        NvParserHal_FileRead,
        NvParserHal_FileReadBuffer,
        NvParserHal_FileReleaseReadBuffer,
        NvParserHal_FileWrite,
        NvParserHal_FileGetWriteBuffer,
        NvParserHal_FileWriteBuffer,
        NvParserHal_FileRegisterCallback
    },
    NvParserHal_FileSetPosition64,
    NvParserHal_FileGetPosition64,
    NvParserHal_FileGetBaseAddress,
    NvParserHal_FileRegisterClientCallback,
    NvParserHal_FileGetAvailableBytes,
    NvParserHal_FileGetSize,
    NULL,
    NvParserHal_InitializeCP,
    NvParserHal_IsStreaming,
    NvParserHal_InvalidateCache,
    NvParserHal_PauseCaching,
    NvParserHal_GetPositionEx,
    NvParserHal_GetCPConfig,
    NvParserHal_SetCPConfig,
    NvParserHal_StopCaching,
    NvParserHal_StartCaching
};

/*********************************************************************************************************/
void NvParserHal_GetFileContentPipe(CP_PIPETYPE_EXTENDED **pipe)
{
    *pipe = &s_oNVHALLocalFileContentPipe;
}

#ifdef __cplusplus
}
#endif

} //namespace android
