/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: MAX897x pmu driver</b>
  *
  * @b Description: Implements the Max897x pmu drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "max897x/nvodm_pmu_max897x_pmu_driver.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"

#define MAX897X_PMUGUID NV_ODM_GUID('m','a','x','8','9','7','x',' ')

#define MAX897X_I2C_SPEED 100

typedef struct Max897xPmuInfoRec *Max897xPmuInfoHandle;

typedef struct Max897xPmuOpsRec {
    NvBool (*IsRailEnabled)(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 VddId);
    NvBool (*EnableRail)(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 VddId, NvU32 *DelayUS);
    NvBool (*DisableRail)(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 VddId, NvU32 *DelayUS);
    NvBool (*SetRailVoltage)(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);
    NvBool (*GetRailVoltage)(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 VddId, NvU32 *uV);
} Max897xPmuOps;

typedef struct Max897xPmuPropsRec {
    NvU32    Id;
    /* Regulator register address.*/
    NvU8     vout_reg;
    NvU8     vout_mask;

    /* Pmu voltage props */
    NvU32     min_uV;
    NvU32     max_uV;
    NvU32     step_uV;
    NvU32     nsteps;

    /* Power rail specific turn-on delay */
    NvU32     delay;
    Max897xPmuOps *pOps;
} Max897xPmuProps;

typedef struct Max897xRailInfoRec {
    Max897xPmuSupply   VddId;
    NvU32              SystemRailId;
    NvU32              UserCount;
    NvBool             IsValid;
    Max897xPmuProps    *pPmuProps;
    NvOdmPmuRailProps  OdmProps;
    NvOdmPmuVddRailCapabilities Caps;
} Max897xRailInfo;

typedef struct Max897xPmuInfoRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvOdmServicesPmuHandle hOdmPmuService;
    NvOdmOsMutexHandle hPmuMutex;
    NvU32 DeviceAddr;
    Max897xRailInfo RailInfo[Max897xPmuSupply_Num];
} Max897xPmuInfo;

static NvBool Max897xIsRailEnabled(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 VddId);

static NvBool Max897xRailEnable(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 VddId, NvU32 *DelayUS);

static NvBool Max897xRailDisable(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 VddId, NvU32 *DelayUS);

static NvBool Max897xRailSetVoltage(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 VddId, NvU32 new_uV, NvU32 *DelayUS);

static NvBool Max897xRailGetVoltage(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 VddId, NvU32 *uV);

static Max897xPmuOps s_PmuOps = {
    Max897xIsRailEnabled,
    Max897xRailEnable,
    Max897xRailDisable,
    Max897xRailSetVoltage,
    Max897xRailGetVoltage,
};

#define REG_DATA(_id, _vout_reg, _vout_mask, _min_uV, _max_uV, _step_uV, \
            _nsteps, _pOps, _delay)  \
    {       \
            Max897xMode_##_id, _vout_reg, _vout_mask, \
            _min_uV, _max_uV, _step_uV, _nsteps, _delay, _pOps, \
    }

/* Should be in same sequence as the enums are */
static Max897xPmuProps s_Max897xPmuProps[] = {
    REG_DATA(Invalid, 0,    0,      0,       0,     0,    0, &s_PmuOps, 0),
    REG_DATA(0,     0x0, 0x7F, 606250, 1400000,  6250, 0x80, &s_PmuOps, 0),
    REG_DATA(1,     0x1, 0x7F, 606250, 1400000,  6250, 0x80, &s_PmuOps, 0),
};

static NvBool Max897xIsRailEnabled(Max897xPmuInfoHandle hMax897xPmuInfo, NvU32 RailId)
{
    return NV_TRUE;
}

static NvBool Max897xRailEnable(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 RailId, NvU32 *DelayUS)
{
    return NV_TRUE;
}

static NvBool Max897xRailDisable(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 RailId, NvU32 *DelayUS)
{
    return NV_TRUE;
}

static NvBool Max897xRailSetVoltage(Max897xPmuInfoHandle hMax897xPmuInfo,
            NvU32 RailId, NvU32 new_uV, NvU32 *DelayUS)
{
    NvU32 vsel;
    NvBool ret;
    Max897xPmuProps *pPmuProps;

    NV_ASSERT(hMax897xPmuInfo);
    NV_ASSERT(hMax897xPmuInfo->RailInfo[RailId].pPmuProps);

    if (!hMax897xPmuInfo || !hMax897xPmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hMax897xPmuInfo->RailInfo[RailId].pPmuProps;

    if ((new_uV < pPmuProps->min_uV) || (new_uV > pPmuProps->max_uV))
        return NV_FALSE;

    vsel = (new_uV - pPmuProps->min_uV + pPmuProps->step_uV - 1)/ pPmuProps->step_uV;

    if (vsel > pPmuProps->nsteps)
        return NV_FALSE;

    ret = NvOdmPmuI2cUpdate8(hMax897xPmuInfo->hOdmPmuI2c, hMax897xPmuInfo->DeviceAddr,
            MAX897X_I2C_SPEED, pPmuProps->vout_reg, vsel, pPmuProps->vout_mask);
    if (!ret) {
        NvOdmOsPrintf("%s():  Error in writing the Voltage register\n", __func__);
    }
    if (DelayUS && ret)
        *DelayUS = pPmuProps->delay;
    return ret;
}

static NvBool Max897xRailGetVoltage(Max897xPmuInfoHandle hMax897xPmuInfo,
        NvU32 RailId, NvU32 *uV)
{
    NvU8 vsel;
    NvBool ret;
    Max897xPmuProps *pPmuProps;

    NV_ASSERT(hMax897xPmuInfo);
    NV_ASSERT(hMax897xPmuInfo->RailInfo[RailId].pPmuProps);

    if (!hMax897xPmuInfo || !hMax897xPmuInfo->RailInfo[RailId].pPmuProps)
        return NV_FALSE;

    pPmuProps = hMax897xPmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hMax897xPmuInfo->hOdmPmuI2c, hMax897xPmuInfo->DeviceAddr,
            MAX897X_I2C_SPEED, pPmuProps->vout_reg, &vsel);
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
    const NvOdmPeripheralConnectivity *pConnectivity;

    NV_ASSERT(pI2cModule);
    NV_ASSERT(pI2cInstance);
    NV_ASSERT(pI2cAddress);

    if (!pI2cModule || !pI2cInstance || !pI2cAddress)
        return NV_FALSE;

    pConnectivity = NvOdmPeripheralGetGuid(MAX897X_PMUGUID);
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
    NvOdmOsPrintf("MAX897x: %s: The system did not discover PMU from the data base OR "
                     "the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

NvOdmPmuDriverInfoHandle Max897xPmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
        NvOdmPmuRailProps *pOdmRailProps, NvU32 NrOfOdmRailProps)
{
    Max897xPmuInfoHandle hMax897xPmuInfo;
    NvU32 i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvBool Ret;
    NvU32 VddId;
    Max897xBoardData *pBoardData;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmRailProps);

    if (!hDevice || !pOdmRailProps)
        return NULL;

    /* Information for all rail is required */
    if (NrOfOdmRailProps != Max897xPmuSupply_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }
    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
    if (!Ret)
        return NULL;

    hMax897xPmuInfo = NvOdmOsAlloc(sizeof(Max897xPmuInfo));
    if (!hMax897xPmuInfo)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hMax897xPmuInfo, 0, sizeof(Max897xPmuInfo));

    hMax897xPmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hMax897xPmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        goto I2cOpenFailed;
    }
    hMax897xPmuInfo->DeviceAddr = I2cAddress;

    hMax897xPmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hMax897xPmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hMax897xPmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hMax897xPmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 1; i < Max897xPmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;
        if (VddId == Max897xPmuSupply_Invalid)
                continue;
        pBoardData = pOdmRailProps[i].pBoardData;

        if (!pBoardData || (pBoardData->VoltageMode > Max897xMode_1))
        {
            NvOdmOsPrintf("%s(): The board information is not correct, "
                        "Ignoring this entry\n", __func__);
            continue;
        }
        if (hMax897xPmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        hMax897xPmuInfo->RailInfo[VddId].pPmuProps = &s_Max897xPmuProps[pBoardData->VoltageMode];

        NvOdmOsMemcpy(&hMax897xPmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));

        hMax897xPmuInfo->RailInfo[VddId].Caps.OdmProtected =
                        hMax897xPmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        if (hMax897xPmuInfo->RailInfo[VddId].OdmProps.Min_mV)
             hMax897xPmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        NV_MAX(hMax897xPmuInfo->RailInfo[VddId].OdmProps.Min_mV,
                           hMax897xPmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000);
        else
             hMax897xPmuInfo->RailInfo[VddId].Caps.MinMilliVolts =
                        hMax897xPmuInfo->RailInfo[VddId].pPmuProps->min_uV/1000;

        if (hMax897xPmuInfo->RailInfo[VddId].OdmProps.Max_mV)
             hMax897xPmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                        NV_MIN(hMax897xPmuInfo->RailInfo[VddId].OdmProps.Max_mV,
                           hMax897xPmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000);
        else
             hMax897xPmuInfo->RailInfo[VddId].Caps.MaxMilliVolts =
                           hMax897xPmuInfo->RailInfo[VddId].pPmuProps->max_uV/1000;

        hMax897xPmuInfo->RailInfo[VddId].Caps.StepMilliVolts =
                           hMax897xPmuInfo->RailInfo[VddId].pPmuProps->step_uV/1000;

        if (hMax897xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV)
             hMax897xPmuInfo->RailInfo[VddId].Caps.requestMilliVolts =
                        hMax897xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV;
        else
        {
            NvOdmOsPrintf("%s(): The setup voltage %d is not proper for rail %d, "
                        "Ignoring this entry\n", __func__,
                         hMax897xPmuInfo->RailInfo[VddId].OdmProps.Setup_mV,
                         VddId);
            continue;
        }

        hMax897xPmuInfo->RailInfo[VddId].UserCount  = 0;
        hMax897xPmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hMax897xPmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;
    }
    return (NvOdmPmuDriverInfoHandle)hMax897xPmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hMax897xPmuInfo->hOdmPmuService);

PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hMax897xPmuInfo->hOdmPmuI2c);

I2cOpenFailed:
    NvOdmOsFree(hMax897xPmuInfo);
    return NULL;
}

void Max897xPmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Max897xPmuInfoHandle hMax897xPmuInfo = (Max897xPmuInfoHandle)hPmuDriver;

    NV_ASSERT(hMax897xPmuInfo);

    if (!hMax897xPmuInfo)
        return;

    NvOdmServicesPmuClose(hMax897xPmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hMax897xPmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hMax897xPmuInfo);
}

void Max897xPmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Max897xPmuInfoHandle hMax897xPmuInfo =  (Max897xPmuInfoHandle)hPmuDriver;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Max897xPmuSupply_Num);
    NV_ASSERT(pCapabilities);

    if(!hPmuDriver || !(VddId < Max897xPmuSupply_Num) || !pCapabilities)
        return;

    if (!hMax897xPmuInfo->RailInfo[VddId].IsValid)
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);

    NvOdmOsMemcpy(pCapabilities, &hMax897xPmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Max897xPmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    Max897xPmuInfoHandle hMax897xPmuInfo =  (Max897xPmuInfoHandle)hPmuDriver;
    Max897xPmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    NvU32 uV;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Max897xPmuSupply_Num);
    NV_ASSERT(pMilliVolts);

    if(!hPmuDriver || !(VddId < Max897xPmuSupply_Num) || !pMilliVolts)
        return NV_FALSE;

    if (!hMax897xPmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pPmuProps = hMax897xPmuInfo->RailInfo[VddId].pPmuProps;
    *pMilliVolts = 0;
    if (pPmuProps->pOps->IsRailEnabled(hMax897xPmuInfo, VddId)) {
        Status = pPmuProps->pOps->GetRailVoltage(hMax897xPmuInfo, VddId, &uV);
        if (Status) {
            *pMilliVolts = uV /1000;
        }
    }
    return Status;
}

NvBool Max897xPmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    Max897xPmuInfoHandle hMax897xPmuInfo =  (Max897xPmuInfoHandle)hPmuDriver;
    Max897xPmuProps *pPmuProps;
    Max897xRailInfo *pRailInfo;
    NvBool Status = NV_TRUE;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < Max897xPmuSupply_Num);
    NV_ASSERT(pSettleMicroSeconds);

    if(!hPmuDriver || !(VddId < Max897xPmuSupply_Num) || !pSettleMicroSeconds)
        return NV_FALSE;

    if (!hMax897xPmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pRailInfo = &hMax897xPmuInfo->RailInfo[VddId];
    pPmuProps = hMax897xPmuInfo->RailInfo[VddId].pPmuProps;
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

    NvOdmOsMutexLock(hMax897xPmuInfo->hPmuMutex);
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
            NvOdmServicesPmuSetSocRailPowerState(hMax897xPmuInfo->hOdmPmuService,
                        hMax897xPmuInfo->RailInfo[VddId].SystemRailId, NV_FALSE);
            if (pPmuProps->pOps->DisableRail)
                Status = pPmuProps->pOps->DisableRail(hMax897xPmuInfo, VddId,
                                            pSettleMicroSeconds);
        }
        pRailInfo->UserCount--;
        goto End;
    }
    else
    {
        NvOdmServicesPmuSetSocRailPowerState(hMax897xPmuInfo->hOdmPmuService,
                        hMax897xPmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

        NV_ASSERT(pPmuProps->pOps->SetRailVoltage || pPmuProps->pOps->EnableRail);
        if (pPmuProps->pOps->SetRailVoltage)
            Status = pPmuProps->pOps->SetRailVoltage(hMax897xPmuInfo, VddId,
                                MilliVolts * 1000, pSettleMicroSeconds);

        if (Status && pPmuProps->pOps->EnableRail)
            Status = pPmuProps->pOps->EnableRail(hMax897xPmuInfo, VddId,
                                                        pSettleMicroSeconds);
        if (!Status) {
            NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
                        __func__, VddId);
            goto End;
        }
        pRailInfo->UserCount++;
    }

End:
    NvOdmOsMutexUnlock(hMax897xPmuInfo->hPmuMutex);
    return Status;
}
