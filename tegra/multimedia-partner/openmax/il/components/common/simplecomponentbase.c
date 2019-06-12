/* Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "common/NvxComponent.h"
#include "NVOMX_ComponentBase.h"

#include "nvassert.h"
#include "nvos.h"

#define MAX_EXTENSIONS 256

struct NvxExtEntry {
    const char *name;
    OMX_INDEXTYPE eExtensionIndex;
};

typedef struct SimpleComponentType
{
    NvxComponent *pComp;
    NVOMX_Component *pSimpleBase;
    struct NvxExtEntry oExtensions[MAX_EXTENSIONS];
    OMX_BOOL bFirstRun;
} SimpleComponentType;

static OMX_ERRORTYPE NvxSimpleComponentWorkerFunction(NvxComponent *pThis, OMX_BOOL bAllPortsReady, OMX_BOOL *pbMoreWork, NvxTimeMs *puMaxMsecToNextCall)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    SimpleComponentType *pBase = pThis->pComponentData;
    OMX_BOOL nextReady = OMX_TRUE;
    OMX_U32 i;

    if (!pBase || !pBase->pSimpleBase)
        return OMX_ErrorNone;

    if (!bAllPortsReady)
    {
        OMX_BOOL bAllThere = OMX_TRUE;
        for (i = 0; i < pBase->pSimpleBase->nPorts; i++)
        {
            if (!pThis->pPorts[i].pCurrentBufferHdr && 
                pThis->pPorts[i].oPortDef.eDomain != OMX_PortDomainOther)
            {
                bAllThere = OMX_FALSE;
            }
        }

        if (!bAllThere)
            return OMX_ErrorNone;
    }

    for (i = 0; i < pBase->pSimpleBase->nPorts; i++)
    {
        if (!pThis->pPorts[i].oPortDef.bEnabled)
            pBase->pSimpleBase->pPorts[i].pCurrentBufferHdr = NULL;
        pBase->pSimpleBase->pPorts[i].pCurrentBufferHdr = pThis->pPorts[i].pCurrentBufferHdr;

        if (pBase->pSimpleBase->pPorts[i].pCurrentBufferHdr &&
            pBase->pSimpleBase->pPorts[i].pCurrentBufferHdr->nFlags & NVX_BUFFERFLAG_CONFIGCHANGED)
        {
            pBase->bFirstRun = OMX_TRUE;
        }
    }

    if (pBase->bFirstRun)
    {
        NvxPort *pInput = NULL, *pOutput = NULL;
        OMX_BOOL bCopyFormat = OMX_TRUE, bCopyAudio = OMX_FALSE;
        OMX_PARAM_PORTDEFINITIONTYPE oPortDef;
        OMX_AUDIO_PARAM_PCMMODETYPE oPCMMode;

        OMX_ERRORTYPE err = OMX_ErrorNone;

        oPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
        oPortDef.nVersion.nVersion = NvxComponentSpecVersion.nVersion;
        oPCMMode.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
        oPCMMode.nVersion.nVersion = NvxComponentSpecVersion.nVersion;

        for (i = 0; i < pBase->pSimpleBase->nPorts; i++)
        {
            NvxPort *pPort = &(pThis->pPorts[i]);
            if (pPort->oPortDef.bEnabled)
            {
                if (pPort->oPortDef.eDir == OMX_DirInput && 
                    pPort->hTunnelComponent)
                {
                    if (!pInput)
                        pInput = pPort;
                    else
                        bCopyFormat = OMX_FALSE;

                    oPortDef.nPortIndex = pPort->nTunnelPort;

                    err = OMX_GetParameter(pPort->hTunnelComponent,
                                           OMX_IndexParamPortDefinition,
                                           &oPortDef);
                    if (OMX_ErrorNone == err)
                        NvOsMemcpy(&(pPort->oPortDef.format),
                                   &(oPortDef.format), sizeof(oPortDef.format));

                    if (pBase->pSimpleBase->pPorts[i].pPCMMode)
                    {
                        oPCMMode.nPortIndex = pPort->nTunnelPort;

                        err = OMX_GetParameter(pPort->hTunnelComponent,
                                               OMX_IndexParamAudioPcm,
                                               &oPCMMode);
                        if (OMX_ErrorNone == err)
                            NvOsMemcpy(pBase->pSimpleBase->pPorts[i].pPCMMode,
                                       &oPCMMode, 
                                       sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
                        pBase->pSimpleBase->pPorts[i].pPCMMode->nPortIndex = i;
                        bCopyAudio = (pInput == pPort);
                    }
                }
                else if (pPort->oPortDef.eDir == OMX_DirOutput)
                {
                    if (!pOutput)
                        pOutput = pPort;
                    else
                        bCopyFormat = OMX_FALSE;
                }
            }
        }

        if (bCopyFormat && pInput && pOutput)
        {
            if (NvxComponentCheckPortCompatibility(&pInput->oPortDef, &pOutput->oPortDef))
            {
                NvOsMemcpy(&(pOutput->oPortDef.format), 
                           &(pInput->oPortDef.format),
                           sizeof(pInput->oPortDef.format));
                if (bCopyAudio)
                {
                    OMX_U32 nIn = pInput->oPortDef.nPortIndex;
                    OMX_U32 nOut = pOutput->oPortDef.nPortIndex;

                    if (pBase->pSimpleBase->pPorts[nOut].pPCMMode &&
                        pBase->pSimpleBase->pPorts[nIn].pPCMMode)
                    {
                        NvOsMemcpy(pBase->pSimpleBase->pPorts[nOut].pPCMMode,
                                   pBase->pSimpleBase->pPorts[nIn].pPCMMode,
                                   sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
                        pBase->pSimpleBase->pPorts[nOut].pPCMMode->nPortIndex = nOut;
                    }
                }
            }
        }

        pBase->bFirstRun = 0;
    }

    if (pBase->pSimpleBase->WorkerFunction)
    {
        eError = pBase->pSimpleBase->WorkerFunction(pBase->pSimpleBase, pbMoreWork);
        if (eError != OMX_ErrorNone)
        {
            *pbMoreWork = OMX_FALSE;
            return eError;
        }
    }

    for (i = 0; i < pBase->pSimpleBase->nPorts; i++)
    {
        if (!pThis->pPorts[i].oPortDef.bEnabled)
            continue;

        if (!pThis->pPorts[i].pCurrentBufferHdr)
            NvxPortGetNextHdr(&(pThis->pPorts[i]));

        if (!pThis->pPorts[i].pCurrentBufferHdr)
            nextReady = OMX_FALSE;
    }

    if (nextReady)
        *pbMoreWork = OMX_TRUE;
        
    return eError;
}

static OMX_ERRORTYPE NvxSimpleComponentDeInit(NvxComponent *pThis)
{
    SimpleComponentType *pBase = pThis->pComponentData;

    if (!pBase || !pBase->pSimpleBase)
        return OMX_ErrorNone;

    if (pBase->pSimpleBase->DeInit)
        pBase->pSimpleBase->DeInit(pBase->pSimpleBase);

    NvOsFree(pBase->pSimpleBase);
    pBase->pSimpleBase = NULL;

    NvOsFree(pBase);
    pThis->pComponentData = NULL;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxSimpleComponentAcquireResources(OMX_IN NvxComponent *pThis)
{
   SimpleComponentType *pBase = pThis->pComponentData;

    if (!pBase)
        return OMX_ErrorBadParameter;

    pBase->bFirstRun = OMX_TRUE;

    if (pBase->pSimpleBase->AcquireResources)
        return pBase->pSimpleBase->AcquireResources(pBase->pSimpleBase);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxSimpleComponentReleaseResources(OMX_IN NvxComponent *pThis)
{
    SimpleComponentType *pBase = pThis->pComponentData;

    if (!pBase)
        return OMX_ErrorBadParameter;

    pBase->bFirstRun = OMX_TRUE;

    if (pBase->pSimpleBase->ReleaseResources)
        return pBase->pSimpleBase->ReleaseResources(pBase->pSimpleBase);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxSimpleComponentGetParameter(NvxComponent* pThis, OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    SimpleComponentType *pBase = pThis->pComponentData;
    OMX_BOOL bHandled = OMX_FALSE;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (!pBase)
        return OMX_ErrorBadParameter;

    if (pBase->pSimpleBase->GetParameter)
    {
        eError = pBase->pSimpleBase->GetParameter(pBase->pSimpleBase,
                         nParamIndex, pComponentParameterStructure, &bHandled);
        if (bHandled)
            return eError;
    }

    switch (nParamIndex)
    {
        case OMX_IndexParamAudioPcm:
        {
            OMX_AUDIO_PARAM_PCMMODETYPE *pMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;

            if (pMode->nPortIndex >= pBase->pSimpleBase->nPorts)
                return OMX_ErrorBadParameter;
            if (!pBase->pSimpleBase->pPorts[pMode->nPortIndex].pPCMMode)
                return OMX_ErrorBadParameter;

            NvOsMemcpy(pMode, 
                       pBase->pSimpleBase->pPorts[pMode->nPortIndex].pPCMMode,
                       sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            return OMX_ErrorNone;
        }
        default: break;
    }

    return NvxComponentBaseGetParameter(pThis, nParamIndex, 
                                        pComponentParameterStructure);

}

static OMX_ERRORTYPE NvxSimpleComponentSetParameter(OMX_IN NvxComponent* pThis, OMX_IN OMX_INDEXTYPE nParamIndex, OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    SimpleComponentType *pBase = pThis->pComponentData;
    OMX_BOOL bHandled = OMX_FALSE;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (!pBase)
        return OMX_ErrorBadParameter;

    if (pBase->pSimpleBase->SetParameter)
    {
        eError = pBase->pSimpleBase->SetParameter(pBase->pSimpleBase, 
                         nParamIndex, pComponentParameterStructure, &bHandled);
        if (bHandled)
            return eError;
    }

    switch (nParamIndex)
    {
        case OMX_IndexParamAudioPcm:
        {
            OMX_AUDIO_PARAM_PCMMODETYPE *pMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;

            if (pMode->nPortIndex >= pBase->pSimpleBase->nPorts)
                return OMX_ErrorBadParameter;
            if (!pBase->pSimpleBase->pPorts[pMode->nPortIndex].pPCMMode)
                return OMX_ErrorBadParameter;

            NvOsMemcpy(pBase->pSimpleBase->pPorts[pMode->nPortIndex].pPCMMode,
                       pMode,
                       sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            return OMX_ErrorNone;
        }
        default: break;
    }

    return NvxComponentBaseSetParameter(pThis, nParamIndex, 
                                        pComponentParameterStructure);
}

static OMX_ERRORTYPE NvxSimpleComponentGetConfig(NvxComponent* pThis, OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    SimpleComponentType *pBase = pThis->pComponentData;
    OMX_BOOL bHandled = OMX_FALSE;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (!pBase)
        return OMX_ErrorBadParameter;

    if (pBase->pSimpleBase->GetConfig)
    {
        eError = pBase->pSimpleBase->GetConfig(pBase->pSimpleBase,
                         nIndex, pComponentConfigStructure, &bHandled);
        if (bHandled)
            return eError;
    }

    return NvxComponentBaseGetConfig(pThis, nIndex,
                                     pComponentConfigStructure);
}

static OMX_ERRORTYPE NvxSimpleComponentSetConfig(NvxComponent* pThis, OMX_INDEXTYPE nIndex, OMX_IN OMX_PTR pComponentConfigStructure)
{
    SimpleComponentType *pBase = pThis->pComponentData;
    OMX_BOOL bHandled = OMX_FALSE;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (!pBase)
        return OMX_ErrorBadParameter;

    if (pBase->pSimpleBase->SetConfig)
    {
        eError = pBase->pSimpleBase->SetConfig(pBase->pSimpleBase,
                         nIndex, pComponentConfigStructure, &bHandled);
        if (bHandled)
            return eError;
    }

    return NvxComponentBaseSetConfig(pThis, nIndex, 
                                     pComponentConfigStructure);
}

static OMX_ERRORTYPE NvxSimpleComponentGetExtensionIndex(NvxComponent *pNvComp, OMX_STRING cParameterName, OMX_INDEXTYPE *pIndexType)
{
    SimpleComponentType *pBase = pNvComp->pComponentData;
    struct NvxExtEntry* pEntry;

    if (!pBase)
        return OMX_ErrorBadParameter;

    for (pEntry = pBase->oExtensions; pEntry->name != NULL; pEntry++)
    {
        if (!NvOsStrcmp(pEntry->name, cParameterName)) 
        {
            *pIndexType = pEntry->eExtensionIndex;
            return OMX_ErrorNone;
        }
    }

    return NvxComponentBaseGetExtensionIndex(pNvComp, cParameterName, pIndexType);
}

static OMX_ERRORTYPE NvxSimpleComponentFlush(NvxComponent* pThis, OMX_U32 nPort)
{
    SimpleComponentType *pBase = pThis->pComponentData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (!pBase)
        return OMX_ErrorBadParameter;

    if (pBase->pSimpleBase->Flush)
    {
        eError = pBase->pSimpleBase->Flush(pBase->pSimpleBase, nPort);
    }

    return eError;
}

static OMX_ERRORTYPE NvxSimpleComponentPreChangeState(NvxComponent* pThis, OMX_STATETYPE oNewState)
{
    SimpleComponentType *pBase = pThis->pComponentData;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    
    if (!pBase)
        return OMX_ErrorBadParameter;
    
    if (pBase->pSimpleBase->ChangeState)
    {
        eError = pBase->pSimpleBase->ChangeState(pBase->pSimpleBase, 
                                                 pThis->eState, oNewState);
    }
    
    return eError;
}   


OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_CreateComponent(OMX_HANDLETYPE hComponent, OMX_U32 nPorts, OMX_STRING name, NVOMX_Component **ppComp)
{
    SimpleComponentType *pCompBase;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    NV_ASSERT(name);
    NV_ASSERT(nPorts > 0 && nPorts < NVOMX_COMPONENT_MAX_PORTS);
    NV_ASSERT(ppComp);
    
    pCompBase = NvOsAlloc(sizeof(SimpleComponentType));
    if (!pCompBase)
        return OMX_ErrorInsufficientResources;

    NvOsMemset(pCompBase, 0, sizeof(SimpleComponentType));

    pCompBase->pSimpleBase = NvOsAlloc(sizeof(NVOMX_Component));
    if (!pCompBase->pSimpleBase)
    {
        NvOsFree(pCompBase);
        return OMX_ErrorInsufficientResources;
    }

    NvOsMemset(pCompBase->pSimpleBase, 0, sizeof(NVOMX_Component));

    pCompBase->pSimpleBase->pBase = pCompBase;
    pCompBase->pSimpleBase->nPorts = nPorts;
   
    eError = NvxComponentCreate(hComponent, nPorts, &(pCompBase->pComp));
    if (OMX_ErrorNone != eError)
    {
        NvOsFree(pCompBase->pSimpleBase);
        NvOsFree(pCompBase);
        return eError;
    }

    pCompBase->pComp->eObjectType = NVXT_GENERIC;

    pCompBase->pComp->pComponentName = name;
    pCompBase->pComp->sComponentRoles[0] = NULL;
    pCompBase->pComp->nComponentRoles    = 0;
    pCompBase->pComp->DeInit            = NvxSimpleComponentDeInit;
    pCompBase->pComp->WorkerFunction    = NvxSimpleComponentWorkerFunction;
    pCompBase->pComp->GetParameter      = NvxSimpleComponentGetParameter;
    pCompBase->pComp->SetParameter      = NvxSimpleComponentSetParameter;
    pCompBase->pComp->GetConfig         = NvxSimpleComponentGetConfig;
    pCompBase->pComp->SetConfig         = NvxSimpleComponentSetConfig;
    pCompBase->pComp->AcquireResources  = NvxSimpleComponentAcquireResources;
    pCompBase->pComp->ReleaseResources  = NvxSimpleComponentReleaseResources;
    pCompBase->pComp->GetExtensionIndex = NvxSimpleComponentGetExtensionIndex;
    pCompBase->pComp->Flush             = NvxSimpleComponentFlush;
    pCompBase->pComp->PreChangeState    = NvxSimpleComponentPreChangeState;

    pCompBase->pComp->pComponentData = pCompBase;

    *ppComp = pCompBase->pSimpleBase;

    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_AddRole(NVOMX_Component *pComp, OMX_STRING roleName)
{
    SimpleComponentType *pCompBase;
    OMX_U32 i;

    NV_ASSERT(pComp);
    NV_ASSERT(pComp->pBase);
    NV_ASSERT(roleName);

    pCompBase = (SimpleComponentType *)pComp->pBase;

    i = pCompBase->pComp->nComponentRoles;
    pCompBase->pComp->sComponentRoles[i] = roleName;
    pCompBase->pComp->nComponentRoles    = i + 1;

    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_AddExtensionIndex(NVOMX_Component *pComp, OMX_STRING indexName, OMX_INDEXTYPE indexType)
{
    SimpleComponentType *pCompBase;
    OMX_U32 i = 0;
    struct NvxExtEntry* pEntry;

    NV_ASSERT(pComp);
    NV_ASSERT(pComp->pBase);
    NV_ASSERT(indexName);

    pCompBase = (SimpleComponentType *)pComp->pBase;

    for (pEntry = pCompBase->oExtensions; pEntry->name != NULL; pEntry++, i++)
    {
        if (i == MAX_EXTENSIONS - 1)
            return OMX_ErrorNoMore;
    }

    pEntry->name = indexName;
    pEntry->eExtensionIndex = indexType;
 
    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_InitPort(NVOMX_Component *pComp, OMX_U32 nPort, OMX_DIRTYPE eDir, OMX_U32 nBufferCount, OMX_U32 nBufferSize, OMX_PORTDOMAINTYPE eDomain, OMX_PTR pFormat)
{
    SimpleComponentType *pCompBase;

    NV_ASSERT(pComp);
    NV_ASSERT(pComp->pBase);
    NV_ASSERT(pFormat);
   
    if (nPort >= pComp->nPorts)
        return OMX_ErrorBadParameter;

    pCompBase = (SimpleComponentType *)pComp->pBase;

    switch (eDomain)
    {
        case OMX_PortDomainOther:
            NvxPortInitOther(&(pCompBase->pComp->pPorts[nPort]), eDir, 
                             nBufferCount, nBufferSize, 
                             *(OMX_OTHER_FORMATTYPE *)pFormat);
            break;
        case OMX_PortDomainAudio:
            NvxPortInitAudio(&(pCompBase->pComp->pPorts[nPort]), eDir,
                             nBufferCount, nBufferSize,
                             *(OMX_AUDIO_CODINGTYPE *)pFormat);
            if (*(OMX_AUDIO_CODINGTYPE *)pFormat == OMX_AUDIO_CodingPCM)
            {
                OMX_AUDIO_PARAM_PCMMODETYPE  *pPcm;
                ALLOC_STRUCT(pCompBase->pComp, pPcm, OMX_AUDIO_PARAM_PCMMODETYPE);
                if (!pPcm)
                    return OMX_ErrorInsufficientResources;

                pCompBase->pComp->pPorts[nPort].pPortPrivate = pPcm;

                pPcm->nPortIndex = nPort; 
                pPcm->nChannels = 2;
                pPcm->eNumData = OMX_NumericalDataSigned;
                pPcm->nSamplingRate = 48000;
                pPcm->ePCMMode = OMX_AUDIO_PCMModeLinear;
                pPcm->bInterleaved = OMX_TRUE;
                pPcm->nBitPerSample = 16;
                pPcm->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
                pPcm->eChannelMapping[1] = OMX_AUDIO_ChannelRF;

                pComp->pPorts[nPort].pPCMMode = pPcm;
            }
            break;
        case OMX_PortDomainVideo:
            NvxPortInitVideo(&(pCompBase->pComp->pPorts[nPort]), eDir,
                             nBufferCount, nBufferSize, 
                             *(OMX_VIDEO_CODINGTYPE *)pFormat);
            break;
        case OMX_PortDomainImage:
            NvxPortInitImage(&(pCompBase->pComp->pPorts[nPort]), eDir,
                             nBufferCount, nBufferSize,
                             *(OMX_IMAGE_CODINGTYPE *)pFormat);
            break;
        default:
            return OMX_ErrorBadParameter;
    }

    pComp->pPorts[nPort].pPortDef = &(pCompBase->pComp->pPorts[nPort].oPortDef);

    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_ReleaseEmptyBuffer(NVOMX_Component *pComp, OMX_U32 nPort, OMX_BUFFERHEADERTYPE *pBuffer)
{
    SimpleComponentType *pCompBase;

    NV_ASSERT(pComp);
    NV_ASSERT(pComp->pBase);

    if (nPort >= pComp->nPorts)
        return OMX_ErrorBadParameter;

    pCompBase = (SimpleComponentType *)pComp->pBase;

    NvxPortReleaseBuffer(&(pCompBase->pComp->pPorts[nPort]), pBuffer);

    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_DeliverFullBuffer(NVOMX_Component *pComp, OMX_U32 nPort, OMX_BUFFERHEADERTYPE *pBuffer) 
{
    SimpleComponentType *pCompBase;                                             
    NV_ASSERT(pComp);                                                               NV_ASSERT(pComp->pBase);
                                                                                    if (nPort >= pComp->nPorts)
        return OMX_ErrorBadParameter;                                           
    pCompBase = (SimpleComponentType *)pComp->pBase;                            

    NvxPortDeliverFullBuffer(&(pCompBase->pComp->pPorts[nPort]), pBuffer);

    return OMX_ErrorNone;

}

OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_CopyBufferMetadata(NVOMX_Component *pComp, OMX_U32 nSourcePort, OMX_U32 nDestPort)
{
    SimpleComponentType *pCompBase;
    NV_ASSERT(pComp);                                                               NV_ASSERT(pComp->pBase);

    if (nSourcePort >= pComp->nPorts || nDestPort >= pComp->nPorts)
        return OMX_ErrorBadParameter;
         
    pCompBase = (SimpleComponentType *)pComp->pBase;

    NvxPortCopyMetadata(&(pCompBase->pComp->pPorts[nSourcePort]),
                        &(pCompBase->pComp->pPorts[nDestPort]));

    return OMX_ErrorNone;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY NVOMX_SendClockUpdate(NVOMX_Component *pComp, OMX_U32 nPortClock, OMX_TICKS nAudioTime)
{
    SimpleComponentType *pCompBase;
    NvxPort *pPortClock;
    OMX_HANDLETYPE hClock;
    NV_ASSERT(pComp);                                                               NV_ASSERT(pComp->pBase);

    if (nPortClock >= pComp->nPorts)
        return OMX_ErrorBadParameter;

    pCompBase = (SimpleComponentType *)pComp->pBase;

    pPortClock = &((pCompBase->pComp->pPorts)[nPortClock]);

    if (!pPortClock)
        return OMX_ErrorBadParameter;

    hClock = pPortClock->hTunnelComponent;

    if (!hClock)
        return OMX_ErrorBadParameter;

    if (pPortClock->bNvidiaTunneling)
    {
        NvxCCSetCurrentAudioRefClock(hClock, nAudioTime);
    }
    else
    {
        OMX_TIME_CONFIG_TIMESTAMPTYPE timestamp;
        timestamp.nSize = sizeof(OMX_TIME_CONFIG_TIMESTAMPTYPE);
        timestamp.nVersion = pCompBase->pComp->oSpecVersion;
        timestamp.nPortIndex = pPortClock->nTunnelPort;
        timestamp.nTimestamp = nAudioTime;

        OMX_SetConfig(hClock, OMX_IndexConfigTimeCurrentAudioReference, 
                      &timestamp);
    }

    return OMX_ErrorNone;
}   

