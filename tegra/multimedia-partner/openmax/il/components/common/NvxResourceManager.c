/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NvxResourceManager.c */

#include "NvxComponent.h"
#include "NvxPort.h"
#include "NvxError.h"
#include <stdlib.h>
#include <string.h>
#include "NvxIndexExtensions.h"
#include "NvxHelpers.h"
#include "NvxResourceManager.h"

#include "nvos.h"

#define NVXRM_MAX_CLIENTS 256
#define NVXRM_RESOURCEMAX 64

typedef struct NvxRmClient {
    NvxRmResourceConsumer oConsumer; 
    OMX_PRIORITYMGMTTYPE oPriority; 
    NvxRmResourceIndex uIndex;
    NvxRmRequestHeader *pRequest;
    OMX_HANDLETYPE *phResource;
    OMX_BOOL bWaitIndefinitely;
    OMX_BOOL bReclaiming;
} NvxRmClient; 

typedef struct NvxResourceManager
{
    NvxRmClient             *pClient[NVXRM_MAX_CLIENTS];
    OMX_U32                 nClients;
    OMX_HANDLETYPE          hLock;
    OMX_U32                 nResources;
    const char *            pName[NVXRM_RESOURCEMAX];
    NvxRmResourceProvider   oProvider[NVXRM_RESOURCEMAX];
    OMX_PTR                 pProviderData[NVXRM_RESOURCEMAX];
} NvxResourceManager;

/* local prototypes */
static OMX_ERRORTYPE NvxRmDemoteClient( OMX_IN OMX_PRIORITYMGMTTYPE *pPriority, 
                                        OMX_IN NvxRmResourceIndex uIndex );

/* the RM is a singleton */
static NvxResourceManager g_RM;

OMX_ERRORTYPE NvxRmInit(void)
{
    int i;

    NvxMutexCreate(&g_RM.hLock);
    g_RM.nResources = 0;
    g_RM.nClients = 0;
    for (i=0;i<NVXRM_RESOURCEMAX;i++){
        g_RM.oProvider[i].GetResource       = 0;
        g_RM.oProvider[i].ReleaseResource   = 0;
        g_RM.pName[i] = 0;
        g_RM.pProviderData[i] = 0;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxRmDeInit(void)
{
    if (g_RM.nClients) return OMX_ErrorNotReady;
    NvxMutexDestroy(g_RM.hLock);
    return OMX_ErrorNone;
}

/* this can be called multiple times: there is no deinit! */
OMX_ERRORTYPE NvxRmSetResourceProvider(const char *pName,
                                       NvxRmResourceProvider *pProvider, OMX_PTR pProviderData, OMX_OUT NvxRmResourceIndex* puIndex)
{
    OMX_U32 uIndex;
    if (OMX_ErrorNone == NvxRmGetIndex(pName, &uIndex)) {
        if ( pProviderData == g_RM.pProviderData[uIndex]
             && pProvider->GetResource == g_RM.oProvider[uIndex].GetResource
             && pProvider->ReleaseResource == g_RM.oProvider[uIndex].ReleaseResource) {
            *puIndex = uIndex;
            return OMX_ErrorNone;
        }
        else
            return OMX_ErrorUndefined;
    }
        
    NvxMutexLock(g_RM.hLock);
    if (g_RM.nResources >= NVXRM_RESOURCEMAX) {
        NvxMutexUnlock(g_RM.hLock);
        return OMX_ErrorInsufficientResources;
    }
    
    uIndex = g_RM.nResources++;
    g_RM.pName[uIndex] = pName;
    g_RM.oProvider[uIndex] = *pProvider;
    g_RM.pProviderData[uIndex] = pProviderData;
    *puIndex = uIndex;
    NvxMutexUnlock(g_RM.hLock);
    return OMX_ErrorNone;
}

/* sets the provider interface for a particular type of resource */
OMX_ERRORTYPE NvxRmGetIndex(const char *pName, OMX_OUT NvxRmResourceIndex* puIndex)
{
    OMX_U32 i;
    OMX_ERRORTYPE eError = OMX_ErrorInvalidComponentName;
    NvxMutexLock(g_RM.hLock);
    for (i = 0; i < g_RM.nResources; i++)
        if (strcmp(pName, g_RM.pName[i]) == 0) {
            eError = OMX_ErrorNone;
            *puIndex = i;
            break;
        }
    NvxMutexUnlock(g_RM.hLock);
    return eError;
}

OMX_ERRORTYPE NvxRmGetResourceName(OMX_IN NvxRmResourceIndex uIndex, OMX_OUT const char **ppName)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxMutexLock(g_RM.hLock);

    if (uIndex < g_RM.nResources) {
        *ppName = g_RM.pName[uIndex];
    }
    else {
        eError = OMX_ErrorBadParameter;
    }

    NvxMutexUnlock(g_RM.hLock);
    return eError;
}


static OMX_ERRORTYPE NvxRmAddClient(OMX_IN NvxRmResourceConsumer *pConsumer, 
                                    OMX_IN OMX_PRIORITYMGMTTYPE *pPriority, 
                                    OMX_IN NvxRmResourceIndex uIndex,
                                    NvxRmRequestHeader *pRequest,
                                    OMX_HANDLETYPE *phResource,
                                    OMX_BOOL bWaitIndefinitely)
{
    NvxRmClient *pClient;
    OMX_ERRORTYPE eError;
    eError = OMX_ErrorNone;

    /* add a client */
    pClient = NvOsAlloc(sizeof(NvxRmClient));
    if(!pClient)
        return OMX_ErrorInsufficientResources;
    g_RM.pClient[g_RM.nClients++] = pClient;
    
    /* copy request */
    pClient->pRequest = NvOsAlloc(pRequest->nSize);
    if(!pClient->pRequest)
    {
        g_RM.nClients--;
        NvOsFree(pClient);
        return OMX_ErrorInsufficientResources;
    }
    memcpy(pClient->pRequest, pRequest, pRequest->nSize);

    /* initialize client struct */
    pClient->oConsumer  = *pConsumer;
    pClient->oPriority   = *pPriority;
    pClient->uIndex       = uIndex;
    pClient->phResource  = phResource;
    pClient->bWaitIndefinitely = bWaitIndefinitely;
    pClient->bReclaiming = OMX_FALSE;

    return eError;
}


static OMX_ERRORTYPE NvxRmFindWorstClient(NvxRmResourceIndex uIndex, NvxRmClient **ppClient)
{
    OMX_ERRORTYPE eError;
    OMX_U32 i, nLowMan; /* on the totem pole */
    OMX_U32 nWorstPriority;

    eError = OMX_ErrorNone;
   
    nWorstPriority = 0x7fffffff;
    nLowMan = g_RM.nClients;
    for(i=0;i<g_RM.nClients;i++)
    {
        *ppClient = g_RM.pClient[i];
        if (*((*ppClient)->phResource) &&                           /* have a resource */
            ((*ppClient)->uIndex == uIndex) &&                        /* of right type */
            (((*ppClient)->oPriority.nGroupPriority>nWorstPriority) /* and either lowest priority or */
             ||nLowMan==g_RM.nClients))                              /* no low man yet */
        {
            nWorstPriority = (*ppClient)->oPriority.nGroupPriority;
            nLowMan = i;
        }
    }
    if (nLowMan == g_RM.nClients){
        *ppClient = 0;
        eError = OMX_ErrorUndefined;
    } else {
        *ppClient = g_RM.pClient[nLowMan];
    }

    return eError;
}

OMX_ERRORTYPE NvxRmDemoteClient(OMX_IN OMX_PRIORITYMGMTTYPE *pPriority, 
                                OMX_IN NvxRmResourceIndex uIndex)
{
    OMX_ERRORTYPE eError;
    OMX_U32 i, nLowMan; /* on the totem pole */
    OMX_U32 nWorstPriority;
    NvxRmClient *pClient;
    OMX_U32 nGroupID;
    NvxRmClient  *pOldClients[NVXRM_MAX_CLIENTS];
    OMX_U32 nOldClients = 0;

    eError = OMX_ErrorNone;
   
    nWorstPriority = pPriority->nGroupPriority;
    nLowMan = g_RM.nClients;
    for(i=0;i<g_RM.nClients;i++)
    {
        pClient = g_RM.pClient[i];
        if (*(pClient->phResource) &&                           /* have a resource */
            (pClient->uIndex == uIndex) &&                        /* of right type */
            (pClient->oPriority.nGroupPriority>nWorstPriority)){/* and lowest priority */
            nWorstPriority = pClient->oPriority.nGroupPriority;
            nLowMan = i;
        }
    }
    if (nLowMan == g_RM.nClients){
        eError = OMX_ErrorUndefined;
    } else {
        /* demote all clients of the low man's group id */
        nGroupID = g_RM.pClient[nLowMan]->oPriority.nGroupID;
        for (i=0;i<g_RM.nClients;i++)
        {
            pClient = g_RM.pClient[i];
            if ((pClient->oPriority.nGroupID == nGroupID) && !pClient->bReclaiming)
            {
                /* remember this client */
                pOldClients[nOldClients++] = pClient;
                pClient->bReclaiming = OMX_TRUE;
            }
        }
    }

    NvxMutexUnlock(g_RM.hLock);
    for(i=0;i<nOldClients;i++) {
        pClient = pOldClients[i];

        /* reclaim resource from client */
        /* client will call ReleaseResource */
        if (pClient->bReclaiming == OMX_TRUE) {
            NvxCheckError(eError, pClient->oConsumer.OnResourceReclaimed( pClient->oConsumer.pConsumerData, 
                                                                          pClient->uIndex, pClient->phResource));
        }
    }
    NvxMutexLock(g_RM.hLock);
    for(i=0;i<nOldClients;i++) {
        pClient = pOldClients[i];

        pClient->bReclaiming = OMX_FALSE;
        pClient->oConsumer.OnResourceReclaimed = NULL;
        NvOsFree(pClient->pRequest);
        NvOsFree(pClient); 
    }
    return eError;
}

static OMX_ERRORTYPE NvxRmFindBestCandidate(NvxRmResourceIndex uIndex, NvxRmClient **ppClient)
{
    NvxRmClient *pClient;
    OMX_ERRORTYPE eError;
    OMX_U32 i, nBigMan; /* on campus */
    OMX_U32 nBestPriority;

    eError = OMX_ErrorNone;
   
    /* find the highest priority waiting client */
    nBestPriority = 0x7fffffff;
    nBigMan = g_RM.nClients;
    for(i=0;i<g_RM.nClients;i++)
    {
        pClient = g_RM.pClient[i];
        if (pClient && 
            (OMX_FALSE == pClient->bReclaiming) &&                  /* not being reclaimed */
            (0!=pClient->phResource) &&
            (0==*(pClient->phResource)) &&                        /* need a resource */
            (pClient->uIndex == uIndex) &&                        /* of right type */
            (pClient->oPriority.nGroupPriority<nBestPriority)){ /* and best priority */
            nBestPriority = pClient->oPriority.nGroupPriority;
            nBigMan = i;
        }
    }
    if (nBigMan == g_RM.nClients){
        *ppClient = 0;
        eError = OMX_ErrorUndefined;
    } else {
        *ppClient = g_RM.pClient[nBigMan];
    }

    return eError;
}

static OMX_ERRORTYPE NvxRmServiceWaitingClients(void)
{
    OMX_ERRORTYPE eError, eResourceError;
    NvxRmClient *pBestClient, *pWorstClient, *pClient;
    OMX_U32 uIndex,i,j;

    eResourceError = eError = OMX_ErrorNone;
   
    /* for each resource type */
    for (uIndex=0;uIndex<NVXRM_RESOURCEMAX;uIndex++)
    {
        eResourceError = OMX_ErrorNone;
        /* find best waiting client */
        if (OMX_ErrorNone == NvxRmFindBestCandidate(uIndex, &pBestClient))
        {     
            /* demote a client of the same type of lesser priority */
            if (OMX_ErrorNone == NvxRmDemoteClient(&pBestClient->oPriority,uIndex)){

                /* try to get the resource if it hasn't already been gotten through demotion */
                if (0 == *(pBestClient->phResource)
                    && OMX_ErrorNone == (eResourceError = g_RM.oProvider[uIndex].GetResource(g_RM.pProviderData[uIndex], pBestClient->pRequest, pBestClient->phResource)))
                {
                    /* tell client we got the resource */
                    NvxMutexUnlock(g_RM.hLock);
                    pBestClient->oConsumer.OnResourceAcquired(pBestClient->oConsumer.pConsumerData, 
                                                              pBestClient->uIndex, pBestClient->phResource);
                    NvxMutexLock(g_RM.hLock);
                }
                else
                    eError = eResourceError;
            }
        }
    }

    /* Fail each client that isn't waiting indefinitely but isn't the best candidate */
    for (i=0; i<g_RM.nClients;i++)
    {
        pClient = g_RM.pClient[i];
        if (0==*(pClient->phResource) && !pClient->bWaitIndefinitely && !pClient->bReclaiming)
        {
            NvxRmFindBestCandidate(pClient->uIndex, &pBestClient);
            NvxRmFindWorstClient(pClient->uIndex, &pWorstClient);            
        
            if ((pBestClient != pClient) || 
                !pWorstClient ||
                (pClient->oPriority.nGroupPriority >= pWorstClient->oPriority.nGroupPriority))
            {
                /* send a zero to indicate the resource couldn't be acquired */
                pClient->oConsumer.OnResourceAcquired(pClient->oConsumer.pConsumerData,
                                                      pClient->uIndex, pClient->phResource);
                
                /* remove client from list */
                g_RM.nClients--;
                for (j=i;j<g_RM.nClients;j++) g_RM.pClient[j] = g_RM.pClient[j+1];
                g_RM.pClient[g_RM.nClients] = NULL;

                /* Free client memory */
                if (pClient->bReclaiming != OMX_TRUE) {
                    NvOsFree(pClient->pRequest);
                    NvOsFree(pClient);
                }
                else {
                    pClient->oConsumer.OnResourceReclaimed = NULL;
                }
            }
        }
     
        if (eError != OMX_ErrorNone)
            return eError;
    }
    return eError;
}


OMX_ERRORTYPE NvxRmAcquireResource(OMX_IN NvxRmResourceConsumer *pConsumer, 
                                   OMX_IN OMX_PRIORITYMGMTTYPE *pPriority, 
                                   OMX_IN NvxRmResourceIndex uIndex,
                                   NvxRmRequestHeader *pRequest, 
                                   OMX_OUT OMX_HANDLETYPE *phResource)
{
    OMX_ERRORTYPE eError;

    NvxMutexLock(g_RM.hLock);
    if (uIndex >= g_RM.nResources) {
        NvxMutexUnlock(g_RM.hLock);
        return OMX_ErrorBadParameter;
    }

    /* if successful then remember the acquisition (we may need to take the resource back later) */
    if (OMX_ErrorNone != 
        (eError = NvxRmAddClient(pConsumer, pPriority, uIndex, pRequest, phResource, OMX_FALSE)))
    {
        NvxMutexUnlock(g_RM.hLock);
        return eError;
    }

    /* try to get the resource - if we failed then return without remembering the request */
    if (OMX_ErrorNone != (eError = g_RM.oProvider[uIndex].GetResource(g_RM.pProviderData[uIndex], pRequest, phResource))){
        NvxRmServiceWaitingClients();
        if (eError == OMX_ErrorInsufficientResources)
            eError = OMX_ErrorNotReady;
    }

    NvxMutexUnlock(g_RM.hLock);

    return eError;
}

OMX_ERRORTYPE NvxRmWaitForResource( OMX_IN NvxRmResourceConsumer *pConsumer, 
                                    OMX_PRIORITYMGMTTYPE *pPriority, 
                                    NvxRmResourceIndex uIndex,
                                    NvxRmRequestHeader *pRequest,  /* resource specific parameters */
                                    OMX_HANDLETYPE *phResource)
{
    OMX_ERRORTYPE eError;

    NvxMutexLock(g_RM.hLock);
    if (uIndex >= g_RM.nResources) {
        NvxMutexUnlock(g_RM.hLock);
        return OMX_ErrorBadParameter;
    }

    /* add the client to the list */
    *phResource = 0;
    eError = NvxRmAddClient(pConsumer, pPriority, uIndex, pRequest, phResource, OMX_TRUE);
   
    /* service waiting clients */
    NvxRmServiceWaitingClients();

    NvxMutexUnlock(g_RM.hLock);

    return eError;
}

OMX_ERRORTYPE NvxRmCancelWaitForResource( OMX_IN NvxRmResourceConsumer *pConsumer, 
                                          OMX_IN OMX_PRIORITYMGMTTYPE *pPriority, 
                                          OMX_IN NvxRmResourceIndex uIndex,
                                          OMX_IN OMX_HANDLETYPE *phResource)
{
    OMX_ERRORTYPE eError;
    OMX_U32 i,j;
    NvxRmClient *pClient;

    NvxMutexLock(g_RM.hLock);
    if (uIndex >= g_RM.nResources) {
        NvxMutexUnlock(g_RM.hLock);
        return OMX_ErrorBadParameter;
    }

    eError = OMX_ErrorNone;

    /* find the associated client and delete it */
    for(i=0;i<g_RM.nClients;i++){
        pClient = g_RM.pClient[i];
        if (pClient->uIndex == uIndex && pClient->phResource == phResource)
        {
            /* remove client from list */
            g_RM.nClients--;
            for (j=i;j<g_RM.nClients;j++) g_RM.pClient[j] = g_RM.pClient[j+1];
            g_RM.pClient[g_RM.nClients+1] = NULL;

            /* Free client memory */
            NvOsFree(pClient->pRequest);
            NvOsFree(pClient); 
            break;
        }
    }

    NvxRmServiceWaitingClients();

    NvxMutexUnlock(g_RM.hLock);

    return eError;
}

OMX_ERRORTYPE NvxRmReleaseResource(NvxRmResourceIndex uIndex, OMX_IN OMX_HANDLETYPE *phResource)
{
    OMX_ERRORTYPE eError;
    OMX_U32 i,j;
    NvxRmClient *pClient;

    eError = OMX_ErrorNone;

    NvxMutexLock(g_RM.hLock);
    if (uIndex >= g_RM.nResources) {
        NvxMutexUnlock(g_RM.hLock);
        return OMX_ErrorBadParameter;
    }

    /* find the associated resource and release it */
    for(i=0;i<g_RM.nClients;i++){
        pClient = g_RM.pClient[i];
        if (pClient && pClient->uIndex == uIndex && pClient->phResource == phResource  && *(pClient->phResource) == *phResource)
        {
            /* remove client from list */
            g_RM.nClients--;
            for (j=i;j<g_RM.nClients;j++) g_RM.pClient[j] = g_RM.pClient[j+1];
            g_RM.pClient[g_RM.nClients] = NULL;

            /* reclaim resource from client */
            g_RM.oProvider[uIndex].ReleaseResource(g_RM.pProviderData[uIndex], *pClient->phResource);
            *(pClient->phResource) = 0;
            if (pClient->bReclaiming != OMX_TRUE) {
                NvOsFree(pClient->pRequest);
                NvOsFree(pClient); 
            }
            break;
        }
    }
     
    /* find best waiting client */
    if (OMX_ErrorNone == NvxRmFindBestCandidate(uIndex, &pClient))
    {     

        /* try to get the resource - if we failed then return without remembering the request */
        if (OMX_ErrorNone == 
            (eError = g_RM.oProvider[uIndex].GetResource(g_RM.pProviderData[uIndex], pClient->pRequest, pClient->phResource)))
        {
            /* tell client we got the resource */
            NvxMutexUnlock(g_RM.hLock);
            pClient->oConsumer.OnResourceAcquired(pClient->oConsumer.pConsumerData, pClient->uIndex, pClient->phResource);
            NvxMutexLock(g_RM.hLock);
        }
    }

    NvxMutexUnlock(g_RM.hLock);

    return eError;
}
