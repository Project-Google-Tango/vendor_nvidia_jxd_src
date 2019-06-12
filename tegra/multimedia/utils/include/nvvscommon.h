/*
 * Copyright 2012 by NVIDIA Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 */

#ifndef __NV_VS_COMMON_H__
#define __NV_VS_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NVVSRESULT_OK = 0,
    NVVSRESULT_FAIL = 0x80000000 + NVVSRESULT_OK + 1,
    NVVSRESULT_INVALID_POINTER,
    NVVSRESULT_INVALID_DATA,
    NVVSRESULT_BAD_PARAMETER,
    NVVSRESULT_INSUFFICIENT_BUFFERING,
    NVVSRESULT_TIMED_OUT,
    NVVSRESULT_PENDING,
    NVVSRESULT_NONE_PENDING,
    NVVSRESULT_EGLIMAGE_NOT_AVAILABLE,
    NVVSRESULT_INSUFFICIENT_MEMORY,
    NVVSRESULT_NOT_IMPLEMENTED,
    NVVSRESULT_BUSY,
    NVVSRESULT_STREAM_NOT_SUPPORTED,
    NVVSRESULT_FORCE_32 = 0x7FFFFFFF
} NvVSResult;

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* __NV_VS_COMMON_H__ */

