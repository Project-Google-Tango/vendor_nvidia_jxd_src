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
  * @file: <b>NVIDIA Tegra ODM Kit: LP8755 pmu driver</b>
  *
  * @b Description: Implements the Lp8755 pmu drivers.
  *
  */
#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "lp8755/nvodm_pmu_lp8755_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"

#define LP8755_PMUGUID NV_ODM_GUID('l','p','8','7','5','5',' ',' ')
#define LP8755_I2C_SPEED 100

typedef struct Lp8755PmuInfoRec *Lp8755PmuInfoHandle;

typedef struct Lp8755PmuOpsRec {
    NvBool (*IsRailEnabled)(Lp8755PmuInfoHandle hLp8755PmuInfo, NvU32 VddId);

    NvBool (*EnableRail)(Lp8755PmuInfoHandle hLp8755PmuInfo, NvU32 VddId, NvU32 *DelayUS);

    NvBool (*DisableRail)(Lp8755PmuInfoHandle hLp8755PmuInfo, NvU32 VddId, NvU32 *DelayUS);

    NvBool (*SetRailVoltage)(Lp8755PmuInfoHandle hLp8755PmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);

    NvBool (*GetRailVoltage)(Lp8755PmuInfoHandle hLp8755PmuInfo,
            NvU32 VddId, NvU32 *uV);
} Lp8755PmuOps;

typedef struct Lp8755PmuPropsRec {
    NvU32        Id;

    /* Regulator register address.*/
    NvU8         vout_reg;
    NvU8         vout_mask;

    /* Pmu voltage props */
    NvU32        min_uV;
    NvU32        max_uV;
    NvU32        step_uV;

    /* Power rail specific turn-on delay */
    NvU32        delay;
    Lp8755PmuOps *pOps;
} Lp8755PmuProps;

typedef struct Lp8755RailInfoRec {
    Lp8755PmuSupply   VddId;
    NvU32               SystemRailId;
    NvU32               UserCount;
    NvBool              IsValid;
    Lp8755PmuProps   *pPmuProps;
    NvOdmPmuRailProps   OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Lp8755RailInfo;

typedef struct Lp8755PmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    Lp8755RailInfo RailInfo[Lp8755PmuSupply_Num];
} Lp8755PmuInfo;

static NvBool Lp8755IsRailEnabled(Lp8755PmuInfoHandle hLp8755PmuInfo,
            NvU32 VddId);

static NvBool Lp8755RailEnable(Lp8755PmuInfoHandle hLp8755PmuInfo,
            NvU32 VddId, NvU32 *DelayUS);

static NvBool Lp8755RailDisable(Lp8755PmuInfoHandle hLp8755PmuInfo,
            NvU32 VddId, NvU32 *DelayUS);

static NvBool Lp8755RailSetVoltage(Lp8755PmuInfoHandle hLp8755PmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Lp8755RailGetVoltage(Lp8755PmuInfoHandle hLp8755PmuInfo,
            NvU32 VddId, NvU32 *uV);

static Lp8755PmuOps s_PmuOps = {
    Lp8755IsRailEnabled,
    Lp8755RailEnable,
    Lp8755RailDisable,
    Lp8755RailSetVoltage,
    Lp8755RailGetVoltage,
};

#define REG_DATA(_id, _min_mV, _max_mV \
            ,_step_mV, _pOps, _delay)  \
    {       \
            Lp8755PmuSupply_##_id, LP8755_REG_##_id, LP8755_BUCK_EN, \
            _min_mV * 1000, _max_mV * 1000, _step_mV * 1000, _delay , _pOps \
    }

/* The default config is 6 which uses BUCK0 */
static Lp8755PmuProps s_Lp8755PmuProps[] = {
    REG_DATA(Invalid,   0,    0, 10, &s_PmuOps, 0),
    REG_DATA(BUCK0  , 500, 1670, 10, &s_PmuOps, 0),
};

static NvBool Lp8755IsRailEnabled(Lp8755PmuInfoHandle hLp8755PmuInfo, NvU32 RailId)
{
    NvU8 Control;
    NvBool ret;
    Lp8755PmuProps *pPmuProps;

    if (!hLp8755PmuInfo || !hLp8755PmuInfo->RailInfo[RailId].pPmuProps)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hLp8755PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hLp8755PmuInfo->hOdmPmuI2c, hLp8755PmuInfo->DeviceAddr,
            LP8755_I2C_SPEED, pPmuProps->vout_reg, &Control);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in reading the control register\n", __func__);
        return ret;
    }

    Control &= pPmuProps->vout_mask;
    return (Control != 0);

}

static NvBool Lp8755RailEnable(Lp8755PmuInfoHandle hLp8755PmuInfo,
            NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Lp8755PmuProps *pPmuProps;

    if (!hLp8755PmuInfo || !hLp8755PmuInfo->RailInfo[RailId].pPmuProps)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hLp8755PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cUpdate8(hLp8755PmuInfo->hOdmPmuI2c,
                             hLp8755PmuInfo->DeviceAddr,
                             LP8755_I2C_SPEED, pPmuProps->vout_reg,
                             pPmuProps->vout_mask, pPmuProps->vout_mask);
    if (!ret)
    {
        NvOdmOsPrintf("%s() Error in reading the control register\n", __func__);
        return ret;
    }

    return ret;
}
static NvBool Lp8755RailDisable(Lp8755PmuInfoHandle hLp8755PmuInfo,
            NvU32 RailId, NvU32 *DelayUS)
{
    NvBool ret;
    Lp8755PmuProps *pPmuProps;

    if (!hLp8755PmuInfo || !hLp8755PmuInfo->RailInfo[RailId].pPmuProps)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hLp8755PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cUpdate8(hLp8755PmuInfo->hOdmPmuI2c,
                             hLp8755PmuInfo->DeviceAddr, LP8755_I2C_SPEED,
                             pPmuProps->vout_reg, 0,
                             pPmuProps->vout_mask);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in updating the control register\n", __func__);
        return ret;
    }
    if (DelayUS)
        *DelayUS = pPmuProps->delay;
    return ret;

}

static NvBool Lp8755RailSetVoltage(Lp8755PmuInfoHandle hLp8755PmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU8 vsel;
    NvBool ret;
    Lp8755PmuProps *pPmuProps;

    if (!hLp8755PmuInfo || !hLp8755PmuInfo->RailInfo[RailId].pPmuProps)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pPmuProps = hLp8755PmuInfo->RailInfo[RailId].pPmuProps;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    if (RailId != Lp8755PmuSupply_BUCK0){
        NvOdmOsPrintf("%s() VDD_CPU is supplied by BUCK0 by default\n", __func__);
    }

    if (new_uV < 1670000)
        vsel = ((new_uV - pPmuProps->min_uV)/pPmuProps->step_uV);
    else
        vsel = 117;

    vsel |= pPmuProps->vout_mask;

    ret = NvOdmPmuI2cWrite8(hLp8755PmuInfo->hOdmPmuI2c, hLp8755PmuInfo->DeviceAddr,
            LP8755_I2C_SPEED, pPmuProps->vout_reg, vsel);

    if (!ret) {
        NvOdmOsPrintf("%s() Error in writing the volt register\n", __func__);
        return ret;
    }

    if (DelayUS)
        *DelayUS = pPmuProps->delay;

    return ret;


}

static NvBool Lp8755RailGetVoltage(Lp8755PmuInfoHandle hLp8755PmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    NvU8 vsel;
    NvBool ret;
    Lp8755PmuProps *pPmuProps;

    NV_ASSERT(hLp8755PmuInfo);
    NV_ASSERT(uV);

    if (!hLp8755PmuInfo || !uV)
        return NV_FALSE;

    pPmuProps = hLp8755PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hLp8755PmuInfo->hOdmPmuI2c, hLp8755PmuInfo->DeviceAddr,
            LP8755_I2C_SPEED, pPmuProps->vout_reg, &vsel);

    if (!ret) {
        NvOdmOsPrintf("%s(): Error in reading the Voltage register\n", __func__);
        return ret;
    }

    vsel &= ~ pPmuProps->vout_mask;
    if(vsel < 117)
       *uV = pPmuProps->min_uV + vsel * pPmuProps->step_uV;
    else
       *uV = 1670000;

    return NV_TRUE;
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;

    if (!pI2cModule  || !pI2cInstance || !pI2cAddress)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    pConnectivity = NvOdmPeripheralGetGuid(LP8755_PMUGUID);
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
    NvOdmOsPrintf("LP8755: %s: The system did not doscover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

NvOdmPmuDriverInfoHandle Lp8755PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps)
{
    Lp8755PmuInfoHandle hLp8755PmuInfo;
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
    if (NrOfOdmRailProps != Lp8755PmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }

    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
    if (!Ret)
        return NULL;

    hLp8755PmuInfo = NvOdmOsAlloc(sizeof(Lp8755PmuInfo));
    if (!hLp8755PmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hLp8755PmuInfo, 0, sizeof(Lp8755PmuInfo));

    hLp8755PmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hLp8755PmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        goto I2cOpenFailed;
    }
    hLp8755PmuInfo->DeviceAddr = I2cAddress;

    hLp8755PmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hLp8755PmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hLp8755PmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hLp8755PmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 1; i < Lp8755PmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;
        if (VddId == Lp8755PmuSupply_Invalid)
                continue;

        if (pOdmRailProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been provided, "
                    "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        if (VddId != s_Lp8755PmuProps[VddId].Id)
        {
            NvOdmOsPrintf("%s(): The entries are not in proper sequence, "
                    "Ignoring this entry\n", __func__);
            continue;
        }

        if (hLp8755PmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                    "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        hLp8755PmuInfo->RailInfo[VddId].pPmuProps = &s_Lp8755PmuProps[VddId];

        NvOdmOsMemcpy(&hLp8755PmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));

        hLp8755PmuInfo->RailInfo[VddId].Caps.OdmProtected =
                        hLp8755PmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        /* Maximum current */
        if (VddId == Lp8755PmuSupply_BUCK0)
            hLp8755PmuInfo->RailInfo[VddId].Caps.MaxCurrentMilliAmp = 12000;

        if (hLp8755PmuInfo->RailInfo[VddId].OdmProps.Min_mV)
             hLp8755PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        NV_MAX(hLp8755PmuInfo->RailInfo[VddId].OdmProps.Min_mV,
                           hLp8755PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000);
        else
             hLp8755PmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        hLp8755PmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000;

        if (hLp8755PmuInfo->RailInfo[VddId].OdmProps.Max_mV)
             hLp8755PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                        NV_MIN(hLp8755PmuInfo->RailInfo[VddId].OdmProps.Max_mV,
                           hLp8755PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000);
        else
             hLp8755PmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                           hLp8755PmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000;

        hLp8755PmuInfo->RailInfo[VddId].Caps.StepMilliVolts =
                           hLp8755PmuInfo->RailInfo[VddId].pPmuProps->step_uV/1000;

        if (hLp8755PmuInfo->RailInfo[VddId].OdmProps.Setup_mV)
            hLp8755PmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                hLp8755PmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                    "Ignoring this entry\n", __func__,
                    hLp8755PmuInfo->RailInfo[VddId].OdmProps.Setup_mV,
                    VddId);
            continue;
        }

        hLp8755PmuInfo->RailInfo[VddId].UserCount  = 0;
        hLp8755PmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hLp8755PmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;
    }
    return (NvOdmPmuDriverInfoHandle)hLp8755PmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hLp8755PmuInfo->hOdmPmuService);

PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hLp8755PmuInfo->hOdmPmuI2c);

I2cOpenFailed:
    NvOdmOsFree(hLp8755PmuInfo);
    return NULL;
}

void Lp8755PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Lp8755PmuInfoHandle hLp8755PmuInfo;

    NV_ASSERT(hPmuDriver);

    hLp8755PmuInfo = (Lp8755PmuInfoHandle)hPmuDriver;

    NvOdmServicesPmuClose(hLp8755PmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hLp8755PmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hLp8755PmuInfo);
    hLp8755PmuInfo = NULL;
}

void Lp8755PmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Lp8755PmuInfoHandle hLp8755PmuInfo;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Lp8755PmuSupply_Num);
    NV_ASSERT(pCapabilities);

    if (!hPmuDriver || !(VddId < Lp8755PmuSupply_Num) || !pCapabilities)
        return;

    hLp8755PmuInfo =  (Lp8755PmuInfoHandle)hPmuDriver;

    if (!hLp8755PmuInfo->RailInfo[VddId].IsValid)
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);

    NvOdmOsMemcpy(pCapabilities, &hLp8755PmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Lp8755PmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    Lp8755PmuInfoHandle hLp8755PmuInfo;
    Lp8755PmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Lp8755PmuSupply_Num);
    NV_ASSERT(pMilliVolts);

    if (!hPmuDriver || !(VddId < Lp8755PmuSupply_Num) || !pMilliVolts)
        return NV_FALSE;

    hLp8755PmuInfo =  (Lp8755PmuInfoHandle)hPmuDriver;
    if (!hLp8755PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pPmuProps = hLp8755PmuInfo->RailInfo[VddId].pPmuProps;
    *pMilliVolts = 0;
    if (pPmuProps->pOps->IsRailEnabled(hLp8755PmuInfo, VddId)) {
        Status = pPmuProps->pOps->GetRailVoltage(hLp8755PmuInfo, VddId, &uV);
        if (Status) {
            *pMilliVolts = uV /1000;
        }
    }
    return Status;
}

NvBool Lp8755PmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    Lp8755PmuInfoHandle hLp8755PmuInfo;
    Lp8755PmuProps *pPmuProps;
    Lp8755RailInfo *pRailInfo;
    NvBool Status = NV_TRUE;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Lp8755PmuSupply_Num);
    NV_ASSERT(pSettleMicroSeconds);

    if (!hPmuDriver || !(VddId < Lp8755PmuSupply_Num) || !pSettleMicroSeconds)
        return NV_FALSE;

    hLp8755PmuInfo =  (Lp8755PmuInfoHandle)hPmuDriver;

    if (!hLp8755PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pRailInfo = &hLp8755PmuInfo->RailInfo[VddId];
    pPmuProps = hLp8755PmuInfo->RailInfo[VddId].pPmuProps;
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

    NvOdmOsMutexLock(hLp8755PmuInfo->hPmuMutex);
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
            NvOdmServicesPmuSetSocRailPowerState(hLp8755PmuInfo->hOdmPmuService,
                    hLp8755PmuInfo->RailInfo[VddId].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hLp8755PmuInfo, VddId,
                        pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hLp8755PmuInfo->hOdmPmuService,
                hLp8755PmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hLp8755PmuInfo, VddId,
                    MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hLp8755PmuInfo, VddId,
                    pSettleMicroSeconds);
        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                    __func__, VddId);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hLp8755PmuInfo->hPmuMutex);
    return Status;
}
