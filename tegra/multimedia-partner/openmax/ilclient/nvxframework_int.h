/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVXFRAMEWORK_INT_H_
#define NVXFRAMEWORK_INT_H_

#include "nvxframework.h"

// Non-public framework structure
struct NvxFrameworkRec
{
    // Store the version of the client code for later param/config structs
    OMX_VERSIONTYPE version;

    // function pointers for public IL functions
    OMX_ERRORTYPE (*O_Init)(void);
    OMX_ERRORTYPE (*O_Deinit)(void);
    OMX_ERRORTYPE (*O_SetupTunnel)(OMX_HANDLETYPE hOutput,
                                   OMX_U32 nPortOutput,
                                   OMX_HANDLETYPE hInput,
                                   OMX_U32 nPortInput);
    OMX_ERRORTYPE (*O_GetHandle)(OMX_HANDLETYPE* pHandle,
                                 OMX_STRING cComponentName,
                                 OMX_PTR pAppData,
                                 OMX_CALLBACKTYPE* pCallBacks);
    OMX_ERRORTYPE (*O_FreeHandle)(OMX_HANDLETYPE hComponent);
    OMX_ERRORTYPE (*O_GetComponentsOfRole)(OMX_STRING role,
                                           OMX_U32 *pNumcomps,
                                           OMX_U8 **compNames);
    CPresult (*NVO_RegisterProto)(OMX_STRING szProto,
                                  NV_CUSTOM_PROTOCOL *pProtocol);
    OMX_ERRORTYPE (*NVO_RegisterComp)(OMX_COMPONENTREGISTERTYPE *reg);

    // shared library handle for IL lib
    NvOsLibraryHandle oOMXLibHandle;

};

#endif
