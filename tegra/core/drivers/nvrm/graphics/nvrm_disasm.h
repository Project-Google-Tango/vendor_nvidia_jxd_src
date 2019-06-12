/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_DISASM_H
#define INCLUDED_NVRM_DISASM_H

/**
 * Loads the libnvstreamdisasm shared library.
 *
 * This must be called prior to using the other NvRmDisasm* functions.
 *
 * @returns NV_TRUE if the library was loaded succesfully, otherwise NV_FALSE.
 */
NvBool NvRmDisasmLibraryLoad(void);

/**
 * Unloads the libnvstreamdisasm shared library.
 */
void NvRmDisasmLibraryUnload(void);

/**
 * The following functions are internal wrappers around the functions provided
 * by the libnvstreamdisasm library, to hide the complexity of dynamically
 * loading this shared library. It also ensures nvrm does not have a hard
 * dependency against libnvstreamdisasm.
 * For documentation, refer to the nvstreamdisasm.h header and the following
 * functions:
 *   NvStreamDisasmInitialize
 *   NvStreamDisasmCreate
 *   NvStreamDisasmDestroy
 *   NvStreamDisasmPrint
 */
typedef void * NvStreamDisasmContext;
void NvRmDisasmInitialize(NvStreamDisasmContext *context);
NvStreamDisasmContext *NvRmDisasmCreate(void);
void NvRmDisasmDestroy(NvStreamDisasmContext *context);
NvS32 NvRmDisasmPrint(NvStreamDisasmContext *context,
                      NvU32 value,
                      char *buffer,
                      size_t bufferSize);

#endif // INCLUDED_NVRM_STREAM_H
