/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: TPS6238X pmu driver</b>
  *
  * @b Description: Implements the Tps6238x pmu drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "tps6238x/nvodm_pmu_tps6238x_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"

#define TPS6238X_PMUGUID NV_ODM_GUID('t','p','s','6','2','3','8','x')

#define TPS6238X_I2C_SPEED 100

typedef struct Tps6238xPmuInfoRec *Tps6238xPmuInfoHandle;

typedef struct Tps6238xPmuOpsRec {
    NvBool (*IsRailEnabled)(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 VddId);
    NvBool (*EnableRail)(Tps6238xPmuInfoHandle hTps6238xPmuInfo, NvU32 VddId, NvU32 *DelayUS);
    NvBool (*DisableRail)(Tps6238xPmuInfoHandle hTps6238xPmuInfo, NvU32 VddId, NvU32 *DelayUS);
    NvBool (*SetRailVoltage)(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);
    NvBool (*GetRailVoltage)(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 VddId, NvU32 *uV);
} Tps6238xPmuOps;

typedef struct Tps6238xPmuPropsRec {
    NvU32       Id;
    /* Regulator register address.*/
    NvU8        vout_reg;
    NvU8        vout_mask;

    /* Pmu voltage props */
    NvU32       min_uV;
    NvU32       max_uV;
    NvU32       step_uV;
    NvU32       nsteps;

    /* Power rail specific turn-on delay */
    NvU32       delay;
    Tps6238xPmuOps *pOps;
} Tps6238xPmuProps;

typedef struct Tps6238xRailInfoRec {
    Tps6238xPmuSupply   VddId;
    NvU32               SystemRailId;
    NvU32               UserCount;
    NvBool              IsValid;
    Tps6238xPmuProps   *pPmuProps;
    NvOdmPmuRailProps   OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Tps6238xRailInfo;

typedef struct Tps6238xPmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    Tps6238xRailInfo RailInfo[Tps6238xPmuSupply_Num];
} Tps6238xPmuInfo;

static NvBool Tps6238xIsRailEnabled(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 VddId);

static NvBool Tps6238xRailEnable(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 VddId, NvU32 *DelayUS);

static NvBool Tps6238xRailDisable(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 VddId, NvU32 *DelayUS);

static NvBool Tps6238xRailSetVoltage(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Tps6238xRailGetVoltage(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 VddId, NvU32 *uV);

static Tps6238xPmuOps s_PmuOps = {
    Tps6238xIsRailEnabled,
    Tps6238xRailEnable,
    Tps6238xRailDisable,
    Tps6238xRailSetVoltage,
    Tps6238xRailGetVoltage,
};

#define REG_DATA(_id, _vout_reg, _vout_mask, _min_uV, _max_mV, _step_uV, \
            _nsteps, _pOps, _delay)  \
    {       \
            Tps6238xMode_##_id, _vout_reg, _vout_mask, \
            _min_uV*1000, _max_mV*1000, _step_uV, _nsteps, _delay, _pOps, \
    }

/* Should be in same sequence as the enums are */
static Tps6238xPmuProps s_Tps6238xPmuProps[] = {
    REG_DATA(Invalid, 0, 0, 0,  0,    0,     0,    &s_PmuOps, 500),
    REG_DATA(0, 0x0, 0x7F, 500, 1770, 10000, 0x80, &s_PmuOps, 500),
    REG_DATA(1, 0x1, 0x7F, 500, 1770, 10000, 0x80, &s_PmuOps, 500),
};

static NvBool Tps6238xIsRailEnabled(Tps6238xPmuInfoHandle hTps6238xPmuInfo, NvU32 RailId)
{
    return NV_TRUE;
}

static NvBool Tps6238xRailEnable(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 RailId, NvU32 *DelayUS)
{
    return NV_TRUE;
}

static NvBool Tps6238xRailDisable(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 RailId, NvU32 *DelayUS)
{
    return NV_TRUE;
}

static NvBool Tps6238xRailSetVoltage(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU32 vsel;
    NvBool ret;
    Tps6238xPmuProps *pPmuProps = hTps6238xPmuInfo->RailInfo[RailId].pPmuProps;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    vsel = (new_uV - pPmuProps->min_uV + pPmuProps->step_uV - 1)/ pPmuProps->step_uV;
    if (vsel > pPmuProps->nsteps)
        return NV_FALSE;

    ret = NvOdmPmuI2cUpdate8(hTps6238xPmuInfo->hOdmPmuI2c, hTps6238xPmuInfo->DeviceAddr,
            TPS6238X_I2C_SPEED, pPmuProps->vout_reg, vsel, pPmuProps->vout_mask);
    if (!ret) {
        NvOdmOsPrintf("%s():  Error in writing the Voltage register\n", __func__);
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Tps6238xRailGetVoltage(Tps6238xPmuInfoHandle hTps6238xPmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    NvU8 vsel;
    NvBool ret;
    Tps6238xPmuProps *pPmuProps = hTps6238xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps6238xPmuInfo->hOdmPmuI2c, hTps6238xPmuInfo->DeviceAddr,
            TPS6238X_I2C_SPEED, pPmuProps->vout_reg, &vsel);
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
                           NvOdmPeripheralGetGuid(TPS6238X_PMUGUID);
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
    NvOdmOsPrintf("TPS6238X: %s: The system did not doscover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

NvOdmPmuDriverInfoHandle Tps6238xPmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps)
{
    Tps6238xPmuInfoHandle hTps6238xPmuInfo;
    NvU32 i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvBool Ret;
    NvU32 VddId;
    Tps6238xBoardData *pBoardData;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    /* Information for all rail is required */
    if (NrOfOdmRailProps != Tps6238xPmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }
    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
    if (!Ret)
        return NULL;

    hTps6238xPmuInfo = NvOdmOsAlloc(sizeof(Tps6238xPmuInfo));
    if (!hTps6238xPmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hTps6238xPmuInfo, 0, sizeof(Tps6238xPmuInfo));

    hTps6238xPmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hTps6238xPmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        goto I2cOpenFailed;
    }
    hTps6238xPmuInfo->DeviceAddr = I2cAddress;

    hTps6238xPmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hTps6238xPmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hTps6238xPmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hTps6238xPmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 1; i < Tps6238xPmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;
        if (VddId == Tps6238xPmuSupply_Invalid)
                continue;
        pBoardData = pOdmRailProps[i].pBoardData;

        if (!pBoardData || (pBoardData->VoltageMode > Tps6238xMode_1))
        {
            NvOdmOsPrintf("%s(): The board information is not correct, "
                        "Ignoring this entry\n", __func__);
            continue;
        }
        if (hTps6238xPmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        hTps6238xPmuInfo->RailInfo[VddId].pPmuProps = &s_Tps6238xPmuProps[pBoardData->VoltageMode];

        NvOdmOsMemcpy(&hTps6238xPmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));

        hTps6238xPmuInfo->RailInfo[VddId].Caps.OdmProtected =
                        hTps6238xPmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        if (hTps6238xPmuInfo->RailInfo[VddId].OdmProps.Min_mV)
             hTps6238xPmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        NV_MAX(hTps6238xPmuInfo->RailInfo[VddId].OdmProps.Min_mV,
                           hTps6238xPmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000);
        else
             hTps6238xPmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        hTps6238xPmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000;

        if (hTps6238xPmuInfo->RailInfo[VddId].OdmProps.Max_mV)
             hTps6238xPmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                        NV_MIN(hTps6238xPmuInfo->RailInfo[VddId].OdmProps.Max_mV,
                           hTps6238xPmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000);
        else
             hTps6238xPmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                           hTps6238xPmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000;

        hTps6238xPmuInfo->RailInfo[VddId].Caps.StepMilliVolts =
                           hTps6238xPmuInfo->RailInfo[VddId].pPmuProps->step_uV/1000;

        if (hTps6238xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV)
             hTps6238xPmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                        hTps6238xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                        "Ignoring this entry\n", __func__,
                         hTps6238xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV,
                         VddId);
            continue;
        }
        hTps6238xPmuInfo->RailInfo[VddId].UserCount  = 0;
        hTps6238xPmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hTps6238xPmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;
    }
    return (NvOdmPmuDriverInfoHandle)hTps6238xPmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hTps6238xPmuInfo->hOdmPmuService);

PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hTps6238xPmuInfo->hOdmPmuI2c);

I2cOpenFailed:
    NvOdmOsFree(hTps6238xPmuInfo);
    return NULL;
}

void Tps6238xPmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps6238xPmuInfoHandle hTps6238xPmuInfo = (Tps6238xPmuInfoHandle)hPmuDriver;

    NvOdmServicesPmuClose(hTps6238xPmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hTps6238xPmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hTps6238xPmuInfo);
}

void Tps6238xPmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Tps6238xPmuInfoHandle hTps6238xPmuInfo =  (Tps6238xPmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Tps6238xPmuSupply_Num);
    if (!hTps6238xPmuInfo->RailInfo[VddId].IsValid)
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);

    NvOdmOsMemcpy(pCapabilities, &hTps6238xPmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Tps6238xPmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    Tps6238xPmuInfoHandle hTps6238xPmuInfo =  (Tps6238xPmuInfoHandle)hPmuDriver;
    Tps6238xPmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Tps6238xPmuSupply_Num);
    if (!hTps6238xPmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pPmuProps = hTps6238xPmuInfo->RailInfo[VddId].pPmuProps;
    *pMilliVolts = 0;
    if (pPmuProps->pOps->IsRailEnabled(hTps6238xPmuInfo, VddId)) {
        Status = pPmuProps->pOps->GetRailVoltage(hTps6238xPmuInfo, VddId, &uV);
        if (Status) {
            *pMilliVolts = uV /1000;
        }
    }
    return Status;
}

NvBool Tps6238xPmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    Tps6238xPmuInfoHandle hTps6238xPmuInfo =  (Tps6238xPmuInfoHandle)hPmuDriver;
    Tps6238xPmuProps *pPmuProps;
    Tps6238xRailInfo *pRailInfo;
    NvBool Status = NV_TRUE;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Tps6238xPmuSupply_Num);
    if (!hTps6238xPmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pRailInfo = &hTps6238xPmuInfo->RailInfo[VddId];
    pPmuProps = hTps6238xPmuInfo->RailInfo[VddId].pPmuProps;
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

    NvOdmOsMutexLock(hTps6238xPmuInfo->hPmuMutex);
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
            NvOdmServicesPmuSetSocRailPowerState(hTps6238xPmuInfo->hOdmPmuService,
                    hTps6238xPmuInfo->RailInfo[VddId].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hTps6238xPmuInfo, VddId,
                        pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hTps6238xPmuInfo->hOdmPmuService,
                hTps6238xPmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hTps6238xPmuInfo, VddId,
                    MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hTps6238xPmuInfo, VddId,
                    pSettleMicroSeconds);
        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                    __func__, VddId);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hTps6238xPmuInfo->hPmuMutex);
    return Status;
}
