/* Copyright (c) 2006-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NVX_Component.h */

#ifndef NVXCOMPONENT_H_
#define NVXCOMPONENT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Index.h>
#include <OMX_Image.h>
#include <OMX_Audio.h>
#include <OMX_Video.h>
#include <OMX_IVCommon.h>
#include <OMX_Other.h>
#include <NVOMX_IndexExtensions.h>

#include <NvxPort.h>
#include <NvxScheduler.h>
#include <NvxMutex.h>
#include <NvxTrace.h>

#include "nvos.h"
#include "nvmm_queue.h"

/* Max size is the maximum of any command */
#define MAX_COMMANDDATA_SIZE sizeof(OMX_MARKTYPE)
#define NVX_SUPPLIERTIEBREAKER OMX_BufferSupplyInput

typedef struct {
    OMX_COMMANDTYPE Cmd;
    OMX_U32 nParam1;
    OMX_U8 oCmdData[MAX_COMMANDDATA_SIZE];
} NvxCommand;

#define NVX_COMPONENT_MAX_RM_HANDLES 16

extern OMX_VERSIONTYPE NvxComponentSpecVersion;
extern OMX_VERSIONTYPE NvxComponentReleaseVersion;

typedef enum ENvxSetConfigState
{
    NVX_SETCONFIGSTATE_NOTHINGPENDING,
    NVX_SETCONFIGSTATE_PENDING,
    NVX_SETCONFIGSTATE_PROCESSING,
    NVX_SETCONFIGSTATE_COMPLETE,
} ENvxSetConfigState;

typedef enum ENvxResourcesState
{
    NVX_RESOURCES_HAVENONE,     /* Component has no resources and hasn't requested any */
    NVX_RESOURCES_REQUESTED,    /* Component has requested resources */
    NVX_RESOURCES_HAVESOME,     /* Component has some of the requested resources */
    NVX_RESOURCES_PROVIDED,     /* Resource manager has provided resources that the component needs to accept */
    NVX_RESOURCES_HAVEALL,      /* Component has all resources it needs */
    NVX_RESOURCES_RECLAIMED,    /* Resource manager needs the resources back */
} ENvxResourcesState;

/* Maximum roles a component can support. */
#define NVX_COMPONENT_MAX_ROLES 16

struct _NvxComponent {
    OMX_HANDLETYPE          hBaseComponent;
    NvxWorker               oWorkerData;

    OMX_U32                 nPorts;
    NvxPort                 *pPorts;

    OMX_CALLBACKTYPE        *pCallbacks;
    OMX_PTR                 pCallbackAppData;

    OMX_STRING              pComponentName;
    OMX_VERSIONTYPE         oComponentVersion;
    OMX_VERSIONTYPE         oSpecVersion;
    OMX_UUIDTYPE            oUUID;

    void                    *pComponentData;
    OMX_BOOL                bEnableUlpMode;

    OMX_STATETYPE           eState;
    OMX_STATETYPE           ePendingState;
    NvMMQueueHandle          pCommandQueue;

    OMX_HANDLETYPE          hWorkerMutex;

    OMX_HANDLETYPE          hConfigMutex;
    NvOsSemaphoreHandle     hSetConfigEvent;
    ENvxSetConfigState      eSetConfigState;
    OMX_INDEXTYPE           nPendingConfigIndex;
    OMX_PTR                 pPendingConfigStructure;
    OMX_ERRORTYPE           eSetConfigError;
    OMX_HANDLETYPE          oPendingConfigEvent;
    OMX_PRIORITYMGMTTYPE    oPriorityState;

    ENvxResourcesState      eResourcesState;
    OMX_U32                 nRmResources;
    OMX_U32                 nRmResourcesAcquired;
    OMX_U32                 uRmIndex[NVX_COMPONENT_MAX_RM_HANDLES];
    OMX_HANDLETYPE          hRmHandle[NVX_COMPONENT_MAX_RM_HANDLES];

    OMX_BOOL                bWasExecuting;
    ENvxtObjectType         eObjectType;

    OMX_STRING              sComponentRoles[NVX_COMPONENT_MAX_ROLES];   // each string limited to 128 bytes
    OMX_U32                 nComponentRoles;
    OMX_STRING              sCurrentComponentRole;

    OMX_BOOL                bMajorStateChangePending;

    OMX_BOOL                bNeedRunWorker;

    OMX_BOOL                bCanUsePartialFrame;

    OMX_U32                 nCanStopCnt;

    /* Function protos (mirrors openmax component) that each component can override */
    OMX_ERRORTYPE (*GetParameter)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure);
    OMX_ERRORTYPE (*SetParameter)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure);
    OMX_ERRORTYPE (*GetConfig)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure);
    OMX_ERRORTYPE (*SetConfig)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure);
    OMX_ERRORTYPE (*GetExtensionIndex)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE* pIndexType);
    OMX_ERRORTYPE (*DeInit)(OMX_IN NvxComponent *pComponent);
    OMX_ERRORTYPE (*EmptyThisBuffer)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BUFFERHEADERTYPE* pBufferHdr, OMX_OUT OMX_BOOL *bHandled);
    OMX_ERRORTYPE (*Flush)(OMX_IN NvxComponent *pComponent, OMX_U32 nPort);
    OMX_ERRORTYPE (*ReturnBuffers)(OMX_IN NvxComponent *pComponent, OMX_U32 nPort, OMX_BOOL bWorkerLocked);

    /* New functions that a component may override */
    /* Tests if a component can use a buffer on a given port.  Returns OMX_ErrorNone if it can. */
    OMX_ERRORTYPE (*CanUseBuffer)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_U32 nPortIndex, OMX_IN OMX_U32 nSizeBytes, OMX_IN OMX_U8* pBuffer);
    /* Tests if a component can use an eglImage as a buffer on a given port.  Returns OMX_ErrorNone if it can. */
    OMX_ERRORTYPE (*CanUseEGLImage)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_U32 nPortIndex, OMX_IN void* eglImage);
    /* Tests if ports are compatible for tunneling. Returns OMX_ErrorNone if it can */
    OMX_ERRORTYPE (*CompatiblePorts)(OMX_IN NvxComponent *pComponent, NvxPort *pPort, OMX_HANDLETYPE hOtherComponent, OMX_U32 hOtherPort);

    /* Break up into produce/transform/consume? */
    OMX_ERRORTYPE (*WorkerFunction)(OMX_IN NvxComponent *pComponent,
                                    OMX_BOOL bAllPortsReady,            /* OMX_TRUE if all ports have a buffer ready */
                                    OMX_INOUT OMX_BOOL *pbMoreWork,      /* function should set to OMX_TRUE if there is more work */
                                    OMX_INOUT NvxTimeMs* puMaxMsecToNextCall);
    OMX_ERRORTYPE (*PreChangeState)(OMX_IN NvxComponent *pComponent,OMX_IN OMX_STATETYPE oNewState);
    OMX_ERRORTYPE (*ChangeState)(OMX_IN NvxComponent *pComponent,OMX_IN OMX_STATETYPE oNewState);
    /* Command actions, which a component may want to know about */
    OMX_ERRORTYPE (*AcquireResources)(OMX_IN NvxComponent *pComponent);
    OMX_ERRORTYPE (*ReleaseResources)(OMX_IN NvxComponent *pComponent);
    OMX_ERRORTYPE (*WaitForResources)(OMX_IN NvxComponent *pComponent);
    OMX_ERRORTYPE (*CancelWaitForResources)(OMX_IN NvxComponent *pComponent);
    OMX_ERRORTYPE (*PortEventHandler)(OMX_IN NvxComponent *pComponent, OMX_U32 uPortIndex, OMX_U32 uEventType);
    OMX_ERRORTYPE (*FillThisBufferCB)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BUFFERHEADERTYPE *pBufferHdr);
    OMX_ERRORTYPE (*PreCheckChangeState)(OMX_IN NvxComponent *pComponent,OMX_IN OMX_STATETYPE oNewState); // called by the SendCommand function
    OMX_ERRORTYPE (*ComponentTunnelRequest)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_U32 nPort,
                                            OMX_IN OMX_HANDLETYPE hTunneledComp, OMX_IN OMX_U32 nTunneledPort,
                                            OMX_INOUT OMX_TUNNELSETUPTYPE* pTunnelSetup);
    OMX_ERRORTYPE (*FreeBufferCB)(OMX_IN NvxComponent *pComponent, OMX_IN OMX_BUFFERHEADERTYPE *pBufferHdr);
};

/* Base implementations of component specific functions */


OMX_ERRORTYPE NvxComponentBaseGetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN  OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure);
OMX_ERRORTYPE NvxComponentBaseSetConfig(OMX_IN NvxComponent* pNvComp, OMX_IN  OMX_INDEXTYPE nIndex, OMX_IN  OMX_PTR pComponentConfigStructure);
OMX_ERRORTYPE NvxComponentBaseGetParameter(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure);
OMX_ERRORTYPE NvxComponentBaseSetParameter(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure);
OMX_ERRORTYPE NvxComponentBaseGetExtensionIndex(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE* pIndexType);
OMX_ERRORTYPE NvxComponentBaseCompatiblePorts(OMX_IN NvxComponent *pComponent, NvxPort *pPort, OMX_HANDLETYPE hOtherComponent, OMX_U32 hOtherPort);

OMX_BOOL NvxComponentCheckPortCompatibility(OMX_IN OMX_PARAM_PORTDEFINITIONTYPE *pPortDef1, OMX_IN OMX_PARAM_PORTDEFINITIONTYPE *pPortDef2);

/* Utility functions */
#define NvxComponentFromHandle(_hComponent_)  ((NvxComponent*)(((OMX_COMPONENTTYPE *)(_hComponent_))->pComponentPrivate))
#define NvxComponentTrigger(_pNvComp_)   NvxWorkerTrigger(&((_pNvComp_)->oWorkerData))

#define ALLOC_STRUCT(_pNvComp_, _pStruct_, _oType_) \
    (_pStruct_) = (_oType_ *)NvOsAlloc(sizeof(_oType_)); \
    if (_pStruct_) \
    { \
        NvOsMemset(_pStruct_, 0, sizeof(_oType_)); \
        (_pStruct_)->nSize = sizeof(_oType_); \
        (_pStruct_)->nVersion = (_pNvComp_)->oSpecVersion; \
    }

OMX_ERRORTYPE NvxSendEvent(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_EVENTTYPE eEvent,
                           OMX_IN OMX_U32 nData1, OMX_IN OMX_U32 nData2, OMX_IN OMX_PTR pEventData);

OMX_ERRORTYPE NvxRetrySetConfig(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure);

/* Implementations of OMX component functions */
OMX_ERRORTYPE NvxComponentCreate(OMX_HANDLETYPE hComponent, int nPorts, NvxComponent** ppComponent);
OMX_ERRORTYPE NvxComponentDestroy(OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE NvxSetCallbacks(OMX_IN  OMX_HANDLETYPE hComponent, OMX_IN  OMX_CALLBACKTYPE* pCallbacks, OMX_IN  OMX_PTR pAppData);
OMX_ERRORTYPE NvxGetComponentVersion(OMX_IN OMX_HANDLETYPE hComponent, OMX_OUT OMX_STRING pComponentName, OMX_OUT OMX_VERSIONTYPE* pComponentVersion, OMX_OUT OMX_VERSIONTYPE* pSpecVersion, OMX_OUT OMX_UUIDTYPE* pComponentUUID);
OMX_ERRORTYPE NvxSendCommand(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_COMMANDTYPE Cmd, OMX_IN OMX_U32 nParam1, OMX_IN OMX_PTR pCmdData );
OMX_ERRORTYPE NvxGetParameter(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure);
OMX_ERRORTYPE NvxSetParameter(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentParameterStructure);
OMX_ERRORTYPE NvxGetConfig(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_INOUT OMX_PTR pComponentConfigStructure);
OMX_ERRORTYPE NvxSetConfig(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure);
OMX_ERRORTYPE NvxGetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE* pIndexType);
OMX_ERRORTYPE NvxGetState(OMX_IN OMX_HANDLETYPE hComponent, OMX_OUT OMX_STATETYPE* pState);
OMX_ERRORTYPE NvxComponentTunnelRequest(OMX_IN  OMX_HANDLETYPE hComp, OMX_IN  OMX_U32 nPort, OMX_IN  OMX_HANDLETYPE hTunneledComp, OMX_IN  OMX_U32 nTunneledPort, OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup);

OMX_ERRORTYPE NvxUseBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_IN OMX_U32 nPortIndex, OMX_IN OMX_PTR pAppPrivate, OMX_IN OMX_U32 nSizeBytes, OMX_IN OMX_U8* pBuffer);
OMX_ERRORTYPE NvxAllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_IN OMX_U32 nPortIndex, OMX_IN OMX_PTR pAppPrivate, OMX_IN OMX_U32 nSizeBytes);
OMX_ERRORTYPE NvxFreeBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_U32 nPortIndex, OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE NvxEmptyThisBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE NvxFillThisBuffer(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE NvxBaseWorkerFunction(OMX_IN NvxWorker* pWorker, OMX_IN NvxTimeMs uMaxTime, OMX_INOUT OMX_BOOL* pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall);
OMX_ERRORTYPE NvxComponentRoleEnum(OMX_IN OMX_HANDLETYPE hComponent, OMX_OUT OMX_U8 *cRole, OMX_IN OMX_U32 nIndex);
OMX_ERRORTYPE NvxUseEGLImage(OMX_IN OMX_HANDLETYPE hComponent, OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_IN OMX_U32 nPortIndex, OMX_IN OMX_PTR pAppPrivate,  OMX_IN void* eglImage);

OMX_ERRORTYPE NvxUseBufferInt(OMX_HANDLETYPE hComponent,
                              OMX_BUFFERHEADERTYPE** ppBufferHdr,
                              OMX_U32 nPortIndex,
                              OMX_PTR pAppPrivate,
                              OMX_U32 nSizeBytes,
                              OMX_U8* pBuffer,
                              NvxBufferType oBufferType);

/* resource manager calls: this interface *will* change, so don't use it unless absolutely necessary */
OMX_ERRORTYPE NvxComponentBaseOnResourceAcquired(OMX_IN OMX_PTR pConsumerData,
                                                 OMX_IN OMX_U32 eType,
                                                 OMX_IN OMX_HANDLETYPE *phResource);
OMX_ERRORTYPE NvxComponentBaseOnResourceReclaimed(OMX_IN OMX_PTR pConsumerData,
                                               OMX_IN OMX_U32 eType,
                                                  OMX_IN OMX_HANDLETYPE *phResource);
OMX_ERRORTYPE NvxComponentBaseAcquireResources(OMX_IN NvxComponent *pComponent);
OMX_ERRORTYPE NvxComponentBaseWaitForResources(OMX_IN NvxComponent *pComponent);
OMX_ERRORTYPE NvxComponentBaseCancelWaitForResources(OMX_IN NvxComponent *pComponent);
OMX_ERRORTYPE NvxComponentBaseReleaseResources(OMX_IN NvxComponent *pComponent);

/* bypass adding components to the scheduler */
void NvxAddCompsToSched(OMX_BOOL bRunSched);

/* Clock bypass helper functions */
void NvxCCSetCurrentAudioRefClock(OMX_HANDLETYPE hComponent,
                                  OMX_TICKS mtAudioTS);
OMX_BOOL NvxCCWaitUntilTimestamp(OMX_HANDLETYPE hComponent,
                                 OMX_TICKS mtTarget, OMX_BOOL *bDropFrame,
                                 OMX_BOOL *bVideoBehind);
void NvxCCSetWaitForAudio(OMX_HANDLETYPE hComponent);
void NvxCCSetWaitForVideo(OMX_HANDLETYPE hComponent);
void NvxCCWaitForVideoToStart(OMX_HANDLETYPE hComponent);
void NvxCCUnblockWaitUntilTimestamp(OMX_HANDLETYPE hComponent);
void NvxCCGetClockHandle(OMX_HANDLETYPE hComponent, void *hClock);
OMX_S32 NvxCCGetClockRate(OMX_HANDLETYPE hComponent);

typedef enum NVX_ResourceNum
{
    NVX_THREAD_RES = 0,
    NVX_AUDIO_MEDIAPROCESSOR_RES,
    NVX_AUDIO_RENDERER_RES,
    NVX_JPG_DECODER_RES,
    NVX_NUM_RESOURCES /* Keep this last */
} NVX_ResourceNum;

void NvxAddResource(NvxComponent *pNvComp, NVX_ResourceNum nResource);

#define NVX_BUFFERFLAG_DECODERSLOW    0x00010000
#define NVX_BUFFERFLAG_DECODERNOTSLOW 0x00020000

#endif

