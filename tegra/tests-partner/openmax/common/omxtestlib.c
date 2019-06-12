/*
 * Copyright (c) 2007 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "testassert.h"
#include "samplebase.h"
#include "samplewindow.h"
#include "nvodm_imager.h"
#include "nvtest.h"
#include "nvfxmath.h"
#include "nvrm_power.h"
#include "omxtestlib.h"

#if NVOS_IS_LINUX
#include <stdio.h>
#endif

// Pause playback
OMX_ERRORTYPE PlaybackPause(NvxGraph *pCurGraph, TestContext *pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    // Stop all components in the graph
    PauseClockGraph(pCurGraph->hClock);
    eError = TransitionAllComponentsToState(&pCurGraph->oCompGraph, OMX_StatePause, OMX_TRUE, pAppData);
    TEST_ASSERT_NOTERROR(eError);
    pCurGraph->playState = OMX_StatePause;
    return eError;
}

// Resume playback from pause mode
OMX_ERRORTYPE PlaybackResumeFromPause(NvxGraph *pCurGraph, TestContext *pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    // Start all components in the graph
    eError = TransitionAllComponentsToState(&pCurGraph->oCompGraph, OMX_StateExecuting, OMX_TRUE, pAppData);
    TEST_ASSERT_NOTERROR(eError);
    UnPauseClockGraph(pCurGraph->hClock);
    pCurGraph->playState = OMX_StateExecuting;
    return eError;
}

void SetLowMem(OMX_HANDLETYPE hComp, OMX_BOOL bLowMem)
{
    NVX_PARAM_LOWMEMMODE oLow;
    OMX_INDEXTYPE eIndex;
    OMX_ERRORTYPE eError;

    INIT_PARAM(oLow);
    eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_PARAM_LOWMEMMODE, &eIndex);
    TEST_ASSERT_NOTERROR(eError);

    oLow.bLowMemMode = bLowMem;

    OMX_SetParameter(hComp, eIndex, &oLow);
}

// Create a file reader/writer, and set the input/output filename
OMX_ERRORTYPE InitReaderWriter(char *component, char *filename,
                               OMX_HANDLETYPE *hComp, NvxGraph* pGraph,
                               TestContext *pAppData, OMX_CALLBACKTYPE *pCallbacks)
{
    OMX_ERRORTYPE eError;
    OMX_INDEXTYPE eIndexParamFilename;
    NVX_PARAM_FILENAME oFilenameParam;

    eError = OMX_GetHandle(hComp, component, pAppData, pCallbacks);
    TEST_ASSERT_NOTERROR(eError);

    eError = OMX_GetExtensionIndex(*hComp, NVX_INDEX_PARAM_FILENAME,
                                   &eIndexParamFilename);
    TEST_ASSERT_NOTERROR(eError);

    SetLowMem(*hComp, pGraph->bLowMem);

    if (filename)
    {
        INIT_PARAM(oFilenameParam);
        oFilenameParam.pFilename = filename;
        eError = OMX_SetParameter(*hComp, eIndexParamFilename, &oFilenameParam);
    }

    if (pGraph->nReadBufferSize)
    {
        OMX_VIDEO_CONFIG_NALSIZE oNalSize;
        INIT_PARAM(oNalSize);
        oNalSize.nNaluBytes = pGraph->nReadBufferSize;
        eError = OMX_SetConfig(*hComp, OMX_IndexConfigVideoNalSize, &oNalSize);
        TEST_ASSERT_NOTERROR(eError);
    }

    return eError;
}

// Create a clock component, disable all ports, and set it to use audio as
// reference
OMX_ERRORTYPE InitClock(char *component, NvxGraph *pGraph, TestContext *pAppData, OMX_CALLBACKTYPE *pCallbacks)
{
    OMX_ERRORTYPE eError;
    OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE oActiveClockType;

    eError = OMX_GetHandle(&(pGraph->hClock), component, pAppData, pCallbacks);
    TEST_ASSERT_NOTERROR(eError);

    OMX_SendCommand(pGraph->hClock, OMX_CommandPortDisable, -1, 0);

    INIT_PARAM(oActiveClockType);
    oActiveClockType.eClock = OMX_TIME_RefClockAudio;
    eError = OMX_SetConfig(pGraph->hClock, OMX_IndexConfigTimeActiveRefClock,
                           &oActiveClockType);

    return eError;
}

void SetProfileMode(OMX_HANDLETYPE hComp, NvxProfile *pProf)
{
    if (pProf->bProfile || pProf->bVerbose || pProf->bStubOutput ||
        pProf->nForceLocale || pProf->bNoAVSync || pProf->nAVSyncOffset ||
        !pProf->bFlip || pProf->nFrameDrop ||
        pProf->bSanity || pProf->bDisableRendering)
    {
        OMX_INDEXTYPE eIndexConfigProfile;
        NVX_CONFIG_PROFILE oProf;
        OMX_ERRORTYPE eError;

        INIT_PARAM(oProf);

        eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_CONFIG_PROFILE,
                                       &eIndexConfigProfile);
        TEST_ASSERT_NOTERROR(eError);

        oProf.nPortIndex = 0;
        oProf.bProfile = pProf->bProfile;
        if (pProf->bProfile)
            NvOsStrncpy(oProf.ProfileFileName, pProf->ProfileFileName, NvOsStrlen(pProf->ProfileFileName)+1);
        oProf.bVerbose = pProf->bVerbose;
        oProf.bStubOutput = pProf->bStubOutput;
        oProf.nForceLocale = pProf->nForceLocale;
        oProf.nNvMMProfile = pProf->nProfileOpts;
        oProf.bNoAVSync = pProf->bNoAVSync;
        oProf.nAVSyncOffset = pProf->nAVSyncOffset;
        oProf.bFlip = pProf->bFlip;
        oProf.nFrameDrop = pProf->nFrameDrop;
        oProf.bSanity = pProf->bSanity;
        oProf.bDisableRendering = pProf->bDisableRendering;

        OMX_SetConfig(hComp, eIndexConfigProfile, &oProf);
    }
}

void GetProfile(OMX_HANDLETYPE hComp, NvxProfile *pProf)
{
    OMX_INDEXTYPE eIndexConfigProfile;
    NVX_CONFIG_PROFILE oProf;
    OMX_ERRORTYPE eError;

    INIT_PARAM(oProf);

    eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_CONFIG_PROFILE,
                                   &eIndexConfigProfile);
    TEST_ASSERT_NOTERROR(eError);

    OMX_GetConfig(hComp, eIndexConfigProfile, &oProf);

    pProf->nAvgFPS = oProf.nAvgFPS;
    pProf->nTotFrameDrops = oProf.nTotFrameDrops;

    pProf->nTSPreviewStart = oProf.nTSPreviewStart;
    pProf->nTSCaptureStart = oProf.nTSCaptureStart;
    pProf->nTSCaptureEnd = oProf.nTSCaptureEnd;
    pProf->nTSPreviewEnd = oProf.nTSPreviewEnd;
    pProf->nTSStillConfirmationFrame = oProf.nTSStillConfirmationFrame;
    pProf->nTSFirstPreviewFrameAfterStill = oProf.nTSFirstPreviewFrameAfterStill;
    pProf->nPreviewStartFrameCount = oProf.nPreviewStartFrameCount;
    pProf->nPreviewEndFrameCount = oProf.nPreviewEndFrameCount;
    pProf->nCaptureStartFrameCount = oProf.nCaptureStartFrameCount;
    pProf->nCaptureEndFrameCount = oProf.nCaptureEndFrameCount;
    pProf->xExposureTime = oProf.xExposureTime;
    pProf->nExposureISO = oProf.nExposureISO;
    pProf->nBadFrameCount = oProf.nBadFrameCount;
}

void SetAudioOutput(OMX_HANDLETYPE hComp, NvxGraph *pGraph)
{
    OMX_INDEXTYPE eIndexConfigOutputType;
    NVX_CONFIG_AUDIOOUTPUT ao;
    OMX_ERRORTYPE eError;

    INIT_PARAM(ao);

    eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_CONFIG_AUDIO_OUTPUT,
                                   &eIndexConfigOutputType);
    TEST_ASSERT_NOTERROR(eError);

    ao.eOutputType = pGraph->eAudOutputType;

    OMX_SetConfig(hComp, eIndexConfigOutputType, &ao);
}

void SetVolume(OMX_HANDLETYPE hComp, NvxGraph *pGraph)
{
    OMX_AUDIO_CONFIG_VOLUMETYPE vt;

    if (pGraph->volume == -1)
        return;

    if (pGraph->volume > 100)
        pGraph->volume = 100;

    if (pGraph->volume == 0)
    {
        OMX_AUDIO_CONFIG_MUTETYPE mt;
        INIT_PARAM(mt);
        mt.nPortIndex = 0;
        mt.bMute = OMX_TRUE;

        OMX_SetConfig(hComp, OMX_IndexConfigAudioMute, &mt);
        return;
    }

    INIT_PARAM(vt);
    vt.nPortIndex = 0;
    vt.bLinear = OMX_TRUE;
    vt.sVolume.nMin = 0;
    vt.sVolume.nMax = 100;
    vt.sVolume.nValue = pGraph->volume;

    OMX_SetConfig(hComp, OMX_IndexConfigAudioVolume, &vt);
}

void SetEQ(OMX_HANDLETYPE hComp, NvxGraph *pGraph)
{
    NVX_CONFIG_AUDIO_GRAPHICEQ oGraphicEqProp;
    OMX_INDEXTYPE  eIndex;
    int i,ch;

    OMX_GetExtensionIndex(hComp, NVX_INDEX_CONFIG_AUDIO_GRAPHICEQ, &eIndex);
    INIT_PARAM(oGraphicEqProp);

    oGraphicEqProp.nPortIndex = 0;
    oGraphicEqProp.bEnable = OMX_TRUE;

    for(ch = 0; ch < EQ_NO_CHANNEL; ch++)
    {
        for (i = 0; i < EQBANDS; i++)
            oGraphicEqProp.dBGain[ch][i] = pGraph->dBGain[ch][i];
    }
    OMX_SetConfig(hComp, eIndex, &oGraphicEqProp);
}

void SetSize(OMX_HANDLETYPE hComp, NvxGraph *pGraph)
{
    OMX_CONFIG_POINTTYPE oPoint;
    OMX_FRAMESIZETYPE oSize;

    if (pGraph->nX == 0 && pGraph->nY == 0 &&
        pGraph->nWidth == 0 && pGraph->nHeight == 0)
        return;

    INIT_PARAM(oPoint);
    oPoint.nPortIndex = 0;
    oPoint.nX = pGraph->nX;
    oPoint.nY = pGraph->nY;

    OMX_SetConfig(hComp, OMX_IndexConfigCommonOutputPosition, &oPoint);

    INIT_PARAM(oSize);
    oSize.nPortIndex = 0;
    oSize.nWidth = pGraph->nWidth;
    oSize.nHeight = pGraph->nHeight;

    OMX_SetConfig(hComp, OMX_IndexConfigCommonOutputSize, &oSize);
}

void SetCrop(OMX_HANDLETYPE hComp, NvxGraph *pGraph)
{
    OMX_CONFIG_RECTTYPE oCrop;

    if (pGraph->nCropX == 0 && pGraph->nCropY == 0 &&
        pGraph->nCropWidth == 0 && pGraph->nCropHeight == 0)
        return;

    INIT_PARAM(oCrop);
    oCrop.nPortIndex = 0;
    oCrop.nLeft = pGraph->nCropX;
    oCrop.nTop = pGraph->nCropY;
    oCrop.nWidth = pGraph->nCropWidth;
    oCrop.nHeight = pGraph->nCropHeight;

    OMX_SetConfig(hComp, OMX_IndexConfigCommonInputCrop, &oCrop);
}

void SetKeepAspect(OMX_HANDLETYPE hComp, NvxGraph *pGraph)
{
    OMX_INDEXTYPE eIndex;
    NVX_CONFIG_KEEPASPECT aspect;
    OMX_ERRORTYPE eError;

    INIT_PARAM(aspect);

    eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_CONFIG_KEEPASPECT,
                                   &eIndex);
    TEST_ASSERT_NOTERROR(eError);

    aspect.bKeepAspect = pGraph->bKeepAspect;

    OMX_SetConfig(hComp, eIndex, &aspect);
}

void SetAudioOnlyHint(OMX_HANDLETYPE hComp, OMX_BOOL bAudioOnlyHint)
{
    NVX_CONFIG_AUDIOONLYHINT oAO;
    OMX_INDEXTYPE eIndex;
    OMX_ERRORTYPE eError;

    INIT_PARAM(oAO);
    eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_CONFIG_AUDIOONLYHINT, &eIndex);
    TEST_ASSERT_NOTERROR(eError);

    oAO.bAudioOnlyHint = bAudioOnlyHint;

    OMX_SetConfig(hComp, eIndex, &oAO);
}

void SetDisableTimestampUpdates(OMX_HANDLETYPE hComp, OMX_BOOL bDisableUpdates)
{
    NVX_CONFIG_DISABLETSUPDATES oConf;
    OMX_INDEXTYPE eIndex;
    OMX_ERRORTYPE eError;

    INIT_PARAM(oConf);
    eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_CONFIG_DISABLETIMESTAMPUPDATES, &eIndex);
    TEST_ASSERT_NOTERROR(eError);

    oConf.bDisableTSUpdates = bDisableUpdates;

    OMX_SetConfig(hComp, eIndex, &oConf);
}

void SetFileCacheSize(OMX_HANDLETYPE hComp, OMX_U32 nSize)
{
    NVX_CONFIG_FILECACHESIZE oConf;
    OMX_INDEXTYPE eIndex;
    OMX_ERRORTYPE eError;

    INIT_PARAM(oConf);
    eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_CONFIG_FILECACHESIZE, &eIndex);
    TEST_ASSERT_NOTERROR(eError);

    oConf.nFileCacheSize = nSize;

    OMX_SetConfig(hComp, eIndex, &oConf);
}

NvF32 GetCurrentMediaTime(OMX_HANDLETYPE hClock)
{
    OMX_TIME_CONFIG_TIMESTAMPTYPE stamp;
    stamp.nPortIndex = OMX_ALL;

    OMX_GetConfig(hClock, OMX_IndexConfigTimeCurrentMediaTime, &stamp);
    return (NvF32) (stamp.nTimestamp * (1.0f/1000000.0f));
}


void SetSmartDimmer(OMX_HANDLETYPE hComp, NvxGraph *pGraph)
{
    OMX_INDEXTYPE eIndex;
     NVX_CONFIG_SMARTDIMMER sd;
    OMX_ERRORTYPE eError;

    INIT_PARAM(sd);

    eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_CONFIG_SMARTDIMMER,
                                   &eIndex);
    TEST_ASSERT_NOTERROR(eError);

    sd.bSmartDimmerEnable = pGraph->bSmartDimmerEnable;

    OMX_SetConfig(hComp, eIndex, &sd);
}

#define FILENAMESIZE 1024
OMX_ERRORTYPE RemoveFile(OMX_STRING filename)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvOsStatType stat = { 0, NvOsFileType_File };
    NvError err;
    err = NvOsStat(filename, &stat);
    if ((err == NvSuccess) && (stat.type == NvOsFileType_File))
    {
#if NVOS_IS_LINUX
    remove(filename);
#endif
    }
    else
    {
        //NvTestPrintf("Could not find file %s\n", filename);
        eError = OMX_ErrorUndefined;
    }
    return eError;
}

#define LENGTH_FILE_INDEX   (9 + 4)   // 12345678.jpg
#define FILE_SUFFIX          ".jpg"
OMX_ERRORTYPE RemoveJPEGFiles(char *basename)
{
    OMX_STRING pFilenameWithIndex;
    int len = NvOsStrlen(basename) + LENGTH_FILE_INDEX;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pFilenameWithIndex = NvOsAlloc(len);
    if(!pFilenameWithIndex)
    {
        NvTestPrintf("no memory for pFilenameWithIndex\n");
        eError = OMX_ErrorInsufficientResources;
    }
    else
    {
        int id;

        for (id = 0; id < 1000; id++)
        {
            NvOsSnprintf(pFilenameWithIndex, len, "%s%08d%s", basename, id, FILE_SUFFIX);
            if (OMX_ErrorNone != RemoveFile(pFilenameWithIndex))
                break;
        }
        NvOsFree(pFilenameWithIndex);
    }

    return eError;
}

void GetFileCacheInfo(OMX_HANDLETYPE hComp)
{
    OMX_INDEXTYPE eIndex;
    NVX_CONFIG_FILECACHEINFO oCacheInfo;
    OMX_ERRORTYPE eError;

    INIT_PARAM(oCacheInfo);

    eError = OMX_GetExtensionIndex(hComp, NVX_INDEX_CONFIG_FILECACHEINFO,
                                   &eIndex);
    TEST_ASSERT_NOTERROR(eError);
    OMX_GetConfig(hComp, eIndex, &oCacheInfo);

    NvTestPrintf("");
    NvTestPrintf("Begin = %lx\n", oCacheInfo.nDataBegin);
    NvTestPrintf("First = %lx\n", oCacheInfo.nDataFirst);
    NvTestPrintf("Current = %lx\n", oCacheInfo.nDataCur);
    NvTestPrintf("Last = %lx\n", oCacheInfo.nDataLast);
    NvTestPrintf("End = %lx\n", oCacheInfo.nDataEnd);
}
