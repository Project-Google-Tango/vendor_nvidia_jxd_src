/*
 * Copyright (c) 2009-2012, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define LOG_TAG "jni_MutableDisplay"

#include "JNIHelp.h"
#include <jni.h>
#include "utils/Log.h"
#include "utils/misc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvdispmgr.h"

NvDispMgrClientHandle g_DispMgrClient = NULL;

static jintArray com_nvidia_display_getDisplayModeNative(
    JNIEnv* env, jclass clazz, 
    jint id
) {
    NvError e;
    jintArray result = 0;
    NvU32 buffer[4];

    e = NvDispMgrDisplayGetAttributes(
        g_DispMgrClient,
        id,
        NvDispMgrAttrFlags_None,
        buffer,
        NULL, // sequence number
        NvDispMgrDisplayAttr_ModeWidth,
        NvDispMgrDisplayAttr_ModeHeight,
        NvDispMgrDisplayAttr_ColorPrecision,
        NvDispMgrDisplayAttr_ModeRefreshRate,
        0 // terminate list
    );
    if (e != NvSuccess)
        goto report_error;

    // allocate a java array to hold the result
    result = env->NewIntArray(4); // we return a 4-tuple
    if (!result)
        goto report_error;

    // copy buffer into the java array
    env->SetIntArrayRegion(result, 0, 4, (jint *)buffer);

    // done
    return result;

report_error:
    ALOGE("Error occurred getting display mode");
    return 0;
}

static void com_nvidia_display_setDisplayModeNative(
    JNIEnv* env, jclass clazz,
    jint id, jint width, jint height, jint bitsPerPixel, jint refreshRateFx16
) {
    NvError e;

    // set width, height, and refresh rate
    // bitsPerPixel is what it is
    e = NvDispMgrDisplaySetAttributes(
        g_DispMgrClient, 1<<id, NvDispMgrAttrFlags_None, NULL,
        NvDispMgrDisplayAttr_ModeWidth, NvU32(width),
        NvDispMgrDisplayAttr_ModeHeight, NvU32(height),
//      NvDispMgrDisplayAttr_ColorPrecision, NvU32(bitsPerPixel),
        NvDispMgrDisplayAttr_ModeRefreshRate, NvU32(refreshRateFx16),
        0
    );

    if (e != NvSuccess) {
        ALOGE("Error occurred setting display mode");
    }
}

static bool is_good(const NvDispMgrDisplayMode &mode, const NvDispMgrDisplayListModesFlags &flags) {
    if (mode.Flags & NvDispMgrDisplayModeFlags_PartialMode)
        return false; // reject partial modes
    if (0) // disable this check because nothing claims to be native
        if (flags & NvDispMgrDisplayListModesFlags_CheckEDID) // if we checked the EDID
            if ((mode.Flags & NvDispMgrDisplayModeFlags_NativeMode)==0)
                return false; // reject non-native modes
    return true;
}

static jintArray com_nvidia_display_getSupportedDisplayModesNative(
    JNIEnv* env, jclass clazz, 
    jint id
) {
    NvError e;
    NvDispMgrDisplayMode *modes = NULL;
    NvU32 count, good_count;
    jintArray result;
    NvDispMgrDisplayListModesFlags flags = NvDispMgrDisplayListModesFlags_CheckEDID;

    for (;;) {
        // call list modes to figure out how many modes are supported
        e = NvDispMgrDisplayListModes(
            g_DispMgrClient, flags, 1<<id,
            0, NULL,
            &count,
            NULL
        );
        if (e==NvSuccess)
            break; // yay
        if (flags==NvDispMgrDisplayListModesFlags_CheckEDID)
            flags = NvDispMgrDisplayListModesFlags_None; // try without checking EDID
        else
            goto report_error; // out of ideas
    }

    ALOGE(
        "found %d modes for display %d with edid checking %s", 
        count, id,
        (flags==NvDispMgrDisplayListModesFlags_CheckEDID? "on": "off")
    );

    // allocate space for all those modes
    modes = (NvDispMgrDisplayMode *)malloc(sizeof(NvDispMgrDisplayMode)*count);
    if (!modes)
        goto report_error;

    // call list modes again to read the modes
    e = NvDispMgrDisplayListModes(
        g_DispMgrClient, flags, 1<<id,
        count, modes,
        &count,
        NULL
    );
    if (e != NvSuccess)
        goto report_error;

    // count the good modes (the non-partial ones)
    good_count = 0;
    for (NvU32 i = 0; i<count; ++i)
        if (is_good(modes[i], flags))
            ++good_count;

    ALOGE("of the %d found, only %d were considered good", count, good_count);

    // allocate a java array to hold the result
    result = env->NewIntArray(4*good_count); // we return 4-tuples
    if (!result)
        goto report_error;

    // store each supported mode in the java array
    for (NvU32 i = 0; i<count; ++i)
        if (is_good(modes[i], flags)) {
            jint mode[] = {
                modes[i].Width, 
                modes[i].Height, 
                modes[i].BitsPerPixel, 
                modes[i].RefreshRate 
            };
            env->SetIntArrayRegion(result, i*4, 4, mode);
        }

    // done
    free(modes);
    return result;

report_error:
    ALOGE("Error occurred getting supported display modes");
    free(modes);
    return 0;
}

static jintArray com_nvidia_display_getDisplayIdsNative(
    JNIEnv* env, jclass clazz, 
    jboolean connectedOnly
) {
    NvError e;
    NvDispMgrDisplayMask mask;
    jintArray result;
    int count;
    
    // get a mask with the displays that match the criteria
    NvDispMgrDisplayAttr attributes[] = {
      NvDispMgrDisplayAttr_IsConnected,
    };
    NvU32 values[] = {
      NV_TRUE,
    };
    e = NvDispMgrFindDisplays(
        g_DispMgrClient,
        NvDispMgrFindDisplaysFlags_None,
        NV_DISP_MGR_DISPLAY_MASK_ALL,
        0, NULL, // no GUIDs
        connectedOnly? sizeof(attributes)/sizeof(attributes[0]): 0,
        attributes, values,
        &mask,
        NULL
    );
    if (e != NvSuccess)
        goto report_error;

    // count the number of ones in the mask
    count = 0;
    for (int i = 0; i<32; ++i)
        if ((mask>>i) & 1)
            count++;

    // allocate a java array to hold the result
    result = env->NewIntArray(count);
    if (!result)
        goto report_error;

    // store each display ID in the java array
    count = 0;
    for (jint i = 0; i<32; ++i)
        if ((mask>>i) & 1)
            env->SetIntArrayRegion(result, count++, 1, &i);

    // done
    return result;

report_error:
    ALOGE("Error occurred getting display IDs");
    return 0;
}

static void com_nvidia_display_setRouteAudioToHdmi(
    JNIEnv* env, jclass clazz, 
    jboolean routeAudioToHdmi
) {
    // TODO: set audio routing
}

static jboolean com_nvidia_display_getRouteAudioToHdmi(
    JNIEnv* env, jclass clazz
) {
    // TODO: report audio routing
    return JNI_FALSE;
}

static JNINativeMethod sMethods[] = {
     /* name, signature, funcPtr */
    { "getDisplayModeNative", "(I)[I",  (void*)com_nvidia_display_getDisplayModeNative
    },
    { "setDisplayModeNative", "(IIIII)V",  (void*)com_nvidia_display_setDisplayModeNative
    },
    { "getSupportedDisplayModesNative", "(I)[I",  (void*)com_nvidia_display_getSupportedDisplayModesNative
    },
    { "getDisplayIdsNative", "(Z)[I",  (void*)com_nvidia_display_getDisplayIdsNative
    },
    { "setRouteAudioToHdmi", "(Z)V",  (void*)com_nvidia_display_setRouteAudioToHdmi
    },
    { "getRouteAudioToHdmi", "()Z",  (void*)com_nvidia_display_getRouteAudioToHdmi
    },
};

int register_com_nvidia_display_MutableDisplay(JNIEnv* env)
{
    static const char* const kClassName = "com/nvidia/display/MutableDisplay";
    jclass clazz = env->FindClass(kClassName);

    if (clazz == NULL) {
        ALOGE("Can't find %s", kClassName);
        return -1;
    }

    if (env->RegisterNatives(clazz, sMethods,
            sizeof(sMethods)/sizeof(sMethods[0])) != JNI_OK) {
        ALOGE("Failed registering methods for %s\n", kClassName);
        return -1;
    }

    return 0;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;
    NvError e;

    e = NvDispMgrClientInitialize(&g_DispMgrClient);

    if (e != NvSuccess) {
        ALOGE("failed to initialize NvDispMgr client");
        return -1;
    }

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("GetEnv failed!");
        return result;
    }
    LOG_ASSERT(env, "Could not retrieve the env!");

    register_com_nvidia_display_MutableDisplay(env);

    return JNI_VERSION_1_4;
}
