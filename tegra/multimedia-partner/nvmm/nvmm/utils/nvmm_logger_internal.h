/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NV_LOGGER_INTERNAL_H
#define INCLUDED_NV_LOGGER_INTERNAL_H

/**
 * Platform specific implementation of NvLogger debug output.
 */
 void NvLoggerPrintf(NvLoggerLevel Level, const char* pClientTag, const char* pFormat, va_list ap);

#endif //INCLUDED_NV_LOGGER_INTERNAL_H
