/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Abstraction layer stub for audio codec adaptations</b>
 */

#ifndef INCLUDED_NVODM_AUDIOCODEC_ADAPTATION_HAL_H
#define INCLUDED_NVODM_AUDIOCODEC_ADAPTATION_HAL_H

#include "nvcommon.h"
#include "nvodm_audiocodec.h"
#include "nvodm_query.h"

#define WOLFSON_8350_CODEC_GUID NV_ODM_GUID('w','o','l','f','8','3','5','0')
#define WOLFSON_8731_CODEC_GUID NV_ODM_GUID('w','o','l','f','8','7','3','1')
#define WOLFSON_8750_CODEC_GUID NV_ODM_GUID('w','o','l','f','8','7','5','0')
#define WOLFSON_8753_CODEC_GUID NV_ODM_GUID('w','o','l','f','8','7','5','3')
#define WOLFSON_8903_CODEC_GUID NV_ODM_GUID('w','o','l','f','8','9','0','3')
#define WOLFSON_8903_I2C_1_CODEC_GUID NV_ODM_GUID('w','o','8','9','0','3','_','1')
#define WOLFSON_8961_CODEC_GUID NV_ODM_GUID('w','o','l','f','8','9','6','1')
#define WOLFSON_8994_CODEC_GUID NV_ODM_GUID('w','o','l','f','8','9','9','4')
#define MAXIM_9888_CODEC_GUID NV_ODM_GUID('m','a','x','m','9','8','8','8')
#define NULL_CODEC_GUID NV_ODM_GUID('n','u','l','a','u','d','e','o')

#ifdef __cplusplus
extern "C"
{
#endif

typedef void *NvOdmPrivAudioCodecHandle;

typedef NvBool (*pfnAudioOpen)(NvOdmPrivAudioCodecHandle, const NvOdmQueryI2sACodecInterfaceProp *, void* hCodecNotifySem);
typedef void (*pfnAudioClose)(NvOdmPrivAudioCodecHandle);
typedef const NvOdmAudioPortCaps* (*pfnAudioGetPortCaps)(void);
typedef const NvOdmAudioOutputPortCaps* (*pfnAudioGetOutputPortCaps)(NvU32);
typedef const NvOdmAudioInputPortCaps* (*pfnAudioGetInputPortCaps)(NvU32);
typedef const NvOdmAudioWave* (*pfnAudioGetPortPcmCaps)(NvOdmPrivAudioCodecHandle, NvOdmAudioPortName, NvU32*);
typedef NvBool (*pfnAudioSetPortPcmProps)(NvOdmPrivAudioCodecHandle, NvOdmAudioPortName, NvOdmAudioWave*);
typedef void (*pfnAudioSetVolume)(NvOdmPrivAudioCodecHandle, NvOdmAudioPortName, NvOdmAudioVolume*, NvU32);
typedef void (*pfnAudioSetMuteControl)(NvOdmPrivAudioCodecHandle, NvOdmAudioPortName, NvOdmAudioMuteData*, NvU32);
typedef NvBool (*pfnAudioSetConfiguration)(NvOdmPrivAudioCodecHandle, NvOdmAudioPortName, NvOdmAudioConfigure, void*);
typedef void (*pfnAudioGetConfiguration)(NvOdmPrivAudioCodecHandle, NvOdmAudioPortName, NvOdmAudioConfigure, void*);
typedef void (*pfnAudioSetPowerState)(NvOdmPrivAudioCodecHandle, NvOdmAudioPortName, NvOdmAudioSignalType, NvU32, NvBool);
typedef void (*pfnAudioSetPowerMode)(NvOdmPrivAudioCodecHandle, NvOdmAudioCodecPowerMode);
typedef void (*pfnAudioSetOperationMode)(NvOdmPrivAudioCodecHandle, NvOdmAudioPortName, NvBool IsMaster);

typedef struct NvOdmAudioCodecRec
{
    pfnAudioOpen              pfnOpen;
    pfnAudioClose             pfnClose;
    pfnAudioGetPortCaps       pfnGetPortCaps;
    pfnAudioGetOutputPortCaps pfnGetOutputPortCaps;
    pfnAudioGetInputPortCaps  pfnGetInputPortCaps;
    pfnAudioGetPortPcmCaps    pfnGetPortPcmCaps;
    pfnAudioSetPortPcmProps   pfnSetPortPcmProps;
    pfnAudioSetVolume         pfnSetVolume;
    pfnAudioSetMuteControl    pfnSetMuteControl;
    pfnAudioSetConfiguration  pfnSetConfiguration;
    pfnAudioGetConfiguration  pfnGetConfiguration;
    pfnAudioSetPowerState     pfnSetPowerState;
    pfnAudioSetPowerMode      pfnSetPowerMode;
    pfnAudioSetOperationMode  pfnSetOperationMode;

    NvOdmPrivAudioCodecHandle hCodecPrivate;
    NvBool                    IsInited;
} NvOdmAudioCodec;

void W8903InitCodecInterface(NvOdmAudioCodec *pCodec);
//void W8350InitCodecInterface(NvOdmAudioCodec *pCodec);
//void W8731InitCodecInterface(NvOdmAudioCodec *pCodec);
//void W8750InitCodecInterface(NvOdmAudioCodec *pCodec);
void W8753InitCodecInterface(NvOdmAudioCodec *pCodec);
void W8961InitCodecInterface(NvOdmAudioCodec *pCodec);
void W8994InitCodecInterface(NvOdmAudioCodec *pCodec);
void M9888InitCodecInterface(NvOdmAudioCodec *pCodec);
void NullInitCodecInterface(NvOdmAudioCodec *pCodec);

#ifdef __cplusplus
}
#endif

#endif
