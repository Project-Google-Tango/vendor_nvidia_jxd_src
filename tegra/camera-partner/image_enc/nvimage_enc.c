/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvassert.h"
#include "nvimage_enc.h"
#include "nvimage_enc_pvt.h"
#include "nvimage_enc_exifheader.h"

#include "enc_T3.h"
#include "enc_T2.h"
#include "enc_sw.h"

NvError
NvImageEnc_Open(
    NvImageEncHandle *hImageEnc,
    NvImageEncOpenParameters *pOpenParams)
{
    NvImageEncHandle pCtx;
    NvError status = NvSuccess;
    pfnImageEncSelect pfnGetEnc = NULL;

    pCtx = NvOsAlloc(sizeof(NvImageEnc));
    if (!pCtx)
    {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }
    NvOsMemset(pCtx, 0, sizeof(NvImageEnc));

    pCtx->pEncoder = NvOsAlloc(sizeof(NvEncoderPrivate));
    if (!pCtx->pEncoder)
    {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }
    NvOsMemset(pCtx->pEncoder, 0, sizeof(NvEncoderPrivate));

    pCtx->pEncoder->SupportLevel = pOpenParams->LevelOfSupport;
    pCtx->pEncoder->HeaderParams.lenSubjectArea = DEFAULT_SUBJECT_AREA_LENGTH;

    pCtx->ClientCB = pOpenParams->ClientIOBufferCB;
    pCtx->pClientContext = pOpenParams->pContext;

    switch (pOpenParams->Type)
    {
        case NvImageEncoderType_HwEncoder:
#if USE_NVMM_ENC
            pfnGetEnc = ImgEnc_Tegra2;
#else
            pfnGetEnc = ImgEnc_Tegra3;
#endif
            break;

        case NvImageEncoderType_SwEncoder:
            pfnGetEnc = ImgEnc_swEnc;
            break;

        case NvImageEncoderType_Unknown:
        default:
            status = NvError_NotSupported;
            goto cleanup;
    }

    pfnGetEnc(pCtx);

    status = pCtx->pEncoder->pfnOpen(pCtx);
    if (status != NvSuccess)
    {
        status = NvError_InsufficientMemory;
        goto cleanup;
    }

    *hImageEnc = pCtx;
    return NvSuccess;

cleanup:
    if (pCtx)
    {
        if (pCtx->pEncoder)
        {
            NvOsFree(pCtx->pEncoder);
            pCtx->pEncoder = NULL;
        }

        NvOsFree(pCtx);
        pCtx = NULL;
    }

    return status;
}

NvError
NvImageEnc_SetParam(
    NvImageEncHandle hImageEnc,
    NvJpegEncParameters *params)
{
    if (hImageEnc == NULL)
        return NvError_BadParameter;

    return hImageEnc->pEncoder->pfnSetParam(hImageEnc, params);
}

NvError
NvImageEnc_GetParam(
    NvImageEncHandle hImageEnc,
    NvJpegEncParameters *params)
{
    if (hImageEnc == NULL)
        return NvError_BadParameter;

    return hImageEnc->pEncoder->pfnGetParam(hImageEnc, params);
}

NvError
NvImageEnc_Start(
    NvImageEncHandle hImageEnc,
    NvMMBuffer *InputBuffer,
    NvMMBuffer *OutPutBuffer,
    NvBool  IsThumbnail)
{
    if (hImageEnc == NULL)
        return NvError_BadParameter;

    return hImageEnc->pEncoder->pfnStartEncoding(hImageEnc,
                InputBuffer, OutPutBuffer, IsThumbnail);
}

void
NvImageEnc_Close(
    NvImageEncHandle hImageEnc)
{
    if (hImageEnc == NULL)
        return;

    hImageEnc->pEncoder->pfnClose(hImageEnc);

    if (hImageEnc->pEncoder)
    {
        NvOsFree(hImageEnc->pEncoder);
        hImageEnc->pEncoder = NULL;
    }

    NvOsFree(hImageEnc);
    hImageEnc = NULL;
}
