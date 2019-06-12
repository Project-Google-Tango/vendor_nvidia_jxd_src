/*
* Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and
* proprietary rights in and to this software and related documentation.  Any
* use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation
* is strictly prohibited.
*/

#define LOG_TAG "NvCapTest_JNI"

#include "nvcap_api.h"

#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <cutils/log.h>
#include <math.h>

void com_nvidia_NvCapTest_startTest();
void com_nvidia_NvCapTest_endTest();
void com_nvidia_NvCapTest_runtimeUpdate();
int register_com_nvidia_NvCapTest_NvCapTest(JNIEnv* env);

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

// Release Buffers
static void ReleaseBuffers(void)
{
    if(g_ppBuf)
    {
        unsigned char i;
        // release all buffers
        for (i = 0; i < MAX_LIST_SIZE; i++)
        {
            NvCapBuf* pBuf = g_ppBuf[i];
            // Free the buffer in the list
            if(pBuf)
            {
                if(pBuf->pPtr)
                {
                    free(pBuf->pPtr);
                }
                free(pBuf);
            }
            g_ppBuf[i] = NULL;
        }
        free(g_ppBuf);
        g_ppBuf = NULL;
    }

    if(g_ppBufList)
        free(g_ppBufList);

    g_ppBufList = NULL;
}

// Allocate buffers
static int AllocateBuffers(void)
{
    // Allocate the list to store the capture buffers
    g_ppBuf = (NvCapBuf **)malloc(MAX_LIST_SIZE * sizeof(NvCapBuf*));
    if(!g_ppBuf)
        goto ERROR;
    memset(g_ppBuf,0, MAX_LIST_SIZE*sizeof(NvCapBuf*));

    // Allocate the list that is passed to nvcap
    g_ppBufList = (NvCapBuf **)malloc(MAX_LIST_SIZE * sizeof(NvCapBuf*));
    if(!g_ppBufList)
        goto ERROR;
    memset(g_ppBufList,0, MAX_LIST_SIZE*sizeof(NvCapBuf*));

    unsigned char i;
    for(i=0; i<MAX_LIST_SIZE; i++)
    {
        g_ppBuf[i] = NULL;
        g_ppBufMask[i] = 0;
        NvCapBuf* pBuf = (NvCapBuf *)malloc(sizeof(NvCapBuf));
        if (pBuf == NULL)
        {
            ALOGD("Could not allocate NvCapBuf structure");
            goto ERROR;
        }
        pBuf->pPtr = (void *)malloc(BUFFER_SIZE);
        if (pBuf->pPtr == NULL)
        {
            ALOGD("Could not allocate pPtr buffer in the NvCapBuf structure");
            free(pBuf);
            goto ERROR;
        }
        pBuf->uiSize = BUFFER_SIZE;
        g_ppBuf[i] = pBuf;
    }

    return 1;

ERROR:
    ReleaseBuffers();
    return 0;
}

// Callbacks
static void GetBuffersCB(NVCAP_CTX ctx, NvCapBuf*** pppBuf, int* pCount)
{
    unsigned char i, j;
    int count;
    //ALOGD("GetBuffersCB callback: START");

    // This returns a random buffer count between 0 and MAX_LIST_SIZE, its intended to simulate
    // a real time scenario where the number of available buffers depends on N/W conditions etc
    // Ideally count returned should be the number of unused/available NvCapBuf buffers
    count = (int)ceil((MAX_LIST_SIZE*((float)rand()/RAND_MAX)));
    j = 0;
    for(i=0; i<MAX_LIST_SIZE; i++)
    {
        if(!g_ppBufMask[i])
        {
            NvCapBuf* pBuf = g_ppBuf[i];
            memset(pBuf->pPtr,0xCC, BUFFER_SIZE); // fill buffer with pattern (for debug purposes)
            pBuf->uiSize = BUFFER_SIZE;
            g_ppBufList[j++] = pBuf;
            g_ppBufMask[i] = 1;       // mark this buffer as used
            if(j== count)
                break;
        }
    }

    //ALOGD("Providing %d buffers", j);
    *pppBuf = g_ppBufList;
    *pCount = j;

    //ALOGD("GetBuffersCB callback %x: END", (unsigned int)pBuf->pPtr);
}


static void SendBuffersCB(NVCAP_CTX ctx, NvCapBuf** ppBuf, int count)
{
    //ALOGD("SendBuffersCB callback: START, count = %d", count);

    unsigned int uiFileCreated = 0;
    int i = 0, j = 0;

    if (ppBuf == NULL)
    {
        ALOGD("SendBuffersCB(): ppBuf is NULL");
    }

    // Dump all the buffers to the file
    for (i = 0; i < count; i++)
    {
        NvCapBuf* pBuf = ppBuf[i];

        // uiSize is updated by the library to specify the number of valid bits
        if (g_pFile)
        {
            fwrite((char *)pBuf->pPtr, 1, pBuf->uiDataSize, g_pFile);
        }

        // Mark buffer as available
        for (j = 0; j < MAX_LIST_SIZE; j++)
        {
            if(g_ppBuf[i] == pBuf)
                g_ppBufMask[j] = 0;
        }
    }

    //ALOGD("SendBuffersCB callback: END");
}

static void NotifyEventCB(NVCAP_CTX ctx, NvCapEventCode eEventCode, void* pData)
{
    ALOGD("NotifyEventCB callback: START");

    switch (eEventCode)
    {
        case NvCapEventCode_Protection:
        {
            NvCapEventProtection* pstProtect = (NvCapEventProtection *)pData;
            if (pstProtect == NULL)
            {
                ALOGD("NotifyEventCB callback: Protection Event, NULL data");
                return;
            }
            ALOGD("NotifyEventCB callback: Protection Event, status = %d", pstProtect->iStatus);
            break;
        }
        case NvCapEventCode_FormatChange:
        {
            NvCapEventFormatChange* pstFormatChange = (NvCapEventFormatChange *)pData;
            if (pstFormatChange == NULL)
            {
                ALOGD("NotifyEventCB callback: Format Change Event, NULL data");
                return;
            }
            ALOGD("NotifyEventCB callback: Format Change Event. [video,stereo,w,h] = [%d,%d,%d,%d]",
                    pstFormatChange->uiVideo, pstFormatChange->eStereoFmt,
                    pstFormatChange->uiWidth, pstFormatChange->uiHeight);
            break;
        }
        case NvCapEventCode_FrameDrop:
        {
            NvCapEventFrameDrop* pstFrameDrop = (NvCapEventFrameDrop *)pData;
            if (pstFrameDrop == NULL)
            {
                ALOGD("NotifyEventCB callback: Frame Drop Event, NULL data");
                return;
            }
            ALOGD("NotifyEventCB callback: Frame Drop Event. [drops,time] = [%d,%d]",
                    pstFrameDrop->uiDrops, pstFrameDrop->uiTime);
            break;
        }
        default:
            ALOGD("NotifyEventCB callback: Unknown Event");
            break;
    }

    ALOGD("NotifyEventCB callback: END");
}

static int SetAttributes(NVCAP_CTX ctx)
{
    NvCapStatus status = NvCapStatus_Success;

    NvCapAttrType attrType = NvCapAttrType_StreamFormat;
    int attrVal = NvCapStreamFormat_MPEG2TS_AV_SingleStream;
    ALOGD("NvCapSetAttribute() attrType = %d, attrVal = %d", attrType, attrVal);
    status = NvCapSetAttribute(ctx, attrType, (void *)&attrVal, sizeof(attrVal));
    if (status != NvCapStatus_Success)
    {
        ALOGD("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_VideoCodec;
    attrVal = NvCapVideoCodec_H264;
    ALOGD("NvCapSetAttribute() attrType = %d, attrVal = %d", attrType, attrVal);
    status = NvCapSetAttribute(ctx, attrType, (void *)&attrVal, sizeof(attrVal));
    if (status != NvCapStatus_Success)
    {
        ALOGD("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_AudioCodec;
    attrVal = NvCapAudioCodec_LPCM_16_48_2ch;
    ALOGD("NvCapSetAttribute() attrType = %d, attrVal = %d", attrType, attrVal);
    status = NvCapSetAttribute(ctx, attrType, (void *)&attrVal, sizeof(attrVal));
    if (status != NvCapStatus_Success)
    {
        ALOGD("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_MuxerParams;
    NvCapMuxerParams stMuxerParams;
    stMuxerParams.eAudDesc = NvCapAudDesc_Bluray;
    ALOGD("NvCapSetAttribute() attrType = %d, Aud Desc = %d", attrType, stMuxerParams.eAudDesc);
    status = NvCapSetAttribute(ctx, attrType, (void *)&stMuxerParams, sizeof(stMuxerParams));
    if (status != NvCapStatus_Success)
    {
        ALOGD("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_Profile;
    attrVal = NvCapProfile_CBP;
    ALOGD("NvCapSetAttribute() attrType = %d, attrVal = %d", attrType, attrVal);
    status = NvCapSetAttribute(ctx, attrType, (void *)&attrVal, sizeof(attrVal));
    if (status != NvCapStatus_Success)
    {
        ALOGD("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_Level;
    attrVal = NvCapLevel_4_2;
    ALOGD("NvCapSetAttribute() attrType = %d, attrVal = %d", attrType, attrVal);
    status = NvCapSetAttribute(ctx, attrType, (void *)&attrVal, sizeof(attrVal));
    if (status != NvCapStatus_Success)
    {
        ALOGD("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_Resolution;
    NvCapResolution stResol = {1280, 720};
    ALOGD("NvCapSetAttribute() attrType = %d, width, height = %d, %d", attrType, stResol.uiWidth, stResol.uiHeight);
    status = NvCapSetAttribute(ctx, attrType, (void *)&stResol, sizeof(stResol));
    if (status != NvCapStatus_Success)
    {
        ALOGD("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_RefreshRate;
    NvCapRefreshRate stRefreshRate = {30, 1};
    ALOGD("NvCapSetAttribute() attrType = %d, fps = %d / %d", attrType, stRefreshRate.uiFPSNum,
            stRefreshRate.uiFPSDen);
    status = NvCapSetAttribute(ctx, attrType, (void *)&stRefreshRate, sizeof(stRefreshRate));
    if (status != NvCapStatus_Success)
    {
        ALOGD("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_BitRate;
    NvCapBitRate stBitRate = {7*1024*1024, 14*1024*1024}; // 7Mbps video rate, 14mbps transmission
    ALOGD("NvCapSetAttribute() attrType = %d, bitrate = %d, transmission rate = % d",
            attrType, stBitRate.uiBitRate, stBitRate.uiTransmissionRate);
    status = NvCapSetAttribute(ctx, attrType, (void *)&stBitRate, sizeof(stBitRate));
    if (status != NvCapStatus_Success)
    {
        ALOGD("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_FrameSkipInfo;
    NvCapFrameSkip stFrameSkip = {0, 0}; // {enable, skipfactor}
    ALOGD("NvCapSetAttribute() attrType = %d, skip enable = %d, skip factor = %d",
        attrType, stFrameSkip.uiEnable, stFrameSkip.uiSkipFactor);
    status = NvCapSetAttribute(ctx, attrType, (void *)&stFrameSkip, sizeof(stFrameSkip));
    if (status != NvCapStatus_Success)
    {
        ALOGD("NvCapSetAttribute() failed");
        return -1;
    }

    return 0;
}
static int UpdateRuntimeParams(NVCAP_CTX ctx)
{
    NvCapStatus status = NvCapStatus_Success;

    // Find out the current PTS
    unsigned long long qwPTS = 0;
    status = NvCapGetCurrentPTS(ctx, &qwPTS);
    if (status != NvCapStatus_Success)
    {
        ALOGD("UpdateRuntimeParams(): Failed to get current PTS value");
        return -1;
    }
    ALOGD("******** Curent PTS = %llu", qwPTS);

    // Force IDR picture
    NvCapNetworkFeedback stNWFeedback;
    stNWFeedback.uiForceIDRPic = 1;
    ALOGD("UpdateRuntimeParams(): Force IDR picture");
    status = NvCapSetAttribute(ctx, NvCapAttrType_NetworkParams, (void *)&stNWFeedback,
            sizeof(stNWFeedback));
    if (status != NvCapStatus_Success)
    {
        ALOGD("UpdateRuntimeParams(): Failed to set forcedIDR attribute");
        return -1;
    }

    // Frame rate change
    NvCapRefreshRate stRefreshRate = {2, 1};
    ALOGD("UpdateRuntimeParams(): Setting FPS to %d / %d",
            stRefreshRate.uiFPSNum, stRefreshRate.uiFPSDen);
    status = NvCapSetAttribute(ctx, NvCapAttrType_RefreshRate, (void *)&stRefreshRate,
            sizeof(stRefreshRate));
    if (status != NvCapStatus_Success)
    {
        ALOGD("UpdateRuntimeParams() failed");
        return -1;
    }

    // Bit rate change
    NvCapBitRate stBitRate = {7*1024*1024, 14*1024*1024}; //7 Mbps video rate, 14 mbps transmission
    ALOGD("UpdateRuntimeParams(): Setting Video Bitrate to = %d, Transmission Rate to %d",
                                    stBitRate.uiBitRate, stBitRate.uiTransmissionRate);
    status = NvCapSetAttribute(ctx, NvCapAttrType_BitRate, (void *)&stBitRate,
            sizeof(stBitRate));
    if (status != NvCapStatus_Success)
    {
        ALOGD("UpdateRuntimeParams() failed");
        return -1;
    }

    // Resolution change
    NvCapResolution stResol = {640, 480};
    ALOGD("UpdateRuntimeParams(): Setting [Width, Height] to [%d, %d]",
        stResol.uiWidth, stResol.uiHeight);
    status = NvCapSetAttribute(ctx, NvCapAttrType_Resolution, (void *)&stResol, sizeof(stResol));
    if (status != NvCapStatus_Success)
    {
        ALOGD("UpdateRuntimeParams() failed");
        return -1;
    }

    return 0;
}

void com_nvidia_NvCapTest_startTest()
{
    NvCapStatus status = NvCapStatus_Success;

    ALOGD("com_nvidia_NvCapTest_startTest(): START");

    // Create the dump file
    if (g_pFile == NULL)
    {
        ALOGD("Creating dump file");
        g_pFile = fopen ("/data/VideoTest.ts", "w");
        if (g_pFile == NULL)
        {
            ALOGD("Could not create the dump file");
        }
    }

    // Allocate the list to store the capture buffers
    if(!AllocateBuffers())
    {
        ALOGD("******** NvCap Buffer List allocation failed");
        return;
    }

    // Initialize the NvCap library
    NvCapCallbackInfo CBInfo;
    CBInfo.pfnGetBuffersCB = GetBuffersCB;
    CBInfo.pfnSendBuffersCB = SendBuffersCB;
    CBInfo.pfnNotifyEventCB = NotifyEventCB;
    status = NvCapInit(&g_ctx, &CBInfo);

    if ((status != NvCapStatus_Success) || (g_ctx == NULL))
    {
        ALOGD("******** NvCapInit failed");
        ReleaseBuffers();   // Free the output buffer lists
        return;
    }

    if (0 != SetAttributes(g_ctx))
    {
        ALOGD("******** Failed to set attributes");
        ReleaseBuffers();   // Free the output buffer lists
        return;
    }

    // Start the capture
    status = NvCapStart(g_ctx);

    if (status != NvCapStatus_Success)
    {
        ALOGD("******** NvCapStart failed");
        ReleaseBuffers();   // Free the output buffer lists
        return;
    }

    ALOGD("com_nvidia_NvCapTest_startTest(): END");

    return;
}

void com_nvidia_NvCapTest_endTest()
{
    NvCapStatus status = NvCapStatus_Success;

    ALOGD("com_nvidia_NvCapTest_endTest(): START");

    status = NvCapPause(g_ctx);
    if (status != NvCapStatus_Success)
    {
        ALOGD("******** NvCapPause failed");
        ReleaseBuffers();   // Free the output buffer lists
        return;
    }

    status = NvCapRelease(g_ctx);
    if (status != NvCapStatus_Success)
    {
        ALOGD("******** NvCapRelease failed");
        ReleaseBuffers();   // Free the output buffer lists
        return;
    }

    ReleaseBuffers();   // Free the output buffer lists

    ALOGD("com_nvidia_NvCapTest_endTest(): END");

    return;

}
void com_nvidia_NvCapTest_runtimeUpdate()
{
    ALOGD("com_nvidia_NvCapTest_runtimeUpdate(): START");
    if (0 != UpdateRuntimeParams(g_ctx))
    {
        ALOGD("******** Failed to update runtime parameters");
    }

    ALOGD("com_nvidia_NvCapTest_runtimeUpdate(): END");

}


static JNINativeMethod sMethods[] = {
     /* name, signature, funcPtr */
    { "startTestUsingJNI", "()V",  (void*)com_nvidia_NvCapTest_startTest
    },
    { "endTestUsingJNI", "()V",  (void*)com_nvidia_NvCapTest_endTest
    },
    { "runtimeUpdateUsingJNI", "()V",  (void*)com_nvidia_NvCapTest_runtimeUpdate
    },
};

int register_com_nvidia_NvCapTest_NvCapTest(JNIEnv* env)
{
    static const char* const kClassName = "com/nvidia/NvCapTest/NvCapTest";
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

    register_com_nvidia_NvCapTest_NvCapTest(env);

    return JNI_VERSION_1_4;
}

