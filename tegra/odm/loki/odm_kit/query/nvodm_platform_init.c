/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_query.h"
#include "nvrm_module.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "pmu/boards/loki/nvodm_pmu_loki_supply_info_table.h"

static NvBool NvOdmVddCoreInit(void)
{
     NvOdmPmuBoardInfo PmuInfo;

     NvBool Status;
     NvU32 SettlingTime;
     NvU32 VddId;
     static NvOdmServicesPmuHandle hPmu = NULL;

     Status = NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_PmuBoard,
                    &PmuInfo, sizeof(PmuInfo));
     if (!Status)
          return NV_FALSE;

     if (!PmuInfo.core_edp_mv)
          return NV_TRUE;

     VddId = LokiPowerRails_VDD_CORE;

     if (!hPmu)
          hPmu = NvOdmServicesPmuOpen();

     if (!hPmu)
     {
          NvOdmOsDebugPrintf("PMU Device can not open\n");
          return NV_FALSE;
     }

     NvOdmOsDebugPrintf("Setting VddId=%d to %d mV\n", VddId, PmuInfo.core_edp_mv);
     NvOdmServicesPmuSetVoltage(hPmu, VddId, PmuInfo.core_edp_mv, &SettlingTime);
     if (SettlingTime)
         NvOdmOsWaitUS(SettlingTime);

     return NV_TRUE;
}

static NvBool NvOdmPlatformConfigure_BlStart(void)
{
    return NV_TRUE;
}

static NvBool NvOdmPlatformConfigure_OsLoad(void)
{
    NvOdmVddCoreInit();
    return NV_TRUE;
}

NvBool NvOdmPlatformConfigure(int Config)
{
    if (Config == NvOdmPlatformConfig_OsLoad)
         return NvOdmPlatformConfigure_OsLoad();

    if (Config == NvOdmPlatformConfig_BlStart)
         return NvOdmPlatformConfigure_BlStart();

    return NV_TRUE;
}

