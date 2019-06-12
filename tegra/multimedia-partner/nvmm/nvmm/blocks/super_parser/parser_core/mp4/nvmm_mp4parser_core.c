/*
* Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#include "nvmm_debug.h"
#include "nvmm_util.h"
#include "nvmm_ulp_util.h"
#include "nvmm_videodec.h"
#include "nvmm_mp4parser_core.h"
#include "nv_mp4parser.h"
#include "nv_mp4parser_defines.h"
#include "nvmm_aac.h"
#include "nvmm_ulp_kpi_logger.h"
#include "nvmm_metadata.h"
#include "nvmm_contentpipe.h"
#include "nvmm_parser.h"

#define NUM_CHANNELS_SUPPORTED      6



#define AAC_DEC_MAX_INPUT_BUFFER_SIZE       (768 * NUM_CHANNELS_SUPPORTED)
#define AAC_DEC_MIN_INPUT_BUFFER_SIZE       (768 * NUM_CHANNELS_SUPPORTED)
#define BSAC_DEC_MAX_INPUT_BUFFER_SIZE       2048
#define BSAC_DEC_MIN_INPUT_BUFFER_SIZE       2048
#define AMR_DEC_MAX_INPUT_BUFFER_SIZE       1536
#define AMR_DEC_MIN_INPUT_BUFFER_SIZE       1536

#define ENABLE_KPI_LOG_FOR_NORMAL_MODE 0

#define NVMM_MP4PARSER_MAX_TRACKS      10     /* Video and audio track */
#define MAX_VIDEO_BUFFERS 10

typedef enum NvMMMp4ParseStateRec
{
    MP4PARSE_INIT,
    MP4PARSE_HEADER,
    MP4PARSE_DATA,
    MP4PARSE_EOS
} NvMMMp4ParseState;


/* Context for mp4 Parser (reader) core */
typedef struct NvMMMp4ParserCoreContextRec
{
    NvMp4Parser Parser;
    NvMMAttrib_ParseRate rate;
    NvMMAttrib_ParsePosition Position;
    NvMMMp4ParseState ParserState[NVMM_MP4PARSER_MAX_TRACKS];
    NvU32 AudioFPS[NVMM_MP4PARSER_MAX_TRACKS];
    NvU32 VideoFPS;
    NvU64 VideoDuration;
    NvU64 AudioDuration;
    NvU64 TotalDuration;
    NvU32 Width; // specifies vertical size of pixel aspect ratio
    NvU32 Height;// specifies horizontal size of pixel aspect ratio
    //To store Parser Header memory handle for Audio
    NvRmMemHandle hAudioHeaderMemHandle;
    // To store the parser header physical address
    NvRmPhysAddr pAudioHeaderAddress;
    // To store parser header virtual address
    NvU32 vAudioHeaderAddress;
    //To store Parser Header memory handle for Video
    NvRmMemHandle hVideoHeaderMemHandle;
    // To store the parser header physical address
    NvRmPhysAddr pVideoHeaderAddress;
    // To store parser header virtual address
    NvU32 vVideoHeaderAddress;
    // video header size
    NvU32 VideoHeaderSize;
    // Max Video Frame size
    NvU32 MaxVidFrameSize;
    // Audio Bitrate
    NvU32 AudioBitrate;
    // Indicates file reached to EOF while seeking
    NvBool IsFileEOF;
    NvU64 CurrentTimeStamp;
    NvS64 PerFrameTime;
    NvOsLibraryHandle DrmLibHandle;
    NvU32 NvDrmUpdateMeteringInfo;
    NvU32 NvDrmDestroyContext;
    NvU32 NvDrmContext;
    NvU32 MeteringTime;
    NvU32 LicenseConsumeTime;
    NvU32 MeteringPolicyType;
    NvU32 CoreContext;
} NvMMMp4ParserCoreContext;

static NvError
Mp4CoreParserGetNextRMRAStream (NvMMMp4ParserCoreContext *pContext, NvU32 Index);

static NvError
Mp4CoreParserDrmCommit (
                        NvMMParserCoreHandle hParserCore,
                        NvU32 DrmCore)
{
    NvError Status = NvSuccess;
    NvMMMp4ParserCoreContext *pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;

    // and commit right now.
    if (pContext->MeteringPolicyType != 3)
    {
        if (pContext->Parser.DrmInterface.pNvDrmUpdateMeteringInfo != NULL)
        {
            Status = pContext->Parser.DrmInterface.pNvDrmUpdateMeteringInfo ( (NvDrmContextHandle) DrmCore) ;
            if (NvError_Success != Status)
            {
                hParserCore->domainName = NvMMErrorDomain_DRM;
                return Status;
            }
        }
    }

cleanup:
    return Status;
}

static NvError
Mp4CoreParserGetNumberOfStreams (
                                 NvMMParserCoreHandle hParserCore,
                                 NvU32 *pStreamCount)
{
    NvError Status = NvSuccess;
    NvMMMp4ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pStreamCount);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;

    *pStreamCount = NvMp4ParserGetNumTracks (&pContext->Parser);

cleanup:
    return Status;
}

static NvBool
Mp4CoreParserGetBufferRequirements (
                                    NvMMParserCoreHandle hParserCore,
                                    NvU32 StreamIndex,
                                    NvU32 Retry,
                                    NvMMNewBufferRequirementsInfo *pBufReq)
{
    NvMMMp4ParserCoreContext *pContext = NULL;
    NvMp4TrackTypes TrackType;
    NvU32 NoOfStreams = 0;
    NvError Status = NvSuccess;
    NvBool Video_Flag = NV_FALSE;
    NvF32 BufferedDuration = 0.f;
    NvU32 i;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pBufReq);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;
    NVMM_CHK_ARG (Retry == 0);

    NvOsMemset (pBufReq, 0, sizeof (NvMMNewBufferRequirementsInfo));

    pBufReq->structSize    = sizeof (NvMMNewBufferRequirementsInfo);
    pBufReq->event         = NvMMEvent_NewBufferRequirements;
    pBufReq->minBuffers    = 4;
    pBufReq->maxBuffers    = NVMMSTREAM_MAXBUFFERS;
    pBufReq->memorySpace   = NvMMMemoryType_SYSTEM;
    pBufReq->byteAlignment = 4;

    NVMM_CHK_ERR(Mp4CoreParserGetNumberOfStreams (hParserCore, &NoOfStreams));

    // Calculate the BufferedDuration of video for given i/p buffers, buffersize and video bitrate
    if (!Video_Flag)
    {
        for (i = 0; i < NoOfStreams; i++)
        {
            TrackType = (NvMp4TrackTypes) NvMp4ParserGetTracksInfo (&pContext->Parser, i);
            switch (TrackType)
            {
            case NvMp4TrackTypes_MPEG4:
            case NvMp4TrackTypes_S263:
            case NvMp4TrackTypes_AVC:
            case NvMp4TrackTypes_MJPEGA:
            case NvMp4TrackTypes_MJPEGB:
                // Calculated Buffered duration of file using fps field and number of frames sent to decoder
                if (pContext->VideoFPS && (i == pContext->Parser.VideoIndex))
                {
                    Video_Flag = NV_TRUE;
                    BufferedDuration = MAX_VIDEO_BUFFERS / ( (NvF32) pContext->VideoFPS / FLOAT_MULTIPLE_FACTOR);
                }
                break;
            default:
                break;
            }
            if (Video_Flag)
                break;
        }
    }

    TrackType = (NvMp4TrackTypes) NvMp4ParserGetTracksInfo (&pContext->Parser, StreamIndex);

    switch (TrackType)
    {
    case NvMp4TrackTypes_AAC:
    case NvMp4TrackTypes_AACSBR:
        if (hParserCore->bEnableUlpMode == NV_TRUE)
        {
            pBufReq->minBufferSize = sizeof (NvMMOffsetList) + (sizeof (NvMMOffset) * MAX_OFFSETS);
            pBufReq->maxBufferSize = sizeof (NvMMOffsetList) + (sizeof (NvMMOffset) * MAX_OFFSETS);
            pBufReq->minBuffers    = 6;
            pBufReq->maxBuffers    = 6;
        }
        else
        {
            pBufReq->minBufferSize = AAC_DEC_MIN_INPUT_BUFFER_SIZE;
            pBufReq->maxBufferSize = AAC_DEC_MAX_INPUT_BUFFER_SIZE;
        }
        // Calculate the number of audiobuffers required in given BufferedDuration.
        if (Video_Flag)
        {
            pBufReq->minBuffers = (NvU32) (BufferedDuration * pContext->AudioFPS[StreamIndex]);

            // FIXME: Minbuffers should n't be less than 2(1 at parser and 1 at decoder)
            if (pBufReq->minBuffers < 5)
                pBufReq->minBuffers = 5;
            else if (pBufReq->minBuffers > 20)
                pBufReq->minBuffers = 20;

            pBufReq->maxBuffers = pBufReq->minBuffers;
        }
        break;

    case NvMp4TrackTypes_BSAC:
        pBufReq->minBufferSize = BSAC_DEC_MIN_INPUT_BUFFER_SIZE;
        pBufReq->maxBufferSize = BSAC_DEC_MAX_INPUT_BUFFER_SIZE;

        // Calculate the number of audiobuffers required in given BufferedDuration.
        if (Video_Flag)
        {
            pBufReq->minBuffers = (NvU32) (BufferedDuration * pContext->AudioFPS[StreamIndex]);
            // FIXME: Minbuffers should n't be less than 2(1 at parser and 1 at decoder)
            if (pBufReq->minBuffers < 5)
                pBufReq->minBuffers = 5;
            else if (pBufReq->minBuffers > 32)
                pBufReq->minBuffers = 32;

            pBufReq->maxBuffers = pBufReq->minBuffers;
        }
        break;
    case NvMp4TrackTypes_QCELP:
    case NvMp4TrackTypes_EVRC:
        pBufReq->minBufferSize = BSAC_DEC_MIN_INPUT_BUFFER_SIZE; // should be changed later based on decoder
        pBufReq->maxBufferSize = BSAC_DEC_MAX_INPUT_BUFFER_SIZE;
        break;
    case NvMp4TrackTypes_NAMR:
    case NvMp4TrackTypes_WAMR:
        pBufReq->minBufferSize = AMR_DEC_MIN_INPUT_BUFFER_SIZE;
        pBufReq->maxBufferSize = AMR_DEC_MAX_INPUT_BUFFER_SIZE;
        // Calculate the number of audiobuffers required in given BufferedDuration.
        if (Video_Flag)
        {
            pBufReq->minBuffers = (NvU32) (BufferedDuration * pContext->AudioFPS[StreamIndex]);
            // FIXME: Minbuffers should n't be less than 2(1 at parser and 1 at decoder)
            if (pBufReq->minBuffers < 5)
                pBufReq->minBuffers = 5;
            else if (pBufReq->minBuffers > 32)
                pBufReq->minBuffers = 32;

            pBufReq->maxBuffers = pBufReq->minBuffers;
        }
        break;

    case NvMp4TrackTypes_MPEG4:
    case NvMp4TrackTypes_S263:
    case NvMp4TrackTypes_AVC:
    case NvMp4TrackTypes_MJPEGA:
    case NvMp4TrackTypes_MJPEGB:
        // reduced memory mode
        if (hParserCore->bReduceVideoBuffers)
        {
            pBufReq->minBuffers = MAX_VIDEO_BUFFERS;
            pBufReq->maxBuffers = MAX_VIDEO_BUFFERS;
        }
        if (hParserCore->bEnableUlpMode == NV_TRUE)
        {
            pBufReq->minBufferSize = sizeof (NvMMOffsetList) + (sizeof (NvMMOffset) * MAX_OFFSETS);
            pBufReq->maxBufferSize = sizeof (NvMMOffsetList) + (sizeof (NvMMOffset) * MAX_OFFSETS);
            pBufReq->minBuffers = 6;
            pBufReq->maxBuffers = 6;
        }
        else
        {
            if ( (pContext->Width == 0) || (pContext->Height == 0))
            {
                pBufReq->minBufferSize = 32768;
                pBufReq->maxBufferSize = 512000 * 2 - 32768;
            }
            else
            {
                if ( (pContext->Width > 320) && (pContext->Height > 240))
                {
                    pBufReq->minBufferSize = (pContext->Width * pContext->Height * 3) >> 2;
                    pBufReq->maxBufferSize = (pContext->Width * pContext->Height * 3) >> 2;
                }
                else
                {
                    pBufReq->minBufferSize = (pContext->Width * pContext->Height * 3) >> 1;
                    pBufReq->maxBufferSize = (pContext->Width * pContext->Height * 3) >> 1;
                }
            }
            if(pBufReq->maxBufferSize < 1024)
            {
                pBufReq->maxBufferSize = 1024;
                pBufReq->minBufferSize = 1024;
            }
            pContext->MaxVidFrameSize = pBufReq->maxBufferSize;
        }
        break;
    default:
        break;
    }

cleanup:
    return (Status != NvSuccess) ? NV_FALSE : NV_TRUE;
}

static NvError
Mp4AllocateHeapMemory (
                       NvRmDeviceHandle hRmDevice,
                       NvRmMemHandle *hMemHandle,
                       NvRmPhysAddr *pAddress,
                       NvU32 *vAddress,
                       NvU32 AllocSize)
{
    NvError Status = NvSuccess;
    const NvU32 ALIGN = 16;
    void *pvirtualBuf = NULL;

    // Get memory handle(hMemHandle) for the requested size
    NVMM_CHK_ERR (NvRmMemHandleAlloc(
        hRmDevice,
        NULL,
        0,
        ALIGN,
        NvOsMemAttribute_Uncached,
        AllocSize,
        0,
        0,
        hMemHandle));

    /* Get the physical address */
    *pAddress = NvRmMemPin (*hMemHandle);
    NVMM_CHK_ERR (NvRmMemMap (
        *hMemHandle,
        0,
        AllocSize,
        NVOS_MEM_READ_WRITE,
        (void **) &pvirtualBuf));

    *vAddress = (NvU32) pvirtualBuf;

cleanup:
    if (Status != NvSuccess)
    {
        NvRmMemHandleFree (*hMemHandle);
    }

    return Status;
}


static NvError
Mp4FreeHeapMemory (
                   NvRmMemHandle *hMemHandle,
                   NvRmPhysAddr *pAddress,
                   NvU32 *vAddress,
                   NvU32 AllocSize)
{
    // Can accept the null parameter. If it is not null then only destroy.
    if (NULL != *hMemHandle)
    {
        //Unmap the virtual memory
        NvRmMemUnmap (*hMemHandle, (void*) *vAddress, AllocSize);
        // Unpin the memory allocation.
        NvRmMemUnpin (*hMemHandle);
        // Free the memory handle.
        NvRmMemHandleFree (*hMemHandle);
    }

    return NvSuccess;
}

static NvError
Mp4CoreParserClose (
                    NvMMParserCoreHandle hParserCore)
{
    NvError Status = NvSuccess;
    NvU64 Time1 =  0;
    NvF64 Time2 =  0;
    NvU32 Count = 0;
    NvU32 i;
    NvMMMp4ParserCoreContext *pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;

    if (hParserCore->bEnableUlpMode == NV_TRUE)
    {
        if (pContext->hAudioHeaderMemHandle)
        {
            Status = Mp4FreeHeapMemory (&pContext->hAudioHeaderMemHandle,
                &pContext->pAudioHeaderAddress,
                &pContext->vAudioHeaderAddress,
                sizeof (NvMMAudioAacTrackInfo));
            pContext->hAudioHeaderMemHandle = NULL;
        }

        if (pContext->hVideoHeaderMemHandle)
        {
            Status = Mp4FreeHeapMemory (&pContext->hVideoHeaderMemHandle,
                &pContext->pVideoHeaderAddress,
                &pContext->vVideoHeaderAddress,
                pContext->VideoHeaderSize);
            pContext->hVideoHeaderMemHandle = NULL;
        }

    }

    if (hParserCore->UlpKpiMode)
    {
        // Parser is being closed, so set Idle End time.
        NvmmUlpKpiSetIdleEndTime (NvOsGetTimeUS() * US_100NS);

        // Calculate the KPIs and print them on console.
        NvmmUlpKpiGetParseTimeIdleTimeRatio (&Time2);
        NvmmUlpKpiGetAverageTimeBwReadRequests (&Time1);
        NvmmUlpKpiGetTotalReadRequests (&Count);
        NvmmUlpKpiPrintAllKpis();

        // Call KPI logger Deinit here.
        NvmmUlpKpiLoggerDeInit();
    }

    NvMp4ParserDeInit (&pContext->Parser);

    for (i = 0 ; i < pContext->Parser.URICount; i++)
    {
        if (pContext->Parser.URIList[i])
        {
            NvOsFree (pContext->Parser.URIList[i]);
            pContext->Parser.URIList[i] = NULL;
        }
    }
    if (pContext->Parser.EmbeddedURL)
    {
        NvOsFree (pContext->Parser.EmbeddedURL);
        pContext->Parser.EmbeddedURL = NULL;
    }
    NvOsFree (hParserCore->pContext);
    hParserCore->pContext = NULL;

cleanup:
    return Status;
}

static NvError
Mp4CoreParserOpen (
                   NvMMParserCoreHandle hParserCore,
                   NvString szURI)
{
    NvError Status = NvSuccess;
    NvU64 StreamDuration = 0;
    NvU64 MediaTimeScale = 0;
    NvU64 TotalTrackTimeNs = 0;
    NvMMMp4ParserCoreContext *pContext = NULL;

    NVMM_CHK_ARG (hParserCore && szURI);
    pContext = NvOsAlloc (sizeof (NvMMMp4ParserCoreContext));
    NVMM_CHK_MEM (pContext);
    hParserCore->pContext = (void*) pContext;
    NvOsMemset (pContext, 0, sizeof (NvMMMp4ParserCoreContext));
    pContext->Parser.URIList[0] = NvOsAlloc (NvOsStrlen (szURI) + 1);
    NVMM_CHK_MEM (pContext->Parser.URIList[0]);
    NvOsStrncpy (pContext->Parser.URIList[0], szURI, NvOsStrlen (szURI) + 1);
    pContext->Parser.URICount = 1;

    if ( (hParserCore->MinCacheSize == 0) &&
        (hParserCore->MaxCacheSize == 0) &&
        (hParserCore->SpareCacheSize == 0))
    {
        hParserCore->bUsingCachedCP = NV_FALSE;
        hParserCore->UlpKpiMode     = NV_FALSE;
        hParserCore->bEnableUlpMode = NV_FALSE;
    }
    else
    {
        hParserCore->bUsingCachedCP = NV_TRUE;
    }

    NVMM_CHK_ERR (NvMp4ParserInit (&pContext->Parser));

    if (hParserCore->bUsingCachedCP)
    {
        NVMM_CHK_ERR (pContext->Parser.pPipe->InitializeCP (pContext->Parser.hContentPipe,
            hParserCore->MinCacheSize,
            hParserCore->MaxCacheSize,
            hParserCore->SpareCacheSize,
            (CPuint64 *) &hParserCore->pActualCacheSize));
    }

    if (hParserCore->bEnableUlpMode == NV_TRUE)
    {
        if (hParserCore->UlpKpiMode == KpiMode_Normal || hParserCore->UlpKpiMode == KpiMode_Tuning)
        {
            NvmmUlpKpiLoggerInit();
            NvmmUlpSetKpiMode (hParserCore->UlpKpiMode);
            NvmmUlpUpdateKpis (KpiFlag_IdleStart, NV_FALSE, NV_FALSE, 0);
        }
        else
        {
            hParserCore->UlpKpiMode = 0;
        }
    }
    else
    {
        // Kpi Mode is not enabled for non-ULP mode.
#if ENABLE_KPI_LOG_FOR_NORMAL_MODE
        hParserCore->UlpKpiMode = KpiMode_Normal;
        NvmmUlpKpiLoggerInit();
        NvmmUlpSetKpiMode (hParserCore->UlpKpiMode);
        NvmmUlpUpdateKpis (KpiFlag_IdleStart, NV_FALSE, NV_FALSE, 0);
#else
        hParserCore->UlpKpiMode = 0;
#endif
    }
    if (hParserCore->UlpKpiMode)
    {
        NvmmUlpUpdateKpis ( (KpiFlag_IdleEnd | KpiFlag_ParseStart), NV_FALSE, NV_FALSE, 0);
    }
    NVMM_CHK_ERR (NvMp4ParserParse (&pContext->Parser));
    pContext->MeteringPolicyType = 0;
    pContext->MeteringTime = 0;
    pContext->LicenseConsumeTime = 0;
    pContext->Parser.pDrmContext = 0;
    pContext->CoreContext = 0;
    /*For Drmed file*/
    if (pContext->Parser.IsSINF)
    {
        char DrmLibName[256];
        NvU32 MeteringTime = 0;
        NvU32 PolicyType = 0;
        NvU32 LicenseConsumeTime = 0;
        NvU64 TotalTrackTime = 0;
        NvDrmBindInfo bContent;
        NvU8 *Atom = NULL;
        NvU32 AtomSize = 0;
        NvDrmContextHandle* pDrmContext  = &pContext->Parser.pDrmContext;

        Status =  NvOsGetConfigString ("MarlinDrmlib", DrmLibName, 256);
        if (Status == NvSuccess)
        {
            if (pContext->DrmLibHandle == NULL)
            {
                if (NvSuccess == NvOsLibraryLoad (DrmLibName, &pContext->DrmLibHandle))
                {
                    pContext->Parser.DrmInterface.pNvDrmCreateContext = (NvError (*) (NvDrmContextHandle *))
                        NvOsLibraryGetSymbol (pContext->DrmLibHandle, "NvDrmCreateContext");
                    pContext->Parser.DrmInterface.pNvDrmDestroyContext = (NvError (*) (NvDrmContextHandle))
                        NvOsLibraryGetSymbol (pContext->DrmLibHandle, "NvDrmDestroyContext");
                    pContext->Parser.DrmInterface.pNvDrmBindLicense = (NvError (*) (NvDrmContextHandle))
                        NvOsLibraryGetSymbol (pContext->DrmLibHandle, "NvDrmBindLicense");
                    pContext->Parser.DrmInterface.pNvDrmUpdateMeteringInfo = (NvError (*) (NvDrmContextHandle))
                        NvOsLibraryGetSymbol (pContext->DrmLibHandle, "NvDrmUpdateMeteringInfo");
                    pContext->Parser.DrmInterface.pNvDrmDecrypt = (NvError (*) (NvDrmContextHandle, NvU8 *, NvU32))
                        NvOsLibraryGetSymbol (pContext->DrmLibHandle, "NvDrmDecrypt");
                    pContext->Parser.DrmInterface.pNvDrmBindContent = (NvError (*) (NvDrmContextHandle, NvDrmBindInfo *, NvU32))
                        NvOsLibraryGetSymbol (pContext->DrmLibHandle, "NvDrmBindContent");
                    pContext->Parser.DrmInterface.pNvDrmGenerateLicenseChallenge = (NvError (*) (NvDrmContextHandle,
                        NvU16 *,
                        NvU32 *,
                        NvU8 *,
                        NvU32 *))
                        NvOsLibraryGetSymbol (pContext->DrmLibHandle, "NvDrmGenerateLicenseChallenge");
                }
                else
                {
                    hParserCore->domainName = NvMMErrorDomain_DRM;
                    return NvError_ParserDRMFailure;
                }
            }
        }
        if (pContext->Parser.DrmInterface.pNvDrmCreateContext != NULL)
        {
            Status = pContext->Parser.DrmInterface.pNvDrmCreateContext (pDrmContext);
            if (NvError_Success != Status)
            {
                hParserCore->domainName = NvMMErrorDomain_DRM;
                return Status;
            }
            //               pContext->NvDrmUpdateMeteringInfo = (NvU32)pContext->parser.DrmInterface.pNvDrmUpdateMeteringInfo;
            //               pContext->NvDrmDestroyContext = (NvU32)pContext->parser.DrmInterface.pNvDrmDestroyContext;
            //               pContext->NvDrmContext =(NvU32)(*pDrmContext);
            Status =  NvOsGetConfigU32 ("meteringPolicy", &PolicyType);
            if ( (Status != NvSuccess))
                PolicyType = 1; //defualt
            Status =  NvOsGetConfigU32 ("meteringTime", &MeteringTime);
            if ( (Status != NvSuccess))
                MeteringTime = 10;//defult
            Status =  NvOsGetConfigU32 ("licenseConsumeTime", &LicenseConsumeTime);
            if ( (Status != NvSuccess))
                LicenseConsumeTime = 10;

            TotalTrackTime = pContext->TotalDuration;
            if (TotalTrackTime != 0)
            {
                TotalTrackTime = TotalTrackTime / 10000000;
                if (TotalTrackTime < LicenseConsumeTime)
                    LicenseConsumeTime = (NvU32) TotalTrackTime;
            }
            if (MeteringTime <= LicenseConsumeTime)
                MeteringTime = LicenseConsumeTime;

            pContext->LicenseConsumeTime = LicenseConsumeTime;
            pContext->MeteringTime = MeteringTime;
            pContext->MeteringPolicyType = PolicyType;
            pContext->CoreContext = (NvU32) hParserCore;
        }

        if (pContext->Parser.DrmInterface.pNvDrmBindContent != NULL)
        {
            //Pass SINF atom here
            Status = NvMp4ParserGetAtom (&pContext->Parser, MP4_FOURCC('s', 'i', 'n', 'f'), Atom, &AtomSize);
            if (NvError_Success != Status)
            {
                hParserCore->domainName = NvMMErrorDomain_DRM;
                return Status;
            }
            bContent.privData = (void *) Atom;
            Status = pContext->Parser.DrmInterface.pNvDrmBindContent (*pDrmContext, &bContent, AtomSize);
            if (NvError_Success != Status)
            {
                hParserCore->domainName = NvMMErrorDomain_DRM;
                return Status;
            }
        }
        if (pContext->Parser.DrmInterface.pNvDrmGenerateLicenseChallenge != NULL)
        {
            Status = pContext->Parser.DrmInterface.pNvDrmGenerateLicenseChallenge (*pDrmContext, NULL,
                &hParserCore->licenseUrlSize, NULL,
                &hParserCore->licenseChallengeSize
                );

            if (NvError_Success == Status)
            {
                hParserCore->licenseUrl      = (NvU16 *) NvOsAlloc (hParserCore->licenseUrlSize * sizeof (NvU16));
                if (hParserCore->licenseUrl == NULL)
                    return NvError_InsufficientMemory;

                hParserCore->licenseChallenge = (NvU8  *) NvOsAlloc (hParserCore->licenseChallengeSize);
                if (hParserCore->licenseChallenge == NULL)
                {
                    NvOsFree (hParserCore->licenseUrl);
                    return NvError_InsufficientMemory;
                }
                Status = pContext->Parser.DrmInterface.pNvDrmGenerateLicenseChallenge (*pDrmContext, hParserCore->licenseUrl,
                    &hParserCore->licenseUrlSize, hParserCore->licenseChallenge,
                    &hParserCore->licenseChallengeSize
                    );
                if (NvError_Success != Status)
                {
                    hParserCore->domainName = NvMMErrorDomain_DRM;
                    return Status;
                }
            }
            else
            {
                hParserCore->domainName = NvMMErrorDomain_DRM;
                return Status;
            }
        }
        if (pContext->Parser.DrmInterface.pNvDrmBindLicense != NULL)
        {
            Status = pContext->Parser.DrmInterface.pNvDrmBindLicense (*pDrmContext);
            if (NvError_Success != Status)
            {
                hParserCore->domainName = NvMMErrorDomain_DRM;
                return Status;
            }
        }

    }
    if (pContext->Parser.AudioIndex == MP4_INVALID_NUMBER && pContext->Parser.VideoIndex == MP4_INVALID_NUMBER)
    {
        NvMp4ParserDeInit (&pContext->Parser);

        if(pContext->Parser.EmbeddedURL != NULL)
        {
            Status=NvError_RefURLAvailable;
        } 
        else if (pContext->Parser.URICount > 1)
        {
            Status=NvError_RefURLAvailable;
            Mp4CoreParserGetNextRMRAStream (pContext,1);
        }
    }



    if (hParserCore->UlpKpiMode)
    {
        MediaTimeScale = NvMp4ParserGetMediaTimeScale (&pContext->Parser , 0);
        StreamDuration = NvMp4ParserGetMediaDuration (&pContext->Parser , 0);
        if (MediaTimeScale != 0)
            TotalTrackTimeNs = ( (StreamDuration * TICKS_PER_SECOND) / MediaTimeScale) * 10;
        else
            TotalTrackTimeNs = 0;
        // Store the song duration in 100 nano seconds.
        NvmmUlpKpiSetSongDuration (TotalTrackTimeNs);
    }

    pContext->VideoFPS = 0;
    pContext->Width = 0;
    pContext->Height = 0;
    pContext->rate.Rate  = 1000;

    /* FIX this , why declare these handles everywhere? */
    hParserCore->pPipe    = pContext->Parser.pPipe;
    hParserCore->cphandle = pContext->Parser.hContentPipe;

cleanup:
    if (Status != NvSuccess && Status != NvError_RefURLAvailable)
    {
        Mp4CoreParserClose (hParserCore);
    }

    return Status;
}

static
NvError Mp4ParserTrackTimeToSampleChunk (NvMp4Parser *pNvMp4Parser, NvU64 RefrenceTime, NvU64 *RefrenceFrame)
{
    NvError Status = NvSuccess;
    NvU64 RequestedTime = 0;
    NvU32 EntryCount = 0;
    NvU32 EntrySize = 0;
    NvU32 ReadCount = 0;
    NvU32 TotalReadCount = 0;
    NvU32 i = 0, j = 0;
    NvBool ReferenceIdentified = NV_FALSE;
    NvU64 FileOffset = 0;
    NvU8 *ReadBuffer = NULL;
    NvU64 FrameCount = 0, TimeIncrement = 0;
    NvU32 k = 0, SampleCount = 0,  SampleDelta = 0;
    NvMp4TrackInformation *pTrackInfo = NULL;

    NV_LOGGER_PRINT((NVLOG_MP4_PARSER, NVLOG_DEBUG, "++%s[%d]", __FUNCTION__, __LINE__));
    NVMM_CHK_ARG (pNvMp4Parser);

    RequestedTime = ((RefrenceTime*(NvU64)pNvMp4Parser->TrackInfo[pNvMp4Parser->VideoIndex].TimeScale)/(10*TICKS_PER_SECOND));
    pTrackInfo = &pNvMp4Parser->TrackInfo[pNvMp4Parser->VideoIndex];
    // Now find the corresponding Frame for this time.
    // read STTS
    FileOffset = pTrackInfo->STTSOffset + 16;
    NVMM_CHK_CP (pNvMp4Parser->pPipe->SetPosition64 (pNvMp4Parser->hContentPipe, (CPint64) FileOffset, CP_OriginBegin));

    EntryCount = pTrackInfo->STTSEntries;
    EntrySize = sizeof (NvU32) *2;
    TotalReadCount = EntryCount * EntrySize;
    for (i = 0; i <= TotalReadCount / READ_BULK_DATA_SIZE; i++)
    {
        ReadBuffer = pNvMp4Parser->pTempBuffer;
        ReadCount = (i < TotalReadCount / READ_BULK_DATA_SIZE) ? READ_BULK_DATA_SIZE : (TotalReadCount % READ_BULK_DATA_SIZE);
        if (ReadCount > 0)
            NVMM_CHK_CP (pNvMp4Parser->pPipe->cpipe.Read (pNvMp4Parser->hContentPipe, (CPbyte *) ReadBuffer, (CPuint) ReadCount));
        for (j = 0; j < ReadCount; j += EntrySize, ReadBuffer += EntrySize)
        {
            if (EntryCount == 1)
            {
                // EntryCount == 1: indicates all frames are have constant incrementing time stamp
                SampleCount = NV_BE_TO_INT_32 (&ReadBuffer[0]);
                SampleDelta = NV_BE_TO_INT_32 (&ReadBuffer[4]);
                TimeIncrement =  (NvU64) SampleDelta;
                for (k = 0; k < SampleCount; k++)
                {
                if (TimeIncrement >= RequestedTime)
                {
                    *RefrenceFrame = FrameCount;
                    Status = NvSuccess;
                    ReferenceIdentified = NV_TRUE;
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "STSS EntryCount 1 :Mp4ParserTrackTimeToSampleChunk RefrenceFrame = %d\n", FrameCount));
                    goto cleanup; // we are done
                }
                FrameCount++;
                TimeIncrement += SampleDelta;
                }
            }
            else
            {
            // sample table is:
            // 4-byte: # of frames
            // 4-byte: duration for the given # of frames
            // Now Scan thru dts sample count and get the refrence frame match for time.
            //
                SampleCount = NV_BE_TO_INT_32 (&ReadBuffer[0]);
                SampleDelta = NV_BE_TO_INT_32 (&ReadBuffer[4]);
                for (k = 0; k < SampleCount; k++)
                {
                    if (TimeIncrement >= RequestedTime)
                    {
                        *RefrenceFrame = FrameCount;
                        Status = NvSuccess;
                        ReferenceIdentified = NV_TRUE;
                        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Mp4ParserTrackTimeToSampleChunk RefrenceFrame = %d\n", FrameCount));
                        goto cleanup; // we are done
                    }
                    FrameCount++;
                    TimeIncrement += SampleDelta;
                }
            }
        }
    }

cleanup:
    if (!ReferenceIdentified)
    {
        Status = NvError_ParserFailure;
    }
    NV_LOGGER_PRINT((NVLOG_MP4_PARSER, NVLOG_DEBUG, "--%s[%d]", __FUNCTION__, __LINE__));
    return Status;

}

static NvError
Mp4CoreParserGetStreamInfo (
                            NvMMParserCoreHandle hParserCore,
                            NvMMStreamInfo **pInfo)
{
    NvError Status = NvSuccess;
    NvError err = NvSuccess;
    NvError Mp4Status = NvSuccess;
    NvU32 TrackIndex, TotalTracks, Count;
    NvU64 MediaTimeScale = 0;
    NvMMMp4ParserCoreContext* pContext = NULL;
    NvS64 StreamDuration;
    NvF32 Fps = 0;
    NvMp4TrackTypes TrackType;
    void * HeaderData = NULL ;
    NvU8 * Buffer ;
    NvU32 HeaderSize = 0;
    NvU32 MaxBitRate;
    NvU32 NumFilledLen = 0;
    NvU64 dts = 0;
    NvS32 cts = 0;
    NvU64 elst = 0;
    NvS64 Delta = 0;
    NvBool IsAudio, IsVideo;
    NvAVCConfigData *pAVCConfigData = NULL;
    NvU64 MediaDuration = 0;
    NvMMStreamInfo *pCurrentStreamInfo = NULL ;
    NvMp4TrackInformation *pTrackInfo = NULL;

    //Check for pContext validity
    NVMM_CHK_ARG(hParserCore && hParserCore->pContext && pInfo);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;
    pCurrentStreamInfo = *pInfo;
     // Get Mp4 Handle
    TotalTracks = NvMp4ParserGetNumTracks (&pContext->Parser);

    IsAudio = IsVideo = NV_FALSE;
    for (TrackIndex = 0; TrackIndex < TotalTracks; TrackIndex++)
    {
        Mp4Status = NvSuccess;
        pTrackInfo = &pContext->Parser.TrackInfo[TrackIndex];
        TrackType = (NvMp4TrackTypes) NvMp4ParserGetTracksInfo (&pContext->Parser , TrackIndex) ;
        MediaTimeScale = NvMp4ParserGetMediaTimeScale (&pContext->Parser , TrackIndex);

        // Set Block Type as Mp4 parser
        pCurrentStreamInfo[TrackIndex].BlockType = NvMMBlockType_SuperParser;
        // Initialize parser state
        pContext->ParserState[TrackIndex] = MP4PARSE_INIT;

        /*According the the track type, following properties are filled in below block:
        Stream Type
        Maximum acudio frame size
        sampling freq index
        Sample rate
        Number of channels
        Bitrate
        */
        IsAudio = NV_FALSE;
        switch (TrackType)
        {
        case NvMp4TrackTypes_AAC:
            if (!IsAudio)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_AAC;
                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                pContext->Parser.FramingInfo[TrackIndex].MaxFrameSize = AAC_DEC_MAX_INPUT_BUFFER_SIZE;
                IsAudio = NV_TRUE;
                if ( (Mp4Status = NvMp4ParserGetAudioProps (&pContext->Parser, &pCurrentStreamInfo[TrackIndex], TrackIndex, &MaxBitRate)) != NvSuccess)
                {
                    break;
                }
            }
            break;

        case NvMp4TrackTypes_BSAC:
            if (!IsAudio)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_BSAC;
                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                pContext->Parser.FramingInfo[TrackIndex].MaxFrameSize = BSAC_DEC_MAX_INPUT_BUFFER_SIZE;
                IsAudio = NV_TRUE;
                Mp4Status = NvMp4ParserGetAudioProps (&pContext->Parser, &pCurrentStreamInfo[TrackIndex], TrackIndex, &MaxBitRate);
                if ( (Mp4Status = NvMp4ParserGetAudioProps (&pContext->Parser, &pCurrentStreamInfo[TrackIndex], TrackIndex, &MaxBitRate)) != NvSuccess)
                {
                    break;
                }
            }
            break;

        case NvMp4TrackTypes_AACSBR:
            if (!IsAudio)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_AACSBR;

                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                IsAudio = NV_TRUE;
                pContext->Parser.FramingInfo[TrackIndex].MaxFrameSize = AAC_DEC_MAX_INPUT_BUFFER_SIZE;
                if ( (Mp4Status = NvMp4ParserGetAudioProps (&pContext->Parser, &pCurrentStreamInfo[TrackIndex], TrackIndex, &MaxBitRate)) != NvSuccess)
                {
                    break;
                }
            }
            break;
        case NvMp4TrackTypes_NAMR:
            if (!IsAudio)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_NAMR;
                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                IsAudio = NV_TRUE;
                pContext->Parser.FramingInfo[TrackIndex].MaxFrameSize = AMR_DEC_MAX_INPUT_BUFFER_SIZE;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.BitRate = 0;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.SampleRate = 0;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.NChannels = 0;
            }
            break;
        case NvMp4TrackTypes_WAMR:
            if (!IsAudio)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_WAMR;
                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                IsAudio = NV_TRUE;
                pContext->Parser.FramingInfo[TrackIndex].MaxFrameSize = AMR_DEC_MAX_INPUT_BUFFER_SIZE;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.BitRate = 0;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.SampleRate = 0;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.NChannels = 0;
            }
            break;
        case NvMp4TrackTypes_QCELP:
            if (!IsAudio)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_QCELP;
                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                IsAudio = NV_TRUE;
                pContext->Parser.FramingInfo[TrackIndex].MaxFrameSize = BSAC_DEC_MAX_INPUT_BUFFER_SIZE;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.BitRate = 0;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.SampleRate = 0;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.NChannels = 0;
            }
            break;
        case NvMp4TrackTypes_EVRC:
            if (!IsAudio)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_EVRC;
                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                IsAudio = NV_TRUE;
                pContext->Parser.FramingInfo[TrackIndex].MaxFrameSize = BSAC_DEC_MAX_INPUT_BUFFER_SIZE;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.BitRate = 0;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.SampleRate = 0;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.NChannels = 0;
            }
            break;
        case NvMp4TrackTypes_MPEG4:
            if (!IsVideo)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_MPEG4;
                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Width = pContext->Parser.TrackInfo[TrackIndex].Width;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Height = pContext->Parser.TrackInfo[TrackIndex].Height;
                if ( (Mp4Status = NvMp4ParserGetBitRate (&pContext->Parser, TrackIndex, &pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate ,
                    &MaxBitRate)) != NvSuccess)
                {
                    break;
                }
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "MPEG4: AvgBitRate = %ld - MaxBitRate = %ld\n",
                    pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate, MaxBitRate));

            }
            break;
        case NvMp4TrackTypes_AVC:
            if (!IsVideo)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_H264;
                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                /*take the first avcC*/
                pAVCConfigData = pContext->Parser.pAVCConfigData[0];
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Width = pAVCConfigData->Width;
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Height = pAVCConfigData->Height;
                if ( (Mp4Status = NvMp4ParserGetBitRate (&pContext->Parser, TrackIndex, &pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate ,
                    &MaxBitRate)) != NvSuccess)
                {
                    break;
                }
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "H264: AvgBitRate = %ld - MaxBitRate = %ld\n",
                    pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate, MaxBitRate));
            }
            break;
        case NvMp4TrackTypes_S263:
            if (!IsVideo)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_H263;
                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                HeaderData = NvMp4ParserGetDecConfig (&pContext->Parser, TrackIndex, &HeaderSize);
                Buffer = (NvU8*) HeaderData;
                Buffer += (8 + 8 + 8);  /* Version + Revision Level and Vendor  + Temporal quality and Spatial Quality*/
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Width = (Buffer[0] << 8) | (Buffer[1]);
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Height = (Buffer[2] << 8) | (Buffer[3]);
                if ( (Mp4Status = NvMp4ParserGetBitRate (&pContext->Parser, TrackIndex, &pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate ,
                    &MaxBitRate)) != NvSuccess)
                {
                    break;
                }
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "H263: AvgBitRate = %ld - MaxBitRate = %ld\n",
                    pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate, MaxBitRate));

            }
            break;
        case NvMp4TrackTypes_MJPEGA:
        case NvMp4TrackTypes_MJPEGB:
            if (!IsVideo)
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_MJPEG;
                if ( (Mp4Status = NvMp4ParserSetTrack (&pContext->Parser, TrackIndex)) != NvSuccess)
                {
                    break;
                }
                HeaderData = NvMp4ParserGetDecConfig (&pContext->Parser, TrackIndex, &HeaderSize);
                Buffer = (NvU8*) HeaderData;
                Buffer += (8 + 8 + 8);  /* Version + Revision Level and Vendor  + Temporal quality and Spatial Quality*/
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Width = (Buffer[0] << 8) | (Buffer[1]);
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Height = (Buffer[2] << 8) | (Buffer[3]);
                if ( (Mp4Status = NvMp4ParserGetBitRate (&pContext->Parser, TrackIndex, &pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate ,
                    &MaxBitRate)) != NvSuccess)
                {
                    break;
                }
            }
            break;
        case NvMp4TrackTypes_OTHER:
            pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_OTHER;
            break;
        case NvMp4TrackTypes_UnsupportedAudio:
            pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_UnsupportedAudio;
            break;
        case NvMp4TrackTypes_UnsupportedVideo:
            pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_UnsupportedVideo;
            break;
        default:
            break;
        }

        if ( (!IsVideo) && NVMP4_ISTRACKVIDEO (TrackType))
        {
            if (Mp4Status != NvSuccess)
            {
                Status = NvError_ParserFailure;
                continue;
            }
            cts = 0;
            elst = 0;
            if ( (!IsVideo) && NVMP4_ISTRACKVIDEO (TrackType))
            {
                if(!pContext->Parser.IsStreaming)
                {
                    NvU32 SyncEntry =0;
                    NvU32 TempSize = 0;
                    pTrackInfo->LargestIFrameTimestamp = (NvU64)-1;
                    /* Compute size and time stamps for top 20 I frames*/
                    /* Check for Max I frame size with in 20 values and store corresponding I-frame time for thumb nail metadata type (NvMMMetaDataInfo_ThumbnailSeekTime*/
                    for (SyncEntry= 0; SyncEntry< pTrackInfo->MaxScanSTSSEntries; SyncEntry++)
                    {
                        pContext->Parser.FramingInfo[TrackIndex].FrameCounter = pTrackInfo->IFrameNumbers[SyncEntry] -1;

                        NVMM_CHK_ERR (NvMp4ParserGetCurrentAccessUnit (&pContext->Parser, TrackIndex,
                            NULL,
                            &NumFilledLen,
                            (NvU64*) & dts,
                            (NvS32*) & cts,
                            (NvU64*) & elst));

                        if(pContext->Parser.TrackInfo[TrackIndex].SyncFrameSize >= TempSize)
                        {
                            TempSize = pContext->Parser.TrackInfo[TrackIndex].SyncFrameSize;
                            if (MediaTimeScale != 0)
                                pTrackInfo->LargestIFrameTimestamp = (NvU64) ( ( ( ( (NvU64) (cts + dts)) * TICKS_PER_SECOND * 10) / MediaTimeScale));
                            else
                                pTrackInfo->LargestIFrameTimestamp =(NvU64) -1;
                        }
                    }
                    pContext->Parser.FramingInfo[TrackIndex].FrameCounter = 0;
                }

                //Get the timestamps
                if (pContext->Parser.FramingInfo[TrackIndex].TotalNoFrames)
                {
                    pContext->Parser.FramingInfo[TrackIndex].FrameCounter = pContext->Parser.FramingInfo[TrackIndex].TotalNoFrames - 1;
                    err = NvMp4ParserGetCurrentAccessUnit (&pContext->Parser, TrackIndex,
                        NULL,
                        &NumFilledLen,
                        (NvU64*) & dts,
                        (NvS32*) & cts,
                        (NvU64*) & elst);
                    MediaDuration = dts;
                    if (err != NvSuccess)
                    {
                        Status = NvError_ParserFailure;
                    }
                    pContext->Parser.FramingInfo[TrackIndex].FrameCounter = 0;
                    StreamDuration  = NvMp4ParserGetMediaDuration (&pContext->Parser , TrackIndex);

                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "GetStreamInfo: StreamDuration = %d - MediaDuration = %ld\n", StreamDuration, MediaDuration));
                    if (StreamDuration != (NvS64) MediaDuration)
                    {
                        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "GetStreamInfo: VIDEO DURATION MIS-MATCH\n"));
                        Delta = (NvS64) (MediaDuration - StreamDuration);
                        if (Delta > 1000)
                        {
                            StreamDuration = MediaDuration;
                            NvMp4ParserSetMediaDuration (&pContext->Parser , TrackIndex, MediaDuration);
                        }
                    }
                }
                else
                {
                    StreamDuration  = NvMp4ParserGetMediaDuration (&pContext->Parser , TrackIndex);
                }

                Count = NvMp4ParserGetTotalMediaSamples (&pContext->Parser , TrackIndex);

                // Calculate total time from media Time Scale
                if (MediaTimeScale != 0)
                    pCurrentStreamInfo[TrackIndex].TotalTime = (NvU64) ( ( (NvU64) StreamDuration * TICKS_PER_SECOND) / MediaTimeScale) * 10;
                else
                    pCurrentStreamInfo[TrackIndex].TotalTime = 0;

            }
            pContext->VideoDuration = pCurrentStreamInfo[TrackIndex].TotalTime;
            pContext->Parser.FramingInfo[TrackIndex].MaxFrameSize = pContext->MaxVidFrameSize;
            pContext->ParserState[TrackIndex] = MP4PARSE_HEADER;
            if (pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate == 0)
            {
                if (pCurrentStreamInfo[TrackIndex].TotalTime != 0)
                {
                    pContext->Parser.TrackInfo[TrackIndex].AvgBitRate = (NvU32) ( (pContext->Parser.MDATSize * (10 * TICKS_PER_SECOND) * 8) / pCurrentStreamInfo[TrackIndex].TotalTime);
                    pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate = pContext->Parser.TrackInfo[TrackIndex].AvgBitRate;
                }
            }

            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "GetStreamInfo: VideoBitRate = %ld\n", pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate));

            if (pCurrentStreamInfo[TrackIndex].TotalTime > 0)
                Fps = (NvF32) ( ( (NvU64) Count * 10.0 * TICKS_PER_SECOND) / (pCurrentStreamInfo[TrackIndex].TotalTime));
            else
                Fps = 0;

            if (Fps)
            {
                pContext->PerFrameTime = (NvS64) (10 * TICKS_PER_SECOND / Fps);
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "GetStreamInfo: PerFrameTime = %ld\n", pContext->PerFrameTime));
            }
            pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Fps = (NvU32) (Fps * FLOAT_MULTIPLE_FACTOR);
            pContext->VideoFPS = pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Fps;
            pContext->Width = pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Width;
            pContext->Height = pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Height;
            if(pCurrentStreamInfo[TrackIndex].StreamType == NvMMStreamType_H264)
            {
//VARUN-need conf from devs
                //pAVCConfigData = &pContext->Parser.AVCConfigData;
                pAVCConfigData = pContext->Parser.pAVCConfigData[0];
//end-need conf from devs
                // Profile: High -100 : Main - 77
#if 0
                if ( ((pContext->Parser.AVCConfigData.ProfileIndication == 100) &&
                    ((pContext->Width*pContext->Height) > (640*480))) ||              // HP: VGA
                    ((pContext->Parser.AVCConfigData.ProfileIndication == 77) &&      // MP: CABAC 720
                    (pAVCConfigData->EntropyType == AvcEntropy_Cabac) &&
                    ((pContext->Width*pContext->Height) > (1280*720))) )
                {
                    for (TrackIndex=0; TrackIndex < TotalTracks; TrackIndex++)
                    {
                        pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_OTHER;
                    }
                    pContext->Parser.MvHdData.AudioTracks = 0;
                    pContext->Parser.MvHdData.VideoTracks = 0;
                    Status = NvError_ParserFailure;
                    goto cleanup;
                }
#endif
            }
            // Restricting the resolution to max qsxga size
            if ( (pContext->Width > 2560) || (pContext->Height > 2048))
            {
                pCurrentStreamInfo[TrackIndex].StreamType = NvMMStreamType_UnsupportedVideo;
            }
            IsVideo = NV_TRUE;
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "VID: TOTAL TIME= %ld\n", pCurrentStreamInfo[TrackIndex].TotalTime));
        }
        if ( (IsAudio) && NVMP4_ISTRACKAUDIO (TrackType))
        {
            if (Mp4Status != NvSuccess)
            {
                Status = NvError_ParserFailure;
                continue;
            }
            StreamDuration  = NvMp4ParserGetMediaDuration (&pContext->Parser , TrackIndex);

            Count = NvMp4ParserGetTotalMediaSamples (&pContext->Parser , TrackIndex);

            // Calculate total time from media Time Scale
            if (MediaTimeScale != 0)
                pCurrentStreamInfo[TrackIndex].TotalTime = ( (StreamDuration * TICKS_PER_SECOND) / MediaTimeScale) * 10;
            else
                pCurrentStreamInfo[TrackIndex].TotalTime = 0;

            if (pCurrentStreamInfo[TrackIndex].TotalTime >= TICKS_PER_SECOND)
                pContext->AudioFPS[TrackIndex] = (NvU32) ( (Count * 10) / (pCurrentStreamInfo[TrackIndex].TotalTime / TICKS_PER_SECOND));
            else
                pContext->AudioFPS[TrackIndex] = 0;
            if (pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.BitRate >= pContext->AudioBitrate)
            {
                pContext->AudioBitrate = pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.BitRate;
                // Update Audio FPS to the reader &pContext->parser
                pContext->Parser.AudioFPS = pContext->AudioFPS[TrackIndex];
                pContext->AudioDuration = pCurrentStreamInfo[TrackIndex].TotalTime;
                pContext->Parser.AudioIndex = TrackIndex;
            }
            pContext->ParserState[TrackIndex] = MP4PARSE_HEADER;
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "AUD: TOTAL TIME= %ld\n", pCurrentStreamInfo[TrackIndex].TotalTime));
        }

        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, " MP4 Stream Type : %x, Block Type: %x", pCurrentStreamInfo[TrackIndex].StreamType, pCurrentStreamInfo[TrackIndex].BlockType));
        if (NVMM_ISSTREAMAUDIO (pCurrentStreamInfo[TrackIndex].StreamType))
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "SampleRate : %d, BitRate: %d, channels - %d, bits per Sample - %d",
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.SampleRate,
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.BitRate,
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.NChannels,
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.AudioProps.BitsPerSample));
        }
        else
        {
            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "Fps : %d, BitRate: %d, Width - %d, Height - %d",
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Fps,
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.VideoBitRate,
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Width,
                pCurrentStreamInfo[TrackIndex].NvMMStream_Props.VideoProps.Height));
        }

    }

    if (pContext->AudioDuration < pContext->VideoDuration)
        pContext->TotalDuration = pContext->VideoDuration;
    else
        pContext->TotalDuration = pContext->AudioDuration;

cleanup:
    return Status;
}

static NvError
Mp4CoreParserSetRate (
                      NvMMParserCoreHandle hParserCore,
                      NvS32 rate)
{
    NvError Status = NvSuccess;
    NvMMMp4ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;
    NVMM_CHK_ARG (pContext);

    pContext->rate.Rate = rate;

    if (rate < -12000)
    {
        pContext->Parser.pPipe->StopCaching (pContext->Parser.hContentPipe);
    }
    else
    {
        pContext->Parser.pPipe->StartCaching (pContext->Parser.hContentPipe);
    }

cleanup:
    return Status;
}

static NvS32
Mp4CoreParserGetRate (
                      NvMMParserCoreHandle hParserCore)
{
    NvError Status = NvSuccess;
    NvMMMp4ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;

cleanup:
    return (Status == NvSuccess) ? pContext->rate.Rate : NvError_ParserFailure;    
}


static NvError
Mp4CoreParserGetPosition (
                          NvMMParserCoreHandle hParserCore,
                          NvU64 *pTimestamp)
{
    NvError Status = NvSuccess;
    NvMMMp4ParserCoreContext* pContext = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pTimestamp);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;

    * (pTimestamp) = pContext->Position.Position;

cleanup:
    return Status;
}

static NvError
Mp4CoreParserSetPosition (
                          NvMMParserCoreHandle hParserCore,
                          NvU64 *pTimeStamp)
{
    NvMMMp4ParserCoreContext* pContext = NULL;
    NvS64 StreamDuration = 0, TotalTime = 0;
    NvU32 Count = 0;
    NvS32 Direction = 1000;
    NvError err = NvSuccess;
    NvU32 TotalTracks = 0, TrackIndex = 0;
    NvMp4TrackTypes TrackType;
    NvMp4TrackInformation *pTrackInfo;
    NvS32 rate = 1000, ReferenceFrame = 0, FrameCounter = 0;
    NvU32 NumFilledLen = 0;
    NvU64 dts = 0, ts = 0;
    NvS32 cts = 0;
    NvU64 MediaTimeScale = 0, elst = 0, CurrentTimeStamp = 0;
    NvS64 Delta = 0;
    NvU64 DesiredFrame =0;
    NvError Status = NvSuccess;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pTimeStamp);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;

    pContext->Parser.SeekVideoFrameCounter = 0;
    pContext->Parser.TempCount = 0;
    TotalTracks = NvMp4ParserGetNumTracks (&pContext->Parser);
    rate = Mp4CoreParserGetRate (hParserCore);
    pContext->Parser.rate = rate;

    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG,
        "Mp4CoreParserSetPosition : SeekToTime = %lld msec, CurrentTime=%lld msec, TotalTime=%lld msec\n",
        *pTimeStamp / 10000, pContext->CurrentTimeStamp/10000, pContext->TotalDuration/10000));

    // SEEK TO ZERO position
    if (*pTimeStamp == 0)
    {
        // Reset audio video frame counters and position
        pContext->Parser.FramingInfo[pContext->Parser.VideoIndex].FrameCounter = 0;
        pContext->Parser.FramingInfo[pContext->Parser.AudioIndex].FrameCounter = 0;
        pContext->CurrentTimeStamp = 0;
        pContext->Position.Position = 0;
        //Reset the flag is needed for repeat mode
        pContext->IsFileEOF = NV_FALSE;
        for (TrackIndex = 0; TrackIndex < TotalTracks ; TrackIndex++)
        {
            TrackType = (NvMp4TrackTypes) NvMp4ParserGetTracksInfo (&pContext->Parser , TrackIndex) ;
            if (NVMP4_ISTRACKVIDEO (TrackType) || NVMP4_ISTRACKAUDIO (TrackType))
            {
                pContext->ParserState[TrackIndex] = MP4PARSE_HEADER;
            }
        }
        Status = NvSuccess;
        goto cleanup;
    }

    // SEEK TO END position
    if (*pTimeStamp >= pContext->TotalDuration)
    {
        // Set audio video frame counters to total number of frames
        pContext->Parser.FramingInfo[pContext->Parser.VideoIndex].FrameCounter = pContext->Parser.FramingInfo[pContext->Parser.VideoIndex].TotalNoFrames;
        pContext->Parser.FramingInfo[pContext->Parser.AudioIndex].FrameCounter = pContext->Parser.FramingInfo[pContext->Parser.AudioIndex].TotalNoFrames;
        pContext->CurrentTimeStamp = pContext->TotalDuration;
        pContext->Position.Position = pContext->TotalDuration;
        //Update the time stamp with total duration
        *pTimeStamp = pContext->TotalDuration;
        //Set the flag is needed for repeat mode
        pContext->IsFileEOF = NV_TRUE;
        Status = NvSuccess;
        goto cleanup;
    }

    for (TrackIndex = 0; TrackIndex < TotalTracks ; TrackIndex++)
    {
        TrackType = (NvMp4TrackTypes) NvMp4ParserGetTracksInfo (&pContext->Parser , TrackIndex) ;
        pContext->Position.Position = *pTimeStamp;
        pTrackInfo = &pContext->Parser.TrackInfo[TrackIndex];
        if (NVMP4_ISTRACKVIDEO (TrackType) && (TrackIndex == pContext->Parser.VideoIndex))
        {
            Count = NvMp4ParserGetTotalMediaSamples (&pContext->Parser , TrackIndex);
            MediaTimeScale = NvMp4ParserGetMediaTimeScale (&pContext->Parser , TrackIndex);
            StreamDuration = NvMp4ParserGetMediaDuration (&pContext->Parser , TrackIndex);

            if (MediaTimeScale != 0)
                TotalTime = ( (StreamDuration * TICKS_PER_SECOND) / MediaTimeScale) * 10;
            else
                TotalTime = 0;
            if (pTrackInfo->IsELSTPresent && pTrackInfo->ELSTEntries > 2)
            {
                Status = Mp4ParserTrackTimeToSampleChunk(&pContext->Parser, *pTimeStamp, &DesiredFrame);
                ReferenceFrame = (NvS32)DesiredFrame;
                if(Status != NvSuccess)
                {
                    if ((Status == NvError_ParserFailure) && TotalTime)
                    {
                        ReferenceFrame = (NvS32) ( ( (*pTimeStamp) * (NvU64) Count) / TotalTime);
                    }
                    else
                    {
                        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_ERROR,
                        "Mp4CoreParserSetPosition : MpParserTrackTimeToSampleChunk Failed! NvError=0x%08x", Status));
                        goto cleanup;
                    }
                }
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                "Mp4CoreParserSetPosition - MpParserTrackTimeToSampleChunk : ReferenceFrame = %d\n",ReferenceFrame));
            }
            else if (TotalTime)
            {
                ReferenceFrame = (NvS32) ( ( (*pTimeStamp) * (NvU64) Count) / TotalTime);
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                "Mp4CoreParserSetPosition : ReferenceFrame = %d\n",ReferenceFrame));
            }

            CurrentTimeStamp = pContext->CurrentTimeStamp;
            Direction = 1000;
            if ( (*pTimeStamp < CurrentTimeStamp) && rate == 1000)
            {
                Direction = -1000;
                Delta = (CurrentTimeStamp - *pTimeStamp);

                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                "Mp4CoreParserSetPosition : Delta = %lld msec PerFrameTime = %ld msec\n",
                    Delta / 10000, pContext->PerFrameTime / 10000));

                if (Delta <= ( (MAX_VIDEO_BUFFERS) * pContext->PerFrameTime))
                {
                    Direction = 1000;
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                        "Mp4CoreParserSetPosition : Delta < MAX_VIDEO_BUFFERS*PerFrameTime=%lld. DIRECTION CHANGE\n",
                        (MAX_VIDEO_BUFFERS*pContext->PerFrameTime) / 10000));
                }
            }

            //check for video track
            if (*pTimeStamp >= pContext->VideoDuration)
            {
                //Go to Last frame if requested time stamp is greater than video duration
                pContext->Parser.FramingInfo[pContext->Parser.VideoIndex].FrameCounter = pContext->Parser.FramingInfo[pContext->Parser.VideoIndex].TotalNoFrames;
            }

            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                "Mp4CoreParserSetPosition : CurrentFrame = %d - rate = %d - dir = %d\n",
                pContext->Parser.FramingInfo[pContext->Parser.VideoIndex].FrameCounter, rate, Direction));

            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                "Mp4CoreParserSetPosition : RequestedFrame = %d - rate = %d - dir = %d\n",
                ReferenceFrame, rate, Direction));

            if (rate == 1000)
            {
                if (Direction == 1000)
                    FrameCounter = NvMp4ParserGetNextSyncUnit (&pContext->Parser, ReferenceFrame, NV_TRUE);
                else if (Direction == -1000)
                    FrameCounter = NvMp4ParserGetNextSyncUnit (&pContext->Parser, ReferenceFrame, NV_FALSE);
            }
            if (rate > 0 && rate != 1000)
            {
                FrameCounter = NvMp4ParserGetNextSyncUnit (&pContext->Parser, ReferenceFrame, NV_TRUE);
            }
            else if (rate < 0)
            {    //seek to previous I frame
                FrameCounter = NvMp4ParserGetNextSyncUnit (&pContext->Parser, ReferenceFrame, NV_FALSE);
            }

            if (FrameCounter == -1) //error returned by the above API
            {
                Status = NvError_ParserFailure;
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_ERROR,
                    "Mp4CoreParserSetPosition : NvMp4ParserGetNextSyncUnit Failed! NvError=0x%08x", Status));
                goto cleanup;
            }
            else if (FrameCounter == NvMp4SyncUnitStatus_STSSENTRIES_DONE)
            {
                // Set the below flag as true
                pContext->IsFileEOF = NV_TRUE;
                // Update the pTimeStamp with total video duration
                (*pTimeStamp) = pContext->VideoDuration;
                // Update the postion with total duration
                pContext->CurrentTimeStamp = pContext->VideoDuration;;
                // Update the postion with total duration
                pContext->Position.Position = pContext->VideoDuration;
                pContext->Parser.FramingInfo[pContext->Parser.VideoIndex].FrameCounter = pContext->Parser.FramingInfo[pContext->Parser.VideoIndex].TotalNoFrames;
                pContext->Parser.FramingInfo[pContext->Parser.AudioIndex].FrameCounter = pContext->Parser.FramingInfo[pContext->Parser.AudioIndex].TotalNoFrames;
                NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                    "Mp4CoreParserSetPosition : NvMp4SyncUnitStatus_STSSENTRIES_DONE\n"));
                Status = NvSuccess;
                goto cleanup;
            }
            else if (FrameCounter != 0)
                FrameCounter--;//The actual I frame is one less than the frame returned by the getNextSyncUnit

            NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG,
                "Mp4CoreParserSetPosition : UpdatedFrame = %d - rate = %d - dir = %d\n",
                FrameCounter, rate, Direction));
            //Get the timestamps
            pContext->Parser.FramingInfo[pContext->Parser.VideoIndex].FrameCounter = FrameCounter;
            err = NvMp4ParserGetCurrentAccessUnit (&pContext->Parser, TrackIndex,
                NULL,
                &NumFilledLen,
                (NvU64*) & dts,
                (NvS32*) & cts,
                (NvU64*) & elst);
            ts = cts + dts;
            if (err != NvSuccess)
            {
                Status = NvError_ParserFailure;
                goto cleanup;
            }

            //Update the current time stamp
            //Calculate total time from media Time Scale
            if (MediaTimeScale != 0)
                *pTimeStamp = ( ( ( (NvU64) ts + elst) * TICKS_PER_SECOND) / MediaTimeScale) * 10;
            else
                *pTimeStamp = 0;


            if (TrackType == NvMp4TrackTypes_AVC)
            {
                pContext->ParserState[TrackIndex] = MP4PARSE_HEADER;
            }

            break;
        }
    }

    //Update the context position according to the pTimeStamp
    pContext->Position.Position = (*pTimeStamp);
    pContext->CurrentTimeStamp = (*pTimeStamp);

    for (TrackIndex = 0; TrackIndex < TotalTracks ; TrackIndex++)
    {

        TrackType = (NvMp4TrackTypes) NvMp4ParserGetTracksInfo (&pContext->Parser , TrackIndex) ;
        if (NVMP4_ISTRACKAUDIO (TrackType) && (TrackIndex == pContext->Parser.AudioIndex))
        {
            Count = NvMp4ParserGetTotalMediaSamples (&pContext->Parser , TrackIndex);
            MediaTimeScale = NvMp4ParserGetMediaTimeScale (&pContext->Parser , TrackIndex);
            StreamDuration = NvMp4ParserGetMediaDuration (&pContext->Parser , TrackIndex);
            if (MediaTimeScale != 0)
                TotalTime = ( (StreamDuration * TICKS_PER_SECOND) / MediaTimeScale) * 10;
            else
                TotalTime = 0;

            //check for EOF
            if ( (*pTimeStamp) >= pContext->AudioDuration)
            {
                //Go to Last frame
                pContext->Parser.FramingInfo[pContext->Parser.AudioIndex].FrameCounter = pContext->Parser.FramingInfo[pContext->Parser.AudioIndex].TotalNoFrames;
            }
            else if (TotalTime)
                pContext->Parser.FramingInfo[pContext->Parser.AudioIndex].FrameCounter = (NvU32) ( ( (*pTimeStamp) * (NvU64) Count) / TotalTime);

            break;
        }
    }

cleanup:
    if (pContext && pTimeStamp)
    {
        NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG,
            "Mp4CoreParserSetPosition : FrameCounter = %d, SeekedTimeStamp = %lld msec",
            FrameCounter, *pTimeStamp / 10000));

        pContext->Parser.SeekVideoFrameCounter = pContext->Parser.FramingInfo[pContext->Parser.AudioIndex].FrameCounter;
    }

    return Status;
}

static NvError
Mp4CoreParserGetNextWorkUnit (
                              NvMMParserCoreHandle hParserCore,
                              NvU32 StreamIndex,
                              NvMMBuffer *pBuffer,
                              NvU32 *pSize,
                              NvBool *pMoreWorkPending)
{
    NvError Status = NvSuccess;
    NvMMMp4ParserCoreContext* pContext = NULL;
    NvMMPayloadMetadata payload;
    NvS8 *TmpAVCBufPtr, *pHeader;
    NvS32  cts = 0;
    NvU64  dts  = 0, elst = 0, MediaTimeScale = 0, ts = 0;
    NvU32 NumFilledLen = 0;
    void *HeaderData = NULL;
    NvU32 HeaderSize = 0;
    NvU32 NALSize;
    NvAVCConfigData *pAVCConfigData;
    NvU32 ULength;
    NvU8 SamplingFreqIndex;
    NvMp4TrackTypes TrackType;
    NvS32 rate = 1000;
    NvMMAudioAacTrackInfo AACTrackInfo;
    NvMp4AacConfigurationData *pAACConfigData = NULL;

    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pBuffer && pMoreWorkPending);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;;
    NumFilledLen = pBuffer->Payload.Ref.sizeOfBufferInBytes;

    AACTrackInfo.objectType             = 0;
    AACTrackInfo.profile                = 0x40;
    AACTrackInfo.samplingFreqIndex      = 0;
    AACTrackInfo.samplingFreq           = 0;
    AACTrackInfo.noOfChannels           = 0;
    AACTrackInfo.sampleSize             = 0;
    AACTrackInfo.channelConfiguration   = 0;
    AACTrackInfo.bitRate                = 0;

    // Initially pMoreWorkPending is set to FALSE
    *pMoreWorkPending = NV_FALSE;

    if (pContext == NULL)
    {
        return NvError_ParserFailure;
    }

    if (hParserCore->UlpKpiMode)
    {
        // End of Idle Time and Start of Parse time
        NvmmUlpUpdateKpis ( (KpiFlag_IdleEnd | KpiFlag_ParseStart), NV_FALSE, NV_FALSE, 0);
    }

    TrackType = NvMp4ParserGetTracksInfo (&pContext->Parser, StreamIndex);
    pAACConfigData = &pContext->Parser.AACConfigData;

    if (TrackType == NvMp4TrackTypes_AVC)
    {
        if (NvSuccess != NvMp4ParserGetAVCConfigData(&pContext->Parser,
                StreamIndex, &pAVCConfigData))
        {
            return NvError_ParserFailure;
        }
        /*pAVCConfigData points to the corresponding avcC*/
        if(pContext->Parser.pCurAVCConfigData != pAVCConfigData)
        {
            /*need to send new avcC, change parser state to MP4PARSE_HEADER*/
            /*A NvMMBuffer will be filled and HandleReturnFromGetNextWU() will send*/
            pContext->ParserState[StreamIndex] = MP4PARSE_HEADER;
            pContext->Parser.pCurAVCConfigData = pAVCConfigData;
        }
    }

    switch (pContext->ParserState[StreamIndex])
    {
    case MP4PARSE_DATA:
        rate = Mp4CoreParserGetRate (hParserCore);
        pContext->Parser.rate  = rate;

        if (hParserCore->bEnableUlpMode == NV_TRUE)
        {
            if(StreamIndex == pContext->Parser.AudioIndex)
            {
                Status = NvMp4ParserGetMultipleFrames (&pContext->Parser,
                    StreamIndex,
                    pBuffer,
                    hParserCore->vBufferMgrBaseAddress,
                    hParserCore->pBufferMgrBaseAddress);
                //Adding error check for DRM case, not sure why other error's are not handled here
                if (Status == NvError_ParserDRMFailure)
                {
                    Status = pContext->Parser.DrmError;;
                    hParserCore->domainName = NvMMErrorDomain_DRM;
                    return Status;
                }
                *pSize = sizeof (NvMMOffsetList);
            }
        }
        else
        {
            Status = NvMp4ParserGetNextAccessUnit (&pContext->Parser, StreamIndex ,
                (NvS8 *) pBuffer->Payload.Ref.pMem,
                &NumFilledLen,
                (NvU64*) & dts,
                (NvS32*) & cts,
                (NvU64*) & elst);
            //Adding error check for DRM case, not sure why other error's are not handled here
            if (Status == NvError_ParserDRMFailure)
            {
                Status = pContext->Parser.DrmError;
                hParserCore->domainName = NvMMErrorDomain_DRM;
                return Status;
            }
            *pSize = NumFilledLen;
            MediaTimeScale = NvMp4ParserGetMediaTimeScale (&pContext->Parser, StreamIndex);
            ts = cts + dts;
            pBuffer->Payload.Ref.sizeOfValidDataInBytes = NumFilledLen;

            if (MediaTimeScale != 0)
                pBuffer->PayloadInfo.TimeStamp = (NvU64) ( ( ( ( (NvU64) ts + elst) * TICKS_PER_SECOND * 10) / MediaTimeScale));
            else
                pBuffer->PayloadInfo.TimeStamp = 0;

            if (NVMP4_ISTRACKVIDEO (TrackType) && (StreamIndex == pContext->Parser.VideoIndex))
                pContext->CurrentTimeStamp = pBuffer->PayloadInfo.TimeStamp;

            if (pContext->Parser.TempCount < 10)
            {
                if ( (TrackType == NvMp4TrackTypes_AVC) || (TrackType == NvMp4TrackTypes_MPEG4) || (TrackType == NvMp4TrackTypes_S263))
                {
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "VIDEO:vfc = %d -  ct= %d - dts = %lld - TSMsec = %lld\n", 
                        pContext->Parser.FramingInfo[pContext->Parser.VideoIndex].FrameCounter, cts, dts, pBuffer->PayloadInfo.TimeStamp / 10000));
                }
                else if ( (TrackType == NvMp4TrackTypes_AAC) || (TrackType == NvMp4TrackTypes_AACSBR))
                {
                    NV_LOGGER_PRINT ((NVLOG_MP4_PARSER, NVLOG_DEBUG, "AUDIO:afc = %d - ct= %d - dts = %lld - TSMsec = %lld\n", 
                        pContext->Parser.FramingInfo[pContext->Parser.AudioIndex].FrameCounter, cts, dts, pBuffer->PayloadInfo.TimeStamp / 10000));
                }
                pContext->Parser.TempCount++;
            }

            pBuffer->Payload.Ref.startOfValidData = 0;
        }

        if (TrackType == NvMp4TrackTypes_AVC)
        {
            NALSize = 0;
            NvMp4ParserGetNALSize (&pContext->Parser, StreamIndex , &NALSize);
            pBuffer->PayloadInfo.BufferMetaDataType = NvMBufferMetadataType_H264;
            pBuffer->PayloadInfo.BufferMetaData.H264BufferMetadata.NALUSizeFieldWidthInBytes = NALSize + 1;
            pBuffer->PayloadInfo.BufferMetaData.H264BufferMetadata.BufferFormat = NvH264BufferFormat_RTPStreamFormat;
        }
        break;

    case MP4PARSE_HEADER:
        if (TrackType == NvMp4TrackTypes_AVC)
        {
            if (hParserCore->bEnableUlpMode == NV_TRUE)
            {
                /*pAVCConfigData is assigned before switch() statement*/
                HeaderSize          = pAVCConfigData->SeqParamSetLength[0] + pAVCConfigData->PicParamSetLength[0] + 8;
                pHeader = (NvS8 *) NvOsAlloc (HeaderSize);

                if (pHeader == NULL)
                {
                    Status = NvError_InsufficientMemory;
                }
                else
                {
                    TmpAVCBufPtr    = (NvS8 *) pHeader;
                    ULength = pAVCConfigData->SeqParamSetLength[0];
                    TmpAVCBufPtr[0] = (NvU8) ( (ULength >> 24) & 0xFF);
                    TmpAVCBufPtr[1] = (NvU8) ( (ULength >> 16) & 0xFF);
                    TmpAVCBufPtr[2] = (NvU8) ( (ULength >> 8) & 0xFF);
                    TmpAVCBufPtr[3] = (NvU8) ( (ULength) & 0xFF);
                    NvOsMemcpy (TmpAVCBufPtr + 4, pAVCConfigData->SpsNALUnit[0], ULength);
                    TmpAVCBufPtr    += ULength + 4;
                    ULength             = pAVCConfigData->PicParamSetLength[0];
                    TmpAVCBufPtr[0] = (NvU8) ( (ULength >> 24) & 0xFF);
                    TmpAVCBufPtr[1] = (NvU8) ( (ULength >> 16) & 0xFF);
                    TmpAVCBufPtr[2] = (NvU8) ( (ULength >> 8) & 0xFF);
                    TmpAVCBufPtr[3] = (NvU8) ( (ULength) & 0xFF);
                    NvOsMemcpy (TmpAVCBufPtr + 4, pAVCConfigData->PpsNALUnit[0], ULength);

                    if (!pContext->hVideoHeaderMemHandle)
                    {
                        Status = Mp4AllocateHeapMemory (hParserCore->hRmDevice,
                            &pContext->hVideoHeaderMemHandle,
                            &pContext->pVideoHeaderAddress,
                            &pContext->vVideoHeaderAddress,
                            HeaderSize);
                    }

                    if (Status != NvSuccess)
                    {
                        if (pHeader)
                            NvOsFree (pHeader);
                        Status = NvError_InsufficientMemory;
                    }
                    else
                    {
                        NvRmMemWrite (pContext->hVideoHeaderMemHandle, 0, (NvU8*) (pHeader), HeaderSize);
                        if (pHeader)
                            NvOsFree (pHeader);

                        payload.BufferFlags = 0;
                        payload.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
                        payload.TimeStamp = 0;
                        pBuffer->PayloadInfo.BufferFlags = NvMMBufferFlag_OffsetList;
                        NvmmSetBaseAddress (pContext->pVideoHeaderAddress, pContext->vVideoHeaderAddress, pBuffer);
                        NvmmPushToOffsetList (pBuffer,
                            pContext->vVideoHeaderAddress,
                            HeaderSize,
                            payload);
                        // Setting False in case of header offsetlist.
                        NvmmSetOffsetListStatus (pBuffer, NV_FALSE);
                        *pSize = sizeof (NvMMOffsetList);
                        pContext->VideoHeaderSize = HeaderSize;
                    }

                }

            }
            else
            {
                TmpAVCBufPtr    = (NvS8 *) pBuffer->Payload.Ref.pMem;
                /*pAVCConfigData is assigned before switch() statement*/
                ULength = pAVCConfigData->SeqParamSetLength[0];
                TmpAVCBufPtr[0] = (NvU8) ( (ULength >> 24) & 0xFF);
                TmpAVCBufPtr[1] = (NvU8) ( (ULength >> 16) & 0xFF);
                TmpAVCBufPtr[2] = (NvU8) ( (ULength >> 8) & 0xFF);
                TmpAVCBufPtr[3] = (NvU8) ( (ULength) & 0xFF);
                NvOsMemcpy (TmpAVCBufPtr + 4, pAVCConfigData->SpsNALUnit[0], ULength);
                TmpAVCBufPtr    += ULength + 4;
                ULength             = pAVCConfigData->PicParamSetLength[0];
                TmpAVCBufPtr[0] = (NvU8) ( (ULength >> 24) & 0xFF);
                TmpAVCBufPtr[1] = (NvU8) ( (ULength >> 16) & 0xFF);
                TmpAVCBufPtr[2] = (NvU8) ( (ULength >> 8) & 0xFF);
                TmpAVCBufPtr[3] = (NvU8) ( (ULength) & 0xFF);
                NvOsMemcpy (TmpAVCBufPtr + 4, pAVCConfigData->PpsNALUnit[0], ULength);
                HeaderSize          = pAVCConfigData->SeqParamSetLength[0] + pAVCConfigData->PicParamSetLength[0] + 8;
            }

            pBuffer->PayloadInfo.BufferMetaDataType = NvMBufferMetadataType_H264;
            pBuffer->PayloadInfo.BufferMetaData.H264BufferMetadata.NALUSizeFieldWidthInBytes = 4;
            pBuffer->PayloadInfo.BufferMetaData.H264BufferMetadata.BufferFormat = NvH264BufferFormat_RTPStreamFormat;

        }
        else if (TrackType == NvMp4TrackTypes_S263)
        {
            HeaderSize = 0;
        }
        else if ( (TrackType == NvMp4TrackTypes_AAC) || (TrackType == NvMp4TrackTypes_AACSBR))
        {
            HeaderData = NvMp4ParserGetDecConfig (&pContext->Parser, StreamIndex, &HeaderSize);
            AACTrackInfo.objectType = pAACConfigData->ObjectType;
            SamplingFreqIndex = pAACConfigData->SamplingFreqIndex;
            AACTrackInfo.samplingFreqIndex = SamplingFreqIndex;
            AACTrackInfo.samplingFreq = pAACConfigData->SamplingFreq;
            AACTrackInfo.channelConfiguration = pAACConfigData->ChannelConfiguration;
            AACTrackInfo.bitRate = pContext->Parser.TrackInfo[StreamIndex].AvgBitRate;
            AACTrackInfo.noOfChannels = pAACConfigData->ChannelConfiguration;
            if (hParserCore->bEnableUlpMode == NV_TRUE)
            {
                if (!pContext->hAudioHeaderMemHandle)
                {
                    Status = Mp4AllocateHeapMemory (hParserCore->hRmDevice,
                        &pContext->hAudioHeaderMemHandle,
                        &pContext->pAudioHeaderAddress,
                        &pContext->vAudioHeaderAddress,
                        sizeof (NvMMAudioAacTrackInfo));
                }
                if (Status != NvSuccess)
                {
                    Status = NvError_InsufficientMemory;
                }

                NvRmMemWrite (pContext->hAudioHeaderMemHandle, 0, (NvU8*) (&AACTrackInfo), sizeof (NvMMAudioAacTrackInfo));
                payload.BufferFlags = 0;
                payload.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
                // Enable Gapless for Audio only case
                payload.BufferFlags |= NvMMBufferFlag_EnableGapless;
                payload.TimeStamp = 0;
                pBuffer->PayloadInfo.BufferFlags = NvMMBufferFlag_OffsetList;
                NvmmSetBaseAddress (pContext->pAudioHeaderAddress, pContext->vAudioHeaderAddress, pBuffer);

                NvmmPushToOffsetList (pBuffer,
                    pContext->vAudioHeaderAddress,
                    sizeof (NvMMAudioAacTrackInfo),
                    payload);
                // Setting False in case of header offsetlist.
                NvmmSetOffsetListStatus (pBuffer, NV_FALSE);
                *pSize = sizeof (NvMMOffsetList);
            }
            else
            {
                AACTrackInfo.HasDRM = pContext->Parser.IsSINF;
                AACTrackInfo.PolicyType = pContext->MeteringPolicyType;
                AACTrackInfo.MeteringTime = pContext->MeteringTime;
                AACTrackInfo.LicenseConsumeTime = pContext->LicenseConsumeTime;
                AACTrackInfo.DrmContext = (NvU32) pContext->Parser.pDrmContext;
                AACTrackInfo.CoreContext = pContext->CoreContext;
                NvOsMemcpy (pBuffer->Payload.Ref.pMem, (NvU8*) (&AACTrackInfo), sizeof (NvMMAudioAacTrackInfo));
            }
            HeaderSize = sizeof (NvMMAudioAacTrackInfo);
        }
        else
        {
            HeaderData = NvMp4ParserGetDecConfig (&pContext->Parser, StreamIndex, &HeaderSize);
            if (hParserCore->bEnableUlpMode == NV_TRUE)
            {
                if (TrackType == NvMp4TrackTypes_MPEG4)
                {
                    Status = Mp4AllocateHeapMemory (hParserCore->hRmDevice,
                        &pContext->hVideoHeaderMemHandle,
                        &pContext->pVideoHeaderAddress,
                        &pContext->vVideoHeaderAddress,
                        HeaderSize);

                    if (Status != NvSuccess)
                    {
                        Status = NvError_InsufficientMemory;
                    }
                    NvRmMemWrite (pContext->hVideoHeaderMemHandle, 0, (NvU8*) (HeaderData), HeaderSize);

                    payload.BufferFlags = 0;
                    payload.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
                    payload.TimeStamp = 0;
                    pBuffer->PayloadInfo.BufferFlags = NvMMBufferFlag_OffsetList;
                    NvmmSetBaseAddress (pContext->pVideoHeaderAddress, pContext->vVideoHeaderAddress, pBuffer);

                    NvmmPushToOffsetList (pBuffer,
                        pContext->vVideoHeaderAddress,
                        HeaderSize,
                        payload);
                    // Setting False in case of header offsetlist.
                    NvmmSetOffsetListStatus (pBuffer, NV_FALSE);
                    *pSize = sizeof (NvMMOffsetList);
                    pContext->VideoHeaderSize = HeaderSize;
                }
            }
            else
                NvOsMemcpy (pBuffer->Payload.Ref.pMem, (NvU8*) (HeaderData), HeaderSize);
        }

        if (hParserCore->bEnableUlpMode == NV_FALSE)
        {
            pBuffer->Payload.Ref.sizeOfValidDataInBytes = HeaderSize;
            pBuffer->Payload.Ref.startOfValidData = 0;
            pBuffer->PayloadInfo.TimeStamp = 0;
            pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_HeaderInfoFlag;
            *pSize = HeaderSize;
        }
        pContext->ParserState[StreamIndex] = MP4PARSE_DATA;
        break;

    case MP4PARSE_INIT:
    default:
        Status = NvError_ParserFailedToGetData; // error we are trying to read data before setting the track
        break;
    }

    if (hParserCore->UlpKpiMode)
    {
        // End of Parse Time and Start of Idle time
        NvmmUlpUpdateKpis ( (KpiFlag_IdleStart | KpiFlag_ParseEnd), NV_FALSE, NV_FALSE, 0);
    }

cleanup:
    return Status;
}

static NvError
Mp4CoreParserGetAttribute (
                           NvMMParserCoreHandle hParserCore,
                           NvU32 AttributeType,
                           NvU32 AttributeSize,
                           void *pAttribute)
{
    NvError Status = NvSuccess;
    NvMMMp4ParserCoreContext *pContext = NULL;
    NvU32 TrackIndex =0, TotalTracks = 0, AVCIndex = 0, MetadataSize = 0;
    NvMp4TrackTypes TrackType;
    NvU32 NALSize, *TempNALSize;
    NvMMMetaDataInfo *md;
    NvS8 *MetadataPtr = NULL;
    CPint64 FilePos;
    NvU64  TempOffset;
    NvU32 URLLength = 0;
    NvMMMetaDataCharSet *MetaEncoding = NULL;


    NVMM_CHK_ARG (hParserCore && hParserCore->pContext && pAttribute);
    pContext = (NvMMMp4ParserCoreContext *) hParserCore->pContext;

    switch (AttributeType)
    {
        // Specifically related to H264 content.
    case NvMMVideoDecAttribute_H264NALLen:
        {
            TotalTracks = NvMp4ParserGetNumTracks (&pContext->Parser);
            TempNALSize = (NvU32*) pAttribute;
            for (TrackIndex = 0; TrackIndex < TotalTracks; TrackIndex++)
            {
                TrackType = (NvMp4TrackTypes) NvMp4ParserGetTracksInfo (&pContext->Parser, TrackIndex);
                switch (TrackType)
                {
                case NvMp4TrackTypes_AVC:
                    AVCIndex = TrackIndex;
                    break;
                default:
                    break;
                }
            }
            NVMM_CHK_ERR (NvMp4ParserGetNALSize (&pContext->Parser, AVCIndex, &NALSize));
            *TempNALSize = (NALSize + 1);
        }
        break;
    case NvMMAttribute_FileName:
        {
            NvMMAttrib_ParseUri *EmbeddedURI=(NvMMAttrib_ParseUri*)pAttribute;

            if(pContext->Parser.EmbeddedURL)
            {
                URLLength = NvOsStrlen(pContext->Parser.EmbeddedURL);
                EmbeddedURI->szURI = NvOsAlloc(sizeof(NvU8)* (URLLength + 1 ));
                NvOsMemset( EmbeddedURI->szURI,0, sizeof(NvU8) * (URLLength + 1));
                NvOsStrncpy (EmbeddedURI->szURI,pContext->Parser.EmbeddedURL,URLLength + 1);
            }
            else
            {
                URLLength = NvOsStrlen(pContext->Parser.URIList[0]);
                EmbeddedURI->szURI = NvOsAlloc(sizeof(NvU8)* (URLLength + 1 ));
                NvOsMemset( EmbeddedURI->szURI,0, sizeof(NvU8) * (URLLength + 1));
                NvOsStrncpy (EmbeddedURI->szURI,pContext->Parser.URIList[0],URLLength + 1);
            }
        }
        break;
    case NvMMAttribute_Metadata:
        {
            md = (NvMMMetaDataInfo *) pAttribute;

            switch (md->eMetadataType)
            {
            case NvMMMetaDataInfo_Artist:
                MetadataPtr = & (pContext->Parser.MediaMetadata.Artist[0]);
                MetadataSize = pContext->Parser.MediaMetadata.ArtistSize;
                MetaEncoding = &pContext->Parser.MediaMetadata.ArtistEncoding;
                break;
            case NvMMMetaDataInfo_AlbumArtist:
                MetadataPtr = & (pContext->Parser.MediaMetadata.AlbumArtist[0]);
                MetadataSize = pContext->Parser.MediaMetadata.AlbumArtistSize;
                MetaEncoding = &pContext->Parser.MediaMetadata.AlbumArtistEncoding;
                break;
            case NvMMMetaDataInfo_Album:
                MetadataPtr = & (pContext->Parser.MediaMetadata.Album[0]);
                MetadataSize = pContext->Parser.MediaMetadata.AlbumSize;
                MetaEncoding = &pContext->Parser.MediaMetadata.AlbumEncoding;
                break;
            case NvMMMetaDataInfo_Genre:
                MetadataPtr = & (pContext->Parser.MediaMetadata.Genre[0]);
                MetadataSize = pContext->Parser.MediaMetadata.GenreSize;
                MetaEncoding = &pContext->Parser.MediaMetadata.GenreEncoding;
                break;
            case NvMMMetaDataInfo_Title:
                MetadataPtr = & (pContext->Parser.MediaMetadata.Title[0]);
                MetadataSize = pContext->Parser.MediaMetadata.TitleSize;
                MetaEncoding = &pContext->Parser.MediaMetadata.TitleEncoding;
                break;
            case NvMMMetaDataInfo_Year:
                MetadataPtr = & (pContext->Parser.MediaMetadata.Year[0]);
                MetadataSize = pContext->Parser.MediaMetadata.YearSize;
                MetaEncoding = &pContext->Parser.MediaMetadata.YearEncoding;
                break;
            case NvMMMetaDataInfo_Composer:
                MetadataPtr = & (pContext->Parser.MediaMetadata.Composer[0]);
                MetadataSize = pContext->Parser.MediaMetadata.ComposerSize;
                MetaEncoding = &pContext->Parser.MediaMetadata.ComposerEncoding;
                break;
            case NvMMMetaDataInfo_Copyright:
                MetadataPtr = & (pContext->Parser.MediaMetadata.Copyright[0]);
                MetadataSize = pContext->Parser.MediaMetadata.CopyrightSize;
                MetaEncoding = &pContext->Parser.MediaMetadata.CopyrightEncoding;
                break;
            case NvMMMetaDataInfo_ThumbnailSeekTime:
                {
                    if(pContext->Parser.TrackInfo[pContext->Parser.VideoIndex].IsValidSeekTable == NV_FALSE)
                        return NvError_UnSupportedMetadata;

                    if (md->pClientBuffer == NULL || (md->nBufferSize < sizeof (NvU64)))
                    {
                        md->nBufferSize = sizeof (NvU64);
                        return NvError_InSufficientBufferSize;
                    }
                    NvOsMemcpy (md->pClientBuffer, & (pContext->Parser.TrackInfo[pContext->Parser.VideoIndex].LargestIFrameTimestamp), sizeof (NvU64));
                    md->nBufferSize = sizeof (NvU64);
                    md->eEncodeType = NvMMMetaDataEncode_NvU64;
                    return NvSuccess;
                }
            case NvMMMetaDataInfo_TrackNumber:
                if (pContext->Parser.MediaMetadata.TrackNumber == MP4_INVALID_NUMBER)
                    return NvError_UnSupportedMetadata;
                if (md->pClientBuffer == NULL || (md->nBufferSize < sizeof (NvU32)))
                {
                    md->nBufferSize = sizeof (NvU32);
                    return NvError_InSufficientBufferSize;
                }

                NvOsMemcpy (md->pClientBuffer, & (pContext->Parser.MediaMetadata.TrackNumber), sizeof (NvU32));
                md->nBufferSize = sizeof (NvU32);
                md->eEncodeType = NvMMMetaDataEncode_NvU32;
                return NvSuccess;
            case NvMMMetaDataInfo_CoverArt:
                {
                    NvU32 nSize = pContext->Parser.MediaMetadata.CoverArtSize;
                    if (nSize)
                    {
                        if (md->pClientBuffer == NULL || (md->nBufferSize < nSize))
                        {
                            md->nBufferSize = nSize;
                            return NvError_InSufficientBufferSize;
                        }
                        NVMM_CHK_ERR (pContext->Parser.pPipe->GetPosition64 (pContext->Parser.hContentPipe, (CPuint64 *) &FilePos));
                        TempOffset = FilePos;
                        NVMM_CHK_ERR (NvMp4ParserGetCoverArt (&pContext->Parser, (void *) md->pClientBuffer));
                        NVMM_CHK_ERR (pContext->Parser.pPipe->SetPosition64 (pContext->Parser.hContentPipe, (CPint64) TempOffset, CP_OriginBegin));

                        if (NvOsMemcmp (md->pClientBuffer, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0)
                        {
                            md->eEncodeType = NvMMMetaDataEncode_PNG;
                        }
                        else if (NvOsMemcmp (md->pClientBuffer, "\xFF\xD8\xFF\xE0", 4) == 0 ||
                            NvOsMemcmp (md->pClientBuffer, "\xFF\xD8\xFF\xE1", 4) == 0)
                        {
                            md->eEncodeType = NvMMMetaDataEncode_JPEG;
                        }
                        else
                            md->eEncodeType = NvMMMetaDataEncode_Other;
                    }
                    else
                        return NvError_UnSupportedMetadata; // no coverart
                    md->nBufferSize = nSize;
                    return NvSuccess;
                }
            default:
                return NvError_UnSupportedMetadata;
            }

            if (!MetadataPtr)
                return NvError_UnSupportedMetadata;

            if (md->pClientBuffer == NULL || (md->nBufferSize < MetadataSize))
            {
                md->nBufferSize = MetadataSize;
                return NvError_InSufficientBufferSize;
            }
            NvOsMemcpy (md->pClientBuffer, MetadataPtr, MetadataSize);
            md->nBufferSize = MetadataSize;
            md->eEncodeType = *MetaEncoding;


            break;
        }
    default:
        break;
    }

cleanup:
    return Status;
}


static void
Mp4CoreParserGetMaxOffsets (
                            NvMMParserCoreHandle hParserCore,
                            NvU32 *pMaxOffsets)
{
    if (pMaxOffsets)
    {
        *pMaxOffsets = MAX_OFFSETS;
    }
}

// TODO: These headers should go away by replacing strrchr API with appropriate implementation
#include <stdio.h> 
#include <string.h>
static NvError
Mp4CoreParserGetNextRMRAStream (
                                NvMMMp4ParserCoreContext *pContext,
                                NvU32 Index)
{
    NvError Status = NvSuccess;
    NvString pRefURL = NULL;
    NvString pRefPtr = NULL;
    NvU32 URLSize = 0;

    if (pContext->Parser.URIList[Index][0] == '/' || pContext->Parser.URIList[Index][0] == '\\' ||
        !NvOsStrncmp ( (char*) pContext->Parser.URIList[Index], "http://",NvOsStrlen ( "http://")) ||
        !NvOsStrncmp ( (char*) pContext->Parser.URIList[Index], "rtsp://",NvOsStrlen ( "rtsp://")))
    {
        // absolute, just use it
        pRefURL = NvOsAlloc (NvOsStrlen ( (char*) pContext->Parser.URIList[Index]) + 1);
        NVMM_CHK_MEM (pRefURL);
        NvOsMemset ( (char*) pRefURL, 0, NvOsStrlen ( (char*) pContext->Parser.URIList[Index]) + 1);
        NvOsStrncpy ( (char*) pRefURL, (char*) pContext->Parser.URIList[Index],
            NvOsStrlen ( (char*) pContext->Parser.URIList[Index]) + 1);
    }
    else
    {
        URLSize = NvOsStrlen (pContext->Parser.URIList[0]) +
            NvOsStrlen ( (char*) pContext->Parser.URIList[Index]) + 1;
        pRefURL = NvOsAlloc (URLSize);
        NVMM_CHK_MEM (pRefURL);
        NvOsMemset (pRefURL, 0, URLSize);

        NvOsStrncpy (pRefURL, pContext->Parser.URIList[0],
            NvOsStrlen (pContext->Parser.URIList[0]) + 1);
        pRefPtr = strrchr (pRefURL, '/');
        if (!pRefPtr)
        {
            pRefPtr = strrchr (pRefURL, '\\');
            if (!pRefPtr)
                pRefPtr = pRefURL;
            else
                pRefPtr++;
        }
        else
            pRefPtr++;

        NvOsStrncpy (pRefPtr, (char*) pContext->Parser.URIList[Index],
            NvOsStrlen ( (char*) pContext->Parser.URIList[Index]) + 1);
    }
    pContext->Parser.URIList[0] = pRefURL;

cleanup:
    return Status;
}


NvError
NvMMCreateMp4CoreParser (
                         NvMMParserCoreHandle *hParserCore,
                         NvMMParserCoreCreationParameters *pParams)
{
    NvError Status = NvSuccess;
    NvMMParserCore *pParserCore = NULL;

    NVMM_CHK_ARG (hParserCore && pParams);

    pParserCore = NvOsAlloc (sizeof (NvMMParserCore));
    NVMM_CHK_MEM (pParserCore);
    NvOsMemset (pParserCore, 0, sizeof (NvMMParserCore));

    NVMM_CHK_ERR (NvRmOpen (&pParserCore->hRmDevice, 0));

    pParserCore->Open               = Mp4CoreParserOpen;
    pParserCore->GetNumberOfStreams = Mp4CoreParserGetNumberOfStreams;
    pParserCore->GetStreamInfo      = Mp4CoreParserGetStreamInfo;
    pParserCore->SetRate            = Mp4CoreParserSetRate;
    pParserCore->GetRate            = Mp4CoreParserGetRate;
    pParserCore->SetPosition        = Mp4CoreParserSetPosition;
    pParserCore->GetPosition        = Mp4CoreParserGetPosition;
    pParserCore->GetNextWorkUnit    = Mp4CoreParserGetNextWorkUnit;
    pParserCore->commitDrm = Mp4CoreParserDrmCommit;
    pParserCore->Close              = Mp4CoreParserClose;
    pParserCore->GetMaxOffsets      = Mp4CoreParserGetMaxOffsets;
    pParserCore->GetAttribute       = Mp4CoreParserGetAttribute;
    pParserCore->GetBufferRequirements = Mp4CoreParserGetBufferRequirements;
    pParserCore->eCoreType          = NvMMParserCoreType_Mp4Parser;
    pParserCore->UlpKpiMode         = pParams->UlpKpiMode;
    pParserCore->bEnableUlpMode     = pParams->bEnableUlpMode;
    pParserCore->bReduceVideoBuffers = pParams->bReduceVideoBuffers;
    *hParserCore = pParserCore;

cleanup:
    if (Status != NvSuccess)
    {
        if (pParserCore)
        {
            NvOsFree (pParserCore);
        }
        *hParserCore = NULL;
    }

    return Status;
}

void NvMMDestroyMp4CoreParser (NvMMParserCoreHandle hParserCore)
{
    if (hParserCore)
    {
        if (hParserCore->hRmDevice)
        {
            NvRmClose (hParserCore->hRmDevice);
        }        
        NvOsFree (hParserCore);
    }
}

