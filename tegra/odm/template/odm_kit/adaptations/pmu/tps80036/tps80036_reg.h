/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef TPS80036_REG_HEADER
#define TPS80036_REG_HEADER

#if defined(__cplusplus)
extern "C"
{
#endif

/* TPS80036 registers */
#define TPS80036_RFF_INVALID                     0xFF

/* smps registers */
#define TPS80036_SMPS_BASE                       0x20

/* SMPS1_2, SMPS3, SMPS6 and SMPS8 are DVS-capable */
#define TPS80036_SMPS1_2_CTRL                    TPS80036_SMPS_BASE + 0x00
#define TPS80036_SMPS1_2_TSTEP                   TPS80036_SMPS_BASE + 0x01
#define TPS80036_SMPS1_2_FORCE                   TPS80036_SMPS_BASE + 0x02
#define TPS80036_SMPS1_2_VOLTAGE                 TPS80036_SMPS_BASE + 0x03

#define TPS80036_SMPS3_CTRL                      TPS80036_SMPS_BASE + 0x04
#define TPS80036_SMPS3_TSTEP                     TPS80036_SMPS_BASE + 0x05
#define TPS80036_SMPS3_FORCE                     TPS80036_SMPS_BASE + 0x06
#define TPS80036_SMPS3_VOLTAGE                   TPS80036_SMPS_BASE + 0x07

#define TPS80036_SMPS6_CTRL                      TPS80036_SMPS_BASE + 0x0C
#define TPS80036_SMPS6_TSTEP                     TPS80036_SMPS_BASE + 0x0D
#define TPS80036_SMPS6_FORCE                     TPS80036_SMPS_BASE + 0x0E
#define TPS80036_SMPS6_VOLTAGE                   TPS80036_SMPS_BASE + 0x0F

#define TPS80036_SMPS8_CTRL                      TPS80036_SMPS_BASE + 0x14
#define TPS80036_SMPS8_TSTEP                     TPS80036_SMPS_BASE + 0x15
#define TPS80036_SMPS8_FORCE                     TPS80036_SMPS_BASE + 0x16
#define TPS80036_SMPS8_VOLTAGE                   TPS80036_SMPS_BASE + 0x17

/* SMPS7 and SMPS 9 are not DVS capable */
#define TPS80036_SMPS7_CTRL                      TPS80036_SMPS_BASE + 0x10
#define TPS80036_SMPS7_VOLTAGE                   TPS80036_SMPS_BASE + 0x13

#define TPS80036_SMPS9_CTRL                      TPS80036_SMPS_BASE + 0x18
#define TPS80036_SMPS9_VOLTAGE                   TPS80036_SMPS_BASE + 0x1B

#define TPS80036_SMPS_CTRL                       TPS80036_SMPS_BASE + 0x24

#define TPS80036_SMPS_INVALID                    0xFF

#define SMPS_CTRL_MODE_OFF                       0x00
#define SMPS_CTRL_MODE_ON                        0x01
#define SMPS_CTRL_MODE_ECO                       0x02
#define SMPS_CTRL_MODE_PWM                       0x03

/* ldo registers */
#define TPS80036_LDO_BASE                        0x50
#define TPS80036_LDO1_CTRL                       TPS80036_LDO_BASE + 0x00
#define TPS80036_LDO1_VOLTAGE                    TPS80036_LDO_BASE + 0x01

#define TPS80036_LDO2_CTRL                       TPS80036_LDO_BASE + 0x02
#define TPS80036_LDO2_VOLTAGE                    TPS80036_LDO_BASE + 0x03

#define TPS80036_LDO3_CTRL                       TPS80036_LDO_BASE + 0x04
#define TPS80036_LDO3_VOLTAGE                    TPS80036_LDO_BASE + 0x05

#define TPS80036_LDO4_CTRL                       TPS80036_LDO_BASE + 0x06
#define TPS80036_LDO4_VOLTAGE                    TPS80036_LDO_BASE + 0x07

#define TPS80036_LDO5_CTRL                       TPS80036_LDO_BASE + 0x08
#define TPS80036_LDO5_VOLTAGE                    TPS80036_LDO_BASE + 0x09

#define TPS80036_LDO6_CTRL                       TPS80036_LDO_BASE + 0x0A
#define TPS80036_LDO6_VOLTAGE                    TPS80036_LDO_BASE + 0x0B

#define TPS80036_LDO7_CTRL                       TPS80036_LDO_BASE + 0x0C
#define TPS80036_LDO7_VOLTAGE                    TPS80036_LDO_BASE + 0x0D

#define TPS80036_LDO8_CTRL                       TPS80036_LDO_BASE + 0x0E
#define TPS80036_LDO8_VOLTAGE                    TPS80036_LDO_BASE + 0x0F

#define TPS80036_LDO9_CTRL                       TPS80036_LDO_BASE + 0x10
#define TPS80036_LDO9_VOLTAGE                    TPS80036_LDO_BASE + 0x11

#define TPS80036_LDOLN_CTRL                      TPS80036_LDO_BASE + 0x12
#define TPS80036_LDOLN_VOLTAGE                   TPS80036_LDO_BASE + 0x13

#define TPS80036_LDOUSB_CTRL                     TPS80036_LDO_BASE + 0x14
#define TPS80036_LDOUSB_VOLTAGE                  TPS80036_LDO_BASE + 0x15

#define TPS80036_LDO10_CTRL                      TPS80036_LDO_BASE + 0x16
#define TPS80036_LDO10_VOLTAGE                   TPS80036_LDO_BASE + 0x17

#define TPS80036_LDO11_CTRL                      TPS80036_LDO_BASE + 0x18
#define TPS80036_LDO11_VOLTAGE                   TPS80036_LDO_BASE + 0x19

#define TPS80036_LDO_CTRL                        TPS80036_LDO_BASE + 0x1A

#define TPS80036_LDO12_CTRL                      TPS80036_LDO_BASE + 0x1F
#define TPS80036_LDO12_VOLTAGE                   TPS80036_LDO_BASE + 0x20

#define TPS80036_LDO13_CTRL                      TPS80036_LDO_BASE + 0x21
#define TPS80036_LDO13_VOLTAGE                   TPS80036_LDO_BASE + 0x22

#define TPS80036_LDO14_CTRL                      TPS80036_LDO_BASE + 0x23
#define TPS80036_LDO14_VOLTAGE                   TPS80036_LDO_BASE + 0x24

#define TPS80036_LDO_INVALID                     0xFF

/* Interrupt Mask/Status registers*/
#define TPS80036_INT1_STATUS                      0x10
#define TPS80036_INT1_MASK                        0x11

#define TPS80036_INT2_STATUS                      0x15
#define TPS80036_INT2_MASK                        0x16
#define TPS80036_SHIFT_INT2_STATUS_RTC_ALARM      0
#define TPS80036_SHIFT_INT2_MASK_RTC_ALARM        0

#define TPS80036_INT3_STATUS                      0x1A
#define TPS80036_INT3_LINE_STATE                  0x1C

#define TPS80036_INT3_MASK                        0x1B

#define TPS80036_INT4_STATUS                      0x1F
#define TPS80036_INT4_MASK                        0x20

#define TPS80036_INT5_STATUS                      0x25

#define TPS80036_INT6_STATUS                      0x2A
#define TPS80036_INT6_MASK                        0x2B

#define TPS80036_INT_CTRL                         0x24

// RTC Interrupt Mask/Status registers
#define TPS80036_RTC_STATUS_REG                    0x11
#define TPS80036_RTC_INTERRUPTS_REG                0x12
#define TPS80036_SHIFT_RTC_STATUS_REG_IT_ALARM     6
#define TPS80036_SHIFT_RTC_INTERRUPTS_REG_IT_ALARM 3


/* bit definitions */
#define TPS80036_SMPS_CTRL_SMPS45_SMPS457_EN     0x20
#define TPS80036_SMPS_CTRL_SMPS12_SMPS123_EN     0x10

/*pmu control*/
#define TPS80036_DEVCNTRL                        0xA0
#define TPS80036_POWER_OFF                       0x00

#if defined(__cplusplus)
}
#endif

#endif /* TPS80036_REG_HEADER */
