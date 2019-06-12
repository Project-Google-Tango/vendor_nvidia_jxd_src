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
  * @file: <b>NVIDIA Tegra ODM Kit: Ricoh 583 pmu driver</b>
  *
  * @b Description: Implements the Ricoh583 pmu drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_pmu_ricoh583_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"

#define RICOHPMUGUID NV_ODM_GUID('r','i','c','o','h','5','8','3')
#define INVALID_VOLTAGE 0xFFFFFFFF

#define RICOH583_I2C_SPEED 100

typedef struct Ricoh583PmuInfoRec *Ricoh583PmuInfoHandle;

typedef struct Ricoh583PmuOpsRec {
    NvBool (*IsRailEnabled)(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 VddId);
    NvBool (*EnableRail)(Ricoh583PmuInfoHandle hR583PmuInfo, NvU32 VddId,
            NvU32 *DelayUS);
    NvBool (*DisableRail)(Ricoh583PmuInfoHandle hR583PmuInfo, NvU32 VddId,
            NvU32 *DelayUS);
    NvBool (*SetRailVoltage)(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);
    NvBool (*GetRailVoltage)(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 VddId, NvU32 *uV);
} Ricoh583PmuOps;

typedef struct Ricoh583PmuPropsRec {
    NvU32 Id;
    /* Regulator register address.*/
    NvU8        reg_en_reg;
    NvU8        en_bit;
    NvU8        reg_disc_reg;
    NvU8        disc_bit;
    NvU8        vout_reg;
    NvU8        vout_mask;

    /* Pmu voltage props */
    NvU32       min_uV;
    NvU32       max_uV;
    NvU32       step_uV;
    NvU32       nsteps;

    /* Power rail specific turn-on delay */
    NvU32       delay;
    Ricoh583PmuOps *pOps;
} Ricoh583PmuProps;

typedef struct Ricoh583RailInfoRec {
    Ricoh583PmuSupply   VddId;
    NvU32               SystemRailId;
    int                 UserCount;
    NvBool              IsValid;
    Ricoh583PmuProps    *pPmuProps;
    NvOdmPmuRailProps     OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Ricoh583RailInfo;

typedef struct Ricoh583PmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    Ricoh583RailInfo RailInfo[Ricoh583PmuSupply_Num];
} Ricoh583PmuInfo;

static NvBool Ricoh583IsRailEnabled(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 VddId);

static NvBool Ricoh583RailEnable(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 VddId, NvU32 *DelayUS);

static NvBool Ricoh583RailDisable(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 VddId, NvU32 *DelayUS);

static NvBool Ricoh583RailSetVoltage(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Ricoh583RailGetVoltage(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 VddId, NvU32 *uV);

static NvBool Ricoh583GpioRailIsEnabled(Ricoh583PmuInfoHandle hR583PmuInfo,
                NvU32 RailId);

static NvBool Ricoh583GpioRailEnable(Ricoh583PmuInfoHandle hR583PmuInfo,
                NvU32 RailId, NvU32 *DelayUS);

static NvBool Ricoh583GpioRailDisable(Ricoh583PmuInfoHandle hR583PmuInfo,
                NvU32 RailId, NvU32 *DelayUS);

static NvBool Ricoh583GpioGetVoltage(Ricoh583PmuInfoHandle hR583PmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool Ricoh583GpioSetVoltage(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static Ricoh583PmuOps s_PmuOps = {
    Ricoh583IsRailEnabled,
    Ricoh583RailEnable,
    Ricoh583RailDisable,
    Ricoh583RailSetVoltage,
    Ricoh583RailGetVoltage,
};

static Ricoh583PmuOps s_GpioPmuOps = {
    Ricoh583GpioRailIsEnabled,
    Ricoh583GpioRailEnable,
    Ricoh583GpioRailDisable,
    Ricoh583GpioSetVoltage,
    Ricoh583GpioGetVoltage,
};

#define REG_DATA(_id, _en_reg, _en_bit, _disc_reg, _disc_bit, _vout_reg, \
        _vout_mask, _min_uV, _max_mV, _step_mV, _nsteps, _pOps, _delay)  \
    {       \
            Ricoh583PmuSupply_##_id,                                     \
            _en_reg, _en_bit, _disc_reg, _disc_bit, _vout_reg, _vout_mask, \
            _min_uV*1000, _max_mV*1000, _step_mV, _nsteps, _delay, _pOps, \
    }
static Ricoh583PmuProps s_Ricoh583PmuProps[] = {
    REG_DATA(Invalid, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &s_PmuOps, 500),
    REG_DATA(DC0, 0x30, 0, 0x30, 1, 0x31, 0x7F, 700, 1500, 12500, 0x40, &s_PmuOps, 500),
    REG_DATA(DC1, 0x34, 0, 0x34, 1, 0x35, 0x7F, 700, 1500, 12500, 0x40, &s_PmuOps, 500),
    REG_DATA(DC2, 0x38, 0, 0x38, 1, 0x39, 0x7F, 900, 2400, 12500, 0x78, &s_PmuOps, 500),
    REG_DATA(DC3, 0x3C, 0, 0x3C, 1, 0x3D, 0x7F, 900, 2400, 12500, 0x78, &s_PmuOps, 500),
    REG_DATA(LDO0, 0x51, 0, 0x53, 0, 0x54, 0x7F, 900, 3400, 25000, 0x64, &s_PmuOps, 500),
    REG_DATA(LDO1, 0x51, 1, 0x53, 1, 0x55, 0x7F, 900, 3400, 25000, 0x64, &s_PmuOps, 500),
    REG_DATA(LDO2, 0x51, 2, 0x53, 2, 0x56, 0x7F, 900, 3400, 25000, 0x64, &s_PmuOps, 500),
    REG_DATA(LDO3, 0x51, 3, 0x53, 3, 0x57, 0x7F, 900, 3400, 25000, 0x64, &s_PmuOps, 500),
    REG_DATA(LDO4, 0x51, 4, 0x53, 4, 0x58, 0x3F, 750, 1500, 12500, 0x3C, &s_PmuOps, 500),
    REG_DATA(LDO5, 0x51, 5, 0x53, 5, 0x59, 0x7F, 900, 3400, 25000, 0x64, &s_PmuOps, 500),
    REG_DATA(LDO6, 0x51, 6, 0x53, 6, 0x5A, 0x7F, 900, 3400, 25000, 0x64, &s_PmuOps, 500),
    REG_DATA(LDO7, 0x51, 7, 0x53, 7, 0x5B, 0x7F, 900, 3400, 25000, 0x64, &s_PmuOps, 500),
    REG_DATA(LDO8, 0x50, 0, 0x52, 0, 0x5C, 0x7F, 900, 3400, 25000, 0x64, &s_PmuOps, 500),
    REG_DATA(LDO9, 0x50, 1, 0x52, 1, 0x5D, 0x7F, 900, 3400, 25000, 0x64, &s_PmuOps, 500),
    REG_DATA(GPIO0, 0xA0, 0, 0xA2, 0, 0xFF, 0xFF, 0, 5000, 5000, 2, &s_GpioPmuOps, 500),
    REG_DATA(GPIO1, 0xA0, 1, 0xA2, 1, 0xFF, 0xFF, 0, 5000, 5000, 2, &s_GpioPmuOps, 500),
    REG_DATA(GPIO2, 0xA0, 2, 0xA2, 2, 0xFF, 0xFF, 0, 5000, 5000, 2, &s_GpioPmuOps, 500),
    REG_DATA(GPIO3, 0xA0, 3, 0xA2, 3, 0xFF, 0xFF, 0, 5000, 5000, 2, &s_GpioPmuOps, 500),
    REG_DATA(GPIO4, 0xA0, 4, 0xA2, 4, 0xFF, 0xFF, 0, 5000, 5000, 2, &s_GpioPmuOps, 500),
    REG_DATA(GPIO5, 0xA0, 5, 0xA2, 5, 0xFF, 0xFF, 0, 5000, 5000, 2, &s_GpioPmuOps, 500),
    REG_DATA(GPIO6, 0xA0, 6, 0xA2, 6, 0xFF, 0xFF, 0, 5000, 5000, 2, &s_GpioPmuOps, 500),
    REG_DATA(GPIO7, 0xA0, 7, 0xA2, 7, 0xFF, 0xFF, 0, 5000, 5000, 2, &s_GpioPmuOps, 500),
};

static NvBool Ricoh583IsRailEnabled(Ricoh583PmuInfoHandle hR583PmuInfo, NvU32 RailId)
{
    NvU8 Control;
    NvBool ret;
    Ricoh583PmuProps *pPmuProps = hR583PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, pPmuProps->reg_en_reg, &Control);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the control register 0x%02x\n",
                    __func__, pPmuProps->reg_en_reg);
        return ret;
    }
    return (((Control >> pPmuProps->en_bit) & 1) == 1);
}

static NvBool Ricoh583RailEnable(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Ricoh583PmuProps *pPmuProps = hR583PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cSetBits(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, pPmuProps->reg_en_reg, (1 << pPmuProps->en_bit));
    if (!ret) {
        NvOdmOsPrintf("%s() Error in setting bits the control register 0x%02x\n",
                    __func__, pPmuProps->reg_en_reg);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Ricoh583RailDisable(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Ricoh583PmuProps *pPmuProps = hR583PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cClrBits(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, pPmuProps->reg_en_reg, (1 << pPmuProps->en_bit));
    if (!ret) {
        NvOdmOsPrintf("%s() Error in clearing bits the control register 0x%02x\n",
                    __func__, pPmuProps->reg_en_reg);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Ricoh583RailSetVoltage(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU32 vsel;
    NvBool ret;
    Ricoh583PmuProps *pPmuProps = hR583PmuInfo->RailInfo[RailId].pPmuProps;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    vsel = (new_uV - pPmuProps->min_uV + pPmuProps->step_uV - 1)/ pPmuProps->step_uV;
    if (vsel > pPmuProps->nsteps)
        return NV_FALSE;

    ret = NvOdmPmuI2cUpdate8(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, pPmuProps->vout_reg, vsel, pPmuProps->vout_mask);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating voltage register 0x%02x\n",
                    __func__, pPmuProps->vout_reg);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Ricoh583RailGetVoltage(Ricoh583PmuInfoHandle hR583PmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    NvU8 vsel;
    NvBool ret;
    Ricoh583PmuProps *pPmuProps = hR583PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, pPmuProps->vout_reg, &vsel);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading voltage register 0x%02x\n",
                    __func__, pPmuProps->vout_reg);
        return ret;
    }
    vsel &= vsel & pPmuProps->vout_mask;
    *uV = pPmuProps->min_uV + vsel * pPmuProps->step_uV;
    return NV_TRUE;
}

static NvBool Ricoh583GpioRailIsEnabled(Ricoh583PmuInfoHandle hR583PmuInfo,
                NvU32 RailId)
{
    NvBool ret;
    Ricoh583PmuProps *pPmuProps = hR583PmuInfo->RailInfo[RailId].pPmuProps;
    NvU8 Val;

    ret = NvOdmPmuI2cRead8(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, pPmuProps->reg_disc_reg, &Val);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in readin the supply register\n", __func__);
        return ret;
    }
    if (Val & (1 << pPmuProps->disc_bit))
        return NV_TRUE;
    return NV_FALSE;
}
static NvBool Ricoh583GpioRailEnable(Ricoh583PmuInfoHandle hR583PmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Ricoh583PmuProps *pPmuProps = hR583PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cSetBits(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, pPmuProps->reg_disc_reg, 1 << pPmuProps->disc_bit);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Ricoh583GpioRailDisable(Ricoh583PmuInfoHandle hR583PmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Ricoh583PmuProps *pPmuProps = hR583PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cClrBits(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, pPmuProps->reg_disc_reg, 1 << pPmuProps->disc_bit);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the supply register\n", __func__);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Ricoh583GpioGetVoltage(Ricoh583PmuInfoHandle hR583PmuInfo,
                NvU32 RailId, NvU32 *uV)
{
    Ricoh583PmuProps *pPmuProps = hR583PmuInfo->RailInfo[RailId].pPmuProps;
    NvBool Ret;
    NvU8 Val;

    Ret = NvOdmPmuI2cRead8(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, pPmuProps->reg_disc_reg, &Val);
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in reading rail %d register\n", __func__, RailId);
        return Ret;
    }

    if (Val & (1 << pPmuProps->disc_bit))
        *uV = hR583PmuInfo->RailInfo[RailId].Caps.requestMilliVolts * 1000;
    else
        *uV = NVODM_VOLTAGE_OFF;
    return NV_TRUE;
}

static NvBool Ricoh583GpioSetVoltage(Ricoh583PmuInfoHandle hR583PmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)

{
    Ricoh583PmuProps *pPmuProps = hR583PmuInfo->RailInfo[RailId].pPmuProps;
    NvBool Ret;
    Ret = NvOdmPmuI2cSetBits(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, pPmuProps->reg_en_reg, (1 << pPmuProps->en_bit));
    if (!Ret) {
        NvOdmOsPrintf("%s() Error in reading rail %d register\n", __func__, RailId);
        return Ret;
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return NV_TRUE;
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(RICOHPMUGUID);
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

NvOdmPmuDriverInfoHandle Ricoh583PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, int NrOfOdmRailProps)
{
    Ricoh583PmuInfoHandle hR583PmuInfo;
    int i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvBool Ret;
    NvU32 VddId;
    NvU8 RegVal;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    /* Information for all rail is required */
    if (NrOfOdmRailProps != Ricoh583PmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }
    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
    if (!Ret)
        return NULL;

    hR583PmuInfo = NvOdmOsAlloc(sizeof(Ricoh583PmuInfo));
    if (!hR583PmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hR583PmuInfo, 0, sizeof(Ricoh583PmuInfo));

    hR583PmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hR583PmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        goto I2cOpenFailed;
    }
    hR583PmuInfo->DeviceAddr = I2cAddress;

    hR583PmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hR583PmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hR583PmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hR583PmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 0; i < Ricoh583PmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;
        if (VddId == Ricoh583PmuSupply_Invalid)
                continue;

        if (pOdmRailProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been provided, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        if (VddId != s_Ricoh583PmuProps[VddId].Id)
        {
            NvOdmOsPrintf("%s(): The entries for vdd %d are not in proper sequence, "
                        "got %d, Ignoring this entry\n", __func__, VddId, s_Ricoh583PmuProps[VddId].Id);
            continue;
        }

        if (hR583PmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        hR583PmuInfo->RailInfo[VddId].pPmuProps = &s_Ricoh583PmuProps[VddId];

        NvOdmOsMemcpy(&hR583PmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));

        hR583PmuInfo->RailInfo[VddId].Caps.OdmProtected =
                        hR583PmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        if (hR583PmuInfo->RailInfo[VddId].OdmProps.Min_mV)
             hR583PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        NV_MAX(hR583PmuInfo->RailInfo[VddId].OdmProps.Min_mV,
                           hR583PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000);
        else
             hR583PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        hR583PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000;

        if (hR583PmuInfo->RailInfo[VddId].OdmProps.Max_mV)
             hR583PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                        NV_MIN(hR583PmuInfo->RailInfo[VddId].OdmProps.Max_mV,
                           hR583PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000);
        else
             hR583PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                           hR583PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000;

        hR583PmuInfo->RailInfo[VddId].Caps.StepMilliVolts =
                           hR583PmuInfo->RailInfo[VddId].pPmuProps->step_uV/1000;

        if (hR583PmuInfo->RailInfo[VddId].OdmProps.Setup_mV)
             hR583PmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                        hR583PmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                        "Ignoring this entry\n", __func__,
                         hR583PmuInfo->RailInfo[VddId].OdmProps.Setup_mV,
                         VddId);
            continue;
        }
        hR583PmuInfo->RailInfo[VddId].UserCount  = 0;
        hR583PmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hR583PmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;

        hR583PmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                        hR583PmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
    }

    Ret = NvOdmPmuI2cRead8(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, 0, &RegVal);
    if (!Ret)
        NvOdmOsPrintf("%s() Error in reading the register 0 -> 0x%02x\n",
                    __func__, 0);
    NvOdmOsPrintf("%s() The LSI version number 0x%02x\n", __func__, RegVal);

    Ret = NvOdmPmuI2cRead8(hR583PmuInfo->hOdmPmuI2c, hR583PmuInfo->DeviceAddr,
            RICOH583_I2C_SPEED, 1, &RegVal);
    if (!Ret)
        NvOdmOsPrintf("%s() Error in reading the register 1 -> 0x%02x\n",
                    __func__, 0);
    NvOdmOsPrintf("%s() The Fuse version number 0x%02x\n", __func__, RegVal);

    return (NvOdmPmuDriverInfoHandle)hR583PmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hR583PmuInfo->hOdmPmuService);
PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hR583PmuInfo->hOdmPmuI2c);
I2cOpenFailed:
    NvOdmOsFree(hR583PmuInfo);
    return NULL;
}

void Ricoh583PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Ricoh583PmuInfoHandle hR583PmuInfo = (Ricoh583PmuInfoHandle)hPmuDriver;

    NvOdmServicesPmuClose(hR583PmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hR583PmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hR583PmuInfo);
}

void Ricoh583PmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Ricoh583PmuInfoHandle hR583PmuInfo =  (Ricoh583PmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Ricoh583PmuSupply_Num);
    NvOdmOsMemcpy(pCapabilities, &hR583PmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Ricoh583PmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    Ricoh583PmuInfoHandle hR583PmuInfo =  (Ricoh583PmuInfoHandle)hPmuDriver;
    Ricoh583PmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Ricoh583PmuSupply_Num);

    pPmuProps = hR583PmuInfo->RailInfo[VddId].pPmuProps;
    *pMilliVolts = 0;
    if (pPmuProps->pOps->IsRailEnabled(hR583PmuInfo, VddId)) {
        Status = pPmuProps->pOps->GetRailVoltage(hR583PmuInfo, VddId, &uV);
        if (Status) {
            *pMilliVolts = uV /1000;
        }
    }
    return Status;
}

NvBool Ricoh583PmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    Ricoh583PmuInfoHandle hR583PmuInfo =  (Ricoh583PmuInfoHandle)hPmuDriver;
    Ricoh583PmuProps *pPmuProps;
    Ricoh583RailInfo *pRailInfo;
    NvBool Status = NV_TRUE;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Ricoh583PmuSupply_Num);

    pRailInfo = &hR583PmuInfo->RailInfo[VddId];
    pPmuProps = hR583PmuInfo->RailInfo[VddId].pPmuProps;
    if (pSettleMicroSeconds)
        *pSettleMicroSeconds = 0;

    if (pRailInfo->OdmProps.IsOdmProtected == NV_TRUE)
    {
        NvOdmOsPrintf("%s:The voltage rail %d is protected and can not be set.\n",
                    __func__, VddId);
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

    NvOdmOsMutexLock(hR583PmuInfo->hPmuMutex);
    if (MilliVolts == ODM_VOLTAGE_OFF)
    {
        Status = NV_TRUE;
        if (!pRailInfo->UserCount)
        {
            NvOdmOsPrintf("%s:The imbalance in voltage off No user for rail %d\n",
                        __func__, VddId);
            goto End;
        }

        if (pRailInfo->UserCount == 1)
        {
            NvOdmServicesPmuSetSocRailPowerState(hR583PmuInfo->hOdmPmuService,
                        hR583PmuInfo->RailInfo[VddId].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hR583PmuInfo, VddId,
                                            pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hR583PmuInfo->hOdmPmuService,
                        hR583PmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hR583PmuInfo, VddId,
                                MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hR583PmuInfo, VddId,
                                                        pSettleMicroSeconds);
        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                        __func__, VddId);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hR583PmuInfo->hPmuMutex);
    return Status;
}
