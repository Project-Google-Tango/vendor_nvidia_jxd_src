/*
 * Copyright 2007 - 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_RMCTRACE_H
#define INCLUDED_NVRM_RMCTRACE_H

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_init.h"

/**
 * RMC is a file format for capturing accesses to hardware, both memory
 * and register, that may be played back against a simulator.  Drivers
 * are expected to emit RMC tracing if RMC tracing is enabled.
 *
 * The RM will already have an RMC file open before any drivers are expected
 * to access it, so it is not necessary for NvRmRmcOpen or Close to be called
 * by anyone except the RM itself (but drivers may want to if capturing a
 * subset of commands is useful).
 */

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

#define NV_DEF_RMC_TRACE 0

/**
 * exposed structure for RMC files.
 */
typedef struct NvRmRMCFile_t
{
    NvOsFileHandle file;
    NvBool enable; /* enable bit for writes */
} NvRmRmcFile;

/**
 * opens the an RMC file.
 *
 * @param name The name of the rmc file
 * @param rmc Out param - the opened rmc file (if successful)
 *
 * NvOsFile* operatations should not be used directly since RMC commands
 * or comments may be emited to the file on open/close/etc.
 */
NvError
NvRmRmcOpen( const char *name, NvRmRmcFile *rmc );

/**
 * closes an RMC file.
 *
 * @param rmc The rmc file to close.
 */
void
NvRmRmcClose( NvRmRmcFile *rmc );

/**
 * emits a string to the RMC file.
 *
 * @param file The RMC file
 * @param format Printf style argument format string
 *
 * NvRmRmcOpen must be called before this function.
 *
 * This function should be called via a macro so that it may be compiled out.
 * Note that double parens will be needed:
 *
 *     NVRM_RMC_TRACE(( file, "# filling memory with stuff\n" ));
 */
void
NvRmRmcTrace( NvRmRmcFile *rmc, const char *format, ... );

/**
 * retrieves the RM's global RMC file.
 *
 * @param hDevice The RM instance
 * @param file Output param: the RMC file
 */
NvError
NvRmGetRmcFile( NvRmDeviceHandle hDevice, NvRmRmcFile **file );

#define NVRM_RMC_TRACE(a) (void)0
#define NVRM_RMC_ENABLE(f,e) (void)0
#define NVRM_RMC_IS_ENABLED(f) (void)0

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* NVRM_RMCTRACE_H */
