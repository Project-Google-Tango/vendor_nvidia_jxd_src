/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVXANDROID_BUFFER_H_
#define NVXANDROID_BUFFER_H_

#include "NvxHelpers.h"
#include <hardware/gralloc.h>


#ifdef __cplusplus
extern "C" {
#endif

#include <NVOMX_IndexExtensions.h>

OMX_ERRORTYPE HandleEnableANB(NvxComponent *pNvComp, OMX_U32 portallowed,
                              void *ANBParam);
OMX_ERRORTYPE HandleUseANB(NvxComponent *pNvComp, void *ANBParam);
OMX_ERRORTYPE HandleUseNBH(NvxComponent *pNvComp, void *NBHParam);
OMX_ERRORTYPE HandleGetANBUsage(NvxComponent *pNvComp, void *ANBParam,
                                OMX_U32 SWAccess);

OMX_ERRORTYPE ImportAllANBs(NvxComponent *pNvComp,
                            SNvxNvMMTransformData *pData,
                            OMX_U32 streamIndex,
                            OMX_U32 matchWidth,
                            OMX_U32 matchHeight);
OMX_ERRORTYPE ImportANB(NvxComponent *pNvComp,
                        SNvxNvMMTransformData *pData,
                        OMX_U32 streamIndex,
                        OMX_BUFFERHEADERTYPE *buffer,
                        OMX_U32 matchWidth,
                        OMX_U32 matchHeight);
OMX_ERRORTYPE ExportANB(NvxComponent *pNvComp,
                        NvMMBuffer *nvmmbuf,
                        OMX_BUFFERHEADERTYPE **pOutBuf,
                        NvxPort *pPortOut,
                        OMX_BOOL bFreeBacking);
OMX_ERRORTYPE FreeANB(NvxComponent *pNvComp,
                      SNvxNvMMTransformData *pData,
                      OMX_U32 streamIndex,
                      OMX_BUFFERHEADERTYPE *buffer);
OMX_ERRORTYPE CopyNvxSurfaceToANB(NvxComponent *pNvComp,
                                  NvMMBuffer *nvmmbuf,
                                  OMX_BUFFERHEADERTYPE *buffer);

// For Gralloc-based metadata buffers anb handle may not directly come in omxbuffer.
// Hence passing anb handle as void pointer.

OMX_ERRORTYPE CopyANBToNvxSurface(NvxComponent *pNvComp,
                                  buffer_handle_t buffer,
                                  NvxSurface *pSrcSurface);
OMX_ERRORTYPE HandleStoreMetaDataInBuffersParam(void *pParam,
                                                SNvxNvMMPort *pPort,
                                                OMX_U32 PortAllowed);

#ifdef __cplusplus
}
#endif

#endif
