/*
 * Copyright (c) 2012-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define LOG_TAG "jni_nvidia"

#include "jni.h"
#include "utils/Log.h"
#include "nvcommon.h"

NvBool register_com_nvidia_graphics_blit(JNIEnv *env);

jint
JNI_OnLoad(JavaVM *vm,
           void *reserved)
{
    JNIEnv *env;

    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        ALOGE("GetEnv failed!");
        return -1;
    }

    register_com_nvidia_graphics_blit(env);

    return JNI_VERSION_1_4;
}
