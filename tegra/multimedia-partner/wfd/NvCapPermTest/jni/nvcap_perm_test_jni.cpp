/*
* Copyright (c) 2012, NVIDIA CORPORATION.  All Rights Reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and
* proprietary rights in and to this software and related documentation.  Any
* use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation
* is strictly prohibited.
*/

#define LOG_TAG "NvCapPermTest_JNI"

#include "nvcap_api.h"

#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <cutils/log.h>

void com_nvidia_NvCapPermTest_startTest();
int register_com_nvidia_NvCapPermTest_NvCapPermTest(JNIEnv* env);

static FILE *g_pFile = NULL;
static NVCAP_CTX g_ctx = NULL;
static NvCapBuf** g_ppBuf;        // This is the array of all pre-allocated NvCapBuf instances
static NvCapBuf** g_ppBufList;    // This is the List of all NvCapBuf instances passed to nvcap

#define MAX_LIST_SIZE 10             // Max size of the above preallocated array of NvCapBufs
#define BUFFER_SIZE   (12 + (7*188)) // Size of each buffer(uiSize) in the NvCapBuf list
                                     // Ideally this is the size of one RTP packet i.e
                                     // (7 TS packets(188) + RTP header(12)). The client should set
                                     // pPtr member of NvCapBuf to the offset where it wants
                                     // the RTP payload to be copied. The client should prepend the
                                     // 12 byte header once the RTP payload is copied and returned
                                     // in pfnSendBuffersCB
static char g_ppBufMask[MAX_LIST_SIZE]; // 0/1 free/used

// Callbacks
static void GetBuffersCB(NVCAP_CTX ctx, NvCapBuf*** pppBuf, int* pCount)
{
}

static void SendBuffersCB(NVCAP_CTX ctx, NvCapBuf** ppBuf, int count)
{
}

static void NotifyEventCB(NVCAP_CTX ctx, NvCapEventCode eEventCode, void* pData)
{
}

void com_nvidia_NvCapPermTest_startTest()
{
    NvCapStatus status = NvCapStatus_Success;

    ALOGD("com_nvidia_NvCapPermTest_startTest(): START");

    // Initialize the NvCap library
    NvCapCallbackInfo CBInfo;
    CBInfo.pfnGetBuffersCB = GetBuffersCB;
    CBInfo.pfnSendBuffersCB = SendBuffersCB;
    CBInfo.pfnNotifyEventCB = NotifyEventCB;
    status = NvCapInit(&g_ctx, &CBInfo);

    if (status == NvCapStatus_PermissionsFailure)
        ALOGE("NvCapPermTest: SUCCESS, User App not granted Permissions\n");
    else if (status == NvCapStatus_Success) {
        ALOGE("NvCapPermTest: FAIL, User App granted Permissions\n");
        NvCapRelease(g_ctx);
    }
    else
        ALOGE("NvCapPermTest: FAIL, User App failed to connect for unknown reasons\n");

    ALOGD("com_nvidia_NvCapPermTest_startTest(): END");

    return;
}

static JNINativeMethod sMethods[] = {
     /* name, signature, funcPtr */
    { "startTestUsingJNI", "()V",  (void*)com_nvidia_NvCapPermTest_startTest
    },
};

int register_com_nvidia_NvCapPermTest_NvCapPermTest(JNIEnv* env)
{
    static const char* const kClassName = "com/nvidia/NvCapPermTest/NvCapPermTest";
    jclass capSvcClass = env->FindClass(kClassName);

    ALOGD("Register JNI Methods");

    if (capSvcClass == NULL) {
        ALOGD("Cannot find classname %s", kClassName);
        return -1;
    }

    if (env->RegisterNatives(capSvcClass, sMethods, sizeof(sMethods)/sizeof(sMethods[0])) != JNI_OK) {
        ALOGD("Failed registering methods for %s", kClassName);
        return -1;
    }

    return 0;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGD("GetEnv Failed!");
        return -1;
    }
    if (env == 0) {
        ALOGD("Could not retrieve the env!");
    }

    register_com_nvidia_NvCapPermTest_NvCapPermTest(env);

    return JNI_VERSION_1_4;
}

