/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: TPS65914 pmu driver</b>
 *
 * @b Description: Implements the TPS65914 pmu drivers.
 *
 */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "tps65914/nvodm_pmu_tps65914_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "tps65914_reg.h"
#include "datetime.h"
#include "nvodm_charging.h"
#include "nvodm_query_gpio.h"
#include "nvodm_services.h"

#define TPS65914_PMUGUID NV_ODM_GUID('t','p','s','6','5','9','1','4')

#define TPS65914_I2C_SPEED 100

#define PALMAS_SMPS12_VOLTAGE_RANGE 0x80

#define PMU_E1735           0x06C7 //TI 913/4
#define PMU_E1736           0x06C8 //TI 913/4
#define PMU_E1936           0x0790 //TI 913/4
#define PMU_E1769           0x06E9 //TI 913/4
#define PMU_E1761           0x06E1 //TI 913/4
#define PROC_BOARD_E1780    0x06F4

static NvU8 PowerOnSource = 0;
static NvOdmBoardInfo s_ProcBoardInfo, s_PmuBoardInfo;

typedef struct Tps65914PmuInfoRec *Tps65914PmuInfoHandle;

typedef struct Tps65914PmuOpsRec {
    NvBool (*IsRailEnabled)(Tps65914PmuInfoHandle hTps65914PmuInfo, NvU32 VddId);
    NvBool (*EnableRail)(Tps65914PmuInfoHandle hTps65914PmuInfo, NvU32 VddId, NvU32 *DelayUS);
    NvBool (*DisableRail)(Tps65914PmuInfoHandle hTps65914PmuInfo, NvU32 VddId, NvU32 *DelayUS);
    NvBool (*SetRailVoltage)(Tps65914PmuInfoHandle hTps65914PmuInfo, NvU32 VddId, NvU32 new_uV,
            NvU32 *DelayUS);
    NvBool (*GetRailVoltage)(Tps65914PmuInfoHandle hTps65914PmuInfo, NvU32 VddId, NvU32 *uV);
} Tps65914PmuOps;

typedef struct Tps65914PmuPropsRec {
    NvU32           Id;

    /* Regulator register information.*/
    NvU8            ctrl_reg;
    NvU8            tstep_reg;
    NvU8            force_reg;
    NvU8            volt_reg;

    /* Pmu voltage props */
    NvU32           min_uV;
    NvU32           max_uV;
    NvU32           step_uV;
    NvU32           nsteps;

   /* multiplier set in vsel */
    NvU32           mult;

    /* Power rail specific turn-on delay */
    NvU32           delay;
    Tps65914PmuOps *pOps;
} Tps65914PmuProps;

typedef struct Tps65914RailInfoRec {
    TPS65914PmuSupply   VddId;
    NvU32               SystemRailId;
    NvU32               UserCount;
    NvBool              IsValid;
    Tps65914PmuProps    *pPmuProps;
    NvOdmPmuRailProps   OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Tps65914RailInfo;

typedef struct Tps65914PmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddrPage1;
    NvU32 DeviceAddrPage2;
    NvU32 DeviceAddrPage3;
    NvBool Smps123;
    NvBool Smps457;
    NvU8 ESMajorVersion;
    NvU8 ESMinorVersion;
    NvU8 SwOTPVersion;
    NvU8 DesignRev;
    Tps65914RailInfo RailInfo[TPS65914PmuSupply_Num];
} Tps65914PmuInfo;


struct batt_adc_gain{
	NvU32 adc_val;
	NvU32 gain;
};

static struct batt_adc_gain gain_table[] = {
	{1048,184}, {1064,200}, {1097,224}, {1105,233}, {1126,249}, {1140,258}, {1157,270}, {1172,278}, {1190,290},
	{1228,302}, {1229,310},

};

#define GAIN_NUM  (sizeof(gain_table) / sizeof(gain_table[0]))


extern void NvMlUsbfSetVbusOverrideT1xx(NvBool VbusStatus);

static NvBool Tps65914IsRailEnabled(Tps65914PmuInfoHandle hTps65914PmuInfo,
               NvU32 VddId);

static NvBool Tps65914RailEnable(Tps65914PmuInfoHandle hTps65914PmuInfo,
               NvU32 VddId, NvU32 *DelayUS);

static NvBool Tps65914RailDisable(Tps65914PmuInfoHandle hTps65914PmuInfo,
               NvU32 VddId, NvU32 *DelayUS);

static NvBool Tps65914LDOGetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Tps65914LDOSetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Tps65914SMPSGetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Tps65914SMPSSetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 uV, NvU32 *DelayUS);

static NvBool Tps65914GpioRailIsEnabled(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId);

static NvBool Tps65914GpioRailEnable(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 *DelayUS);

static NvBool Tps65914GpioRailDisable(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 *DelayUS);

static NvBool Tps65914GpioGetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Tps65914GpioSetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);


static Tps65914PmuOps s_PmuLDOOps = {
    Tps65914IsRailEnabled,
    Tps65914RailEnable,
    Tps65914RailDisable,
    Tps65914LDOSetVoltage,
    Tps65914LDOGetVoltage,
};

static Tps65914PmuOps s_PmuSMPSOps = {
    Tps65914IsRailEnabled,
    Tps65914RailEnable,
    Tps65914RailDisable,
    Tps65914SMPSSetVoltage,
    Tps65914SMPSGetVoltage,
};

static Tps65914PmuOps s_PmuGpioOps = {
    Tps65914GpioRailIsEnabled,
    Tps65914GpioRailEnable,
    Tps65914GpioRailDisable,
    Tps65914GpioSetVoltage,
    Tps65914GpioGetVoltage,
};

#define REG_DATA(_id, ctrl_reg, tstep_reg, force_reg, volt_reg,  \
                 _min_mV, _max_mV, _step_uV, _nsteps, _mult, _pOps, _delay)  \
{       \
    TPS65914PmuSupply_##_id, \
    TPS65914_##ctrl_reg,     \
    TPS65914_##tstep_reg,    \
    TPS65914_##force_reg,    \
    TPS65914_##volt_reg,     \
    _min_mV*1000,            \
    _max_mV*1000,            \
    _step_uV,                \
    _nsteps,                 \
    _delay,                  \
    _mult,                   \
    _pOps,                   \
}

/* Should be in same sequence as the enums are */
static const Tps65914PmuProps s_Tps65914PmuProps[] = {
    REG_DATA(Invalid, RFF_INVALID, RFF_INVALID, RFF_INVALID, RFF_INVALID,
                    0, 0, 0, 0, 0, NULL, 0),

    REG_DATA(SMPS12,  SMPS12_CTRL,  SMPS12_TSTEP,  SMPS12_FORCE, SMPS12_VOLTAGE,
                 500, 1650, 10000, 0, 1, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS123, SMPS12_CTRL,  SMPS12_TSTEP,  SMPS12_FORCE, SMPS12_VOLTAGE,
                 500, 1650, 10000, 0, 1, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS3,  SMPS3_CTRL,  SMPS3_TSTEP,  SMPS3_FORCE, SMPS3_VOLTAGE,
                 500, 1650, 10000, 0, 1, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS45,  SMPS45_CTRL,  SMPS45_TSTEP,  SMPS45_FORCE, SMPS45_VOLTAGE,
                 500, 1650, 10000, 0, 1, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS457, SMPS45_CTRL,  SMPS45_TSTEP,  SMPS45_FORCE, SMPS45_VOLTAGE,
                 500, 1650, 10000, 0, 1, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS6,  SMPS6_CTRL,  SMPS6_TSTEP,  SMPS6_FORCE, SMPS6_VOLTAGE,
                 500, 1650, 10000, 0, 1, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS7,  SMPS7_CTRL,  SMPS7_TSTEP,  SMPS7_FORCE, SMPS7_VOLTAGE,
                 500, 1650, 10000, 0, 1, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS8,  SMPS8_CTRL,  SMPS8_TSTEP,  SMPS8_FORCE, SMPS8_VOLTAGE,
                 500, 1650, 10000, 0, 1, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS9,  SMPS9_CTRL,  SMPS9_TSTEP,  SMPS9_FORCE, SMPS9_VOLTAGE,
                 500, 1650, 10000, 0, 1, &s_PmuSMPSOps, 0),
    REG_DATA(SMPS10,  SMPS10_CTRL,  SMPS_INVALID,  SMPS_INVALID, SMPS_INVALID,
                 500, 1650, 10000, 0, 0, &s_PmuSMPSOps, 0),

    REG_DATA(LDO1,  LDO1_CTRL,  LDO_INVALID,  LDO_INVALID, LDO1_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),
    REG_DATA(LDO2,  LDO2_CTRL,  LDO_INVALID,  LDO_INVALID, LDO2_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),
    REG_DATA(LDO3,  LDO3_CTRL,  LDO_INVALID,  LDO_INVALID, LDO3_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),
    REG_DATA(LDO4,  LDO4_CTRL,  LDO_INVALID,  LDO_INVALID, LDO4_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),
    REG_DATA(LDO5,  LDO5_CTRL,  LDO_INVALID,  LDO_INVALID, LDO5_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),
    REG_DATA(LDO6,  LDO6_CTRL,  LDO_INVALID,  LDO_INVALID, LDO6_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),
    REG_DATA(LDO7,  LDO7_CTRL,  LDO_INVALID,  LDO_INVALID, LDO7_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),
    REG_DATA(LDO8,  LDO8_CTRL,  LDO_INVALID,  LDO_INVALID, LDO8_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),
    REG_DATA(LDO9,  LDO9_CTRL,  LDO_INVALID,  LDO_INVALID, LDO9_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),
    REG_DATA(LDOLN, LDOLN_CTRL,  LDO_INVALID,  LDO_INVALID, LDOLN_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),
    REG_DATA(LDOUSB,LDOUSB_CTRL,  LDO_INVALID,  LDO_INVALID, LDOUSB_VOLTAGE,
                 900, 3300, 50000, 0, 0, &s_PmuLDOOps, 0),

    REG_DATA(GPIO0, GPIO_INVALID,  GPIO_INVALID, GPIO_INVALID, GPIO_INVALID,
                 0, 0, 0, 0, 0, &s_PmuGpioOps, 0),
    REG_DATA(GPIO1, GPIO_INVALID,  GPIO_INVALID, GPIO_INVALID, GPIO_INVALID,
                 0, 0, 0, 0, 0, &s_PmuGpioOps, 0),
    REG_DATA(GPIO2, GPIO_INVALID,  GPIO_INVALID, GPIO_INVALID, GPIO_INVALID,
                 0, 0, 0, 0, 0, &s_PmuGpioOps, 0),
    REG_DATA(GPIO3, GPIO_INVALID,  GPIO_INVALID, GPIO_INVALID, GPIO_INVALID,
                 0, 0, 0, 0, 0, &s_PmuGpioOps, 0),
    REG_DATA(GPIO4, GPIO_INVALID,  GPIO_INVALID, GPIO_INVALID, GPIO_INVALID,
                 900, 3300, 0, 0, 0, &s_PmuGpioOps, 0),
    REG_DATA(GPIO5, GPIO_INVALID,  GPIO_INVALID, GPIO_INVALID, GPIO_INVALID,
                 0, 0, 0, 0, 0, &s_PmuGpioOps, 0),
    REG_DATA(GPIO6, GPIO_INVALID,  GPIO_INVALID, GPIO_INVALID, GPIO_INVALID,
                 900, 3300, 0, 0, 0, &s_PmuGpioOps, 0),
    REG_DATA(GPIO7, GPIO_INVALID,  GPIO_INVALID, GPIO_INVALID, GPIO_INVALID,
                 0, 0, 0, 0, 0, &s_PmuGpioOps, 0)
};

static Tps65914PmuProps *PmuPropsTable;

static NvBool Tps65914IsRailEnabled(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId)
{
    NvU8 Control;
    NvBool ret;
    Tps65914PmuProps *pPmuProps;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hTps65914PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
            TPS65914_I2C_SPEED, pPmuProps->ctrl_reg, &Control);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the control register\n", __func__);
        return ret;
    }
    return ((Control & 1) == 1);
}

static NvBool Tps65914RailEnable(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps65914PmuProps *pPmuProps;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hTps65914PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cUpdate8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
            TPS65914_I2C_SPEED, pPmuProps->ctrl_reg, 1, 0x3);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the control register\n", __func__);
        return ret;
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps65914RailDisable(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps65914PmuProps *pPmuProps;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hTps65914PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cUpdate8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
            TPS65914_I2C_SPEED, pPmuProps->ctrl_reg, 0, 0x3);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the control register\n", __func__);
        return ret;
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;

    return ret;
}

static NvBool Tps65914SMPSGetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    Tps65914PmuProps *pPmuProps;
    NvU8 vsel;
    NvBool ret;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);
    NV_ASSERT(uV);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps || !uV)
        return NV_FALSE;

    pPmuProps = hTps65914PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
            TPS65914_I2C_SPEED, pPmuProps->volt_reg, &vsel);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the volt register\n", __func__);
        return ret;
    }

    vsel &= ~PALMAS_SMPS12_VOLTAGE_RANGE;

    if (vsel <= 0x06)
        *uV = 500000 * pPmuProps->mult;
    else
        *uV = (vsel - 0x06) * (pPmuProps->step_uV * pPmuProps->mult) + (pPmuProps->mult * 500000);

    return ret;
}

static NvBool Tps65914SMPSSetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel = 0;
    NvBool ret = NV_FALSE;
    Tps65914PmuProps *pPmuProps;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hTps65914PmuInfo->RailInfo[RailId].pPmuProps;
    if (pPmuProps->ctrl_reg == TPS65914_SMPS10_CTRL)
    {
        ret = NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
                TPS65914_I2C_SPEED, pPmuProps->ctrl_reg, 0x0D);
    }
    else
    {
        if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
            return NV_FALSE;

        if (new_uV == 500000 && (pPmuProps->mult == 1))
            vsel = 0x06;
        else if (new_uV == 1000000 && (pPmuProps->mult == 2))
            vsel = 0x06;
        else
            vsel = ((new_uV - (pPmuProps->mult * 500000))/(pPmuProps->step_uV * pPmuProps->mult)) + 0x06;

        if (pPmuProps->mult == 2)
            vsel |= PALMAS_SMPS12_VOLTAGE_RANGE;

        ret = NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
                TPS65914_I2C_SPEED, pPmuProps->volt_reg, vsel);

        if (!ret) {
            NvOdmOsPrintf("%s() Error in writing the volt register\n", __func__);
            return ret;
        }

        ret = NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
                TPS65914_I2C_SPEED, pPmuProps->ctrl_reg, 0x01);

        if (!ret) {
            NvOdmOsPrintf("%s() Error in writing the ctrl register\n", __func__);
            return ret;
        }
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;

    return ret;
}

static NvBool Tps65914LDOGetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    Tps65914PmuProps *pPmuProps;
    NvU8 vsel;
    NvBool ret;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hTps65914PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
            TPS65914_I2C_SPEED, pPmuProps->volt_reg, &vsel);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the volt register\n", __func__);
        return ret;
    }

    if (vsel == 0x01)
        *uV = 900000;
    else if (vsel >= 0x3f)
        *uV = 3300000;
    else
        *uV = (vsel - 0x02)*pPmuProps->step_uV + 900000;

    return ret;
}

static NvBool Tps65914LDOSetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel;
    NvBool ret;
    Tps65914PmuProps *pPmuProps;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hTps65914PmuInfo->RailInfo[RailId].pPmuProps;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    if (new_uV == 900000)
        vsel = 0x1;
    else if (new_uV > 3300000)
        vsel = 0x3f;
    else
        vsel = ((new_uV - 900000)/pPmuProps->step_uV) + 1;

    ret = NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
            TPS65914_I2C_SPEED, pPmuProps->volt_reg, vsel);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in writing the volt register\n", __func__);
        return ret;
    }

    ret = NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
            TPS65914_I2C_SPEED, pPmuProps->ctrl_reg, 0x01);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in writing the ctrl register\n", __func__);
        return ret;
    }

    if (DelayUS)
        *DelayUS = pPmuProps->delay;

    return ret;
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress, NvU8 PmuPage)
{
    NvU8 i;
    const NvOdmPeripheralConnectivity *pConnectivity = NvOdmPeripheralGetGuid(TPS65914_PMUGUID);

    if (pConnectivity)
    {
        for (i = 0; i < pConnectivity->NumAddress; i++)
        {
            if (i == (PmuPage - 1))
            {
                if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_I2c)
                {
                    if (pI2cModule)
                        *pI2cModule   = NvOdmIoModule_I2c;
                    if (pI2cInstance)
                        *pI2cInstance = pConnectivity->AddressList[i].Instance;
                    if (pI2cAddress)
                        *pI2cAddress  = pConnectivity->AddressList[i].Address;
                    return NV_TRUE;
                }
            }
        }
    }
    NvOdmOsPrintf("%s: The system did not discover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

static NvBool Tps65914GpioRailIsEnabled(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId)
{
    NvBool ret;
    NvU8 Val;
    NvU8 GpioRail;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    if ((RailId < TPS65914PmuSupply_GPIO0) || (RailId > TPS65914PmuSupply_GPIO7))
    {
        NvOdmOsPrintf("%s(): Invalid Rail\n", __func__);
        return NV_FALSE;
    }

    GpioRail = RailId - TPS65914PmuSupply_GPIO0;

    ret = NvOdmPmuI2cUpdate8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage2,
            TPS65914_I2C_SPEED, TPS65914_GPIO_DATA_DIR, (1 << GpioRail), 0xFF);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
    }

    ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage2,
            TPS65914_I2C_SPEED, TPS65914_GPIO_DATA_OUT, &Val);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in readin the supply register\n", __func__);
        return ret;
    }

    if (Val &  (1 << GpioRail))
        return NV_TRUE;

    return NV_FALSE;
}

static NvBool Tps65914GpioRailEnable(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    NvU8 GpioRail;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);
    NV_ASSERT(DelayUS);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps || !DelayUS)
        return NV_FALSE;

    if ((RailId < TPS65914PmuSupply_GPIO0) || (RailId > TPS65914PmuSupply_GPIO7))
    {
        NvOdmOsPrintf("%s(): Invalid Rail\n", __func__);
        return NV_FALSE;
    }
    GpioRail = RailId - TPS65914PmuSupply_GPIO0;

    ret = NvOdmPmuI2cUpdate8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage2,
            TPS65914_I2C_SPEED, TPS65914_GPIO_DATA_DIR, (1 << GpioRail), 0xFF);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
        return ret;
    }

    ret = NvOdmPmuI2cUpdate8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage2,
            TPS65914_I2C_SPEED, TPS65914_GPIO_DATA_OUT, (1 << GpioRail), 0xFF);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
        return ret;
    }

    if (DelayUS)
        NvOdmOsWaitUS(*DelayUS);

    return ret;
}

static NvBool Tps65914GpioRailDisable(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    NvU8 GpioRail;;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);
    NV_ASSERT(DelayUS);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps || !DelayUS)
        return NV_FALSE;

    if ((RailId < TPS65914PmuSupply_GPIO0) || (RailId > TPS65914PmuSupply_GPIO7))
    {
        NvOdmOsPrintf("%s(): Invalid Rail\n", __func__);
        return NV_FALSE;
    }
    GpioRail = RailId - TPS65914PmuSupply_GPIO0;

    ret = NvOdmPmuI2cUpdate8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage2,
            TPS65914_I2C_SPEED, TPS65914_GPIO_DATA_DIR, (1 << GpioRail), 0xFF);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
        return ret;
    }

    ret = NvOdmPmuI2cUpdate8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage2,
            TPS65914_I2C_SPEED, TPS65914_GPIO_DATA_OUT, ~(1 << GpioRail), 0xFF);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
        return ret;
    }

    if (DelayUS)
        NvOdmOsWaitUS(*DelayUS);

    return ret;
}

static NvBool Tps65914GpioGetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
                NvU32 RailId, NvU32 *uV)
{
    NvBool Ret;

    NV_ASSERT(hTps65914PmuInfo->RailInfo[RailId].pPmuProps);
    NV_ASSERT(uV);

    if (!hTps65914PmuInfo->RailInfo[RailId].pPmuProps || !uV)
        return NV_FALSE;

    Ret = Tps65914GpioRailIsEnabled(hTps65914PmuInfo, RailId);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in reading rail %d register\n", __func__, RailId);
        return Ret;
    }
    return NV_TRUE;
}

static NvBool Tps65914GpioSetVoltage(Tps65914PmuInfoHandle hTps65914PmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    if (new_uV)
        Tps65914GpioRailEnable(hTps65914PmuInfo, RailId, DelayUS);
    else
        Tps65914GpioRailDisable(hTps65914PmuInfo, RailId, DelayUS);

    if (DelayUS)
        NvOdmOsWaitUS(*DelayUS);

    return NV_TRUE;
}

static NvBool Tps65914ReadVersions(Tps65914PmuInfoHandle hTps65914PmuInfo)
{
    NvBool Ret;
    NvU8 sw_rev;
    NvU8 design_rev;

    Ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
            TPS65914_I2C_SPEED, TPS65914_SW_REVISION & 0xFF, &sw_rev);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in reading the SW_REVISION  register\n", __func__);
        return Ret;
    }

    Ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage3,
            TPS65914_I2C_SPEED, TPS65914_INTERNAL_DESIGNREV & 0xFF, &design_rev);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in reading the INTERNAL_DESIGNREV register\n", __func__);
        return Ret;
    }

    hTps65914PmuInfo->SwOTPVersion = sw_rev;
    NvOdmOsPrintf("%s() Internal DesignRev 0x%02X, SWRev 0x%02X\n", __func__, sw_rev, design_rev);
    design_rev = design_rev & 0xF;

    switch (design_rev) {
    case 0:
            hTps65914PmuInfo->ESMajorVersion = 1;
            hTps65914PmuInfo->ESMinorVersion = 0;
            hTps65914PmuInfo->DesignRev = 0xA0;
            break;
    case 1:
            hTps65914PmuInfo->ESMajorVersion = 2;
            hTps65914PmuInfo->ESMinorVersion = 0;
            hTps65914PmuInfo->DesignRev = 0xB0;
            break;
    case 2:
            hTps65914PmuInfo->ESMajorVersion = 2;
            hTps65914PmuInfo->ESMinorVersion = 1;
            hTps65914PmuInfo->DesignRev = 0xB1;
            break;
    case 3:
            hTps65914PmuInfo->ESMajorVersion = 2;
            hTps65914PmuInfo->ESMinorVersion = 2;
            hTps65914PmuInfo->DesignRev = 0xB2;
            break;
    case 4:
            hTps65914PmuInfo->ESMajorVersion = 2;
            hTps65914PmuInfo->ESMinorVersion = 3;
            hTps65914PmuInfo->DesignRev = 0xB3;
            break;
    default:
            NvOdmOsPrintf("%s(): Invalid design revision\n");
            return NV_FALSE;
    }

    NvOdmOsPrintf("%s(): ES version %d.%d: ChipRevision 0x%02X%02X\n",
           __func__, hTps65914PmuInfo->ESMajorVersion,
           hTps65914PmuInfo->ESMinorVersion,
            hTps65914PmuInfo->DesignRev, hTps65914PmuInfo->SwOTPVersion);

     return NV_TRUE;
}

static NvU32 GetMaximumCurrentFromOtp(Tps65914PmuInfoHandle hTps65914PmuInfo,
                   NvU32 VddId)
{
   switch(hTps65914PmuInfo->SwOTPVersion) {
       case 0x01:
       case 0x02:
       case 0xB4:
       case 0xB6:
       case 0xB7:
       case 0xB9:
               return 6000;
       case 0xB5:
       case 0xBC:
               return 8000;

       case 0xB8:
       case 0xC8:
               return 9000;

       case 0xB1:
       case 0xB2:
    default:
               return 4000;
    }
}

NvBool Tps65914PmuClearAlarm(NvOdmPmuDriverInfoHandle hDevice)
{
    NvBool ret = NV_FALSE;
    Tps65914PmuInfoHandle hTps65914PmuInfo;

    hTps65914PmuInfo = (Tps65914PmuInfoHandle)hDevice;
    if(!hDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return NV_FALSE;
    }

    /* Disable RTC Alarm interrupt to AP by masking RTC_ALARM in INT2 */
    ret = NvOdmPmuI2cSetBits(hTps65914PmuInfo->hOdmPmuI2c,
            hTps65914PmuInfo->DeviceAddrPage2,
            TPS65914_I2C_SPEED, TPS65914_INT2_MASK,
            (1 << TPS65914_SHIFT_INT2_MASK_RTC_ALARM));

    NvOdmOsDebugPrintf ("[%u] %s::Disabled RTC alarm\n",NvOdmOsGetTimeMS(),__func__);
    NV_ASSERT(ret);
    return ret;
}

NvU8 Tps65914PmuPowerOnSource(NvOdmPmuDriverInfoHandle hDevice)
{
    // This function is exposed to other files and
    // returns PowerOnSource read during PmuOpen
    return PowerOnSource;
}

static NvU8 GetPmuPowerOnSource(Tps65914PmuInfoHandle hDevice)
{
    NvBool ret;
    NvU8 reg = 0;
    NvU8 Source = NvOdmPmuPowerKey;
    Tps65914PmuInfoHandle hTps65914PmuInfo;

    hTps65914PmuInfo = hDevice;
    if(!hDevice)
    {
        NvOdmOsDebugPrintf("%s::NULL pmu handle\n",__func__);
        return NV_FALSE;
    }

    /* Enable Clear on Read */
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT_CTRL, 0x1);

    /* Check PWRON status bit. */
    ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT1_STATUS, &reg);
    if (!ret)
        NV_ASSERT(!"Tps65914PmuPowerOnSource: NvOdmPmuI2cRead8() failed");
    if (reg & (1 << TPS65914_SHIFT_INT1_STATUS_PWRON))
        Source |= NvOdmPmuPowerKey;
    NvOdmOsDebugPrintf("Tps65914PmuPowerOnSource::Int1_Status - 0x%02x\n",reg);

    /* Check RTC alarm status bit. */
    ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
            TPS65914_I2C_SPEED, TPS65914_INT2_STATUS, &reg);
    if (!ret)
        NV_ASSERT(!"Tps65914PmuPowerOnSource: NvOdmPmuI2cRead8() failed");
    if (reg & (1 << TPS65914_SHIFT_INT2_STATUS_RTC_ALARM))
        Source |= NvOdmPmuRTCAlarm;
    NvOdmOsDebugPrintf("Tps65914PmuPowerOnSource::Int2_Status2 - 0x%02x\n",reg);

    /* Check VBUS status bit */
    ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
            TPS65914_I2C_SPEED, TPS65914_INT3_STATUS, &reg);
    if (!ret)
        NV_ASSERT(!"Tps65914PmuPowerOnSource: NvOdmPmuI2cRead8() failed");
    if (reg & (1 << TPS65914_SHIFT_INT3_STATUS_VBUS))
        Source |= NvOdmUsbHotPlug;
  //  NvOdmOsDebugPrintf("Tps65914PmuPowerOnSource::Int3_Status3 - 0x%02x\n",reg);

	NvU32 time=1;	
	Tps65914GpioSetVoltage(hTps65914PmuInfo,TPS65914PmuSupply_GPIO6, 18000, &time);

    return Source;
}


static int global_bus_state=0;
int get_vbus_state(void)
{
	return global_bus_state;


}

static NvBool GetPmuVbusLineState(Tps65914PmuInfoHandle hTps65914PmuInfo)
{
#define TPS65914_INT3_LINE_STATE 0x1C
    NvU8 reg = 0;
    /* Check Vbus line state and set override */
    NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage2,
            TPS65914_I2C_SPEED, TPS65914_INT3_LINE_STATE, &reg);

	global_bus_state = ((reg & (1 << TPS65914_SHIFT_INT3_STATUS_VBUS)) ? 1 : 0); 
    NvOdmOsPrintf("VBUS Line State = %s\n",
        (reg & (1 << TPS65914_SHIFT_INT3_STATUS_VBUS)) ? "CONNECTED" : "DISCONNECTED");
    return (reg & (1 << TPS65914_SHIFT_INT3_STATUS_VBUS)) ? NV_TRUE : NV_FALSE;
}


NvOdmPmuDriverInfoHandle Tps65914PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps)
{
    Tps65914PmuInfoHandle hTps65914PmuInfo;
    Tps65914BoardData *pBoardData;
    NvU32 i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddressPage1 = 0;
    NvU32 I2cAddressPage2 = 0;
    NvU32 I2cAddressPage3 = 0;
    NvBool Ret;
    NvU32 VddId;
    NvU8 Control;
    NvBool Status;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    if (!hDevice || !pOdmRailProps)
        return NULL;

    /* Information for all rail is required */
    if (NrOfOdmRailProps != TPS65914PmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }

    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddressPage1, 1);
    if (!Ret)
        return NULL;

    Ret = GetInterfaceDetail(NULL, NULL, &I2cAddressPage2, 2);
    if (!Ret)
        return NULL;

    Ret = GetInterfaceDetail(NULL, NULL, &I2cAddressPage3, 3);
    if (!Ret)
        return NULL;

    NvOdmPeripheralGetBoardInfo(0, &s_ProcBoardInfo);
    Status = NvOdmPeripheralGetBoardInfo(PMU_E1735, &s_PmuBoardInfo);
    if(!Status)
        Status = NvOdmPeripheralGetBoardInfo(PMU_E1736, &s_PmuBoardInfo);
    if(!Status)
        Status = NvOdmPeripheralGetBoardInfo(PMU_E1936, &s_PmuBoardInfo);
    if(!Status)
        Status = NvOdmPeripheralGetBoardInfo(PMU_E1761, &s_PmuBoardInfo);
    if(!Status)
        Status = NvOdmPeripheralGetBoardInfo(PMU_E1769, &s_PmuBoardInfo);
    if(!Status)
        NvOdmOsPrintf("%s:Unsupported PMU board ID\n",__func__);

    hTps65914PmuInfo = NvOdmOsAlloc(sizeof(Tps65914PmuInfo));
    if (!hTps65914PmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hTps65914PmuInfo, 0, sizeof(Tps65914PmuInfo));

    hTps65914PmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hTps65914PmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        goto I2cOpenFailed;
    }
    hTps65914PmuInfo->DeviceAddrPage1 = I2cAddressPage1;
    hTps65914PmuInfo->DeviceAddrPage2 = I2cAddressPage2;
    hTps65914PmuInfo->DeviceAddrPage3 = I2cAddressPage3;

    hTps65914PmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hTps65914PmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hTps65914PmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hTps65914PmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    Tps65914ReadVersions(hTps65914PmuInfo);

    /* read smps ctrl register */
    Ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
            TPS65914_I2C_SPEED, TPS65914_SMPS_CTRL, &Control);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in reading the control register\n", __func__);
        return NULL;
    }

    if (Control & TPS65914_SMPS_CTRL_SMPS12_SMPS123_EN)
        hTps65914PmuInfo->Smps123 = NV_TRUE;

    if (Control & TPS65914_SMPS_CTRL_SMPS45_SMPS457_EN)
        hTps65914PmuInfo->Smps457 = NV_TRUE;

    PmuPropsTable = (Tps65914PmuProps *)NvOdmOsAlloc(sizeof(s_Tps65914PmuProps));
    NvOdmOsMemcpy(PmuPropsTable, &s_Tps65914PmuProps, sizeof(s_Tps65914PmuProps));

    for (i = 1; i < TPS65914PmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;
        pBoardData = pOdmRailProps[i].pBoardData;

        if (VddId == TPS65914PmuSupply_Invalid)
            continue;

        if (pOdmRailProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been provided, "
                    "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        if (VddId != PmuPropsTable[VddId].Id)
        {
            NvOdmOsPrintf("%s(): The entries are not in proper sequence, "
                    "Ignoring this entry\n", __func__);
            continue;
        }

        if (hTps65914PmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                    "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        hTps65914PmuInfo->RailInfo[VddId].pPmuProps = &PmuPropsTable[VddId];

        NvOdmOsMemcpy(&hTps65914PmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));

        hTps65914PmuInfo->RailInfo[VddId].Caps.OdmProtected =
            hTps65914PmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        if ( pBoardData && (VddId < TPS65914PmuSupply_LDO1))
        {
            if (pBoardData->Range)
            {
                hTps65914PmuInfo->RailInfo[VddId].pPmuProps->min_uV *= 2;
                hTps65914PmuInfo->RailInfo[VddId].pPmuProps->max_uV *= 2;
                hTps65914PmuInfo->RailInfo[VddId].pPmuProps->mult = 2;
            }
        }
        else
        {
            hTps65914PmuInfo->RailInfo[VddId].pPmuProps->mult = 1;
        }

        /* Maximum current */
        if ((VddId == TPS65914PmuSupply_SMPS45) || (VddId == TPS65914PmuSupply_SMPS457))
        {
            hTps65914PmuInfo->RailInfo[VddId].Caps.MaxCurrentMilliAmp =
                GetMaximumCurrentFromOtp(hTps65914PmuInfo, VddId);
            NvOdmOsPrintf("%s(): The smps45/smps457 current is %d\n", __func__,
                    hTps65914PmuInfo->RailInfo[VddId].Caps.MaxCurrentMilliAmp);
        }

        /* min voltage */
        if (hTps65914PmuInfo->RailInfo[VddId].OdmProps.Min_mV)
            hTps65914PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                NV_MAX(hTps65914PmuInfo->RailInfo[VddId].OdmProps.Min_mV,
                        hTps65914PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000);
        else
            hTps65914PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                hTps65914PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000;

        /* max voltage */
        if (hTps65914PmuInfo->RailInfo[VddId].OdmProps.Max_mV)
            hTps65914PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                NV_MIN(hTps65914PmuInfo->RailInfo[VddId].OdmProps.Max_mV,
                        hTps65914PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000);
        else
            hTps65914PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                hTps65914PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000;

        /* step voltage */
        hTps65914PmuInfo->RailInfo[VddId].Caps.StepMilliVolts =
            hTps65914PmuInfo->RailInfo[VddId].pPmuProps->step_uV/1000;

        if (hTps65914PmuInfo->RailInfo[VddId].OdmProps.Setup_mV)
            hTps65914PmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                hTps65914PmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                    "Ignoring this entry\n", __func__,
                    hTps65914PmuInfo->RailInfo[VddId].OdmProps.Setup_mV,
                    VddId);
            continue;
        }
        hTps65914PmuInfo->RailInfo[VddId].UserCount  = 0;
        hTps65914PmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hTps65914PmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;

        hTps65914PmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
            hTps65914PmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
    }
    // Use GetPmuPowerOnSource (static function) to read PowerOnSource the first time
    PowerOnSource = GetPmuPowerOnSource(hTps65914PmuInfo);
    NvMlUsbfSetVbusOverrideT1xx(GetPmuVbusLineState(hTps65914PmuInfo));
    Tps65914SetInterruptMasks((NvOdmPmuDriverInfoHandle)hTps65914PmuInfo);
    return (NvOdmPmuDriverInfoHandle)hTps65914PmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hTps65914PmuInfo->hOdmPmuService);
PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hTps65914PmuInfo->hOdmPmuI2c);
I2cOpenFailed:
    NvOdmOsFree(hTps65914PmuInfo);
    return NULL;
}

void Tps65914PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps65914PmuInfoHandle hTps65914PmuInfo;

    NV_ASSERT(hPmuDriver);

    if (!hPmuDriver)
        return;

    hTps65914PmuInfo = (Tps65914PmuInfoHandle)hPmuDriver;
    NvOdmOsMemset(PmuPropsTable, 0, sizeof(s_Tps65914PmuProps));
    NvOdmOsFree(PmuPropsTable);
    PmuPropsTable = NULL;
    NvOdmOsMutexDestroy(hTps65914PmuInfo->hPmuMutex);
    hTps65914PmuInfo->hPmuMutex = NULL;
    NvOdmServicesPmuClose(hTps65914PmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hTps65914PmuInfo->hOdmPmuI2c);
    NvOdmOsMemset(hTps65914PmuInfo, 0, sizeof(Tps65914PmuInfo));
    NvOdmOsFree(hTps65914PmuInfo);
}

void Tps65914PmuDriverSuspend(void)
{
    NvBool Ret;
    NvU8 Val;
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvU32 I2cAddress2 = 0;

    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress, 1);
    if (!Ret)
    {
        NvOdmOsDebugPrintf("%s: GetInterfaceDetail failed\n", __func__);
        return;
    }

    // Using same variables as above for I2C Module and Instance
    // since they will be same in both the cases
    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress2, 2);
    if (!Ret)
    {
        NvOdmOsDebugPrintf("%s: GetInterfaceDetail failed\n", __func__);
        return;
    }

    hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);

    //POWER_CTRL: NSLEEP_MASK=0, ENABLE1_MASK=0, ENABLE2_MASK=0
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, TPS65914_POWER_CNTRL, 0x00);

#if defined(NV_TARGET_PLUTO)
    //ENABLE1_SMPS_ASSIGN: SMPS123
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, TPS65914_ENABLE1_SMPS_ASSIGN, 0x03);

    //SMPS123 - CPU
    //SMPS12_CTRL = 0x09 //ECO Mode in Sleep and Auto in Active
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, TPS65914_SMPS12_CTRL, 0x41);
    //SMPS12 DVS Register: Range=0 (0.5V-1.65V), VSEL = 1.08v
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, 0x23, 0x51);
    //SMPS3_CTRL = 0x09 //ECO Mode
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, TPS65914_SMPS3_CTRL, 0x01);
#endif

    //SMPS45 - CORE
    //SMPS45_CTRL = 0x09 //ECOMode in sleep and auto in active
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, TPS65914_SMPS45_CTRL, 0x51);

    //PRIMARY_SECONDARY_PAD2
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, TPS65914_PRIMARY_SECONDARY_PAD2, 0x00);

    //GPIO_DATA_DIR
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress2, TPS65914_I2C_SPEED, TPS65914_GPIO_DATA_DIR, 0xFF);
    //GPIO_DATA_OUT
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress2, TPS65914_I2C_SPEED, TPS65914_GPIO_DATA_OUT, 0x00);

    //INT2_MASK
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress2, TPS65914_I2C_SPEED, TPS65914_INT2_MASK, 0xFF);
    //INT1_MASK
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress2, TPS65914_I2C_SPEED, TPS65914_INT1_MASK, 0xFD);
    //INT3_MASK
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress2, TPS65914_I2C_SPEED, TPS65914_INT3_MASK, 0xFF);

    //NSLEEP_RES_ASSIGN: REGEN3, CLK32KGAUDIO, CLK32KG, SYSEN2, SYSEN1
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, TPS65914_NSLEEP_RES_ASSIGN, 0x00);

    //NSLEEP_SMPS_ASSIGN: SMPS10, SMPS9, SMPS8, SMPS7, SMPS45
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, TPS65914_NSLEEP_SMPS_ASSIGN, 0x04);

    //NSLEEP_LDO_ASSIGN1: LDO1, LDO2, LDO3, LDO4, LDO5, LDO7
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, TPS65914_NSLEEP_LDO_ASSIGN1, 0x5F);

    //NSLEEP_LDO_ASSIGN2: LDOUSB, LDOLN, LDO9
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress, TPS65914_I2C_SPEED, TPS65914_NSLEEP_LDO_ASSIGN2, 0x07);

    //INT_CTRL: Set Clear-on-Read
    NvOdmPmuI2cWrite8( hOdmPmuI2c, I2cAddress2, TPS65914_I2C_SPEED, TPS65914_INT_CTRL, 0x01);

    // Clear interrupts by reading
    NvOdmPmuI2cRead8(hOdmPmuI2c, I2cAddress2, TPS65914_I2C_SPEED, TPS65914_INT1_STATUS, &Val);
    NvOdmPmuI2cRead8(hOdmPmuI2c, I2cAddress2, TPS65914_I2C_SPEED, TPS65914_INT1_STATUS, &Val);
    NvOdmPmuI2cRead8(hOdmPmuI2c, I2cAddress2, TPS65914_I2C_SPEED, TPS65914_INT2_STATUS, &Val);
    NvOdmPmuI2cRead8(hOdmPmuI2c, I2cAddress2, TPS65914_I2C_SPEED, TPS65914_INT3_STATUS, &Val);
}

void Tps65914PmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Tps65914PmuInfoHandle hTps65914PmuInfo;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS65914PmuSupply_Num);
    NV_ASSERT(pCapabilities);

    if (!hPmuDriver || !(VddId < TPS65914PmuSupply_Num) || !pCapabilities)
        return;

    hTps65914PmuInfo = (Tps65914PmuInfoHandle)hPmuDriver;

    if (!hTps65914PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d\n", __func__, VddId);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }

    NvOdmOsMemcpy(pCapabilities, &hTps65914PmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Tps65914PmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    Tps65914PmuInfoHandle hTps65914PmuInfo;
    Tps65914PmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;
    hTps65914PmuInfo =  (Tps65914PmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS65914PmuSupply_Num);
    NV_ASSERT(pMilliVolts);

    if (!hPmuDriver || !(VddId < TPS65914PmuSupply_Num) || !pMilliVolts)
        return NV_FALSE;

    hTps65914PmuInfo = (Tps65914PmuInfoHandle)hPmuDriver;

    if (!hTps65914PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provided the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pPmuProps = hTps65914PmuInfo->RailInfo[VddId].pPmuProps;

    *pMilliVolts = 0;

    if (pPmuProps->pOps->IsRailEnabled(hTps65914PmuInfo, VddId)) {
        Status = pPmuProps->pOps->GetRailVoltage(hTps65914PmuInfo, VddId, &uV);
        if (Status) {
            *pMilliVolts = uV/1000;
        }
    }
    return Status;
}

NvBool Tps65914PmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    Tps65914PmuInfoHandle hTps65914PmuInfo;
    Tps65914PmuProps *pPmuProps;
    Tps65914RailInfo *pRailInfo;
    NvBool Status = NV_TRUE;
    hTps65914PmuInfo =  (Tps65914PmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS65914PmuSupply_Num);
    NV_ASSERT(pSettleMicroSeconds);

    if (!hPmuDriver || !(VddId < TPS65914PmuSupply_Num) || !pSettleMicroSeconds)
        return NV_FALSE;

    if (!hTps65914PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pRailInfo = &hTps65914PmuInfo->RailInfo[VddId];
    pPmuProps = hTps65914PmuInfo->RailInfo[VddId].pPmuProps;
    if (pSettleMicroSeconds)
        *pSettleMicroSeconds = 0;

    if (pRailInfo->OdmProps.IsOdmProtected == NV_TRUE)
    {
        NvOdmOsPrintf("%s:The voltage is protected and can not be set.\n", __func__);
        return NV_TRUE;
    }

    if ((MilliVolts != ODM_VOLTAGE_OFF) &&
        ((MilliVolts > pRailInfo->Caps.MaxMilliVolts) ||
         (MilliVolts < pRailInfo->Caps.MinMilliVolts)))
    {
        NvOdmOsPrintf("%s:The required voltage is not supported..vddRail: %d,"
                         " MilliVolts: %d\n", __func__, VddId, MilliVolts);
        return NV_TRUE;
    }

    NvOdmOsMutexLock(hTps65914PmuInfo->hPmuMutex);
    if (pPmuProps->ctrl_reg == TPS65914_SMPS10_CTRL)
    {
        NvOdmServicesPmuSetSocRailPowerState(hTps65914PmuInfo->hOdmPmuService,
            hTps65914PmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hTps65914PmuInfo, VddId,
                         MilliVolts * 1000, pSettleMicroSeconds);
        goto End;
    }

    if (MilliVolts == ODM_VOLTAGE_OFF)
    {
        if (!pRailInfo->UserCount)
        {
            NvOdmOsPrintf("%s:The imbalance in voltage off No user for rail %d\n",
                        __func__, VddId);
            Status = NV_TRUE;
            goto End;
        }

        if (pRailInfo->UserCount == 1)
        {
            NvOdmServicesPmuSetSocRailPowerState(hTps65914PmuInfo->hOdmPmuService,
                        hTps65914PmuInfo->RailInfo[VddId].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hTps65914PmuInfo, VddId,
                                            pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hTps65914PmuInfo->hOdmPmuService,
                        hTps65914PmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hTps65914PmuInfo, VddId,
                                MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hTps65914PmuInfo, VddId,
                                                        pSettleMicroSeconds);
        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                        __func__, VddId);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hTps65914PmuInfo->hPmuMutex);
    return Status;
}

NvBool Tps65914PmuSetLongPressKeyDuration(NvOdmPmuDriverInfoHandle hPmuDriver,
        KeyDurationType duration)
{
    NvBool Ret;
    Tps65914PmuInfoHandle hTps65914PmuInfo;

    NV_ASSERT(hPmuDriver);
    if (!hPmuDriver)
        return NV_FALSE;

    hTps65914PmuInfo = (Tps65914PmuInfoHandle)hPmuDriver;

    Ret = NvOdmPmuI2cUpdate8(hTps65914PmuInfo->hOdmPmuI2c,
            hTps65914PmuInfo->DeviceAddrPage1, TPS65914_I2C_SPEED,
            TPS65914_LONGPRESSKEY_REG & 0xFF, (NvU8)(duration << 2), 0x0C);

    if (!Ret)
    {
        NvOdmOsPrintf("%s() Error in updating the long press key register\n", __func__);
        return NV_FALSE;
    }

    return NV_TRUE;
}

NvBool Tps65914PmuReadRstReason(NvOdmPmuDriverInfoHandle hPmuDriver,
    NvOdmPmuResetReason *rst_reason)
{
    NvU8 SwOffStatus;
    NvBool Ret;
    Tps65914PmuInfoHandle hTps65914PmuInfo;

    NV_ASSERT(hPmuDriver);
    if (!hPmuDriver)
        return NV_FALSE;

    NV_ASSERT(rst_reason);
    if (!rst_reason)
        return NV_FALSE;

    hTps65914PmuInfo = (Tps65914PmuInfoHandle)hPmuDriver;
    Ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
            TPS65914_I2C_SPEED, TPS65914_SWOFF_STATUS & 0xFF, &SwOffStatus);
    if (Ret)
    {
        switch(SwOffStatus) {
        case 0x00:
            *rst_reason = NvOdmPmuResetReason_NoReason;
            break;
        case 0x01:
            *rst_reason = NvOdmPmuResetReason_GPADC;
            break;
        case 0x02:
            *rst_reason = NvOdmPmuResetReason_BatLow;
            break;
        case 0x04:
            *rst_reason = NvOdmPmuResetReason_SwReset;
            break;
        case 0x08:
            *rst_reason = NvOdmPmuResetReason_ResetSig;
            break;
        case 0x10:
            *rst_reason = NvOdmPmuResetReason_Thermal;
            break;
        case 0x20:
            *rst_reason = NvOdmPmuResetReason_WTD;
            break;
        case 0x40:
            *rst_reason = NvOdmPmuResetReason_PwrDown;
            break;
        case 0x80:
            *rst_reason = NvOdmPmuResetReason_PwrOnLPK;
            break;
        default:
            *rst_reason = NvOdmPmuResetReason_Invalid;
        }
    }

    return Ret;
}

static int Bcd2Binary(int x)
{
    int binary = 0;
    binary = ((x & 0xF0 )>>4 ) * 10 + (x & 0x0F);
    return (binary);
}
static int Binary2Bcd (int x)
{
    int bcd = 0;

    bcd = x/10 << 4;
    bcd += x%10;

    return bcd;
}

#define NUM_TIME_REGS 6

void Tps65914PmuGetDate(NvOdmPmuDriverInfoHandle hPmuDriver, DateTime* time)
{
    NvU32 count = 0;
    NvU8 rtc_data[NUM_TIME_REGS] = {0};
    Tps65914PmuInfoHandle hTps65914PmuInfo;

    hTps65914PmuInfo = (Tps65914PmuInfoHandle)hPmuDriver;
    NV_ASSERT(hPmuDriver);
    NV_ASSERT(time);

    if(!hPmuDriver || !time)
        NvOdmOsDebugPrintf("%s::NULL pmu/time handle\n",__func__);

    //RTC and Enable GET_TIME for shadow registers
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, 0xB0, TPS65914_I2C_SPEED, 0x10, 0x41);

    for ( ; count < NUM_TIME_REGS; count++)
        NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c, 0xB0, TPS65914_I2C_SPEED, count, &rtc_data[count]);

    //Stop RTC and Disable GET_TIME for shadow registers
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, 0xB0,TPS65914_I2C_SPEED, 0x10, 0x01);

    //Convert data from BCD to Binary
    time->Second = Bcd2Binary(rtc_data[0]);
    time->Minute = Bcd2Binary(rtc_data[1]);
    time->Hour   = Bcd2Binary(rtc_data[2] );
    time->Day = Bcd2Binary(rtc_data[3]);
    time->Month = Bcd2Binary(rtc_data[4]);
    time->Year = Bcd2Binary(rtc_data[5]);
}

void Tps65914PmuSetAlarm (NvOdmPmuDriverInfoHandle hPmuDriver, DateTime time)
{
    Tps65914PmuInfoHandle hTps65914PmuInfo;
    NvOdmPmuI2cHandle hBqI2c = NULL;
    NvU8 RtcStatus = 0;

    NV_ASSERT(hPmuDriver);
    if (!hPmuDriver)
        return;

    hTps65914PmuInfo = (Tps65914PmuInfoHandle)hPmuDriver;

    //SWOFF_COLDRST.SW_RST = 0
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, 0xB0,
                       TPS65914_I2C_SPEED, 0xb0, 0xfb);

    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, 0xB0,
                         TPS65914_I2C_SPEED, 0x08, Binary2Bcd(time.Second));
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, 0xB0,
                     TPS65914_I2C_SPEED, 0x09, Binary2Bcd(time.Minute));
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, 0xB0,
                     TPS65914_I2C_SPEED, 0x0a, Binary2Bcd (time.Hour));
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, 0xB0,
                     TPS65914_I2C_SPEED, 0x0b, Binary2Bcd (time.Day));
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, 0xB0,
                     TPS65914_I2C_SPEED, 0x0c, Binary2Bcd (time.Month));
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, 0xB0,
                     TPS65914_I2C_SPEED, 0x0d, Binary2Bcd(time.Year));

    /* Clear ALARM If already set */
    NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage1,
        TPS65914_I2C_SPEED, TPS65914_RTC_STATUS_REG,
        &RtcStatus);
        NvOdmOsPrintf("\n[%u] TPS65914::RtcStatus - 0x%08x\n",NvOdmOsGetTimeMS(),RtcStatus);
    if(RtcStatus & (1 << TPS65914_SHIFT_RTC_STATUS_REG_IT_ALARM))
    {
            // Clear Status by writing 1 to the corresponding bit
            NvOdmPmuI2cSetBits(hTps65914PmuInfo->hOdmPmuI2c,
                hTps65914PmuInfo->DeviceAddrPage1,
                TPS65914_I2C_SPEED,
                TPS65914_RTC_STATUS_REG,
                (1 << TPS65914_SHIFT_RTC_STATUS_REG_IT_ALARM));
            NvOdmOsPrintf("[%u] TPS65914::RTC ALARM DETECTED AND CLEARED\n",NvOdmOsGetTimeMS());
    }

    /* Enable IT_ALARM INTERRUPT */
    NvOdmPmuI2cSetBits(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage1,
        TPS65914_I2C_SPEED, TPS65914_RTC_INTERRUPTS_REG,
        (1 << TPS65914_SHIFT_RTC_INTERRUPTS_REG_IT_ALARM));

    /* Enable RTC Alarm interrupt to AP by unmasking RTC_ALARM in INT2 */
    NvOdmPmuI2cClrBits(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT2_MASK,
        (1 << TPS65914_SHIFT_INT2_MASK_RTC_ALARM));

    if(s_ProcBoardInfo.BoardID == PROC_BOARD_E1780)
    {
        if(s_PmuBoardInfo.BoardID == PMU_E1735)
            return;
    }

    // Clear BQ WDT and HiZ bits
    if (s_PmuBoardInfo.BoardID == PMU_E1761)
        hBqI2c = NvOdmPmuI2cOpen(NvOdmIoModule_I2c, 0x1);
    else
        hBqI2c = NvOdmPmuI2cOpen(NvOdmIoModule_I2c, 0x0);

    NvOdmPmuI2cWrite8(hBqI2c, 0xD6, TPS65914_I2C_SPEED, 0x01, 0x5B);
    NvOdmPmuI2cWrite8(hBqI2c, 0xD6, TPS65914_I2C_SPEED, 0x01, 0x5B);
    NvOdmPmuI2cUpdate8(hBqI2c,0xD6, TPS65914_I2C_SPEED, 0x00, 0, (1 << 7));

    NvOdmOsPrintf("[%u] TPS65914::BQ WDT and HiZ CLEARED\n\n",NvOdmOsGetTimeMS());
    NvOdmOsDebugPrintf("[%u] TPS65914::Alarm set to: %d/%d/%d, %d:%d:%d \n",
                                NvOdmOsGetTimeMS(),
                                time.Day, time.Month, time.Year,
                                time.Hour, time.Minute, time.Second );
}

void Tps65914PmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps65914PmuInfoHandle hTps65914PmuInfo;

    NV_ASSERT(hPmuDriver);
    if (!hPmuDriver)
        return;
    NvOsDebugPrintf("TPS65914::poweroff n\n");

    hTps65914PmuInfo = (Tps65914PmuInfoHandle)hPmuDriver;
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c, hTps65914PmuInfo->DeviceAddrPage1,
                       TPS65914_I2C_SPEED, TPS65914_DEVCNTRL, TPS65914_POWER_OFF);
    //shouldn't reach here
    NV_ASSERT(0);
}

void Tps65914SetInterruptMasks(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps65914PmuInfoHandle hTps65914PmuInfo;

    NV_ASSERT(hPmuDriver);

    if(!hPmuDriver)
        return;
    hTps65914PmuInfo = (Tps65914PmuInfoHandle) hPmuDriver;

    // Enable Clear-on-Read for Interrupt Status Registers
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT_CTRL, 0x1);

    // Unmask the necessary interrupts
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT1_MASK, 0xF8);
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT2_MASK, 0xFE);
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT3_MASK, 0x7F);
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT4_MASK, 0xFF);
/*
    // Enable RTC Alarm Interrupt
    NvOdmPmuI2cSetBits(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage1,
        TPS65914_I2C_SPEED, TPS65914_RTC_INTERRUPTS_REG,
        (1 << TPS65914_SHIFT_RTC_INTERRUPTS_REG_IT_ALARM));
*/
    NvOdmOsDebugPrintf("[%u] TPS65914::Enabled Interrupts\n",NvOdmOsGetTimeMS());
}

static void SetGpio_Pk5(NvBool Enable)
{
    NvOdmGpioPinHandle hOdmPin = NULL;
    NvOdmServicesGpioHandle hOdmGpio = NULL;
    hOdmGpio  = NvOdmGpioOpen();
    hOdmPin = NvOdmGpioAcquirePinHandle(hOdmGpio, 'k' - 'a', 5);
    NvOdmGpioConfig(hOdmGpio, hOdmPin, NvOdmGpioPinMode_Output);
    NvOdmGpioSetState(hOdmGpio, hOdmPin, Enable);
    NvOdmGpioClose(hOdmGpio);
}

void Tps65914DisableInterrupts(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps65914PmuInfoHandle hTps65914PmuInfo;
    NvU8 Status1 = 0;
    NvU8 Status2 = 0;
    NvU8 Status3 = 0;

    NV_ASSERT(hPmuDriver);

    if(!hPmuDriver)
        return;
    hTps65914PmuInfo = (Tps65914PmuInfoHandle) hPmuDriver;

    // Mask all interrupts
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT1_MASK, 0xFF);
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT2_MASK, 0xFF);
    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT3_MASK, 0xFF);

    NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT4_MASK, 0xFF);

    NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT1_STATUS, &Status1);
    NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT2_STATUS, &Status2);
    NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT3_STATUS, &Status3);
    NvOdmOsDebugPrintf("[%u] TPS65914::Disable  and cleared all interrupts\n",  \
                                        NvOdmOsGetTimeMS());
}

NvBool Tps65914PmuInterruptHandler(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps65914PmuInfoHandle hTps65914PmuInfo;
    NvU8 Status1 = 0;
    NvU8 Status2 = 0;
    NvU8 Status3 = 0;
    NvU8 RtcStatus = 0;
    NvBool Ret = NV_FALSE; // Set only for PWRON interrupt
    NvBool VbusLineState = NV_FALSE;
    NvOdmPmuI2cHandle hBq2477xI2c = NULL;
    NvU16 Val = 0;
    NvU8 Soc = 0;
    DateTime date;

    NV_ASSERT(hPmuDriver);

    if(!hPmuDriver)
        return NV_FALSE;

    hTps65914PmuInfo = (Tps65914PmuInfoHandle) hPmuDriver;

    NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT1_STATUS, &Status1);
    NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT2_STATUS, &Status2);
    NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
        hTps65914PmuInfo->DeviceAddrPage2,
        TPS65914_I2C_SPEED, TPS65914_INT3_STATUS, &Status3);

 //   NvOdmOsPrintf("[%u] TPS65914::Int1_Status - 0x%08x\n",NvOdmOsGetTimeMS(),Status1);
 //   NvOdmOsPrintf("[%u] TPS65914::Int2_Status - 0x%08x\n",NvOdmOsGetTimeMS(),Status2);
 //   NvOdmOsPrintf("[%u] TPS65914::Int3_Status - 0x%08x\n\n",NvOdmOsGetTimeMS(),Status3);

    if(Status1 & (1 << TPS65914_SHIFT_INT1_STATUS_PWRON))
    {
        NvOdmOsPrintf("\n[%u] TPS65914::PWRON DETECTED\n",NvOdmOsGetTimeMS());
        Ret = NV_TRUE;
		
    }
	return Ret;
    // Check for RTC Alarm interrupt
    if(Status2 & (1 << TPS65914_SHIFT_INT2_STATUS_RTC_ALARM))
    {
         NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
             hTps65914PmuInfo->DeviceAddrPage1,
             TPS65914_I2C_SPEED, TPS65914_RTC_STATUS_REG,
             &RtcStatus);

         NvOdmOsPrintf("\n[%u] TPS65914::RtcStatus - 0x%08x\n",NvOdmOsGetTimeMS(),RtcStatus);
         // Confirm that it is Alarm Interrupt
         if(RtcStatus & (1 << TPS65914_SHIFT_RTC_STATUS_REG_IT_ALARM))
         {
            // Clear Status by writing 1 to the corresponding bit
            NvOdmPmuI2cSetBits(hTps65914PmuInfo->hOdmPmuI2c,
                hTps65914PmuInfo->DeviceAddrPage1,
                TPS65914_I2C_SPEED,
                TPS65914_RTC_STATUS_REG,
                (1 << TPS65914_SHIFT_RTC_STATUS_REG_IT_ALARM));
            NvOdmOsPrintf("[%u] TPS65914::RTC ALARM DETECTED\n",NvOdmOsGetTimeMS());
            if(s_PmuBoardInfo.BoardID == PMU_E1735)
            {
#define BQ2477X_CHARGE_CURRENT_LSB              0x0A
#define BQ24Z45_SOC                             0x0D
                // Read ChargeCurrent register and write back the same value
                hBq2477xI2c = NvOdmPmuI2cOpen(NvOdmIoModule_I2c, 0x1);
                NvOdmPmuI2cRead16(hBq2477xI2c, 0xD4, 100,
                    BQ2477X_CHARGE_CURRENT_LSB, &Val);
                NvOdmPmuI2cWrite16(hBq2477xI2c, 0xD4, 100,
                    BQ2477X_CHARGE_CURRENT_LSB, Val);
                // Enable GPIO_PK5 to read the fuel gauge
                SetGpio_Pk5(NV_TRUE);
                // Doesn't matter using the same I2c Handle since it's the same instance
                NvOdmPmuI2cRead8(hBq2477xI2c, 0x16, 100,
                    BQ24Z45_SOC, &Soc);
                // Disable GPIO_PK5 after read
                SetGpio_Pk5(NV_FALSE);
                NvOdmOsPrintf("[%u] BQ24Z45::Soc - %u %\n\n",
                    NvOdmOsGetTimeMS(),Soc);
#ifdef LPM_BATTERY_CHARGING
                // If Charging complete, poweroff
                if(Soc == 100 && (NvOdmChargingInterface(NvOdmChargingID_IsCharging) == NV_TRUE))
                    Tps65914PmuPowerOff(hTps65914PmuInfo);
#endif
            }
            NvOdmOsPrintf("[%u] TPS65914::BQ WDT/HiZ CLEARED\n\n",NvOdmOsGetTimeMS());
            NvOdmPmuGetDate(hPmuDriver, &date);
            NvOdmPmuSetAlarm(hPmuDriver,NvAppAddAlarmSecs(date, 30));
        }
    }
    if(Status3 & (1 << TPS65914_SHIFT_INT3_STATUS_VBUS))
    {
        VbusLineState = GetPmuVbusLineState(hTps65914PmuInfo);
        NvMlUsbfSetVbusOverrideT1xx(VbusLineState);
        if(s_PmuBoardInfo.BoardID == PMU_E1735)
        {
#ifdef LPM_BATTERY_CHARGING
            if(NvOdmChargingInterface(NvOdmChargingID_IsCharging) == NV_TRUE)
            {
                // Set alarm to 1 hr from now
                NvOdmPmuGetDate(hPmuDriver, &date);
                NvOdmPmuSetAlarm(hPmuDriver,NvAppAddAlarmSecs(date, 3600));
                NvOdmOsPrintf("[%u] TPS65914::Charger detached\n[%u] TPS65914::Powering Off\n",
                    NvOdmOsGetTimeMS(),NvOdmOsGetTimeMS());
                // Poweroff
                Tps65914PmuPowerOff(hTps65914PmuInfo);
            }
#endif
        }
        NvOdmOsPrintf("\n[%u] TPS65914::VBUS CHANGE DETECTED\n",NvOdmOsGetTimeMS());
    }
    return Ret;
}

void Tps65914PmuGetBatCapacity(NvOdmPmuDriverInfoHandle hPmuDriver, NvU32* BatCapacity)
{
	Tps65914PmuInfoHandle hTps65914PmuInfo;
	NvU8 BatCap = 0;

	NV_ASSERT(hPmuDriver);
	NV_ASSERT(BatCapacity);
	if(!hPmuDriver || !BatCapacity)
		return;

	hTps65914PmuInfo = (Tps65914PmuInfoHandle)hPmuDriver;
	NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
			hTps65914PmuInfo->DeviceAddrPage1,
			TPS65914_I2C_SPEED, TPS65914_RTC_COMP_LSB_REG,
			&BatCap);

	*BatCapacity = BatCap;
	NvOdmOsPrintf("\n[%u] TPS65914::BatCapacity - 0x%x\n",NvOdmOsGetTimeMS(),*BatCapacity);
}

NvBool Tps65914AdcDriverReadData(NvOdmPmuDriverInfoHandle hAdcDriverInfo, NvU32 AdcChannelId, NvU32 *pData)
{
    NvU8 RegMSB = 0;
    NvU8 RegLSB = 0;
    NvBool ret;
	NvU32 i,temp,gain = 0;
	struct batt_adc_gain *p;
	p = gain_table;


    Tps65914PmuInfoHandle hTps65914PmuInfo = (Tps65914PmuInfoHandle)hAdcDriverInfo;

    NV_ASSERT(hTps65914PmuInfo);
    NV_ASSERT(pData);
    if (!hTps65914PmuInfo)
    {
        NvOdmOsPrintf("%s(): hTps65914PmuInfo is NULL\n", __func__);
        return NV_FALSE;
    }
    if (!pData)
    {
        NvOdmOsPrintf("%s(): pData is NULL\n", __func__);
        return NV_FALSE;
    }

    //Start the conversion
    ret = NvOdmPmuI2cWrite8(hTps65914PmuInfo->hOdmPmuI2c,
                       hTps65914PmuInfo->DeviceAddrPage2, TPS65914_I2C_SPEED,
                                                TPS65914_GPADC_SW_SELECT & 0xFF, 0x90 | AdcChannelId);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error writing to GPADC_SW_SEL\n", __func__);
        return NV_FALSE;
    }
    //Read the voltage values
    ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
                             hTps65914PmuInfo->DeviceAddrPage2, TPS65914_I2C_SPEED,
                                        TPS65914_GPADC_SW_CONV0_LSB & 0xFF, &RegLSB);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error reading GPADC_SW_CONV0_LSB \n", __func__);
        return NV_FALSE;
    }

    ret = NvOdmPmuI2cRead8(hTps65914PmuInfo->hOdmPmuI2c,
                             hTps65914PmuInfo->DeviceAddrPage2, TPS65914_I2C_SPEED,
                                        TPS65914_GPADC_SW_CONV0_MSB & 0xFF, &RegMSB);
    if (!ret)
    {
        NvOdmOsPrintf("%s(): Error reading GPADC_SW_CONV0_MSB \n", __func__);
        return NV_FALSE;
    }

    *pData =(RegMSB << 8) + RegLSB;

	temp = *pData;

	if(temp >= (p[GAIN_NUM - 1].adc_val)){
		gain = p[GAIN_NUM - 1].gain;
	}
	else
	{
		if(temp <= (p[0].adc_val))
		{
			gain = p[0].gain;;
		}
		else
		{
			for(i = 0; i < GAIN_NUM - 1; i++)
			{
				if(((p[i].adc_val) <= temp) && (temp < (p[i+1].adc_val))){
						gain = p[i].gain + ((temp - p[i].adc_val)*(p[i+1].gain - p[i].gain)) / (p[i+1].adc_val - p[i].adc_val);
						break;
				}
			}
		}
	}

	*pData = (temp + 303)*2 + gain;// scale =2 , 303 ==>the gain of adc sample, gain ==>the gain of div voltage resistor

    return NV_TRUE;
}
