/*
* Copyright (c) 2011-2012, NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_PARSER_HAL_CONTENT_PIPE_H
#define INCLUDED_PARSER_HAL_CONTENT_PIPE_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include "nvlocalfilecontentpipe.h"
#include <media/stagefright/DataSource.h>

namespace android {

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NvBool              bIsStreaming;
    NvU64               nFileOffset;
    off64_t             FileSize;
    DataSource          *mDataSource;
}NvParserHalFileCache;

void NvParserHal_GetFileContentPipe(CP_PIPETYPE_EXTENDED **pipe);

#ifdef __cplusplus
}
#endif

} // namespace android

#endif //INCLUDED_PARSER_HAL_CONTENT_PIPE_H
