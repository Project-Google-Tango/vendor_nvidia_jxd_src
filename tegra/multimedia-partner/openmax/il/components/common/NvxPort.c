/* Copyright (c) 2006-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NvxPort.c */

#include "NvxPort.h"
#include "NvxError.h"
#include <stdlib.h>
#include "NvxComponent.h"

#include "nvos.h"
#include "NvxTrace.h"

#define OMX_TIMEOUT_MS 5

static OMX_ERRORTYPE NvxPortBufferPtrToIdx(NvxPort *pThis, OMX_U8 *pBuf, OMX_U32 *pIdx)
{
    OMX_U32 i;
    *pIdx = 0xffffffff;
    for (i=0;i<pThis->nReqBufferCount;i++) 
    {
        if (pBuf == pThis->ppBufferHdrs[i]->pBuffer) 
        {
            *pIdx = i;
            return OMX_ErrorNone;
        }
    }
    return OMX_ErrorUndefined;
}

static void NvxPortCommonInit(NvxPort *pThis, OMX_DIRTYPE eDir, OMX_U32 nBufferCount, OMX_U32 nBufferSize,
                              OMX_PORTDOMAINTYPE ePortDomain )
{
    /* Note: no memset 0 necessary because NvxComponent does this on allocation */
    /* pThis->pNvComp and pThis->hMutex set by NvxComponent */
    pThis->nBuffers         = 0;
    pThis->ppBufferHdrs     = NULL; /* filled in when we alloc resources */
    pThis->ppBuffers        = NULL; /* filled in when we alloc resources */
    pThis->pbMySharedBuffer = NULL; /* filled in when we alloc resources */
    pThis->nReqBufferCount  = nBufferCount;
    pThis->nReqBufferSize   = nBufferSize;
    pThis->nMinBufferCount  = nBufferCount;  /* assume can't go below initial count */
    pThis->nMinBufferSize   = nBufferSize;  /* assume can't go below initial count */
    pThis->nMaxBufferCount  = 0x10000;   /* default max buffer count: 64K*/
    pThis->nMaxBufferSize   = 0x2000000; /* default max buffer size: 32M */
    pThis->nNonTunneledBufferSize = 0;
    pThis->eSupplierPreference = OMX_BufferSupplyUnspecified;
    pThis->eSupplierSetting = OMX_BufferSupplyUnspecified;
    pThis->eSupplierTieBreaker = NVX_SUPPLIERTIEBREAKER; 
    pThis->nSharingCandidates = 0;  
    pThis->nSharingPorts      = 0;       
    pThis->bShareIfNvidiaTunnel = OMX_FALSE;
    pThis->bStarted = OMX_FALSE;
    pThis->bIsStopping = OMX_FALSE;
    pThis->pBuffersToSend = NULL;
 
    /* stuff common to all port definitions */
    pThis->oPortDef.nSize               = sizeof(pThis->oPortDef);
    pThis->oPortDef.nVersion.nVersion   = NvxComponentSpecVersion.nVersion;
    /* pThis->oPortDef.nPortIndex set by NvxComponent */
    pThis->oPortDef.eDir                = eDir;
    pThis->oPortDef.eDomain             = ePortDomain;
    pThis->oPortDef.nBufferCountMin     = nBufferCount;
    pThis->oPortDef.nBufferCountActual  = nBufferCount;
    pThis->oPortDef.nBufferSize         = nBufferSize;
    pThis->oPortDef.bEnabled            = OMX_TRUE;
    pThis->oPortDef.bPopulated          = OMX_FALSE;
    NvOsMemset(pThis->ppRmSurf, 0, sizeof(pThis->ppRmSurf));
}

OMX_ERRORTYPE NvxPortSetNonTunneledSize(NvxPort *pPort, OMX_U32 nBufSize)
{
    pPort->nNonTunneledBufferSize = nBufSize;
    pPort->nReqBufferSize = nBufSize;
    pPort->oPortDef.nBufferSize = nBufSize;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxPortInitOther(NvxPort *pThis, OMX_DIRTYPE eDir, OMX_U32 nBufferCount, OMX_U32 nBufferSize,
                               OMX_OTHER_FORMATTYPE eFormat)
{
    NvxPortCommonInit(pThis, eDir, nBufferCount, nBufferSize, OMX_PortDomainOther);
    pThis->oPortDef.format.other.eFormat = eFormat;
    return OMX_ErrorNone;
}

#define ARRAY_MAX(_X_) (sizeof(_X_)/sizeof(_X_[0]))

/* NOTE: this table is only an approximation.  In some cases, a different mime type should be used based on parameters to the codec */
static char *s_pszAudioMimeTypes[] = {
    "application/octet-stream",  /**< Placeholder value when coding is N/A  */
    "audio/x-PCM",          /**< Any variant of PCM coding */
    /* should be L16 (big endian 16 bit signed) or L8 or WAV ? */
    "audio/x-adpcm",        /**< Any variant of ADPCM encoded data */
    "audio/AMR",            /**< Any variant of AMR encoded data */
    "audio/GSM",            /**< Any variant of GSM fullrate (i.e. GSM610) */
    "audio/GSM-EFR",        /**< Any variant of GSM Enhanced Fullrate encoded data*/
    "audio/GSM-HR",         /**< Any variant of GSM Halfrate encoded data */
    "audio/x-PDC-FR",       /**< Any variant of PDC Fullrate encoded data */
    "audio/AMR",            /**< Any variant of PDC Enhanced Fullrate encoded data */
    "audio/x-PDC-HR",       /**< Any variant of PDC Halfrate encoded data */
    "audio/x-TDMA-FR",      /**< Any variant of TDMA Fullrate encoded data (TIA/EIA-136-420) */
    "audio/x-TDMA-EFR",     /**< Any variant of TDMA Enhanced Fullrate encoded data (TIA/EIA-136-410) */
    "audio/qcelp",          /**< Any variant of QCELP 8kbps encoded data */
    "audio/qcelp",          /**< Any variant of QCELP 13kbps encoded data */
    "audio/EVRC",           /**< Any variant of EVRC encoded data */
    "audio/SMV",            /**< Any variant of SMV encoded data */
    "audio/g711",           /**< Any variant of G.711 encoded data */
    "audio/G723",           /**< Any variant of G.723 dot 1 encoded data */
    "audio/G726-32",        /**< Any variant of G.726 encoded data */
    "audio/G729",           /**< Any variant of G.729 encoded data */
    "audio/mpeg4-generic",  /**< Any variant of AAC encoded data */
    "audio/mpeg",           /**< Any variant of MP3 encoded data */
    "audio/x-SBC",          /**< Any variant of SBC encoded data */
    "audio/x-VORBIS",       /**< Any variant of VORBIS encoded data */
    "audio/x-ms-wma",       /**< Any variant of WMA encoded data */
    "audio/x-pn-realaudio", /**< Any variant of RA encoded data */
    "audio/x-midi"          /**< Any variant of MIDI encoded data */
};

OMX_ERRORTYPE NvxPortInitAudio(NvxPort *pThis, OMX_DIRTYPE eDir, OMX_U32 nBufferCount, OMX_U32 nBufferSize,
                               OMX_AUDIO_CODINGTYPE eCodingType)
{
    NvxPortCommonInit(pThis, eDir, nBufferCount, nBufferSize, OMX_PortDomainAudio);
    pThis->oPortDef.format.audio.eEncoding = eCodingType;
    if ((int)eCodingType < 0 || eCodingType >= ARRAY_MAX(s_pszAudioMimeTypes))
        pThis->oPortDef.format.audio.cMIMEType = s_pszAudioMimeTypes[0];
    else
        pThis->oPortDef.format.audio.cMIMEType = s_pszAudioMimeTypes[eCodingType];
    return OMX_ErrorNone;
}

static char *s_pszVideoMimeTypes[] = {
    "application/octet-stream", /**< Placeholder value when coding is N/A  */
    "video/octet-stream",       /**< autodetection of coding type */
    "video/mpeg",               /**< mpeg2 video, also known as H.262 */
    "video/h263",               /**< covers H.263 */
    "video/mpeg4-generic",      /**< covers MPEG4 */
    "video/x-ms-wmv",           /**< covers all versions of Windows Media Video */
    "video/vnd.rn-realvideo",   /**< covers all versions of Real Video */
    "video/H264",               /**< covers H.264 */
    "video/x-motion-jpeg"       /**< Motion JPEG */
};

OMX_ERRORTYPE NvxPortInitVideo(NvxPort *pThis, OMX_DIRTYPE eDir, OMX_U32 nBufferCount, OMX_U32 nBufferSize,
                               OMX_VIDEO_CODINGTYPE eCodingType)
{
    NvxPortCommonInit(pThis, eDir, nBufferCount, nBufferSize, OMX_PortDomainVideo);
    pThis->oPortDef.format.video.eCompressionFormat = eCodingType;
    if ((int)eCodingType < 0 || eCodingType >= ARRAY_MAX(s_pszVideoMimeTypes))
        pThis->oPortDef.format.video.cMIMEType = s_pszVideoMimeTypes[0];
    else
        pThis->oPortDef.format.video.cMIMEType = s_pszVideoMimeTypes[eCodingType];

    pThis->oPortDef.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pThis->oPortDef.format.video.nFrameWidth = 176;
    pThis->oPortDef.format.video.nFrameHeight = 144;
    pThis->oPortDef.format.video.nBitrate = 64000;
    pThis->oPortDef.format.video.xFramerate = (15 << 16);

    return OMX_ErrorNone;
}

static char *s_pszImageMimeTypes[] = {
    "application/octet-stream",  /**< Placeholder value when coding is N/A  */
    "image/octet-stream",        /**< auto detection of image format */
    "image/jpeg",                /**< JFIF image format */
    "image/jp2",                 /**< JPEG 2000 image format */
    "image/jpeg",                /**< Exif image format */
    "image/tiff",                /**< TIFF image format */
    "image/gif",                 /**< Graphics image format */
    "image/png",                 /**< PNG image format */
    "image/tiff",                /**< LZW image format */
    /* is LZW the tiff LZW or gif LZW formats? */
    "image/x-bmp"                /**< Windows Bitmap format */
};

OMX_ERRORTYPE NvxPortInitImage(NvxPort *pThis, OMX_DIRTYPE eDir, OMX_U32 nBufferCount, OMX_U32 nBufferSize,
                               OMX_IMAGE_CODINGTYPE eCodingType)
{
    NvxPortCommonInit(pThis, eDir, nBufferCount, nBufferSize, OMX_PortDomainImage);
    pThis->oPortDef.format.image.eCompressionFormat = eCodingType;
    if ((int)eCodingType < 0 || eCodingType >= ARRAY_MAX(s_pszImageMimeTypes))
        pThis->oPortDef.format.image.cMIMEType = s_pszImageMimeTypes[0];
    else
        pThis->oPortDef.format.image.cMIMEType = s_pszImageMimeTypes[eCodingType];
    return OMX_ErrorNone;
}

#ifndef MAX
#define MAX(_a_,_b_) ( (_a_) > (_b_) ? (_a_)  : (_b_) )
#endif

static OMX_ERRORTYPE NvxPortGetBufferRequirements(NvxPort *pLast, NvxPort *pThis, OMX_U32 *pnBufferCount, 
                                                  OMX_U32 *pnBufferSize)
{
    OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
    OMX_U32 i;

    /* consider any tunneling port's requirements */
    if (pThis->hTunnelComponent) 
    {
        oPortDef.nSize      = pThis->oPortDef.nSize;
        oPortDef.nVersion   = pThis->oPortDef.nVersion;
        oPortDef.nPortIndex = pThis->nTunnelPort;

        if (NvxIsError( OMX_GetParameter(pThis->hTunnelComponent, OMX_IndexParamPortDefinition, &oPortDef) ))
            return OMX_ErrorUndefined;

        *pnBufferCount = MAX(*pnBufferCount, oPortDef.nBufferCountMin);
        *pnBufferSize = MAX(*pnBufferSize, oPortDef.nBufferSize);

        if (!pThis->bNvidiaTunneling && pThis->nNonTunneledBufferSize > 0)
        {
            *pnBufferSize = MAX(*pnBufferSize, pThis->nNonTunneledBufferSize);
        }
    }  
    
    /* consider any sharing ports' requirements */
    for(i = 0; i < pThis->nSharingPorts; i++) 
    {
        if (pThis->pSharingPorts[i] != pLast)
            NvxPortGetBufferRequirements(pThis, pThis->pSharingPorts[i], pnBufferCount, pnBufferSize);  
    }
 
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxPortUpdateParams(NvxPort *pThis)
{
    return NvxPortGetBufferRequirements(pThis, pThis, &pThis->nReqBufferCount, &pThis->nReqBufferSize);
}

OMX_ERRORTYPE NvxPortAllocResources(NvxPort *pThis)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError err = NvSuccess;

    NvxMutexLock(pThis->hMutex);

    if (pThis->ppBufferHdrs == NULL)
        eError = NvxPortUpdateParams(pThis);

    if (NvxIsSuccess(eError)) 
    {
        if (pThis->pFullBuffers == NULL)
            err = NvMMQueueCreate(&(pThis->pFullBuffers), pThis->nReqBufferCount, sizeof(OMX_BUFFERHEADERTYPE*), NV_FALSE);
        if (pThis->pEmptyBuffers == NULL)
            err |= NvxListCreate(&(pThis->pEmptyBuffers));
        if (pThis->pBufferMarks == NULL)
            err |= NvMMQueueCreate( &(pThis->pBufferMarks),  pThis->nReqBufferCount, sizeof(OMX_MARKTYPE), NV_FALSE);
        if (pThis->pBuffersToSend == NULL)
            err |= NvMMQueueCreate( &(pThis->pBuffersToSend), pThis->nReqBufferCount, sizeof(OMX_BUFFERHEADERTYPE*), NV_FALSE);

        if (pThis->ppBufferHdrs == NULL) 
        {
            pThis->nMaxBuffers = pThis->nReqBufferCount + 40;
            pThis->ppBufferHdrs = NvOsAlloc(pThis->nMaxBuffers * sizeof(OMX_BUFFERHEADERTYPE*));
            if(!pThis->ppBufferHdrs)
            {
                eError = OMX_ErrorInsufficientResources;
            }
        }

        if (pThis->ppBuffers == NULL) 
        {
            pThis->nMaxBuffers = pThis->nReqBufferCount + 40;
            pThis->ppBuffers = NvOsAlloc(pThis->nMaxBuffers * sizeof(OMX_U8 *));
            pThis->pbMySharedBuffer = NvOsAlloc(pThis->nMaxBuffers * sizeof(OMX_BOOL));
            if(!pThis->ppBuffers || !pThis->pbMySharedBuffer)
            {
                NvOsFree(pThis->ppBuffers);
                NvOsFree(pThis->pbMySharedBuffer);
                pThis->ppBuffers = NULL;
                pThis->pbMySharedBuffer = NULL;
                eError = OMX_ErrorInsufficientResources; 
            }
        }
    }

    if (err != NvSuccess)
        eError = OMX_ErrorInsufficientResources;

    NvxMutexUnlock(pThis->hMutex);
    return eError;
}

OMX_BOOL NvxPortIsSupplier(NvxPort *pThis)
{
    if (!pThis->hTunnelComponent)
        return OMX_FALSE;           /* can only be supplier if tunnelling */

    if ((pThis->eSupplierSetting == OMX_BufferSupplyInput && pThis->oPortDef.eDir == OMX_DirInput) ||
        (pThis->eSupplierSetting == OMX_BufferSupplyOutput && pThis->oPortDef.eDir == OMX_DirOutput) )
    {
        return OMX_TRUE;
    }
    return OMX_FALSE;
}

/* decide what the sharing relationship is */
OMX_ERRORTYPE NvxPortDecideSharing(NvxPort *pThis)
{    
    OMX_U32 i;

    /* if disabled then no decision to be made */
    if (!pThis->oPortDef.bEnabled) 
        return OMX_ErrorNone;

    /* buffer supplier must be specified to determine sharing */
    if (pThis->eSupplierSetting == OMX_BufferSupplyUnspecified) 
        return OMX_ErrorNotReady;

    /* buffer supplier must be specified for all sharing candidates to determine sharing */
    for (i = 0; i < pThis->nSharingCandidates; i++) 
    {
        if ((pThis->pSharingCandidates[i]->oPortDef.bEnabled) && 
            (pThis->pSharingCandidates[i]->eSupplierSetting == OMX_BufferSupplyUnspecified))
           return OMX_ErrorNotReady;
    }

    /* is there any possible sharing relationship? */
    if (pThis->nSharingCandidates) 
    {
        if (pThis->oPortDef.eDir == OMX_DirInput) 
        {
            /* input port */
            if (pThis->nSharingCandidates > 1)
            {
                /* Candidate sharing relationship is 1-to-N (multiple outputs re-using 
                 * one input's buffers). */
                /* Share with all output candidates that are suppliers. */ 
                for (i = 0; i < pThis->nSharingCandidates; i++)
                {
                    if ((pThis->pSharingCandidates[i]->oPortDef.bEnabled) && 
                        (pThis->pSharingCandidates[i]->eSupplierSetting == OMX_BufferSupplyOutput) &&
                        (!pThis->bShareIfNvidiaTunnel || 
                         (pThis->bShareIfNvidiaTunnel && pThis->pSharingCandidates[i]->bNvidiaTunneling)))
                    {
                        /* output is a supplier -> share with it */
                        pThis->pSharingPorts[pThis->nSharingPorts] = pThis->pSharingCandidates[i];
                        pThis->nSharingPorts++;
                    }            
                }                 
            } 
            else
            {
                /* Candidate sharing relationship is 1-to-1 (a single port re-using 
                 * another single port's buffers). */
                if ((pThis->eSupplierSetting == OMX_BufferSupplyInput) && 
                    (pThis->pSharingCandidates[0]->oPortDef.bEnabled) &&
                    (!pThis->bShareIfNvidiaTunnel ||
                     (pThis->bShareIfNvidiaTunnel && pThis->pSharingCandidates[0]->bNvidiaTunneling)))

                {
                    /* Supplier input port */
                    /* Can always share with output. Output will re-use input buffers.  */
                    pThis->nSharingPorts = 1;
                    pThis->pSharingPorts[0] = pThis->pSharingCandidates[0];
                }
                else
                {
                    /* Non-supplier input port */
                    /* Is associated output a supplier? */
                    if ((pThis->pSharingCandidates[0]->eSupplierSetting == OMX_BufferSupplyOutput) &&
                        (pThis->pSharingCandidates[0]->oPortDef.bEnabled) &&
                        (!pThis->bShareIfNvidiaTunnel ||
                         (pThis->bShareIfNvidiaTunnel && pThis->pSharingCandidates[0]->bNvidiaTunneling)))
                    {                    
                        /* Can share. Output will re-use input buffers */
                        pThis->nSharingPorts = 1;
                        pThis->pSharingPorts[0] = pThis->pSharingCandidates[0];
                    }
                }
            }
        } 
        else 
        {
            /* Note: an output can only shares with at most one input. Thus... */
            /* Candidate sharing relationship is 1-to-1 (a single port re-using 
             *  another single port's buffers). */
            if ((pThis->eSupplierSetting == OMX_BufferSupplyOutput) &&
                (pThis->pSharingCandidates[0]->oPortDef.bEnabled) &&
                (!pThis->bShareIfNvidiaTunnel ||
                 (pThis->bShareIfNvidiaTunnel && pThis->bNvidiaTunneling)))
            {
                /* Supplier output port */
                /* Can always share with input. Output will re-use input buffers.  */
                pThis->nSharingPorts = 1;
                pThis->pSharingPorts[0] = pThis->pSharingCandidates[0];
            }
            else
            {
                /* Non-supplier output port */
                /* Is associated input a supplier? */
                if ((pThis->pSharingCandidates[0]->eSupplierSetting == OMX_BufferSupplyInput) &&
                    (pThis->pSharingCandidates[0]->oPortDef.bEnabled) &&
                    (!pThis->bShareIfNvidiaTunnel ||
                     (pThis->bShareIfNvidiaTunnel && pThis->bNvidiaTunneling)))
                {                
                    /* Can share. Input will re-use output buffers */
                    pThis->nSharingPorts = 1;
                    pThis->pSharingPorts[0] = pThis->pSharingCandidates[0];
                }
            }
        }
    }

    return OMX_ErrorNone;
}

/* Return true if and only if this port is re-using buffers from another port 
 * of the same component. */
OMX_BOOL NvxPortIsReusingBuffers(NvxPort *pThis)
{
    /* must be a supplier */
    if (OMX_FALSE == NvxPortIsSupplier(pThis))
        return OMX_FALSE;
    
    /* must be sharing with a port */
    if (0 == pThis->nSharingPorts)
        return OMX_FALSE;
    
    /* must not be sharing with an supplier output (in this case, by policy, 
     * the output port re-uses buffers instead) */
    if ((pThis->oPortDef.eDir == OMX_DirInput) && 
        NvxPortIsSupplier(pThis->pSharingPorts[0]))
       return OMX_FALSE;

    return OMX_TRUE;
}

/* Return true if and only if this port allocates its own buffers */
OMX_BOOL NvxPortIsAllocator(NvxPort *pThis)
{
    /* must be a supplier */
    if (OMX_FALSE == NvxPortIsSupplier(pThis))
        return OMX_FALSE;

    /* must not be sharing with a port */
    if (pThis->nSharingPorts)
        return OMX_FALSE;

    return OMX_TRUE;
}

OMX_ERRORTYPE NvxPortCreateTunnel(NvxPort *pThis)
{
    OMX_U32 nSizeBytes, i;
    OMX_U8 *pBuf;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BOOL bReusingBuffers;

    if (!pThis->hTunnelComponent)
    {
        if (pThis->nBuffers < pThis->nReqBufferCount &&
            pThis->oPortDef.bEnabled)
        {
            return OMX_ErrorNotReady;
        }

        if (pThis->nBuffers >= pThis->nReqBufferCount)
            pThis->oPortDef.bPopulated = OMX_TRUE;

        return OMX_ErrorNone;
    }

    /* we should know who the supplier is by now if we are tunneling */
    if (pThis->eSupplierSetting == OMX_BufferSupplyUnspecified)
        return OMX_ErrorNotReady;

    /* allocate resource for tunneling clients */
    if (NvxIsError( NvxPortAllocResources(pThis) ))
        return OMX_ErrorUndefined;

    /* initialize pbMySharedBuffer */
    for (i = 0; i < pThis->nReqBufferCount; i++)
        pThis->pbMySharedBuffer[i] = OMX_TRUE;

    if (pThis->nBuffers < pThis->nReqBufferCount)
    {
        /* TODO: update supplier based on negotiation, ensure port compatbility, etc */
        /* are we the supplier? */
        if (NvxPortIsSupplier(pThis))
        {
            /* then create the buffers */

            nSizeBytes = pThis->nReqBufferSize;

            /* determine if we'll re-use buffers from another port of this component */
            bReusingBuffers = NvxPortIsReusingBuffers(pThis);
            if (bReusingBuffers)
            {
                /* make sure the sharing port is ready */
                eError = NvxPortCreateTunnel(pThis->pSharingPorts[0]);
                if (eError)
                    return eError;
            }

            for (pThis->nBuffers = 0; pThis->nBuffers < pThis->nReqBufferCount; ++pThis->nBuffers)
            {
                OMX_ERRORTYPE eError = OMX_ErrorNone;
                OMX_BUFFERHEADERTYPE *pBufferHeader;

                /* By policy if both input and output are suppliers then this input allocates buffers */
                
                /* If sharing buffers and either the input or the sharing port is a non-supplier */
                if (bReusingBuffers)
                {   
                    /* then re-use the sharing port's buffers */
                    pBuf = pThis->pSharingPorts[0]->ppBufferHdrs[pThis->nBuffers]->pBuffer; 
                    pThis->pbMySharedBuffer[pThis->nBuffers] = OMX_FALSE;
                    pThis->ppBuffers[pThis->nBuffers] = 0;    /* we didn't allocate this buffer */
                }
                else 
                { 
                    /* otherwise allocate new ones */
                    pBuf = NvOsAlloc(nSizeBytes);
                    NvOsMemset(pBuf, 0, nSizeBytes);
                    pThis->ppBuffers[pThis->nBuffers] = pBuf; /* cache for deallocation */
                }

                if (pBuf == NULL)
                    return OMX_ErrorInsufficientResources;

                pBufferHeader = NULL;
                eError = OMX_UseBuffer(pThis->hTunnelComponent,
                                        &pBufferHeader,
                                        pThis->nTunnelPort,
                                        NULL,
                                        nSizeBytes,
                                        pBuf);

                if (NvxIsSuccess(eError)) 
                {
                    pThis->ppBufferHdrs[pThis->nBuffers] = pBufferHeader;
                    if (pThis->oPortDef.eDir == OMX_DirOutput) 
                    {
                        pBufferHeader->pOutputPortPrivate = pThis;
                        pBufferHeader->nOutputPortIndex = pThis->oPortDef.nPortIndex;
                        pBufferHeader->nInputPortIndex = pThis->nTunnelPort;
                    }
                    else 
                    {
                        pBufferHeader->pInputPortPrivate = pThis;
                        pBufferHeader->nInputPortIndex = pThis->oPortDef.nPortIndex;
                        pBufferHeader->nOutputPortIndex = pThis->nTunnelPort;
                    }
                    if (pThis->oPortDef.eDir == OMX_DirOutput)
                        NvxListEnQ(pThis->pEmptyBuffers, pBufferHeader, OMX_TIMEOUT_MS);
                }
                else 
                {
                    if (!bReusingBuffers)
                        NvOsFree(pBuf); /* free up buffer we're not going to use anywhere */
                    /* previous headers and buffers will be deallocated when we deallocate the rest of the resources */
                    return eError;
                }
            }
        } 
        else 
        { /* not the supplier */
            return OMX_ErrorNotReady;
        }
    }
    
    if (pThis->nBuffers >= pThis->nReqBufferCount)
        pThis->oPortDef.bPopulated = OMX_TRUE;

    return eError;
}

OMX_ERRORTYPE NvxPortReleaseResources(NvxPort *pThis)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U8 *pBuffer;
    OMX_BOOL bIsAllocator;
    OMX_BUFFERHEADERTYPE *pBufferHdr;
    NvxMutexLock(pThis->hMutex);

    if (NvxPortIsSupplier(pThis))
    {        
        /* determine if this port allocated its own buffers */
        bIsAllocator = NvxPortIsAllocator(pThis);
        while (pThis->nBuffers > 0)
        {
            /* free the buffer */
            --pThis->nBuffers;
            pBuffer = pThis->ppBuffers[pThis->nBuffers];
            if (pBuffer && bIsAllocator)
                NvOsFree(pBuffer);

            pThis->ppBuffers[pThis->nBuffers] = NULL;

            /* tell the non-supplier to free the buffer header */
            pBufferHdr = pThis->ppBufferHdrs[pThis->nBuffers];
            pThis->ppBufferHdrs[pThis->nBuffers] = NULL;
            if (pBufferHdr)
            {
                NvxMutexUnlock(pThis->hMutex);
                NvxCheckError(eError, OMX_FreeBuffer(pThis->hTunnelComponent, pThis->nTunnelPort, pBufferHdr));
                NvxMutexLock(pThis->hMutex);
            }
        }
    }

    /* todo: non-tunneled case */
    /* if all buffers are freed, deallocate pointers */
    if (pThis->nBuffers < pThis->nReqBufferCount)
        pThis->oPortDef.bPopulated = OMX_FALSE;

    if (eError == OMX_ErrorNone && pThis->nBuffers > 0)
    {
        eError = OMX_ErrorNotReady;
        pThis->bStarted = OMX_FALSE;

        NvxMutexUnlock(pThis->hMutex);
        return eError;
    }
    else if (pThis->nBuffers == 0) 
    {
        NvOsFree(pThis->ppBufferHdrs);
        NvOsFree(pThis->pbMySharedBuffer);        
        NvOsFree(pThis->ppBuffers);
        pThis->ppBufferHdrs = NULL;
        pThis->ppBuffers = NULL;
        pThis->pbMySharedBuffer = NULL;
    }

    if (pThis->pFullBuffers != NULL) 
    {
        NvMMQueueDestroy(&pThis->pFullBuffers);
        pThis->pFullBuffers = NULL;
    }
    if (pThis->pEmptyBuffers != NULL)
    {
        NvxListDestroy(&pThis->pEmptyBuffers);
        pThis->pEmptyBuffers = NULL;
    }
    if (pThis->pBufferMarks != NULL) {
        NvMMQueueDestroy(&pThis->pBufferMarks);
        pThis->pBufferMarks = NULL;
    }
    if (pThis->pBuffersToSend != NULL) {
        NvMMQueueDestroy(&pThis->pBuffersToSend);
        pThis->pBuffersToSend = NULL;
    }   


    pThis->bStarted = OMX_FALSE;

    NvxMutexUnlock(pThis->hMutex);
    return eError;
}

/* Add buffer to the full queue */
OMX_ERRORTYPE NvxPortEmptyThisBuffer(NvxPort *pThis, OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
    NvError err;
    OMX_BOOL bReady;

    bReady = NvxPortAcceptingBuffers(pThis);

    if (!bReady)
    {
        NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortEmptyThisBuffer not ready: %s:%d (enabled: %d)\n", pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex, pThis->oPortDef.bEnabled));
        return pThis->oPortDef.bEnabled == OMX_FALSE ? OMX_ErrorIncorrectStateOperation : OMX_ErrorPortUnpopulated;
    }

    /* TODO: validate pBuffer ports, etc. */
    if (!pThis->pFullBuffers)
        return OMX_ErrorResourcesLost; 

    err = NvMMQueueEnQ(pThis->pFullBuffers, &pBuffer, OMX_TIMEOUT_MS);

    if (pThis->nSharingPorts)
    {
        OMX_U32 idx;
        NvxPortBufferPtrToIdx(pThis, pBuffer->pBuffer, &idx);    
        pThis->pbMySharedBuffer[idx] = OMX_TRUE;
        pThis->pSharingPorts[0]->pbMySharedBuffer[idx] = OMX_FALSE;
    }

    NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortEmptyThisBuffer: %s:%d (buffer: %p) %x\n",
              pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex, pBuffer, err));

    if (err == NvSuccess)
        return OMX_ErrorNone;
    return OMX_ErrorNotReady;
}

int NvxPortCountBuffersSharedAndDelivered(NvxPort *pThis)
{
    int val = 0;
    OMX_U32 i;

    if (pThis->nSharingPorts && pThis->pSharingPorts[0])
    {
        for (i = 0; i < pThis->nReqBufferCount; i++)
        {
            if (pThis->pSharingPorts[0]->pbMySharedBuffer[i] == OMX_TRUE)
                val++;
        }
    }

    return val;
}

/* Add buffer to the empty queue. */
OMX_ERRORTYPE NvxPortFillThisBuffer(NvxPort *pThis, OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
    NvError err;
    OMX_BOOL bReady;
    OMX_U32 idx;

    bReady = NvxPortAcceptingBuffers(pThis);
    if (!bReady)
    {
        NVXTRACE((NVXT_ERROR, pThis->pNvComp->eObjectType, "PortFillThisBuffer not ready: %s:%d (enabled: %d)\n", pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex, pThis->oPortDef.bEnabled));
        return pThis->oPortDef.bEnabled == OMX_FALSE ? OMX_ErrorIncorrectStateOperation : OMX_ErrorPortUnpopulated;
    }

    /* if sharing transfer internal ownership of shared buffer */
    if (pThis->nSharingPorts){
        NvxPortBufferPtrToIdx(pThis, pBuffer->pBuffer, &idx);
        pThis->pbMySharedBuffer[idx] = OMX_FALSE;

        NvxPortBufferPtrToIdx(pThis->pSharingPorts[0], pBuffer->pBuffer, &idx);
        pThis->pSharingPorts[0]->pbMySharedBuffer[idx] = OMX_TRUE;
        NvxMutexLock(pThis->pNvComp->hWorkerMutex);
        NvxPortReleaseBuffer(pThis->pSharingPorts[0], pThis->pSharingPorts[0]->ppBufferHdrs[idx]);
        NvxMutexUnlock(pThis->pNvComp->hWorkerMutex);
    }

    /* TODO: validate pBuffer ports, etc. */
    if (!pThis->pEmptyBuffers)
        return OMX_ErrorResourcesLost;

    /* clear out metadata before trying to fill in */
    err = NvxListEnQ(pThis->pEmptyBuffers, pBuffer, OMX_TIMEOUT_MS);
    NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortFillThisBuffer: %s:%d (buffer: %p) %x\n",
              pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex, pBuffer, err));

    if (NvSuccess == err)
        return OMX_ErrorNone;
    return OMX_ErrorNotReady;
}

/* grab an empty buffer (and init some of the buffer data) if possible) */
OMX_ERRORTYPE NvxPortGetEmptyBuffer(NvxPort *pThis, OMX_OUT OMX_BUFFERHEADERTYPE** ppBuffer)
{
    NvError err = NvError_InvalidSize;
    OMX_BUFFERHEADERTYPE* pBuffer;

    if (!pThis->pEmptyBuffers)
        return OMX_ErrorNoMore;

    *ppBuffer = NULL;
    if (NvxListGetNumEntries(pThis->pEmptyBuffers) > 0)
        err = NvxListDeQ(pThis->pEmptyBuffers, (void **)(&pBuffer));

    if (NvSuccess == err) 
    {
        pBuffer->nTickCount = 0;
        pBuffer->hMarkTargetComponent = 0;
        pBuffer->pMarkData = 0;
        pBuffer->nTickCount = 0;
        pBuffer->nTimeStamp = 0;
        pBuffer->nFlags = 0;
        /* FIXME: thes should be buffer private, not port private! */
        pThis->bSendEvents = OMX_TRUE;
        pThis->uMetaDataCopyCount = 1;
        pThis->pMetaDataSource = pThis;
        *ppBuffer = pBuffer;
    }

    if (NvSuccess == err)
        return OMX_ErrorNone;
    return OMX_ErrorNoMore;
}

/* Grab a full buffer from the queue, if possible */
OMX_ERRORTYPE NvxPortGetFullBuffer(NvxPort *pThis, OMX_OUT OMX_BUFFERHEADERTYPE** ppBuffer)
{
    NvError err = NvError_InvalidSize;
    pThis->bSendEvents = OMX_TRUE;
    pThis->uMetaDataCopyCount = 1;
    pThis->pMetaDataSource = pThis;

    if (!pThis->pFullBuffers)
        return OMX_ErrorNoMore;

    if (NvMMQueueGetNumEntries(pThis->pFullBuffers) > 0)
        err = NvMMQueueDeQ(pThis->pFullBuffers, ppBuffer);

    if (NvSuccess == err)
        return OMX_ErrorNone;
    return OMX_ErrorNoMore;
}

/* Get the next buffer (full if on an input port, empty if on an output port) 
 * to process */
OMX_BOOL NvxPortGetNextHdr(NvxPort *pThis)
{
    NvxMutexLock(pThis->hMutex);
    if (!pThis->pCurrentBufferHdr) 
    {
        if (pThis->oPortDef.eDir == OMX_DirInput && 
            NvxIsSuccess(NvxPortGetFullBuffer(pThis, &pThis->pCurrentBufferHdr)))
        {
            // nothin
        }
        else if (pThis->oPortDef.eDir == OMX_DirOutput && 
            NvxIsSuccess(NvxPortGetEmptyBuffer(pThis, &pThis->pCurrentBufferHdr))) 
        {
            pThis->pCurrentBufferHdr->nOffset = 0;
            pThis->pCurrentBufferHdr->nFilledLen = 0;
        }
        else 
        {
            pThis->pCurrentBufferHdr = NULL;
        }
    }
    
    if (pThis->pCurrentBufferHdr && 0 == pThis->pCurrentBufferHdr->hMarkTargetComponent)
    {
        OMX_MARKTYPE oMark;
        if (NvMMQueueDeQ(pThis->pBufferMarks, &oMark) == NvSuccess)
        {
            pThis->pCurrentBufferHdr->hMarkTargetComponent = oMark.hMarkTargetComponent;
            pThis->pCurrentBufferHdr->pMarkData = oMark.pMarkData;
        }
    }
    NvxMutexUnlock(pThis->hMutex);    
    return (OMX_BOOL)(pThis->pCurrentBufferHdr);
}

static OMX_BOOL CheckIfNeedSendEOSEvent(NvxPort *pThis)
{
    OMX_U32 nPort = 0;

    // Outputs, all the time
    if (pThis->oPortDef.eDir == OMX_DirOutput)
        return OMX_TRUE;

    // Inputs, only if there are no active outputs
    for (nPort = 0; nPort < pThis->pNvComp->nPorts; nPort++)
    {
         if (NvxPortIsReady(&(pThis->pNvComp->pPorts[nPort])) && 
             pThis->pNvComp->pPorts[nPort].oPortDef.eDir == OMX_DirOutput )
         {
             return OMX_FALSE;
         }
    }

    return OMX_TRUE;
}

/* (internal) Done with a buffer, get the next one if possible */
static OMX_ERRORTYPE FinishBufferHdrHandling(NvxPort *pThis, OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BOOL bReady;

    bReady = NvxPortIsReady(pThis);

    if (!bReady)
        return pThis->oPortDef.bEnabled == OMX_FALSE ? OMX_ErrorIncorrectStateOperation : OMX_ErrorPortUnpopulated;

    /* If this was the current buffer, get a new current buffer */
    if (pThis->pCurrentBufferHdr == pBuffer)
    {
        pThis->pCurrentBufferHdr = NULL;
    }

    /* Send an EOS event if desired */
    if ((pBuffer->nFlags & OMX_BUFFERFLAG_EOS) && pThis->bSendEvents && 
        CheckIfNeedSendEOSEvent(pThis))
    {
        NvDebugOmx(("Component %s: NvxPort: Sending EOS event on Port - %d", pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex));
        NvxCheckError(eError, NvxSendEvent(pThis->pNvComp, OMX_EventBufferFlag, pThis->oPortDef.nPortIndex, pBuffer->nFlags, 0) );
    }

    /* Propogate buffer marks */
    if ( pBuffer->hMarkTargetComponent == pThis->pNvComp->hBaseComponent
         && pThis->bSendEvents
         && pThis->pMetaDataSource && pThis->pMetaDataSource->uMetaDataCopyCount == 1) {
        NvxCheckError(eError, NvxSendEvent(pThis->pNvComp, OMX_EventMark, 0, 0, pBuffer->pMarkData) );
        pBuffer->hMarkTargetComponent = 0;
        pBuffer->pMarkData = NULL;
    }

    if (pThis->pMetaDataSource && pThis->pMetaDataSource->uMetaDataCopyCount > 0)
        --pThis->pMetaDataSource->uMetaDataCopyCount;

    return eError;
}

/* Done with a buffer, get a new one, return the old one */
OMX_ERRORTYPE NvxPortReleaseBuffer(NvxPort *pThis, OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError;
    OMX_U32 idx;

    NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortReleaseBuffer start %s:%d (buffer %p)\n",
              pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex, pBuffer));

    eError = FinishBufferHdrHandling(pThis, pBuffer);
    if (eError == OMX_ErrorIncorrectStateOperation)
    {
        NVXTRACE((NVXT_ERROR, pThis->pNvComp->eObjectType, "PortReleaseBuffer FinishBuffer failed "
                  "%s:%d (buffer %p error %x)\n", pThis->pNvComp->pComponentName, 
                  pThis->oPortDef.nPortIndex, pBuffer, eError));
        return eError;
    }

    /* if sharing don't release unless we own the shared buffer */
    if (pThis->nSharingPorts)
    {
        NvxPortBufferPtrToIdx(pThis, pBuffer->pBuffer, &idx);    
        if (OMX_FALSE == pThis->pbMySharedBuffer[idx])
        {
            NVXTRACE((NVXT_ERROR, pThis->pNvComp->eObjectType, "PortReleaseBuffer not releasing, "
                      "sharing not ready %s:%d (buffer: %p)\n", pThis->pNvComp->pComponentName,
                       pThis->oPortDef.nPortIndex, pBuffer));
            return OMX_ErrorNotReady;
        }
        pThis->pbMySharedBuffer[idx] = OMX_FALSE;
    }

    if (pThis->oPortDef.eDir == OMX_DirInput)
     {
        /* return buffer to client */
        if (pThis->hTunnelComponent != NULL)
        {
            NvxCheckError(eError, OMX_FillThisBuffer(pThis->hTunnelComponent, pBuffer));
            if (eError == OMX_ErrorNotReady)
            {
                NVXTRACE((NVXT_ERROR, pThis->pNvComp->eObjectType, "PortReleaseBuffer tunnel not "
                          "ready, queueing %s:%d (buffer: %p)\n", pThis->pNvComp->pComponentName,
                          pThis->oPortDef.nPortIndex, pBuffer));

                NvMMQueueEnQ(pThis->pBuffersToSend, &pBuffer, OMX_TIMEOUT_MS);
                eError = OMX_ErrorNone;
            }
        }
        else if (pThis->pNvComp->pCallbacks && pThis->pNvComp->pCallbacks->EmptyBufferDone)
        {
            if (pThis->pNvComp->eState == OMX_StatePause)
                NvMMQueueEnQ(pThis->pBuffersToSend, &pBuffer, OMX_TIMEOUT_MS);
            else
                NvxCheckError(eError, pThis->pNvComp->pCallbacks->EmptyBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
        }
    }
    else 
    {
        /* recycle the buffer for use later */
        NvxCheckError(eError, NvxPortFillThisBuffer(pThis, pBuffer));
    }

    NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortReleaseBuffer end %s:%d "
              "(buffer %p  error %x)\n", pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex,
              pBuffer, eError));

    return eError;
}

/* return all buffers to the component/port that created them */
OMX_ERRORTYPE NvxPortReturnBuffers(OMX_IN NvxPort *pThis, OMX_BOOL bWorkerLocked)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError err = NvSuccess;
    OMX_BUFFERHEADERTYPE *pBuffer;
    OMX_U32 idx;

    if (pThis->nBuffers == 0)
        return OMX_ErrorNone;

    NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortReturnBuffers %s:%d\n",
              pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex));

    if (pThis->pNvComp->ReturnBuffers)
        pThis->pNvComp->ReturnBuffers(pThis->pNvComp, pThis->oPortDef.nPortIndex, bWorkerLocked);

    /* Return buffers that aren't ours */
    if (pThis->hTunnelComponent)
    {
        if(!pThis->pFullBuffers || !pThis->pEmptyBuffers)
            return OMX_ErrorResourcesLost;

        NvxMutexLock(pThis->hMutex);
        if (NvxPortIsReusingBuffers(pThis))
        {
            while (NvxPortNumPendingBuffers(pThis))
            {
                NvMMQueuePeek(pThis->pBuffersToSend, &pBuffer);
                if (!pBuffer)
                    break;

                pBuffer->nFilledLen = 0;
                pBuffer->nOffset = 0;

                if (pThis->oPortDef.eDir == OMX_DirInput)
                    err = NvMMQueueEnQ(pThis->pFullBuffers, &pBuffer, OMX_TIMEOUT_MS);
                else
                    err = NvxListEnQ(pThis->pEmptyBuffers, pBuffer, OMX_TIMEOUT_MS);

                if (NvSuccess == err)
                    NvMMQueueDeQ(pThis->pBuffersToSend, &pBuffer);
                else
                    break;
            }

            if (pThis->oPortDef.eDir == OMX_DirInput)
            {

                NvxPortGetNextHdr(pThis);
                while (pThis->pCurrentBufferHdr)
                {
                    pThis->pCurrentBufferHdr->nFilledLen = 0;
                    NvxPortReleaseBuffer(pThis, pThis->pCurrentBufferHdr);
                    NvxPortGetNextHdr(pThis);
                }
            }
            else if (pThis->oPortDef.eDir == OMX_DirOutput)               
            {
                while (pThis->pCurrentBufferHdr)
                {
                    pThis->pCurrentBufferHdr->nFilledLen = 0;
                    NvxPortDeliverFullBuffer(pThis, pThis->pCurrentBufferHdr);
                }
            }
        }
        else if ((pThis->oPortDef.eDir == OMX_DirInput) && (pThis->eSupplierSetting != OMX_BufferSupplyInput))
        {
            /* non-supplier input return all buffers to the output */
            while (NvxIsSuccess(NvxPortGetFullBuffer(pThis, &pBuffer)))
            {
                pBuffer->nFilledLen = 0;
                NvxCheckError(eError, OMX_FillThisBuffer(pThis->hTunnelComponent,pBuffer));
            }

            /* return the "bullet in the chamber" */
            if (pThis->pCurrentBufferHdr)
            {
                NvxCheckError(eError, OMX_FillThisBuffer(pThis->hTunnelComponent, pThis->pCurrentBufferHdr));
                if (eError == OMX_ErrorNone)
                {
                    pThis->pCurrentBufferHdr = NULL;
                }
            }

            if (NvxPortNumPendingBuffers(pThis))
            {
                NvxCheckError(eError, NvxPortSendPendingBuffers(pThis));
            }
        }
        else if ((pThis->oPortDef.eDir == OMX_DirOutput) && (pThis->eSupplierSetting != OMX_BufferSupplyOutput))
        {
            /* non-supplier Output return all buffers to the output */
            while (NvxIsSuccess(NvxPortGetEmptyBuffer(pThis, &pBuffer)))
            {
                pBuffer->nFilledLen = 0;
                if (pThis->nSharingPorts)
                {
                    NvxPortBufferPtrToIdx(pThis, pBuffer->pBuffer, &idx);
                    pThis->pbMySharedBuffer[idx] = OMX_TRUE;
                    pThis->pSharingPorts[0]->pbMySharedBuffer[idx] = OMX_FALSE;
                }

                NvxCheckError(eError, OMX_EmptyThisBuffer(pThis->hTunnelComponent, pBuffer));
            }

            /* return any "bullet in the chamber" */
            if (pThis->pCurrentBufferHdr)
            {
                if (pThis->nSharingPorts)
                { 
                    NvxPortBufferPtrToIdx(pThis, pThis->pCurrentBufferHdr->pBuffer, &idx);
                    pThis->pbMySharedBuffer[idx] = OMX_TRUE;
                    pThis->pSharingPorts[0]->pbMySharedBuffer[idx] = OMX_FALSE;
                }

                NvxCheckError(eError, OMX_EmptyThisBuffer(pThis->hTunnelComponent, pThis->pCurrentBufferHdr));
                pThis->pCurrentBufferHdr = NULL;
            }

            if (NvxPortNumPendingBuffers(pThis))
            {
                NvxCheckError(eError, NvxPortSendPendingBuffers(pThis));
            }

        }
        else /* this port is a supplier */
        {
            /* put any "bullet in the chamber" back in the queue */
            if (pThis->pCurrentBufferHdr)
            {
                pThis->pCurrentBufferHdr->nFilledLen = 0;
                pThis->pCurrentBufferHdr->nOffset = 0;
                pThis->pCurrentBufferHdr->nFlags = 0;

                if (pThis->oPortDef.eDir == OMX_DirInput)
                    err = NvMMQueueEnQ( pThis->pFullBuffers, &pThis->pCurrentBufferHdr, OMX_TIMEOUT_MS);
                else
                    err = NvxListEnQ( pThis->pEmptyBuffers, pThis->pCurrentBufferHdr, OMX_TIMEOUT_MS);

                if (NvSuccess == err)
                {
                    pThis->pCurrentBufferHdr = NULL;
                }
            }

            while (NvxPortNumPendingBuffers(pThis))
            {
                NvMMQueuePeek(pThis->pBuffersToSend, &pBuffer);
                if (!pBuffer)
                    break;

                pBuffer->nFilledLen = 0;
                pBuffer->nOffset = 0;
                pBuffer->nFlags = 0;

                if (pThis->oPortDef.eDir == OMX_DirInput)
                    err = NvMMQueueEnQ(pThis->pFullBuffers, &pBuffer, OMX_TIMEOUT_MS);
                else
                    err = NvxListEnQ(pThis->pEmptyBuffers, pBuffer, OMX_TIMEOUT_MS);

                if (NvSuccess == err)
                    NvMMQueueDeQ(pThis->pBuffersToSend, &pBuffer);
                else
                    break;
            }
        }
        NvxMutexUnlock(pThis->hMutex);
    }
    else
    {
        NvxMutexLock(pThis->hMutex);

        /* Empty the queues */
        if (pThis->oPortDef.eDir == OMX_DirInput)
        {
            if (pThis->pNvComp->pCallbacks != NULL && pThis->pNvComp->pCallbacks->EmptyBufferDone != NULL)
            {
                while (NvSuccess == NvMMQueueDeQ(pThis->pFullBuffers, (void *)&pBuffer))
                {
                    NvxCheckError(eError, pThis->pNvComp->pCallbacks->EmptyBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
                }
            }
        }
        else
        {
            if (pThis->pNvComp->pCallbacks != NULL && pThis->pNvComp->pCallbacks->FillBufferDone != NULL)
            {
                while (NvSuccess == NvxListDeQ(pThis->pEmptyBuffers, (void *)&pBuffer))
                {
                    pBuffer->nFilledLen = 0;
                    NvxCheckError(eError, pThis->pNvComp->pCallbacks->FillBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
                }
            }
        }

        /* Return any "bullet in the chamber" */ 
        if (pThis->pCurrentBufferHdr) 
        {
            pBuffer = pThis->pCurrentBufferHdr;
            pThis->pCurrentBufferHdr = NULL;
            if (pThis->pNvComp->pCallbacks) 
            {
                if (pThis->oPortDef.eDir == OMX_DirInput && pThis->pNvComp->pCallbacks->EmptyBufferDone != NULL)
                {
                    NvxCheckError(eError, pThis->pNvComp->pCallbacks->EmptyBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
                }
                else if (pThis->pNvComp->pCallbacks->FillBufferDone != NULL)
                {
                    pBuffer->nFilledLen = 0;
                    NvxCheckError(eError, pThis->pNvComp->pCallbacks->FillBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
                }
            }
        }

        if (NvxPortNumPendingBuffers(pThis))
        {
            NvxCheckError(eError, NvxPortSendPendingBuffers(pThis));
        }

        NvxMutexUnlock(pThis->hMutex);
    }

    pThis->bStarted = OMX_FALSE;
    return OMX_ErrorNone;
}

/* return all buffers to the component/port that needs to fill them */
OMX_ERRORTYPE NvxPortFlushBuffers(OMX_IN NvxPort *pThis)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvError err = NvSuccess;
    OMX_BUFFERHEADERTYPE *pBuffer = NULL;

    NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "%s port %d flushing buffers\n",
              pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex));

    if (pThis->nBuffers == 0)
        return OMX_ErrorNone;

    /* Return buffers that aren't ours */
    if (pThis->hTunnelComponent)
    {
        if(!pThis->pFullBuffers || !pThis->pEmptyBuffers)
            return OMX_ErrorResourcesLost;

        if (pThis->oPortDef.eDir == OMX_DirInput)
        {
            /* input return all buffers to the output */
            while (NvSuccess == NvMMQueueDeQ(pThis->pFullBuffers, (void *)&pBuffer))
            {
                pBuffer->nFilledLen = 0;
                NvxCheckError(eError, OMX_FillThisBuffer(pThis->hTunnelComponent,pBuffer));
                if (NvxIsError(eError))
                {
                    goto FLUSH_FAILED;
                }
            }

            /* return the "bullet in the chamber" */
            if (pThis->pCurrentBufferHdr) {
                pThis->pCurrentBufferHdr->nFilledLen = 0;
                NvxCheckError(eError, OMX_FillThisBuffer(pThis->hTunnelComponent, pThis->pCurrentBufferHdr));
                pThis->pCurrentBufferHdr = NULL;
                if (NvxIsError(eError))
                {
                    goto FLUSH_FAILED;
                }
            }

            if (NvxPortNumPendingBuffers(pThis))
            {
                NvxCheckError(eError, NvxPortSendPendingBuffers(pThis));
            }
        }
        else if (pThis->oPortDef.eDir == OMX_DirOutput)
        {
            /* dump any "bullet in the chamber" */
            if (pThis->pCurrentBufferHdr)
            {
                pThis->pCurrentBufferHdr->nFilledLen = 0;
            }

            while (NvxPortNumPendingBuffers(pThis))
            {
                NvMMQueuePeek(pThis->pBuffersToSend, &pBuffer);
                if (!pBuffer)
                    break;
                err = NvxListEnQ(pThis->pEmptyBuffers, pBuffer, OMX_TIMEOUT_MS);
                if (err == NvSuccess)
                    NvMMQueueDeQ(pThis->pBuffersToSend, &pBuffer);
                else
                    break;
            }
        }
    }
    else
    {
        /* Empty the queues */
        if (pThis->oPortDef.eDir == OMX_DirInput)
        {
            if (pThis->pNvComp->pCallbacks != NULL && pThis->pNvComp->pCallbacks->EmptyBufferDone != NULL)
            {
                while (NvSuccess == NvMMQueueDeQ(pThis->pFullBuffers, (void *)&pBuffer))
                {
                    NvxCheckError(eError, pThis->pNvComp->pCallbacks->EmptyBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
                    if (NvxIsError(eError))
                    {
                        goto FLUSH_FAILED;
                    }
                }
            }

            /* Return any "bullet in the chamber" */
            if (pThis->pCurrentBufferHdr)
            {
                pBuffer = pThis->pCurrentBufferHdr;
                if (pThis->pNvComp->pCallbacks)
                {
                    if (pThis->oPortDef.eDir == OMX_DirInput && pThis->pNvComp->pCallbacks->EmptyBufferDone != NULL)
                    {
                        pBuffer->nFilledLen = 0;
                        NvxCheckError(eError, pThis->pNvComp->pCallbacks->EmptyBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
                        pThis->pCurrentBufferHdr = NULL;
                        if (NvxIsError(eError))
                        {
                            goto FLUSH_FAILED;
                        }
                    }
                }
            }
        }
        else if (pThis->oPortDef.eDir == OMX_DirOutput)
        {
            /* return/clear all the buffers on output port, when flushed & make necessary callbacks */
            if (pThis->pNvComp->pCallbacks != NULL && pThis->pNvComp->pCallbacks->FillBufferDone != NULL)
            {
                while (NvSuccess == NvxListDeQ(pThis->pEmptyBuffers, (void *)&pBuffer))
                {
                    NvxCheckError(eError, pThis->pNvComp->pCallbacks->FillBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
                    if (NvxIsError(eError))
                    {
                        goto FLUSH_FAILED;
                    }
                }
            }
        
            /* dump any "bullet in the chamber" */
            if (pThis->pCurrentBufferHdr)
            {
                pThis->pCurrentBufferHdr->nFilledLen = 0;
                pBuffer = pThis->pCurrentBufferHdr;
                if (pThis->pNvComp->pCallbacks)
                {
                    if (pThis->oPortDef.eDir == OMX_DirOutput && pThis->pNvComp->pCallbacks->FillBufferDone != NULL)
                    {
                        pBuffer->nFilledLen = 0;
                        NvxCheckError(eError, pThis->pNvComp->pCallbacks->FillBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
                        pThis->pCurrentBufferHdr = NULL;
                        if (NvxIsError(eError))
                        {
                            goto FLUSH_FAILED;
                        }
                    }
                }
            }
        }

        if (NvxPortNumPendingBuffers(pThis))
        {
            NvxCheckError(eError, NvxPortSendPendingBuffers(pThis));
        }        
    }
    return OMX_ErrorNone;
FLUSH_FAILED:
    NvMMQueueEnQ(pThis->pBuffersToSend, &pBuffer, OMX_TIMEOUT_MS);
    NVXTRACE((NVXT_ERROR, pThis->pNvComp->eObjectType, "[%s:%d] not "
      "ready, %s:%d (buffer: %p)\n", __FUNCTION__, __LINE__, pThis->pNvComp->pComponentName,
      pThis->oPortDef.nPortIndex, pBuffer));
    return OMX_ErrorNotReady;
}

/* Finished filling a buffer, deliver it */
OMX_ERRORTYPE NvxPortDeliverFullBuffer(NvxPort *pThis, OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError;
    OMX_U32 idx;
    if (pThis->oPortDef.eDir == OMX_DirInput)
        return OMX_ErrorBadParameter;
    if (!pBuffer)
        return OMX_ErrorBadParameter;

    NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortDeliverFullBuffer start %s:%d (buffer %p)\n",
              pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex, pBuffer));

    eError = FinishBufferHdrHandling(pThis, pBuffer);
    if (eError == OMX_ErrorIncorrectStateOperation)
    {
        NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortDeliverFullBuffer FinishBuffer failed "
                  "%s:%d (buffer %p error %x)\n", pThis->pNvComp->pComponentName,
                  pThis->oPortDef.nPortIndex, pBuffer, eError));
        return eError;
    }

    /* if sharing transfer internal ownership of shared buffer */
    if (pThis->nSharingPorts)
    {
        NvxPortBufferPtrToIdx(pThis, pBuffer->pBuffer, &idx);
        pThis->pbMySharedBuffer[idx] = OMX_TRUE;
        pThis->pSharingPorts[0]->pbMySharedBuffer[idx] = OMX_FALSE;
    }

    /* deliver buffer to client */
    if (pThis->hTunnelComponent != NULL)
    {
        NvxCheckError(eError, OMX_EmptyThisBuffer(pThis->hTunnelComponent, pBuffer));
        if (eError == OMX_ErrorNotReady)
        {
            NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortDeliverFullBuffer tunnel not "
                      "ready, queueing %s:%d (buffer: %p)\n", pThis->pNvComp->pComponentName,
                      pThis->oPortDef.nPortIndex, pBuffer));

            NvMMQueueEnQ(pThis->pBuffersToSend, &pBuffer, OMX_TIMEOUT_MS);
            eError = OMX_ErrorNone;
        }
    }
    else
        NvxCheckError(eError, pThis->pNvComp->pCallbacks->FillBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));

    NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortDeliverFullBuffer end %s:%d "
              "(buffer %p  error %x)\n", pThis->pNvComp->pComponentName, pThis->oPortDef.nPortIndex,
              pBuffer, eError));

    return eError;
}

int NvxPortNumPendingBuffers(NvxPort *pThis)
{
    if (!pThis->pBuffersToSend)
        return 0;
    return (int)NvMMQueueGetNumEntries(pThis->pBuffersToSend);
}

OMX_ERRORTYPE NvxPortSendPendingBuffers(NvxPort *pThis)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    if (!pThis || NvxPortNumPendingBuffers(pThis) <= 0)
        return eError;

    while (NvxPortNumPendingBuffers(pThis) > 0)
    {
        OMX_BUFFERHEADERTYPE *pBuffer;

        NvMMQueuePeek(pThis->pBuffersToSend, &pBuffer);
        if (!pBuffer)
            break;

        if (pThis->hTunnelComponent != NULL)
        {
            if (pThis->oPortDef.eDir == OMX_DirInput)
                NvxCheckError(eError, OMX_FillThisBuffer(pThis->hTunnelComponent, pBuffer));
            else
                NvxCheckError(eError, OMX_EmptyThisBuffer(pThis->hTunnelComponent, pBuffer));
        }
        else if (pThis->pNvComp->pCallbacks)
        {
            if (pThis->oPortDef.eDir == OMX_DirInput)
                NvxCheckError(eError, pThis->pNvComp->pCallbacks->EmptyBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
            else
                NvxCheckError(eError, pThis->pNvComp->pCallbacks->FillBufferDone(pThis->pNvComp->hBaseComponent, pThis->pNvComp->pCallbackAppData, pBuffer));
        }

        NVXTRACE((NVXT_BUFFER, pThis->pNvComp->eObjectType, "PortSendPendingBuffers %s:%d "
                  "(buffer %p  error %x)\n", pThis->pNvComp->pComponentName, 
                  pThis->oPortDef.nPortIndex, pBuffer, eError));

        if (OMX_ErrorNone == eError)
            NvMMQueueDeQ(pThis->pBuffersToSend, &pBuffer);
        else
            break;
    }

    return eError;
}

/* Copy marks, timestamps, flags, etc between current buffers */
OMX_ERRORTYPE NvxPortCopyMetadata(NvxPort *pInPort, NvxPort *pOutPort)
{
    OMX_BUFFERHEADERTYPE* pInHdr = pInPort->pCurrentBufferHdr;
    OMX_BUFFERHEADERTYPE* pOutHdr = pOutPort->pCurrentBufferHdr;
    pOutHdr->hMarkTargetComponent = pInHdr->hMarkTargetComponent;
    pOutHdr->pMarkData = pInHdr->pMarkData;
    pOutHdr->nTimeStamp = pInHdr->nTimeStamp;
    pOutHdr->nFlags = pInHdr->nFlags;

    pInPort->bSendEvents = OMX_FALSE;

    pOutPort->bSendEvents = OMX_TRUE;
    pOutPort->uMetaDataCopyCount = 0;
    pOutPort->pMetaDataSource = pInPort->pMetaDataSource; /* this is for marks, so we can send on last output */

    pOutPort->pMetaDataSource->uMetaDataCopyCount++;
    return OMX_ErrorNone;
}

/* Stop the port, return all buffers, release any resources */
OMX_ERRORTYPE NvxPortStop(OMX_IN NvxPort *pThis)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    pThis->bIsStopping = OMX_TRUE;

    if (pThis->oPortDef.bEnabled)
    {
        // this function only gets called from NvxDisablePort,
        // which is only called inside of code sections where
        // bWorkerMutex is locked, so we'll pass OMX_TRUE
        // directly here.  minimal code churn / high stability
        // was the chief consideration when this change was made.
        eError = NvxPortReturnBuffers(pThis, OMX_TRUE);
        NvxCheckError(eError, NvxPortReleaseResources(pThis));
    }

    if (pThis->nBuffers != 0)
        return OMX_ErrorNotReady;

    if (eError == OMX_ErrorNone)
    {
        pThis->oPortDef.bEnabled = OMX_FALSE;
        pThis->bIsStopping = OMX_FALSE;
    }    

    return eError;
}

/* Start the port, send buffers to be filled if we're the input/supplier */
OMX_ERRORTYPE NvxPortStart(OMX_IN NvxPort *pThis)
{
    OMX_U32 i;
    if (pThis->hTunnelComponent &&                              /* tunneling */
        (pThis->oPortDef.eDir == OMX_DirInput) &&               /* is input */
        (pThis->eSupplierSetting == OMX_BufferSupplyInput) &&   /* input is supplier */
        !pThis->nSharingPorts &&                               /* not sharing ports */
        !pThis->bStarted)                                       /* haven't already called PortStart */

    {
        OMX_BUFFERHEADERTYPE *pBuffer = NULL;


        if (NvMMQueueGetNumEntries(pThis->pFullBuffers) > 0)
        {
            // Exec->idle->Exec operations will lead to this code path.
            while (NvxPortNumPendingBuffers(pThis) > 0)
            {
                NvMMQueuePeek(pThis->pBuffersToSend, &pBuffer);
                pBuffer->nFilledLen = 0;
                if (OMX_ErrorNone != OMX_FillThisBuffer(pThis->hTunnelComponent, pBuffer))
                {
                    return OMX_ErrorNotReady;
                }
                NvMMQueueDeQ(pThis->pBuffersToSend, &pBuffer);
            }

            // sending through queue makes sure that no buffer is send twice.
            while (NvxIsSuccess(NvxPortGetFullBuffer(pThis, &pBuffer)))
            {
                pBuffer->nFilledLen = 0;
                if (OMX_ErrorNone != OMX_FillThisBuffer(pThis->hTunnelComponent, pBuffer))
                {
                    NvMMQueueEnQ(pThis->pBuffersToSend, &pBuffer, OMX_TIMEOUT_MS);
                    return OMX_ErrorNotReady;
                }
            }
        }
        else
        {
            // loaded->idle->exec will lead to this code path.
            for (i=0;i<pThis->nBuffers;i++)
            {
                if (OMX_ErrorNone != OMX_FillThisBuffer(pThis->hTunnelComponent, pThis->ppBufferHdrs[i]))
                {
                    return OMX_ErrorNotReady;
                }
            }
        }
    }

    pThis->bStarted = OMX_TRUE;
    return OMX_ErrorNone;
}

/* re-create the tunnel/buffers/etc */
OMX_ERRORTYPE NvxPortRestart(OMX_IN NvxPort *pThis, OMX_BOOL bIdle)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    if (!pThis->oPortDef.bPopulated)
    {
        eError = NvxPortCreateTunnel(pThis);
        if (NvxIsError(eError))
            return eError;
    }

    /* todo: anything else to do here? */
    if (!pThis->oPortDef.bPopulated)
        return OMX_ErrorNotReady;

    if (!bIdle)
    {   
        eError = NvxPortStart(pThis);
        if (eError != OMX_ErrorNone)
            pThis->oPortDef.bEnabled = OMX_FALSE;
    }
    if (eError == OMX_ErrorNone)
        pThis->oPortDef.bEnabled = OMX_TRUE;

    return eError;
}

OMX_ERRORTYPE NvxPortMarkBuffer(OMX_IN NvxPort *pThis, OMX_MARKTYPE* pMark)
{
    NvError err;
    if (pThis->pBufferMarks == NULL)
    {
        NVXTRACE((NVXT_ERROR,pThis->pNvComp->eObjectType, "ERROR at %s, %d\n", __FILE__, __LINE__));
        return OMX_ErrorIncorrectStateOperation;
    }
    err = NvMMQueueEnQ(pThis->pBufferMarks, pMark, OMX_TIMEOUT_MS);
    if (err == NvSuccess)
        return OMX_ErrorNone;
    return OMX_ErrorNotReady;
}

