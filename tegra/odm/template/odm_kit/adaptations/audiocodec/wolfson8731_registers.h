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
 * @file
 * <b>NVIDIA APX ODM Kit:
 *         Wolfson codec Drivers</b>
 *
 * @b Description: This file contains the register details of the wolfson codec 
 * 8731.
 */
#ifndef INCLUDED_WOLFSON8731_REGISTERS_H
#define INCLUDED_WOLFSON8731_REGISTERS_H


// Left input volume control
#define W8731_LEFT_LINEIN_0_LIN_VOL_LOC                             0
#define W8731_LEFT_LINEIN_0_LIN_VOL_SEL                             0x1F

// Left line in mute
#define W8731_LEFT_LINEIN_0_LIN_MUTE_LOC                            7
#define W8731_LEFT_LINEIN_0_LIN_MUTE_SEL                            0x1
#define W8731_LEFT_LINEIN_0_LIN_MUTE_ENABLE                         1
#define W8731_LEFT_LINEIN_0_LIN_MUTE_DISABLE                        0

// Left line in select both for left value as well as right value
#define W8731_LEFT_LINEIN_0_LRIN_BOTH_LOC                           8
#define W8731_LEFT_LINEIN_0_LRIN_BOTH_SEL                           0x1
#define W8731_LEFT_LINEIN_0_LRIN_BOTH_ENABLE_SIMULT                 1
#define W8731_LEFT_LINEIN_0_LRIN_BOTH_DISABLE_SIMULT                0

// Right input volume control
#define W8731_RIGHT_LINEIN_0_RIN_VOL_LOC                            0
#define W8731_RIGHT_LINEIN_0_RIN_VOL_SEL                            0x1F

// Right line in mute
#define W8731_RIGHT_LINEIN_0_RIN_MUTE_LOC                           7
#define W8731_RIGHT_LINEIN_0_RIN_MUTE_SEL                           0x1
#define W8731_RIGHT_LINEIN_0_RIN_MUTE_ENABLE                        1
#define W8731_RIGHT_LINEIN_0_RIN_MUTE_DISABLE                       0

// Right line in select both for left value as well as right value
#define W8731_RIGHT_LINEIN_0_RLIN_BOTH_LOC                          8
#define W8731_RIGHT_LINEIN_0_RLIN_BOTH_SEL                          0x1
#define W8731_RIGHT_LINEIN_0_RLIN_BOTH_ENABLE_SIMULT                1
#define W8731_RIGHT_LINEIN_0_RLIN_BOTH_DISABLE_SIMULT               0

// Left channel headphone volume
#define W8731_LEFT_HEADPHONE_0_LHP_VOL_LOC                          0
#define W8731_LEFT_HEADPHONE_0_LHP_VOL_SEL                          0x7F

// Left channel headphone zero cross detect
#define W8731_LEFT_HEADPHONE_0_LHP_ZCEN_LOC                         7
#define W8731_LEFT_HEADPHONE_0_LHP_ZCEN_SEL                         0x1
#define W8731_LEFT_HEADPHONE_0_LHP_ZCEN_ENABLE                      1
#define W8731_LEFT_HEADPHONE_0_LHP_ZCEN_DISABLE                     0

// Left channel headphone select both
#define W8731_LEFT_HEADPHONE_0_LRHP_BOTH_LOC                        8
#define W8731_LEFT_HEADPHONE_0_LRHP_BOTH_SEL                        0x1
#define W8731_LEFT_HEADPHONE_0_LRHP_BOTH_ENABLE_SIMULT              1
#define W8731_LEFT_HEADPHONE_0_LRHP_BOTH_DISABLE_SIMULT             0

// Right channel headphone volume
#define W8731_RIGHT_HEADPHONE_0_RHP_VOL_LOC                         0
#define W8731_RIGHT_HEADPHONE_0_RHP_VOL_SEL                         0x7F

// Right channel headphone zero cross detect
#define W8731_RIGHT_HEADPHONE_0_RHP_ZCEN_LOC                        7
#define W8731_RIGHT_HEADPHONE_0_RHP_ZCEN_SEL                        0x1
#define W8731_RIGHT_HEADPHONE_0_RHP_ZCEN_ENABLE                     1
#define W8731_RIGHT_HEADPHONE_0_RHP_ZCEN_DISABLE                    0

// Right channel headphone select both
#define W8731_RIGHT_HEADPHONE_0_RLHP_BOTH_LOC                       8
#define W8731_RIGHT_HEADPHONE_0_RLHP_BOTH_SEL                       0x1
#define W8731_RIGHT_HEADPHONE_0_RLHP_BOTH_ENABLE_SIMULT             1
#define W8731_RIGHT_HEADPHONE_0_RLHP_BOTH_DISABLE_SIMULT            0

// Microphone boost enable/diasble
#define W8731_ANALOG_PATH_0_MICBOOST_LOC                            0
#define W8731_ANALOG_PATH_0_MICBOOST_SEL                            0x1
#define W8731_ANALOG_PATH_0_MICBOOST_ENABLE                         1
#define W8731_ANALOG_PATH_0_MICBOOST_DISABLE                        0

// Mute/unmute the microphone
#define W8731_ANALOG_PATH_0_MUTEMIC_LOC                             1
#define W8731_ANALOG_PATH_0_MUTEMIC_SEL                             0x1
#define W8731_ANALOG_PATH_0_MUTEMIC_ENABLE                          1
#define W8731_ANALOG_PATH_0_MUTEMIC_DISABLE                         0

// Input analog path selection
#define W8731_ANALOG_PATH_0_INSEL_LOC                               2
#define W8731_ANALOG_PATH_0_INSEL_SEL                               0x1
#define W8731_ANALOG_PATH_0_INSEL_MIC                               1
#define W8731_ANALOG_PATH_0_INSEL_LINEIN                            0

// Enable/disable the analog input bypass
#define W8731_ANALOG_PATH_0_BYPASS_LOC                              3
#define W8731_ANALOG_PATH_0_BYPASS_SEL                              0x1
#define W8731_ANALOG_PATH_0_BYPASS_ENABLE                           1
#define W8731_ANALOG_PATH_0_BYPASS_DISABLE                          0

// Enable/disable the analog path for DAC selection
#define W8731_ANALOG_PATH_0_DACSEL_LOC                              4
#define W8731_ANALOG_PATH_0_DACSEL_SEL                              0x1
#define W8731_ANALOG_PATH_0_DACSEL_ENABLE                           1
#define W8731_ANALOG_PATH_0_DACSEL_DISABLE                          0

// Enable/disable the side tone attenuation
#define W8731_ANALOG_PATH_0_SIDETONE_LOC                            5
#define W8731_ANALOG_PATH_0_SIDETONE_SEL                            0x1
#define W8731_ANALOG_PATH_0_SIDETONE_ENABLE                         1
#define W8731_ANALOG_PATH_0_SIDETONE_DISABLE                        0

// Select the side tone attenuation
#define W8731_ANALOG_PATH_0_SIDEATT_LOC                             6
#define W8731_ANALOG_PATH_0_SIDEATT_SEL                             0x3
#define W8731_ANALOG_PATH_0_SIDEATT_DB_6                            0
#define W8731_ANALOG_PATH_0_SIDEATT_DB_9                            1
#define W8731_ANALOG_PATH_0_SIDEATT_DB_12                           2
#define W8731_ANALOG_PATH_0_SIDEATT_DB_15                           3

// ADC high pass filter enable/disable
#define W8731_DIGITAL_PATH_0_ADC_HPD_LOC                            0
#define W8731_DIGITAL_PATH_0_ADC_HPD_SEL                            0x1
#define W8731_DIGITAL_PATH_0_ADC_HPD_ENABLE                         1
#define W8731_DIGITAL_PATH_0_ADC_HPD_DISABLE                        0

// Control the de-emphasis
#define W8731_DIGITAL_PATH_0_DEEMPH_LOC                             1
#define W8731_DIGITAL_PATH_0_DEEMPH_SEL                             0x3
#define W8731_DIGITAL_PATH_0_DEEMPH_DISABLE                         0
#define W8731_DIGITAL_PATH_0_DEEMPH_KHZ_32                          1
#define W8731_DIGITAL_PATH_0_DEEMPH_KHZ_44_1                        2
#define W8731_DIGITAL_PATH_0_DEEMPH_KHZ_48                          3

// DAC soft mute control
#define W8731_DIGITAL_PATH_0_DACMU_LOC                              3
#define W8731_DIGITAL_PATH_0_DACMU_SEL                              0x1
#define W8731_DIGITAL_PATH_0_DACMU_ENABLE                           1
#define W8731_DIGITAL_PATH_0_DACMU_DISABLE                          0

// Store/clear DC offset when high pass filter disabled
#define W8731_DIGITAL_PATH_0_HPOR_LOC                               4
#define W8731_DIGITAL_PATH_0_HPOR_SEL                               0x1
#define W8731_DIGITAL_PATH_0_HPOR_STORE_OFFSET                      1
#define W8731_DIGITAL_PATH_0_HPOR_CLEAR_OFFSET                      0

// Power control for the line in
#define W8731_POWER_CONTROL_0_LININPD_LOC                           0
#define W8731_POWER_CONTROL_0_LININPD_SEL                           0x1
#define W8731_POWER_CONTROL_0_LININPD_ENABLE                        1
#define W8731_POWER_CONTROL_0_LININPD_DISABLE                       0

// Power control for the microphone
#define W8731_POWER_CONTROL_0_MICPD_LOC                             1
#define W8731_POWER_CONTROL_0_MICPD_SEL                             0x1
#define W8731_POWER_CONTROL_0_MICPD_ENABLE                          1
#define W8731_POWER_CONTROL_0_MICPD_DISABLE                         0

// Power control for the ADC
#define W8731_POWER_CONTROL_0_ADCPD_LOC                             2
#define W8731_POWER_CONTROL_0_ADCPD_SEL                             0x1
#define W8731_POWER_CONTROL_0_ADCPD_ENABLE                          1
#define W8731_POWER_CONTROL_0_ADCPD_DISABLE                         0

// Power control for the DAC
#define W8731_POWER_CONTROL_0_DACPD_LOC                             3
#define W8731_POWER_CONTROL_0_DACPD_SEL                             0x1
#define W8731_POWER_CONTROL_0_DACPD_ENABLE                          1
#define W8731_POWER_CONTROL_0_DACPD_DISABLE                         0

// Power control for the outputs
#define W8731_POWER_CONTROL_0_OUTPD_LOC                             4
#define W8731_POWER_CONTROL_0_OUTPD_SEL                             0x1
#define W8731_POWER_CONTROL_0_OUTPD_ENABLE                          1
#define W8731_POWER_CONTROL_0_OUTPD_DISABLE                         0

// Power control for the oscilator
#define W8731_POWER_CONTROL_0_OSCPD_LOC                             5
#define W8731_POWER_CONTROL_0_OSCPD_SEL                             0x1
#define W8731_POWER_CONTROL_0_OSCPD_ENABLE                          1
#define W8731_POWER_CONTROL_0_OSCPD_DISABLE                         0

// Power control for the clock out
#define W8731_POWER_CONTROL_0_CLKOUTPD_LOC                          6
#define W8731_POWER_CONTROL_0_CLKOUTPD_SEL                          0x1
#define W8731_POWER_CONTROL_0_CLKOUTPD_ENABLE                       1
#define W8731_POWER_CONTROL_0_CLKOUTPD_DISABLE                      0

// Power off all the modules
#define W8731_POWER_CONTROL_0_POWEROFF_LOC                          7
#define W8731_POWER_CONTROL_0_POWEROFF_SEL                          0x1
#define W8731_POWER_CONTROL_0_POWEROFF_ENABLE                       1
#define W8731_POWER_CONTROL_0_POWEROFF_DISABLE                      0

// Audio data format select
#define W8731_DIGITAL_INTERFACE_0_FORMAT_LOC                        0
#define W8731_DIGITAL_INTERFACE_0_FORMAT_SEL                        0x3
#define W8731_DIGITAL_INTERFACE_0_FORMAT_DSP_MODE                   3
#define W8731_DIGITAL_INTERFACE_0_FORMAT_I2S_MODE                   2
#define W8731_DIGITAL_INTERFACE_0_FORMAT_LEFT_JUSTIFIED             1
#define W8731_DIGITAL_INTERFACE_0_FORMAT_RIGHT_JUSTIFIED            0

// Input audio data bit length select
#define W8731_DIGITAL_INTERFACE_0_IWL_LOC                           2
#define W8731_DIGITAL_INTERFACE_0_IWL_SEL                           0x3
#define W8731_DIGITAL_INTERFACE_0_IWL_BIT_16                        0
#define W8731_DIGITAL_INTERFACE_0_IWL_BIT_20                        1
#define W8731_DIGITAL_INTERFACE_0_IWL_BIT_24                        2
#define W8731_DIGITAL_INTERFACE_0_IWL_BIT_32                        3

// DAC LRC phase control (left, rigth, or I2S format)
// 1 Right channel DAC data when DACLRC high
// 0 Right channel DAC data when DACLRC low
#define W8731_DIGITAL_INTERFACE_0_LRP_LOC                           4
#define W8731_DIGITAL_INTERFACE_0_LRP_SEL                           0x1
#define W8731_DIGITAL_INTERFACE_0_LRP_LEFT_DACLRC_HIGH              1
#define W8731_DIGITAL_INTERFACE_0_LRP_LEFT_DACLRC_LOW               0

// DAC left right clock swap
// 1 Right channel DAC data left
// 0 Right channel DAC data right
#define W8731_DIGITAL_INTERFACE_0_LRSWAP_LOC                        5
#define W8731_DIGITAL_INTERFACE_0_LRSWAP_SEL                        0x1
#define W8731_DIGITAL_INTERFACE_0_LRSWAP_RIGHT_DATA_DAC_LEFT        1
#define W8731_DIGITAL_INTERFACE_0_LRSWAP_RIGHT_DATA_DAC_RIGHT       0 

// Master/slave mode control
#define W8731_DIGITAL_INTERFACE_0_MS_LOC                            6
#define W8731_DIGITAL_INTERFACE_0_MS_SEL                            0x1
#define W8731_DIGITAL_INTERFACE_0_MS_MASTER_MODE                    1
#define W8731_DIGITAL_INTERFACE_0_MS_SLAVE_MODE                     0

// Bit clock invert
#define W8731_DIGITAL_INTERFACE_0_BCLK_INV_LOC                      7
#define W8731_DIGITAL_INTERFACE_0_BCLK_INV_SEL                      0x1
#define W8731_DIGITAL_INTERFACE_0_BCLK_INV_ENABLE                   1
#define W8731_DIGITAL_INTERFACE_0_BCLK_INV_DISABLE                  0

// USB/normal mode
// USB mode (250/272 fs)
// Normal mode (256/384 fs)
#define W8731_SAMPLING_CONTROL_0_USB_NORM_LOC                       0
#define W8731_SAMPLING_CONTROL_0_USB_NORM_SEL                       0x1
#define W8731_SAMPLING_CONTROL_0_USB_NORM_MODE_USB                  1
#define W8731_SAMPLING_CONTROL_0_USB_NORM_MODE_NORM                 0

// Base oversampling rate
// USB mode 0-= 250, 1 = 272
// Normal mode 0 = 256 fs, 1 = 384 fs
#define W8731_SAMPLING_CONTROL_0_BOSR_LOC                           1
#define W8731_SAMPLING_CONTROL_0_BOSR_SEL                           0x1
#define W8731_SAMPLING_CONTROL_0_BOSR_ENABLE                        1
#define W8731_SAMPLING_CONTROL_0_BOSR_DISABLE                       0

// Sampling rate selection
#define W8731_SAMPLING_CONTROL_0_SR_LOC                             2
#define W8731_SAMPLING_CONTROL_0_SR_SEL                             0xF
#define W8731_SAMPLING_CONTROL_0_SR_ADC_48_DAC_48                   0X0
#define W8731_SAMPLING_CONTROL_0_SR_ADC_441_DAC_441                 0X8
#define W8731_SAMPLING_CONTROL_0_SR_ADC_48_DAC_8                    0X1
#define W8731_SAMPLING_CONTROL_0_SR_ADC_441_DAC_8                   0X9
#define W8731_SAMPLING_CONTROL_0_SR_ADC_8_DAC_48                    0X2
#define W8731_SAMPLING_CONTROL_0_SR_ADC_8_DAC_441                   0XA
#define W8731_SAMPLING_CONTROL_0_SR_ADC_8_DAC_8                     0X3
#define W8731_SAMPLING_CONTROL_0_SR_ADC_8_DAC_8_1                   0XB
#define W8731_SAMPLING_CONTROL_0_SR_ADC_32_DAC_32                   0X6
#define W8731_SAMPLING_CONTROL_0_SR_ADC_96_DAC_96                   0X7
#define W8731_SAMPLING_CONTROL_0_SR_ADC_882_DAC_882                 0XF

#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ENABLE                 1      
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_DISABLE                0      
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_48_DAC_48          0      
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_441_DAC_441        1
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_48_DAC_8           0
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_441_DAC_8          1
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_8_DAC_48           0
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_8_DAC_441          1
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_8_DAC_8            0
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_8_DAC_8_           1
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_32_DAC_32          0
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_96_DAC_96          0
#define W8731_SAMPLING_CONTROL_0_SR_USB_BOSR_ADC_882_DAC_882        1

// Core clock divider select
// 1 = Core clock is MCLK divided by 2
// 0 = Core clock is MCLK 
#define W8731_SAMPLING_CONTROL_0_CLKI_DIV2_LOC                      6
#define W8731_SAMPLING_CONTROL_0_CLKI_DIV2_SEL                      0x1
#define W8731_SAMPLING_CONTROL_0_CLKI_DIV2_MCLK                     1
#define W8731_SAMPLING_CONTROL_0_CLKI_DIV2_MCLK_DIV2                0

// Clock out divider select
// 1 = clock out is core clock divided by 2
// 0 = clock out is core clock
#define W8731_SAMPLING_CONTROL_0_CLK0_DIV2_LOC                      7
#define W8731_SAMPLING_CONTROL_0_CLK0_DIV2_SEL                      0x1
#define W8731_SAMPLING_CONTROL_0_CLK0_DIV2_DIV2                     1
#define W8731_SAMPLING_CONTROL_0_CLK0_DIV2_DIV_DISABLE              0

// Activate/deactivate interface control
#define W8731_ACTIVE_0_ACTIVE_LOC                                   0
#define W8731_ACTIVE_0_ACTIVE_SEL                                   0x1
#define W8731_ACTIVE_0_ACTIVE_ACTIVATE                              1
#define W8731_ACTIVE_0_ACTIVE_DEACTIVATE                            0

// Reset register
#define W8731_RESET_0_RESET_LOC                                     0
#define W8731_RESET_0_RESET_SEL                                     0xFF
#define W8731_RESET_0_RESET_VAL                                     0X00


#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_WOLFSON8731_REGISTERS_H
