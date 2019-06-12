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
  * @file: <b>NVIDIA Tegra ODM Kit: TPS51632 pmu driver</b>
  *
  * @b Description: Implements the Tps51632 pmu drivers.
  *
  */
#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "tps51632/nvodm_pmu_tps51632_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"

#define TPS51632_PMUGUID NV_ODM_GUID('t','p','s','5','1','6','3','2')

#define TPS51632_I2C_SPEED 400

typedef struct Tps51632PmuInfoRec *Tps51632PmuInfoHandle;

typedef struct Tps51632PmuOpsRec {
    NvBool (*IsRailEnabled)(Tps51632PmuInfoHandle hTps51632PmuInfo, NvU32 VddId);

    NvBool (*EnableRail)(Tps51632PmuInfoHandle hTps51632PmuInfo, NvU32 VddId, NvU32 *DelayUS);

    NvBool (*DisableRail)(Tps51632PmuInfoHandle hTps51632PmuInfo, NvU32 VddId, NvU32 *DelayUS);

    NvBool (*SetRailVoltage)(Tps51632PmuInfoHandle hTps51632PmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);

    NvBool (*GetRailVoltage)(Tps51632PmuInfoHandle hTps51632PmuInfo,
            NvU32 VddId, NvU32 *uV);
} Tps51632PmuOps;

typedef struct Tps51632PmuPropsRec {
    NvU32        Id;

    /* Regulator register address.*/
    NvU8         vout_reg;
    NvU8         vout_mask;

    /* Pmu voltage props */
    NvU32        min_uV;
    NvU32        max_uV;
    NvU32        step_uV;
    NvU32        nsteps;

    /* Power rail specific turn-on delay */
    NvU32        delay;
    Tps51632PmuOps *pOps;
} Tps51632PmuProps;

typedef struct Tps51632RailInfoRec {
    Tps51632PmuSupply   VddId;
    NvU32               SystemRailId;
    NvU32               UserCount;
    NvBool              IsValid;
    Tps51632PmuProps   *pPmuProps;
    NvOdmPmuRailProps   OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Tps51632RailInfo;

typedef struct Tps51632PmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    Tps51632RailInfo RailInfo[Tps51632PmuSupply_Num];
} Tps51632PmuInfo;

static NvBool Tps51632IsRailEnabled(Tps51632PmuInfoHandle hTps51632PmuInfo,
            NvU32 VddId);

static NvBool Tps51632RailEnable(Tps51632PmuInfoHandle hTps51632PmuInfo,
            NvU32 VddId, NvU32 *DelayUS);

static NvBool Tps51632RailDisable(Tps51632PmuInfoHandle hTps51632PmuInfo,
            NvU32 VddId, NvU32 *DelayUS);

static NvBool Tps51632RailSetVoltage(Tps51632PmuInfoHandle hTps51632PmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Tps51632RailGetVoltage(Tps51632PmuInfoHandle hTps51632PmuInfo,
            NvU32 VddId, NvU32 *uV);

static Tps51632PmuOps s_PmuOps = {
    Tps51632IsRailEnabled,
    Tps51632RailEnable,
    Tps51632RailDisable,
    Tps51632RailSetVoltage,
    Tps51632RailGetVoltage,
};

#define REG_DATA(_id, _vout_reg, _vout_mask, _min_uV, _max_mV, _step_uV, \
            _nsteps, _pOps, _delay)  \
    {       \
            Tps51632Mode_##_id, _vout_reg, _vout_mask, \
            _min_uV*1000, _max_mV*1000, _step_uV, _nsteps, _delay, _pOps, \
    }

/* Should be in same sequence as the enums are */
static Tps51632PmuProps s_Tps51632PmuProps[] = {
    REG_DATA(Invalid,   0,    0,      0,       0,     0,    0,  &s_PmuOps, 0),
    REG_DATA(      0, 0x0, 0x7F,    500,    1520, 10000, 0x7F,  &s_PmuOps, 0),
};

static NvBool Tps51632IsRailEnabled(Tps51632PmuInfoHandle hTps51632PmuInfo, NvU32 RailId)
{
    return NV_TRUE;
}

static NvBool Tps51632RailEnable(Tps51632PmuInfoHandle hTps51632PmuInfo,
            NvU32 RailId, NvU32 *DelayUS)
{
    return NV_TRUE;
}

static NvBool Tps51632RailDisable(Tps51632PmuInfoHandle hTps51632PmuInfo,
            NvU32 RailId, NvU32 *DelayUS)
{
    return NV_TRUE;
}

static NvBool Tps51632RailSetVoltage(Tps51632PmuInfoHandle hTps51632PmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU32 vsel;
    NvBool ret;
    Tps51632PmuProps *pPmuProps;
    NvU32 min_vsel = 0x19;

    NV_ASSERT(hTps51632PmuInfo);

    if (!hTps51632PmuInfo)
        return NV_FALSE;

    pPmuProps = hTps51632PmuInfo->RailInfo[RailId].pPmuProps;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    vsel = (new_uV - pPmuProps->min_uV + pPmuProps->step_uV - 1)/ pPmuProps->step_uV + min_vsel;
    if (vsel > pPmuProps->nsteps)
        return NV_FALSE;

    ret = NvOdmPmuI2cUpdate8(hTps51632PmuInfo->hOdmPmuI2c, hTps51632PmuInfo->DeviceAddr,
            TPS51632_I2C_SPEED, pPmuProps->vout_reg, vsel, pPmuProps->vout_mask);

    if (!ret) {
        NvOdmOsPrintf("%s():  Error in writing the Voltage register\n", __func__);
    }

    if (DelayUS && ret)
        *DelayUS = pPmuProps->delay;

    return ret;
}

static NvBool Tps51632RailGetVoltage(Tps51632PmuInfoHandle hTps51632PmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    NvU8 vsel;
    NvBool ret;
    Tps51632PmuProps *pPmuProps;

    NV_ASSERT(hTps51632PmuInfo);
    NV_ASSERT(uV);

    if (!hTps51632PmuInfo || !uV)
        return NV_FALSE;

    pPmuProps = hTps51632PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps51632PmuInfo->hOdmPmuI2c, hTps51632PmuInfo->DeviceAddr,
            TPS51632_I2C_SPEED, pPmuProps->vout_reg, &vsel);

    if (!ret) {
        NvOdmOsPrintf("%s(): Error in reading the Voltage register\n", __func__);
        return ret;
    }

    vsel &= vsel & pPmuProps->vout_mask;
    *uV = pPmuProps->min_uV + vsel * pPmuProps->step_uV;

    return NV_TRUE;
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(TPS51632_PMUGUID);
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
    NvOdmOsPrintf("TPS51632: %s: The system did not doscover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

NvOdmPmuDriverInfoHandle Tps51632PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps)
{
    Tps51632PmuInfoHandle hTps51632PmuInfo;
    NvU32 i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvBool Ret;
    NvU32 VddId;
    Tps51632BoardData *pBoardData;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    if (!hDevice || !pOdmRailProps)
        return NULL;

    /* Information for all rail is required */
    if (NrOfOdmRailProps != Tps51632PmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }

    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
    if (!Ret)
        return NULL;

    hTps51632PmuInfo = NvOdmOsAlloc(sizeof(Tps51632PmuInfo));
    if (!hTps51632PmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hTps51632PmuInfo, 0, sizeof(Tps51632PmuInfo));

    hTps51632PmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hTps51632PmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        goto I2cOpenFailed;
    }
    hTps51632PmuInfo->DeviceAddr = I2cAddress;

    hTps51632PmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hTps51632PmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hTps51632PmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hTps51632PmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 1; i < Tps51632PmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;
        if (VddId == Tps51632PmuSupply_Invalid)
                continue;
        pBoardData = pOdmRailProps[i].pBoardData;

        if (!pBoardData )
        {
            NvOdmOsPrintf("%s(): The board information is not correct, "
                        "Ignoring this entry\n", __func__);
            continue;
        }

        if (pBoardData->enable_pwm || (pBoardData->VoltageMode == Tps51632Mode_1)) {
            NvOdmOsPrintf("%s():  "
                    "Driver doesn't support DVFS mode\n", __func__);
            goto MutexCreateFailed;
        }

        if (hTps51632PmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        hTps51632PmuInfo->RailInfo[VddId].pPmuProps = &s_Tps51632PmuProps[VddId];

        NvOdmOsMemcpy(&hTps51632PmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));

        hTps51632PmuInfo->RailInfo[VddId].Caps.OdmProtected =
                        hTps51632PmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        if (hTps51632PmuInfo->RailInfo[VddId].OdmProps.Min_mV)
             hTps51632PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        NV_MAX(hTps51632PmuInfo->RailInfo[VddId].OdmProps.Min_mV,
                           hTps51632PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000);
        else
             hTps51632PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        hTps51632PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000;

        if (hTps51632PmuInfo->RailInfo[VddId].OdmProps.Max_mV)
             hTps51632PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                        NV_MIN(hTps51632PmuInfo->RailInfo[VddId].OdmProps.Max_mV,
                           hTps51632PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000);
        else
             hTps51632PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                           hTps51632PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000;

        hTps51632PmuInfo->RailInfo[VddId].Caps.StepMilliVolts =
                           hTps51632PmuInfo->RailInfo[VddId].pPmuProps->step_uV/1000;

        if (hTps51632PmuInfo->RailInfo[VddId].OdmProps.Setup_mV)
            hTps51632PmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                hTps51632PmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                    "Ignoring this entry\n", __func__,
                    hTps51632PmuInfo->RailInfo[VddId].OdmProps.Setup_mV,
                    VddId);
            continue;
        }

        hTps51632PmuInfo->RailInfo[VddId].UserCount  = 0;
        hTps51632PmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hTps51632PmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;
    }
    return (NvOdmPmuDriverInfoHandle)hTps51632PmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hTps51632PmuInfo->hOdmPmuService);

PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hTps51632PmuInfo->hOdmPmuI2c);

I2cOpenFailed:
    NvOdmOsFree(hTps51632PmuInfo);
    return NULL;
}

void Tps51632PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps51632PmuInfoHandle hTps51632PmuInfo;

    NV_ASSERT(hPmuDriver);

    hTps51632PmuInfo = (Tps51632PmuInfoHandle)hPmuDriver;

    NvOdmServicesPmuClose(hTps51632PmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hTps51632PmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hTps51632PmuInfo);
}

void Tps51632PmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Tps51632PmuInfoHandle hTps51632PmuInfo;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Tps51632PmuSupply_Num);
    NV_ASSERT(pCapabilities);

    if (!hPmuDriver || !(VddId < Tps51632PmuSupply_Num) || !pCapabilities)
        return;

    hTps51632PmuInfo =  (Tps51632PmuInfoHandle)hPmuDriver;

    if (!hTps51632PmuInfo->RailInfo[VddId].IsValid)
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);

    NvOdmOsMemcpy(pCapabilities, &hTps51632PmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Tps51632PmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    Tps51632PmuInfoHandle hTps51632PmuInfo;
    Tps51632PmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Tps51632PmuSupply_Num);
    NV_ASSERT(pMilliVolts);

    if (!hPmuDriver || !(VddId < Tps51632PmuSupply_Num) || !pMilliVolts)
        return NV_FALSE;

    hTps51632PmuInfo =  (Tps51632PmuInfoHandle)hPmuDriver;
    if (!hTps51632PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pPmuProps = hTps51632PmuInfo->RailInfo[VddId].pPmuProps;
    *pMilliVolts = 0;
    if (pPmuProps->pOps->IsRailEnabled(hTps51632PmuInfo, VddId)) {
        Status = pPmuProps->pOps->GetRailVoltage(hTps51632PmuInfo, VddId, &uV);
        if (Status) {
            *pMilliVolts = uV /1000;
        }
    }
    return Status;
}

NvBool Tps51632PmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    Tps51632PmuInfoHandle hTps51632PmuInfo;
    Tps51632PmuProps *pPmuProps;
    Tps51632RailInfo *pRailInfo;
    NvBool Status = NV_TRUE;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Tps51632PmuSupply_Num);
    NV_ASSERT(pSettleMicroSeconds);

    if (!hPmuDriver || !(VddId < Tps51632PmuSupply_Num) || !pSettleMicroSeconds)
        return NV_FALSE;

    hTps51632PmuInfo =  (Tps51632PmuInfoHandle)hPmuDriver;

    if (!hTps51632PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pRailInfo = &hTps51632PmuInfo->RailInfo[VddId];
    pPmuProps = hTps51632PmuInfo->RailInfo[VddId].pPmuProps;
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

    NvOdmOsMutexLock(hTps51632PmuInfo->hPmuMutex);
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
            NvOdmServicesPmuSetSocRailPowerState(hTps51632PmuInfo->hOdmPmuService,
                    hTps51632PmuInfo->RailInfo[VddId].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hTps51632PmuInfo, VddId,
                        pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hTps51632PmuInfo->hOdmPmuService,
                hTps51632PmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hTps51632PmuInfo, VddId,
                    MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hTps51632PmuInfo, VddId,
                    pSettleMicroSeconds);
        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                    __func__, VddId);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hTps51632PmuInfo->hPmuMutex);
    return Status;
}
