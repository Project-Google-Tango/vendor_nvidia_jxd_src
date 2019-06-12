/*
 * Copyright (c) 2008-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation.  
 * Any use, reproduction, disclosure or distribution of this software and
 * related documentation without an express license agreement from NVIDIA
 * Corporation is strictly prohibited.
 */


#include "nvxegl.h"
#include "nvxeglimage.h"

NvEglApiExports g_EglExports = {0};
static NvOsLibraryHandle s_EglLibraryNV = NULL;

#if defined(ANDROID) && defined(PLATFORM_IS_ICECREAMSANDWICH)
static NvOsLibraryHandle s_EglLibraryAndroid = NULL;
EglGetImageForImplementation g_EglGetEglImageAndroid = NULL;
EglGetImageForCurrentContext g_EglGetEglImageAndroidLegacy = NULL;
#endif

typedef void (*NvEglRegClientApiType) (NvEglApiIdx apiId, NvEglApiInitFnPtr apiInitFnct);

NvError NvxEglInit (NvEglApiImports *imports, const NvEglApiExports *exports)
{
    g_EglExports = *exports;
    return NvSuccess;
}

NvError NvxConnectToEgl (void)
{
    // This will load NvEglRegClientApi function from NVIDIA's EGL
    // implementation.
    //
    // -On WinCE, libEGL.dll is NVIDIA's EGL that contains the function.
    //
    // -On Android, libEGL.so is Google's EGL that forwards calls to
    //  our implementation that is located in egl/libEGL_tegra.so.
    //  In addition, Google's EGL contains a helper function required
    //  to unwrap Android's EGL image handles.

#if defined(ANDROID)
    static const char* eglNameNV = "egl/libEGL_tegra";
#if defined(PLATFORM_IS_ICECREAMSANDWICH)
    static const char* eglNameAndroid = "libEGL";
#endif
#else
    static const char* eglNameNV = LIBEGL_DSO;
#endif

    NvEglRegClientApiType regf = NULL;
    
    // Check if library is already successfully loaded
    if (s_EglLibraryNV)
    {
        NV_ASSERT(g_EglExports.swapBuffers);
        return NvSuccess;
    }

    // Load our EGL library and get the registeration function
    s_EglLibraryNV = NULL;
    if (NvOsLibraryLoad(eglNameNV, &s_EglLibraryNV) != NvSuccess)
    {
        NV_ASSERT(!"Cannot load EGL");
        return NvError_NotInitialized;
    }

    regf = (NvEglRegClientApiType)NvOsLibraryGetSymbol(
        s_EglLibraryNV, "NvEglRegClientApi");

    if (!regf)
    {
        NV_ASSERT(!"Cannot find function NvEglRegClientApi");
        NvOsLibraryUnload(s_EglLibraryNV);
        s_EglLibraryNV = NULL;
        return NvError_NotInitialized;
    }

#if defined(ANDROID) && defined(PLATFORM_IS_ICECREAMSANDWICH)
    // On Android, load Google's EGL and get the helper function
    s_EglLibraryAndroid = NULL;
    if (NvOsLibraryLoad(eglNameAndroid, &s_EglLibraryAndroid) != NvSuccess)
    {
        NV_ASSERT(!"Cannot load EGL");
        return NvError_NotInitialized;
    }

    // Mangled C++ function name in namespace android
    g_EglGetEglImageAndroid = NvOsLibraryGetSymbol(
        s_EglLibraryAndroid,
        "_ZN7android32egl_get_image_for_implementationEPKcPv");

    if (!g_EglGetEglImageAndroid)
        g_EglGetEglImageAndroidLegacy = NvOsLibraryGetSymbol(
            s_EglLibraryAndroid,
            "_ZN7android33egl_get_image_for_current_contextEPv");

    if (!g_EglGetEglImageAndroid && !g_EglGetEglImageAndroidLegacy)
    {
        NV_ASSERT(!"Cannot find function egl_get_image_for_current_context");
        NvOsLibraryUnload(s_EglLibraryAndroid);
        s_EglLibraryAndroid = NULL;
        NvOsLibraryUnload(s_EglLibraryNV);
        s_EglLibraryNV = NULL;
        regf = NULL;
        return NvError_NotInitialized;
    }
#endif

    // Call the register client api function
    regf(NV_EGL_API_OMX_IL, NvxEglInit);
    NV_ASSERT(g_EglExports.swapBuffers);

    return NvSuccess;
}
