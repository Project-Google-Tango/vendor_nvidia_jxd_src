/* Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* Includes */
#include "NVOMX_ComponentBase.h"

#include <stdlib.h>
#include <string.h>

/*
   Sample data structure used for this component. This is passed back to every
   function callback as the pComponentData member of the NVOMX_Component
   structure.
 */
typedef struct SampleCompData
{
    int data;
} SampleCompData;

/*
   Port numbers.
 */
#define PORT_IN  0
#define PORT_OUT 1

/*
   The main worker function for this sample. Simply copies over the data
   from input to output.
 */
static OMX_ERRORTYPE SampleWorkerFunction(NVOMX_Component *pComp, 
                                          OMX_BOOL *pbMoreWork)
{
    OMX_BUFFERHEADERTYPE *pInBuffer, *pOutBuffer;

    /* The NVOMX_Component base guarantees these to be non-null. */
    pInBuffer = pComp->pPorts[PORT_IN].pCurrentBufferHdr;
    pOutBuffer = pComp->pPorts[PORT_OUT].pCurrentBufferHdr;

    /* Copy over the buffer flags, marks, etc. */
    NVOMX_CopyBufferMetadata(pComp, PORT_IN, PORT_OUT);

    /* If there is data, and it will fit, copy it over to the output buffer. */
    if (pInBuffer->nFilledLen > 0 && 
        pInBuffer->nFilledLen <= pOutBuffer->nAllocLen)
    {
        memcpy(pOutBuffer->pBuffer, pInBuffer->pBuffer, pInBuffer->nFilledLen);
        pOutBuffer->nFilledLen = pInBuffer->nFilledLen;
    }

    /* Deliver the output buffer on to the next component. */
    NVOMX_DeliverFullBuffer(pComp, PORT_OUT, pOutBuffer);
    /* Done with the input buffer, so now release it */
    NVOMX_ReleaseEmptyBuffer(pComp, PORT_IN, pInBuffer);

    /* There is no more work to do. */
    *pbMoreWork = OMX_FALSE;

    return OMX_ErrorNone;
}

/*
   Default GetParameter implementation. Does not do anything, so sets
   bHandled to OMX_FALSE.
  */
static OMX_ERRORTYPE SampleGetParameter(NVOMX_Component *pComp, 
                                        OMX_INDEXTYPE nParamIndex, 
                                        OMX_PTR pComponentParameterStructure, 
                                        OMX_BOOL *bHandled)
{
    *bHandled = OMX_TRUE;
    switch (nParamIndex)
    {
        default:
            *bHandled = OMX_FALSE;
            break;
    }

    return OMX_ErrorNone;    
}

/*
   Default SetParameter implementation. Doesnot do anything, so sets
   bHandled to OMX_FALSE.
  */
static OMX_ERRORTYPE SampleSetParameter(NVOMX_Component *pComp, 
                                        OMX_INDEXTYPE nIndex, 
                                        OMX_PTR pComponentParameterStructure, 
                                        OMX_BOOL *bHandled)
{
    *bHandled = OMX_TRUE;
    switch (nIndex)
    {
        default:
            *bHandled = OMX_FALSE;
            break;
    }

    return OMX_ErrorNone;
}

/*
   Default GetConfig implementation. Does not do anything, so sets
   bHandled to OMX_FALSE.
  */
static OMX_ERRORTYPE SampleGetConfig(NVOMX_Component *pComp, 
                                     OMX_INDEXTYPE nIndex, 
                                     OMX_PTR pComponentConfigStructure, 
                                     OMX_BOOL *bHandled)
{
    *bHandled = OMX_TRUE;
    switch (nIndex)
    {
        default:
            *bHandled = OMX_FALSE;
            break;
    }

    return OMX_ErrorNone;

}

/*
   Default SetConfig implementation. Does not do anything, so sets
   bHandled to OMX_FALSE.
  */
static OMX_ERRORTYPE SampleSetConfig(NVOMX_Component *pComp, 
                                     OMX_INDEXTYPE nIndex, 
                                     OMX_PTR pComponentConfigStructure,
                                     OMX_BOOL *bHandled)
{
    *bHandled = OMX_TRUE;
    switch (nIndex)
    {
        default:
            *bHandled = OMX_FALSE;
            break;
    }

    return OMX_ErrorNone;
}

/*
   Default AcquireResources implementation. Does not do anything.
  */
static OMX_ERRORTYPE SampleAcquireResources(NVOMX_Component *pComp)
{
    return OMX_ErrorNone;
}

/*
   Default ReleaseResources implementation. Does not do anything.
  */
static OMX_ERRORTYPE SampleReleaseResources(NVOMX_Component *pComp)
{
    return OMX_ErrorNone;
}

/*
   Default DeInit implementation. Free the data allocated in the
   component init function here.
  */
static OMX_ERRORTYPE SampleDeInit(NVOMX_Component *pComp)
{
    SampleCompData *pSampleComp = (SampleCompData *)pComp->pComponentData;

    free(pSampleComp);
    pComp->pComponentData = NULL;

    return OMX_ErrorNone;
}

/*
   OMX Component init function. Register this function with 
   NVOMX_RegisterComponent for it to be loadable.
 */
OMX_API OMX_ERRORTYPE SampleComponentInit(OMX_IN OMX_HANDLETYPE hComponent);
OMX_API OMX_ERRORTYPE SampleComponentInit(OMX_IN OMX_HANDLETYPE hComponent)
{
    NVOMX_Component *pBase = NULL;
    SampleCompData *pData = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_AUDIO_CODINGTYPE oFormat = OMX_AUDIO_CodingPCM; 

    /* Allocate the SampleCompData struct here. */
    pData = malloc(sizeof(SampleCompData));
    if (!pData)
        return OMX_ErrorInsufficientResources;

    /* Create a component with two inputs named 'sample.component'. */
    eError = NVOMX_CreateComponent(hComponent, 2, "sample.component", &pBase);
    if (OMX_ErrorNone != eError)
    {
        free(pData);
        return eError;
    }

    /* Set the component's pComponentData to the SampleCompData struct we
       allocated earlier. */
    pBase->pComponentData = pData;

    /* Add a role, 'sample.role'. */
    NVOMX_AddRole(pBase, "sample.role");

    /* Setup one input port, with 10 buffers of 16384 bytes. It is
       audio domain, and 'AutoDetect' format. */
    NVOMX_InitPort(pBase, PORT_IN, OMX_DirInput, 10, 16384, 
                   OMX_PortDomainAudio, &oFormat);

    /* Setup one output port, with 10 buffers of 16384 bytes. It is
       audio domain, and 'AutoDetect' format. */
    NVOMX_InitPort(pBase, PORT_OUT, OMX_DirOutput, 10, 16384, 
                   OMX_PortDomainAudio, &oFormat);

    /* Set the component's function callbacks to the definitions above. */
    pBase->DeInit = SampleDeInit;
    pBase->GetParameter = SampleGetParameter;
    pBase->SetParameter = SampleSetParameter;
    pBase->GetConfig = SampleGetConfig;
    pBase->SetConfig = SampleSetConfig;
    pBase->AcquireResources = SampleAcquireResources;
    pBase->ReleaseResources = SampleReleaseResources;
    pBase->WorkerFunction = SampleWorkerFunction;

    return OMX_ErrorNone;
}

