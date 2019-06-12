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

#include "nvmm_customprotocol_internal.h"
#include "nvmm_protocol_builtin.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvutil.h"
#include "nvmm_logger.h"

// Lock this?  Probably unnecessary..

typedef struct _ProtoList
{
    char *sProtocol;
    NV_CUSTOM_PROTOCOL *pReadIf;
    struct _ProtoList *next;
} ProtoList;

static ProtoList *pProtoList = NULL;

void NvGetProtocolForFile(const char *url, NV_CUSTOM_PROTOCOL **pReader)
{
    ProtoList *pEntry = pProtoList;
    char *s;

    NvGetLocalFileProtocol(pReader);

    s = strchr(url, ':');
    if (!s)
    {
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_DEBUG, "NvMMContentPipe Protocol = LOCAL"));
        return;
    }

    while (pEntry)
    {
        if (!NvOsStrncmp(pEntry->sProtocol, url, 
                         NvOsStrlen(pEntry->sProtocol)))
        {
            *pReader = pEntry->pReadIf;
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_DEBUG, "NvMMContentPipe Protocol = %s ", pEntry->sProtocol));
            return;
        }
        pEntry = pEntry->next;
    }

    if (!NvOsStrncmp("http:", url, 5))
    {
        s = NvUStrstr((char *)url, ".sdp");
        if (s)
        {
            NvGetRTSPProtocol(pReader);
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_DEBUG, "NvMMContentPipe Protocol = RTSP "));
        }
        else
        {
            NvGetHTTPProtocol(pReader);
            NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_DEBUG, "NvMMContentPipe Protocol = HTTP "));
        }
    }

    if (!NvOsStrncmp("rtsp:", url, 5))
    {
        NvGetRTSPProtocol(pReader);
        NV_LOGGER_PRINT((NVLOG_CONTENT_PIPE, NVLOG_DEBUG, "NvMMContentPipe Protocol = RTSP "));
    }
}

NvError NvRegisterProtocol(const char *proto, NV_CUSTOM_PROTOCOL *pReader)
{
    ProtoList *pEntry;
    int len;
    char *s;

    NV_ASSERT(proto);
    NV_ASSERT(pReader);

    len = NvOsStrlen(proto);
    if (!len)
        return NvError_BadParameter;

    s = strchr(proto, ':');
    if (!s)
        return NvError_BadParameter;

    pEntry = NvOsAlloc(sizeof(ProtoList));
    NvOsMemset(pEntry, 0, sizeof(ProtoList));

    pEntry->pReadIf = pReader;
    pEntry->sProtocol = NvOsAlloc(len + 1);
    NvOsMemset(pEntry->sProtocol, 0, len + 1);
    NvOsStrncpy(pEntry->sProtocol, proto, len + 1);

    pEntry->next = pProtoList;
    pProtoList = pEntry;

    return NvSuccess;
}

void NvFreeAllProtocols(void)
{
    ProtoList *pEntry = pProtoList;

    while (pProtoList)
    {
        NvOsFree(pProtoList->sProtocol);
        pEntry = pProtoList->next;
        NvOsFree(pProtoList);
        pProtoList = pEntry;
    }
}


