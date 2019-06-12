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
  * @file: <b>NVIDIA Tegra ODM Kit: TPS65090 pmu driver</b>
  *
  * @b Description: Implements the TPS65090 pmu drivers.
  *
  */

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "tps65090/nvodm_pmu_tps65090_pmu_driver.h"

#define TPS65090_PMUGUID NV_ODM_GUID('t','p','s','6','5','0','9','0')
#define TPS65090_I2C_SPEED 100

static NvBool Tps65090IsRailEnabled(Tps65090PmuInfoHandle hTps65090PmuInfo,
               NvU32 RailId);

static NvBool Tps65090RailEnable(Tps65090PmuInfoHandle hTps65090PmuInfo,
               NvU32 RailId, NvU32 *DelayUs);

static NvBool Tps65090RailDisable(Tps65090PmuInfoHandle hTps65090PmuInfo,
               NvU32 RailId, NvU32 *DelayUs);

static Tps65090PmuOps s_Tps65090PmuOps = {
    Tps65090IsRailEnabled,
    Tps65090RailEnable,
    Tps65090RailDisable,
};

#define REG_DATA(_id, _en_reg, _op_val) \
    {    \
            TPS65090PmuSupply_##_id,    \
                    TPS65090_##_en_reg, (1 << 0), \
                    _op_val * 1000, \
                    &s_Tps65090PmuOps, \
    }

/* Should be in same sequence as the enums are */
static Tps65090PmuProps s_Tps65090PmuProps[] = {

    REG_DATA(Invalid, RFF_INVALID,  0),
    REG_DATA(DCDC1, R12_DCDC1_CTRL, 5050),
    REG_DATA(DCDC2, R13_DCDC2_CTRL, 3330),
    /* FIND THE OPERATING VOLTAGE FOR DCDC3 */
    REG_DATA(DCDC3, R14_DCDC3_CTRL, 0),
    REG_DATA(FET1,  R15_FET1_CTRL,  500),
    REG_DATA(FET2,  R16_FET2_CTRL,  500),
    REG_DATA(FET3,  R17_FET3_CTRL,  500),
    REG_DATA(FET4,  R18_FET4_CTRL,  500),
    REG_DATA(FET5,  R19_FET5_CTRL,  500),
    REG_DATA(FET6,  R20_FET6_CTRL,  500),
    REG_DATA(FET7,  R21_FET7_CTRL,  500),
};

static NvBool Tps65090IsRailEnabled(Tps65090PmuInfoHandle hTps65090PmuInfo,
                NvU32 RailId)
{
    NvU8 Control;
    NvBool ret;
    Tps65090PmuProps *pPmuProps;

    NV_ASSERT(hTps65090PmuInfo);

    if (!hTps65090PmuInfo)
        return NV_FALSE;

    pPmuProps = hTps65090PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cRead8(hTps65090PmuInfo->hOdmPmuI2c, hTps65090PmuInfo->DeviceAddr,
            TPS65090_I2C_SPEED, pPmuProps->en_reg, &Control);

    if (!ret) {
        NvOdmOsPrintf("%s(): Error in reading the supply register\n", __func__);
        return ret;
    }
    return (((Control >> pPmuProps->en_bit) & 1) == 1);
}

static NvBool Tps65090RailEnable(Tps65090PmuInfoHandle hTps65090PmuInfo,
                NvU32 RailId, NvU32 *DelayUs)
{
    NvBool ret;
    Tps65090PmuProps *pPmuProps;

    NV_ASSERT(hTps65090PmuInfo);

    if (!hTps65090PmuInfo)
        return NV_FALSE;

    pPmuProps = hTps65090PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cSetBits(hTps65090PmuInfo->hOdmPmuI2c, hTps65090PmuInfo->DeviceAddr,
            TPS65090_I2C_SPEED, pPmuProps->en_reg, pPmuProps->en_bit);
    if (!ret) {
        NvOdmOsPrintf("%s(): Error in enabling the rail\n", __func__);
    }

    if (ret && DelayUs)
        *DelayUs = 0;

    return ret;
}

static NvBool Tps65090RailDisable(Tps65090PmuInfoHandle hTps65090PmuInfo,
                NvU32 RailId, NvU32 *DelayUs)
{
    NvBool ret;
    Tps65090PmuProps *pPmuProps;

    NV_ASSERT(hTps65090PmuInfo);

    if (!hTps65090PmuInfo)
        return NV_FALSE;

    pPmuProps = hTps65090PmuInfo->RailInfo[RailId].pPmuProps;

    ret = NvOdmPmuI2cClrBits(hTps65090PmuInfo->hOdmPmuI2c, hTps65090PmuInfo->DeviceAddr,
            TPS65090_I2C_SPEED, pPmuProps->en_reg, pPmuProps->en_bit);
    if (!ret) {
        NvOdmOsPrintf("%s() Error in disabling the rail\n", __func__);
    }

    if (ret && DelayUs)
        *DelayUs = 0;

    return ret;
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(TPS65090_PMUGUID);
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
    NvOdmOsPrintf("TPS65090: %s(): The system did not discover PMU from the "
                     "database OR the Interface entry is not available\n", __func__);
    return NV_FALSE;
}

NvOdmPmuDriverInfoHandle Tps65090PmuDriverOpen(NvOdmPmuDeviceHandle hDevice,
                NvOdmPmuRailProps *pOdmRailProps, NvS32 NrOfOdmRailProps)
{
    Tps65090PmuInfoHandle hTps65090PmuInfo;
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
    if (NrOfOdmRailProps != TPS65090PmuSupply_Num)
    {
        NvOdmOsPrintf("%s(): Rail properties are not matching\n", __func__);
        return NULL;
    }
    Ret = GetInterfaceDetail(&I2cModule, &I2cInstance, &I2cAddress);
    if (!Ret)
        return NULL;

    hTps65090PmuInfo = NvOdmOsAlloc(sizeof(Tps65090PmuInfo));
    if (!hTps65090PmuInfo)
    {
        NvOdmOsPrintf("%s(): Memory allocation failed.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hTps65090PmuInfo, 0, sizeof(Tps65090PmuInfo));

    hTps65090PmuInfo->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hTps65090PmuInfo->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s(): Error in opening PmuI2c instance.\n", __func__);
        goto I2cOpenFailed;
    }
    hTps65090PmuInfo->DeviceAddr = I2cAddress;

    PmuDebugPrintf("%s(): Device address is 0x%x\n", __func__, hTps65090PmuInfo->DeviceAddr);

    hTps65090PmuInfo->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hTps65090PmuInfo->hOdmPmuService)
    {
        NvOdmOsPrintf("%s(): Error Opening Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hTps65090PmuInfo->hPmuMutex = NvOdmOsMutexCreate();
    if (!hTps65090PmuInfo->hPmuMutex)
    {
        NvOdmOsPrintf("%s(): Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 1; i < TPS65090PmuSupply_Num; ++i)
    {
        VddId = pOdmRailProps[i].VddId;
        if (VddId == TPS65090PmuSupply_Invalid)
                continue;

        if (pOdmRailProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been provided, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        if (VddId != s_Tps65090PmuProps[VddId].Id)
        {
            NvOdmOsPrintf("%s(): The entries are not in proper sequence, "
                        "Ignoring this entry\n", __func__);
            continue;
        }

        if (hTps65090PmuInfo->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        hTps65090PmuInfo->RailInfo[VddId].pPmuProps = &s_Tps65090PmuProps[VddId];

        NvOdmOsMemcpy(&hTps65090PmuInfo->RailInfo[VddId].OdmProps,
                &pOdmRailProps[VddId], sizeof(NvOdmPmuRailProps));

        hTps65090PmuInfo->RailInfo[VddId].Caps.OdmProtected =
                        hTps65090PmuInfo->RailInfo[VddId].OdmProps.IsOdmProtected;

        hTps65090PmuInfo->RailInfo[VddId].IsValid = NV_TRUE;
        hTps65090PmuInfo->RailInfo[VddId].SystemRailId = pOdmRailProps[VddId].SystemRailId;
    }
    return (NvOdmPmuDriverInfoHandle)hTps65090PmuInfo;

MutexCreateFailed:
    NvOdmServicesPmuClose(hTps65090PmuInfo->hOdmPmuService);
PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hTps65090PmuInfo->hOdmPmuI2c);
I2cOpenFailed:
    NvOdmOsFree(hTps65090PmuInfo);
    return NULL;
}

void Tps65090PmuDriverClose(NvOdmPmuDriverInfoHandle hPmuDriver)
{
    Tps65090PmuInfoHandle hTps65090PmuInfo;
    NV_ASSERT(hPmuDriver);

    if (!hPmuDriver)
        return;

    hTps65090PmuInfo = (Tps65090PmuInfoHandle)hPmuDriver;

    NvOdmServicesPmuClose(hTps65090PmuInfo->hOdmPmuService);
    NvOdmPmuI2cClose(hTps65090PmuInfo->hOdmPmuI2c);
    NvOdmOsFree(hTps65090PmuInfo);
}

void Tps65090PmuGetCaps(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    Tps65090PmuInfoHandle hTps65090PmuInfo;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS65090PmuSupply_Num);
    NV_ASSERT(pCapabilities);

    if (!hPmuDriver || !(VddId < TPS65090PmuSupply_Num) || !pCapabilities)
        return;

    hTps65090PmuInfo =  (Tps65090PmuInfoHandle)hPmuDriver;

    if (!hTps65090PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provided the information about this "
                    "rail %d", __func__, VddId);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }

    NvOdmOsMemcpy(pCapabilities, &hTps65090PmuInfo->RailInfo[VddId].Caps,
                sizeof(NvOdmPmuVddRailCapabilities));
}

NvBool Tps65090PmuGetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32* pMilliVolts)
{
    Tps65090PmuProps *pPmuProps;
    NvBool Status = NV_TRUE;
    Tps65090PmuInfoHandle hTps65090PmuInfo;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS65090PmuSupply_Num);
    NV_ASSERT(pMilliVolts);

    if (!hPmuDriver || !(VddId < TPS65090PmuSupply_Num) || !pMilliVolts)
        return NV_FALSE;

    hTps65090PmuInfo =  (Tps65090PmuInfoHandle)hPmuDriver;

    if (!hTps65090PmuInfo->RailInfo[VddId].IsValid)
    {
        NvOdmOsPrintf("%s(): The client has not provide the information about this "
                    "rail %d", __func__, VddId);
        return NV_FALSE;
    }

    pPmuProps = hTps65090PmuInfo->RailInfo[VddId].pPmuProps;

    *pMilliVolts = 0;

    if (pPmuProps->pOps->IsRailEnabled(hTps65090PmuInfo, VddId))
        *pMilliVolts = pPmuProps->op_val/1000;
    else
        return NV_FALSE;

    return Status;
}

NvBool Tps65090PmuSetVoltage(
    NvOdmPmuDriverInfoHandle hPmuDriver,
    NvU32 VddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    Tps65090PmuProps *pPmuProps;
    Tps65090RailInfo *pRailInfo;
    NvBool Status = NV_TRUE;
    Tps65090PmuInfoHandle hTps65090PmuInfo;

    NV_ASSERT(hPmuDriver);
    NV_ASSERT(VddId < TPS65090PmuSupply_Num);
    NV_ASSERT(pSettleMicroSeconds);

    if (!hPmuDriver || !(VddId < TPS65090PmuSupply_Num) || !pSettleMicroSeconds)
       return NV_FALSE;

    hTps65090PmuInfo =  (Tps65090PmuInfoHandle)hPmuDriver;

    PmuDebugPrintf("%s(): Rail %u and voltage %u\n", __func__, VddId, MilliVolts);

    if (!hTps65090PmuInfo->RailInfo[VddId].IsValid)
    {
       NvOdmOsPrintf("%s(): The client has not provide the information about this "
              "rail %d", __func__, VddId);
       return NV_FALSE;
    }

    pRailInfo = &hTps65090PmuInfo->RailInfo[VddId];
    pPmuProps = hTps65090PmuInfo->RailInfo[VddId].pPmuProps;
    *pSettleMicroSeconds = 0;

    if (pRailInfo->OdmProps.IsOdmProtected == NV_TRUE)
    {
       NvOdmOsPrintf("%s:The voltage is protected and cannot be set.\n", __func__);
       return NV_FALSE;
    }

    if(MilliVolts > pPmuProps->op_val)
    {
       NvOdmOsPrintf("%s:The required voltage is not supported. vddRail: %d,"
              " MilliVolts: %d\n", __func__, VddId, MilliVolts);
       return NV_FALSE;
    }

    NvOdmOsMutexLock(hTps65090PmuInfo->hPmuMutex);
    NvOdmServicesPmuSetSocRailPowerState(hTps65090PmuInfo->hOdmPmuService,
           hTps65090PmuInfo->RailInfo[VddId].SystemRailId, NV_TRUE);

    Status = pPmuProps->pOps->EnableRail(hTps65090PmuInfo, VddId, pSettleMicroSeconds);

    if (!Status) {
       NvOdmOsPrintf("%s:The failure in enabling voltage rail %d\n",
              __func__, VddId);
       goto End;
    }

End:
    NvOdmOsMutexUnlock(hTps65090PmuInfo->hPmuMutex);
    return Status;
}
