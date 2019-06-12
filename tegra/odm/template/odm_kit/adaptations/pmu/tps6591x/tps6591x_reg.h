/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TPS6591X_REG_HEADER
#define TPS6591X_REG_HEADER

#if defined(__cplusplus)
extern "C"
{
#endif

/* TPS6591x registers */
/* Time log register */
#define TPS6591x_R00_TIME_SECOND                0x00
#define TPS6591x_R01_TIME_MINUTES               0x01
#define TPS6591x_R02_TIME_HOUR                  0x02
#define TPS6591x_R03_TIME_DAYS                  0x03
#define TPS6591x_R04_TIME_MONTHS                0x04
#define TPS6591x_R05_TIME_YEARS                 0x05
#define TPS6591x_R06_TIME_WEEKS                 0x06

/* Alarm register */
#define TPS6591x_R08_ALARM_SECONDS              0x08
#define TPS6591x_R09_ALARM_MINUTES              0x09
#define TPS6591x_R0A_ALARM_HOURS                0x0A
#define TPS6591x_R0B_ALARM_DAYS                 0x0B
#define TPS6591x_R0C_ALARM_MONTHS               0x0C
#define TPS6591x_R0D_ALARM_YEARS                0x0D

/* RTC */
#define TPS6591x_R10_RTC_CTRL                   0x10
#define TPS6591x_R11_RTC_STATUS                 0x11
#define TPS6591x_R12_RTC_INTERRPT               0x12
#define TPS6591x_R13_RTC_COMP_LSB               0x13
#define TPS6591x_R14_RTC_COM_MSB                0x14
#define TPS6591x_R15_RTC_RES_PROG               0x15
#define TPS6591x_R16_RTC_RESET_STATUS           0x16

/* BCK Reg*/
#define TPS6591x_R17_BCK1                       0x17
#define TPS6591x_R18_BCK2                       0x18
#define TPS6591x_R19_BCK3                       0x19
#define TPS6591x_R1A_BCK4                       0x1A
#define TPS6591x_R1B_BCK5                       0x1B

/* Power supply register */
#define TPS6591x_R1C_PUADEN                     0x1C
#define TPS6591x_R1D_REF                        0x1D
#define TPS6591x_R1E_VRTC                       0x1E
#define TPS6591x_R20_VIO                        0x20
#define TPS6591x_R21_VDD1                       0x21
#define TPS6591x_R22_VDD1_OP                    0x22
#define TPS6591x_R23_VDD1_SR                    0x23
#define TPS6591x_R24_VDD2                       0x24
#define TPS6591x_R25_VDD2_OP                    0x25
#define TPS6591x_R26_VDD2_SR                    0x26
#define TPS6591x_R27_VDDCTRL                    0x27
#define TPS6591x_R28_VDDCTRL_OP                 0x28
#define TPS6591x_R29_VDDCTRL_SR                 0x29
#define TPS6591x_R30_LDO1                       0x30
#define TPS6591x_R31_LDO2                       0x31
#define TPS6591x_R32_LDO5                       0x32
#define TPS6591x_R33_LDO8                       0x33
#define TPS6591x_R34_LDO7                       0x34
#define TPS6591x_R35_LDO6                       0x35
#define TPS6591x_R36_LDO4                       0x36
#define TPS6591x_R37_LDO3                       0x37
#define TPS6591x_R38_THERM                      0x38
#define TPS6591x_R39_BBCH                       0x39
#define TPS6591x_R3E_DCDCCTRL                   0x3E
#define TPS6591x_R3F_DEVCTRL                    0x3F
#define TPS6591x_R40_DEVCTRL2                   0x40
#define TPS6591x_R41_SLEEP_KEEP_LDO_ON          0x41
#define TPS6591x_R42_SLEEP_KEEP_RES_ON          0x42
#define TPS6591x_R43_SLEEP_SET_LDO_OFF          0x43
#define TPS6591x_R44_SLEEP_KEEP_RES_OFF         0x44
#define TPS6591x_R45_EN1_LDO_ASS                0x45
#define TPS6591x_R46_EN1_SMPS_ASS               0x46
#define TPS6591x_R47_EN2_LDO_ASS                0x47
#define TPS6591x_R48_EN2_SMPS_ASS               0x48

/* Interrupt Control */
#define TPS6591x_R50_INT_STS                    0x50
#define TPS6591x_R51_INT_MSK                    0x51
#define TPS6591x_R52_INT_STS2                   0x52
#define TPS6591x_R53_INT_MSK2                   0x53
#define TPS6591x_R54_INT_STS3                   0x54
#define TPS6591x_R55_INT_MSK3                   0x55

/* GPIO Control */
#define TPS6591x_R60_GPIO0                      0x60
#define TPS6591x_R61_GPIO1                      0x61
#define TPS6591x_R62_GPIO2                      0x62
#define TPS6591x_R63_GPIO3                      0x63
#define TPS6591x_R64_GPIO4                      0x64
#define TPS6591x_R65_GPIO5                      0x65
#define TPS6591x_R66_GPIO6                      0x66
#define TPS6591x_R67_GPIO7                      0x67
#define TPS6591x_R68_GPIO8                      0x68

#define TPS6591x_R69_WATCHDOG                   0x69
#define TPS6591x_R6A_WMBCH                      0x6A
#define TPS6591x_R6B_VMBCH2                     0x6B
#define TPS6591x_R6C_LED_CTRL1                  0x6C
#define TPS6591x_R6D_LED_CTRL2                  0x6D
#define TPS6591x_R6E_PWM_CTRL1                  0x6E
#define TPS6591x_R6F_PWM_CTRL2                  0x6F

#define TPS6591x_R70_SPARE                      0x70
#define TPS6591x_R80_VERNUM                     0x80

#define TPS6591x_RFF_INVALID                    0xFF

#if defined(__cplusplus)
}
#endif


#endif //TPS6591X_REG_HEADER
