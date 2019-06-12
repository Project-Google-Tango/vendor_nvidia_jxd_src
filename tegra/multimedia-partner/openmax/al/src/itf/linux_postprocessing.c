/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "linux_mediaplayer.h"
#include "linux_cameradevice.h"
#include "linux_outputmix.h"

/* Media Player ImageControls Interface */
XAresult
ALBackendMediaPlayerSetBrightness(
    CMediaPlayer *pCMediaPlayer,
    XAuint32 Brightness)
{
    XAint32 BrightnessValue;
    GParamSpec *pParamSpec = NULL;
    GParamSpecInt *pIntegerParamSpec = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData)

    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    AL_CHK_ARG(pXAMediaPlayer->pVideoSink)

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pXAMediaPlayer->pVideoSink), "brightness");
    if (pParamSpec)
    {
        pIntegerParamSpec = (GParamSpecInt *)pParamSpec;
        // Translate to scale of 0 to 100
        BrightnessValue = pIntegerParamSpec->minimum + ((Brightness *
            (pIntegerParamSpec->maximum - pIntegerParamSpec->minimum)) / 100);
        g_object_set(pXAMediaPlayer->pVideoSink, "brightness",
            BrightnessValue, NULL);
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerGetBrightness(
    CMediaPlayer *pCMediaPlayer,
    XAuint32 *pBrightness)
{
    XAint32 Brightness;
    GParamSpec *pParamSpec = NULL;
    GParamSpecInt *pIntegerParamSpec = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData && pBrightness)

    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    AL_CHK_ARG(pXAMediaPlayer->pVideoSink)

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pXAMediaPlayer->pVideoSink), "brightness");
    if (pParamSpec)
    {
        pIntegerParamSpec = (GParamSpecInt *)pParamSpec;
        g_object_get(pXAMediaPlayer->pVideoSink,
            "brightness", &Brightness, NULL);
        Brightness = abs(Brightness - pIntegerParamSpec->minimum) * 100;
        *pBrightness = (XAuint32)(Brightness /
            abs(pIntegerParamSpec->maximum - pIntegerParamSpec->minimum));
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE
}


XAresult
ALBackendMediaPlayerSetContrast(
    CMediaPlayer *pCMediaPlayer,
    XAint32 Contrast)
{
    XAint32 ContrastValue;
    GParamSpec *pParamSpec = NULL;
    GParamSpecInt *pIntegerParamSpec = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData)

    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    AL_CHK_ARG(pXAMediaPlayer->pVideoSink)

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pXAMediaPlayer->pVideoSink), "contrast");
    if (pParamSpec)
    {
        pIntegerParamSpec = (GParamSpecInt *)pParamSpec;
        // Translate to scale of -100 to 100. Add 100 to normalize scale to 0
        ContrastValue = pIntegerParamSpec->minimum + (((Contrast + 100) *
            (pIntegerParamSpec->maximum - pIntegerParamSpec->minimum)) / 200);
        g_object_set(pXAMediaPlayer->pVideoSink, "contrast",
            ContrastValue, NULL);
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerGetContrast(
    CMediaPlayer *pCMediaPlayer,
    XAint32 *pContrast)
{
    XAint32 Contrast;
    GParamSpec *pParamSpec = NULL;
    GParamSpecInt *pIntegerParamSpec = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData && pContrast)

    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    AL_CHK_ARG(pXAMediaPlayer->pVideoSink)

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pXAMediaPlayer->pVideoSink), "contrast");
    if (pParamSpec)
    {
        pIntegerParamSpec = (GParamSpecInt *)pParamSpec;
        g_object_get(pXAMediaPlayer->pVideoSink,
            "contrast", &Contrast, NULL);
        Contrast = abs(Contrast - pIntegerParamSpec->minimum) * 200;
        *pContrast = (Contrast /
            abs(pIntegerParamSpec->maximum - pIntegerParamSpec->minimum));
        *pContrast -= 100; //shift scale from -100 to 100
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerSetGamma(
    CMediaPlayer *pCMediaPlayer,
    XApermille Gamma)
{
    GParamSpec *pParamSpec = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData)

    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    AL_CHK_ARG(pXAMediaPlayer->pVideoGamma)

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pXAMediaPlayer->pVideoGamma), "gamma");
    if (pParamSpec)
    {
        g_object_set(pXAMediaPlayer->pVideoGamma, "gamma",
            ((gdouble)Gamma / 1000), NULL);
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerGetGamma(
    CMediaPlayer *pCMediaPlayer,
    XApermille *pGamma)
{
    gdouble Gamma;
    GParamSpec *pParamSpec = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData && pGamma)

    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    AL_CHK_ARG(pXAMediaPlayer->pVideoGamma)

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pXAMediaPlayer->pVideoGamma), "gamma");
    if (pParamSpec)
    {
        g_object_get(pXAMediaPlayer->pVideoGamma, "gamma",
            &Gamma, NULL);
        *pGamma = (XApermille)(Gamma * 1000);
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE
}

XAresult
ALBackendMediaPlayerGetSupportedGammaSettings(
    CMediaPlayer *pCMediaPlayer,
    XApermille *pMinValue,
    XApermille *pMaxValue)
{
    GParamSpec *pParamSpec = NULL;
    GParamSpecDouble *pDoubleParamSpec = NULL;
    XAMediaPlayer *pXAMediaPlayer = NULL;

    XA_ENTER_INTERFACE
    AL_CHK_ARG(pCMediaPlayer && pCMediaPlayer->pData)
    AL_CHK_ARG(pMinValue && pMaxValue)

    pXAMediaPlayer = (XAMediaPlayer *)pCMediaPlayer->pData;
    AL_CHK_ARG(pXAMediaPlayer->pVideoGamma)

    pParamSpec = g_object_class_find_property(
        G_OBJECT_GET_CLASS(pXAMediaPlayer->pVideoGamma), "gamma");
    if (pParamSpec)
    {
        pDoubleParamSpec = (GParamSpecDouble *)pParamSpec;
        *pMinValue = (XApermille)(pDoubleParamSpec->minimum * 1000);
        *pMaxValue = (XApermille)(pDoubleParamSpec->maximum * 1000);
    }
    else
    {
        result = XA_RESULT_FEATURE_UNSUPPORTED;
    }

    XA_LEAVE_INTERFACE
}

