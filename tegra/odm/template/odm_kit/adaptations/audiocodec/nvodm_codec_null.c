 /**
 * Copyright (c) 2007-2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * NVIDIA APX ODM Kit for NULL Implementation
 *
 *  This audio codec adaptation expects to find a discovery database entry
 *  in the following format:
 *
 * FIXME: NvOdmPeripheralConnectivity->AddressList should be set as NULL
 * as well as NvOdmPeripheralConnectivity->NumAddress should be set as 0,
 * But null entry will be filtered out by NvIsFilteredPeripheral() function. We 
 * fake one to work it around.
 * // Audio Codec (null)
 * {
 *      NV_ODM_GUID('n','u','l','a','u','d','e','o'),
 *      s_NullAduioCodecAddresses,
 *      1,
 *      NvOdmPeripheralClass_Other
 * },
 *
 * static const NvOdmIoAddress s_NullAduioCodecAddresses[] = 
 * {
 *     { NvOdmIoModule_I2c, 0x00, 0x00, 0},                
 * };
 */

#include "nvodm_services.h"
#include "nvodm_query.h"
#include "nvodm_audiocodec.h"
#include "audiocodec_hal.h"
#include "nvodm_query_discovery.h"

// Module debug: 0=disable, 1=enable
#define NVODMAUDIOCODECNULL_ENABLE_PRINTF (1)

#if NVODMAUDIOCODECNULL_ENABLE_PRINTF
    #define NVODMAUDIOCODECNULL_PRINTF(x)   NvOdmOsDebugPrintf x
#else
    #define NVODMAUDIOCODECNULL_PRINTF(x)   do {} while(0)
#endif

typedef struct NullAudioCodecRec
{
    NvOdmIoModule IoModule;
    NvU32   InstanceId;
    NvOdmQueryI2sACodecInterfaceProp WCodecInterface;
} NullAudioCodec, *NullAudioCodecHandle;

static NullAudioCodec s_nullaudiocodec;


static const NvOdmAudioPortCaps * ACNullGetPortCaps(void)
{
    static const NvOdmAudioPortCaps s_PortCaps =
        {
            0,    // MaxNumberOfInputPort;
            0     // MaxNumberOfOutputPort;
        };    
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:NullGetPortCaps\r\n"));        
    return &s_PortCaps;
}


static const NvOdmAudioOutputPortCaps * 
ACNullGetOutputPortCaps(
    NvU32 OutputPortId)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullGetOutputPortCaps\r\n"));
    return NULL;    
}


static const NvOdmAudioInputPortCaps * 
ACNullGetInputPortCaps(
    NvU32 InputPortId)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullGetInputPortCaps\r\n"));
    return NULL;    
}


static NvBool ACNullOpen(NvOdmPrivAudioCodecHandle hOdmAudioCodec,
                        const NvOdmQueryI2sACodecInterfaceProp *pI2sCodecInt,
                        void* hCodecNotifySem )
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullOpen\r\n"));
    return NV_TRUE;
}

static void ACNullClose(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullClose\r\n"));
}


static const NvOdmAudioWave * 
ACNullGetPortPcmCaps(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvU32 *pValidListCount)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullGetPortPcmCaps\r\n"));
    return NULL;
}

static void 
ACNullSetVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioVolume *pVolume,
    NvU32 ListCount)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullSetVolume\r\n"));
}

static void 
ACNullSetMuteControl(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioMuteData *pMuteParam,
    NvU32 ListCount)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullSetMuteControl\r\n"));
}

static NvBool 
ACNullSetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullSetConfiguration\r\n"));
    return NV_TRUE;
}

static void 
ACNullGetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullGetConfiguration\r\n"));
}

static void 
ACNullSetPowerState(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullSetPowerState\r\n"));
}

static void 
ACNullSetPowerMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioCodecPowerMode PowerMode)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullSetPowerMode\r\n"));
}

static void 
ACNullSetOperationMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvBool IsMaster)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:ACNullSetOperationMode\r\n"));
}

void NullInitCodecInterface(NvOdmAudioCodec *pCodecInterface)
{
    NVODMAUDIOCODECNULL_PRINTF(("AudioCodec:NullInitCodecInterface\r\n"));
    pCodecInterface->pfnGetPortCaps = ACNullGetPortCaps;
    pCodecInterface->pfnGetOutputPortCaps = ACNullGetOutputPortCaps;
    pCodecInterface->pfnGetInputPortCaps = ACNullGetInputPortCaps;
    pCodecInterface->pfnGetPortPcmCaps = ACNullGetPortPcmCaps;
    pCodecInterface->pfnOpen = ACNullOpen;
    pCodecInterface->pfnClose = ACNullClose;
    pCodecInterface->pfnSetVolume = ACNullSetVolume;
    pCodecInterface->pfnSetMuteControl = ACNullSetMuteControl;
    pCodecInterface->pfnSetConfiguration = ACNullSetConfiguration;
    pCodecInterface->pfnGetConfiguration = ACNullGetConfiguration;
    pCodecInterface->pfnSetPowerState = ACNullSetPowerState;
    pCodecInterface->pfnSetPowerMode = ACNullSetPowerMode;
    pCodecInterface->pfnSetOperationMode = ACNullSetOperationMode;
    pCodecInterface->hCodecPrivate = &s_nullaudiocodec;
    pCodecInterface->IsInited = NV_TRUE;
}

