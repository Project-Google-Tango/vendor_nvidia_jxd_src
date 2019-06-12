/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvodm_audiocodec.h"
#include "audiocodec_hal.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"

#define INVALID_HANDLE ((NvOdmAudioCodecHandle)0)
#define HANDLE(x) (NvOdmAudioCodecHandle)(((x)&0x7fffffffUL) | 0x80000000UL)
#define IS_VALID_HANDLE(x) ( ((NvU32)(x)) & 0x80000000UL )
#define HANDLE_INDEX(x) (((NvU32)(x)) & 0x7fffffffUL)

static const NvU64 AudioCodecGuidList[] = {
    WOLFSON_8350_CODEC_GUID,   // 1
    WOLFSON_8731_CODEC_GUID,   // 2
    WOLFSON_8750_CODEC_GUID,   // 3
    WOLFSON_8753_CODEC_GUID,   // 4
    WOLFSON_8903_CODEC_GUID,   // 5
    WOLFSON_8961_CODEC_GUID,   // 6
    WOLFSON_8994_CODEC_GUID,   // 7
    MAXIM_9888_CODEC_GUID,     // 8
    NULL_CODEC_GUID,           // 9
};

// Currently supporting only one codec.
#define NUM_CODECS 1
NvOdmAudioCodec Codecs[NUM_CODECS];
static NvBool s_Codecinit  = NV_FALSE;
static NvU32  s_CodecIndex = 0;

static NvU64
GetCodecGuidByInstance(NvU32 InstanceId)
{

    NvU32 index = 0;

    NV_ASSERT(InstanceId < NV_ARRAY_SIZE(AudioCodecGuidList));

    if (!s_Codecinit)
    {
        NvU32 CodecCount = 0;
        NvOdmOsMemset(Codecs, 0, sizeof(Codecs));
        CodecCount = NV_ARRAY_SIZE(AudioCodecGuidList);
        // Find the codec through searching the peripherals
        for (index=0; index < CodecCount; index++)
        {
            // Get the peripheral connectivity information
            if (NvOdmPeripheralGetGuid(AudioCodecGuidList[index]))    
            {
                // currently supporting only one codec - 
                // can support more if needed later
                s_CodecIndex = index;
                s_Codecinit = NV_TRUE;
                //NvOdmOsDebugPrintf("Found a device for index 0x%08x\n", index);
                break;
            }
        }
    }

    if (s_Codecinit) 
        return AudioCodecGuidList[s_CodecIndex];
    else 
        return 0;
}

static NvOdmAudioCodec*
GetCodecByPseudoHandle(NvOdmAudioCodecHandle PseudoHandle)
{
    if (!s_Codecinit)
    {
        GetCodecGuidByInstance(0);
    }

    if (IS_VALID_HANDLE(PseudoHandle) &&
        (HANDLE_INDEX(PseudoHandle)<NUM_CODECS))
        return &Codecs[HANDLE_INDEX(PseudoHandle)];

    return NULL;
}

static NvOdmAudioCodec*
GetCodecByGuid(NvU64 OdmGuid)
{
    NvOdmAudioCodec *codec = NULL;
    codec = GetCodecByPseudoHandle(HANDLE(0));
    if (!codec->IsInited)
    {
        switch (OdmGuid) 
        {
            /*case WOLFSON_8350_CODEC_GUID:
                W8350InitCodecInterface(codec);
                break;
            case WOLFSON_8731_CODEC_GUID:
                W8731InitCodecInterface(codec);
                break;
            case WOLFSON_8750_CODEC_GUID:
                W8750InitCodecInterface(codec);
                break;
            */
            case WOLFSON_8753_CODEC_GUID:
                W8753InitCodecInterface(codec);
                break;
            case WOLFSON_8903_CODEC_GUID:
                W8903InitCodecInterface(codec);
                break;
            case WOLFSON_8961_CODEC_GUID:
                W8961InitCodecInterface(codec);
                break;
            case WOLFSON_8994_CODEC_GUID:
                W8994InitCodecInterface(codec);
                break;
            case MAXIM_9888_CODEC_GUID:
                M9888InitCodecInterface(codec);
                break;
            case NULL_CODEC_GUID:
                NullInitCodecInterface(codec);
                break;
        }
    }
    return codec;
}

const NvOdmAudioPortCaps*
NvOdmAudioCodecGetPortCaps(NvU32 InstanceId)
{
    NvOdmAudioCodec *codec = GetCodecByGuid(GetCodecGuidByInstance(InstanceId));

    if (codec && codec->pfnGetPortCaps)
        return codec->pfnGetPortCaps();

    return NULL;
}

const NvOdmAudioOutputPortCaps* 
NvOdmAudioCodecGetOutputPortCaps(
    NvU32 InstanceId,
    NvU32 OutputPortId)
{
    NvOdmAudioCodec *codec = GetCodecByGuid(GetCodecGuidByInstance(InstanceId));

    if (codec && codec->pfnGetOutputPortCaps)
        return codec->pfnGetOutputPortCaps(OutputPortId);

    return NULL;
}

const NvOdmAudioInputPortCaps* 
NvOdmAudioCodecGetInputPortCaps(
    NvU32 InstanceId,
    NvU32 InputPortId)
{
    NvOdmAudioCodec *codec = GetCodecByGuid(GetCodecGuidByInstance(InstanceId));

    if (codec && codec->pfnGetInputPortCaps)
        return codec->pfnGetInputPortCaps(InputPortId);

    return NULL;
}

NvOdmAudioCodecHandle
NvOdmAudioCodecOpen(NvU32 InstanceId, void* hCodecNotifySem)
{
    NvOdmAudioCodec *codec = GetCodecByGuid(GetCodecGuidByInstance(InstanceId));
    const NvOdmQueryI2sACodecInterfaceProp *pI2sCodecInt = NvOdmQueryGetI2sACodecInterfaceProperty(InstanceId);
    
    if (codec && codec->pfnOpen && pI2sCodecInt)
    {
        NvBool IsSuccess = codec->pfnOpen(codec->hCodecPrivate, pI2sCodecInt, hCodecNotifySem);
        if (IsSuccess)
            return HANDLE(InstanceId);
    }

    return INVALID_HANDLE;
}


void
NvOdmAudioCodecClose(NvOdmAudioCodecHandle hAudioCodec)
{
    NvOdmAudioCodec *codec = GetCodecByPseudoHandle(hAudioCodec);

    if (codec && codec->pfnClose)
        codec->pfnClose(codec->hCodecPrivate);
}

const NvOdmAudioWave* 
NvOdmAudioCodecGetPortPcmCaps(
    NvOdmAudioCodecHandle hAudioCodec, 
    NvOdmAudioPortName PortName,
    NvU32 *pValidListCount)
{
    NvOdmAudioCodec *codec = GetCodecByPseudoHandle(hAudioCodec);

    if (codec && codec->pfnGetPortPcmCaps)
        return codec->pfnGetPortPcmCaps(codec->hCodecPrivate, PortName, pValidListCount);

    return NULL;

}

NvBool 
NvOdmAudioCodecSetPortPcmProps(
    NvOdmAudioCodecHandle hAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioWave *pWaveParams)
{
    NvOdmAudioCodec *codec = GetCodecByPseudoHandle(hAudioCodec);

    if (codec && codec->pfnSetPortPcmProps)
        return codec->pfnSetPortPcmProps(codec->hCodecPrivate, PortName, pWaveParams);

    return NV_FALSE;
}

void 
NvOdmAudioCodecSetVolume(
    NvOdmAudioCodecHandle hAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioVolume *pVolume,
    NvU32 ListCount)
{
    NvOdmAudioCodec *codec = GetCodecByPseudoHandle(hAudioCodec);

    if (codec && codec->pfnSetVolume)
        codec->pfnSetVolume(codec->hCodecPrivate, PortName, pVolume, ListCount);
}

 void 
 NvOdmAudioCodecSetMuteControl(
    NvOdmAudioCodecHandle hAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioMuteData *pMuteParam,
    NvU32 ListCount)
 {
     NvOdmAudioCodec *codec = GetCodecByPseudoHandle(hAudioCodec);

     if (codec && codec->pfnSetMuteControl)
         codec->pfnSetMuteControl(codec->hCodecPrivate, PortName, pMuteParam, ListCount);
 }
        
 NvBool 
 NvOdmAudioCodecSetConfiguration(
    NvOdmAudioCodecHandle hAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
 {
     NvOdmAudioCodec *codec = GetCodecByPseudoHandle(hAudioCodec);

     if (codec && codec->pfnSetConfiguration)
         return codec->pfnSetConfiguration(codec->hCodecPrivate, PortName, ConfigType, pConfigData);

     return NV_FALSE;
 }

void 
 NvOdmAudioCodecGetConfiguration(
    NvOdmAudioCodecHandle hAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
    NvOdmAudioCodec *codec = GetCodecByPseudoHandle(hAudioCodec);

    if (codec && codec->pfnGetConfiguration)
        codec->pfnGetConfiguration(codec->hCodecPrivate, PortName, ConfigType, pConfigData);

}

void 
NvOdmAudioCodecSetPowerState(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    NvOdmAudioCodec *codec = GetCodecByPseudoHandle(hAudioCodec);

    if (codec && codec->pfnSetPowerState)
        codec->pfnSetPowerState(codec->hCodecPrivate, PortName, SignalType, SignalId, IsPowerOn);
}

void 
NvOdmAudioCodecSetPowerMode(
    NvOdmAudioCodecHandle hAudioCodec, 
    NvOdmAudioCodecPowerMode PowerMode)
{
    NvOdmAudioCodec *codec = GetCodecByPseudoHandle(hAudioCodec);

    if (codec && codec->pfnSetPowerMode)
        codec->pfnSetPowerMode(codec->hCodecPrivate, PowerMode);
}

void
NvOdmAudioCodecSetOperationMode(
    NvOdmAudioCodecHandle hAudioCodec,
    NvOdmAudioPortName PortName,
    NvBool IsMaster)
{
    NvOdmAudioCodec *codec = GetCodecByPseudoHandle(hAudioCodec);

    if (codec && codec->pfnSetOperationMode)
        codec->pfnSetOperationMode(codec->hCodecPrivate, PortName, IsMaster);
}
