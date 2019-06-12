/*
 * Copyright 2007-2008 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVOS_TRACE_H
#define INCLUDED_NVOS_TRACE_H

#define NVOS_TRACE 0

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

/**
 * The nvos_trace.txt is a nvos trace file to collect aggregate statistics
 * about how system calls and resources are used in higher-level software.
 * It has format:
 *    NvOsFunctionName , CallingFile , CallingLine , CallTime , Data 
 * NvOsFunctionName is the function name 
 * CallingFile and CallingLine are just the __FILE__ and __LINE__ parameters. 
 * CallTime is a NvU32 storing time in miliseconds. 
 * Data is a function-specific data parameter. For MutexLock and MutexUnlock 
 * this would be the mutex handle. For NvOsAlloc * and NvOsFree this 
 * would be the allocated address. 
 *
 */

/**
 * opens the trace file nvos_trace.txt
 *
 * This function should be called via a macro so that it may be compiled out.
 *
 */
void 
NvOsTraceLogStart(void);

/**
 * closes the trace file nvos_trace.txt.
 *
 * This function should be called via a macro so that it may be compiled out.
 */
void
NvOsTraceLogEnd(void);

/**
 * emits a string to the trace file nvos_trace.txt
 *
 * @param format Printf style argument format string
 *
 * This function should be called via a macro so that it may be compiled out.
 *
 */
void
NvOsTraceLogPrintf( const char *format, ... );

/**
 * Helper macro to go along with NvOsTraceLogPrintf.  Usage:
 *    NVOS_TRACE_LOG_PRINTF(("foo: %s\n", bar));
 * The NvOs trace log prints will be disabled by default in all builds, debug
 * and release.
 * Note the use of double parentheses.
 *
 * To enable NvOs trace log prints
 *     #define NVOS_TRACE 1
 */
#if NVOS_TRACE
#define NVOS_TRACE_LOG_PRINTF(a)    NvOsTraceLogPrintf a
#define NVOS_TRACE_LOG_START    \
    do {                        \
        NvOsTraceLogStart();    \
    } while (0);
#define NVOS_TRACE_LOG_END      \
    do {                        \
        NvOsTraceLogEnd();      \
    } while (0);
#else
#define NVOS_TRACE_LOG_PRINTF(a)    (void)0
#define NVOS_TRACE_LOG_START        (void)0
#define NVOS_TRACE_LOG_END          (void)0
#endif

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* NVOS_TRACE_H */

