/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: TPS6591x pmu driver</b>
  *
  * @b Description: Implements the TPS6591x pmu drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "tps6591x/nvodm_pmu_tps6591x_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "tps6591x_reg.h"

#define TPS6591X_PMUGUID NV_ODM_GUID('t','p','s','6','5','9','1','x')

/* Tps659119 (mostly use in Automotive) has significant difference in
 * vddctrl compare to other TPs6591xy. There is no defined/specific reg
 * to identify Tps659119.
 */
#define TPS6591A_PMUGUID NV_ODM_GUID('t','p','s','6','5','9','1','a')
#define TPS6591X_I2C_SPEED 100
/* device control registers */
#define TPS6591X_DEVCTRL        0x3F
#define DEVCTRL_PWR_OFF_SEQ     (1 << 7)
#define DEVCTRL_DEV_ON          (1 << 2)

static const NvU64 Tps6591xyGuidList[] =
{
    TPS6591X_PMUGUID,
    TPS6591A_PMUGUID
};

typedef struct Tps6591xPmuInfoRec *Tps6591xPmuInfoHandle;

typedef struct Tps6591xPmuOpsRec {
    NvBool (*IsRailEnabled)(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
            NvU32 VddId);
    NvBool (*EnableRail)(Tps6591xPmuInfoHandle hTps6591xPmuInfo, NvU32 VddId,
            NvU32 *DelayUS);
    NvBool (*DisableRail)(Tps6591xPmuInfoHandle hTps6591xPmuInfo, NvU32 VddId,
            NvU32 *DelayUS);
    NvBool (*SetRailVoltage)(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);
    NvBool (*GetRailVoltage)(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
            NvU32 VddId, NvU32 *uV);
} Tps6591xPmuOps;

typedef struct Tps6591xPmuPropsRec {
    NvU32           Id;
    /* Regulator supply register information.*/
    NvU8            supply_reg;
    NvU8            supply_def;
    NvU8            supply_nbits;
    NvU8            supply_sbit;

    /* Regulator Op/Sr control register information.*/
    NvU8            op_reg;
    NvU8            sr_reg;

    /* Pmu voltage props */
    NvU32            min_uV;
    NvU32            max_uV;
    NvU32            step_uV;
    NvU32            nsteps;

    /* Power rail specific turn-on delay */
    NvU32            delay;
    Tps6591xPmuOps *pOps;
} Tps6591xPmuProps;

typedef struct Tps6591xRailInfoRec {
    TPS6591xPmuSupply   VddId;
    NvU32               SystemRailId;
    int                 UserCount;
    NvBool              IsValid;
    Tps6591xPmuProps    *pPmuProps;
    NvOdmPmuRailProps     OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Tps6591xRailInfo;

typedef struct Tps6591xPmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    NvU64 ChipGuid;
    Tps6591xRailInfo RailInfo[TPS6591xPmuSupply_Num];
} Tps6591xPmuInfo;

static NvBool Tps6591xIsRailEnabled(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
               NvU32 VddId);

static NvBool Tps6591xRailEnable(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
               NvU32 VddId, NvU32 *DelayUS);

static NvBool Tps6591xRailDisable(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
               NvU32 VddId, NvU32 *DelayUS);

static NvBool Tps6591xVDDGetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
               NvU32 RailId, NvU32 *uV);

static NvBool Tps6591xVDDSetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
               NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Tps6591xVIOGetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Tps6591xVIOSetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Tps6591xLDO124GetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Tps6591xLDO124SetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Tps6591xLDO35678GetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Tps6591xLDO35678SetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Tps6591xGpioRailIsEnabled(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId);

static NvBool Tps6591xGpioRailEnable(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS);

static NvBool Tps6591xGpioRailDisable(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS);

static NvBool Tps6591xGpioGetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Tps6591xGpioSetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static Tps6591xPmuOps s_PmuVDDOps = {
    Tps6591xIsRailEnabled,
    Tps6591xRailEnable,
    Tps6591xRailDisable,
    Tps6591xVDDSetVoltage,
    Tps6591xVDDGetVoltage,
};

static Tps6591xPmuOps s_PmuVIOOps = {
    Tps6591xIsRailEnabled,
    Tps6591xRailEnable,
    Tps6591xRailDisable,
    Tps6591xVIOSetVoltage,
    Tps6591xVIOGetVoltage,
};
static Tps6591xPmuOps s_PmuLDO124Ops = {
    Tps6591xIsRailEnabled,
    Tps6591xRailEnable,
    Tps6591xRailDisable,
    Tps6591xLDO124SetVoltage,
    Tps6591xLDO124GetVoltage,
};
static Tps6591xPmuOps s_PmuLDO35678Ops = {
    Tps6591xIsRailEnabled,
    Tps6591xRailEnable,
    Tps6591xRailDisable,
    Tps6591xLDO35678SetVoltage,
    Tps6591xLDO35678GetVoltage,
};

static Tps6591xPmuOps s_PmuGpioOps = {
    Tps6591xGpioRailIsEnabled,
    Tps6591xGpioRailEnable,
    Tps6591xGpioRailDisable,
    Tps6591xGpioSetVoltage,
    Tps6591xGpioGetVoltage,
};

#define REG_DATA(_id, _sup_reg, _sup_def, _sup_nbit, _sup_sbit,  _op_reg, \
            _sr_reg, _min_mV, _max_mV, _step_uV, _nsteps, _pOps, _delay)  \
    {       \
            TPS6591xPmuSupply_##_id,    \
                    TPS6591x_##_sup_reg, _sup_def, _sup_nbit, _sup_sbit, \
                    TPS6591x_##_op_reg, TPS6591x_##_sr_reg, \
                    _min_mV*1000, _max_mV*1000, _step_uV, _nsteps, _delay, _pOps, \
    }
/* Should be in same sequence as the enums are */
static Tps6591xPmuProps s_Tps6591xPmuProps[] = {
    REG_DATA(Invalid, RFF_INVALID, 0, 0, 0, RFF_INVALID, RFF_INVALID,
                    0, 0, 0, 0, &s_PmuVIOOps, 500),
    REG_DATA(VIO,     R20_VIO,     0xC1, 2, 2, RFF_INVALID, RFF_INVALID,
                            1500, 3300, 300, 0, &s_PmuVIOOps, 500),

    REG_DATA(VDD1,    R21_VDD1,    0x0D, 0, 0, R22_VDD1_OP, R23_VDD1_SR,
                            0, 1500, 12500, 0, &s_PmuVDDOps, 500),

    REG_DATA(VDD2,    R24_VDD2,    0x0D, 0, 0, R25_VDD2_OP, R26_VDD2_SR,
                            0, 1500, 12500, 0, &s_PmuVDDOps, 500),

    REG_DATA(VDDCTRL, R27_VDDCTRL,    0x01, 0, 0, R28_VDDCTRL_OP, R29_VDDCTRL_SR,
                            0, 1500, 12500, 0, &s_PmuVDDOps, 500),

    REG_DATA(LDO1,    R30_LDO1,    0x01, 6, 2, RFF_INVALID, RFF_INVALID,
                            1000, 3300, 50000, 0, &s_PmuLDO124Ops, 500),

    REG_DATA(LDO2,    R31_LDO2,    0x01, 6, 2, RFF_INVALID, RFF_INVALID,
                            1000, 3300, 50000, 0, &s_PmuLDO124Ops, 500),

    REG_DATA(LDO3,    R37_LDO3,    0x01, 5, 2, RFF_INVALID, RFF_INVALID,
                            1000, 3300, 100000, 0, &s_PmuLDO35678Ops, 500),

    REG_DATA(LDO4,    R36_LDO4,    0x01, 6, 2, RFF_INVALID, RFF_INVALID,
                            800, 3300, 50000, 0, &s_PmuLDO124Ops, 500),

    REG_DATA(LDO5,    R32_LDO5,    0x01, 5, 2, RFF_INVALID, RFF_INVALID,
                            1000, 3300, 100000, 0, &s_PmuLDO35678Ops, 500),

    REG_DATA(LDO6,    R35_LDO6,    0x01, 5, 2, RFF_INVALID, RFF_INVALID,
                            1000, 3300, 100000, 0, &s_PmuLDO35678Ops, 500),

    REG_DATA(LDO7,    R34_LDO7,    0x01, 5, 2, RFF_INVALID, RFF_INVALID,
                            1000, 3300, 100000, 0, &s_PmuLDO35678Ops, 500),

    REG_DATA(LDO8,    R33_LDO8,    0x01, 5, 2, RFF_INVALID, RFF_INVALID,
                            1000, 3300, 100000, 0, &s_PmuLDO35678Ops, 500),

    REG_DATA(RTC_OUT, R1E_VRTC,    0x01, 1, 3, RFF_INVALID, RFF_INVALID,
                            0, 1800, 1800000, 0, &s_PmuGpioOps, 500),

    REG_DATA(GPO0,    R60_GPIO0,  0x20, 1, 0, RFF_INVALID, RFF_INVALID,
                            0, 5000, 5000000, 0, &s_PmuGpioOps, 500),

    REG_DATA(GPO1,    R61_GPIO1,  0x20, 1, 0, RFF_INVALID, RFF_INVALID,
                            0, 5000, 5000000, 0, &s_PmuGpioOps, 500),

    REG_DATA(GPO2,    R62_GPIO2,  0x00, 1, 0, RFF_INVALID, RFF_INVALID,
                            0, 5000, 5000000, 0, &s_PmuGpioOps, 500),

    REG_DATA(GPO3,    R63_GPIO3,  0x20, 1, 0, RFF_INVALID, RFF_INVALID,
                            0, 5000, 5000000, 0, &s_PmuGpioOps, 500),

    REG_DATA(GPO4,    R64_GPIO4,  0x20, 1, 0, RFF_INVALID, RFF_INVALID,
                            0, 5000, 5000000, 0, &s_PmuGpioOps, 500),

    REG_DATA(GPO5,    R63_GPIO3,  0x20, 1, 0, RFF_INVALID, RFF_INVALID,
                            0, 5000, 5000000, 0, &s_PmuGpioOps, 500),

    REG_DATA(GPO6,    R66_GPIO6,  0x00, 1, 0, RFF_INVALID, RFF_INVALID,
                            0, 5000, 5000000, 0, &s_PmuGpioOps, 500),

    REG_DATA(GPO7,    R67_GPIO7,  0x00, 1, 0, RFF_INVALID, RFF_INVALID,
                            0, 5000, 5000000, 0, &s_PmuGpioOps, 500),

    REG_DATA(GPO8,    R68_GPIO8,  0x20, 1, 0, RFF_INVALID, RFF_INVALID,
                            0, 5000, 5000000, 0, &s_PmuGpioOps, 500),
};

#define mask_val(nbits) ((1U << (nbits)) - 1)
#define get_val(val, sbits, nbits) (((val) >> (sbits)) & mask_val(nbits))
#define update_val(val, num, sbits, nbits) \
       ((val & ~(mask_val(nbits) << sbits)) | ((num & mask_val(nbits)) << sbits))

static NvBool Tps6591xIsRailEnabled(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId)
{
    NvU8 Control;
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, &Control);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the supply register\n", __func__);
        return ret;
    }
    return ((Control & 1) == 1);
}

static NvBool Tps6591xRailEnable(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cUpdate8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, 1, 0x3);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps6591xRailDisable(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cUpdate8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, 0, 0x3);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps6591xVDDGetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    NvU8 Supply;
    NvU8 Sr;
    NvU8 Op;
    NvU8 vsel;
    NvBool ret;
    NvU32 min_prog_uV = 600000;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, &Supply);

    if (ret)
        ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c,
                    hTps6591xPmuInfo->DeviceAddr, TPS6591X_I2C_SPEED,
                    pPmuProps->sr_reg, &Sr);
    if (ret)
        ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c,
                    hTps6591xPmuInfo->DeviceAddr, TPS6591X_I2C_SPEED,
                    pPmuProps->op_reg, &Op);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the Voltage register\n", __func__);
        return ret;
    }
    if (!(Supply & 0x1))
        return ODM_VOLTAGE_OFF;

    vsel = (Op & 0x80) ? Sr & 0x7F : Op & 0x7F;

    if ((RailId == TPS6591xPmuSupply_VDDCTRL) &&
            (hTps6591xPmuInfo->ChipGuid == TPS6591A_PMUGUID))
    {
        min_prog_uV = 800000;
    }

    if (vsel == 0)
         *uV = ODM_VOLTAGE_OFF;
    else if (vsel <= 0x3)
        *uV = min_prog_uV;
    else
        *uV = (vsel - 0x3) * pPmuProps->step_uV + min_prog_uV;
    return NV_TRUE;
}

static NvBool Tps6591xVDDSetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel;
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 RegAdd;
    NvU8 RegVal;
    NvU8 Op;
    NvU32 min_prog_uV = 600000;

    if ((RailId == TPS6591xPmuSupply_VDDCTRL) &&
            (hTps6591xPmuInfo->ChipGuid == TPS6591A_PMUGUID))
    {
        min_prog_uV = 800000;
    }

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

     if (new_uV < min_prog_uV)
        vsel = 0x3;
     else
        vsel = (new_uV - min_prog_uV)/pPmuProps->step_uV + 0x3;

    ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c,
                hTps6591xPmuInfo->DeviceAddr, TPS6591X_I2C_SPEED,
                pPmuProps->op_reg, &Op);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the Voltage register\n", __func__);
        return ret;
    }

    if (Op & 0x80)
    {
        RegAdd = pPmuProps->sr_reg;
        RegVal = vsel | 0x80;
    }
    else
    {
        RegAdd = pPmuProps->op_reg;
        RegVal = vsel;
    }

    ret = NvOdmPmuI2cWrite8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, RegAdd, RegVal);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in writing the Voltage register\n", __func__);
        return ret;
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps6591xVIOGetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    NvU8 vsel;
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU32 VoltageLevelMv[] = {1500, 1800, 2500, 3300};

    ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, &vsel);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the supply register\n", __func__);
        return ret;
    }
    if (!(vsel & 0x1))
        return ODM_VOLTAGE_OFF;

    vsel = get_val(vsel, pPmuProps->supply_sbit, pPmuProps->supply_nbits);
    *uV = VoltageLevelMv[vsel] * 1000;
    return NV_TRUE;
}

static NvBool Tps6591xVIOSetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel;
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU32 VoltageLevelMv[] = {1500, 1800, 2500, 3300};
    NvU32 newVoltageMV = new_uV/1000;
    NvU8 ControlReg;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    if (newVoltageMV <= VoltageLevelMv[0])
        vsel = 0x0;
    else if ((newVoltageMV > VoltageLevelMv[0]) && newVoltageMV <= VoltageLevelMv[1])
        vsel = 0x1;
    else if ((newVoltageMV > VoltageLevelMv[1]) && newVoltageMV <= VoltageLevelMv[2])
        vsel = 0x2;
    else
        vsel = 0x3;

    ControlReg = pPmuProps->supply_def;
    ControlReg = update_val(ControlReg, vsel, pPmuProps->supply_sbit,
                            pPmuProps->supply_nbits);

    ret = NvOdmPmuI2cWrite8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, ControlReg);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in writing the supply register\n", __func__);
        return ret;
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps6591xLDO124GetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 val;
    NvBool ret;

    ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, &val);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the supply register\n", __func__);
        return ret;
    }

    if (!(val & 0x1))
        return ODM_VOLTAGE_OFF;

    val = get_val(val, pPmuProps->supply_sbit, pPmuProps->supply_nbits);
    if (val < 4)
        *uV = 1000000;
    else if (val > 0x32)
        *uV = 3300000;
    else
        *uV = (val - 0x4) * pPmuProps->step_uV + 1000000;
    return ret;
}

static NvBool Tps6591xLDO124SetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel;
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 ControlReg;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    if (new_uV <= 1000000)
        vsel = 0x4;
    else if (new_uV >= 3300000)
        vsel = 0x32;
    else
        vsel = ((new_uV - 1000000)/pPmuProps->step_uV) + 4;

    ControlReg = pPmuProps->supply_def;
    ControlReg = update_val(ControlReg, vsel, pPmuProps->supply_sbit,
                            pPmuProps->supply_nbits);
    ret = NvOdmPmuI2cWrite8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, ControlReg);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in writing the supply register\n", __func__);
        return ret;
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps6591xLDO35678GetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 val;
    NvBool ret;

    ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, &val);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the supply register\n", __func__);
        return ret;
    }

    if (!(val & 0x1))
        return ODM_VOLTAGE_OFF;

    val = get_val(val, pPmuProps->supply_sbit, pPmuProps->supply_nbits);
    if (val < 2)
        *uV = 1000000;
    else if (val > 0x19)
        *uV = 3300000;
    *uV = (val - 0x2)*pPmuProps->step_uV + 1000000;
    return ret;
}

static NvBool Tps6591xLDO35678SetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel;
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 ControlReg;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    if (new_uV <= 1000000)
        vsel = 0x2;
    else if (new_uV >= 3300000)
        vsel = 0x19;
    else
        vsel = ((new_uV - 1000000)/pPmuProps->step_uV) + 2;

    ControlReg = pPmuProps->supply_def;
    ControlReg = update_val(ControlReg, vsel, pPmuProps->supply_sbit,
                            pPmuProps->supply_nbits);
    ret = NvOdmPmuI2cWrite8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, ControlReg);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in writing the supply register\n", __func__);
        return ret;
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps6591xGpioRailIsEnabled(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId)
{
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 Val;

    ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, &Val);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in readin the supply register\n", __func__);
        return ret;
    }
    if ((Val & 0x5) == 0x5)
        return NV_TRUE;
    return NV_FALSE;
}

static NvBool Tps6591xGpioRailEnable(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cUpdate8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, 0x5, 0x7);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps6591xGpioRailDisable(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cUpdate8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, 0x4, 0x7);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps6591xGpioGetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
                NvU32 RailId, NvU32 *uV)
{
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;
    NvBool Ret;
    NvU8 Val;

    Ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
            TPS6591X_I2C_SPEED, pPmuProps->supply_reg, &Val);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in reading rail %d register\n", __func__, RailId);
        return Ret;
    }

    if ((Val & 0x5) == 0x5)
        *uV = hTps6591xPmuInfo->RailInfo[RailId].Caps.requestMilliVolts * 1000;
    else
        *uV = NVODM_VOLTAGE_OFF;
    return NV_TRUE;
}

static NvBool Tps6591xGpioSetVoltage(Tps6591xPmuInfoHandle hTps6591xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)

{
    Tps6591xPmuProps *pPmuProps = hTps6591xPmuInfo->RailInfo[RailId].pPmuProps;
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return NV_TRUE;
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress, NvU64 *pTpsChipGuid)
{
    NvU32 i, index;

    for (index = 0; index < NV_ARRAY_SIZE(Tps6591xyGuidList); index++)
    {
        const  NvOdmPeripheralConnectivity *pConnectivity =
            NvOdmPeripheralGetGuid(Tps6591xyGuidList[index]);
        if (pConnectivity)
        {
            *pTpsChipGuid = Tps6591xyGuidList[index];
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
    }

    NvOdmOsPrintf("TPS6591X: %s(): The system did not doscover PMU from the "
                     "database OR the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

NvOdmPmuDriverInfoHandle Tps6591xPmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, int NrOfOdmRailProps)
{
    Tps6591xPmuInfoHandle hTps6591xPmuInfo;
    int i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvU64 TpsChipGuid = 0;
    NvBool Ret;
    NvU32 VddId;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    /* Information for all rail is required */
    if (NrOfOdmRailProps != TPS6591xPmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }
    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress, &TpsChipGuid);
    if (!Ret)
        return NULL;

    hTps6591xPmuInfo = NvOdmOsAlloc(sizeof(Tps6591xPmuInfo));
    if (!hTps6591xPmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hTps6591xPmuInfo, 0, sizeof(Tps6591xPmuInfo));

    hTps6591xPmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hTps6591xPmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        goto I2cOpenFailed;
    }
    hTps6591xPmuInfo->DeviceAddr = I2cAddress;

    PmuDebugPrintf("%s() Device address is 0x%x\n", __func__, hTps6591xPmuInfo->DeviceAddr);

    hTps6591xPmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hTps6591xPmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hTps6591xPmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hTps6591xPmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    hTps6591xPmuInfo->ChipGuid = TpsChipGuid;

#ifdef  NV_EMBEDDED_BUILD
#define VMBCH_REG_OFFSET 0x6A
#define DCDCCTRL_REG_OFFSET 0x3E
#define VMBCH_TPS659119_VAL 0x22
#define PSKIP_MASK 0x38
    /* if current Chip GUID is TPS6591A_PMUGUID and VMBCH_REG != 0x22
     * fallback to TPS6591X_PMUGUID.
     */
    {
        NvU8 PmuRegVal = 0;

        Ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
                TPS6591X_I2C_SPEED, DCDCCTRL_REG_OFFSET, &PmuRegVal);
        if (!Ret) {
            NvOdmOsPrintf("%s() Error in reading reg DCDCCTRL.\n", __func__);
            goto  MutexCreateFailed;
        }

        // Clear bit 3,4,5 for better VDD1/VDD2/VIO voltage
        if (PmuRegVal & PSKIP_MASK) {
            PmuRegVal &= ~PSKIP_MASK;
            Ret = NvOdmPmuI2cWrite8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
                    TPS6591X_I2C_SPEED, DCDCCTRL_REG_OFFSET, PmuRegVal);

            if (!Ret) {
                NvOdmOsPrintf("%s() Error in writing reg DCDCCTRL.\n", __func__);
                goto  MutexCreateFailed;
            }
        }

        Ret = NvOdmPmuI2cRead8(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
                TPS6591X_I2C_SPEED, VMBCH_REG_OFFSET, &PmuRegVal);
        if (!Ret) {
            NvOdmOsPrintf("%s() Error in reading reg VMBCH.\n", __func__);
            goto  MutexCreateFailed;
        }

        if ((hTps6591xPmuInfo->ChipGuid == TPS6591X_PMUGUID) &&
                (PmuRegVal >= VMBCH_TPS659119_VAL))
            hTps6591xPmuInfo->ChipGuid =  TPS6591A_PMUGUID;
    }
#undef VMBCH_REG_OFFSET
#undef DCDCCTRL_REG_OFFSET
#undef PSKIP_MASK
#undef VMBCH_TPS659119_VAL
#endif

    PmuDebugPrintf("%s() Chip GUID is 0x%x\n", __func__, hTps6591xPmuInfo->ChipGuid);

    if (hTps6591xPmuInfo->ChipGuid == TPS6591A_PMUGUID)
    {
        if (s_Tps6591xPmuProps[TPS6591xPmuSupply_VDDCTRL].Id == TPS6591xPmuSupply_VDDCTRL)
        {
            s_Tps6591xPmuProps[TPS6591xPmuSupply_VDDCTRL].max_uV = 1870000;
            s_Tps6591xPmuProps[TPS6591xPmuSupply_VDDCTRL].step_uV = 16666;
        }
        else
        {
            NvOdmOsPrintf("%s(): The VDDCTRL entry not in proper sequence\n", __func__);
        }
    }

    for (i = 1; i < TPS6591xPmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;
        if (VddId == TPS6591xPmuSupply_Invalid)
                continue;

        if (pOdmRailProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been provided, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        if (VddId != s_Tps6591xPmuProps[VddId].Id)
        {
            NvOdmOsPrintf("%s(): The entries are not in proper sequence, "
                        "Ignoring this entry\n", __func__);
            continue;
        }

        if (hTps6591xPmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        hTps6591xPmuInfo->RailInfo[VddId].pPmuProps = &s_Tps6591xPmuProps[VddId];

        NvOdmOsMemcpy(&hTps6591xPmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));

        hTps6591xPmuInfo->RailInfo[VddId].Caps.OdmProtected =
                        hTps6591xPmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        if (hTps6591xPmuInfo->RailInfo[VddId].OdmProps.Min_mV)
             hTps6591xPmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        NV_MAX(hTps6591xPmuInfo->RailInfo[VddId].OdmProps.Min_mV,
                           hTps6591xPmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000);
        else
             hTps6591xPmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        hTps6591xPmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000;

        if (hTps6591xPmuInfo->RailInfo[VddId].OdmProps.Max_mV)
             hTps6591xPmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                        NV_MIN(hTps6591xPmuInfo->RailInfo[VddId].OdmProps.Max_mV,
                           hTps6591xPmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000);
        else
             hTps6591xPmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                           hTps6591xPmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000;

        hTps6591xPmuInfo->RailInfo[VddId].Caps.StepMilliVolts =
                           hTps6591xPmuInfo->RailInfo[VddId].pPmuProps->step_uV/1000;

        if (hTps6591xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV)
             hTps6591xPmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                        hTps6591xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                        "Ignoring this entry\n", __func__,
                         hTps6591xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV,
                         VddId);
            continue;
        }
        hTps6591xPmuInfo->RailInfo[VddId].UserCount  = 0;
        hTps6591xPmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hTps6591xPmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;

        hTps6591xPmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                        hTps6591xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV;

    }
    return (NvOdmPmuDriverInfoHandle)hTps6591xPmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hTps6591xPmuInfo->hOdmPmuService);
PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hTps6591xPmuInfo->hOdmPmuI2c);
I2cOpenFailed:
    NvOdmOsFree(hTps6591xPmuInfo);
    return NULL;
}

void Tps6591xPmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps6591xPmuInfoHandle hTps6591xPmuInfo = (Tps6591xPmuInfoHandle)hPmuDriver;

    NvOdmServicesPmuClose(hTps6591xPmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hTps6591xPmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hTps6591xPmuInfo);
}

void Tps6591xPmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Tps6591xPmuInfoHandle hTps6591xPmuInfo =  (Tps6591xPmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS6591xPmuSupply_Num);
    if (!hTps6591xPmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }
    if (VddId == TPS6591xPmuSupply_RTC_OUT)
    {
        NvOdmOsPrintf("%s(): Voltage control is not possible in this rail %d",
                        __func__, VddId);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }

    NvOdmOsMemcpy(pCapabilities, &hTps6591xPmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Tps6591xPmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    Tps6591xPmuInfoHandle hTps6591xPmuInfo =  (Tps6591xPmuInfoHandle)hPmuDriver;
    Tps6591xPmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS6591xPmuSupply_Num);

    if (!hTps6591xPmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    if (VddId == TPS6591xPmuSupply_RTC_OUT)
    {
        NvOdmOsPrintf("%s(): Voltage control is not possible in this rail %d",
                    __func__, VddId);
        return NV_FALSE;
    }

    pPmuProps = hTps6591xPmuInfo->RailInfo[VddId].pPmuProps;
    *pMilliVolts = 0;
    if (pPmuProps->pOps->IsRailEnabled(hTps6591xPmuInfo, VddId)) {
        Status = pPmuProps->pOps->GetRailVoltage(hTps6591xPmuInfo, VddId, &uV);
        if (Status) {
            *pMilliVolts = uV /1000;
        }
    }
    return Status;
}

NvBool Tps6591xPmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    Tps6591xPmuInfoHandle hTps6591xPmuInfo =  (Tps6591xPmuInfoHandle)hPmuDriver;
    Tps6591xPmuProps *pPmuProps;
    Tps6591xRailInfo *pRailInfo;
    NvBool Status = NV_TRUE;

    PmuDebugPrintf("%s(): Rail %u and voltage %u\n", __func__, VddId, MilliVolts);
    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS6591xPmuSupply_Num);
    if (!hTps6591xPmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }
    if (VddId == TPS6591xPmuSupply_RTC_OUT)
    {
        NvOdmOsPrintf("%s(): Voltage control is not possible in this rail %d",
                    __func__, VddId);
        return NV_FALSE;
    }

    pRailInfo = &hTps6591xPmuInfo->RailInfo[VddId];
    pPmuProps = hTps6591xPmuInfo->RailInfo[VddId].pPmuProps;
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

    NvOdmOsMutexLock(hTps6591xPmuInfo->hPmuMutex);
    if (MilliVolts == ODM_VOLTAGE_OFF)
    {
        if (!pRailInfo->UserCount)
        {
            NvOdmOsPrintf("%s:The imbalance in voltage off No user for rail %d\n",
                        __func__, VddId);
            Status = NV_TRUE;
            goto End;
        }

        Status = NV_TRUE;
        if (pRailInfo->UserCount == 1)
        {
            NvOdmServicesPmuSetSocRailPowerState(hTps6591xPmuInfo->hOdmPmuService,
                        hTps6591xPmuInfo->RailInfo[VddId].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hTps6591xPmuInfo, VddId,
                                            pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hTps6591xPmuInfo->hOdmPmuService,
                        hTps6591xPmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hTps6591xPmuInfo, VddId,
                                MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hTps6591xPmuInfo, VddId,
                                                        pSettleMicroSeconds);
        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                        __func__, VddId);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hTps6591xPmuInfo->hPmuMutex);
    return Status;
}

void Tps6591xPmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps6591xPmuInfoHandle hTps6591xPmuInfo;

    NV_ASSERT(hPmuDriver);
    hTps6591xPmuInfo = (Tps6591xPmuInfoHandle)hPmuDriver;

    NvOdmPmuI2cSetBits(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
                TPS6591X_I2C_SPEED, TPS6591X_DEVCTRL, DEVCTRL_PWR_OFF_SEQ);


    NvOdmPmuI2cClrBits(hTps6591xPmuInfo->hOdmPmuI2c, hTps6591xPmuInfo->DeviceAddr,
                TPS6591X_I2C_SPEED, TPS6591X_DEVCTRL, DEVCTRL_DEV_ON);
    //shouldn't reach here
    NV_ASSERT(0);
}
