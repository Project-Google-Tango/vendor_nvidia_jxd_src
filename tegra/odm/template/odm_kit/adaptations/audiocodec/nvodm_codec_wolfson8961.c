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
 * NVIDIA APX ODM Kit for Wolfson 8961 Implementation
 *
 * Note: Please search "FIXME" for un-implemented part/might_be_wrong part
 */

/*  This audio codec adaptation expects to find a discovery database entry
 *  in the following format:
 *
 * // Audio Codec wm8961
 * static const NvOdmIoAddress s_ffaAudioCodecWM8961Addresses[] = 
 * {
 *     { NvOdmIoModule_Vdd, 0x00, PCF50626PmuSupply_DCD2   },
 *     { NvOdmIoModule_Vdd, 0x00, PCF50626PmuSupply_RF4REG },
 *     { NvOdmIoModule_ExternalClock, 0, 0 },               
 *     { NvOdmIoModule_I2c, 0x00, 0x94},
 *     { .... },                
 * };
 * 
 * 
 */


#include "nvodm_services.h"
#include "nvodm_query.h"
#include "nvodm_audiocodec.h"
#include "audiocodec_hal.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"

/* I2C Settings */
#define W8961_I2C_TIMEOUT   1000    // 1 seconds
#define W8961_I2C_SCLK      100     // 400KHz

#define IS_POWER_CONTROL_ENABLE 1 

/* Register & Bit definitions */

#define R2_LOUT1_VOLUME         2
#define B0_LOUT1VOL             0

#define R3_ROUT1_VOLUME         3
#define B8_OUT1VU               8
#define B0_ROUT1VOL             0

#define R4_CLOCK_1              4

#define R8_CLOCK_2          	8
#define R8_BCLOCKDIV_DEFAULT	0x4
#define R8_CLK_DSP_ENABLE       (0x01 << 4)
#define R8_CLK_SYS_ENABLE       (0x01 << 5)
#define R8_DCLOCKDIV_DEFAULT	(0x07 << 6)



#define SOFTWARE_RESET          15

#define R26_POWER_MANAGEMENT    0x1a
#define B8_DACL                 (1 << 8)
#define B7_DACR                 (1 << 7)
#define B6_LOUT1_PGA            (1 << 6)
#define B5_ROUT1_PGA            (1 << 5)
#define B4_SPKL_PGA             (1 << 4)
#define B3_SPKR_PGA             (1 << 3)

#define R40_LOUT2_VOL           0x28
#define R41_ROUT2_VOL           0x29

#define R49_CLASS_D_CTL1        0x31

#define R51_CLASS_D_CTL2        0x33


#define R82_LOW_POWER_MODE      0x52    //Enable dynamic (Class W) power saving
#define DYNAMIC_POWER_DISABLE   0x0
#define DYNAMIC_POWER_ENABLE    0x3

#define R87_WRITE_SEQUENCER_1   0x57    
#define WSEQ_ENABLE             0x20    //Bit 5
#define WSEQ_DISABLE            0x00    


#define R90_WRITE_SEQUENCER_4   0x5a    //5Ah
#define WSEQ_START              0x80
#define WSEQ_ABORT              0x100   


#define R93_WRITE_SEQUENCER_7   0x5d    //5Dh (Read Only)             



/*
 * Wolfson codec register sequences.
 */
typedef NvU32 WCodecRegIndex;

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

typedef struct W8961AudioCodecRec
{
    NvOdmServicesI2cHandle hOdmService;
    NvOdmServicesPmuHandle hOdmServicePmuDevice;
    WolfsonCodecControls WCodecControl;
    NvOdmQueryI2sACodecInterfaceProp WCodecInterface;
    NvU32   RegValues[94];
} W8961AudioCodec, *W8961AudioCodecHandle;

static W8961AudioCodec s_W8961;       /* Unique audio codec for the whole system*/

//static void DumpWM8961(W8961AudioCodecHandle hOdmAudioCodec);
//static void PingWM8961(W8961AudioCodecHandle hOdmAudioCodec); //fix linux compiler error
//static NvBool EnableInput( W8961AudioCodecHandle hOdmAudioCodec, NvU32  IsMic, NvBool IsPowerOn);
#define NVODM_CODEC_W8961_MAX_CLOCKS 3
static NvBool SetPowerOnCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsEnable)
{
#if IS_POWER_CONTROL_ENABLE
    W8961AudioCodecHandle hAudioCodec = (W8961AudioCodecHandle)hOdmAudioCodec;
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 Index;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime;
    NvU32 ClockInstances[NVODM_CODEC_W8961_MAX_CLOCKS];
    NvU32 ClockFrequencies[NVODM_CODEC_W8961_MAX_CLOCKS];
    NvU32 NumClocks = 0;

    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(WOLFSON_8961_CODEC_GUID);    
    if (pPerConnectivity == NULL)
        return NV_FALSE;

    if (IsEnable)
    {
        if (hAudioCodec->hOdmServicePmuDevice == NULL)
        {
            hAudioCodec->hOdmServicePmuDevice = NvOdmServicesPmuOpen();
            if (!hAudioCodec->hOdmServicePmuDevice)
                return NV_FALSE;
        }

        if (!NvOdmExternalClockConfig(
                WOLFSON_8961_CODEC_GUID, NV_FALSE,
                ClockInstances, ClockFrequencies, &NumClocks))
            return NV_FALSE;

        // Search for the Vdd rail and set the proper volage to the rail.
        for (Index = 0; Index < pPerConnectivity->NumAddress; ++Index)
        {
            if (pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_Vdd)
            {
                NvOdmServicesPmuGetCapabilities(hAudioCodec->hOdmServicePmuDevice,
                        pPerConnectivity->AddressList[Index].Address, &RailCaps);
                NvOdmServicesPmuSetVoltage(hAudioCodec->hOdmServicePmuDevice, 
                            pPerConnectivity->AddressList[Index].Address,
                                RailCaps.requestMilliVolts, &SettlingTime);
                if (SettlingTime)
                    NvOdmOsWaitUS(SettlingTime);
            }
        }    
#if 0
                NvOdmOsDebugPrintf("Power On completed on the Wolfson codec\n");
#endif
    
    }
    else
    {
        if (hAudioCodec->hOdmServicePmuDevice != NULL)
        {
            // Search for the Vdd rail and power Off the module
            for (Index = 0; Index < pPerConnectivity->NumAddress; ++Index)
            {
                if (pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_Vdd)
                {
                    NvOdmServicesPmuGetCapabilities(hAudioCodec->hOdmServicePmuDevice,
                            pPerConnectivity->AddressList[Index].Address, &RailCaps);
                    NvOdmServicesPmuSetVoltage(hAudioCodec->hOdmServicePmuDevice, 
                                pPerConnectivity->AddressList[Index].Address,
                                    NVODM_VOLTAGE_OFF, &SettlingTime);
                    if (SettlingTime)
                        NvOdmOsWaitUS(SettlingTime);
                }
            }    
            NvOdmServicesPmuClose(hAudioCodec->hOdmServicePmuDevice);
            hAudioCodec->hOdmServicePmuDevice = NULL;

            if (!NvOdmExternalClockConfig(
                    WOLFSON_8961_CODEC_GUID, NV_TRUE,
                    ClockInstances, ClockFrequencies, &NumClocks))
                return NV_FALSE;
        }

    }

#endif    
    return NV_TRUE;
}

static NvBool 
ReadWolfsonRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 *Data)
{
    NvOdmI2cStatus I2cTransStatus = NvOdmI2cStatus_Timeout;    
    NvOdmI2cTransactionInfo TransactionInfo[2];
    W8961AudioCodecHandle hAudioCodec = (W8961AudioCodecHandle)hOdmAudioCodec;
    NvU8 ReadBuffer[2] = {0};

    ReadBuffer[0] = (RegIndex & 0xFF);

    TransactionInfo[0].Address = hAudioCodec->WCodecInterface.DeviceAddress;
    TransactionInfo[0].Buf = ReadBuffer;
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo[0].NumBytes = 1;    
    TransactionInfo[1].Address = (hAudioCodec->WCodecInterface.DeviceAddress | 0x1);
    TransactionInfo[1].Buf = ReadBuffer;
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 2;

    // Read data from PMU at the specified offset
    I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo[0], 2, 
            W8961_I2C_SCLK, W8961_I2C_TIMEOUT);

    *Data = ReadBuffer[0];
    *Data = (*Data << 8) | ReadBuffer[1];
    if (I2cTransStatus == 0) 
        NvOdmOsDebugPrintf("Read 0X%02X = 0X%04X\n", RegIndex, *Data);
    else 
        NvOdmOsDebugPrintf("0x%02x Read Error: %08x\n", RegIndex, I2cTransStatus);

    return (NvBool)(I2cTransStatus == 0);
}

static NvBool 
WriteWolfsonRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 Data)
{
    NvOdmI2cStatus I2cTransStatus;    
    NvOdmI2cTransactionInfo TransactionInfo;
    W8961AudioCodecHandle hAudioCodec = (W8961AudioCodecHandle)hOdmAudioCodec;
    NvU8    Buffer[3] = {0};


    /* Pack the data, p70 */
    Buffer[0] =  RegIndex & 0xFF;
    Buffer[1] = (Data>>8) & 0xFF;
    Buffer[2] =  Data     & 0xFF;
    
    TransactionInfo.Address = hAudioCodec->WCodecInterface.DeviceAddress;
    TransactionInfo.Buf = (NvU8*)Buffer;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 3;

    // write the Accelator offset (from where data is to be read)
    I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, W8961_I2C_SCLK,
                    W8961_I2C_TIMEOUT);

    // HW- BUG!! If timeout, again retransmit the data.                    
    if (I2cTransStatus == NvOdmI2cStatus_Timeout)
        I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, W8961_I2C_SCLK,
                        W8961_I2C_TIMEOUT);

    if (I2cTransStatus)
        NvOdmOsDebugPrintf("\t --- Failed(0x%08x)\n", I2cTransStatus);
    else 
        NvOdmOsDebugPrintf("Write 0X%02X = 0X%04X", RegIndex, Data);


    return (NvBool)(I2cTransStatus == 0);
}
/*fix for linux build error: ../nvodm_codec_wolfson8961.c:345: warning: `SetInterfaceActiveState' defined but not used
static NvBool SetInterfaceActiveState(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsActive)
{
    return NV_TRUE;
}
*/

static NvBool 
W8961_ReadRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 *Data)
{
    return ReadWolfsonRegister(hOdmAudioCodec, RegIndex, Data); 
}

static NvBool 
W8961_WriteRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 Data)
{
	return WriteWolfsonRegister(hOdmAudioCodec, RegIndex, Data); 
}

static NvBool 
SetSpeakerOutVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask, 
    NvU32 LeftVolume,
    NvU32 RightVolume)
{
    NvBool IsSuccess = NV_TRUE;


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
    NvU32 pwrManagement;
    W8961AudioCodecHandle hAudioCodec = (W8961AudioCodecHandle)hOdmAudioCodec;

    IsSuccess &= W8961_ReadRegister(hOdmAudioCodec, R26_POWER_MANAGEMENT, &pwrManagement);
    IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R26_POWER_MANAGEMENT, pwrManagement | 0x60);

    if (!(ChannelMask & NvOdmAudioSpeakerType_FrontLeft))
        LeftVolume = hAudioCodec->WCodecControl.HPOutVolLeft;

    if (!(ChannelMask & NvOdmAudioSpeakerType_FrontRight))
        RightVolume = hAudioCodec->WCodecControl.HPOutVolRight; 

    LeftVol  =  ((LeftVolume*80)/100 + 47);
    RightVol =  ((RightVolume*80)/100 + 47);

    LeftVolReg = (LeftVol<<B0_LOUT1VOL);       
    RightVolReg = (RightVol<<B0_ROUT1VOL);       

    IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R2_LOUT1_VOLUME, LeftVolReg);
    IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R3_ROUT1_VOLUME, RightVolReg | (1<<B8_OUT1VU));

    hAudioCodec->WCodecControl.HPOutVolLeft  = LeftVolume;
    hAudioCodec->WCodecControl.HPOutVolRight = RightVolume;

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

    return IsSuccess;
}

static NvBool 
SetHeadphoneOutMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask, 
    NvBool IsMute)
{
    NvU32 LeftVol;
    NvU32 RightVol;
    W8961AudioCodecHandle hAudioCodec = (W8961AudioCodecHandle)hOdmAudioCodec;

    if (IsMute)
    {
        LeftVol =  0x0;
        RightVol = 0x0;
    }
    else
    {
        LeftVol  =  hAudioCodec->WCodecControl.HPOutVolLeft;
        RightVol =  hAudioCodec->WCodecControl.HPOutVolRight;
    }

    return SetHeadphoneOutVolume(hOdmAudioCodec, ChannelMask, LeftVol, RightVol);
}


static NvBool 
SetLineInMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask, 
    NvBool IsMute)
{
    NvU32 LeftVol;
    NvU32 RightVol;
    W8961AudioCodecHandle hAudioCodec = (W8961AudioCodecHandle)hOdmAudioCodec;

    if (IsMute)
    {
        LeftVol =  0x0;
        RightVol = 0x0;
    }
    else
    {
        /* FIXME: check Left/Right Volume range */
        LeftVol  =  ((hAudioCodec->WCodecControl.LineInVolLeft*64)/100 - 1);
        RightVol =  ((hAudioCodec->WCodecControl.LineInVolRight*64)/100 - 1);
    }

    return SetLineInVolume(hOdmAudioCodec, ChannelMask, LeftVol, RightVol);
}

static NvBool SetMicrophoneInVolume(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 MicGain)
{
    NvBool IsSuccess = NV_TRUE;

    return IsSuccess;
}

static NvBool SetMicrophoneInMute(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    /* FIXME: Disable Mic Input path */
    return NV_TRUE;
}

/*
 * Shuts down the audio codec driver.
 */
static NvBool ShutDownCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    /* Quick Shutdown */
    return NV_TRUE;
}



static NvBool InitialCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    /* Reset 8961 */
    //DumpWM8961(hOdmAudioCodec);
    NvBool IsSuccess = NV_TRUE;
    IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, SOFTWARE_RESET, 0);
    IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R8_CLOCK_2, R8_BCLOCKDIV_DEFAULT | R8_CLK_DSP_ENABLE | R8_CLK_SYS_ENABLE | R8_DCLOCKDIV_DEFAULT);
    IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R82_LOW_POWER_MODE, DYNAMIC_POWER_ENABLE);
    return IsSuccess;
}

static NvBool SetCodecPower(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 data;
    NvU32 pwrManagement = 0;
    if (IsPowerOn) {
        /* START-UP */
        //Enable Head Phone
        
        IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R87_WRITE_SEQUENCER_1, WSEQ_ENABLE);
        IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R90_WRITE_SEQUENCER_4, WSEQ_START);
        NvOdmOsSleepMS(4000);    /* Wait 800ms */
        IsSuccess &= W8961_ReadRegister(hOdmAudioCodec, R93_WRITE_SEQUENCER_7, &data);
                                                        
        //Enable Speaker output
        //IsSuccess &= W8961_ReadRegister(hOdmAudioCodec, R26_POWER_MANAGEMENT, &pwrManagement);
        IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R26_POWER_MANAGEMENT, pwrManagement | 0x01fc );
        IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R51_CLASS_D_CTL2, 0x1);
        IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R49_CLASS_D_CTL1, 0xc0);
        IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R40_LOUT2_VOL, 0x79);
        IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R41_ROUT2_VOL, 0x79);
        IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R40_LOUT2_VOL, 0x179);
        IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, R41_ROUT2_VOL, 0x179);

    } else {

    }

    return IsSuccess;
}

/*
static NvBool InitializeDigitalInterface(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    return IsSuccess;
}
*/
/*
static NvBool InitializeHeadphoneOut(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    IsSuccess &= SetHeadphoneOutVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                100, 100);

    return IsSuccess;
}
*/
/*
static NvBool InitializeSpeakereOut(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    IsSuccess &= SetSpeakerOutVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                100, 100);
	return IsSuccess;
}
*/
/*
../nvodm_codec_wolfson8961.c:637: warning: `InitializeLineInput' defined but not used'
static NvBool InitializeLineInput(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    IsSuccess &= W8961_WriteRegister(hOdmAudioCodec, 0x0C, 0x03); */  /* left/right PGA enable */
/*

    return IsSuccess;
}
*/

/*
static NvBool InitializeAnalogAudioPath(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    return IsSuccess;
}
*/
/*
static NvBool SetDacMute(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    return NV_TRUE;
}
*/
/*
 * Sets the PCM size for the audio data.
 */
/*
static NvBool  
SetPcmSize(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortType PortType, 
    NvU32 PcmSize)
{
    return NV_TRUE;
}
*/
/*
 * Sets the sampling rate for the audio playback and record path.
 */
/*
static NvBool 
SetSamplingRate(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortType PortType, 
    NvU32 SamplingRate)
{
    NvBool IsSuccess = NV_TRUE;

    return IsSuccess;
}
*/
/*
 * Sets the input analog line to the input to ADC.
 */
/*
static NvBool 
SetInputPortIO(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,  
    NvU32 PortId,
    NvOdmAudioConfigInOutSignal *pConfigIO)
{
    NvBool IsSuccess = NV_TRUE;

    if (PortId != 0)
        return NV_FALSE;

    if ((pConfigIO->InSignalId != 0) || (pConfigIO->OutSignalId != 0))
        return NV_FALSE;

    if ((pConfigIO->InSignalType != NvOdmAudioSignalType_LineIn)
         && (pConfigIO->InSignalType != NvOdmAudioSignalType_MicIn))
        return NV_FALSE;

#if 0
    if (pConfigIO->OutSignalType != NvOdmAudioSignalType_Digital_I2s)
        return NV_FALSE;
#endif

    if (pConfigIO->InSignalType & NvOdmAudioSignalType_LineIn)
    {
        IsSuccess &= EnableInput(hOdmAudioCodec, 0, pConfigIO->IsEnable);
    }
    
    if (pConfigIO->InSignalType & NvOdmAudioSignalType_MicIn)
    {
        IsSuccess &= EnableInput(hOdmAudioCodec, 1, pConfigIO->IsEnable);
    }    

    return IsSuccess;
}
*/
/*
 * Sets the output analog line from the DAC.
 */
/*
static NvBool 
SetOutputPortIO(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,  
    NvU32 PortId,
    NvOdmAudioConfigInOutSignal *pConfigIO)
{
    if (PortId != 0)
        return NV_FALSE;

    if ((pConfigIO->InSignalId != 0) || (pConfigIO->OutSignalId != 0))
        return NV_FALSE;

    if (pConfigIO->InSignalType != NvOdmAudioSignalType_Digital_I2s)
        return NV_FALSE;

    if ((pConfigIO->InSignalType != NvOdmAudioSignalType_LineOut)
         && (pConfigIO->InSignalType != NvOdmAudioSignalType_HeadphoneOut))
        return NV_FALSE;

    return InitializeAnalogAudioPath(hOdmAudioCodec);
}
*/
/*
 * Sets the codec bypass.
 */
/*
static NvBool 
SetBypass(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioSignalType InputAnalogLineType,
    NvU32 InputLineId,
    NvOdmAudioSignalType OutputAnalogLineType,
    NvU32 OutputLineId,
    NvU32 IsEnable)
{
    return NV_TRUE;
}
*/

/*
 * Sets the side attenuation.
 */
/*
static NvBool 
SetSideToneAttenuation(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvU32 SideToneAtenuation,
    NvBool IsEnabled)
{
    return NV_TRUE;
}
*/
/*
 * Sets the power status of the specified devices of the audio codec.
 */
/*
static NvBool 
SetPowerStatus(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvU32 ChannelMask,
    NvBool IsPowerOn)
{
    return NV_TRUE;
}
*/
static NvBool OpenWolfsonCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    /* h/w verify for bringup */
    {
        NvU32 Data = 0;
        W8961_ReadRegister(hOdmAudioCodec, 1, &Data);   /* = 8961 */
    }

    // Initialize the codec.
    IsSuccess = InitialCodec(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = SetCodecPower(hOdmAudioCodec, NV_TRUE);

/*
    if (IsSuccess)
        IsSuccess = InitializeDigitalInterface(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeAnalogAudioPath(hOdmAudioCodec);

    // can be moved later once we set this along with the volume change
    if (IsSuccess)
        IsSuccess = SetMicrophoneInVolume(hOdmAudioCodec,1);
    
    if (IsSuccess)
        IsSuccess = InitializeHeadphoneOut(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeSpeakereOut(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = SetDacMute(hOdmAudioCodec, NV_FALSE);

#if defined(ENABLE_VDAC_SETTINGS)
    // Enable Vdac by default
    if (IsSuccess)
        IsSuccess = EnableVdac(hOdmAudioCodec);
#endif

#if defined(ENABLE_RXN_SETTINGS)
    // Enable Rxn
    if (IsSuccess)
        IsSuccess = EnableRxn(hOdmAudioCodec);
#endif

    //IsSuccess = SetSpeakerMode(hOdmAudioCodec, NV_FALSE);

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_HeadphoneOut, 0,
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                NV_TRUE);

    if (!IsSuccess)
        ShutDownCodec(hOdmAudioCodec);
*/
    return IsSuccess;
}

static const NvOdmAudioPortCaps * ACW8961GetPortCaps(void)
{
    static const NvOdmAudioPortCaps s_PortCaps =
        {
            1,    // MaxNumberOfInputPort;
            1     // MaxNumberOfOutputPort;
        };    
    return &s_PortCaps;
}


static const NvOdmAudioOutputPortCaps * 
ACW8961GetOutputPortCaps(
    NvU32 OutputPortId)
{
    static const NvOdmAudioOutputPortCaps s_OutputPortCaps =
            {
                1,          // MaxNumberOfDigitalInput;
                1,          // MaxNumberOfLineOut;
                1,          // MaxNumberOfHeadphoneOut;
                2,          // MaxNumberOfSpeakers;
                NV_FALSE,   // IsShortCircuitCurrLimitSupported;
                NV_TRUE,    // IsNonLinerVolumeSupported;
                600,        // MaxVolumeInMilliBel;
                -5700,      // MinVolumeInMilliBel;            
            };

    if (OutputPortId == 0)
        return &s_OutputPortCaps;
    return NULL;    
}


static const NvOdmAudioInputPortCaps * 
ACW8961GetInputPortCaps(
    NvU32 InputPortId)
{
    /* FIXME: vol is different according to PGA_MODE */
    static const NvOdmAudioInputPortCaps s_InputPortCaps =
            {
                3,          // MaxNumberOfLineIn;
                1,          // MaxNumberOfMicIn;
                0,          // MaxNumberOfCdIn;
                0,          // MaxNumberOfVideoIn;
                0,          // MaxNumberOfMonoInput;
                1,          // MaxNumberOfOutput;
                NV_TRUE,    // IsSideToneAttSupported;
                NV_TRUE,    // IsNonLinerGainSupported;
                1762,       // MaxVolumeInMilliBel;
                -7162       // MinVolumeInMilliBel;
            };

    if (InputPortId == 0)
        return &s_InputPortCaps;
    return NULL;    
}
/*
static void DumpWM8961(W8961AudioCodecHandle hOdmAudioCodec)
{
    int i = 0;
    NvU32 Data = 0;
    for (i=0; i<=94; i++) 
    {
        Data = 0;
        W8961_ReadRegister(hOdmAudioCodec, i, &Data);
        hOdmAudioCodec->RegValues[i] = Data;
        //NvOdmOsDebugPrintf("R%d = 0X%04X\n", i, Data);
    }
}
*/
/*
../nvodm_codec_wolfson8961.c:1075: warning: `PingWM8961' defined but not used
static void PingWM8961(W8961AudioCodecHandle hOdmAudioCodec)
{
    NvU32 Data;
    int i = 0;

    NvOdmI2cStatus I2cTransStatus = NvOdmI2cStatus_Timeout;    
    NvOdmI2cTransactionInfo TransactionInfo[2];
    W8961AudioCodecHandle hAudioCodec = (W8961AudioCodecHandle)hOdmAudioCodec;
    NvU8 ReadBuffer[2] = {0, 0};

    for (i=0; i<128; i++) {
        TransactionInfo[0].Address = i*2;
        TransactionInfo[0].Buf = ReadBuffer;
        TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
        TransactionInfo[0].NumBytes = 1;    
        TransactionInfo[1].Address = (i*2 | 0x1);
        TransactionInfo[1].Buf = ReadBuffer;
        TransactionInfo[1].Flags = 0;
        TransactionInfo[1].NumBytes = 2;

        // Read data from PMU at the specified offset
        I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo[0], 2, 
                W8961_I2C_SCLK, W8961_I2C_TIMEOUT);


        Data = (ReadBuffer[0] << 8) | ReadBuffer[1];
        if (I2cTransStatus == 0) 
            NvOdmOsDebugPrintf("@%02x: Data = %04x\n", i*2, Data);
        //else 
            //NvOdmOsDebugPrintf("Read Error: %08x\n", I2cTransStatus);
    }
}
*/
/*
#define NvOdmGpioPinGroup_OEM_MCLK    0
static void SetMclkSelector(NvBool IsOn)
{
    const NvOdmGpioPinInfo *pPinInfo;   // Pin information
    NvOdmGpioPinHandle  hPin;           // Pin handle
    NvU32   count;                      // Pin count
    NvU32   Port;                       // GPIO Port
    NvU32   Pin;                        // GPIO Pin
    NvOdmGpioPinMode mode;

    NvOdmServicesGpioHandle hOdmGpio = NvOdmGpioOpen();

    if (hOdmGpio == NULL)
    {
        return;
    }

    // Retrieve the pin configuration
    pPinInfo = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_OEM, 0, &count);

    if (pPinInfo)
    {
        Port = pPinInfo[NvOdmGpioPinGroup_OEM_MCLK].Port;
        Pin  = pPinInfo[NvOdmGpioPinGroup_OEM_MCLK].Pin;

        hPin = NvOdmGpioAcquirePinHandle(hOdmGpio, Port, Pin);
        if (hPin == NULL)
        {
            goto SetMclkSelectorExit;
        }
        // Configure the pin.
        mode = NvOdmGpioPinMode_Output;
        NvOdmGpioConfig(hOdmGpio, hPin, mode);
        NvOdmGpioSetState(hOdmGpio, hPin, IsOn);
        NvOdmGpioReleasePinHandle(hOdmGpio, hPin);

        
     }
SetMclkSelectorExit:
     NvOdmGpioClose(hOdmGpio);

}
*/

static NvBool ACW8961Open(NvOdmPrivAudioCodecHandle hOdmAudioCodec, const NvOdmQueryI2sACodecInterfaceProp *pI2sCodecInt, void* hCodecNotifySem)
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 Index;
    W8961AudioCodecHandle hWM8961 = (W8961AudioCodecHandle)hOdmAudioCodec;

    hWM8961->hOdmService = NULL;

    // Codec interface paramater
    pPerConnectivity = NvOdmPeripheralGetGuid(WOLFSON_8961_CODEC_GUID);
    if (pPerConnectivity == NULL)
        goto ErrorExit;

    // Search for the Io interfacing module
    for (Index = 0; Index < pPerConnectivity->NumAddress; ++Index)
    {
        if (pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_I2c_Pmu)
        {
            break;
        }
        if (pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_I2c)
        {
            break;
        }
    }

    // If IO module is not found then return fail.
    if (Index == pPerConnectivity->NumAddress)
        goto ErrorExit;

    hWM8961->WCodecInterface.DeviceAddress = pPerConnectivity->AddressList[Index].Address;

    hWM8961->WCodecInterface.IsCodecMasterMode = pI2sCodecInt->IsCodecMasterMode;
    hWM8961->WCodecInterface.IsUsbMode = pI2sCodecInt->IsUsbMode;
    hWM8961->WCodecInterface.I2sCodecDataCommFormat = pI2sCodecInt->I2sCodecDataCommFormat;
    hWM8961->WCodecInterface.I2sCodecLRLineControl = pI2sCodecInt->I2sCodecLRLineControl;

    // Codec control parameter
    hWM8961->WCodecControl.LineInVolLeft = 0;
    hWM8961->WCodecControl.LineInVolRight = 0;
    hWM8961->WCodecControl.HPOutVolLeft = 0;
    hWM8961->WCodecControl.HPOutVolRight = 0;
    hWM8961->WCodecControl.AdcSamplingRate = 0;
    hWM8961->WCodecControl.DacSamplingRate = 0;

    // Turn on the power rail
    SetPowerOnCodec(hWM8961, NV_TRUE);

    // Opening the I2C ODM Service
    hWM8961->hOdmService = NvOdmI2cOpen(pPerConnectivity->AddressList[Index].Interface, 0);

    /* Verify I2C connection */
    //PingWM8961(hWM8961);

    if (!hWM8961->hOdmService) {
        goto ErrorExit;
    }

    if (OpenWolfsonCodec(hWM8961)) 
        return NV_TRUE;

ErrorExit:
    NvOdmI2cClose(hWM8961->hOdmService);
    hWM8961->hOdmService = NULL;
    return NV_FALSE;
}

static void ACW8961Close(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8961AudioCodecHandle hAudioCodec = (W8961AudioCodecHandle)hOdmAudioCodec;
    if (hOdmAudioCodec != NULL)
    {
        ShutDownCodec(hOdmAudioCodec);
        SetPowerOnCodec(hOdmAudioCodec, NV_FALSE);
        NvOdmI2cClose(hAudioCodec->hOdmService);
    }
}


static const NvOdmAudioWave * 
ACW8961GetPortPcmCaps(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvU32 *pValidListCount)
{
    static const NvOdmAudioWave s_AudioPcmProps[] = 
    {
        // NumberOfChannels;
        // IsSignedData;
        // IsLittleEndian;
        // IsInterleaved;
        // NumberOfBitsPerSample; 
        // SamplingRateInHz; 
        // DatabitsPerLRCLK
        // DataFormat
        // NvOdmAudioWaveFormat FormatType;
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 8000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 20, 32000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 24, 44100, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 32, 48000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 32, 88200, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 32, 96000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
    };

    *pValidListCount = sizeof(s_AudioPcmProps)/sizeof(s_AudioPcmProps[0]);
    return &s_AudioPcmProps[0];
}
/*
static NvBool 
ACW8961SetPortPcmProps(
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
*/
static void 
ACW8961SetVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioVolume *pVolume,
    NvU32 ListCount)
{
    NvU32 PortId;
    NvU32 Index;
    NvU32 NewVolume;
    
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);
    if (PortId != 0)
        return;

    for (Index = 0; Index < ListCount; ++Index)
    {
        // FIX ME: Should check value of NvOdmAudioVolume.VolumeLevel for Sign-to-Unsigned casting
        NewVolume = (NvU32)pVolume[Index].VolumeLevel;
        if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_HeadphoneOut)
        {
            (void)SetHeadphoneOutVolume(hOdmAudioCodec, pVolume[Index].ChannelMask, 
                        NewVolume, NewVolume);
        }
        else if (pVolume[Index].SignalType == NvOdmAudioSignalType_Speakers)
        {
            (void)SetSpeakerOutVolume(hOdmAudioCodec, pVolume[Index].ChannelMask, 
                        NewVolume, NewVolume);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_LineIn)
        {
            (void)SetLineInVolume(hOdmAudioCodec, pVolume[Index].ChannelMask, 
                        NewVolume, NewVolume);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_MicIn)
        {
            (void)SetMicrophoneInVolume(hOdmAudioCodec, NewVolume);
        }
    }
}


static void 
ACW8961SetMuteControl(
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

    /* There two mute method in 8961: 
     * 1), set volume to 0 
     * 2), shutdown DAC output 
     * in this function, method 1) will be used */

    for (Index = 0; Index < ListCount; ++Index)
    {
        if (pMuteParam[Index].SignalType ==  NvOdmAudioSignalType_HeadphoneOut)
        {
            (void)SetHeadphoneOutMute(hOdmAudioCodec, pMuteParam[Index].ChannelMask, 
                        pMuteParam[Index].IsMute);
        }
        else if (pMuteParam[Index].SignalType ==  NvOdmAudioSignalType_LineIn)
        {
            (void)SetLineInMute(hOdmAudioCodec, pMuteParam[Index].ChannelMask, 
                        pMuteParam[Index].IsMute);
        }
        else if (pMuteParam[Index].SignalType ==  NvOdmAudioSignalType_MicIn)
        {
            (void)SetMicrophoneInMute(hOdmAudioCodec, pMuteParam[Index].IsMute);
        }
    }
}


static NvBool 
ACW8961SetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
    return NV_TRUE;    
}

static void 
ACW8961GetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
}
/*
static NvBool EnableInput(
        W8961AudioCodecHandle hOdmAudioCodec,
        NvU32  IsMic,  // 0 for LineIn, 1 for Mic
        NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;
    return IsSuccess;
}
*/
static void 
ACW8961SetPowerState(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{
    return;
}

static void 
ACW8961SetPowerMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioCodecPowerMode PowerMode)
{
    
}

static void
ACW8961SetOperationMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvBool IsMaster)
{
    // ToDo
}

void W8961InitCodecInterface(NvOdmAudioCodec *pCodecInterface)
{
    pCodecInterface->pfnGetPortCaps = ACW8961GetPortCaps;
    pCodecInterface->pfnGetOutputPortCaps = ACW8961GetOutputPortCaps;
    pCodecInterface->pfnGetInputPortCaps = ACW8961GetInputPortCaps;
    pCodecInterface->pfnGetPortPcmCaps = ACW8961GetPortPcmCaps;
    pCodecInterface->pfnOpen = ACW8961Open;
    pCodecInterface->pfnClose = ACW8961Close;
    pCodecInterface->pfnSetVolume = ACW8961SetVolume;
    pCodecInterface->pfnSetMuteControl = ACW8961SetMuteControl;
    pCodecInterface->pfnSetConfiguration = ACW8961SetConfiguration;
    pCodecInterface->pfnGetConfiguration = ACW8961GetConfiguration;
    pCodecInterface->pfnSetPowerState = ACW8961SetPowerState;
    pCodecInterface->pfnSetPowerMode = ACW8961SetPowerMode;
    pCodecInterface->pfnSetOperationMode = ACW8961SetOperationMode;

    pCodecInterface->hCodecPrivate = &s_W8961;
    pCodecInterface->IsInited = NV_TRUE;
}

