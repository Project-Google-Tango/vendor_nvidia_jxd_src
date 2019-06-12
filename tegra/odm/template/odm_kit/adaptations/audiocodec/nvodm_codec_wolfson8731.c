 /**
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * NVIDIA APX ODM Kit:: Wolfson Codec Implementation
 *
 * This file implements the Wolfson audio codec driver.
 */

#include "nvodm_services.h"
#include "nvodm_query.h"
#include "nvodm_audiocodec.h"
#include "audiocodec_hal.h"
#include "wolfson8731_registers.h"
#include "nvodm_query_discovery.h"


#define W8731_SET_REG_VAL(r,f,n, v) \
    (((v)& (~(W8731_##r##_0_##f##_SEL << W8731_##r##_0_##f##_LOC))) | \
        (((n) & W8731_##r##_0_##f##_SEL) << W8731_##r##_0_##f##_LOC))

#define W8731_SET_REG_DEF(r,f,c,v) \
    (((v)& (~(W8731_##r##_0_##f##_SEL << W8731_##r##_0_##f##_LOC))) | \
        (((W8731_##r##_0_##f##_##c) & W8731_##r##_0_##f##_SEL) << W8731_##r##_0_##f##_LOC))

#define W8731_GET_REG_VAL(r,f,v) \
    ((((v) >> W8731_##r##_0_##f##_LOC)& W8731_##r##_0_##f##_SEL))

/*
 * Wolfson codec register sequences.
 */
typedef enum
{
    WCodecRegIndex_LeftLineIn          = 0,
    WCodecRegIndex_RightLineIn,
    WCodecRegIndex_LeftHeadphoneOut,
    WCodecRegIndex_RightHeadphoneOut,
    WCodecRegIndex_AnalogPath,
    WCodecRegIndex_DigitalPath,
    WCodecRegIndex_PowerControl,
    WCodecRegIndex_DigInterface,
    WCodecRegIndex_SamplingControl,
    WCodecRegIndex_Active,
    WCodecRegIndex_Reset,
    WCodecRegIndex_Max,
    WCodecRegIndex_Force32             = 0x7FFFFFFF
} WCodecRegIndex;

/*
 * The codec control variables.
 */
typedef struct
{

    NvU32 HPOutVolLeft;
    NvU32 HPOutVolRight;

    NvU32 AdcSamplingRate;
    NvU32 DacSamplingRate;


    NvU32 LineInVolLeft;
    NvU32 LineInVolRight;
} WolfsonCodecControls;

typedef struct W8731AudioCodecRec
{
    NvOdmServicesI2cHandle hOdmService;
    WolfsonCodecControls WCodecControl;
    NvOdmQueryI2sACodecInterfaceProp WCodecInterface;
    NvU32 WCodecRegVals[WCodecRegIndex_Max];
} W8731AudioCodec, *W8731AudioCodecHandle;

static W8731AudioCodec s_W8731 = {0};       /* Unique audio codec for the whole system*/

#define W8731_WRITETIMEOUT 1000 // 1 seconds

static NvBool 
WriteWolfsonRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 Data)
{
    NvOdmI2cStatus I2cTransStatus;    
    NvU32 DataToSend;
    NvU8 pTxBuffer[5];
    NvOdmI2cTransactionInfo TransactionInfo;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    
    DataToSend = (RegIndex << 9) | (Data & 0x1FF);
    pTxBuffer[0] = (NvU8)((DataToSend >> 8) & 0xFF);
    pTxBuffer[1] = (NvU8)((DataToSend) & 0xFF);

    TransactionInfo.Address = hAudioCodec->WCodecInterface.DeviceAddress;
    TransactionInfo.Buf = pTxBuffer;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    // write the Accelator offset (from where data is to be read)
    I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, 400,
                    W8731_WRITETIMEOUT);

    // HW- BUG!! If timeout, again retransmit the data.                    
    if (I2cTransStatus == NvOdmI2cStatus_Timeout)
        I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, 400,
                        W8731_WRITETIMEOUT);

    if (I2cTransStatus == NvOdmI2cStatus_Success)
    {
        hAudioCodec->WCodecRegVals[RegIndex] = Data;
        return NV_TRUE;
    }    

    NvOdmOsDebugPrintf("Error in the i2c communication 0x%08x\n", I2cTransStatus);
    return NV_FALSE;
}

static NvBool SetInterfaceActiveState(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsActive)
{
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    NvU32 ActiveControlReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_Active];
    if (IsActive)
        ActiveControlReg = W8731_SET_REG_DEF(ACTIVE, ACTIVE, ACTIVATE, ActiveControlReg);
    else
        ActiveControlReg = W8731_SET_REG_DEF(ACTIVE, ACTIVE, DEACTIVATE, ActiveControlReg);

    return WriteWolfsonRegister(hOdmAudioCodec, WCodecRegIndex_Active, ActiveControlReg);
}

static NvBool 
W8731_WriteRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 Data)
{
    NvBool IsSuccess = NV_TRUE;
    NvBool IsActivationControl = NV_FALSE;
    
    // Deactivate if it is the digital path : Programming recomendation
    if ((RegIndex == WCodecRegIndex_DigInterface) || 
            (RegIndex == WCodecRegIndex_SamplingControl) ||
            (RegIndex == WCodecRegIndex_DigitalPath))
    {
        IsActivationControl = NV_TRUE;
    }

    if (IsActivationControl)
        IsSuccess = SetInterfaceActiveState(hOdmAudioCodec, NV_FALSE);

    // Write the wolfson register.
    if (IsSuccess)
        IsSuccess = WriteWolfsonRegister(hOdmAudioCodec, RegIndex, Data); 

    // Activate again if it is inactivated.
    if (IsSuccess)
    {
        if (IsActivationControl)
            IsSuccess = SetInterfaceActiveState(hOdmAudioCodec, NV_TRUE);
    }
    return IsSuccess;
}

static NvBool 
SetHeadphoneOutVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask, 
    NvU32 LeftVolume,
    NvU32 RightVolume)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVol;
    NvU32 RightVol;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;

    LeftVol =  0x30 + ((LeftVolume*80 + 50)/100) -1;
    RightVol =  0x30 + ((RightVolume*80 + 50)/100) -1;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        LeftVolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftHeadphoneOut];
        LeftVolReg = W8731_SET_REG_VAL(LEFT_HEADPHONE, LHP_VOL, LeftVol, LeftVolReg);
        IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftHeadphoneOut, LeftVolReg);
        if (IsSuccess)
            hAudioCodec->WCodecControl.HPOutVolLeft = LeftVolume;
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        RightVolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightHeadphoneOut];
        RightVolReg = W8731_SET_REG_VAL(RIGHT_HEADPHONE, RHP_VOL, RightVol, RightVolReg);
        IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightHeadphoneOut, RightVolReg);
        if (IsSuccess)
            hAudioCodec->WCodecControl.HPOutVolRight = RightVolume;
    }
    return IsSuccess;
}

static NvBool 
SetLineInVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask, 
    NvU32 LeftVolume,
    NvU32 RightVolume)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVol;
    NvU32 RightVol;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;

    LeftVol =  ((LeftVolume*32 + 50)/100) -1;
    RightVol = ((RightVolume*32 + 50)/100) -1;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        LeftVolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftLineIn];
        LeftVolReg = W8731_SET_REG_VAL(LEFT_LINEIN, LIN_VOL, LeftVol, LeftVolReg);
        IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftLineIn, LeftVolReg);
        if (IsSuccess)
            hAudioCodec->WCodecControl.LineInVolLeft = LeftVolume;
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        RightVolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightLineIn];
        RightVolReg = W8731_SET_REG_VAL(RIGHT_LINEIN, RIN_VOL, RightVol, RightVolReg);
        IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightLineIn, RightVolReg);
        if (IsSuccess)
            hAudioCodec->WCodecControl.LineInVolRight = RightVolume;
    }
    return IsSuccess;    
}

static NvBool 
SetHeadphoneOutMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask, 
    NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVol;
    NvU32 RightVol;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;

    if (IsMute)
    {
        LeftVol =  0x0;
        RightVol = 0x0;
    }
    else
    {
        LeftVol =  0x30 + ((hAudioCodec->WCodecControl.HPOutVolLeft*80 + 50)/100) -1;
        RightVol =  0x30 + ((hAudioCodec->WCodecControl.HPOutVolRight*80 + 50)/100) -1;
    }

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        LeftVolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftHeadphoneOut];
        LeftVolReg = W8731_SET_REG_VAL(LEFT_HEADPHONE, LHP_VOL, LeftVol, LeftVolReg);
        IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftHeadphoneOut, LeftVolReg);
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        RightVolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightHeadphoneOut];
        RightVolReg = W8731_SET_REG_VAL(RIGHT_HEADPHONE, RHP_VOL, RightVol, RightVolReg);
        IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightHeadphoneOut, RightVolReg);
    }
    return IsSuccess;
}


static NvBool 
SetLineInMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask, 
    NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        LeftVolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftLineIn];
        if (IsMute)
            LeftVolReg = W8731_SET_REG_DEF(LEFT_LINEIN, LIN_MUTE, ENABLE, LeftVolReg);
        else
            LeftVolReg = W8731_SET_REG_DEF(LEFT_LINEIN, LIN_MUTE, DISABLE, LeftVolReg);
        IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LeftLineIn, LeftVolReg);
    }

    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        RightVolReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightLineIn];
        if (IsMute)
            RightVolReg = W8731_SET_REG_DEF(RIGHT_LINEIN, RIN_MUTE, ENABLE, RightVolReg);
        else
            RightVolReg = W8731_SET_REG_DEF(RIGHT_LINEIN, RIN_MUTE, DISABLE, RightVolReg);
        IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RightLineIn, RightVolReg);
    }    
    return IsSuccess;
}

static NvBool SetMicrophoneInVolume(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 MicGain)
{
    NvU32 AnalogPathReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;

    AnalogPathReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_AnalogPath];
    if (MicGain >= 50)
        AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, MICBOOST, ENABLE, AnalogPathReg);
    else
        AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, MICBOOST, DISABLE, AnalogPathReg);

    return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AnalogPath, AnalogPathReg);
}

static NvBool SetMicrophoneInMute(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    NvU32 AnalogPathReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;

    AnalogPathReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_AnalogPath];
    if (IsMute)
        AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, MUTEMIC, ENABLE, AnalogPathReg);
    else
        AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, MUTEMIC, DISABLE, AnalogPathReg);
    return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AnalogPath, AnalogPathReg);
}

/*
 * Shuts down the audio codec driver.
 */

#define NVODM_CODEC_W8731_MAX_CLOCKS 3

static NvBool ShutDownCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 PowerControl;
    NvU32 ResetVal;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    NvU32 ClockInstances[NVODM_CODEC_W8731_MAX_CLOCKS];
    NvU32 ClockFrequencies[NVODM_CODEC_W8731_MAX_CLOCKS];
    NvU32 NumClocks;

    PowerControl = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerControl];
    PowerControl = W8731_SET_REG_DEF(POWER_CONTROL, POWEROFF, DISABLE, PowerControl);
    IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerControl, PowerControl);
    if (IsSuccess)
    {
        ResetVal = W8731_RESET_0_RESET_VAL;
        IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Reset, ResetVal);
    }

    if (IsSuccess)
        IsSuccess = NvOdmExternalClockConfig(
            WOLFSON_8731_CODEC_GUID, NV_TRUE, ClockInstances, ClockFrequencies, &NumClocks);

    return IsSuccess;
}

static NvBool ResetCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_Reset, W8731_RESET_0_RESET_VAL);
}


static NvBool SetCodecPower(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsPowerOn)
{
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    NvU32 PowerControl = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerControl];
    if (IsPowerOn)
        PowerControl = W8731_SET_REG_DEF(POWER_CONTROL, POWEROFF, DISABLE, PowerControl);
    else
        PowerControl = W8731_SET_REG_DEF(POWER_CONTROL, POWEROFF, ENABLE, PowerControl);

    PowerControl = W8731_SET_REG_DEF(POWER_CONTROL, CLKOUTPD, DISABLE, PowerControl);
    PowerControl = W8731_SET_REG_DEF(POWER_CONTROL, OSCPD, DISABLE, PowerControl);
    
    return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_PowerControl, PowerControl);
}

static NvBool InitializeDigitalInterface(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess;
    NvU32 DigIntReg = 0;
    NvU32 SampleContReg = 0;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    
    // Initializing the configuration parameters.
    if (hAudioCodec->WCodecInterface.IsUsbMode)
        SampleContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, USB_NORM, MODE_USB, SampleContReg);
    else
        SampleContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, USB_NORM, MODE_NORM, SampleContReg);
    
    if (hAudioCodec->WCodecInterface.IsCodecMasterMode)
        DigIntReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, MS, MASTER_MODE, DigIntReg);
    else
        DigIntReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, MS, SLAVE_MODE, DigIntReg);

    switch (hAudioCodec->WCodecInterface.I2sCodecDataCommFormat)
    {
        case NvOdmQueryI2sDataCommFormat_Dsp:
            DigIntReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, FORMAT, DSP_MODE, DigIntReg);
            break;

        case NvOdmQueryI2sDataCommFormat_I2S:
            DigIntReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, FORMAT, I2S_MODE, DigIntReg);
            break;

        case NvOdmQueryI2sDataCommFormat_LeftJustified:
            DigIntReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, FORMAT, LEFT_JUSTIFIED, DigIntReg);
            break;
            
        case NvOdmQueryI2sDataCommFormat_RightJustified:
            DigIntReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, FORMAT, RIGHT_JUSTIFIED, DigIntReg);
            break;
            
        default:
            // Default will be the i2s mode
            DigIntReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, FORMAT, I2S_MODE, DigIntReg);
            break;
    }

    if (hAudioCodec->WCodecInterface.I2sCodecLRLineControl == NvOdmQueryI2sLRLineControl_LeftOnLow)
        DigIntReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, LRP, LEFT_DACLRC_LOW, DigIntReg);
    else
        DigIntReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, LRP, LEFT_DACLRC_HIGH, DigIntReg);


    IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DigInterface, DigIntReg);
    if (IsSuccess)
        IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_SamplingControl, SampleContReg);
    return IsSuccess;
}

static NvBool InitializeHeadphoneOut(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    NvU32 LHPOutReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftHeadphoneOut];
    NvU32 RHPOutReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_RightHeadphoneOut];

    LHPOutReg = W8731_SET_REG_DEF(LEFT_HEADPHONE, LHP_ZCEN, DISABLE, LHPOutReg);
    RHPOutReg = W8731_SET_REG_DEF(RIGHT_HEADPHONE, RHP_ZCEN, DISABLE, RHPOutReg);
    hAudioCodec->WCodecRegVals[WCodecRegIndex_LeftHeadphoneOut] = LHPOutReg;
    hAudioCodec->WCodecRegVals[WCodecRegIndex_RightHeadphoneOut] = RHPOutReg;

    return SetHeadphoneOutVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                100, 100);
}

static NvBool InitializeLineInput(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    return SetLineInVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                100, 100);
}

static NvBool InitializeAnalogAudioPath(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvU32 AnAudPathReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;

    AnAudPathReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_AnalogPath];
    AnAudPathReg = W8731_SET_REG_DEF(ANALOG_PATH, BYPASS, DISABLE, AnAudPathReg);
    AnAudPathReg = W8731_SET_REG_DEF(ANALOG_PATH, DACSEL, DISABLE, AnAudPathReg);
    AnAudPathReg = W8731_SET_REG_DEF(ANALOG_PATH, SIDETONE, DISABLE, AnAudPathReg);
    AnAudPathReg = W8731_SET_REG_DEF(ANALOG_PATH, DACSEL, ENABLE, AnAudPathReg);

    return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AnalogPath, AnAudPathReg);
}

static NvBool SetDacMute(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    NvU32 DigPathReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;

    DigPathReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_DigitalPath];
    if (IsMute)
        DigPathReg = W8731_SET_REG_DEF(DIGITAL_PATH, DACMU, ENABLE, DigPathReg);
    else
        DigPathReg = W8731_SET_REG_DEF(DIGITAL_PATH, DACMU, DISABLE, DigPathReg);

    return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DigitalPath, DigPathReg);
}

/*
 * Sets the PCM size for the audio data.
 */
static NvBool  
SetPcmSize(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortType PortType, 
    NvU32 PcmSize)
{
    NvU32 DigitalPathReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    
    DigitalPathReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_DigInterface];
    switch (PcmSize)
    {
        case 16:
            DigitalPathReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, IWL, BIT_16,
                                        DigitalPathReg);
            break;
            
        case 20:
            DigitalPathReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, IWL, BIT_20,
                                        DigitalPathReg);
            break;
            
        case 24:
            DigitalPathReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, IWL, BIT_24,
                                        DigitalPathReg);
            break;
            
        case 32:
            DigitalPathReg = W8731_SET_REG_DEF(DIGITAL_INTERFACE, IWL, BIT_32,
                                        DigitalPathReg);
            break;
            
        default:
            return NV_FALSE;
    }
//    NvOdmOsDebugPrintf("The new Pcm control Reg = 0x%08x \n",DigitalPathReg);

    return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DigInterface,
                                            DigitalPathReg);
}

/*
 * Sets the sampling rate for the audio playback and record path.
 */
static NvBool 
SetSamplingRate(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortType PortType, 
    NvU32 SamplingRate)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 SamplingContReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
   
    if (!hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        hAudioCodec->WCodecControl.AdcSamplingRate = SamplingRate;
        hAudioCodec->WCodecControl.DacSamplingRate = SamplingRate;
        return NV_TRUE;
    }
    
    SamplingContReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_SamplingControl];
    switch (SamplingRate)
    {
        case 8000:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, BOSR, 
                                                DISABLE, SamplingContReg);
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, SR,
                                        ADC_8_DAC_8, SamplingContReg);
            }    
            break;
            
        case 32000:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, BOSR, 
                                                DISABLE, SamplingContReg);
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, SR,
                                        ADC_32_DAC_32, SamplingContReg);
            }    
            break;
            
        case 44100:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, BOSR, 
                                                ENABLE, SamplingContReg);
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, SR,
                                        ADC_441_DAC_441, SamplingContReg);
            }    
            break;
            
        case 48000:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, BOSR, 
                                                ENABLE, SamplingContReg);
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, SR,
                                        ADC_48_DAC_48, SamplingContReg);
            }    
            break;
            
        case 88200:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, BOSR, 
                                                DISABLE, SamplingContReg);
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, SR,
                                        ADC_882_DAC_882, SamplingContReg);
            }    
            break;
            
        case 96000:
            if (hAudioCodec->WCodecInterface.IsUsbMode)
            {
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, BOSR, 
                                                DISABLE, SamplingContReg);
                SamplingContReg = W8731_SET_REG_DEF(SAMPLING_CONTROL, SR,
                                        ADC_96_DAC_96, SamplingContReg);
            }    
            break;
            
        default:
            return NV_FALSE;
    }

//    NvOdmOsDebugPrintf("The new sampling control Reg = 0x%08x \n",SamplingContReg);
    IsSuccess = W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_SamplingControl,
                            SamplingContReg);
    if (IsSuccess)
    {
        hAudioCodec->WCodecControl.AdcSamplingRate = SamplingRate;
        hAudioCodec->WCodecControl.DacSamplingRate = SamplingRate;
    }
    return IsSuccess;
}

/*
 * Sets the input analog line to the input to ADC.
 */
static NvBool 
SetInputPortIO(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,  
    NvU32 PortId,
    NvOdmAudioConfigInOutSignal *pConfigIO)
{
    NvU32 AnalogPathReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;

    if (PortId != 0)
        return NV_FALSE;

    if ((pConfigIO->InSignalId != 0) || (pConfigIO->OutSignalId != 0))
        return NV_FALSE;

    if ((pConfigIO->InSignalType != NvOdmAudioSignalType_LineIn)
         && (pConfigIO->InSignalType != NvOdmAudioSignalType_MicIn))
        return NV_FALSE;

    if (pConfigIO->OutSignalType != NvOdmAudioSignalType_Digital_I2s)
        return NV_FALSE;
    
    AnalogPathReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_AnalogPath];
    if (pConfigIO->InSignalType == NvOdmAudioSignalType_LineIn)
        AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, INSEL, LINEIN, AnalogPathReg);
    else
        AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, INSEL, MIC, AnalogPathReg);
    return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AnalogPath,
                                            AnalogPathReg);
}

/*
 * Sets the output analog line from the DAC.
 */
static NvBool 
SetOutputPortIO(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,  
    NvU32 PortId,
    NvOdmAudioConfigInOutSignal *pConfigIO)
{
    NvU32 AnalogPathReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;

    if (PortId != 0)
        return NV_FALSE;

    if ((pConfigIO->InSignalId != 0) || (pConfigIO->OutSignalId != 0))
        return NV_FALSE;

    if (pConfigIO->InSignalType != NvOdmAudioSignalType_Digital_I2s)
        return NV_FALSE;

    if ((pConfigIO->InSignalType != NvOdmAudioSignalType_LineOut)
         && (pConfigIO->InSignalType != NvOdmAudioSignalType_HeadphoneOut))
        return NV_FALSE;

    
    AnalogPathReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_AnalogPath];
    AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, DACSEL, ENABLE, AnalogPathReg); 

    return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AnalogPath, AnalogPathReg);
}

/*
 * Sets the codec bypass.
 */
static NvBool 
SetBypass(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioSignalType InputAnalogLineType,
    NvU32 InputLineId,
    NvOdmAudioSignalType OutputAnalogLineType,
    NvU32 OutputLineId,
    NvU32 IsEnable)
{
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    NvU32 AnalogPathReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_AnalogPath];

    if ((InputLineId != 0) || (OutputLineId != 0))
        return NV_FALSE;

    if (OutputLineId != 0)
        return NV_FALSE;

    if (InputAnalogLineType & NvOdmAudioSignalType_LineIn)
    {
        if (IsEnable)
            AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, BYPASS, ENABLE, AnalogPathReg);
        else
            AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, BYPASS, DISABLE, AnalogPathReg);
    }
    
    if (InputAnalogLineType & NvOdmAudioSignalType_MicIn)
    {
        if (IsEnable)
            AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, SIDETONE, ENABLE, AnalogPathReg);
        else
            AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, SIDETONE, DISABLE, AnalogPathReg);
    }
    
    return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AnalogPath, AnalogPathReg);
}


/*
 * Sets the side attenuation.
 */
static NvBool 
SetSideToneAttenuation(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvU32 SideToneAtenuation,
    NvBool IsEnabled)
{
    NvU32 AnalogPathReg;
    
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    AnalogPathReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_AnalogPath];
    if (!IsEnabled)
    {
        AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, SIDETONE, DISABLE, 
                                            AnalogPathReg);
        return W8731_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AnalogPath, 
                                            AnalogPathReg);
    }

    if ((SideToneAtenuation > 0) && (SideToneAtenuation <= 100))
    {
        AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, SIDETONE, ENABLE, 
                                                AnalogPathReg);
        if (SideToneAtenuation < 25)
            AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, SIDEATT, DB_6, 
                                                    AnalogPathReg);
        else if ((SideToneAtenuation >= 25) && (SideToneAtenuation < 50))
            AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, SIDEATT, DB_9, 
                                                    AnalogPathReg);
        else if ((SideToneAtenuation >= 50) && (SideToneAtenuation < 75))
            AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, SIDEATT, DB_12, 
                                                    AnalogPathReg);
        else
            AnalogPathReg = W8731_SET_REG_DEF(ANALOG_PATH, SIDEATT, DB_15, 
                                                    AnalogPathReg);
        return W8731_WriteRegister(hOdmAudioCodec,WCodecRegIndex_AnalogPath , 
                            AnalogPathReg);
    }
    return NV_FALSE;
}

/*
 * Sets the power status of the specified devices of the audio codec.
 */
static NvBool 
SetPowerStatus(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvU32 ChannelMask,
    NvBool IsPowerOn)
{
    NvU32 PowerControlReg;
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    
    PowerControlReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_PowerControl];
    if (SignalType & NvOdmAudioSignalType_LineIn)
    {
        if (IsPowerOn)
        {
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, LININPD, DISABLE, 
                                        PowerControlReg);
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, ADCPD, DISABLE, 
                                    PowerControlReg);
        }
        else
        {
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, LININPD, ENABLE, 
                                    PowerControlReg);
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, ADCPD, ENABLE, 
                                    PowerControlReg);
        }
    }

    if (SignalType & NvOdmAudioSignalType_MicIn)
    {
        if (IsPowerOn)
        {
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, ADCPD, DISABLE, 
                                    PowerControlReg);
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, MICPD, DISABLE, 
                                    PowerControlReg);
        }
        else
        {
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, ADCPD, ENABLE, 
                                    PowerControlReg);
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, MICPD, ENABLE, 
                                    PowerControlReg);
        }
    }    

    if ((SignalType & NvOdmAudioSignalType_LineOut) ||
                (SignalType & NvOdmAudioSignalType_LineOut))
    {
        if (IsPowerOn)
        {
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, OUTPD, DISABLE, 
                                    PowerControlReg);
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, DACPD, DISABLE, 
                                    PowerControlReg);

        }
        else
        {
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, OUTPD, ENABLE, 
                                    PowerControlReg);
            PowerControlReg = W8731_SET_REG_DEF(POWER_CONTROL, DACPD, ENABLE, 
                                    PowerControlReg);
        }
    }

    return W8731_WriteRegister(hOdmAudioCodec,WCodecRegIndex_PowerControl, 
                    PowerControlReg);
}

static NvBool OpenWolfsonCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    // Reset the codec.
    IsSuccess = ResetCodec(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = SetCodecPower(hOdmAudioCodec, NV_TRUE);

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_LineIn, 0, 
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
            NV_TRUE);

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_HeadphoneOut, 0, 
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight), 
            NV_TRUE);
    if (IsSuccess)
        IsSuccess = InitializeDigitalInterface(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeHeadphoneOut(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeLineInput(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeAnalogAudioPath(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = SetDacMute(hOdmAudioCodec, NV_TRUE);

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_HeadphoneOut, 0,
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight), NV_TRUE);

    if (IsSuccess)
        IsSuccess = SetDacMute(hOdmAudioCodec, NV_FALSE);

    if (!IsSuccess)
        ShutDownCodec(hOdmAudioCodec);

    return IsSuccess;
}

static const NvOdmAudioPortCaps * ACW8731GetPortCaps(void)
{
    static const NvOdmAudioPortCaps s_PortCaps =
    {
        1,    // MaxNumberOfInputPort;
        1     // MaxNumberOfOutputPort;
    };    
    
    return &s_PortCaps;
}


static const NvOdmAudioOutputPortCaps * 
ACW8731GetOutputPortCaps(
    NvU32 OutputPortId)
{
    static const NvOdmAudioOutputPortCaps s_OutputPortCaps =
            {
                1,          // MaxNumberOfDigitalInput;
                1,          // MaxNumberOfLineOut;
                1,          // MaxNumberOfHeadphoneOut;
                2,          // MaxNumberOfSpeakers;
                NV_TRUE,    // IsShortCircuitCurrLimitSupported;
                NV_FALSE,   // IsNonLinerVolumeSupported;
                0,          // MaxVolumeInMilliBel;
                0,          // MinVolumeInMilliBel;            
            };

    if (OutputPortId == 0)
        return &s_OutputPortCaps;
    return NULL;    
}


static const NvOdmAudioInputPortCaps * 
ACW8731GetInputPortCaps(
    NvU32 InputPortId)
{
    static const NvOdmAudioInputPortCaps s_InputPortCaps =
            {
                2,          // MaxNumberOfLineIn;
                1,          // MaxNumberOfMicIn;
                0,          // MaxNumberOfCdIn;
                0,          // MaxNumberOfVideoIn;
                0,          // MaxNumberOfMonoInput;
                1,          // MaxNumberOfOutput;
                NV_FALSE,   // IsSideToneAttSupported;
                NV_FALSE,   // IsNonLinerGainSupported;
                0,          // MaxVolumeInMilliBel;
                0           // MinVolumeInMilliBel;
            };

    if (InputPortId == 0)
        return &s_InputPortCaps;
    return NULL;    
}


static void ACW8731Open(NvOdmPrivAudioCodecHandle hOdmAudioCodec,
                        const NvOdmQueryI2sACodecInterfaceProp *pI2sCodecInt,
                        void* hCodecNotifySem )
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 Index;
    NvU32 I2CInstance =0;
    NvU32 ClockInstances[NVODM_CODEC_W8731_MAX_CLOCKS];
    NvU32 ClockFrequencies[NVODM_CODEC_W8731_MAX_CLOCKS];
    NvU32 NumClocks;
    W8731AudioCodecHandle hNewAcodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    hNewAcodec->hOdmService = NULL;

    // Codec interface paramater
    pPerConnectivity = NvOdmPeripheralGetGuid(WOLFSON_8731_CODEC_GUID);    
    if (pPerConnectivity == NULL)
        goto ErrorExit;

    // Search for the Io interfacing module
    for (Index = 0; Index < pPerConnectivity->NumAddress; ++Index)
    {
        if (pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_I2c)
        {
            break;
        }
    }

    // If IO module is not found then return fail.
    if (Index == pPerConnectivity->NumAddress)
        goto ErrorExit;

    I2CInstance = pPerConnectivity->AddressList[Index].Instance;

    hNewAcodec->WCodecInterface.DeviceAddress = pPerConnectivity->AddressList[Index].Address;

    hNewAcodec->WCodecInterface.IsCodecMasterMode = pI2sCodecInt->IsCodecMasterMode;
    hNewAcodec->WCodecInterface.IsUsbMode = pI2sCodecInt->IsUsbMode;
    hNewAcodec->WCodecInterface.I2sCodecDataCommFormat = pI2sCodecInt->I2sCodecDataCommFormat;
    hNewAcodec->WCodecInterface.I2sCodecLRLineControl = pI2sCodecInt->I2sCodecLRLineControl;

    // Codec control parameter
    hNewAcodec->WCodecControl.LineInVolLeft = 0;
    hNewAcodec->WCodecControl.LineInVolRight = 0;
    hNewAcodec->WCodecControl.HPOutVolLeft = 0;
    hNewAcodec->WCodecControl.HPOutVolRight = 0;
    hNewAcodec->WCodecControl.AdcSamplingRate = 0;
    hNewAcodec->WCodecControl.DacSamplingRate = 0;

    // Set all register values to reset and initailize the addresses
    for (Index = 0; Index < WCodecRegIndex_Max; ++Index)
        hNewAcodec->WCodecRegVals[Index] = 0;
    
    if (!NvOdmExternalClockConfig(
            WOLFSON_8731_CODEC_GUID, NV_FALSE,
            ClockInstances, ClockFrequencies, &NumClocks))
        goto ErrorExit;

    // Opening the I2C ODM Service
    hNewAcodec->hOdmService = NvOdmI2cOpen(NvOdmIoModule_I2c, I2CInstance);
    if (!hNewAcodec->hOdmService)
        goto ErrorExit_CodecClock;

    if (OpenWolfsonCodec(hNewAcodec))
        return;

ErrorExit_CodecClock:
    NvOdmExternalClockConfig(
        WOLFSON_8731_CODEC_GUID, NV_TRUE, ClockInstances, ClockFrequencies, &NumClocks);
ErrorExit:
    NvOdmI2cClose(hNewAcodec->hOdmService);
    hNewAcodec->hOdmService = NULL;
}

static void ACW8731Close(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8731AudioCodecHandle hAudioCodec = (W8731AudioCodecHandle)hOdmAudioCodec;
    if (hOdmAudioCodec != NULL)
    {
        ShutDownCodec(hOdmAudioCodec);
        NvOdmI2cClose(hAudioCodec->hOdmService);
        NvOdmOsFree(hOdmAudioCodec);
    }
}


static const NvOdmAudioWave * 
ACW8731GetPortPcmCaps(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvU32 *pValidListCount)
{
    static const NvOdmAudioWave s_AudioPcmProps[6] = 
    {
        // NumberOfChannels;
        // IsSignedData;
        // IsLittleEndian;
        // IsInterleaved;
        // NumberOfBitsPerSample; 
        // SamplingRateInHz; 
        // NvOdmAudioWaveFormat FormatType;
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 8000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 20, 32000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 24, 44100, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 32, 48000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 32, 88200, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 32, 96000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
    };

    *pValidListCount = 6;
    return &s_AudioPcmProps[0];
}

static NvBool 
ACW8731SetPortPcmProps(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioWave *pWaveParams)
{
    NvBool IsSuccess;
    NvOdmAudioPortType PortType;
    NvU32 PortId;
    
    PortType = NVODMAUDIO_GET_PORT_TYPE(PortName);
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);
    if (PortId != 0)
        return NV_FALSE;

    if ((PortType == NvOdmAudioPortType_Input) ||
        (PortType == NvOdmAudioPortType_Output)) 
    {
        IsSuccess = SetPcmSize(hOdmAudioCodec, PortType, pWaveParams->NumberOfBitsPerSample);
        if (IsSuccess)
            IsSuccess = SetSamplingRate(hOdmAudioCodec, PortType, pWaveParams->SamplingRateInHz);
        return IsSuccess;
    }

    return NV_FALSE;    
}

static void 
ACW8731SetVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioVolume *pVolume,
    NvU32 ListCount)
{
    NvU32 PortId;
    NvU32 Index;
    
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);
    if (PortId != 0)
        return;

    for (Index = 0; Index < ListCount; ++Index)
    {
        if (pVolume->SignalType ==  NvOdmAudioSignalType_HeadphoneOut)
        {
            (void)SetHeadphoneOutVolume(hOdmAudioCodec, pVolume->ChannelMask, 
                        pVolume->VolumeLevel, pVolume->VolumeLevel);
        }
        else if (pVolume->SignalType ==  NvOdmAudioSignalType_LineIn)
        {
            (void)SetLineInVolume(hOdmAudioCodec, pVolume->ChannelMask, 
                        pVolume->VolumeLevel, pVolume->VolumeLevel);
        }
        else if (pVolume->SignalType ==  NvOdmAudioSignalType_MicIn)
        {
            (void)SetMicrophoneInVolume(hOdmAudioCodec, pVolume->VolumeLevel);
        }
    }
}


static void 
ACW8731SetMuteControl(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioMuteData *pMuteParam,
    NvU32 ListCount)
{
    NvU32 PortId;
    NvU32 Index;
    
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);
    if (PortId != 0)
        return;

    for (Index = 0; Index < ListCount; ++Index)
    {
        if (pMuteParam->SignalType ==  NvOdmAudioSignalType_HeadphoneOut)
        {
            (void)SetHeadphoneOutMute(hOdmAudioCodec, pMuteParam->ChannelMask, 
                        pMuteParam->IsMute);
        }
        else if (pMuteParam->SignalType ==  NvOdmAudioSignalType_LineIn)
        {
            (void)SetLineInMute(hOdmAudioCodec, pMuteParam->ChannelMask, 
                        pMuteParam->IsMute);
        }
        else if (pMuteParam->SignalType ==  NvOdmAudioSignalType_MicIn)
        {
            (void)SetMicrophoneInMute(hOdmAudioCodec, pMuteParam->IsMute);
        }
    }
}


static NvBool 
ACW8731SetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
    NvBool IsSuccess = NV_TRUE;
    NvOdmAudioPortType PortType;
    NvU32 PortId;
    NvOdmAudioConfigBypass *pBypassConfig = pConfigData;
    NvOdmAudioConfigSideToneAttn *pSideToneAttn = pConfigData;
    
    PortType = NVODMAUDIO_GET_PORT_TYPE(PortName);
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);
    if (PortId != 0)
        return NV_FALSE;

    if (ConfigType == NvOdmAudioConfigure_Pcm)
    {
        return ACW8731SetPortPcmProps(hOdmAudioCodec, PortName, pConfigData);
    }    

    if (ConfigType == NvOdmAudioConfigure_InOutSignal)
    {
        if (PortType == NvOdmAudioPortType_Input)
        {
            IsSuccess = SetInputPortIO(hOdmAudioCodec, PortId, pConfigData);
            return IsSuccess;
        }
        
        if (PortType == NvOdmAudioPortType_Output)
        {
            IsSuccess = SetOutputPortIO(hOdmAudioCodec, PortId, pConfigData);
            return IsSuccess;
        }
        return NV_TRUE;
    }

    if (ConfigType == NvOdmAudioConfigure_ByPass)
    {
        IsSuccess = SetBypass(hOdmAudioCodec, pBypassConfig->InSignalType, 
            pBypassConfig->InSignalId, pBypassConfig->OutSignalType, 
            pBypassConfig->OutSignalId, pBypassConfig->IsEnable);
        return IsSuccess;
    }    

    if (ConfigType == NvOdmAudioConfigure_SideToneAtt)
    {
        IsSuccess = SetSideToneAttenuation(hOdmAudioCodec, pSideToneAttn->SideToneAttnValue,
                            pSideToneAttn->IsEnable);
        return IsSuccess;
    }    
    return NV_TRUE;
}

static void 
ACW8731GetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
}

static void 
ACW8731SetPowerState(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    NvU32 PortId;
    
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);
    if (PortId != 0)
        return;
    SetPowerStatus(hOdmAudioCodec, SignalType, SignalId, NvOdmAudioSpeakerType_All, 
                    IsPowerOn);
}

static void 
ACW8731SetPowerMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioCodecPowerMode PowerMode)
{
    
}

static void 
ACWM8731SetOperationMode(
    NvOdmAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvBool IsMaster)
{

}

void W8731InitCodecInterface(NvOdmAudioCodec *pCodecInterface)
{
    pCodecInterface->pfnGetPortCaps       = ACW8731GetPortCaps;
    pCodecInterface->pfnGetOutputPortCaps = ACW8731GetOutputPortCaps;
    pCodecInterface->pfnGetInputPortCaps  = ACW8731GetInputPortCaps;
    pCodecInterface->pfnGetPortPcmCaps    = ACW8731GetPortPcmCaps;
    pCodecInterface->pfnOpen              = ACW8731Open;
    pCodecInterface->pfnClose             = ACW8731Close;
    pCodecInterface->pfnSetVolume         = ACW8731SetVolume;
    pCodecInterface->pfnSetMuteControl    = ACW8731SetMuteControl;
    pCodecInterface->pfnSetConfiguration  = ACW8731SetConfiguration;
    pCodecInterface->pfnGetConfiguration  = ACW8731GetConfiguration;
    pCodecInterface->pfnSetPowerState     = ACW8731SetPowerState;
    pCodecInterface->pfnSetPowerMode      = ACW8731SetPowerMode;
    pCodecInterface->pfnSetOperationMode  = ACWM8731SetOperationMode;

    pCodecInterface->hCodecPrivate = &s_W8731;
    pCodecInterface->IsInited = NV_TRUE;
}

