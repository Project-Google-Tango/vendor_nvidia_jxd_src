/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcommon.h"
#include "nvassert.h"
#include "nvodm_pmu.h"
#include "nvodm_charging.h"
#include "stdarg.h"

#ifdef LPM_BATTERY_CHARGING
static NvBool s_BatteryConnected = NV_FALSE;
static NvBool s_ChargerConnected = NV_FALSE;
static NvBool s_IsCharging = NV_FALSE;
extern NvOdmUsbChargerType ChargerType;

extern NvBool NvOdmIsChargerConnected (void);
extern NvBool NvOdmStartCharging(NvOdmPmuDeviceHandle pPmu);
extern NvBool NvOdmStopCharging(NvOdmPmuDeviceHandle pPmu);
extern NvBool NvOdmBatteryInit(NvOdmPmuDeviceHandle pPmu);
extern NvU32 NvOdmBatteryGetCharge(NvOdmPmuDeviceHandle pPmu);
extern NvBool NvOdmBatteryGetChargingConfigData(NvOdmChargingConfigData *pData);

NvBool NvOdmChargingInterface(NvOdmChargingID id, ...)
{
    NvBool  ret = NV_TRUE;
    va_list ap;
    va_start( ap, id );

    switch (id)
    {
    case NvOdmChargingID_InitInterface:
        // Initialize charging interface (called once and first)
        // NvOdmPmuDeviceHandle *pmu (a handle for pmu)
        {
            NvOdmPmuDeviceHandle pPmu = va_arg(ap, NvOdmPmuDeviceHandle);
            s_BatteryConnected = NvOdmBatteryInit(pPmu);
        }
        break;

    case NvOdmChargingID_IsBatteryConnected:
        // Determines if a battery is connected
        // NvBool *Connected (set to TRUE if connected, FALSE otherwise)
        {
            NvBool *IsBatteryConnected = va_arg(ap, NvBool *);
            *IsBatteryConnected = s_BatteryConnected;
        }
        break;

    case NvOdmChargingID_GetBatteryCharge:
        // Get the current charge level of the battery
        // NvU32 *Charge (0..100%)
        {
            NvOdmPmuDeviceHandle pPmu = va_arg(ap, NvOdmPmuDeviceHandle);
            NvU32 *Charge = va_arg(ap, NvU32 *);
            *Charge = NvOdmBatteryGetCharge(pPmu);
        }
        break;
    case NvOdmChargingID_GetChargingConfigData:
        {
            NvOdmChargingConfigData *pData    = va_arg(ap, NvOdmChargingConfigData *);
            ret = NvOdmBatteryGetChargingConfigData(pData);
        }
        break;

    case NvOdmChargingID_IsChargerConnected:
        // Determines if a charger is connected
        // NvBool *Connected (set to TRUE if connected, FALSE otherwise)
        {
            /* NOTE: Charger connection status is done here instead of in the init section
                     to keep the init section from taking to long since this is only called
                     if there is a battery and if the battery charge is too low */
            NvBool *IsChargerConnected = va_arg(ap, NvBool *);
            *IsChargerConnected = s_ChargerConnected = NvOdmIsChargerConnected();
        }
        break;

    case NvOdmChargingID_GetChargerType:
        // Get connected charger type
        {
            NvOdmUsbChargerType *Type= va_arg(ap, NvOdmUsbChargerType *);
            *Type = ChargerType;
        }
        break;

    case NvOdmChargingID_StartCharging:
        // Turns the charging current on
        // NvOdmPmuDeviceHandle *pmu (a handle for pmu)
        {
            NvOdmPmuDeviceHandle pPmu;
            if (!s_ChargerConnected) break;   // Sanity check

            pPmu = va_arg(ap, NvOdmPmuDeviceHandle);
            ret = NvOdmStartCharging(pPmu);
        }
        s_IsCharging = NV_TRUE;
        break;

    case NvOdmChargingID_StopCharging:
        // Turns the charging current off
        // NvOdmPmuDeviceHandle *pmu (a handle for pmu)
        {
            NvOdmPmuDeviceHandle pPmu;
            if (!s_ChargerConnected) break;   // Sanity check

            pPmu = va_arg(ap, NvOdmPmuDeviceHandle);
            ret = NvOdmStopCharging(pPmu);
        }
        s_IsCharging = NV_FALSE;
        break;

    case NvOdmChargingID_IsCharging:
        ret = s_IsCharging;
        break;

    default:
        ret = NV_FALSE;
    }

    va_end( ap );
    return ret;
}
#endif
