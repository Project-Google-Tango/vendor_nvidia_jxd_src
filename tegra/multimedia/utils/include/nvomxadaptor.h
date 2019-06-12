/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef nvomxadaptor_h
#define nvomxadaptor_h

#include <OMX_Index.h>
#include <OMX_Core.h>

struct BufferReg;

class INVOMXAdaptor {

public:

    INVOMXAdaptor() { }

    virtual ~INVOMXAdaptor() {}

    virtual OMX_ERRORTYPE ComponentNameEnum(
        OMX_OUT OMX_STRING cComponentName,
        OMX_IN OMX_U32 nNameLength,
        OMX_IN OMX_U32 nIndex) = 0;

    virtual OMX_ERRORTYPE GetHandle(
        OMX_OUT OMX_HANDLETYPE* pHandle,
        OMX_IN OMX_STRING cComponentName,
        OMX_IN OMX_PTR pAppData,
        OMX_IN OMX_CALLBACKTYPE* pCallBacks) = 0;

    virtual OMX_ERRORTYPE FreeHandle(OMX_IN OMX_HANDLETYPE hComponent) = 0;

    virtual OMX_ERRORTYPE GetComponentsOfRole(
        OMX_IN OMX_STRING role,
        OMX_INOUT OMX_U32 *pNumComps,
        OMX_INOUT OMX_U8 **compNames) = 0;

    virtual OMX_ERRORTYPE GetRolesOfComponent(
        OMX_IN OMX_STRING compName,
        OMX_INOUT OMX_U32 *pNumRoles,
        OMX_OUT OMX_U8 **roles) = 0;

    virtual OMX_ERRORTYPE SendCommand(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_COMMANDTYPE Cmd,
        OMX_IN OMX_U32 nParam1) = 0;

    virtual OMX_ERRORTYPE GetParameter(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_INDEXTYPE nParamIndex,
        OMX_INOUT OMX_PTR pComponentParameterStructure,
        OMX_IN OMX_U32 nSizeBytes) = 0;

    virtual OMX_ERRORTYPE SetParameter(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_INDEXTYPE nIndex,
        OMX_IN OMX_PTR pComponentParameterStructure,
        OMX_IN OMX_U32 nSizeBytes) = 0;

    virtual OMX_ERRORTYPE GetConfig(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_INDEXTYPE nIndex,
        OMX_INOUT OMX_PTR pComponentConfigStructure,
        OMX_IN OMX_U32 nSizeBytes) = 0;

    virtual OMX_ERRORTYPE SetConfig(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_INDEXTYPE nIndex,
        OMX_IN OMX_PTR pComponentConfigStructure,
        OMX_IN OMX_U32 nSizeBytes) = 0;

    virtual OMX_ERRORTYPE GetExtensionIndex(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_CSTRING cParameterName,
        OMX_OUT OMX_INDEXTYPE* pIndexType) = 0;

    virtual OMX_ERRORTYPE EnableNativeBuffers(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_U32 nPortIndex,
        OMX_BOOL enable) = 0;

    virtual OMX_ERRORTYPE UseNativeBuffer(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes,
        OMX_IN OMX_U8* pBuffer) = 0;

    virtual OMX_ERRORTYPE AllocateBuffer(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_PTR pAppPrivate,
        OMX_IN OMX_U32 nSizeBytes) = 0;

    virtual OMX_ERRORTYPE FreeBuffer(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_U32 nPortIndex,
        OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) = 0;

    virtual OMX_ERRORTYPE EmptyThisBuffer(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) = 0;

    virtual OMX_ERRORTYPE FillThisBuffer(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) = 0;

    virtual OMX_ERRORTYPE GetIOMXBufferInfo(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_IN OMX_BUFFERHEADERTYPE *pOMXBuffHdr,
            OMX_OUT BufferReg *bufInfo) = 0;
};

// the types of the class factories
typedef INVOMXAdaptor* create_t();
typedef void destroy_t(INVOMXAdaptor*);

#endif // nvomxadaptor_h

