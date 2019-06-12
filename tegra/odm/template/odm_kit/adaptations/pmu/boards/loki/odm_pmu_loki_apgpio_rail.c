/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */
 /**
  * @file
  * <b>NVIDIA Tegra ODM Kit: Loki pmu rail driver using apgpio pmu driver</b>
  *
  * @b Description: Implements the pmu controls api based on AP GPIO pmu driver.
  */

#include "nvodm_pmu.h"
#include "nvodm_pmu_loki.h"
#include "loki_pmu.h"
#include "nvodm_pmu_loki_supply_info_table.h"
#include "apgpio/nvodm_pmu_apgpio.h"
#include "nvodm_query_gpio.h"

typedef enum
{
    LokiAPGpioRails_Invalid = 0,
    // AP GPIO (GPIO_W3)
    LokiAPGpioRails_EN_3V3_FUSE,
    LokiAPGpioRails_EN_VDDIO_VID,

    LokiAPGpioRails_Num,
    LokiAPGpioRails_Force32 = 0x7FFFFFFF,
} LokiAPGpioRails;

static NvBool Enable(NvOdmServicesGpioHandle hOdmGpio, NvOdmGpioPinHandle hPinGpio,
                    NvU32 GpioPort, NvU32 GpioPin)
{
    NvOdmGpioConfig(hOdmGpio, hPinGpio, NvOdmGpioPinMode_InputData);
    NvOdmGpioConfig(hOdmGpio, hPinGpio, NvOdmGpioPinMode_Tristate);
    return NV_TRUE;
}
static NvBool Disable(NvOdmServicesGpioHandle hOdmGpio, NvOdmGpioPinHandle hPinGpio,
                    NvU32 GpioPort, NvU32 GpioPin)
{
     NvOdmGpioSetState(hOdmGpio, hPinGpio, NvOdmGpioPinActiveState_Low);
     NvOdmGpioConfig(hOdmGpio, hPinGpio, NvOdmGpioPinMode_Output);
     return NV_TRUE;
}

/* RailId, GpioPort,pioPin,
   IsActiveLow, IsInitEnable, OnValue, OffValue,
   Enable, Disable, IsOdmProt, Delay
*/
#define PORT(x) ((x) - 'a')
static ApGpioInfo s_LokiAPiGpioInfo[LokiAPGpioRails_Num] =
{
    {   // Invalid
        LokiAPGpioRails_Invalid, LokiPowerRails_Invalid, 0, 0,
        NV_FALSE, NV_FALSE, 0, 0,
        NULL, NULL,
        NV_FALSE, 0,
    },

    {   // GMI_CS2 K.03
        LokiAPGpioRails_EN_3V3_FUSE, LokiPowerRails_Invalid, PORT('w'), 3,
        NV_FALSE, NV_FALSE, 3300, 0,
        Enable, Disable,
        NV_FALSE, 10,
    },
    {   // GMI_CLK K.01
        LokiAPGpioRails_EN_VDDIO_VID, LokiPowerRails_Invalid, PORT('k'), 1,
        NV_FALSE, NV_TRUE, 3300, 0,
        NULL, NULL,
        NV_FALSE, 10,
    },
};

static void ApGpioGetCapsWrap(void *pDriverData, NvU32 vddRail,
        NvOdmPmuVddRailCapabilities* pCapabilities)
{
        OdmPmuApGpioGetCapabilities((NvOdmPmuApGpioDriverHandle)pDriverData, vddRail,
                     pCapabilities);
}

static NvBool ApGpioGetVoltageWrap(void *pDriverData, NvU32 vddRail, NvU32* pMilliVolts)
{
        return OdmPmuApGpioGetVoltage((NvOdmPmuApGpioDriverHandle)pDriverData, vddRail, pMilliVolts);
}

static NvBool ApGpioSetVoltageWrap(void *pDriverData, NvU32 vddRail, NvU32 MilliVolts,
                NvU32* pSettleMicroSeconds)
{
        return OdmPmuApGpioSetVoltage((NvOdmPmuApGpioDriverHandle)pDriverData, vddRail, MilliVolts,
                        pSettleMicroSeconds);
}

static PmuChipDriverOps s_PmuApGpioDriverOps =
{ApGpioGetCapsWrap, ApGpioGetVoltageWrap, ApGpioSetVoltageWrap};

static void InitGpioRailMap(NvU32 *pRailMap)
{
  /* Will fill on need basis */
}

void *LokiPmu_GetApGpioRailMap(NvOdmPmuDeviceHandle hPmuDevice,
    PmuChipDriverOpsHandle *hDriverOps, NvU32 *pRailMap, NvOdmBoardInfo *pProcBoard,
    NvOdmBoardInfo *pPmuBoard)
{
    NvOdmPmuApGpioDriverHandle hApGpioDriver = NULL;
    NV_ASSERT(hPmuDevice);
    NV_ASSERT(pProcBoard);
    NV_ASSERT(pPmuBoard);

    if (!hPmuDevice || !pProcBoard || !pPmuBoard)
        return NULL;

    hApGpioDriver = OdmPmuApGpioOpen(hPmuDevice, s_LokiAPiGpioInfo,
                                    LokiAPGpioRails_Num);
    if (!hApGpioDriver)
    {
        NvOdmOsPrintf("%s(): Fails in opening GpioPmu Handle\n", __func__);
        return NULL;
    }
    InitGpioRailMap(pRailMap);
    *hDriverOps = &s_PmuApGpioDriverOps;

    return hApGpioDriver;
}
