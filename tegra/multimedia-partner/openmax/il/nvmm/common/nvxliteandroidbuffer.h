/* Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
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

OMX_ERRORTYPE HandleEnableANBLite(NvxComponent *pNvComp, OMX_U32 portallowed,
                                  void *ANBParam);
OMX_ERRORTYPE HandleUseANBLite(NvxComponent *pNvComp, void *ANBParam);
OMX_ERRORTYPE HandleUseNBHLite(NvxComponent *pNvComp, void *NBHParam);
OMX_ERRORTYPE HandleGetANBUsageLite(NvxComponent *pNvComp, void *ANBParam,
                                OMX_U32 SWAccess);

OMX_ERRORTYPE ImportAllANBsLite(NvxComponent *pNvComp,
                            SNvxNvMMLiteTransformData *pData,
                            OMX_U32 streamIndex,
                            OMX_U32 matchWidth,
                            OMX_U32 matchHeight);
OMX_ERRORTYPE ImportANBLite(NvxComponent *pNvComp,
                        SNvxNvMMLiteTransformData *pData,
                        OMX_U32 streamIndex,
                        OMX_BUFFERHEADERTYPE *buffer,
                        OMX_U32 matchWidth,
                        OMX_U32 matchHeight);
OMX_ERRORTYPE ExportANBLite(NvxComponent *pNvComp,
                        SNvxNvMMLiteTransformData *pData,
                        NvMMBuffer *nvmmbuf,
                        OMX_BUFFERHEADERTYPE **pOutBuf,
                        SNvxNvMMLitePort *pPort,
                        OMX_BOOL bFreeBacking);
OMX_ERRORTYPE FreeANBLite(NvxComponent *pNvComp,
                      SNvxNvMMLiteTransformData *pData,
                      OMX_U32 streamIndex,
                      OMX_BUFFERHEADERTYPE *buffer);
OMX_ERRORTYPE CopyNvxSurfaceToANBLite(NvxComponent *pNvComp,
                                  NvMMBuffer *nvmmbuf,
                                  OMX_BUFFERHEADERTYPE *buffer);
OMX_ERRORTYPE CopyANBToNvxSurfaceLite(NvxComponent *pNvComp,
                                  buffer_handle_t buffer,
                                  NvxSurface *pSrcSurface,
                                  OMX_BOOL bSkip2DBlit);
OMX_ERRORTYPE HandleStoreMetaDataInBuffersParamLite(void *pParam,
                                                SNvxNvMMLitePort *pPort,
                                                OMX_U32 PortAllowed);

#ifdef __cplusplus
}
#endif

#endif

