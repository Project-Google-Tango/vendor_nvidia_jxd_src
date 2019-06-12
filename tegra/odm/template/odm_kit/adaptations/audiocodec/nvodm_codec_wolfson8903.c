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
 * NVIDIA APX ODM Kit for Wolfson 8903 Implementation
 *
 * Note: Please search "FIXME" for un-implemented part/might_be_wrong part
 */

/*  This audio codec adaptation expects to find a discovery database entry
 *  in the following format:
 *
 * // Audio Codec wm8903
 * static const NvOdmIoAddress s_ffaAudioCodecWM8903Addresses[] =
 * {
 *     { NvOdmIoModule_Vdd, 0x00, PCF50626PmuSupply_DCD2   },
 *     { NvOdmIoModule_Vdd, 0x00, PCF50626PmuSupply_RF4REG },
 *     { NvOdmIoModule_ExternalClock, 0, 0 },
 *     { NvOdmIoModule_I2c_Pmu, 0x00, 0x34},
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
#define NVODMAUDIOCODEC8903_ENABLE_PRINTF (0)

#if NVODMAUDIOCODEC8903_ENABLE_PRINTF
    #define NVODMAUDIOCODEC8903_PRINTF(x)   NvOdmOsDebugPrintf x
#else
    #define NVODMAUDIOCODEC8903_PRINTF(x)   do {} while(0)
#endif

#define W8903_SET_REG_DEF(r,f,c,v) \
    (((v)& (~(W8903_##r##_0_##f##_SEL << W8903_##r##_0_##f##_LOC))) | \
        (((W8903_##r##_0_##f##_##c) & W8903_##r##_0_##f##_SEL) << W8903_##r##_0_##f##_LOC))

/* I2C Settings */
#define W8903_I2C_TIMEOUT   1000    // 1 seconds
#define W8903_I2C_SCLK      100     // 400KHz

/* Microphone Types */
#define MIC_Differential        0
#define MIC_SingleEnded         1


/* Register & Bit definitions */

#define R04_BIAS_CTRL           4
#define B04_POBCTRL             4
#define B02_ISEL                2
#define B01_STARTUP_BIAS_ENA    1
#define B00_BIAS_ENA            0

#define R06_MICBIAS_CTRL_0      6
#define B07_MICDET_HYST_ENA     7
#define B04_MICDET_THR          4
#define B02_MICSHORT_THR        2
#define B01_MICDET_ENA          1
#define B00_MICBIAS_ENA         0

#define R0C_INPUT_SEL           12
#define B01_INL_ENA             1
#define B00_INR_ENA             0

#define R0D_MIXOUT              13
#define B01_MIXOUTL_ENA         1
#define B00_MIXOUTR_ENA         0

#define R0E_HP_PGA              14
#define B01_HPL_PGA_ENA         1
#define B00_HPR_PGA_ENA         0

#define R0F_LINEOUT_PGA         15
#define B01_LINEOUTL_PGA_ENA    1
#define B00_LINEOUTR_PGA_ENA    0

#define R10_MIXSPK              16
#define B01_MIXSPKL_ENA         1
#define B00_MIXSPKR_ENA         0

#define R11_SPK                 17
#define B01_SPKL_ENA            1
#define B00_SPKR_ENA            0

#define R12_ADCDAC_ENA          18
#define B03_DACL_ENA            3
#define B02_DACR_ENA            2
#define B01_ADCL_ENA            1
#define B00_ADCR_ENA            0

#define R14_CLOCK_RATES_0       20
#define B00_MCLKDIV2            0

#define R15_CLOCK_RATES_1       21
#define B10_CLK_SYS_RATE        10
#define B08_CLK_SYS_MODE        8
#define B00_SAMPLE_RATE         0

#define R16_CLOCK_RATES_2       22
#define B02_CLK_SYS_ENA         2

#define R18_AUDIO_INTERFACE_0   24
#define B12_DACL_DATINV         12
#define B11_DACR_DATINV         11
#define B09_DAC_BOOST           9
#define B08_LOOPBACK            8
#define B07_AIFADCL_SRC         7
#define B06_AIFADCR_SRC         6
#define B05_AIFDACL_SRC         5
#define B04_AIFDACR_SRC         4
#define B03_ADC_COMP            3
#define B02_ADC_COMPMODE        2
#define B01_DAC_COMP            1
#define B00_DAC_COMPMODE        0

#define R19_AUDIO_INTERFACE_1   25
#define B09_LRCLK_DIR           9
#define B06_BCLK_DIR            6
#define B04_AIF_LRCLK_INV       4
#define B02_AIF_WL              2
#define B00_AIF_FMT             0

// Audio data format select
#define W8903_AUDIO_INTERFACE_0_FORMAT_LOC                            0
#define W8903_AUDIO_INTERFACE_0_FORMAT_SEL                            3
#define W8903_AUDIO_INTERFACE_0_FORMAT_RIGHT_JUSTIFIED                0
#define W8903_AUDIO_INTERFACE_0_FORMAT_LEFT_JUSTIFIED                 1
#define W8903_AUDIO_INTERFACE_0_FORMAT_I2S_MODE                       2
#define W8903_AUDIO_INTERFACE_0_FORMAT_DSP_MODE                       3

#define R20_SIDETONE_CTRL       32
#define B08_ADCL_DAC_SVOL       8
#define B04_ADCR_DAC_SVOL       4
#define B02_ADC_TO_DACL         2
#define B00_ADC_TO_DACR         0

#define R21_DAC_DIGITAL_1       33
#define B12_MONO_MIX            12
#define B11_DAC_FILTER          11
#define B10_RAMP_RATE           10
#define B09_SOFT_MUTE           9
#define B03_MUTE                3
#define B01_DEEMPH              1
#define B00_DAC_OSR             0

#define R26_ADC_DIGITAL_0       38
#define B05_ADC_HPF_CUT         5
#define B04_ADC_HPF_ENA         4
#define B01_ADCL_DATINV         1
#define B00_ADCR_DATINV         0

#define R28_DRC_0               40
#define B15_DRC_ENA             15
#define B11_DRC_THRESH_HYST     11
#define B06_DRC_STARTUP_GAIN    6
#define B05_DRC_FF_DELAY        5
#define B03_DRC_SMOOTH_ENA      3
#define B02_DRC_QR_ENA          2
#define B01_DRC_ANTICLIP_ENA    1
#define B00_DRC_HYST_ENA        0

#define R29_DRC_1               41
#define B12_DRC_ATTACK_RATE     12
#define B08_DRC_DECAY_RATE      8
#define B06_DRC_THRESH_QR       6
#define B04_DRC_RATE_QR         4
#define B02_DRC_MINGAIN         2
#define B00_DRC_MAXGAIN         0

#define R2C_ANALOG_INPUT_L_0    44
#define R2D_ANALOG_INPUT_R_0    45
/* Common for both Left/Right analog input 0 */
#define B07_INEMUTE             7
#define B06_VOL_M3DB            6
#define B00_IN_VOL              0

#define R2E_ANALOG_INPUT_L_1    46
#define R2F_ANALOG_INPUT_R_1    47
/* Common for both Left/Right analog input 1 */
#define B06_IN_CM_ENA           6
#define B04_IP_SEL_N            4
#define B02_IP_SEL_P            2
#define B00_MODE                0


#define R32_MIXOUT_CTRL_DAC_L   50
#define R33_MIXOUT_CTRL_DAC_R   51
#define R34_MIXOUT_CTRL_DAC_SPK_L 52
#define R36_MIXOUT_CTRR_DAC_SPK_R 54

#define R35_MIXOUT_CTRL_DAC_SPK_L_VOL 53
#define R37_MIXOUT_CTRR_DAC_SPK_R_VOL 55

/* Bits common to Reg50/51/52 */
#define B03_DACL_TO_MIXOUT      3
#define B02_DACR_TO_MIXOUT      2
#define B01_BYPASSL_TO_MIXOUT   1
#define B00_BYPASSR_TO_MIXOUT   0

#define R39_HPOUTL_CTRL         57
#define B08_HPL_MUTE            8
#define B07_HPOUTVU             7
#define B06_HPOUTLZC            6
#define B00_HPOUTL_VOL          0

#define R3A_HPOUTR_CTRL         58
#define B08_HPR_MUTE            8
#define B06_HPOUTRZC            6
#define B00_HPOUTR_VOL          0

#define R3B_LINEOUTL_CTRL       59
#define B08_LINEOUTL_MUTE       8
#define B07_LINEOUTVU           7
#define B06_LINEOUTLZC          6
#define B00_LINEOUTL_VOL        0

#define R3C_LINEOUTR_CTRL       60
#define B08_LINEOUTR_MUTE       8
#define B06_LINEOUTRZC          6
#define B00_LINEOUTR_VOL        0


#define R3E_SPKL_CTRL           62
#define B08_SPKL_MUTE           8
#define B07_SPKVU               7
#define B06_SPKLZC              6
#define B00_SPKL_VOL            0

#define R3F_SPKR_CTRL           63
#define B08_SPKR_MUTE           8
#define B06_SPKRZC              6
#define B00_SPKR_VOL            0

#define R3E_ANALOG_OUT3_L       62
#define R3F_ANALOG_OUT3_R       63
#define B08_OUT_MUTE            8
#define B07_OUT_VU              7
#define B06_OUT_ZC              6
#define B00_OUT_VOL             0

#define R43_DC_SERVO_0          67
#define B04_DCS_MASTER_ENA      4
#define B00_DCS_ENA             0

#define R5E_ANALOG_LINEOUT      94
#define B07_LINEOUTL_RMV_SHORT  7
#define B06_LINEOUTL_ENA_OUTP   6
#define B05_LINEOUTL_ENA_DLY    5
#define B04_LINEOUTL_ENA        4
#define B03_LINEOUTR_RMV_SHORT  3
#define B02_LINEOUTR_ENA_OUTP   2
#define B01_LINEOUTR_ENA_DLY    1
#define B00_LINEOUTR_ENA        0

#define R68_CLASS_H             104
#define B01_CP_DYN_FREQ         1
#define B00_CP_DYN_V            0

#define R6C_WSEQ_0              108
#define B08_WSEQ_ENA            8
#define R6D_WSEQ_1              109
#define R6E_WSEQ_2              110

#define R6F_WSEQ_3              111
#define B09_WSEQ_ABORT          9
#define B08_WSEQ_START          8
#define B00_WSEQ_START_INDEX    0

#define R70_WSEQ_4              112
#define B04_WSEQ_CURRENT_INDEX  4
#define B00_WSEQ_BUSY           0

#define R74_GPIO_CTRL_1         116
#define R75_GPIO_CTRL_2         117
#define R76_GPIO_CTRL_3         118
#define R77_GPIO_CTRL_4         119     /* Interupt */
#define B08_GPIO_FN             8
#define B07_GPIO_DIR            7
#define B06_GPIO_OP_CFG         6
#define B05_GPIO_IP_CFG         5
#define B04_GPIO_LVL            4
#define B03_GPIO_PD             3
#define B02_GPIO_PU             2
#define B01_GPIO_INTMODE        1
#define B00_GPIO_DB             0

#define R79_INTERRUPT_STATUS    121
#define R7A_INTERRUPT_MASK      122
/* Common for R79/R7A */
#define B14_MICDEC_EINT         14
#define B03_GPIO5_EINT          4
#define B03_GPIO4_EINT          3
#define B02_GPIO3_EINT          2
#define B01_GPIO2_EINT          1
#define B00_GPIO1_EINT          0

#define R7B_INTERRUPT_POLARITY  123
#define B15_MICSHRT_INV         15
#define B14_MICDET_INV          14

#define R7E_INTERRUPT_CTRL      126
#define B00_IRQ_POL             0

#define R81_TEST_KEY            129
#define B01_USER_KEY            1
#define B00_TEST_KEY            0

#define RA4_ADC_DIG_MIC         164
#define B09_DIGMIC              9

#define RA5_ADC_NEG_RETIME      165
#define B02_RA5_UNKNOWN         2

#define RA6_ADC_TEST_0          166
#define B01_ADC_BIASXOP5        1

#define RA7_ADC_DITHER          167


/* Setting a value to a register
* r- registerval m- maskbit l-locationbit v-valuetoset
*/
#define SET_REG_VAL(r,m,l,v) (((r)&(~((m)<<(l))))|(((v)&(m))<<(l)))

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

    NvU32 LineOutVolLeft;
    NvU32 LineOutVolRight;

    NvU32 SpeakerVolLeft;
    NvU32 SpeakerVolRight;

    NvU32 AdcSamplingRate;
    NvU32 DacSamplingRate;

    NvU32 AdcBitsPerSample;
    NvU32 DacBitsPerSample;

    NvU32 LineInVolLeft;
    NvU32 LineInVolRight;

} WolfsonCodecControls;


#define W8903_GPIO_INTERRUPT_HP_DET       0
#define W8903_GPIO_INTERRUPT_CDC_IRQ      1
#define W8903_GPIO_INTERRUPT_NUMBER       2

typedef struct W8903GpioInterruptRec
{
    NvOdmGpioPinHandle hPin;
    NvOdmServicesGpioIntrHandle hInterrupt;
    NvOdmOsSemaphoreHandle hSemaphore;
    NvOdmOsThreadHandle hThread;
    volatile NvU32 ThreadExit;

} W8903GpioInterrupt;


typedef struct W8903AudioCodecRec
{
    NvOdmIoModule IoModule;
    NvU32 InstanceId;
    NvOdmServicesI2cHandle hOdmService;
    WolfsonCodecControls WCodecControl;
    NvOdmQueryI2sACodecInterfaceProp WCodecInterface;
    NvU32 RegValues[173];
    NvU32 PowerupComplete;
    NvU64 CodecGuid;

    NvU32 SpeakerEnabled;
    NvU32 HeadphoneEnabled;
    NvU32 MicEnabled;

    NvOdmServicesGpioHandle hGpio;
    W8903GpioInterrupt GpioInterrupt[W8903_GPIO_INTERRUPT_NUMBER];

    NvOdmAudioConfigInOutSignalCallback IoSignalChange;

} W8903AudioCodec, *W8903AudioCodecHandle;


static W8903AudioCodec s_W8903;       /* Unique audio codec for the whole system*/

static void DumpWM8903(W8903AudioCodecHandle hOdmAudioCodec);
/*static void PingWM8903(W8903AudioCodecHandle hOdmAudioCodec); fix linux compiler error */
static NvBool EnableInput( W8903AudioCodecHandle hOdmAudioCodec, NvU32  IsMic, NvBool IsPowerOn);
static void HeadphoneDetectThread(void *arg);
static void CleanupInterrupt(NvOdmPrivAudioCodecHandle hOdmAudioCodec);
static NvBool SetHeadphoneState(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 Enable);
static NvBool SetSpeakerState(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 Enable);
static NvBool
SetDataFormat(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmQueryI2sDataCommFormat DataFormat);

static NvBool
ReadWolfsonRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    WCodecRegIndex RegIndex,
    NvU32 *Data)
{
    NvOdmI2cStatus I2cTransStatus = NvOdmI2cStatus_Timeout;
    NvOdmI2cTransactionInfo TransactionInfo[2];
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
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
            W8903_I2C_SCLK, W8903_I2C_TIMEOUT);


    *Data = (ReadBuffer[0] << 8) | ReadBuffer[1];
    if (I2cTransStatus == 0)
    {
        NVODMAUDIOCODEC8903_PRINTF(("Read %d = 0X%04X\n", RegIndex, *Data));
        hAudioCodec->RegValues[RegIndex] = *Data;
    }
    else
        NVODMAUDIOCODEC8903_PRINTF(("0x%02x Read Error: %08x\n", RegIndex, I2cTransStatus));

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
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
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
    I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, W8903_I2C_SCLK,
                    W8903_I2C_TIMEOUT);

    // HW- BUG!! If timeout, again retransmit the data.
    if (I2cTransStatus == NvOdmI2cStatus_Timeout)
        I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, W8903_I2C_SCLK,
                        W8903_I2C_TIMEOUT);

    if (I2cTransStatus)
        NVODMAUDIOCODEC8903_PRINTF(("\t --- Failed(0x%08x)\n", I2cTransStatus));
    else
    {
        NVODMAUDIOCODEC8903_PRINTF(("Write 0X%02X = 0X%04X", RegIndex, Data));
        hAudioCodec->RegValues[RegIndex] = Data;
    }

    return (NvBool)(I2cTransStatus == 0);
}

static NvBool
W8903_ReadRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    WCodecRegIndex RegIndex,
    NvU32 *Data)
{
    return ReadWolfsonRegister(hOdmAudioCodec, RegIndex, Data);
}

static NvBool
W8903_WriteRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    WCodecRegIndex RegIndex,
    NvU32 Data)
{
    return WriteWolfsonRegister(hOdmAudioCodec, RegIndex, Data);
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
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NVODMAUDIOCODEC8903_PRINTF(("Headphone Volume inputs: LeftVol: x%x  RightVol: x%x Mask: %x\n", LeftVolume, RightVolume, ChannelMask));

    // a volume of 100 is +6dB and a register value of x3f (highest setting)
    // a volume of 91 is 0dB and a register value of x39 (no gain, no attenuation)
    // a volume of 82 is -6dB and a register value of x33 (our setting so that loopback runs without clipping, x34 should be ok also)
    // a volume of 2 or less is -57dB and a register value of x0 (lowest setting)
    LeftVol  =  LeftVolume <= 2 ? 0 : (LeftVolume >= 100 ? 0x3f : ((LeftVolume * 64) / 100 - 1));
    RightVol =  RightVolume <= 2 ? 0 : (RightVolume >= 100 ? 0x3f : ((RightVolume * 64) / 100 - 1));
    NVODMAUDIOCODEC8903_PRINTF(("Headphone Volume values: LeftVol: x%x  RightVol: x%x\n", LeftVol, RightVol));

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
        LeftVolReg = (LeftVol<<B00_HPOUTL_VOL);
    else
        LeftVolReg = (hAudioCodec->WCodecControl.HPOutVolLeft<<B00_HPOUTL_VOL);

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
        RightVolReg = (RightVol<<B00_HPOUTR_VOL);
    else
        RightVolReg = (hAudioCodec->WCodecControl.HPOutVolRight<<B00_HPOUTR_VOL);

    NVODMAUDIOCODEC8903_PRINTF(("Headphone Volume regs:   LeftVol: x%x  RightVol: x%x\n", LeftVolReg, RightVolReg));
    /* FIXME: please enable zero-cross if necessary */
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R39_HPOUTL_CTRL, LeftVolReg);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R3A_HPOUTR_CTRL, RightVolReg | (1<<B07_HPOUTVU));

    hAudioCodec->WCodecControl.HPOutVolLeft  = LeftVolReg;
    hAudioCodec->WCodecControl.HPOutVolRight = RightVolReg;

    return IsSuccess;
}

static NvBool
SetLineOutVolume(
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
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NVODMAUDIOCODEC8903_PRINTF(("Line Out Volume inputs: LeftVol: x%x  RightVol: x%x Mask: %x\n", LeftVolume, RightVolume, ChannelMask));

    // a volume of 100 is +6dB and a register value of x3f (highest setting)
    // a volume of 91 is 0dB and a register value of x39 (no gain, no attenuation)
    // a volume of 82 is -6dB and a register value of x33 (our setting so that loopback runs without clipping, x34 should be ok also)
    // a volume of 2 or less is -57dB and a register value of x0 (lowest setting)
    LeftVol  =  LeftVolume <= 2 ? 0 : (LeftVolume >= 100 ? 0x3f : ((LeftVolume * 64) / 100 - 1));
    RightVol =  RightVolume <= 2 ? 0 : (RightVolume >= 100 ? 0x3f : ((RightVolume * 64) / 100 - 1));
    NVODMAUDIOCODEC8903_PRINTF(("Line Out Volume values: LeftVol: x%x  RightVol: x%x\n", LeftVol, RightVol));

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
        LeftVolReg = (LeftVol<<B00_LINEOUTL_VOL);
    else
        LeftVolReg = (hAudioCodec->WCodecControl.LineOutVolLeft<<B00_LINEOUTL_VOL);

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
        RightVolReg = (RightVol<<B00_LINEOUTR_VOL);
    else
        RightVolReg = (hAudioCodec->WCodecControl.LineOutVolRight<<B00_LINEOUTR_VOL);

    NVODMAUDIOCODEC8903_PRINTF(("Line Out Volume regs:   LeftVol: x%x  RightVol: x%x\n", LeftVolReg, RightVolReg));
    /* FIXME: please enable zero-cross if necessary */
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R3B_LINEOUTL_CTRL, LeftVolReg);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R3C_LINEOUTR_CTRL, RightVolReg | (1<<B07_LINEOUTVU));

    hAudioCodec->WCodecControl.LineOutVolLeft  = LeftVolReg;
    hAudioCodec->WCodecControl.LineOutVolRight = RightVolReg;

    return IsSuccess;
}


static NvBool
SetSpeakerVolume(
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
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NVODMAUDIOCODEC8903_PRINTF(("Speaker Volume inputs: LeftVol: x%x  RightVol: x%x Mask: %x\n", LeftVolume, RightVolume, ChannelMask));

    // a volume of 100 is +6dB and a register value of x3f (highest setting)
    // a volume of 91 is 0dB and a register value of x39 (no gain, no attenuation)
    // a volume of 82 is -6dB and a register value of x33 (our setting so that loopback runs without clipping, x34 should be ok also)
    // a volume of 2 or less is -57dB and a register value of x0 (lowest setting)
    LeftVol  =  LeftVolume <= 2 ? 0 : (LeftVolume >= 100 ? 0x3f : ((LeftVolume * 64) / 100 - 1));
    RightVol =  RightVolume <= 2 ? 0 : (RightVolume >= 100 ? 0x3f : ((RightVolume * 64) / 100 - 1));
    NVODMAUDIOCODEC8903_PRINTF(("Speaker Volume values: LeftVol: x%x  RightVol: x%x\n", LeftVol, RightVol));

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
        LeftVolReg = (LeftVol<<B00_SPKL_VOL);
    else
        LeftVolReg = (hAudioCodec->WCodecControl.SpeakerVolLeft<<B00_SPKL_VOL);

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
        RightVolReg = (RightVol<<B00_SPKR_VOL);
    else
        RightVolReg = (hAudioCodec->WCodecControl.SpeakerVolRight<<B00_SPKR_VOL);

    NVODMAUDIOCODEC8903_PRINTF(("Speaker Volume regs:   LeftVol: x%x  RightVol: x%x\n", LeftVolReg, RightVolReg));
    /* FIXME: please enable zero-cross if necessary */
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R3E_SPKL_CTRL, LeftVolReg);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R3F_SPKR_CTRL, RightVolReg | (1<<B07_SPKVU));

    hAudioCodec->WCodecControl.SpeakerVolLeft  = LeftVolReg;
    hAudioCodec->WCodecControl.SpeakerVolRight = RightVolReg;

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
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NVODMAUDIOCODEC8903_PRINTF(("LineIn Volume inputs: LeftVol: x%x  RightVol: x%x Mask: %x\n", LeftVolume, RightVolume, ChannelMask));

    /* FIXME: check Left/Right Volume range */
    LeftVol  =  ((LeftVolume *64)/100 - 1);
    RightVol =  ((RightVolume*64)/100 - 1);
    NVODMAUDIOCODEC8903_PRINTF(("LineIn Volume values: LeftVol: x%x  RightVol: x%x\n", LeftVol, RightVol));

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
        LeftVolReg = LeftVol;
    else
        LeftVolReg = hAudioCodec->WCodecControl.LineInVolLeft;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
        RightVolReg = RightVol;
    else
        RightVolReg = hAudioCodec->WCodecControl.LineInVolRight;

    NVODMAUDIOCODEC8903_PRINTF(("LineIn Volume regs:   LeftVol: x%x   RightVol: x%x\n", LeftVolReg, RightVolReg));
    /* FIXME: the volume control is done inside ADC, PGA is not enabled yet */
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, 0x24, LeftVolReg);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, 0x25, RightVolReg | (1<<7));

    hAudioCodec->WCodecControl.LineInVolLeft = LeftVolReg;
    hAudioCodec->WCodecControl.LineInVolRight= RightVolReg;

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
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;

    if (IsMute)
    {
        LeftVol = 0x0;
        RightVol = 0x0;
    }
    else
    {
        LeftVol = hAudioCodec->WCodecControl.HPOutVolLeft;
        RightVol = hAudioCodec->WCodecControl.HPOutVolRight;
    }

    return SetHeadphoneOutVolume(hOdmAudioCodec, ChannelMask, LeftVol, RightVol);
}

static NvBool
SetLineOutMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask,
    NvBool IsMute)
{
    NvU32 LeftVol;
    NvU32 RightVol;
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;

    if (IsMute)
    {
        LeftVol = 0x0;
        RightVol = 0x0;
    }
    else
    {
        LeftVol = hAudioCodec->WCodecControl.LineOutVolLeft;
        RightVol = hAudioCodec->WCodecControl.LineOutVolRight;
    }

    return SetLineOutVolume(hOdmAudioCodec, ChannelMask, LeftVol, RightVol);
}

static NvBool
SetSpeakerMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask,
    NvBool IsMute)
{
    NvU32 LeftVol;
    NvU32 RightVol;
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;

    if (IsMute)
    {
        LeftVol = 0x0;
        RightVol = 0x0;
    }
    else
    {
        LeftVol  = hAudioCodec->WCodecControl.SpeakerVolLeft;
        RightVol = hAudioCodec->WCodecControl.SpeakerVolRight;
    }

    return SetSpeakerVolume(hOdmAudioCodec, ChannelMask, LeftVol, RightVol);
}
static NvBool
SetLineInMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask,
    NvBool IsMute)
{
    NvU32 LeftVol;
    NvU32 RightVol;
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;

    if (IsMute)
    {
        LeftVol = 0x0;
        RightVol = 0x0;
    }
    else
    {
        LeftVol = hAudioCodec->WCodecControl.LineInVolLeft;
        RightVol = hAudioCodec->WCodecControl.LineInVolRight;
    }

    return SetLineInVolume(hOdmAudioCodec, ChannelMask, LeftVol, RightVol);
}

static NvBool SetMicrophoneInVolume(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 MicGain)
{
    NvBool IsSuccess = NV_TRUE;
#if 0
    NvU32 CtrReg = 0;
    /* Enable Mic Max Gain */
    W8903_ReadRegister(hOdmAudioCodec, R29_DRC_1, &CtrReg);
    CtrReg &= ~(0x3);
    if (MicGain >= 50)
        CtrReg |= 0x3;  /* 36dB */
    else
        CtrReg |= 0x3;  /* 18dB(default) */

    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R29_DRC_1, CtrReg);
#endif

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
    return W8903_WriteRegister(hOdmAudioCodec, R6F_WSEQ_3, (0x20<<B00_WSEQ_START_INDEX)|(1<<B08_WSEQ_START)|(0<<B09_WSEQ_ABORT));
}



static NvBool ResetCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    /* Reset 8903 */
    IsSuccess = W8903_WriteRegister(hOdmAudioCodec, 0, 0);
    // Read the state of all of our registers
    DumpWM8903(hOdmAudioCodec);
    return IsSuccess;
}

static NvBool SetCodecPower(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 IsBusy = 1;
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;

    // Never power up if we are already powered up, or power down if we are already powered down!
    if ((IsPowerOn && hAudioCodec->PowerupComplete) ||
        (!IsPowerOn && !hAudioCodec->PowerupComplete))
      return IsSuccess;

    if (IsPowerOn)
    {
        /* START-UP */
        //NvOsDebugPrintf("Wolfson WM8903.....Begin Powering UP!!!!\n");
        IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R6C_WSEQ_0, (1<<B08_WSEQ_ENA));
        IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R16_CLOCK_RATES_2, (1<<B02_CLK_SYS_ENA));
        IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R6F_WSEQ_3, (0x0<<B00_WSEQ_START_INDEX) | (1<<B08_WSEQ_START) | (0x0<<B09_WSEQ_ABORT));
        hAudioCodec->PowerupComplete = 1;
    }
    else
    {
        /* SHUTDOWN */
        //NvOsDebugPrintf("Wolfson WM8903.....Begin Powering DOWN!!!!\n");
        IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R6F_WSEQ_3, (0x20<<B00_WSEQ_START_INDEX) | (1<<B08_WSEQ_START) | (0x0<<B09_WSEQ_ABORT));
        hAudioCodec->PowerupComplete = 0;
    }

    do
    {
        NvOdmOsSleepMS(50);
        W8903_ReadRegister(hOdmAudioCodec, R70_WSEQ_4, &IsBusy);

    } while(IsBusy & (1 << B00_WSEQ_BUSY));

    //NvOsDebugPrintf("Wolfson WM8903.....Complete Powering Codec!!!!\n");
    return IsSuccess;
}


static NvBool InitializeDigitalInterface(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 AudioIF1Reg = 0;
    NvU32 SampleContReg = 0;
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;

    /* Set MCLK */
    AudioIF1Reg = (0x0<<B00_MCLKDIV2);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R14_CLOCK_RATES_0, AudioIF1Reg);

    /* USB mode
     * 00: Std Mode
     * 01/10/11: USB Mode */
    if (hAudioCodec->WCodecInterface.IsUsbMode)
        SampleContReg = (0x7 << B00_SAMPLE_RATE) | (0x1 <<B08_CLK_SYS_MODE) | (0x3<<B10_CLK_SYS_RATE);
    else
        SampleContReg = (0x7 << B00_SAMPLE_RATE) | (0x0 <<B08_CLK_SYS_MODE) | (0x3<<B10_CLK_SYS_RATE);

    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R15_CLOCK_RATES_1, SampleContReg);

    AudioIF1Reg = 0;
    /* Master Mode */
    if (hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        NVODMAUDIOCODEC8903_PRINTF(("Codec is working on MASTER mode\n"));
        AudioIF1Reg |= (1<<B09_LRCLK_DIR) | (1<<B06_BCLK_DIR);   /* Master Mode */
    }
    else
    {
        NVODMAUDIOCODEC8903_PRINTF(("Codec is working on SLAVE mode\n"));
        AudioIF1Reg |= (0<<B09_LRCLK_DIR) | (0<<B06_BCLK_DIR);   /* Slave Mode */
    }
    /* LRLine Control, defines the Left/Right channel polarity */
    if (hAudioCodec->WCodecInterface.I2sCodecLRLineControl == NvOdmQueryI2sLRLineControl_LeftOnLow)
        AudioIF1Reg |= (0x0<<B04_AIF_LRCLK_INV);
    else
        AudioIF1Reg |= (0x1<<B04_AIF_LRCLK_INV);

    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R19_AUDIO_INTERFACE_1, AudioIF1Reg);

    IsSuccess &= SetDataFormat(
        hOdmAudioCodec,
        hAudioCodec->WCodecInterface.I2sCodecDataCommFormat);

    return IsSuccess;
}

static NvBool InitializeSpeaker(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R76_GPIO_CTRL_3,
            (1<<B04_GPIO_LVL) | (1<<B05_GPIO_IP_CFG)| (1<<B00_GPIO_DB) | (1<<B01_GPIO_INTMODE));

    return IsSuccess;
}

static NvBool InitializeHeadphoneOut(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    IsSuccess &= SetHeadphoneOutVolume(hOdmAudioCodec,
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                82, 82); // Our initial setting which yeilds -6dB, so that loopback can work.

    return IsSuccess;
}
/*
../nvodm_codec_wolfson8903.c:637: warning: `InitializeLineInput' defined but not used'
static NvBool InitializeLineInput(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, 0x0C, 0x03); */  /* left/right PGA enable */
/*

    return IsSuccess;
}
*/

static NvBool SetSpeakerState(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 Enable)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NvBool IsSuccess   = NV_TRUE;
    NvU32 MixSpkReg = hAudioCodec->RegValues[R10_MIXSPK];
    NvU32 SpkOutL = hAudioCodec->RegValues[R3E_ANALOG_OUT3_L];
    NvU32 SpkOutR = hAudioCodec->RegValues[R3F_ANALOG_OUT3_R];
    NvU32 SpkEnable = 0;
    NvU32 Gpio3Enable = 0;

    if (Enable)
    {
        MixSpkReg |= (1<<B01_MIXSPKL_ENA) | (1<<B00_MIXSPKR_ENA);
        SpkOutL &= ~(1<<B08_OUT_MUTE);
        SpkOutR &= ~(1<<B08_OUT_MUTE);
        SpkEnable = (1<<B00_SPKR_ENA) | (1<<B01_SPKL_ENA);
        Gpio3Enable = (1<<B04_GPIO_LVL) | (1<<B05_GPIO_IP_CFG)| (1<<B00_GPIO_DB) | (1<<B01_GPIO_INTMODE);
        NVODMAUDIOCODEC8903_PRINTF(("Speaker enabled\n"));
    }

    else
    {
        MixSpkReg &= ~(1<<B01_MIXSPKL_ENA) | (1<<B00_MIXSPKR_ENA);
        SpkEnable = 0;      /* disable speaker */

        SpkOutL |= (1<<B08_OUT_MUTE);
        SpkOutR |= (1<<B08_OUT_MUTE);

        Gpio3Enable = 0x0;
        NVODMAUDIOCODEC8903_PRINTF(("Speaker disabled\n"));
    }

    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R10_MIXSPK, MixSpkReg);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R3E_ANALOG_OUT3_L, SpkOutL);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R3F_ANALOG_OUT3_R, SpkOutR | (1<<B07_SPKVU));
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R11_SPK, SpkEnable);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R76_GPIO_CTRL_3, Gpio3Enable);

    hAudioCodec->SpeakerEnabled = Enable;
    return IsSuccess;
}


static NvBool SetHeadphoneState(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 Enable)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NvBool IsSuccess = NV_TRUE;
    NvU32 MixOutReg = hAudioCodec->RegValues[B00_MIXOUTR_ENA];

    if (Enable)
    {
        MixOutReg |= (1<<B01_MIXOUTL_ENA) | (1<<B00_MIXOUTR_ENA);
        NVODMAUDIOCODEC8903_PRINTF(("Headphone enabled\n"));
    }

    else
    {
        MixOutReg &= ~(1<<B01_MIXOUTL_ENA) | (1<<B00_MIXOUTR_ENA);
        NVODMAUDIOCODEC8903_PRINTF(("Headphone disabled\n"));
    }

    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R0D_MIXOUT, MixOutReg);
    return IsSuccess;
}


static NvBool InitializeAnalogAudioPath(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NvU32 DacCtrlReg = 0;
    NvU32 MixOutCtrlDacL = 0;
    NvU32 MixOutCtrlDacR = 0;
    NvU32 MixOutCtrlDacSpkL = 0;
    NvU32 MixOutCtrlDacSpkR = 0;
    NvBool IsSuccess = NV_TRUE;

    /* Enable DAC L/R to Mixer L/R */
    MixOutCtrlDacL  = (1<<B03_DACL_TO_MIXOUT) | (0<<B02_DACR_TO_MIXOUT) | (0<<B01_BYPASSL_TO_MIXOUT) | (0<<B00_BYPASSR_TO_MIXOUT);
    MixOutCtrlDacR  = (0<<B03_DACL_TO_MIXOUT) | (1<<B02_DACR_TO_MIXOUT) | (0<<B01_BYPASSL_TO_MIXOUT) | (0<<B00_BYPASSR_TO_MIXOUT);

    MixOutCtrlDacSpkL= (1<<B03_DACL_TO_MIXOUT) | (0<<B02_DACR_TO_MIXOUT) | (0<<B01_BYPASSL_TO_MIXOUT) | (0<<B00_BYPASSR_TO_MIXOUT);
    MixOutCtrlDacSpkR= (0<<B03_DACL_TO_MIXOUT) | (1<<B02_DACR_TO_MIXOUT) | (0<<B01_BYPASSL_TO_MIXOUT) | (0<<B00_BYPASSR_TO_MIXOUT);

    /* Enable L/R DAC */
    W8903_ReadRegister(hOdmAudioCodec, R12_ADCDAC_ENA, &DacCtrlReg);
    DacCtrlReg |= (1<<B03_DACL_ENA) | (1<<B02_DACR_ENA);
    if (DacCtrlReg !=  ((W8903AudioCodecHandle)hOdmAudioCodec)->RegValues[R12_ADCDAC_ENA])
    {
        IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R12_ADCDAC_ENA, DacCtrlReg);
        NvOdmOsSleepMS(2);
    }

    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R32_MIXOUT_CTRL_DAC_L, MixOutCtrlDacL);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R33_MIXOUT_CTRL_DAC_R, MixOutCtrlDacR);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R34_MIXOUT_CTRL_DAC_SPK_L, MixOutCtrlDacSpkL);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R35_MIXOUT_CTRL_DAC_SPK_L_VOL, MixOutCtrlDacSpkL);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R36_MIXOUT_CTRR_DAC_SPK_R, MixOutCtrlDacSpkR);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R37_MIXOUT_CTRR_DAC_SPK_R_VOL, MixOutCtrlDacSpkR);

    //By default set speakers to max volume as they are very quiet
    hAudioCodec->RegValues[R3E_ANALOG_OUT3_L] = 0x3f;
    hAudioCodec->RegValues[R3F_ANALOG_OUT3_R] = 0x3f;

    IsSuccess &= SetSpeakerState(hOdmAudioCodec, 1);

    if (hAudioCodec->HeadphoneEnabled)
    {
        IsSuccess &= SetHeadphoneState(hOdmAudioCodec, 1);
    }

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
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NvBool IsSuccess = NV_TRUE;
    NvU32 BitsPerSampleReg = 0;

    if (!hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        hAudioCodec->WCodecControl.AdcBitsPerSample = PcmSize;
        hAudioCodec->WCodecControl.DacBitsPerSample = PcmSize;
        return NV_TRUE;
    }

    W8903_ReadRegister(hOdmAudioCodec, R19_AUDIO_INTERFACE_1, &BitsPerSampleReg);
    BitsPerSampleReg &= ~(0xC);

    switch (PcmSize)
    {
        case 16: BitsPerSampleReg |= (0x0 << B02_AIF_WL); break;
        case 20: BitsPerSampleReg |= (0x1 << B02_AIF_WL); break;
        case 24: BitsPerSampleReg |= (0x2 << B02_AIF_WL); break;
        case 32: BitsPerSampleReg |= (0x3 << B02_AIF_WL); break;
        default: return NV_FALSE;
    }

    IsSuccess = W8903_WriteRegister(hOdmAudioCodec, R19_AUDIO_INTERFACE_1, BitsPerSampleReg);
    if (IsSuccess)
    {
        hAudioCodec->WCodecControl.AdcBitsPerSample = PcmSize;
        hAudioCodec->WCodecControl.DacBitsPerSample = PcmSize;
    }

    return IsSuccess;
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
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;

    if (!hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        hAudioCodec->WCodecControl.AdcSamplingRate = SamplingRate;
        hAudioCodec->WCodecControl.DacSamplingRate = SamplingRate;
        return NV_TRUE;
    }

    W8903_ReadRegister(hOdmAudioCodec, R15_CLOCK_RATES_1, &SamplingContReg);
    SamplingContReg &= ~(0xF);

    /* FIXME: Should we set MCLKDIV2 for MCLK division? */
    switch (SamplingRate)
    {
        case 8000:  SamplingContReg |= (0x0 << B00_SAMPLE_RATE); break;
        case 32000: SamplingContReg |= (0x6 << B00_SAMPLE_RATE); break;
        case 44100: SamplingContReg |= (0x7 << B00_SAMPLE_RATE); break;
        case 48000: SamplingContReg |= (0x8 << B00_SAMPLE_RATE); break;
        case 88200: SamplingContReg |= (0x9 << B00_SAMPLE_RATE); break;
        case 96000: SamplingContReg |= (0xA << B00_SAMPLE_RATE); break;
        default:    return NV_FALSE;
    }

    IsSuccess = W8903_WriteRegister(hOdmAudioCodec, R15_CLOCK_RATES_1, SamplingContReg);
    if (IsSuccess)
    {
        hAudioCodec->WCodecControl.AdcSamplingRate = SamplingRate;
        hAudioCodec->WCodecControl.DacSamplingRate = SamplingRate;
    }

    return IsSuccess;
}

/*
 * Sets the Data Format.
 */
static NvBool
SetDataFormat(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmQueryI2sDataCommFormat DataFormat)
{
    NvU32 DataFormatReg = 0;
    /* Audio Data Format
    * 00: Right Justified
    * 01: Left Justified
    * 10: I2S
    * 11: DSP   */
    W8903_ReadRegister(hOdmAudioCodec, R19_AUDIO_INTERFACE_1, &DataFormatReg);

    switch (DataFormat)
    {
        case NvOdmQueryI2sDataCommFormat_Dsp:
            DataFormatReg = W8903_SET_REG_DEF(AUDIO_INTERFACE,
                FORMAT, DSP_MODE, DataFormatReg);
            break;
         case NvOdmQueryI2sDataCommFormat_LeftJustified:
            DataFormatReg = W8903_SET_REG_DEF(AUDIO_INTERFACE,
                FORMAT, LEFT_JUSTIFIED, DataFormatReg);
            break;
        case NvOdmQueryI2sDataCommFormat_RightJustified:
            DataFormatReg = W8903_SET_REG_DEF(AUDIO_INTERFACE,
                FORMAT, RIGHT_JUSTIFIED, DataFormatReg);
            break;
        case NvOdmQueryI2sDataCommFormat_I2S:
        default:
            // Default will be the i2s mode
            DataFormatReg  = W8903_SET_REG_DEF(AUDIO_INTERFACE,
                FORMAT, I2S_MODE, DataFormatReg);
            break;
    }
    return  W8903_WriteRegister(hOdmAudioCodec, R19_AUDIO_INTERFACE_1, DataFormatReg);
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

/*
 * Sets the output analog line from the DAC.
 */
static NvBool
SetOutputPortIO(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 PortId,
    NvOdmAudioConfigInOutSignal *pConfigIO)
{
    NvBool IsSuccess = NV_FALSE;

    if (PortId != 0) goto EXIT;
    if (pConfigIO->InSignalId != 0 || pConfigIO->OutSignalId != 0) goto EXIT;
    if (pConfigIO->InSignalType != NvOdmAudioSignalType_Digital_I2s) goto EXIT;

    if (pConfigIO->OutSignalType & NvOdmAudioSignalType_HeadphoneOut)
    {
        IsSuccess &= SetHeadphoneState(hOdmAudioCodec, pConfigIO->IsEnable);
    }

    if (pConfigIO->OutSignalType & NvOdmAudioSignalType_Speakers)
    {
        IsSuccess &= SetSpeakerState(hOdmAudioCodec, pConfigIO->IsEnable);
    }

EXIT:

    return IsSuccess;
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
    NvU32 CtrlReg = 0;

    NVODMAUDIOCODEC8903_PRINTF(("Inside Bypassing\n"));

    if ((InputLineId > 0) || (OutputLineId > 1))
        return NV_FALSE;

    /* Bias Ctrl */
    CtrlReg = (0x1<<B00_MICBIAS_ENA) | (0x1<<B01_MICDET_ENA);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R06_MICBIAS_CTRL_0, CtrlReg);

    if (InputAnalogLineType & NvOdmAudioSignalType_LineIn)
    {
        if (IsEnable) {
            /* Input Ctrl Enable */
            CtrlReg = (0x1<<B01_INL_ENA) | (0x1<<B00_INR_ENA);
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R0C_INPUT_SEL, CtrlReg);
            NV_ASSERT(CtrlReg == 0x3);

            /* Input Vol */
            CtrlReg = (0x5 << B00_IN_VOL);
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2C_ANALOG_INPUT_L_0, CtrlReg);
            CtrlReg = (0x5 << B00_IN_VOL);
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2D_ANALOG_INPUT_R_0, CtrlReg);
            NV_ASSERT(CtrlReg == 0x5);

            /* PGA Input */
            /* NOTE: Enable CM will consume more power */
            CtrlReg = (0x1<<B06_IN_CM_ENA) | (0x1<<B02_IP_SEL_P);
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2E_ANALOG_INPUT_L_1, CtrlReg);
            CtrlReg = (0x1<<B06_IN_CM_ENA) | (0x1<<B02_IP_SEL_P);
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2F_ANALOG_INPUT_R_1, CtrlReg);
            NV_ASSERT(CtrlReg == 0x44);
        }
    }

    if (InputAnalogLineType & NvOdmAudioSignalType_MicIn)
    {
        if (IsEnable)
        {
            /* FIXME: implement it */
        }
    }

    /* Out Mixer */
    CtrlReg = (0x1<<B01_BYPASSL_TO_MIXOUT);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R32_MIXOUT_CTRL_DAC_L, CtrlReg);
    NV_ASSERT(CtrlReg == 0x2);
    CtrlReg = (0x1<<B00_BYPASSR_TO_MIXOUT);
    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R33_MIXOUT_CTRL_DAC_R, CtrlReg);
    NV_ASSERT(CtrlReg == 0x1);

    /* Output Line Select */
    if (OutputLineId == 0)
    {
    }
    else if (OutputLineId == 1)
    {
    }

    return NV_TRUE;
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

    if (!IsEnabled)
    {
        SidetoneCtrlReg = (0x0<<B02_ADC_TO_DACL) | (0x0<<B00_ADC_TO_DACR);
        return W8903_WriteRegister(hOdmAudioCodec, R20_SIDETONE_CTRL, SidetoneCtrlReg);
    }

    if ((SideToneAtenuation > 0) && (SideToneAtenuation <= 100))
    {
        SidetoneCtrlReg = (0x1<<B02_ADC_TO_DACL) | (0x2<<B00_ADC_TO_DACR);
        SideToneAtenuation = 12 - ((12 * SideToneAtenuation) / 100);
        SidetoneCtrlReg |= (SideToneAtenuation<<B08_ADCL_DAC_SVOL) | (SideToneAtenuation<<B04_ADCR_DAC_SVOL);
        return W8903_WriteRegister(hOdmAudioCodec, R20_SIDETONE_CTRL, SidetoneCtrlReg);
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
    NvU32 ChannelMask,
    NvBool IsPowerOn)
{
    return NV_TRUE;
}


static void HeadphoneDetectThread(void *arg)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)arg;
    W8903GpioInterrupt* gpioInterrupt = &hAudioCodec->GpioInterrupt[W8903_GPIO_INTERRUPT_HP_DET];
    NvBool IsSuccess;
    NvU32 pinState;

    NVODMAUDIOCODEC8903_PRINTF(("Headphone detection thread started\n"));

    while (!gpioInterrupt->ThreadExit)
    {
        NVODMAUDIOCODEC8903_PRINTF(("Waiting for headphone detection\n"));

        // Wait for the level to get settled after interrupt
        NvOdmOsSemaphoreWait(gpioInterrupt->hSemaphore);
        if (gpioInterrupt->ThreadExit) break;

        // Wait 500ms to get rid of spurious interrupts from moving the connector in the jack before we really
        // figure out the state of the headphone.
        do
        {
            IsSuccess = NvOdmOsSemaphoreWaitTimeout(gpioInterrupt->hSemaphore, 500);

        } while (IsSuccess);

        NvOdmGpioGetState(hAudioCodec->hGpio, gpioInterrupt->hPin, &pinState);
        hAudioCodec->HeadphoneEnabled = !pinState;
        NVODMAUDIOCODEC8903_PRINTF(("Headphone state change. Headphone State:%s\n",
                                    hAudioCodec->HeadphoneEnabled ? "ENABLED" : "DISABLED"));

        if (hAudioCodec->IoSignalChange.Callback)
        {
            NvOdmAudioConfigInOutSignal ioSignal;

            NvOdmOsMemset(&ioSignal, 0, sizeof(NvOdmAudioConfigInOutSignal));
            ioSignal.OutSignalType = NvOdmAudioSignalType_HeadphoneOut;
            ioSignal.IsEnable = hAudioCodec->HeadphoneEnabled;

            hAudioCodec->IoSignalChange.Callback(hAudioCodec->IoSignalChange.pContext, &ioSignal);
        }
    }
}

static void CodecIrqThread(void *arg)
{
    NvOdmPrivAudioCodecHandle hOdmAudioCodec = (NvOdmPrivAudioCodecHandle)arg;
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)arg;
    W8903GpioInterrupt* gpioInterrupt = &hAudioCodec->GpioInterrupt[W8903_GPIO_INTERRUPT_CDC_IRQ];
    NvBool IsSuccess;
    NvU32 intStatus;

    NVODMAUDIOCODEC8903_PRINTF(("Codec IRQ thread started\n"));

    while (!gpioInterrupt->ThreadExit)
    {
        NVODMAUDIOCODEC8903_PRINTF(("Waiting for Codec IRQ.\n"));

        // Wait for the level to get settled after interrupt
        NvOdmOsSemaphoreWait(gpioInterrupt->hSemaphore);
        if (gpioInterrupt->ThreadExit) break;

        // Wait 500ms to get rid of spurious interrupts from moving the connector in the jack before we really
        // figure out the state of the headphone.
        do
        {
            IsSuccess = NvOdmOsSemaphoreWaitTimeout(gpioInterrupt->hSemaphore, 500);

        } while (IsSuccess);


        IsSuccess &= W8903_ReadRegister(hOdmAudioCodec, R79_INTERRUPT_STATUS, &intStatus);
        NVODMAUDIOCODEC8903_PRINTF(("Codec IRQ received. intStatus:0x%08x\n", intStatus));
        if (IsSuccess)
        {
            if (intStatus && (1<<B14_MICDEC_EINT))
            {
                NvU32 intPolarity = hAudioCodec->RegValues[R7B_INTERRUPT_POLARITY];
                if (intPolarity & (1<<B14_MICDET_INV))
                {
                    NVODMAUDIOCODEC8903_PRINTF(("Mic removed.\n"));
                    intPolarity &= ~(1<<B14_MICDET_INV);
                    hAudioCodec->MicEnabled = 0;
                }

                else
                {
                    NVODMAUDIOCODEC8903_PRINTF(("Mic attached.\n"));
                    intPolarity |= (1<<B14_MICDET_INV);
                    hAudioCodec->MicEnabled = 1;
                }

                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R7B_INTERRUPT_POLARITY, intPolarity);

                if (hAudioCodec->IoSignalChange.Callback)
                {
                    NvOdmAudioConfigInOutSignal ioSignal;

                    NvOdmOsMemset(&ioSignal, 0, sizeof(NvOdmAudioConfigInOutSignal));
                    ioSignal.InSignalType = NvOdmAudioSignalType_MicIn;
                    ioSignal.InSignalId = 1;
                    ioSignal.IsEnable = hAudioCodec->MicEnabled;

                    hAudioCodec->IoSignalChange.Callback(hAudioCodec->IoSignalChange.pContext, &ioSignal);
                }
            }
        }
    }
}

static void GpioIsr(void *arg)
{
    W8903GpioInterrupt* gpioInterrupt = (W8903GpioInterrupt*)arg;
    NvOdmOsSemaphoreSignal(gpioInterrupt->hSemaphore);
    NvOdmGpioInterruptDone(gpioInterrupt->hInterrupt);
}

static NvBool SetupInterrupt(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NvU32 GpioPort = 0;
    NvU32 GpioPin = 0;
    W8903GpioInterrupt* gpioInterrupt = 0;
    NvU32 i;
    const NvOdmPeripheralConnectivity* pPerConnectivity = NULL;
    NvBool IsSuccess = NV_TRUE;

    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(hAudioCodec->CodecGuid);
    if (pPerConnectivity == NULL)
    {
        goto EXIT_WITH_ERROR;
    }

    hAudioCodec->hGpio = NvOdmGpioOpen();

    if (!hAudioCodec->hGpio)
    {
        goto EXIT_WITH_ERROR;
    }

    // Search for the Vdd rail and set the proper volage to the rail.
    for (i = 0; i < pPerConnectivity->NumAddress; i++)
    {
        if (pPerConnectivity->AddressList[i].Interface != NvOdmIoModule_Gpio)
        {
            continue;
        }

        GpioPort = pPerConnectivity->AddressList[i].Instance;
        GpioPin = pPerConnectivity->AddressList[i].Address;
        if (!GpioPin)
        {
            goto EXIT_WITH_ERROR;
        }

        // We only support to codec GPIO interrupts, one for headphone detection and one generic interrupt.
        NV_ASSERT((GpioPort == (NvU32)('w'-'a') && GpioPin == (NvU32)(2)) ||
                  (GpioPort == (NvU32)('x'-'a') && GpioPin == (NvU32)(3)));

        gpioInterrupt = (GpioPin == (NvU32)(2) ? &hAudioCodec->GpioInterrupt[W8903_GPIO_INTERRUPT_HP_DET] :
                         &hAudioCodec->GpioInterrupt[W8903_GPIO_INTERRUPT_CDC_IRQ]);

        gpioInterrupt->hPin = NvOdmGpioAcquirePinHandle(hAudioCodec->hGpio, GpioPort, GpioPin);
        if (!gpioInterrupt->hPin)
        {
            goto EXIT_WITH_ERROR;
        }

        NvOdmGpioConfig(hAudioCodec->hGpio, gpioInterrupt->hPin, NvOdmGpioPinMode_InputData);

        gpioInterrupt->hSemaphore = NvOdmOsSemaphoreCreate(0);
        if (!gpioInterrupt->hSemaphore)
        {
            goto EXIT_WITH_ERROR;
        }

        gpioInterrupt->hThread = NvOdmOsThreadCreate(GpioPin == 2 ? HeadphoneDetectThread : CodecIrqThread, (void*)hOdmAudioCodec);
        if (!gpioInterrupt->hThread)
        {
            goto EXIT_WITH_ERROR;
        }

        // Setup the Interrupt Routine
        if (!NvOdmGpioInterruptRegister(hAudioCodec->hGpio, &gpioInterrupt->hInterrupt, gpioInterrupt->hPin,
                                        NvOdmGpioPinMode_InputInterruptRisingEdge, GpioIsr, gpioInterrupt, 0))
        {
            goto EXIT_WITH_ERROR;
        }
    }

    {
        NvU32 pinState;
        NvOdmGpioGetState(hAudioCodec->hGpio, hAudioCodec->GpioInterrupt[W8903_GPIO_INTERRUPT_HP_DET].hPin, &pinState);
        hAudioCodec->HeadphoneEnabled = !pinState;
        NVODMAUDIOCODEC8903_PRINTF(("Initial headphone state:%s\n",
                                    hAudioCodec->HeadphoneEnabled ? "ENABLED" : "DISABLED"));
    }

    {
#if 0
        NvU32 intMask = hAudioCodec->RegValues[R7A_INTERRUPT_MASK];
        NvU32 micBiasCtrl = hAudioCodec->RegValues[R06_MICBIAS_CTRL_0];

        NVODMAUDIOCODEC8903_PRINTF(("Setting up mic detection\n"));

        intMask &= ~(1<<B14_MICDEC_EINT);
        W8903_WriteRegister(hOdmAudioCodec, R7A_INTERRUPT_MASK, intMask);

        micBiasCtrl = (0x1<<B00_MICBIAS_ENA) | (0x1<<B01_MICDET_ENA);
        W8903_WriteRegister(hOdmAudioCodec, R06_MICBIAS_CTRL_0, micBiasCtrl);
#endif
        // TODO: Hardcoded for now.
        hAudioCodec->MicEnabled = 1;
    }

    goto EXIT;


EXIT_WITH_ERROR:

    IsSuccess = NV_FALSE;
    CleanupInterrupt(hOdmAudioCodec);

EXIT:

    return IsSuccess;
}

static void CleanupInterrupt(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    W8903GpioInterrupt* gpioInterrupt = 0;
    NvU32 i;

    if (!hAudioCodec) return;

    for (i = 0; i < W8903_GPIO_INTERRUPT_NUMBER; i++)
    {
        gpioInterrupt = &hAudioCodec->GpioInterrupt[i];

        if (gpioInterrupt->hThread)
        {
            gpioInterrupt->ThreadExit = 1;
            NvOdmOsSemaphoreSignal(gpioInterrupt->hSemaphore);
            NvOdmOsThreadJoin(gpioInterrupt->hThread);
            gpioInterrupt->hThread = 0;
        }

        if (gpioInterrupt->hSemaphore)
        {
            NvOdmOsSemaphoreDestroy(gpioInterrupt->hSemaphore);
            gpioInterrupt->hSemaphore = 0;
        }

        if (gpioInterrupt->hInterrupt)
        {
            NvOdmGpioInterruptUnregister(hAudioCodec->hGpio, gpioInterrupt->hPin, gpioInterrupt->hInterrupt);
            gpioInterrupt->hInterrupt = 0;
        }

        if (gpioInterrupt->hPin)
        {
            NvOdmGpioReleasePinHandle(hAudioCodec->hGpio, gpioInterrupt->hPin);
            gpioInterrupt->hPin = 0;
        }
    }

    if (hAudioCodec->hGpio)
    {
        NvOdmGpioClose(hAudioCodec->hGpio);
        hAudioCodec->hGpio = 0;
    }
}

static NvBool OpenWolfsonCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    /* h/w verify for bringup */
    {
        NvU32 Data = 0;
        W8903_ReadRegister(hOdmAudioCodec, 0, &Data);   /* = 8903 */
    }

    // Reset the codec.
    IsSuccess = ResetCodec(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = SetCodecPower(hOdmAudioCodec, NV_TRUE);

    if (IsSuccess)
        // Ignoring the return value
        // Codec library being statically linked, calling the
        // Codec functions from test app will result in Gpio faillure if
        // Codec got initialized by the audio driver.
        (void)SetupInterrupt(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeDigitalInterface(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeAnalogAudioPath(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = SetMicrophoneInVolume(hOdmAudioCodec,1);

    if (IsSuccess)
        IsSuccess = InitializeHeadphoneOut(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeSpeaker(hOdmAudioCodec);

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

    return IsSuccess;
}

static const NvOdmAudioPortCaps * ACW8903GetPortCaps(void)
{
    static const NvOdmAudioPortCaps s_PortCaps =
        {
            1,    // MaxNumberOfInputPort;
            1     // MaxNumberOfOutputPort;
        };
    return &s_PortCaps;
}


static const NvOdmAudioOutputPortCaps *
ACW8903GetOutputPortCaps(
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
ACW8903GetInputPortCaps(
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

static void DumpWM8903(W8903AudioCodecHandle hOdmAudioCodec)
{
    int i = 0;
    NvU32 Data = 0;
    for (i=0; i<173; i++)
    {
        Data = 0;
        W8903_ReadRegister(hOdmAudioCodec, i, &Data);
    }
}
/*
../nvodm_codec_wolfson8903.c:1075: warning: `PingWM8903' defined but not used
static void PingWM8903(W8903AudioCodecHandle hOdmAudioCodec)
{
    NvU32 Data;
    int i = 0;

    NvOdmI2cStatus I2cTransStatus = NvOdmI2cStatus_Timeout;
    NvOdmI2cTransactionInfo TransactionInfo[2];
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
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
                W8903_I2C_SCLK, W8903_I2C_TIMEOUT);


        Data = (ReadBuffer[0] << 8) | ReadBuffer[1];
        if (I2cTransStatus == 0)
            NVODMAUDIOCODEC8903_PRINTF(("@%02x: Data = %04x\n", i*2, Data));
        //else
            //NVODMAUDIOCODEC8903_PRINTF(("Read Error: %08x\n", I2cTransStatus));
    }
}
*/


#define NVODM_CODEC_W8903_MAX_CLOCKS 1
static NvBool SetClockSourceOnCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsEnable)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 ClockInstances[NVODM_CODEC_W8903_MAX_CLOCKS];
    NvU32 ClockFrequencies[NVODM_CODEC_W8903_MAX_CLOCKS];
    NvU32 NumClocks;

    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(hAudioCodec->CodecGuid);
    if (pPerConnectivity == NULL)
        return NV_FALSE;

    if (IsEnable)
    {
        if (!NvOdmExternalClockConfig(hAudioCodec->CodecGuid, NV_FALSE, ClockInstances, ClockFrequencies, &NumClocks))
            return NV_FALSE;
    }
    else
    {
        if (!NvOdmExternalClockConfig(hAudioCodec->CodecGuid, NV_TRUE, ClockInstances, ClockFrequencies, &NumClocks))
            return NV_FALSE;
    }

    return NV_TRUE;
}

#define BOARD_ID_E1162  (0x0B3E) /* Decimal  1162. => ((11<<8) | 62)*/
#define BOARD_SKU_5541  (0x3729) /* Decimal  5541. => ((55<<8) | 41) / 512MB @ 400MHz */
#define BOARD_SKU_5641  (0x3829) /* Decimal  5641. => ((56<<8) | 41) / 1GB @ 400MHz */

static NvBool ACW8903Open(NvOdmPrivAudioCodecHandle hOdmAudioCodec,
                        const NvOdmQueryI2sACodecInterfaceProp *pI2sCodecInt,
                        void* hCodecNotifySem )
{
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 Index;
    //NvU32 I2CInstance =0, Interface = 0;
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NvOdmBoardInfo BoardInfo;

    hAudioCodec->hOdmService = NULL;

    // Codec interface paramater
    /* Check Board ID - Some Harmony boards requires GEN1_I2C configuration */
    hAudioCodec->CodecGuid = WOLFSON_8903_CODEC_GUID;
    if (NvOdmPeripheralGetBoardInfo((BOARD_ID_E1162), &BoardInfo))
    {
        if ( (BoardInfo.SKU == BOARD_SKU_5541 || BoardInfo.SKU == BOARD_SKU_5641) && (BoardInfo.Fab >= 001) )
        {
            hAudioCodec->CodecGuid = WOLFSON_8903_I2C_1_CODEC_GUID;
    }
    }

    pPerConnectivity = NvOdmPeripheralGetGuid(hAudioCodec->CodecGuid);

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

    hAudioCodec->IoModule    = pPerConnectivity->AddressList[Index].Interface;
    hAudioCodec->InstanceId  = pPerConnectivity->AddressList[Index].Instance;

    hAudioCodec->WCodecInterface.DeviceAddress = pPerConnectivity->AddressList[Index].Address;

    hAudioCodec->WCodecInterface.IsCodecMasterMode = pI2sCodecInt->IsCodecMasterMode;
    hAudioCodec->WCodecInterface.IsUsbMode = pI2sCodecInt->IsUsbMode;
    hAudioCodec->WCodecInterface.I2sCodecDataCommFormat = pI2sCodecInt->I2sCodecDataCommFormat;
    hAudioCodec->WCodecInterface.I2sCodecLRLineControl = pI2sCodecInt->I2sCodecLRLineControl;

    // Codec control parameter
    hAudioCodec->WCodecControl.LineInVolLeft = 0;
    hAudioCodec->WCodecControl.LineInVolRight = 0;
    hAudioCodec->WCodecControl.LineOutVolLeft = 0;
    hAudioCodec->WCodecControl.LineOutVolRight = 0;
    hAudioCodec->WCodecControl.SpeakerVolLeft = 0x3f;
    hAudioCodec->WCodecControl.SpeakerVolRight = 0x3f;
    hAudioCodec->WCodecControl.HPOutVolLeft = 0;
    hAudioCodec->WCodecControl.HPOutVolRight = 0;
    hAudioCodec->WCodecControl.AdcSamplingRate = 0;
    hAudioCodec->WCodecControl.DacSamplingRate = 0;
    hAudioCodec->WCodecControl.AdcBitsPerSample = 0;
    hAudioCodec->WCodecControl.DacBitsPerSample = 0;

    // Opening the I2C ODM Service
    hAudioCodec->hOdmService = NvOdmI2cOpen(hAudioCodec->IoModule, hAudioCodec->InstanceId);

    /* Verify I2C connection */
    //PingWM8903(hAudioCodec);

    if (!hAudioCodec->hOdmService) {
        goto ErrorExit;
    }

    SetClockSourceOnCodec(hOdmAudioCodec, NV_TRUE);

    if (OpenWolfsonCodec(hAudioCodec))
        return NV_TRUE;

ErrorExit:
    SetClockSourceOnCodec(hOdmAudioCodec, NV_FALSE);
    NvOdmI2cClose(hAudioCodec->hOdmService);
    hAudioCodec->hOdmService = NULL;
    return NV_FALSE;
}

static void ACW8903Close(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    if (hOdmAudioCodec != NULL)
    {
        CleanupInterrupt(hOdmAudioCodec);
        ShutDownCodec(hOdmAudioCodec);
        SetClockSourceOnCodec(hOdmAudioCodec, NV_FALSE);
        NvOdmI2cClose(hAudioCodec->hOdmService);
    }
}


static const NvOdmAudioWave *
ACW8903GetPortPcmCaps(
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
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 44100, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 24, 44100, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 32, 44100, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 48000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 24, 48000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 32, 48000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 32, 88200, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 32, 96000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
    };

    *pValidListCount = NV_ARRAY_SIZE(s_AudioPcmProps);
    return &s_AudioPcmProps[0];
}

static NvBool
ACW8903SetPortPcmProps(
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
        if (IsSuccess)
            IsSuccess = SetDataFormat(hOdmAudioCodec, pWaveParams->DataFormat);
        return IsSuccess;
    }

    return NV_FALSE;
}

static void
ACW8903SetVolume(
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
            NVODMAUDIOCODEC8903_PRINTF(("ACW8903SetVolume...Headphone...x%x\n", pVolume[Index].VolumeLevel));
            (void)SetHeadphoneOutVolume(hOdmAudioCodec, pVolume[Index].ChannelMask,
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
#ifdef ENABLE_SPEAKER
            (void)SetLineOutVolume(hOdmAudioCodec, pVolume[Index].ChannelMask,
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
            (void)SetSpeakerVolume(hOdmAudioCodec, pVolume[Index].ChannelMask,
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
#endif
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_LineIn)
        {
            NVODMAUDIOCODEC8903_PRINTF(("ACW8903SetVolume...LineIn...x%x\n", pVolume[Index].VolumeLevel));
            (void)SetLineInVolume(hOdmAudioCodec, pVolume[Index].ChannelMask,
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_MicIn)
        {
            NVODMAUDIOCODEC8903_PRINTF(("ACW8903SetVolume...MicIn...x%x\n", pVolume[Index].VolumeLevel));
            (void)SetMicrophoneInVolume(hOdmAudioCodec, pVolume[Index].VolumeLevel);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_LineOut)
        {
           (void)SetLineOutVolume(hOdmAudioCodec, pVolume[Index].ChannelMask,
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
        }
    }
}


static void
ACW8903SetMuteControl(
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
            (void)SetHeadphoneOutMute(hOdmAudioCodec, pMuteParam[Index].ChannelMask,
                        pMuteParam[Index].IsMute);
        }
        else if (pMuteParam[Index].SignalType ==  NvOdmAudioSignalType_LineOut)
        {
            (void)SetLineOutMute(hOdmAudioCodec, pMuteParam[Index].ChannelMask,
                        pMuteParam[Index].IsMute);
        }
        else if (pMuteParam[Index].SignalType ==  NvOdmAudioSignalType_Speakers)
        {
            (void)SetSpeakerMute(hOdmAudioCodec, pMuteParam[Index].ChannelMask,
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
ACW8903SetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NvBool IsSuccess = NV_TRUE;
    NvOdmAudioPortType PortType;
    NvU32 PortId;
    NvOdmAudioConfigBypass *pBypassConfig = pConfigData;
    NvOdmAudioConfigSideToneAttn *pSideToneAttn = pConfigData;

    NVODMAUDIOCODEC8903_PRINTF(("++[ACW8903SetConfiguration] ConfigType:%s\n",
                                ConfigType == NvOdmAudioConfigure_Pcm ? "Pcm" :
                                ConfigType == NvOdmAudioConfigure_InOutSignal ? "InOutSignal" :
                                ConfigType == NvOdmAudioConfigure_ByPass ? "ByPass" :
                                ConfigType == NvOdmAudioConfigure_SideToneAtt ? "SideToneAtt" :
                                ConfigType == NvOdmAudioConfigure_InOutSignalCallback ? "InOutSignalCallback" : "UNKNOWN"));

    PortType = NVODMAUDIO_GET_PORT_TYPE(PortName);
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);
    if (PortId != 0)
    {
        NVODMAUDIOCODEC8903_PRINTF(("[ACW8903SetConfiguration] PortId not NULL\n"));
        IsSuccess &= NV_FALSE;
        goto EXIT;
    }

    if (ConfigType == NvOdmAudioConfigure_Pcm)
    {
        IsSuccess &= ACW8903SetPortPcmProps(hOdmAudioCodec, PortName, pConfigData);
        goto EXIT;
    }

    if (ConfigType == NvOdmAudioConfigure_InOutSignal)
    {
        NVODMAUDIOCODEC8903_PRINTF(("[ACW8903SetConfiguration] Type:%s State:%s ist:%x isi:%x ost:%x osi:%x\n",
                                    ((PortType & NvOdmAudioPortType_Input) ? "INPUT" :
                                     (PortType & NvOdmAudioPortType_Output) ? "OUTPUT" : "UNKNOWN"),
                                    (((NvOdmAudioConfigInOutSignal*)pConfigData)->IsEnable ? "ENABLE" : "DISABLE"),
                                    ((NvOdmAudioConfigInOutSignal*)pConfigData)->InSignalType,
                                    ((NvOdmAudioConfigInOutSignal*)pConfigData)->InSignalId,
                                    ((NvOdmAudioConfigInOutSignal*)pConfigData)->OutSignalType,
                                    ((NvOdmAudioConfigInOutSignal*)pConfigData)->OutSignalId));

        if (PortType & NvOdmAudioPortType_Input)
        {
            IsSuccess &= SetInputPortIO(hOdmAudioCodec, PortId, pConfigData);
            goto EXIT;
        }

        if (PortType & NvOdmAudioPortType_Output)
        {
            IsSuccess &= SetOutputPortIO(hOdmAudioCodec, PortId, pConfigData);
            goto EXIT;
        }

        goto EXIT;
    }

    if (ConfigType == NvOdmAudioConfigure_ByPass)
    {
        IsSuccess &= SetBypass(hOdmAudioCodec, pBypassConfig->InSignalType,
                               pBypassConfig->InSignalId, pBypassConfig->OutSignalType,
                               pBypassConfig->OutSignalId, pBypassConfig->IsEnable);
        goto EXIT;
    }

    if (ConfigType == NvOdmAudioConfigure_SideToneAtt)
    {
        IsSuccess &= SetSideToneAttenuation(hOdmAudioCodec, pSideToneAttn->SideToneAttnValue, pSideToneAttn->IsEnable);
        goto EXIT;
    }

    if (ConfigType == NvOdmAudioConfigure_InOutSignalCallback)
    {
        hAudioCodec->IoSignalChange = *(NvOdmAudioConfigInOutSignalCallback*)pConfigData;
        NVODMAUDIOCODEC8903_PRINTF(("[ACW8903SetConfiguration] InOutSignalCallback Callback:%08x pContext:%08x\n",
                                    hAudioCodec->IoSignalChange.Callback, hAudioCodec->IoSignalChange.pContext));
        goto EXIT;
    }

EXIT:

    NVODMAUDIOCODEC8903_PRINTF(("--[ACW8903SetConfiguration]"));
    return IsSuccess;
}

static void
ACW8903GetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NvOdmAudioPortType PortType = NVODMAUDIO_GET_PORT_TYPE(PortName);
    NvU32 PortId = NVODMAUDIO_GET_PORT_ID(PortName);

    NVODMAUDIOCODEC8903_PRINTF(("++[ACW8903GetConfiguration] ConfigType:%s\n",
                                ConfigType == NvOdmAudioConfigure_Pcm ? "Pcm" :
                                ConfigType == NvOdmAudioConfigure_InOutSignal ? "InOutSignal" :
                                ConfigType == NvOdmAudioConfigure_ByPass ? "ByPass" :
                                ConfigType == NvOdmAudioConfigure_SideToneAtt ? "SideToneAtt" :
                                ConfigType == NvOdmAudioConfigure_InOutSignalCallback ? "InOutSignalCallback" : "UNKNOWN"));

    if (ConfigType == NvOdmAudioConfigure_InOutSignal)
    {
        NvOdmAudioConfigInOutSignal* pConfigIO = (NvOdmAudioConfigInOutSignal*)pConfigData;

        NVODMAUDIOCODEC8903_PRINTF(("[ACW8903GetConfiguration] Type:%s ist:%x isi:%x ost:%x osi:%x\n",
                                    ((PortType & NvOdmAudioPortType_Input) ? "INPUT" :
                                     (PortType & NvOdmAudioPortType_Output) ? "OUTPUT" : "UNKNOWN"),
                                    ((NvOdmAudioConfigInOutSignal*)pConfigData)->InSignalType,
                                    ((NvOdmAudioConfigInOutSignal*)pConfigData)->InSignalId,
                                    ((NvOdmAudioConfigInOutSignal*)pConfigData)->OutSignalType,
                                    ((NvOdmAudioConfigInOutSignal*)pConfigData)->OutSignalId));

        pConfigIO->IsEnable = NV_FALSE;
        if (PortType == NvOdmAudioPortType_Input)
        {
            //
            // Set the default to the Headset Mic until auto detection at the start is enabled.
            //
            if (pConfigIO->InSignalType == NvOdmAudioSignalType_MicIn)
            {
                if (PortId == 0)
                {
                    NVODMAUDIOCODEC8903_PRINTF(("[ACW8903SetConfiguration] External mic ENABLED."));
                    pConfigIO->IsEnable = hAudioCodec->MicEnabled;
                }

                else if (PortId == 1)
                {
                    NVODMAUDIOCODEC8903_PRINTF(("[ACW8903SetConfiguration] Builtin mic DISABLED."));
                }
            }

            else if (pConfigIO->InSignalType == NvOdmAudioSignalType_LineIn)
            {
                NVODMAUDIOCODEC8903_PRINTF(("[ACW8903SetConfiguration] Linein ENABLED."));
                pConfigIO->IsEnable = NV_TRUE;
            }
        }

        else if (PortType == NvOdmAudioPortType_Output)
        {
            if (pConfigIO->OutSignalType == NvOdmAudioSignalType_HeadphoneOut)
            {
                NVODMAUDIOCODEC8903_PRINTF(("[ACW8903SetConfiguration] Headphone %s.", hAudioCodec->HeadphoneEnabled ? "ENABLED" : "DISABLED"));
                pConfigIO->IsEnable = hAudioCodec->HeadphoneEnabled;
            }
        }
    }

    NVODMAUDIOCODEC8903_PRINTF(("--[ACW8903GetConfiguration]"));
    return;
}

static NvBool EnableInput(
        W8903AudioCodecHandle hOdmAudioCodec,
        NvU32  IsMic,  /* 0 for LineIn, 1 for Mic */
        NvBool IsPowerOn)
{
    W8903AudioCodecHandle hAudioCodec = (W8903AudioCodecHandle)hOdmAudioCodec;
    NvBool IsSuccess = NV_TRUE;
    NvU32  CtrlReg = 0;
    NvU32  VolumeCtrlReg = 0;
    NvU32  Mic = MIC_SingleEnded;

    if (!hAudioCodec->PowerupComplete)
      return NV_FALSE;

    if (IsPowerOn)
    {
        /* Analog Input Settings */
        CtrlReg = (0x5 << B00_IN_VOL);
        if (CtrlReg != hOdmAudioCodec->RegValues[R2C_ANALOG_INPUT_L_0])
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2C_ANALOG_INPUT_L_0, CtrlReg);

        CtrlReg = (0x5 << B00_IN_VOL);
        if (CtrlReg != hOdmAudioCodec->RegValues[R2D_ANALOG_INPUT_R_0])
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2D_ANALOG_INPUT_R_0, CtrlReg);

        if (IsMic)
        {
            /* Mic Bias enable */
            CtrlReg = (0x1<<B00_MICBIAS_ENA) | (0x1<<B01_MICDET_ENA);
            if (CtrlReg != hOdmAudioCodec->RegValues[R06_MICBIAS_CTRL_0])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R06_MICBIAS_CTRL_0, CtrlReg);

            /* Enable DRC */
            W8903_ReadRegister(hOdmAudioCodec, R28_DRC_0, &CtrlReg);
            CtrlReg |= (1<<B15_DRC_ENA);
            if (CtrlReg != hOdmAudioCodec->RegValues[R28_DRC_0])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R28_DRC_0, CtrlReg);

            if (Mic == MIC_Differential)
            {
                CtrlReg = (0x0<<B06_IN_CM_ENA) |
                          (0x2<<B00_MODE) |     // Differential Mic
                          (0x0<<B04_IP_SEL_N) | // LINPUT 1
                          (0x1<<B02_IP_SEL_P);  // LINPUT 2

                VolumeCtrlReg = (0x5 << B00_IN_VOL);
            }
            else // Mic = Single Ended
            {
                CtrlReg = (0x0<<B06_IN_CM_ENA) |
                          (0x0<<B00_MODE) |     // Single Ended
                          (0x0<<B04_IP_SEL_N) | // LINPUT 1
                          (0x1<<B02_IP_SEL_P);  // LINPUT 2

                VolumeCtrlReg = (0x5 << B00_IN_VOL);
            }

            /* Mic Settings */
            if (CtrlReg != hOdmAudioCodec->RegValues[R2E_ANALOG_INPUT_L_1])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2E_ANALOG_INPUT_L_1, CtrlReg);

            if (CtrlReg != hOdmAudioCodec->RegValues[R2F_ANALOG_INPUT_R_1])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2F_ANALOG_INPUT_R_1, CtrlReg);

            // For a single ended connection we need the volume set differently
            if (VolumeCtrlReg != hOdmAudioCodec->RegValues[R2C_ANALOG_INPUT_L_0])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2C_ANALOG_INPUT_L_0, VolumeCtrlReg);

            if (VolumeCtrlReg != hOdmAudioCodec->RegValues[R2D_ANALOG_INPUT_R_0])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2D_ANALOG_INPUT_R_0, VolumeCtrlReg);


            /* Replicate the Mic on both channels */
            CtrlReg  = hOdmAudioCodec->RegValues[R18_AUDIO_INTERFACE_0];
            CtrlReg  = SET_REG_VAL(CtrlReg, 0x1, B06_AIFADCR_SRC, 0x0);  /* Left adc in right channel */
            CtrlReg  = SET_REG_VAL(CtrlReg, 0x1, B07_AIFADCL_SRC, 0x0);  /* Left adc in left channel */

            if (CtrlReg != hOdmAudioCodec->RegValues[R18_AUDIO_INTERFACE_0])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R18_AUDIO_INTERFACE_0, CtrlReg);

            NVODMAUDIOCODEC8903_PRINTF(("Input set to: Mic (%s)\n", Mic == MIC_Differential ? "Differential" : "SingleEnded"));
        }
        else
        {
            /* Mic Bias disable */
            W8903_ReadRegister(hOdmAudioCodec, R06_MICBIAS_CTRL_0, &CtrlReg);
            CtrlReg &= ~(0x1<<B00_MICBIAS_ENA);
            if (CtrlReg != hOdmAudioCodec->RegValues[R06_MICBIAS_CTRL_0])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R06_MICBIAS_CTRL_0, CtrlReg);

            /* Disable DRC */
            W8903_ReadRegister(hOdmAudioCodec, R28_DRC_0, &CtrlReg);
            CtrlReg &= ~(1<<B15_DRC_ENA);
            if (CtrlReg != hOdmAudioCodec->RegValues[R28_DRC_0])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R28_DRC_0, CtrlReg);

            CtrlReg = (0x0<<B06_IN_CM_ENA) | (0x0<<B04_IP_SEL_N) | (0x0<<B02_IP_SEL_P) | (0x0<<B00_MODE);   /* LINPUT 1 */
            if (CtrlReg != hOdmAudioCodec->RegValues[R2E_ANALOG_INPUT_L_1])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2E_ANALOG_INPUT_L_1, CtrlReg);

            CtrlReg = (0x0<<B06_IN_CM_ENA) | (0x0<<B04_IP_SEL_N) | (0x0<<B02_IP_SEL_P) | (0x0<<B00_MODE);   /* RINPUT 1 */
            if (CtrlReg != hOdmAudioCodec->RegValues[R2F_ANALOG_INPUT_R_1])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R2F_ANALOG_INPUT_R_1, CtrlReg);

            /* Restore the LineIn on each channels */
            CtrlReg  = hOdmAudioCodec->RegValues[R18_AUDIO_INTERFACE_0];
            // Correct code below, but since Harmony only has a single wire for LineIn we do the following instead
            //CtrlReg  = SET_REG_VAL(CtrlReg, 0x1, B06_AIFADCR_SRC, 0x1);  /* Right adc in right channel */
            CtrlReg  = SET_REG_VAL(CtrlReg, 0x1, B06_AIFADCR_SRC, 0x0);  /* Left adc in right channel */
            CtrlReg  = SET_REG_VAL(CtrlReg, 0x1, B07_AIFADCL_SRC, 0x0);  /* Left adc in left channel */

            if (CtrlReg != hOdmAudioCodec->RegValues[R18_AUDIO_INTERFACE_0])
                IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R18_AUDIO_INTERFACE_0, CtrlReg);

            NVODMAUDIOCODEC8903_PRINTF(("Input set to: LineIn\n"));
        }

        /* Enable analog inputs */
        CtrlReg = (0x1<<B01_INL_ENA) | (0x1<<B00_INR_ENA);
        if (CtrlReg != hOdmAudioCodec->RegValues[R0C_INPUT_SEL])
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R0C_INPUT_SEL, CtrlReg);
        NV_ASSERT(CtrlReg == 0x3);

        /* ADC Settings */
        W8903_ReadRegister(hOdmAudioCodec, R26_ADC_DIGITAL_0, &CtrlReg);
        CtrlReg |= (0x1<<B04_ADC_HPF_ENA);
        if (CtrlReg != hOdmAudioCodec->RegValues[R26_ADC_DIGITAL_0])
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R26_ADC_DIGITAL_0, CtrlReg);

        /* ADC Dither Off */
        //if (0 != hOdmAudioCodec->RegValues[RA7_ADC_DITHER])
        //    IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, RA7_ADC_DITHER, 0);

        SetSideToneAttenuation(hOdmAudioCodec, 0, 0);

        /* Enable ADC & DAC */
        W8903_ReadRegister(hOdmAudioCodec, R12_ADCDAC_ENA, &CtrlReg);
        CtrlReg |= (0x1<<B00_ADCR_ENA)|(0x1<<B01_ADCL_ENA);
        if (CtrlReg != hOdmAudioCodec->RegValues[R12_ADCDAC_ENA])
        {
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R12_ADCDAC_ENA, CtrlReg);
            NvOdmOsSleepMS(2); // Just like the startup code
        }

        /* Enable Sidetone */
        SetSideToneAttenuation(hOdmAudioCodec, 0, 1);
    }
    else
    {
        if (IsMic)
        {
            /* Disable Bias Ctrl */
            CtrlReg = 0;
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R06_MICBIAS_CTRL_0, CtrlReg);

            /* Disable DRC */
            W8903_ReadRegister(hOdmAudioCodec, R28_DRC_0, &CtrlReg);
            CtrlReg &= ~(1<<B15_DRC_ENA);
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R28_DRC_0, CtrlReg);
        }

        /* Disable analog inputs */
        CtrlReg = (0x0<<B01_INL_ENA) | (0x0<<B00_INR_ENA);
        if (CtrlReg != hOdmAudioCodec->RegValues[R0C_INPUT_SEL])
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R0C_INPUT_SEL, CtrlReg);

        /* disable ADC & DAC */
        W8903_ReadRegister(hOdmAudioCodec, R12_ADCDAC_ENA, &CtrlReg);
        CtrlReg &= ~((0x1<<B00_ADCR_ENA)|(0x1<<B01_ADCL_ENA));
        if (CtrlReg != hOdmAudioCodec->RegValues[R12_ADCDAC_ENA])
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R12_ADCDAC_ENA, CtrlReg);

        /* ADC Settings */
        W8903_ReadRegister(hOdmAudioCodec, R26_ADC_DIGITAL_0, &CtrlReg);
        CtrlReg &= ~(0x1<<B04_ADC_HPF_ENA);
        if (CtrlReg != hOdmAudioCodec->RegValues[R26_ADC_DIGITAL_0])
            IsSuccess &= W8903_WriteRegister(hOdmAudioCodec, R26_ADC_DIGITAL_0, CtrlReg);

        NVODMAUDIOCODEC8903_PRINTF(("Audio Input is off\n"));
    }

    return IsSuccess;
}

static void
ACW8903SetPowerState(
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
#if 0
    if ((SignalType & NvOdmAudioSignalType_LineOut) ||
                (SignalType & NvOdmAudioSignalType_HeadphoneOut))
    {
        if (PortId == 0)
        {
            IsSuccess = SetOutPortPower(hOdmAudioCodec, SignalType, SignalId, IsPowerOn);
        }
        else
        {
            // Control the power for Vdac
            IsSuccess = SetVoiceDacPower(hOdmAudioCodec, SignalType, SignalId, IsPowerOn);
        }
        if (!IsSuccess)
            return IsSuccess;
    }

    if (SignalType & NvOdmAudioSignalType_Analog_Rxn)
    {
        IsSuccess = SetAnalogRxnPower(hOdmAudioCodec, SignalType, SignalId, IsPowerOn);
    }
#endif
}

static void
ACW8903SetPowerMode(
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
ACWM8903SetOperationMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvOdmAudioPortName PortName,
    NvBool IsMaster)
{
}

void W8903InitCodecInterface(NvOdmAudioCodec *pCodecInterface)
{
    NvOdmOsMemset(&s_W8903, 0, sizeof(W8903AudioCodec));

    pCodecInterface->pfnGetPortCaps = ACW8903GetPortCaps;
    pCodecInterface->pfnGetOutputPortCaps = ACW8903GetOutputPortCaps;
    pCodecInterface->pfnGetInputPortCaps = ACW8903GetInputPortCaps;
    pCodecInterface->pfnGetPortPcmCaps = ACW8903GetPortPcmCaps;
    pCodecInterface->pfnOpen = ACW8903Open;
    pCodecInterface->pfnClose = ACW8903Close;
    pCodecInterface->pfnSetVolume = ACW8903SetVolume;
    pCodecInterface->pfnSetMuteControl = ACW8903SetMuteControl;
    pCodecInterface->pfnSetConfiguration = ACW8903SetConfiguration;
    pCodecInterface->pfnGetConfiguration = ACW8903GetConfiguration;
    pCodecInterface->pfnSetPowerState = ACW8903SetPowerState;
    pCodecInterface->pfnSetPowerMode = ACW8903SetPowerMode;
    pCodecInterface->pfnSetOperationMode = ACWM8903SetOperationMode;

    pCodecInterface->hCodecPrivate = &s_W8903;
    pCodecInterface->IsInited = NV_TRUE;
}

