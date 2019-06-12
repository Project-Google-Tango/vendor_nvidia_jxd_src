/*
 * Copyright 2008-2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_AOS_PROCESS_ARGS_H
#define INCLUDED_AOS_PROCESS_ARGS_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    NvAosSemihostStyle_none,
    NvAosSemihostStyle_rvice = 1,
    NvAosSemihostStyle_tio_uart,
    NvAosSemihostStyle_tio_usb,
    NvAosSemihostStyle_uart,

    NvAosSemihostStyle_Force32 = 0x7FFFFFFF
} NvAosSemihostStyle;

#define NV_AOS_PROCESS_ARGS_VERSION 2

/* A loader may pass the arguments to an AOS process at load time
 * by overriding the contents of this struct.  This is only possible
 * if the struct is loaded at a location known to the loader.
 *
 * By convention, if the load map includes a load region called
 * AosProcessArgs, the loader may assume that the region solely contains
 * this struct.  The size of the region determines the maximum allowed
 * length of the command-line arguments.
 *
 * An AOS program linked without a region called AosProcessArgs will
 * never receive arguments via this struct.
 */
typedef struct NvAosProcessArgsRec
{
    NvU32 Version; /* identify version of structure */

    NvAosSemihostStyle SemiStyle;

    /* Deprecated -- setting this has no effect. */
    NvBool CpuJtagEnable;

    /* The length, in bytes of the actual contents of CommandLine. */
    NvU32 CommandLineLength;

    /* A (packed) sequence of NULL terminated strings defining the command
     * line arguments to the current process.  The total length of the sequence
     * (in bytes) is indicated in CommandLineLength.
     *
     * Though CommandLine is declared as an array of length 1, the loader may
     * assume that it extends to the end of the AosProcessArgs region.  AOS may
     * assume that it is of length CommandLineLength.
     */
    char CommandLine[1];

} NvAosProcessArgs;

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_AOS_PROCESS_ARGS_H

