/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_COMMON_H
#define INCLUDED_NVMM_COMMON_H

#include "nvmm_logger.h"

/**
 * NvMM common error handing macros
 *
 * @note Please define the NVLOG_CLIENT macro to the appropriate value (select from 
 * NvLoggerClient in nvmm_logger.h) in the client files before using the below APIs.
 * Ex: #define NVLOG_CLIENT NVLOG_MP4_PARSER
 * 
 */
#define NVMM_CHK_CP(expr) {               \
            Status = (expr);            \
            if (Status == NvError_BadParameter)     \
            {     \
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_WARN, "%s[%d] Warning! NvError = 0x%08x", __FUNCTION__, __LINE__, Status)); \
                Status = NvSuccess;     \
            }     \
            else if((Status != NvSuccess) && (Status != NvError_EndOfFile))   \
            {                       \
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "%s[%d] Failed! NvError = 0x%08x", __FUNCTION__, __LINE__, Status)); \
                goto cleanup;     \
            }                       \
        }


#define NVMM_CHK_ERR(expr) {               \
            Status = (expr);            \
            if ((Status != NvSuccess))   \
            {                       \
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "%s[%d] Failed! NvError = 0x%08x", __FUNCTION__, __LINE__, Status)); \
                goto cleanup;     \
            }                       \
        }

#define NVMM_CHK_MEM(expr) {               \
            if (((expr) == 0))   \
            {                       \
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "%s[%d] Memory Allocation Failed!", __FUNCTION__, __LINE__)); \
                Status = NvError_InsufficientMemory;\
                goto cleanup;\
            }                       \
        }

#define NVMM_CHK_ARG(expr) {               \
            if (((expr) == 0))   \
            {                       \
                NV_LOGGER_PRINT((NVLOG_CLIENT, NVLOG_ERROR, "%s[%d] Assert Failed!", __FUNCTION__, __LINE__)); \
                Status = NvError_BadParameter;\
                goto cleanup;\
            }                       \
        }

/* BYTE ORDER to INT CONVERSION MACROS */
#define NV_BE_TO_INT_32(buffer) ((NvU32)((NvU32)((*(buffer)) << 24) | (NvU32)((*(buffer+1)) << 16) | (NvU32)((*(buffer+2)) << 8) | (NvU32)(*(buffer+3))))

#define NV_BE_TO_INT_16(buffer) ((NvU16)((NvU16)((*(buffer)) << 8) | (NvU16)((*(buffer+1)))))

#define NV_LE_TO_INT_32(buffer) ((NvU32)((NvU32)((*(buffer+3)) << 24) | (NvU32)((*(buffer+2)) << 16) | (NvU32)((*(buffer+1)) << 8) | (NvU32)(*(buffer))))

#define NV_LE_TO_INT_16(buffer) ((NvU16)((NvU16)((*(buffer+1)) << 8) | (NvU16)((*(buffer)))))


/* UTILITY MACROS */
#define NV_TO_UPPER(c) ((c)-(32*(((c)>='a') && ((c)<='z'))))
#define NV_TO_LOWER(c) ( (c) | 0x20)

#endif //INCLUDED_NVMM_COMMON_H

