/* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "nvcustomprotocol.h"
#include "nvmm_protocol_builtin.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvmm_logger.h"
#include "nvmm_common.h"

#define NVLOG_CLIENT NVLOG_CONTENT_PIPE // required for logging contentpipe traces

/* Default file reader */
 
typedef struct LocalFileHandle
{
    NvOsFileHandle hFile;
    NvU64 nSize;
} LocalFileHandle;

static NvError LocalFileGetVersion(NvS32 *pnVersion)
{
    *pnVersion = NV_CUSTOM_PROTOCOL_VERSION;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++--LocalFileGetVersion = NV_CUSTOM_PROTOCOL_VERSION"));
    return NvSuccess;
}

static NvError LocalFileClose(NvCPRHandle hContent)
{
    LocalFileHandle *pFile = (LocalFileHandle *)hContent;
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++LocalFileClose"));
    if (pFile)
    {
        if (pFile->hFile)
            NvOsFclose(pFile->hFile);
        NvOsFree(pFile);
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--LocalFileClose"));    
    return NvSuccess;
}

static NvError LocalFileOpen(NvCPRHandle* hHandle, char *szURI, 
                             NvCPR_AccessType eAccess)
{
    NvError Status = NvSuccess;
    LocalFileHandle *pFile = NULL;
    NvU32 flags = 0;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++LocalFileOpen"));
    
    NVMM_CHK_ARG (eAccess == NvCPR_AccessWrite || 
                                      eAccess == NvCPR_AccessRead ||
                                      eAccess == NvCPR_AccessReadWrite);
    
    if (eAccess == NvCPR_AccessWrite)
        flags = (NVOS_OPEN_CREATE | NVOS_OPEN_WRITE);
    else if (eAccess == NvCPR_AccessRead)
        flags = NVOS_OPEN_READ;
    else if (eAccess == NvCPR_AccessReadWrite)
        flags = NVOS_OPEN_CREATE;

    pFile = NvOsAlloc(sizeof(LocalFileHandle));
    NVMM_CHK_MEM(pFile);

    NvOsMemset(pFile, 0, sizeof(LocalFileHandle));

    NVMM_CHK_ERR(NvOsFopen(szURI, flags, &(pFile->hFile)));
    NVMM_CHK_ERR(NvOsFseek(pFile->hFile, 0, NvOsSeek_End));
    NVMM_CHK_ERR(NvOsFtell(pFile->hFile, &pFile->nSize));
    NVMM_CHK_ERR(NvOsFseek(pFile->hFile, 0, NvOsSeek_Set));

    *hHandle = pFile;

cleanup:
    if (Status != NvSuccess)
    {
        (void)LocalFileClose((NvCPRHandle)pFile);
    }
    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "--LocalFileOpen - Status =0x%x ", Status));
    return Status;
}

static NvError LocalFileSetPosition(NvCPRHandle hContent, NvS64 nOffset, 
                                    NvCPR_OriginType eOrigin)
{
    NvError Status = NvSuccess;
    LocalFileHandle *pFile = (LocalFileHandle *)hContent;
    NvOsSeekEnum whence;

    NVMM_CHK_ARG(pFile);

    if (eOrigin == NvCPR_OriginBegin)
        whence = NvOsSeek_Set;
    else if (eOrigin == NvCPR_OriginCur)
        whence = NvOsSeek_Cur;
    else if (eOrigin == NvCPR_OriginEnd)
        whence = NvOsSeek_End;
    else
        return NvError_NotSupported;

    Status = NvOsFseek(pFile->hFile, nOffset, whence);

cleanup:    
    return Status;
}

static NvError LocalFileGetPosition(NvCPRHandle hContent, 
                                    NvU64 *pPosition)
{
    NvError Status = NvSuccess;
    LocalFileHandle *pFile = (LocalFileHandle *)hContent;

    NVMM_CHK_ARG(pFile);

    Status = NvOsFtell(pFile->hFile, (NvU64*)pPosition);

cleanup:    
    return Status;
}

static NvU32 LocalFileRead(NvCPRHandle hContent, NvU8 *pData, NvU32 nSize)
{
    NvError Status = NvSuccess;
    size_t bytesread = 0;
    LocalFileHandle *pFile = (LocalFileHandle *)hContent;

    NVMM_CHK_ARG(pFile);

    Status = NvOsFread(pFile->hFile, pData, nSize, &bytesread);

cleanup:
    return (Status == NvSuccess) ? (NvU32)bytesread : 0; 
}

static NvU32 LocalFileWrite(NvCPRHandle hContent, NvU8 *pData, NvU32 nSize)
{
    NvError Status = NvSuccess;
    LocalFileHandle *pFile = (LocalFileHandle *)hContent;

    NVMM_CHK_ARG(pFile);

    Status = NvOsFwrite(pFile->hFile, pData, nSize);
    
cleanup:
    return (Status == NvSuccess) ? nSize : 0;
}

static NvError LocalFileGetSize(NvCPRHandle hContent, NvU64 *pFileSize)
{
    NvError Status = NvSuccess;
    LocalFileHandle *pFile = (LocalFileHandle *)hContent;

    NVMM_CHK_ARG(pFile);

    *pFileSize = pFile->nSize;

    NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_DEBUG, "++--LocalFileGetSize - Size =0x%x ", *pFileSize));    

cleanup:    
    return Status;
}

static NvBool LocalFileIsStreaming(NvCPRHandle hContent)
{
    return NV_FALSE;
}

static NvParserCoreType LocalFileProbeParserType(char *szURI)
{
    return NvParserCoreType_UnKnown;
}

static NvError LocalFileQueryConfig(NvCPRHandle hContent, 
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
        case NvCPR_ConfigGetChunkSize:
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

static NvError LocalFileSetPause(NvCPRHandle hContent, int bPause)
{
    return NvError_NotSupported;
}

static NV_CUSTOM_PROTOCOL s_LocalFileProtocol =
{
    LocalFileGetVersion,
    LocalFileOpen,
    LocalFileClose,
    LocalFileSetPosition,
    LocalFileGetPosition,
    LocalFileRead,
    LocalFileWrite,
    LocalFileGetSize,
    LocalFileIsStreaming,
    LocalFileProbeParserType,
    LocalFileQueryConfig,
    LocalFileSetPause
};

void NvGetLocalFileProtocol(NV_CUSTOM_PROTOCOL **proto)
{
    *proto = &s_LocalFileProtocol;
}

