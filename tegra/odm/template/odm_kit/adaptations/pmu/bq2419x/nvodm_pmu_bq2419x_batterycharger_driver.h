/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_BQ2419x_CORE_DRIVER_H
#define INCLUDED_BQ2419x_CORE_DRIVER_H

#include "nvodm_pmu.h"
#include "nvodm_query.h"
#include "pmu/nvodm_battery_charging_driver.h"

typedef struct BQ2419xCoreDriverRec  *Bq2419xCoreDriverHandle;

#if defined(__cplusplus)
extern "C"
{
#endif

#define BQ2419X_INPUT_SRC_REG   0x00
#define BQ2419X_PWR_ON_REG      0x01
#define BQ2419X_CHRG_CTRL_REG   0x02
#define BQ2419X_CHRG_TERM_REG   0x03
#define BQ2419X_VOLT_CTRL_REG   0x04
#define BQ2419X_TIME_CTRL_REG   0x05
#define BQ2419X_THERM_REG       0x06
#define BQ2419X_MISC_OPER_REG   0x07
#define BQ2419X_SYS_STAT_REG    0x08
#define BQ2419X_FAULT_REG       0x09
#define BQ2419X_REVISION_REG    0x0a

#define BQ24190_IC_VER          0x40
#define BQ24192_IC_VER          0x28
#define BQ24192i_IC_VER         0x18
#define ENABLE_CHARGE_MASK      0x30
#define ENABLE_CHARGE           0x10
#define DISABLE_CHARGE           0x00

#define INPUT_CURRENT_LIMIT_MASK 0x07

#define BQ2419X_REG0                    0x0
#define BQ2419X_EN_HIZ                  BIT(7)

#define BQ2419X_OTG                     0x1
#define BQ2419X_OTG_ENABLE_MASK         0x30
#define BQ2419X_OTG_ENABLE              0x20

#define BQ2419X_WD                      0x5
#define BQ2419X_WD_MASK                 0x30
#define BQ2419X_WD_DISABLE              0x0

#define CHGR_CNTRL                      0xBB
#define WIRELESS_DIS_MASK               BIT(2)
#define WIRELESS_DIS                    0x04

#define USB_CHGCTL1                     0x00
#define USB_SUSPEND_MASK                0x0C
#define USB_SUSPEND                     0x08

#define DISABLE_HIZ             0x80
#define INPUT_CURRENT_LIMIT_MASK 0x07

#define INPUT_VOLTAGE_LIMIT_MASK 0x78
#define NV_VOLTAGE_LIMIT 0x40
#define DEFAULT_VOLTAGE_LIMIT 0x30

#define WD_RESET_BIT 6
#define SAFETY_TIMER_BIT 4
#define SAFETY_TIMER_SET 3
#define ENABLE_TIMER_BIT 3

#define INPUT_CURRENT_CTRL_MASK 0xFC
#define MAX_CURRENT  0x6c

#define BQ2419X_REG0                    0x0
#define BQ2419X_EN_HIZ                  BIT(7)

#define BQ2419X_OTG                     0x1
#define BQ2419X_OTG_ENABLE_MASK         0x30
#define BQ2419X_OTG_ENABLE              0x20

#define BQ2419X_WD                      0x5
#define BQ2419X_WD_MASK                 0x30
#define BQ2419X_WD_DISABLE              0x0

#define BQ2419x_CHARGE_COMPLETE      0x3



/* Opens the charging driver
  * @param hDevice is pmu handle for the device
  * returns charging driver handle
  */
NvOdmBatChargingDriverInfoHandle
BQ2419xDriverOpen(NvOdmPmuDeviceHandle hDevice);

/* Closes charging driver and disables charging
  * @param hBchargingDriver Handle to charging driver
  */
void BQ2419xDriverClose(NvOdmBatChargingDriverInfoHandle hBChargingDriver);

/* Set current to charge
  * @param hBChargingDriverIndo  is charging info handle
  * @param charging path is the charging path usb or power etc
  * @param chargingCurrentLimitma is the charging current limit to charge
  *  @param ChargerType refers to USB charger type
  */
NvBool
BQ2419xBatChargingDriverSetCurrent(
    NvOdmBatChargingDriverInfoHandle hBChargingDriverInfo,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

/* Check whether battery is fully charged
  * @param hBChargingDriverIndo  is charging info handle
  * returns True if fully charged else False
  */

NvBool BQ2419xCheckChargeComplete (NvOdmBatChargingDriverInfoHandle hBChargingDriver);


#if defined(__cplusplus)
}
#endif

#endif
