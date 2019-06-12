/* Copyright (c) 2006-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NvxComponent.c */


#include "NvxComponent.h"
#include "NvxPort.h"
#include "NvxError.h"
#include <stdlib.h>
#include <string.h>
#include "NvxIndexExtensions.h"
#include "NvxHelpers.h"
#include "NvxResourceManager.h"
#include "NvxScheduler.h"
#include "NvxTrace.h"
#include "nvxeglimage.h"
#include "nvxegl.h"

#include "nvassert.h"

#define NVX_COMPONENT_PRINT(...)
//#define NVX_COMPONENT_PRINT(...) NvOsDebugPrintf(__VA_ARGS__)

OMX_VERSIONTYPE NvxComponentSpecVersion = {{1,1,0,0}};    /* 1.1 */

OMX_VERSIONTYPE NvxComponentReleaseVersion = {{1,1,0,0}};

#define OMX_TIMEOUT_MS 2000

typedef struct
{
    OMX_U32 nInstantiations;
    OMX_U32 nMax;
    OMX_STRING sName;
} NvxResourceStruct;

static NvxResourceStruct s_oRealResource[NVX_NUM_RESOURCES] = {
    { 0, 256, "Nvx.SW.ActiveThreads" },
    { 0,  32, "Nvx.HW.AudioMediaProcessors" },
    { 0,  32, "Nvx.HW.AudioRenderers" },
    { 0, 15, "Nvx.HW.ImageDecoders" }};

static NvxRmResourceIndex s_nRealResourceIndex[NVX_NUM_RESOURCES] =
    { 0, 0, 0 };

static OMX_HANDLETYPE hResourceLock = 0;

static OMX_ERRORTYPE GetRealResource(OMX_PTR pProviderData,
                                     NvxRmRequestHeader *pRequest,
                                     OMX_HANDLETYPE *phResource)
{
    OMX_ERRORTYPE eError;
    NvxResourceStruct *pRealResource = (NvxResourceStruct *)pProviderData;

    if (!pRealResource)
        return OMX_ErrorBadParameter;

    NvxMutexLock(hResourceLock);

    if (pRealResource->nInstantiations < pRealResource->nMax)
    {
        pRealResource->nInstantiations++;
        *phResource = (OMX_HANDLETYPE)(pRealResource->nInstantiations);
        eError = OMX_ErrorNone;
    }
    else
    {
        *phResource = (OMX_HANDLETYPE)(0);
        eError = OMX_ErrorInsufficientResources;
    }

    NvxMutexUnlock(hResourceLock);
    return eError;
}

static OMX_ERRORTYPE ReleaseRealResource(OMX_PTR pProviderData,
                                         OMX_HANDLETYPE hResource)
{
    OMX_ERRORTYPE eError = OMX_ErrorUndefined;
    NvxResourceStruct *pRealResource = (NvxResourceStruct *)pProviderData;

    NvxMutexLock(hResourceLock);

    if (pRealResource->nInstantiations)
    {
        pRealResource->nInstantiations--;
        eError = OMX_ErrorNone;
    }

    NvxMutexUnlock(hResourceLock);
    return eError;
}

static OMX_ERRORTYPE CheckOMXVersion(OMX_VERSIONTYPE nVerA,
                                     OMX_VERSIONTYPE nVerB)
{
    if (nVerA.s.nVersionMajor != nVerB.s.nVersionMajor)
        return OMX_ErrorVersionMismatch;
    if (nVerA.s.nVersionMinor > nVerB.s.nVersionMinor)
    {
        NvOsDebugPrintf("OMX minor version mismatch\n");
    }
    return OMX_ErrorNone;
}

static int s_nInits = 0;

static void NvxComponentGlobalInit(void)
{
    if (!hResourceLock)
        NvxMutexCreate(&hResourceLock);

    NvxMutexLock(hResourceLock);

    if (++s_nInits == 1) {
        NvxRmResourceProvider oProvider;
        int i;

        /* initialize resource manager */
        NvxRmInit();

        for (i = 0; i < NVX_NUM_RESOURCES; i++)
        {
            oProvider.GetResource = GetRealResource;
            oProvider.ReleaseResource = ReleaseRealResource;

            NvxRmSetResourceProvider(s_oRealResource[i].sName, &oProvider,
                                     &s_oRealResource[i],
                                     &s_nRealResourceIndex[i]);
        }
    }

    NvxMutexUnlock(hResourceLock);
}

static void NvxComponentGlobalDeInit(void)
{
    if (!hResourceLock)
        return;

    NvxMutexLock(hResourceLock);
    if (--s_nInits == 0) {
        NvxRmDeInit();
        NvxMutexUnlock(hResourceLock);
        NvxMutexDestroy(hResourceLock);
        hResourceLock = NULL;
    }
    else {
        NvxMutexUnlock(hResourceLock);
    }
}

OMX_ERRORTYPE NvxSetCallbacks(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
            OMX_IN  OMX_PTR pAppData)
{
    NvxComponent *pNvComp;

    if (!hComponent)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxSetCallbacks:"
                  ":hComponent = (%d):[%s(%d)]\n",hComponent,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }
    pNvComp = NvxComponentFromHandle(hComponent);
    if (!pNvComp->pCallbacks)
        pNvComp->pCallbacks = NvOsAlloc(sizeof(OMX_CALLBACKTYPE));

    *(pNvComp->pCallbacks) = *pCallbacks;
    pNvComp->pCallbackAppData = pAppData;

    return OMX_ErrorNone;
}

static OMX_BOOL g_bAddToScheduler = OMX_TRUE;

void NvxAddCompsToSched(OMX_BOOL bRunSched)
{
    g_bAddToScheduler = bRunSched;
}

void NvxAddResource(NvxComponent *pNvComp, NVX_ResourceNum nResource)
{
    if (nResource >= NVX_NUM_RESOURCES)
        return;

    pNvComp->uRmIndex[pNvComp->nRmResources] = s_nRealResourceIndex[nResource];
    pNvComp->nRmResources++;
}

OMX_ERRORTYPE NvxComponentCreate(OMX_HANDLETYPE hComponent, int nPorts, NvxComponent** ppComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pComp;
    NvxComponent *pNvComp;
    int nPort;
    OMX_U32 nRoleIndex;


    NvxComponentGlobalInit();
    pComp = (OMX_COMPONENTTYPE *)hComponent;

    /* fill in function pointers */
    pComp->SetCallbacks = NvxSetCallbacks;
    pComp->GetComponentVersion = NvxGetComponentVersion;
    pComp->SendCommand = NvxSendCommand;
    pComp->GetParameter = NvxGetParameter;
    pComp->SetParameter = NvxSetParameter;
    pComp->GetConfig = NvxGetConfig;
    pComp->SetConfig = NvxSetConfig;
    pComp->GetExtensionIndex = (void*)NvxGetExtensionIndex;
    pComp->GetState = NvxGetState;
    pComp->ComponentTunnelRequest = NvxComponentTunnelRequest;
    pComp->UseBuffer = NvxUseBuffer;
    pComp->AllocateBuffer = NvxAllocateBuffer;
    pComp->FreeBuffer = NvxFreeBuffer;
    pComp->EmptyThisBuffer = NvxEmptyThisBuffer;
    pComp->FillThisBuffer = NvxFillThisBuffer;
    pComp->ComponentDeInit = NvxComponentDestroy;
    pComp->UseEGLImage = NvxUseEGLImage;
    pComp->ComponentRoleEnum = NvxComponentRoleEnum;

    /* create and initialize internal state for component */
    *ppComponent = pNvComp = NvOsAlloc(sizeof(NvxComponent));
    if(!pNvComp)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxComponentCreate:"
                  ":pNvComp = %d:[%s(%d)]\n",pNvComp,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }
    NvOsMemset(pNvComp, 0, sizeof(NvxComponent));

    pComp->pComponentPrivate = pNvComp;

    pNvComp->GetParameter = NvxComponentBaseGetParameter;
    pNvComp->SetParameter = NvxComponentBaseSetParameter;
    pNvComp->GetConfig = NvxComponentBaseGetConfig;
    pNvComp->SetConfig = NvxComponentBaseSetConfig;
    pNvComp->GetExtensionIndex = NvxComponentBaseGetExtensionIndex;
    pNvComp->CompatiblePorts = NvxComponentBaseCompatiblePorts;
    pNvComp->AcquireResources = NvxComponentBaseAcquireResources;
    pNvComp->ReleaseResources = NvxComponentBaseReleaseResources;
    pNvComp->WaitForResources = NvxComponentBaseWaitForResources;
    pNvComp->CancelWaitForResources = NvxComponentBaseCancelWaitForResources;
    pNvComp->PortEventHandler = NULL;
    pNvComp->EmptyThisBuffer = NULL;
    pNvComp->Flush = NULL;
    pNvComp->ReturnBuffers = NULL;
    pNvComp->ChangeState = NULL;
    pNvComp->PreChangeState = NULL;
    pNvComp->hBaseComponent = hComponent;
    pNvComp->nPorts = nPorts;
    pNvComp->pPorts = NvOsAlloc(pNvComp->nPorts * sizeof(NvxPort));
    if (!pNvComp->pPorts)
    {
        NvOsFree(pNvComp);
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxComponentCreate:Out of memory[%s(%d)]\n",
            __FILE__, __LINE__));
      return OMX_ErrorInsufficientResources;
    }

    NvOsMemset(pNvComp->pPorts, 0, (pNvComp->nPorts * sizeof(NvxPort)));

    for(nPort = 0; nPort < nPorts; ++nPort){
        pNvComp->pPorts[nPort].pNvComp = pNvComp;
        pNvComp->pPorts[nPort].oPortDef.nPortIndex = nPort;
        if (OMX_ErrorNone != NvxMutexCreate( &pNvComp->pPorts[nPort].hMutex )) {
            pNvComp->pPorts[nPort].hMutex = 0;

            /* Explicitly set NVIDIA tunneling to false (use default mechanisms) */
            pNvComp->pPorts[nPort].bNvidiaTunneling         = OMX_FALSE;
            pNvComp->pPorts[nPort].eNvidiaTunnelTransaction = ENVX_TRANSACTION_DEFAULT;
        }

        /* all other fields should be set in NvxPortInit */
    }

    pNvComp->oSpecVersion.nVersion = NvxComponentSpecVersion.nVersion;
    pNvComp->oComponentVersion.nVersion = NvxComponentReleaseVersion.nVersion;

    pNvComp->eState = OMX_StateLoaded;
    pNvComp->ePendingState = OMX_StateLoaded;

    pNvComp->oPriorityState.nSize          = sizeof(OMX_PRIORITYMGMTTYPE);
    pNvComp->oPriorityState.nVersion       = pNvComp->oSpecVersion;
    pNvComp->oPriorityState.nGroupID       = 0xFFFFFFFF;
    pNvComp->oPriorityState.nGroupPriority = 0xFFFFFFFF;

    pNvComp->eResourcesState = NVX_RESOURCES_HAVENONE;
    NvOsMemset(pNvComp->uRmIndex, 0, sizeof pNvComp->hRmHandle);
    NvOsMemset(pNvComp->hRmHandle, 0, sizeof pNvComp->hRmHandle);

    pNvComp->nRmResourcesAcquired = 0;
    pNvComp->nRmResources = 0;
    NvxAddResource(pNvComp, NVX_THREAD_RES); /* require 1 resource: a thread */

    if(OMX_ErrorNone != NvxMutexCreate( &pNvComp->hConfigMutex )) {
        pNvComp->hConfigMutex = 0;
    }

    if(OMX_ErrorNone != NvxMutexCreate( &pNvComp->hWorkerMutex )) {
        pNvComp->hWorkerMutex = 0;
    }

    pNvComp->eSetConfigState = NVX_SETCONFIGSTATE_NOTHINGPENDING;
    if (NvSuccess != NvOsSemaphoreCreate(&pNvComp->hSetConfigEvent, 0))
        pNvComp->hSetConfigEvent = 0;

    NvOsMemset(pNvComp->oUUID, 0, sizeof(OMX_UUIDTYPE));
    /* Generate a UUID, put it in pNvComp->oUUID */
    /* Actual components should fill in the real version number. */

    /* No standard component roles by default. */
    /* No memory allocated yet for the component role strings. The component
       init function should do that. */
    pNvComp->nComponentRoles = 0;
    for (nRoleIndex = 0; nRoleIndex < NVX_COMPONENT_MAX_ROLES; nRoleIndex++)
        pNvComp->sComponentRoles[nRoleIndex] = NULL;

    /* FIXME: handle */
    eError = NvMMQueueCreate(&pNvComp->pCommandQueue, 32, sizeof(NvxCommand), NV_FALSE);

    /* FIXME: error handler, warning too */
    if (g_bAddToScheduler)
        NvxWorkerInit(&(pNvComp->oWorkerData), &(NvxBaseWorkerFunction), pNvComp, NULL);
    return eError;
}

OMX_ERRORTYPE NvxGetComponentVersion(OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_OUT OMX_STRING pComponentName,
                                     OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
                                     OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
                                     OMX_OUT OMX_UUIDTYPE* pComponentUUID)
{
    NvxComponent *pNvComp;
    if (!hComponent)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxGetComponentVersion:hComponent = %d:[%s(%d)]\n",
        hComponent,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pNvComp = NvxComponentFromHandle(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxGetComponentVersion: %s\n",pNvComp->pComponentName));

    pComponentVersion->nVersion = pNvComp->oComponentVersion.nVersion;
    pSpecVersion->nVersion = pNvComp->oSpecVersion.nVersion;

    NvOsStrncpy(pComponentName, pNvComp->pComponentName, 127);
    pComponentName[127] = 0;
    NvOsMemcpy(pComponentUUID, &pNvComp->oUUID, sizeof(OMX_UUIDTYPE));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxSendCommand(OMX_IN OMX_HANDLETYPE hComponent,
                             OMX_IN OMX_COMMANDTYPE Cmd,
                             OMX_IN OMX_U32 nParam1,
                             OMX_IN OMX_PTR pCmdData )
{
    NvxCommand command;
    NvxComponent *pNvComp;
    NvError err;
    if (!hComponent)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxSendCommand"
                  ":hComponent = %d:[%s(%d)]\n",hComponent,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pNvComp = NvxComponentFromHandle(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxSendCommand: %s %x %d\n",pNvComp->pComponentName, (int)Cmd, (int)nParam1));

    if (pNvComp->eState == OMX_StateInvalid)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSendCommand:%s"
                  ":eState = %d :[%s(%d)]\n",pNvComp->pComponentName,(int)pNvComp->eState,__FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    switch (Cmd) {
    case OMX_CommandStateSet:
        if (nParam1 > OMX_StateWaitForResources)  /* update this if OMX_STATETYPE changes */
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSendCommand:%s"
                   ":nParam1 (%d)> OMX_StateWaitForResources(%d):[%s(%d)]\n",
                   pNvComp->pComponentName,nParam1,OMX_StateWaitForResources,__FILE__, __LINE__));
            return OMX_ErrorNotImplemented;
        }
        pNvComp->bMajorStateChangePending = OMX_TRUE;
        break;

    case OMX_CommandFlush:
    case OMX_CommandPortEnable:
    case OMX_CommandPortDisable:
        if (nParam1 != OMX_ALL && nParam1 >= pNvComp->nPorts)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSendCommand:%s"
                  ":nParam1 (%d)  >= pNvComp->nPorts(%d):[%s(%d)]\n",
                  pNvComp->pComponentName,nParam1,pNvComp->nPorts,__FILE__, __LINE__));
            return OMX_ErrorBadPortIndex;
        }
        pNvComp->bMajorStateChangePending = OMX_TRUE;
        break;

    case OMX_CommandMarkBuffer:
        if (nParam1 >= pNvComp->nPorts)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSendCommand:%s"
                   ":nParam1 (%d)  >= pNvComp->nPorts(%d):[%s(%d)]\n",
                   pNvComp->pComponentName,nParam1,pNvComp->nPorts,__FILE__, __LINE__));
            return OMX_ErrorBadPortIndex;
        }
        if (!pCmdData)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSendCommand:%s"
                  ":pCmdData = %d:[%s(%d)]\n",pNvComp->pComponentName,pCmdData,__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }
        break;

    default:
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSendCommand:%s"
                    ":Cmd = %d:[%s(%d)]\n",pNvComp->pComponentName,Cmd,__FILE__, __LINE__));
            return OMX_ErrorNotImplemented;
        }
    }

    command.Cmd = Cmd;
    command.nParam1 = nParam1;

    if (Cmd == OMX_CommandMarkBuffer) {
        NvOsMemcpy(&(command.oCmdData), pCmdData, sizeof(OMX_MARKTYPE));
    }

    err = NvMMQueueEnQ(pNvComp->pCommandQueue, &command, OMX_TIMEOUT_MS);

    if (Cmd == OMX_CommandStateSet && pNvComp->PreCheckChangeState)
    {
        pNvComp->PreCheckChangeState(pNvComp, nParam1);
    }

    NvxWorkerTrigger(&(pNvComp->oWorkerData));

    if (err == NvSuccess)
        return OMX_ErrorNone;
    return OMX_ErrorNotReady;
}

OMX_ERRORTYPE NvxGetParameter(OMX_IN OMX_HANDLETYPE hComponent,
                              OMX_IN OMX_INDEXTYPE nParamIndex,
                              OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    NvxComponent *pNvComp;
    OMX_ERRORTYPE error = OMX_ErrorNotImplemented;
    if (!hComponent || !pComponentParameterStructure)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxGetParameter"
             ":hComponent = %d:pComponentParameterStructure = %d:[%s(%d)]\n",
             hComponent,pComponentParameterStructure,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pNvComp = NvxComponentFromHandle(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxGetParameter: %s\n",pNvComp->pComponentName));

    if (pNvComp->eState == OMX_StateInvalid)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxGetParameter:%s"
            ":eState = %d:[%s(%d)]\n",pNvComp->pComponentName,(int)pNvComp->eState,__FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    if (pNvComp->GetParameter)
        error = pNvComp->GetParameter(pNvComp, nParamIndex, pComponentParameterStructure);

    return error;
}


typedef struct PortParamHead {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
} PortParamHead;

OMX_ERRORTYPE NvxSetParameter(OMX_IN OMX_HANDLETYPE hComponent,
                              OMX_IN OMX_INDEXTYPE nIndex,
                              OMX_IN OMX_PTR pComponentParameterStructure)
{
    NvxComponent *pNvComp;
    PortParamHead* pPortParam;
    OMX_ERRORTYPE error = OMX_ErrorNotImplemented;
    if (!hComponent || !pComponentParameterStructure)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxSetParameter"
              ":hComponent = %d:pComponentParameterStructure = %d:[%s(%d)]\n",
              hComponent,pComponentParameterStructure,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pNvComp = NvxComponentFromHandle(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxSetParameter: %s\n",pNvComp->pComponentName));

    if ( pNvComp->eState == OMX_StateInvalid)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSetParameter:%s"
              ":eState = %d:[%s(%d)]\n",pNvComp->pComponentName,(int)pNvComp->eState,__FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    /* read-only parameter? */
    if ((nIndex == OMX_IndexParamAudioInit) ||
        (nIndex == OMX_IndexParamVideoInit) ||
        (nIndex == OMX_IndexParamImageInit) ||
        (nIndex == OMX_IndexParamOtherInit))
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSetParameter:%s"
              ":nIndex = %d:[%s(%d)]\n",pNvComp->pComponentName,(int)nIndex,__FILE__, __LINE__));
        return OMX_ErrorUnsupportedSetting;
    }

    /* check size and version for reasonableness */
    pPortParam = pComponentParameterStructure;
    if ( pPortParam->nSize < sizeof (*pPortParam))
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSetParameter:%s"
               ":PortParam->nSize(%d) <  *pPortParam(%d):[%s(%d)]\n",
               pNvComp->pComponentName,pPortParam->nSize,(int)sizeof (*pPortParam),__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    if (CheckOMXVersion(pPortParam->nVersion, NvxComponentSpecVersion) !=
        OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSetParameter:%s"
              ":nVersion (%d) != NvxComponentSpecVersion.nVersion(%d):[%s(%d)]\n",
              pNvComp->pComponentName,pPortParam->nVersion.nVersion ,(int) NvxComponentSpecVersion.nVersion,__FILE__, __LINE__));
        return OMX_ErrorVersionMismatch;
    }

    /* assume that for all parameters that are port params, the paramater has the port index first */
    /* reject a param request if it we're not loaded in loaded state or the port is not stopped or the port is not valid */
    if ( pNvComp->eState != OMX_StateLoaded
         && (pPortParam->nPortIndex >= pNvComp->nPorts
             || NvxPortIsReady(&pNvComp->pPorts[pPortParam->nPortIndex]))) {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSetParameter:%s"
              ":eState = %d and nPortIndex (%d) > nPorts (%d):[%s(%d)]\n",
              pNvComp->pComponentName,pNvComp->eState,(int)pPortParam->nPortIndex,pNvComp->nPorts,__FILE__, __LINE__));
        return OMX_ErrorIncorrectStateOperation;
    }

    if (pNvComp->SetParameter) {
        error = pNvComp->SetParameter(pNvComp, nIndex, pComponentParameterStructure);
    }

    return error;
}

OMX_ERRORTYPE NvxGetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                           OMX_IN OMX_INDEXTYPE nIndex,
                           OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    NvxComponent *pNvComp;
    OMX_ERRORTYPE error = OMX_ErrorNotImplemented;
   if (!hComponent || !pComponentConfigStructure)
   {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxSetParameter"
             ":hComponent = %d and pComponentConfigStructure = %d:[%s(%d)]\n",
             hComponent ,pComponentConfigStructure,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
   }
    pNvComp = NvxComponentFromHandle(hComponent);

    if (pNvComp->eState == OMX_StateInvalid)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSetParameter:%s"
           ":eState = %d :[%s(%d)]\n",pNvComp->pComponentName,pNvComp->eState,__FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    if (pNvComp->GetConfig)
        error = pNvComp->GetConfig(pNvComp, nIndex, pComponentConfigStructure);
    return error;
}

OMX_ERRORTYPE NvxSetConfig(OMX_IN OMX_HANDLETYPE hComponent,
                           OMX_IN OMX_INDEXTYPE nIndex,
                           OMX_IN OMX_PTR pComponentConfigStructure)
{
    NvxComponent *pNvComp;
    OMX_BOOL bMTSched = OMX_TRUE;
    if (!hComponent || !pComponentConfigStructure)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxSetConfig"
             ":hComponent = %ld ,pComponentConfigStructure = %ld:[%s(%d)]\n",
             hComponent,pComponentConfigStructure,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }
    pNvComp = NvxComponentFromHandle(hComponent);
    if (!pNvComp)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxSetConfig:pNvComp = "
             "%p:[%s(%d)]\n", pNvComp, __FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxSetConfig: %s (%x)\n",pNvComp->pComponentName, (int)nIndex));

    if (pNvComp->eState == OMX_StateInvalid)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSetConfig:%s"
             ":eState = %ld :[%s(%d)]\n",pNvComp->pComponentName,pNvComp->eState ,__FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    /* setup the pending SetConfig */
    NvxMutexLock( pNvComp->hConfigMutex );

    /* Check to see if something else is pending, first */
    if (pNvComp->eSetConfigState != NVX_SETCONFIGSTATE_NOTHINGPENDING)
    {
        NvxMutexUnlock(pNvComp->hConfigMutex);

        /* Trigger worker */
        NvxWorkerTrigger(&(pNvComp->oWorkerData));
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSetConfig:%s"
                  ": eSetConfigState = %d:[%s(%d)]\n",pNvComp->pComponentName,
                  (int)pNvComp->eSetConfigState,__FILE__, __LINE__));
        return OMX_ErrorNotReady;
    }

    /* save parameters of pending SetConfig */
    pNvComp->nPendingConfigIndex        = nIndex;
    pNvComp->pPendingConfigStructure    = pComponentConfigStructure;
    pNvComp->eSetConfigError            = OMX_ErrorTimeout;
    pNvComp->eSetConfigState            = NVX_SETCONFIGSTATE_PENDING;

    NvxMutexUnlock(pNvComp->hConfigMutex);

    /* Trigger worker */
    NvxWorkerTrigger(&(pNvComp->oWorkerData));

    /* wait for the config to be processed */
    NvxSchedulerIsMultithreaded(&bMTSched);
    if (bMTSched && pNvComp->hSetConfigEvent) {
        (void)NvOsSemaphoreWaitTimeout(pNvComp->hSetConfigEvent, OMX_TIMEOUT_MS);
    }
    else {
        NvxWorkerRun(&(pNvComp->oWorkerData));
    }

    /* check the SetConfig state */
    NvxMutexLock( pNvComp->hConfigMutex );

    switch(pNvComp->eSetConfigState)
    {
        case NVX_SETCONFIGSTATE_NOTHINGPENDING:
            /* Only SetConfig can set this value - something's wrong */
            NvxMutexUnlock(pNvComp->hConfigMutex);
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxSetConfig:%s"
                 ":estateconfigstate = %d:[%s(%d)]\n",pNvComp->pComponentName,
                 pNvComp->eSetConfigState,__FILE__, __LINE__));
            return OMX_ErrorUndefined;

        case NVX_SETCONFIGSTATE_PROCESSING:
            /* worker thread has started processing - wait indefinitely for the SetConfig to finish */
            NvxMutexUnlock(pNvComp->hConfigMutex);
            if (pNvComp->hSetConfigEvent)
                NvOsSemaphoreWait(pNvComp->hSetConfigEvent);
            NvxMutexLock( pNvComp->hConfigMutex );
            break;

        case NVX_SETCONFIGSTATE_PENDING:
            pNvComp->eSetConfigError = OMX_ErrorTimeout;
            break;

        case NVX_SETCONFIGSTATE_COMPLETE:
            break;
    }

    /* one way or the other SetConfig is done - set state to nothing pending */
    pNvComp->eSetConfigState = NVX_SETCONFIGSTATE_NOTHINGPENDING;

    NvxMutexUnlock(pNvComp->hConfigMutex);
    return pNvComp->eSetConfigError;
}

OMX_ERRORTYPE NvxGetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_STRING cParameterName,
                                   OMX_OUT OMX_INDEXTYPE* pIndexType)
{
    NvxComponent *pNvComp;
    OMX_ERRORTYPE error = OMX_ErrorNotImplemented;
    if (!hComponent || !cParameterName || !pIndexType)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxGetExtensionIndex"
            ":hComponent = %ld :[%s(%d)]\n",hComponent,__FILE__, __LINE__ ));
        return OMX_ErrorBadParameter;
    }
    pNvComp = NvxComponentFromHandle(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxGetExtensionIndex: %s\n",pNvComp->pComponentName));
    if (pNvComp->eState == OMX_StateInvalid)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxGetExtensionIndex:%s"
             ":eState = %d :[%s(%d)]\n",pNvComp->pComponentName,pNvComp->eState ,__FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    if (pNvComp->GetExtensionIndex)
        error = pNvComp->GetExtensionIndex(pNvComp, cParameterName, pIndexType);
    return error;
}

OMX_ERRORTYPE NvxGetState(OMX_IN OMX_HANDLETYPE hComponent,
                          OMX_OUT OMX_STATETYPE* pState)
{
    NvxComponent *pNvComp;
    if (!hComponent)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxGetState"
             ":hComponent = %ld :[%s(%d)]\n",hComponent,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }
    pNvComp = NvxComponentFromHandle(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxGetState: %s\n",pNvComp->pComponentName));
    *pState = pNvComp->eState;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxComponentTunnelRequest(
    OMX_IN  OMX_HANDLETYPE hComp,
    OMX_IN  OMX_U32 nPort,
    OMX_IN  OMX_HANDLETYPE hTunneledComp,
    OMX_IN  OMX_U32 nTunneledPort,
    OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    NvxComponent *pNvComp;
    NvxPort *pPort;
    OMX_PARAM_BUFFERSUPPLIERTYPE oSupplier;
    OMX_ERRORTYPE error = OMX_ErrorNone;
    if (!hComp)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED,"ERROR:NvxComponentTunnelRequest:hComp = %ld :[%s(%d)]\n",
        hComp,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }
    pNvComp = NvxComponentFromHandle(hComp);
    if (pNvComp == NULL || pNvComp->hBaseComponent != hComp)
    {
        if (pNvComp == NULL)
            NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxComponentTunnelRequest"
                  ":hComp = %p :[%s(%d)]\n", pNvComp,__FILE__, __LINE__));
        else
            NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxComponentTunnelRequest"
                  ":hComp = %p :hBaseComponent = %p :[%s(%d)]\n",
                  pNvComp,pNvComp->hBaseComponent,__FILE__, __LINE__));

        return OMX_ErrorBadParameter;
    }

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentTunnelRequest:%s\n",pNvComp->pComponentName));

    if (nPort >= pNvComp->nPorts)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentTunnelRequest:%s:"
              "nPort (%d) >= pNvComp->nPorts(%d ):[%s(%d)]\n",
              pNvComp->pComponentName,nPort,pNvComp->nPorts,__FILE__, __LINE__));
        return OMX_ErrorBadPortIndex;
    }

    pPort = &pNvComp->pPorts[nPort];

    if (pNvComp->eState != OMX_StateLoaded && pPort->oPortDef.bEnabled)
        return (pNvComp->eState == OMX_StateInvalid) ? OMX_ErrorInvalidState : OMX_ErrorIncorrectStateOperation;

    if (pNvComp->pCallbacks == NULL || pNvComp->pCallbacks->EmptyBufferDone == NULL)
        return OMX_ErrorPortsNotCompatible;

    if (pTunnelSetup == NULL || hTunneledComp == 0)
    {
        /* cancel previous tunnel */
        if (pPort->hTunnelComponent)
        {
            NvxLockAcqLock();
            NvxPortReleaseResources(pPort);
            pPort->oPortDef.bEnabled = OMX_FALSE;
            NvxUnlockAcqLock();
        }
        pPort->hTunnelComponent = 0;
        pPort->nTunnelPort = 0;
        pPort->bNvidiaTunneling = OMX_FALSE;
        //pPort->bShareIfNvidiaTunnel = OMX_FALSE;
        pPort->bAllocateRmSurf = OMX_FALSE;
        pPort->eSupplierSetting = OMX_BufferSupplyUnspecified;
    }
    else
    {
        if (pPort->oPortDef.eDir != OMX_DirInput &&
            pPort->oPortDef.eDir != OMX_DirOutput)
        {
            return OMX_ErrorBadPortIndex;
        }

        if (pNvComp->CompatiblePorts &&
            NvxIsError(pNvComp->CompatiblePorts(pNvComp, pPort,
                                                hTunneledComp, nTunneledPort)))
        {
            return OMX_ErrorPortsNotCompatible;
        }

        pPort->hTunnelComponent = hTunneledComp;
        pPort->nTunnelPort = nTunneledPort;

        if (pPort->oPortDef.eDir == OMX_DirOutput)
        {
            /* first call, where we're the output (source of data) */
            NvxParamNvidiaTunnel oVendTunnel;

            /* FIXME: does this logic need to be more complicated to handle shared buffering? */
            pTunnelSetup->eSupplier = pPort->eSupplierPreference;

            /* If sink is also an NVIDIA component, allow vendor specific tunneling */
            oVendTunnel.nPort = nTunneledPort;
            if (OMX_ErrorNone == OMX_GetParameter(hTunneledComp, NVX_IndexParamVendorSpecificTunneling, &oVendTunnel))
            {
                pPort->bNvidiaTunneling = OMX_TRUE;
                /* If default transaction not specified, then do proprietary tunneling */
                if (pPort->nMinBufferSize < pPort->oPortDef.nBufferSize)
                {
                    pPort->oPortDef.nBufferSize = pPort->nMinBufferSize;
                    pPort->nReqBufferSize = pPort->nMinBufferSize;
                }
                if(pPort->nMinBufferCount < pPort->nReqBufferCount)
                {
                    pPort->nReqBufferCount = pPort->nMinBufferCount;
                    pPort->oPortDef.nBufferCountActual = pPort->nMinBufferCount;
                }

            }
        }
        else
        {
            /* second call, where we're the input (sink of data) */
            NvxParamNvidiaTunnel oVendTunnel;

            /* FIXME: put in real supplier logic based on what the OpenMax spec says for shared buffering */
            /* If specified obey output port's preferences. Otherwise choose self (input). */
            if ((pPort->eSupplierPreference != OMX_BufferSupplyUnspecified) &&
                (pPort->eSupplierPreference != pTunnelSetup->eSupplier))
            {
                if (pTunnelSetup->eSupplier == OMX_BufferSupplyUnspecified)
                {
                    pPort->eSupplierSetting = pPort->eSupplierPreference;
                }
                else
                {
                    pPort->eSupplierSetting = pPort->eSupplierTieBreaker;
                }
            }
            else
            {
                pPort->eSupplierSetting = pTunnelSetup->eSupplier;
            }

            if (OMX_BufferSupplyUnspecified == pPort->eSupplierSetting)
            {
                pPort->eSupplierSetting = pTunnelSetup->eSupplier = pPort->eSupplierTieBreaker;
            }

            /* Tell the output who the supplier is */
            oSupplier.nSize = sizeof(oSupplier);
            oSupplier.nVersion = pNvComp->oSpecVersion;
            oSupplier.nPortIndex = nTunneledPort;
            oSupplier.eBufferSupplier = pPort->eSupplierSetting;
            error = OMX_SetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &oSupplier);

            /* Check if supplier is an NVIDIA comp and whether vendor specific tunneling is specified */
            oVendTunnel.nPort = nTunneledPort;
            if (OMX_ErrorNone == OMX_GetParameter(hTunneledComp, NVX_IndexParamVendorSpecificTunneling, &oVendTunnel))
            {
                // Check if this comp supports the transaction type.
                // If not, set back to use default (slower) transaction

                if (pPort->eNvidiaTunnelTransaction == oVendTunnel.eTransactionType)
                {
                    /* Take supplier settings */
                    pPort->bNvidiaTunneling = oVendTunnel.bNvidiaTunnel;
                    pPort->eNvidiaTunnelTransaction = oVendTunnel.eTransactionType;
                }
                else if ( (ENVX_TRANSACTION_GFSURFACES == oVendTunnel.eTransactionType &&
                           ENVX_TRANSACTION_NVMM_TUNNEL == pPort->eNvidiaTunnelTransaction) ||
                          (ENVX_TRANSACTION_NVMM_TUNNEL == oVendTunnel.eTransactionType &&
                           ENVX_TRANSACTION_GFSURFACES == pPort->eNvidiaTunnelTransaction) )
                {
                    /* Take supplier settings */
                    pPort->bNvidiaTunneling = oVendTunnel.bNvidiaTunnel;
                    pPort->eNvidiaTunnelTransaction = oVendTunnel.eTransactionType = ENVX_TRANSACTION_GFSURFACES;
                    oVendTunnel.nSize = sizeof(NvxParamNvidiaTunnel);
                    oVendTunnel.nVersion = pNvComp->oSpecVersion;
                    OMX_SetParameter(hTunneledComp, NVX_IndexParamVendorSpecificTunneling, &oVendTunnel);
                }
                else
                {
                    /* Transaction modes do not match. Fall back to normal transactions */
                    /* Possible exception is from NVMM -> GFSURFACE transaction fallback */
                    pPort->bNvidiaTunneling = oVendTunnel.bNvidiaTunnel;
                    pPort->eNvidiaTunnelTransaction = ENVX_TRANSACTION_DEFAULT;
                    oVendTunnel.eTransactionType = ENVX_TRANSACTION_DEFAULT;
                    oVendTunnel.nSize = sizeof(NvxParamNvidiaTunnel);
                    oVendTunnel.nVersion = pNvComp->oSpecVersion;
                    OMX_SetParameter(hTunneledComp, NVX_IndexParamVendorSpecificTunneling, &oVendTunnel);
                }

                if (pPort->bNvidiaTunneling)
                {
                    if (pPort->nMinBufferSize < pPort->oPortDef.nBufferSize )
                    {
                        pPort->oPortDef.nBufferSize = pPort->nMinBufferSize;
                        pPort->nReqBufferSize = pPort->nMinBufferSize;
                    }
                    if(pPort->nMinBufferCount < pPort->nReqBufferCount)
                    {
                        pPort->nReqBufferCount = pPort->nMinBufferCount;
                        pPort->oPortDef.nBufferCountActual = pPort->nMinBufferCount;
                    }
                }
            }
        }
    }
    if (pNvComp->ComponentTunnelRequest != NULL)
    {
        error = pNvComp->ComponentTunnelRequest(
                                  pNvComp,nPort,
                                  hTunneledComp,nTunneledPort,
                                  pTunnelSetup);
    }
    return error;
}

/* all parameters checked already
 * If isEglImage is NV_TRUE, then pBuffer is an EGLImageKHR cast to (OMX_U8 *)
 * and nSizeBytes must be 0.*/
static OMX_ERRORTYPE NvxCreateBufferHeader(NvxComponent *pNvComp,
                                           OMX_BUFFERHEADERTYPE** ppHdr,
                                           OMX_PTR pAppPrivate,
                                           OMX_U32 nSizeBytes,
                                           OMX_U8 *pBuffer,
                                           NvxPort *pPort,
                                           NvxBufferType oBufferType)
{
    OMX_BUFFERHEADERTYPE *pHdr;
    NvxBufferPlatformPrivate *pPlatPriv;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType,
             "+NvxCreateBufferHeader: %s\n", pNvComp->pComponentName));
    pHdr = NvOsAlloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (!pHdr)
    {
        return OMX_ErrorInsufficientResources;
    }

    NvOsMemset(pHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

    pHdr->nSize = sizeof(OMX_BUFFERHEADERTYPE);
    pHdr->nVersion.nVersion = pNvComp->oSpecVersion.nVersion;
    if (oBufferType != NVX_BUFFERTYPE_NORMAL)
    {
        // Must be a normal buffer to tunnel
        if (pPort->hTunnelComponent)
        {
            NvOsFree(pHdr);
            return OMX_ErrorBadParameter;
        }
        nSizeBytes = 0;
    }

    pHdr->nAllocLen = nSizeBytes;

    if (oBufferType == NVX_BUFFERTYPE_EGLIMAGE)
    {
        NvxEglImageSiblingHandle hEglImgSibling = NULL;
        NvError err = NvxEglCreateImageSibling((EGLImageKHR)pBuffer,
                                               &hEglImgSibling);
        if (err != OMX_ErrorNone)
        {
            NvOsFree(pHdr);
            return err;
        }

        pHdr->pBuffer = (OMX_U8*)hEglImgSibling;
    }
    else
    {
        pHdr->pBuffer = pBuffer;
    }

    pHdr->pAppPrivate = pAppPrivate;
    if (pPort->oPortDef.eDir == OMX_DirOutput)
    {
        pHdr->pOutputPortPrivate = pPort;
        pHdr->nOutputPortIndex = pPort->oPortDef.nPortIndex;
        if (pPort->hTunnelComponent)
        {
            pHdr->nInputPortIndex = pPort->nTunnelPort;
        }
        else
        {
            pHdr->nInputPortIndex = -1;
        }
        pHdr->pInputPortPrivate = NULL;
    }
    else
    {
        pHdr->pInputPortPrivate = pPort;
        pHdr->nInputPortIndex = pPort->oPortDef.nPortIndex;
        if (pPort->hTunnelComponent)
        {
            pHdr->nOutputPortIndex = pPort->nTunnelPort;
        }
        else
        {
            pHdr->nOutputPortIndex = -1;
        }
        pHdr->pOutputPortPrivate = NULL;
    }

    pPlatPriv = NvOsAlloc(sizeof(NvxBufferPlatformPrivate));
    NvOsMemset(pPlatPriv, 0, sizeof(NvxBufferPlatformPrivate));
    pPlatPriv->eType = oBufferType;
    pHdr->pPlatformPrivate = pPlatPriv;

    *ppHdr = pHdr;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxUseBufferInt(OMX_HANDLETYPE hComponent,
                              OMX_BUFFERHEADERTYPE** ppBufferHdr,
                              OMX_U32 nPortIndex,
                              OMX_PTR pAppPrivate,
                              OMX_U32 nSizeBytes,
                              OMX_U8* pBuffer,
                              NvxBufferType oBufferType)
{
    NvxComponent *pNvComp;
    OMX_ERRORTYPE error = OMX_ErrorNone;
    NvxPort *pPort;

    if (!hComponent || !ppBufferHdr || !pBuffer)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxUseBufferInt:"
             ":hComponent = %ld :ppBufferHdr = %ld:pBuffer = %d:[%s(%d)]\n",
             hComponent,ppBufferHdr,pBuffer,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pNvComp = NvxComponentFromHandle(hComponent);

    if (pNvComp->eState == OMX_StateInvalid)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxUseBufferInt:%s"
             ":estate = %d :[%s(%d)]\n",pNvComp->pComponentName,(int)pNvComp->eState,__FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    if (nPortIndex >= pNvComp->nPorts)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxUseBufferInt:%s"
             ":nPortIndex = %d >  nPorts %d :[%s(%d)]\n",pNvComp->pComponentName
             ,nPortIndex,pNvComp->nPorts,__FILE__, __LINE__));
        return OMX_ErrorBadPortIndex;
    }

    pPort = &pNvComp->pPorts[nPortIndex];
    if (pNvComp->eState != OMX_StateLoaded && NvxPortIsReady(pPort))
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxUseBufferInt:%s: "
            "estate = %d and PortIsReady:[%s(%d)]\n",pNvComp->pComponentName,
            (int)pNvComp->eState,__FILE__, __LINE__));
        return OMX_ErrorIncorrectStateOperation;
    }

    /* create queues as necessary */
    error = NvxPortAllocResources(pPort);
    if (error != OMX_ErrorNone)
        return error;

    if (pPort->nBuffers >= pPort->nMaxBuffers)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxUseBufferInt:%s:"
              "nBuffers (%d) > nMaxBuffers :[%s(%d)]\n",pNvComp->pComponentName,
              pPort->nBuffers,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    /* Other checks to see if we can use it */
    switch (oBufferType)
    {
        case NVX_BUFFERTYPE_NORMAL:
            if (pNvComp->CanUseBuffer)
                error = pNvComp->CanUseBuffer(pNvComp, nPortIndex,
                                              nSizeBytes, pBuffer);
            break;
        case NVX_BUFFERTYPE_EGLIMAGE:
            //if (pPort->oPortDef.eDir != OMX_DirInput)
                //pPort->nReqBufferCount = (pPort->nReqBufferCount < 2) ? pPort->nReqBufferCount : 2;
            if (pNvComp->CanUseEGLImage)
                error = pNvComp->CanUseEGLImage(pNvComp, nPortIndex, pBuffer);
            break;
        default:
            break;
    }
    if (error != OMX_ErrorNone)
        return error;

    error = NvxCreateBufferHeader(pNvComp, ppBufferHdr, pAppPrivate, nSizeBytes,
                                  pBuffer, pPort, oBufferType);

    if (error != OMX_ErrorNone)
        return error;

    pPort->ppBufferHdrs[pPort->nBuffers] = *ppBufferHdr;
    pPort->ppBuffers[pPort->nBuffers] = 0;
    pPort->ppRmSurf[pPort->nBuffers] = NULL;

    if (nSizeBytes > NvOsStrlen("NVNVRMSURFACENV"))
    {
        if (!NvOsStrncmp((char *)pBuffer, "NVNVRMSURFACENV",
                         NvOsStrlen("NVNVRMSURFACENV")))
        {
            OMX_U32 RmSurf;
            RmSurf = *((NvU32 *)(pBuffer + strlen("NVNVRMSURFACENV") + 1));

            NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType,
                     "%s[%d] pBuffer(%p) NVNVRMSURFACENV(%p) matched\n",
                     __FUNCTION__, __LINE__, pBuffer, RmSurf));

            pPort->ppRmSurf[pPort->nBuffers] = (void *)RmSurf;
        }
    }

    pPort->nBuffers++;
    if (pPort->nBuffers >= pPort->nReqBufferCount)
    {
        pPort->oPortDef.bPopulated = OMX_TRUE;
        error = NvxWorkerTrigger(&(pNvComp->oWorkerData));
    }

    return error;
}

OMX_ERRORTYPE NvxUseEGLImage(OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN void* eglImage)
{
    // Make sure OMX is registered with EGL
    if (NvxConnectToEgl() != NvSuccess)
        return OMX_ErrorInvalidState;

    return NvxUseBufferInt(hComponent,
            ppBufferHdr,
            nPortIndex,
            pAppPrivate,
            0,
            (OMX_U8 *)eglImage,
            NVX_BUFFERTYPE_EGLIMAGE);
}

OMX_ERRORTYPE NvxUseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                           OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                           OMX_IN OMX_U32 nPortIndex,
                           OMX_IN OMX_PTR pAppPrivate,
                           OMX_IN OMX_U32 nSizeBytes,
                           OMX_IN OMX_U8* pBuffer)
{
    return NvxUseBufferInt(hComponent,
            ppBufferHdr,
            nPortIndex,
            pAppPrivate,
            nSizeBytes,
            pBuffer,
            NVX_BUFFERTYPE_NORMAL);
}

OMX_ERRORTYPE NvxAllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
                                OMX_IN OMX_U32 nPortIndex,
                                OMX_IN OMX_PTR pAppPrivate,
                                OMX_IN OMX_U32 nSizeBytes)
{
    NvxComponent *pNvComp = NULL;
    OMX_U8 *pBuf = NULL;
    OMX_ERRORTYPE error = OMX_ErrorNone;
    NvxPort *pPort = NULL;
    if (!hComponent)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxAllocateBuffer"
          ":hComponent = %ld:[%s(%d)]\n",hComponent,__FILE__, __LINE__));
      return OMX_ErrorBadParameter;
    }
    pNvComp = NvxComponentFromHandle(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxAllocateBuffer: %s\n",pNvComp->pComponentName));

    if (pNvComp->eState == OMX_StateInvalid)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAllocateBuffer:%s:eState = %ld:[%s(%d)]\n",
            pNvComp->pComponentName,pNvComp->eState,__FILE__, __LINE__));
        return OMX_ErrorInvalidState;
    }

    if (nPortIndex >= pNvComp->nPorts)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAllocateBuffer:%s"
             ":nPortIndex (%ld) >= nPorts(%d) :[%s(%d)]\n", pNvComp->pComponentName,
             nPortIndex,pNvComp->nPorts,__FILE__, __LINE__));
        return OMX_ErrorBadPortIndex;
    }
    if (!ppBuffer)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAllocateBuffer:%s"
             ":ppBuffer = %d :[%s(%d)]\n",pNvComp->pComponentName,ppBuffer,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pPort = &pNvComp->pPorts[nPortIndex];
    if (pNvComp->eState != OMX_StateLoaded && NvxPortIsReady(pPort))
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAllocateBuffer:%s: "
        "eState = %d (not OMX_StateLoaded) and NvxPortIsReady:[%s(%d)]\n",
        pNvComp->pComponentName,(int)pNvComp->eState,__FILE__, __LINE__));
        return OMX_ErrorIncorrectStateOperation;
    }

    error = NvxPortAllocResources(pPort);
    if (error != OMX_ErrorNone)
        return error;

    if (pPort->nBuffers >= pPort->nMaxBuffers)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAllocateBuffer:%s:"
        "nBuffers (%d) >= nMaxBuffers(%d):[%s(%d)]\n",pNvComp->pComponentName,
        pPort->nBuffers,pPort->nMaxBuffers,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    if (pPort->bAllocateRmSurf)
    {
        NvxSurface *pSurfHandle = NULL;
        pBuf = NvxAllocRmSurf(&nSizeBytes,
                pPort->oPortDef.format.video.nFrameWidth,
                pPort->oPortDef.format.video.nFrameHeight,
                pPort->oPortDef.format.video.eColorFormat,
                NvRmSurfaceLayout_Pitch,
                &pSurfHandle);
        pPort->ppRmSurf[pPort->nBuffers] = (void *)pSurfHandle;
        NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "%s[%d] pSurfHandle %p \n",
            __FUNCTION__, __LINE__, pSurfHandle));
    }
    if (!pBuf)
    {
        NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "%s[%d] \n",__FUNCTION__, __LINE__));
        pBuf = NvOsAlloc (nSizeBytes);
    }

    if (!pBuf)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxAllocateBuffer:%s:"
        "pBuf = %d:[%s(%d)]\n",pNvComp->pComponentName,(int)pBuf,__FILE__, __LINE__));
        return OMX_ErrorInsufficientResources;
    }

    error = NvxCreateBufferHeader(pNvComp, ppBuffer, pAppPrivate, nSizeBytes,
                                  pBuf, pPort, NVX_BUFFERTYPE_NORMAL);

    if (error != OMX_ErrorNone)
        return error;

    pPort->ppBufferHdrs[pNvComp->pPorts[nPortIndex].nBuffers] = *ppBuffer;
    pPort->ppBuffers[pPort->nBuffers] = pBuf; /* cache for deallocation */
    pPort->nBuffers++;

    if (pPort->nBuffers >= pPort->nReqBufferCount) {
        error = NvxWorkerTrigger(&(pNvComp->oWorkerData));
    }
    return error;
}

OMX_ERRORTYPE NvxFreeBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                            OMX_IN OMX_U32 nPortIndex,
                            OMX_IN OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    NvxComponent *pNvComp;
    NvxPort *pPort;
    OMX_U32 i;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BOOL bNeedRelease;
    if (!hComponent || pBufferHdr == NULL || pBufferHdr->nSize != sizeof(OMX_BUFFERHEADERTYPE))
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxFreeBuffer:"
          "hComponent = %p,pBufferHdr= %p,pBufferHdr->nSize(%ld) != sizeof(OMX_BUFFERHEADERTYPE)(%ld):[%s(%d)]\n",
          hComponent, pBufferHdr, ((pBufferHdr) ? pBufferHdr->nSize : 0),
          sizeof(OMX_BUFFERHEADERTYPE), __FILE__, __LINE__));
      return OMX_ErrorBadParameter;
    }

    pNvComp = NvxComponentFromHandle(hComponent);
    if (!pNvComp)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxFreeBuffer:"
             "pNvComp = %p:[%s(%d)]\n",pNvComp,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxFreeBuffer: %s\n",pNvComp->pComponentName));

    if (pNvComp->eState == OMX_StateWaitForResources)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFreeBuffer:%s "
             ":eState = %ld:[%s(%d)]\n",pNvComp->pComponentName,pNvComp->eState,__FILE__, __LINE__));
        return OMX_ErrorIncorrectStateOperation;
    }

    if (nPortIndex >= pNvComp->nPorts)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFreeBuffer:%s:"
              "nPortIndex (%d) >= pNvComp->nPorts (%d):[%s(%d)]\n",pNvComp->pComponentName
              ,nPortIndex,pNvComp->nPorts,__FILE__, __LINE__));
        return OMX_ErrorBadPortIndex;
    }

    pPort = &pNvComp->pPorts[nPortIndex];
    if (pPort->oPortDef.eDir == OMX_DirOutput) {
        if (pBufferHdr->nOutputPortIndex != nPortIndex || pBufferHdr->pOutputPortPrivate != pPort)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFreeBuffer:%s:"
                 "nOutputPortIndex (%d) != nPortIndex (%d) :[%s(%d)]\n",pNvComp->pComponentName
                 ,pBufferHdr->nOutputPortIndex,nPortIndex ,__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }
    }
    else {
        if (pBufferHdr->nInputPortIndex != nPortIndex || (pBufferHdr->pInputPortPrivate != pPort))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFreeBuffer:%s:"
                 "InputPortIndex (%d) != nPortIndex (%d) || InputPortPrivate  != pPort :[%s(%d)]\n",
                 pNvComp->pComponentName,pBufferHdr->nInputPortIndex,nPortIndex,__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }
    }

    if (!pPort->ppBuffers || !pPort->ppBufferHdrs)
    {
        NVXTRACE((NVXT_ERROR, pNvComp->eObjectType, "ERROR:NvxFreeBuffer:%s:"
            "pPort->ppBuffers (%p), pPort->ppBufferHdrs (%p) : [%s(%d)]\n",
            pNvComp->pComponentName, pPort->ppBuffers, pPort->ppBufferHdrs,
            __FILE__,__LINE__));
    }

    if (NvxPortIsSupplier(pPort)) {
        /* if port supplier in a tunnel, we don't supply buffer headers so this is an invalid call */
        return OMX_ErrorBadParameter;
    }

    NvxMutexLock(pNvComp->hWorkerMutex);

    NvxMutexLock(pPort->hMutex);
    if (pPort->oPortDef.bPopulated) {
        pPort->oPortDef.bPopulated = OMX_FALSE;
        NvxMutexUnlock(pPort->hMutex);
        // worker mutex should already be locked from a previous line in
        // this function, so we'll pass OMX_TRUE directly here
        NvxPortReturnBuffers(pPort, OMX_TRUE);
        NvxWorkerTrigger(&(pNvComp->oWorkerData));
        NvxMutexLock(pPort->hMutex);
    }

    /* find the buffer header and delete it */
    if (pPort->ppBuffers && pPort->ppBufferHdrs)
    {
        for (i = 0; i < pPort->nBuffers; i++)
        {
            if (pPort->ppBufferHdrs[i] == pBufferHdr)
            {
                NvxBufferPlatformPrivate *pPlatPriv;
                pPlatPriv = pBufferHdr->pPlatformPrivate;

                if (pPlatPriv && pPlatPriv->eType == NVX_BUFFERTYPE_EGLIMAGE)
                {
                    /* delete OMX-specific data about EGL image */
                    NvxEglFreeImageSibling(
                            (NvxEglImageSiblingHandle)pBufferHdr->pBuffer);
                }
                else if (pPort->ppBuffers[i] == pBufferHdr->pBuffer)
                {
                    NvxSurface *pSurf = NULL;
                    if (pPort->bAllocateRmSurf)
                    {
                        NvU32 i = 0;
                        for (i = 0; i < pPort->nReqBufferCount; i++)
                        {
                            if (pBufferHdr->pBuffer == pPort->ppBufferHdrs[i]->pBuffer)
                            {
                                pSurf = (NvxSurface *)pPort->ppRmSurf[i];
                                break;
                            }
                        }
                    }
                    if (pSurf)
                    {
                        NvxFreeRmSurf(pBufferHdr->pBuffer, pBufferHdr->nAllocLen);
                    }
                    else
                    {
                        /* delete buffer we have allocated */
                        NvOsFree(pBufferHdr->pBuffer);
                    }
                }

                if (pNvComp->FreeBufferCB)
                    pNvComp->FreeBufferCB(pNvComp, pBufferHdr);

                if (pBufferHdr->pPlatformPrivate)
                {
                    NvOsFree(pBufferHdr->pPlatformPrivate);
                }

                /* delete this buffer by replacing it with last buffer and reducing the number of buffers by 1 */
                --pPort->nBuffers;
                pPort->ppBufferHdrs[i] = pPort->ppBufferHdrs[pPort->nBuffers];
                pPort->ppBuffers[i] = pPort->ppBuffers[pPort->nBuffers];
                pPort->ppBufferHdrs[pPort->nBuffers] = NULL;
                pPort->ppBuffers[pPort->nBuffers] = NULL;
                pPort->ppRmSurf[pPort->nBuffers] = NULL;

                if (pBufferHdr == pPort->pCurrentBufferHdr)
                    pPort->pCurrentBufferHdr = NULL;

                NvOsFree(pBufferHdr);
                break;
            }
        }
    }

    /* release remaining port resources when last buffer header is freed */
    bNeedRelease = (pPort->nBuffers == 0 && (!NvxPortIsSupplier(pPort) || !pPort->hTunnelComponent));

    NvxMutexUnlock(pPort->hMutex);

    NvxMutexUnlock(pNvComp->hWorkerMutex);
    if (bNeedRelease) {
        NvxCheckError(eError, NvxWorkerTrigger(&(pNvComp->oWorkerData)));
    }

    return eError;
}

static OMX_ERRORTYPE NvxCheckBufferHdr(OMX_IN OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    if (pBufferHdr == NULL)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxCheckBufferHdr:pBufferHdr = %ld:[%s(%d)]\n",
        pBufferHdr,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    if (pBufferHdr->nSize != sizeof(OMX_BUFFERHEADERTYPE))
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxCheckBufferHdr:nSize (%ld) !="
           " sizeof(OMX_BUFFERHEADERTYPE)(%ld):[%s(%d)]\n",pBufferHdr->nSize,sizeof(OMX_BUFFERHEADERTYPE),__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    if (CheckOMXVersion(pBufferHdr->nVersion, NvxComponentSpecVersion) !=
        OMX_ErrorNone)
    {
        return OMX_ErrorVersionMismatch;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxEmptyThisBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    NvxComponent *pNvComp;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BOOL bHandled = OMX_FALSE;
    if (NvxIsError(eError = NvxCheckBufferHdr(pBufferHdr)))
      return eError;
    if (!hComponent)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED,"ERROR:NvxEmptyThisBuffer:hComponent = %ld:[%s(%d)]\n",
            hComponent,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }
    pNvComp = NvxComponentFromHandle(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxEmptyThisBuffer: %s\n",pNvComp->pComponentName));

    if (pBufferHdr->nInputPortIndex >= pNvComp->nPorts)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxEmptyThisBuffer:%s"
             ":nInputPortIndex(%d) >= nPorts(%d) :[%s(%d)]\n",pNvComp->pComponentName
             ,pBufferHdr->nInputPortIndex,pNvComp->nPorts,__FILE__, __LINE__));
        return OMX_ErrorBadPortIndex;
    }

    NvxMutexLock(pNvComp->pPorts[pBufferHdr->nInputPortIndex].hMutex);

    if ((pNvComp->eState != OMX_StateExecuting &&
        pNvComp->ePendingState != OMX_StateExecuting &&
        pNvComp->eState != OMX_StatePause &&
        pNvComp->ePendingState != OMX_StatePause) ||
        (pNvComp->ePendingState == OMX_StateIdle &&
        !NvxPortIsAllocator(&pNvComp->pPorts[pBufferHdr->nInputPortIndex])))
    {
        NvxMutexUnlock(pNvComp->pPorts[pBufferHdr->nInputPortIndex].hMutex);
        NVXTRACE((NVXT_BUFFER,pNvComp->eObjectType, "ERROR:NvxEmptyThisBuffer:%s:"
              " state not ready: eState = %d:ePendingState = %d:[%s(%d)]\n",pNvComp->pComponentName
              ,(int)pNvComp->eState,(int)pNvComp->ePendingState,__FILE__, __LINE__));
        return OMX_ErrorNotReady;
    }

    if (pNvComp->EmptyThisBuffer)
        pNvComp->EmptyThisBuffer(pNvComp, pBufferHdr, &bHandled);

    /* Store port indexes that need work somewhere? */
    if (OMX_FALSE == bHandled)
    {
        eError = NvxPortEmptyThisBuffer( &pNvComp->pPorts[pBufferHdr->nInputPortIndex], pBufferHdr );
        if (NvxIsError(eError))
        {
            NvxMutexUnlock(pNvComp->pPorts[pBufferHdr->nInputPortIndex].hMutex);
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxEmptyThisBuffer:%s:eError "
               "= %x:[%s(%d)]\n",pNvComp->pComponentName,eError,__FILE__, __LINE__));
            return eError;
        }

        eError = NvxWorkerTrigger(&(pNvComp->oWorkerData));
    }
    NvxMutexUnlock(pNvComp->pPorts[pBufferHdr->nInputPortIndex].hMutex);
    return eError;
}

OMX_ERRORTYPE NvxFillThisBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                OMX_IN OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    NvxComponent *pNvComp;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    if (NvxIsError(eError = NvxCheckBufferHdr(pBufferHdr)))
      return eError;

    if (!hComponent)
    {
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:"
                  "hComponent = %ld:[%s(%d)]\n",hComponent,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pNvComp = NvxComponentFromHandle(hComponent);

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxFillThisBuffer: %s\n",
              pNvComp->pComponentName));

    if (pBufferHdr->nOutputPortIndex >= pNvComp->nPorts)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:%s: "
             "nOutputPortIndex(%d) >= nPorts(%d) :[%s(%d)]\n",pNvComp->pComponentName
             ,pBufferHdr->nOutputPortIndex,pNvComp->nPorts,__FILE__, __LINE__));
        return OMX_ErrorBadPortIndex;
    }


    NvxMutexLock(pNvComp->pPorts[pBufferHdr->nOutputPortIndex].hMutex);

    if ((pNvComp->eState != OMX_StateExecuting &&
        pNvComp->ePendingState != OMX_StateExecuting &&
        pNvComp->eState != OMX_StatePause &&
        pNvComp->ePendingState != OMX_StatePause) ||
        (pNvComp->ePendingState == OMX_StateIdle &&
        !NvxPortIsAllocator(&pNvComp->pPorts[pBufferHdr->nOutputPortIndex])))
    {
        NvxMutexUnlock(pNvComp->pPorts[pBufferHdr->nOutputPortIndex].hMutex);
        NVXTRACE((NVXT_BUFFER, pNvComp->eObjectType, "NvxFillThisBuffer state not ready: %s "
                  "eState = %d:ePendingState = %d\n", pNvComp->pComponentName,
               (int)pNvComp->eState,(int)pNvComp->ePendingState,__FILE__, __LINE__));
        return OMX_ErrorNotReady;
    }

    /* Store port indexes that need work somewhere? */
    eError = NvxPortFillThisBuffer( &pNvComp->pPorts[pBufferHdr->nOutputPortIndex], pBufferHdr );
    if (NvxIsError(eError))
    {
        NvxMutexUnlock(pNvComp->pPorts[pBufferHdr->nOutputPortIndex].hMutex);
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFillThisBuffer:%s:eError = %x :[%s(%d)]\n",
        pNvComp->pComponentName,eError,__FILE__, __LINE__));
        return eError;
    }

    if (OMX_ErrorNone == eError && pNvComp->FillThisBufferCB)
    {
        if (OMX_ErrorNone != pNvComp->FillThisBufferCB(pNvComp, pBufferHdr))
            eError = NvxWorkerTrigger(&(pNvComp->oWorkerData));
    }
    else
        eError = NvxWorkerTrigger(&(pNvComp->oWorkerData));

    NvxMutexUnlock(pNvComp->pPorts[pBufferHdr->nOutputPortIndex].hMutex);

    return eError;
}

OMX_ERRORTYPE NvxComponentRoleEnum(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_INOUT OMX_U8 *cRole,
                                   OMX_IN OMX_U32 nIndex)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent *pNvComp;

    if (!hComponent){
        NVXTRACE((NVXT_ERROR,NVXT_UNDESIGNATED, "ERROR:NvxComponentRoleEnum"
             ":hComponent = %ld :[%s(%d)]\n",hComponent,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    pNvComp = NvxComponentFromHandle(hComponent);
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType,
                        "+NvxComponentRoleEnum: %s\n",pNvComp->pComponentName));

    if ((pNvComp->nComponentRoles == 0) || (nIndex >= pNvComp->nComponentRoles))
        return OMX_ErrorNoMore;

    NvOsStrncpy((char*)cRole, pNvComp->sComponentRoles[nIndex],
                OMX_MAX_STRINGNAME_SIZE);
    return eError;

}

static OMX_ERRORTYPE NvxComponentCreateTunnels(OMX_IN NvxComponent *pNvComp)
{
    OMX_U32 uPort;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE ePortError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentCreateTunnels: %s\n"
        ,pNvComp->pComponentName));
    /* first decide on buffer sharing */
    for (uPort = 0; uPort < pNvComp->nPorts; ++uPort) {
        NvxPortDecideSharing(pNvComp->pPorts + uPort);
    }

    for (uPort = 0; uPort < pNvComp->nPorts; ++uPort) {
        if (pNvComp->pPorts[uPort].oPortDef.bEnabled) {
            ePortError = NvxPortCreateTunnel(pNvComp->pPorts + uPort);
            if (ePortError == OMX_ErrorNotReady) {
                /* save not ready error until the end so that we attempt to create a tunnel on all ports before quitting */
                NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "ComponentCreateTunnels not ready yet: %s:%d\n", pNvComp->pComponentName, uPort));
                eError = OMX_ErrorNotReady;
            }
            else if (NvxIsError(ePortError)) {
                /* return other errors immediately */
                return ePortError;
            }
        }
    }
    return eError;
}

static OMX_ERRORTYPE NvxComponentDestroyTunnels(OMX_IN NvxComponent *pNvComp)
{
    OMX_U32 uPort;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentCreateTunnels: %s\n",pNvComp->pComponentName));

    for (uPort = 0; uPort < pNvComp->nPorts; ++uPort) {
        OMX_ERRORTYPE ePortError = NvxPortReleaseResources(pNvComp->pPorts + uPort);
        if (ePortError == OMX_ErrorNotReady) {
            /* save not ready error until the end so that we attempt to create a tunnel on all ports before quitting */
            NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "ComponentDestroyTunnels not ready yet: %s:%d\n", pNvComp->pComponentName, uPort));
            eError = OMX_ErrorNotReady;
        }
        else if (NvxIsError(ePortError)) {
            /* return other errors immediately */
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentDestroyTunnels:%s:port = %d:[%s(%d)]\n",
            pNvComp->pComponentName,(int)(pNvComp->pPorts + uPort),__FILE__, __LINE__));
            return ePortError;
        }
    }
    return eError;
}

static OMX_BOOL NvxValidStateTransition( OMX_IN OMX_STATETYPE oOldState, OMX_IN OMX_STATETYPE oNewState)
{
    if (oNewState == OMX_StateInvalid)
        return OMX_TRUE;

    if (oOldState == OMX_StateLoaded && (oNewState != OMX_StateIdle &&
        oNewState != OMX_StateWaitForResources))
    {
        return OMX_FALSE;
    }
    else if (oOldState == OMX_StateWaitForResources &&
             (oNewState != OMX_StateIdle && oNewState != OMX_StateLoaded))
    {
        return OMX_FALSE;
    }
    else if (oOldState == OMX_StateIdle && (oNewState != OMX_StatePause &&
             oNewState != OMX_StateExecuting && oNewState != OMX_StateLoaded))
    {
        return OMX_FALSE;
    }
    else if (oOldState == OMX_StateExecuting &&
             (oNewState != OMX_StateIdle && oNewState != OMX_StatePause))
    {
        return OMX_FALSE;
    }
    else if (oOldState == OMX_StatePause &&
             (oNewState != OMX_StateExecuting && oNewState != OMX_StateIdle))
    {
        return OMX_FALSE;
    }
    else if (oOldState == OMX_StateInvalid)
    {
        return OMX_FALSE;
    }

    return OMX_TRUE;
}

static OMX_BOOL NvxCanStop(OMX_IN NvxComponent *pNvComp)
{
    NvxPort *pPort;
    OMX_U32 i;
    int nBuffers;
    OMX_BOOL bAllMyBuffersReturned;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxCanStop: %s\n",pNvComp->pComponentName));

    /* begin with the assumption that all our buffers are returned */
    bAllMyBuffersReturned = OMX_TRUE;

    /* check for all our buffers returned. Return buffers that aren't ours */
    for(i=0;i<pNvComp->nPorts;i++)
    {
        pPort = &pNvComp->pPorts[i];
        if (pPort->hTunnelComponent)
        {
            if (NvxPortIsReusingBuffers(pPort))
            {
                int sharedBuffers = 0;
                sharedBuffers = NvxPortCountBuffersSharedAndDelivered(pPort);
                if ((OMX_U32)sharedBuffers != pPort->nBuffers)
                {
                    NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "CanStop0: %s:%d is sharing but still has buffers (%d of %d)\n", pNvComp->pComponentName, i, sharedBuffers, pPort->nBuffers));
                    bAllMyBuffersReturned = OMX_FALSE;
                }
            }
            else if ((pPort->oPortDef.eDir == OMX_DirInput) &&
                 (pPort->eSupplierSetting == OMX_BufferSupplyInput) &&
                  pNvComp->bWasExecuting)
            {
                /* supplier input - check for returned buffers */
                if(pPort->pFullBuffers ==NULL)
                {
                   nBuffers=0;
                }
                else
                {
                    nBuffers = NvMMQueueGetNumEntries(pPort->pFullBuffers);
                }

                if ((OMX_U32)nBuffers != pPort->nBuffers){
                    NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "CanStop1: %s:%d has %d but needs %d\n",
                              pNvComp->pComponentName, i, nBuffers, pPort->nBuffers));


                    bAllMyBuffersReturned = OMX_FALSE;
                }
            }
            else if ((pPort->oPortDef.eDir == OMX_DirOutput) && (pPort->eSupplierSetting == OMX_BufferSupplyOutput))
            {
                /* supplier output - check for returned buffers */
                nBuffers = NvxListGetNumEntries(pPort->pEmptyBuffers);
                if ((OMX_U32)nBuffers != pPort->nBuffers){
                    NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "CanStop2: %s:%d has %d but needs %d\n",
                              pNvComp->pComponentName, i, nBuffers, pPort->nBuffers));
                    bAllMyBuffersReturned = OMX_FALSE;
                }
            }
        }
    }
    return bAllMyBuffersReturned;
}

static OMX_ERRORTYPE NvxChangeState(OMX_IN NvxComponent *pNvComp,
                                    OMX_IN OMX_STATETYPE oNewState,
                                    OMX_BOOL bWorkerLocked)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 i;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxChangeState: %s\n",pNvComp->pComponentName));

    NVX_COMPONENT_PRINT("State change started: %s from %ld to %ld\n",
              pNvComp->pComponentName, pNvComp->eState, oNewState);

    if (pNvComp->eState == oNewState)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "Same state - %s: eState = %ld "
              ": oNewState =%ld:[%s(%d)]\n",pNvComp->pComponentName,pNvComp->eState,oNewState,__FILE__, __LINE__));
        return OMX_ErrorSameState;
    }

    /* check for valid transitions */
    if (!NvxValidStateTransition(pNvComp->eState, oNewState))
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "Not a valid state transition - %s:"
             " from %d to %d\n",pNvComp->pComponentName,pNvComp->eState,oNewState,__FILE__, __LINE__));
        return OMX_ErrorIncorrectStateTransition;
    }

    pNvComp->ePendingState = oNewState;

    if (pNvComp->PreChangeState)
    {

        pNvComp->PreChangeState(pNvComp, oNewState);
    }

    /* change state */
    switch (oNewState)
    {
        case OMX_StateInvalid:
        case OMX_StateLoaded:
        {
            if ((pNvComp->eState == OMX_StateExecuting ||
                 pNvComp->eState == OMX_StatePause) &&
                pNvComp->ePendingState != OMX_StateInvalid)
            {
                /* Return buffers that aren't ours */
                for(i=0;i<pNvComp->nPorts;i++)
                {
                    if (pNvComp->ReturnBuffers)
                        pNvComp->ReturnBuffers(pNvComp, i, bWorkerLocked);
                    NvxPortReturnBuffers(&pNvComp->pPorts[i], bWorkerLocked);
                }
            }
            if (pNvComp->eState == OMX_StateWaitForResources)
            {
                if (pNvComp->CancelWaitForResources)
                    pNvComp->CancelWaitForResources(pNvComp);
            }
            NvxLockAcqLock();
            if (pNvComp->ReleaseResources)
                eError = pNvComp->ReleaseResources(pNvComp);
            if (OMX_ErrorNone != eError)
                NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "%s to loaded: release resources error: %x\n", pNvComp->pComponentName, eError));
            pNvComp->eResourcesState = NVX_RESOURCES_HAVENONE;
            pNvComp->nRmResourcesAcquired = 0;
            NvxCheckError( eError, NvxComponentDestroyTunnels(pNvComp) );
            NvxUnlockAcqLock();
            if (oNewState == OMX_StateInvalid) {
                for(i=0;i<pNvComp->nPorts;i++){
                    pNvComp->pPorts[i].hTunnelComponent = NULL;
                    pNvComp->pPorts[i].nTunnelPort = 0;
                }
            }
            if (eError == OMX_ErrorNotReady) {
                if (oNewState == OMX_StateInvalid) {
                    eError = OMX_ErrorNone;  /* don't care if we're not ready, we'll clean up further on component destroy */
                }
                else
                {
                    NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "%s is not ready for loaded\n", pNvComp->pComponentName));
                    return OMX_ErrorNotReady;
                }
            }
            break;
        }
        case OMX_StateIdle:
        {
            if (pNvComp->eState == OMX_StateExecuting ||
                pNvComp->eState == OMX_StatePause)
            {
                /* Return buffers that aren't ours */
                for(i=0;i<pNvComp->nPorts;i++){
                    if (pNvComp->ReturnBuffers)
                    {
                        pNvComp->ReturnBuffers(pNvComp, i, bWorkerLocked);
                    }
                    NvxPortReturnBuffers(&pNvComp->pPorts[i], bWorkerLocked);
                }

                /* are we ready to stop now? */
                if (!NvxCanStop(pNvComp))
                {
                    pNvComp->nCanStopCnt++;
                    /* set pending state and fail with not ready */
                    if (pNvComp->nCanStopCnt == 100)
                    {
                        NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "%s to idle not ready yet\n", pNvComp->pComponentName));
                        pNvComp->nCanStopCnt = 0;
                    }

                    return OMX_ErrorNotReady;
                }
                break;
            }

            NvxLockAcqLock();
            eError = NvxComponentCreateTunnels(pNvComp);
            if (eError == OMX_ErrorNone && pNvComp->AcquireResources){
                if(OMX_ErrorNone != (eError = pNvComp->AcquireResources(pNvComp))){
                    if (pNvComp->eResourcesState == NVX_RESOURCES_HAVENONE
                        || pNvComp->eResourcesState == NVX_RESOURCES_HAVESOME) {
                        eError = OMX_ErrorInsufficientResources;
                        if (pNvComp->ReleaseResources)
                            pNvComp->ReleaseResources(pNvComp);
                    }
                    NvxComponentDestroyTunnels(pNvComp);  /* ignore error (propogate previous error) */
                    NvxUnlockAcqLock();
                    return eError;
                }
            }
            NvxUnlockAcqLock();

            if (eError != OMX_ErrorNone) {
                if (eError == OMX_ErrorNotReady) {
                    /* one or more ports are not ready to go idle, try again later */
                    NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "%s to idle not ready yet\n", pNvComp->pComponentName));
                    return OMX_ErrorNotReady;
                }
                /* some other error, release any resources we acquired */

                NvxLockAcqLock();
                if (pNvComp->ReleaseResources)
                    pNvComp->ReleaseResources(pNvComp); /* ignore error (propogate previous error) */
                pNvComp->eResourcesState = NVX_RESOURCES_HAVENONE;
                NvxComponentDestroyTunnels(pNvComp);  /* ignore error (propogate previous error) */
                NvxUnlockAcqLock();
            }

            pNvComp->bWasExecuting = OMX_FALSE;
            pNvComp->eResourcesState = NVX_RESOURCES_HAVEALL;
            pNvComp->nRmResourcesAcquired = pNvComp->nRmResources;
            break;
        }
        case OMX_StateExecuting:
        {
            OMX_BOOL bNotReady = OMX_FALSE;
            if (pNvComp->eState == OMX_StatePause && pNvComp->bWasExecuting)
                break;

            /* send buffers to application */

            /* if we have input ports that are suppliers give the buffers to the output via fillthisbuffer */
            for (i=0;i<pNvComp->nPorts;i++){
                NvxPort *pPort = &pNvComp->pPorts[i];
                // Only start enabled ports:
                if (pPort->oPortDef.bEnabled)
                {
                    NvxCheckError(eError, NvxPortStart(pPort));
                    if (eError == OMX_ErrorNotReady)
                    {
                        NVXTRACE((NVXT_STATE, pNvComp->eObjectType, "%s to executing not ready yet (port %d not started)\n", pNvComp->pComponentName, i));
                        bNotReady = OMX_TRUE;
                    }
                }
            }

            /* if any ports couldn't transition, fail temporarily */
            if (bNotReady)
            {
                return OMX_ErrorNotReady;
            }

            pNvComp->bWasExecuting = OMX_TRUE;
            break;
        }
        case OMX_StatePause:
        {
            /* stops sending buffers temporarily */
            break;
        }
        case OMX_StateWaitForResources:
        {
            if (pNvComp->WaitForResources){
                eError = pNvComp->WaitForResources(pNvComp);
            }
            break;
        }
        default:
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxChangeState:%s"
                 ":NewState = %d :[%s(%d)]\n", pNvComp->pComponentName,oNewState,__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
    }

    if (pNvComp->ChangeState)
    {
        pNvComp->ChangeState(pNvComp, oNewState);
    }

    if (eError != OMX_ErrorNone) {
        oNewState = OMX_StateInvalid;
    }

    if (pNvComp->eState == OMX_StateWaitForResources && pNvComp->eState != oNewState){
        if (pNvComp->CancelWaitForResources){
            pNvComp->CancelWaitForResources(pNvComp);
        }
    }

    NVX_COMPONENT_PRINT("State change completed: %s from %ld to %ld\n",
              pNvComp->pComponentName, pNvComp->eState, oNewState);

    pNvComp->bMajorStateChangePending = OMX_FALSE;
    pNvComp->ePendingState = oNewState;
    pNvComp->eState = oNewState;
    return eError;
}

static OMX_ERRORTYPE NvxFlush(OMX_IN NvxComponent *pNvComp, OMX_U32 nPort)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 i = 0;
    OMX_U32 nFirstPort = 0, nLastPort = pNvComp->nPorts - 1;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxFlush: %s\n",pNvComp->pComponentName));

    if (pNvComp->Flush)
        pNvComp->Flush(pNvComp, nPort);

   if (nPort == OMX_ALL) {
        /* flush all */
    }
    else if ((OMX_U32)nPort < pNvComp->nPorts) {
        nFirstPort = nLastPort = (OMX_U32)nPort;
    }
    else
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxFlush:%s:"
            "nPort (%d) < pNvComp->nPorts (%d):[%s(%d)]\n",pNvComp->pComponentName
            ,nPort,pNvComp->nPorts,__FILE__, __LINE__));
        return OMX_ErrorBadPortIndex;
    }

    for (i = nFirstPort; i <= nLastPort; i++) {
        OMX_ERRORTYPE ePortError = NvxPortFlushBuffers(&pNvComp->pPorts[i]);
        NvxCheckError(eError, ePortError);
        NvxCheckError(eError, pNvComp->pCallbacks->EventHandler(
                                  pNvComp->hBaseComponent,
                                  pNvComp->pCallbackAppData,
                                  OMX_EventCmdComplete, OMX_CommandFlush, i,
                                  NULL));
    }

    if (nPort == OMX_ALL)
    {
        NvxCheckError(eError, pNvComp->pCallbacks->EventHandler(
                                  pNvComp->hBaseComponent,
                                  pNvComp->pCallbackAppData,
                                  OMX_EventCmdComplete, OMX_CommandFlush,
                                  OMX_ALL, NULL));
    }

    if (OMX_ErrorNone == eError)
        pNvComp->bMajorStateChangePending = OMX_FALSE;

    return eError;
}

static OMX_ERRORTYPE NvxDisablePort(OMX_IN NvxComponent *pNvComp, OMX_U32 nPort)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 i = 0;
    OMX_U32 nFirstPort = 0, nLastPort = pNvComp->nPorts - 1;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxDisablePort: %s\n",pNvComp->pComponentName));

    if ((OMX_U32)nPort < pNvComp->nPorts) {
        nFirstPort = nLastPort = (OMX_U32)nPort;
    }
    else if (nPort != OMX_ALL)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:DisablePort:%s:"
            "nPort (%d) != OMX_ALL and >= pNvComp->nPorts:[%s(%d)]\n",
            pNvComp->pComponentName,nPort,__FILE__, __LINE__));
        return OMX_ErrorBadPortIndex;
    }

    for (i = nFirstPort; i <= nLastPort; i++) {
        if(pNvComp->PortEventHandler)
        {
            eError = pNvComp->PortEventHandler(pNvComp, i, 0);
            if (eError != OMX_ErrorNone)
                goto finish;
        }
    }

    for (i = nFirstPort; i <= nLastPort; i++) {
        if (pNvComp->eState == OMX_StateLoaded) {
            pNvComp->pPorts[i].oPortDef.bEnabled = OMX_FALSE;
        }
        else {
            if (pNvComp->pPorts[i].oPortDef.bEnabled == OMX_TRUE) {
                OMX_ERRORTYPE ePortError = NvxPortStop(pNvComp->pPorts + i);
                NvxCheckError(eError, ePortError);
            }
        }
    }
    if (eError != OMX_ErrorNone)
        goto finish;

    for (i = nFirstPort; i <= nLastPort; i++) {
        NvxCheckError(eError, pNvComp->pCallbacks->EventHandler(pNvComp->hBaseComponent,
                                                       pNvComp->pCallbackAppData,
                                                       OMX_EventCmdComplete, OMX_CommandPortDisable, i, NULL));
    }

finish:

    if (OMX_ErrorNone == eError)
        pNvComp->bMajorStateChangePending = OMX_FALSE;

    return eError;
}

static OMX_ERRORTYPE NvxEnablePort(OMX_IN NvxComponent *pNvComp, OMX_U32 nPort)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nFirstPort = 0, nLastPort = pNvComp->nPorts - 1;
    OMX_U32 i = 0;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxEnablePort: %s\n",pNvComp->pComponentName));
    if ((OMX_U32)nPort < pNvComp->nPorts) {
        nFirstPort = nLastPort = (OMX_U32)nPort;
    }
    else if (nPort != OMX_ALL)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxDisablePort:%s:"
            "nPort(%d) > nPorts(%d) and != OMX_ALL :[%s(%d)]\n",pNvComp->pComponentName
            ,nPort, pNvComp->nPorts,__FILE__, __LINE__));
        return OMX_ErrorBadPortIndex;
    }

    for (i = nFirstPort; i <= nLastPort; i++) {
        if(pNvComp->PortEventHandler)
        {
            eError = pNvComp->PortEventHandler(pNvComp, i, 1);
            if (eError != OMX_ErrorNone)
                goto finish;
        }
    }

    for (i = nFirstPort; i <= nLastPort; i++) {
        if (pNvComp->eState == OMX_StateLoaded) {
            pNvComp->pPorts[i].oPortDef.bEnabled = OMX_TRUE;
        }
        else {
            OMX_BOOL bIdle = (pNvComp->eState == OMX_StateIdle);
            NvxPort *pPort = pNvComp->pPorts + i;
            if (!NvxPortIsReady(pPort)) {
                OMX_ERRORTYPE ePortError = NvxPortRestart(pPort, bIdle);
                NvxCheckError(eError, ePortError);
            }
        }
    }
    if (eError != OMX_ErrorNone)
        goto finish;

    for (i = nFirstPort; i <= nLastPort; i++) {
        NvxCheckError(eError, pNvComp->pCallbacks->EventHandler(pNvComp->hBaseComponent,
                                                       pNvComp->pCallbackAppData,
                                                       OMX_EventCmdComplete, OMX_CommandPortEnable, i, NULL));
    }

finish:

    if (OMX_ErrorNone == eError)
        pNvComp->bMajorStateChangePending = OMX_FALSE;

    return eError;
}

OMX_ERRORTYPE NvxSendEvent(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_EVENTTYPE eEvent,
                           OMX_IN OMX_U32 nData1, OMX_IN OMX_U32 nData2, OMX_IN OMX_PTR pEventData)
{
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxSendEvent: %s\n",pNvComp->pComponentName));
    if (!pNvComp || !pNvComp->pCallbacks || !pNvComp->pCallbacks->EventHandler)
        return OMX_ErrorBadParameter;
    return pNvComp->pCallbacks->EventHandler(pNvComp->hBaseComponent, pNvComp->pCallbackAppData, eEvent, nData1, nData2, pEventData);
}

static OMX_ERRORTYPE NvxCompleteCmd(OMX_IN NvxComponent* pNvComp, OMX_COMMANDTYPE eCmd, OMX_U32 data2, OMX_ERRORTYPE eError)
{
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxCompleteCmd: %s\n",pNvComp->pComponentName));
    if (pNvComp->eState == OMX_StateInvalid)
        eError = OMX_ErrorInvalidState;

    if (eError != OMX_ErrorNone)
        eError = NvxSendEvent(pNvComp, OMX_EventError, eError, data2, NULL);
    else
        eError = NvxSendEvent(pNvComp, OMX_EventCmdComplete, eCmd, data2, NULL);

    return eError;
}

static OMX_ERRORTYPE NvxProcessCommands(OMX_IN NvxComponent *pNvComp,
                                        OMX_IN NvxCommand *pCommand)
{
    OMX_U32 data2 = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxProcessCommands: %s\n",pNvComp->pComponentName));

    switch (pCommand->Cmd)
    {
        case OMX_CommandStateSet:
        {
            data2 = pCommand->nParam1;
            // worker mutex is unlocked here, this function is called directly
            // from the worker function.  pass that info directly to NvxChangeState
            if (OMX_ErrorNotReady == (eError = NvxChangeState(pNvComp, pCommand->nParam1, OMX_FALSE)))
                return eError;
            break;
        }
        case OMX_CommandFlush:
        {

            NvxMutexLock(pNvComp->hWorkerMutex);
            eError = NvxFlush(pNvComp, pCommand->nParam1);
            NvxMutexUnlock(pNvComp->hWorkerMutex);

           if (NvxIsSuccess(eError))
                return OMX_ErrorNone; /* bypass event handling, flush must do it internally */
            break;
        }
        case OMX_CommandPortDisable:
        {
            NvxMutexLock(pNvComp->hWorkerMutex);
            data2 = pCommand->nParam1;
            eError = NvxDisablePort(pNvComp, data2);
            NvxMutexUnlock(pNvComp->hWorkerMutex);

            if (NvxIsSuccess(eError))
                return OMX_ErrorNone; /* bypass event handling, disable port must do it internally */
            if (OMX_ErrorNotReady == eError)
            {
              /*  NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxProcessCommands:%s:Disable port"
                     "=%d:[%s(%d)]\n",pNvComp->pComponentName,(int)data2,__FILE__, __LINE__);
              */
                return eError;
            }
            break;
        }
        case OMX_CommandPortEnable:
        {
            NvxMutexLock(pNvComp->hWorkerMutex);
            data2 = pCommand->nParam1;
            eError = NvxEnablePort(pNvComp, data2);
            NvxMutexUnlock(pNvComp->hWorkerMutex);

            if (NvxIsSuccess(eError))
                return OMX_ErrorNone; /* bypass event handling, enable port must do it internally */
            if (OMX_ErrorNotReady == eError)
            {
             /*   NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxProcesscommands:%s:Enable port"
                     "=%d:[%s(%d)]\n",pNvComp->pComponentName,(int)data2,__FILE__, __LINE__);
             */
                return eError;
            }
            break;
        }
        case OMX_CommandMarkBuffer:
        {
            data2 = pCommand->nParam1;
            if (data2 < pNvComp->nPorts)
                eError = NvxPortMarkBuffer(pNvComp->pPorts + data2, (OMX_MARKTYPE *)(void *)(&pCommand->oCmdData[0]));
            else
                eError = OMX_ErrorBadPortIndex;
            break;
        }
        default:
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxProcessCommands:%s:Cmd = %d :[%s(%d)]\n",
            pNvComp->pComponentName,pCommand->Cmd,__FILE__, __LINE__));
            eError = OMX_ErrorBadParameter;
        }
    }
    return NvxCompleteCmd(pNvComp, pCommand->Cmd, data2, eError);
}

static OMX_ERRORTYPE PerformAnyPendingSetConfig(NvxComponent *pNvComp)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+PerformAnyPendingSetConfig: %s\n",pNvComp->pComponentName));

    /* check for pending SetConfig */
    if (NVX_SETCONFIGSTATE_PENDING == pNvComp->eSetConfigState)
    {
        NvxMutexLock(pNvComp->hConfigMutex);
        /* indicate we are now processing */
        pNvComp->eSetConfigState = NVX_SETCONFIGSTATE_PROCESSING;
        NvxMutexUnlock(pNvComp->hConfigMutex);

        /* actually perform the SetConfig */
        error = pNvComp->SetConfig(pNvComp,
            pNvComp->nPendingConfigIndex, pNvComp->pPendingConfigStructure);

        /* save result and indicate we processing has completed */
        NvxMutexLock(pNvComp->hConfigMutex);
        pNvComp->eSetConfigError = error;
        pNvComp->eSetConfigState = NVX_SETCONFIGSTATE_COMPLETE;
        NvxMutexUnlock(pNvComp->hConfigMutex);

        /* signal waiting SetConfig entrypoint on the other thread */
        NvOsSemaphoreSignal(pNvComp->hSetConfigEvent);
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxTransitionToState(NvxComponent *pNvComp, OMX_STATETYPE eState)
{
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxTransitionToState: %s\n",pNvComp->pComponentName));
    // worker mutex is held in every call path that leads here
    // pass that info on to NvxChangeState
    return NvxCompleteCmd(pNvComp, OMX_CommandStateSet, eState, NvxChangeState(pNvComp, eState, OMX_TRUE));
}

static OMX_ERRORTYPE NvxComponentCheckResourceFlags(NvxComponent *pNvComp)
{
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentCheckResourceFlags: %s\n",pNvComp->pComponentName));
    /* If we're waiting for resources and they've been made available */
    if (pNvComp->eState == OMX_StateWaitForResources && pNvComp->eResourcesState == NVX_RESOURCES_PROVIDED) {
        return NvxTransitionToState(pNvComp, OMX_StateIdle);
    }

    if (pNvComp->eResourcesState == NVX_RESOURCES_RECLAIMED) {
        if (pNvComp->eState == OMX_StateExecuting) {
            /* If we're executing but lost a resource */
            pNvComp->pCallbacks->EventHandler(pNvComp->hBaseComponent, pNvComp->pCallbackAppData, OMX_EventError, OMX_ErrorResourcesPreempted, 0, 0);
            return NvxTransitionToState(pNvComp, OMX_StateIdle);
        }
        else if (pNvComp->eState == OMX_StateIdle) {
            /* If we're idle and lost a resource */
            pNvComp->pCallbacks->EventHandler(pNvComp->hBaseComponent, pNvComp->pCallbackAppData, OMX_EventError, OMX_ErrorResourcesLost, 0, 0);
            return NvxTransitionToState(pNvComp, OMX_StateLoaded);
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxBaseWorkerFunction(OMX_IN NvxWorker* pWorker,
                                    OMX_IN NvxTimeMs uMaxTime,
                                    OMX_OUT OMX_BOOL* pbMoreWork,
                                    OMX_INOUT NvxTimeMs* puMaxMsecToNextCall)
{
    NvxComponent *pNvComp;
    NvxCommand command;
    OMX_ERRORTYPE error = OMX_ErrorNone;
    OMX_U32 uPort;
    OMX_BOOL bAllPortsReady;

    NV_ASSERT(pWorker);
    NV_ASSERT(pbMoreWork);

    *pbMoreWork = OMX_FALSE;
    *puMaxMsecToNextCall = NVX_TIMEOUT_NEVER;

    pNvComp = (NvxComponent*)(pWorker->pWorkerData);
    NV_ASSERT(pNvComp);

    PerformAnyPendingSetConfig(pNvComp);

    /* try to execute the command before actually removing the command from the queue */
    if (NvMMQueueGetNumEntries(pNvComp->pCommandQueue) > 0)
    {
        NvMMQueuePeek(pNvComp->pCommandQueue, &command);
        if (OMX_ErrorNotReady != NvxProcessCommands(pNvComp, &command))
        {
            NvMMQueueDeQ(pNvComp->pCommandQueue, &command);
        }
        else
        {
            /* wait for trigger on not ready */
            *pbMoreWork = OMX_FALSE;
            *puMaxMsecToNextCall = 100;
            return OMX_ErrorNone;
        }
    }
    else
    {
        if (pNvComp->eState == OMX_StateExecuting ||
            pNvComp->eState == OMX_StatePause ||
            pNvComp->eState == OMX_StateIdle)
        {
            for (uPort = 0; uPort < pNvComp->nPorts; ++uPort) {
                NvxPort *pPort = &(pNvComp->pPorts[uPort]);
                OMX_BOOL bUnpopulated = pPort->oPortDef.bEnabled &&
                                        !pPort->oPortDef.bPopulated;
                if (bUnpopulated)
                {
                   pNvComp->nCanStopCnt++;
                   NvOsDebugPrintf("Error Unpopulated %s:%d (Populated %d)\n",
                                  pNvComp->pComponentName,pPort->oPortDef.nPortIndex,
                                  pPort->oPortDef.bPopulated);
                   if (pNvComp->nCanStopCnt >= 5)
                   {
                      pNvComp->pCallbacks->EventHandler(pNvComp->hBaseComponent,
                                         pNvComp->pCallbackAppData,
                                         OMX_EventError,
                                         OMX_ErrorPortUnpopulated,
                                         uPort, 0);
                   }
                   break;
                }
            }
        }
    }

    NvxMutexLock(pNvComp->hWorkerMutex);

    NvxComponentCheckResourceFlags(pNvComp);

    /* let worker process buffers */
    if (pNvComp->WorkerFunction &&
        ((pNvComp->eState == OMX_StateExecuting &&
          pNvComp->ePendingState == OMX_StateExecuting) ||
         (OMX_TRUE == pNvComp->bNeedRunWorker)))
    {
        for (uPort = 0; uPort < pNvComp->nPorts; ++uPort)
        {
            NvxPort *pPort = &(pNvComp->pPorts[uPort]);

            if (NvxPortNumPendingBuffers(pPort) > 0)
            {
                if (OMX_ErrorNone != NvxPortSendPendingBuffers(pPort))
                {
                    *puMaxMsecToNextCall = 1;
                    *pbMoreWork = OMX_FALSE;
                    NvxMutexUnlock(pNvComp->hWorkerMutex);
                    return OMX_ErrorNone;
                }
            }
        }

        /* Check for buffers to process here + valid ports.. */
        bAllPortsReady = OMX_TRUE;
        for (uPort = 0; uPort < pNvComp->nPorts; ++uPort) {
            NvxPort *pPort;
            pPort = &(pNvComp->pPorts[uPort]);

            // Skip non enabled ports
            if (!pPort->oPortDef.bEnabled)
                continue;

            if (!NvxPortIsReady(pPort)) {
                // skip stopped ports and ports in the process of starting/stopping
                bAllPortsReady = OMX_FALSE;
                continue;
            }

            if (OMX_FALSE == NvxPortGetNextHdr(pPort)){
                bAllPortsReady = OMX_FALSE;
            }
        }

        error = pNvComp->WorkerFunction(pNvComp, bAllPortsReady, pbMoreWork, puMaxMsecToNextCall);

        pNvComp->bNeedRunWorker = OMX_FALSE;

        if (error == OMX_ErrorNotReady)
            error = OMX_ErrorNone;
        if (error != OMX_ErrorNone)
        {
            NvOsDebugPrintf("%s[%d] comp %s Error %d \n",__FUNCTION__, __LINE__,
                pNvComp->pComponentName, error);
            *puMaxMsecToNextCall = 1;
            *pbMoreWork = OMX_FALSE;
            NvxMutexUnlock(pNvComp->hWorkerMutex);
            NvxSendEvent(pNvComp, OMX_EventError, error, 0, NULL);
            return OMX_ErrorNone;
        }
    }

    if (NvMMQueueGetNumEntries(pNvComp->pCommandQueue) > 0)
        *pbMoreWork = OMX_TRUE;

    NvxMutexUnlock(pNvComp->hWorkerMutex);

    return error;
}


static OMX_ERRORTYPE fillInInitStructure(OMX_IN NvxComponent* pNvComp,
    OMX_IN OMX_PORTDOMAINTYPE ePortDomain, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    /* note: OMX_xxxx_PARAM_TYPE are the same for audio, video, image,
     * and other.  If this changes in the future we'll need to change
     * the way we handle these params */
    OMX_PORT_PARAM_TYPE* pInitStruct = pComponentParameterStructure;
    unsigned i, nFirstPort, nLastPort;

    if (pInitStruct->nSize != sizeof(OMX_PORT_PARAM_TYPE))
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxBaseWorkerFunction:%s:"
             "pInitStruct->nSize (%d) != sizeof(OMX_PORT_PARAM_TYPE)%d:[%s(%d)]\n",
             pNvComp->pComponentName,pInitStruct->nSize , sizeof(OMX_PORT_PARAM_TYPE),__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    if (CheckOMXVersion(pInitStruct->nVersion, pNvComp->oSpecVersion) !=
        OMX_ErrorNone)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxBaseWorkerFunction:%s:nVersion (%d)"
              "!= oSpecVersion.nVersion (%d):[%s(%d)]\n",pNvComp->pComponentName,
              pInitStruct->nVersion.nVersion,pNvComp->oSpecVersion.nVersion,__FILE__, __LINE__));
        return OMX_ErrorVersionMismatch;
    }

    nFirstPort = nLastPort = pNvComp->nPorts;
    for (i = 0; i < pNvComp->nPorts; i++) {
        if (pNvComp->pPorts[i].oPortDef.eDomain == ePortDomain)
        {
            if (nFirstPort > i)
                nFirstPort = i;
            nLastPort = i;
        }
        else if (nFirstPort < i)
            /* some previous iteration found a matching port, and this one doesn't match, so we found the last one already. */
            break;
    }
    if (nFirstPort < pNvComp->nPorts)
    {
        pInitStruct->nPorts = nLastPort-nFirstPort+1;
        pInitStruct->nStartPortNumber = nFirstPort;
        return OMX_ErrorNone;
    }
    else
    {
        pInitStruct->nPorts = 0;
        pInitStruct->nStartPortNumber = 0;
        return OMX_ErrorNone;
    }

}


OMX_ERRORTYPE NvxComponentBaseGetConfig(
        OMX_IN    NvxComponent* pNvComp,
        OMX_IN    OMX_INDEXTYPE nIndex,
        OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetConfig:%s:nIndex "
        " = %d :[%s(%d)]\n",pNvComp->pComponentName,nIndex,__FILE__, __LINE__));
    return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE NvxComponentBaseSetConfig(
        OMX_IN NvxComponent* pNvComp,
        OMX_IN OMX_INDEXTYPE nIndex,
        OMX_IN OMX_PTR pComponentConfigStructure)
{
    NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetConfig:%s:nIndex "
         "= %d :[%s(%d)]\n",pNvComp->pComponentName,nIndex,__FILE__, __LINE__));
    return OMX_ErrorUnsupportedIndex;
}


OMX_ERRORTYPE NvxComponentBaseGetParameter(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_INDEXTYPE nParamIndex,
    OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    NvxPort *pPort;
    OMX_U32 nPort;

    if(!pComponentParameterStructure)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
            "pComponentParameterStructure = %d :[%s(%d)]\n",pNvComp->pComponentName,
            pComponentParameterStructure,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    switch (nParamIndex)
    {
    case OMX_IndexParamAudioInit:  return fillInInitStructure(pNvComp, OMX_PortDomainAudio, pComponentParameterStructure);
    case OMX_IndexParamVideoInit:  return fillInInitStructure(pNvComp, OMX_PortDomainVideo, pComponentParameterStructure);
    case OMX_IndexParamImageInit:  return fillInInitStructure(pNvComp, OMX_PortDomainImage, pComponentParameterStructure);
    case OMX_IndexParamOtherInit:  return fillInInitStructure(pNvComp, OMX_PortDomainOther, pComponentParameterStructure);

    case OMX_IndexParamPortDefinition:{
        OMX_U32 nPort;
        OMX_PARAM_PORTDEFINITIONTYPE* pPortDef = pComponentParameterStructure;

        // error check
        nPort = pPortDef->nPortIndex;
        if (nPort >= pNvComp->nPorts)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:nPort(%d)"
                ">= pNvComp->nPorts(%d) :[%s(%d)]\n", pNvComp->pComponentName,nPort,pNvComp->nPorts,__FILE__, __LINE__));
            return OMX_ErrorBadPortIndex;    // incorrect port number
        }

        pPort = &(pNvComp->pPorts[nPort]);
        if (CheckOMXVersion(pPortDef->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "nVersion(%d) != oPortDef.nVersion(%d) :[%s(%d)]\n", pNvComp->pComponentName,
                 pPortDef->nVersion.nVersion , pPort->oPortDef.nVersion.nVersion,__FILE__, __LINE__));
            return OMX_ErrorVersionMismatch; //* incompatbile version numbers
        }

        if (pPortDef->nSize != pPort->oPortDef.nSize)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "Port->nSize (%d) != oPortDef.nSize(%d) :[%s(%d)]\n",\
                 pNvComp->pComponentName,pPortDef->nSize,pPort->oPortDef.nSize,__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }

        NvOsMemcpy(pComponentParameterStructure, &(pPort->oPortDef), pPortDef->nSize);
        return OMX_ErrorNone;
    }
    case OMX_IndexParamAudioPortFormat:{
        OMX_AUDIO_PARAM_PORTFORMATTYPE *pPortFormat = pComponentParameterStructure;

        //error checking
        if (pPortFormat->nPortIndex  >= pNvComp->nPorts)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "nPortIndex(%d)  >= pNvComp->nPorts (%d) :[%s(%d)]\n",
                 pNvComp->pComponentName,pPortFormat->nPortIndex, pNvComp->nPorts,__FILE__, __LINE__));
            return OMX_ErrorBadPortIndex;    // incorrect port number
        }
        pPort = &pNvComp->pPorts[pPortFormat->nPortIndex];

        if (CheckOMXVersion(pPortFormat->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "nVersion(%d) != oPortDef.nVersion(%d) :[%s(%d)]\n",
                 pNvComp->pComponentName,pPortFormat->nVersion.nVersion,pPort->oPortDef.nVersion.nVersion,__FILE__, __LINE__));
            return OMX_ErrorVersionMismatch; // incompatbile version numbers
        }

        if (pPortFormat->nSize != sizeof (*pPortFormat))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                  "nSize (%d) !=sizeof (*pPortFormat)(%d):[%s(%d)]\n",pNvComp->pComponentName,
                  pPortFormat->nSize,sizeof (*pPortFormat),__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }

        //default implementation is to support only one format.  Component must override this to support more then one
        if (pPortFormat->nIndex > 0 || pPort->oPortDef.eDomain != OMX_PortDomainAudio)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                  "Port->nIndex(%d) > 0 || PortDef.eDomain (%d)!= OMX_PortDomainAudio:[%s(%d)]\n",
                  pNvComp->pComponentName,pPortFormat->nIndex,pPort->oPortDef.eDomain,__FILE__, __LINE__));
            return OMX_ErrorNoMore;
        }

            pPortFormat->eEncoding = pPort->oPortDef.format.audio.eEncoding;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamVideoPortFormat:{
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = pComponentParameterStructure;

        //* error checking
        if (pPortFormat->nPortIndex  >= pNvComp->nPorts)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "nPortIndex (%d) > nPorts (%d):[%s(%d)]\n",pNvComp->pComponentName,
                 pPortFormat->nPortIndex,pNvComp->nPorts,__FILE__, __LINE__));
            return OMX_ErrorBadPortIndex;   // incorrect port number
        }
        pPort = &pNvComp->pPorts[pPortFormat->nPortIndex];

        if (CheckOMXVersion(pPortFormat->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                   "nVersion (%d) != oPortDef.nVersion (%d):[%s(%d)]\n",pNvComp->pComponentName
                   ,pPortFormat->nVersion.nVersion , pPort->oPortDef.nVersion.nVersion,__FILE__, __LINE__));
            return OMX_ErrorVersionMismatch; // incompatbile version numbers
        }

        if (pPortFormat->nSize != sizeof (*pPortFormat))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "nSize (%d) != sizeof (*pPortFormat)(%d):[%s(%d)]\n",pNvComp->pComponentName
                 ,pPortFormat->nSize,sizeof (*pPortFormat),__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }
        // default implementation is to support only one format.  Component must override this to support more then one
        if (pPortFormat->nIndex > 0 || pPort->oPortDef.eDomain != OMX_PortDomainVideo)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s: nIndex"
                  "(%d)> 0 || eDomain (%d)!= OMX_PortDomainVideo:[%s(%d)]\n",pNvComp->pComponentName
                  ,pPortFormat->nIndex,pPort->oPortDef.eDomain ,__FILE__, __LINE__));
            return OMX_ErrorNoMore;
        }

        pPortFormat->eColorFormat = pPort->oPortDef.format.video.eColorFormat;
        pPortFormat->eCompressionFormat = pPort->oPortDef.format.video.eCompressionFormat;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamImagePortFormat:{
        OMX_IMAGE_PARAM_PORTFORMATTYPE *pPortFormat = pComponentParameterStructure;

        // error checking
        if (pPortFormat->nPortIndex  >= pNvComp->nPorts)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "nPortIndex  (%d) >= nPorts (%d):[%s(%d)]\n",pNvComp->pComponentName,
                 pPortFormat->nPortIndex,pNvComp->nPorts,__FILE__, __LINE__));
            return OMX_ErrorBadParameter;    // incorrect port number
        }
        pPort = &pNvComp->pPorts[pPortFormat->nPortIndex];

        if (CheckOMXVersion(pPortFormat->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "nVersion (%d)!= oPortDef.nVersion(%d):[%s(%d)]\n",pNvComp->pComponentName,
                 pPortFormat->nVersion.nVersion,pPort->oPortDef.nVersion.nVersion,__FILE__, __LINE__));
            return OMX_ErrorVersionMismatch; // incompatbile version numbers
        }

        if (pPortFormat->nSize != sizeof (*pPortFormat))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "nSize (%%d) != sizeof (*pPortFormat) (%d):[%s(%d)]\n",pNvComp->pComponentName,
                 pPortFormat->nSize , sizeof (*pPortFormat),__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }

        // default implementation is to support only one format.  Component must override this to support more then one
        if (pPortFormat->nIndex > 0 || pPort->oPortDef.eDomain != OMX_PortDomainImage)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                "nIndex(%d) > 0 || eDomain(%d)!= OMX_PortDomainImage:[%s(%d)]\n",
                pNvComp->pComponentName,pPortFormat->nIndex,pPort->oPortDef.eDomain,__FILE__, __LINE__));
            return OMX_ErrorNoMore;
        }

        pPortFormat->eColorFormat = pPort->oPortDef.format.image.eColorFormat;
        pPortFormat->eCompressionFormat = pPort->oPortDef.format.image.eCompressionFormat;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamOtherPortFormat:{
        OMX_OTHER_PARAM_PORTFORMATTYPE *pPortFormat = pComponentParameterStructure;

        // error checking
        if (pPortFormat->nPortIndex  >= pNvComp->nPorts)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "nPortIndex (%d) >= nPorts (%d) :[%s(%d)]\n",pNvComp->pComponentName,
                 pPortFormat->nIndex,pNvComp->nPorts,__FILE__, __LINE__));
            return OMX_ErrorBadPortIndex;    // incorrect port number
        }
        pPort = &pNvComp->pPorts[pPortFormat->nPortIndex];

        if (CheckOMXVersion(pPortFormat->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                 "nVersion != oPortDef.nVersion :[%s(%d)]\n", pNvComp->pComponentName,
                 pPortFormat->nVersion.nVersion, pPort->oPortDef.nVersion.nVersion,__FILE__, __LINE__));
            return OMX_ErrorVersionMismatch; // incompatbile version numbers
        }

        if (pPortFormat->nSize != sizeof (*pPortFormat))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s: "
                 "nSize (%d) != sizeof (*pPortFormat)(%d) :[%s(%d)]\n",pNvComp->pComponentName,
                 pPortFormat->nSize,sizeof (*pPortFormat),__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }

        // default implementation is to support only one format.  Component must override this to support more then one
        if (pPortFormat->nIndex > 0 || pPort->oPortDef.eDomain != OMX_PortDomainOther)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                  "nIndex(%d) > 0 ||oPortDef.eDomain (%d)!= OMX_PortDomainOther :[%s(%d)]\n",
                  pNvComp->pComponentName,pPortFormat->nIndex,pPort->oPortDef.eDomain,__FILE__, __LINE__));
            return OMX_ErrorNoMore;
        }

        pPortFormat->eFormat = pPort->oPortDef.format.other.eFormat;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamCompBufferSupplier: {
        OMX_PARAM_BUFFERSUPPLIERTYPE *pSupplier = pComponentParameterStructure;
        if (pSupplier->nSize != sizeof (*pSupplier))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                  "nSize(%d)!= sizeof (*pSupplier)(%d) :[%s(%d)]\n",pNvComp->pComponentName,
                  pSupplier->nSize,sizeof (*pSupplier),__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }
        nPort = pSupplier->nPortIndex;
        if (nPort >= pNvComp->nPorts)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                  "nPort(%d) >= nPorts(%d) :[%s(%d)]\n", pNvComp->pComponentName,nPort,
                  pNvComp->nPorts,__FILE__, __LINE__));
            return OMX_ErrorBadPortIndex;    // incorrect port number
        }
        pPort = &pNvComp->pPorts[nPort];
        if (CheckOMXVersion(pSupplier->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                  "nVersion (%d)!= oPortDef.nVersion :[%s(%d)]\n",pNvComp->pComponentName,
                  pSupplier->nVersion.nVersion,pNvComp->pPorts[nPort].oPortDef.nVersion.nVersion
                  ,__FILE__, __LINE__));
            return OMX_ErrorVersionMismatch; // incompatbile version numbers
        }

        if (pPort->hTunnelComponent){
            pSupplier->eBufferSupplier = pPort->eSupplierSetting;
        } else {
            pSupplier->eBufferSupplier = pPort->eSupplierPreference;
        }
        return OMX_ErrorNone;
    }
    case OMX_IndexParamPriorityMgmt: {
        OMX_PRIORITYMGMTTYPE *pPriorityState = pComponentParameterStructure;
        if (pPriorityState->nSize != sizeof (*pPriorityState))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                   "pPriorityState->nSize(%d) != sizeof (*pPriorityState)(%d) :[%s(%d)]\n",
                   pNvComp->pComponentName,pPriorityState->nSize ,sizeof (*pPriorityState),
                   __FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }

        if (CheckOMXVersion(pPriorityState->nVersion, pNvComp->oSpecVersion) !=
            OMX_ErrorNone)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                    "nVersion (%d)!= oPortDef.nVersion(%d) :[%s(%d)]\n",pNvComp->pComponentName,
                    pPriorityState->nVersion.nVersion , pNvComp->oSpecVersion.nVersion,__FILE__, __LINE__));
            return OMX_ErrorVersionMismatch; // incompatbile version numbers
        }
        *pPriorityState = pNvComp->oPriorityState;
        return OMX_ErrorNone;
    }
    case NVX_IndexParamVendorSpecificTunneling:
        {
            NvxParamNvidiaTunnel *pTunnelParam = (NvxParamNvidiaTunnel *)pComponentParameterStructure;
            OMX_U32 uPort = pTunnelParam->nPort;
            if(uPort >= pNvComp->nPorts)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
                     "uPort(%d) >=  pNvComp->nPorts (%d):[%s(%d)]\n",pNvComp->pComponentName,
                     uPort, pNvComp->nPorts,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            pTunnelParam->bNvidiaTunnel = pNvComp->pPorts[uPort].bNvidiaTunneling;
            pTunnelParam->eTransactionType = pNvComp->pPorts[uPort].eNvidiaTunnelTransaction;
        }
        return OMX_ErrorNone;
    case OMX_IndexParamStandardComponentRole:{
        OMX_PARAM_COMPONENTROLETYPE *pRole = pComponentParameterStructure;

        if (!pNvComp->sCurrentComponentRole && pNvComp->nComponentRoles > 0)
            pNvComp->sCurrentComponentRole = pNvComp->sComponentRoles[0];

        if (pNvComp->sCurrentComponentRole)
        {
            NvOsStrncpy((char*)(pRole->cRole), pNvComp->sCurrentComponentRole,
                        OMX_MAX_STRINGNAME_SIZE);
            return OMX_ErrorNone;
        }

        return OMX_ErrorUnsupportedSetting;
    }
    case PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX:
    {
        PV_OMXComponentCapabilityFlagsType *pCap = pComponentParameterStructure;

        pCap->iIsOMXComponentMultiThreaded = OMX_TRUE;
        pCap->iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
        pCap->iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
        pCap->iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;
        pCap->iOMXComponentSupportsPartialFrames = pNvComp->bCanUsePartialFrame;
        pCap->iOMXComponentUsesNALStartCodes = OMX_TRUE;
        pCap->iOMXComponentCanHandleIncompleteFrames = OMX_TRUE; // ???
        pCap->iOMXComponentUsesFullAVCFrames = OMX_TRUE; // ???
        return OMX_ErrorNone;
    }
    case OMX_IndexParamMediaContainer:
    {
        return OMX_ErrorNone;
    }
    default:
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetParameter:%s:"
            "nParamIndex = %d :[%s(%d)]\n", pNvComp->pComponentName,nParamIndex,__FILE__, __LINE__));
        return OMX_ErrorNotImplemented;
    }
}

OMX_ERRORTYPE NvxComponentBaseSetParameter(
    OMX_IN NvxComponent* pNvComp,
    OMX_IN OMX_INDEXTYPE nParamIndex,
    OMX_IN OMX_PTR pComponentParameterStructure)
{
    OMX_U32 nPort;
    NvxPort *pPort;
    OMX_PARAM_BUFFERSUPPLIERTYPE oTunnelBufferSupply;

    if(!pComponentParameterStructure)
    {
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:%s:"
             "pComponentParameterStructure = %d :[%s(%d)]\n",pNvComp->pComponentName,
             pComponentParameterStructure,__FILE__, __LINE__));
        return OMX_ErrorBadParameter;
    }

    switch(nParamIndex)
    {
    case OMX_IndexParamCompBufferSupplier: {
        OMX_PARAM_BUFFERSUPPLIERTYPE *pSupplier = pComponentParameterStructure;
        if (pSupplier->nSize != sizeof (*pSupplier))
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseSetParameter:%s:"
                  "pSupplier->nSize(%d) != sizeof (*pSupplier)( %d) :[%s(%d)]\n",
                  pNvComp->pComponentName, pSupplier->nSize ,sizeof (*pSupplier),__FILE__, __LINE__));
            return OMX_ErrorBadParameter;
        }
        nPort = pSupplier->nPortIndex;
        if (nPort >= pNvComp->nPorts)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseSetParameter:%s:"
                   "nPort (%d) >= pNvComp->nPorts (%d) :[%s(%d)]\n",pNvComp->pComponentName,nPort,
                   pNvComp->nPorts,__FILE__, __LINE__));
            return OMX_ErrorBadPortIndex;    /* incorrect port number */
        }
        pPort = &pNvComp->pPorts[nPort];
        if (CheckOMXVersion(pSupplier->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
        {
            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseSetParameter:%s:"
                    "nVersion(%d)!= oPortDef.nVersion(%d):[%s(%d)]\n",pNvComp->pComponentName,
                    pSupplier->nVersion.nVersion,pNvComp->pPorts[nPort].oPortDef.nVersion.nVersion,__FILE__, __LINE__));
            return OMX_ErrorVersionMismatch; /* incompatbile version numbers */
        }

        if (pPort->hTunnelComponent){
            pPort->eSupplierSetting = pSupplier->eBufferSupplier;
            /* if we're an input port set the output port */
            if (pPort->oPortDef.eDir == OMX_DirInput){
                NvOsMemcpy(&oTunnelBufferSupply, pComponentParameterStructure, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
                oTunnelBufferSupply.nPortIndex = pPort->nTunnelPort;
                return OMX_SetParameter(pPort->hTunnelComponent, OMX_IndexParamCompBufferSupplier, &oTunnelBufferSupply);
            }
        } else {
            pPort->eSupplierPreference = pSupplier->eBufferSupplier;
        }
        return OMX_ErrorNone;
    }
    case OMX_IndexParamPriorityMgmt: {
        OMX_PRIORITYMGMTTYPE *pPriorityState = pComponentParameterStructure;
        if (pPriorityState->nSize != sizeof (*pPriorityState))
            return OMX_ErrorBadParameter;
        if (CheckOMXVersion(pPriorityState->nVersion, pNvComp->oSpecVersion) !=
            OMX_ErrorNone)
            return OMX_ErrorVersionMismatch; /* incompatbile version numbers */
        pNvComp->oPriorityState = *pPriorityState;
        NvxWorkerTrigger(&(pNvComp->oWorkerData));
        return OMX_ErrorNone;
    }
    case OMX_IndexParamPortDefinition:{
        OMX_U32 nPort;
        OMX_PARAM_PORTDEFINITIONTYPE* pPortDef = pComponentParameterStructure;

        /* error check */
        nPort = pPortDef->nPortIndex;
        if (nPort >= pNvComp->nPorts)
            return OMX_ErrorBadPortIndex;    /* incorrect port number */
        pPort = &pNvComp->pPorts[nPort];

        if (CheckOMXVersion(pPortDef->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
        {
            return OMX_ErrorVersionMismatch; /* incompatbile version numbers */
        }

        if (pPortDef->nSize != pPort->oPortDef.nSize)
            return OMX_ErrorBadParameter;

        /* default implementation is to support only one format.  Component must override this to support more then one */
        if (pPortDef->eDomain != pPort->oPortDef.eDomain)
            return OMX_ErrorUnsupportedSetting;

        switch (pPort->oPortDef.eDomain) {
        case OMX_PortDomainAudio:
            NvOsMemcpy(&pPort->oPortDef.format.audio, &pPortDef->format.audio, sizeof(pPortDef->format.audio));
            break;
        case OMX_PortDomainVideo:
            NvOsMemcpy(&pPort->oPortDef.format.video, &pPortDef->format.video, sizeof(pPortDef->format.video));
            break;
        case OMX_PortDomainImage:
            NvOsMemcpy(&pPort->oPortDef.format.image, &pPortDef->format.image, sizeof(pPortDef->format.image));
            break;
        case OMX_PortDomainOther:
            NvOsMemcpy(&pPort->oPortDef.format.other, &pPortDef->format.other, sizeof(pPortDef->format.other));
            break;
        default:
            return OMX_ErrorBadParameter;
        }

        /* validate count and size are in allowable range */
        if (pPortDef->nBufferCountMin != pPort->nMinBufferCount ||
            pPortDef->nBufferCountActual < pPort->nMinBufferCount ||
            pPortDef->nBufferCountActual > pPort->nMaxBufferCount ||
            pPortDef->nBufferSize < pPort->nMinBufferSize ||
            pPortDef->nBufferSize > pPort->nMaxBufferSize
            )
            return OMX_ErrorUnsupportedSetting;

        pPort->nReqBufferCount = pPort->oPortDef.nBufferCountActual = pPortDef->nBufferCountActual;
        pPort->nReqBufferSize = pPort->oPortDef.nBufferSize = pPortDef->nBufferSize;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamAudioPortFormat:{
        OMX_AUDIO_PARAM_PORTFORMATTYPE *pPortFormat = pComponentParameterStructure;

        /* error checking */
        if (pPortFormat->nPortIndex  >= pNvComp->nPorts)
            return OMX_ErrorBadPortIndex;    /* incorrect port number */
        pPort = &pNvComp->pPorts[pPortFormat->nPortIndex];

        if (CheckOMXVersion(pPortFormat->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
            return OMX_ErrorVersionMismatch; /* incompatbile version numbers */
        if (pPortFormat->nSize != sizeof (*pPortFormat))
            return OMX_ErrorUnsupportedIndex;

        /* default implementation is to support only one format.  Component must override this to support more then one */
        if (pPort->oPortDef.eDomain != OMX_PortDomainAudio)
            return OMX_ErrorUnsupportedIndex; /* index does not match current port definition */

        if (pPortFormat->eEncoding != pPort->oPortDef.format.audio.eEncoding)
            return OMX_ErrorUnsupportedSetting;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamVideoPortFormat:{
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat = pComponentParameterStructure;

        /* error checking */
        if (pPortFormat->nPortIndex  >= pNvComp->nPorts)
            return OMX_ErrorBadPortIndex;    /* incorrect port number */
        pPort = &pNvComp->pPorts[pPortFormat->nPortIndex];

        if (CheckOMXVersion(pPortFormat->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
            return OMX_ErrorVersionMismatch; /* incompatbile version numbers */
        if (pPortFormat->nSize != sizeof (*pPortFormat))
            return OMX_ErrorUnsupportedIndex;

        /* default implementation is to support only one format.  Component must override this to support more then one */
        if (pPort->oPortDef.eDomain != OMX_PortDomainVideo)
            return OMX_ErrorUnsupportedIndex; /* index does not match current port definition */

        if (pPortFormat->eCompressionFormat != pPort->oPortDef.format.video.eCompressionFormat
            &&  pPortFormat->eColorFormat != pPort->oPortDef.format.video.eColorFormat)
            return OMX_ErrorUnsupportedSetting;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamImagePortFormat:{
        OMX_IMAGE_PARAM_PORTFORMATTYPE *pPortFormat = pComponentParameterStructure;

        /* error checking */
        if (pPortFormat->nPortIndex  >= pNvComp->nPorts)
            return OMX_ErrorBadPortIndex;    /* incorrect port number */
        pPort = &pNvComp->pPorts[pPortFormat->nPortIndex];

        if (CheckOMXVersion(pPortFormat->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
            return OMX_ErrorVersionMismatch; /* incompatbile version numbers */
        if (pPortFormat->nSize != sizeof (*pPortFormat))
            return OMX_ErrorBadParameter;

        /* default implementation is to support only one format.  Component must override this to support more then one */
        if (pPort->oPortDef.eDomain != OMX_PortDomainImage)
            return OMX_ErrorUnsupportedIndex; /* index does not match current port definition */

        if (pPortFormat->eCompressionFormat != pPort->oPortDef.format.video.eCompressionFormat
            &&  pPortFormat->eColorFormat != pPort->oPortDef.format.video.eColorFormat)
            return OMX_ErrorUnsupportedSetting;
        return OMX_ErrorNone;
    }
    case OMX_IndexParamOtherPortFormat:{
        OMX_OTHER_PARAM_PORTFORMATTYPE *pPortFormat = pComponentParameterStructure;

        /* error checking */
        if (pPortFormat->nPortIndex  >= pNvComp->nPorts)
            return OMX_ErrorBadPortIndex;    /* incorrect port number */
        pPort = &pNvComp->pPorts[pPortFormat->nPortIndex];

        if (CheckOMXVersion(pPortFormat->nVersion, pPort->oPortDef.nVersion) !=
            OMX_ErrorNone)
            return OMX_ErrorVersionMismatch; /* incompatbile version numbers */
        if (pPortFormat->nSize != sizeof (*pPortFormat))
            return OMX_ErrorBadParameter;

        if (pPort->oPortDef.eDomain != OMX_PortDomainOther)
            return OMX_ErrorUnsupportedIndex; /* index does not match current port definition */

        if (pPortFormat->eFormat != pPort->oPortDef.format.other.eFormat)
            return OMX_ErrorUnsupportedSetting;

        return OMX_ErrorNone;
    }
    case OMX_IndexParamStandardComponentRole:{
        OMX_PARAM_COMPONENTROLETYPE *pRole = pComponentParameterStructure;
        OMX_U32 i;

        for (i = 0; i < pNvComp->nComponentRoles; i++)
        {
            if (!strcmp(pNvComp->sComponentRoles[i], (char*)(pRole->cRole)))
            {
                pNvComp->sCurrentComponentRole = pNvComp->sComponentRoles[i];
                return OMX_ErrorNone;
            }
        }

        return OMX_ErrorUnsupportedSetting;
    }
    case NVX_IndexParamVendorSpecificTunneling:
        {
            NvxParamNvidiaTunnel *pTunnelParam = (NvxParamNvidiaTunnel *)pComponentParameterStructure;
            OMX_U32 uPort = pTunnelParam->nPort;
            if(uPort >= pNvComp->nPorts)
            {
                NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseSetParameter:%s:"
                     "uPort(%d) >=  pNvComp->nPorts (%d):[%s(%d)]\n",pNvComp->pComponentName,
                     uPort, pNvComp->nPorts,__FILE__, __LINE__));
                return OMX_ErrorBadParameter;
            }
            pNvComp->pPorts[uPort].bNvidiaTunneling = pTunnelParam->bNvidiaTunnel;
            pNvComp->pPorts[uPort].eNvidiaTunnelTransaction = pTunnelParam->eTransactionType;
        }
        return OMX_ErrorNone;
    case OMX_IndexParamMediaContainer:
    {
        return OMX_ErrorNone;
    }
    default:
        NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseSetParameter:%s:"
            "nParamIndex = %d :[%s(%d)]\n", pNvComp->pComponentName,nParamIndex,__FILE__, __LINE__));
        return OMX_ErrorNotImplemented;
    }

}

struct NvxExtEntry {
    const char *name;
    NVX_INDEXTYPE eExtensionIndex;
};

struct NvxExtEntry NvxExtensionIndexTable[] = {
    { NVX_INDEX_PARAM_FILENAME, NVX_IndexParamFilename},
    { NVX_OTHER_FORMAT_BYTESTREAM, NVX_OTHER_FormatByteStream},
    { NVX_INDEX_CONFIG_TIMESTAMPOVERRIDE, NVX_IndexConfigTimestampOverride},
    { NVX_INDEX_CONFIG_FRAMESTATTYPE, NVX_IndexConfigGatherFrameStats},
    { NVX_INDEX_CONFIG_CAMERAIMAGER, NVX_IndexConfigCameraImager},
    { NVX_INDEX_CONFIG_VIDEO_CONFIGINFO, NVX_IndexConfigVideoConfigInfo},
    { NVX_INDEX_CONFIG_VIDEO_CAPABILITIES, NVX_IndexConfigVideoCapabilities},
    { NVX_INDEX_PARAM_COMMON_CAP_PORT_RESOLUTION, NVX_IndexParamCommonCapPortResolution},
    { NVX_INDEX_CONFIG_PROFILE, NVX_IndexConfigProfile },
    { NVX_INDEX_PARAM_SENSORNAME, NVX_IndexParamSensorName },
    { NVX_INDEX_CONFIG_PREVIEWENABLE, NVX_IndexConfigPreviewEnable },
    { NVX_INDEX_CONFIG_ALGORITHMWARMUP, NVX_IndexConfigAlgorithmWarmUp },
    { NVX_INDEX_CONFIG_AUTOFRAMERATE, NVX_IndexConfigAutoFrameRate },
    { NVX_INDEX_CONFIG_EXPOSURETIMERANGE, NVX_IndexConfigExposureTimeRange },
    { NVX_INDEX_CONFIG_AEOVERRIDE, NVX_IndexConfigAEOverride },
    { NVX_INDEX_CONFIG_SENSORETRANGE, NVX_IndexConfigSensorETRange },
    { NVX_INDEX_CONFIG_FUSEID, NVX_IndexConfigFuseId },
    { NVX_INDEX_CONFIG_ISOSENSITIVITYRANGE, NVX_IndexConfigISOSensitivityRange },
    { NVX_INDEX_CONFIG_CCTWHITEBALANCERANGE, NVX_IndexConfigWhitebalanceCCTRange },
    { NVX_INDEX_CONFIG_FOCUSERPARAMETERS, NVX_IndexConfigFocuserParameters },
    { NVX_INDEX_CONFIG_FLASHPARAMETERS, NVX_IndexConfigFlashParameters },
    { NVX_INDEX_CONFIG_AUDIO_OUTPUT, NVX_IndexConfigAudioOutputType },
    { NVX_INDEX_PARAM_VIDEO_ENCODE_PROPERTY, NVX_IndexParamVideoEncodeProperty },
    { NVX_INDEX_PARAM_VIDEO_ENCODE_STRINGENTBITRATE, NVX_IndexParamVideoEncodeStringentBitrate },
    { NVX_INDEX_CONFIG_THUMBNAIL, NVX_IndexConfigThumbnail },
    { NVX_INDEX_PARAM_DURATION, NVX_IndexParamDuration },
    { NVX_INDEX_PARAM_STREAMTYPE, NVX_IndexParamStreamType },
    { NVX_INDEX_PARAM_LOWMEMMODE, NVX_IndexParamLowMemMode },
    { NVX_INDEX_PARAM_RECORDINGMODE, NVX_IndexParamRecordingMode},
    { NVX_INDEX_PARAM_CHANNELID, NVX_IndexParamChannelID},
    { NVX_INDEX_CONFIG_PAUSEPLAYBACK, NVX_IndexConfigPausePlayback},
    { NVX_INDEX_PARAM_SOURCE, NVX_IndexParamSource},
    { NVX_INDEX_CONFIG_ULPMODE, NVX_IndexConfigUlpMode },
    { NVX_INDEX_CONFIG_CONVERGEANDLOCK, NVX_IndexConfigConvergeAndLock },
    { NVX_INDEX_CONFIG_QUERYMETADATA, NVX_IndexConfigQueryMetadata },
    { NVX_INDEX_CONFIG_HEADER, NVX_IndexConfigHeader},
    { NVX_INDEX_CONFIG_MP3TSENABLE,NVX_IndexConfigMp3Enable},
    { NVX_INDEX_PARAM_OTHER_3GPMUX_BUFFERCONFIG, NVX_IndexParam3GPMuxBufferConfig },
    { NVX_INDEX_CONFIG_CAMERARAWCAPTURE, NVX_IndexConfigCameraRawCapture },
    { NVX_INDEX_CONFIG_CONCURRENTRAWDUMPFLAG, NVX_IndexConfigConcurrentRawDumpFlag },
    { NVX_INDEX_CONFIG_ISPDATA, NVX_IndexConfigIspData },
    { NVX_INDEX_CONFIG_CAPTUREPAUSE, NVX_IndexConfigCapturePause },
    { NVX_INDEX_CONFIG_FLICKER, NVX_IndexConfigFlicker },
    { NVX_INDEX_CONFIG_PRECAPTURECONVERGE, NVX_IndexConfigPreCaptureConverge },
    { NVX_INDEX_CONFIG_KEEPASPECT, NVX_IndexConfigKeepAspect },
    { NVX_INDEX_CONFIG_TRACKLIST, NvxIndexConfigTracklist },
    { NVX_INDEX_CONFIG_TRACKLIST_TRACK, NvxIndexConfigTracklistTrack },
    { NVX_INDEX_CONFIG_TRACKLIST_DELETE, NvxIndexConfigTracklistDelete },
    { NVX_INDEX_CONFIG_TRACKLIST_CURRENT, NvxIndexConfigTracklistCurrent },
    { NVX_INDEX_CONFIG_TRACKLIST_SIZE, NvxIndexConfigTracklistSize },
    { NVX_INDEX_CONFIG_WINDOW_DISP_OVERRIDE, NvxIndexConfigWindowDispOverride },
    { NVX_INDEX_CONFIG_FOCUSREGIONSRECT, NVX_IndexConfigFocusRegionsRect },
    { NVX_INDEX_CONFIG_CAMERATESTPATTERN, NVX_IndexConfigCameraTestPattern},
    { NVX_INDEX_CONFIG_BURSTSKIPCOUNT, NVX_IndexConfigBurstSkipCount},
    { NVX_INDEX_CONFIG_DZSCALEFACTORMULTIPLIER, NVX_IndexConfigDZScaleFactorMultiplier},
    { NVX_INDEX_CONFIG_3GPMUXGETAVRECFRAMES, NVX_IndexConfig3GPMuxGetAVRecFrames},
    { NVX_INDEX_CONFIG_CAPTURERAWFRAME, NVX_IndexConfigCaptureRawFrame},
    { NVX_INDEX_CONFIG_HUE, NVX_IndexConfigHue},
    { NVX_INDEX_PARAM_SENSORID, NVX_IndexParamSensorId},
    { NVX_INDEX_PARAM_AVAILABLESENSORS, NVX_IndexParamAvailableSensors },
    { NVX_INDEX_CONFIG_CUSTOMCOLORFORMAT, NVX_IndexConfigCustomColorFormt},
    { NVX_INDEX_CONFIG_SINGLE_FRAME, NvxIndexConfigSingleFrame},
    { NVX_INDEX_CONFIG_EDGEENHANCEMENT, NVX_IndexConfigEdgeEnhancement},
    { NVX_INDEX_CONFIG_PREVIEW_FRAME_COPY, NVX_IndexConfigPreviewFrameCopy},
    { NVX_INDEX_CONFIG_STILL_CONFIRMATION_FRAME_COPY, NVX_IndexConfigStillConfirmationFrameCopy},
    { NVX_INDEX_CONFIG_STILL_YUV_FRAME_COPY, NVX_IndexConfigStillFrameCopy},
    { NVX_INDEX_CONFIG_STEREOCAPABLE, NVX_IndexConfigStereoCapable},
    { NVX_INDEX_CONFIG_SCENEBRIGHTNESS, NVX_IndexConfigSceneBrightness},
    { NVX_INDEX_PARAM_STEREOCAMERAMODE, NVX_IndexParamStereoCameraMode},
    { NVX_INDEX_PARAM_STEREOCAPTUREINFO, NVX_IndexParamStereoCaptureInfo},
    { NVX_INDEX_CONFIG_DISABLETIMESTAMPUPDATES, NVX_IndexConfigDisableTimestampUpdates },
    { NVX_INDEX_CONFIG_AUDIOONLYHINT, NVX_IndexConfigAudioOnlyHint },
    { NVX_INDEX_CONFIG_FILECACHESIZE, NVX_IndexConfigFileCacheSize },
    { NVX_INDEX_CONFIG_DISABLEBUFFCONFIG, NVX_IndexConfigDisableBuffConfig},
    { NVX_INDEX_CONFIG_PROGRAMS_LIST, NVX_IndexConfigProgramsList },
    { NVX_INDEX_CONFIG_CURRENT_PROGRAM, NVX_IndexConfigCurrentProgram },
    { NVX_INDEX_CONFIG_AUDIO_DRCPROPERTY, NVX_IndexConfigAudioDrcProperty },
    { NVX_INDEX_CONFIG_FOCUSREGIONSSHARPNESS, NVX_IndexConfigFocusRegionsSharpness},
    { NVX_INDEX_CONFIG_FOCUSERCAPABILITIES, NVX_IndexConfigFocuserCapabilities},
    { NVX_INDEX_CONFIG_FOCUSER_INFO, NVX_IndexConfigFocuserInfo},
    { NVX_INDEX_CONFIG_AUTOFOCUS_SPEED, NVX_IndexConfigAutofocusSpeed},
    { NVX_INDEX_CONFIG_VIDEO_ENCODE_TEMPORALTRADEOFF, NVX_IndexConfigTemporalTradeOff },
    { NVX_INDEX_CONFIG_VIDEO_ENCODE_DCI, NVX_IndexConfigDecoderConfigInfo },
    { NVX_INDEX_PARAM_MAXFRAMES, NVX_IndexParamMaxFrames },
    { NVX_INDEX_PARAM_AUDIOPARAMS, NVX_IndexParamAudioParams },
    { NVX_INDEX_CONFIG_EXIFINFO, NVX_IndexConfigExifInfo },
    { NVX_INDEX_CONFIG_AUDIO_PARAEQFILTER, NVX_IndexConfigAudioParaEqFilter },
    { NVX_INDEX_CONFIG_SMOOTHZOOMTIME, NVX_IndexConfigSmoothZoomTime },
    { NVX_INDEX_CONFIG_ZOOMABORT, NVX_IndexConfigZoomAbort },
    { NVX_INDEX_CONFIG_AUDIO_GRAPHICEQ, NVX_IndexConfigAudioGraphicEq },
    { NVX_INDEX_CONFIG_SENSORMODELIST, NVX_IndexConfigSensorModeList },
    { NVX_INDEX_CONFIG_CAPTUREMODE,NVX_IndexConfigCaptureMode},
    { NVX_INDEX_PARAM_PREVIEWMODE,NVX_IndexParamPreviewMode},
    { NVX_INDEX_CONFIG_FILECACHEINFO, NVX_IndexConfigFileCacheInfo },
    { NVX_INDEX_CONFIG_NUMRENDEREDFRAMES, NVX_IndexConfigNumRenderedFrames},
    { NVX_INDEX_CONFIG_RENDERHINTMIXEDFRAMES, NVX_IndexConfigRenderHintMixedFrames},
    { NVX_INDEX_PARAM_SYNCDECODE, NVX_IndexParamSyncDecodeMode },
    { NVX_INDEX_PARAM_DEINTERLACING, NVX_IndexParamDeinterlaceMethod},
    { NVX_INDEX_CONFIG_EXTERNALOVERLAY, NVX_IndexConfigExternalOverlay },
    { NVX_INDEX_CONFIG_SPLITFILENAME, NVX_IndexConfigSplitFilename },
    { NVX_INDEX_CONFIG_USENVBUFFER, NVX_IndexConfigUseNvBuffer},
    { NVX_INDEX_PARAM_TEMPFILEPATH, NVX_IndexParamTempFilePath},
    { NVX_INDEX_CONFIG_ALLOCATERMSURF, NVX_IndexParamAllocateRmSurf},
    { NVX_INDEX_CONFIG_SMARTDIMMER, NVX_IndexConfigSmartDimmer},
    { NVX_INDEX_CONFIG_ENCODEEXIFINFO, NVX_IndexConfigEncodeExifInfo},
    { NVX_INDEX_CONFIG_ENCODEGPSINFO, NVX_IndexConfigEncodeGPSInfo},
    { NVX_INDEX_CONFIG_CAMERACONFIGURATION, NVX_IndexConfigCameraConfiguration},
    { NVX_INDEX_CONFIG_ENCODE_INTEROPINFO, NVX_IndexConfigEncodeInterOpInfo},
    { NVX_INDEX_CONFIG_CUSTOMIZEDBLOCKINFO, NVX_IndexConfigCustomizedBlockInfo},
    { NVX_INDEX_CONFIG_ADVANCED_NOISE_REDUCTION, NVX_IndexConfigAdvancedNoiseReduction},
    { NVX_INDEX_CONFIG_BAYERGAINS, NVX_IndexConfigBayerGains},
    { NVX_INDEX_CONFIG_GAINRANGE, NVX_IndexConfigGainRange},
    { NVX_INDEX_CONFIG_ANDROIDWINDOW, NVX_IndexConfigAndroidWindow},
    { NVX_INDEX_CONFIG_CAMERAPREVIEW, NVX_IndexConfigCameraPreview},
    { NVX_INDEX_PARAM_ENABLE_ANB, NVX_IndexParamEnableAndroidBuffers },
    { NVX_INDEX_PARAM_USE_ANB, NVX_IndexParamUseAndroidNativeBuffer },
    { NVX_INDEX_PARAM_USE_NBH, NVX_IndexParamUseNativeBufferHandle },
    { NVX_INDEX_PARAM_GET_ANB_USAGE, NVX_IndexParamGetAndroidNativeBufferUsage },
    { NVX_INDEX_CONFIG_VIRTUALDESKTOP, NVX_IndexConfigVirtualDesktop },
    { NVX_INDEX_PARAM_LOWRESOURCEMODE, NVX_IndexParamLowResourceMode },
    { NVX_INDEX_PARAM_RATECONTROLMODE, NVX_IndexParamRateControlMode},
    { NVX_INDEX_CONFIG_STEREORENDMODE, NVX_IndexConfigStereoRendMode },
    { NVX_INDEX_CONFIG_ENCODE_QUANTIZATIONTABLE, OMX_IndexParamQuantizationTable },
    { NVX_INDEX_CONFIG_VIDEOFRAME_CONVERSION, NVX_IndexConfigVideoFrameDataConversion },
    { NVX_INDEX_CONFIG_CHECKRESOURCES, NVX_IndexConfigCheckResources },
    { NVX_INDEX_PARAM_STREAMCOUNT, NVX_IndexParamStreamCount},
    { NVX_INDEX_PARAM_STORE_METADATA_BUFFER, NVX_IndexParamStoreMetaDataBuffer },
    { NVX_INDEX_CONFIG_FOCUSDISTANCES, NVX_IndexConfigFocusDistances },
    { NVX_INDEX_CONFIG_VIDEO_STEREOINFO, NVX_IndexConfigVideoStereoInfo },
    { NVX_INDEX_CONFIG_CAPTURE_PRIORITY, NVX_IndexConfigCapturePriority},
    { NVX_INDEX_PARAM_OUTPUTFORMAT, NVX_IndexParamOutputFormat },
    { NVX_INDEX_CONFIG_THUMBNAILQF, NVX_IndexConfigThumbnailQF},
    { NVX_INDEX_CONFIG_THUMBNAILENABLE, NVX_IndexConfigThumbnailEnable},
    { NVX_INDEX_CONFIG_MANUALTORCHAMOUNT, NVX_IndexConfigManualTorchAmount },
    { NVX_INDEX_CONFIG_MANUALFLASHAMOUNT, NVX_IndexConfigManualFlashAmount },
    { NVX_INDEX_CONFIG_AUTOFLASHAMOUNT,   NVX_IndexConfigAutoFlashAmount  },
    { NVX_INDEX_CONFIG_LENS_PHYSICAL_ATTR, NVX_IndexConfigLensPhysicalAttr},
    { NVX_INDEX_CONFIG_RENDERHINTCAMERAPREVIEW, NVX_IndexConfigRenderHintCameraPreview },
    { NVX_INDEX_CONFIG_VIDEO_ENCODE_QUANTIZATION_RANGE, NVX_IndexConfigVideoEncodeQuantizationRange },
    { NVX_INDEX_PARAM_WRITERFILESIZE,NVX_IndexParamWriterFileSize},
    { NVX_INDEX_PARAM_USERAGENT, NVX_IndexParamUserAgent },
    { NVX_INDEX_PARAM_SENSORMODE, NVX_IndexParamSensorMode},
    { NVX_INDEX_CONFIG_COMMONSCALETYPE, NVX_IndexConfigCommonScaleType},
    { NVX_INDEX_PARAM_USERAGENT_PROFILE, NVX_IndexParamUserAgentProf },
    {DRM_INDEX_CONFIG_LICENSE_CHALLENGE,NVX_IndexParamLicenseChallenge},
    {DRM_INDEX_CONFIG_LICENSE_RESPONSE,NVX_IndexParamLicenseResponse},
    {DRM_INDEX_CONFIG_DELETE_LICENSE,NVX_IndexParamDeleteLicense},
    {DRM_BUFFER_HEADER_EXTRADATA_ENCRYPTION_METADATA,NVX_IndexExtraDataEncryptionMetadata},
    {DRM_BUFFER_HEADER_EXTRADATA_INITIALIZATION_VECTOR,NVX_IndexExtraDataInitializationVector},
    {DRM_BUFFER_HEADER_EXTRADATA_ENCRYPTION_OFFSET,NVX_IndexExtraDataInitializationOffset},
    { NVX_INDEX_PARAM_PRESCALER_ENABLE_FOR_CAPTURE, NVX_IndexParamPreScalerEnableForCapture},
    { NVX_INDEX_PARAM_ENABLE_TIMELAPSE_VIEW, NVX_IndexParamTimeLapseView },
    { NVX_INDEX_PARAM_VIDENC_H264_QUALITY_PARAMS, NVX_IndexParamVideoEncodeH264QualityParams },
    { NVX_INDEX_PARAM_H264_DISABLE_DPB, NVX_IndexParamH264DisableDPB},
    { NVX_INDEX_PARAM_VIDENC_SKIP_FRAME, NVX_IndexParamSkipFrame},
    { NVX_INDEX_PARAM_VDEC_FULL_FRAME_INPUT_DATA, NVX_IndexParamVideoDecFullFrameData},
    {DRM_INDEX_CONFIG_SET_SURFACE,NVX_IndexConfigSetSurface},
    { NVX_INDEX_CONFIG_FRAME_COPY_COLOR_FORMAT, NVX_IndexConfigFrameCopyColorFormat },
    { NVX_INDEX_PARAM_USELOWBUFFER, NVX_IndexParamUseLowBuffer},
    { NVX_INDEX_CONFIG_EXPOSUREREGIONSRECT, NVX_IndexConfigExposureRegionsRect },
    { NVX_INDEX_PARAM_SENSOR_GUID, NVX_IndexParamSensorGuid},
    { NVX_INDEX_PARAM_EMBEDRMSURACE, NVX_IndexParamEmbedRmSurface },
    { NVX_INDEX_CONFIG_NSLBUFFERS, NVX_IndexConfigNSLBuffers },
    { NVX_INDEX_CONFIG_NSLSKIPCOUNT, NVX_IndexConfigNSLSkipCount },
    { NVX_INDEX_CONFIG_COMBINEDCAPTURE, NVX_IndexConfigCombinedCapture },
    { NVX_INDEX_CONFIG_BRACKETCAPTURE, NVX_IndexConfigBracketCapture },
    { NVX_INDEX_CONFIG_CONTINUOUSAUTOFOCUS, NVX_IndexConfigContinuousAutoFocus },
    { NVX_INDEX_CONFIG_CONTINUOUSAUTOFOCUS_PAUSE, NVX_IndexConfigContinuousAutoFocusPause },
    { NVX_INDEX_CONFIG_CONTINUOUSAUTOFOCUS_STATE, NVX_IndexConfigContinuousAutoFocusState },
    { NVX_INDEX_CONFIG_EXPOSURETIME, NVX_IndexConfigExposureTime },
    { NVX_INDEX_CONFIG_CILTHRESHOLD, NVX_IndexConfigCILThreshold },
    { NVX_INDEX_CONFIG_LOWLIGHTTHRESHOLD, NVX_IndexConfigLowLightThreshold },
    { NVX_INDEX_CONFIG_MACROMODETHRESHOLD, NVX_IndexConfigMacroModeThreshold },
    { NVX_INDEX_CONFIG_FOCUSMOVEMSG, NVX_IndexConfigFocusMoveMsg },
    { NVX_INDEX_CONFIG_FOCUSPOSITION, NVX_IndexConfigFocusPosition },
    { NVX_INDEX_CONFIG_COLORCORRECTIONMATRIX, NVX_IndexConfigColorCorrectionMatrix },
    { NVX_INDEX_CONFIG_SENSORISPSUPPORT, NVX_IndexConfigSensorIspSupport},
    { NVX_INDEX_CONFIG_VIDEOSPEED, NVX_IndexConfigCameraVideoSpeed},
    { NVX_INDEX_CONFIG_MAXOUTPUTCHANNELS, NVX_IndexConfigMaxOutputChannels},
    { NVX_INDEX_CONFIG_CUSTOMPOSTVIEW, NVX_IndexConfigCustomPostview},
    { NVX_INDEX_PARAM_EMBEDMMBUFFER, NVX_IndexParamEmbedMMBuffer },
    { NVX_INDEX_PARAM_ENCMAXCLOCK, NVX_IndexParamEncMaxClock },
    { NVX_INDEX_PARAM_ENC2DCOPY, NVX_IndexParamEnc2DCopy },
    { NVX_INDEX_CONFIG_REMAININGSTILLIMAGES, NVX_IndexConfigRemainingStillImages },
    { NVX_INDEX_CONFIG_FORCEASPECT, NVX_IndexConfigForceAspect },
    { NVX_INDEX_CONFIG_NOISE_REDUCTION_V1, NVX_IndexConfigNoiseReductionV1},
    { NVX_INDEX_CONFIG_NOISE_REDUCTION_V2, NVX_IndexConfigNoiseReductionV2},
    { NVX_INDEX_CONFIG_DUALMONO_OUPUTMODE, NVX_IndexConfigAudioDualMonoOuputMode},
    { NVX_INDEX_CONFIG_FDLIMIT, NVX_IndexConfigFDLimit },
    { NVX_INDEX_CONFIG_FDMAXLIMIT, NVX_IndexConfigFDMaxLimit },
    { NVX_INDEX_CONFIG_FDRESULT, NVX_IndexConfigFDResult },
    { NVX_INDEX_CONFIG_FDDEBUG, NVX_IndexConfigFDDebug },
    { NVX_INDEX_PARAM_CODECCAPABILITY, NVX_IndexParamCodecCapability },
    { NVX_INDEX_CONFIG_ARIBCONSTRAINTS, NVX_IndexConfigAribConstraints },
    { NVX_INDEX_PARAM_AUDIOCODECCAPABILITY, NVX_IndexParamAudioCodecCapability },
    { NVX_INDEX_CONFIG_VIDEO2DPROC, NVX_IndexConfigVideo2DProcessing },
    { NVX_INDEX_CONFIG_OVERLAYINDEX, NVX_IndexConfigOverlayIndex },
    { NVX_INDEX_CONFIG_ALLOWSECONDARYWINDOW, NVX_IndexConfigAllowSecondaryWindow },
    { NVX_INDEX_CONFIG_STILLPASSTHROUGH, NVX_IndexConfigStillPassthrough },
    { NVX_INDEX_CONFIG_CONCURRENTCAPTUREREQUESTS, NVX_IndexConfigConcurrentCaptureRequests},
    { NVX_INDEX_PARAM_SURFACE_LAYOUT, NVX_IndexParamSurfaceLayout },
    { NVX_INDEX_PARAM_THUMBNAILIMAGEQUANTTABLE, NVX_IndexParamThumbnailQTable },
    { NVX_INDEX_PARAM_AC3, NVX_IndexParamAudioAc3 },
    { NVX_INDEX_PARAM_DTS, NVX_IndexParamAudioDts },
    { NVX_INDEX_CONFIG_AUDIO_CAPS, NVX_IndexConfigAudioCaps },
    { NVX_INDEX_CONFIG_SILENCE_OUTPUT, NVX_IndexConfigAudioSilenceOutput },
    { NVX_INDEX_CONFIG_CAPTURESENSORMODEPREF, NVX_IndexConfigCaptureSensorModePref },
    { NVX_INDEX_CONFIG_AUDIO_SESSION_ID, NVX_IndexConfigAudioSessionId },
    { NVX_INDEX_CONFIG_DECODE_IFRAMES, NVX_IndexConfigVideoDecodeIFrames },
    { NVX_INDEX_CONFIG_SENSORPOWERON, NVX_IndexConfigSensorPowerOn},
    { NVX_INDEX_CONFIG_ALLOCATEREMAININGBUFFERS, NVX_IndexConfigAllocateRemainingBuffers}, // deprecated
    { NVX_INDEX_CONFIG_FASTBURSTEN, NVX_IndexConfigFastBurstEn},
    { NVX_INDEX_CONFIG_CAMERAMODE, NVX_IndexConfigCameraMode},
    { NVX_INDEX_CONFIG_FORCEPOSTVIEW, NVX_IndexConfigForcePostView},
    { NVX_INDEX_PARAM_RAWOUTPUT, NVX_IndexParamRawOutput },
    { NVX_INDEX_CONFIG_VIDEO_MVCINFO, NVX_IndexConfigMVCStitchInfo},
    { NVX_INDEX_CONFIG_DEVICEROTATE, NVX_IndexConfigDeviceRotate },
    { NVX_INDEX_PARAM_FILTER_TIMESTAMPS, NVX_IndexParamFilterTimestamps},
    { NVX_INDEX_CONFIG_WAIT_ON_FENCE, NVX_IndexConfigWaitOnFence },
    { NVX_INDEX_PARAM_VDEC_FULL_SLICE_INPUT_DATA, NVX_IndexParamVideoDecFullSliceData},
    { NVX_INDEX_PARAM_VIDEO_DISABLE_DVFS, NVX_IndexParamVideoDisableDvfs},
    { NVX_INDEX_CONFIG_VIDEO_PROTECT, NVX_IndexConfigVideoProtect},
    { NVX_INDEX_CONFIG_VIDEO_SLICELEVELENCODE, NVX_IndexConfigSliceLevelEncode},
    { NVX_INDEX_CONFIG_VIDEO_SUPER_FINE_QUALITY, NVX_IndexConfigVideoSuperFineQuality },
    { NVX_INDEX_PARAM_INSERT_SPSPPS_AT_IDR, NVX_IndexParamInsertSPSPPSAtIDR },
    { NVX_INDEX_CONFIG_THUMBNAIL_MODE, NVX_IndexConfigThumbnailMode},
    { NVX_INDEX_PARAM_VP8TYPE, NVX_IndexParamVideoVP8 },
    { NVX_INDEX_PARAM_VIDEO_MJOLNIR_STREAMING, NVX_IndexParamVideoMjolnirStreaming},
    { NVX_INDEX_CONFIG_VIDEO_ENCODE_LAST_FRAME_QP, NVX_IndexConfigVideoEncodeLastFrameQP },
    { NVX_INDEX_PARAM_VPP, NVX_IndexParamVideoPostProcessType },
    { NVX_INDEX_PARAM_SYNC_PT_IN_BUFFER, NVX_IndexParamUseSyncFdFromNativeBuffer },
    { NVX_INDEX_CONFIG_USENVBUFFER2, NVX_IndexConfigUseNvBuffer2},
    { NVX_INDEX_CONFIG_VIDEO_DECODESTATE, NVX_IndexConfigVideoDecodeState },
    { NULL, 0}    /* end */
};

OMX_ERRORTYPE NvxComponentBaseGetExtensionIndex(OMX_IN NvxComponent* pNvComp, OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE* pIndexType)
{
    // BKC says: Oh my goodness!  This is not efficient.  The list should
    // be sorted at compile time and then searched by a bsearch, or use a
    // hash table, or something.
    struct NvxExtEntry* pEntry;
    for (pEntry = NvxExtensionIndexTable; pEntry->name != NULL; pEntry++)
        if (strcmp(pEntry->name, cParameterName) == 0) {
            *pIndexType = pEntry->eExtensionIndex;
             NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentBaseGetExtensionIndex:%s\n",
                    pNvComp->pComponentName));
             return OMX_ErrorNone;
        }
     NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseGetExtensionIndex:%s:entry(%d)"
            "not present in ExtensionIndexTable:[%s(%d)]\n", __FILE__,__LINE__,pNvComp->pComponentName,cParameterName,__FILE__, __LINE__));
     return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE NvxComponentBaseCompatiblePorts(OMX_IN NvxComponent *pNvComp, NvxPort *pPort, OMX_HANDLETYPE hOtherComponent, OMX_U32 hOtherPort)
{
    OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
    OMX_ERRORTYPE error = OMX_ErrorNone;

    oPortDef.nSize = pPort->oPortDef.nSize;
    oPortDef.nVersion = pPort->oPortDef.nVersion;
    oPortDef.nPortIndex = hOtherPort;

    if (OMX_ErrorNone !=
        (error = OMX_GetParameter(hOtherComponent,
                                  OMX_IndexParamPortDefinition, &oPortDef)))
      return error;

    if (!NvxComponentCheckPortCompatibility(&pPort->oPortDef, &oPortDef))
    {
        pPort->hTunnelComponent = 0;
        pPort->nTunnelPort = 0;
        return OMX_ErrorPortsNotCompatible;
    }

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentBaseCompatiblePorts: %s\n",pNvComp->pComponentName));
    return error;
}

OMX_BOOL NvxComponentCheckPortCompatibility(OMX_IN OMX_PARAM_PORTDEFINITIONTYPE *pPortDef1, OMX_IN OMX_PARAM_PORTDEFINITIONTYPE *pPortDef2)
{
    switch (pPortDef1->eDomain)
    {
    case OMX_PortDomainOther:
        if (pPortDef1->format.other.eFormat != pPortDef2->format.other.eFormat)
        {
            return OMX_FALSE;
        }
        break;
    case OMX_PortDomainAudio:
        if ((pPortDef1->format.audio.eEncoding != pPortDef2->format.audio.eEncoding &&
            pPortDef1->format.audio.eEncoding != OMX_AUDIO_CodingAutoDetect &&
            pPortDef2->format.audio.eEncoding != OMX_AUDIO_CodingAutoDetect) ||
            pPortDef1->format.audio.eEncoding == OMX_AUDIO_CodingMax ||
            pPortDef2->format.audio.eEncoding == OMX_AUDIO_CodingMax)
        {
            return OMX_FALSE;
        }
        break;
    case OMX_PortDomainVideo:
        if ((pPortDef1->format.video.eCompressionFormat != pPortDef2->format.video.eCompressionFormat &&
            pPortDef1->format.video.eCompressionFormat != OMX_VIDEO_CodingAutoDetect &&
            pPortDef2->format.video.eCompressionFormat != OMX_VIDEO_CodingAutoDetect) ||
            pPortDef1->format.video.eCompressionFormat == OMX_VIDEO_CodingMax ||
            pPortDef2->format.video.eCompressionFormat == OMX_VIDEO_CodingMax)
        {
            return OMX_FALSE;
        }
        break;
    case OMX_PortDomainImage:
        if ((pPortDef1->format.image.eCompressionFormat != pPortDef2->format.image.eCompressionFormat &&
            pPortDef1->format.image.eCompressionFormat != OMX_IMAGE_CodingAutoDetect &&
            pPortDef2->format.image.eCompressionFormat != OMX_IMAGE_CodingAutoDetect) ||
            pPortDef1->format.image.eCompressionFormat == OMX_IMAGE_CodingMax ||
            pPortDef2->format.image.eCompressionFormat == OMX_IMAGE_CodingMax)
        {
            return OMX_FALSE;
        }
        break;
    default:
        return OMX_FALSE;
    }
    return OMX_TRUE;
}

OMX_ERRORTYPE NvxComponentDestroy(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxComponent *pNvComp;
    NvxPort *pPort;
    OMX_U32 i;

    pNvComp = NvxComponentFromHandle(hComponent);
    if (!pNvComp)
        return OMX_ErrorBadParameter;

    if (g_bAddToScheduler)
        NvxWorkerDeinit(&(pNvComp->oWorkerData), NVX_TIMEOUT_NEVER);

    /* clear callbacks so that we don't use them: we can not trust
     * callbacks here because IL client may have destroyed other
     * dependent objects.  To ensure good memory cleanup, IL client
     * needs to transition us to Loaded or Invalid before
     * destruction */
    NvOsFree(pNvComp->pCallbacks);
    pNvComp->pCallbacks = NULL;
    pNvComp->pCallbackAppData = NULL;

    if (pNvComp->eState != OMX_StateInvalid && pNvComp->eState != OMX_StateLoaded) {
        // worker mutex isn't held here, pass that info on to NvxChangeState
        eError = NvxChangeState(pNvComp, OMX_StateInvalid, OMX_FALSE);
        if (eError == OMX_ErrorInvalidState)
            eError = OMX_ErrorNone;
    }

    if (pNvComp->DeInit)
        eError = pNvComp->DeInit(pNvComp);

    /* free ports */
    for (i = 0; i < pNvComp->nPorts; i++){
        pPort = pNvComp->pPorts + i;

        NvOsFree(pPort->pPortPrivate);
        pPort->pPortPrivate = 0;

        eError = NvxPortReleaseResources(pPort);
        // Handle Invalid state case also.
        if (((pNvComp->ePendingState != pNvComp->eState)  ||
            (pNvComp->eState == OMX_StateInvalid) ||
            (eError == OMX_ErrorNotReady)) &&
             pPort->hTunnelComponent &&
            pPort->bNvidiaTunneling)
        {
            // Do we need global lock here ?
            OMX_SetupTunnel(pPort->hTunnelComponent, pPort->nTunnelPort,
                NULL, 0);
            eError = NvxPortReleaseResources(pPort);
            if (eError == OMX_ErrorNotReady)
            {
                NvOsDebugPrintf("ERROR: %s: %s[%d] port i %d OMX_ErrorNotReady \n",
                    pNvComp->pComponentName,
                    __FUNCTION__, __LINE__, i);
            }
        }
        if (pPort->hMutex){
            NvxMutexDestroy( pPort->hMutex );
            pPort->hMutex = 0;
        }
    }
    pNvComp->nPorts = 0;
    NvOsFree(pNvComp->pPorts);
    pNvComp->pPorts = NULL;

    /* free command queue */
    NvMMQueueDestroy(&(pNvComp->pCommandQueue));
    pNvComp->pCommandQueue = NULL;

    if(pNvComp->hConfigMutex) {
        NvxMutexDestroy(pNvComp->hConfigMutex);
        pNvComp->hConfigMutex = NULL;
    }

    if(pNvComp->hWorkerMutex) {
        NvxMutexDestroy(pNvComp->hWorkerMutex);
        pNvComp->hWorkerMutex = NULL;
    }

    NvOsSemaphoreDestroy(pNvComp->hSetConfigEvent);

    /* free self */
    NvOsFree(pNvComp);
    ((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate = NULL;

    NvxComponentGlobalDeInit();

    return eError;
}


OMX_ERRORTYPE NvxComponentBaseOnResourceAcquired(OMX_IN OMX_PTR pConsumerData,
                                                 OMX_IN OMX_U32 uRmIndex,
                                                 OMX_IN OMX_HANDLETYPE *phResource)
{
    NvxComponent *pNvComp = (NvxComponent *)pConsumerData;
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentBaseOnResourceAcquired\n"));
    /* got them all, end wait */
    if (pNvComp->nRmResourcesAcquired < pNvComp->nRmResources)
        ++pNvComp->nRmResourcesAcquired;

    if (pNvComp->eResourcesState != NVX_RESOURCES_HAVEALL && pNvComp->nRmResourcesAcquired == pNvComp->nRmResources) {
        pNvComp->eResourcesState = NVX_RESOURCES_PROVIDED;
        return NvxWorkerTrigger(&(pNvComp->oWorkerData));
    }
    else
        return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxComponentBaseOnResourceReclaimed(OMX_IN OMX_PTR pConsumerData,
                                               OMX_IN OMX_U32 eType,
                                               OMX_IN OMX_HANDLETYPE *phResource)
{
    OMX_BOOL bMTSched = OMX_TRUE;
    NvxComponent *pNvComp;
    pNvComp = (NvxComponent *)pConsumerData;
    if (!pNvComp)
        return OMX_ErrorBadParameter;

    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentBaseOnResourceReclaimed\n"));

    if (pNvComp->eResourcesState != NVX_RESOURCES_HAVENONE)
        pNvComp->eResourcesState = NVX_RESOURCES_RECLAIMED;

    if (pNvComp->nRmResourcesAcquired > 0)
        --pNvComp->nRmResourcesAcquired;

    NvxSchedulerIsMultithreaded(&bMTSched);

    do {
        NvxWorkerTrigger(&(pNvComp->oWorkerData));
        if (!bMTSched) {
            NvxWorkerRun(&(pNvComp->oWorkerData));
        }
    } while (pNvComp->eResourcesState == NVX_RESOURCES_RECLAIMED);

    return OMX_ErrorNone;
}


OMX_ERRORTYPE NvxComponentBaseAcquireResources(OMX_IN NvxComponent *pNvComp)
{
    OMX_U32 i;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxRmRequestHeader oRequest;
    NvxRmResourceConsumer oConsumer;
    NvxRmResourceIndex *puRmIndex  = &pNvComp->uRmIndex[0];
    OMX_HANDLETYPE *phResource = &pNvComp->hRmHandle[0];
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentBaseAcquireResources:%s\n",pNvComp->pComponentName));

    if (pNvComp->eResourcesState == NVX_RESOURCES_PROVIDED) {
        /* see if we have all the resources we need */
        OMX_U32 nHaveRmResources = 0;
        for (i = 0; i < pNvComp->nRmResources; i++) {
            if (pNvComp->hRmHandle[i] != 0) {
                ++nHaveRmResources;
            }
        }

        if (nHaveRmResources == pNvComp->nRmResources) {
            pNvComp->eResourcesState = NVX_RESOURCES_HAVEALL;
            pNvComp->nRmResourcesAcquired = nHaveRmResources;
            return OMX_ErrorNone;
        }
        else if (nHaveRmResources != 0) {
            pNvComp->eResourcesState = NVX_RESOURCES_HAVESOME;
            return OMX_ErrorInsufficientResources;
        }
        else {
            /* no, must have lost them in the mean time */
            pNvComp->eResourcesState = NVX_RESOURCES_HAVENONE;

            NVXTRACE((NVXT_ERROR,pNvComp->eObjectType, "ERROR:NvxComponentBaseAcquireResources:%s:"
                 "nHaveRmResources = %d:[%s(%d)] \n", pNvComp->pComponentName,nHaveRmResources,__FILE__,__LINE__));
            return OMX_ErrorInsufficientResources;
        }
    }

    NvxUnlockAcqLock();

    for (i = 0; i < pNvComp->nRmResources; ++i, ++phResource, ++puRmIndex) {
        if (!*phResource) {
            OMX_ERRORTYPE eRmError;
            oRequest.nSize = sizeof(oRequest); /* dummy request */
            oConsumer.pConsumerData = (OMX_PTR)pNvComp;
            oConsumer.OnResourceAcquired = NvxComponentBaseOnResourceAcquired;
            oConsumer.OnResourceReclaimed = NvxComponentBaseOnResourceReclaimed;

            eRmError = NvxRmAcquireResource(&oConsumer, &pNvComp->oPriorityState, *puRmIndex,
                                            &oRequest, phResource);
            if (eRmError != OMX_ErrorNone) {
                eError = eRmError;
                if (eError != OMX_ErrorNotReady)
                {
                    NvxLockAcqLock();
                    return eError;
                }
            }
        }
    }

    NvxLockAcqLock();

    return eError;
}

OMX_ERRORTYPE NvxComponentBaseWaitForResources(OMX_IN NvxComponent *pNvComp)
{
    OMX_U32 i;
    OMX_ERRORTYPE eError = OMX_ErrorNone, eLocalError = OMX_ErrorNone;

    NvxRmRequestHeader oRequest;
    NvxRmResourceConsumer oConsumer;
    NvxRmResourceIndex *puRmIndex  = &pNvComp->uRmIndex[0];
    OMX_HANDLETYPE     *phResource = &pNvComp->hRmHandle[0];
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentBaseWaitForResources:%s\n",pNvComp->pComponentName));
    /* acquire resources in order */
    for (i = 0; i < pNvComp->nRmResources; ++i, ++phResource, ++puRmIndex) {
        if (!*phResource) {
            oRequest.nSize = sizeof(oRequest); /* dummy request */
            oConsumer.pConsumerData = (OMX_PTR)pNvComp;
            oConsumer.OnResourceAcquired = NvxComponentBaseOnResourceAcquired;
            oConsumer.OnResourceReclaimed = NvxComponentBaseOnResourceReclaimed;

            eLocalError = NvxRmWaitForResource(&oConsumer, &pNvComp->oPriorityState, *puRmIndex, &oRequest, phResource);
            NvxCheckError( eError, eLocalError );
        }
    }
    return eError;
}

OMX_ERRORTYPE NvxComponentBaseCancelWaitForResources(OMX_IN NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone, eLocalError = OMX_ErrorNone;
    NvxRmResourceConsumer oConsumer;
    NvxRmResourceIndex *puRmIndex  = &pNvComp->uRmIndex[pNvComp->nRmResources];
    OMX_HANDLETYPE     *phResource = &pNvComp->hRmHandle[pNvComp->nRmResources];
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentBaseCancelWaitForResources: %s\n",
        pNvComp->pComponentName));
    /* release resources in reverse order */
    while (phResource > &pNvComp->hRmHandle[0]) {
        --phResource;
        --puRmIndex;
        if (*phResource == 0) {
            oConsumer.pConsumerData = (OMX_PTR)pNvComp;
            oConsumer.OnResourceAcquired = NvxComponentBaseOnResourceAcquired;
            oConsumer.OnResourceReclaimed = NvxComponentBaseOnResourceReclaimed;

            eLocalError = NvxRmCancelWaitForResource(&oConsumer, &pNvComp->oPriorityState, *puRmIndex, phResource);
            NvxCheckError( eError, eLocalError );
            if (eLocalError == OMX_ErrorNone)
                *phResource = 0;
        }
    }
    return eError;
}

OMX_ERRORTYPE NvxComponentBaseReleaseResources(OMX_IN NvxComponent *pNvComp)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone, eLocalError = OMX_ErrorNone;
    NvxRmResourceIndex *puRmIndex  = &pNvComp->uRmIndex[pNvComp->nRmResources];
    OMX_HANDLETYPE     *phResource = &pNvComp->hRmHandle[pNvComp->nRmResources];
    NVXTRACE((NVXT_CALLGRAPH, pNvComp->eObjectType, "+NvxComponentBaseReleaseResources: %s\n",
        pNvComp->pComponentName));    /* release resources in reverse order */
    /* release resources in reverse order */

    NvxUnlockAcqLock();

    while (phResource > &pNvComp->hRmHandle[0]) {
        --phResource;
        --puRmIndex;
        if (*phResource) {
            NvxCheckError(eError, (eLocalError = NvxRmReleaseResource(*puRmIndex, phResource)) );
            if (eLocalError == OMX_ErrorNone)
                *phResource = 0;
        }
    }

    NvxLockAcqLock();

    return eError;
}

#define SETCONFIG_RETRIES 50
OMX_ERRORTYPE NvxRetrySetConfig(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    int retry = SETCONFIG_RETRIES;
    OMX_ERRORTYPE eError = OMX_ErrorNotReady;

    while (eError == OMX_ErrorNotReady && retry > 0)
    {
        eError = OMX_SetConfig(hComponent, nIndex, pComponentConfigStructure);
        retry--;

        if (eError == OMX_ErrorNotReady)
            NvOsSleepMS(1);
    }
    return eError;
}
