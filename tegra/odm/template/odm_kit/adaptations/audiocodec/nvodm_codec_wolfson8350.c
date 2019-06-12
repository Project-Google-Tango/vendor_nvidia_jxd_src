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
#include "wolfson8350_registers.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"

#define AUDIO_I2C_SPEED_KHZ    400
#define ENABLE 1
#define DISABLE 0

#define WM8350_POWER_CONTROL_ENABLE 1 
#define WM8350_AUDIOCODEC_DEBUG_PRINT 1

#define LINEOUT_ENABLED  0
#define ALL_INPUT_ENABLED  0
#define LOOPBACK_ENABLED  0


#define WM8350_SET_REG_VAL(r,f,n, v) \
    (((v)& (~(WM8350_##r##_##f##_MASK))) | \
        (((n << WM8350_##r##_##f##_SHIFT) & WM8350_##r##_##f##_MASK) ))

#define WM8350_SET_REG_DEF(r,f,c,v) \
    (((v)& (~(WM8350_##r##_##f##_MASK))) | \
        (((WM8350_##r##_##f##_##c << WM8350_##r##_##f##_SHIFT) & WM8350_##r##_##f##_MASK) ))

#define WM8350_GET_REG_VAL(r,f,v) \
    ((((v & WM8350_##r##_##f##_MASK) >> WM8350_##r##_##f##_SHIFT)))

/*
 * Wolfson codec register sequences.
 */
typedef enum
{
    WCodecRegIndex_RESET_ID = 0,
    WCodecRegIndex_ID,
    WCodecRegIndex_SYSTEM_CONTROL_1,
    WCodecRegIndex_SYSTEM_CONTROL_2,
    WCodecRegIndex_SYSTEM_HIBERNATE,
    WCodecRegIndex_INTERFACE_CONTROL,                  
    WCodecRegIndex_POWER_MGMT_1,                       
    WCodecRegIndex_POWER_MGMT_2,                       
    WCodecRegIndex_POWER_MGMT_3,                       
    WCodecRegIndex_POWER_MGMT_4,                       
    WCodecRegIndex_POWER_MGMT_5,                       
    WCodecRegIndex_POWER_MGMT_6,                       
    WCodecRegIndex_POWER_MGMT_7,                       
    WCodecRegIndex_RTC_SECONDS_MINUTES,                
    WCodecRegIndex_RTC_HOURS_DAY,                      
    WCodecRegIndex_RTC_DATE_MONTH,                     
    WCodecRegIndex_RTC_YEAR,                           
    WCodecRegIndex_ALARM_SECONDS_MINUTES,              
    WCodecRegIndex_ALARM_HOURS_DAY,                    
    WCodecRegIndex_ALARM_DATE_MONTH,                   
    WCodecRegIndex_RTC_TIME_CONTROL,                   
    WCodecRegIndex_SYSTEM_INTERRUPTS,                  
    WCodecRegIndex_INTERRUPT_STATUS_1,                 
    WCodecRegIndex_INTERRUPT_STATUS_2,                 
    WCodecRegIndex_POWER_UP_INTERRUPT_STATUS,          
    WCodecRegIndex_UNDER_VOLTAGE_INTERRUPT_STATUS,     
    WCodecRegIndex_OVER_CURRENT_INTERRUPT_STATUS,      
    WCodecRegIndex_GPIO_INTERRUPT_STATUS,              
    WCodecRegIndex_COMPARATOR_INTERRUPT_STATUS,        
    WCodecRegIndex_SYSTEM_INTERRUPTS_MASK,             
    WCodecRegIndex_INTERRUPT_STATUS_1_MASK,            
    WCodecRegIndex_INTERRUPT_STATUS_2_MASK,            
    WCodecRegIndex_POWER_UP_INTERRUPT_STATUS_MASK,     
    WCodecRegIndex_UNDER_VOLTAGE_INTERRUPT_STATUS_MASK,
    WCodecRegIndex_OVER_CURRENT_INTERRUPT_STATUS_MASK,
    WCodecRegIndex_GPIO_INTERRUPT_STATUS_MASK,         
    WCodecRegIndex_COMPARATOR_INTERRUPT_STATUS_MASK,   
    WCodecRegIndex_CLOCK_CONTROL_1,                    
    WCodecRegIndex_CLOCK_CONTROL_2,                    
    WCodecRegIndex_FLL_CONTROL_1,                      
    WCodecRegIndex_FLL_CONTROL_2,                      
    WCodecRegIndex_FLL_CONTROL_3,                      
    WCodecRegIndex_FLL_CONTROL_4,                      
    WCodecRegIndex_DAC_CONTROL,                        
    WCodecRegIndex_DAC_DIGITAL_VOLUME_L,               
    WCodecRegIndex_DAC_DIGITAL_VOLUME_R,               
    WCodecRegIndex_DAC_LR_RATE,                        
    WCodecRegIndex_DAC_CLOCK_CONTROL,                  
    WCodecRegIndex_DAC_MUTE,                           
    WCodecRegIndex_DAC_MUTE_VOLUME,                    
    WCodecRegIndex_DAC_SIDE,                           
    WCodecRegIndex_ADC_CONTROL,                        
    WCodecRegIndex_ADC_DIGITAL_VOLUME_L,               
    WCodecRegIndex_ADC_DIGITAL_VOLUME_R,               
    WCodecRegIndex_ADC_DIVIDER,                        
    WCodecRegIndex_ADC_LR_RATE,                        
    WCodecRegIndex_INPUT_CONTROL,                      
    WCodecRegIndex_IN3_INPUT_CONTROL,                  
    WCodecRegIndex_MIC_BIAS_CONTROL,                   
    WCodecRegIndex_OUTPUT_CONTROL,                     
    WCodecRegIndex_JACK_DETECT,                        
    WCodecRegIndex_ANTI_POP_CONTROL,                   
    WCodecRegIndex_LEFT_INPUT_VOLUME,                  
    WCodecRegIndex_RIGHT_INPUT_VOLUME,                 
    WCodecRegIndex_LEFT_MIXER_CONTROL,                 
    WCodecRegIndex_RIGHT_MIXER_CONTROL,                
    WCodecRegIndex_OUT3_MIXER_CONTROL,                 
    WCodecRegIndex_OUT4_MIXER_CONTROL,                 
    WCodecRegIndex_OUTPUT_LEFT_MIXER_VOLUME,           
    WCodecRegIndex_OUTPUT_RIGHT_MIXER_VOLUME,          
    WCodecRegIndex_INPUT_MIXER_VOLUME_L,               
    WCodecRegIndex_INPUT_MIXER_VOLUME_R,               
    WCodecRegIndex_INPUT_MIXER_VOLUME,                 
    WCodecRegIndex_LOUT1_VOLUME,                       
    WCodecRegIndex_ROUT1_VOLUME,                       
    WCodecRegIndex_LOUT2_VOLUME,                       
    WCodecRegIndex_ROUT2_VOLUME,                       
    WCodecRegIndex_BEEP_VOLUME,                        
    WCodecRegIndex_AI_FORMATING,                       
    WCodecRegIndex_ADC_DAC_COMP,                       
    WCodecRegIndex_AI_ADC_CONTROL,                     
    WCodecRegIndex_AI_DAC_CONTROL,                     
    WCodecRegIndex_AIF_TEST,                           
    WCodecRegIndex_GPIO_DEBOUNCE,                      
    WCodecRegIndex_GPIO_PIN_PULL_UP_CONTROL,           
    WCodecRegIndex_GPIO_PULL_DOWN_CONTROL,             
    WCodecRegIndex_GPIO_INTERRUPT_MODE,                
    WCodecRegIndex_GPIO_CONTROL,                       
    WCodecRegIndex_GPIO_CONFIGURATION_I_O,             
    WCodecRegIndex_GPIO_PIN_POLARITY_TYPE,             
    WCodecRegIndex_GPIO_FUNCTION_SELECT_1,             
    WCodecRegIndex_GPIO_FUNCTION_SELECT_2,             
    WCodecRegIndex_GPIO_FUNCTION_SELECT_3,             
    WCodecRegIndex_GPIO_FUNCTION_SELECT_4,             
    WCodecRegIndex_DIGITISER_CONTROL_1,                
    WCodecRegIndex_DIGITISER_CONTROL_2,                
    WCodecRegIndex_AUX1_READBACK,                      
    WCodecRegIndex_AUX2_READBACK,                      
    WCodecRegIndex_AUX3_READBACK,                      
    WCodecRegIndex_AUX4_READBACK,                      
    WCodecRegIndex_USB_VOLTAGE_READBACK,               
    WCodecRegIndex_LINE_VOLTAGE_READBACK,              
    WCodecRegIndex_BATT_VOLTAGE_READBACK,              
    WCodecRegIndex_CHIP_TEMP_READBACK,                 
    WCodecRegIndex_GENERIC_COMPARATOR_CONTROL,         
    WCodecRegIndex_GENERIC_COMPARATOR_1,               
    WCodecRegIndex_GENERIC_COMPARATOR_2,               
    WCodecRegIndex_GENERIC_COMPARATOR_3,               
    WCodecRegIndex_GENERIC_COMPARATOR_4,               
    WCodecRegIndex_BATTERY_CHARGER_CONTROL_1,          
    WCodecRegIndex_BATTERY_CHARGER_CONTROL_2,          
    WCodecRegIndex_BATTERY_CHARGER_CONTROL_3,          
    WCodecRegIndex_CURRENT_SINK_DRIVER_A,              
    WCodecRegIndex_CSA_FLASH_CONTROL,                  
    WCodecRegIndex_CURRENT_SINK_DRIVER_B,              
    WCodecRegIndex_CSB_FLASH_CONTROL,                  
    WCodecRegIndex_DCDC_LDO_REQUESTED,                 
    WCodecRegIndex_DCDC_ACTIVE_OPTIONS,                
    WCodecRegIndex_DCDC_SLEEP_OPTIONS,                 
    WCodecRegIndex_POWER_CHECK_COMPARATOR,             
    WCodecRegIndex_DCDC1_CONTROL,                      
    WCodecRegIndex_DCDC1_TIMEOUTS,                     
    WCodecRegIndex_DCDC1_LOW_POWER,                    
    WCodecRegIndex_DCDC2_CONTROL,                      
    WCodecRegIndex_DCDC2_TIMEOUTS,                     
    WCodecRegIndex_DCDC3_CONTROL,                      
    WCodecRegIndex_DCDC3_TIMEOUTS,                     
    WCodecRegIndex_DCDC3_LOW_POWER,                    
    WCodecRegIndex_DCDC4_CONTROL,                      
    WCodecRegIndex_DCDC4_TIMEOUTS,                     
    WCodecRegIndex_DCDC4_LOW_POWER,                    
    WCodecRegIndex_DCDC5_CONTROL,                      
    WCodecRegIndex_DCDC5_TIMEOUTS,                     
    WCodecRegIndex_DCDC6_CONTROL,                      
    WCodecRegIndex_DCDC6_TIMEOUTS,                     
    WCodecRegIndex_DCDC6_LOW_POWER,                    
    WCodecRegIndex_LIMIT_SWITCH_CONTROL,               
    WCodecRegIndex_LDO1_CONTROL,                       
    WCodecRegIndex_LDO1_TIMEOUTS,                      
    WCodecRegIndex_LDO1_LOW_POWER,                     
    WCodecRegIndex_LDO2_CONTROL,                       
    WCodecRegIndex_LDO2_TIMEOUTS,                      
    WCodecRegIndex_LDO2_LOW_POWER,                     
    WCodecRegIndex_LDO3_CONTROL,                       
    WCodecRegIndex_LDO3_TIMEOUTS,                      
    WCodecRegIndex_LDO3_LOW_POWER,                     
    WCodecRegIndex_LDO4_CONTROL,                       
    WCodecRegIndex_LDO4_TIMEOUTS,                      
    WCodecRegIndex_LDO4_LOW_POWER,                     
    WCodecRegIndex_VCC_FAULT_MASKS,                    
    WCodecRegIndex_MAIN_BANDGAP_CONTROL,               
    WCodecRegIndex_OSC_CONTROL,                        
    WCodecRegIndex_RTC_TICK_CONTROL,                   
    WCodecRegIndex_RAM_BIST_1,                         
    WCodecRegIndex_DCDC_LDO_STATUS,                    
    WCodecRegIndex_GPIO_PIN_STATUS,   
    WCodecRegIndex_Max,
    WCodecRegIndex_Force32             = 0x7FFFFFFF
} WCodecRegIndex;

NvU8 g_WCodecRegAddress[WM8350_REGISTER_COUNT] = {
    WM8350_RESET_ID,
    WM8350_ID,
    WM8350_SYSTEM_CONTROL_1,
    WM8350_SYSTEM_CONTROL_2,
    WM8350_SYSTEM_HIBERNATE,
    WM8350_INTERFACE_CONTROL,                  
    WM8350_POWER_MGMT_1,                       
    WM8350_POWER_MGMT_2,                       
    WM8350_POWER_MGMT_3,                       
    WM8350_POWER_MGMT_4,                       
    WM8350_POWER_MGMT_5,                       
    WM8350_POWER_MGMT_6,                       
    WM8350_POWER_MGMT_7,                       
    WM8350_RTC_SECONDS_MINUTES,                
    WM8350_RTC_HOURS_DAY,                      
    WM8350_RTC_DATE_MONTH,                     
    WM8350_RTC_YEAR,                           
    WM8350_ALARM_SECONDS_MINUTES,              
    WM8350_ALARM_HOURS_DAY,                    
    WM8350_ALARM_DATE_MONTH,                   
    WM8350_RTC_TIME_CONTROL,                   
    WM8350_SYSTEM_INTERRUPTS,                  
    WM8350_INTERRUPT_STATUS_1,                 
    WM8350_INTERRUPT_STATUS_2,                 
    WM8350_POWER_UP_INTERRUPT_STATUS,          
    WM8350_UNDER_VOLTAGE_INTERRUPT_STATUS,     
    WM8350_OVER_CURRENT_INTERRUPT_STATUS,      
    WM8350_GPIO_INTERRUPT_STATUS,              
    WM8350_COMPARATOR_INTERRUPT_STATUS,        
    WM8350_SYSTEM_INTERRUPTS_MASK,             
    WM8350_INTERRUPT_STATUS_1_MASK,            
    WM8350_INTERRUPT_STATUS_2_MASK,            
    WM8350_POWER_UP_INTERRUPT_STATUS_MASK,     
    WM8350_UNDER_VOLTAGE_INTERRUPT_STATUS_MASK,
    WM8350_OVER_CURRENT_INTERRUPT_STATUS_MASK,
    WM8350_GPIO_INTERRUPT_STATUS_MASK,         
    WM8350_COMPARATOR_INTERRUPT_STATUS_MASK,   
    WM8350_CLOCK_CONTROL_1,                    
    WM8350_CLOCK_CONTROL_2,                    
    WM8350_FLL_CONTROL_1,                      
    WM8350_FLL_CONTROL_2,                      
    WM8350_FLL_CONTROL_3,                      
    WM8350_FLL_CONTROL_4,                      
    WM8350_DAC_CONTROL,                        
    WM8350_DAC_DIGITAL_VOLUME_L,               
    WM8350_DAC_DIGITAL_VOLUME_R,               
    WM8350_DAC_LR_RATE,                        
    WM8350_DAC_CLOCK_CONTROL,                  
    WM8350_DAC_MUTE,                           
    WM8350_DAC_MUTE_VOLUME,                    
    WM8350_DAC_SIDE,                           
    WM8350_ADC_CONTROL,                        
    WM8350_ADC_DIGITAL_VOLUME_L,               
    WM8350_ADC_DIGITAL_VOLUME_R,               
    WM8350_ADC_DIVIDER,                        
    WM8350_ADC_LR_RATE,                        
    WM8350_INPUT_CONTROL,                      
    WM8350_IN3_INPUT_CONTROL,                  
    WM8350_MIC_BIAS_CONTROL,                   
    WM8350_OUTPUT_CONTROL,                     
    WM8350_JACK_DETECT,                        
    WM8350_ANTI_POP_CONTROL,                   
    WM8350_LEFT_INPUT_VOLUME,                  
    WM8350_RIGHT_INPUT_VOLUME,                 
    WM8350_LEFT_MIXER_CONTROL,                 
    WM8350_RIGHT_MIXER_CONTROL,                
    WM8350_OUT3_MIXER_CONTROL,                 
    WM8350_OUT4_MIXER_CONTROL,                 
    WM8350_OUTPUT_LEFT_MIXER_VOLUME,           
    WM8350_OUTPUT_RIGHT_MIXER_VOLUME,          
    WM8350_INPUT_MIXER_VOLUME_L,               
    WM8350_INPUT_MIXER_VOLUME_R,               
    WM8350_INPUT_MIXER_VOLUME,                 
    WM8350_LOUT1_VOLUME,                       
    WM8350_ROUT1_VOLUME,                       
    WM8350_LOUT2_VOLUME,                       
    WM8350_ROUT2_VOLUME,                       
    WM8350_BEEP_VOLUME,                        
    WM8350_AI_FORMATING,                       
    WM8350_ADC_DAC_COMP,                       
    WM8350_AI_ADC_CONTROL,                     
    WM8350_AI_DAC_CONTROL,                     
    WM8350_AIF_TEST,                           
    WM8350_GPIO_DEBOUNCE,                      
    WM8350_GPIO_PIN_PULL_UP_CONTROL,           
    WM8350_GPIO_PULL_DOWN_CONTROL,             
    WM8350_GPIO_INTERRUPT_MODE,                
    WM8350_GPIO_CONTROL,                       
    WM8350_GPIO_CONFIGURATION_I_O,             
    WM8350_GPIO_PIN_POLARITY_TYPE,             
    WM8350_GPIO_FUNCTION_SELECT_1,             
    WM8350_GPIO_FUNCTION_SELECT_2,             
    WM8350_GPIO_FUNCTION_SELECT_3,             
    WM8350_GPIO_FUNCTION_SELECT_4,             
    WM8350_DIGITISER_CONTROL_1,                
    WM8350_DIGITISER_CONTROL_2,                
    WM8350_AUX1_READBACK,                      
    WM8350_AUX2_READBACK,                      
    WM8350_AUX3_READBACK,                      
    WM8350_AUX4_READBACK,                      
    WM8350_USB_VOLTAGE_READBACK,               
    WM8350_LINE_VOLTAGE_READBACK,              
    WM8350_BATT_VOLTAGE_READBACK,              
    WM8350_CHIP_TEMP_READBACK,                 
    WM8350_GENERIC_COMPARATOR_CONTROL,         
    WM8350_GENERIC_COMPARATOR_1,               
    WM8350_GENERIC_COMPARATOR_2,               
    WM8350_GENERIC_COMPARATOR_3,               
    WM8350_GENERIC_COMPARATOR_4,               
    WM8350_BATTERY_CHARGER_CONTROL_1,          
    WM8350_BATTERY_CHARGER_CONTROL_2,          
    WM8350_BATTERY_CHARGER_CONTROL_3,          
    WM8350_CURRENT_SINK_DRIVER_A,              
    WM8350_CSA_FLASH_CONTROL,                  
    WM8350_CURRENT_SINK_DRIVER_B,              
    WM8350_CSB_FLASH_CONTROL,                  
    WM8350_DCDC_LDO_REQUESTED,                 
    WM8350_DCDC_ACTIVE_OPTIONS,                
    WM8350_DCDC_SLEEP_OPTIONS,                 
    WM8350_POWER_CHECK_COMPARATOR,             
    WM8350_DCDC1_CONTROL,                      
    WM8350_DCDC1_TIMEOUTS,                     
    WM8350_DCDC1_LOW_POWER,                    
    WM8350_DCDC2_CONTROL,                      
    WM8350_DCDC2_TIMEOUTS,                     
    WM8350_DCDC3_CONTROL,                      
    WM8350_DCDC3_TIMEOUTS,                     
    WM8350_DCDC3_LOW_POWER,                    
    WM8350_DCDC4_CONTROL,                      
    WM8350_DCDC4_TIMEOUTS,                     
    WM8350_DCDC4_LOW_POWER,                    
    WM8350_DCDC5_CONTROL,                      
    WM8350_DCDC5_TIMEOUTS,                     
    WM8350_DCDC6_CONTROL,                      
    WM8350_DCDC6_TIMEOUTS,                     
    WM8350_DCDC6_LOW_POWER,                    
    WM8350_LIMIT_SWITCH_CONTROL,               
    WM8350_LDO1_CONTROL,                       
    WM8350_LDO1_TIMEOUTS,                      
    WM8350_LDO1_LOW_POWER,                     
    WM8350_LDO2_CONTROL,                       
    WM8350_LDO2_TIMEOUTS,                      
    WM8350_LDO2_LOW_POWER,                     
    WM8350_LDO3_CONTROL,                       
    WM8350_LDO3_TIMEOUTS,                      
    WM8350_LDO3_LOW_POWER,                     
    WM8350_LDO4_CONTROL,                       
    WM8350_LDO4_TIMEOUTS,                      
    WM8350_LDO4_LOW_POWER,                     
    WM8350_VCC_FAULT_MASKS,                    
    WM8350_MAIN_BANDGAP_CONTROL,               
    WM8350_OSC_CONTROL,                        
    WM8350_RTC_TICK_CONTROL,                   
    WM8350_RAM_BIST_1,                         
    WM8350_DCDC_LDO_STATUS,                    
    WM8350_GPIO_PIN_STATUS     
};    
/*
 * The codec control variables.
 */
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

typedef struct WM8350AudioCodecRec
{
    NvOdmServicesI2cHandle hOdmService;
    WolfsonCodecControls WCodecControl;
    NvOdmQueryI2sACodecInterfaceProp WCodecInterface;
    NvOdmServicesPmuHandle hOdmServicePmuDevice;
    NvU32 WCodecRegVals[WCodecRegIndex_Max];
} WM8350AudioCodec, *WM8350AudioCodecHandle;

static WM8350AudioCodec s_W8350 = {0};       /* Unique audio codec for the whole system*/

#define WM8350_WRITETIMEOUT 1000 // 1 seconds

static NvBool 
WriteWolfsonRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 Data)
{
    NvOdmI2cStatus I2cTransStatus;    
    NvU8 pTxBuffer[3];
    NvOdmI2cTransactionInfo TransactionInfo;
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;
    
    pTxBuffer[0] = (NvU8)(g_WCodecRegAddress[RegIndex] & 0xFF);
    pTxBuffer[1] = (NvU8)((Data >> 8) & 0xFF);
    pTxBuffer[2] = (NvU8)((Data) & 0xFF);

    TransactionInfo.Address = hAudioCodec->WCodecInterface.DeviceAddress;
    TransactionInfo.Buf = pTxBuffer;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 3;

    // write the Accelator offset (from where data is to be read)
    I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, AUDIO_I2C_SPEED_KHZ,
                    WM8350_WRITETIMEOUT);

    // HW- BUG!! If timeout, again retransmit the data.                    
    if (I2cTransStatus == NvOdmI2cStatus_Timeout)
        I2cTransStatus = NvOdmI2cTransaction(hAudioCodec->hOdmService, &TransactionInfo, 1, AUDIO_I2C_SPEED_KHZ,
                        WM8350_WRITETIMEOUT);

    if (I2cTransStatus == NvOdmI2cStatus_Success)
    {
        hAudioCodec->WCodecRegVals[RegIndex] = Data;
        return NV_TRUE;
    }    

    NvOdmOsDebugPrintf("Error in the i2c communication 0x%08x\n", I2cTransStatus);
    return NV_FALSE;
}

static NvBool ReadWolfsonRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU8 Addr,
    NvU32 *Data)
{
    NvU8 *pReadBuffer;
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo *pTransactionInfo;
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;

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

    pReadBuffer[0] = g_WCodecRegAddress[Addr] & 0xFF;

    pTransactionInfo[0].Address = hAudioCodec->WCodecInterface.DeviceAddress;
    pTransactionInfo[0].Buf = pReadBuffer;
    pTransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
    pTransactionInfo[0].NumBytes = 1;

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
                NvOdmOsDebugPrintf("NvOdmPmuI2cRead Failed: Timeout\n");
                break;
             case NvOdmI2cStatus_SlaveNotFound:
             default:
                NvOdmOsDebugPrintf("NvOdmPmuI2cRead Failed: SlaveNotFound\n");
                break;
        }
        NvOdmOsFree(pReadBuffer);
        NvOdmOsFree(pTransactionInfo);
        return NV_FALSE;
    }

    *Data = (pReadBuffer[0] << 8) | pReadBuffer[1];

    NvOdmOsFree(pReadBuffer);
    NvOdmOsFree(pTransactionInfo);
    return NV_TRUE;
}

static NvBool SetInterfaceActiveState(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsActive)
{
#if 0
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;
    NvU32 ActiveControlReg = hAudioCodec->WCodecRegVals[WCodecRegIndex_POWER_MGMT_5];
    if (IsActive)
    {
        ActiveControlReg = WM8350_SET_REG_VAL(CODEC, ENA, NV_TRUE, ActiveControlReg);
    }
    else
    {
        ActiveControlReg = WM8350_SET_REG_VAL(CODEC, ENA, NV_FALSE, ActiveControlReg);
    }
    return WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_5, ActiveControlReg);
#else
    /*
        Do we need to activate/disable I2C interface?
        We don't need to do this, because WM8350 shares I2C bus with PMU block.
        We should not change I2C interface availity.
    */
    return NV_TRUE;
#endif
    
}

static NvBool 
WM8350_WriteRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32 Data)
{
    NvBool IsSuccess = NV_TRUE;
    NvBool IsActivationControl = NV_FALSE;
    
    // Deactivate if it is the digital path : Programming recomendation
 /*   if ((RegIndex == WCodecRegIndex_DigInterface) || 
            (RegIndex == WCodecRegIndex_SamplingControl) ||
            (RegIndex == WCodecRegIndex_DigitalPath))
    {
        IsActivationControl = NV_TRUE;
    }
*/
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
WM8350_ReadRegister(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    WCodecRegIndex RegIndex, 
    NvU32* Data)
{
    NvBool IsSuccess = NV_TRUE;
    NvBool IsActivationControl = NV_FALSE;
    
    // Deactivate if it is the digital path : Programming recomendation
 /*   if ((RegIndex == WCodecRegIndex_DigInterface) || 
            (RegIndex == WCodecRegIndex_SamplingControl) ||
            (RegIndex == WCodecRegIndex_DigitalPath))
    {
        IsActivationControl = NV_TRUE;
    }
*/
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
    NvU32 DacMuteReg;
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_DAC_MUTE, &DacMuteReg);
    if (!IsSuccess)
        return IsSuccess;

    if (IsMute)
        DacMuteReg = WM8350_SET_REG_VAL(R58_DAC, MUTE, ENABLE, DacMuteReg);
    else
        DacMuteReg = WM8350_SET_REG_VAL(R58_DAC, MUTE, DISABLE, DacMuteReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DAC_MUTE, DacMuteReg);
    if (IsSuccess)
        hAudioCodec->WCodecRegVals[WCodecRegIndex_DAC_MUTE] = DacMuteReg;

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
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT1_VOLUME, &LeftVolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT1_VOLUME, &RightVolReg);
    if (!IsSuccess)
        return IsSuccess;

    LeftVol =  (NvU32)(LeftVolume*64)/100;
    RightVol = (NvU32)(RightVolume*64)/100;
    if (LeftVol >= 64)
        LeftVol = 63;
    if (RightVol >= 64)
        RightVol = 63;
   
    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        LeftVolReg = WM8350_SET_REG_VAL(OUT1L, ZC, DISABLE, LeftVolReg);
        LeftVolReg = WM8350_SET_REG_VAL(R104_OUT1, VU, ENABLE, LeftVolReg);
        LeftVolReg = WM8350_SET_REG_VAL(OUT1L, VOL, LeftVol, LeftVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT1_VOLUME, LeftVolReg);
        if (IsSuccess)
        {
            hAudioCodec->WCodecControl.HPOutVolLeft = LeftVolume;
        }
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        RightVolReg = WM8350_SET_REG_VAL(OUT1R, ZC, DISABLE, RightVolReg);
        RightVolReg = WM8350_SET_REG_VAL(R105_OUT1, VU, ENABLE, RightVolReg);
        RightVolReg = WM8350_SET_REG_VAL(OUT1R, VOL, RightVol, RightVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT1_VOLUME, RightVolReg);
        if (IsSuccess)
        {
            hAudioCodec->WCodecControl.HPOutVolRight = RightVolume;
        }
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
    NvU32 LeftVolReg;
    NvU32 RightVolReg;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT1_VOLUME, &LeftVolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT1_VOLUME, &RightVolReg);
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        if (IsMute)
            LeftVolReg = WM8350_SET_REG_VAL(OUT1L, MUTE, ENABLE, LeftVolReg);
        else
            LeftVolReg = WM8350_SET_REG_VAL(OUT1L, MUTE, DISABLE, LeftVolReg);
            
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT1_VOLUME, LeftVolReg);
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        if (IsMute)
            RightVolReg = WM8350_SET_REG_VAL(OUT1R, MUTE, ENABLE, RightVolReg);
        else
            RightVolReg = WM8350_SET_REG_VAL(OUT1R, MUTE, DISABLE, RightVolReg);
        
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT1_VOLUME, RightVolReg);
    }
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
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, &LeftVolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, &RightVolReg);
    if (!IsSuccess)
        return IsSuccess;

    LeftVol =  (NvU32)(LeftVolume*64)/100;
    RightVol = (NvU32)(RightVolume*64)/100;
    if (LeftVol >= 64)
        LeftVol = 63;
    if (RightVol >= 64)
        RightVol = 63;
    
    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        LeftVolReg = WM8350_SET_REG_VAL(OUT2L, ZC, DISABLE, LeftVolReg);
        LeftVolReg = WM8350_SET_REG_VAL(R106_OUT2, VU, ENABLE, LeftVolReg);
        LeftVolReg = WM8350_SET_REG_VAL(OUT2L, VOL, LeftVol, LeftVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, LeftVolReg);
        if (IsSuccess)
        {
            hAudioCodec->WCodecControl.LineOutVolLeft = LeftVolume;
        }
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        RightVolReg = WM8350_SET_REG_VAL(OUT2R, ZC, DISABLE, RightVolReg);
        RightVolReg = WM8350_SET_REG_VAL(R107_OUT2, VU, ENABLE, RightVolReg);
        RightVolReg = WM8350_SET_REG_VAL(OUT2R, VOL, RightVol, RightVolReg);
        RightVolReg = WM8350_SET_REG_VAL(OUT2R, INV, DISABLE, RightVolReg);
        RightVolReg = WM8350_SET_REG_VAL(OUT2R_INV, MUTE, ENABLE, RightVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, RightVolReg);
        if (IsSuccess)
        {
            hAudioCodec->WCodecControl.LineOutVolRight = RightVolume;
        }
    }
    return IsSuccess;
}

static NvBool 
SetLineOutMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask, 
    NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVolReg;
    NvU32 RightVolReg;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, &LeftVolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, &RightVolReg);
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        if (IsMute)
            LeftVolReg = WM8350_SET_REG_VAL(OUT2L, MUTE, ENABLE, LeftVolReg);
        else
            LeftVolReg = WM8350_SET_REG_VAL(OUT2L, MUTE, DISABLE, LeftVolReg);
            
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, LeftVolReg);
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        if (IsMute)
            RightVolReg = WM8350_SET_REG_VAL(OUT2R, MUTE, ENABLE, RightVolReg);
        else
            RightVolReg = WM8350_SET_REG_VAL(OUT2R, MUTE, DISABLE, RightVolReg);
        
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, RightVolReg);
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
    NvU32 LeftMixVolReg, RightMixVolReg;
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_INPUT_VOLUME, &LeftVolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_INPUT_VOLUME, &RightVolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_L, &LeftMixVolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_R, &RightMixVolReg);
    if (!IsSuccess)
        return IsSuccess;

    // defualt is 010000= 0x10 = 0d16
    // boost mic gain from default value.
    // Gain has 10000 - 111111, 48 level.
    LeftVol = WM8350_INL_VOL_DEFAULT + ((NvU32)(LeftVolume*48)/100);
    RightVol = WM8350_INL_VOL_DEFAULT + ((NvU32)(RightVolume*48)/100);
    if (LeftVol >= 64)
        LeftVol = 63;
    if (RightVol >= 64)
        RightVol = 63;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        LeftVolReg = WM8350_SET_REG_VAL(INL, ZC, DISABLE, LeftVolReg);
        LeftVolReg = WM8350_SET_REG_VAL(IN,  VU, ENABLE, LeftVolReg);
        LeftVolReg = WM8350_SET_REG_VAL(INL, VOL, LeftVol, LeftVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_INPUT_VOLUME, LeftVolReg);
        LeftMixVolReg = WM8350_SET_REG_DEF(IN2L_MIXINL, VOL, DEFAULT, LeftMixVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_L, LeftMixVolReg);

        if (IsSuccess)
        {
            hAudioCodec->WCodecControl.LineInVolLeft = LeftVolume;
        }
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        RightVolReg = WM8350_SET_REG_VAL(INR, ZC, DISABLE, RightVolReg);
        RightVolReg = WM8350_SET_REG_VAL(IN, VU, ENABLE, RightVolReg);
        RightVolReg = WM8350_SET_REG_VAL(INR, VOL, RightVol, RightVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_INPUT_VOLUME, RightVolReg);
        RightMixVolReg = WM8350_SET_REG_DEF(IN2R_MIXINR, VOL, DEFAULT, RightMixVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_R, RightMixVolReg);

        if (IsSuccess)
        {
            hAudioCodec->WCodecControl.LineInVolRight = RightVolume;
        }
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
    NvU32 InCtlReg;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, &InCtlReg);
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        if (IsMute)
            InCtlReg = WM8350_SET_REG_VAL(IN2L, ENA, DISABLE, InCtlReg);
        else
            InCtlReg = WM8350_SET_REG_VAL(IN2L, ENA, ENABLE, InCtlReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, InCtlReg);
    }
    if (!IsSuccess)
        return IsSuccess;

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        if (IsMute)
            InCtlReg = WM8350_SET_REG_VAL(IN2R, ENA, DISABLE, InCtlReg);
        else
            InCtlReg = WM8350_SET_REG_VAL(IN2R, ENA, ENABLE, InCtlReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, InCtlReg);
    }    
    return IsSuccess;
}

static NvBool SetMicrophoneInVolume(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvU32 MicGain)
{
    NvBool IsSuccess = NV_TRUE;    
    NvU32 LeftInVol, LeftMixVol;
    NvU32 MicVol = 0;
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_INPUT_VOLUME, &LeftInVol);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_L, &LeftMixVol);
    if (!IsSuccess)
        return IsSuccess;

    // defualt is 010000= 0x10 = 0d16
    // boost mic gain from default value.
    // Gain has 10000 - 111111, 48 level.
    MicVol = WM8350_INL_VOL_DEFAULT + ((NvU32)(MicGain*48)/100);
    hAudioCodec->WCodecControl.MICINVol = MicGain;
    if (MicVol >= 64)
        MicVol = 63;

    LeftInVol = WM8350_SET_REG_VAL(INL, ZC, DISABLE, LeftInVol);
    LeftInVol = WM8350_SET_REG_VAL(IN,  VU, ENABLE, LeftInVol);
    LeftInVol = WM8350_SET_REG_VAL(INL, VOL, MicVol, LeftInVol);
    
    // Boost gain to +20db
//    LeftMixVol = WM8350_SET_REG_VAL(INL_MIXINL, VOL, ENABLE, LeftMixVol);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_INPUT_VOLUME, LeftInVol);    
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_L, LeftMixVol);    

    return IsSuccess;
}

static NvBool SetMicrophoneInMute(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;    
    NvU32 LeftInVol;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, &LeftInVol);
    if (!IsSuccess)
        return IsSuccess;

    if (IsMute)
        LeftInVol = WM8350_SET_REG_VAL(IN1LN, ENA, DISABLE, LeftInVol);
    else
        LeftInVol = WM8350_SET_REG_VAL(IN1LN, ENA, ENABLE, LeftInVol);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, LeftInVol);    
    
    return IsSuccess;
}
static NvBool 
SetAUXInVolume(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec,
    NvU32 ChannelMask, 
    NvU32 LeftVolume,
    NvU32 RightVolume)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVolReg, LeftGain;
    NvU32 RightVolReg, RightGain;
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_L, &LeftVolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_R, &RightVolReg);
    if (!IsSuccess)
        return IsSuccess;

    
    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        hAudioCodec->WCodecControl.AUXInVolLeft = LeftVolume;
        // Gain from 0b000 to 0b111
        LeftGain = (LeftVolume*8) /100;
        if (LeftGain >= 8)
            LeftGain = 7;
        LeftVolReg = WM8350_SET_REG_VAL(IN3L_MIXINL, VOL, LeftGain, LeftVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_L, LeftVolReg);
        if (!IsSuccess)
            return IsSuccess;
    }

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        hAudioCodec->WCodecControl.AUXInVolRight = RightVolume;
        // Gain from 0b000 to 0b111
        RightGain = (RightVolume*8) /100;
        if (RightGain >= 8)
            RightGain = 7;
        RightVolReg = WM8350_SET_REG_VAL(IN3R_MIXINR, VOL, RightGain, RightVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_R, RightVolReg);
        if (!IsSuccess)
            return IsSuccess;
    }
    return IsSuccess;
}

static NvBool 
SetAUXInMute(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvU32  ChannelMask, 
    NvBool IsMute)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 LeftVolReg, LeftGain;
    NvU32 RightVolReg, RightGain;
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_L, &LeftVolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_MIXER_VOLUME_R, &RightVolReg);
    if (!IsSuccess)
        return IsSuccess;

    
    if (ChannelMask & NvOdmAudioSpeakerType_FrontLeft)
    {
        // Gain 0 is MUTE
        if (IsMute)
            LeftVolReg = WM8350_SET_REG_VAL(IN3L_MIXINL, VOL, 0, LeftVolReg);
        else
        {
            // Gain from 0b000 to 0b111
            LeftGain = (hAudioCodec->WCodecControl.AUXInVolLeft * 8)/ 100;
            if (LeftGain >= 8)
                LeftGain = 7;            
            LeftVolReg = WM8350_SET_REG_VAL(IN3L_MIXINL, VOL, LeftGain, LeftVolReg);
        }
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_OUTPUT_LEFT_MIXER_VOLUME, LeftVolReg);
    }

    if (ChannelMask & NvOdmAudioSpeakerType_FrontRight)
    {
        // Gain 0 is MUTE
        if (IsMute)
            RightVolReg = WM8350_SET_REG_VAL(IN3R_MIXINR, VOL, 0, RightVolReg);
        else
        {
            // Gain from 0b000 to 0b111
            RightGain = (hAudioCodec->WCodecControl.AUXInVolRight * 8)/ 100;
            if (RightGain >= 8)
                RightGain = 7;
            RightVolReg = WM8350_SET_REG_VAL(IN3R_MIXINR, VOL, RightGain, RightVolReg);
        }
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_OUTPUT_RIGHT_MIXER_VOLUME, RightVolReg);
    }
    return IsSuccess;
}



/*
 * Shuts down the audio codec driver.
 */
static NvBool ShutDownCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_FALSE;
    NvU32 PowerControl;
    NvU32 SysClkControl;
    NvU32 BiasControl;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_5, &PowerControl);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_4, &SysClkControl);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_1, &BiasControl);
    if (!IsSuccess)
        return NV_FALSE;
    
    PowerControl = WM8350_SET_REG_VAL(CODEC, ENA, DISABLE, PowerControl);
    SysClkControl = WM8350_SET_REG_VAL(SYSCLK, ENA, DISABLE, SysClkControl);
    BiasControl = WM8350_SET_REG_VAL(BIAS, ENA, DISABLE, BiasControl);
    BiasControl = WM8350_SET_REG_DEF(CODEC, ISEL, X1_0, BiasControl);
    BiasControl = WM8350_SET_REG_VAL(VBUF, ENA, DISABLE, BiasControl);
    BiasControl = WM8350_SET_REG_VAL(VMID, ENA, DISABLE, BiasControl);
    BiasControl = WM8350_SET_REG_DEF(VMID, SEL, OFF, BiasControl);

    
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_5, PowerControl);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_4, SysClkControl);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_1, BiasControl);
    if (IsSuccess)
        NvOdmOsSleepMS(500);

    return IsSuccess;
}

static NvBool ResetCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 PowerControl;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_5, &PowerControl);

    PowerControl = WM8350_SET_REG_VAL(CODEC, ENA, DISABLE, PowerControl);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_5, PowerControl);
    PowerControl = WM8350_SET_REG_VAL(CODEC, ENA, ENABLE, PowerControl);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_5, PowerControl);

    return IsSuccess;
}

#define NVODM_CODEC_W8350_MAX_CLOCKS 3

static NvBool SetPowerOnCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsEnable)
{
#if WM8350_POWER_CONTROL_ENABLE
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;
    const NvOdmPeripheralConnectivity *pPerConnectivity = NULL;
    NvU32 Index;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime;
    NvU32 ClockInstances[NVODM_CODEC_W8350_MAX_CLOCKS];
    NvU32 ClockFrequencies[NVODM_CODEC_W8350_MAX_CLOCKS];
    NvU32 NumClocks;

    // Get the peripheral connectivity information
    pPerConnectivity = NvOdmPeripheralGetGuid(WOLFSON_8350_CODEC_GUID);    
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
                WOLFSON_8350_CODEC_GUID, NV_FALSE,
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
#if WM8350_AUDIOCODEC_DEBUG_PRINT
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
                    WOLFSON_8350_CODEC_GUID, NV_TRUE,
                    ClockInstances, ClockFrequencies, &NumClocks))
                return NV_FALSE;

#if WM8350_AUDIOCODEC_DEBUG_PRINT
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
    NvU32 AntiPopCtl;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ANTI_POP_CONTROL, &AntiPopCtl);
    if (!IsSuccess)
        return NV_FALSE;
    if (IsAntiPop)
    {
        AntiPopCtl = WM8350_SET_REG_VAL(ANTI, POP, 0x01, AntiPopCtl);
        AntiPopCtl = WM8350_SET_REG_VAL(DIS_OP, OUT1, 0x01, AntiPopCtl);
        AntiPopCtl = WM8350_SET_REG_VAL(DIS_OP, OUT2, 0x01, AntiPopCtl);
    }
    else
    {
        AntiPopCtl = WM8350_SET_REG_VAL(ANTI, POP, 0x00, AntiPopCtl);
        AntiPopCtl = WM8350_SET_REG_VAL(DIS_OP, OUT1, 0x00, AntiPopCtl);
        AntiPopCtl = WM8350_SET_REG_VAL(DIS_OP, OUT2, 0x00, AntiPopCtl);
    }
        
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ANTI_POP_CONTROL, AntiPopCtl);
    if (IsSuccess)
        NvOdmOsSleepMS(250);
    
    return IsSuccess;
}    
static NvBool SetCodecPower(NvOdmPrivAudioCodecHandle hOdmAudioCodec, NvBool IsPowerOn)
{
    NvBool IsSuccess = NV_FALSE;
    NvU32 PowerControl;
    NvU32 SysClkControl;
    NvU32 BiasControl;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_5, &PowerControl);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_4, &SysClkControl);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_1, &BiasControl);
    if (!IsSuccess)
        return NV_FALSE;
    
    if (IsPowerOn)
    {
        PowerControl = WM8350_SET_REG_VAL(CODEC, ENA, ENABLE, PowerControl);
        SysClkControl = WM8350_SET_REG_VAL(SYSCLK, ENA, ENABLE, SysClkControl);
        BiasControl = WM8350_SET_REG_VAL(BIAS, ENA, ENABLE, BiasControl);
        BiasControl = WM8350_SET_REG_DEF(CODEC, ISEL, X0_75, BiasControl);
        BiasControl = WM8350_SET_REG_VAL(VBUF, ENA, ENABLE, BiasControl);
        BiasControl = WM8350_SET_REG_VAL(VMID, ENA, ENABLE, BiasControl);
        BiasControl = WM8350_SET_REG_DEF(VMID, SEL, R5K, BiasControl);
    }
    else
    {
        PowerControl = WM8350_SET_REG_VAL(CODEC, ENA, DISABLE, PowerControl);
        SysClkControl = WM8350_SET_REG_VAL(SYSCLK, ENA, DISABLE, SysClkControl);
        BiasControl = WM8350_SET_REG_VAL(BIAS, ENA, DISABLE, BiasControl);
        BiasControl = WM8350_SET_REG_DEF(CODEC, ISEL, X0_75, BiasControl);
        BiasControl = WM8350_SET_REG_VAL(VBUF, ENA, DISABLE, BiasControl);
        BiasControl = WM8350_SET_REG_VAL(VMID, ENA, DISABLE, BiasControl);
        BiasControl = WM8350_SET_REG_DEF(VMID, SEL, OFF, BiasControl);
    }

    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_5, PowerControl);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_4, SysClkControl);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_1, BiasControl);
    if (IsSuccess)
        NvOdmOsSleepMS(500);
    
    return IsSuccess;
}

#if 0
#define WM8350_CHIP_ID 0x3000
static NvBool CheckCodecRevision(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_FALSE;
    NvU32 ChipID = 0;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ID, &ChipID);
    if (ChipID == WM8350_CHIP_ID)    
    {
        IsSuccess = NV_TRUE;
    }
    else
    {
        IsSuccess = NV_FALSE;
    }
    
    return IsSuccess;
}
#endif

static NvBool InitializeDigitalInterface(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess;
    NvU32 DigIntReg = 0;
    NvU32 AIDACReg = 0;
    NvU32 DACCtlReg = 0;
    NvU32 ClkCtl1Reg = 0, ClkCtl2Reg = 0, DACLRRateReg = 0, ADCLRRateReg = 0;
#if 0
    NvU32 IntCtlReg = 0;
#endif

    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;
    
    // Initializing the configuration parameters.

    // Is USB clock mode?
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_DAC_CONTROL, &DACCtlReg);
    if (!IsSuccess)
        return IsSuccess;
    if (hAudioCodec->WCodecInterface.IsUsbMode)
        DACCtlReg = WM8350_SET_REG_VAL(AIF, LRCLKRATE, ENABLE, DACCtlReg);
    else
        DACCtlReg = WM8350_SET_REG_VAL(AIF, LRCLKRATE, DISABLE, DACCtlReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DAC_CONTROL, DACCtlReg);
    if (!IsSuccess)
        return IsSuccess;
    
    // Set I2S data format.
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_AI_FORMATING, &DigIntReg);
    if (!IsSuccess)
        return IsSuccess;

    switch (hAudioCodec->WCodecInterface.I2sCodecDataCommFormat)
    {
        case NvOdmQueryI2sDataCommFormat_Dsp:
            DigIntReg = WM8350_SET_REG_DEF(AIF, FMT, DSP, DigIntReg);
            break;

        case NvOdmQueryI2sDataCommFormat_I2S:
            DigIntReg = WM8350_SET_REG_DEF(AIF, FMT, I2S, DigIntReg);
            break;

        case NvOdmQueryI2sDataCommFormat_LeftJustified:
            DigIntReg = WM8350_SET_REG_DEF(AIF, FMT, LEFTJUSTIFIED, DigIntReg);
            break;
            
        case NvOdmQueryI2sDataCommFormat_RightJustified:
            DigIntReg = WM8350_SET_REG_DEF(AIF, FMT, RIGHTJUSTIFIED, DigIntReg);
            break;
            
        default:
            // Default will be the i2s mode
            DigIntReg = WM8350_SET_REG_DEF(AIF, FMT, I2S, DigIntReg);
            break;
    }

/*    if (hAudioCodec->WCodecInterface.I2sCodecLRLineControl == NvOdmQueryI2sLRLineControl_LeftOnLow)
//J        DigIntReg = WM8350_SET_REG_DEF(DIGITAL_INTERFACE, LRP, LEFT_DACLRC_LOW, DigIntReg);
    else
//J        DigIntReg = WM8350_SET_REG_DEF(DIGITAL_INTERFACE, LRP, LEFT_DACLRC_HIGH, DigIntReg);
*/

    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AI_FORMATING, DigIntReg);
    if (!IsSuccess)
        return IsSuccess;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_CLOCK_CONTROL_1, &ClkCtl1Reg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_CLOCK_CONTROL_2, &ClkCtl2Reg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_DAC_LR_RATE, &DACLRRateReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ADC_LR_RATE, &ADCLRRateReg);
    if (!IsSuccess)
        return IsSuccess;
    ClkCtl1Reg = WM8350_SET_REG_DEF(BCLK, DIV, DEFAULT, ClkCtl1Reg);
    ClkCtl2Reg = WM8350_SET_REG_VAL(LRC_ADC, SEL, DISABLE, ClkCtl2Reg);
    DACLRRateReg = WM8350_SET_REG_VAL(DACLRC, ENA, DISABLE, DACLRRateReg);
    DACLRRateReg = WM8350_SET_REG_DEF(DACLRC, RATE, DEFAULT, DACLRRateReg);
    ADCLRRateReg = WM8350_SET_REG_VAL(ADCLRC, ENA, DISABLE, ADCLRRateReg);
    ADCLRRateReg = WM8350_SET_REG_DEF(ADCLRC, RATE, DEFAULT, ADCLRRateReg);
    
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_CLOCK_CONTROL_1, ClkCtl1Reg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_CLOCK_CONTROL_2, ClkCtl2Reg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DAC_LR_RATE, DACLRRateReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ADC_LR_RATE, ADCLRRateReg);

    hAudioCodec->WCodecRegVals[WCodecRegIndex_CLOCK_CONTROL_1] = ClkCtl1Reg;
    hAudioCodec->WCodecRegVals[WCodecRegIndex_CLOCK_CONTROL_2] = ClkCtl2Reg;
    hAudioCodec->WCodecRegVals[WCodecRegIndex_DAC_LR_RATE] = DACLRRateReg;
    hAudioCodec->WCodecRegVals[WCodecRegIndex_ADC_LR_RATE] = ADCLRRateReg;
   
#if 0
    // this is only for development mode.
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INTERFACE_CONTROL , &IntCtlReg);
    IntCtlReg = WM8350_SET_REG_VAL(CONFIG, DONE, ENABLE, IntCtlReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INTERFACE_CONTROL, IntCtlReg);
#endif

    // Is Codec master mode?
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_AI_DAC_CONTROL, &AIDACReg);
    if (!IsSuccess)
        return IsSuccess;
    if (hAudioCodec->WCodecInterface.IsCodecMasterMode)
        AIDACReg = WM8350_SET_REG_VAL(BCLK, MSTR, ENABLE, AIDACReg);
    else
        AIDACReg = WM8350_SET_REG_VAL(BCLK, MSTR, DISABLE, AIDACReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AI_DAC_CONTROL, AIDACReg);
    hAudioCodec->WCodecRegVals[WCodecRegIndex_AI_DAC_CONTROL] = AIDACReg;
    
    return IsSuccess;
}

static NvBool InitializeHeadphoneOut(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{

    return SetHeadphoneOutVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                90, 90);
}

#if LINEOUT_ENABLED
static NvBool InitializeLineOut(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{

    return SetLineOutVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                90, 90);
}
#endif

#if ALL_INPUT_ENABLED
static NvBool InitializeLineInput(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 InCtlReg;
    
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, &InCtlReg);
    if (!IsSuccess)
        return IsSuccess;


    InCtlReg = WM8350_SET_REG_VAL(IN2L, ENA, ENABLE, InCtlReg);
    InCtlReg = WM8350_SET_REG_VAL(IN2R, ENA, ENABLE, InCtlReg);

    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, InCtlReg);

    return SetLineInVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                0, 0);
}
static NvBool InitializeAuxInput(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 IN3CtlReg;
    
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_IN3_INPUT_CONTROL, 
        &IN3CtlReg);
    if (!IsSuccess)
        return IsSuccess;

    //port enable.
    IN3CtlReg = WM8350_SET_REG_VAL(R73_IN3L, ENA, ENABLE, IN3CtlReg);
    IN3CtlReg = WM8350_SET_REG_VAL(R73_IN3R, ENA, ENABLE, IN3CtlReg);
//    IN3CtlReg = WM8350_SET_REG_VAL(IN3L, SHORT, ENABLE, IN3CtlReg);
//    IN3CtlReg = WM8350_SET_REG_VAL(IN3R, SHORT, ENABLE, IN3CtlReg);

    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_IN3_INPUT_CONTROL, 
        IN3CtlReg);
    if (!IsSuccess)
        return IsSuccess;
    
    return SetAUXInVolume(hOdmAudioCodec, 
                (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
                70, 70);
}
#endif

static NvBool InitializeMICInput(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 INCtlReg;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, 
        &INCtlReg);
    if (!IsSuccess)
        return IsSuccess;

    INCtlReg = WM8350_SET_REG_VAL(IN1LN, ENA, ENABLE, INCtlReg);

    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, 
        INCtlReg);

    return SetMicrophoneInVolume(hOdmAudioCodec, 
                0);
}
static NvBool InitializeAnalogAudioPath(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 DACDigVolLReg, DACDigVolRReg, LeftMIXCtlReg, RightMIXCtlReg;

    //audio path selection.
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_DAC_DIGITAL_VOLUME_L, &DACDigVolLReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_DAC_DIGITAL_VOLUME_R, &DACDigVolRReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_MIXER_CONTROL, &LeftMIXCtlReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_MIXER_CONTROL, &RightMIXCtlReg);
    if (!IsSuccess)
        return IsSuccess;
    DACDigVolLReg = WM8350_SET_REG_VAL(R50_DACL, ENA, ENABLE, DACDigVolLReg);
    DACDigVolLReg = WM8350_SET_REG_VAL(DACL, VU, ENABLE, DACDigVolLReg);
    DACDigVolLReg = WM8350_SET_REG_DEF(DACL, VOL, DEFAULT, DACDigVolLReg);
    DACDigVolRReg = WM8350_SET_REG_VAL(R51_DACR, ENA, ENABLE, DACDigVolRReg);
    DACDigVolRReg = WM8350_SET_REG_VAL(DACR, VU, ENABLE, DACDigVolRReg);
    DACDigVolRReg = WM8350_SET_REG_DEF(DACR, VOL, DEFAULT, DACDigVolRReg);

    LeftMIXCtlReg = WM8350_SET_REG_VAL(R88_MIXOUTL, ENA, ENABLE, LeftMIXCtlReg);
    LeftMIXCtlReg = WM8350_SET_REG_VAL(DACL_TO, MIXOUTL, ENABLE, LeftMIXCtlReg);
    RightMIXCtlReg = WM8350_SET_REG_VAL(R89_MIXOUTR, ENA, ENABLE, RightMIXCtlReg);
    RightMIXCtlReg = WM8350_SET_REG_VAL(DACR_TO, MIXOUTR, ENABLE, RightMIXCtlReg);

  
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DAC_DIGITAL_VOLUME_L, DACDigVolLReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DAC_DIGITAL_VOLUME_R, DACDigVolRReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_MIXER_CONTROL, LeftMIXCtlReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_MIXER_CONTROL, RightMIXCtlReg);

    
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
    NvU32 AIFMTReg;
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_AI_FORMATING, &AIFMTReg);
    if (!IsSuccess)
        return IsSuccess;
    
    switch (PcmSize)
    {
        case 16:
            AIFMTReg = WM8350_SET_REG_DEF(AIF, WL, W16BIT,
                                        AIFMTReg);
            break;
            
        case 20:
            AIFMTReg = WM8350_SET_REG_DEF(AIF, WL, W20BIT,
                                        AIFMTReg);
            break;
            
        case 24:
            AIFMTReg = WM8350_SET_REG_DEF(AIF, WL, W24BIT,
                                        AIFMTReg);
            break;
            
        case 32:
            AIFMTReg = WM8350_SET_REG_DEF(AIF, WL, W32BIT,
                                        AIFMTReg);
            break;
            
        default:
            return NV_FALSE;
    }
//    NvOdmOsDebugPrintf("The new Pcm control Reg = 0x%08x \n",DigitalPathReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_AI_FORMATING, AIFMTReg);
    if (IsSuccess)
        hAudioCodec->WCodecRegVals[WCodecRegIndex_AI_FORMATING] = AIFMTReg;
    
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
    NvU32 ADCClkCtlReg, DACClkCtlReg, DACCtlReg;
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;
   
    if (!hAudioCodec->WCodecInterface.IsCodecMasterMode)
    {
        hAudioCodec->WCodecControl.AdcSamplingRate = SamplingRate;
        hAudioCodec->WCodecControl.DacSamplingRate = SamplingRate;
        return NV_TRUE;
    }

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIVIDER, &ADCClkCtlReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_DAC_CLOCK_CONTROL, &DACClkCtlReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_DAC_CONTROL, &DACCtlReg);
    if (!IsSuccess)
        return IsSuccess;
    if (hAudioCodec->WCodecInterface.IsUsbMode)
    {
        DACCtlReg = WM8350_SET_REG_VAL(AIF, LRCLKRATE, ENABLE,
                                        DACCtlReg);    
    }
    else
    {
        DACCtlReg = WM8350_SET_REG_VAL(AIF, LRCLKRATE, DISABLE,
                                        DACCtlReg);            
    }
    
    switch (SamplingRate)
    {
        case 8000:
            ADCClkCtlReg = WM8350_SET_REG_DEF(ADC, CLKDIV, 
                                            SR8018HZ, ADCClkCtlReg);
            DACClkCtlReg = WM8350_SET_REG_DEF(DAC, CLKDIV,
                                    SR8018HZ, DACClkCtlReg);
            break;
            
        case 11025:
            ADCClkCtlReg = WM8350_SET_REG_DEF(ADC, CLKDIV, 
                                            SR11025HZ, ADCClkCtlReg);
            DACClkCtlReg = WM8350_SET_REG_DEF(DAC, CLKDIV,
                                    SR11025HZ, DACClkCtlReg);
            break;
        case 22050:
            ADCClkCtlReg = WM8350_SET_REG_DEF(ADC, CLKDIV, 
                                            SR22050HZ, ADCClkCtlReg);
            DACClkCtlReg = WM8350_SET_REG_DEF(DAC, CLKDIV,
                                    SR22050HZ, DACClkCtlReg);
            break;
            
        case 44100:
            ADCClkCtlReg = WM8350_SET_REG_DEF(ADC, CLKDIV, 
                                          SR44100HZ, ADCClkCtlReg);
            DACClkCtlReg = WM8350_SET_REG_DEF(DAC, CLKDIV,
                                    SR44100HZ, DACClkCtlReg);
            break;
            
            
        default:
            return NV_FALSE;
    }

    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIVIDER, ADCClkCtlReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DAC_CLOCK_CONTROL, DACClkCtlReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_DAC_CONTROL, DACCtlReg);
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
    NvU32 INCtlReg;
    NvU32 IN3CtlReg;

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

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, 
        &INCtlReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_IN3_INPUT_CONTROL, 
        &IN3CtlReg);
    if (!IsSuccess)
        return IsSuccess;

    if (pConfigIO->InSignalType == NvOdmAudioSignalType_Aux)
    {
        //port enable.
        IN3CtlReg = WM8350_SET_REG_VAL(R73_IN3L, ENA, ENABLE, IN3CtlReg);
        IN3CtlReg = WM8350_SET_REG_VAL(R73_IN3R, ENA, ENABLE, IN3CtlReg);
//        IN3CtlReg = WM8350_SET_REG_VAL(IN3L, SHORT, ENABLE, IN3CtlReg);
//        IN3CtlReg = WM8350_SET_REG_VAL(IN3R, SHORT, ENABLE, IN3CtlReg);
    }
    else if (pConfigIO->InSignalType == NvOdmAudioSignalType_LineIn)
    {
        //port enable.
        INCtlReg = WM8350_SET_REG_VAL(IN2L, ENA, ENABLE, INCtlReg);
        INCtlReg = WM8350_SET_REG_VAL(IN2R, ENA, ENABLE, INCtlReg);
    }
    else if (pConfigIO->InSignalType == NvOdmAudioSignalType_MicIn)
    {
        //port enable.
        INCtlReg = WM8350_SET_REG_VAL(IN1LN, ENA, ENABLE, INCtlReg);
    }
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_INPUT_CONTROL, 
        INCtlReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_IN3_INPUT_CONTROL, 
        IN3CtlReg);
    
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

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT1_VOLUME, 
        &LOut1VolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT1_VOLUME, 
        &ROut1VolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, 
        &LOut2VolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, 
        &ROut2VolReg);
    if (!IsSuccess)
        return IsSuccess;
    if (pConfigIO->InSignalType == NvOdmAudioSignalType_HeadphoneOut)
    {
        LOut1VolReg = WM8350_SET_REG_VAL(R104_OUT1L, ENA, ENABLE, LOut1VolReg); 
        ROut1VolReg = WM8350_SET_REG_VAL(R105_OUT1R, ENA, ENABLE, ROut1VolReg); 
    }
    else if (pConfigIO->InSignalType == NvOdmAudioSignalType_LineOut)
    {
        LOut2VolReg = WM8350_SET_REG_VAL(R106_OUT2L, ENA, ENABLE, LOut2VolReg); 
        ROut2VolReg = WM8350_SET_REG_VAL(R107_OUT2R, ENA, ENABLE, ROut2VolReg); 
    }
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT1_VOLUME, 
        LOut1VolReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT1_VOLUME, 
        ROut1VolReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, 
        LOut2VolReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, 
        ROut2VolReg);
    
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
    NvU32 LMIXCtlReg;
    NvU32 RMIXCtlReg;
    NvU32 LMIXVolReg;
    NvU32 RMIXVolReg;

    if ((InputLineId != 0) || (OutputLineId != 0))
        return NV_FALSE;

    if (OutputLineId != 0)
        return NV_FALSE;

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_MIXER_CONTROL, 
        &LMIXCtlReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_MIXER_CONTROL, 
        &RMIXCtlReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_OUTPUT_LEFT_MIXER_VOLUME, 
        &LMIXVolReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_OUTPUT_RIGHT_MIXER_VOLUME, 
        &RMIXVolReg);
    if (!IsSuccess)
        return IsSuccess;


    if (InputAnalogLineType & NvOdmAudioSignalType_LineIn)
    {
        if (IsEnable)
        {
            LMIXCtlReg = WM8350_SET_REG_VAL(INL_TO, MIXOUTL, ENABLE, LMIXCtlReg);
            RMIXCtlReg = WM8350_SET_REG_VAL(INR_TO, MIXOUTR, ENABLE, RMIXCtlReg);
            LMIXVolReg = WM8350_SET_REG_DEF(INL_MIXOUTL, VOL, DEFAULT, LMIXVolReg);
            RMIXVolReg = WM8350_SET_REG_DEF(INR_MIXOUTR, VOL, DEFAULT, RMIXVolReg);
        }
        else
        {
            LMIXCtlReg = WM8350_SET_REG_VAL(INL_TO, MIXOUTL, DISABLE, LMIXCtlReg);
            RMIXCtlReg = WM8350_SET_REG_VAL(INR_TO, MIXOUTR, DISABLE, RMIXCtlReg);
        }
    }
    
    if (InputAnalogLineType & NvOdmAudioSignalType_MicIn)
    {
        if (IsEnable)
        {
            LMIXCtlReg = WM8350_SET_REG_VAL(INL_TO, MIXOUTL, ENABLE, LMIXCtlReg);
            LMIXVolReg = WM8350_SET_REG_DEF(INL_MIXOUTL, VOL, DEFAULT, LMIXVolReg);
        }
        else
        {
            LMIXCtlReg = WM8350_SET_REG_VAL(INL_TO, MIXOUTL, DISABLE, LMIXCtlReg);
        }
    }
    if (InputAnalogLineType & NvOdmAudioSignalType_Aux)
    {
        if (IsEnable)
        {
            LMIXCtlReg = WM8350_SET_REG_VAL(IN3L_TO, MIXOUTL, ENABLE, LMIXCtlReg);
            RMIXCtlReg = WM8350_SET_REG_VAL(IN3R_TO, MIXOUTR, ENABLE, RMIXCtlReg);
            LMIXVolReg = WM8350_SET_REG_DEF(IN3L_MIXOUTL, VOL, DEFAULT, LMIXVolReg);
            RMIXVolReg = WM8350_SET_REG_DEF(IN3R_MIXOUTR, VOL, DEFAULT, RMIXVolReg);
      }
        else
        {
            LMIXCtlReg = WM8350_SET_REG_VAL(IN3L_TO, MIXOUTL, DISABLE, LMIXCtlReg);
            RMIXCtlReg = WM8350_SET_REG_VAL(IN3R_TO, MIXOUTR, DISABLE, RMIXCtlReg);
        }
    }
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_MIXER_CONTROL, 
        LMIXCtlReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_MIXER_CONTROL, 
        RMIXCtlReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_OUTPUT_LEFT_MIXER_VOLUME, 
        LMIXVolReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_OUTPUT_RIGHT_MIXER_VOLUME, 
        RMIXVolReg);

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
    NvU32 PowerControlInReg;
    NvU32 PowerControlOutReg;
    NvU32 PowerADCDACReg;
    NvU32 LeftInVolReg, RightInVolReg;
    NvU32 LOut1VolReg, ROut1VolReg, LOut2VolReg, ROut2VolReg;
    NvU32 ADCVolLReg, ADCVolRReg;
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;
    

    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_2, 
        &PowerControlInReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_3, 
        &PowerControlOutReg);
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_4, 
        &PowerADCDACReg);
    if (!IsSuccess)
        return IsSuccess;

    if (SignalType & NvOdmAudioSignalType_LineIn)
    {
        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.LineInPower = NV_TRUE;
            PowerControlInReg = WM8350_SET_REG_VAL(R9_INL, ENA, ENABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8350_SET_REG_VAL(R9_INR, ENA, ENABLE, 
                                         PowerControlInReg);
        }
        else
        {
            hAudioCodec->WCodecControl.LineInPower = NV_FALSE;
            if (!hAudioCodec->WCodecControl.LineInPower && 
                !hAudioCodec->WCodecControl.MICPower)
            {
                PowerControlInReg = WM8350_SET_REG_VAL(R9_INL, ENA, DISABLE, 
                                             PowerControlInReg);
                PowerControlInReg = WM8350_SET_REG_VAL(R9_INR, ENA, DISABLE, 
                                             PowerControlInReg);
            }
        }
    }
    
    if ((SignalType & NvOdmAudioSignalType_MicIn))
    {
        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.MICPower = NV_TRUE;
            PowerControlInReg = WM8350_SET_REG_VAL(R9_INL, ENA, ENABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8350_SET_REG_VAL(R9_INR, ENA, ENABLE, 
                                         PowerControlInReg);
        }
        else
        {
            hAudioCodec->WCodecControl.MICPower = NV_FALSE;
            if (!hAudioCodec->WCodecControl.LineInPower && 
                !hAudioCodec->WCodecControl.MICPower)
            {
                PowerControlInReg = WM8350_SET_REG_VAL(R9_INL, ENA, DISABLE, 
                                             PowerControlInReg);
                PowerControlInReg = WM8350_SET_REG_VAL(R9_INR, ENA, DISABLE, 
                                             PowerControlInReg);
            }
        }
    }
    
    if (SignalType & NvOdmAudioSignalType_Aux)
    {
        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.AUXPower= NV_TRUE;
            PowerControlInReg = WM8350_SET_REG_VAL(R9_IN3L, ENA, ENABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8350_SET_REG_VAL(R9_IN3R, ENA, ENABLE, 
                                         PowerControlInReg);
        }
        else
        {
            hAudioCodec->WCodecControl.AUXPower= NV_FALSE;
            PowerControlInReg = WM8350_SET_REG_VAL(R9_IN3L, ENA, DISABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8350_SET_REG_VAL(R9_IN3R, ENA, DISABLE, 
                                         PowerControlInReg);
        }
    }    
    if ((SignalType & NvOdmAudioSignalType_Aux) ||
        (SignalType & NvOdmAudioSignalType_MicIn) ||
        (SignalType & NvOdmAudioSignalType_LineIn))
    {
        IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_INPUT_VOLUME, 
            &LeftInVolReg);
        IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_INPUT_VOLUME, 
            &RightInVolReg);
        IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIGITAL_VOLUME_L, 
            &ADCVolLReg);
        IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIGITAL_VOLUME_R, 
            &ADCVolRReg);
        if (!IsSuccess)
            return IsSuccess;
    
        if (hAudioCodec->WCodecControl.AUXPower || 
            hAudioCodec->WCodecControl.MICPower ||
            hAudioCodec->WCodecControl.LineInPower)
        {
            //Input Mixer power on
            PowerControlInReg = WM8350_SET_REG_VAL(MIXINL, ENA, ENABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8350_SET_REG_VAL(MIXINR, ENA, ENABLE, 
                                         PowerControlInReg);
            //ADC power on
            PowerADCDACReg = WM8350_SET_REG_VAL(R11_ADCL, ENA, ENABLE, 
                                         PowerADCDACReg);
            PowerADCDACReg = WM8350_SET_REG_VAL(R11_ADCR, ENA, ENABLE, 
                                         PowerADCDACReg);
            //Input for Mic/Line-In power on
            LeftInVolReg = WM8350_SET_REG_VAL(R80_INL, ENA, ENABLE, 
                                         LeftInVolReg);
            RightInVolReg = WM8350_SET_REG_VAL(R81_INR, ENA, ENABLE, 
                                         RightInVolReg);
            //ADC enable.
            ADCVolLReg = WM8350_SET_REG_VAL(R66_ADCL, ENA, ENABLE, 
                                         ADCVolLReg);
            ADCVolLReg = WM8350_SET_REG_VAL(R66_ADC, VU, ENABLE, 
                                         ADCVolLReg);
            ADCVolLReg = WM8350_SET_REG_DEF(ADCL, VOL, DEFAULT, 
                                         ADCVolLReg);
            ADCVolRReg = WM8350_SET_REG_VAL(R67_ADCR, ENA, ENABLE, 
                                         ADCVolRReg);
            ADCVolRReg = WM8350_SET_REG_VAL(R67_ADC, VU, ENABLE, 
                                         ADCVolRReg);
            ADCVolRReg = WM8350_SET_REG_DEF(ADCR, VOL, DEFAULT, 
                                         ADCVolRReg);
            
        }
        else
        {
            //Input Mixer power off
            PowerControlInReg = WM8350_SET_REG_VAL(MIXINL, ENA, DISABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8350_SET_REG_VAL(MIXINR, ENA, DISABLE, 
                                         PowerControlInReg);
            //ADC power off
            PowerADCDACReg = WM8350_SET_REG_VAL(R11_ADCL, ENA, DISABLE, 
                                         PowerADCDACReg);
            PowerADCDACReg = WM8350_SET_REG_VAL(R11_ADCR, ENA, DISABLE, 
                                         PowerADCDACReg);
            //Input for Mic/Line-In power off
            LeftInVolReg = WM8350_SET_REG_VAL(R80_INL, ENA, DISABLE, 
                                         LeftInVolReg);
            RightInVolReg = WM8350_SET_REG_VAL(R81_INR, ENA, DISABLE, 
                                         RightInVolReg);        
            //ADC enable.
            ADCVolLReg = WM8350_SET_REG_VAL(R66_ADCL, ENA, DISABLE, 
                                         ADCVolLReg);
            ADCVolLReg = WM8350_SET_REG_VAL(R66_ADC, VU, DISABLE, 
                                         ADCVolLReg);
            ADCVolLReg = WM8350_SET_REG_DEF(ADCL, VOL, DEFAULT, 
                                         ADCVolLReg);
            ADCVolRReg = WM8350_SET_REG_VAL(R67_ADCR, ENA, DISABLE, 
                                         ADCVolRReg);
            ADCVolRReg = WM8350_SET_REG_VAL(R67_ADC, VU, DISABLE, 
                                         ADCVolRReg);
            ADCVolRReg = WM8350_SET_REG_DEF(ADCR, VOL, DEFAULT, 
                                         ADCVolRReg);
        }
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LEFT_INPUT_VOLUME, 
            LeftInVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_RIGHT_INPUT_VOLUME, 
            RightInVolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIGITAL_VOLUME_L, 
            ADCVolLReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DIGITAL_VOLUME_R, 
            ADCVolRReg);
        if (!IsSuccess)
            return IsSuccess;

    }

    if ((SignalType & NvOdmAudioSignalType_HeadphoneOut))
    {
        IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT1_VOLUME, 
            &LOut1VolReg);
        IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT1_VOLUME, 
            &ROut1VolReg);
        if (!IsSuccess)
            return IsSuccess;
        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.HPOutPower = NV_TRUE;
            //OUT1 enable
            PowerControlOutReg = WM8350_SET_REG_VAL(R10_OUT1L, ENA, ENABLE, 
                                         PowerControlOutReg);
            PowerControlOutReg = WM8350_SET_REG_VAL(R10_OUT1R, ENA, ENABLE, 
                                         PowerControlOutReg);
            //OUT1 Volume enable
            LOut1VolReg = WM8350_SET_REG_VAL(R104_OUT1L, ENA, ENABLE, 
                                         LOut1VolReg);
            ROut1VolReg = WM8350_SET_REG_VAL(R105_OUT1R, ENA, ENABLE, 
                                         ROut1VolReg);
        }
        else
        {
            hAudioCodec->WCodecControl.HPOutPower = NV_FALSE;
            //OUT1 disable
            PowerControlOutReg = WM8350_SET_REG_VAL(R10_OUT1L, ENA, DISABLE, 
                                         PowerControlOutReg);
            PowerControlOutReg = WM8350_SET_REG_VAL(R10_OUT1R, ENA, DISABLE, 
                                         PowerControlOutReg);
            //OUT1 Volume disable
            LOut1VolReg = WM8350_SET_REG_VAL(R104_OUT1L, ENA, DISABLE, 
                                         LOut1VolReg);
            ROut1VolReg = WM8350_SET_REG_VAL(R105_OUT1R, ENA, DISABLE, 
                                         ROut1VolReg);
        }
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT1_VOLUME, 
            LOut1VolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT1_VOLUME, 
            ROut1VolReg);
        if (!IsSuccess)
            return IsSuccess;
    }
    if ((SignalType & NvOdmAudioSignalType_LineOut))
    {
        IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, 
            &LOut2VolReg);
        IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, 
            &ROut2VolReg);
        if (!IsSuccess)
            return IsSuccess;
        
        if (IsPowerOn)
        {
            hAudioCodec->WCodecControl.LineOutPower = NV_TRUE;
            //OUT2 enable
            PowerControlOutReg = WM8350_SET_REG_VAL(R10_OUT2L, ENA, ENABLE, 
                                         PowerControlOutReg);
            PowerControlOutReg = WM8350_SET_REG_VAL(R10_OUT2R, ENA, ENABLE, 
                                         PowerControlOutReg);
            //OUT2 Volume enable
            LOut2VolReg = WM8350_SET_REG_VAL(R106_OUT2L, ENA, ENABLE, 
                                         LOut2VolReg);
            ROut2VolReg = WM8350_SET_REG_VAL(R107_OUT2R, ENA, ENABLE, 
                                         ROut2VolReg);
        }
        else
        {
            hAudioCodec->WCodecControl.LineOutPower = NV_FALSE;
            //OUT2 disable
            PowerControlOutReg = WM8350_SET_REG_VAL(R10_OUT2L, ENA, DISABLE, 
                                         PowerControlOutReg);
            PowerControlOutReg = WM8350_SET_REG_VAL(R10_OUT2R, ENA, DISABLE, 
                                         PowerControlOutReg);
            //OUT2 Volume disable
            LOut2VolReg = WM8350_SET_REG_VAL(R106_OUT2L, ENA, DISABLE, 
                                         LOut2VolReg);
            ROut2VolReg = WM8350_SET_REG_VAL(R107_OUT2R, ENA, DISABLE, 
                                         ROut2VolReg);
        }
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_LOUT2_VOLUME, 
            LOut2VolReg);
        IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ROUT2_VOLUME, 
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
            PowerADCDACReg = WM8350_SET_REG_VAL(R11_DACL, ENA, ENABLE, 
                                         PowerADCDACReg);
            PowerADCDACReg = WM8350_SET_REG_VAL(R11_DACR, ENA, ENABLE, 
                                         PowerADCDACReg);
            //Out Mixer enable.
            PowerControlInReg = WM8350_SET_REG_VAL(R9_MIXOUTL, ENA, ENABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8350_SET_REG_VAL(R9_MIXOUTR, ENA, ENABLE, 
                                         PowerControlInReg);
        }
        else
        {
            //DAC disable.
            PowerADCDACReg = WM8350_SET_REG_VAL(R11_DACL, ENA, DISABLE, 
                                         PowerADCDACReg);
            PowerADCDACReg = WM8350_SET_REG_VAL(R11_DACR, ENA, DISABLE, 
                                         PowerADCDACReg);
            //Out Mixer disable.
            PowerControlInReg = WM8350_SET_REG_VAL(R9_MIXOUTL, ENA, DISABLE, 
                                         PowerControlInReg);
            PowerControlInReg = WM8350_SET_REG_VAL(R9_MIXOUTR, ENA, DISABLE, 
                                         PowerControlInReg);
        }

    }
    
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_2, 
                    PowerControlInReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_3, 
                    PowerControlOutReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_POWER_MGMT_4, 
                    PowerADCDACReg);
    
    return IsSuccess;
}

#if LOOPBACK_ENABLED
static NvBool SetLoopBackCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;
    NvU32 ADCDACCompReg;
    IsSuccess = WM8350_ReadRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DAC_COMP, 
        &ADCDACCompReg);
    if (!IsSuccess)
        return IsSuccess;
    
    ADCDACCompReg = WM8350_SET_REG_VAL(LOOPBACK, ENA, ENABLE, 
                                         ADCDACCompReg);
    IsSuccess = WM8350_WriteRegister(hOdmAudioCodec, WCodecRegIndex_ADC_DAC_COMP, 
                    ADCDACCompReg);

    return IsSuccess;
}
#endif

static NvBool OpenWolfsonCodec(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    NvBool IsSuccess = NV_TRUE;

    // Reset the codec.
    IsSuccess = ResetCodec(hOdmAudioCodec);
   
    if (IsSuccess)
        IsSuccess = SetDacMute(hOdmAudioCodec, NV_TRUE);

    if (IsSuccess)
        IsSuccess = SetAntiPopCodec(hOdmAudioCodec, NV_TRUE);
    
    if (IsSuccess)
        IsSuccess = SetCodecPower(hOdmAudioCodec, NV_TRUE);
#if ALL_INPUT_ENABLED
    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_LineIn, 0, 
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
            NV_TRUE);

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_Aux, 0, 
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
            NV_TRUE);
#endif
    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_MicIn, 0, 
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight),
            NV_TRUE);
        
    if (IsSuccess)
        IsSuccess = InitializeDigitalInterface(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeHeadphoneOut(hOdmAudioCodec);
#if LINEOUT_ENABLED
    if (IsSuccess)
        IsSuccess = InitializeLineOut(hOdmAudioCodec);
#endif
#if ALL_INPUT_ENABLED
    if (IsSuccess)
        IsSuccess = InitializeLineInput(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = InitializeAuxInput(hOdmAudioCodec);
#endif
    if (IsSuccess)
        IsSuccess = InitializeMICInput(hOdmAudioCodec);

/*
     if (IsSuccess)       
        IsSuccess = SetBypass(hOdmAudioCodec, NvOdmAudioSignalType_Aux, 
            0, NvOdmAudioSignalType_HeadphoneOut, 
            0, NV_TRUE);
*/    
    if (IsSuccess)
        IsSuccess = InitializeAnalogAudioPath(hOdmAudioCodec);

    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_HeadphoneOut, 0,
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight), NV_TRUE);

#if LINEOUT_ENABLED
    if (IsSuccess)
        IsSuccess = SetPowerStatus(hOdmAudioCodec, NvOdmAudioSignalType_LineOut, 0,
            (NvOdmAudioSpeakerType_FrontLeft | NvOdmAudioSpeakerType_FrontRight), NV_TRUE);
#endif

    if (IsSuccess)
        IsSuccess = SetAntiPopCodec(hOdmAudioCodec, NV_FALSE);

#if LOOPBACK_ENABLED
    // ADC to DAC loopback for test..
    if (IsSuccess)
        IsSuccess = SetLoopBackCodec(hOdmAudioCodec);
#endif

    if (IsSuccess)
        IsSuccess = SetDacMute(hOdmAudioCodec, NV_FALSE);

    if (!IsSuccess)
        ShutDownCodec(hOdmAudioCodec);

    return IsSuccess;
}

static const NvOdmAudioPortCaps * ACWM8350GetPortCaps(void)
{
    static const NvOdmAudioPortCaps s_PortCaps =
        {
            1,    // MaxNumberOfInputPort;
            1     // MaxNumberOfOutputPort;
        };    
    return &s_PortCaps;
}


static const NvOdmAudioOutputPortCaps * 
ACWM8350GetOutputPortCaps(
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
ACWM8350GetInputPortCaps(
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

#define NvOdmGpioPinGroup_OEM_Mute    3
static void AudioCodecHeadphoneOutHWMute(NvBool IsMute)
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
        //-----------------------------------------------------
        // Set Mute key as LOW - Unmute
        //-----------------------------------------------------
        Port = pPinInfo[NvOdmGpioPinGroup_OEM_Mute].Port;
        Pin  = pPinInfo[NvOdmGpioPinGroup_OEM_Mute].Pin;

        hPin = NvOdmGpioAcquirePinHandle(hOdmGpio, Port, Pin);
        if (hPin == NULL)
        {
            goto AudioCodecHeadphoneOutHWMuteExit;
        }
        // Configure the pin.
        mode = NvOdmGpioPinMode_Output;
        NvOdmGpioConfig(hOdmGpio, hPin, mode);
        // hold pwr key
        NvOdmGpioSetState(hOdmGpio, hPin, IsMute);
        NvOdmGpioReleasePinHandle(hOdmGpio, hPin);

        
     }
AudioCodecHeadphoneOutHWMuteExit:
     NvOdmGpioClose(hOdmGpio);

}

static void ACWM8350Open(NvOdmPrivAudioCodecHandle hOdmAudioCodec,
                        const NvOdmQueryI2sACodecInterfaceProp *pI2sCodecInt,
                        void* hCodecNotifySem)
{
    NvBool IsSuccess = NV_TRUE;
    WM8350AudioCodecHandle hNewAcodec = (WM8350AudioCodecHandle)hOdmAudioCodec;
    NvU32 Index;
    
    hNewAcodec->hOdmService = NULL;
    hNewAcodec->hOdmServicePmuDevice = NULL;

    // Codec interface paramater
    hNewAcodec->WCodecInterface.DeviceAddress = pI2sCodecInt->DeviceAddress;
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
    
    // Set all register values to reset and initailize the addresses
    for (Index = 0; Index < WCodecRegIndex_Max; ++Index)
        hNewAcodec->WCodecRegVals[Index] = 0;

    IsSuccess = SetPowerOnCodec(hNewAcodec, NV_TRUE);
    if (!IsSuccess)
        goto ErrorExit;        

    // Opening the I2C ODM Service
    hNewAcodec->hOdmService = NvOdmI2cOpen(NvOdmIoModule_I2c_Pmu, 0);
    if (!hNewAcodec->hOdmService)
        goto ErrorExit;

    // Unmute H/W blocking to Headphone out
    AudioCodecHeadphoneOutHWMute(NV_TRUE);

    if (OpenWolfsonCodec(hNewAcodec))
        return;

ErrorExit:
    NvOdmI2cClose(hNewAcodec->hOdmService);
    hNewAcodec->hOdmService = NULL;
}

static void ACWM8350Close(NvOdmPrivAudioCodecHandle hOdmAudioCodec)
{
    WM8350AudioCodecHandle hAudioCodec = (WM8350AudioCodecHandle)hOdmAudioCodec;
    if (hOdmAudioCodec != NULL)
    {
        SetDacMute(hOdmAudioCodec, NV_TRUE);
        SetAntiPopCodec(hOdmAudioCodec, NV_TRUE);
        ShutDownCodec(hOdmAudioCodec);
        SetAntiPopCodec(hOdmAudioCodec, NV_FALSE);
        (void)SetPowerOnCodec(hOdmAudioCodec, NV_FALSE);
        NvOdmI2cClose(hAudioCodec->hOdmService);
    }
}


static const NvOdmAudioWave * 
ACWM8350GetPortPcmCaps(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvU32 *pValidListCount)
{
    static const NvOdmAudioWave s_AudioPcmProps[4] = 
    {
        // NumberOfChannels;
        // IsSignedData;
        // IsLittleEndian;
        // IsInterleaved;
        // NumberOfBitsPerSample; 
        // SamplingRateInHz; 
        // NvOdmAudioWaveFormat FormatType;
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 16, 8000, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 20, 11025, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 20, 22050, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
        {2, NV_FALSE, NV_FALSE, NV_TRUE, 24, 44100, 32, NvOdmQueryI2sDataCommFormat_I2S, NvOdmAudioWaveFormat_Pcm},
    };

    *pValidListCount = 4;
    return &s_AudioPcmProps[0];
}

static NvBool 
ACWM8350SetPortPcmProps(
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
ACWM8350SetVolume(
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
        else if (pVolume->SignalType ==  NvOdmAudioSignalType_LineOut)
        {
            (void)SetLineOutVolume(hOdmAudioCodec, pVolume->ChannelMask, 
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
        else if (pVolume->SignalType ==  NvOdmAudioSignalType_Aux)
        {
            (void)SetAUXInVolume(hOdmAudioCodec, pVolume->ChannelMask,
                pVolume->VolumeLevel, pVolume->VolumeLevel);
        }
    }
}


static void 
ACWM8350SetMuteControl(
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
}


static NvBool 
ACWM8350SetConfiguration(
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
        return ACWM8350SetPortPcmProps(hOdmAudioCodec, PortName, pConfigData);
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
ACWM8350GetConfiguration(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvOdmAudioConfigure ConfigType,
    void *pConfigData)
{
}

static void 
ACWM8350SetPowerState(
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
ACWM8350SetPowerMode(
    NvOdmPrivAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioCodecPowerMode PowerMode)
{
    return;
    
}

static void 
ACWM8350SetOperationMode(
    NvOdmAudioCodecHandle hOdmAudioCodec, 
    NvOdmAudioPortName PortName,
    NvBool IsMaster)
{
    return;
}

void W8350InitCodecInterface(NvOdmAudioCodec *pCodecInterface)
{
    pCodecInterface->pfnGetPortCaps      = ACWM8350GetPortCaps;
    pCodecInterface->pfnGetOutputPortCaps = ACWM8350GetOutputPortCaps;
    pCodecInterface->pfnGetInputPortCaps = ACWM8350GetInputPortCaps;
    pCodecInterface->pfnGetPortPcmCaps   = ACWM8350GetPortPcmCaps;
    pCodecInterface->pfnOpen             = ACWM8350Open;
    pCodecInterface->pfnClose            = ACWM8350Close;
    pCodecInterface->pfnSetVolume        = ACWM8350SetVolume;
    pCodecInterface->pfnSetMuteControl   = ACWM8350SetMuteControl;
    pCodecInterface->pfnSetConfiguration = ACWM8350SetConfiguration;
    pCodecInterface->pfnGetConfiguration = ACWM8350GetConfiguration;
    pCodecInterface->pfnSetPowerState    = ACWM8350SetPowerState;
    pCodecInterface->pfnSetPowerMode     = ACWM8350SetPowerMode;
    pCodecInterface->pfnSetOperationMode = ACWM8350SetOperationMode;

    pCodecInterface->hCodecPrivate = &s_W8350;
    pCodecInterface->IsInited = NV_TRUE;
    
}
