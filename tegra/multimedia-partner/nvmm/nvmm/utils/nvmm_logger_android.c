/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved. 
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvmm_logger.h"
#include "nvmm_logger_internal.h"
#include <cutils/log.h>

void NvLoggerPrintf(NvLoggerLevel Level, const char* pClientTag, const char* pFormat, va_list ap)
{
    int priority;

    /* map the nvlog output level  to android level */
    switch(Level)
    {
        case NVLOG_FATAL:
            priority = ANDROID_LOG_FATAL;
            break;
        case NVLOG_ERROR:
            priority = ANDROID_LOG_ERROR;
            break;
        case NVLOG_WARN:
            priority = ANDROID_LOG_WARN;
            break;
        case NVLOG_INFO:
            priority = ANDROID_LOG_INFO;
            break;
        case NVLOG_VERBOSE:
            priority = ANDROID_LOG_VERBOSE;
            break;
        case NVLOG_DEBUG:
        default:
            priority = ANDROID_LOG_DEBUG;
    }

    /* print the data */
    LOG_PRI_VA(priority, pClientTag, pFormat, ap);
}

