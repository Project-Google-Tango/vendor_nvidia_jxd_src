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
#include "wolfson8994_registers.h"
#include "nvodm_pmu.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "audiocodec_hal.h"

#define _READBACK 1
#define _VOICE_CALL_SETTING 1
#define _ONE_PASS_VOICE_CALL_SETTING 0

#define AUDIO_I2C_SPEED_KHZ    400
#define ENABLE 1
#define DISABLE 0

#define WM8994_POWER_CONTROL_ENABLE 1 
#define WM8994_AUDIOCODEC_DEBUG_PRINT 1

#define LINEOUT_ENABLED  0
#define ALL_INPUT_ENABLED  0
#define LOOPBACK_ENABLED  0

#define WOLFSON_8994_CODEC_GUID NV_ODM_GUID('w','o','l','f','8','9','9','4')


#define WM8994_SET_REG_VAL(r,f,n, v) \
    (((v)& (~(WM8994_##r##_##f##_MASK))) | \
        (((n << WM8994_##r##_##f##_SHIFT) & WM8994_##r##_##f##_MASK) ))

#define WM8994_SET_REG_DEF(r,f,c,v) \
    (((v)& (~(WM8994_##r##_##f##_MASK))) | \
        (((WM8994_##r##_##f##_##c << WM8994_##r##_##f##_SHIFT) & WM8994_##r##_##f##_MASK) ))

#define WM8994_GET_REG_VAL(r,f,v) \
    ((((v & WM8994_##r##_##f##_MASK) >> WM8994_##r##_##f##_SHIFT)))

/*
 * Wolfson codec register sequences.
 */

#define LDO_SETUP 1

#ifdef LDO_SETUP
void WolfsonLDO_Release(void);
NvBool WolfsonLDO_Setup(void);
static NvOdmServicesGpioHandle Wolfson_hGpio;
static NvOdmGpioPinHandle s_hWM8994_LD01GpioPin;
#endif

typedef struct
{

    NvU32 HPOutVolLeft;
    NvU32 HPOutVolRight;

    NvU32 LineOutVolLeft;
    NvU32 LineOutVolRight;

    NvU32 AdcSamplingRate;
    NvU32 DacSamplingRate;


    NvU32 LineInVolLeft;
    NvU32 LineInVolRight;
    
    NvU32 AUXInVolLeft;
    NvU32 AUXInVolRight;

    NvU32 MICINVol;

    NvBool LineInPower;
    NvBool MICPower;
    NvBool AUXPower;

    NvBool HPOutPower;
    NvBool LineOutPower;

} WolfsonCodecControls;

typedef NvU32 WCodecRegIndex;

#define WCodecRegIndex_Max 0x00FF


typedef struct WM8994AudioCodecRec
{
    NvOdmServicesI2cHandle hOdmService;
    WolfsonCodecControls WCodecControl;
    NvOdmQueryI2sACodecInterfaceProp WCodecInterface;
    NvOdmServicesPmuHandle hOdmServicePmuDevice;
    NvU32 WCodecRegVals[WCodecRegIndex_Max];
} WM8994AudioCodec, *WM8994AudioCodecHandle;

static WM8994AudioCodec s_W8994;       /* Unique audio codec for the whole system*/

#define WM8994_WRITETIMEOUT 1000 // 1 seconds

static NvBool 
WriteWolfsonRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 Data)
{
    NvOdmI2cStatus I2cTransStatus;    
    NvU8 pTxBuffer[4];
    NvOdmI2cTransactionInfo TransactionInfo;
    WM8994AudioCodecHandle hAudioCodec = (WM8994AudioCodecHandle)hOdmAudioCodec;

    pTxBuffer[0] = (NvU8)((RegIndex >> 8) & 0xFF); 
    pTxBuffer[1] = (NvU8)(RegIndex & 0xFF); 
    pTxBuffer[2] = (NvU8)((Data >> 8) & 0xFF);
    pTxBuffer[3] = (NvU8)((Data) & 0xFF);

    TransactionInfo.Address = hAudioCodec->WCodecInterface.DeviceAddress;
    TransactionInfo.Buf = pTxBuffer;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 4;

    // write the Accelator offset (from where data is to be read)
    I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, AUDIO_I2C_SPEED_KHZ,
                    WM8994_WRITETIMEOUT);

    // HW- BUG!! If timeout, again retransmit the data.                    
    if (I2cTransStatus == NvOdmI2cStatus_Timeout)
        I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, AUDIO_I2C_SPEED_KHZ,
                        WM8994_WRITETIMEOUT);

    if (I2cTransStatus == NvOdmI2cStatus_Success)
    {
      //  hAudioCodec->WCodecRegVals[RegIndex] = Data;
        return NV_TRUE;
    }    

    NvOdmOsDebugPrintf("Error in the i2c communication 0x%08x\n", I2cTransStatus);
    return NV_FALSE;
}

static NvBool ReadWolfsonRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    WCodecRegIndex RegIndex,
    NvU32 *Data)
{
    NvU8 *pReadBuffer;
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo *pTransactionInfo;
    WM8994AudioCodecHandle hAudioCodec = (WM8994AudioCodecHandle)hOdmAudioCodec;

    pReadBuffer = NvOdmOsAlloc(2);
    if (!pReadBuffer)
    {
        return NV_FALSE;
    }

    pTransactionInfo = NvOdmOsAlloc(sizeof(NvOdmI2cTransactionInfo) *2 );
    if (!pTransactionInfo)
    {
        NvOdmOsFree(pReadBuffer);
        return NV_FALSE;
    }

   // pReadBuffer[0] = RegIndex & 0xFF;
    pReadBuffer[0] = (NvU8)((RegIndex >> 8) & 0xFF); 
    pReadBuffer[1] = (NvU8)(RegIndex & 0xFF); 

    pTransactionInfo[0].Address = hAudioCodec->WCodecInterface.DeviceAddress;
    pTransactionInfo[0].Buf = pReadBuffer;
    pTransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
    pTransactionInfo[0].NumBytes = 2;

    pTransactionInfo[1].Address = (hAudioCodec->WCodecInterface.DeviceAddress | 0x1);
    pTransactionInfo[1].Buf = pReadBuffer;
    pTransactionInfo[1].Flags = 0;
    pTransactionInfo[1].NumBytes = 2;

    status = NvOdmI2cTransaction(hAudioCodec->hOdmService, pTransactionInfo, 2,
                                        AUDIO_I2C_SPEED_KHZ, NV_WAIT_INFINITE);
    if (status != NvOdmI2cStatus_Success)
    {
        switch (status)
        {
            case NvOdmI2cStatus_Timeout:
                NvOdmOsDebugPrintf("NvOdmWM8994I2cRead Failed: Timeout\n");
                break;
             case NvOdmI2cStatus_SlaveNotFound:
             default:
                NvOdmOsDebugPrintf("NvOdmWM8994I2cRead Failed: SlaveNotFound\n");
                break;
        }
        NvOdmOsFree(pReadBuffer);
        NvOdmOsFree(pTransactionInfo);
        return NV_FALSE;
    }

    *Data = (NvU32)((pReadBuffer[0] << 8) | pReadBuffer[1]);

    NvOdmOsFree(pReadBuffer);
    NvOdmOsFree(pTransactionInfo);
    return NV_TRUE;
}

static NvBool SetInterfaceActiveState(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsActive)
{
#if 0
    WM8994AudioCodecHandle hAudioCodec = (WM8994AudioCodecHandle)hOdmAudioCodec;
    NvU32 ActiveControlReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_POWER_MGMT_5];
    if (IsActive)
    {
        ActiveControlReg = WM8994_SET_REG_VAL(CODEC, ENA, NV_TRUE, ActiveControlReg);
    }
    else
    {
        ActiveControlReg = WM8994_SET_REG_VAL(CODEC, ENA, NV_FALSE, ActiveControlReg);
    }
    return WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_5, ActiveControlReg);
#else
    /*
        Do we need to activate/disable I2C interface?
        We don't need to do this, because WM8994 shares I2C bus with PMU block.
        We should not change I2C interface availity.
    */
    return NV_TRUE;
#endif
    
}

static NvBool 
WM8994_WriteRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 Data)
{
    NvBool IsSuccess = NV_TRUE;
    NvBool IsActivationControl = NV_FALSE;
    
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
WM8994_ReadRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32* Data)
{
    NvBool IsSuccess = NV_TRUE;
    NvBool IsActivationControl = NV_FALSE;
    
    if (IsActivationControl)
        IsSuccess = SetInterfaceActiveState(hOdmAudioCodec, NV_FALSE);

    // Write the wolfson register.
    if (IsSuccess)
        IsSuccess = ReadWolfsonRegister(hOdmAudioCodec, RegIndex, Data); 

    // Activate again if it is inactivated.
    if (IsSuccess)
    {
        if (IsActivationControl)
            IsSuccess = SetInterfaceActiveState(hOdmAudioCodec, NV_TRUE);
    }
    return IsSuccess;
}

static NvBool SetDacMute(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
#if 0
    NvU32 DacMuteReg;
    WM8994AudioCodecHandle hAudioCodec = (WM8994AudioCodecHandle)hOdmAudioCodec;
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_DAC_MUTE, &DacMuteReg);
    if (!IsSuccess)
        return IsSuccess;

    if (IsMute)
        DacMuteReg = WM8994_SET_REG_VAL(R58_DAC, MUTE, ENABLE, DacMuteReg);
    else
        DacMuteReg = WM8994_SET_REG_VAL(R58_DAC, MUTE, DISABLE, DacMuteReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DAC_MUTE, DacMuteReg);
    if (IsSuccess)
        hAudioCodec->WCodecRegVals[WCodecRegIndex_DAC_MUTE] = DacMuteReg;
#endif 
    return IsSuccess;
}

#if _ONE_PASS_VOICE_CALL_SETTING

static NvBool SetVoiceCallPath( NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
NvBool IsSuccess = NV_TRUE;
NvU32 reg39, reg01, reg02, reg18, reg28, reg29, reg03, reg2d, reg2e, reg33;
NvU32 reg04, reg05, reg520, reg601, reg602, reg603, reg604, reg610, reg611;
NvU32 reg612, reg613, reg620, reg621, reg200, reg204, reg208, reg210, reg211, reg310, reg1f; 
NvU32 reg700, reg701, reg702, reg703, reg704, reg705, reg706;
NvU32 reg4c, reg60, reg1c, reg1d;
// Power - UP seqeunce // DAC1 --> SPL
// 0x39 0x006C SMbus_16inx_16dat     Write  0x34      * WR
//INSERT_DELAY_MS [5]
//   0x01 0x0003 SMbus_16inx_16dat     Write  0x34      * WR
//INSERT_DELAY_MS [50]
//   0x01 0x0803 SMbus_16inx_16dat     Write  0x34      * Enable bias generator, Enable VMID, Enable MICBIAS 1, Enable HPOUT2 (Earpiece)

reg39 = 0x006c;
NvOdmOsWaitUS(5); 
reg01 = 0x0003;
NvOdmOsWaitUS(50); 
reg01 = 0x0803; 
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_ANTIPOP_2, reg39);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_1, reg01);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_1, reg01);


// Analogue Input Configuration
//   0x02 0x6240 SMbus_16inx_16dat     Write  0x34      * Enable Left Input Mixer (MIXINL), Enable IN1L PGA
//   0x18 0x0117 SMbus_16inx_16dat     Write  0x34      * Unmute IN1L PGA
//   0x28 0x0010 SMbus_16inx_16dat     Write  0x34      * Connect IN1LN to IN1L PGA inverting input
//   0x29 0x0020 SMbus_16inx_16dat     Write  0x34      * Unmute IN1L PGA output to Left Input Mixer (MIXINL) path

reg02 = 0x6240;
reg18 = 0x0117;
reg28 = 0x0010;
reg29 = 0x0020;


IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_2, reg02);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, reg18);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_2, reg28);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_3, reg29);

//* Analogue Output Configuration
//   0x03 0x00F0 SMbus_16inx_16dat     Write  0x34      * Enable Left Output Mixer (MIXOUTL), Enable Right Output Mixer (MIXOUTR), Enable Left Output Mixer Volume Control, Enable Right Output Mixer Volume Control
//*   0x1F 0x0000 SMbus_16inx_16dat     Write  0x34      * Unmute HPOUT2 (Earpiece)
//   0x2D 0x0001 SMbus_16inx_16dat     Write  0x34      * Unmute DAC1 (Left) to Left Output Mixer (MIXOUTL) path
//  0x2E 0x0001 SMbus_16inx_16dat     Write  0x34      * Unmute DAC1 (Right) to Right Output Mixer (MIXOUTR) path
//   0x33 0x0018 SMbus_16inx_16dat     Write  0x34      * Unmute Left Output Mixer to HPOUT2 (Earpiece) path, Unmute Right Output Mixer to HPOUT2 (Earpiece) path

reg03 = 0x00f0;
reg2d = 0x0101;
reg2e = 0x0101;
reg33 = 0x0018;

IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_3, reg03);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_OUTPUT_MIXER_1, reg2d);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_OUTPUT_MIXER_2, reg2e);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_HPOUT2_MIXER, reg33);



// head phone out
// 0x4C 0x9F25 SMbus_16inx_16dat     Write  0x34      * Enable Charge Pump
 //0x60 0x00EE SMbus_16inx_16dat     Write  0x34      * Enable HPOUT1 (Left) and HPOUT1 (Right) intermediate stages

reg4c = 0x9f25;
reg60 = 0x00ee;

IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_CHARGE_PUMP_1, reg4c);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_ANALOGUE_HP_1, reg60);


//* Digital Path Enables and Unmutes
//   0x04 0x2002 SMbus_16inx_16dat     Write  0x34      * Enable ADC (Left), Enable AIF2 ADC (Left) Path
//   0x05 0x2002 SMbus_16inx_16dat     Write  0x34      * Enable DAC1 (Left), 
//*  0x420 0x0000 SMbus_16inx_16dat     Write  0x34      * Unmute the AIF1 Timeslot 0 DAC path
//  0x520 0x0000 SMbus_16inx_16dat     Write  0x34      * Unmute the AIF2 DAC path
//  0x601 0x0004 SMbus_16inx_16dat     Write  0x34      * Enable the AIF1 Timeslot 
//  0x602 0x0004 SMbus_16inx_16dat     Write  0x34      * Enable the AIF1 Timeslot 
 // 0x603 0x000C SMbus_16inx_16dat     Write  0x34      * Set ADC (Left) to DAC 2 sidetone path volume to 0dB
//  0x604 0x0010 SMbus_16inx_16dat     Write  0x34      * Enable the ADC (Left) to DAC 2 (Left) mixer path
//  0x610 0x00C0 SMbus_16inx_16dat     Write  0x34      * Unmute DAC 1 (Left)
//  0x611 0x01C0 SMbus_16inx_16dat     Write  0x34      * Unmute DAC 1 (Right)
//  0x612 0x01C0 SMbus_16inx_16dat     Write  0x34      * Unmute DAC 2 (Left)
//  0x613 0x01C0 SMbus_16inx_16dat     Write  0x34      * Unmute DAC 2 (Right)
//  0x620 0x0000 SMbus_16inx_16dat     Write  0x34      * ADC OSR Set 0
//  0x621 0x01C0 SMbus_16inx_16dat     Write  0x34      * Sidetone HPF Enable LADC Select

reg04 = 0x2002;
reg05 = 0x2002;
reg520 = 0x0000;
reg601 = 0x0004;
reg602 = 0x0004;
reg603 = 0x000c;
reg604 = 0x0010;
reg610 = 0x00c0;
reg611 = 0x01c0;
reg612 = 0x01c0;
reg613 = 0x01c0;
reg620 = 0x0000;
reg621 = 0x01c0;

IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_4, reg04);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_5, reg05);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_DAC_FILTERS_1, reg520);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_MIXER_ROUTING, reg601);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_MIXER_ROUTING, reg602);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC2_MIXER_VOLUMES, reg603);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC2_LEFT_MIXER_ROUTING, reg604);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_VOLUME, reg610);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_VOLUME, reg611);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC2_LEFT_VOLUME, reg612);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC2_RIGHT_VOLUME, reg613);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_OVERSAMPLING, reg620);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_SIDETONE, reg621);

//* Clocking
//* 0x200 0x0001 SMbus_16inx_16dat     Write  0x34      * Enable AIF1 Clock, AIF1 Clock Source = MCLK
//  0x204 0x0009 SMbus_16inx_16dat     Write  0x34      * Enable AIF2 Clock, AIF2 Clock Source = MCLK2
//  0x208 0x0007 SMbus_16inx_16dat     Write  0x34      * Enable the DSP processing core for AIF1, 
//  0x210 0x0073 SMbus_16inx_16dat     Write  0x34      * Set the AIF1 sample rate to 44.1 kHz,
//  0x211 0x0003 SMbus_16inx_16dat     Write  0x34      * Set the AIF2 sample rate to 8 kHz, AIF2CLK = 256 Fs
reg200 = 0x0001; //MClock1
reg204 = 0x0001;//MClock1
reg208 = 0x000e;//
reg210 = 0x0073;
//reg211 = 0x0007;// MClock2 and AIF2 source / DSP_FS2 CLK enable. 
reg211 = 0x0073;

IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_CLOCKING_1, reg200);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_CLOCKING_1, reg204);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_CLOCKING_1, reg208);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_RATE, reg210);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_RATE, reg211);
//* AIF2 Interface // Just for QC Solution
//* CODEC Slave PCM Mode
//* 아래 4개 형식중 BB 와 Interface Matching 검토하여 적용하시면 됩니다.
//*  0x310 0x4018 SMbus_16inx_16dat     Write  0x34      * AIF2 PCM Mode 16bit Enable 
//*  0x310 0x4098 SMbus_16inx_16dat     Write  0x34      * AIF2 PCM Mode 16bit Enable
//  0x310 0x4118 SMbus_16inx_16dat     Write  0x34      * AIF2 PCM Mode 16bit Enable 
//*  0x310 0x4198 SMbus_16inx_16dat     Write  0x34      * AIF2 PMC Mode 16bit Enable 
//reg310 = 0x4118;
reg310 = 0x0010;
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_CONTROL_1, reg310);

//* General Purpose Input/Output (GPIO) Configuration
//  0x700 0xA101 SMbus_16inx_16dat     Write  0x34      * GPIO1 is Input Enable
//  0x701 0x8100 SMbus_16inx_16dat     Write  0x34      * GPIO 2 - Set to MCLK2 input
//  0x702 0x8100 SMbus_16inx_16dat     Write  0x34      * GPIO 3 - Set to BCLK2 input
//  0x703 0x8100 SMbus_16inx_16dat     Write  0x34      * GPIO 4 - Set to DACLRCLK2 input
//  0x704 0x8100 SMbus_16inx_16dat     Write  0x34      * GPIO 5 - Set to DACDAT2 input
//  0x705 0xA101 SMbus_16inx_16dat     Write  0x34      * GPIO 6 is Input Enable
//  0x706 0x0100 SMbus_16inx_16dat     Write  0x34      * GPIO 7 - Set to ADCDAT2 output

//NvU32 reg700, reg701, reg702, reg703, reg704, reg705, reg706;
reg700 = 0xa101;
//reg701 = 0x8100; // logic level input, not mclock 2 
reg701 = 0xa101; // logic level input, not mclock 2 
reg702 = 0x8100;
reg703 = 0x8100;
reg704 = 0x8100;
reg705 = 0xa101;
reg706 = 0x0100;


IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_1, reg700);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_2, reg701);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_3, reg702);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_4, reg703);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_5, reg704);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_6, reg705);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_7, reg706);


//* Unmute
//   0x1F 0x0000 SMbus_16inx_16dat     Write  0x34      * Unmute HPOUT2 (Earpiece)
//* AP 에서 신호 오면 Unmute Command 끝나면 다시 Mute 
//*  0x420 0x0000 SMbus_16inx_16dat     Write  0x34      * Unmute the AIF1 Timeslot 0 DAC path
//*  0x420 0x0200 SMbus_16inx_16dat     Write  0x34      * Mute the AIF1 Timeslot 0 DAC path
reg1f = 0x0000;
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_HPOUT2_VOLUME, reg1f);

//* Unmute
//   0x1C 0x016D SMbus_16inx_16dat     Write  0x34      * HP Un-Mute
//   0x1D 0x016D SMbus_16inx_16dat     Write  0x34      * HP Un-Mute
reg1c = 0x016D;
reg1d = 0x016D;
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_LEFT_OUTPUT_VOLUME, reg1c);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_RIGHT_OUTPUT_VOLUME, reg1d);

#if _READBACK
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_ANTIPOP_2, &reg39);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_1, &reg01);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_1, &reg01);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_2, &reg02);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, &reg18);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_2, &reg28);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_3, &reg29);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_3, &reg03);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_OUTPUT_MIXER_1, &reg2d);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_OUTPUT_MIXER_2, &reg2e);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_CHARGE_PUMP_1, &reg4c);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_ANALOGUE_HP_1, &reg60);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_HPOUT2_MIXER, &reg33);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_4, &reg04);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_5, &reg05);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_DAC_FILTERS_1, &reg520);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_MIXER_ROUTING, &reg601);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_MIXER_ROUTING, &reg602);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC2_MIXER_VOLUMES, &reg603);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC2_LEFT_MIXER_ROUTING, &reg604);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_VOLUME, &reg610);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_3, &reg611);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_VOLUME, &reg612);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC2_RIGHT_VOLUME, &reg613);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_OVERSAMPLING, &reg620);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SIDETONE, &reg621);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_CLOCKING_1, &reg200);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_CLOCKING_1, &reg204);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_CLOCKING_1, &reg208);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_RATE, &reg210);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_RATE, &reg211);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_CONTROL_1, &reg310);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_1, &reg700);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_2, &reg701);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_3, &reg702);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_4, &reg703);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_5, &reg704);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_6, &reg705);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_7, &reg706);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_HPOUT2_VOLUME, &reg1f);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_LEFT_OUTPUT_VOLUME, &reg1c);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_RIGHT_OUTPUT_VOLUME, &reg1d);

NvOdmOsDebugPrintf("\n ### Set One-Path Voice Call Path\n");
NvOdmOsDebugPrintf("0x39 0x%x \n", reg39);
NvOdmOsDebugPrintf("0x01 0x%x \n", reg01);
NvOdmOsDebugPrintf("0x02 0x%x \n", reg02);
NvOdmOsDebugPrintf("0x18 0x%x \n", reg18);
NvOdmOsDebugPrintf("0x28 0x%x \n", reg28);
NvOdmOsDebugPrintf("0x29 0x%x \n", reg29);
NvOdmOsDebugPrintf("0x03 0x%x \n", reg03);
NvOdmOsDebugPrintf("0x2d 0x%x \n", reg2d);
NvOdmOsDebugPrintf("0x2e 0x%x \n", reg2e);
NvOdmOsDebugPrintf("0x4c 0x%x \n", reg4c);
NvOdmOsDebugPrintf("0x60 0x%x \n", reg60);
NvOdmOsDebugPrintf("0x33 0x%x \n", reg33);
NvOdmOsDebugPrintf("0x04 0x%x \n", reg04);
NvOdmOsDebugPrintf("0x05 0x%x \n", reg05);
NvOdmOsDebugPrintf("0x520 0x%x \n", reg520);
NvOdmOsDebugPrintf("0x601 0x%x \n", reg601);
NvOdmOsDebugPrintf("0x602 0x%x \n", reg602);
NvOdmOsDebugPrintf("0x603 0x%x \n", reg603);
NvOdmOsDebugPrintf("0x604 0x%x \n", reg604);
NvOdmOsDebugPrintf("0x610 0x%x \n", reg610);
NvOdmOsDebugPrintf("0x611 0x%x \n", reg611);
NvOdmOsDebugPrintf("0x612 0x%x \n", reg612);
NvOdmOsDebugPrintf("0x613 0x%x \n", reg613);
NvOdmOsDebugPrintf("0x620 0x%x \n", reg620);
NvOdmOsDebugPrintf("0x621 0x%x \n", reg621);
NvOdmOsDebugPrintf("0x200 0x%x \n", reg200);
NvOdmOsDebugPrintf("0x204 0x%x \n", reg204);
NvOdmOsDebugPrintf("0x208 0x%x \n", reg208);
NvOdmOsDebugPrintf("0x210 0x%x \n", reg210);
NvOdmOsDebugPrintf("0x211 0x%x \n", reg211);
NvOdmOsDebugPrintf("0x310 0x%x \n", reg310);
NvOdmOsDebugPrintf("0x700 0x%x \n", reg700);
NvOdmOsDebugPrintf("0x701 0x%x \n", reg701);
NvOdmOsDebugPrintf("0x702 0x%x \n", reg702);
NvOdmOsDebugPrintf("0x703 0x%x \n", reg703);
NvOdmOsDebugPrintf("0x704 0x%x \n", reg704);
NvOdmOsDebugPrintf("0x705 0x%x \n", reg705);
NvOdmOsDebugPrintf("0x706 0x%x \n", reg706);
NvOdmOsDebugPrintf("0x1f 0x%x \n", reg1f);
NvOdmOsDebugPrintf("0x1c 0x%x \n", reg1c);
NvOdmOsDebugPrintf("0x1d 0x%x \n", reg1d);
#endif 

return IsSuccess;

}


#endif 

static NvBool 
SetHeadphoneOutVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask, 
    NvU32 LeftVolume,
    NvU32 RightVolume)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVol, RightVol, LeftVolReg, RightVolReg;
    WM8994AudioCodecHandle hAudioCodec = (WM8994AudioCodecHandle)hOdmAudioCodec;

 //### Set Speaker Volume: AIF1 DAC1 
 //WM8994_DAC1_LEFT_VOLUME:Reg 0x610 0xc0 
 //WM8994_DAC1_RIGHT_VOLUME:Reg 0x611 0xc0

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_VOLUME, &LeftVolReg);
    if (!IsSuccess)
        return IsSuccess;
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_VOLUME, &RightVolReg);
    if (!IsSuccess)
        return IsSuccess;

    LeftVol =  (NvU32)(LeftVolume);
    RightVol = (NvU32)(RightVolume);

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        //01C0  DAC1L_MUTE=0, DAC1_VU=1, DAC1L_VOL=1100_0000
        LeftVolReg = WM8994_SET_REG_VAL(DAC1L, MUTE, DISABLE, LeftVolReg);
        LeftVolReg = WM8994_SET_REG_VAL(DAC1, VU, ENABLE, LeftVolReg);
        LeftVolReg = WM8994_SET_REG_VAL(DAC1L, VOL, LeftVol, LeftVolReg);
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_VOLUME, LeftVolReg);
        if (IsSuccess)
        {
            hAudioCodec->WCodecControl.HPOutVolLeft = LeftVolume;
        }
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        //01C0  DAC1R_MUTE=0, DAC1_VU=1, DAC1R_VOL=1100_0000
        RightVolReg = WM8994_SET_REG_VAL(DAC1R, MUTE, DISABLE, RightVolReg);
        RightVolReg = WM8994_SET_REG_VAL(DAC1, VU, ENABLE, RightVolReg);
        RightVolReg = WM8994_SET_REG_VAL(DAC1R, VOL, RightVol, RightVolReg);
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_VOLUME, RightVolReg);
        if (IsSuccess)
        {
            hAudioCodec->WCodecControl.HPOutVolRight = RightVolume;
        }
    }

#if _READBACK
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_VOLUME, &LeftVolReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_VOLUME, &RightVolReg);

    NvOdmOsDebugPrintf("### Set Speaker Volume: AIF1 DAC1 \n");
    NvOdmOsDebugPrintf("WM8994_DAC1_LEFT_VOLUME:Reg 0x610 0x%x \n", LeftVolReg); 
    NvOdmOsDebugPrintf("WM8994_DAC1_RIGHT_VOLUME:Reg 0x611 0x%x \n", RightVolReg);
    
#endif
    return IsSuccess;
}




/*
 * Shuts down the audio codec driver.
 */
static NvBool ShutDownCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_FALSE;

    return IsSuccess;
}

static NvBool ResetCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = 0;
    NvU32  SWResetAddressReg = 0;
    NvU32  ControlInterfaceReg = 0;
 
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SOFTWARE_RESET, &SWResetAddressReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_CONTROL_INTERFACE, &ControlInterfaceReg);
  //  IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_SOFTWARE_RESET, 0x8994);
  //  IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SOFTWARE_RESET, &SWResetAddress);
#if _READBACK
NvOdmOsDebugPrintf("\n ### Set Reset Codec \n");
NvOdmOsDebugPrintf("0x00 0x%x \n", SWResetAddressReg);
NvOdmOsDebugPrintf("0x101 0x%x \n", ControlInterfaceReg);
#endif

    return IsSuccess;
}

#define NVODM_CODEC_W8994_MAX_CLOCKS 3

static NvBool SetPowerOnCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsEnable)
{
#if WM8994_POWER_CONTROL_ENABLE
    WM8994AudioCodecHandle hAudioCodec = (WM8994AudioCodecHandle)hOdmAudioCodec;
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 Index;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime;
    NvU32 ClockInstances[NVODM_CODEC_W8994_MAX_CLOCKS];
    NvU32 ClockFrequencies[NVODM_CODEC_W8994_MAX_CLOCKS];
    NvU32 NumClocks;
   
    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(WOLFSON_8994_CODEC_GUID);    
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
                WOLFSON_8994_CODEC_GUID, NV_FALSE,
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
                NvOdmOsWaitUS(100000);
#if WM8994_AUDIOCODEC_DEBUG_PRINT
                NvOdmOsDebugPrintf("!!! Power On completed on the Wolfson 8994 codec\n");
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
                    WOLFSON_8994_CODEC_GUID, NV_TRUE,
                    ClockInstances, ClockFrequencies, &NumClocks))
                return NV_FALSE;

#if WM8994_AUDIOCODEC_DEBUG_PRINT
                NvOdmOsDebugPrintf("Power Off completed on the Wolfson codec\n");
#endif            
        }
    }
#endif    
    return NV_TRUE;
}
static NvBool SetAntiPopCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsAntiPop)
{
    NvBool IsSuccess = NV_FALSE;
    
    return IsSuccess;
}    

static NvBool InitializeClocking(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
   NvBool IsSuccess;
   NvU32 AifClockingReg1 = 0;
   NvU32 ClokingReg1 = 0;
   NvU32 Aif1RateReg = 0;
   NvU32 Aif2RateReg = 0;
   NvU32 Aif1Control1 = 0;
   NvU32 Aif2ClockingReg1= 0;
   NvU32 GPIO1Reg= 0;
  
//### Set Clocking: 
//WM8994_AIF1_CLOCKING_1:Reg 0x200 0x1 
//WM8994_CLOCKING_1:Reg 0x208 0xa 
//WM8994_AIF1_RATE:Reg 0x210 0x73 
//WM8994_AIF1_CONTROL_1:Reg 0x300 0xc050
//WM8994_GPIO_1

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_CLOCKING_1, &AifClockingReg1);
    if (!IsSuccess)
        return IsSuccess;
        // Set (0001)  AIF1CLK_SRC=00, AIF1CLK_INV=0, AIF1CLK_DIV=0, AIF1CLK_ENA=1
    AifClockingReg1 = WM8994_SET_REG_VAL(AIF1CLK, ENA, ENABLE, AifClockingReg1);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_CLOCKING_1, AifClockingReg1);

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_CLOCKING_1, &ClokingReg1);
    if (!IsSuccess)
        return IsSuccess;
    //set 000A  CLASSD_EDGE=00, TOCLK_ENA=0, DSP_FS1CLK_ENA=1, DSP_FS2CLK_ENA=1, DSP_FSINTCLK_ENA=1, SYSCLK_SRC=0
    ClokingReg1 = WM8994_SET_REG_VAL(DSP, FS1CLK_ENA, ENABLE, ClokingReg1);
    ClokingReg1 = WM8994_SET_REG_VAL(DSP, FS2CLK_ENA, ENABLE, ClokingReg1);
    ClokingReg1 = WM8994_SET_REG_VAL(DSP, FSINTCLK_ENA, ENABLE, ClokingReg1);
#if _VOICE_CALL_SETTING
    ClokingReg1 = 0x000e;// this effect to AIF1 music path 
#endif
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_CLOCKING_1, ClokingReg1);

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_RATE, &Aif1RateReg);
     if (!IsSuccess)
        return IsSuccess;
    //* Set the AIF1 sample rate to 44.1 kHz, AIF1CLK = 256 Fs
    Aif1RateReg = 0x0073;
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_RATE, Aif1RateReg);

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_CONTROL_1, &Aif1Control1);
     if (!IsSuccess)
        return IsSuccess;

    Aif1Control1 = 0x0010;
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_CONTROL_1, Aif1Control1);

    GPIO1Reg = 0xA101;
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_1, GPIO1Reg);

#if _READBACK
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_CLOCKING_1, &AifClockingReg1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_CLOCKING_1, &ClokingReg1); 
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_RATE, &Aif1RateReg); 
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_CONTROL_1, &Aif1Control1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_1, &GPIO1Reg);

    NvOdmOsDebugPrintf("\n ### Set Clocking: \n");
    NvOdmOsDebugPrintf("0x200 0x%x \n", AifClockingReg1);
    NvOdmOsDebugPrintf("0x208 0x%x \n", ClokingReg1);
    NvOdmOsDebugPrintf("0x210 0x%x \n", Aif1RateReg);
    NvOdmOsDebugPrintf("0x300 0x%x \n", Aif1Control1);
    NvOdmOsDebugPrintf("0x700 0x%x \n", GPIO1Reg);
#endif

//### set clocking for voice in on AIF2

//WM8994_AIF2_CLOCKING_1: Reg 0x204 0x0009   * Enable AIF2 Clock, AIF2 Clock Source = MCLK2
//WM8994_CLOCKING_1:Reg 0x208 0xE * Enable the DSP processing core for AIF1, Enable the DSP processing core for AIF2, Enable the core clock
//WM8994_AIF2_RATE:Reg 0x211 0x0073  * Set the AIF2 sample rate to 8 kHz, AIF2CLK = 256 Fs

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_CLOCKING_1, &Aif2ClockingReg1);
    if (!IsSuccess)
        return IsSuccess;
        // Set (0001)  AIF2CLK_SRC=00, AIF2CLK_INV=0, AIF1CLK_DIV=0, AIF2CLK_ENA=1
    Aif2ClockingReg1 = WM8994_SET_REG_VAL(AIF1CLK, ENA, ENABLE, Aif2ClockingReg1);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_CLOCKING_1, Aif2ClockingReg1);


    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_RATE, &Aif2RateReg);
    if (!IsSuccess)
        return IsSuccess;
    //* Set the AIF2 sample rate to 44.1 kHz, AIF1CLK = 256 Fs
 
#if _VOICE_CALL_SETTING
   //Aif2RateReg = 0x0003;
    Aif2RateReg = 0x0073;
#else
   Aif2RateReg = 0x0073;
#endif
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_RATE, Aif2RateReg);    

#if _READBACK
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_CLOCKING_1, &Aif2ClockingReg1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_RATE, &Aif2RateReg); 
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_CLOCKING_1, &ClokingReg1);

    NvOdmOsDebugPrintf("\n ### Set Clocking for AIF2: \n");
    NvOdmOsDebugPrintf("0x204 0x%x \n", Aif2ClockingReg1);
    NvOdmOsDebugPrintf("0x211 0x%x \n", Aif2RateReg);
    NvOdmOsDebugPrintf("0x208 0x%x \n", ClokingReg1);
#endif

    return IsSuccess;
}



static NvBool InitializeDigitalInterface(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess;
    NvU32 Aif1Dac1Filters1Reg = 0;
    NvU32 Dac1LMixerRoutingReg = 0, Dac1RMixerRoutingReg = 0;
    NvU32 DacLeftVolumeReg = 0, DacRightVolumeReg = 0;
    NvU32 AdcOversampleReg = 0;
    NvU32 AdcLeftVolumeReg = 0, AdcRightVolumeReg = 0;
    NvU32 Aif1ADC1LeftMixerRouting = 0, Aif1ADC1RightMixerRouting = 0;

//###Set Speaker Out Digital interface
//WM8994_AIF1_DAC1_FILTERS_1:Reg 0x420 0x0 
//WM8994_DAC1_LEFT_VOLUME:Reg 0x610 0xc0 
//WM8994_DAC1_RIGHT_VOLUME:Reg 0x611 0xc0 
//WM8994_DAC1_LEFT_MIXER_ROUTING:Reg 0x601 0x1 
//WM8994_DAC1_RIGHT_MIXER_ROUTING:Reg 0x602 0x1 
//WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING     0x606 0x2
//WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING    0x607 0x2

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_DAC1_FILTERS_1, &Aif1Dac1Filters1Reg);
    if (!IsSuccess)
        return IsSuccess;
    //* Unmute the AIF1 Timeslot 0 DAC path
    Aif1Dac1Filters1Reg = WM8994_SET_REG_VAL(AIF1DAC1, MUTE, DISABLE, Aif1Dac1Filters1Reg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_DAC1_FILTERS_1, Aif1Dac1Filters1Reg);


    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_MIXER_ROUTING, &Dac1LMixerRoutingReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_MIXER_ROUTING, &Dac1RMixerRoutingReg);
    if (!IsSuccess)
        return IsSuccess;
        //0001  ADC2_TO_DAC1L=0, ADC1_TO_DAC1L=0, AIF2DACL_TO_DAC1L=0, AIF1DAC2L_TO_DAC1L=0, AIF1DAC1L_TO_DAC1L=1
    Dac1LMixerRoutingReg = WM8994_SET_REG_VAL(AIF1DAC1L, TO_DAC1L, ENABLE, Dac1LMixerRoutingReg);
        //0001  ADC2_TO_DAC1R=0, ADC1_TO_DAC1R=0, AIF2DACR_TO_DAC1R=0, AIF1DAC2R_TO_DAC1R=0, AIF1DAC1R_TO_DAC1R=1
    Dac1RMixerRoutingReg = WM8994_SET_REG_VAL(AIF1DAC1R, TO_DAC1R, ENABLE, Dac1RMixerRoutingReg);
    //0004  ADC2_TO_DAC1L=1, ADC1_TO_DAC1L=0, AIF2DACL_TO_DAC1L=1, AIF1DAC2L_TO_DAC1L=0, AIF1DAC1L_TO_DAC1L=1
    Dac1LMixerRoutingReg = WM8994_SET_REG_VAL(AIF2DACL, TO_DAC1L, ENABLE, Dac1LMixerRoutingReg);
        //0004  ADC2_TO_DAC1R=1, ADC1_TO_DAC1R=0, AIF2DACR_TO_DAC1R=1, AIF1DAC2R_TO_DAC1R=0, AIF1DAC1R_TO_DAC1R=1
    Dac1RMixerRoutingReg = WM8994_SET_REG_VAL(AIF2DACR, TO_DAC1R, ENABLE, Dac1RMixerRoutingReg);

    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_MIXER_ROUTING, Dac1LMixerRoutingReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_MIXER_ROUTING, Dac1RMixerRoutingReg);


    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_VOLUME, &DacLeftVolumeReg);
    if (!IsSuccess)
        return IsSuccess;
    //* Unmute DAC 1 (Left)
    DacLeftVolumeReg = 0x00C0;
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_VOLUME, DacLeftVolumeReg);

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_VOLUME, &DacRightVolumeReg);
    if (!IsSuccess)
        return IsSuccess;
    //* Unmute DAC 1 (Right)
    DacRightVolumeReg = 0x01C0;
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_VOLUME, DacRightVolumeReg);

    Aif1ADC1LeftMixerRouting = 0x02;
    Aif1ADC1RightMixerRouting = 0x02;
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING, Aif1ADC1LeftMixerRouting);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING, Aif1ADC1RightMixerRouting);
{
//* General Purpose Input/Output (GPIO) Configuration
//  0x700 0xA101 SMbus_16inx_16dat     Write  0x34      * GPIO1 is Input Enable
//  0x701 0x8100 SMbus_16inx_16dat     Write  0x34      * GPIO 2 - Set to MCLK2 input
//  0x702 0x8100 SMbus_16inx_16dat     Write  0x34      * GPIO 3 - Set to BCLK2 input
//  0x703 0x8100 SMbus_16inx_16dat     Write  0x34      * GPIO 4 - Set to DACLRCLK2 input
//  0x704 0x8100 SMbus_16inx_16dat     Write  0x34      * GPIO 5 - Set to DACDAT2 input
//  0x705 0xA101 SMbus_16inx_16dat     Write  0x34      * GPIO 6 is Input Enable
//  0x706 0x0100 SMbus_16inx_16dat     Write  0x34      * GPIO 7 - Set to ADCDAT2 output

//NvU32 reg700, reg701, reg702, reg703, reg704, reg705, reg706;
NvU32 reg700 = 0xa101;
NvU32 reg701 = 0xa101; // logic level input, not mclock 2 
NvU32 reg702 = 0x8100;
NvU32 reg703 = 0x8100;
NvU32 reg704 = 0x8100;
NvU32 reg705 = 0xa101;
NvU32 reg706 = 0x0100;
NvU32 reg1c = 0x016D;
NvU32 reg1d = 0x016D;

IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_1, reg700);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_2, reg701);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_3, reg702);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_4, reg703);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_5, reg704);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_6, reg705);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_GPIO_7, reg706);


//* Unmute
//   0x1C 0x016D SMbus_16inx_16dat     Write  0x34      * HP Un-Mute
//   0x1D 0x016D SMbus_16inx_16dat     Write  0x34      * HP Un-Mute

IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_LEFT_OUTPUT_VOLUME, reg1c);
IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_RIGHT_OUTPUT_VOLUME, reg1d);

IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_1, &reg700);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_2, &reg701);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_3, &reg702);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_4, &reg703);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_5, &reg704);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_6, &reg705);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_GPIO_7, &reg706);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_LEFT_OUTPUT_VOLUME, &reg1c);
IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_RIGHT_OUTPUT_VOLUME, &reg1d);


NvOdmOsDebugPrintf("\n ### Set GPIO Interface: \n");
NvOdmOsDebugPrintf("0x700 0x%x \n", reg700);
NvOdmOsDebugPrintf("0x701 0x%x \n", reg701);
NvOdmOsDebugPrintf("0x702 0x%x \n", reg702);
NvOdmOsDebugPrintf("0x703 0x%x \n", reg703);
NvOdmOsDebugPrintf("0x704 0x%x \n", reg704);
NvOdmOsDebugPrintf("0x705 0x%x \n", reg705);
NvOdmOsDebugPrintf("0x706 0x%x \n", reg706);
NvOdmOsDebugPrintf("0x1c 0x%x \n", reg1c);
NvOdmOsDebugPrintf("0x1d 0x%x \n", reg1d);

}

#if _READBACK //readback confirmation
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_DAC1_FILTERS_1, &Aif1Dac1Filters1Reg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_VOLUME, &DacLeftVolumeReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_VOLUME, &DacRightVolumeReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_LEFT_MIXER_ROUTING, &Dac1LMixerRoutingReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC1_RIGHT_MIXER_ROUTING, &Dac1RMixerRoutingReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING, &Aif1ADC1LeftMixerRouting);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING, &Aif1ADC1RightMixerRouting);
    NvOdmOsDebugPrintf("\n ### Set Digital Interface: AIF1 DAC1 \n");
    NvOdmOsDebugPrintf("0x420 0x%x \n", Aif1Dac1Filters1Reg); 
    NvOdmOsDebugPrintf("0x610 0x%x \n", DacLeftVolumeReg); 
    NvOdmOsDebugPrintf("0x611 0x%x \n", DacRightVolumeReg);
    NvOdmOsDebugPrintf("0x601 0x%x \n", Dac1LMixerRoutingReg);
    NvOdmOsDebugPrintf("0x602 0x%x \n", Dac1RMixerRoutingReg);
    NvOdmOsDebugPrintf("0x606 0x%x \n", Aif1ADC1LeftMixerRouting);
    NvOdmOsDebugPrintf("0x607 0x%x \n", Aif1ADC1RightMixerRouting);
#endif 


#if _VOICE_CALL_SETTING
{
    NvU32 AifDacFilter1 = 0, Dac2MixerVolume = 0, Dac2LeftMixerRouting = 0, Dac2RightMixerRouting = 0;
    NvU32 Dac2LeftVolume = 0, Dac2RightVolume = 0, Oversample = 0, SideTone = 0;

//#### adding AIF2 path for voice path 
//WM8994_AIF2_DAC_FILTERS_1    0x520 0x0
//WM8994_DAC2_MIXER_VOLUMES 0x603 0x000c
//WM8994_DAC2_LEFT_MIXER_ROUTING          0x604 0x0010
//WM8994_DAC2_RIGHT_MIXER_ROUTING         0x605 0x0010
//WM8994_DAC2_LEFT_VOLUME                 0x612  0x01c0
//WM8994_DAC2_RIGHT_VOLUME                0x613 0x01c0
//WM8994_OVERSAMPLING                     0x620 0x0000
//WM8994_SIDETONE                         0x621 0x01c0   

    AifDacFilter1 = 0x0000;
    Dac2MixerVolume = 0x000C; 
    Dac2LeftMixerRouting = 0x0010;
    Dac2RightMixerRouting = 0x0010;
    Dac2LeftVolume = 0x01C0;
    Dac2RightVolume = 0x01C0;
    Oversample = 0x0000;
    SideTone = 0x01C0;

    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_DAC_FILTERS_1, AifDacFilter1);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC2_MIXER_VOLUMES, Dac2MixerVolume);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC2_LEFT_MIXER_ROUTING, Dac2LeftMixerRouting);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC2_RIGHT_MIXER_ROUTING, Dac2RightMixerRouting);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC2_LEFT_VOLUME, Dac2LeftVolume);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_DAC2_RIGHT_VOLUME, Dac2RightVolume);   
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_OVERSAMPLING, Oversample);    
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_SIDETONE, SideTone);

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_DAC_FILTERS_1, &AifDacFilter1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC2_MIXER_VOLUMES, &Dac2MixerVolume);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC2_LEFT_MIXER_ROUTING, &Dac2LeftMixerRouting);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC2_RIGHT_MIXER_ROUTING, &Dac2RightMixerRouting);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC2_LEFT_VOLUME, &Dac2LeftVolume);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_DAC2_RIGHT_VOLUME, &Dac2RightVolume);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_OVERSAMPLING, &Oversample);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SIDETONE, &SideTone);
    NvOdmOsDebugPrintf("\n ### Set Digital Interface: AIF2DAC \n");
    NvOdmOsDebugPrintf("0x520 0x%x \n", AifDacFilter1); 
    NvOdmOsDebugPrintf("0x603 0x%x \n", Dac2MixerVolume); 
    NvOdmOsDebugPrintf("0x604 0x%x \n", Dac2LeftMixerRouting);
    NvOdmOsDebugPrintf("0x605 0x%x \n", Dac2RightMixerRouting);
    NvOdmOsDebugPrintf("0x612 0x%x \n", Dac2LeftVolume);
    NvOdmOsDebugPrintf("0x613 0x%x \n", Dac2RightVolume);
    NvOdmOsDebugPrintf("0x620 0x%x \n", Oversample);
    NvOdmOsDebugPrintf("0x621 0x%x \n", SideTone);
}
#endif

//###Set Mic In Digital interface
//WM8994_AIF2_ADC_LEFT_VOLUME:Reg 0x500 0x0 
//WM8994_AIF2_ADC_RIGHT_VOLUME:Reg 0x501 0xc0 
//WM8994_AIF2_ADC_FILTERS:Reg 0x510 
//WM8994_OVERSAMPLING:	Reg 0x620 0x00 * ADC OSR Set 0

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_OVERSAMPLING, &AdcOversampleReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_ADC_LEFT_VOLUME, &AdcLeftVolumeReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_ADC_RIGHT_VOLUME, &AdcRightVolumeReg);
    if (!IsSuccess)
        return IsSuccess;

    AdcOversampleReg = 0x0000;
    AdcLeftVolumeReg = 0x00C0;
    AdcRightVolumeReg = 0x00C0;
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_OVERSAMPLING, AdcOversampleReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_ADC_LEFT_VOLUME, AdcLeftVolumeReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_ADC_RIGHT_VOLUME, AdcRightVolumeReg);

#if _READBACK //readback confirmation
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_OVERSAMPLING, &AdcOversampleReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_ADC_LEFT_VOLUME, &AdcLeftVolumeReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_ADC_RIGHT_VOLUME, &AdcRightVolumeReg);
    NvOdmOsDebugPrintf("\n ###Set Mic In Digital interface \n");
    NvOdmOsDebugPrintf("0x620 0x%x \n", AdcOversampleReg); 
    NvOdmOsDebugPrintf("0x500 0x%x \n", AdcLeftVolumeReg); 
    NvOdmOsDebugPrintf("0x501 0x%x \n", AdcRightVolumeReg); 
#endif 


    return IsSuccess;

}

static NvBool InitializeHeadphoneOut(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{

    return SetHeadphoneOutVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                192, 192);
}

static NvBool InitializeAnalogInPut(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftLineInput_1_2_Volume = 0, RightLineInput_1_2_Volume = 0;
    NvU32 InputMixer2 = 0;
    NvU32 InputMixer3 = 0;
    NvU32 InputMixer4 = 0;
    NvU32 InputMixer5 = 0;

//### Set Analog Audio Path for Mic: 
//WM8994_LEFT_LINE_INPUT_1_2_VOLUME:Reg 0x18 0x0117   * Unmute IN1L PGA
//WM8994_RIGHT_LINE_INPUT_1_2_VOLUME:Reg 0x1A 0x0117   * Unmute IN1L PGA
//WM8994_INPUT_MIXER_2:Reg 0x28 0x0022 * Connect IN1LN to IN1L PGA inverting input, Connect IN1RN to IN1R PGA inverting input
//WM8994_INPUT_MIXER_3:Reg 0x29 0x0020 * Unmute IN1L PGA output to Left Input Mixer (MIXINL) path, IN1L_MIXINL_VOL +30db
//WM8994_INPUT_MIXER_3:Reg 0x2A 0x0020 * Unmute IN1R PGA output to Right Input Mixer (MIXINR) path, IN1R_MIXINR_VOL +30db

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, &LeftLineInput_1_2_Volume);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_RIGHT_LINE_INPUT_1_2_VOLUME, &RightLineInput_1_2_Volume);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_2, &InputMixer2);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_3, &InputMixer3);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_4, &InputMixer4);

    if (!IsSuccess)
        return IsSuccess;

    LeftLineInput_1_2_Volume = 0x0117;
    RightLineInput_1_2_Volume = 0x0117;
    //InputMixer2 = InputMixer2 | 0x11;
    InputMixer2 = InputMixer2 | 0x33;
    InputMixer3 = 0x20;
    InputMixer4 = 0x20;
    InputMixer5 = 0x140;

    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, LeftLineInput_1_2_Volume);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_RIGHT_LINE_INPUT_1_2_VOLUME, RightLineInput_1_2_Volume);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_2, InputMixer2);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_3, InputMixer3);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_4, InputMixer4);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_5, InputMixer5);

#if _READBACK //readback confirmation
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, &LeftLineInput_1_2_Volume);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_RIGHT_LINE_INPUT_1_2_VOLUME, &RightLineInput_1_2_Volume);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_2, &InputMixer2);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_3, &InputMixer3);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_INPUT_MIXER_4, &InputMixer4);
    NvOdmOsDebugPrintf("\n # Set Analog Input Audio Path: \n");
    NvOdmOsDebugPrintf("0x18 0x%x \n", LeftLineInput_1_2_Volume);
    NvOdmOsDebugPrintf("0x1A 0x%x \n", RightLineInput_1_2_Volume);
    NvOdmOsDebugPrintf("0x28 0x%x \n", InputMixer2);
    NvOdmOsDebugPrintf("0x29 0x%x \n", InputMixer3);
    NvOdmOsDebugPrintf("0x2A 0x%x \n", InputMixer4);
#endif

    return IsSuccess;
}

static NvBool InitializeAnalogOutPut(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 OutMixerReg1 = 0;
    NvU32 OutMixerReg2 = 0;
    NvU32 ChargePumpReg1 = 0;
    NvU32 AnalogHPReg1 = 0;
    NvU32 SpkMixLReg = 0; //WM8994_SPKMIXL_ATTENUATION
    NvU32 SpkMixRReg = 0; //WM8994_SPKMIXR_ATTENUATION
    NvU32 SpkOutMixerReg = 0; //WM8994_SPKOUT_MIXERS
    NvU32 SpkMixerReg = 0; //WM8994_SPEAKER_MIXER

//### Set Analog Audio Path for Speaker: 
//WM8994_OUTPUT_MIXER_1:Reg 0x2D 0x100 
//WM8994_OUTPUT_MIXER_2:Reg 0x2E 0x100 
//WM8994_CHARGE_PUMP_1:Reg 0x4C 0x9f25 
//WM8994_ANALOGUE_HP_1:Reg 0x60 0xee 
//WM8994_SPKMIXL_ATTENUATION:Reg 0x22 0x0 
//WM8994_SPKMIXR_ATTENUATION:Reg 0x23 0x0 
//WM8994_SPKOUT_MIXERS:Reg 0x24 0x1b 
//WM8994_SPEAKER_MIXER:Reg 0x36 0x3 
 
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_OUTPUT_MIXER_1, &OutMixerReg1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_OUTPUT_MIXER_2, &OutMixerReg2);

    if (!IsSuccess)
        return IsSuccess;

    //* Select DAC1 (Left) to Left Headphone Output PGA (HPOUT1LVOL) path
    OutMixerReg1 = WM8994_SET_REG_VAL(DAC1L, TO_HPOUT1L, ENABLE, OutMixerReg1);
    OutMixerReg2 = WM8994_SET_REG_VAL(DAC1R, TO_HPOUT1R, ENABLE, OutMixerReg2);

#if _VOICE_CALL_SETTING
{ NvU32 HPout2Mixer = 0x0018; //WM8994_SPEAKER_MIXER
   NvU32 reg1f = 0x0000;
   
    OutMixerReg1 = WM8994_SET_REG_VAL(DAC1L, TO_MIXOUTL, ENABLE, OutMixerReg1);
    OutMixerReg2 = WM8994_SET_REG_VAL(DAC1R, TO_MIXOUTR, ENABLE, OutMixerReg2);

    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_HPOUT2_MIXER, HPout2Mixer);

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_HPOUT2_MIXER, &HPout2Mixer);

           
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_HPOUT2_VOLUME, reg1f);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_HPOUT2_VOLUME, &reg1f);

 
    NvOdmOsDebugPrintf("0x1f 0x%x \n", reg1f);
    NvOdmOsDebugPrintf("0x33 0x%x \n", HPout2Mixer);
}
#endif 


    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_OUTPUT_MIXER_1, OutMixerReg1);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_OUTPUT_MIXER_2, OutMixerReg2);

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_CHARGE_PUMP_1, &ChargePumpReg1);

    if (!IsSuccess)
        return IsSuccess;
        // Set (0x9F25h) CP_ENA charge pump up default register 0x1F25h | 8000 = 0x9F25h
    ChargePumpReg1 = WM8994_SET_REG_VAL(CP, ENA, ENABLE, ChargePumpReg1);

    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_CHARGE_PUMP_1, ChargePumpReg1);

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_ANALOGUE_HP_1, &AnalogHPReg1);

    if (!IsSuccess)
        return IsSuccess;
    // Set (00EE) : HPOUT1L_RMV_SHORT=1, HPOUT1L_OUTP=1, HPOUT1L_DLY=1, HPOUT1R_RMV_SHORT=1, HPOUT1R_OUTP=1, HPOUT1R_DLY=1
    AnalogHPReg1 = WM8994_SET_REG_VAL(HPOUT1L, RMV_SHORT, ENABLE, AnalogHPReg1);
    AnalogHPReg1 = WM8994_SET_REG_VAL(HPOUT1L, OUTP, ENABLE, AnalogHPReg1);
    AnalogHPReg1 = WM8994_SET_REG_VAL(HPOUT1L, DLY, ENABLE, AnalogHPReg1);
    AnalogHPReg1 = WM8994_SET_REG_VAL(HPOUT1R, RMV_SHORT, ENABLE, AnalogHPReg1);
    AnalogHPReg1 = WM8994_SET_REG_VAL(HPOUT1R, OUTP, ENABLE, AnalogHPReg1);
    AnalogHPReg1 = WM8994_SET_REG_VAL(HPOUT1R, DLY, ENABLE, AnalogHPReg1);

    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_ANALOGUE_HP_1, AnalogHPReg1);

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SPKMIXL_ATTENUATION, &SpkMixLReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SPKMIXR_ATTENUATION, &SpkMixRReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SPKOUT_MIXERS, &SpkOutMixerReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SPEAKER_MIXER, &SpkMixerReg);

    if (!IsSuccess)
        return IsSuccess;

    SpkMixLReg = 0x0000;
    SpkMixRReg = 0x0000;
    SpkOutMixerReg = 0x001b;
    SpkMixerReg = 0x0003;
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_SPKMIXL_ATTENUATION, SpkMixLReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_SPKMIXR_ATTENUATION, SpkMixRReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_SPKOUT_MIXERS, SpkOutMixerReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_SPEAKER_MIXER, SpkMixerReg);

#if _READBACK //readback confirmation
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_OUTPUT_MIXER_1, &OutMixerReg1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_OUTPUT_MIXER_2, &OutMixerReg2);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_CHARGE_PUMP_1, &ChargePumpReg1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_ANALOGUE_HP_1, &AnalogHPReg1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SPKMIXL_ATTENUATION, &SpkMixLReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SPKMIXR_ATTENUATION, &SpkMixRReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SPKOUT_MIXERS, &SpkOutMixerReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_SPEAKER_MIXER, &SpkMixerReg);
   
    NvOdmOsDebugPrintf("\n # Set Analog Output Audio Path: \n");
    NvOdmOsDebugPrintf("0x2D 0x%x \n", OutMixerReg1);
    NvOdmOsDebugPrintf("0x2E 0x%x \n", OutMixerReg2);
    NvOdmOsDebugPrintf("0x4C 0x%x \n", ChargePumpReg1);
    NvOdmOsDebugPrintf("0x60 0x%x \n", AnalogHPReg1);
    NvOdmOsDebugPrintf("0x22 0x%x \n", SpkMixLReg);
    NvOdmOsDebugPrintf("0x23 0x%x \n", SpkMixRReg);
    NvOdmOsDebugPrintf("0x24 0x%x \n", SpkOutMixerReg);
    NvOdmOsDebugPrintf("0x36 0x%x \n", SpkMixerReg);
    
#endif

    return IsSuccess;

}

static NvBool  
SetDataFormat(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 PortId, 
    NvOdmQueryI2sDataCommFormat DataFormat)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 AIFControlReg1 = 0;
    NvU32 Aif2ControlReg1= 0; 

 //### Set Data Format 
 //WM8994_AIF1_CONTROL_1:Reg 0x300 0x0050

//* AIF2 Interface
//WM8994_AIF2_CONTROL_1 0x310    * AIF2 I2S Mode 24bit Enable // Test CODE
//WM8994_AIF2_MASTER_SLAVE 0x312       * AIF2 CODEC Master Mode Enalbe

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_CONTROL_1, &AIFControlReg1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_CONTROL_1, &Aif2ControlReg1);
    if (!IsSuccess)
        return IsSuccess;

    switch (DataFormat)
    {
        case NvOdmQueryI2sDataCommFormat_Dsp:
#if _VOICE_CALL_SETTING
            //Aif2ControlReg1 = 0x4198;
           // Aif2ControlReg1 = 0x4118;  //
           // Aif2ControlReg1 = 0x4098; 
           // Aif2ControlReg1 = 0x4018; 
            Aif2ControlReg1 = Aif2ControlReg1 | WM8994_AIF1_FMT_I2S_ENABLE;
#else
            Aif2ControlReg1 = Aif2ControlReg1 | WM8994_AIF1_FMT_I2S_ENABLE;
#endif 
            break;

        case NvOdmQueryI2sDataCommFormat_I2S: 
            AIFControlReg1 = AIFControlReg1 | WM8994_AIF1_FMT_I2S_ENABLE;       
            break;

        case NvOdmQueryI2sDataCommFormat_LeftJustified:
            AIFControlReg1 = WM8994_SET_REG_DEF(AIF1, FMT_LEFTJUSTIFIED, ENABLE, AIFControlReg1);                                   
            break;
            
        case NvOdmQueryI2sDataCommFormat_RightJustified:
            AIFControlReg1 = WM8994_SET_REG_DEF(AIF1, FMT_RIGHTJUSTIFIED, ENABLE, AIFControlReg1);                                    
            break;
            
        default:
            // Default will be the i2s mode
            AIFControlReg1 =AIFControlReg1 | WM8994_AIF1_FMT_I2S_ENABLE;                                        

            break;
    }

    //IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_CONTROL_1, AIFControlReg1);
    //IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_CONTROL_1, Aif2ControlReg1);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_CONTROL_1, 0x4010); //HiFi
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_CONTROL_1, 0x0118); //Voice call

#if _READBACK //readback confirmation
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_CONTROL_1, &AIFControlReg1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_CONTROL_1, &Aif2ControlReg1);
    NvOdmOsDebugPrintf("\n ### Set Data Format \n");
    NvOdmOsDebugPrintf("0x300 0x%x \n", AIFControlReg1);
    NvOdmOsDebugPrintf("0x310 0x%x \n", Aif2ControlReg1);
#endif 

    return IsSuccess;
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
    NvBool IsSuccess = NV_TRUE;
    NvU32 AIFControlReg1 = 0;
    NvU32 AIF2ControlReg1 = 0;

 //### Set PCM size 
 //WM8994_AIF1_CONTROL_1:Reg 0x300 0x0050 
 //WM8994_AIF1_CONTROL_1:Reg 0x310 0x0050 

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_CONTROL_1, &AIFControlReg1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_CONTROL_1, &AIF2ControlReg1);
    if (!IsSuccess)
        return IsSuccess;
    
    switch (PcmSize)
    {
        case 16:
            AIFControlReg1 = AIFControlReg1 | WM8994_AIF1_WL_W16BIT_ENABLE;
#if _VOICE_CALL_SETTING
            //AIF2ControlReg1 = 0x4118;  
            //AIF2ControlReg1 = 0xC010;
            AIF2ControlReg1 = AIF2ControlReg1 | WM8994_AIF1_WL_W16BIT_ENABLE;
#else
            AIF2ControlReg1 = AIF2ControlReg1 | WM8994_AIF1_WL_W16BIT_ENABLE;
#endif            
            break;
            
        case 20:
            AIFControlReg1 = WM8994_SET_REG_DEF(AIF1, WL_W20BIT, ENABLE, AIFControlReg1);                                   
            break;
            
        case 24:
            AIFControlReg1 = WM8994_SET_REG_DEF(AIF1, WL_W24BIT, ENABLE, AIFControlReg1);                                      
            break;
            
        case 32:
            AIFControlReg1 = WM8994_SET_REG_DEF(AIF1, WL_W32BIT,  ENABLE, AIFControlReg1);
                                       
            break;
            
        default:
            return NV_FALSE;
    }

    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF1_CONTROL_1, AIFControlReg1);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_AIF2_CONTROL_1, AIF2ControlReg1);
#if _READBACK //readback confirmation
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF1_CONTROL_1, &AIFControlReg1);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_AIF2_CONTROL_1, &AIF2ControlReg1);
    NvOdmOsDebugPrintf("\n ### Set PCM size \n");
    NvOdmOsDebugPrintf("0x300 0x%x \n", AIFControlReg1);
    NvOdmOsDebugPrintf("0x310 0x%x \n", AIF2ControlReg1);
#endif
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

// for master mode
//    NvU32 ADCClkCtlReg, DACClkCtlReg, DACCtlReg;
    WM8994AudioCodecHandle hAudioCodec = (WM8994AudioCodecHandle)hOdmAudioCodec;
   
    if (!hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        hAudioCodec->WCodecControl.AdcSamplingRate = SamplingRate;
        hAudioCodec->WCodecControl.DacSamplingRate = SamplingRate;
        return NV_TRUE;
    }

#if 0 
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIVIDER, &ADCClkCtlReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_DAC_CLOCK_CONTROL, &DACClkCtlReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_DAC_CONTROL, &DACCtlReg);
    if (!IsSuccess)
        return IsSuccess;
    if (hAudioCodec->WCodecInterface.IsUsbMode)
    {
        DACCtlReg = WM8994_SET_REG_VAL(AIF, LRCLKRATE, ENABLE,
                                        DACCtlReg);    
    }
    else
    {
        DACCtlReg = WM8994_SET_REG_VAL(AIF, LRCLKRATE, DISABLE,
                                        DACCtlReg);            
    }
    
    switch (SamplingRate)
    {
        case 8000:
            ADCClkCtlReg = WM8994_SET_REG_DEF(ADC, CLKDIV, 
                                            SR8018HZ, ADCClkCtlReg);
            DACClkCtlReg = WM8994_SET_REG_DEF(DAC, CLKDIV,
                                    SR8018HZ, DACClkCtlReg);
            break;
            
        case 11025:
            ADCClkCtlReg = WM8994_SET_REG_DEF(ADC, CLKDIV, 
                                            SR11025HZ, ADCClkCtlReg);
            DACClkCtlReg = WM8994_SET_REG_DEF(DAC, CLKDIV,
                                    SR11025HZ, DACClkCtlReg);
            break;
        case 22050:
            ADCClkCtlReg = WM8994_SET_REG_DEF(ADC, CLKDIV, 
                                            SR22050HZ, ADCClkCtlReg);
            DACClkCtlReg = WM8994_SET_REG_DEF(DAC, CLKDIV,
                                    SR22050HZ, DACClkCtlReg);
            break;
            
        case 44100:
            ADCClkCtlReg = WM8994_SET_REG_DEF(ADC, CLKDIV, 
                                          SR44100HZ, ADCClkCtlReg);
            DACClkCtlReg = WM8994_SET_REG_DEF(DAC, CLKDIV,
                                    SR44100HZ, DACClkCtlReg);
            break;
            
            
        default:
            return NV_FALSE;
    }

    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIVIDER, ADCClkCtlReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DAC_CLOCK_CONTROL, DACClkCtlReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DAC_CONTROL, DACCtlReg);
    if (IsSuccess)
    {
        hAudioCodec->WCodecControl.AdcSamplingRate = SamplingRate;
        hAudioCodec->WCodecControl.DacSamplingRate = SamplingRate;
    }
#endif 
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

#if 1
    NvU32 PowerControlReg2 = 0;
    NvU32 PowerControlReg4 = 0;
    

    if (PortId != 0)
        return NV_FALSE;

    if ((pConfigIO->InSignalId > 1) || (pConfigIO->OutSignalId != 0))
        return NV_FALSE;

    if ((pConfigIO->InSignalType != NvOdmAudioSignalType_LineIn)
         && (pConfigIO->InSignalType != NvOdmAudioSignalType_MicIn)
         && (pConfigIO->InSignalType != NvOdmAudioSignalType_Aux) )
        return NV_FALSE;

    if (pConfigIO->OutSignalType != NvOdmAudioSignalType_Digital_I2s)
        return NV_FALSE;

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_2,  &PowerControlReg2);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_4,  &PowerControlReg4);
    if (!IsSuccess)
        return IsSuccess;

    if (pConfigIO->InSignalType == NvOdmAudioSignalType_Aux)
    {
        //port enable.
//        IN3CtlReg = WM8994_SET_REG_VAL(R73_IN3L, ENA, ENABLE, IN3CtlReg);
//        IN3CtlReg = WM8994_SET_REG_VAL(R73_IN3R, ENA, ENABLE, IN3CtlReg);
//        IN3CtlReg = WM8994_SET_REG_VAL(IN3L, SHORT, ENABLE, IN3CtlReg);
//        IN3CtlReg = WM8994_SET_REG_VAL(IN3R, SHORT, ENABLE, IN3CtlReg);
    }
    else if (pConfigIO->InSignalType == NvOdmAudioSignalType_LineIn)
    {
        //port enable
  //      INCtlReg = WM8994_SET_REG_VAL(IN2L, ENA, ENABLE, INCtlReg);
  //      INCtlReg = WM8994_SET_REG_VAL(IN2R, ENA, ENABLE, INCtlReg);
    }
    else if (pConfigIO->InSignalType == NvOdmAudioSignalType_MicIn)
    {
        //port enable.
            //* Enable Left/Right Input Mixer (MIXINL/MIXINR), Enable IN2L/IN2R PGA

            PowerControlReg2 = WM8994_SET_REG_VAL(TSHUT, ENA, ENABLE, PowerControlReg2);
            PowerControlReg2 = WM8994_SET_REG_VAL(TSHUT, OPDIS, ENABLE,  PowerControlReg2);    
            PowerControlReg2 = WM8994_SET_REG_VAL(MIXINL, ENA, ENABLE, PowerControlReg2);
            PowerControlReg2 = WM8994_SET_REG_VAL(MIXINR, ENA, ENABLE,  PowerControlReg2);                                      
            PowerControlReg2 = WM8994_SET_REG_VAL(IN1L, ENA, ENABLE,  PowerControlReg2);                                        
            PowerControlReg2 = WM8994_SET_REG_VAL(IN1R, ENA, ENABLE, PowerControlReg2);

            //* Enable ADC (Left/Right), Enable AIF2 ADC (Left/Right) Path

            PowerControlReg4 = WM8994_SET_REG_VAL(AIF2ADCL, ENA, ENABLE,  PowerControlReg4);                                        
            PowerControlReg4 = WM8994_SET_REG_VAL(AIF2ADCR, ENA, ENABLE, PowerControlReg4);
            PowerControlReg4 = WM8994_SET_REG_VAL(ADCL, ENA, ENABLE, PowerControlReg4);
            PowerControlReg4 = WM8994_SET_REG_VAL(ADCR, ENA, ENABLE,  PowerControlReg4); 

            IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_2, PowerControlReg2);
            IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_4, PowerControlReg4);
#if _READBACK // readback confirmation.
        NvOdmOsDebugPrintf("\n ### Set Power On MIC In: \n");
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_2, &PowerControlReg2);
        NvOdmOsDebugPrintf("0x02 0x%x \n", PowerControlReg2);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_4, &PowerControlReg4);
        NvOdmOsDebugPrintf("0x04 0x%x \n", PowerControlReg4);
#endif

    }

 #endif 
 
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
    NvBool IsSuccess = NV_TRUE;  

#if 0
    NvU32 LOut1VolReg, ROut1VolReg, LOut2VolReg, ROut2VolReg;    

    if (PortId != 0)
        return NV_FALSE;


    if ((pConfigIO->InSignalId != 0) || (pConfigIO->OutSignalId > 1))
        return NV_FALSE;

    if (pConfigIO->InSignalType != NvOdmAudioSignalType_Digital_I2s)
        return NV_FALSE;

    if ((pConfigIO->OutSignalType != NvOdmAudioSignalType_LineOut)
         && (pConfigIO->OutSignalType != NvOdmAudioSignalType_HeadphoneOut))
        return NV_FALSE;

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT1_VOLUME, 
        &LOut1VolReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT1_VOLUME, 
        &ROut1VolReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, 
        &LOut2VolReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, 
        &ROut2VolReg);
    if (!IsSuccess)
        return IsSuccess;
    if (pConfigIO->InSignalType == NvOdmAudioSignalType_HeadphoneOut)
    {
        LOut1VolReg = WM8994_SET_REG_VAL(R104_OUT1L, ENA, ENABLE, LOut1VolReg); 
        ROut1VolReg = WM8994_SET_REG_VAL(R105_OUT1R, ENA, ENABLE, ROut1VolReg); 
    }
    else if (pConfigIO->InSignalType == NvOdmAudioSignalType_LineOut)
    {
        LOut2VolReg = WM8994_SET_REG_VAL(R106_OUT2L, ENA, ENABLE, LOut2VolReg); 
        ROut2VolReg = WM8994_SET_REG_VAL(R107_OUT2R, ENA, ENABLE, ROut2VolReg); 
    }
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT1_VOLUME, 
        LOut1VolReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT1_VOLUME, 
        ROut1VolReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, 
        LOut2VolReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, 
        ROut2VolReg);

#endif

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

#if 0
    NvU32 LMIXCtlReg;
    NvU32 RMIXCtlReg;
    NvU32 LMIXVolReg;
    NvU32 RMIXVolReg;

    if ((InputLineId != 0) || (OutputLineId != 0))
        return NV_FALSE;

    if (OutputLineId != 0)
        return NV_FALSE;

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_MIXER_CONTROL, 
        &LMIXCtlReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_MIXER_CONTROL, 
        &RMIXCtlReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_OUTPUT_LEFT_MIXER_VOLUME, 
        &LMIXVolReg);
    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_OUTPUT_RIGHT_MIXER_VOLUME, 
        &RMIXVolReg);
    if (!IsSuccess)
        return IsSuccess;


    if (InputAnalogLineType & NvOdmAudioSignalType_LineIn)
    {
        if (IsEnable)
        {
            LMIXCtlReg = WM8994_SET_REG_VAL(INL_TO, MIXOUTL, ENABLE, LMIXCtlReg);
            RMIXCtlReg = WM8994_SET_REG_VAL(INR_TO, MIXOUTR, ENABLE, RMIXCtlReg);
            LMIXVolReg = WM8994_SET_REG_DEF(INL_MIXOUTL, VOL, DEFAULT, LMIXVolReg);
            RMIXVolReg = WM8994_SET_REG_DEF(INR_MIXOUTR, VOL, DEFAULT, RMIXVolReg);
        }
        else
        {
            LMIXCtlReg = WM8994_SET_REG_VAL(INL_TO, MIXOUTL, DISABLE, LMIXCtlReg);
            RMIXCtlReg = WM8994_SET_REG_VAL(INR_TO, MIXOUTR, DISABLE, RMIXCtlReg);
        }
    }
    
    if (InputAnalogLineType & NvOdmAudioSignalType_MicIn)
    {
        if (IsEnable)
        {
            LMIXCtlReg = WM8994_SET_REG_VAL(INL_TO, MIXOUTL, ENABLE, LMIXCtlReg);
            LMIXVolReg = WM8994_SET_REG_DEF(INL_MIXOUTL, VOL, DEFAULT, LMIXVolReg);
        }
        else
        {
            LMIXCtlReg = WM8994_SET_REG_VAL(INL_TO, MIXOUTL, DISABLE, LMIXCtlReg);
        }
    }
    if (InputAnalogLineType & NvOdmAudioSignalType_Aux)
    {
        if (IsEnable)
        {
            LMIXCtlReg = WM8994_SET_REG_VAL(IN3L_TO, MIXOUTL, ENABLE, LMIXCtlReg);
            RMIXCtlReg = WM8994_SET_REG_VAL(IN3R_TO, MIXOUTR, ENABLE, RMIXCtlReg);
            LMIXVolReg = WM8994_SET_REG_DEF(IN3L_MIXOUTL, VOL, DEFAULT, LMIXVolReg);
            RMIXVolReg = WM8994_SET_REG_DEF(IN3R_MIXOUTR, VOL, DEFAULT, RMIXVolReg);
      }
        else
        {
            LMIXCtlReg = WM8994_SET_REG_VAL(IN3L_TO, MIXOUTL, DISABLE, LMIXCtlReg);
            RMIXCtlReg = WM8994_SET_REG_VAL(IN3R_TO, MIXOUTR, DISABLE, RMIXCtlReg);
        }
    }
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_MIXER_CONTROL, 
        LMIXCtlReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_MIXER_CONTROL, 
        RMIXCtlReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_OUTPUT_LEFT_MIXER_VOLUME, 
        LMIXVolReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_OUTPUT_RIGHT_MIXER_VOLUME, 
        RMIXVolReg);

#endif 

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
    NvBool IsSuccess = NV_TRUE;    
    NvU32 PowerControlReg1 = 0;
    NvU32 PowerControlReg2 = 0;
    NvU32 PowerControlReg3 = 0;
    NvU32 PowerControlReg4 = 0;
    NvU32 PowerControlReg5 = 0;
    NvU32 AntiPopReg = 0;


    WM8994AudioCodecHandle hAudioCodec = (WM8994AudioCodecHandle)hOdmAudioCodec;

    //WM8994_ANTIPOP_2:Reg 0x039 0x6c 
    //WM8994_POWER_MANAGEMENT_1:Reg 0x01 0x3013 
    //WM8994_POWER_MANAGEMENT_3:Reg 0x03 0x300 
    //WM8994_POWER_MANAGEMENT_5:Reg 0x05 0x303 
    if ((SignalType & NvOdmAudioSignalType_Speakers))
    {

        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_1, &PowerControlReg1);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_3, &PowerControlReg3);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_5, &PowerControlReg5);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_ANTIPOP_2, &AntiPopReg);

    if (!IsSuccess)
        return IsSuccess;

        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.HPOutPower = NV_TRUE;

            AntiPopReg = 0x006C;

            //Enable bias generator, Enable VMID, Enable MICBIAS 1, Enable HPOUT1 (Left) and Enable HPOUT1 (Right)
            PowerControlReg1 = WM8994_SET_REG_VAL(VMID, SEL, ENABLE,  PowerControlReg1);
            PowerControlReg1 = WM8994_SET_REG_VAL(MICB1, ENA, ENABLE, PowerControlReg1);
            PowerControlReg1 = WM8994_SET_REG_VAL(BIAS, ENA, ENABLE, PowerControlReg1);

            // enable SPKOUTL:SPKOUTR
            PowerControlReg1 = WM8994_SET_REG_VAL(SPKOUTL, ENA, ENABLE, PowerControlReg1);
            PowerControlReg1 = WM8994_SET_REG_VAL(SPKOUTR, ENA, ENABLE, PowerControlReg1);

             // enable SPKOUTL VOLUME:SPKOUTR VOLUME
            PowerControlReg3 = WM8994_SET_REG_VAL(SPKLVOL, ENA, ENABLE, PowerControlReg3);
            PowerControlReg3 = WM8994_SET_REG_VAL(SPKRVOL, ENA, ENABLE, PowerControlReg3);

#if _VOICE_CALL_SETTING
            PowerControlReg3 = WM8994_SET_REG_VAL(MIXOUTL, ENA, ENABLE, PowerControlReg3);
            PowerControlReg3 = WM8994_SET_REG_VAL(MIXOUTR, ENA, ENABLE, PowerControlReg3);
            PowerControlReg3 = WM8994_SET_REG_VAL(MIXOUTLVOL, ENA, ENABLE, PowerControlReg3);
            PowerControlReg3 = WM8994_SET_REG_VAL(MIXOUTRVOL, ENA, ENABLE, PowerControlReg3);
#endif 

            // enable AIF1DAC1L_ENA / AIF1DAC1R_ENA / DAC1L_ENA / DAC1R_ENA
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF1DAC1L, ENA, ENABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF1DAC1R, ENA, ENABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(DAC1L, ENA, ENABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(DAC1R, ENA, ENABLE, PowerControlReg5);

#if _VOICE_CALL_SETTING
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF2DACL, ENA, ENABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF2DACR, ENA, ENABLE, PowerControlReg5);
#endif


        }
        else
        {
            hAudioCodec->WCodecControl.HPOutPower = NV_FALSE;

           //Enable bias generator, Enable VMID, Enable MICBIAS 1, Enable HPOUT1 (Left) and Enable HPOUT1 (Right)
            PowerControlReg1 = WM8994_SET_REG_VAL(MICB1, ENA, DISABLE, PowerControlReg1);
            PowerControlReg1 = WM8994_SET_REG_VAL(BIAS, ENA, DISABLE, PowerControlReg1);

            // enable SPKOUTL:SPKOUTR
            PowerControlReg1 = WM8994_SET_REG_VAL(SPKOUTL, ENA, DISABLE, PowerControlReg1);
            PowerControlReg1 = WM8994_SET_REG_VAL(SPKOUTR, ENA, DISABLE, PowerControlReg1);

             // enable SPKOUTL VOLUME:SPKOUTR VOLUME
            PowerControlReg3 = WM8994_SET_REG_VAL(SPKLVOL, ENA, DISABLE, PowerControlReg3);
            PowerControlReg3 = WM8994_SET_REG_VAL(SPKRVOL, ENA, DISABLE, PowerControlReg3);

#if _VOICE_CALL_SETTING
            PowerControlReg3 = WM8994_SET_REG_VAL(MIXOUTL, ENA, DISABLE, PowerControlReg3);
            PowerControlReg3 = WM8994_SET_REG_VAL(MIXOUTR, ENA, DISABLE, PowerControlReg3);
            PowerControlReg3 = WM8994_SET_REG_VAL(MIXOUTLVOL, ENA, DISABLE, PowerControlReg3);
            PowerControlReg3 = WM8994_SET_REG_VAL(MIXOUTRVOL, ENA, DISABLE, PowerControlReg3);
#endif 

            // enable AIF1DAC1L_ENA / AIF1DAC1R_ENA / DAC1L_ENA / DAC1R_ENA
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF1DAC1L, ENA, DISABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF1DAC1R, ENA, DISABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(DAC1L, ENA, DISABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(DAC1R, ENA, DISABLE, PowerControlReg5);

#if _VOICE_CALL_SETTING
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF2DACL, ENA, DISABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF2DACR, ENA, DISABLE, PowerControlReg5);
#endif


        }
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_ANTIPOP_2, AntiPopReg);
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_1, PowerControlReg1);
        
        NvOdmOsWaitUS(50); 

        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_3, PowerControlReg3);
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_5, PowerControlReg5);

#if _READBACK // readback confirmation.
        NvOdmOsDebugPrintf("\n ### Set Power On Speaker: SPK AIF1 DAC1 \n");
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_ANTIPOP_2, &AntiPopReg);
        NvOdmOsDebugPrintf("0x039 0x%x \n", AntiPopReg);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_1, &PowerControlReg1);
        NvOdmOsDebugPrintf("0x01 0x%x \n", PowerControlReg1);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_3, &PowerControlReg3); 
        NvOdmOsDebugPrintf("0x03 0x%x \n", PowerControlReg3);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_5, &PowerControlReg5);
        NvOdmOsDebugPrintf("0x05 0x%x \n", PowerControlReg5);
#endif
        if (!IsSuccess)
            return IsSuccess;
    }

    if ((SignalType & NvOdmAudioSignalType_HeadphoneOut))
    {
 //### Set Power On HeadPhone: HPOUT1/AIF1/DAC1 
 //WM8994_POWER_MANAGEMENT_1:Reg 0x01 0x3313 
 //WM8994_POWER_MANAGEMENT_5:Reg 0x05 0x303 

        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_1, &PowerControlReg1);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_5, &PowerControlReg5);

    if (!IsSuccess)
        return IsSuccess;

        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.HPOutPower = NV_TRUE;

            //Enable bias generator, Enable VMID, Enable MICBIAS 1, Enable HPOUT1 (Left) and Enable HPOUT1 (Right)
            PowerControlReg1 = WM8994_SET_REG_VAL(VMID, SEL, ENABLE,  PowerControlReg1);
            PowerControlReg1 = WM8994_SET_REG_VAL(MICB1, ENA, ENABLE, PowerControlReg1);
            PowerControlReg1 = WM8994_SET_REG_VAL(BIAS, ENA, ENABLE, PowerControlReg1);


            // enable HPOUT2_ENA
            PowerControlReg1 = WM8994_SET_REG_VAL(HPOUT2, ENA, ENABLE, PowerControlReg1);

            // enable HPOUT1L_ENA:HPOUT1R_ENA
            PowerControlReg1 = WM8994_SET_REG_VAL(HPOUT1L, ENA, ENABLE, PowerControlReg1);
            PowerControlReg1 = WM8994_SET_REG_VAL(HPOUT1R, ENA, ENABLE, PowerControlReg1);

            // enable AIF1DAC1L_ENA / AIF1DAC1R_ENA / DAC1L_ENA / DAC1R_ENA
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF1DAC1L, ENA, ENABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF1DAC1R, ENA, ENABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(DAC1L, ENA, ENABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(DAC1R, ENA, ENABLE, PowerControlReg5);
        }
        else
        {
            hAudioCodec->WCodecControl.HPOutPower = NV_FALSE;

             //Disable bias generator, Enable VMID, Enable MICBIAS 1, Enable HPOUT1 (Left) and Enable HPOUT1 (Right)

            PowerControlReg1 = WM8994_SET_REG_VAL(MICB1, ENA, DISABLE, PowerControlReg1);
            PowerControlReg1 = WM8994_SET_REG_VAL(BIAS, ENA, DISABLE, PowerControlReg1);

            PowerControlReg1 = WM8994_SET_REG_VAL(HPOUT2, ENA, DISABLE, PowerControlReg1);

            // Disable  HPOUT1L_ENA:HPOUT1R_ENA
            PowerControlReg1 = WM8994_SET_REG_VAL(HPOUT1L, ENA, DISABLE, PowerControlReg1);
            PowerControlReg1 = WM8994_SET_REG_VAL(HPOUT1R, ENA, DISABLE, PowerControlReg1);

            // Disable AIF1DAC1L_ENA / AIF1DAC1R_ENA / DAC1L_ENA / DAC1R_ENA
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF1DAC1L, ENA, DISABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(AIF1DAC1R, ENA, DISABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(DAC1L, ENA, DISABLE, PowerControlReg5);
            PowerControlReg5 = WM8994_SET_REG_VAL(DAC1R, ENA, DISABLE, PowerControlReg5);
        }
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_1, 
            PowerControlReg1);
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_5, 
            PowerControlReg5);

#if _READBACK // readback confirmation.
        NvOdmOsDebugPrintf("\n ### Set Power On HeadPhone: HPOUT1/AIF1/DAC1 \n");
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_1, &PowerControlReg1);
        NvOdmOsDebugPrintf("0x01 0x%x \n", PowerControlReg1);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_5, &PowerControlReg5);
        NvOdmOsDebugPrintf("0x05 0x%x \n", PowerControlReg5);
#endif

        if (!IsSuccess)
            return IsSuccess;
    }

    if (SignalType & NvOdmAudioSignalType_MicIn)
    {
          //0x02 0x6240 SMbus_16inx_16dat     Write  0x34      * Enable Left Input Mixer (MIXINL), Enable IN1L PGA
          //0x04 0x2002 SMbus_16inx_16dat     Write  0x34      * Enable ADC (Left), Enable AIF2 ADC (Left) Path
          IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_2, &PowerControlReg2);
          IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_4, &PowerControlReg4);

        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.LineInPower = NV_TRUE;
            //* Enable TSHUT/TSHUT_OPDIS/Left/Right Input Mixer (MIXINL/MIXINR), Enable IN2L/IN2R PGA

            PowerControlReg2 = WM8994_SET_REG_VAL(TSHUT, ENA, ENABLE, PowerControlReg2);
            PowerControlReg2 = WM8994_SET_REG_VAL(TSHUT, OPDIS, ENABLE,  PowerControlReg2);    
            PowerControlReg2 = WM8994_SET_REG_VAL(MIXINL, ENA, ENABLE, PowerControlReg2);
            PowerControlReg2 = WM8994_SET_REG_VAL(MIXINR, ENA, ENABLE,  PowerControlReg2);                                      
            PowerControlReg2 = WM8994_SET_REG_VAL(IN1L, ENA, ENABLE,  PowerControlReg2);                                        
            PowerControlReg2 = WM8994_SET_REG_VAL(IN1R, ENA, ENABLE, PowerControlReg2);

            //* Enable ADC (Left/Right), Enable AIF2 ADC (Left/Right) Path

            PowerControlReg4 = WM8994_SET_REG_VAL(AIF2ADCL, ENA, ENABLE,  PowerControlReg4);                                        
            PowerControlReg4 = WM8994_SET_REG_VAL(AIF2ADCR, ENA, ENABLE, PowerControlReg4);
            PowerControlReg4 = WM8994_SET_REG_VAL(AIF1ADC1L, ENA, ENABLE,  PowerControlReg4);// ???                                        
            PowerControlReg4 = WM8994_SET_REG_VAL(AIF1ADC1R, ENA, ENABLE, PowerControlReg4);// ???
            PowerControlReg4 = WM8994_SET_REG_VAL(ADCL, ENA, ENABLE, PowerControlReg4);
            PowerControlReg4 = WM8994_SET_REG_VAL(ADCR, ENA, ENABLE,  PowerControlReg4);                                      
                                                                                
        }
        else
        {
            hAudioCodec->WCodecControl.LineInPower = NV_FALSE;
            if (!hAudioCodec->WCodecControl.LineInPower && 
                !hAudioCodec->WCodecControl.MICPower)
            {
                PowerControlReg2 = WM8994_SET_REG_VAL(TSHUT, ENA, DISABLE, PowerControlReg2);
                PowerControlReg2 = WM8994_SET_REG_VAL(TSHUT, OPDIS, DISABLE,  PowerControlReg2);  
                PowerControlReg2 = WM8994_SET_REG_VAL(MIXINL, ENA, DISABLE, PowerControlReg2);
                PowerControlReg2 = WM8994_SET_REG_VAL(MIXINR, ENA, DISABLE,  PowerControlReg2);                                      
                PowerControlReg2 = WM8994_SET_REG_VAL(IN1L, ENA, DISABLE,  PowerControlReg2);                                        
                PowerControlReg2 = WM8994_SET_REG_VAL(IN1R, ENA, DISABLE, PowerControlReg2);

                PowerControlReg4 = WM8994_SET_REG_VAL(AIF2ADCL, ENA, DISABLE,  PowerControlReg4);                                        
                PowerControlReg4 = WM8994_SET_REG_VAL(AIF2ADCR, ENA, DISABLE, PowerControlReg4);
                PowerControlReg4 = WM8994_SET_REG_VAL(AIF1ADC1L, ENA, DISABLE,  PowerControlReg4);// ???                                        
                PowerControlReg4 = WM8994_SET_REG_VAL(AIF1ADC1R, ENA, DISABLE, PowerControlReg4);// ???
                PowerControlReg4 = WM8994_SET_REG_VAL(ADCL, ENA, DISABLE, PowerControlReg4);
                PowerControlReg4 = WM8994_SET_REG_VAL(ADCR, ENA, DISABLE,  PowerControlReg4);                  
            }
        }

    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_2, PowerControlReg2);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_4, PowerControlReg4);

#if _READBACK // readback confirmation.
        NvOdmOsDebugPrintf("\n ### Set Power On MIC In: \n");
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_2, &PowerControlReg2);
        NvOdmOsDebugPrintf("0x02 0x%x \n", PowerControlReg2);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WM8994_POWER_MANAGEMENT_4, &PowerControlReg4);
        NvOdmOsDebugPrintf("0x04 0x%x \n", PowerControlReg4);
#endif


    }

#if 0 

    if (SignalType & NvOdmAudioSignalType_LineIn)
    {
        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.LineInPower = NV_TRUE;
            PowerControlInReg = WM8994_SET_REG_VAL(R9_INL, ENA, ENABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8994_SET_REG_VAL(R9_INR, ENA, ENABLE, 
                                         PowerControlInReg);
        }
        else
        {
            hAudioCodec->WCodecControl.LineInPower = NV_FALSE;
            if (!hAudioCodec->WCodecControl.LineInPower && 
                !hAudioCodec->WCodecControl.MICPower)
            {
                PowerControlInReg = WM8994_SET_REG_VAL(R9_INL, ENA, DISABLE, 
                                             PowerControlInReg);
                PowerControlInReg = WM8994_SET_REG_VAL(R9_INR, ENA, DISABLE, 
                                             PowerControlInReg);
            }
        }
    }
    
    if ((SignalType & NvOdmAudioSignalType_MicIn))
    {
        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.MICPower = NV_TRUE;
            PowerControlInReg = WM8994_SET_REG_VAL(R9_INL, ENA, ENABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8994_SET_REG_VAL(R9_INR, ENA, ENABLE, 
                                         PowerControlInReg);
        }
        else
        {
            hAudioCodec->WCodecControl.MICPower = NV_FALSE;
            if (!hAudioCodec->WCodecControl.LineInPower && 
                !hAudioCodec->WCodecControl.MICPower)
            {
                PowerControlInReg = WM8994_SET_REG_VAL(R9_INL, ENA, DISABLE, 
                                             PowerControlInReg);
                PowerControlInReg = WM8994_SET_REG_VAL(R9_INR, ENA, DISABLE, 
                                             PowerControlInReg);
            }
        }
    }
    
    if (SignalType & NvOdmAudioSignalType_Aux)
    {
        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.AUXPower= NV_TRUE;
            PowerControlInReg = WM8994_SET_REG_VAL(R9_IN3L, ENA, ENABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8994_SET_REG_VAL(R9_IN3R, ENA, ENABLE, 
                                         PowerControlInReg);
        }
        else
        {
            hAudioCodec->WCodecControl.AUXPower= NV_FALSE;
            PowerControlInReg = WM8994_SET_REG_VAL(R9_IN3L, ENA, DISABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8994_SET_REG_VAL(R9_IN3R, ENA, DISABLE, 
                                         PowerControlInReg);
        }
    }    
    if ((SignalType & NvOdmAudioSignalType_Aux) ||
        (SignalType & NvOdmAudioSignalType_MicIn) ||
        (SignalType & NvOdmAudioSignalType_LineIn))
    {
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_INPUT_VOLUME, 
            &LeftInVolReg);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_INPUT_VOLUME, 
            &RightInVolReg);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIGITAL_VOLUME_L, 
            &ADCVolLReg);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIGITAL_VOLUME_R, 
            &ADCVolRReg);
        if (!IsSuccess)
            return IsSuccess;
    
        if (hAudioCodec->WCodecControl.AUXPower || 
            hAudioCodec->WCodecControl.MICPower ||
            hAudioCodec->WCodecControl.LineInPower)
        {
            //Input Mixer power on
            PowerControlInReg = WM8994_SET_REG_VAL(MIXINL, ENA, ENABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8994_SET_REG_VAL(MIXINR, ENA, ENABLE, 
                                         PowerControlInReg);
            //ADC power on
            PowerADCDACReg = WM8994_SET_REG_VAL(R11_ADCL, ENA, ENABLE, 
                                         PowerADCDACReg);
            PowerADCDACReg = WM8994_SET_REG_VAL(R11_ADCR, ENA, ENABLE, 
                                         PowerADCDACReg);
            //Input for Mic/Line-In power on
            LeftInVolReg = WM8994_SET_REG_VAL(R80_INL, ENA, ENABLE, 
                                         LeftInVolReg);
            RightInVolReg = WM8994_SET_REG_VAL(R81_INR, ENA, ENABLE, 
                                         RightInVolReg);
            //ADC enable.
            ADCVolLReg = WM8994_SET_REG_VAL(R66_ADCL, ENA, ENABLE, 
                                         ADCVolLReg);
            ADCVolLReg = WM8994_SET_REG_VAL(R66_ADC, VU, ENABLE, 
                                         ADCVolLReg);
            ADCVolLReg = WM8994_SET_REG_DEF(ADCL, VOL, DEFAULT, 
                                         ADCVolLReg);
            ADCVolRReg = WM8994_SET_REG_VAL(R67_ADCR, ENA, ENABLE, 
                                         ADCVolRReg);
            ADCVolRReg = WM8994_SET_REG_VAL(R67_ADC, VU, ENABLE, 
                                         ADCVolRReg);
            ADCVolRReg = WM8994_SET_REG_DEF(ADCR, VOL, DEFAULT, 
                                         ADCVolRReg);
            
        }
        else
        {
            //Input Mixer power off
            PowerControlInReg = WM8994_SET_REG_VAL(MIXINL, ENA, DISABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8994_SET_REG_VAL(MIXINR, ENA, DISABLE, 
                                         PowerControlInReg);
            //ADC power off
            PowerADCDACReg = WM8994_SET_REG_VAL(R11_ADCL, ENA, DISABLE, 
                                         PowerADCDACReg);
            PowerADCDACReg = WM8994_SET_REG_VAL(R11_ADCR, ENA, DISABLE, 
                                         PowerADCDACReg);
            //Input for Mic/Line-In power off
            LeftInVolReg = WM8994_SET_REG_VAL(R80_INL, ENA, DISABLE, 
                                         LeftInVolReg);
            RightInVolReg = WM8994_SET_REG_VAL(R81_INR, ENA, DISABLE, 
                                         RightInVolReg);        
            //ADC enable.
            ADCVolLReg = WM8994_SET_REG_VAL(R66_ADCL, ENA, DISABLE, 
                                         ADCVolLReg);
            ADCVolLReg = WM8994_SET_REG_VAL(R66_ADC, VU, DISABLE, 
                                         ADCVolLReg);
            ADCVolLReg = WM8994_SET_REG_DEF(ADCL, VOL, DEFAULT, 
                                         ADCVolLReg);
            ADCVolRReg = WM8994_SET_REG_VAL(R67_ADCR, ENA, DISABLE, 
                                         ADCVolRReg);
            ADCVolRReg = WM8994_SET_REG_VAL(R67_ADC, VU, DISABLE, 
                                         ADCVolRReg);
            ADCVolRReg = WM8994_SET_REG_DEF(ADCR, VOL, DEFAULT, 
                                         ADCVolRReg);
        }
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_INPUT_VOLUME, 
            LeftInVolReg);
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_INPUT_VOLUME, 
            RightInVolReg);
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIGITAL_VOLUME_L, 
            ADCVolLReg);
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIGITAL_VOLUME_R, 
            ADCVolRReg);
        if (!IsSuccess)
            return IsSuccess;

    }

    
    if ((SignalType & NvOdmAudioSignalType_LineOut))
    {
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, 
            &LOut2VolReg);
        IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, 
            &ROut2VolReg);
        if (!IsSuccess)
            return IsSuccess;
        
        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.LineOutPower = NV_TRUE;
            //OUT2 enable
            PowerControlOutReg = WM8994_SET_REG_VAL(R10_OUT2L, ENA, ENABLE, 
                                         PowerControlOutReg);
            PowerControlOutReg = WM8994_SET_REG_VAL(R10_OUT2R, ENA, ENABLE, 
                                         PowerControlOutReg);
            //OUT2 Volume enable
            LOut2VolReg = WM8994_SET_REG_VAL(R106_OUT2L, ENA, ENABLE, 
                                         LOut2VolReg);
            ROut2VolReg = WM8994_SET_REG_VAL(R107_OUT2R, ENA, ENABLE, 
                                         ROut2VolReg);
        }
        else
        {
            hAudioCodec->WCodecControl.LineOutPower = NV_FALSE;
            //OUT2 disable
            PowerControlOutReg = WM8994_SET_REG_VAL(R10_OUT2L, ENA, DISABLE, 
                                         PowerControlOutReg);
            PowerControlOutReg = WM8994_SET_REG_VAL(R10_OUT2R, ENA, DISABLE, 
                                         PowerControlOutReg);
            //OUT2 Volume disable
            LOut2VolReg = WM8994_SET_REG_VAL(R106_OUT2L, ENA, DISABLE, 
                                         LOut2VolReg);
            ROut2VolReg = WM8994_SET_REG_VAL(R107_OUT2R, ENA, DISABLE, 
                                         ROut2VolReg);
        }
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, 
            LOut2VolReg);
        IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, 
            ROut2VolReg);
        if (!IsSuccess)
            return IsSuccess;

    }
    if ((SignalType & NvOdmAudioSignalType_HeadphoneOut) ||
        (SignalType & NvOdmAudioSignalType_LineOut))
    {
        if (hAudioCodec->WCodecControl.LineOutPower || 
            hAudioCodec->WCodecControl.HPOutPower )
        {
            //DAC enable.
            PowerADCDACReg = WM8994_SET_REG_VAL(R11_DACL, ENA, ENABLE, 
                                         PowerADCDACReg);
            PowerADCDACReg = WM8994_SET_REG_VAL(R11_DACR, ENA, ENABLE, 
                                         PowerADCDACReg);
            //Out Mixer enable.
            PowerControlInReg = WM8994_SET_REG_VAL(R9_MIXOUTL, ENA, ENABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8994_SET_REG_VAL(R9_MIXOUTR, ENA, ENABLE, 
                                         PowerControlInReg);
        }
        else
        {
            //DAC disable.
            PowerADCDACReg = WM8994_SET_REG_VAL(R11_DACL, ENA, DISABLE, 
                                         PowerADCDACReg);
            PowerADCDACReg = WM8994_SET_REG_VAL(R11_DACR, ENA, DISABLE, 
                                         PowerADCDACReg);
            //Out Mixer disable.
            PowerControlInReg = WM8994_SET_REG_VAL(R9_MIXOUTL, ENA, DISABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8994_SET_REG_VAL(R9_MIXOUTR, ENA, DISABLE, 
                                         PowerControlInReg);
        }

    }
    
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_2, 
                    PowerControlInReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_3, 
                    PowerControlOutReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_4, 
                    PowerADCDACReg);
#endif 

    return IsSuccess;
}

#if LOOPBACK_ENABLED
static NvBool SetLoopBackCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
#if 0
    NvU32 ADCDACCompReg;

    IsSuccess = WM8994_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DAC_COMP, 
        &ADCDACCompReg);
    if (!IsSuccess)
        return IsSuccess;
    
    ADCDACCompReg = WM8994_SET_REG_VAL(LOOPBACK, ENA, ENABLE, 
                                         ADCDACCompReg);
    IsSuccess = WM8994_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DAC_COMP, 
                    ADCDACCompReg);
#endif
    return IsSuccess;
}
#endif

static NvBool OpenWolfsonCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

#if _ONE_PASS_VOICE_CALL_SETTING
    IsSuccess = SetVoiceCallPath(hOdmAudioCodec);
#else
    // Reset the codec.
    IsSuccess = ResetCodec(hOdmAudioCodec);

#if 0 // need to confirm power on/off sequences. 
    if (IsSuccess)
        IsSuccess = SetCodecPower(hOdmAudioCodec, NV_TRUE);
#endif 

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_Speakers/*NvOdmAudioSignalType_HeadphoneOut*/, 0, 
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
            NV_TRUE);
        
    if (IsSuccess)
        IsSuccess = InitializeDigitalInterface(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeHeadphoneOut(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeAnalogInPut(hOdmAudioCodec);
  
    if (IsSuccess)
        IsSuccess = InitializeAnalogOutPut(hOdmAudioCodec);

    if(IsSuccess)
        IsSuccess = InitializeClocking(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_HeadphoneOut, 0,
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight), NV_TRUE);

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_MicIn, 0,
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight), NV_TRUE);

#if LOOPBACK_ENABLED
    // ADC to DAC loopback for test..
    if (IsSuccess)
        IsSuccess = SetLoopBackCodec(hOdmAudioCodec);
#endif

    if (IsSuccess)
        IsSuccess = SetDacMute(hOdmAudioCodec, NV_FALSE);

    if (!IsSuccess)
        ShutDownCodec(hOdmAudioCodec);

#endif 

    return IsSuccess;
}

static const NvOdmAudioPortCaps * ACWM8994GetPortCaps(void)
{
    static const NvOdmAudioPortCaps s_PortCaps =
    {
            1,    // MaxNumberOfInputPort;
            1     // MaxNumberOfOutputPort;
    };    
    return &s_PortCaps;  
}

static const NvOdmAudioOutputPortCaps * 
ACWM8994GetOutputPortCaps(
    NvU32 OutputPortId)
{
    static const NvOdmAudioOutputPortCaps s_OutputPortCaps =
            {
                1,          // MaxNumberOfDigitalInput;
                2,          // MaxNumberOfLineOut;
                2,          // MaxNumberOfHeadphoneOut;
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
ACWM8994GetInputPortCaps(
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

#ifdef LDO_SETUP
NvBool WolfsonLDO_Setup(void)
{       
        Wolfson_hGpio = NvOdmGpioOpen();
        if( !Wolfson_hGpio )
        {
            return NV_FALSE;
        }
        
        s_hWM8994_LD01GpioPin = NvOdmGpioAcquirePinHandle(Wolfson_hGpio,
                                                    'a'-'a',
                                                    1);
        
        /* pull reset low to prevent chip select issues */
        //NvOdmGpioSetState( Wolfson_hGpio, s_hWM8994_LD01GpioPin, 0x0 );
        NvOdmGpioConfig( Wolfson_hGpio, s_hWM8994_LD01GpioPin, NvOdmGpioPinMode_Output);
        
        NvOdmGpioSetState(Wolfson_hGpio, s_hWM8994_LD01GpioPin, 0x1);

        NvOdmOsWaitUS(100000);

    return NV_TRUE;
}

void WolfsonLDO_Release(void)
{
    NvOdmGpioReleasePinHandle(Wolfson_hGpio, s_hWM8994_LD01GpioPin);
    NvOdmGpioClose(Wolfson_hGpio);
    s_hWM8994_LD01GpioPin = 0;
    Wolfson_hGpio = 0;
}
#endif

static NvBool ACWM8994Open(NvOdmPrivAudioCodecHandle hOdmAudioCodec, const NvOdmQueryI2sACodecInterfaceProp *pI2sCodecInt, void* hCodecNotifySem)
{
    NvBool IsSuccess = NV_TRUE;
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 Index;
    WM8994AudioCodecHandle hNewAcodec = (WM8994AudioCodecHandle)hOdmAudioCodec;
    //NvU32 Index;
#ifdef LDO_SETUP
    IsSuccess = WolfsonLDO_Setup();
    if (!IsSuccess)
        return NV_FALSE;
#endif

   hNewAcodec->hOdmService = NULL;

    // Codec interface paramater
    pPerConnectivity = NvOdmPeripheralGetGuid(WOLFSON_8994_CODEC_GUID);
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

    hNewAcodec->WCodecInterface.DeviceAddress = pPerConnectivity->AddressList[Index].Address;

    if (!pI2sCodecInt)
        return NV_FALSE;
    
    hNewAcodec->hOdmService = NULL;
    hNewAcodec->hOdmServicePmuDevice = NULL;

    // Codec interface paramater
    hNewAcodec->WCodecInterface.IsCodecMasterMode = pI2sCodecInt->IsCodecMasterMode;
    hNewAcodec->WCodecInterface.IsUsbMode = pI2sCodecInt->IsUsbMode;
    hNewAcodec->WCodecInterface.I2sCodecDataCommFormat = pI2sCodecInt->I2sCodecDataCommFormat;
    hNewAcodec->WCodecInterface.I2sCodecLRLineControl = pI2sCodecInt->I2sCodecLRLineControl;

    // Codec control parameter
    hNewAcodec->WCodecControl.LineInVolLeft = 0;
    hNewAcodec->WCodecControl.LineInVolRight = 0;
    hNewAcodec->WCodecControl.HPOutVolLeft = 0;
    hNewAcodec->WCodecControl.HPOutVolRight = 0;
    hNewAcodec->WCodecControl.LineOutVolLeft = 0;
    hNewAcodec->WCodecControl.LineOutVolRight = 0;
    hNewAcodec->WCodecControl.AdcSamplingRate = 0;
    hNewAcodec->WCodecControl.DacSamplingRate = 0;
    hNewAcodec->WCodecControl.AUXInVolLeft = 0;
    hNewAcodec->WCodecControl.AUXInVolRight = 0;
    hNewAcodec->WCodecControl.MICINVol = 0;
    hNewAcodec->WCodecControl.LineInPower = NV_FALSE;
    hNewAcodec->WCodecControl.MICPower = NV_FALSE;
    hNewAcodec->WCodecControl.AUXPower = NV_FALSE;
    hNewAcodec->WCodecControl.LineOutPower = NV_FALSE;
    hNewAcodec->WCodecControl.HPOutPower = NV_FALSE;

    IsSuccess = SetPowerOnCodec(hNewAcodec, NV_TRUE);

    if (!IsSuccess)
        goto ErrorExit;        

    // Opening the I2C ODM Service

    hNewAcodec->hOdmService = NvOdmI2cPinMuxOpen(NvOdmIoModule_I2c, 1, NvOdmI2cPinMap_Config2);

    if (!hNewAcodec->hOdmService)
        goto ErrorExit;

    IsSuccess = OpenWolfsonCodec(hNewAcodec);

    if (IsSuccess)
        return NV_TRUE;

    NvOdmI2cClose(hNewAcodec->hOdmService);

ErrorExit:
    return NV_FALSE;
}

static void ACWM8994Close(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    WM8994AudioCodecHandle hAudioCodec = (WM8994AudioCodecHandle)hOdmAudioCodec;
    if (hOdmAudioCodec != NULL)
    {
#ifdef LDO_SETUP
        WolfsonLDO_Release();
#endif
        SetDacMute(hOdmAudioCodec, NV_TRUE);
        SetAntiPopCodec(hOdmAudioCodec, NV_TRUE);
        ShutDownCodec(hOdmAudioCodec);
        SetAntiPopCodec(hOdmAudioCodec, NV_FALSE);
        (void)SetPowerOnCodec(hOdmAudioCodec, NV_FALSE);
        NvOdmI2cClose(hAudioCodec->hOdmService);
        NvOdmOsFree(hOdmAudioCodec);
    }
}


static const NvOdmAudioWave * 
ACWM8994GetPortPcmCaps(
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
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 8000,  32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 32000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 44100, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 48000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 88200, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 96000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
    };

    *pValidListCount = sizeof(s_AudioPcmProps)/sizeof(s_AudioPcmProps[0]);
    return &s_AudioPcmProps[0];
}


// need to add sampling rate routine.
static NvBool 
ACWM8994SetPortPcmProps(
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
            IsSuccess = SetDataFormat(hOdmAudioCodec, PortType, pWaveParams->DataFormat);
        return IsSuccess;
    }

    return NV_FALSE;    
}

static void 
ACWM8994SetVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioVolume *pVolume,
    NvU32 ListCount)
{

#if 0
    NvU32 PortId;
    NvU32 Index;
    
    PortId = NVODMAUDIO_GET_PORT_ID(PortName);
    if (PortId != 0)
        return;

    for (Index = 0; Index < ListCount; ++Index)
    {
        if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_HeadphoneOut)
        {
            (void)SetHeadphoneOutVolume(hOdmAudioCodec, pVolume[Index].ChannelMask, 
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_LineOut)
        {
            (void)SetLineOutVolume(hOdmAudioCodec, pVolume[Index].ChannelMask, 
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_LineIn)
        {
            (void)SetLineInVolume(hOdmAudioCodec, pVolume[Index].ChannelMask, 
                        pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_MicIn)
        {
            (void)SetMicrophoneInVolume(hOdmAudioCodec, pVolume[Index].VolumeLevel);
        }
        else if (pVolume[Index].SignalType ==  NvOdmAudioSignalType_Aux)
        {
            (void)SetAUXInVolume(hOdmAudioCodec, pVolume[Index].ChannelMask,
                pVolume[Index].VolumeLevel, pVolume[Index].VolumeLevel);
        }
    }
#endif 

}


static void 
ACWM8994SetMuteControl(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioMuteData *pMuteParam,
    NvU32 ListCount)
{
#if 0
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
        else if (pMuteParam->SignalType ==  NvOdmAudioSignalType_LineOut)
        {
            (void)SetLineOutMute(hOdmAudioCodec, pMuteParam->ChannelMask, 
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
        else if (pMuteParam->SignalType ==  NvOdmAudioSignalType_Aux)
        {
            (void)SetAUXInMute(hOdmAudioCodec, pMuteParam->ChannelMask, 
                        pMuteParam->IsMute);
        }
    }

#endif 
}


static NvBool 
ACWM8994SetConfiguration(
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
        return ACWM8994SetPortPcmProps(hOdmAudioCodec, PortName, pConfigData);
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
ACWM8994GetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
}

static void 
ACWM8994SetPowerState(
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
ACWM8994SetPowerMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioCodecPowerMode PowerMode)
{
    
}

static void 
ACWM8994SetOperationMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvBool IsMaster)
{

}

void W8994InitCodecInterface(NvOdmAudioCodec *pCodec)
{
    pCodec->pfnGetPortCaps = ACWM8994GetPortCaps;
    pCodec->pfnGetOutputPortCaps = ACWM8994GetOutputPortCaps;
    pCodec->pfnGetInputPortCaps = ACWM8994GetInputPortCaps;
    pCodec->pfnGetPortPcmCaps = ACWM8994GetPortPcmCaps;
    pCodec->pfnOpen = ACWM8994Open;
    pCodec->pfnClose = ACWM8994Close;
    pCodec->pfnSetVolume = ACWM8994SetVolume;
    pCodec->pfnSetMuteControl = ACWM8994SetMuteControl;
    pCodec->pfnSetConfiguration = ACWM8994SetConfiguration;
    pCodec->pfnGetConfiguration = ACWM8994GetConfiguration;
    pCodec->pfnSetPowerState = ACWM8994SetPowerState;
    pCodec->pfnSetPowerMode = ACWM8994SetPowerMode;
    pCodec->pfnSetOperationMode = ACWM8994SetOperationMode;
    pCodec->hCodecPrivate = &s_W8994;
    pCodec->IsInited = NV_TRUE;

}
