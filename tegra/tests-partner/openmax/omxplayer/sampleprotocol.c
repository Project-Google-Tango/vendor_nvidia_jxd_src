/* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifdef _WIN32
#pragma warning( disable : 4996 )
#endif

#include "NVOMX_CustomProtocol.h"
#include "OMX_Core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nvcommon.h"

#include "nvos.h"

typedef struct SampleHandle
{
    FILE *hFile;
    NvU64 nSize;
} SampleHandle;

static NvError SampleGetVersion(NvS32 *pnVersion)
{
    *pnVersion = NV_CUSTOM_PROTOCOL_VERSION;
    return NvSuccess;
}

static NvError SampleOpen(NvCPRHandle* hHandle, char *szURI, 
                          NvCPR_AccessType eAccess)
{
    SampleHandle *pFile;
    const char *flags = "rb";
    char *s;

    if (eAccess == NvCPR_AccessWrite)
        flags = "wb+";
    else if (eAccess == NvCPR_AccessRead)
        flags = "rb";
    else if (eAccess == NvCPR_AccessReadWrite)
        flags = "rb+";
    else
        return OMX_ErrorBadParameter;
    
    // strip protocol off
    s = strchr(szURI, ':');
    if (s)
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

    pFile = malloc(sizeof(SampleHandle));
    if (!pFile)
        return OMX_ErrorInsufficientResources;

    memset(pFile, 0, sizeof(SampleHandle));

    NvOsDebugPrintf("opening: %s\n", s);
    pFile->hFile = fopen(s, flags);
    if (!pFile->hFile)
        goto fail;

    fseek(pFile->hFile, 0, SEEK_END);
    pFile->nSize = ftell(pFile->hFile);
    fseek(pFile->hFile, 0, SEEK_SET);

    *hHandle = pFile;
    return NvSuccess;

fail:
    if (pFile->hFile)
        fclose(pFile->hFile);
    free(pFile);
    return OMX_ErrorContentPipeOpenFailed;
}

static NvError SampleClose(NvCPRHandle hContent)
{
    SampleHandle *pFile = (SampleHandle *)hContent;

    if (pFile->hFile)
        fclose(pFile->hFile);
    free(pFile);
    return NvSuccess;
}

static NvError SampleSetPosition(NvCPRHandle hContent, NvS64 nOffset, 
                                 NvCPR_OriginType eOrigin)
{
    SampleHandle *pFile = (SampleHandle *)hContent;
    int whence;

    if (eOrigin == NvCPR_OriginBegin)
        whence = SEEK_SET;
    else if (eOrigin == NvCPR_OriginCur)
        whence = SEEK_CUR;
    else if (eOrigin == NvCPR_OriginEnd)
        whence = SEEK_END;
    else
        return OMX_ErrorBadParameter;

    fseek(pFile->hFile, (int)nOffset, whence);
    return NvSuccess;
}

static NvError SampleGetPosition(NvCPRHandle hContent, NvU64 *pPosition)
{
    SampleHandle *pFile = (SampleHandle *)hContent;

    *pPosition = ftell(pFile->hFile);
    return NvSuccess;
}

static NvU32 SampleRead(NvCPRHandle hContent, NvU8 *pData, NvU32 nSize)
{
    SampleHandle *pFile = (SampleHandle *)hContent;

    NvOsDebugPrintf("reading: %d bytes\n", nSize);
    return fread(pData, 1, nSize, pFile->hFile);
}

static NvU32 SampleWrite(NvCPRHandle hContent, NvU8 *pData, NvU32 nSize)
{
    SampleHandle *pFile = (SampleHandle *)hContent;

    return fwrite(pData, 1, nSize, pFile->hFile);
}

static NvError SampleGetSize(NvCPRHandle hContent, NvU64 *pFileSize)
{
    SampleHandle *pFile = (SampleHandle *)hContent;

    *pFileSize = pFile->nSize;
    return NvSuccess;
}

static NvBool SampleIsStreaming(NvCPRHandle hContent)
{
    return NV_FALSE;
}

static NvParserCoreType SampleProbeParserType(char *szURI)
{
    return NvParserCoreType_UnKnown;
}

static NvError SampleQueryConfig(NvCPRHandle hContent, 
                                 NvCPR_ConfigQueryType eQuery,
                                 void *pData, int nDataSize)
{
    switch (eQuery)
    {
        case NvCPR_ConfigPreBufferAmount:
        {
            if (nDataSize != sizeof(NvU32))
                return NvError_BadParameter;
            *((NvU32*)pData) = 0;
            return NvSuccess;
        }
        case NvCPR_ConfigCanSeekByTime:
        {
            if (nDataSize != sizeof(NvU32))
                return NvError_BadParameter;
            *((NvU32*)pData) = 0;
            return NvSuccess;
        }
        default:
            break;
    }

    return NvError_NotSupported;
}

static NvError SampleSetPause(NvCPRHandle hContent, int bPause)
{
    return NvError_NotSupported;
}

static NV_CUSTOM_PROTOCOL s_SampleProtocol =
{
    SampleGetVersion,
    SampleOpen,
    SampleClose,
    SampleSetPosition,
    SampleGetPosition,
    SampleRead,
    SampleWrite,
    SampleGetSize,
    SampleIsStreaming,
    SampleProbeParserType,
    SampleQueryConfig,
    SampleSetPause
};

OMX_API NV_CUSTOM_PROTOCOL *GetSampleProtocol(void);
OMX_API NV_CUSTOM_PROTOCOL *GetSampleProtocol(void)
{
    return &s_SampleProtocol;
}

