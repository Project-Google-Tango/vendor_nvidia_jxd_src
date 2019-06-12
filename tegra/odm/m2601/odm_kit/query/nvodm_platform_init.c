/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_query.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvrm_gpio.h"
#include "nvrm_pmu.h"

NvBool NvOdmEnableCpuPowerRail(void)
{
    NvOdmServicesPmuHandle hPmu = NULL;
    NvU32 SettlingTime = 0;
    NvU8 i;
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;
    NvOdmServicesPmuVddRailCapabilities RailCaps;

    pConnectivity = NvOdmPeripheralGetGuid(NV_VDD_CPU_ODM_ID);

    hPmu = NvOdmServicesPmuOpen();
    if (!hPmu)
        goto fail;

    for (i = 0; i < pConnectivity->NumAddress; i++)
    {
        if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd)
        {
            NvOdmServicesPmuGetCapabilities(hPmu,
                pConnectivity->AddressList[i].Address, &RailCaps);

            NvOdmServicesPmuSetVoltage(hPmu,
                pConnectivity->AddressList[i].Address,
                RailCaps.requestMilliVolts, &SettlingTime);
        }
    }
    if (SettlingTime)
        NvOdmOsWaitUS(SettlingTime);

    NvOdmServicesPmuClose(hPmu);
    return NV_TRUE;
fail:
    NvOdmOsDebugPrintf("%s: failed!\n", __func__);
    return NV_FALSE;
}

static void DoM2601SpecificInitialization(void)
{

    NvRmDeviceHandle hRm;

/* Rely on EEPROM for now */
   NvU64 EnablePowerRailGuidList[] = {NV_VDD_PEX_ODM_ID };

    NvU32 NumEnablePowerRailGuid = NV_ARRAY_SIZE(EnablePowerRailGuidList);
    NvU32 i, j;

    if (NvRmOpen(&hRm, 0) != NvError_Success)
    {
        NvOsDebugPrintf("%s: NvRmOpen failed\n",__func__);
        return;
    }

    // Turn ON the power rails needed  by the kernel.
    for (i = 0; i < NumEnablePowerRailGuid; i++)
    {
        const NvOdmPeripheralConnectivity *pConnectivity = NULL;
        NvRmPmuVddRailCapabilities RailCaps;
        pConnectivity = NvOdmPeripheralGetGuid(EnablePowerRailGuidList[i]);


        if (pConnectivity != NULL)
        {
            for (j = 0; j < pConnectivity->NumAddress; j++)
            {
                // Search for the vdd rail entry
                if (pConnectivity->AddressList[j].Interface == NvOdmIoModule_Vdd)
                {
                    NvRmPmuGetCapabilities(hRm,
                            pConnectivity->AddressList[j].Address, &RailCaps);
                    NvRmPmuSetVoltage(hRm, pConnectivity->AddressList[j].Address,
                             RailCaps.requestMilliVolts, NULL);
                }
            }
        }
    }
}


static NvBool NvOdmPlatformConfigure_BlStart(void)
{
        return NV_TRUE;
}

static NvBool NvOdmPlatformConfigure_OsLoad(void)
{
    DoM2601SpecificInitialization();
    return NV_TRUE;
}

NvBool NvOdmPlatformConfigure(int Config)
{
    if (Config == NvOdmPlatformConfig_OsLoad)
         return NvOdmPlatformConfigure_OsLoad();
    else if (Config == NvOdmPlatformConfig_BlStart)
        return NvOdmPlatformConfigure_BlStart();

    return NV_TRUE;
}

