/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: Odm pmu driver</b>
  *
  * @b Description: Implements the TCA6416 based pmu drivers.
  *
  */
#include "nvodm_pmu.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvodm_pmu_tca6416.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"

typedef struct Tca6416RailInfoRec {
    NvU32 SystemRailId;
    NvU32 GpioNr;
    NvU32 UserCount;
    NvBool IsValid;
    NvBool IsCurrentStateOn;
    OdmPmuTca6416RailProps OdmProps;
} Tca6416RailInfo;

typedef struct OdmTca6416GpioDriverRec {
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 DeviceAddress;
    NvOdmOsMutexHandle hPmuMutex;
    NvOdmPmuDeviceHandle hPmuDevice;
    NvOdmServicesPmuHandle hOdmPmuService;
    Tca6416RailInfo RailInfo[OdmPmuTca6416Port_Num];
} OdmTca6416GpioDriver, *OdmTca6416GpioDriverHandle;

#define TCA6416_I2C_SPEED_KHZ 100

static void Tca6416SetDirectionOutput(
    OdmTca6416GpioDriverHandle hTca6416GpioDriver, NvU32 GpioNr,
    NvU32 Value)
{
    NvBool ret;
    if (GpioNr > 15)
    {
        NvOdmOsPrintf("%s(): The gpio nr %d is more than supported\n",
                                  __func__, GpioNr);
        return;
    }

    if (Value)
        ret = NvOdmPmuI2cSetBits(hTca6416GpioDriver->hOdmPmuI2c,
                TCA6416_I2C_SPEED_KHZ, hTca6416GpioDriver->DeviceAddress,
                (0x02 + GpioNr/8), (1 << GpioNr%8));
    else
        ret = NvOdmPmuI2cClrBits(hTca6416GpioDriver->hOdmPmuI2c,
                TCA6416_I2C_SPEED_KHZ, hTca6416GpioDriver->DeviceAddress,
                (0x02 + GpioNr/8), (1 << GpioNr%8));

    if (!ret) {
        NvOdmOsPrintf("%s(): Error in updating the output port register\n",
                        __func__);
        return;
    }

    ret = NvOdmPmuI2cClrBits(hTca6416GpioDriver->hOdmPmuI2c,
                TCA6416_I2C_SPEED_KHZ, hTca6416GpioDriver->DeviceAddress,
                (0x06 + GpioNr/8), (1 << GpioNr%8));
    if (!ret)
        NvOdmOsPrintf("%s(): Error in updating the configuration register\n",
                       __func__);
}

static void Tca6416SetDirectionInput(OdmTca6416GpioDriverHandle hTca6416GpioDriver,
    NvU32 GpioNr)
{
    NvBool ret;
    if (GpioNr > 15)
    {
        NvOdmOsPrintf("%s(): The gpio nr %d is more than supported\n",
                       __func__, GpioNr);
        return;
    }

    ret = NvOdmPmuI2cSetBits(hTca6416GpioDriver->hOdmPmuI2c,
                TCA6416_I2C_SPEED_KHZ, hTca6416GpioDriver->DeviceAddress,
                (0x06 + GpioNr/8), (1 << GpioNr%8));
    if (!ret)
        NvOdmOsPrintf("%s() Error in updating the configuring port register\n",
                       __func__);
}

static void Tca6416SetValue(OdmTca6416GpioDriverHandle hTca6416GpioDriver,
    NvU32 GpioNr, NvU32 Value)
{
    NvBool ret;

    if (GpioNr > 15)
    {
        NvOdmOsPrintf("%s(): The gpio nr %d is more than supported\n",
               __func__, GpioNr);
        return;
    }
    if (Value)
        ret = NvOdmPmuI2cSetBits(hTca6416GpioDriver->hOdmPmuI2c,
                TCA6416_I2C_SPEED_KHZ, hTca6416GpioDriver->DeviceAddress,
                (0x02 + GpioNr/8), (1 << GpioNr%8));
    else
        ret = NvOdmPmuI2cClrBits(hTca6416GpioDriver->hOdmPmuI2c,
                TCA6416_I2C_SPEED_KHZ, hTca6416GpioDriver->DeviceAddress,
                (0x02 + GpioNr/8), (1 << GpioNr%8));
    if (!ret)
        NvOdmOsPrintf("%s() Error in updating the output port register\n",
                    __func__);
}

static NvBool GetInterfaceDetail(NvOdmIoModule *pI2cModule, NvU64 searchGuid,
                            NvU32 *pI2cInstance, NvU32 *pI2cAddress)
{
    NvU32 i;
    const NvOdmPeripheralConnectivity *pConnectivity =
                           NvOdmPeripheralGetGuid(searchGuid);
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
    NvOdmOsPrintf("TCA6416: %s(): The system did not discover PMU from the "
                     "database OR the Interface entry is not available\n",
                  __func__);
    return NV_FALSE;
}

NvOdmPmuDriverInfoHandle OdmPmuTca6416GpioOpen(NvOdmPmuDeviceHandle hDevice,
    OdmPmuTca6416RailProps *pOdmProps, int NrOfRails, NvU64 searchGuid)
{
    OdmTca6416GpioDriverHandle hTca6416GpioDriver;
    int i;
    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;
    NvBool Ret;
    OdmPmuTca6416Port VddId;
    NvU32 Value;

    NV_ASSERT(hDevice);
    NV_ASSERT(pOdmProps);

    /* Information for all rail is required */
    if (NrOfRails != OdmPmuTca6416Port_Num)
    {
        NvOdmOsPrintf("%s:Rail properties are not matching\n", __func__);
        return NULL;
    }

    Ret = GetInterfaceDetail(&I2cModule, searchGuid, &I2cInstance, &I2cAddress);
    if (!Ret)
        return NULL;

    hTca6416GpioDriver = NvOdmOsAlloc(sizeof(OdmTca6416GpioDriver));
    if (!hTca6416GpioDriver)
    {
        NvOdmOsPrintf("%s:Memory allocation fails.\n", __func__);
        return NULL;
    }

    NvOdmOsMemset(hTca6416GpioDriver, 0, sizeof(OdmTca6416GpioDriver));

    hTca6416GpioDriver->hOdmPmuI2c = NvOdmPmuI2cOpen(I2cModule, I2cInstance);
    if (!hTca6416GpioDriver->hOdmPmuI2c)
    {
        NvOdmOsPrintf("%s: Error Open I2C device.\n", __func__);
        goto I2cOpenFailed;
    }
    hTca6416GpioDriver->DeviceAddress = I2cAddress;

    PmuDebugPrintf("%s() Device address is 0x%x\n", __func__,
                                       hTca6416GpioDriver->DeviceAddr);

    hTca6416GpioDriver->hOdmPmuService = NvOdmServicesPmuOpen();
    if (!hTca6416GpioDriver->hOdmPmuService)
    {
        NvOdmOsPrintf("%s: Error Open Pmu Service.\n", __func__);
        goto PmuServiceOpenFailed;
    }

    hTca6416GpioDriver->hPmuMutex = NvOdmOsMutexCreate();
    if (!hTca6416GpioDriver->hPmuMutex)
    {
        NvOdmOsPrintf("%s: Error in Creating Mutex.\n", __func__);
        goto MutexCreateFailed;
    }

    for (i = 0; i < OdmPmuTca6416Port_Num; ++i)
    {
        VddId = pOdmProps[i].PortId;
        if (VddId >= OdmPmuTca6416Port_Num)
                continue;

        if (pOdmProps[i].SystemRailId == 0)
        {
            PmuDebugPrintf("%s(): The system RailId for rail %d has not been "
                        "provided, Ignoring this entry\n", __func__, VddId);
            continue;
        }

        if (hTca6416GpioDriver->RailInfo[VddId].IsValid)
        {
            NvOdmOsPrintf("%s(): The entries are duplicated %d, "
                        "Ignoring this entry\n", __func__, VddId);
            continue;
        }

        NvOdmOsMemcpy(&hTca6416GpioDriver->RailInfo[VddId].OdmProps,
                &pOdmProps[VddId], sizeof(OdmPmuTca6416RailProps));

        hTca6416GpioDriver->RailInfo[VddId].SystemRailId =
                                         pOdmProps[VddId].SystemRailId;
        hTca6416GpioDriver->RailInfo[VddId].GpioNr = VddId;
        hTca6416GpioDriver->RailInfo[VddId].UserCount  = 0;
        hTca6416GpioDriver->RailInfo[VddId].IsValid = NV_TRUE;
        hTca6416GpioDriver->RailInfo[VddId].IsCurrentStateOn = NV_FALSE;
    }

    for (i = 0; i < OdmPmuTca6416Port_Num; ++i)
    {
        if (hTca6416GpioDriver->RailInfo[i].IsValid)
        {
            if (hTca6416GpioDriver->RailInfo[VddId].OdmProps.IsInitStateEnable)
                Value =
                  (hTca6416GpioDriver->RailInfo[VddId].OdmProps.IsActiveLow)? 0: 1;
            else
                Value =
                   (hTca6416GpioDriver->RailInfo[VddId].OdmProps.IsActiveLow)? 1: 0;
            Tca6416SetDirectionOutput(hTca6416GpioDriver,
                        hTca6416GpioDriver->RailInfo[VddId].GpioNr, Value);
        }
        else
        {
            Tca6416SetDirectionInput(hTca6416GpioDriver,
                        hTca6416GpioDriver->RailInfo[VddId].GpioNr);
        }
    }
    return (NvOdmPmuDriverInfoHandle)hTca6416GpioDriver;

MutexCreateFailed:
    NvOdmServicesPmuClose(hTca6416GpioDriver->hOdmPmuService);
PmuServiceOpenFailed:
    NvOdmPmuI2cClose(hTca6416GpioDriver->hOdmPmuI2c);
I2cOpenFailed:
    NvOdmOsFree(hTca6416GpioDriver);
    return NULL;
}

void OdmPmuTca6416GpioClose(NvOdmPmuDriverInfoHandle hOdmPmuDriver)
{
    OdmTca6416GpioDriverHandle hTca6416GpioDriver =
                                 (OdmTca6416GpioDriverHandle)hOdmPmuDriver;

    NvOdmServicesPmuClose(hTca6416GpioDriver->hOdmPmuService);
    NvOdmPmuI2cClose(hTca6416GpioDriver->hOdmPmuI2c);
    NvOdmOsFree(hTca6416GpioDriver);
}

void
OdmPmuTca6416GpioGetCapabilities(
    NvOdmPmuDriverInfoHandle hOdmPmuDriver,
    NvU32 VddRail,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
    OdmTca6416GpioDriver *pTca6416GpioDriver = hOdmPmuDriver;

    if (VddRail >= OdmPmuTca6416Port_Num)
    {
        NvOdmOsPrintf("%s(): The VddRail %d is more than registered "
                        "rails\n", __func__, VddRail);
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        return;
    }

    pCapabilities->OdmProtected =
             pTca6416GpioDriver->RailInfo[VddRail].OdmProps.IsOdmProtected;
    pCapabilities->MinMilliVolts =
             pTca6416GpioDriver->RailInfo[VddRail].OdmProps.OffVoltage_mV;
    pCapabilities->MaxMilliVolts =
             pTca6416GpioDriver->RailInfo[VddRail].OdmProps.OnVoltage_mV;
    pCapabilities->StepMilliVolts =
             pTca6416GpioDriver->RailInfo[VddRail].OdmProps.OnVoltage_mV -
                  pTca6416GpioDriver->RailInfo[VddRail].OdmProps.OffVoltage_mV;
    pCapabilities->requestMilliVolts =
             pTca6416GpioDriver->RailInfo[VddRail].OdmProps.OnVoltage_mV;
}

NvBool
OdmPmuTca6416GpioGetVoltage(
    NvOdmPmuDriverInfoHandle hOdmPmuDriver,
    NvU32 VddRail,
    NvU32* pMilliVolts)
{
    OdmTca6416GpioDriver *pTca6416GpioDriver = hOdmPmuDriver;

    if (VddRail >= OdmPmuTca6416Port_Num)
    {
        NvOdmOsPrintf("%s(): The VddRail %d is more than registered "
                        "rails\n", __func__, VddRail);
        return NV_FALSE;
    }

    if (pTca6416GpioDriver->RailInfo[VddRail].IsCurrentStateOn)
        *pMilliVolts = pTca6416GpioDriver->RailInfo[VddRail].OdmProps.OnVoltage_mV;
    else
        *pMilliVolts = pTca6416GpioDriver->RailInfo[VddRail].OdmProps.OffVoltage_mV;
    return NV_TRUE;
}

NvBool
OdmPmuTca6416GpioSetVoltage(
    NvOdmPmuDriverInfoHandle hOdmPmuDriver,
    NvU32 VddRail,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    OdmTca6416GpioDriver *pTca6416GpioDriver = hOdmPmuDriver;
    NvU32 Value;

    if (VddRail >= OdmPmuTca6416Port_Num)
    {
        NvOdmOsPrintf("%s(): The VddRail %d is more than registered "
                        "rails\n", __func__, VddRail);
        return NV_FALSE;
    }

    if (pTca6416GpioDriver->RailInfo[VddRail].OdmProps.IsOdmProtected)
        return NV_TRUE;

    *pSettleMicroSeconds = 0;
    NvOdmOsMutexLock(pTca6416GpioDriver->hPmuMutex);
    if (MilliVolts == NVODM_VOLTAGE_OFF)
    {
        if (!pTca6416GpioDriver->RailInfo[VddRail].UserCount)
            goto End;
        pTca6416GpioDriver->RailInfo[VddRail].UserCount--;
        if (!pTca6416GpioDriver->RailInfo[VddRail].UserCount)
        {
            NvOdmServicesPmuSetSocRailPowerState(
                pTca6416GpioDriver->hOdmPmuService,
                pTca6416GpioDriver->RailInfo[VddRail].SystemRailId, NV_FALSE);
            Value = (pTca6416GpioDriver->RailInfo[VddRail].OdmProps.IsActiveLow)? 1: 0;
            Tca6416SetValue(pTca6416GpioDriver,
                pTca6416GpioDriver->RailInfo[VddRail].GpioNr, Value);
            *pSettleMicroSeconds =
                 pTca6416GpioDriver->RailInfo[VddRail].OdmProps.Delay_uS;
        }
        goto End;
    }

    if (!pTca6416GpioDriver->RailInfo[VddRail].UserCount)
    {
        NvOdmServicesPmuSetSocRailPowerState(
            pTca6416GpioDriver->hOdmPmuService,
            pTca6416GpioDriver->RailInfo[VddRail].SystemRailId, NV_TRUE);
        Value = (pTca6416GpioDriver->RailInfo[VddRail].OdmProps.IsActiveLow)? 0: 1;
        Tca6416SetValue(pTca6416GpioDriver,
              pTca6416GpioDriver->RailInfo[VddRail].GpioNr, Value);
        *pSettleMicroSeconds = pTca6416GpioDriver->RailInfo[VddRail].OdmProps.Delay_uS;
    }
    pTca6416GpioDriver->RailInfo[VddRail].UserCount++;

End:
    NvOdmOsMutexUnlock(pTca6416GpioDriver->hPmuMutex);
    return NV_TRUE;
}
