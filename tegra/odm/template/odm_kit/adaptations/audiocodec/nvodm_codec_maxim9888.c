 /**
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * NVIDIA APX ODM Kit for Maxim 9888 Implementation
 *
 * Note: Please search "FIXME" for un-implemented part/might_be_wrong part
 */

/*  This audio codec adaptation expects to find a discovery database entry
 *  in the following format:
 *
 * // Audio Codec M9888
 * static const NvOdmIoAddress s_ffaAudioCodecM9888Addresses[] = 
 * {
 *     { NvOdmIoModule_Vdd, 0x00, PCF50626PmuSupply_DCD2, 0   },
 *     { NvOdmIoModule_Vdd, 0x00, PCF50626PmuSupply_RF4REG, 0 },
 *     { NvOdmIoModule_ExternalClock, 0, 0, 0 },               
 *     { NvOdmIoModule_I2c_Pmu, 0x00, 0x20, 0},
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

// Module debug: 0=disable, 1=enable
#define NVODMAUDIOCODEC_M8999_ENABLE_PRINTF (0)

#if NVODMAUDIOCODEC_M8999_ENABLE_PRINTF
    #define NVODMAUDIOCODEC_PRINTF(x)   NvOdmOsDebugPrintf x
#else
    #define NVODMAUDIOCODEC_PRINTF(x)   do {} while(0)
#endif

/* I2C Settings */
#define M9888_I2C_TIMEOUT          1000    // 1 seconds
#define M9888_I2C_SCLK             400     // 400KHz


/* Register & Bit definitions */


#define R10_MCLK                   0x10
#define B04_MCLKDIV                4
/*
00 - PCLK Disabled
01 - 10MHz<= MCLK <= 20MHz (PCLK=MCLK)
10 - 20MHz<= MCLK <= 40MHz (PCLK=MCLK/2)
11 - 40MHz<= MCLK <= 60MHz (PCLK=MCLK/4)
*/ 

#define B00_SAMPLE_RATE            0

#define R11_CLOCK_RATES_1          0x11
#define R19_CLOCK_RATES_2          0x19
#define B04_SAMPLE_RATE            4
/*
0001 8kHz
0010 11.025kHz
0011 16kHz
0100 22.05kHz
0101 24kHz
0110 32kHz
0111 44.1kHz
1000 48kHz
1001 88.2kHz
1010 96kHz
*/

#define R12_PCLK_SEL_HIGH_1        0x12
#define B00_NI_HIGH_1              0

#define R13_PCLK_SEL_LOW_1         0x13
#define B01_NI_LOW_1               1

#define R14_AUDIO_INTERFACE_1      0x14
#define B07_MAS_MODE               7
#define B09_LRCLK_DIR              9
#define B06_AIF_LRCLK_INV          6
#define B04_AIF_FMT                4
#define B00_AIF_24BIT              0

#define R16_AUDIO_INTERFACE_SEC_1  0x16
#define B06_DAI_PORT               6
#define B00_DAI_ENABLE             0

#define R18_AUDIO_INTERFACE_THD_1  0x18
#define B07_DAI_PSBAND_FILLTER     6
#define B03_DAI_HIGH_SAMPLING      3
#define B00_DAC_PSBAND_FILLTER     0

#define R21_DAC_MIXER              0x21
#define B07_DACL_DAI1_L            7
#define B06_DACL_DAI1_R            6
#define B03_DACR_DAI1_L            3
#define B02_DACR_DAI1_R            2

#define R27_HP_MIXER               0x27
#define B07_MIXHPL_DACL            7
#define B06_MIXHPL_DACR            6
#define B03_MIXHPR_DACL            3
#define B02_MIXHPR_DACR            2

#define R29_SPR_MIXER              0x29
#define B07_MIXSPRL_DACL           7
#define B06_MIXSPRL_DACR           6
#define B03_MIXSPRR_DACL           3
#define B02_MIXSPRR_DACR           2

#define R2A_SIDETONE_CTRL          0x2A
#define B07_ADCL_DAC_SEL           7
#define B06_ADCR_DAC_SEL           6
#define B00_ADC_DAC_ST_VOL         0


#define R38_HPOUTL_CTRL            0x38
#define R39_HPOUTR_CTRL            0x39
#define B07_HP_MUTE                7
#define B00_HPOUT_VOL              0

#define R3B_SPROUTL_CTRL           0x3B
#define R3C_SPROUTR_CTRL           0x3C
#define B07_SPR_MUTE               7

#define R4A_PM_REG1                0x4A
#define R4B_PM_REG2                0x4B
#define B07_HPLEN                  7
#define B06_HPREN                  6
#define B05_SPLEN                  5
#define B04_SPREN                  4
#define B03_RECEN                  3
#define B01_DALEN                  1
#define B00_DAREN                  0

#define R4C_PM_REG                 0x4C
#define B07_SHUT_DOWN              7

/*
 * Maxim codec register sequences.
 */
typedef NvU32 WCodecRegIndex;

/*
 * The codec control variables.
 */
typedef struct
{

    NvU32 HPOutVolLeft;
    NvU32 HPOutVolRight;

    NvU32 SprOutVolLeft;
    NvU32 SprOutVolRight;

    NvU32 AdcSamplingRate;
    NvU32 DacSamplingRate;


    NvU32 LineInVolLeft;
    NvU32 LineInVolRight;
} MaximCodecControls;

typedef struct M9888AudioCodecRec
{
    NvOdmIoModule IoModule;
    NvU32   InstanceId;
    NvOdmServicesI2cHandle hOdmService;
    MaximCodecControls WCodecControl;
    NvOdmQueryI2sACodecInterfaceProp WCodecInterface;
    NvU32   RegValues[173];
} M9888AudioCodec, *M9888AudioCodecHandle;

static M9888AudioCodec s_M9888;       /* Unique audio codec for the whole system*/

static void DumpWM8903(M9888AudioCodecHandle hOdmAudioCodec);
/*static void PingM9888(M9888AudioCodecHandle hOdmAudioCodec); fix linux compiler error */
static NvBool EnableInput( M9888AudioCodecHandle hOdmAudioCodec, NvU32  IsMic, NvBool IsPowerOn);


/*fix for linux build error: ../nvodm_codec_maxim9888.c:345: warning: `SetInterfaceActiveState' defined but not used
static NvBool SetInterfaceActiveState(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsActive)
{
    return NV_TRUE;
}
*/


static NvBool ReadMaximRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 *Data)
{
    NvOdmI2cStatus I2cTransStatus = NvOdmI2cStatus_Timeout;    
    NvOdmI2cTransactionInfo TransactionInfo[2];
    M9888AudioCodecHandle hAudioCodec = (M9888AudioCodecHandle)hOdmAudioCodec;
    NvU8 ReadBuffer[1] = {0};

    ReadBuffer[0] = (RegIndex & 0xFF);

    TransactionInfo[0].Address = 0x20;
    TransactionInfo[0].Buf = ReadBuffer;
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
    TransactionInfo[0].NumBytes = 1;
    TransactionInfo[1].Address = (0x20 | 0x1);
    TransactionInfo[1].Buf = ReadBuffer;
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 1;

    NVODMAUDIOCODEC_PRINTF(("Read I2C ADDR 0X%04X Offset 0X%02X\n", TransactionInfo[0].Address, ReadBuffer[0]));


    // Read data from PMU at the specified offset
    I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo[0], 2, 
            M9888_I2C_SCLK, M9888_I2C_TIMEOUT);

    *Data = ReadBuffer[0];
    if (I2cTransStatus == 0) 
    {
        NVODMAUDIOCODEC_PRINTF(("Audio codec Maxim Read 0X%02X = 0X%04X\n", RegIndex, *Data));
        hAudioCodec->RegValues[RegIndex] = *Data;
    }
    else 
        NVODMAUDIOCODEC_PRINTF(("Audio codec Maxim 0x%02x Read Error: %08x\n", RegIndex, I2cTransStatus));

    return (NvBool)(I2cTransStatus == 0);
}



static NvBool WriteMaximRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 Data)
{
    NvOdmI2cStatus I2cTransStatus;    
    NvOdmI2cTransactionInfo TransactionInfo;
    M9888AudioCodecHandle hAudioCodec = (M9888AudioCodecHandle)hOdmAudioCodec;
    NvU8    Buffer[2] = {0};

    /* Pack the data, p70 */
    Buffer[0] = RegIndex & 0xFF;
    Buffer[1] = Data & 0xFF;

    TransactionInfo.Address = 0x20;
    TransactionInfo.Buf = Buffer;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    // write the Accelator offset (from where data is to be read)
    I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, M9888_I2C_SCLK, M9888_I2C_TIMEOUT);


    if (I2cTransStatus)
        NVODMAUDIOCODEC_PRINTF(("\t --- Audio Write (0x%08x) Failed \n", I2cTransStatus));
    else 
    {
        NVODMAUDIOCODEC_PRINTF(("Write 0X%02X = 0X%04X Succeed \n", RegIndex, Data));
    }

    
    return (NvBool)(I2cTransStatus == 0);
}

static NvBool 
M9888_ReadRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 *Data)
{
    return ReadMaximRegister(hOdmAudioCodec, RegIndex, Data); 
}

static NvBool 
M9888_WriteRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 Data)
{
    return WriteMaximRegister(hOdmAudioCodec, RegIndex, Data); 
}


static NvBool
SetSpeakersVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 Channel, 
    NvU32 LeftVolume,
    NvU32 RightVolume)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVol;
    NvU32 RightVol;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;
    M9888AudioCodecHandle hAudioCodec = (M9888AudioCodecHandle)hOdmAudioCodec;


    if (!(Channel & NvOdmAudioSpeakerType_FrontLeft))
         LeftVolume |= (hAudioCodec->WCodecControl.SprOutVolLeft);

    if (!(Channel & NvOdmAudioSpeakerType_FrontRight))
         RightVolume |= (hAudioCodec->WCodecControl.SprOutVolRight);

    LeftVol  =  ((LeftVolume *32)/100 - 1);
    RightVol =  ((RightVolume *32)/100 - 1);

    IsSuccess &= M9888_ReadRegister(hOdmAudioCodec, R3B_SPROUTL_CTRL, &LeftVolReg);
    IsSuccess &= M9888_ReadRegister(hOdmAudioCodec, R3C_SPROUTR_CTRL, &RightVolReg);

    LeftVolReg &= (1<<B07_SPR_MUTE);
    RightVolReg &= (1<<B07_SPR_MUTE);

    LeftVolReg |= LeftVol;
    RightVolReg |= RightVol;

    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R3B_SPROUTL_CTRL, LeftVolReg);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R3C_SPROUTR_CTRL, RightVolReg);

    hAudioCodec->WCodecControl.SprOutVolLeft  = LeftVolume;
    hAudioCodec->WCodecControl.SprOutVolRight = RightVolume;

    return IsSuccess;
};


static NvBool 
SetHeadphoneOutVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 Channel, 
    NvU32 LeftVolume,
    NvU32 RightVolume)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVol;
    NvU32 RightVol;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;
    M9888AudioCodecHandle hAudioCodec = (M9888AudioCodecHandle)hOdmAudioCodec;


    if (!(Channel & NvOdmAudioSpeakerType_FrontLeft))
         LeftVolume |= (hAudioCodec->WCodecControl.HPOutVolLeft);

    if (!(Channel & NvOdmAudioSpeakerType_FrontRight))
         RightVolume |= (hAudioCodec->WCodecControl.HPOutVolRight);

    LeftVol  =  ((LeftVolume *32)/100 - 1);
    RightVol =  ((RightVolume *32)/100 - 1);

    IsSuccess &= M9888_ReadRegister(hOdmAudioCodec, R38_HPOUTL_CTRL, &LeftVolReg);
    IsSuccess &= M9888_ReadRegister(hOdmAudioCodec, R39_HPOUTR_CTRL, &RightVolReg);

    LeftVolReg &= (1<<B07_HP_MUTE);
    RightVolReg &= (1<<B07_HP_MUTE);

    LeftVolReg |= LeftVol;
    RightVolReg |= RightVol;

    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R38_HPOUTL_CTRL, LeftVolReg);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R39_HPOUTR_CTRL, RightVolReg);

    hAudioCodec->WCodecControl.HPOutVolLeft  = LeftVolume;
    hAudioCodec->WCodecControl.HPOutVolRight = RightVolume;

    return IsSuccess;
}

static NvBool 
SetLineInVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 Channel, 
    NvU32 LeftVolume,
    NvU32 RightVolume)
{
    NvBool IsSuccess = NV_TRUE;

    //"FIXME"

    return IsSuccess;
}

static NvBool 
SetHeadphoneOutMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 Channel, 
    NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;

    IsSuccess &= M9888_ReadRegister(hOdmAudioCodec, R3B_SPROUTL_CTRL, &LeftVolReg);
    IsSuccess &= M9888_ReadRegister(hOdmAudioCodec, R3C_SPROUTR_CTRL, &RightVolReg);

    if (IsMute)
    {
        LeftVolReg |= (1<<B07_SPR_MUTE);
        RightVolReg |= (1<<B07_SPR_MUTE);
    }
    else
    {
        LeftVolReg &= ~(1<<B07_SPR_MUTE);
        RightVolReg &= ~(1<<B07_SPR_MUTE);
    }

    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R3B_SPROUTL_CTRL, LeftVolReg);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R3C_SPROUTR_CTRL, RightVolReg);

    return IsSuccess;
}

static NvBool 
SetSpeakerOutMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 Channel, 
    NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;

    IsSuccess &= M9888_ReadRegister(hOdmAudioCodec, R38_HPOUTL_CTRL, &LeftVolReg);
    IsSuccess &= M9888_ReadRegister(hOdmAudioCodec, R39_HPOUTR_CTRL, &RightVolReg);

    if (IsMute)
    {
        LeftVolReg |= (1<<B07_HP_MUTE);
        RightVolReg |= (1<<B07_HP_MUTE);
    }
    else
    {
        LeftVolReg &= ~(1<<B07_HP_MUTE);
        RightVolReg &= ~(1<<B07_HP_MUTE);
    }


    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R38_HPOUTL_CTRL, LeftVolReg);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R39_HPOUTR_CTRL, RightVolReg);

    return IsSuccess;
}



static NvBool 
SetLineInMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 Channel, 
    NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;

    //"FIXME"

    return IsSuccess;
}

static NvBool SetMicrophoneInVolume(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 MicGain)
{
    NvBool IsSuccess = NV_TRUE;

    /* FIXME */
#if 0
    NvU32 CtrReg = 0;
    /* Enable Mic Max Gain */
    M9888_ReadRegister(hOdmAudioCodec, R29_DRC_1, &CtrReg);
    CtrReg &= ~(0x3);
    if (MicGain >= 50)
        CtrReg |= 0x3;  /* 36dB */
    else
        CtrReg |= 0x3;  /* 18dB(default) */

    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R29_DRC_1, CtrReg);
#endif 

    return IsSuccess;
}

static NvBool SetMicrophoneInMute(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    /* FIXME: Disable Mic Input path */
    return NV_TRUE;
}

static NvBool SetCodecPower(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;
    if (IsPowerOn) {
        /* START-UP */
        IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R4C_PM_REG, 0x1<<B07_SHUT_DOWN);
        NvOdmOsSleepMS(300);    /* Wait 300ms */
    } 
   else 
   {
        /* SHUTDOWN */
        IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R4C_PM_REG, 0x0<<B07_SHUT_DOWN);
        NvOdmOsSleepMS(300);    /* Wait 300ms */
   }

    return IsSuccess;
}


/*
 * Shuts down the audio codec driver.
 */
static NvBool ShutDownCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    /* Quick Shutdown */
    return SetCodecPower(hOdmAudioCodec, NV_FALSE);
}


static NvBool InitializeDigitalInterface(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 AudioIF1Reg = 0;
    M9888AudioCodecHandle hAudioCodec = (M9888AudioCodecHandle)hOdmAudioCodec;

    /* Set MCLK */
    AudioIF1Reg = (0x1<<B04_MCLKDIV);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R10_MCLK, AudioIF1Reg);

    /* USB mode 
     * 00: Std Mode
     * 01/10/11: USB Mode */

    AudioIF1Reg = 0;
    /* Master Mode */
    if (hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        NVODMAUDIOCODEC_PRINTF(("Codec is working on MASTER mode\n"));
        AudioIF1Reg |= (1<<B07_MAS_MODE); /* Master Mode */
    }
    else
    {
        NVODMAUDIOCODEC_PRINTF(("Codec is working on SLAVE mode\n"));
        AudioIF1Reg |= (1<<B07_MAS_MODE);   /* Slave Mode */
    }

    /* Audio Data Format
     * 00: Right Justified
     * 01: Left Justified
     * 10: I2S
     * 11: DSP   */
    switch (hAudioCodec->WCodecInterface.I2sCodecDataCommFormat)
    {
/*        case NvOdmQueryI2sDataCommFormat_Dsp:
            AudioIF1Reg |= (0x3<<B00_AIF_FMT);
            break;
*/
        case NvOdmQueryI2sDataCommFormat_LeftJustified:
            AudioIF1Reg |= (0x0<<B04_AIF_FMT);
            break;

/*        case NvOdmQueryI2sDataCommFormat_RightJustified:
            AudioIF1Reg |= (0x0<<B00_AIF_FMT);
            break;
*/
        case NvOdmQueryI2sDataCommFormat_I2S:
        default:
            // Default will be the i2s mode
            AudioIF1Reg |= (0x1<<B04_AIF_FMT);
            break;
    }

    /* LRLine Control, defines the Left/Right channel polarity */
    if (hAudioCodec->WCodecInterface.I2sCodecLRLineControl == NvOdmQueryI2sLRLineControl_LeftOnLow)
        AudioIF1Reg |= (0x0<<B06_AIF_LRCLK_INV);
    else
        AudioIF1Reg |= (0x1<<B06_AIF_LRCLK_INV);

    AudioIF1Reg |= (1<<B00_AIF_24BIT);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R14_AUDIO_INTERFACE_1, AudioIF1Reg);

    AudioIF1Reg = (0x1<<B06_DAI_PORT) | (0x1<<B00_DAI_ENABLE);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R16_AUDIO_INTERFACE_SEC_1, AudioIF1Reg);

    AudioIF1Reg = (0x1<<B07_DAI_PSBAND_FILLTER) | (0x1<<B00_DAC_PSBAND_FILLTER);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R18_AUDIO_INTERFACE_THD_1, AudioIF1Reg);

    return IsSuccess;
}

static NvBool InitializeHeadphoneOut(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    IsSuccess &= SetHeadphoneOutVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                100, 100);

    IsSuccess &= SetSpeakersVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                100, 100);

    return IsSuccess;
}
/*
../nvodm_codec_maxim9888.c:637: warning: `InitializeLineInput' defined but not used'
static NvBool InitializeLineInput(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, 0x0C, 0x03); */  /* left/right PGA enable */
/*

    return IsSuccess;
}
*/


static NvBool InitializeAnalogAudioPath(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvU32 DacCtrlReg = 0;
    NvBool IsSuccess = NV_TRUE;

    /* FIXME: set speaker/lineout/headphone accordingly */

    // Enable DAI1 for left/right DAC
    DacCtrlReg = (1<<B07_DACL_DAI1_L) | (1<<B02_DACR_DAI1_R);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R21_DAC_MIXER, DacCtrlReg);

    // Set headphone for left/right DAC
    DacCtrlReg = (1<<B07_MIXHPL_DACL) | (1<<B02_MIXHPR_DACR);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R27_HP_MIXER, DacCtrlReg);

    // Set speaker for left/right DAC
    DacCtrlReg = (1<<B07_MIXSPRL_DACL) | (1<<B02_MIXSPRR_DACR);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R29_SPR_MIXER, DacCtrlReg);

    //Set PM registers
    DacCtrlReg = (1<<B07_HPLEN) | (1<<B06_HPREN) | (1<<B05_SPLEN) | (1<<B04_SPREN) | (1<<B01_DALEN) | (1<<B00_DAREN);
    IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R4B_PM_REG2, DacCtrlReg);

    return IsSuccess;
}

static NvBool SetDacMute(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    return NV_TRUE;
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
    return NV_TRUE;
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
    NvU32 SamplingContReg = 0;
    M9888AudioCodecHandle hAudioCodec = (M9888AudioCodecHandle)hOdmAudioCodec;

    if (!hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        hAudioCodec->WCodecControl.AdcSamplingRate = SamplingRate;
        hAudioCodec->WCodecControl.DacSamplingRate = SamplingRate;
        return NV_TRUE;
    }

    M9888_ReadRegister(hOdmAudioCodec, R11_CLOCK_RATES_1, &SamplingContReg);

    SamplingContReg &= ~(0xF);

    /* FIXME: Should we set MCLKDIV2 for MCLK division? */
    switch (SamplingRate)
    {
        case 8000:
            SamplingContReg |= (0x0 << B00_SAMPLE_RATE);
            break;
            
        case 32000:
            SamplingContReg |= (0x6 << B00_SAMPLE_RATE);
            break;
            
        case 44100:
            SamplingContReg |= (0x7 << B00_SAMPLE_RATE);
            break;
            
        case 48000:
            SamplingContReg |= (0x8 << B00_SAMPLE_RATE);
            break;
            
        case 88200:
            SamplingContReg |= (0x9 << B00_SAMPLE_RATE);
            break;
            
        case 96000:
            SamplingContReg |= (0xA << B00_SAMPLE_RATE);
            break;
            
        default:
            return NV_FALSE;
    }

    IsSuccess = M9888_WriteRegister(hOdmAudioCodec, R11_CLOCK_RATES_1, SamplingContReg);
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
    NvBool IsSuccess = NV_TRUE;

    if (PortId != 0)
        return NV_FALSE;

    if ((pConfigIO->InSignalId != 0) || (pConfigIO->OutSignalId != 0))
        return NV_FALSE;

    if ((pConfigIO->InSignalType != NvOdmAudioSignalType_LineIn)
         && (pConfigIO->InSignalType != NvOdmAudioSignalType_MicIn))
        return NV_FALSE;


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

/*
 * Sets the output analog line from the DAC.
 */
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
    NvBool IsSuccess = NV_TRUE;

    if ((InputLineId > 0) || (OutputLineId > 1))
        return NV_FALSE;

    /* Bias Ctrl */
    /* FIXME */


    if (InputAnalogLineType & NvOdmAudioSignalType_LineIn)
    {
        if (IsEnable) {
            /* Input Ctrl Enable */

            /* Input Vol */

            /* PGA Input */
        }
    }

    if (InputAnalogLineType & NvOdmAudioSignalType_MicIn)
    {
        if (IsEnable)
        {
        }
    }

    /* Out Mixer */

    /* Output Line Select */
    if (OutputLineId == 0)
    {
    } 
    else if (OutputLineId == 1) 
    {
    }
 
    return IsSuccess;
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
    NvU32 SidetoneCtrlReg = 0;
    NvBool IsSuccess = NV_TRUE;

    if (!IsEnabled)
    {
        IsSuccess &= M9888_ReadRegister(hOdmAudioCodec, R2A_SIDETONE_CTRL, &SidetoneCtrlReg);
        SidetoneCtrlReg &= ~((0x1<<B07_ADCL_DAC_SEL) | (0x1<<B06_ADCR_DAC_SEL));
        IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R2A_SIDETONE_CTRL, SidetoneCtrlReg);
        return IsSuccess;
    }

    if ((SideToneAtenuation > 0) && (SideToneAtenuation <= 100))
    {
        SidetoneCtrlReg = (0x1<<B07_ADCL_DAC_SEL) | (0x1<<B06_ADCR_DAC_SEL);
        SideToneAtenuation = (SideToneAtenuation*32/100);
        SidetoneCtrlReg |= (SideToneAtenuation<<B00_ADC_DAC_ST_VOL);
        return M9888_WriteRegister(hOdmAudioCodec, R2A_SIDETONE_CTRL, SidetoneCtrlReg);
    }

    return NV_TRUE;
}

/*
 * Sets the power status of the specified devices of the audio codec.
 */
static NvBool 
SetPowerStatus(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvOdmAudioSpeakerType Channel,
    NvBool IsPowerOn)
{
    return NV_TRUE;
}

static NvBool OpenMaximCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    // shutdown codec before configuring
    if (IsSuccess)
        IsSuccess = SetCodecPower(hOdmAudioCodec, NV_FALSE);

    if (IsSuccess)
        IsSuccess = InitializeDigitalInterface(hOdmAudioCodec);

    // set sampling rate to 44.1kHz as default
    if (IsSuccess)
    {
        IsSuccess &= WriteMaximRegister(hOdmAudioCodec, R11_CLOCK_RATES_1, 0x70);
    }

    //Set PLL mode for bring-up
    if (IsSuccess)
    {
        IsSuccess &= M9888_WriteRegister(hOdmAudioCodec, R12_PCLK_SEL_HIGH_1, 0x80);
    }

    if (IsSuccess)
        IsSuccess = InitializeAnalogAudioPath(hOdmAudioCodec);

    // can be moved later once we set this along with the volume change
    if (IsSuccess)
        IsSuccess = SetMicrophoneInVolume(hOdmAudioCodec,1);
    
    if (IsSuccess)
        IsSuccess = InitializeHeadphoneOut(hOdmAudioCodec);

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

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_HeadphoneOut, 0,
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                NV_TRUE);

    return IsSuccess;
}

static const NvOdmAudioPortCaps * ACM9888GetPortCaps(void)
{
    /* FIXME */
    static const NvOdmAudioPortCaps s_PortCaps =
        {
            1,    // MaxNumberOfInputPort;
            1     // MaxNumberOfOutputPort;
        };    
    return &s_PortCaps;
}


static const NvOdmAudioOutputPortCaps * 
ACM9888GetOutputPortCaps(
    NvU32 OutputPortId)
{
    /* FIXME */
    static const NvOdmAudioOutputPortCaps s_OutputPortCaps =
            {
                1,          // MaxNumberOfDigitalInput;
                1,          // MaxNumberOfLineOut;
                1,          // MaxNumberOfHeadphoneOut;
                1,          // MaxNumberOfSpeakers;
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
ACM9888GetInputPortCaps(
    NvU32 InputPortId)
{
    /* FIXME: vol is different according to PGA_MODE */
    static const NvOdmAudioInputPortCaps s_InputPortCaps =
            {
                2,          // MaxNumberOfLineIn;
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

static void DumpWM8903(M9888AudioCodecHandle hOdmAudioCodec)
{
    int i = 0;
    NvU32 Data = 0;
    for (i=0; i<173; i++) 
    {
        Data = 0;
        M9888_ReadRegister(hOdmAudioCodec, i, &Data);
    }
}

#define NVODM_CODEC_M9888_MAX_CLOCKS 1
static NvBool SetClockSourceOnCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsEnable)
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 ClockInstances[NVODM_CODEC_M9888_MAX_CLOCKS];
    NvU32 ClockFrequencies[NVODM_CODEC_M9888_MAX_CLOCKS];
    NvU32 NumClocks;

    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(MAXIM_9888_CODEC_GUID);    
    if (pPerConnectivity == NULL)
        return NV_FALSE;

    if (IsEnable)
    {
        if (!NvOdmExternalClockConfig(
                MAXIM_9888_CODEC_GUID, NV_FALSE,
                ClockInstances, ClockFrequencies, &NumClocks))
            return NV_FALSE;
    }
    else
    {
        if (!NvOdmExternalClockConfig(
                MAXIM_9888_CODEC_GUID, NV_TRUE,
                ClockInstances, ClockFrequencies, &NumClocks))
            return NV_FALSE;
    }
    return NV_TRUE;
}


static NvBool ACM9888Open(NvOdmPrivAudioCodecHandle hOdmAudioCodec,
                        const NvOdmQueryI2sACodecInterfaceProp *pI2sCodecInt,
                        void* hCodecNotifySem )
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 Index;
    //NvU32 I2CInstance =0, Interface = 0;
    M9888AudioCodecHandle hM8903 = (M9888AudioCodecHandle)hOdmAudioCodec;

    hM8903->hOdmService = NULL;

    // Codec interface paramater
    pPerConnectivity = NvOdmPeripheralGetGuid(MAXIM_9888_CODEC_GUID);    

    if (pPerConnectivity == NULL)
        goto ErrorExit;

    // Search for the Io interfacing module
    for (Index = 0; Index < pPerConnectivity->NumAddress; ++Index)
    {
        if ((pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_I2c_Pmu) ||
            (pPerConnectivity->AddressList[Index].Interface == NvOdmIoModule_I2c))
        {
            break;
        }
    }

    // If IO module is not found then return fail.
    if (Index == pPerConnectivity->NumAddress)
        goto ErrorExit;

    hM8903->IoModule    = pPerConnectivity->AddressList[Index].Interface;
    hM8903->InstanceId  = pPerConnectivity->AddressList[Index].Instance;

    hM8903->WCodecInterface.DeviceAddress = pPerConnectivity->AddressList[Index].Address;

    hM8903->WCodecInterface.IsCodecMasterMode = pI2sCodecInt->IsCodecMasterMode;
    hM8903->WCodecInterface.IsUsbMode = pI2sCodecInt->IsUsbMode;
    hM8903->WCodecInterface.I2sCodecDataCommFormat = pI2sCodecInt->I2sCodecDataCommFormat;
    hM8903->WCodecInterface.I2sCodecLRLineControl = pI2sCodecInt->I2sCodecLRLineControl;

    // Codec control parameter
    hM8903->WCodecControl.LineInVolLeft = 0;
    hM8903->WCodecControl.LineInVolRight = 0;
    hM8903->WCodecControl.HPOutVolLeft = 0;
    hM8903->WCodecControl.HPOutVolRight = 0;
    hM8903->WCodecControl.AdcSamplingRate = 0;
    hM8903->WCodecControl.DacSamplingRate = 0;

    // Opening the I2C ODM Service
    hM8903->hOdmService = NvOdmI2cOpen(hM8903->IoModule, hM8903->InstanceId);

    /* Verify I2C connection */
    //PingM9888(hM8903);

    if (!hM8903->hOdmService) {
        goto ErrorExit;
    }

    SetClockSourceOnCodec(hOdmAudioCodec, NV_TRUE);

    if (OpenMaximCodec(hM8903)) 
        return NV_TRUE;

ErrorExit:
    SetClockSourceOnCodec(hOdmAudioCodec, NV_FALSE);
    NvOdmI2cClose(hM8903->hOdmService);
    hM8903->hOdmService = NULL;
    return NV_FALSE;
}

static void ACM9888Close(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    M9888AudioCodecHandle hAudioCodec = (M9888AudioCodecHandle)hOdmAudioCodec;

    if (hOdmAudioCodec != NULL)
    {
        ShutDownCodec(hOdmAudioCodec);
        SetClockSourceOnCodec(hOdmAudioCodec, NV_FALSE);
        NvOdmI2cClose(hAudioCodec->hOdmService);
    }
}


static const NvOdmAudioWave * 
ACM9888GetPortPcmCaps(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvU32 *pValidListCount)
{
    /* FIXME */

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

static NvBool 
ACM9888SetPortPcmProps(
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
ACM9888SetVolume(
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
        if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_HeadphoneOut)
        {
            (void)SetHeadphoneOutVolume(hOdmAudioCodec, pVolume[Index].SignalId, 
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
            (void)SetSpeakersVolume(hOdmAudioCodec, pVolume[Index].SignalId, 
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
        }
/*        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_LineIn)
        {
            (void)SetLineInVolume(hOdmAudioCodec, pVolume[Index].SignalId, 
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_MicIn)
        {
            (void)SetMicrophoneInVolume(hOdmAudioCodec, pVolume[Index].VolumeLevel);
        }
*/
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_Speakers)
        {
            (void)SetHeadphoneOutVolume(hOdmAudioCodec, pVolume[Index].SignalId, 
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
            (void)SetSpeakersVolume(hOdmAudioCodec, pVolume[Index].SignalId, 
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
        }

    }
}


static void 
ACM9888SetMuteControl(
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

    /* There two mute method in 8903: 
     * 1), set volume to 0 
     * 2), shutdown DAC output 
     * in this function, method 1) will be used */

    for (Index = 0; Index < ListCount; ++Index)
    {
        if (pMuteParam[Index].SignalType ==  NvOdmAudioSignalType_HeadphoneOut)
        {
            (void)SetHeadphoneOutMute(hOdmAudioCodec, pMuteParam[Index].SignalId, 
                        pMuteParam[Index].IsMute);
            (void)SetSpeakerOutMute(hOdmAudioCodec, pMuteParam[Index].SignalId, 
                        pMuteParam[Index].IsMute);
        }
//         else if (pMuteParam[Index].SignalType ==  NvOdmAudioSignalType_LineIn)
//         {
//             (void)SetLineInMute(hOdmAudioCodec, pMuteParam[Index].SignalId, 
//                         pMuteParam[Index].IsMute);
//         }
//         else if (pMuteParam[Index].SignalType ==  NvOdmAudioSignalType_MicIn)
//         {
//             (void)SetMicrophoneInMute(hOdmAudioCodec, pMuteParam[Index].IsMute);
//         }
        else if (pMuteParam[Index].SignalType ==  NvOdmAudioSignalType_Speakers)
        {
            (void)SetHeadphoneOutMute(hOdmAudioCodec, pMuteParam[Index].SignalId, 
                        pMuteParam[Index].IsMute);
            (void)SetSpeakerOutMute(hOdmAudioCodec, pMuteParam[Index].SignalId, 
                        pMuteParam[Index].IsMute);
        }

    }
}


static NvBool 
ACM9888SetConfiguration(
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
        return ACM9888SetPortPcmProps(hOdmAudioCodec, PortName, pConfigData);
    }    

    if (ConfigType == NvOdmAudioConfigure_InOutSignal)
    {
        if (PortType & NvOdmAudioPortType_Input)
        {
            IsSuccess = SetInputPortIO(hOdmAudioCodec, PortId, pConfigData);
            return IsSuccess;
        }

        if (PortType & NvOdmAudioPortType_Output)
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
ACM9888GetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
}

static NvBool EnableInput(
        M9888AudioCodecHandle hOdmAudioCodec,
        NvU32  IsMic,  /* 0 for LineIn, 1 for Mic */
        NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;

    /* FIXME */

   if (IsPowerOn) 
    {
        /* Analog Input Settings */

        if (IsMic) 
        {
            /* Mic Bias enable */

            /* Enable DRC */

            /* Mic Settings */

            /* Replicate the Mic on both channels */

            NVODMAUDIOCODEC_PRINTF(("Data come from Mic \n"));
        }
        else 
        {
            NVODMAUDIOCODEC_PRINTF(("Data come from LineIn\n"));
        }

        /* Enable analog inputs */

        /* ADC Settings */

        /* ADC Dither Off */

        /* Enable Sidetone */

        /* Enable ADC & DAC */

    } 
    else 
    {
        if (IsMic)
        {
            /* Disable Bias Ctrl */

            /* Disable DRC */

        }

        /* Disable analog inputs */


        /* disable ADC & DAC */


        /* ADC Settings */

        NVODMAUDIOCODEC_PRINTF(("Audio Input is off\n"));
    }

    return IsSuccess;
}

static void 
ACM9888SetPowerState(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioSignalType SignalType,
    NvU32 SignalId,
    NvBool IsPowerOn)
{

    if (SignalType & NvOdmAudioSignalType_LineIn)
    {
        EnableInput(hOdmAudioCodec, 0, IsPowerOn);
    }

    if (SignalType & NvOdmAudioSignalType_MicIn)
    {
        EnableInput(hOdmAudioCodec, 1, IsPowerOn);
    }    
}

static void 
ACM9888SetPowerMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioCodecPowerMode PowerMode)
{
   NvBool IsSuccess = NV_TRUE;

   if (NvOdmAudioCodecPowerMode_Normal == PowerMode)
   {
       SetClockSourceOnCodec(hOdmAudioCodec, NV_TRUE);
       if (IsSuccess)
           IsSuccess = SetCodecPower(hOdmAudioCodec, NV_TRUE);
   }
   else    
   {
       IsSuccess = SetCodecPower(hOdmAudioCodec, NV_FALSE);
       if (IsSuccess)
           SetClockSourceOnCodec(hOdmAudioCodec, NV_FALSE);
    }
}

static void 
ACM9888SetOperationMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvBool IsMaster)
{
    NvU32 CtrlReg;

    SetCodecPower(hOdmAudioCodec, NV_FALSE);

    M9888_ReadRegister(hOdmAudioCodec, R14_AUDIO_INTERFACE_1, &CtrlReg);
    if (IsMaster)
        CtrlReg |= (1<<B07_MAS_MODE);
    else
        CtrlReg &= ~(1<<B07_MAS_MODE);

    M9888_WriteRegister(hOdmAudioCodec, R14_AUDIO_INTERFACE_1, CtrlReg);
    SetCodecPower(hOdmAudioCodec, NV_TRUE);
}

void M9888InitCodecInterface(NvOdmAudioCodec *pCodecInterface)
{
    pCodecInterface->pfnGetPortCaps = ACM9888GetPortCaps;
    pCodecInterface->pfnGetOutputPortCaps = ACM9888GetOutputPortCaps;
    pCodecInterface->pfnGetInputPortCaps = ACM9888GetInputPortCaps;
    pCodecInterface->pfnGetPortPcmCaps = ACM9888GetPortPcmCaps;
    pCodecInterface->pfnOpen = ACM9888Open;
    pCodecInterface->pfnClose = ACM9888Close;
    pCodecInterface->pfnSetVolume = ACM9888SetVolume;
    pCodecInterface->pfnSetMuteControl = ACM9888SetMuteControl;
    pCodecInterface->pfnSetConfiguration = ACM9888SetConfiguration;
    pCodecInterface->pfnGetConfiguration = ACM9888GetConfiguration;
    pCodecInterface->pfnSetPowerState = ACM9888SetPowerState;
    pCodecInterface->pfnSetPowerMode = ACM9888SetPowerMode;
    pCodecInterface->pfnSetOperationMode = ACM9888SetOperationMode;

    pCodecInterface->hCodecPrivate = &s_M9888;
    pCodecInterface->IsInited = NV_TRUE;

    // TODO: delete unused static functions
    (void)DumpWM8903;
    (void)SetLineInVolume;
    (void)SetLineInMute;
    (void)SetMicrophoneInMute;
}

