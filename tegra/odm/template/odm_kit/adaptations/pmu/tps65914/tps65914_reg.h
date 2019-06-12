/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef TPS65914_REG_HEADER
#define TPS65914_REG_HEADER

#if defined(__cplusplus)
extern "C"
{
#endif

/* TPS65914 registers */

#define TPS65914_RFF_INVALID                    0xFF

/* smps registers */
#define TPS65914_SMPS_BASE                       0x20
#define TPS65914_SMPS12_CTRL                     TPS65914_SMPS_BASE + 0x00
#define TPS65914_SMPS12_TSTEP                    TPS65914_SMPS_BASE + 0x01
#define TPS65914_SMPS12_FORCE                    TPS65914_SMPS_BASE + 0x02
#define TPS65914_SMPS12_VOLTAGE                  TPS65914_SMPS_BASE + 0x03

#define TPS65914_SMPS3_CTRL                      TPS65914_SMPS_BASE + 0x04
#define TPS65914_SMPS3_TSTEP                     TPS65914_SMPS_BASE + 0x05
#define TPS65914_SMPS3_FORCE                     TPS65914_SMPS_BASE + 0x06
#define TPS65914_SMPS3_VOLTAGE                   TPS65914_SMPS_BASE + 0x07

#define TPS65914_SMPS45_CTRL                     TPS65914_SMPS_BASE + 0x08
#define TPS65914_SMPS45_TSTEP                    TPS65914_SMPS_BASE + 0x09
#define TPS65914_SMPS45_FORCE                    TPS65914_SMPS_BASE + 0x0A
#define TPS65914_SMPS45_VOLTAGE                  TPS65914_SMPS_BASE + 0x0B

#define TPS65914_SMPS6_CTRL                      TPS65914_SMPS_BASE + 0x0C
#define TPS65914_SMPS6_TSTEP                     TPS65914_SMPS_BASE + 0x0D
#define TPS65914_SMPS6_FORCE                     TPS65914_SMPS_BASE + 0x0E
#define TPS65914_SMPS6_VOLTAGE                   TPS65914_SMPS_BASE + 0x0F

#define TPS65914_SMPS7_CTRL                      TPS65914_SMPS_BASE + 0x10
#define TPS65914_SMPS7_TSTEP                     TPS65914_SMPS_BASE + 0x11
#define TPS65914_SMPS7_FORCE                     TPS65914_SMPS_BASE + 0x12
#define TPS65914_SMPS7_VOLTAGE                   TPS65914_SMPS_BASE + 0x13

#define TPS65914_SMPS8_CTRL                      TPS65914_SMPS_BASE + 0x14
#define TPS65914_SMPS8_TSTEP                     TPS65914_SMPS_BASE + 0x15
#define TPS65914_SMPS8_FORCE                     TPS65914_SMPS_BASE + 0x16
#define TPS65914_SMPS8_VOLTAGE                   TPS65914_SMPS_BASE + 0x17

#define TPS65914_SMPS9_CTRL                      TPS65914_SMPS_BASE + 0x18
#define TPS65914_SMPS9_TSTEP                     TPS65914_SMPS_BASE + 0x19
#define TPS65914_SMPS9_FORCE                     TPS65914_SMPS_BASE + 0x1A
#define TPS65914_SMPS9_VOLTAGE                   TPS65914_SMPS_BASE + 0x1B

#define TPS65914_SMPS10_CTRL                     TPS65914_SMPS_BASE + 0x1C
#define TPS65914_SMPS10_STATUS                   TPS65914_SMPS_BASE + 0x1F

#define TPS65914_SMPS_CTRL                       TPS65914_SMPS_BASE + 0x24

#define TPS65914_SMPS_INVALID                    0xFF

/* ldo registers */
#define TPS65914_LDO_BASE                        0x50
#define TPS65914_LDO1_CTRL                       TPS65914_LDO_BASE + 0x00
#define TPS65914_LDO1_VOLTAGE                    TPS65914_LDO_BASE + 0x01

#define TPS65914_LDO2_CTRL                       TPS65914_LDO_BASE + 0x02
#define TPS65914_LDO2_VOLTAGE                    TPS65914_LDO_BASE + 0x03

#define TPS65914_LDO3_CTRL                       TPS65914_LDO_BASE + 0x04
#define TPS65914_LDO3_VOLTAGE                    TPS65914_LDO_BASE + 0x05

#define TPS65914_LDO4_CTRL                       TPS65914_LDO_BASE + 0x06
#define TPS65914_LDO4_VOLTAGE                    TPS65914_LDO_BASE + 0x07

#define TPS65914_LDO5_CTRL                       TPS65914_LDO_BASE + 0x08
#define TPS65914_LDO5_VOLTAGE                    TPS65914_LDO_BASE + 0x09

#define TPS65914_LDO6_CTRL                       TPS65914_LDO_BASE + 0x0A
#define TPS65914_LDO6_VOLTAGE                    TPS65914_LDO_BASE + 0x0B

#define TPS65914_LDO7_CTRL                       TPS65914_LDO_BASE + 0x0C
#define TPS65914_LDO7_VOLTAGE                    TPS65914_LDO_BASE + 0x0D

#define TPS65914_LDO8_CTRL                       TPS65914_LDO_BASE + 0x0E
#define TPS65914_LDO8_VOLTAGE                    TPS65914_LDO_BASE + 0x0F

#define TPS65914_LDO9_CTRL                       TPS65914_LDO_BASE + 0x10
#define TPS65914_LDO9_VOLTAGE                    TPS65914_LDO_BASE + 0x11

#define TPS65914_LDOLN_CTRL                      TPS65914_LDO_BASE + 0x12
#define TPS65914_LDOLN_VOLTAGE                   TPS65914_LDO_BASE + 0x13

#define TPS65914_LDOUSB_CTRL                     TPS65914_LDO_BASE + 0x14
#define TPS65914_LDOUSB_VOLTAGE                  TPS65914_LDO_BASE + 0x15
#define TPS65914_LDO_INVALID                     0xFF

#define TPS65914_NSLEEP_RES_ASSIGN               0xDA
#define TPS65914_NSLEEP_SMPS_ASSIGN              0xDB
#define TPS65914_NSLEEP_LDO_ASSIGN1              0xDC
#define TPS65914_NSLEEP_LDO_ASSIGN2              0xDD

#define TPS65914_ENABLE1_RES_ASSIGN              0xDE
#define TPS65914_ENABLE1_SMPS_ASSIGN             0xDF
#define TPS65914_ENABLE1_LDO_ASSIGN1             0xE0
#define TPS65914_ENABLE1_LDO_ASSIGN2             0xE1

#define TPS65914_PRIMARY_SECONDARY_PAD1          0xFA
#define TPS65914_PRIMARY_SECONDARY_PAD2          0xFB
#define TPS65914_PRIMARY_SECONDARY_PAD3          0xFE

#define TPS65914_INT1_STATUS                     0x10
#define TPS65914_INT1_MASK                       0x11
#define TPS65914_INT2_STATUS                     0x15
#define TPS65914_INT2_MASK                       0x16
#define TPS65914_INT3_STATUS                     0x1A
#define TPS65914_INT3_MASK                       0x1B

#define TPS65914_INT4_MASK                       0x20


#define TPS65914_SHIFT_INT1_STATUS_PWRON          1
#define TPS65914_SHIFT_INT1_MASK_PWRON            1
#define TPS65914_SHIFT_INT2_STATUS_RTC_ALARM      0
#define TPS65914_SHIFT_INT2_MASK_RTC_ALARM        0
#define TPS65914_SHIFT_INT3_STATUS_VBUS           7
#define TPS65914_SHIFT_INT3_MASK_VBUS             7

#define TPS65914_INT_CTRL                        0x24
#define TPS65914_SHIFT_INT_CTRL_INT_CLEAR         0

#define TPS65914_GPIO_DATA_IN                    0x80
#define TPS65914_GPIO_DATA_DIR                   0x81
#define TPS65914_GPIO_DATA_OUT                   0x82
#define TPS65914_GPIO_INVALID                    0xFF

// RTC Interrupt Mask/Status registers
#define TPS65914_RTC_STATUS_REG                    0x11
#define TPS65914_RTC_INTERRUPTS_REG                0x12
#define TPS65914_SHIFT_RTC_STATUS_REG_IT_ALARM     6
#define TPS65914_SHIFT_RTC_INTERRUPTS_REG_IT_ALARM 3



/****************************************************************/
#define TPS65914_RTC_COMP_LSB_REG				0x13 //add by yunboa 20150611,for saving battery capacity.

/* Registers for function GPADC */
#define TPS65914_GPADC_BASE						0x2C0

#define TPS65914_GPADC_CTRL1					TPS65914_GPADC_BASE + 0x0
#define TPS65914_GPADC_CTRL2					TPS65914_GPADC_BASE + 0x1
#define TPS65914_GPADC_RT_CTRL					TPS65914_GPADC_BASE + 0x2
#define TPS65914_GPADC_AUTO_CTRL				TPS65914_GPADC_BASE + 0x3
#define TPS65914_GPADC_STATUS					TPS65914_GPADC_BASE + 0x4
#define TPS65914_GPADC_RT_SELECT				TPS65914_GPADC_BASE + 0x5
#define TPS65914_GPADC_RT_CONV0_LSB				TPS65914_GPADC_BASE + 0x6
#define TPS65914_GPADC_RT_CONV0_MSB				TPS65914_GPADC_BASE + 0x7
#define TPS65914_GPADC_AUTO_SELECT				TPS65914_GPADC_BASE + 0x8
#define TPS65914_GPADC_AUTO_CONV0_LSB			TPS65914_GPADC_BASE + 0x9
#define TPS65914_GPADC_AUTO_CONV0_MSB			TPS65914_GPADC_BASE + 0xA
#define TPS65914_GPADC_AUTO_CONV1_LSB			TPS65914_GPADC_BASE + 0xB
#define TPS65914_GPADC_AUTO_CONV1_MSB			TPS65914_GPADC_BASE + 0xC
#define TPS65914_GPADC_SW_SELECT				TPS65914_GPADC_BASE + 0xD
#define TPS65914_GPADC_SW_CONV0_LSB				TPS65914_GPADC_BASE + 0xE
#define TPS65914_GPADC_SW_CONV0_MSB				TPS65914_GPADC_BASE + 0xF
#define TPS65914_GPADC_THRES_CONV0_LSB			TPS65914_GPADC_BASE + 0x10
#define TPS65914_GPADC_THRES_CONV0_MSB			TPS65914_GPADC_BASE + 0x11
#define TPS65914_GPADC_THRES_CONV1_LSB			TPS65914_GPADC_BASE + 0x12
#define TPS65914_GPADC_THRES_CONV1_MSB			TPS65914_GPADC_BASE + 0x13
#define TPS65914_GPADC_SMPS_ILMONITOR_EN		TPS65914_GPADC_BASE + 0x14
#define TPS65914_GPADC_SMPS_VSEL_MONITORING		TPS65914_GPADC_BASE + 0x15

/****************************************************************/


/* bit definitions */
#define TPS65914_SMPS_CTRL_SMPS45_SMPS457_EN     0x20
#define TPS65914_SMPS_CTRL_SMPS12_SMPS123_EN     0x10

/*pmu control*/
#define TPS65914_DEVCNTRL                        0xA0
#define TPS65914_POWER_CNTRL                     0xA1
#define TPS65914_POWER_OFF                       0x00

/* Revision register */
#define TPS65914_PMU_CONTROL_BASE                0x1A0
#define TPS65914_SW_REVISION                     (TPS65914_PMU_CONTROL_BASE + 0x17)
#define TPS65914_INTERNAL_DESIGNREV              0x357

/* Switch off register */
#define TPS65914_SWOFF_STATUS                    (TPS65914_PMU_CONTROL_BASE + 0x11)

#define TPS65914_LONGPRESSKEY_REG                0x1A9

#if defined(__cplusplus)
}
#endif

#endif /* TPS65914_REG_HEADER */
