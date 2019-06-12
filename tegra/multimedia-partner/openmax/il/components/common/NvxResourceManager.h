/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** NVX_ResourceManager.h */

#ifndef NVX_RESOURCE_MANAGER_H
#define NVX_RESOURCE_MANAGER_H

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Index.h>
#include <OMX_Image.h>
#include <OMX_Audio.h>
#include <OMX_Video.h>
#include <OMX_IVCommon.h>
#include <OMX_Other.h>
#include <NvxPort.h>
#include <NvxScheduler.h>
#include <NvxMutex.h>

/* Header for any resource request */
/* Every request must begin with the size of the object so the request may be copied. */
typedef struct NvxRmRequestHeader{
    OMX_U32 nSize;
}NvxRmRequestHeader;


/* resource index */
typedef OMX_U32 NvxRmResourceIndex;

/* interface to a resource provider */
typedef struct NvxRmResourceProvider
{
    OMX_ERRORTYPE (*GetResource)(OMX_PTR pProviderData, NvxRmRequestHeader *pRequest, OMX_HANDLETYPE *phResource);
    OMX_ERRORTYPE (*ReleaseResource)(OMX_PTR pProviderData, OMX_HANDLETYPE hResource);

} NvxRmResourceProvider;

/* callbacks to any resource consumer */
typedef struct NvxRmResourceConsumer
{
    OMX_PTR pConsumerData;
    OMX_ERRORTYPE (*OnResourceAcquired)(OMX_IN OMX_PTR pConsumerData, OMX_IN OMX_U32 uIndex, OMX_IN OMX_HANDLETYPE *phResource);
    OMX_ERRORTYPE (*OnResourceReclaimed)(OMX_IN OMX_PTR pConsumerData, OMX_IN OMX_U32 uIndex, OMX_IN OMX_HANDLETYPE *phResource); 
} NvxRmResourceConsumer;

/* initialize the RM */
OMX_ERRORTYPE NvxRmInit(void);

/* de-initialize the RM */
OMX_ERRORTYPE NvxRmDeInit(void);

/* sets the provider interface for a particular type of resource */
OMX_ERRORTYPE NvxRmSetResourceProvider(const char *pName, NvxRmResourceProvider *pProvider, OMX_PTR pProviderData, OMX_OUT NvxRmResourceIndex* puIndex);

/* gets the index for a resource by name */
OMX_ERRORTYPE NvxRmGetIndex(const char *pName, OMX_OUT NvxRmResourceIndex* puIndex);

/* gets the name of a resource by index */
OMX_ERRORTYPE NvxRmGetResourceName(OMX_IN NvxRmResourceIndex uIndex, OMX_OUT const char **ppName);

/* Attempt to acquire a resource - don't add the consumer to the wait queue if acquistion failed */
/* This call is synchronous; return value represents success/failure of acquisition */
OMX_ERRORTYPE NvxRmAcquireResource(OMX_IN NvxRmResourceConsumer *pConsumer, 
                                   OMX_PRIORITYMGMTTYPE *pPriority, 
                                   NvxRmResourceIndex uIndex,
                                   NvxRmRequestHeader *pRequest, /* resource specific parameters */
                                   OMX_HANDLETYPE *phResource);

/* release a resource */
OMX_ERRORTYPE NvxRmReleaseResource( NvxRmResourceIndex uIndex, OMX_IN OMX_HANDLETYPE *hResource );

/* Attempt to acquire a resource - add the consumer to the wait queue if acquistion failed */
/* This call is asynchronous; the consumer will be called if/when the requested resource is acquired */
OMX_ERRORTYPE NvxRmWaitForResource( OMX_IN NvxRmResourceConsumer *pConsumer, 
                                    OMX_PRIORITYMGMTTYPE *pPriority, 
                                    NvxRmResourceIndex uIndex,
                                    NvxRmRequestHeader *pRequest, /* resource specific parameters */
                                    OMX_HANDLETYPE *phResource);

/* Remove waiting consumer from the wait queue */
OMX_ERRORTYPE NvxRmCancelWaitForResource( OMX_IN NvxRmResourceConsumer *pConsumer, 
                                          OMX_PRIORITYMGMTTYPE *pPriority, 
                                          NvxRmResourceIndex uIndex, 
                                          OMX_IN OMX_HANDLETYPE *phResource);


#endif

