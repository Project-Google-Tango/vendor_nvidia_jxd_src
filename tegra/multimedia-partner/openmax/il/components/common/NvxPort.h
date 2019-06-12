/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NVX_Port.h */

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Index.h>
#include <OMX_Image.h>
#include <OMX_Audio.h>
#include <OMX_Video.h>
#include <OMX_IVCommon.h>
#include <OMX_Other.h>
#include <NvxIndexExtensions.h>

#include "nvmm_queue.h"
#include "nvxlist.h"

#ifndef __NVX_PORT
#define __NVX_PORT

typedef struct _NvxComponent NvxComponent;
typedef struct _NvxPort NvxPort;

/** Maximum number of supported buffers per NvMMStream. */
#define NV_MAXBUFFERS 32

struct _NvxPort {
    NvxComponent            *pNvComp;            /* pointer to NvxComponent that owns this port */
    OMX_PARAM_PORTDEFINITIONTYPE  oPortDef;            
    void                    *pPortPrivate;       /* location for extra state component implementation may need */ 
    
    OMX_BUFFERHEADERTYPE    *pCurrentBufferHdr;  /* place to hold current buffer header (metadata, etc.) we're working with */

    OMX_BOOL                bSendEvents;         /* true if this port should send the Mark and BufferFlag events for current */
    OMX_U32                 uMetaDataCopyCount;  /* number of ports that share this metadata (in source only) */
    NvxPort*                pMetaDataSource;     /* pointer to source of this metadata */   

    OMX_U32                 nReqBufferCount;     /* requested number of buffers */
    OMX_U32                 nReqBufferSize;      /* requested size of buffers */

    OMX_U32                 nMinBufferCount;     /* min number of buffers */
    OMX_U32                 nMinBufferSize;      /* min size of buffers */
    OMX_U32                 nMaxBufferCount;     /* max number of buffers */
    OMX_U32                 nMaxBufferSize;      /* max size of buffers */
    OMX_U32                 nNonTunneledBufferSize; 

    NvMMQueueHandle          pFullBuffers;        /* buffers that have been filled in */
    NvxListHandle            pEmptyBuffers;       /* buffers that are empty */
    NvMMQueueHandle          pBufferMarks;        /* buffers that are empty */

    OMX_HANDLETYPE          hTunnelComponent;    /* handle to component to on other side of tunnel (0 if no tunneling) */
    OMX_U32                 nTunnelPort;         /* port index for other side fo tunnel (0 if no tunneling) */

    OMX_BUFFERSUPPLIERTYPE  eSupplierPreference; /* true if and only if this port is a supplier */
    OMX_BUFFERSUPPLIERTYPE  eSupplierSetting;    /* true if and only if this port is a supplier */
    OMX_BUFFERSUPPLIERTYPE  eSupplierTieBreaker; /* tie breaker policy for this port */

    /* Buffer sharing variables. 
     *  nSharingCandidates and pSharingCandidates indicate what buffer sharing relationship this port 
     *     is capable of. The component will examine this information when deciding on actual sharing
     *     relationships once supplier and non-supplier relationships are established.
     *  nSharingPorts and pSharingPorts indicate what sharing this port is currently using. This 
     *     information is the result of the decisions the component made about sharing after the
     *     supplier and non-supplier relationships were established.
     */
    OMX_U32                 nSharingCandidates;     /* number of ports that this port might possibly share with */ 
    NvxPort                 *pSharingCandidates[8]; /* pointer to all ports that this port might possibly share with */
    OMX_U32                 nSharingPorts;          /* number of ports that this port is actually sharing with */
    NvxPort                 *pSharingPorts[8];      /* pointer to all ports that this port is actually sharing with */
    OMX_BOOL                *pbMySharedBuffer;      /* for each shared buffer number pbMySharedBuffer is true only when we
                                                     * own said buffer */
    OMX_BOOL                bShareIfNvidiaTunnel;   /* only enable sharing if we're doing nvidia-specific tunneling on the output port */

    OMX_U32                 nBuffers;            /* number of actual buffers */
    OMX_U32                 nMaxBuffers;         /* maximum number of buffers headers we can allocate */
    OMX_BUFFERHEADERTYPE    **ppBufferHdrs;
    OMX_U8                  **ppBuffers;         /* must maintain this separate from headers to dealloc safely */
                                                 /* if we're supplier, then the non-supplier might be deallocing */
                                                 /* headers at the same time that we're deallocating buffers */ 
                                                 /* Thus header contents are unreliable at dealloc time. */
    void                    *ppRmSurf[NV_MAXBUFFERS];
    OMX_HANDLETYPE          hMutex;              /* protects re-entrant port implementation code */

    OMX_BOOL                  bNvidiaTunneling;  /* Vendor specific (NVIDIA) tunneling support */
    ENvxTunnelTransactionType eNvidiaTunnelTransaction;

    OMX_BOOL                bStarted;
    OMX_BOOL                bIsStopping;
    OMX_BOOL                bAllocateRmSurf;

    NvMMQueueHandle          pBuffersToSend;
};

/* Init functions for the various OpenMax domains.  Destroy works on all types */
OMX_ERRORTYPE NvxPortInitOther(NvxPort *pPort, OMX_DIRTYPE eDir, OMX_U32 nBufferCount, OMX_U32 nBufferSize,
                               OMX_OTHER_FORMATTYPE eFormat);
OMX_ERRORTYPE NvxPortInitAudio(NvxPort *pPort, OMX_DIRTYPE eDir, OMX_U32 nBufferCount, OMX_U32 nBufferSize,
                               OMX_AUDIO_CODINGTYPE eCodingType);
OMX_ERRORTYPE NvxPortInitVideo(NvxPort *pPort, OMX_DIRTYPE eDir, OMX_U32 nBufferCount, OMX_U32 nBufferSize,
                               OMX_VIDEO_CODINGTYPE eCodingType);
OMX_ERRORTYPE NvxPortInitImage(NvxPort *pPort, OMX_DIRTYPE eDir, OMX_U32 nBufferCount, OMX_U32 nBufferSize,
                               OMX_IMAGE_CODINGTYPE eCodingType);
OMX_ERRORTYPE NvxPortDestroy(NvxPort *);

OMX_ERRORTYPE NvxPortSetNonTunneledSize(NvxPort *pPort, OMX_U32 nBufSize);

/* Following functions are mainly for use by the component base */
OMX_BOOL NvxPortIsSupplier(OMX_IN NvxPort *pPort);
OMX_ERRORTYPE NvxPortAllocResources(NvxPort *);
OMX_ERRORTYPE NvxPortDecideSharing(NvxPort *pThis);
OMX_ERRORTYPE NvxPortCreateTunnel(NvxPort *);
OMX_ERRORTYPE NvxPortReleaseResources(NvxPort *);

OMX_ERRORTYPE NvxPortSendPendingBuffers(NvxPort *pThis);

OMX_ERRORTYPE NvxPortEmptyThisBuffer(NvxPort *pPort, OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE NvxPortFillThisBuffer(NvxPort *pPort, OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE NvxPortFlushBuffers(OMX_IN NvxPort *pPort);
OMX_ERRORTYPE NvxPortReturnBuffers(OMX_IN NvxPort *pPort, OMX_BOOL bWorkerLocked);

OMX_ERRORTYPE NvxPortStop(OMX_IN NvxPort *pPort);
OMX_ERRORTYPE NvxPortStart(OMX_IN NvxPort *pPort);
OMX_ERRORTYPE NvxPortRestart(OMX_IN NvxPort *pPort, OMX_BOOL bIdle);

/* May be of use in individual components */
OMX_ERRORTYPE NvxPortMarkBuffer(OMX_IN NvxPort *pThis, OMX_MARKTYPE* pMark);
OMX_ERRORTYPE NvxPortCopyMetadata(NvxPort *pInPort, NvxPort *pOutPort);
OMX_ERRORTYPE NvxPortGetEmptyBuffer(NvxPort *pPort, OMX_OUT OMX_BUFFERHEADERTYPE** ppBuffer);
OMX_ERRORTYPE NvxPortGetFullBuffer(NvxPort *pPort, OMX_OUT OMX_BUFFERHEADERTYPE** ppBuffer);
OMX_ERRORTYPE NvxPortReleaseBuffer(NvxPort *pPort, OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE NvxPortDeliverFullBuffer(NvxPort *pPort, OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);
OMX_BOOL NvxPortGetNextHdr(NvxPort *pPort);

int NvxPortNumPendingBuffers(NvxPort *pPort);

OMX_BOOL NvxPortIsReusingBuffers(NvxPort *pThis);
OMX_BOOL NvxPortIsAllocator(NvxPort *pThis);
int NvxPortCountBuffersSharedAndDelivered(NvxPort *pThis);

#define NvxPortTrigger(_pPort_)   NvxComponentTrigger((_pPort_)->pNvComp)
#define NvxPortIsReady(_pPort_)   ((OMX_BOOL) ((_pPort_)->oPortDef.bEnabled && !(_pPort_)->bIsStopping && (_pPort_)->oPortDef.bPopulated))
#define NvxPortAcceptingBuffers(_pPort_)   ((OMX_BOOL) (NvxPortIsReady(_pPort_) ||((_pPort_)->oPortDef.bPopulated && NvxPortIsSupplier(_pPort_))))

#endif /*__NVX_PORT*/

/* File EOF */

