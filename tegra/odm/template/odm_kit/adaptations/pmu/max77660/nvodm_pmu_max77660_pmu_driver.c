/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: Max77660 pmu driver</b>
  *
  * @b Description: Implements the Max77660 pmu drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_pmu_max77660_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "nvodm_services.h"


#define Max77660PMUGUID    NV_ODM_GUID('m','a','x','7','7','6','6','0')
#define INVALID_VOLTAGE    0xFFFFFFFF

#define Max77660_REG_GLBLCNFG0 0x1C
#define Max77660_REG_GLBLCNFG6 0x22
#define SFT_OFF_RST 0x08

#define Max77660_REG_GLBLCNFG1 0x1D
#define WDT_ENABLE  0x20

//TODO: See if I2c_SPEED is indeed 100
#define Max77660_I2C_SPEED 100
#define Max77660_REG_ONOFF_CFG1         0x41
#define Max77660_REG_WDTC_SYS 0x20
#define Max77660_REG_TWD_SYS 0x1e

// Chip Identification Registers
#define Max77660_REG_CID4 0x9E

// Interrupt registers
#define Max77660_REG_GLBLSTAT1 0x02
#define Max77660_REG_GLBLINT1 0x07
#define Max77660_REG_GLBLINT2 0x08
#define Max77660_REG_GLBLINTM1 0x11
#define Max77660_REG_GLBLINTM2 0x12
#define Max77660_REG_CHGINTM 0x5e
#define Max77660_REG_INTTOPM1 0x0f
#define Max77660_REG_INTTOP1 0x05
#define Max77660_REG_CHGINT 0x5d
#define Max77660_REG_CHGSTAT 0x5f

// Rtc address and registers
#define MAX77660_RTC_ADDRESS 0xD0
#define Max77660_REG_RTCAE1  0x0E
#define Max77660_REG_RTCCTRLM 0x00

#define ONOFF_SFT_RST_MASK              (1 << 7)

#define RAIL_TYPE_BUCK     0
#define RAIL_TYPE_LDO_N    1
#define RAIL_TYPE_LDO_P    2
#define RAIL_TYPE_SW       3
#define RAIL_TYPE_GPIO     4
#define RAIL_TYPE_INVALID  5

/* Power Mode */
#define POWER_MODE_INVALID      4
#define POWER_MODE_NORMAL        3
#define POWER_MODE_LPM            2
#define POWER_MODE_GLPM            1
#define POWER_MODE_DISABLE        0

#define FPS_SRC_0          0
#define FPS_SRC_1          1
#define FPS_SRC_2          2
#define FPS_SRC_3          3
#define FPS_SRC_4          4
#define FPS_SRC_5          5
#define FPS_SRC_6          6
#define FPS_SRC_NONE       7

#define MAX77660_REG_INVALID        0xFF
#define MAX77660_REG_FPS_NONE        MAX77660_REG_INVALID

/* FPS Registers */
#define MAX77660_REG_CNFG_FPS_AP_OFF 0x23
#define MAX77660_REG_CNFG_FPS_AP_SLP 0x24
#define MAX77660_REG_CNFG_FPS_6         0x25

#define MAX77660_REG_FPS_RSO        0x26
#define MAX77660_REG_FPS_BUCK1        0x27
#define MAX77660_REG_FPS_BUCK2        0x28
#define MAX77660_REG_FPS_BUCK3        0x29
#define MAX77660_REG_FPS_BUCK4        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_BUCK5        0x2A
#define MAX77660_REG_FPS_BUCK6        0x2B
#define MAX77660_REG_FPS_BUCK7        0x2C


#define MAX77660_REG_FPS_SW4        0x2E
#define MAX77660_REG_FPS_SW5        0x2D
#define MAX77660_REG_FPS_LDO1        0x2E
#define MAX77660_REG_FPS_LDO2        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO3        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO4        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO5        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO6        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO7        0x2F
#define MAX77660_REG_FPS_LDO8        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO9        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO10        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO11        0x30
#define MAX77660_REG_FPS_LDO12        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO13        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO14        0x31
#define MAX77660_REG_FPS_LDO15        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO16        MAX77660_REG_FPS_NONE
#define MAX77660_REG_FPS_LDO17        0x32
#define MAX77660_REG_FPS_LDO18        0x33

#define MAX77660_REG_FPS_GPIO1        0x34
#define MAX77660_REG_FPS_GPIO2        0x35
#define MAX77660_REG_FPS_GPIO3        0x36

#define MAX77660_REG_SW_EN          0x43
/* BUCK and LDO Registers */
#define MAX77660_REG_BUCK1_VOUT    0x46
#define MAX77660_REG_BUCK2_VOUT    0x47
#define MAX77660_REG_BUCK3_VOUT    0x48
#define MAX77660_REG_BUCK3_VDVS    0x49
#define MAX77660_REG_BUCK5_VOUT    0x4A
#define MAX77660_REG_BUCK5_VDVS    0x4B
#define MAX77660_REG_BUCK6_VOUT    0x4C
#define MAX77660_REG_BUCK7_VOUT    0x4D


#define MAX77660_REG_BUCK6_CNFG    0x4C
#define MAX77660_REG_BUCK7_CNFG    0x4D
#define MAX77660_REG_BUCK1_CNFG    0x4E
#define MAX77660_REG_BUCK2_CNFG    0x4F
#define MAX77660_REG_BUCK3_CNFG    0x50
#define MAX77660_REG_BUCK4_CNFG    0x51
#define MAX77660_REG_BUCK5_CNFG    0x52
#define MAX77660_REG_BUCK4_VOUT    0x53


#define MAX77660_REG_LDO1_CNFG        0x54
#define MAX77660_REG_LDO2_CNFG        0x55
#define MAX77660_REG_LDO3_CNFG        0x56
#define MAX77660_REG_LDO4_CNFG        0x57
#define MAX77660_REG_LDO5_CNFG        0x58
#define MAX77660_REG_LDO6_CNFG        0x59
#define MAX77660_REG_LDO7_CNFG        0x5A
#define MAX77660_REG_LDO8_CNFG        0x5B
#define MAX77660_REG_LDO9_CNFG        0x5C
#define MAX77660_REG_LDO10_CNFG        0x5D
#define MAX77660_REG_LDO11_CNFG        0x5E
#define MAX77660_REG_LDO12_CNFG        0x5F
#define MAX77660_REG_LDO13_CNFG        0x60
#define MAX77660_REG_LDO14_CNFG        0x61
#define MAX77660_REG_LDO15_CNFG        0x62
#define MAX77660_REG_LDO16_CNFG        0x63
#define MAX77660_REG_LDO17_CNFG        0x64
#define MAX77660_REG_LDO18_CNFG        0x65

#define MAX77660_REG_SW1_CNFG       0x66
#define MAX77660_REG_SW2_CNFG       0x67
#define MAX77660_REG_SW3_CNFG       0x68
#define MAX77660_REG_SW4_CNFG       MAX77660_REG_INVALID
#define MAX77660_REG_SW5_CNFG       0x69


#define MAX77660_REG_BUCK_PWR_MODE1 0x37
#define MAX77660_REG_BUCK_PWR_MODE2 0x38

#define MAX77660_REG_LDO_PWR_MODE1  0x3E
#define MAX77660_REG_LDO_PWR_MODE2  0x3F
#define MAX77660_REG_LDO_PWR_MODE3  0x40
#define MAX77660_REG_LDO_PWR_MODE4  0x41
#define MAX77660_REG_LDO_PWR_MODE5  0x42

#define BUCK_POWER_MODE_MASK    0x3
#define BUCK_POWER_MODE_SHIFT    0
#define LDO_POWER_MODE_MASK        0x3
#define LDO_POWER_MODE_SHIFT    0
#define SW_POWER_MODE_MASK     0
#define SW_POWER_MODE_SHIFT     0

/* LDO Configuration 3 */
#define TRACK4_MASK            0x20
#define TRACK4_SHIFT            5

/* Voltage */
#define SDX_VOLT_MASK            0xFF
#define SD1_VOLT_MASK            0x3F
#define LDO_VOLT_MASK            0x3F

#define BIT(x)                  (1 << x)

#define FPS_SRC_MASK            (BIT(6) | BIT(5) | BIT(4))
#define FPS_SRC_SHIFT             4
#define FPS_PU_PERIOD_MASK        (BIT(2) | BIT(3))
#define FPS_PU_PERIOD_SHIFT         2
#define FPS_PD_PERIOD_MASK        (BIT(0) | BIT(1))
#define FPS_PD_PERIOD_SHIFT         0

#define CNFG_FPS_AP_OFF_TU_MASK   (BIT(6) | BIT(5) | BIT(4))
#define CNFG_FPS_AP_OFF_TU_SHIFT   4
#define CNFG_FPS_AP_OFF_TD_MASK   (BIT(0) | BIT(1) | BIT(2))
#define CNFG_FPS_AP_OFF_TD_SHIFT   0

#define CNFG_FPS_AP_SLP_TU_MASK   (BIT(6) | BIT(5) | BIT(4))
#define CNFG_FPS_AP_SLP_TU_SHIFT   4
#define CNFG_FPS_AP_SLP_TD_MASK   (BIT(0) | BIT(1) | BIT(2))
#define CNFG_FPS_AP_SLP_TD_SHIFT   0

#define CNFG_FPS_6_TU_MASK          (BIT(6) | BIT(5) | BIT(4))
#define CNFG_FPS_6_TU_SHIFT        4
#define CNFG_FPS_6_TD_MASK          (BIT(0) | BIT(1) | BIT(2))
#define CNFG_FPS_6_TD_SHIFT        0

#define BUCK6_7_CNFG_ADE_MASK       BIT(7)
#define BUCK6_7_CNFG_ADE_SHIFT      7
#define BUCK6_7_CNFG_FPWM_MASK      BIT(6)
#define BUCK6_7_CNFG_FPWM_SHIFT     6
#define BUCK6_7_CNFG_VOUT_MASK      0x3F    /* BIT(0-5) */
#define BUCK6_7_CNFG_VOUT_SHIFT     0
#define BUCK1_5_CNFG_RAMP_MASK     (BIT(7)|BIT(6))
#define BUCK1_5_CNFG_RAMP_SHIFT     6
#define BUCK1_5_CNFG_ADE_MASK       BIT(3)
#define BUCK1_5_CNFG_ADE_SHIFT      3
#define BUCK1_5_CNFG_FPWM_MASK      BIT(2)
#define BUCK1_5_CNFG_FPWM_SHIFT     2
#define BUCK1_5_CNFG_DVFS_EN_MASK   BIT(1)
#define BUCK1_5_CNFG_DVFS_EN_SHIFT  1
#define BUCK1_5_CNFG_FSRADE_MASK    BIT(0)
#define BUCK1_5_CNFG_FSRADE_SHIFT   0

#define LDO1_18_CNFG_ADE_MASK       BIT(6)
#define LDO1_18_CNFG_ADE_SHIFT      6
#define LDO1_18_CNFG_VOUT_MASK      0x3F  /*  BIT(0-5) */
#define LDO1_18_CNFG_VOUT_SHIFT     0
/* Voltage */
#define SDX_VOLT_MASK            0xFF
#define SD1_VOLT_MASK            0x3F
#define LDO_VOLT_MASK            0x3F

#define Max77660_SHIFT_INT3_STATUS_VBUS 7

enum {

    VOLT_REG = 0,
    CFG_REG,
    FPS_REG,
    PWR_MODE_REG
};

typedef struct Max77660PmuInfoRec *Max77660PmuInfoHandle;

typedef struct Max77660PmuOpsRec {
    NvBool (*IsRailEnabled)(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id);
    NvBool (*EnableRail)(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                         NvU32 *DelayUS);
    NvBool (*DisableRail)(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                          NvU32 *DelayUS);
    NvBool (*SetRailVoltage)(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                             NvU32 new_uV, NvU32 *DelayUS);
    NvBool (*GetRailVoltage)(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                             NvU32 *uV);
} Max77660PmuOps;

typedef struct Max77660PmuPropsRec {
    NvU32 Id;
    NvU8 Type;

    NvU32 MinUV;
    NvU32 MaxUV;
    NvU32 StepUV;

    NvU8 VoltReg;
    NvU8 CfgReg;
    NvU8 FpsReg;
    NvU8 PwrModeReg;

    NvU8 FpsSrc;
    NvU8 VoltMask;

    NvU8 PowerMode;
    NvU8 PowerModeMask;
    NvU8 PowerModeShift;

    /* Power rail specific turn-on delay */
    NvU32 Delay;
    Max77660PmuOps *pOps;
} Max77660PmuProps;

typedef struct Max77660RailInfoRec {
    Max77660PmuSupply Id;
    NvU32 SystemRailId;
    NvU8 PowerMode;
    int UserCount;
    NvBool IsValid;
    Max77660PmuProps *pPmuProps;
    NvOdmPmuRailProps OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Max77660RailInfo;

typedef struct Max77660PmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    NvU8  SwOTPVersion;
    Max77660RailInfo RailInfo[Max77660PmuSupply_Num];
} Max77660PmuInfo;

static NvBool Max77660IsRailEnabled(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id);
static NvBool Max77660RailEnable(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                 NvU32 *DelayUS);
static NvBool Max77660RailDisable(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                  NvU32 *DelayUS);
static NvBool Max77660RailSetVoltage(Max77660PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 new_uV, NvU32 *DelayUS);
static NvBool Max77660RailGetVoltage(Max77660PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 *uV);
static NvBool Max77660IsSwRailEnabled(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id);
static NvBool Max77660SwRailEnable(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                 NvU32 *DelayUS);
static NvBool Max77660SwRailDisable(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                  NvU32 *DelayUS);
void Max77660SetInterruptMasks(NvOdmPmuDriverInfoHandle hPmuDriver);
void NvMlUsbfSetVbusOverrideT1xx(NvBool vbusStatus);


static Max77660PmuOps s_LdoPmuOps = {
    Max77660IsRailEnabled,
    Max77660RailEnable,
    Max77660RailDisable,
    Max77660RailSetVoltage,
    Max77660RailGetVoltage,
};

static Max77660PmuOps s_SwPmuOps = {
    Max77660IsSwRailEnabled,
    Max77660SwRailEnable,
    Max77660SwRailDisable,
    NULL,
    NULL,
};

#define REG_DATA(_Id, _Type, _MinUV, _MaxUV, _StepUV, _VoltReg, _CfgReg, \
                 _FpsReg, _VoltMask, _PowerModeMask, _PowerModeShift, _pOps) \
    { \
            Max77660PmuSupply_##_Id, RAIL_TYPE_##_Type, \
            _MinUV, _MaxUV, _StepUV, _VoltReg, _CfgReg, _FpsReg, 0, 0, _VoltMask, \
            POWER_MODE_NORMAL, _PowerModeMask, _PowerModeShift, 500, _pOps, \
    }

static Max77660PmuProps s_Max77660PmuProps[] = {
    REG_DATA(Invalid, INVALID, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL),
    REG_DATA(BUCK1, BUCK, 600000, 1500000,
    6250, MAX77660_REG_BUCK1_VOUT, MAX77660_REG_BUCK1_CNFG, MAX77660_REG_FPS_BUCK1,
    SDX_VOLT_MASK, BUCK_POWER_MODE_MASK, BUCK_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(BUCK2, BUCK, 600000, 1500000,
    6250, MAX77660_REG_BUCK2_VOUT, MAX77660_REG_BUCK2_CNFG, MAX77660_REG_FPS_BUCK2,
    SDX_VOLT_MASK, BUCK_POWER_MODE_MASK, BUCK_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(BUCK3, BUCK, 600000, 3787500,
    12500, MAX77660_REG_BUCK3_VOUT, MAX77660_REG_BUCK3_CNFG, MAX77660_REG_FPS_BUCK3,
    SDX_VOLT_MASK, BUCK_POWER_MODE_MASK, BUCK_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(BUCK4, BUCK, 800000, 1500000,
    6250, MAX77660_REG_BUCK4_VOUT, MAX77660_REG_BUCK4_CNFG, MAX77660_REG_FPS_BUCK4,
    SDX_VOLT_MASK, BUCK_POWER_MODE_MASK, BUCK_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(BUCK5, BUCK, 600000, 3787500,
    12500, MAX77660_REG_BUCK5_VOUT, MAX77660_REG_BUCK5_CNFG, MAX77660_REG_FPS_BUCK5,
    SDX_VOLT_MASK, BUCK_POWER_MODE_MASK, BUCK_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(BUCK6, BUCK, 1000000, 4000000,
    50000, MAX77660_REG_BUCK6_VOUT, MAX77660_REG_BUCK6_CNFG, MAX77660_REG_FPS_BUCK6,
    SD1_VOLT_MASK, BUCK_POWER_MODE_MASK, BUCK_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(BUCK7, BUCK, 1000000, 4000000,
    50000, MAX77660_REG_BUCK7_VOUT, MAX77660_REG_BUCK7_CNFG, MAX77660_REG_FPS_BUCK7,
    SD1_VOLT_MASK, BUCK_POWER_MODE_MASK, BUCK_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO1, LDO_N, 600000, 2175000,
    25000, MAX77660_REG_LDO1_CNFG, MAX77660_REG_LDO1_CNFG, MAX77660_REG_FPS_LDO1,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO2, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO2_CNFG, MAX77660_REG_LDO2_CNFG, MAX77660_REG_FPS_LDO2,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO3, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO3_CNFG, MAX77660_REG_LDO3_CNFG, MAX77660_REG_FPS_LDO3,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO4, LDO_P, 800000, 3950000,
    12500, MAX77660_REG_LDO4_CNFG, MAX77660_REG_LDO4_CNFG, MAX77660_REG_FPS_LDO4,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO5, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO5_CNFG, MAX77660_REG_LDO5_CNFG, MAX77660_REG_FPS_LDO5,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO6, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO6_CNFG, MAX77660_REG_LDO6_CNFG, MAX77660_REG_FPS_LDO6,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO7, LDO_N, 600000, 2175000,
    50000, MAX77660_REG_LDO7_CNFG, MAX77660_REG_LDO7_CNFG, MAX77660_REG_FPS_LDO7,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO8, LDO_N, 600000, 2175000,
    50000, MAX77660_REG_LDO8_CNFG, MAX77660_REG_LDO8_CNFG, MAX77660_REG_FPS_LDO8,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO9, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO9_CNFG, MAX77660_REG_LDO9_CNFG, MAX77660_REG_FPS_LDO9,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO10, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO10_CNFG, MAX77660_REG_LDO10_CNFG, MAX77660_REG_FPS_LDO10,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO11, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO11_CNFG, MAX77660_REG_LDO11_CNFG, MAX77660_REG_FPS_LDO11,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO12, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO12_CNFG, MAX77660_REG_LDO12_CNFG, MAX77660_REG_FPS_LDO12,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO13, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO13_CNFG, MAX77660_REG_LDO13_CNFG, MAX77660_REG_FPS_LDO13,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO14, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO14_CNFG, MAX77660_REG_LDO14_CNFG, MAX77660_REG_FPS_LDO14,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO15, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO15_CNFG, MAX77660_REG_LDO15_CNFG, MAX77660_REG_FPS_LDO15,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO16, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO16_CNFG, MAX77660_REG_LDO16_CNFG, MAX77660_REG_FPS_LDO16,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO17, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO17_CNFG, MAX77660_REG_LDO17_CNFG, MAX77660_REG_FPS_LDO17,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(LDO18, LDO_P, 800000, 3950000,
    50000, MAX77660_REG_LDO18_CNFG, MAX77660_REG_LDO18_CNFG, MAX77660_REG_FPS_LDO18,
    LDO_VOLT_MASK, LDO_POWER_MODE_MASK, LDO_POWER_MODE_SHIFT, &s_LdoPmuOps),
    REG_DATA(SW1, SW, 0, 1800000,
    0, MAX77660_REG_SW_EN, MAX77660_REG_SW1_CNFG, MAX77660_REG_FPS_NONE,
    0, SW_POWER_MODE_MASK, SW_POWER_MODE_SHIFT, &s_SwPmuOps),
    REG_DATA(SW2, SW, 0, 1800000,
    0, MAX77660_REG_SW_EN, MAX77660_REG_SW2_CNFG, MAX77660_REG_FPS_NONE,
    0, SW_POWER_MODE_MASK, SW_POWER_MODE_SHIFT, &s_SwPmuOps),
    REG_DATA(SW3, SW, 0, 1800000,
    0, MAX77660_REG_SW_EN, MAX77660_REG_SW3_CNFG, MAX77660_REG_FPS_NONE,
    0, SW_POWER_MODE_MASK, SW_POWER_MODE_SHIFT, &s_SwPmuOps),
    REG_DATA(SW4, SW, 0, 1200000,
    0, MAX77660_REG_INVALID, MAX77660_REG_SW4_CNFG, MAX77660_REG_FPS_NONE,
    0, SW_POWER_MODE_MASK, SW_POWER_MODE_SHIFT, &s_SwPmuOps),
    REG_DATA(SW5, SW, 0, 1200000,
    0, MAX77660_REG_SW_EN, MAX77660_REG_SW5_CNFG, MAX77660_REG_FPS_NONE,
    0, SW_POWER_MODE_MASK, SW_POWER_MODE_SHIFT, &s_SwPmuOps),
};

static int Max77660SetPowerMode(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                NvU8 PowerMode)
{
    Max77660PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvU8 Addr = 0;
    NvU8 Mask = pPmuProps->PowerModeMask;
    NvU8 Shift = pPmuProps->PowerModeShift;
    int Ret = 0;

    if (pPmuProps->Type == RAIL_TYPE_SW)
        Addr = pPmuProps->VoltReg;
    else
        Addr = pPmuProps->PwrModeReg;

    Ret = NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                             Max77660_I2C_SPEED, Addr, PowerMode << Shift, Mask);
    if (Ret < 0)
        return Ret;
    pPmuProps->PowerMode = PowerMode;
    return Ret;
}

static NvU8 Max77660GetPowerMode(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id)
{
    Max77660PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvU8 Addr = 0;
    NvU8 Mask = pPmuProps->PowerModeMask;
    NvU8 Shift = pPmuProps->PowerModeShift;
    NvU8 Val = 0;
    int Ret = 0;

    if (pPmuProps->Type == RAIL_TYPE_SW)
        Addr = pPmuProps->VoltReg;
    else
        Addr = pPmuProps->PwrModeReg;

    Ret = NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                           Max77660_I2C_SPEED, Addr, &Val);
    if (Ret < 0)
        return POWER_MODE_INVALID;
    pPmuProps->PowerMode = (Val & Mask) >> Shift;
    return pPmuProps->PowerMode;
}

static NvBool Max77660IsSwRailEnabled(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id)
{
    Max77660PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvU8 idx, val, ret;
    NvU8 Addr = pPmuProps->VoltReg;
    if(Id == Max77660PmuSupply_SW4)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }
    idx = (Id - Max77660PmuSupply_SW1) - (Id == Max77660PmuSupply_SW5)?1:0;
    ret = NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                           Max77660_I2C_SPEED, Addr, &val);

    return val & (1 << idx);
}

static NvBool Max77660SwRailEnable(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                 NvU32 *DelayUS)
{
    Max77660PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvU8 idx, ret;
    NvU8 Addr = pPmuProps->VoltReg;
    if(Id == Max77660PmuSupply_SW4)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }
    idx = (Id - Max77660PmuSupply_SW1) - (Id == Max77660PmuSupply_SW5)?1:0;

    ret = NvOdmPmuI2cSetBits(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                             Max77660_I2C_SPEED, Addr, 1 << idx);
    return ret;
}

static NvBool Max77660SwRailDisable(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                  NvU32 *DelayUS)
{
    Max77660PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvU8 idx, ret;
    NvU8 Addr = pPmuProps->VoltReg;
    if(Id == Max77660PmuSupply_SW4)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }
    idx = (Id - Max77660PmuSupply_SW1) - (Id == Max77660PmuSupply_SW5)?1:0;
    ret = NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                           Max77660_I2C_SPEED, Addr, 1 << idx, 1 << idx);
    return ret;
}

static NvBool Max77660IsRailEnabled(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id)
{
    Max77660PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvBool Ret = NV_TRUE;

    if (pPmuProps->FpsSrc != FPS_SRC_NONE) {
        NvOdmOsDebugPrintf("%s() Rail%u is using FPS%u\n",
                           __func__, pPmuProps->Id, pPmuProps->FpsSrc);
        return Ret;
    }

    if (Max77660GetPowerMode(hMaxPmuInfo, Id) == POWER_MODE_DISABLE)
        Ret = NV_FALSE;

    return Ret;
}

static NvBool Max77660RailEnable(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                 NvU32 *DelayUS)
{
    Max77660PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvBool Ret = NV_TRUE;

    if (pPmuProps->FpsSrc != FPS_SRC_NONE) {
        NvOdmOsDebugPrintf("%s() Rail%u is using FPS%u\n",
                           __func__, pPmuProps->Id, pPmuProps->FpsSrc);
        return Ret;
    }

    Ret = Max77660SetPowerMode(hMaxPmuInfo, Id, POWER_MODE_NORMAL);
    if (DelayUS)
        *DelayUS = pPmuProps->Delay;

    return Ret;
}

static NvBool Max77660RailDisable(Max77660PmuInfoHandle hMaxPmuInfo, NvU32 Id,
                                  NvU32 *DelayUS)
{
    Max77660PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvBool Ret = NV_TRUE;

    if (pPmuProps->FpsSrc != FPS_SRC_NONE) {
        NvOdmOsDebugPrintf("%s() Rail%u is using FPS%u\n",
                           __func__, pPmuProps->Id, pPmuProps->FpsSrc);
        return Ret;
    }

    Ret = Max77660SetPowerMode(hMaxPmuInfo, Id, POWER_MODE_DISABLE);
    if (DelayUS)
        *DelayUS = pPmuProps->Delay;

    return Ret;
}

static NvBool Max77660RailSetVoltage(Max77660PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 new_uV, NvU32 *DelayUS)
{
    Max77660PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvU32 Val;
    NvBool Ret;

    if ((new_uV < pPmuProps->MinUV) || (new_uV > pPmuProps->MaxUV))
        return NV_FALSE;

    Val = (new_uV - pPmuProps->MinUV) / pPmuProps->StepUV;
    Ret = NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                             Max77660_I2C_SPEED, pPmuProps->VoltReg, Val,
                             pPmuProps->VoltMask);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in updating voltage register 0x%02x\n",
                      __func__, pPmuProps->VoltReg);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->Delay;
    return Ret;
}

static NvBool Max77660RailGetVoltage(Max77660PmuInfoHandle hMaxPmuInfo,
                                     NvU32 Id, NvU32 *uV)
{
    Max77660PmuProps *pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    NvU8 Val;
    NvBool Ret;

    Ret = NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                           Max77660_I2C_SPEED, pPmuProps->VoltReg, &Val);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in reading voltage register 0x%02x\n",
                    __func__, pPmuProps->VoltReg);
        return Ret;
    }
    Val &= Val & pPmuProps->VoltMask;
    *uV = Val * pPmuProps->StepUV + pPmuProps->MinUV;
    return NV_TRUE;
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule, NvU32 *pI2cInstance,
                                 NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(Max77660PMUGUID);
    if (pConnectivity)
    {
        for (i = 0; i < pConnectivity->NumAddress; i++)
        {
            if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_I2c)
            {
                *pI2cModule   = NvOdmIoModule_I2c;
                *pI2cInstance = pConnectivity->AddressList[i].Instance;
                *pI2cAddress  = pConnectivity->AddressList[i].Address;
                return NV_TRUE;
            }
        }
    }
    NvOdmOsPrintf("%s: The system did not discover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

static NvU32 GetMaximumCurrentFromOtp(Max77660PmuInfoHandle hMaxPmuInfo,
                   NvU32 VddId)
{
    switch(hMaxPmuInfo->SwOTPVersion) {
        case 0x02:
        case 0x04:
            if(VddId == Max77660PmuSupply_BUCK1)
                return 3000;
            if(VddId == Max77660PmuSupply_BUCK2)
                return 8000;
            break;
        case 0x08:
        case 0x09:
        case 0x0B:
            if(VddId == Max77660PmuSupply_BUCK1)
                return 4000;
            if(VddId == Max77660PmuSupply_BUCK2)
                return 8000;
            break;
        default:
            NvOdmOsDebugPrintf("\nMax77660::Pmu OTP Version NOT supported\n");
    }
    return 3000;
}

NvOdmPmuDriverInfoHandle Max77660PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                                               NvOdmPmuRailProps *pOdmRailProps,
                                               int NrOfOdmRailProps)
{
    Max77660PmuInfoHandle hMaxPmuInfo;
    int i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvBool Ret;
    NvU32 Id;
    NvU8 Val;
    NvU8 idx;
    NvU8 Glblcnfg = 0;
    NvU8 LineState = 0;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    /* Information for all rail is required */
    if (NrOfOdmRailProps != Max77660PmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }
    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);

    if (!Ret)
        return NULL;

    hMaxPmuInfo = NvOdmOsAlloc(sizeof(Max77660PmuInfo));
    if (!hMaxPmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hMaxPmuInfo, 0, sizeof(Max77660PmuInfo));

    hMaxPmuInfo->hOdmPmuI2c =NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hMaxPmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s() Error in NvOdmPmuI2cOpen()\n", __func__);
        goto I2cOpenFailed;
    }

    hMaxPmuInfo->DeviceAddr = I2cAddress;

    // Read OTP version
    Ret = NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, I2cAddress,
                Max77660_I2C_SPEED, Max77660_REG_CID4,
                &hMaxPmuInfo->SwOTPVersion);
    if (!Ret)
    {
        NV_ASSERT(0);
        NvOdmOsPrintf("%s() Error in reading the REG_CID4 register\n"
            "Defaulting to 0x02", __func__);
    }

     /* disable rtc alarm */
     /* clear time */
    Ret = NvOdmPmuI2cWrite8(hMaxPmuInfo->hOdmPmuI2c, MAX77660_RTC_ADDRESS,
                Max77660_I2C_SPEED, Max77660_REG_RTCAE1, 0x00);
    if (!Ret)
    {
        NV_ASSERT(0);
        NvOdmOsPrintf("%s() Error in writing to RTCAE1 register\n", __func__);
    }
     /* clear rtc */
    Ret = NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, MAX77660_RTC_ADDRESS,
                Max77660_I2C_SPEED, Max77660_REG_RTCCTRLM, &Val);
    if (!Ret)
    {
        NV_ASSERT(0);
        NvOdmOsPrintf("%s() Error in reading RTCCTRLM register\n", __func__);
    }

    hMaxPmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hMaxPmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hMaxPmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hMaxPmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 0; i < Max77660PmuSupply_Num; ++i)
    {
        Id = pOdmRailProps[i].VddId;
        if (Id == Max77660PmuSupply_Invalid)
                continue;

        if (pOdmRailProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been provided, "
                        "Ignoring this entry\n", __func__, Id);
            continue;
        }

        if (Id != s_Max77660PmuProps[Id].Id)
        {
            NvOdmOsPrintf("%s(): The entries for vdd %d are not in proper sequence, "
                        "got %d, Ignoring this entry\n", __func__, Id, s_Max77660PmuProps[Id].Id);
            continue;
        }

        if (hMaxPmuInfo->RailInfo[Id].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                        "Ignoring this entry\n", __func__, Id);
            continue;
        }

        hMaxPmuInfo->RailInfo[Id].pPmuProps = &s_Max77660PmuProps[Id];

        NvOdmOsMemcpy(&hMaxPmuInfo->RailInfo[Id].OdmProps,
                &pOdmRailProps[Id], sizeof(NvOdmPmuRailProps));

        /* Update Power Mode register mask and offset */
        if (hMaxPmuInfo->RailInfo[Id].pPmuProps->Type == RAIL_TYPE_BUCK)
        {
            idx = hMaxPmuInfo->RailInfo[Id].pPmuProps->Id - Max77660PmuSupply_BUCK1;
        /*  4 Bucks Pwr Mode in 1 register */
            hMaxPmuInfo->RailInfo[Id].pPmuProps->PwrModeReg =
                        MAX77660_REG_BUCK_PWR_MODE1 + (idx/4);
            hMaxPmuInfo->RailInfo[Id].pPmuProps->PowerModeShift = (idx%4)*2;
            hMaxPmuInfo->RailInfo[Id].pPmuProps->PowerModeMask =
                        hMaxPmuInfo->RailInfo[Id].pPmuProps->PowerModeMask <<
                        (hMaxPmuInfo->RailInfo[Id].pPmuProps->PowerModeShift);
        }

        if ((hMaxPmuInfo->RailInfo[Id].pPmuProps->Type == RAIL_TYPE_LDO_N) ||
            (hMaxPmuInfo->RailInfo[Id].pPmuProps->Type == RAIL_TYPE_LDO_P))
        {
            idx = hMaxPmuInfo->RailInfo[Id].pPmuProps->Id - Max77660PmuSupply_LDO1;
            /* 4 LDOs Pwr Mode in 1 register  */
            hMaxPmuInfo->RailInfo[Id].pPmuProps->PwrModeReg =
                        MAX77660_REG_LDO_PWR_MODE1 + (idx/4);
            hMaxPmuInfo->RailInfo[Id].pPmuProps->PowerModeShift = (idx%4)*2;
            hMaxPmuInfo->RailInfo[Id].pPmuProps->PowerModeMask =
                        hMaxPmuInfo->RailInfo[Id].pPmuProps->PowerModeMask <<
                        (hMaxPmuInfo->RailInfo[Id].pPmuProps->PowerModeShift);
        }
        /* Update FPS Source */
        if (hMaxPmuInfo->RailInfo[Id].pPmuProps->FpsReg == MAX77660_REG_FPS_NONE)
                hMaxPmuInfo->RailInfo[Id].pPmuProps->FpsSrc = FPS_SRC_NONE;
        else
        {
            Ret = NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                               Max77660_I2C_SPEED,
                               hMaxPmuInfo->RailInfo[Id].pPmuProps->FpsReg,
                               &Val);
            if (!Ret)
                NvOdmOsPrintf("%s() Error in reading the register 0x%02x\n",
                    __func__, hMaxPmuInfo->RailInfo[Id].pPmuProps->FpsReg);
            hMaxPmuInfo->RailInfo[Id].pPmuProps->FpsSrc = (Val & FPS_SRC_MASK) >>
                                                        FPS_SRC_SHIFT;
        }

        hMaxPmuInfo->RailInfo[Id].Caps.OdmProtected =
                        hMaxPmuInfo->RailInfo[Id].OdmProps.IsOdmProtected;

        /* Maximum current */
        if ((Id == Max77660PmuSupply_BUCK1) || (Id == Max77660PmuSupply_BUCK2))
            hMaxPmuInfo->RailInfo[Id].Caps.MaxCurrentMilliAmp =
                                      GetMaximumCurrentFromOtp(hMaxPmuInfo, Id);

        if (hMaxPmuInfo->RailInfo[Id].OdmProps.Min_mV)
             hMaxPmuInfo->RailInfo[Id].Caps.MinMilliVolts =
                        NV_MAX(hMaxPmuInfo->RailInfo[Id].OdmProps.Min_mV,
                           hMaxPmuInfo->RailInfo[Id].pPmuProps->MinUV/1000);
        else
             hMaxPmuInfo->RailInfo[Id].Caps.MinMilliVolts =
                        hMaxPmuInfo->RailInfo[Id].pPmuProps->MinUV/1000;

        if (hMaxPmuInfo->RailInfo[Id].OdmProps.Max_mV)
             hMaxPmuInfo->RailInfo[Id].Caps.MaxMilliVolts =
                        NV_MIN(hMaxPmuInfo->RailInfo[Id].OdmProps.Max_mV,
                           hMaxPmuInfo->RailInfo[Id].pPmuProps->MaxUV/1000);
        else
             hMaxPmuInfo->RailInfo[Id].Caps.MaxMilliVolts =
                           hMaxPmuInfo->RailInfo[Id].pPmuProps->MaxUV/1000;

        hMaxPmuInfo->RailInfo[Id].Caps.StepMilliVolts =
                           hMaxPmuInfo->RailInfo[Id].pPmuProps->StepUV/1000;

        if (hMaxPmuInfo->RailInfo[Id].OdmProps.Setup_mV)
             hMaxPmuInfo->RailInfo[Id].Caps.requestMilliVolts =
                        hMaxPmuInfo->RailInfo[Id].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                        "Ignoring this entry\n", __func__,
                         hMaxPmuInfo->RailInfo[Id].OdmProps.Setup_mV, Id);
            continue;
        }
        hMaxPmuInfo->RailInfo[Id].UserCount  = 0;
        hMaxPmuInfo->RailInfo[Id].IsValid = NV_TRUE;
        hMaxPmuInfo->RailInfo[Id].SystemRailId = pOdmRailProps[Id].SystemRailId;
    }
    //clear the watch dog timer to change its period to 128 seconds
    NvOdmPmuI2cWrite8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
            Max77660_I2C_SPEED, Max77660_REG_WDTC_SYS, 1);
    //set its values as 128 seconds
    NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                Max77660_I2C_SPEED, Max77660_REG_TWD_SYS, &Glblcnfg);
    Glblcnfg |= 0x30;
    NvOdmPmuI2cWrite8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                Max77660_I2C_SPEED, Max77660_REG_TWD_SYS, Glblcnfg);
    NvOdmPmuI2cWrite8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
            Max77660_I2C_SPEED, Max77660_REG_WDTC_SYS, 1);

    // Set Interrupt Masks
    Max77660SetInterruptMasks((NvOdmPmuDriverInfoHandle)hMaxPmuInfo);

    NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, 0x44,
                    Max77660_I2C_SPEED, Max77660_REG_CHGSTAT, &LineState);
    NvMlUsbfSetVbusOverrideT1xx(!(LineState & (1 << Max77660_SHIFT_INT3_STATUS_VBUS)));

    return (NvOdmPmuDriverInfoHandle)hMaxPmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hMaxPmuInfo->hOdmPmuService);
PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hMaxPmuInfo->hOdmPmuI2c);
I2cOpenFailed:
    NvOdmOsFree(hMaxPmuInfo);
    return NULL;
}

void Max77660PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Max77660PmuInfoHandle hMaxPmuInfo = (Max77660PmuInfoHandle)hPmuDriver;

    NvOdmServicesPmuClose(hMaxPmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hMaxPmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hMaxPmuInfo);
}

void Max77660PmuGetCaps(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                        NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Max77660PmuInfoHandle hMaxPmuInfo = (Max77660PmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(Id < Max77660PmuSupply_Num);
    NvOdmOsMemcpy(pCapabilities, &hMaxPmuInfo->RailInfo[Id].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Max77660PmuGetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                             NvU32* pMilliVolts)
{
    Max77660PmuInfoHandle hMaxPmuInfo = (Max77660PmuInfoHandle)hPmuDriver;
    Max77660PmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(Id < Max77660PmuSupply_Num);

    pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    *pMilliVolts = 0;
    if (pPmuProps->pOps->IsRailEnabled(hMaxPmuInfo, Id)) {
        if(pPmuProps->Type == RAIL_TYPE_SW)
            *pMilliVolts = pPmuProps->MaxUV/1000;
        else
        {
            Status = pPmuProps->pOps->GetRailVoltage(hMaxPmuInfo, Id, &uV);
            if (Status) {
                *pMilliVolts = uV /1000;
            }
        }
    }
    return Status;
}

NvBool Max77660PmuSetVoltage(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32 Id,
                             NvU32 MilliVolts, NvU32* pSettleMicroSeconds)
{
    Max77660PmuInfoHandle hMaxPmuInfo = (Max77660PmuInfoHandle)hPmuDriver;
    Max77660PmuProps *pPmuProps;
    Max77660RailInfo *pRailInfo;
    NvBool Status = NV_TRUE;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(Id < Max77660PmuSupply_Num);

    pRailInfo = &hMaxPmuInfo->RailInfo[Id];
    pPmuProps = hMaxPmuInfo->RailInfo[Id].pPmuProps;
    if (pSettleMicroSeconds)
        *pSettleMicroSeconds = 0;

    if (pRailInfo->OdmProps.IsOdmProtected == NV_TRUE)
    {
        NvOdmOsPrintf("%s:The voltage rail %d is protected and can not be set.\n",
                    __func__, Id);
        return NV_TRUE;
    }

    if ((MilliVolts != ODM_VOLTAGE_OFF) &&
        ((MilliVolts > pRailInfo->Caps.MaxMilliVolts) ||
         (MilliVolts < pRailInfo->Caps.MinMilliVolts)))
    {
        NvOdmOsPrintf("%s:The required voltage is not supported..vddRail: %d,"
                         " MilliVolts: %d\n", __func__, Id, MilliVolts);
        return NV_TRUE;
    }

    NvOdmOsMutexLock(hMaxPmuInfo->hPmuMutex);
    if (MilliVolts == ODM_VOLTAGE_OFF)
    {
        Status = NV_TRUE;
        if (!pRailInfo->UserCount)
        {
            NvOdmOsPrintf("%s:The imbalance in voltage off No user for rail %d\n",
                        __func__, Id);
            goto End;
        }

        if (pRailInfo->UserCount == 1)
        {
            NvOdmServicesPmuSetSocRailPowerState(hMaxPmuInfo->hOdmPmuService,
                        hMaxPmuInfo->RailInfo[Id].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hMaxPmuInfo, Id,
                                            pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hMaxPmuInfo->hOdmPmuService,
                        hMaxPmuInfo->RailInfo[Id].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);

        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hMaxPmuInfo, Id,
                                MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hMaxPmuInfo, Id,
                                pSettleMicroSeconds);

        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                        __func__, Id);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hMaxPmuInfo->hPmuMutex);
    return Status;
}

void Max77660PmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Max77660PmuInfoHandle hMaxPmuInfo;

    NV_ASSERT(hPmuDriver);
    hMaxPmuInfo = (Max77660PmuInfoHandle) hPmuDriver;

    NvOdmPmuI2cSetBits(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
      Max77660_I2C_SPEED, Max77660_REG_GLBLCNFG0, SFT_OFF_RST);

    NvOdmOsSleepMS(32);

    NV_ASSERT(0);
}

static NvBool s_ChargingMode = NV_FALSE;
void Max77660SetChargingInterrupt(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Max77660PmuInfoHandle hMaxPmuInfo;

    if(!hPmuDriver)
        return;

    hMaxPmuInfo = (Max77660PmuInfoHandle) hPmuDriver;

    NvOdmOsPrintf("Enable required interrupts for charging \n");

    //Enable Power Key
    NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                Max77660_I2C_SPEED, Max77660_REG_GLBLINTM1, 0, (1<<6));

    //Enable USB Unplug
    NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, 0x44,
                Max77660_I2C_SPEED, Max77660_REG_CHGINTM, 0, (1<<4));

    //Enable Thermister interrupt
    NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, 0x44,
                Max77660_I2C_SPEED, Max77660_REG_CHGINTM, 0, (1<<1));
    s_ChargingMode = NV_TRUE;
}

void Max77660UnSetChargingInterrupt(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Max77660PmuInfoHandle hMaxPmuInfo;

    if(!hPmuDriver)
        return;

    hMaxPmuInfo = (Max77660PmuInfoHandle) hPmuDriver;

    //Disable Power Key
    NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                Max77660_I2C_SPEED, Max77660_REG_GLBLINTM1, 1, (1<<6));

    //Disable Thermister interrupt
    NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, 0x44,
                Max77660_I2C_SPEED, Max77660_REG_CHGINTM, 1, (1<<1));

    s_ChargingMode = NV_FALSE;
    NvOdmOsPrintf("Disable required interrupts for charging \n");
}


void Max77660SetInterruptMasks(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Max77660PmuInfoHandle hMaxPmuInfo;

    if(!hPmuDriver)
        return;

    hMaxPmuInfo = (Max77660PmuInfoHandle) hPmuDriver;

    // Disable IRQ_M
    NvOdmPmuI2cWrite8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                Max77660_I2C_SPEED, Max77660_REG_GLBLINTM1, 0xFE); // Max77660_REG_GLBLINTM1_MASK

    // Disable unwanted interrupts in GLBINTM2
    NvOdmPmuI2cWrite8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                Max77660_I2C_SPEED, Max77660_REG_GLBLINTM2, 0x3c); // Max77660_REG_GLBLINTM2_MASK

    // Disable unwanted interrupts in CHGINTM
    NvOdmPmuI2cWrite8(hMaxPmuInfo->hOdmPmuI2c, 0x44,
                Max77660_I2C_SPEED, Max77660_REG_CHGINTM, 0xED); // Max77660_REG_CHGINTM_MASK

    // Disable unwanted interrupts in INTTOPM1
    NvOdmPmuI2cWrite8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                Max77660_I2C_SPEED, Max77660_REG_INTTOPM1, 0x7a); // Max77660_REG_INTTOPM1_MASK
}

void Max77660PmuInterruptHandler(NvOdmPmuDeviceHandle hPmuDriver)
{
    Max77660PmuInfoHandle hMaxPmuInfo;
    NvU8 IntTop1 = 0;
    NvU8 GlblInt2 = 0;
    NvU8 GlblInt1 = 0;
    hMaxPmuInfo = (Max77660PmuInfoHandle) hPmuDriver;

    if(!hPmuDriver)
        return;

    /* We're only checking for WDT warning interrupt as of now.
       Handling other interrupts can be added on a need basis.
     */

    // First mask IRQ_M
    NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
            Max77660_I2C_SPEED, Max77660_REG_GLBLINTM1, 1, 1);

    // Read INTTOP1 to check for TOPSYSINT
    NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
            Max77660_I2C_SPEED, Max77660_REG_INTTOP1, &IntTop1);

    if(IntTop1 & 0x80)
    {
        NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                Max77660_I2C_SPEED, Max77660_REG_GLBLINT2, &GlblInt2);
        NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
            Max77660_I2C_SPEED, Max77660_REG_GLBLINT1, &GlblInt1);

        if(GlblInt2 & 0x2)
        {
            // Reset system watchdog timer
            NvOdmPmuI2cWrite8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                Max77660_I2C_SPEED, Max77660_REG_WDTC_SYS, 1);
            NvOdmOsPrintf("[%u] RESET WDT\n",NvOdmOsGetTimeMS());
        }
        else if(GlblInt2 & 0x1)
        {
            // Reset Charger watchdog timer
            NvOdmPmuI2cWrite8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                Max77660_I2C_SPEED, Max77660_REG_GLBLCNFG6, 1);
            NvOdmOsPrintf("[%u] RESET CHG-WDT\n",NvOdmOsGetTimeMS());
        }
        else
            if((GlblInt1 & 0x40) && s_ChargingMode)
        {
            NvOdmHandlePowerKeyEvent(NvOdmPowerKey_Pressed);
        }
    }

    if(IntTop1 & 0x04)     //Interrupt from charger block
    {
        NvU8 reg = 0;
        NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, 0x44,
                Max77660_I2C_SPEED, Max77660_REG_CHGINT, &reg);
        if (reg & 0x10)
        {
            NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, 0x44,
                Max77660_I2C_SPEED, Max77660_REG_CHGSTAT, &reg);
            if (reg & 0x10)
            {
                NvMlUsbfSetVbusOverrideT1xx(NV_FALSE);
                NvOdmOsPrintf("[%u] Unplug in bootloader \n",NvOdmOsGetTimeMS());
                if (s_ChargingMode)
                    NvOdmPmuPowerOff( hPmuDriver);
            }
            else
            {
                NvMlUsbfSetVbusOverrideT1xx(NV_TRUE);
                NvOdmOsPrintf("[%u] plug in bootloader \n",NvOdmOsGetTimeMS());
            }
        }

        if (reg & 0x02)
        {
           NvOdmPmuI2cRead8(hMaxPmuInfo->hOdmPmuI2c, 0x44,
                            Max77660_I2C_SPEED, Max77660_REG_CHGSTAT, &reg);
           if (reg & 0x02)
           {
              if (s_ChargingMode)
              {
                  NvOdmOsPrintf("Shutting down because thermal conditions are not ok");
                  NvOdmPmuPowerOff( hPmuDriver);
              }
           }
        }
    }

    // Unmask IRQ_M
    NvOdmPmuI2cUpdate8(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
            Max77660_I2C_SPEED, Max77660_REG_GLBLINTM1, 0, 1);
}

void Max77660PmuEnableWdt(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Max77660PmuInfoHandle hMaxPmuInfo;

    NV_ASSERT(hPmuDriver);
    hMaxPmuInfo = (Max77660PmuInfoHandle) hPmuDriver;

    NvOdmPmuI2cSetBits(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                       Max77660_I2C_SPEED, Max77660_REG_GLBLCNFG1, WDT_ENABLE);
}

void Max77660PmuDisableWdt(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Max77660PmuInfoHandle hMaxPmuInfo;

    NV_ASSERT(hPmuDriver);
    hMaxPmuInfo = (Max77660PmuInfoHandle) hPmuDriver;

    NvOdmPmuI2cClrBits(hMaxPmuInfo->hOdmPmuI2c, hMaxPmuInfo->DeviceAddr,
                       Max77660_I2C_SPEED, Max77660_REG_GLBLCNFG1, WDT_ENABLE);
}
