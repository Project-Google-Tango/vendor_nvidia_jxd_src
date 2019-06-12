/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvxframework_int.h"
#include "nvmm_logger.h"

/*
 * Initialize the OMX IL library, and perform some basic setup
 */
OMX_ERRORTYPE NvxFrameworkInit(NvxFramework *framework)
{
    NvxFramework context = NULL;
    NvError nverr = NvSuccess;
    OMX_ERRORTYPE err;

    if (!framework)
        return OMX_ErrorBadParameter;

    *framework = NvOsAlloc(sizeof(struct NvxFrameworkRec));
    if (!*framework)
        return OMX_ErrorInsufficientResources;

    context = *framework;

    // Version 1.1.2
    context->version.s.nVersionMajor = OMX_VERSION_MAJOR;
    context->version.s.nVersionMinor = OMX_VERSION_MINOR;
    context->version.s.nRevision     = OMX_VERSION_REVISION;
    context->version.s.nStep         = OMX_VERSION_STEP;

    // load the actual OMX library
    nverr = NvOsLibraryLoad("libnvomx", &context->oOMXLibHandle);
    if (nverr)
    {
        NvOsFree(*framework);
        *framework = NULL;
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    // get all the symbols we care about
    context->O_Init = NvOsLibraryGetSymbol(context->oOMXLibHandle, "OMX_Init");
    context->O_Deinit = NvOsLibraryGetSymbol(context->oOMXLibHandle, 
                                             "OMX_Deinit");
    context->O_SetupTunnel = NvOsLibraryGetSymbol(context->oOMXLibHandle,
                                                  "OMX_SetupTunnel");
    context->O_GetHandle = NvOsLibraryGetSymbol(context->oOMXLibHandle,
                                                "OMX_GetHandle");
    context->O_FreeHandle = NvOsLibraryGetSymbol(context->oOMXLibHandle,
                                                 "OMX_FreeHandle");
    context->O_GetComponentsOfRole = NvOsLibraryGetSymbol(context->oOMXLibHandle,
                                                 "OMX_GetComponentsOfRole");
    context->NVO_RegisterProto = NvOsLibraryGetSymbol(context->oOMXLibHandle,
                                              "NVOMX_RegisterNVCustomProtocol");
    context->NVO_RegisterComp = NvOsLibraryGetSymbol(context->oOMXLibHandle,
                                              "NVOMX_RegisterComponent");

    // if any symbols are missing, bail
    if (!context->O_Init || !context->O_Deinit || !context->O_SetupTunnel ||
        !context->O_GetHandle || !context->O_FreeHandle ||
        !context->O_GetComponentsOfRole || !context->NVO_RegisterProto)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    // Initialize OMX
    err = context->O_Init();

    return err;
}

/*
 * Clean up and close the IL client library/framework
 *
 * FIXME: refuse to deinit if open graphs/etc?
 */
OMX_ERRORTYPE NvxFrameworkDeinit(NvxFramework *framework)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxFramework context;

    if (!framework || !*framework)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    context = (NvxFramework)*framework;

    // Deinitialize OMX
    if (context->O_Deinit)
        eError = context->O_Deinit();

    // Get rid of the library we opened earlier
    if (context->oOMXLibHandle)
        NvOsLibraryUnload(context->oOMXLibHandle);

    NvOsFree(context);

    *framework = NULL;

    return eError;
}

/*
 * Register a custom protocol implementation with OMX IL
 */
OMX_ERRORTYPE NvxFrameworkRegisterProtocol(NvxFramework framework,
                                           OMX_STRING protoname,
                                           NV_CUSTOM_PROTOCOL *protocol)
{
    if (!framework)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    if (!protoname || !protocol)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    framework->NVO_RegisterProto(protoname, protocol);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxFrameworkRegisterComponent(NvxFramework framework,
                                            OMX_COMPONENTREGISTERTYPE *reg)
{
    if (!framework)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    if (!reg)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    framework->NVO_RegisterComp(reg);

    return OMX_ErrorNone;
}

/*
 * Helper function to return our client version
 */
OMX_VERSIONTYPE NvxFrameworkGetOMXILVersion(NvxFramework framework)
{
    if (!framework)
    {
        OMX_VERSIONTYPE broken;

        broken.s.nVersionMajor = OMX_VERSION_MAJOR;
        broken.s.nVersionMinor = OMX_VERSION_MINOR;
        broken.s.nRevision     = OMX_VERSION_REVISION;
        broken.s.nStep         = OMX_VERSION_STEP;

        return broken;
    }

    return framework->version;
}

NvOsLibraryHandle NvxFrameworkGetILLibraryHandle(NvxFramework framework)
{
    if (!framework)
    {
        NV_LOGGER_PRINT((NVLOG_OMX_IL_CLIENT, NVLOG_ERROR, "ERROR --%s[%d]",
            __FUNCTION__, __LINE__));
        return NULL;
    }

    return framework->oOMXLibHandle;
}

