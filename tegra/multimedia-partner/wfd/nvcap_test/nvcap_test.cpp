/*
* Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and
* proprietary rights in and to this software and related documentation.  Any
* use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation
* is strictly prohibited.
*/

#include "nvcap_api.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <cutils/log.h>
#include <sys/time.h>
#include <math.h>

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
#include <nvos.h>
#include <powerservice/PowerServiceClient.h>
#endif // USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS

typedef struct _NvCapTestCtx
{
    unsigned int uiWidth;   // Width
    unsigned int uiHeight;  // Height
    unsigned int uiFPSNum;  // FPS Numerator
    unsigned int uiFPSDen;  // FPS Denominator
    unsigned int uiVideoBitRate;     // Bitrate for Video
    unsigned int uiTransmissionRate; // Mux throughput rate/nvcap transmission rate
    int iSkipFactor;        // If < 0, frame skip is disabled
    unsigned int uiStallMS; // Amount of Time (ms) to stall in GetBuffersCB()
    unsigned int uiNumIter; // Number of test iterations to run

    unsigned int uiDump;    // 1 => Dump TS to file
    unsigned int uiManual;  // 1 => Manual Mode testing, 0 => Automatic mode
    NvCapLatencyProfilingLevel eLatencyProfileLvl;
    NvCapStereoFormat eStereoFmt; // Stereo format
    NvCapSinkInfo stSinkInfo; // Sink info
    unsigned int uiAudioCodecType; // 0 => LPCM, 1 => AAC
    unsigned int uiSLE; // Slice Buffers 0 => disable, 1 => enable
    unsigned int uiSIR; // Slice Intra Refresh 0 => disable, 1 => enable
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    unsigned int uiPowerHint;  // 1 => enable, 0 => disable
    android::PowerServiceClient *pPowerSvcClient;
    uint64_t prevTimestamp;
#endif // USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS

    FILE* pFile;            // Dump file
} NvCapTestCtx;

static struct timeval tv;         // Timer value
static NvCapTestCtx g_InitialCtx; // Common setup done for all tests
static NvCapTestCtx g_FinalCtx;   // This will reflect runtime param updates
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
            ALOGE("Could not allocate NvCapBuf structure");
            goto ERROR;
        }
        pBuf->pPtr = (void *)malloc(BUFFER_SIZE);
        if (pBuf->pPtr == NULL)
        {
            ALOGE("Could not allocate pPtr buffer in the NvCapBuf structure");
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
    ALOGV("GetBuffersCB callback: START");

    // If we have been asked to stall to simulate network congestion, then do so here
    // If no buffers are available do not block, return immediately with count as 0
    // and a NULL pointer for NvCapBuf list
    if (g_FinalCtx.uiStallMS != 0)
    {
        struct timeval tv_lapsed;
        gettimeofday(&tv_lapsed, NULL);
        if(((unsigned int)(tv_lapsed.tv_usec - tv.tv_usec)) < g_FinalCtx.uiStallMS*1000)
        {
            ALOGE("GetBuffersCB(): No buffers available for %d milliseconds\n", g_FinalCtx.uiStallMS);
            *pppBuf = NULL;
            *pCount = 0;
            return;
        }
        ALOGE("GetBuffersCB(): Resume.\n");
        g_FinalCtx.uiStallMS = 0;
    }

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

     ALOGV("Providing %d buffers", j);
    *pppBuf = g_ppBufList;
    *pCount = j;

    ALOGV("GetBuffersCB callback %x: END", (unsigned int)pBuf->pPtr);
}

static void SendBuffersCB(NVCAP_CTX ctx, NvCapBuf** ppBuf, int count)
{
    ALOGV("SendBuffersCB callback: START, count = %d", count);

    unsigned int uiFileCreated = 0;
    int i = 0, j = 0;

    if (ppBuf == NULL)
    {
        ALOGE("SendBuffersCB(): ppBuf is NULL");
    }

    // Dump all the buffers to the file
    for (i = 0; i < count; i++)
    {
        NvCapBuf* pBuf = ppBuf[i];

        // uiSize is updated by the library to specify the number of valid bits
        if (g_FinalCtx.pFile)
        {
            fwrite((char *)pBuf->pPtr, 1, pBuf->uiDataSize, g_FinalCtx.pFile);
        }

        // Mark buffer as available
        for (j = 0; j < MAX_LIST_SIZE; j++)
        {
            if(g_ppBuf[i] == pBuf)
                g_ppBufMask[j] = 0;
        }
    }

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    if (g_FinalCtx.pPowerSvcClient)
    {
        uint64_t curTimestamp = NvOsGetTimeUS();
        if (curTimestamp - g_FinalCtx.prevTimestamp > 500000)
        {
            g_FinalCtx.pPowerSvcClient->sendPowerHint((int)POWER_HINT_MIRACAST,(void *)1);
            g_FinalCtx.prevTimestamp = curTimestamp;
        }
    }
#endif // USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS

    ALOGV("SendBuffersCB callback: END");
}

static void NotifyEventCB(NVCAP_CTX ctx, NvCapEventCode eEventCode, void* pData)
{
    ALOGE("NotifyEventCB callback: START");

    switch (eEventCode)
    {
        case NvCapEventCode_Protection:
        {
            NvCapEventProtection* pstProtect = (NvCapEventProtection *)pData;
            if (pstProtect == NULL)
            {
                ALOGE("NotifyEventCB callback: Protection Event, NULL data");
                return;
            }
            ALOGE("NotifyEventCB callback: Protection Event, status = %d", pstProtect->iStatus);
            break;
        }
        case NvCapEventCode_FormatChange:
        {
            NvCapEventFormatChange* pstFormatChange = (NvCapEventFormatChange *)pData;
            if (pstFormatChange == NULL)
            {
                ALOGE("NotifyEventCB callback: Format Change Event, NULL data");
                return;
            }
            ALOGE("NotifyEventCB callback: Format Change Event. [video,stereo,w,h] = [%d,%d,%d,%d]",
                    pstFormatChange->uiVideo, pstFormatChange->eStereoFmt,
                    pstFormatChange->uiWidth, pstFormatChange->uiHeight);

            // handle stereo format change (only if sink supports stereo)
            if (g_FinalCtx.stSinkInfo.uiStereoCapable &&
                pstFormatChange->eStereoFmt != g_FinalCtx.eStereoFmt)
            {
                ALOGE("NotifyEventCB callback: Run-time update of stereo format (%d -> %d)",
                        g_FinalCtx.eStereoFmt, pstFormatChange->eStereoFmt);

                NvCapStatus status = NvCapSetAttribute(ctx, NvCapAttrType_StereoFormat,
                                          (void *)&pstFormatChange->eStereoFmt,
                                          sizeof(pstFormatChange->eStereoFmt));

                if (status != NvCapStatus_Success)
                    ALOGE("NvCapSetAttribute() failed");
                else
                    g_FinalCtx.eStereoFmt = pstFormatChange->eStereoFmt;
            }
            break;
        }
        case NvCapEventCode_FrameDrop:
        {
            NvCapEventFrameDrop* pstFrameDrop = (NvCapEventFrameDrop *)pData;
            if (pstFrameDrop == NULL)
            {
                ALOGE("NotifyEventCB callback: Frame Drop Event, NULL data");
                return;
            }
            ALOGE("NotifyEventCB callback: Frame Drop Event. [drops,time] = [%d,%d]",
                    pstFrameDrop->uiDrops, pstFrameDrop->uiTime);
            break;
        }
        case NvCapEventCode_DisplayStatus:
        {
            NvCapEventDisplayStatus* pstDisplayStatus = (NvCapEventDisplayStatus *)pData;
            if (pstDisplayStatus == NULL)
            {
                ALOGE("NotifyEventCB callback: Display Status Event, NULL data");
                return;
            }
            ALOGE("NotifyEventCB callback: Display Status Event, status = %d",
                    pstDisplayStatus->iStatus);
            break;
        }
        default:
            ALOGE("NotifyEventCB callback: Unknown Event");
            break;
    }

    ALOGE("NotifyEventCB callback: END");
}

static int InsertIDR(NVCAP_CTX ctx)
{
    NvCapStatus status = NvCapStatus_Success;

    // Find out the current PTS. This doesn't necessarily belong
    // in the InsertIDR() function, but we need to have it added
    // somewhere to test the NvCapGetCurrentPTS() API
    unsigned long long qwPTS = 0;
    status = NvCapGetCurrentPTS(ctx, &qwPTS);
    if (status != NvCapStatus_Success)
    {
        ALOGD("InsertIDR(): Failed to get current PTS value");
        return -1;
    }
    ALOGE("******** Curent PTS = %llu", qwPTS);

    // Force IDR picture
    NvCapNetworkFeedback stNWFeedback;
    stNWFeedback.uiForceIDRPic = 1;
    ALOGE("InsertIDR(): Force IDR picture\n");
    status = NvCapSetAttribute(ctx, NvCapAttrType_NetworkParams, (void *)&stNWFeedback,
                sizeof(stNWFeedback));
    if (status != NvCapStatus_Success)
    {
        ALOGE("InsertIDR(): Failed to set forcedIDR attribute\n");
        return -1;
    }

    return 0;
}

static int UpdateFramerate(NVCAP_CTX ctx, NvCapTestCtx* pTestCtx)
{
    NvCapStatus status = NvCapStatus_Success;

    // Frame rate change
    NvCapRefreshRate stRefreshRate;
    stRefreshRate.uiFPSNum = pTestCtx->uiFPSNum;
    stRefreshRate.uiFPSDen = pTestCtx->uiFPSDen;
    ALOGE("UpdateFramerate(): Setting FPS to %d / %d\n", stRefreshRate.uiFPSNum,
            stRefreshRate.uiFPSDen);
    status = NvCapSetAttribute(ctx, NvCapAttrType_RefreshRate, (void *)&stRefreshRate,
                sizeof(stRefreshRate));
    if (status != NvCapStatus_Success)
    {
        ALOGE("UpdateFramerate() failed\n");
        return -1;
    }

    return 0;
}

static int UpdateBitrate(NVCAP_CTX ctx, NvCapTestCtx* pTestCtx)
{
    NvCapStatus status = NvCapStatus_Success;

    // Bit rate change
    NvCapBitRate stBitRate;
    stBitRate.uiBitRate = pTestCtx->uiVideoBitRate;
    stBitRate.uiTransmissionRate = pTestCtx->uiTransmissionRate;
    ALOGE("UpdateBitrate(): Setting Bitrate to = %d, Transmission Rate to = %d",
                    stBitRate.uiBitRate, stBitRate.uiTransmissionRate);
    status = NvCapSetAttribute(ctx, NvCapAttrType_BitRate, (void *)&stBitRate,
                sizeof(stBitRate));
    if (status != NvCapStatus_Success)
    {
        ALOGE("UpdateBitrate() failed\n");
        return -1;
    }

    return 0;
}

static int UpdateResolution(NVCAP_CTX ctx, NvCapTestCtx* pTestCtx)
{
    NvCapStatus status = NvCapStatus_Success;

    // Resolution change
    NvCapResolution stResol;
    stResol.uiWidth = pTestCtx->uiWidth;
    stResol.uiHeight = pTestCtx->uiHeight;
    ALOGE("UpdateResolution(): Setting [Width, Height] to [%d, %d]\n",
        stResol.uiWidth, stResol.uiHeight);
    status = NvCapSetAttribute(ctx, NvCapAttrType_Resolution, (void *)&stResol, sizeof(stResol));
    if (status != NvCapStatus_Success)
    {
        ALOGE("UpdateResolution() failed\n");
        return -1;
    }

    return 0;
}

static int UpdateLatencyProfiling(NVCAP_CTX ctx, NvCapTestCtx* pTestCtx)
{
    NvCapStatus status = NvCapStatus_Success;

    // Change latency profiling setting
    NvCapLatencyProfiling stProfiling;
    stProfiling.eLatencyProfileLvl = pTestCtx->eLatencyProfileLvl;
    ALOGE("UpdateLatencyProfiling(): %s latency profiling\n",
            pTestCtx->eLatencyProfileLvl ? "Enable" : "Disable");
    status = NvCapSetAttribute(ctx, NvCapAttrType_LatencyProfiling,
                 (void *)&stProfiling, sizeof(stProfiling));
    if (status != NvCapStatus_Success)
    {
        ALOGE("UpdateLatencyProfiling(): Latency profiling not supported.\n");
    }

    return 0;
}

static int SetAttributes(NVCAP_CTX ctx, NvCapTestCtx* pTestCtx)
{
    NvCapStatus status = NvCapStatus_Success;

    NvCapAttrType attrType = NvCapAttrType_StreamFormat;
    int attrVal = NvCapStreamFormat_MPEG2TS_AV_SingleStream;
    ALOGE("NvCapSetAttribute() attrType = %d, attrVal = %d\n", attrType, attrVal);
    status = NvCapSetAttribute(ctx, attrType, (void *)&attrVal, sizeof(attrVal));
    if (status != NvCapStatus_Success)
    {
        ALOGE("NvCapSetAttribute() failed\n");
        return -1;
    }

    attrType = NvCapAttrType_VideoCodec;
    attrVal = NvCapVideoCodec_H264;
    ALOGE("NvCapSetAttribute() attrType = %d, attrVal = %d\n", attrType, attrVal);
    status = NvCapSetAttribute(ctx, attrType, (void *)&attrVal, sizeof(attrVal));
    if (status != NvCapStatus_Success)
    {
        ALOGE("NvCapSetAttribute() failed\n");
        return -1;
    }

    attrType = NvCapAttrType_AudioCodec;
    if (pTestCtx->uiAudioCodecType)
    {
        attrVal = NvCapAudioCodec_AAC_LC_2;
    }
    else
    {
        attrVal = NvCapAudioCodec_LPCM_16_48_2ch;
    }
    ALOGE("NvCapSetAttribute() attrType = %d, attrVal = %d\n", attrType, attrVal);
    status = NvCapSetAttribute(ctx, attrType, (void *)&attrVal, sizeof(attrVal));
    if (status != NvCapStatus_Success)
    {
        ALOGE("NvCapSetAttribute() failed\n");
        return -1;
    }

    attrType = NvCapAttrType_MuxerParams;
    NvCapMuxerParams stMuxerParams;
    //stMuxerParams.eAudDesc = NvCapAudDesc_WiFiDisplay;
    stMuxerParams.eAudDesc = NvCapAudDesc_Bluray;
    ALOGE("NvCapSetAttribute() attrType = %d, Aud Desc = %d", attrType, stMuxerParams.eAudDesc);
    status = NvCapSetAttribute(ctx, attrType, (void *)&stMuxerParams, sizeof(stMuxerParams));
    if (status != NvCapStatus_Success)
    {
        ALOGE("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_Profile;
    attrVal = NvCapProfile_CBP;
    ALOGE("NvCapSetAttribute() attrType = %d, attrVal = %d", attrType, attrVal);
    status = NvCapSetAttribute(ctx, attrType, (void *)&attrVal, sizeof(attrVal));
    if (status != NvCapStatus_Success)
    {
        ALOGE("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_Level;
    attrVal = NvCapLevel_4_2;
    ALOGE("NvCapSetAttribute() attrType = %d, attrVal = %d", attrType, attrVal);
    status = NvCapSetAttribute(ctx, attrType, (void *)&attrVal, sizeof(attrVal));
    if (status != NvCapStatus_Success)
    {
        ALOGE("NvCapSetAttribute() failed");
        return -1;
    }

    if (-1 == UpdateResolution(ctx, pTestCtx))
    {
        ALOGE("NvCapSetAttribute() failed\n");
        return -1;
    }

    if (-1 == UpdateFramerate(ctx, pTestCtx))
    {
        ALOGE("NvCapSetAttribute() failed\n");
        return -1;
    }

    if (-1 == UpdateBitrate(ctx, pTestCtx))
    {
        ALOGE("NvCapSetAttribute() failed\n");
        return -1;
    }

    if (-1 == UpdateLatencyProfiling(ctx, pTestCtx))
    {
        ALOGE("NvCapSetAttribute() failed\n");
        return -1;
    }

    attrType = NvCapAttrType_FrameSkipInfo;
    NvCapFrameSkip stFrameSkip;
    stFrameSkip.uiEnable = (pTestCtx->iSkipFactor < 0) ? 0 : 1;
    stFrameSkip.uiSkipFactor = (pTestCtx->iSkipFactor < 0) ? 0 : pTestCtx->iSkipFactor;
    ALOGE("NvCapSetAttribute() attrType = %d, skip enable = %d, skip factor = %d\n",
        attrType, stFrameSkip.uiEnable, stFrameSkip.uiSkipFactor);
    status = NvCapSetAttribute(ctx, attrType, (void *)&stFrameSkip, sizeof(stFrameSkip));
    if (status != NvCapStatus_Success)
    {
        ALOGE("NvCapSetAttribute() failed\n");
        return -1;
    }

    attrType = NvCapAttrType_StereoFormat;
    NvCapStereoFormat eStereoFmt = pTestCtx->eStereoFmt;
    ALOGE("NvCapSetAttribute() attrType = %d, eStereoFmt = %d",
        attrType, eStereoFmt);
    status = NvCapSetAttribute(ctx, attrType, (void *)&eStereoFmt, sizeof(eStereoFmt));
    if (status != NvCapStatus_Success)
    {
        ALOGE("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_SinkInfo;
    NvCapSinkInfo stSinkInfo = pTestCtx->stSinkInfo;
    ALOGE("NvCapSetAttribute() attrType = %d, uiStereoCapable = %d",
        attrType, stSinkInfo.uiStereoCapable);
    status = NvCapSetAttribute(ctx, attrType, (void *)&stSinkInfo, sizeof(stSinkInfo));
    if (status != NvCapStatus_Success)
    {
        ALOGE("NvCapSetAttribute() failed");
        return -1;
    }

    attrType = NvCapAttrType_MultiSliceInfo;
    /* Slicing Configurations for CEA/VESA/HH
        MIN_SLICE_SIZE: 480
        MAX_SLICE_SIZE_RATIO: 5
        MAX_SLICE_SIZE: 2400
        MAX_SLICE_NUM: 7
         640 x  480:  600 MB x  2 slices
         720 x  480:  675 MB x  2 slices
         720 x  576:  540 MB x  3 slices
        1280 x  720:  720 MB x  5 slices
        1920 x 1080: 2040 MB x  4 slices
         800 x  600:  950 MB x  2 slices
        1024 x  768:  512 MB x  6 slices
        1152 x  864:  648 MB x  6 slices
        1280 x  768:  640 MB x  6 slices
        1280 x  800:  800 MB x  5 slices
        1360 x  768:  680 MB x  6 slices
        1366 x  768:  688 MB x  6 slices
        1280 x 1024: 1280 MB x  4 slices
        1400 x 1050:  968 MB x  6 slices
        1440 x  900: 1710 MB x  3 slices
        1600 x  900: 1900 MB x  3 slices
        1600 x 1200: 1500 MB x  5 slices
        1680 x 1024: 1680 MB x  4 slices
        1680 x 1050: 1155 MB x  6 slices
        1920 x 1200: 1800 MB x  5 slices
         800 x  480:  500 MB x  3 slices
         854 x  480:  540 MB x  3 slices
         864 x  480:  540 MB x  3 slices
         640 x  360:  920 MB x  1 slices
         960 x  540: 1020 MB x  2 slices
         848 x  480:  530 MB x  3 slices
    */
    NvCapMultiSlice stMultiSlice = {0, 0, 0, 0, 0};

    stMultiSlice.uiEnableSliceIntraRefresh = pTestCtx->uiSIR;
    stMultiSlice.uiEnableSliceBuffers = pTestCtx->uiSLE;
    stMultiSlice.uiMinSliceSzMBs = 480;
    stMultiSlice.uiMaxSliceSzMBs = 480*5;
    stMultiSlice.uiMaxSliceNum = 7;

    ALOGE("NvCapSetAttribute() attrType = %d, uiEnableSliceIntraRefresh = %d, uiEnableSliceBuffers = %d, "
          "uiMinSliceSzMBs = %d, uiMaxSliceSzMBs = %d, uiMaxSliceNum = %d",
        attrType, stMultiSlice.uiEnableSliceIntraRefresh, stMultiSlice.uiEnableSliceBuffers,
        stMultiSlice.uiMinSliceSzMBs, stMultiSlice.uiMaxSliceSzMBs, stMultiSlice.uiMaxSliceNum);
    status = NvCapSetAttribute(ctx, attrType, (void *)&stMultiSlice, sizeof(stMultiSlice));
    if (status != NvCapStatus_Success) {
        ALOGE("NvCapSetAttribute() failed");
        return -1;
    }

    return 0;
}

static void usage()
{
    printf("nvcap_test usage:\n");
    printf("nvcap_test <Commandline options>\n");
    printf("\n**********************************************\n");
    printf("**********************************************\n");
    printf("Commandline Options:\n\n");
    printf("-help       : Prints the usage information\n");
    printf("-m          : Run the test in manual mode\n");
    printf("-w <width>  : Specify the capture width\n");
    printf("-h <height> : Specify the capture height\n");
    printf("-f <N> <D>  : Framerate (in frames per second) as Numerator and Denominator\n");
    printf("-b <bps>    : Bitrate in bits per second\n");
    printf("-t <bps>    : Transmission rate in bits per second\n");
    printf("-s <factor> : Use frame skipping, specify the skip factor\n");
    printf("-d <val>    : Specify whether to dump to disk. 1 => Dump (default), 0 => Don't Dump\n");
    printf("-n <count>  : Specify how many times the test should be repeated\n");
    printf("-l          : Enable latency profiling\n");
    printf("-stereo     : Enable stereo support on sink\n");
    printf("-ac <val>   : Specify audio codec type. 0 => LPCM(default), 1 => AAC\n");
    printf("-sle <val>  : Specify whether to enable slice buffers. 0 => disable(default), 1 => enable\n");
    printf("-ph <val>   : Specify whether to enable power hint. 0 => disable, 1 => enable(default)\n");
    printf("-sir <val>  : Specify whether to enable slice intra refresh. 0 => disable, 1 => enable(default)\n");
    printf("\n**********************************************\n");
    printf("**********************************************\n");
    printf("Runtime Options: You will be prompted to enter additional arguments, if needed\n");
    printf("h : Prints the usage information\n");
    printf("s : Start/Resume capture\n");
    printf("e : Stop capture\n");
    printf("p : Pause capture\n");
    printf("i : Insert IDR\n");
    printf("f : Will be prompted for the new framerate\n");
    printf("b : Will be prompted for the new Video Bitrate and Transmission rate\n");
    printf("r : Will be prompted for the new width and height\n");
    printf("n : Prompt for network stall time; sleep for 'n' ms when next buffer is requested\n");
    printf("l : Change latency profiling settings\n");
}

static void InitCtx()
{
    memset(&g_InitialCtx, 0, sizeof(g_InitialCtx));

    g_InitialCtx.uiWidth = 1280;
    g_InitialCtx.uiHeight = 720;
    g_InitialCtx.uiFPSNum = 30000;
    g_InitialCtx.uiFPSDen = 1001;
    g_InitialCtx.uiVideoBitRate = 6900000;
    g_InitialCtx.uiTransmissionRate = 13800000;
    g_InitialCtx.iSkipFactor = -1;
    g_InitialCtx.uiStallMS = 0;
    g_InitialCtx.uiNumIter = 1;
    g_InitialCtx.uiDump = 1;
    g_InitialCtx.uiManual = 0;
    g_InitialCtx.eLatencyProfileLvl = NvCapLatencyProfilingLevel_None;
    g_InitialCtx.uiAudioCodecType = 0;
    g_InitialCtx.uiSLE = 0;
    g_InitialCtx.uiSIR = 1;
    g_InitialCtx.eStereoFmt = NvCapStereoFormat_None;
    memset(&g_InitialCtx.stSinkInfo, 0, sizeof(g_InitialCtx.stSinkInfo));
    g_InitialCtx.pFile = NULL;
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    g_InitialCtx.uiPowerHint = 1; // enable power hint by default
    g_InitialCtx.pPowerSvcClient = NULL;
    g_InitialCtx.prevTimestamp = 0;
#endif // USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
}

// Returns 0 if successful, -1 if there is a failure
// As the elements are parsed, we update the g_InitCtx
// context structure
static int ParseCmdline(int argc, char *argv[])
{
    int i = 0;
    if (argc == 1)
    {
        // No cmdline args, use the defaults
        return 0;
    }

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            printf("Must have options prefixed by a '-'\n");
            return -1;
        }

        if (!strcmp(&argv[i][1], "help"))
        {
            // Print the usage information. Return a failure, which will
            // ensure that the usage() will get printed
            return -1;
        }
        else if (!strcmp(&argv[i][1], "m"))
        {
            // User wants to run the test in manual mode (that is, user will
            // need to indicate when to start/stop the test, etc.).
            g_InitialCtx.uiManual = 1;
        }
        else if (!strcmp(&argv[i][1], "w"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiWidth = atoi(argv[i]);
        }
        else if (!strcmp(&argv[i][1], "h"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiHeight = atoi(argv[i]);
        }
        else if (!strcmp(&argv[i][1], "f"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiFPSNum = atoi(argv[i]);

            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-2][1]);
                return -1;
            }
            g_InitialCtx.uiFPSDen = atoi(argv[i]);
        }
        else if (!strcmp(&argv[i][1], "b"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiVideoBitRate = atoi(argv[i]);
         }
        else if (!strcmp(&argv[i][1], "t"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiTransmissionRate = atoi(argv[i]);
        }
        else if (!strcmp(&argv[i][1], "s"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.iSkipFactor = atoi(argv[i]);
        }
        else if (!strcmp(&argv[i][1], "d"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiDump = atoi(argv[i]);
        }
        else if (!strcmp(&argv[i][1], "n"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiNumIter = atoi(argv[i]);
        }
        else if (!strcmp(&argv[i][1], "l"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }

            switch(atoi(argv[i])) {
                case 1: g_InitialCtx.eLatencyProfileLvl = NvCapLatencyProfilingLevel_Summary;break;
                case 2: g_InitialCtx.eLatencyProfileLvl = NvCapLatencyProfilingLevel_Complete;break;
                default: g_InitialCtx.eLatencyProfileLvl = NvCapLatencyProfilingLevel_None;break;
            }

        }
        else if (!strcmp(&argv[i][1], "stereo"))
        {
            // Enable sink side stereo support
            g_InitialCtx.stSinkInfo.uiStereoCapable = 1;
        }
        else if (!strcmp(&argv[i][1], "ac"))
        {
            // Audio Codec type
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiAudioCodecType = atoi(argv[i]);
        }
        else if (!strcmp(&argv[i][1], "sle"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiSLE = atoi(argv[i]);
        }
#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
        else if (!strcmp(&argv[i][1], "ph"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiPowerHint = atoi(argv[i]);
        }
#endif // USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
        else if (!strcmp(&argv[i][1], "sir"))
        {
            if (++i >= argc)
            {
                printf("Missing argument for the cmdline switch '-%c'\n", argv[i-1][1]);
                return -1;
            }
            g_InitialCtx.uiSIR = atoi(argv[i]);
        }
        else
        {
            // Invalid option
            printf("Unrecognized commandline option %c\n", argv[i][1]);
            return -1;
        }
    }

    return 0;
}

int
main (int argc, char *argv[])
{
    NvCapStatus status = NvCapStatus_Success;
    unsigned int i = 0;
    int errorVal = 0;

    memset(&g_InitialCtx, 0, sizeof(g_InitialCtx));
    memset(&g_FinalCtx, 0, sizeof(g_FinalCtx));

    // Allocate the list to store the capture buffers
    if(!AllocateBuffers())
    {
        ALOGE("******** List allocation failed");
        return -1;
    }

    // Initialize the NvCapTest context with default values. These can
    // get overridden by cmdline or runtime parameter updates
    InitCtx();

    if (-1 == ParseCmdline(argc, argv))
    {
        usage();
        ReleaseBuffers();
        return -1;
    }

    ALOGE("******** [w,h,fps,bps,skip,stall,iter,dump,manual]=[%d,%d,%d/%d,%d,%d,%d,%d,%d,%d]\n",
        g_InitialCtx.uiWidth, g_InitialCtx.uiHeight, g_InitialCtx.uiFPSNum, g_InitialCtx.uiFPSDen,
        g_InitialCtx.uiVideoBitRate, g_InitialCtx.iSkipFactor, g_InitialCtx.uiStallMS,
        g_InitialCtx.uiNumIter, g_InitialCtx.uiDump, g_InitialCtx.uiManual);

    ALOGE("******** Start running the %d iterations of the test now\n", g_InitialCtx.uiNumIter);

    // Need to repeat the test "g_InitialCtx.uiNumIter" number of times
    for (i = 0; i < g_InitialCtx.uiNumIter; i++)
    {
        bool bDone = false;
        ALOGE("******** Starting Test Iteration %d\n", i);

        // g_InitialCtx has the one time initial setup, based on
        // the presets and cmdline params. g_FinalCtx is what is used
        // in the test, and it reflects runtime param updates, if any
        // If a previous iteration exited abruptly, make sure that the
        // dump file is closed
        if (g_FinalCtx.pFile != NULL)
        {
            fclose(g_FinalCtx.pFile);
            g_FinalCtx.pFile = NULL;
        }

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
        if (g_FinalCtx.pPowerSvcClient)
        {
            delete g_FinalCtx.pPowerSvcClient;
            g_FinalCtx.pPowerSvcClient = NULL;
            g_FinalCtx.prevTimestamp = 0;
        }
#endif // USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS

        g_FinalCtx = g_InitialCtx;

        // Create the dump file
        if (g_FinalCtx.uiDump)
        {
            ALOGE("Creating dump file\n");
            char fName[256];
            sprintf(fName, "/data/VideoTest%02d.ts", i);
            ALOGE("Filename is %s\n", fName);
            g_FinalCtx.pFile = fopen((const char *)fName, "w");
            if (g_FinalCtx.pFile == NULL)
            {
                ALOGE("Could not create the dump file, abort test!\n");
                continue;
            }
        }

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
        if (g_FinalCtx.uiPowerHint)
        {
            g_FinalCtx.pPowerSvcClient = new android::PowerServiceClient();
            if (g_FinalCtx.pPowerSvcClient)
            {
                g_FinalCtx.pPowerSvcClient->sendPowerHint((int)POWER_HINT_MIRACAST,(void *)1);
                g_FinalCtx.prevTimestamp = NvOsGetTimeUS();
            }
            else
            {
                ALOGE("nvcap_test: initialize - failed to initialize PowerServiceClient!");
            }
        }
#endif // USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS

        // Initialize the NvCap library
        NVCAP_CTX ctx;
        NvCapCallbackInfo CBInfo;
        CBInfo.pfnGetBuffersCB = GetBuffersCB;
        CBInfo.pfnSendBuffersCB = SendBuffersCB;
        CBInfo.pfnNotifyEventCB = NotifyEventCB;
        status = NvCapInit(&ctx, &CBInfo);

        if ((status != NvCapStatus_Success) || (ctx == NULL))
        {
            ALOGE("******** Init failed\n");
            continue;
        }

        if (0 != SetAttributes(ctx, &g_FinalCtx))
        {
            ALOGE("******** Failed to set attribute\n");
            continue;
        }

        if (g_InitialCtx.uiManual == 0)
        {
            // In auto mode, just run the test and exit
            // Start the capture. Even if the start fails, let
            // the pause and release happen to cleanup any resources
            ALOGE("******** Start the capture in automatic mode\n");
            if (NvCapStatus_Success != NvCapStart(ctx))
            {
                ALOGE("NvCapStart() failed");
                errorVal = -1;
            }

            // Sleep for 30 seconds
            ALOGE("******** Sleeping for 120 seconds...\n");
            sleep(120);

            ALOGE("******** Wake up! Stop WFD capture and release the capture library\n");
            if (NvCapStatus_Success != NvCapPause(ctx))
            {
                ALOGE("NvCapPause() failed");
                errorVal = -1;
            }
            if (NvCapStatus_Success != NvCapRelease(ctx))
            {
                ALOGE("NvCapRelease() failed");
                errorVal = -1;
            }

            ALOGE("************ Auto Run of the test is finished!\n");
            continue;
        }

        // Now wait for the user to provide commands
        ALOGE("NvCap Initialized, waiting for user commands...\n");

        while (!bDone)
        {
            char option;
            scanf("%c", &option);
            switch (option)
            {
                case 'h':
                    // Usage
                    usage();
                    break;
                case 's':
                    // Start the capture
                    ALOGE("Start the capture\n");
                    if (NvCapStatus_Success != NvCapStart(ctx))
                    {
                        ALOGE("NvCapStart() failed");
                    }
                    break;
                case 'p':
                    // Pause the capture
                    ALOGE("Pause the capture\n");
                    if (NvCapStatus_Success != NvCapPause(ctx))
                    {
                        ALOGE("NvCapPause() failed");
                    }
                    break;
                case 'e':
                    // End capture
                    ALOGE("End the capture\n");
                    if (NvCapStatus_Success != NvCapRelease(ctx))
                    {
                        ALOGE("NvCapRelease() failed");
                    }
                    bDone = true;
                    break;
                case 'i':
                    // Insert IDR
                    ALOGE("Insert IDR\n");
                    if (-1 == InsertIDR(ctx))
                    {
                        ALOGE("Failed to Insert IDR!\n");
                    }
                    break;
                case 'b':
                    // Bitrate
                    printf("Enter Bitrates <VideoBitRate> <TransmissionRate>: ");
                    scanf("%d %d", &(g_FinalCtx.uiVideoBitRate), &(g_FinalCtx.uiTransmissionRate));
                    if (-1 == UpdateBitrate(ctx, &g_FinalCtx))
                    {
                        ALOGE("Failed to Update the Bitrates!\n");
                    }
                    break;
                case 'f':
                    // Framerate
                    printf("Enter Framerate (<Numerator> <Denominator>): ");
                    scanf("%d %d", &(g_FinalCtx.uiFPSNum), &(g_FinalCtx.uiFPSDen));
                    if (-1 == UpdateFramerate(ctx, &g_FinalCtx))
                    {
                        ALOGE("Failed to Update the Framerate!\n");
                    }
                    break;
                case 'r':
                    // Resolution
                    printf("Enter <w> <h>: ");
                    scanf("%d %d", &(g_FinalCtx.uiWidth), &(g_FinalCtx.uiHeight));
                    if (-1 == UpdateResolution(ctx, &g_FinalCtx))
                    {
                        ALOGE("Failed to Update the Resolution!\n");
                    }
                    break;
                case 'n':
                    // Simulate network/app stall
                    if (g_FinalCtx.uiStallMS != 0)
                    {
                        ALOGE("Previous stall test not yet executed. Undefined results.\n");
                    }
                    printf("Enter stall time in milliseconds: ");
                    scanf("%d", &(g_FinalCtx.uiStallMS));
                    ALOGE("Requested a stall time of %d ms\n", g_FinalCtx.uiStallMS);
                    gettimeofday(&tv, NULL);
                    break;
                case 'l':
                    // Enable latency profiling
                    printf("Enable latency profiling? (1/2/0): ");
                    int profLvl;
                    scanf("%d", &profLvl);

                    switch(profLvl) {
                        case 1: g_FinalCtx.eLatencyProfileLvl = NvCapLatencyProfilingLevel_Summary;break;
                        case 2: g_FinalCtx.eLatencyProfileLvl = NvCapLatencyProfilingLevel_Complete;break;
                        default: g_FinalCtx.eLatencyProfileLvl = NvCapLatencyProfilingLevel_None;break;
                    }

                    if (-1 == UpdateLatencyProfiling(ctx, &g_FinalCtx))
                    {
                        ALOGE("Failed to change latency profiling setting!\n");
                    }
                    break;
                case '\n':
                    // Ignore newline
                    break;
                default:
                    // Invalid option
                    printf("Unrecognized runtime option\n");
                    usage();
                    break;
            }
        }

        // Test is finished, close the dump file if needed
        if (g_FinalCtx.pFile != NULL)
        {
            fclose(g_FinalCtx.pFile);
            g_FinalCtx.pFile = NULL;
        }

        ALOGE("******** Finished Test Iteration %d\n", i);
    }

    ReleaseBuffers();   // Free the output buffer lists
    // If the last iteration exits abruptly, make sure the dump file is closed
    if (g_FinalCtx.pFile != NULL)
    {
        fclose(g_FinalCtx.pFile);
        g_FinalCtx.pFile = NULL;
    }

#if USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    if (g_FinalCtx.pPowerSvcClient) {
        delete g_FinalCtx.pPowerSvcClient;
        g_FinalCtx.pPowerSvcClient = NULL;
        g_FinalCtx.prevTimestamp = 0;
    }
#endif // USE_NV_ANDROID_FRAMEWORK_ENHANCEMENTS
    ALOGE("************ EXIT: nvcap_test result = %d\n", errorVal);
    return errorVal;
}
