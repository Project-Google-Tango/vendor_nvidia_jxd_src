/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
/**
 * @file: <b>NVIDIA Tegra ODM Kit: AS3722 pmu driver</b>
 *
 * @b Description: Implements the AS3722 pmu drivers.
 *
 */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "as3722/nvodm_pmu_as3722_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "as3722_reg.h"

#define AS3722_PMUGUID NV_ODM_GUID('a','s','3','7','2','2',' ',' ')

#define AS3722_I2C_SPEED 100
#define AS3722_SD_VOLTAGE_RANGE 0x80

typedef struct As3722PmuInfoRec *As3722PmuInfoHandle;

typedef struct As3722PmuOpsRec {
    NvBool (*IsRailEnabled)(As3722PmuInfoHandle hAs3722PmuInfo, NvU32 VddId);
    NvBool (*EnableRail)(As3722PmuInfoHandle hAs3722PmuInfo, NvU32 VddId, NvU32 *DelayUS);
    NvBool (*DisableRail)(As3722PmuInfoHandle hAs3722PmuInfo, NvU32 VddId, NvU32 *DelayUS);
    NvBool (*SetRailVoltage)(As3722PmuInfoHandle hAs3722PmuInfo, NvU32 VddId, NvU32 new_uV,
            NvU32 *DelayUS);
    NvBool (*GetRailVoltage)(As3722PmuInfoHandle hAs3722PmuInfo, NvU32 VddId, NvU32 *uV);
} As3722PmuOps;

typedef struct As3722PmuPropsRec {
    NvU8  reg_id;
    NvU8  reg_vsel;
    NvU32 reg_enable;
    NvU8  enable_bit;
    NvU32 min_uV;
    NvU32 max_uV;
    NvU32 delay;
    As3722PmuOps *pOps;
} As3722PmuProps;

typedef struct As3722RailInfoRec {
    AS3722PmuSupply   VddId;
    NvU32               SystemRailId;
    NvU32               UserCount;
    NvBool              IsValid;
    As3722PmuProps    *pPmuProps;
    NvOdmPmuRailProps   OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} As3722RailInfo;

typedef struct As3722PmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    As3722RailInfo RailInfo[AS3722PmuSupply_Num];
} As3722PmuInfo;

static NvBool As3722IsRailEnabled(As3722PmuInfoHandle hAs3722PmuInfo,
               NvU32 VddId);

static NvBool As3722RailEnable(As3722PmuInfoHandle hAs3722PmuInfo,
               NvU32 VddId, NvU32 *DelayUS);

static NvBool As3722RailDisable(As3722PmuInfoHandle hAs3722PmuInfo,
               NvU32 VddId, NvU32 *DelayUS);

static NvBool As3722LDOGetVoltage(As3722PmuInfoHandle hAs3722PmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool As3722LDOSetVoltage(As3722PmuInfoHandle hAs3722PmuInfo,
                NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool As3722SDGetVoltage(As3722PmuInfoHandle hAs3722PmuInfo,
                NvU32 RailId, NvU32 *uV);

static NvBool As3722SDSetVoltage(As3722PmuInfoHandle hAs3722PmuInfo,
                NvU32 RailId, NvU32 uV, NvU32 *DelayUS);

static As3722PmuOps s_PmuLDOOps = {
    As3722IsRailEnabled,
    As3722RailEnable,
    As3722RailDisable,
    As3722LDOSetVoltage,
    As3722LDOGetVoltage,
};

static As3722PmuOps s_PmuSDOps = {
    As3722IsRailEnabled,
    As3722RailEnable,
    As3722RailDisable,
    As3722SDSetVoltage,
    As3722SDGetVoltage,
};

#define REG_DATA(_reg_id, _reg_vsel, _reg_enable, _min_uV, _max_uV, _delay, _ops) \
{       \
    AS3722PmuSupply_##_reg_id, \
    AS3722_##_reg_vsel,    \
    AS3722_##_reg_enable,    \
    AS3722_##_reg_id##_##ON, \
    _min_uV, \
    _max_uV, \
    _delay, \
    _ops, \
}

/* Should be in same sequence as the enums are */
static As3722PmuProps s_As3722PmuProps[] = {
    REG_DATA(Invalid, RFF_INVALID, LDOCONTROL0_REG,0,0,0,0),
    REG_DATA(LDO0,    LDO0_VOLTAGE_REG,  LDOCONTROL0_REG, 825000, 1250000, 0, &s_PmuLDOOps),
    REG_DATA(LDO1,    LDO1_VOLTAGE_REG,  LDOCONTROL0_REG, 825000, 3300000, 0, &s_PmuLDOOps),
    REG_DATA(LDO2,    LDO2_VOLTAGE_REG,  LDOCONTROL0_REG, 825000, 3300000, 0, &s_PmuLDOOps),
    REG_DATA(LDO3,    LDO3_VOLTAGE_REG,  LDOCONTROL0_REG, 610000, 1500000, 0, &s_PmuLDOOps),
    REG_DATA(LDO4,    LDO4_VOLTAGE_REG,  LDOCONTROL0_REG, 825000, 3300000, 0, &s_PmuLDOOps),
    REG_DATA(LDO5,    LDO5_VOLTAGE_REG,  LDOCONTROL0_REG, 825000, 3300000, 0, &s_PmuLDOOps),
    REG_DATA(LDO6,    LDO6_VOLTAGE_REG,  LDOCONTROL0_REG, 825000, 3300000, 0, &s_PmuLDOOps),
    REG_DATA(LDO7,    LDO7_VOLTAGE_REG,  LDOCONTROL0_REG, 825000, 3300000, 0, &s_PmuLDOOps),
    REG_DATA(LDO9,    LDO9_VOLTAGE_REG,  LDOCONTROL1_REG, 825000, 3300000, 0, &s_PmuLDOOps),
    REG_DATA(LDO10,   LDO10_VOLTAGE_REG, LDOCONTROL1_REG, 825000, 3300000, 0, &s_PmuLDOOps),
    REG_DATA(LDO11,   LDO11_VOLTAGE_REG, LDOCONTROL1_REG, 825000, 3300000, 0, &s_PmuLDOOps),
    REG_DATA(SD0,     SD0_VOLTAGE_REG,   SD_CONTROL_REG,  600000, 1500000, 0, &s_PmuSDOps),
    REG_DATA(SD1,     SD1_VOLTAGE_REG,   SD_CONTROL_REG,  600000, 1500000, 0, &s_PmuSDOps),
    REG_DATA(SD2,     SD2_VOLTAGE_REG,   SD_CONTROL_REG,  600000, 3350000, 0, &s_PmuSDOps),
    REG_DATA(SD3,     SD3_VOLTAGE_REG,   SD_CONTROL_REG,  600000, 3350000, 0, &s_PmuSDOps),
    REG_DATA(SD4,     SD4_VOLTAGE_REG,   SD_CONTROL_REG,  600000, 3350000, 0, &s_PmuSDOps),
    REG_DATA(SD5,     SD5_VOLTAGE_REG,   SD_CONTROL_REG,  600000, 3350000, 0, &s_PmuSDOps),
    REG_DATA(SD6,     SD6_VOLTAGE_REG,   SD_CONTROL_REG,  600000, 1500000, 0, &s_PmuSDOps),
};

static NvBool As3722IsRailEnabled(As3722PmuInfoHandle hAs3722PmuInfo, 
                NvU32 RailId)
{
    /* All rail are ON by default */
    return NV_TRUE;
}

static NvBool As3722RailEnable(As3722PmuInfoHandle hAs3722PmuInfo, /* need to look */
                NvU32 RailId, NvU32 *DelayUS)
{
    /* All rail are ON by default */
    return NV_TRUE;
}

static NvBool As3722RailDisable(As3722PmuInfoHandle hAs3722PmuInfo,
                NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    As3722PmuProps *pPmuProps;

    NV_ASSERT(hAs3722PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hAs3722PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hAs3722PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cClrBits(hAs3722PmuInfo->hOdmPmuI2c,
                             hAs3722PmuInfo->DeviceAddr,
                             AS3722_I2C_SPEED, pPmuProps->reg_enable,
                             pPmuProps->enable_bit);
    if (!ret)
    {
        NvOdmOsPrintf("%s() Error in updating the control register\n", __func__);
        return ret;
    }

    if (DelayUS)
        *DelayUS = pPmuProps->delay;

    return ret;
}

static NvBool As3722SDGetVoltage(As3722PmuInfoHandle hAs3722PmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    As3722PmuProps *pPmuProps;
    NvU8 vsel;
    NvBool ret;

    NV_ASSERT(hAs3722PmuInfo->RailInfo[RailId].pPmuProps);
    NV_ASSERT(uV);

    if (!hAs3722PmuInfo->RailInfo[RailId].pPmuProps || !uV)
        return NV_FALSE;

    pPmuProps = hAs3722PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hAs3722PmuInfo->hOdmPmuI2c,
                           hAs3722PmuInfo->DeviceAddr,
                           AS3722_I2C_SPEED,
                           pPmuProps->reg_vsel, &vsel);
    if (!ret)
    {
        NvOdmOsPrintf("%s() Error in reading the volt register\n", __func__);
        return ret;
    }

    vsel &= ~AS3722_SD_VOLTAGE_RANGE;

    if (pPmuProps->reg_id == AS3722PmuSupply_SD0 ||
        pPmuProps->reg_id == AS3722PmuSupply_SD1 ||
        pPmuProps->reg_id == AS3722PmuSupply_SD6)
    {
            *uV = 600000 + vsel * 10000;
    }
    else
    {
         if (vsel <= 0x40)
             *uV = 600000 + vsel * 12500;
         else if (vsel <= 0x70)
              *uV = 1400000 + (vsel - 0x40) * 25000;
         else
              *uV = 2600000 + (vsel - 0x70) * 50000;
    }

    return ret;
}

static NvBool As3722SDSetVoltage(As3722PmuInfoHandle hAs3722PmuInfo,
                NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel = 0;
    NvBool ret = NV_FALSE;
    As3722PmuProps *pPmuProps;

    NV_ASSERT(hAs3722PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hAs3722PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hAs3722PmuInfo->RailInfo[RailId].pPmuProps;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    if (pPmuProps->reg_id == AS3722PmuSupply_SD0 ||
        pPmuProps->reg_id == AS3722PmuSupply_SD1 ||
        pPmuProps->reg_id == AS3722PmuSupply_SD6)
    {
            vsel = (new_uV - 600000) / 10000;
    }
    else
    {
         if (new_uV <= 1400000)
             vsel = (new_uV - 600000) / 12500;
         else if (new_uV <= 2600000)
             vsel = (new_uV - 1400000) / 25000 + 0x40;
         else
             vsel = (new_uV - 2600000) / 50000 + 0x70;
    }

    ret = NvOdmPmuI2cUpdate8(hAs3722PmuInfo->hOdmPmuI2c,
                             hAs3722PmuInfo->DeviceAddr,
                             AS3722_I2C_SPEED, pPmuProps->reg_vsel,
                             vsel, AS3722_SD_VOLTAGE_RANGE - 1);
    if (!ret)
    {
        NvOdmOsPrintf("%s() Error in writing the volt register\n", __func__);
        return ret;
    }

    if (DelayUS)
        *DelayUS = pPmuProps->delay;

    return ret;
}

static NvBool As3722LDOGetVoltage(As3722PmuInfoHandle hAs3722PmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    As3722PmuProps *pPmuProps;
    NvU8 vsel;
    NvBool ret;

    NV_ASSERT(hAs3722PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hAs3722PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hAs3722PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hAs3722PmuInfo->hOdmPmuI2c,
                           hAs3722PmuInfo->DeviceAddr,
                           AS3722_I2C_SPEED,pPmuProps->reg_vsel,
                           &vsel);
    if (!ret)
    {
        NvOdmOsPrintf("%s() Error in reading the volt register\n", __func__);
        return ret;
    }

    if (pPmuProps->reg_id == AS3722PmuSupply_LDO0)
        *uV = 800000 + vsel * 25000;
    else if (pPmuProps->reg_id == AS3722PmuSupply_LDO3)
             *uV = 600000 + vsel * 10000;
    else
    {
         if (vsel <= 0x24 )
             *uV = 800000 + vsel * 25000;
         else
             *uV = 1725000 + (vsel - 0x40) * 25000;
    }

    return ret;
}

static NvBool As3722LDOSetVoltage(As3722PmuInfoHandle hAs3722PmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel;
    NvBool ret;
    As3722PmuProps *pPmuProps;

    NV_ASSERT(hAs3722PmuInfo->RailInfo[RailId].pPmuProps);

    if (!hAs3722PmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hAs3722PmuInfo->RailInfo[RailId].pPmuProps;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    if (pPmuProps->reg_id == AS3722PmuSupply_LDO0)
        vsel = (new_uV - 800000) / 25000;
    else if (pPmuProps->reg_id == AS3722PmuSupply_LDO3)
        vsel = (new_uV - 600000) / 10000;
    else
    {
        if (new_uV <= 1700000)
            vsel = (new_uV - 800000) / 25000;
        else if (new_uV > 1400000 && new_uV < 1725000)
        {
            NvOdmOsPrintf("%s() Error ldo_vsel value not allowed between 25h to 3fh \n", __func__);
            return NV_FALSE;
        }
        else
             vsel = (new_uV - 1725000) / 25000 + 0x40;
    }

    ret = NvOdmPmuI2cUpdate8(hAs3722PmuInfo->hOdmPmuI2c,
                             hAs3722PmuInfo->DeviceAddr,
                             AS3722_I2C_SPEED, pPmuProps->reg_vsel,
                             vsel, AS3722_SD_VOLTAGE_RANGE - 1);
    if (!ret)
    {
        NvOdmOsPrintf("%s() Error in writing the volt register\n", __func__);
        return ret;
    }

    if (DelayUS)
        *DelayUS = pPmuProps->delay;

    return ret;
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(AS3722_PMUGUID);
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

NvOdmPmuDriverInfoHandle As3722PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps)
{
    As3722PmuInfoHandle hAs3722PmuInfo;
    NvU32 i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvBool Ret;
    NvU32 VddId;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    if (!hDevice || !pOdmRailProps)
        return NULL;

    /* Information for all rail is required */
    if (NrOfOdmRailProps != AS3722PmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }

    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
    if (!Ret)
        return NULL;

    hAs3722PmuInfo = NvOdmOsAlloc(sizeof(As3722PmuInfo));
    if (!hAs3722PmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hAs3722PmuInfo, 0, sizeof(As3722PmuInfo));

    hAs3722PmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hAs3722PmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        goto I2cOpenFailed;
    }
    hAs3722PmuInfo->DeviceAddr = I2cAddress;

    hAs3722PmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hAs3722PmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hAs3722PmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hAs3722PmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 1; i < AS3722PmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;

        if (VddId == AS3722PmuSupply_Invalid)
            continue;

        if (pOdmRailProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been provided, "
                    "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        if (VddId != s_As3722PmuProps[VddId].reg_id)
        {
            NvOdmOsPrintf("%s(): The entries are not in proper sequence, "
                    "Ignoring this entry\n", __func__);
            continue;
        }

        if (hAs3722PmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                    "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        hAs3722PmuInfo->RailInfo[VddId].pPmuProps = &s_As3722PmuProps[VddId];

        NvOdmOsMemcpy(&hAs3722PmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));

        hAs3722PmuInfo->RailInfo[VddId].Caps.OdmProtected =
            hAs3722PmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        /* min voltage */
        if (hAs3722PmuInfo->RailInfo[VddId].OdmProps.Min_mV)
            hAs3722PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                NV_MAX(hAs3722PmuInfo->RailInfo[VddId].OdmProps.Min_mV,
                        hAs3722PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000);
        else
            hAs3722PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                hAs3722PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000;

        /* max voltage */
        if (hAs3722PmuInfo->RailInfo[VddId].OdmProps.Max_mV)
            hAs3722PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                NV_MIN(hAs3722PmuInfo->RailInfo[VddId].OdmProps.Max_mV,
                        hAs3722PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000);
        else
            hAs3722PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                hAs3722PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000;

        if (hAs3722PmuInfo->RailInfo[VddId].OdmProps.Setup_mV)
            hAs3722PmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                hAs3722PmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                          "Ignoring this entry\n", __func__,
                           hAs3722PmuInfo->RailInfo[VddId].OdmProps.Setup_mV,
                           VddId);
            continue;
        }

        hAs3722PmuInfo->RailInfo[VddId].UserCount  = 0;
        hAs3722PmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hAs3722PmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;
    }

    return (NvOdmPmuDriverInfoHandle)hAs3722PmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hAs3722PmuInfo->hOdmPmuService);
PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hAs3722PmuInfo->hOdmPmuI2c);
I2cOpenFailed:
    NvOdmOsFree(hAs3722PmuInfo);
    return NULL;
}

void As3722PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    As3722PmuInfoHandle hAs3722PmuInfo;

    NV_ASSERT(hPmuDriver);

    if (!hPmuDriver)
        return;

    hAs3722PmuInfo = (As3722PmuInfoHandle)hPmuDriver;

    NvOdmServicesPmuClose(hAs3722PmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hAs3722PmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hAs3722PmuInfo);
}

void As3722PmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    As3722PmuInfoHandle hAs3722PmuInfo;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < AS3722PmuSupply_Num);
    NV_ASSERT(pCapabilities);

    if (!hPmuDriver || !(VddId < AS3722PmuSupply_Num) || !pCapabilities)
        return;

    hAs3722PmuInfo = (As3722PmuInfoHandle)hPmuDriver;

    if (!hAs3722PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d\n", __func__, VddId);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }

    NvOdmOsMemcpy(pCapabilities, &hAs3722PmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool As3722PmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    As3722PmuInfoHandle hAs3722PmuInfo;
    As3722PmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;
    hAs3722PmuInfo =  (As3722PmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < AS3722PmuSupply_Num);
    NV_ASSERT(pMilliVolts);

    if (!hPmuDriver || !(VddId < AS3722PmuSupply_Num) || !pMilliVolts)
        return NV_FALSE;

    hAs3722PmuInfo = (As3722PmuInfoHandle)hPmuDriver;

    if (!hAs3722PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provided the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pPmuProps = hAs3722PmuInfo->RailInfo[VddId].pPmuProps;

    *pMilliVolts = 0;

    if (pPmuProps->pOps->IsRailEnabled(hAs3722PmuInfo, VddId))
    {
        Status = pPmuProps->pOps->GetRailVoltage(hAs3722PmuInfo, VddId, &uV);
        if (Status)
        {
            *pMilliVolts = uV/1000;
        }
    }

    return Status;
}

NvBool As3722PmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    As3722PmuInfoHandle hAs3722PmuInfo;
    As3722PmuProps *pPmuProps;
    As3722RailInfo *pRailInfo;
    NvBool Status = NV_TRUE;
    hAs3722PmuInfo =  (As3722PmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < AS3722PmuSupply_Num);
    NV_ASSERT(pSettleMicroSeconds);

    if (!hPmuDriver || !(VddId < AS3722PmuSupply_Num) || !pSettleMicroSeconds)
        return NV_FALSE;

    if (!hAs3722PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pRailInfo = &hAs3722PmuInfo->RailInfo[VddId];
    pPmuProps = hAs3722PmuInfo->RailInfo[VddId].pPmuProps;
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

    NvOdmOsMutexLock(hAs3722PmuInfo->hPmuMutex);
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
            NvOdmServicesPmuSetSocRailPowerState(hAs3722PmuInfo->hOdmPmuService,
                        hAs3722PmuInfo->RailInfo[VddId].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hAs3722PmuInfo, VddId,
                                            pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hAs3722PmuInfo->hOdmPmuService,
                        hAs3722PmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hAs3722PmuInfo, VddId,
                                MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hAs3722PmuInfo, VddId,
                                                        pSettleMicroSeconds);
        if (!Status)
        {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                        __func__, VddId);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hAs3722PmuInfo->hPmuMutex);
    return Status;
}

void As3722PmuPowerOff(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    //As3722PmuInfoHandle hAs3722PmuInfo;

    NV_ASSERT(hPmuDriver);
    if (!hPmuDriver)
        return;

    //hAs3722PmuInfo = (As3722PmuInfoHandle)hPmuDriver;
/*    NvOdmPmuI2cWrite8(hAs3722PmuInfo->hOdmPmuI2c, hAs3722PmuInfo->DeviceAddr,
                       AS3722_I2C_SPEED, AS3722_DEVCNTRL, AS3722_POWER_OFF); */
    //shouldn't reach here
    NV_ASSERT(0);
}
