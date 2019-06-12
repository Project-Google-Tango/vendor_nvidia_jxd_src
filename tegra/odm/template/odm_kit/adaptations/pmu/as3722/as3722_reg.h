/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef AS3722_REG_HEADER
#define AS3722_REG_HEADER

#if defined(__cplusplus)
extern "C"
{
#endif

/* regulator IDs */
#define AS3722_LDO0                            0
#define AS3722_LDO1                            1
#define AS3722_LDO2                            2
#define AS3722_LDO3                            3
#define AS3722_LDO4                            4
#define AS3722_LDO5                            5
#define AS3722_LDO6                            6
#define AS3722_LDO7                            7
#define AS3722_LDO9                            8
#define AS3722_LDO10                           9
#define AS3722_LDO11                           10
#define AS3722_SD0                             11
#define AS3722_SD1                             12
#define AS3722_SD2                             13
#define AS3722_SD3                             14
#define AS3722_SD4                             15
#define AS3722_SD5                             16
#define AS3722_SD6                             17

/* Interrupt IDs */
#define AS3722_IRQ_MAX_HANDLER                 11
#define AS3722_IRQ_LID                         0
#define AS3722_IRQ_ACOK                        1
#define AS3722_IRQ_CORE_PWRREQ                 2
#define AS3722_IRQ_OCURR_ACOK                  3
#define AS3722_IRQ_ONKEY_LONG                  4
#define AS3722_IRQ_ONKEY                       5
#define AS3722_IRQ_OVTMP                       6
#define AS3722_IRQ_LOWBAT                      7
#define AS3722_IRQ_RTC_REP                     8
#define AS3722_IRQ_RTC_ALARM                   9
#define AS3722_IRQ_SD0                         10
#define AS3722_IRQ_WATCHDOG                    11

/* AS3722 registers */
#define AS3722_SD0_VOLTAGE_REG                 0x00
#define AS3722_SD1_VOLTAGE_REG                 0x01
#define AS3722_SD2_VOLTAGE_REG                 0x02
#define AS3722_SD3_VOLTAGE_REG                 0x03
#define AS3722_SD4_VOLTAGE_REG                 0x04
#define AS3722_SD5_VOLTAGE_REG                 0x05
#define AS3722_SD6_VOLTAGE_REG                 0x06
#define AS3722_GPIO0_CONTROL_REG               0x08
#define AS3722_GPIO1_CONTROL_REG               0x09
#define AS3722_GPIO2_CONTROL_REG               0x0A
#define AS3722_GPIO3_CONTROL_REG               0x0B
#define AS3722_GPIO4_CONTROL_REG               0x0C
#define AS3722_GPIO5_CONTROL_REG               0x0D
#define AS3722_GPIO6_CONTROL_REG               0x0E
#define AS3722_GPIO7_CONTROL_REG               0x0F
#define AS3722_LDO0_VOLTAGE_REG                0x10
#define AS3722_LDO1_VOLTAGE_REG                0x11
#define AS3722_LDO2_VOLTAGE_REG                0x12
#define AS3722_LDO3_VOLTAGE_REG                0x13
#define AS3722_LDO4_VOLTAGE_REG                0x14
#define AS3722_LDO5_VOLTAGE_REG                0x15
#define AS3722_LDO6_VOLTAGE_REG                0x16
#define AS3722_LDO7_VOLTAGE_REG                0x17
#define AS3722_LDO9_VOLTAGE_REG                0x19
#define AS3722_LDO10_VOLTAGE_REG               0x1A
#define AS3722_LDO11_VOLTAGE_REG               0x1B

#define AS3722_GPIO_SIGNAL_OUT_REG             0x20
#define AS3722_GPIO_SIGNAL_IN_REG              0x21

#define AS3722_SD0_CONTROL_REG                 0x29
#define AS3722_SD1_CONTROL_REG                 0x2A
#define AS3722_SDmph_CONTROL_REG               0x2B
#define AS3722_SD23_CONTROL_REG                0x2C
#define AS3722_SD4_CONTROL_REG                 0x2D
#define AS3722_SD5_CONTROL_REG                 0x2E
#define AS3722_SD6_CONTROL_REG                 0x2F

#define AS3722_WATCHDOG_CONTROL_REG            0x38
#define AS3722_WATCHDOG_TIMER_REG              0x46
#define AS3722_WATCHDOG_SOFTWARE_SIGNAL_REG    0x48
#define AS3722_IOVOLTAGE_REG                   0x49
#define AS3722_SD_CONTROL_REG                  0x4D
#define AS3722_LDOCONTROL0_REG                 0x4E
#define AS3722_LDOCONTROL1_REG                 0x4F

#define AS3722_CTRL1_REG                       0x58
#define AS3722_CTRL2_REG                       0x59
#define AS3722_RTC_CONTROL_REG                 0x60
#define AS3722_RTC_SECOND_REG                  0x61
#define AS3722_RTC_MINUTE_REG                  0x62
#define AS3722_RTC_HOUR_REG                    0x63
#define AS3722_RTC_DAY_REG                     0x64
#define AS3722_RTC_MONTH_REG                   0x65
#define AS3722_RTC_YEAR_REG                    0x66
#define AS3722_RTC_ALARM_SECOND_REG            0x67
#define AS3722_RTC_ALARM_MINUTE_REG            0x68
#define AS3722_RTC_ALARM_HOUR_REG              0x69
#define AS3722_RTC_ALARM_DAY_REG               0x6A
#define AS3722_RTC_ALARM_MONTH_REG             0x6B
#define AS3722_RTC_ALARM_YEAR_REG              0x6C

#define AS3722_INTERRUPTMASK1_REG              0x74
#define AS3722_INTERRUPTMASK2_REG              0x75
#define AS3722_INTERRUPTMASK3_REG              0x76
#define AS3722_INTERRUPTMASK4_REG              0x77
#define AS3722_INTERRUPTSTATUS1_REG            0x78
#define AS3722_INTERRUPTSTATUS2_REG            0x79
#define AS3722_INTERRUPTSTATUS3_REG            0x7A
#define AS3722_INTERRUPTSTATUS4_REG            0x7B

#define AS3722_ADC0_CONTROL_REG                0x80
#define AS3722_ADC1_CONTROL_REG                0x81
#define AS3722_ADC0_MSB_RESULT_REG             0x82
#define AS3722_ADC0_LSB_RESULT_REG             0x83
#define AS3722_ADC1_MSB_RESULT_REG             0x84
#define AS3722_ADC1_LSB_RESULT_REG             0x85
#define AS3722_ADC1_THRESHOLD_HI_MSB_REG       0x86
#define AS3722_ADC1_THRESHOLD_HI_LSB_REG       0x87
#define AS3722_ADC1_THRESHOLD_LO_MSB_REG       0x88
#define AS3722_ADC1_THRESHOLD_LO_LSB_REG       0x89
#define AS3722_ADC_CONFIG_REG                  0x8A

/* AS3722 register bits and bit masks */
#define AS3722_LDO_ILIMIT_MASK                 (1 << 7)
#define AS3722_LDO_ILIMIT_BIT                  (1 << 7)
#define AS3722_LDO0_VSEL_MASK                  0x1F
#define AS3722_LDO0_VSEL_MIN                   0x01
#define AS3722_LDO0_VSEL_MAX                   0x12
#define AS3722_LDO3_VSEL_MASK                  0x3F
#define AS3722_LDO3_VSEL_MIN                   0x01
#define AS3722_LDO3_VSEL_MAX                   0x45
#define AS3722_LDO_VSEL_MASK                   0x7F
#define AS3722_LDO_VSEL_MIN                    0x01
#define AS3722_LDO_VSEL_MAX                    0x7F
#define AS3722_LDO_VSEL_DNU_MIN                0x25
#define AS3722_LDO_VSEL_DNU_MAX                0x3F
#define AS3722_LDO_NUM_VOLT                    100

#define AS3722_LDO0_ON                         (1 << 0)
#define AS3722_LDO0_OFF                        (0 << 0)
#define AS3722_LDO0_CTRL_MASK                  (1 << 0)
#define AS3722_LDO1_ON                         (1 << 1)
#define AS3722_LDO1_OFF                        (0 << 1)
#define AS3722_LDO1_CTRL_MASK                  (1 << 1)
#define AS3722_LDO2_ON                         (1 << 2)
#define AS3722_LDO2_OFF                        (0 << 2)
#define AS3722_LDO2_CTRL_MASK                  (1 << 2)
#define AS3722_LDO3_ON                         (1 << 3)
#define AS3722_LDO3_OFF                        (0 << 3)
#define AS3722_LDO3_CTRL_MASK                  (1 << 3)
#define AS3722_LDO4_ON                         (1 << 4)
#define AS3722_LDO4_OFF                        (0 << 4)
#define AS3722_LDO4_CTRL_MASK                  (1 << 4)
#define AS3722_LDO5_ON                         (1 << 5)
#define AS3722_LDO5_OFF                        (0 << 5)
#define AS3722_LDO5_CTRL_MASK                  (1 << 5)
#define AS3722_LDO6_ON                         (1 << 6)
#define AS3722_LDO6_OFF                        (0 << 6)
#define AS3722_LDO6_CTRL_MASK                  (1 << 6)
#define AS3722_LDO7_ON                         (1 << 7)
#define AS3722_LDO7_OFF                        (0 << 7)
#define AS3722_LDO7_CTRL_MASK                  (1 << 7)
#define AS3722_LDO9_ON                         (1 << 1)
#define AS3722_LDO9_OFF                        (0 << 1)
#define AS3722_LDO9_CTRL_MASK                  (1 << 1)
#define AS3722_LDO10_ON                        (1 << 2)
#define AS3722_LDO10_OFF                       (0 << 2)
#define AS3722_LDO10_CTRL_MASK                 (1 << 2)
#define AS3722_LDO11_ON                        (1 << 3)
#define AS3722_LDO11_OFF                       (0 << 3)
#define AS3722_LDO11_CTRL_MASK                 (1 << 3)

#define AS3722_SD_VSEL_MASK                    0x7F
#define AS3722_SD0_VSEL_MIN                    0x01
#define AS3722_SD0_VSEL_MAX                    0x5A
#define AS3722_SD2_VSEL_MIN                    0x01
#define AS3722_SD2_VSEL_MAX                    0x7F
#define AS3722_SD0_ON                          (1 << 0)
#define AS3722_SD0_OFF                         (0 << 0)
#define AS3722_SD0_CTRL_MASK                   (1 << 0)
#define AS3722_SD1_ON                          (1 << 1)
#define AS3722_SD1_OFF                         (0 << 1)
#define AS3722_SD1_CTRL_MASK                   (1 << 1)
#define AS3722_SD2_ON                          (1 << 2)
#define AS3722_SD2_OFF                         (0 << 2)
#define AS3722_SD2_CTRL_MASK                   (1 << 2)
#define AS3722_SD3_ON                          (1 << 3)
#define AS3722_SD3_OFF                         (0 << 3)
#define AS3722_SD3_CTRL_MASK                   (1 << 3)
#define AS3722_SD4_ON                          (1 << 4)
#define AS3722_SD4_OFF                         (0 << 4)
#define AS3722_SD4_CTRL_MASK                   (1 << 4)
#define AS3722_SD5_ON                          (1 << 5)
#define AS3722_SD5_OFF                         (0 << 5)
#define AS3722_SD5_CTRL_MASK                   (1 << 5)
#define AS3722_SD6_ON                          (1 << 6)
#define AS3722_SD6_OFF                         (0 << 6)
#define AS3722_SD6_CTRL_MASK                   (1 << 6)

#define AS3722_SD0_MODE_FAST                   (1 << 4)
#define AS3722_SD0_MODE_NORMAL                 (0 << 4)
#define AS3722_SD0_MODE_MASK                   (1 << 4)
#define AS3722_SD1_MODE_FAST                   (1 << 4)
#define AS3722_SD1_MODE_NORMAL                 (0 << 4)
#define AS3722_SD1_MODE_MASK                   (1 << 4)
#define AS3722_SD2_MODE_FAST                   (1 << 2)
#define AS3722_SD2_MODE_NORMAL                 (0 << 2)
#define AS3722_SD2_MODE_MASK                   (1 << 2)
#define AS3722_SD3_MODE_FAST                   (1 << 6)
#define AS3722_SD3_MODE_NORMAL                 (0 << 6)
#define AS3722_SD3_MODE_MASK                   (1 << 6)
#define AS3722_SD4_MODE_FAST                   (1 << 2)
#define AS3722_SD4_MODE_NORMAL                 (0 << 2)
#define AS3722_SD4_MODE_MASK                   (1 << 2)
#define AS3722_SD5_MODE_FAST                   (1 << 2)
#define AS3722_SD5_MODE_NORMAL                 (0 << 2)
#define AS3722_SD5_MODE_MASK                   (1 << 2)
#define AS3722_SD6_MODE_FAST                   (1 << 4)
#define AS3722_SD6_MODE_NORMAL                 (0 << 4)
#define AS3722_SD6_MODE_MASK                   (1 << 4)

#define AS3722_IRQ_MASK_LID                    (1 << 0)
#define AS3722_IRQ_MASK_ACOK                   (1 << 1)
#define AS3722_IRQ_MASK_CORE_PWRREQ            (1 << 2)
#define AS3722_IRQ_MASK_OCURR_ACOK             (1 << 3)
#define AS3722_IRQ_MASK_ONKEY_LONG             (1 << 4)
#define AS3722_IRQ_MASK_ONKEY                  (1 << 5)
#define AS3722_IRQ_MASK_OVTMP                  (1 << 6)
#define AS3722_IRQ_MASK_LOWBAT                 (1 << 7)

#define AS3722_IRQ_MASK_SD0                    (1 << 0)

#define AS3722_IRQ_MASK_RTC_REP                (1 << 7)
#define AS3722_IRQ_MASK_RTC_ALARM              (1 << 0)

#define AS3722_IRQ_MASK_WATCHDOG               (1 << 6)

#define AS3722_IRQ_BIT_LID                     (1 << 0)
#define AS3722_IRQ_BIT_ACOK                    (1 << 1)
#define AS3722_IRQ_BIT_CORE_PWRREQ             (1 << 2)
#define AS3722_IRQ_BIT_SD0                     (1 << 3)
#define AS3722_IRQ_BIT_ONKEY_LONG              (1 << 4)
#define AS3722_IRQ_BIT_ONKEY                   (1 << 5)
#define AS3722_IRQ_BIT_OVTMP                   (1 << 6)
#define AS3722_IRQ_BIT_LOWBAT                  (1 << 7)

#define AS3722_IRQ_BIT_RTC_REP                 (1 << 7)
#define AS3722_IRQ_BIT_RTC_ALARM               (1 << 0)

#define AS3722_IRQ_BIT_WATCHDOG                (1 << 6)

#define AS3722_ADC_MASK_BUF_ON                 (1 << 2)
#define AS3722_ADC_BIT_BUF_ON                  (1 << 2)
#define AS3722_ADC1_MASK_INT_MODE_ON           (1 << 1)
#define AS3722_ADC1_BIT_INT_MODE_ON            (1 << 1)
#define AS3722_ADC1_MASK_INTERVAL_TIME         (1 << 0)
#define AS3722_ADC1_BIT_INTERVAL_TIME          (1 << 0)

#define AS3722_ADC_MASK_MSB_VAL                0x3F
#define AS3722_ADC_MASK_LSB_VAL                0x07

#define AS3722_ADC0_MASK_CONV_START            (1 << 7)
#define AS3722_ADC0_BIT_CONV_START             (1 << 7)
#define AS3722_ADC0_MASK_CONV_NOTREADY         (1 << 7)
#define AS3722_ADC0_BIT_CONV_NOTREADY          (1 << 7)
#define AS3722_ADC0_MASK_SOURCE_SELECT         0x1F

#define AS3722_ADC1_MASK_CONV_START            (1 << 7)
#define AS3722_ADC1_BIT_CONV_START             (1 << 7)
#define AS3722_ADC1_MASK_CONV_NOTREADY         (1 << 7)
#define AS3722_ADC1_BIT_CONV_NOTREADY          (1 << 7)
#define AS3722_ADC1_MASK_SOURCE_SELECT         0x1F

#define AS3722_GPIO_INV_MASK                   0x80
#define AS3722_GPIO_INV                        0x80
#define AS3722_GPIO_IOSF_MASK                  0x78
#define AS3722_GPIO_IOSF_NORMAL                0
#define AS3722_GPIO_IOSF_INTERRUPT_OUT         (1 << 3)
#define AS3722_GPIO_IOSF_VSUP_LOW_OUT          (2 << 3)
#define AS3722_GPIO_IOSF_GPIO_INTERRUPT_IN     (3 << 3)
#define AS3722_GPIO_IOSF_ISINK_PWM_IN          (4 << 3)
#define AS3722_GPIO_IOSF_VOLTAGE_STBY          (5 << 3)
#define AS3722_GPIO_IOSF_PWR_GOOD_OUT          (7 << 3)
#define AS3722_GPIO_IOSF_Q32K_OUT              (8 << 3)
#define AS3722_GPIO_IOSF_WATCHDOG_IN           (9 << 3)
#define AS3722_GPIO_IOSF_SOFT_RESET_IN         (11 << 3)
#define AS3722_GPIO_IOSF_PWM_OUT               (12 << 3)
#define AS3722_GPIO_IOSF_VSUP_LOW_DEB_OUT      (13 << 3)
#define AS3722_GPIO_IOSF_SD6_LOW_VOLT_LOW      (14 << 3)

#define AS3722_GPIO0_SIGNAL_MASK               (1 << 0)
#define AS3722_GPIO1_SIGNAL_MASK               (1 << 1)
#define AS3722_GPIO2_SIGNAL_MASK               (1 << 2)
#define AS3722_GPIO3_SIGNAL_MASK               (1 << 3)
#define AS3722_GPIO4_SIGNAL_MASK               (1 << 4)
#define AS3722_GPIO5_SIGNAL_MASK               (1 << 5)
#define AS3722_GPIO6_SIGNAL_MASK               (1 << 6)
#define AS3722_GPIO7_SIGNAL_MASK               (1 << 7)

#define AS3722_INT_PULLUP_MASK                 (1<<5)
#define AS3722_INT_PULLUP_ON                   (1<<5)
#define AS3722_INT_PULLUP_OFF                  (0<<5)
#define AS3722_I2C_PULLUP_MASK                 (1<<4)
#define AS3722_I2C_PULLUP_ON                   (1<<4)
#define AS3722_I2C_PULLUP_OFF                  (0<<4)

#define AS3722_RTC_REP_WAKEUP_EN_MASK          (1 << 0)
#define AS3722_RTC_ALARM_WAKEUP_EN_MASK        (1 << 1)
#define AS3722_RTC_ON_MASK                     (1 << 2)
#define AS3722_RTC_IRQMODE_MASK                (3 << 3)

#define AS3722_WATCHDOG_TIMER_MAX              127
#define AS3722_WATCHDOG_ON_MASK                (1<<0)
#define AS3722_WATCHDOG_ON                     (1<<0)
#define AS3722_WATCHDOG_SW_SIG_MASK            (1<<0)
#define AS3722_WATCHDOG_SW_SIG                 (1<<0)

#define AS3722_Invalid                         0
#define AS3722_RFF_INVALID                     0
#define AS3722_Invalid_ON                      0

#if defined(__cplusplus)
}
#endif

#endif /* AS3722_REG_HEADER */
