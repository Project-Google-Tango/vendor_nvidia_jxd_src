/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "enc_T2.h"

static NvError Encoder_Open(NvImageEncHandle hImageEnc);
static void Encoder_Close(NvImageEncHandle hImageEnc);
static NvError Encoder_GetParameters(NvImageEncHandle hImageEnc,
                    NvJpegEncParameters *params);
static NvError Encoder_SetParameters(NvImageEncHandle hImageEnc,
                    NvJpegEncParameters *params);
static NvError Encoder_Start(NvImageEncHandle hImageEnc,
                    NvMMBuffer *InputBuffer,
                    NvMMBuffer *OutPutBuffer,
                    NvBool IsThumbnail);

static NvError Encoder_Open(
    NvImageEncHandle hImageEnc)
{
    /* Stub */
    return NvSuccess;
};

static void Encoder_Close(
    NvImageEncHandle hImageEnc)
{
    /* Stub */
};

static NvError Encoder_GetParameters(
    NvImageEncHandle hImageEnc,
    NvJpegEncParameters *params)
{
    /* Stub */
    return NvSuccess;
};

static NvError Encoder_SetParameters(
    NvImageEncHandle hImageEnc,
    NvJpegEncParameters *params)
{
    /* Stub */
    return NvSuccess;
};

static NvError Encoder_Start(
    NvImageEncHandle hImageEnc,
    NvMMBuffer *InputBuffer,
    NvMMBuffer *OutPutBuffer,
    NvBool IsThumbnail)
{
    /* Stub */
    return NvSuccess;
};


/**
 * return the functions for this particular encoder
 */
NvBool ImgEnc_Tegra2(NvImageEncHandle pContext)
{
    pContext->pEncoder->pfnOpen = Encoder_Open;
    pContext->pEncoder->pfnClose = Encoder_Close;
    pContext->pEncoder->pfnGetParam = Encoder_GetParameters;
    pContext->pEncoder->pfnSetParam  = Encoder_SetParameters;
    pContext->pEncoder->pfnStartEncoding = Encoder_Start;

    return NV_TRUE;
}
