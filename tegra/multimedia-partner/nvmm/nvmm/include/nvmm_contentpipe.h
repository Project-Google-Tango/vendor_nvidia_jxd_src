/* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* This file contains declarations for getting handles of Content Pipes for different kind of contents.*/

#ifndef INCLUDED_NVMM_CONTENTPIPE_H
#define INCLUDED_NVMM_CONTENTPIPE_H

#include "openmax/il/OMX_Types.h"
#include "openmax/il/OMX_ContentPipe.h"
#include "nvlocalfilecontentpipe.h" // TODO: remove this include and define CP_PIPETYPE_EXTENDED here

typedef struct CP_ConfigPropRec
{
    NvU32 propSize;
    char *prop;
}CP_ConfigProp;

typedef struct CP_ConfigTS
{
    NvU64 audioTs;
    NvU64 videoTs;
    NvBool bAudio;
    NvBool bVideo;
}CP_ConfigTS;

void NvmmGetFileContentPipe(CP_PIPETYPE_EXTENDED **pipe);

#endif
