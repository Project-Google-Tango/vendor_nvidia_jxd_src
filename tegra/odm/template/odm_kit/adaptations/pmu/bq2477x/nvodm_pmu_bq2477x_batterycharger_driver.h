/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_BQ2477X_CORE_DRIVER_H
#define INCLUDED_BQ2477X_CORE_DRIVER_H

#include "nvodm_pmu.h"
#include "nvodm_query.h"
#include "pmu/nvodm_battery_charging_driver.h"

typedef struct BQ2477xCoreDriverRec  *Bq2477xCoreDriverHandle;

#if defined(__cplusplus)
extern "C"
{
#endif

#define BQ2477X_CHARGE_OPTION_0_LSB             0x00
#define BQ2477X_CHARGE_OPTION_0_MSB             0x01
#define BQ2477X_CHARGE_CURRENT_LSB              0x0A
#define BQ2477X_MAX_CHARGE_VOLTAGE_LSB          0x0C
#define BQ2477X_MIN_SYS_VOLTAGE                 0x0E
#define BQ2477X_INPUT_CURRENT                   0x0F
#define BQ2477X_CHARGE_OPTION_POR_LSB           0x0E
#define BQ2477X_CHARGE_OPTION_POR_MSB           0x81
#define BQ2477X_CHARGE_CURRENT_SHIFT            6
#define BQ2477X_MAX_CHARGE_VOLTAGE_SHIFT        4
#define BQ2477X_MIN_SYS_VOLTAGE_SHIFT           8
#define BQ2477X_INPUT_CURRENT_SHIFT             6

/* Opens the charging driver
  * @param hDevice is pmu handle for the device
  * returns charging driver handle
  */
NvOdmBatChargingDriverInfoHandle
BQ2477xDriverOpen(NvOdmPmuDeviceHandle hDevice);

/* Closes charging driver and disables charging
  * @param hBchargingDriver Handle to charging driver
  */
void BQ2477xDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver);

/* Set current to charge
  * @param hBChargingDriverIndo  is charging info handle
  * @param charging path is the charging path usb or power etc
  * @param chargingCurrentLimitma is the charging current limit to charge
  *  @param ChargerType refers to USB charger type
  */
NvBool
BQ2477xBatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

#if defined(__cplusplus)
}
#endif

#endif
