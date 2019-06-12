/*
 * Copyright (c) 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/** NvxVideoPostProcess.c */

/* =========================================================================
 *                     I N C L U D E S
 * ========================================================================= */

#include <OMX_Core.h>
#include <nvassert.h>
#include <nvos.h>
#include <NvxTrace.h>
#include <NvxComponent.h>
#include <NvxPort.h>
#include <NvxCompReg.h>
#include <nvmmlitetransformbase.h>
#include <NvxHelpers.h>
#include <nvxlitevideopostprocess.h>
#include "nvxlitevpp.h"

#include "NvxHelpers.h"
#include "nvxliteandroidbuffer.h"
#include "vblt.h"

#if defined(__ARM_NEON__)
#include <arm_neon.h>
#endif // __ARM_NEON__

NvError NvVPPEGLBasedInit(SNvxNvMMLiteTransformData* pData, void *cookie)
{
    NvError status = NvSuccess;
    NvU32 *ctxt = (NvU32*)cookie;
    vblt_t *blt = vblt_create();
    NvOsMemcpy(ctxt, &blt, 4);
    return status;
}

NvError NvVPPEGLBasedDestroy(SNvxNvMMLiteTransformData* pData, void *cookie)
{
    NvError status = NvSuccess;
    NvU32* ctxt = (NvU32*)cookie;
    vblt_destroy((vblt_t*)*ctxt);
    return status;
}

NvError NvVPPApplyEGLBasedEffect(SNvxNvMMLiteTransformData* pData, NvMMBuffer* pDecSurf, NvMMBuffer* output, void *cookie)
{
    NvError status = NvSuccess;
    NvU32* ctxt = (NvU32*)cookie;
    vblt_blit((vblt_t*)*ctxt,
              pDecSurf,
              output,
              (hwc_rect_t*)&(pDecSurf->Payload.Surfaces.CropRect),
              0,
              NV_FALSE);
    return status;
}
