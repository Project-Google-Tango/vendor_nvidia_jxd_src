/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef NVATRACE_H
#define NVATRACE_H

#ifndef NV_ATRACE
#define NV_ATRACE 0
#endif

#if NV_ATRACE

#include <cutils/trace.h>

#define NV_ATRACE_INIT() ATRACE_INIT()
#define NV_ATRACE_GET_ENABLED_TAGS() ATRACE_GET_ENABLED_TAGS()
#define NV_ATRACE_ENABLED() ATRACE_ENABLED()
#define NV_ATRACE_BEGIN(name) ATRACE_BEGIN(name)
#define NV_ATRACE_END() ATRACE_END()
#define NV_ATRACE_ASYNC_BEGIN(name, cookie) ATRACE_ASYNC_BEGIN(name, cookie)
#define NV_ATRACE_ASYNC_END(name, cookie) ATRACE_ASYNC_END(name, cookie)
#define NV_ATRACE_INT(name, value) ATRACE_INT(name, value)

#else // not NV_ATRACE

#define NV_ATRACE_INIT()
#define NV_ATRACE_GET_ENABLED_TAGS()
#define NV_ATRACE_ENABLED()
#define NV_ATRACE_BEGIN(name)
#define NV_ATRACE_END()
#define NV_ATRACE_ASYNC_BEGIN(name, cookie)
#define NV_ATRACE_ASYNC_END(name, cookie)
#define NV_ATRACE_INT(name, value)

#endif // not NV_ATRACE

#endif // NVATRACE_H
