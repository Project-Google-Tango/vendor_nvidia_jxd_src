/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "EGL/egl.h"
#include "GLES2/gl2.h"
#include "GLES2/gl2ext.h"
#include "nvwinsys.h"
#include "nvxplayergraph.h"
#include "nvxframework.h"
#include "NVOMX_RendererExtensions.h"

static OMX_VERSIONTYPE vOMX;

// Helpful macros
#define NOTSET_U8 ((OMX_U8)0xDE)
#define NOTSET_U16 ((OMX_U16)0xDEDE)
#define NOTSET_U32 ((OMX_U32)0xDEDEDEDE)
#define INIT_PARAM(_X_)  (memset(&(_X_), NOTSET_U8, sizeof(_X_)), ((_X_).nSize = sizeof (_X_)), (_X_).nVersion = vOMX)
typedef enum
{
    StatusCode_ErrorNotOpen = -1,
    StatusCode_ErrorOpenFailed = -2,
    StatusCode_ErrorAllocateFailed = -4,
    StatusCode_ErrorNotSupported = -8,
    StatusCode_ErrorNotReady = -16,
    StatusCode_StateInit = 0,
    StatusCode_StateError = 1,
    StatusCode_StateOpen = 2,
    StatusCode_NoError = 3,
    StatusCode_Force32 = 0x7FFFFFFF
}StatusCode;

typedef enum
{
    TestCmd_Pause = 1,
    TestCmd_ComplexSeek = 2,
    TestCmd_Stop = 3,
    TestCmd_Force32 = 0x7FFFFFFF
}TestCmd;

typedef enum
{
    TestType_Player = 0,
    TestType_JpegDecode = 1,
    TestType_Force32 = 0x7FFFFFFF
}TestType;


struct PlayerContext
{
    NvxFramework        m_Framework;
    StatusCode          mState;
    NvxPlayerGraph2    *m_pGraph;
    NvOsSemaphoreHandle hConditionSema;
    char *sPath;
    TestCmd testcmd;
    TestType testtype;
    int playTimeInSecs;
    NvBool bDisplayX, bAlphaBlend;
    NvBool bExtractMetadata, bStreaming, userPause, bVirtualDesktop;
    NvU32 nAlphaValue;
    NvU32 nVirtualDesktop;

    void *winsysDesktop;
    void *winsysWindow;
    EGLNativeWindowType   nativeWindow;
    EGLNativeDisplayType  nativeDisplay;
    EGLDisplay dpy;
    EGLConfig  cfg;
    EGLSurface surf;
    EGLContext ctx;
};

NvError ParseArguments(int argc, char *argv[], struct PlayerContext *pPlayerContext);
int GatherStreamInfo(struct PlayerContext *pPlayerContext);
void GatherMetadata(struct PlayerContext *pPlayerContext);
int ExecuteUserCommands(struct PlayerContext *pPlayerContext);
NvError WinSysInit(struct PlayerContext *pPlayerContext);
NvError InitEglState(struct PlayerContext *pPlayerContext);

//===========================================================================
// InitEglState() - initialize EGL
//===========================================================================
NvError InitEglState(struct PlayerContext *pPlayerContext)
{
    static EGLint contextAttrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLBoolean rv;

    pPlayerContext->dpy = eglGetDisplay(pPlayerContext->nativeDisplay);
    if (pPlayerContext->dpy == EGL_NO_DISPLAY)
    {
        NvOsDebugPrintf("EGL failed to obtain egl display \n");
        return NvError_BadParameter;
    }

    rv = eglInitialize(pPlayerContext->dpy, 0, 0);
    if (rv==0)
    {
        NvOsDebugPrintf("EGL failed to initialize \n");
        return NvError_BadParameter;
    }

    {
#define MAX_MATCHING_CONFIGS 128
        static const EGLint configAttrs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };
        EGLConfig matchingConfigs[MAX_MATCHING_CONFIGS];
        EGLint numMatchingConfigs;
        EGLint i = 0;

        if (!eglChooseConfig(
                pPlayerContext->dpy,
                configAttrs,
                matchingConfigs,
                MAX_MATCHING_CONFIGS,
                &numMatchingConfigs))
        {
            NvOsDebugPrintf("ERROR (omxplayer2): eglChooseConfig failed.\n");
            return NvError_BadParameter;
        }

        // for now, pick first RGBA8888 config we come across.
        for (i = 0; i < numMatchingConfigs; i++)
        {
            EGLint greenSize;
            eglGetConfigAttrib(
                    pPlayerContext->dpy,
                    matchingConfigs[i],
                    EGL_GREEN_SIZE,
                    &greenSize);
            if (greenSize == 8)
                break;
        }
        if (i == numMatchingConfigs)
        {
            NvOsDebugPrintf("ERROR (omxplayer2): no RGBA8888 configs available.\n");
            return NvError_BadParameter;
        }
        pPlayerContext->cfg = matchingConfigs[i];
    }

    pPlayerContext->ctx = eglCreateContext(
                            pPlayerContext->dpy,
                            pPlayerContext->cfg,
                            EGL_NO_CONTEXT,
                            contextAttrs);
    if (pPlayerContext->ctx == EGL_NO_CONTEXT)
    {
        NvOsDebugPrintf("EGL failed to create context\n ");
        return NvError_BadParameter;
    }

    pPlayerContext->surf = eglCreateWindowSurface(
                pPlayerContext->dpy,
                pPlayerContext->cfg,
                pPlayerContext->nativeWindow, 0);
    if (pPlayerContext->surf == EGL_NO_SURFACE)
    {
        NvOsDebugPrintf("EGL failed to create surface \n");
        return NvError_BadParameter;
    }

    rv = eglMakeCurrent( pPlayerContext->dpy, pPlayerContext->surf, pPlayerContext->surf, pPlayerContext->ctx);
    if (rv==0)
    {
        NvOsDebugPrintf("EGL couldn't make context current \n");
        return NvError_BadParameter;
    }

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();

    eglSwapBuffers(pPlayerContext->dpy, pPlayerContext->surf);

    return 0;
}

NvError WinSysInit(struct PlayerContext *pPlayerContext)
{
    NvError err = NvError_Success;
    NvWinSysInterface Interface = NvWinSysInterface_Default;
    NvWinSysDesktopHandle desktop;
    NvWinSysWindowHandle  window;

    if (pPlayerContext->bDisplayX)
    {
        Interface = NvWinSysInterface_X11;
    }

    err = NvWinSysInterfaceSelect(Interface);
    if (err != NvSuccess ) {
        NvOsDebugPrintf("Could not set requested window system interface. \
                         Error 0x%x\n", err);
        return err;
    }

    err = NvWinSysDesktopOpen(NULL, &desktop);
    if (err != NvSuccess ) {
        NvOsDebugPrintf("Could not get desktop handle. \
                         Error 0x%x\n", err);
        return err;
    }

    err = NvWinSysWindowCreate(
                    desktop,
                    "X Window",
                    NULL,
                    NULL,
                    &window);

    if (err != NvSuccess ) {
        NvOsDebugPrintf("Could not create window. "
                            "Error 0x%x\n", err);
        return err;
    }

    pPlayerContext->winsysDesktop = (void*)desktop;
    pPlayerContext->winsysWindow  = (void*)window;
    pPlayerContext->nativeDisplay = NvWinSysDesktopGetNativeHandle(desktop);
    pPlayerContext->nativeWindow  = NvWinSysWindowGetNativeHandle(window);

    NvWinSysPollEvents();
    return err;
}

static const char *StreamTypeToChar(ENvxStreamType eType)
{
    switch (eType)
    {
        case NvxStreamType_MPEG4:
        case NvxStreamType_MPEG4Ext: return "mpeg4";
        case NvxStreamType_H264:
        case NvxStreamType_H264Ext: return "h264";
        case NvxStreamType_H263: return "h263";
        case NvxStreamType_WMV: return "wmv";
        case NvxStreamType_MPEG2V: return "mpeg-2v";
        case NvxStreamType_VP6: return "vp6";
        case NvxStreamType_JPG: return "jpg";
        case NvxStreamType_MP3: return "mp3";
        case NvxStreamType_MP2: return "mp2";
        case NvxStreamType_WAV: return "wav";
        case NvxStreamType_AAC: return "aac";
        case NvxStreamType_AACSBR: return "aac-sbr";
        case NvxStreamType_BSAC: return "bsac";
        case NvxStreamType_WMA: return "wma";
        case NvxStreamType_WMAPro: return "wma-pro";
        case NvxStreamType_WMALossless: return "wma-lossless";
        case NvxStreamType_AMRWB: return "amr-wb";
        case NvxStreamType_AMRNB: return "amr-nb";
        case NvxStreamType_VORBIS: return "vorbis";
        case NvxStreamType_ADPCM: return "adpcm";
        case NvxStreamType_AC3: return "ac3";
        case NvxStreamType_MS_MPG4: return "ms-mpg4";
        case NvxStreamType_QCELP: return "qcelp";
        case NvxStreamType_EVRC: return "evrc";
        default: break;
    }

    return "unknown";
}

int GatherStreamInfo(struct PlayerContext *pPlayerContext)
{
    NvxComponent *ImageDecoder = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE portDef;
    
    if (pPlayerContext->testtype == TestType_JpegDecode)
    {
        ImageDecoder = NvxGraphLookupComponent(pPlayerContext->m_pGraph->graph, "VIDDEC");
        
        if (ImageDecoder)
        {
            portDef.nPortIndex = 1;
            OMX_GetParameter(ImageDecoder->handle, OMX_IndexParamPortDefinition,
                             &portDef);
            NvOsDebugPrintf("Image Dimensions: %dx%d\n", 
                            portDef.format.video.nFrameWidth,
                            portDef.format.video.nFrameHeight);
        }
        else
        {
            NvOsDebugPrintf("Failed to find jpeg decoder component \n");
            return 0;
        }
        
        return 1;
    }
    
    // get duration
    if (pPlayerContext->m_pGraph->durationInMS >= 0)
    {
        NvOsDebugPrintf("File duration is %lld ms\n", pPlayerContext->m_pGraph->durationInMS);
    }

    if (pPlayerContext->m_pGraph->videoType != NvxStreamType_NONE)
    {
        NvOsDebugPrintf("Video stream is %s\n", StreamTypeToChar(pPlayerContext->m_pGraph->videoType));
        NvOsDebugPrintf("Size: %dx%d\n", pPlayerContext->m_pGraph->vidWidth, pPlayerContext->m_pGraph->vidHeight);
        NvOsDebugPrintf("Video Bitrate is %d Hz \n", pPlayerContext->m_pGraph->vidBitrate);
        
    }

    if (pPlayerContext->m_pGraph->audioType != NvxStreamType_NONE)
    {
        NvOsDebugPrintf("Audio stream is %s\n", StreamTypeToChar(pPlayerContext->m_pGraph->audioType));
        NvOsDebugPrintf("Rate is %d Hz \n", pPlayerContext->m_pGraph->audSample);
        NvOsDebugPrintf("No of Channels is %d \n", pPlayerContext->m_pGraph->audChannels);
        NvOsDebugPrintf("Bits per sample is %d \n", pPlayerContext->m_pGraph->audBitsPerSample);
        NvOsDebugPrintf("Audio Bitrate is %d Hz \n", pPlayerContext->m_pGraph->audBitrate);
    }
    
    return 1;
}

void GatherMetadata(struct PlayerContext *pPlayerContext)
{
     NvU8 *pValue = NULL;
     int len = 0, i = 0;
     char *metatypestr[] = {"Title: ", "Artist: ", "Album: ",
                            "Album Artist: ", "Composer: ", "Genre: ",
                            "Year: ", "Comment: ", "Copyright: "};
     ENvxMetadataType metatype[9] = {NvxMetadata_Title, NvxMetadata_Artist, NvxMetadata_Album,
                                   NvxMetadata_AlbumArtist, NvxMetadata_Composer, NvxMetadata_Genre,
                                   NvxMetadata_Year, NvxMetadata_Comment, NvxMetadata_Copyright};
                            
    for (i = 0; i < 9; i++)
    {
         NvxPlayerGraphExtractMetadata(pPlayerContext->m_pGraph, metatype[i], (OMX_U8**)&pValue, &len);
         if (pValue)
         {
             NvOsDebugPrintf("%s %s\n", metatypestr[i], pValue);
             NvOsFree(pValue);
             pValue = NULL;
         }
    }
}

int ExecuteUserCommands(struct PlayerContext *pPlayerContext)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    static int seekTimeOffsetMsec = 0;
    
    switch (pPlayerContext->testcmd)
    {
        case TestCmd_Pause:
        {
            if ((pPlayerContext->m_pGraph->graph->state == OMX_StateExecuting ||
                pPlayerContext->m_pGraph->graph->state == OMX_StateIdle))
            {
                NvOsDebugPrintf("Pausing the graph for %d secs \n", pPlayerContext->playTimeInSecs);
                pPlayerContext->userPause = NV_TRUE;
                eError = NvxPlayerGraphPause(pPlayerContext->m_pGraph, OMX_TRUE);
            }
            else if (pPlayerContext->m_pGraph->graph->state == OMX_StatePause)
            {
                NvOsDebugPrintf("Resuming playback of graph for %d secs \n", pPlayerContext->playTimeInSecs);
                pPlayerContext->userPause = NV_FALSE;
                eError = NvxPlayerGraphPause(pPlayerContext->m_pGraph, OMX_FALSE);
            }
            
            if (eError != OMX_ErrorNone)
            {
                NvOsDebugPrintf("NvxPlayerGraph Pause Failed! NvError = 0x%08x\n", eError);
                return 0;
            }
            
            return 1;
        }
       case TestCmd_Stop:
       {
            NvOsDebugPrintf("Stop playback \n");
            eError = NvxPlayerGraphStop(pPlayerContext->m_pGraph);
            
            if (eError != OMX_ErrorNone)
            {
                NvOsDebugPrintf("NvxPlayerGraph Stop Failed! NvError = 0x%08x\n", eError);
            }
            
            return 0;
       }
       case TestCmd_ComplexSeek:
       {
            static int cnt = 0;

            cnt++;
            if(cnt == 7)
            {
                NvOsDebugPrintf("- Seeking playback to 0 sec\n");
                eError = NvxPlayerGraphSeek(pPlayerContext->m_pGraph, &seekTimeOffsetMsec);
                if (eError != OMX_ErrorNone)
                {
                    NvOsDebugPrintf("Seek failed! NvError = 0x%08x\n", eError);
                    return 0;
                    
                }
                NvOsDebugPrintf("- Resuming playback at 0 sec\n");
                cnt = 10;
            }
            else if(cnt % 7 == 0)
            {
                NvOsDebugPrintf("- Seeking playback forward by %d sec\n", 20);
                seekTimeOffsetMsec+=20000;
                eError = NvxPlayerGraphSeek(pPlayerContext->m_pGraph, &seekTimeOffsetMsec);
                if (eError != OMX_ErrorNone)
                {
                    NvOsDebugPrintf("Seek failed! NvError = 0x%08x\n", eError);
                    return 0;
                    
                }
                NvOsDebugPrintf("- Resuming playback at %d sec\n", (seekTimeOffsetMsec / 1000));
            }
            else if(cnt % 10 == 0)
            {
                NvOsDebugPrintf("- Seeking playback backward by %d sec\n", 5);
                seekTimeOffsetMsec-=5000;
                eError = NvxPlayerGraphSeek(pPlayerContext->m_pGraph, &seekTimeOffsetMsec);
                if (eError != OMX_ErrorNone)
                {
                    NvOsDebugPrintf("Seek failed! NvError = 0x%08x\n", eError);
                    return 0;
                    
                }
                NvOsDebugPrintf("- Resuming playback at %d sec \n", (seekTimeOffsetMsec / 1000));
            }
            return 1;
       }
       default: 
       {
           return 1;
       }
    }

    return 1;
}

NvError ParseArguments(int argc, char *argv[], struct PlayerContext *pPlayerContext)
{
    int i = 0;
    char strRTSPFileStart[8] = "rtsp://";
    char strHTTPFileStart[8] = "http://";
    char startstr[8] ="";
    
    NvOsDebugPrintf("Parsing arguments \n");

    for ( i=1; i< argc; i++)
    {    
        if ((!NvOsStrcmp(argv[i], "--input")  ||
            (!NvOsStrcmp(argv[i], "-i"))))
         {
            pPlayerContext->sPath = argv[++i];
            NvOsStrncpy(startstr, pPlayerContext->sPath, 7);
            if (!NvOsStrcmp(startstr, strHTTPFileStart) ||
                !NvOsStrcmp(startstr, strRTSPFileStart))
            {
                pPlayerContext->bStreaming = NV_TRUE;
            }
         }
         else if (!NvOsStrcmp(argv[i], "--pausetest"))
         {
             pPlayerContext->testcmd = TestCmd_Pause;
         }
         else if (!NvOsStrcmp(argv[i], "--complexseektest"))
         {
             pPlayerContext->testcmd = TestCmd_ComplexSeek;
             pPlayerContext->playTimeInSecs = 1;
         }
         else if (!NvOsStrcmp(argv[i], "--stoptest"))
         {
             pPlayerContext->testcmd = TestCmd_Stop;
         }
         else if (!NvOsStrcmp(argv[i], "--playtimeupdate"))
         {
                pPlayerContext->playTimeInSecs = atoi(argv[++i]);
         }
         else if (!NvOsStrcmp(argv[i], "--metadata"))
         {
                pPlayerContext->bExtractMetadata = NV_TRUE;
         }
         else if (!NvOsStrcmp(argv[i], "--jpegdecode"))
         {
             pPlayerContext->testtype = TestType_JpegDecode;
         }
         else if (!NvOsStrcmp(argv[i], "--X11"))
         {
             pPlayerContext->bDisplayX = NV_TRUE;
         }
         else if (!NvOsStrcmp(argv[i], "--VD"))
         {
             pPlayerContext->bVirtualDesktop = NV_TRUE;
             pPlayerContext->nVirtualDesktop = atoi(argv[++i]);
         }
         else if (!NvOsStrcmp(argv[i], "--alphablend"))
         {
             pPlayerContext->bAlphaBlend = NV_TRUE;
             pPlayerContext->nAlphaValue = atoi(argv[++i]);
         }
         else
         {
             NvOsDebugPrintf("unknown argument \n");
             return NvError_BadParameter;
         }
    }
    
    return NvError_Success;
}

int main(int argc, char **argv)
{
    NvError status = NvError_Success;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BOOL PauseForBuffering = NV_FALSE;
    OMX_U32 curBufferingPercent = 0;
    NvxComponent *VideoRenderer = NULL;
    OMX_INDEXTYPE eIndex;
    NVX_CONFIG_WINDOWOVERRIDE oWinOverride;
    NVX_CONFIG_VIRTUALDESKTOP oVirtualDesktop;

    // Streaming buffering event is generated every second, to test user commands in streaming use-cases we need this counter.
    int cntEventTimeOut = 0; 
    
    // allocate the player context
    struct PlayerContext *pPlayerContext = NvOsAlloc(sizeof(struct PlayerContext));
    if (pPlayerContext == NULL)
    {
        NvOsDebugPrintf("Unable to allocate memory for player context structures");
        goto exit;
    }
    
    NvOsMemset(pPlayerContext, 0, sizeof(struct PlayerContext));
    pPlayerContext->playTimeInSecs = 10;
    
    // parse command -line arguments
    if (argc > 1)
    {
        status = ParseArguments(argc++, argv++, pPlayerContext);
        if (status != NvError_Success)
        {
            NvOsDebugPrintf("Incorrect arguments \n");
            goto exit;
        }
    }
    else
    {
        NvOsDebugPrintf("No arguments for playback !!\n");
        return 0;
    }

    status = NvOsSemaphoreCreate(&pPlayerContext->hConditionSema, 0);
    if (status != NvSuccess)
    {
        NvOsDebugPrintf("Seamphore creation for ConditionSema failed");
        goto exit;
    }
    
    // Initialize OMX
    if (OMX_ErrorNone != NvxFrameworkInit(&pPlayerContext->m_Framework))
    {
        pPlayerContext->mState = StatusCode_StateError;
        NvOsDebugPrintf("Unable to intialize OMX \n");
        goto exit;
    }
    
    pPlayerContext->mState = StatusCode_NoError;

    status = WinSysInit(pPlayerContext);
    if (status)
    {
        NvOsDebugPrintf("Window initialization failed \n");
        goto exit;
    }

    status = InitEglState(pPlayerContext);
    if (status)
    {
        NvOsDebugPrintf("EGL initialization failed \n");
        goto exit;
    }

    eError = NvxPlayerGraphInit(pPlayerContext->m_Framework, &pPlayerContext->m_pGraph, 
                                NvxPlayerGraphType_Normal, (OMX_U8 *)pPlayerContext->sPath, NULL);
    NvOsDebugPrintf("Creating player for: %s\n", pPlayerContext->sPath);
    
    if (eError != OMX_ErrorNone)
    {
        NvOsDebugPrintf("Player for: %s failed \n", pPlayerContext->sPath);
        pPlayerContext->mState = StatusCode_StateError;
        goto exit;
    }
    
    if (pPlayerContext->testtype == TestType_JpegDecode)
    {
        pPlayerContext->m_pGraph->type = NvxPlayerGraphType_JpegDecode;
    }
    
    eError = NvxPlayerGraphCreate(pPlayerContext->m_pGraph);
    if (eError != OMX_ErrorNone)
    {
        NvOsDebugPrintf("Graph creation failed \n");
        pPlayerContext->mState = StatusCode_StateError;
        goto exit;
    }
    
    // prints stream related info
    if (pPlayerContext->testtype == TestType_Player)
    {
        if (!GatherStreamInfo(pPlayerContext))
        {
            NvOsDebugPrintf("Failed to gather stream info \n");
            pPlayerContext->mState = StatusCode_StateError;
            goto exit;
        }
    }

    // Setting up full-screen by default
    VideoRenderer = NvxGraphLookupComponent(pPlayerContext->m_pGraph->graph, "VIDREND");
    if (VideoRenderer)
    {
        INIT_PARAM(oWinOverride);
        oWinOverride.eWindow = NvxWindow_A;
        oWinOverride.eAction = NvxWindowAction_TurnOff;

        if (OMX_ErrorNone == OMX_GetExtensionIndex(VideoRenderer->handle,
            NVX_INDEX_CONFIG_WINDOW_DISP_OVERRIDE, &eIndex))
        {
            eError = OMX_SetConfig(VideoRenderer->handle, eIndex,
                     &oWinOverride);
            if (OMX_ErrorNone != eError)
            {
                NvOsDebugPrintf("Error: Incorrect window specified. Use NvxWindow_A only.\n");
                pPlayerContext->mState = StatusCode_StateError;
                goto exit;
            }
        }

        if (pPlayerContext->bVirtualDesktop)
        {
            INIT_PARAM(oVirtualDesktop);
            oVirtualDesktop.nDesktopNumber = pPlayerContext->nVirtualDesktop;

            if (OMX_ErrorNone == OMX_GetExtensionIndex(VideoRenderer->handle,
                NVX_INDEX_CONFIG_VIRTUALDESKTOP, &eIndex))
            {
                eError = OMX_SetConfig(VideoRenderer->handle, eIndex,
                         &oVirtualDesktop);
                if (OMX_ErrorNone != eError)
                {
                    NvOsDebugPrintf("Error: Valid values for VD are 0-2 \n\n");
                    pPlayerContext->mState = StatusCode_StateError;
                    goto exit;
                }
            }
        }

        if (pPlayerContext->bAlphaBlend)
        {
            OMX_CONFIG_COLORBLENDTYPE oColorBlend;

            INIT_PARAM(oColorBlend);
            oColorBlend.nRGBAlphaConstant = (OMX_U32)(pPlayerContext->nAlphaValue << 24);
            oColorBlend.eColorBlend = OMX_ColorBlendAlphaConstant;
            OMX_SetConfig( VideoRenderer->handle, OMX_IndexConfigCommonColorBlend, &oColorBlend);
        }
    }

    if (pPlayerContext->bExtractMetadata)
    {
        GatherMetadata(pPlayerContext);
    }
    
    eError = NvxPlayerGraphToIdle(pPlayerContext->m_pGraph);
    if (OMX_ErrorNone != eError)
    {
        NvOsDebugPrintf("Hit error when transitioning from loaded to idle\n");
        pPlayerContext->mState = StatusCode_StateError;
        goto exit;
    }
    
    pPlayerContext->mState = StatusCode_StateOpen;
    
    // start the playback
    if (pPlayerContext->m_pGraph->graph->state == OMX_StateIdle)
    {
        NvxPlayerGraphStartPlayback(pPlayerContext->m_pGraph);
    }
    
    while (1)
    {
        // wait for event or timeout
       if (!pPlayerContext->bStreaming)
       {
           NvOsDebugPrintf("Wait for %d secs \n", pPlayerContext->playTimeInSecs);
       }
       
       NvWinSysPollEvents();
       NvxGraphWaitForEndOfStream(pPlayerContext->m_pGraph->graph, pPlayerContext->playTimeInSecs * 1000);

        if (NvxGraphIsAtEndOfStream(pPlayerContext->m_pGraph->graph))
        {
            if (pPlayerContext->testtype == TestType_JpegDecode)
            {
                 GatherStreamInfo(pPlayerContext);
                 NvOsDebugPrintf("Jpeg Image Display for %d secs \n", pPlayerContext->playTimeInSecs);
                 NvOsSleepMS(pPlayerContext->playTimeInSecs * 1000);
            }
            
            NvOsDebugPrintf("Playback complete \n");
            NvxPlayerGraphPause(pPlayerContext->m_pGraph, OMX_TRUE);
            break;
        }
        else if (NvxPlayerGraphHasBufferingEvent(pPlayerContext->m_pGraph, &PauseForBuffering,
                                                    &curBufferingPercent))
        {
            cntEventTimeOut++;
            
            if (cntEventTimeOut == pPlayerContext->playTimeInSecs)
            {
                if (!ExecuteUserCommands(pPlayerContext))
                {
                    break;
                }

                cntEventTimeOut = 0;
            }
            
            if ((pPlayerContext->m_pGraph->graph->state == OMX_StateExecuting ||
                    pPlayerContext->m_pGraph->graph->state == OMX_StateIdle))
            {
                if ((PauseForBuffering) && (!pPlayerContext->userPause))
                {
                    NvOsDebugPrintf("Pause for buffering \n");
                    NvxPlayerGraphPause(pPlayerContext->m_pGraph, OMX_TRUE);
                }
                else if (curBufferingPercent) // can be 0 for stream-size we dont know, for example live radio station streaming
                {
                    NvOsDebugPrintf("buffered %d %% ...\n", curBufferingPercent);
                }
            }
            else if (pPlayerContext->m_pGraph->graph->state == OMX_StatePause)
            {
                if ((PauseForBuffering == OMX_FALSE) && (!pPlayerContext->userPause))
                {
                    NvOsDebugPrintf("Unpause for playing \n");
                    NvxPlayerGraphPause(pPlayerContext->m_pGraph, OMX_FALSE);
                }
                else if (curBufferingPercent) // can be 0 for stream-size we dont know, for example live radio station streaming
                {
                    NvOsDebugPrintf("buffering %d %% ...\n", curBufferingPercent);
                }
            }
        }
        else if (NvxGraphGetError(pPlayerContext->m_pGraph->graph) != OMX_ErrorNone) 
        {
            NvOsDebugPrintf("Error while playing back \n");
            break;
        }
        else // time-out 
        {
            if(!ExecuteUserCommands(pPlayerContext))
            {
                break;
            }
            
            continue;
        }
    }

exit:
        if (pPlayerContext->m_pGraph)
        {
            NvxPlayerGraphTeardown(pPlayerContext->m_pGraph);
            NvxPlayerGraphDeinit(pPlayerContext->m_pGraph);
            pPlayerContext->m_pGraph = NULL;
        }

        NvxFrameworkDeinit(&pPlayerContext->m_Framework);

        NvOsSemaphoreDestroy(pPlayerContext->hConditionSema);

        if (pPlayerContext->dpy)
            eglTerminate(pPlayerContext->dpy);
        eglReleaseThread();

        if (pPlayerContext->winsysDesktop)
            NvWinSysWindowDestroy(pPlayerContext->winsysDesktop);
        if (pPlayerContext->winsysWindow)
            NvWinSysDesktopClose(pPlayerContext->winsysWindow);

        pPlayerContext->winsysWindow = NULL;
        pPlayerContext->winsysDesktop = NULL;

        NvOsFree(pPlayerContext);
        return(0);
}


