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
 * 8753.
 */
#ifndef INCLUDED_WOLFSON8753_REGISTERS_H
#define INCLUDED_WOLFSON8753_REGISTERS_H


/////////// ADC Control (R2) ///////
#define W8753_ADC_CONTROL_0_DATSEL_LOC                                       7
#define W8753_ADC_CONTROL_0_DATSEL_SEL                                       0x3
#define W8753_ADC_CONTROL_0_DATSEL_LEFT_RIGHT                                0
#define W8753_ADC_CONTROL_0_DATSEL_LEFT_LEFT                                 1
#define W8753_ADC_CONTROL_0_DATSEL_RIGHT_RIGHT                               2
#define W8753_ADC_CONTROL_0_DATSEL_RIGHT_LEFT                                3

#define W8753_ADC_CONTROL_0_VXFILT_LOC                                       4
#define W8753_ADC_CONTROL_0_VXFILT_SEL                                       0x1
#define W8753_ADC_CONTROL_0_VXFILT_HIFI_FILTER                               0x0
#define W8753_ADC_CONTROL_0_VXFILT_VOICE_FILTER                              0x1



/////////// Left channel and Right Channle PGA (R49 and R50) ///////
// Input volume control Reg (PGA Control) R49 and R50
// Left Input volume control R49(31H) and right input volume control Reg R50 (32H)
// This is the PGA control to the ADC.

// Left and right both control
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LIVU_LOC                               8
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LIVU_SEL                               0x1
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LIVU_LEFT                              0
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LIVU_BOTH                              1

// Left input mute control
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LINMUTE_LOC                            7
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LINMUTE_SEL                            0x1
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LINMUTE_DISABLE                        0
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LINMUTE_ENABLE                         1

// Left input volume zero cross
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LZCEN_LOC                              6
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LZCEN_SEL                              0x1
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LZCEN_DISABLE                          0
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LZCEN_ENABLE                           1

// Left input volume control
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LINVOL_LOC                             0
#define W8753_LEFT_CHANNEL_PGA_VOL_0_LINVOL_SEL                             0x3F

// Right and left both control
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RIVU_LOC                              8
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RIVU_SEL                              0x1
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RIVU_RIGHT                            0
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RIVU_BOTH                             1

// Right input mute control
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RINMUTE_LOC                           7
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RINMUTE_SEL                           0x1
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RINMUTE_DISABLE                       0
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RINMUTE_ENABLE                        1

// Right input volume zero cross
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RZCEN_LOC                             6
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RZCEN_SEL                             0x1
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RZCEN_DISABLE                         0
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RZCEN_ENABLE                          1

// Right input volume control
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RINVOL_LOC                            0
#define W8753_RIGHT_CHANNEL_PGA_VOL_0_RINVOL_SEL                            0x3F


/////////// Left channel and Right Channle digital adc volume Control
////////// (R16 and R17) ///////
// Left and right both control
#define W8753_LEFT_ADC_DIG_VOL_0_LAVU_LOC                                    8
#define W8753_LEFT_ADC_DIG_VOL_0_LAVU_SEL                                    0x1
#define W8753_LEFT_ADC_DIG_VOL_0_LAVU_LEFT                                   0
#define W8753_LEFT_ADC_DIG_VOL_0_LAVU_BOTH                                   1

// Left ADC digital volume control
#define W8753_LEFT_ADC_DIG_VOL_0_LADCVOL_LOC                                 0
#define W8753_LEFT_ADC_DIG_VOL_0_LADCVOL_SEL                                 0xFF
#define W8753_LEFT_ADC_DIG_VOL_0_LADCVOL_DIGMUTE                             0x0
#define W8753_LEFT_ADC_DIG_VOL_0_LADCVOL_NORMAL                              0xC3

// Right and left both control
#define W8753_RIGHT_ADC_DIG_VOL_0_RAVU_LOC                                   8
#define W8753_RIGHT_ADC_DIG_VOL_0_RAVU_SEL                                   0x1
#define W8753_RIGHT_ADC_DIG_VOL_0_RAVU_RIGHT                                 0
#define W8753_RIGHT_ADC_DIG_VOL_0_RAVU_BOTH                                  1

// Right ADC digital volume control
#define W8753_RIGHT_ADC_DIG_VOL_0_RADCVOL_LOC                                0
#define W8753_RIGHT_ADC_DIG_VOL_0_RADCVOL_SEL                                0xFF
#define W8753_RIGHT_ADC_DIG_VOL_0_RADCVOL_DIGMUTE                            0x0
#define W8753_RIGHT_ADC_DIG_VOL_0_RADCVOL_NORMAL                             0xC3


// Line Output volume R40/41/42/43/44/45
// LOUT1/ROUT1/LOUT2/ROUT2/MONOOUT1/MONOOUT2 Volume
// Speaker/headphone/mono output/LineOut control.

// Left volume out
#define W8753_LOUT1_VOL_0_LOUT1VOL_LOC                                      0
#define W8753_LOUT1_VOL_0_LOUT1VOL_SEL                                      0x7F

// Zero cross enable/disable
#define W8753_LOUT1_VOL_0_LO1ZC_LOC                                         7
#define W8753_LOUT1_VOL_0_LO1ZC_SEL                                         0x1
#define W8753_LOUT1_VOL_0_LO1ZC_DISABLE                                     0
#define W8753_LOUT1_VOL_0_LO1ZC_ENABLE                                      1

// Select left or both
#define W8753_LOUT1_VOL_0_LO1VU_LOC                                         8
#define W8753_LOUT1_VOL_0_LO1VU_SEL                                         0x1
#define W8753_LOUT1_VOL_0_LO1VU_LEFT                                        0
#define W8753_LOUT1_VOL_0_LO1VU_BOTH                                        1

// Right volume out
#define W8753_ROUT1_VOL_0_ROUT1VOL_LOC                                      0
#define W8753_ROUT1_VOL_0_ROUT1VOL_SEL                                      0x7F

// Zero cross enable/disable
#define W8753_ROUT1_VOL_0_RO1ZC_LOC                                         7
#define W8753_ROUT1_VOL_0_RO1ZC_SEL                                         0x1
#define W8753_ROUT1_VOL_0_RO1ZC_DISABLE                                     0
#define W8753_ROUT1_VOL_0_RO1ZC_ENABLE                                      1

// Select right or both
#define W8753_ROUT1_VOL_0_RO1VU_LOC                                         8
#define W8753_ROUT1_VOL_0_RO1VU_SEL                                         0x1
#define W8753_ROUT1_VOL_0_RO1VU_LEFT                                        0
#define W8753_ROUT1_VOL_0_RO1VU_BOTH                                        1

// Left volume out
#define W8753_LOUT2_VOL_0_LOUT2VOL_LOC                                      0
#define W8753_LOUT2_VOL_0_LOUT2VOL_SEL                                      0x7F

// Zero cross enable/disable
#define W8753_LOUT2_VOL_0_LO2ZC_LOC                                         7
#define W8753_LOUT2_VOL_0_LO2ZC_SEL                                         0x1
#define W8753_LOUT2_VOL_0_LO2ZC_DISABLE                                     0
#define W8753_LOUT2_VOL_0_LO2ZC_ENABLE                                      1

// Select left or both
#define W8753_LOUT2_VOL_0_LO2VU_LOC                                         8
#define W8753_LOUT2_VOL_0_LO2VU_SEL                                         0x1
#define W8753_LOUT2_VOL_0_LO2VU_LEFT                                        0
#define W8753_LOUT2_VOL_0_LO2VU_BOTH                                        1

// Right volume out
#define W8753_ROUT2_VOL_0_ROUT2VOL_LOC                                      0
#define W8753_ROUT2_VOL_0_ROUT2VOL_SEL                                      0x7F

// Zero cross enable/disable
#define W8753_ROUT2_VOL_0_RO2ZC_LOC                                         7
#define W8753_ROUT2_VOL_0_RO2ZC_SEL                                         0x1
#define W8753_ROUT2_VOL_0_RO2ZC_DISABLE                                     0
#define W8753_ROUT2_VOL_0_RO2ZC_ENABLE                                      1

// Select right or both
#define W8753_ROUT2_VOL_0_RO2VU_LOC                                         8
#define W8753_ROUT2_VOL_0_RO2VU_SEL                                         0x1
#define W8753_ROUT2_VOL_0_RO2VU_LEFT                                        0
#define W8753_ROUT2_VOL_0_RO2VU_BOTH                                        1

// Mono volume out
#define W8753_MONOOUT_VOL_0_MONO1VOL_LOC                                    0
#define W8753_MONOOUT_VOL_0_MONO1VOL_SEL                                    0x7F

// Mono Zero cross enable/disable
#define W8753_MONOOUT_VOL_0_MOZC_LOC                                        7
#define W8753_MONOOUT_VOL_0_MOZC_SEL                                        0x1
#define W8753_MONOOUT_VOL_0_MOZC_DISABLE                                    0
#define W8753_MONOOUT_VOL_0_MOZC_ENABLE                                     1


// Output control (R45)

// Out3 control
#define W8753_OUTPUT_CONTROL_0_OUT3SW_LOC                                   0
#define W8753_OUTPUT_CONTROL_0_OUT3SW_SEL                                   0x3
#define W8753_OUTPUT_CONTROL_0_OUT3SW_VREF                                  0
#define W8753_OUTPUT_CONTROL_0_OUT3SW_ROUT2                                 1
#define W8753_OUTPUT_CONTROL_0_OUT3SW_LEFT_RIGHT_MIX_DIV2                   2
#define W8753_OUTPUT_CONTROL_0_OUT3SW_UNUSED                                3

// Monot 2 output select
#define W8753_OUTPUT_CONTROL_0_MONO2SW_LOC                                  7
#define W8753_OUTPUT_CONTROL_0_MONO2SW_SEL                                  0x3
#define W8753_OUTPUT_CONTROL_0_MONO2SW_INVERT_MONO1                         0
#define W8753_OUTPUT_CONTROL_0_MONO2SW_LEFTMIX_DIV2                         1
#define W8753_OUTPUT_CONTROL_0_MONO2SW_RIGHTMIX_DIV2                        2
#define W8753_OUTPUT_CONTROL_0_MONO2SW_LEFT_RIGHT_MIX_DIV2                  3

/////////// Left channel and Right Channle digital DAC volume Control
////////// (R8 and R9) ///////
// Left and right both control
#define W8753_LEFT_DAC_DIG_VOL_0_LDVU_LOC                                    8
#define W8753_LEFT_DAC_DIG_VOL_0_LDVU_SEL                                    0x1
#define W8753_LEFT_DAC_DIG_VOL_0_LDVU_LEFT                                   0
#define W8753_LEFT_DAC_DIG_VOL_0_LDVU_BOTH                                   1

// Left DAC digital volume control
#define W8753_LEFT_DAC_DIG_VOL_0_LDACVOL_LOC                                 0
#define W8753_LEFT_DAC_DIG_VOL_0_LDACVOL_SEL                                 0xFF
#define W8753_LEFT_DAC_DIG_VOL_0_LDACVOL_DIGMUTE                             0x0
#define W8753_LEFT_DAC_DIG_VOL_0_LDACVOL_NORMAL                              0xFF

// Right and left both control
#define W8753_RIGHT_DAC_DIG_VOL_0_RDVU_LOC                                   8
#define W8753_RIGHT_DAC_DIG_VOL_0_RDVU_SEL                                   0x1
#define W8753_RIGHT_DAC_DIG_VOL_0_RDVU_RIGHT                                 0
#define W8753_RIGHT_DAC_DIG_VOL_0_RDVU_BOTH                                  1

// Right DAC digital volume control
#define W8753_RIGHT_DAC_DIG_VOL_0_RDACVOL_LOC                                0
#define W8753_RIGHT_DAC_DIG_VOL_0_RDACVOL_SEL                                0xFF
#define W8753_RIGHT_DAC_DIG_VOL_0_RDACVOL_DIGMUTE                            0x0
#define W8753_RIGHT_DAC_DIG_VOL_0_RDACVOL_NORMAL                             0xFF


// ADC input mode/Input control R46/R47/R48
// ADC Input Mode/ Input Control1/ Input control2

// Adc input selection.
// Left ADC selection
#define W8753_ADC_INPUT_0_LADCSEL_LOC                                       0
#define W8753_ADC_INPUT_0_LADCSEL_SEL                                       0x3
#define W8753_ADC_INPUT_0_LADCSEL_PGA                                       0
#define W8753_ADC_INPUT_0_LADCSEL_LINE1                                     1
#define W8753_ADC_INPUT_0_LADCSEL_LINE1DC                                   2
#define W8753_ADC_INPUT_0_LADCSEL_UNUSED                                    3

// Right ADC selection
#define W8753_ADC_INPUT_0_RADCSEL_LOC                                       2
#define W8753_ADC_INPUT_0_RADCSEL_SEL                                       0x3
#define W8753_ADC_INPUT_0_RADCSEL_PGA                                       0
#define W8753_ADC_INPUT_0_RADCSEL_LINE2                                     1
#define W8753_ADC_INPUT_0_RADCSEL_LEFT_RIGHT_MONO_MIX                       2
#define W8753_ADC_INPUT_0_RADCSEL_UNUSED                                    3

// Mono mix selection
#define W8753_ADC_INPUT_0_MONOMIX_LOC                                       4
#define W8753_ADC_INPUT_0_MONOMIX_SEL                                       0x3
#define W8753_ADC_INPUT_0_MONOMIX_STEREO                                    0
#define W8753_ADC_INPUT_0_MONOMIX_MONOMIX_LEFTADC                           1
#define W8753_ADC_INPUT_0_MONOMIX_MONOMIX_RIGHTADC                          2
#define W8753_ADC_INPUT_0_MONOMIX_DIGITAL_MONOMIX                           3

// Input control R47
// Left Mux select
#define W8753_INPUT_CONTROL1_0_LM_LOC                                       0
#define W8753_INPUT_CONTROL1_0_LM_SEL                                       0x1
#define W8753_INPUT_CONTROL1_0_LM_LINE1                                     0
#define W8753_INPUT_CONTROL1_0_LM_RXMIX_OUT                                 1

// Right mux select
#define W8753_INPUT_CONTROL1_0_RM_LOC                                       1
#define W8753_INPUT_CONTROL1_0_RM_SEL                                       0x1
#define W8753_INPUT_CONTROL1_0_RM_LINE2                                     0
#define W8753_INPUT_CONTROL1_0_RM_RXMIX_OUT                                 1

// Mono mux select
#define W8753_INPUT_CONTROL1_0_MM_LOC                                       2
#define W8753_INPUT_CONTROL1_0_MM_SEL                                       0x1
#define W8753_INPUT_CONTROL1_0_MM_LINE_MIX                                  0
#define W8753_INPUT_CONTROL1_0_MM_RXMIX_OUT                                 1

// Line mix select
#define W8753_INPUT_CONTROL1_0_LMSEL_LOC                                    3
#define W8753_INPUT_CONTROL1_0_LMSEL_SEL                                    0x3
#define W8753_INPUT_CONTROL1_0_LMSEL_LINE1_PLUS_2                           0
#define W8753_INPUT_CONTROL1_0_LMSEL_LINE1_MIN_2                            1
#define W8753_INPUT_CONTROL1_0_LMSEL_LINE1                                  2
#define W8753_INPUT_CONTROL1_0_LMSEL_LINE2                                  3

// Mic 1 preamp gain
#define W8753_INPUT_CONTROL1_0_MIC1BOOST_LOC                                5
#define W8753_INPUT_CONTROL1_0_MIC1BOOST_SEL                                0x3
#define W8753_INPUT_CONTROL1_0_MIC1BOOST_DB12                               0
#define W8753_INPUT_CONTROL1_0_MIC1BOOST_DB18                               1
#define W8753_INPUT_CONTROL1_0_MIC1BOOST_DB24                               2
#define W8753_INPUT_CONTROL1_0_MIC1BOOST_DB30                               3

// Mic 2 preamp gain
#define W8753_INPUT_CONTROL1_0_MIC2BOOST_LOC                                7
#define W8753_INPUT_CONTROL1_0_MIC2BOOST_SEL                                0x3
#define W8753_INPUT_CONTROL1_0_MIC2BOOST_DB12                               0
#define W8753_INPUT_CONTROL1_0_MIC2BOOST_DB18                               1
#define W8753_INPUT_CONTROL1_0_MIC2BOOST_DB24                               2
#define W8753_INPUT_CONTROL1_0_MIC2BOOST_DB30                               3

/// Input control 2 R48
// ALC Mix input select Rx
#define W8753_INPUT_CONTROL2_0_RXALC_LOC                                    0
#define W8753_INPUT_CONTROL2_0_RXALC_SEL                                    0x1
#define W8753_INPUT_CONTROL2_0_RXALC_NO_RX                                  0
#define W8753_INPUT_CONTROL2_0_RXALC_RX                                     1

// ALC Mix input select MIC1
#define W8753_INPUT_CONTROL2_0_MIC1ALC_LOC                                  1
#define W8753_INPUT_CONTROL2_0_MIC1ALC_SEL                                  0x1
#define W8753_INPUT_CONTROL2_0_MIC1ALC_NO_MIC1                              0
#define W8753_INPUT_CONTROL2_0_MIC1ALC_MIC1                                 1

// ALC Mix input select MIC2
#define W8753_INPUT_CONTROL2_0_MIC2ALC_LOC                                  2
#define W8753_INPUT_CONTROL2_0_MIC2ALC_SEL                                  0x1
#define W8753_INPUT_CONTROL2_0_MIC2ALC_NO_MIC2                              0
#define W8753_INPUT_CONTROL2_0_MIC2ALC_MIC2                                 1

// ALC Mix input select Line Mix
#define W8753_INPUT_CONTROL2_0_LINEALC_LOC                                  3
#define W8753_INPUT_CONTROL2_0_LINEALC_SEL                                  0x1
#define W8753_INPUT_CONTROL2_0_LINEALC_NO_LINE                              0
#define W8753_INPUT_CONTROL2_0_LINEALC_LINE                                 1

// Mic mix sidetone select
#define W8753_INPUT_CONTROL2_0_MICMUX_LOC                                   4
#define W8753_INPUT_CONTROL2_0_MICMUX_SEL                                   0x3
#define W8753_INPUT_CONTROL2_0_MICMUX_LEFT_PGA                              0
#define W8753_INPUT_CONTROL2_0_MICMUX_MIC1PREAMP                            1
#define W8753_INPUT_CONTROL2_0_MICMUX_MIC2PREAMP                            2
#define W8753_INPUT_CONTROL2_0_MICMUX_RIGHTPGA                              3

// Differential input, Rx, Mixer
#define W8753_INPUT_CONTROL2_0_RXMSEL_LOC                                   6
#define W8753_INPUT_CONTROL2_0_RXMSEL_SEL                                   0x1
#define W8753_INPUT_CONTROL2_0_RXMSEL_RXP_MIN_RXN                           0
#define W8753_INPUT_CONTROL2_0_RXMSEL_RXP_PLUS_RXN                          1
#define W8753_INPUT_CONTROL2_0_RXMSEL_RXP                                   2
#define W8753_INPUT_CONTROL2_0_RXMSEL_RXN                                   3

//// DAC Control R1
// Dac control R1
// Deemphasis control
#define W8753_DAC_CONTROL_0_DEEMP_LOC                                       1
#define W8753_DAC_CONTROL_0_DEEMP_SEL                                       0x3
#define W8753_DAC_CONTROL_0_DEEMP_DISABLE                                   0
#define W8753_DAC_CONTROL_0_DEEMP_KHZ32                                     0
#define W8753_DAC_CONTROL_0_DEEMP_KHZ44_1                                   0
#define W8753_DAC_CONTROL_0_DEEMP_KHZ48                                     0

// Dac mute control
#define W8753_DAC_CONTROL_0_DACMU_LOC                                       3
#define W8753_DAC_CONTROL_0_DACMU_SEL                                       0x1
#define W8753_DAC_CONTROL_0_DACMU_UNMUTE                                    0
#define W8753_DAC_CONTROL_0_DACMU_MUTE                                      1

// Dac mono mixtrol
#define W8753_DAC_CONTROL_0_DMONOMIX_LOC                                    4
#define W8753_DAC_CONTROL_0_DMONOMIX_SEL                                    0x3
#define W8753_DAC_CONTROL_0_DMONOMIX_STEREO                                 0
#define W8753_DAC_CONTROL_0_DMONOMIX_MONO_TO_DACL                           1
#define W8753_DAC_CONTROL_0_DMONOMIX_MONO_TO_DACR                           2
#define W8753_DAC_CONTROL_0_DMONOMIX_MONO_TO_DACL_DACR                      3

// Dac phase invert
#define W8753_DAC_CONTROL_0_DACINV_LOC                                      6
#define W8753_DAC_CONTROL_0_DACINV_SEL                                      0x1
#define W8753_DAC_CONTROL_0_DACINV_NOINVERT                                 0
#define W8753_DAC_CONTROL_0_DACINV_INVERT                                   1

/////////////////// MicBias Control (R51) ///////////////////////////
#define W8753_MICBIAS_CONTROL_0_MBCEN_LOC                                   0
#define W8753_MICBIAS_CONTROL_0_MBCEN_SEL                                   0x1
#define W8753_MICBIAS_CONTROL_0_MBCEN_DISABLE                               0
#define W8753_MICBIAS_CONTROL_0_MBCEN_ENABLE                                1

#define W8753_MICBIAS_CONTROL_0_MBTHRESH_LOC                                1
#define W8753_MICBIAS_CONTROL_0_MBTHRESH_SEL                                0x7
#define W8753_MICBIAS_CONTROL_0_MBTHRESH_MIN                                0
#define W8753_MICBIAS_CONTROL_0_MBTHRESH_DEFAULT                            1
#define W8753_MICBIAS_CONTROL_0_MBTHRESH_MAX                                7

#define W8753_MICBIAS_CONTROL_0_MBSCTHRESH_LOC                              4
#define W8753_MICBIAS_CONTROL_0_MBSCTHRESH_SEL                              0x3
#define W8753_MICBIAS_CONTROL_0_MBSCTHRESH_MAX                              3

#define W8753_MICBIAS_CONTROL_0_MICSEL_LOC                                  6
#define W8753_MICBIAS_CONTROL_0_MICSEL_SEL                                  0x3
#define W8753_MICBIAS_CONTROL_0_MICSEL_MIC1                                 0
#define W8753_MICBIAS_CONTROL_0_MICSEL_MIC2                                 1
#define W8753_MICBIAS_CONTROL_0_MICSEL_MIC3                                 2
#define W8753_MICBIAS_CONTROL_0_MICSEL_UNUSED                               3

#define W8753_MICBIAS_CONTROL_0_MBVSEL_LOC                                  8
#define W8753_MICBIAS_CONTROL_0_MBVSEL_SEL                                  0x1
#define W8753_MICBIAS_CONTROL_0_MBVSEL_90V                                  0
#define W8753_MICBIAS_CONTROL_0_MBVSEL_75V                                  1


/////////////////// Left Mixer control1 (R34) ///////////////////////////
// LM Signal to Left mixer volume
#define W8753_LEFTMIXER_CONTROL1_0_LM2LOVOL_LOC                             4
#define W8753_LEFTMIXER_CONTROL1_0_LM2LOVOL_SEL                             0x7
#define W8753_LEFTMIXER_CONTROL1_0_LM2LOVOL_DB0                             0x2
#define W8753_LEFTMIXER_CONTROL1_0_LM2LOVOL_DEFAULT                         0x5
#define W8753_LEFTMIXER_CONTROL1_0_LM2LOVOL_MIN                             0x7


// LM2 signal to Left mixer
#define W8753_LEFTMIXER_CONTROL1_0_LM2LO_LOC                                7
#define W8753_LEFTMIXER_CONTROL1_0_LM2LO_SEL                                0x1
#define W8753_LEFTMIXER_CONTROL1_0_LM2LO_ENABLE                             1
#define W8753_LEFTMIXER_CONTROL1_0_LM2LO_DISABLE                            0

// Left dac to Left mixer
#define W8753_LEFTMIXER_CONTROL1_0_LD2LO_LOC                                8
#define W8753_LEFTMIXER_CONTROL1_0_LD2LO_SEL                                0x1
#define W8753_LEFTMIXER_CONTROL1_0_LD2LO_DISABLE                            0
#define W8753_LEFTMIXER_CONTROL1_0_LD2LO_ENABLE                             1


/////////////////// Left Mixer control2 (R35) ///////////////////////////

// Voice dac signal to left mixeer volume
#define W8753_LEFTMIXER_CONTROL2_0_VXD2LOVOL_LOC                            0
#define W8753_LEFTMIXER_CONTROL2_0_VXD2LOVOL_SEL                            0x7
#define W8753_LEFTMIXER_CONTROL2_0_VXD2LOVOL_DB0                            0x2
#define W8753_LEFTMIXER_CONTROL2_0_VXD2LOVOL_MAX                            0x0
#define W8753_LEFTMIXER_CONTROL2_0_VXD2LOVOL_MIN                            0x7

// Side tone signal to left mixer volume
#define W8753_LEFTMIXER_CONTROL2_0_ST2LOVOL_LOC                             4
#define W8753_LEFTMIXER_CONTROL2_0_ST2LOVOL_SEL                             0x7
#define W8753_LEFTMIXER_CONTROL2_0_ST2LOVOL_DB0                             0x2
#define W8753_LEFTMIXER_CONTROL2_0_ST2LOVOL_MIN                             0x7

// Sidetone siggnal to left mixer
#define W8753_LEFTMIXER_CONTROL2_0_ST2LO_LOC                                7
#define W8753_LEFTMIXER_CONTROL2_0_ST2LO_SEL                                0x1
#define W8753_LEFTMIXER_CONTROL2_0_ST2LO_DISABLE                            0
#define W8753_LEFTMIXER_CONTROL2_0_ST2LO_ENABLE                             1

// VOice dac to left mixer
#define W8753_LEFTMIXER_CONTROL2_0_VXD2LO_LOC                               8
#define W8753_LEFTMIXER_CONTROL2_0_VXD2LO_SEL                               0x1
#define W8753_LEFTMIXER_CONTROL2_0_VXD2LO_DISABLE                           0
#define W8753_LEFTMIXER_CONTROL2_0_VXD2LO_ENABLE                            1

/////////////////// Right Mixer control1 (R34) ///////////////////////////
// LM Signal to Right mixer volume
#define W8753_RIGHTMIXER_CONTROL1_0_RM2ROVOL_LOC                             4
#define W8753_RIGHTMIXER_CONTROL1_0_RM2ROVOL_SEL                             0x7
#define W8753_RIGHTMIXER_CONTROL1_0_RM2ROVOL_DB0                             0x2
#define W8753_RIGHTMIXER_CONTROL1_0_RM2ROVOL_MIN                             0x7

// RM2 signal to Right mixer
#define W8753_RIGHTMIXER_CONTROL1_0_RM2RO_LOC                                7
#define W8753_RIGHTMIXER_CONTROL1_0_RM2RO_SEL                                0x1
#define W8753_RIGHTMIXER_CONTROL1_0_RM2RO_ENABLE                             1
#define W8753_RIGHTMIXER_CONTROL1_0_RM2RO_DISABLE                            0

// Right dac to Right mixer
#define W8753_RIGHTMIXER_CONTROL1_0_RD2RO_LOC                                8
#define W8753_RIGHTMIXER_CONTROL1_0_RD2RO_SEL                                0x1
#define W8753_RIGHTMIXER_CONTROL1_0_RD2RO_DISABLE                            0
#define W8753_RIGHTMIXER_CONTROL1_0_RD2RO_ENABLE                             1

/////////////////// Right Mixer control2 (R35) ///////////////////////////
// Voice dac signal to right mixeer volume
#define W8753_RIGHTMIXER_CONTROL2_0_VXD2ROVOL_LOC                            0
#define W8753_RIGHTMIXER_CONTROL2_0_VXD2ROVOL_SEL                            0x7
#define W8753_RIGHTMIXER_CONTROL2_0_VXD2ROVOL_DB0                            0x2
#define W8753_RIGHTMIXER_CONTROL2_0_VXD2ROVOL_MAX                            0x0
#define W8753_RIGHTMIXER_CONTROL2_0_VXD2ROVOL_MIN                            0x7

// Side tone signal to right mixer volume
#define W8753_RIGHTMIXER_CONTROL2_0_ST2ROVOL_LOC                             4
#define W8753_RIGHTMIXER_CONTROL2_0_ST2ROVOL_SEL                             0x7
#define W8753_RIGHTMIXER_CONTROL2_0_ST2ROVOL_DB0                             0x2
#define W8753_RIGHTMIXER_CONTROL2_0_ST2ROVOL_MIN                             0x7

// Sidetone siggnal to right mixer
#define W8753_RIGHTMIXER_CONTROL2_0_ST2RO_LOC                                7
#define W8753_RIGHTMIXER_CONTROL2_0_ST2RO_SEL                                0x1
#define W8753_RIGHTMIXER_CONTROL2_0_ST2RO_DISABLE                            0
#define W8753_RIGHTMIXER_CONTROL2_0_ST2RO_ENABLE                             1

// VOice dac to right mixer
#define W8753_RIGHTMIXER_CONTROL2_0_VXD2RO_LOC                               8
#define W8753_RIGHTMIXER_CONTROL2_0_VXD2RO_SEL                               0x1
#define W8753_RIGHTMIXER_CONTROL2_0_VXD2RO_DISABLE                           0
#define W8753_RIGHTMIXER_CONTROL2_0_VXD2RO_ENABLE                            1


/////////////////// Mono Mixer control1 (R38) ///////////////////////////
// MM Signal to Mono mixer volume
#define W8753_MONOMIXER_CONTROL1_0_MM2MOVOL_LOC                             4
#define W8753_MONOMIXER_CONTROL1_0_MM2MOVOL_SEL                             0x7
#define W8753_MONOMIXER_CONTROL1_0_MM2MOVOL_DB0                             0x2
#define W8753_MONOMIXER_CONTROL1_0_MM2MOVOL_MIN                             0x7

// MM signal to Mono mixer
#define W8753_MONOMIXER_CONTROL1_0_MM2MO_LOC                                7
#define W8753_MONOMIXER_CONTROL1_0_MM2MO_SEL                                0x1
#define W8753_MONOMIXER_CONTROL1_0_MM2MO_ENABLE                             1
#define W8753_MONOMIXER_CONTROL1_0_MM2MO_DISABLE                            0

// Left dac to Mono mixer
#define W8753_MONOMIXER_CONTROL1_0_LD2MO_LOC                                8
#define W8753_MONOMIXER_CONTROL1_0_LD2MO_SEL                                0x1
#define W8753_MONOMIXER_CONTROL1_0_LD2MO_DISABLE                            0
#define W8753_MONOMIXER_CONTROL1_0_LD2MO_ENABLE                             1

/////////////////// Mono Mixer control2 (R39) ///////////////////////////
// Voice dac signal to mono mixeer volume
#define W8753_MONOMIXER_CONTROL2_0_VXD2MOVOL_LOC                            0
#define W8753_MONOMIXER_CONTROL2_0_VXD2MOVOL_SEL                            0x7
#define W8753_MONOMIXER_CONTROL2_0_VXD2MOVOL_DB0                            0x2
#define W8753_MONOMIXER_CONTROL2_0_VXD2MOVOL_MIN                            0x7

// Voice dac to mono mixer
#define W8753_MONOMIXER_CONTROL2_0_VXD2MO_LOC                               3
#define W8753_MONOMIXER_CONTROL2_0_VXD2MO_SEL                               0x1
#define W8753_MONOMIXER_CONTROL2_0_VXD2MO_DISABLE                           0
#define W8753_MONOMIXER_CONTROL2_0_VXD2MO_ENABLE                            1

// Side tone signal to mono mixer volume
#define W8753_MONOMIXER_CONTROL2_0_ST2MOVOL_LOC                             4
#define W8753_MONOMIXER_CONTROL2_0_ST2MOVOL_SEL                             0x7
#define W8753_MONOMIXER_CONTROL2_0_ST2MOVOL_DB0                             0x2
#define W8753_MONOMIXER_CONTROL2_0_ST2MOVOL_MIN                             0x7

// Sidetone siggnal to mono mixer
#define W8753_MONOMIXER_CONTROL2_0_ST2MO_LOC                                7
#define W8753_MONOMIXER_CONTROL2_0_ST2MO_SEL                                0x1
#define W8753_MONOMIXER_CONTROL2_0_ST2MO_DISABLE                            0
#define W8753_MONOMIXER_CONTROL2_0_ST2MO_ENABLE                             1

// Right dac to mono mixer
#define W8753_MONOMIXER_CONTROL2_0_RD2MO_LOC                                8
#define W8753_MONOMIXER_CONTROL2_0_RD2MO_SEL                                0x1
#define W8753_MONOMIXER_CONTROL2_0_RD2MO_DISABLE                            0
#define W8753_MONOMIXER_CONTROL2_0_RD2MO_ENABLE                             1


/// Power managemnet 1/2/3/4 (R20/21/22/23) ////////////
// Disable MCLK inot digital
#define W8753_POWER_MANAGEMENT1_0_DIGEN_LOC                                  0
#define W8753_POWER_MANAGEMENT1_0_DIGEN_SEL                                  0x1
#define W8753_POWER_MANAGEMENT1_0_DIGEN_OFF                                  0
#define W8753_POWER_MANAGEMENT1_0_DIGEN_ON                                   1

// DACR enable
#define W8753_POWER_MANAGEMENT1_0_DACR_LOC                                   2
#define W8753_POWER_MANAGEMENT1_0_DACR_SEL                                   0x1
#define W8753_POWER_MANAGEMENT1_0_DACR_OFF                                   0
#define W8753_POWER_MANAGEMENT1_0_DACR_ON                                    1

// DACL enable
#define W8753_POWER_MANAGEMENT1_0_DACL_LOC                                   3
#define W8753_POWER_MANAGEMENT1_0_DACL_SEL                                   0x1
#define W8753_POWER_MANAGEMENT1_0_DACL_OFF                                   0
#define W8753_POWER_MANAGEMENT1_0_DACL_ON                                    1

// VDAC enable
#define W8753_POWER_MANAGEMENT1_0_VDAC_LOC                                   4
#define W8753_POWER_MANAGEMENT1_0_VDAC_SEL                                   0x1
#define W8753_POWER_MANAGEMENT1_0_VDAC_OFF                                   0
#define W8753_POWER_MANAGEMENT1_0_VDAC_ON                                    1

// MICB enable
#define W8753_POWER_MANAGEMENT1_0_MICB_LOC                                   5
#define W8753_POWER_MANAGEMENT1_0_MICB_SEL                                   0x1
#define W8753_POWER_MANAGEMENT1_0_MICB_OFF                                   0
#define W8753_POWER_MANAGEMENT1_0_MICB_ON                                    1

// VREF enable
#define W8753_POWER_MANAGEMENT1_0_VREF_LOC                                   6
#define W8753_POWER_MANAGEMENT1_0_VREF_SEL                                   0x1
#define W8753_POWER_MANAGEMENT1_0_VREF_OFF                                   0
#define W8753_POWER_MANAGEMENT1_0_VREF_ON                                    1

// VMID divider enable and select
#define W8753_POWER_MANAGEMENT1_0_VMIDSEL_LOC                                7
#define W8753_POWER_MANAGEMENT1_0_VMIDSEL_SEL                                0x3
#define W8753_POWER_MANAGEMENT1_0_VMIDSEL_DISABLE                            0
#define W8753_POWER_MANAGEMENT1_0_VMIDSEL_DIV_50K                            1
#define W8753_POWER_MANAGEMENT1_0_VMIDSEL_DIV_500K                           2
#define W8753_POWER_MANAGEMENT1_0_VMIDSEL_DIV_5K                             3

// Power managment 2 R15
// LINEMIX
#define W8753_POWER_MANAGEMENT2_0_LINEMIX_LOC                                0
#define W8753_POWER_MANAGEMENT2_0_LINEMIX_SEL                                0x1
#define W8753_POWER_MANAGEMENT2_0_LINEMIX_OFF                                0
#define W8753_POWER_MANAGEMENT2_0_LINEMIX_ON                                 1

// RXMIX
#define W8753_POWER_MANAGEMENT2_0_RXMIX_LOC                                  1
#define W8753_POWER_MANAGEMENT2_0_RXMIX_SEL                                  0x1
#define W8753_POWER_MANAGEMENT2_0_RXMIX_OFF                                  0
#define W8753_POWER_MANAGEMENT2_0_RXMIX_ON                                   1

// ADCR
#define W8753_POWER_MANAGEMENT2_0_ADCR_LOC                                   2
#define W8753_POWER_MANAGEMENT2_0_ADCR_SEL                                   0x1
#define W8753_POWER_MANAGEMENT2_0_ADCR_OFF                                   0
#define W8753_POWER_MANAGEMENT2_0_ADCR_ON                                    1

// ADCL
#define W8753_POWER_MANAGEMENT2_0_ADCL_LOC                                   3
#define W8753_POWER_MANAGEMENT2_0_ADCL_SEL                                   0x1
#define W8753_POWER_MANAGEMENT2_0_ADCL_OFF                                   0
#define W8753_POWER_MANAGEMENT2_0_ADCL_ON                                    1

// PGAR
#define W8753_POWER_MANAGEMENT2_0_PGAR_LOC                                   4
#define W8753_POWER_MANAGEMENT2_0_PGAR_SEL                                   0x1
#define W8753_POWER_MANAGEMENT2_0_PGAR_OFF                                   0
#define W8753_POWER_MANAGEMENT2_0_PGAR_ON                                    1

// PGAL
#define W8753_POWER_MANAGEMENT2_0_PGAL_LOC                                   5
#define W8753_POWER_MANAGEMENT2_0_PGAL_SEL                                   0x1
#define W8753_POWER_MANAGEMENT2_0_PGAL_OFF                                   0
#define W8753_POWER_MANAGEMENT2_0_PGAL_ON                                    1

// ALC mixer
#define W8753_POWER_MANAGEMENT2_0_ALCMIX_LOC                                 6
#define W8753_POWER_MANAGEMENT2_0_ALCMIX_SEL                                 0x1
#define W8753_POWER_MANAGEMENT2_0_ALCMIX_OFF                                 0
#define W8753_POWER_MANAGEMENT2_0_ALCMIX_ON                                  1

// Mic2 preamble
#define W8753_POWER_MANAGEMENT2_0_MICAMP2EN_LOC                              7
#define W8753_POWER_MANAGEMENT2_0_MICAMP2EN_SEL                              0x1
#define W8753_POWER_MANAGEMENT2_0_MICAMP2EN_OFF                              0
#define W8753_POWER_MANAGEMENT2_0_MICAMP2EN_ON                               1

// Mic1 preamble
#define W8753_POWER_MANAGEMENT2_0_MICAMP1EN_LOC                              8
#define W8753_POWER_MANAGEMENT2_0_MICAMP1EN_SEL                              0x1
#define W8753_POWER_MANAGEMENT2_0_MICAMP1EN_OFF                              0
#define W8753_POWER_MANAGEMENT2_0_MICAMP1EN_ON                               1

// Power Management 3 R22
// MONO2
#define W8753_POWER_MANAGEMENT3_0_MONO2_LOC                                  1
#define W8753_POWER_MANAGEMENT3_0_MONO2_SEL                                  0x1
#define W8753_POWER_MANAGEMENT3_0_MONO2_OFF                                  0
#define W8753_POWER_MANAGEMENT3_0_MONO2_ON                                   1

// MONO1
#define W8753_POWER_MANAGEMENT3_0_MONO1_LOC                                  2
#define W8753_POWER_MANAGEMENT3_0_MONO1_SEL                                  0x1
#define W8753_POWER_MANAGEMENT3_0_MONO1_OFF                                  0
#define W8753_POWER_MANAGEMENT3_0_MONO1_ON                                   1
// OUT4
#define W8753_POWER_MANAGEMENT3_0_OUT4_LOC                                   3
#define W8753_POWER_MANAGEMENT3_0_OUT4_SEL                                   0x1
#define W8753_POWER_MANAGEMENT3_0_OUT4_OFF                                   0
#define W8753_POWER_MANAGEMENT3_0_OUT4_ON                                    1
// OUT3
#define W8753_POWER_MANAGEMENT3_0_OUT3_LOC                                   4
#define W8753_POWER_MANAGEMENT3_0_OUT3_SEL                                   0x1
#define W8753_POWER_MANAGEMENT3_0_OUT3_OFF                                   0
#define W8753_POWER_MANAGEMENT3_0_OUT3_ON                                    1

// ROUT2
#define W8753_POWER_MANAGEMENT3_0_ROUT2_LOC                                  5
#define W8753_POWER_MANAGEMENT3_0_ROUT2_SEL                                  0x1
#define W8753_POWER_MANAGEMENT3_0_ROUT2_OFF                                  0
#define W8753_POWER_MANAGEMENT3_0_ROUT2_ON                                   1
// LOUT2
#define W8753_POWER_MANAGEMENT3_0_LOUT2_LOC                                  6
#define W8753_POWER_MANAGEMENT3_0_LOUT2_SEL                                  0x1
#define W8753_POWER_MANAGEMENT3_0_LOUT2_OFF                                  0
#define W8753_POWER_MANAGEMENT3_0_LOUT2_ON                                   1

// ROUT1
#define W8753_POWER_MANAGEMENT3_0_ROUT1_LOC                                  7
#define W8753_POWER_MANAGEMENT3_0_ROUT1_SEL                                  0x1
#define W8753_POWER_MANAGEMENT3_0_ROUT1_OFF                                  0
#define W8753_POWER_MANAGEMENT3_0_ROUT1_ON                                   1

// LOUT1
#define W8753_POWER_MANAGEMENT3_0_LOUT1_LOC                                  8
#define W8753_POWER_MANAGEMENT3_0_LOUT1_SEL                                  0x1
#define W8753_POWER_MANAGEMENT3_0_LOUT1_OFF                                  0
#define W8753_POWER_MANAGEMENT3_0_LOUT1_ON                                   1


// Power managemnet 4 R23
// LEFTMIX
#define W8753_POWER_MANAGEMENT4_0_LEFTMIX_LOC                                0
#define W8753_POWER_MANAGEMENT4_0_LEFTMIX_SEL                                0x1
#define W8753_POWER_MANAGEMENT4_0_LEFTMIX_OFF                                0
#define W8753_POWER_MANAGEMENT4_0_LEFTMIX_ON                                 1

// RIGHTMIX
#define W8753_POWER_MANAGEMENT4_0_RIGHTMIX_LOC                               1
#define W8753_POWER_MANAGEMENT4_0_RIGHTMIX_SEL                               0x1
#define W8753_POWER_MANAGEMENT4_0_RIGHTMIX_OFF                               0
#define W8753_POWER_MANAGEMENT4_0_RIGHTMIX_ON                                1

// MONOMIX
#define W8753_POWER_MANAGEMENT4_0_MONOMIX_LOC                                2
#define W8753_POWER_MANAGEMENT4_0_MONOMIX_SEL                                0x1
#define W8753_POWER_MANAGEMENT4_0_MONOMIX_OFF                                0
#define W8753_POWER_MANAGEMENT4_0_MONOMIX_ON                                 1

// RECMIX
#define W8753_POWER_MANAGEMENT4_0_RECMIX_LOC                                 3
#define W8753_POWER_MANAGEMENT4_0_RECMIX_SEL                                 0x1
#define W8753_POWER_MANAGEMENT4_0_RECMIX_OFF                                 0
#define W8753_POWER_MANAGEMENT4_0_RECMIX_ON                                  1

 ///// Pcm Audio interface R3
 // Adc mode selection
#define W8753_PCM_AUDIO_INTERFACE_0_ADCDOP_LOC                               8
#define W8753_PCM_AUDIO_INTERFACE_0_ADCDOP_SEL                               0x1
#define W8753_PCM_AUDIO_INTERFACE_0_ADCDOP_ADCDAT                            1
#define W8753_PCM_AUDIO_INTERFACE_0_ADCDOP_IFMODE                            0

// VXCLKINV
#define W8753_PCM_AUDIO_INTERFACE_0_VXCLKINV_LOC                             7
#define W8753_PCM_AUDIO_INTERFACE_0_VXCLKINV_SEL                             0x1
#define W8753_PCM_AUDIO_INTERFACE_0_VXCLKINV_ENABLE                          1
#define W8753_PCM_AUDIO_INTERFACE_0_VXCLKINV_DISABLE                         0

// PMS - Voice codec in MasterMode or Slave
#define W8753_PCM_AUDIO_INTERFACE_0_PMS_LOC                                  6
#define W8753_PCM_AUDIO_INTERFACE_0_PMS_SEL                                  0x1
#define W8753_PCM_AUDIO_INTERFACE_0_PMS_MASTERMODE                           1
#define W8753_PCM_AUDIO_INTERFACE_0_PMS_SLAVEMODE                            0

// MONO
#define W8753_PCM_AUDIO_INTERFACE_0_MONO_LOC                                 5
#define W8753_PCM_AUDIO_INTERFACE_0_MONO_SEL                                 0x1
#define W8753_PCM_AUDIO_INTERFACE_0_MONO_LEFTONLYDATA                        1
#define W8753_PCM_AUDIO_INTERFACE_0_MONO_LEFTRIGHTDATA                       0

// PLRP
#define W8753_PCM_AUDIO_INTERFACE_0_PLRP_LOC                                 4
#define W8753_PCM_AUDIO_INTERFACE_0_PLRP_SEL                                 0x1
#define W8753_PCM_AUDIO_INTERFACE_0_PLRP_VXCLKINV                            1
#define W8753_PCM_AUDIO_INTERFACE_0_PLRP_VXCLKNORMAL                         0
#define W8753_PCM_AUDIO_INTERFACE_0_PLRP_DSPMODEB                            1
#define W8753_PCM_AUDIO_INTERFACE_0_PLRP_DSPMODEA                            0

// PWL
#define W8753_PCM_AUDIO_INTERFACE_0_PWL_LOC                                  2
#define W8753_PCM_AUDIO_INTERFACE_0_PWL_SEL                                  0x3
#define W8753_PCM_AUDIO_INTERFACE_0_PWL_32BITS                               3
#define W8753_PCM_AUDIO_INTERFACE_0_PWL_24BITS                               2
#define W8753_PCM_AUDIO_INTERFACE_0_PWL_20BITS                               1
#define W8753_PCM_AUDIO_INTERFACE_0_PWL_16BITS                               0

// PFORMAT
#define W8753_PCM_AUDIO_INTERFACE_0_PFORMAT_LOC                              0
#define W8753_PCM_AUDIO_INTERFACE_0_PFORMAT_SEL                              0x3
#define W8753_PCM_AUDIO_INTERFACE_0_PFORMAT_DSP_MODE                         3
#define W8753_PCM_AUDIO_INTERFACE_0_PFORMAT_I2S_MODE                         2
#define W8753_PCM_AUDIO_INTERFACE_0_PFORMAT_LEFT_JUSTIFIED                   1
#define W8753_PCM_AUDIO_INTERFACE_0_PFORMAT_RIGHT_JUSTIFIED                  0



//// Digital interface R4
// HiFi DAC Audio data format select
#define W8753_DIGITAL_HIFIINTERFACE_0_FORMAT_LOC                            0
#define W8753_DIGITAL_HIFIINTERFACE_0_FORMAT_SEL                            0x3
#define W8753_DIGITAL_HIFIINTERFACE_0_FORMAT_RIGHT_JUSTIFIED                0
#define W8753_DIGITAL_HIFIINTERFACE_0_FORMAT_LEFT_JUSTIFIED                 1
#define W8753_DIGITAL_HIFIINTERFACE_0_FORMAT_I2S_MODE                       2
#define W8753_DIGITAL_HIFIINTERFACE_0_FORMAT_DSP_MODE                       3

// HiFi Dac audio data word length
#define W8753_DIGITAL_HIFIINTERFACE_0_WL_LOC                                2
#define W8753_DIGITAL_HIFIINTERFACE_0_WL_SEL                                0x3
#define W8753_DIGITAL_HIFIINTERFACE_0_WL_16BITS                             0
#define W8753_DIGITAL_HIFIINTERFACE_0_WL_20BITS                             1
#define W8753_DIGITAL_HIFIINTERFACE_0_WL_24BITS                             2
#define W8753_DIGITAL_HIFIINTERFACE_0_WL_32BITS                             3

// LRP HiFI Dac rigth, left and i2s modes - LRC polarity
// 1 Right channel DAC data when DACLRC high
// 0 Right channel DAC data when DACLRC low
#define W8753_DIGITAL_HIFIINTERFACE_0_LRP_LOC                               4
#define W8753_DIGITAL_HIFIINTERFACE_0_LRP_SEL                               0x1
#define W8753_DIGITAL_HIFIINTERFACE_0_LRP_LEFT_DACLRC_LOW                   0
#define W8753_DIGITAL_HIFIINTERFACE_0_LRP_LEFT_DACLRC_HIGH                  1

// HiFi DAC Left/Rigth channel swap
// 1 Right channel DAC data left
// 0 Right channel DAC data right
#define W8753_DIGITAL_HIFIINTERFACE_0_LRSWAP_LOC                            5
#define W8753_DIGITAL_HIFIINTERFACE_0_LRSWAP_SEL                            0x1
#define W8753_DIGITAL_HIFIINTERFACE_0_LRSWAP_RIGHT_DATA_DAC_RIGHT           0
#define W8753_DIGITAL_HIFIINTERFACE_0_LRSWAP_RIGHT_DATA_DAC_LEFT            1

// HiFi interface master/slave mode control
#define W8753_DIGITAL_HIFIINTERFACE_0_MS_LOC                                6
#define W8753_DIGITAL_HIFIINTERFACE_0_MS_SEL                                0x1
#define W8753_DIGITAL_HIFIINTERFACE_0_MS_MASTER_MODE                        1
#define W8753_DIGITAL_HIFIINTERFACE_0_MS_SLAVE_MODE                         0

// HiFi DAC BCLK invert bit (for master and slave mode)
#define W8753_DIGITAL_HIFIINTERFACE_0_BCLKINV_LOC                           7
#define W8753_DIGITAL_HIFIINTERFACE_0_BCLKINV_SEL                           0x1
#define W8753_DIGITAL_HIFIINTERFACE_0_BCLKINV_ENABLE                        1
#define W8753_DIGITAL_HIFIINTERFACE_0_BCLKINV_DISABLE                       0

///// Interface control R5
// VXCLKTRI tristate enable/disable
#define W8753_INTERFACE_CONTROL_0_VXCLKTRI_LOC                              7
#define W8753_INTERFACE_CONTROL_0_VXCLKTRI_SEL                              0x1
#define W8753_INTERFACE_CONTROL_0_VXCLKTRI_TRISTATE                         0x1
#define W8753_INTERFACE_CONTROL_0_VXCLKTRI_NORMAL                           0x0

// ADC tristate enable/disable
#define W8753_INTERFACE_CONTROL_0_ADCDTRI_LOC                              4
#define W8753_INTERFACE_CONTROL_0_ADCDTRI_SEL                              0x1
#define W8753_INTERFACE_CONTROL_0_ADCDTRI_TRISTATE                         0x1
#define W8753_INTERFACE_CONTROL_0_ADCDTRI_NORMAL                           0x0

// VX tristate enable/disable
#define W8753_INTERFACE_CONTROL_0_VXDTRI_LOC                               5
#define W8753_INTERFACE_CONTROL_0_VXDTRI_SEL                               0x1
#define W8753_INTERFACE_CONTROL_0_VXDTRI_TRISTATE                          0x1
#define W8753_INTERFACE_CONTROL_0_VXDTRI_NORMAL                            0x0

// If mode
#define W8753_INTERFACE_CONTROL_0_IFMODE_LOC                                2
#define W8753_INTERFACE_CONTROL_0_IFMODE_SEL                                0x3
#define W8753_INTERFACE_CONTROL_0_IFMODE_VOICE_HIFI                         0x0
#define W8753_INTERFACE_CONTROL_0_IFMODE_VOICECODEC                         0x1
#define W8753_INTERFACE_CONTROL_0_IFMODE_HIFI                               0x2
#define W8753_INTERFACE_CONTROL_0_IFMODE_HIFI_VXFS                          0x3

//LRC pin mode
#define W8753_INTERFACE_CONTROL_0_LRCOE_LOC                                0
#define W8753_INTERFACE_CONTROL_0_LRCOE_SEL                                0x1
#define W8753_INTERFACE_CONTROL_0_LRCOE_INPUT                              0x0
#define W8753_INTERFACE_CONTROL_0_LRCOE_OUTPUT                             0x1

//VXFSOE pin mode
#define W8753_INTERFACE_CONTROL_0_VXFSOE_LOC                                1
#define W8753_INTERFACE_CONTROL_0_VXFSOE_SEL                                0x1
#define W8753_INTERFACE_CONTROL_0_VXFSOE_INPUT                              0x0
#define W8753_INTERFACE_CONTROL_0_VXFSOE_OUTPUT                             0x1

///// Reset register R31
#define W8753_RESET_0_RESET_LOC                                             0
#define W8753_RESET_0_RESET_SEL                                             0xFF
#define W8753_RESET_0_RESET_VAL                                             0X00

////// Clock Control (R52)
// PCM clock dividor
#define W8753_CLOCK_CONTROL_0_PCMDIV_LOC                                    6
#define W8753_CLOCK_CONTROL_0_PCMDIV_SEL                                   0x7
#define W8753_CLOCK_CONTROL_0_PCMDIV_DISABLE                               0
#define W8753_CLOCK_CONTROL_0_PCMDIV_UNUSED                                1
#define W8753_CLOCK_CONTROL_0_PCMDIV_DIV3                                  2
#define W8753_CLOCK_CONTROL_0_PCMDIV_DIV5_5                                3
#define W8753_CLOCK_CONTROL_0_PCMDIV_DIV2                                  4
#define W8753_CLOCK_CONTROL_0_PCMDIV_DIV4                                  5
#define W8753_CLOCK_CONTROL_0_PCMDIV_DIV6                                  6
#define W8753_CLOCK_CONTROL_0_PCMDIV_DIV8                                  7

// Select internal master clock for HIFI codec
#define W8753_CLOCK_CONTROL_0_MCLKSEL_LOC                                   4
#define W8753_CLOCK_CONTROL_0_MCLKSEL_SEL                                   0x1
#define W8753_CLOCK_CONTROL_0_MCLKSEL_MCLK_PIN                              0
#define W8753_CLOCK_CONTROL_0_MCLKSEL_PLL1                                  1

// Select internal master clock for voice codec
#define W8753_CLOCK_CONTROL_0_PCMCLKSEL_LOC                                 3
#define W8753_CLOCK_CONTROL_0_PCMCLKSEL_SEL                                 0x1
#define W8753_CLOCK_CONTROL_0_PCMCLKSEL_PCMCLK_PIN                          0
#define W8753_CLOCK_CONTROL_0_PCMCLKSEL_PLL2                                1

// Select clock for voice codec
#define W8753_CLOCK_CONTROL_0_CLKEQ_LOC                                     2
#define W8753_CLOCK_CONTROL_0_CLKEQ_SEL                                     0x1
#define W8753_CLOCK_CONTROL_0_CLKEQ_PCMCLK_PLL2                             0
#define W8753_CLOCK_CONTROL_0_CLKEQ_MCLK_PLL1                               1

// Select GP1CLK1
#define W8753_CLOCK_CONTROL_0_GP1CLK1SEL_LOC                                1
#define W8753_CLOCK_CONTROL_0_GP1CLK1SEL_SEL                                0x1
#define W8753_CLOCK_CONTROL_0_GP1CLK1SEL_GP1_OUT                            0
#define W8753_CLOCK_CONTROL_0_GP1CLK1SEL_CLK1_OUT                           1

// Select GP2CLK2
#define W8753_CLOCK_CONTROL_0_GP2CLK2SEL_LOC                                0
#define W8753_CLOCK_CONTROL_0_GP2CLK2SEL_SEL                                0x1
#define W8753_CLOCK_CONTROL_0_GP2CLK2SEL_GP2_OUT                            0
#define W8753_CLOCK_CONTROL_0_GP2CLK2SEL_CLK2_OUT                           1

///// PLL1 controls R53/54/55/56
// PLL1 Control
#define W8753_PLL1_CONTROL1_0_PLL1EN_LOC                                    0
#define W8753_PLL1_CONTROL1_0_PLL1EN_SEL                                    0x1
#define W8753_PLL1_CONTROL1_0_PLL1EN_ENABLE                                 1
#define W8753_PLL1_CONTROL1_0_PLL1EN_DISABLE                                0

#define W8753_PLL1_CONTROL1_0_PLL1RB_LOC                                    1
#define W8753_PLL1_CONTROL1_0_PLL1RB_SEL                                    0x1
#define W8753_PLL1_CONTROL1_0_PLL1RB_RESET                                  0
#define W8753_PLL1_CONTROL1_0_PLL1RB_ACTIVE                                 1

#define W8753_PLL1_CONTROL1_0_PLL1DIV2_LOC                                  2
#define W8753_PLL1_CONTROL1_0_PLL1DIV2_SEL                                  0x1
#define W8753_PLL1_CONTROL1_0_PLL1DIV2_ENABLE                               1
#define W8753_PLL1_CONTROL1_0_PLL1DIV2_DISABLE                              0

#define W8753_PLL1_CONTROL1_0_MCLK1DIV2_LOC                                 3
#define W8753_PLL1_CONTROL1_0_MCLK1DIV2_SEL                                 0x1
#define W8753_PLL1_CONTROL1_0_MCLK1DIV2_ENABLE                              1
#define W8753_PLL1_CONTROL1_0_MCLK1DIV2_DISABLE                             0

#define W8753_PLL1_CONTROL1_0_CLK1DIV2_LOC                                  4
#define W8753_PLL1_CONTROL1_0_CLK1DIV2_SEL                                  0x1
#define W8753_PLL1_CONTROL1_0_CLK1DIV2_ENABLE                               1
#define W8753_PLL1_CONTROL1_0_CLK1DIV2_DISABLE                              0

// Clock out select 1
#define W8753_PLL1_CONTROL1_0_CLK1SEL_LOC                                   5
#define W8753_PLL1_CONTROL1_0_CLK1SEL_SEL                                   0x1
#define W8753_PLL1_CONTROL1_0_CLK1SEL_MCLK                                  0
#define W8753_PLL1_CONTROL1_0_CLK1SEL_PLL1BLE                               1

 // PLL1 COntrol2 R54

// PLL1K[21:18]
#define W8753_PLL1_CONTROL2_0_PLL1K_LOC                                     0
#define W8753_PLL1_CONTROL2_0_PLL1K_SEL                                     0xF
#define W8753_PLL1_CONTROL2_0_PLL1K_13MHZ_MCLK                              8

// Integer N part of PLL1 input/output freq ratio.
#define W8753_PLL1_CONTROL2_0_PLL1N_LOC                                     5
#define W8753_PLL1_CONTROL2_0_PLL1N_SEL                                     0xF
#define W8753_PLL1_CONTROL2_0_PLL1N_13MHZ_MCLK                              7

// PLL1K[17:9]
#define W8753_PLL1_CONTROL3_0_PLL1K_LOC                                     0
#define W8753_PLL1_CONTROL3_0_PLL1K_SEL                                     0x1FF
#define W8753_PLL1_CONTROL3_0_PLL1K_13MHZ_MCLK                              0x1fa

// PLL1K[8:0]
#define W8753_PLL1_CONTROL4_0_PLL1K_LOC                                     0
#define W8753_PLL1_CONTROL4_0_PLL1K_SEL                                     0x1FF
#define W8753_PLL1_CONTROL4_0_PLL1K_13MHZ_MCLK                              0x14a


///// PLL2 controls R57/58/59/60

// PLL2 Control
#define W8753_PLL2_CONTROL1_0_PLL2EN_LOC                                    0
#define W8753_PLL2_CONTROL1_0_PLL2EN_SEL                                    0x1
#define W8753_PLL2_CONTROL1_0_PLL2EN_ENABLE                                 1
#define W8753_PLL2_CONTROL1_0_PLL2EN_DISABLE                                0

#define W8753_PLL2_CONTROL1_0_PLL2RB_LOC                                    1
#define W8753_PLL2_CONTROL1_0_PLL2RB_SEL                                    0x1
#define W8753_PLL2_CONTROL1_0_PLL2RB_RESET                                  0
#define W8753_PLL2_CONTROL1_0_PLL2RB_ACTIVE                                 1

#define W8753_PLL2_CONTROL1_0_PLL2DIV2_LOC                                  2
#define W8753_PLL2_CONTROL1_0_PLL2DIV2_SEL                                  0x1
#define W8753_PLL2_CONTROL1_0_PLL2DIV2_ENABLE                               1
#define W8753_PLL2_CONTROL1_0_PLL2DIV2_DISABLE                              0

#define W8753_PLL2_CONTROL1_0_MCLK2DIV2_LOC                                 3
#define W8753_PLL2_CONTROL1_0_MCLK2DIV2_SEL                                 0x1
#define W8753_PLL2_CONTROL1_0_MCLK2DIV2_ENABLE                              1
#define W8753_PLL2_CONTROL1_0_MCLK2DIV2_DISABLE                             0

#define W8753_PLL2_CONTROL1_0_CLK2DIV2_LOC                                  4
#define W8753_PLL2_CONTROL1_0_CLK2DIV2_SEL                                  0x1
#define W8753_PLL2_CONTROL1_0_CLK2DIV2_ENABLE                               1
#define W8753_PLL2_CONTROL1_0_CLK2DIV2_DISABLE                              0

// Clock out select 1
#define W8753_PLL2_CONTROL1_0_CLK2SEL_LOC                                   5
#define W8753_PLL2_CONTROL1_0_CLK2SEL_SEL                                   0x1
#define W8753_PLL2_CONTROL1_0_CLK2SEL_MCLK                                  0
#define W8753_PLL2_CONTROL1_0_CLK2SEL_PLL2BLE                               1

// PLL2 Control 2 R58
// PLL2K[21:18]
#define W8753_PLL2_CONTROL2_0_PLL2K_LOC                                     0
#define W8753_PLL2_CONTROL2_0_PLL2K_SEL                                     0xF

// Integer N part of PLL2 input/output freq ratio.
#define W8753_PLL2_CONTROL2_0_PLL2N_LOC                                     5
#define W8753_PLL2_CONTROL2_0_PLL2N_SEL                                     0xF


// PLL2 Control 3 R59
// PLL2K[17:9]
#define W8753_PLL2_CONTROL3_0_PLL2K_LOC                                     0
#define W8753_PLL2_CONTROL3_0_PLL2K_SEL                                     0x1FF

// PLL2 Control 4 R60
// PLL2K[8:0]
#define W8753_PLL2_CONTROL4_0_PLL2K_LOC                                     0
#define W8753_PLL2_CONTROL4_0_PLL2K_SEL                                     0x1FF


// GPIO control 1 and 2 (R27 and R28)
// Interrupt Control
#define W8753_GPIO_CONTROL1_0_INTCON_LOC                                 7
#define W8753_GPIO_CONTROL1_0_INTCON_SEL                                 0x3
#define W8753_GPIO_CONTROL1_0_INTCON_DISABLE                             0
#define W8753_GPIO_CONTROL1_0_INTCON_DRAIN_LOW                           1
#define W8753_GPIO_CONTROL1_0_INTCON_ACTIVE_HIGH                         2
#define W8753_GPIO_CONTROL1_0_INTCON_ACTIVE_LOW                          3

// GPIO 5 Control
#define W8753_GPIO_CONTROL1_0_GPIO5M_LOC                                    3
#define W8753_GPIO_CONTROL1_0_GPIO5M_SEL                                    0x3
#define W8753_GPIO_CONTROL1_0_GPIO5M_INPUT                                  0
#define W8753_GPIO_CONTROL1_0_GPIO5M_INPUT_INT                              1
#define W8753_GPIO_CONTROL1_0_GPIO5M_DRIVE_LOW                              2
#define W8753_GPIO_CONTROL1_0_GPIO5M_DRIVE_HIGH                             3

// GPIO 4 Control
#define W8753_GPIO_CONTROL1_0_GPIO4M_LOC                                    0
#define W8753_GPIO_CONTROL1_0_GPIO4M_SEL                                    0x7
#define W8753_GPIO_CONTROL1_0_GPIO4M_INPUT                                  0
#define W8753_GPIO_CONTROL1_0_GPIO4M_UNUSED                                 1
#define W8753_GPIO_CONTROL1_0_GPIO4M_INPUT_PULL_DOWN                        2
#define W8753_GPIO_CONTROL1_0_GPIO4M_INPUT_PULL_UP                          3
#define W8753_GPIO_CONTROL1_0_GPIO4M_DRIVE_LOW                              4
#define W8753_GPIO_CONTROL1_0_GPIO4M_DRIVE_HIGH                             5
#define W8753_GPIO_CONTROL1_0_GPIO4M_SDOUT                                  6
#define W8753_GPIO_CONTROL1_0_GPIO4M_INTERRUPT                              7

// GPIO Control 2 register  R28
// GPIO 3Control
#define W8753_GPIO_CONTROL2_0_GPIO3M_LOC                                    6
#define W8753_GPIO_CONTROL2_0_GPIO3M_SEL                                    0x7
#define W8753_GPIO_CONTROL2_0_GPIO3M_INPUT                                  0
#define W8753_GPIO_CONTROL2_0_GPIO3M_DRIVE_LOW                              4
#define W8753_GPIO_CONTROL2_0_GPIO3M_DRIVE_HIGH                             5
#define W8753_GPIO_CONTROL2_0_GPIO3M_SDOUT                                  6
#define W8753_GPIO_CONTROL2_0_GPIO3M_INTERRUPT                              7

// GPIO 2 control
#define W8753_GPIO_CONTROL2_0_GPIO2M_LOC                                    3
#define W8753_GPIO_CONTROL2_0_GPIO2M_SEL                                    0x7
#define W8753_GPIO_CONTROL2_0_GPIO2M_DRIVE_LOW                              0
#define W8753_GPIO_CONTROL2_0_GPIO2M_DRIVE_HIGH                             1
#define W8753_GPIO_CONTROL2_0_GPIO2M_SDOUT                                  2
#define W8753_GPIO_CONTROL2_0_GPIO2M_INTERRUPT                              3
#define W8753_GPIO_CONTROL2_0_GPIO2M_ADC_CLK                                4
#define W8753_GPIO_CONTROL2_0_GPIO2M_DAC_CLK                                5
#define W8753_GPIO_CONTROL2_0_GPIO2M_ADC_CLK_DIV2                           6
#define W8753_GPIO_CONTROL2_0_GPIO2M_DAC_CLK_DIV2                           7

// GPIO 1 Control
#define W8753_GPIO_CONTROL2_0_GPIO1M_LOC                                    0
#define W8753_GPIO_CONTROL2_0_GPIO1M_SEL                                    0x7
#define W8753_GPIO_CONTROL2_0_GPIO1M_DRIVE_LOW                              0
#define W8753_GPIO_CONTROL2_0_GPIO1M_DRIVE_HIGH                             1
#define W8753_GPIO_CONTROL2_0_GPIO1M_SDOUT                                  2
#define W8753_GPIO_CONTROL2_0_GPIO1M_INTERRUPT                              3


///// Read Control (R24)
#define W8753_READ_CONTROL_0_READEN_LOC                                     0
#define W8753_READ_CONTROL_0_READEN_SEL                                     0x1
#define W8753_READ_CONTROL_0_READEN_ENABLE                                  0x1
#define W8753_READ_CONTROL_0_READEN_DISABLE                                 0x0

#define W8753_READ_CONTROL_0_READSEL_LOC                                    1
#define W8753_READ_CONTROL_0_READSEL_SEL                                    0x7
#define W8753_READ_CONTROL_0_READSEL_DEVID_HI                               0
#define W8753_READ_CONTROL_0_READSEL_DEVID_LO                               1
#define W8753_READ_CONTROL_0_READSEL_DEV_REV                                2
#define W8753_READ_CONTROL_0_READSEL_DEV_CAP                                3
#define W8753_READ_CONTROL_0_READSEL_DEV_STATUS                             4
#define W8753_READ_CONTROL_0_READSEL_INT_STATUS                             5

// Select the adc data for the read back or not. 0 will not select.
#define W8753_READ_CONTROL_0_RDDAT_LOC                                      4
#define W8753_READ_CONTROL_0_RDDAT_SEL                                      0x1
#define W8753_READ_CONTROL_0_RDDAT_ENABLE                                   1
#define W8753_READ_CONTROL_0_RDDAT_DISABLE                                  0x0


////// Sampling rate control R6
// USB/normal mode
// USB mode (250/272 fs)
// Normal mode (256/384 fs)
#define W8753_SAMPLING_CONTROL1_0_USB_NORM_MODE_LOC                          0
#define W8753_SAMPLING_CONTROL1_0_USB_NORM_MODE_SEL                          0x1
#define W8753_SAMPLING_CONTROL1_0_USB_NORM_MODE_USB                          1
#define W8753_SAMPLING_CONTROL1_0_USB_NORM_MODE_NORM                         0

// Sample rate control
#define W8753_SAMPLING_CONTROL1_0_SR_LOC                                     1
#define W8753_SAMPLING_CONTROL1_0_SR_SEL                                     0x1F

// COMMON for Normal and USB mode - with ADC and DAC on same samplerate
// MCLK 12.2888 KHZ
#define W8753_SAMPLING_CONTROL1_0_SR_ADC_8_DAC_8                            0x06
#define W8753_SAMPLING_CONTROL1_0_SR_ADC_8_DAC_48                           0x04
#define W8753_SAMPLING_CONTROL1_0_SR_ADC_12_DAC_12                          0x08
#define W8753_SAMPLING_CONTROL1_0_SR_ADC_16_DAC_16                          0x0A
#define W8753_SAMPLING_CONTROL1_0_SR_ADC_24_DAC_24                          0x1C
#define W8753_SAMPLING_CONTROL1_0_SR_ADC_32_DAC_32                          0x0C
#define W8753_SAMPLING_CONTROL1_0_SR_ADC_48_DAC_48                          0x0
#define W8753_SAMPLING_CONTROL1_0_SR_ADC_48_DAC_8                           0x2
#define W8753_SAMPLING_CONTROL1_0_SR_ADC_96_DAC_96                          0x0E
// MCLK 11.2896 KHZ
#define W8753_SAMPLING_CONTROL1_0_SR_NORMAL_ADC_8_01_DAC_8_01               0x16
#define W8753_SAMPLING_CONTROL1_0_SR_NORMAL_ADC_8_01_DAC_44_1               0x14
#define W8753_SAMPLING_CONTROL1_0_SR_NORMAL_ADC_11_025_DAC_11_025           0x18
#define W8753_SAMPLING_CONTROL1_0_SR_NORMAL_ADC_22_05_DAC_22_05             0x1A
#define W8753_SAMPLING_CONTROL1_0_SR_NORMAL_ADC_44_1_DAC_44_1               0x10
#define W8753_SAMPLING_CONTROL1_0_SR_NORMAL_ADC_44_1_DAC_8_01               0x12
#define W8753_SAMPLING_CONTROL1_0_SR_NORMAL_ADC_88_2_DAC_88_2               0x1E

// USB Mode
#define W8753_SAMPLING_CONTROL1_0_SR_USB_ADC_8_01_DAC_8_01                  0x17
#define W8753_SAMPLING_CONTROL1_0_SR_USB_ADC_11_025_DAC_11_025              0x19
#define W8753_SAMPLING_CONTROL1_0_SR_USB_ADC_22_05_DAC_22_05                0x1B
#define W8753_SAMPLING_CONTROL1_0_SR_USB_ADC_44_1_DAC_44_1                  0x11
#define W8753_SAMPLING_CONTROL1_0_SR_USB_ADC_88_2_DAC_88_2                  0x1F

// Voice codec sample rate control
#define W8753_SAMPLING_CONTROL1_0_PSR_LOC                                    7
#define W8753_SAMPLING_CONTROL1_0_PSR_SEL                                    0x1
#define W8753_SAMPLING_CONTROL1_0_PSR_FS256                                  0
#define W8753_SAMPLING_CONTROL1_0_PSR_FS384                                  1

// ADC sample rate
#define W8753_SAMPLING_CONTROL1_0_SRMODE_LOC                                 8
#define W8753_SAMPLING_CONTROL1_0_SRMODE_SEL                                 0x1
#define W8753_SAMPLING_CONTROL1_0_SRMODE_ADC_SAMP_SR_USB                     0
#define W8753_SAMPLING_CONTROL1_0_SRMODE_ADC_SAMP_PSR                        1

// Sample Control 2 reg 7
#define W8753_SAMPLING_CONTROL2_0_BMODE_LOC                                 3
#define W8753_SAMPLING_CONTROL2_0_BMODE_SEL                                 0x7
#define W8753_SAMPLING_CONTROL2_0_BMODE_DIV1                                0
#define W8753_SAMPLING_CONTROL2_0_BMODE_DIV2                                1
#define W8753_SAMPLING_CONTROL2_0_BMODE_DIV4                                2
#define W8753_SAMPLING_CONTROL2_0_BMODE_DIV8                                3
#define W8753_SAMPLING_CONTROL2_0_BMODE_DIV16                               4

#define W8753_SAMPLING_CONTROL2_0_PBMODE_LOC                                 6
#define W8753_SAMPLING_CONTROL2_0_PBMODE_SEL                                 0x7
#define W8753_SAMPLING_CONTROL2_0_PBMODE_DIV1                                0
#define W8753_SAMPLING_CONTROL2_0_PBMODE_DIV2                                1
#define W8753_SAMPLING_CONTROL2_0_PBMODE_DIV4                                2
#define W8753_SAMPLING_CONTROL2_0_PBMODE_DIV8                                3
#define W8753_SAMPLING_CONTROL2_0_PBMODE_DIV16                               4

#define W8753_SAMPLING_CONTROL2_0_VXDOSR_LOC                                 2
#define W8753_SAMPLING_CONTROL2_0_VXDOSR_SEL                                 0x1
#define W8753_SAMPLING_CONTROL2_0_VXDOSR_128X                                0
#define W8753_SAMPLING_CONTROL2_0_VXDOSR_64X                                 1

#define W8753_SAMPLING_CONTROL2_0_ADCOSR_LOC                                 1
#define W8753_SAMPLING_CONTROL2_0_ADCOSR_SEL                                 0x1
#define W8753_SAMPLING_CONTROL2_0_ADCOSR_128X                                0
#define W8753_SAMPLING_CONTROL2_0_ADCOSR_64X                                 1

#define W8753_SAMPLING_CONTROL2_0_DACOSR_LOC                                 0
#define W8753_SAMPLING_CONTROL2_0_DACOSR_SEL                                 0x1
#define W8753_SAMPLING_CONTROL2_0_DACOSR_128X                                0
#define W8753_SAMPLING_CONTROL2_0_DACOSR_64X                                 1

#define W8753_INT_POLARITY_0_HPSWIPOL_LOC                              6
#define W8753_INT_POLARITY_0_HPSWIPOL_SEL                              0x1
#define W8753_INT_POLARITY_0_HPSWIPOL_HPCONNECT                        0
#define W8753_INT_POLARITY_0_HPSWIPOL_HPDISCONNECT                     1

#define W8753_INT_POLARITY_0_GPIO4IPOL_LOC                             4
#define W8753_INT_POLARITY_0_GPIO4IPOL_SEL                             0x1
#define W8753_INT_POLARITY_0_GPIO4IPOL_INT_HIGH                        0
#define W8753_INT_POLARITY_0_GPIO4IPOL_INT_LOW                         1

#define W8753_INT_POLARITY_0_MICDETPOL_LOC                              1
#define W8753_INT_POLARITY_0_MICDETPOL_SEL                              0x1
#define W8753_INT_POLARITY_0_MICDETPOL_ABOVETHRESHOLD                   0
#define W8753_INT_POLARITY_0_MICDETPOL_BELOWTHRESHOLD                   1

#define W8753_INT_POLARITY_0_MICSHTPOL_LOC                              0
#define W8753_INT_POLARITY_0_MICSHTPOL_SEL                              0x1
#define W8753_INT_POLARITY_0_MICSHTPOL_BIASOVER                         0
#define W8753_INT_POLARITY_0_MICSHTPOL_BIASNORMAL                       1

#define W8753_INT_MASK_0_HPSWIEN_LOC                               6
#define W8753_INT_MASK_0_HPSWIEN_SEL                               0x1
#define W8753_INT_MASK_0_HPSWIEN_DISABLE                           0
#define W8753_INT_MASK_0_HPSWIEN_ENABLE                            1

#define W8753_INT_MASK_0_GPIO4IEN_LOC                               4
#define W8753_INT_MASK_0_GPIO4IEN_SEL                               0x1
#define W8753_INT_MASK_0_GPIO4IEN_DISABLE                           0
#define W8753_INT_MASK_0_GPIO4IEN_ENABLE                            1

#define W8753_INT_MASK_0_MICDETEN_LOC                              1
#define W8753_INT_MASK_0_MICDETEN_SEL                              0x1
#define W8753_INT_MASK_0_MICDETEN_DISABLE                          0
#define W8753_INT_MASK_0_MICDETEN_ENABLE                           1

#define W8753_INT_MASK_0_MICSHTEN_LOC                              0
#define W8753_INT_MASK_0_MICSHTEN_SEL                              0x1
#define W8753_INT_MASK_0_MICSHTEN_DISABLE                          0
#define W8753_INT_MASK_0_MICSHTEN_ENABLE                           1

#define W8753_ADDCTRL_0_TOEN_LOC                                   0
#define W8753_ADDCTRL_0_TOEN_SEL                                   0x1
#define W8753_ADDCTRL_0_TOEN_DISABLE                               0
#define W8753_ADDCTRL_0_TOEN_ENABLE                                1

#define W8753_OUTPUT_CONTROL_0_HPSWEN_LOC                          6
#define W8753_OUTPUT_CONTROL_0_HPSWEN_SEL                          0x1
#define W8753_OUTPUT_CONTROL_0_HPSWEN_DISABLE                      0
#define W8753_OUTPUT_CONTROL_0_HPSWEN_ENABLE                       1

#define W8753_OUTPUT_CONTROL_0_HPSWPOL_LOC                         5
#define W8753_OUTPUT_CONTROL_0_HPSWPOL_SEL                         0x1
#define W8753_OUTPUT_CONTROL_0_HPSWPOL_HIGHHP                      0
#define W8753_OUTPUT_CONTROL_0_HPSWPOL_HIGHSPKR                    1

#define W8753_OUTPUT_CONTROL_0_ROUT2INV_LOC                        2
#define W8753_OUTPUT_CONTROL_0_ROUT2INV_SEL                        0x1
#define W8753_OUTPUT_CONTROL_0_ROUT2INV_DISABLE                    0
#define W8753_OUTPUT_CONTROL_0_ROUT2INV_ENABLE                     1

/*************************************************************************/
#endif  // INCLUDED_WOLFSON8753_REGISTERS_H
