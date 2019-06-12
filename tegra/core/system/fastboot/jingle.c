/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "fastboot.h"
#include "nvddk_i2s.h"
#include "nvodm_audiocodec.h"
#include "nverror.h"
#include "nvrm_pinmux.h"
#include "nvassert.h"
#include "nvodm_query.h"
#include "nvodm_services.h"

#define BUFFER_SIZE 4096
#define NUM_BUFFERS 2

typedef struct FastbootWaveRec
{
    NvRmDeviceHandle      hRm;
    NvDdkI2sHandle        hI2s;
    NvOdmAudioCodecHandle hCodec;
    NvOsThreadHandle      hPlayThread;
    NvOsMutexHandle       hShutdownMutex;
    NvOsSemaphoreHandle   Trigger;
    NvU32                 DapIndex;
    NvU32                 BufferId;
    NvU32                 BufferTimeoutMs;
    GetMoreBitsCallback   GetMoreBits;
    NvU8*                 pBuff[NUM_BUFFERS];
    NvRmPhysAddr          BuffPhys[NUM_BUFFERS];
    NvRmMemHandle         hBuffMem;
    volatile NvBool       Shutdown;
} FastbootWave;

static void PlayWaveThread(void *pArgs)
{
    FastbootWave *pWave = (FastbootWave *)pArgs;
    NvU32   LastBits=BUFFER_SIZE, Written;
    NvError e;

    while (!pWave->Shutdown && LastBits)
    {
        Written = LastBits;
        NV_CHECK_ERROR_CLEANUP(
            NvDdkI2sWrite(pWave->hI2s, pWave->BuffPhys[pWave->BufferId],
                &Written, 0, pWave->Trigger)
        );
        pWave->BufferId++;
        if (pWave->BufferId >= NUM_BUFFERS)
            pWave->BufferId = 0;
        LastBits = (*pWave->GetMoreBits)(pWave->pBuff[pWave->BufferId],
            BUFFER_SIZE);
        NV_CHECK_ERROR_CLEANUP(
            NvOsSemaphoreWaitTimeout(pWave->Trigger, pWave->BufferTimeoutMs)
        );
        if (Written != BUFFER_SIZE)
        {
            e = NvError_InvalidSize;
            goto fail;
        }
    }

 fail:
    /* The codec is shut down immediately when audio stops, to avoid replaying
     * the last buffer in the case that the jingle ends before the bootloader
     * asks to shut down the audio system */
    if(e != NvSuccess)
    {
        NvOsMutexLock(pWave->hShutdownMutex);
        NvDdkI2sWriteAbort(pWave->hI2s);

        NvOdmAudioCodecSetPowerState(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioSignalType_HeadphoneOut, 0, NV_FALSE);
        NvOdmAudioCodecSetPowerState(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioSignalType_LineOut, 0, NV_FALSE);
        NvOdmAudioCodecSetPowerState(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioSignalType_Speakers, 0, NV_FALSE);

        NvOdmAudioCodecClose(pWave->hCodec);
        NvDdkI2sClose(pWave->hI2s);
        NvOsMutexUnlock(pWave->hShutdownMutex);
    }
}

void FastbootCloseWave(FastbootWaveHandle pWave)
{
    if (!pWave)
        return;
    if (pWave->hShutdownMutex)
    {
        NvOsMutexLock(pWave->hShutdownMutex);
        if (pWave->hPlayThread)
        {
            pWave->Shutdown = NV_TRUE;
            NvOsMutexUnlock(pWave->hShutdownMutex);
            NvOsSemaphoreSignal(pWave->Trigger);
            NvOsThreadJoin(pWave->hPlayThread);
        }
        else
        {
            NvOdmAudioCodecSetPowerState(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioSignalType_HeadphoneOut, 0, NV_FALSE);
            NvOdmAudioCodecSetPowerState(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioSignalType_LineOut, 0, NV_FALSE);
            NvOdmAudioCodecSetPowerState(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioSignalType_Speakers, 0, NV_FALSE);

            NvOdmAudioCodecClose(pWave->hCodec);
            NvDdkI2sClose(pWave->hI2s);
            NvOsMutexUnlock(pWave->hShutdownMutex);
        }
        NvOsMutexDestroy(pWave->hShutdownMutex);
    }

    NvOsSemaphoreDestroy(pWave->Trigger);
    if (pWave->pBuff[0] && pWave->BuffPhys[0])
        NvRmMemUnmap(pWave->hBuffMem, pWave->pBuff[0],
            NUM_BUFFERS*BUFFER_SIZE);
    if (pWave->hBuffMem)
    {
        NvRmMemUnpin(pWave->hBuffMem);
        NvRmMemHandleFree(pWave->hBuffMem);
    }

    NvRmClose(pWave->hRm);
    NvOsFree(pWave);
}

extern NvBool s_FpgaEmulation;          // !!!DELETEME!!! HACK FOR BUG 629562


NvError FastbootPlayWave(
    GetMoreBitsCallback Callback,
    NvU32 SampleRateHz,
    NvBool Stereo,
    NvBool Signed,
    NvU32 BitsPerSample,
    FastbootWaveHandle *hWav)
{
    NvOdmAudioWave          AudioWave;
    NvDdkI2sPcmDataProperty I2sData;
    FastbootWave           *pWave;
    NvError                 e;
    NvU32                   i;
    NvOdmAudioConfigInOutSignal ConfigIO;
    NvOdmAudioConfigBypass      ConfigBypass;

    if (s_FpgaEmulation)                // !!!DELETEME!!! HACK FOR BUG 629562
    {                                   // !!!DELETEME!!! HACK FOR BUG 629562
        *hWav = NULL;                   // !!!DELETEME!!! HACK FOR BUG 629562
        return NvError_NotSupported;    // !!!DELETEME!!! HACK FOR BUG 629562
    }

    pWave = NvOsAlloc(sizeof(FastbootWave));
    if (!pWave)
        return NvError_InsufficientMemory;

    NvOsMemset(pWave, 0, sizeof(FastbootWave));
    NvOsMemset(&AudioWave, 0, sizeof(AudioWave));
    pWave->GetMoreBits = Callback;
    pWave->Shutdown = NV_FALSE;
    pWave->BufferTimeoutMs = 1000;
    NV_CHECK_ERROR_CLEANUP(NvOsSemaphoreCreate(&pWave->Trigger, 0));
    NV_CHECK_ERROR_CLEANUP(NvOsMutexCreate(&pWave->hShutdownMutex));
    NV_CHECK_ERROR_CLEANUP(NvRmOpenNew(&pWave->hRm));
    NV_CHECK_ERROR_CLEANUP(
        NvRmMemHandleAlloc(pWave->hRm, NULL, 0, 32,
            NvOsMemAttribute_Uncached, BUFFER_SIZE*NUM_BUFFERS,
            NVRM_MEM_TAG_SYSTEM_MISC, 1, &pWave->hBuffMem)
    );
    pWave->BuffPhys[0] = NvRmMemPin(pWave->hBuffMem);
    NV_CHECK_ERROR_CLEANUP(
        NvRmMemMap(pWave->hBuffMem, 0, BUFFER_SIZE*NUM_BUFFERS,
            NVOS_MEM_READ_WRITE, (void**)&pWave->pBuff[0])
    );

    for (i=1; ; i++)
    {
        const NvOdmQueryDapPortProperty *pDap =
            NvOdmQueryDapPortGetProperty(i);
        if (!pDap)
        {
            e = NvError_NotSupported;
            goto fail;
        }
        if (pDap->DapDestination == NvOdmDapPort_HifiCodecType)
        {
            pWave->DapIndex = i;
            break;
        }
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkI2sOpen(pWave->hRm, 0, &pWave->hI2s));

    NvOsMemset(pWave->pBuff[0], 0, BUFFER_SIZE*NUM_BUFFERS);
    for (i=1; i < NUM_BUFFERS; i++)
    {
        pWave->pBuff[i] = pWave->pBuff[i-1] + BUFFER_SIZE;
        pWave->BuffPhys[i] = pWave->BuffPhys[i-1] + BUFFER_SIZE;
    }

    pWave->hCodec = NvOdmAudioCodecOpen(0, NULL);
    if (!pWave->hCodec)
    {
        e = NvError_NotSupported;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvDdkI2sGetDataProperty(pWave->hI2s, NvDdkI2SMode_PCM, &I2sData)
    );

    I2sData.I2sDataWordFormat = BitsPerSample==16 ? NvDdkI2SDataWordValidBits_InPackedFormat : NvDdkI2SDataWordValidBits_StartFromLsb;
    I2sData.SamplingRate = SampleRateHz;
    I2sData.ValidBitsInI2sDataWord = BitsPerSample;
    I2sData.IsMonoDataFormat = !Stereo;
    I2sData.DatabitsPerLRCLK = 32;
    NV_CHECK_ERROR_CLEANUP(
        NvDdkI2sSetDataProperty(pWave->hI2s, NvDdkI2SMode_PCM, &I2sData)
    );

    AudioWave.NumberOfChannels = (Stereo) ? 2 : 1;
    AudioWave.IsSignedData = Signed;
    AudioWave.IsInterleaved = NV_TRUE;
    AudioWave.SamplingRateInHz = SampleRateHz;
    AudioWave.NumberOfBitsPerSample = BitsPerSample;
    AudioWave.IsLittleEndian = NV_FALSE;
    AudioWave.DataFormat = NvOdmQueryI2sDataCommFormat_I2S;
    AudioWave.FormatType = NvOdmAudioWaveFormat_Pcm;

    if (!NvOdmAudioCodecSetConfiguration(pWave->hCodec,
             NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Input,0),
             NvOdmAudioConfigure_Pcm, (void*)&AudioWave) ||
        !NvOdmAudioCodecSetConfiguration(pWave->hCodec,
             NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output,0),
             NvOdmAudioConfigure_Pcm, (void*)&AudioWave))
    {
        e = NvError_NotSupported;
        goto fail;
    }

    NvOdmAudioCodecSetPowerState(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioSignalType_HeadphoneOut, 0, NV_TRUE);
    NvOdmAudioCodecSetPowerState(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioSignalType_LineOut, 0, NV_TRUE);
    NvOdmAudioCodecSetPowerState(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioSignalType_Speakers, 0, NV_TRUE);

    NvOdmOsMemset(&ConfigIO, 0, sizeof(ConfigIO));
    ConfigIO.IsEnable      = NV_TRUE;
    ConfigIO.InSignalType  = NvOdmAudioSignalType_Digital_I2s;
    ConfigIO.InSignalId    = 0;
    ConfigIO.OutSignalType = NvOdmAudioSignalType_HeadphoneOut;
    ConfigIO.OutSignalId   = 0;

    NvOdmAudioCodecSetConfiguration(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioConfigure_InOutSignal, &ConfigIO);

    //NvOdmAudioCodecSetPowerState(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioSignalType_LineIn, 0, NV_TRUE);

    NvOdmOsMemset(&ConfigBypass, 0, sizeof(ConfigBypass));
    ConfigBypass.IsEnable      = NV_TRUE;
    ConfigBypass.InSignalType  = NvOdmAudioSignalType_LineIn;
    ConfigBypass.InSignalId    = 0;
    ConfigBypass.OutSignalType = NvOdmAudioSignalType_HeadphoneOut;
    ConfigBypass.OutSignalId   = 0;

    //NvOdmAudioCodecSetConfiguration(pWave->hCodec, NVODMAUDIO_PORTNAME(NvOdmAudioPortType_Output, 0), NvOdmAudioConfigure_ByPass, &ConfigBypass);

    if (!(*Callback)(pWave->pBuff[pWave->BufferId], BUFFER_SIZE))
    {
        e = NvError_InvalidSize;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvOsThreadCreate(PlayWaveThread, pWave, &pWave->hPlayThread)
    );

 fail:
    if (e!=NvSuccess)
    {
        FastbootCloseWave(pWave);
        NvOsFree(pWave);
        pWave = NULL;
    }

    *hWav = pWave;
    return e;
}

