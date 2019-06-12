/*
 * Copyright (c) 2006-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Power Management Unit Interface</b>
 *
 * @b Description: Defines the ODM interface for NVIDIA PMU devices.
 *
 */

#ifndef INCLUDED_NVODM_PMU_H
#define INCLUDED_NVODM_PMU_H

#include "nvcommon.h"
#include "nvodm_query.h"
#include "datetime.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nvodm_pmu Power Management Unit Adaptation Interface
 *
 * This is the power management unit (PMU) ODM adaptation interface, which
 * handles the abstraction of external power management devices.
 * For NVIDIA&reg; Driver Development Kit (DDK) clients, PMU is a
 * set of voltages used to provide power to the SoC or to monitor low battery
 * conditions. The API allows DDK clients to determine whether the
 * particular voltage is supported by the ODM platform, retrieve the
 * capabilities of PMU, and get/set voltage levels at runtime.
 * On systems without a power management device, APIs should be dummy implemented.
 *
 * All voltage rails are referenced using ODM-assigned unsigned integers. ODMs
 * may select any convention for assigning these values; however, the values
 * accepted as input parameters by the PMU ODM adaptation interface must
 * match the values stored in the address field of ::NvOdmIoModule_Vdd buses
 * defined in the Peripheral Discovery ODM adaptation.
 *
 * @ingroup nvodm_adaptation
 * @{
 */


/**
 * Defines an opaque handle that exists for each PMU device in the
 * system, each of which is defined by the customer implementation.
 */
typedef struct NvOdmPmuDeviceRec *NvOdmPmuDeviceHandle;

/**
 * Combines information for the particular PMU Vdd rail.
 */
typedef struct NvOdmPmuVddRailCapabilitiesRec
{
    /// Specifies ODM protection attribute; if \c NV_TRUE PMU hardware
    ///  or ODM Kit would protect this voltage from being changed by NvDdk client.
    NvBool OdmProtected;

    /// Specifies the minimum voltage level in mV.
    NvU32 MinMilliVolts;

    /// Specifies the step voltage level in mV.
    NvU32 StepMilliVolts;

    /// Specifies the maximum voltage level in mV.
    NvU32 MaxMilliVolts;

    /// Specifies the request voltage level in mV.
    NvU32 requestMilliVolts;

    /// Specifies the maximum current it can draw in mA.
    NvU32 MaxCurrentMilliAmp;
} NvOdmPmuVddRailCapabilities;

/// Special level to indicate voltage plane is turned off.
#define ODM_VOLTAGE_OFF (0UL)

/// Special level to enable voltage plane on/off control
///  by the external signal (e.g., low power request from SoC).
#define ODM_VOLTAGE_ENABLE_EXT_ONOFF (0xFFFFFFFFUL)

/// Special level to disable voltage plane on/off control
///  by the external signal (e.g., low power request from SoC).
#define ODM_VOLTAGE_DISABLE_EXT_ONOFF (0xFFFFFFFEUL)

/**
 * Gets capabilities for the specified PMU voltage.
 *
 * @param vddId The ODM-defined PMU rail ID.
 * @param pCapabilities A pointer to the targeted
 *  capabilities returned by the ODM.
 *
 */
void
NvOdmPmuGetCapabilities(
    NvU32 vddId,
    NvOdmPmuVddRailCapabilities* pCapabilities);


/**
 * Gets current voltage level for the specified PMU voltage.
 *
 * @param hDevice A handle to the PMU.
 * @param vddId The ODM-defined PMU rail ID.
 * @param pMilliVolts A pointer to the voltage level returned
 *  by the ODM.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuGetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddId,
    NvU32* pMilliVolts);


/**
 * Sets new voltage level for the specified PMU voltage.
 *
 * @param hDevice A handle to the PMU.
 * @param vddId The ODM-defined PMU rail ID.
 * @param MilliVolts The new voltage level to be set in millivolts (mV).
 * - Set to ::ODM_VOLTAGE_OFF to turn off the target voltage.
 * - Set to ::ODM_VOLTAGE_ENABLE_EXT_ONOFF to enable external control of
 *   target voltage.
 * - Set to ::ODM_VOLTAGE_DISABLE_EXT_ONOFF to disable external control of
 *   target voltage.
 * @param pSettleMicroSeconds A pointer to the settling time in microseconds (uS),
 *  which is the time for supply voltage to settle after this function
 *  returns; this may or may not include PMU control interface transaction time,
 *  depending on the ODM implementation. If NULL this parameter is ignored, and the
 *  function must return only after the supply voltage has settled.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuSetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds);

/**
 * Gets a handle to the PMU in the system.
 *
 * @param hDevice A pointer to the handle of the PMU.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuDeviceOpen( NvOdmPmuDeviceHandle *hDevice );


/**
 *  Releases the PMU handle.
 *
 * @param hDevice The PMU handle to be released. If
 *     NULL, this API has no effect.
 */
void NvOdmPmuDeviceClose(NvOdmPmuDeviceHandle hDevice);

/**
 *  Suspends the PMU. This is usually done while entering Deep Sleep (LP0).
 */
void NvOdmPmuDeviceSuspend(void);

/**
 *  Shuts down the I2C.
 */
void NvOdmPmuI2cShutDown(void);

/**
 * Defines AC status.
 */
typedef enum
{
    /// Specifies AC is offline.
    NvOdmPmuAcLine_Offline,

    /// Specifies AC is online.
    NvOdmPmuAcLine_Online,

    /// Specifies backup power.
    NvOdmPmuAcLine_BackupPower,

    NvOdmPmuAcLine_Num,
    NvOdmPmuAcLine_Force32 = 0x7FFFFFFF
}NvOdmPmuAcLineStatus;

/** @name Battery Status Defines */
/*@{*/

#define NVODM_BATTERY_STATUS_HIGH               0x01
#define NVODM_BATTERY_STATUS_LOW                0x02
#define NVODM_BATTERY_STATUS_CRITICAL           0x04
#define NVODM_BATTERY_STATUS_CHARGING           0x08
#define NVODM_BATTERY_STATUS_FULL_BATTERY       0x10
#define NVODM_BATTERY_STATUS_NO_BATTERY         0x80
#define NVODM_BATTERY_STATUS_UNKNOWN            0xFF

/*@}*/
/** @name Battery Data Defines */
/*@{*/
#define NVODM_BATTERY_DATA_UNKNOWN              0x7FFFFFFF

/*@}*/
/**
 * Defines battery instances.
 */
typedef enum
{
    /// Specifies main battery.
    NvOdmPmuBatteryInst_Main,

    /// Specifies backup battery.
    NvOdmPmuBatteryInst_Backup,

    NvOdmPmuBatteryInst_Num,
    NvOdmPmuBatteryInst_Force32 = 0x7FFFFFFF

}NvOdmPmuBatteryInstance;

/*@}*/
/**
 * Defines PowerOn Sources.
 */
typedef enum
{
	unknown = 0,
    NvOdmPmuPowerKey = 1,
    NvOdmPmuRTCAlarm,
    NvOdmUsbHotPlug,
    NVOdmPmuUndefined = 0xFF
}NvOdmPmuPowerOnSourceType;

/**
 * Defines Reboot Sources.
 */
typedef enum
{
    NvOdmRebootPowerKey = 0,
    NvOdmRebootAdbReboot,
    NvOdmRebootRecovery,
    NvOdmRebootRebootKey,
    NVOdmRebootUndefined = 0xFF
}NvOdmRebootSourceType;


/**
 * Defines battery data.
 */
typedef struct NvOdmPmuBatteryDataRec
{
    /// Specifies battery life percent.
    NvU32 batteryLifePercent;

    /// Specifies battery life time.
    NvU32 batteryLifeTime;

    /// Specifies voltage.
    NvU32 batteryVoltage;

    /// Specifies battery current.
    NvS32 batteryCurrent;

    /// Specifies battery average current.
    NvS32 batteryAverageCurrent;

    /// Specifies battery interval.
    NvU32 batteryAverageInterval;

    /// Specifies the mAH consumed.
    NvU32 batteryMahConsumed;

    /// Specifies battery temperature.
    NvU32 batteryTemperature;

    /// Specifies the state of the battery charge [0 - 100]%.
    NvU32 batterySoc;

}NvOdmPmuBatteryData;

/**
 * Defines battery chemistry.
 */
typedef enum
{
    /// Specifies an alkaline battery.
    NvOdmPmuBatteryChemistry_Alkaline,

    /// Specifies a nickel-cadmium (NiCd) battery.
    NvOdmPmuBatteryChemistry_NICD,

    /// Specifies a nickel-metal hydride (NiMH) battery.
    NvOdmPmuBatteryChemistry_NIMH,

    /// Specifies a lithium-ion (Li-ion) battery.
    NvOdmPmuBatteryChemistry_LION,

    /// Specifies a lithium-ion polymer (Li-poly) battery.
    NvOdmPmuBatteryChemistry_LIPOLY,

    /// Specifies a zinc-air battery.
    NvOdmPmuBatteryChemistry_XINCAIR,

    NvOdmPmuBatteryChemistry_Num,
    NvOdmPmuBatteryChemistry_Force32 = 0x7FFFFFFF
}NvOdmPmuBatteryChemistry;

/**
 * Gets the AC line status.
 *
 * @param hDevice A handle to the PMU.
 * @param pStatus A pointer to the AC line
 *  status returned by the ODM.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuGetAcLineStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuAcLineStatus *pStatus);


/**
 * Gets the battery status.
 *
 * @param hDevice A handle to the PMU.
 * @param batteryInst The battery type.
 * @param pStatus A pointer to the battery
 *  status returned by the ODM.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuGetBatteryStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU8 *pStatus);

/**
 * Gets the battery data.
 *
 * @param hDevice A handle to the PMU.
 * @param batteryInst The battery type.
 * @param pData A pointer to the battery
 *  data returned by the ODM.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuGetBatteryData(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryData *pData);


/**
 * Gets the battery full life time.
 *
 * @param hDevice A handle to the PMU.
 * @param batteryInst The battery type.
 * @param pLifeTime A pointer to the battery
 *  full life time returned by the ODM.
 *
 */
void
NvOdmPmuGetBatteryFullLifeTime(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU32 *pLifeTime);


/**
 * Gets the battery chemistry.
 *
 * @param hDevice A handle to the PMU.
 * @param batteryInst The battery type.
 * @param pChemistry A pointer to the battery
 *  chemistry returned by the ODM.
 *
 */
void
NvOdmPmuGetBatteryChemistry(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryChemistry *pChemistry);


/**
* Defines the charging path.
*/
typedef enum
{
    /// Specifies external wall plug charger.
    NvOdmPmuChargingPath_MainPlug,

    /// Specifies external USB bus charger.
    NvOdmPmuChargingPath_UsbBus,

    NvOdmPmuChargingPath_Num,
    /// Ignore. Forces compilers to make 32-bit enums.
    NvOdmPmuChargingPath_Force32 = 0x7FFFFFFF

}NvOdmPmuChargingPath;

/**
* Defines the POWER key event.
*/
typedef enum
{
    /// Indicates the POWER key was pressed.
    NvOdmPowerKey_Pressed,

    /// Indicates the POWER key was pressed.
    NvOdmPowerKey_Released,

    /// Ignore. Forces compilers to make 32-bit enums.
    NvOdmPowerKey_Force32 = 0x7FFFFFFF
}NvOdmPowerKeyEvents;

typedef void (*NvOdmPowerKeyHandler)(void );

/// Special level to indicate dumb charger current limit.
#define NVODM_DUMB_CHARGER_LIMIT (0xFFFFFFFFUL)

/// Special level to indicate USB Host mode current limit.
#define NVODM_USB_HOST_MODE_LIMIT (0x80000000UL)

/**
* Sets the charging current limit.
*
* @param hDevice A handle to the PMU.
* @param chargingPath The charging path.
* @param chargingCurrentLimitMa The charging current limit in mA.
* @param ChargerType The charger type.
* @return NV_TRUE if successful, or NV_FALSE otherwise.
*/
NvBool
NvOdmPmuSetChargingCurrent(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuChargingPath chargingPath,
    NvU32 chargingCurrentLimitMa,
    NvOdmUsbChargerType ChargerType);

/**
 * Gets the value of date and time as read from the RTC registers.
 *
 * @param hDevice A handle to the PMU.
 * @param time Holds the value of date-time read from RTC.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuGetDate(NvOdmPmuDeviceHandle hDevice, DateTime* time);

/**
 * Sets the RTC alarn to the date and time specified.
 *
 * @param hDevice A handle to the PMU.
 * @param time Specifies the value of date-time for which the alarm will be set.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuSetAlarm (NvOdmPmuDeviceHandle hDevice, DateTime time);


/**
 * Handles the PMU interrupt.
 *
 * @param hDevice A handle to the PMU.
 */
void NvOdmPmuInterruptHandler( NvOdmPmuDeviceHandle  hDevice);

/**
 * Gets the count in seconds of the current external RTC (in PMU).
 *
 * @param hDevice A handle to the PMU.
 * @param Count A pointer to where to return the current counter in sec.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuReadRtc(
    NvOdmPmuDeviceHandle hDevice,
    NvU32* Count);

/**
 * Updates current RTC value.
 *
 * @param hDevice A handle to the PMU.
 * @param Count data with which to update the current counter in sec.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuWriteRtc(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 Count);

/**
 * Returns whether or not the RTC is initialized.
 *
 * @param hDevice A handle to the PMU.
 * @return NV_TRUE if initialized, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuIsRtcInitialized(
    NvOdmPmuDeviceHandle hDevice);

/**
* Defines the reset reasons caused by PMIC.
*/
typedef enum
{
    /// Indicates invalid reason is reported.
    /// This option is returned when valid bits are not
    /// returned from PMIC.
    NvOdmPmuResetReason_Invalid,

    /// Indicates PMIC did not trigger reset.
    /// This means that the reset was triggered by our chip.
    NvOdmPmuResetReason_NoReason,

    /// Indicates power on long press key reset.
    /// Pressing power on for a certain period of time caused
    /// PMIC to shutdown the rail.
    NvOdmPmuResetReason_PwrOnLPK,

    /// Indicates power down reset.
    /// Pressng power down caused PMIC to shutdown the rail.
    NvOdmPmuResetReason_PwrDown,

    /// Indicates watch dog reset.
    /// PMIC watch dog timeout caused to shutdown the rail.
    NvOdmPmuResetReason_WTD,

    /// Indicates thermal sensor shutdown reset.
    /// PMIC reached thermal limit where it has to shutdown.
    NvOdmPmuResetReason_Thermal,

    /// Indicates reset signal was sent.
    /// Reset signal, normally caused by reset button, triggered
    /// PMIC to shutdown the rail.
    NvOdmPmuResetReason_ResetSig,

    /// Indicates software reset.
    /// Software command triggered PMIC to shutdown.
    NvOdmPmuResetReason_SwReset,

    /// Indicates battery low reset.
    /// VSYS is too low to sustain the machine powered on.
    NvOdmPmuResetReason_BatLow,

    /// Indicates gpadc_shutdown reset.
    /// General-purpose analog-to-digital converter detects a
    /// shutdown request on PMIC.
    NvOdmPmuResetReason_GPADC,

    /// Indicates number of reset reasons.
    NvOdmPmuResetReason_Num,

    /// Ignore. Forces compilers to make 32-bit enums.
    /// This is for error handling purposes.
    NvOdmPmuResetReason_Force32 = 0x7FFFFFFF

}NvOdmPmuResetReason;

/**
 * Returns whether or not reset reason is successfully read.
 *
 * @param hDevice A handle to the PMU.
 * @param rst_reason A pointer to where to return the reset reason.
 * @return NV_TRUE if read, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuReadRstReason(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuResetReason *rst_reason);

/**
 * Returns whether or not RTC Alarm is successfully cleared.
 *
 * @param hDevice A handle to the PMU.
 * @return NV_TRUE if read, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuClearAlarm(
    NvOdmPmuDeviceHandle hDevice);

/**
 * Enables PMIC Interrupts for charging -
 * USB hotplug, Power Key press and RTC Alarm events
 *
 * @param hDevice A handle to the PMU.
 */
void
NvOdmPmuEnableInterrupts(
    NvOdmPmuDeviceHandle hDevice);

/**
 * Disables PMIC Interrupts for charging -
 * USB hotplug, Power Key press and RTC Alarm events
 *
 * @param hDevice A handle to the PMU.
 */
void
NvOdmPmuDisableInterrupts(
    NvOdmPmuDeviceHandle hDevice);

/**
 * Query the Power Source
 *
 * @param hDevice A handle to the PMU.
 * @return Power source, one of NvOdmPmuPowerOnSourceType
 */
NvU8
NvOdmPmuGetPowerOnSource(
    NvOdmPmuDeviceHandle hDevice);

/**
 * Powers off the PMU.
 *
 * @param hDevice A handle to the PMU.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmPmuPowerOff(
    NvOdmPmuDeviceHandle hDevice);

/**
 * Enable the WDT.
 *
 * @param hDevice A handle to the PMU.
 */
void
NvOdmPmuEnableWdt(
    NvOdmPmuDeviceHandle hDevice);

/**
 * Disable the WDT.
 *
 * @param hDevice A handle to the PMU.
 */
void
NvOdmPmuDisableWdt(
    NvOdmPmuDeviceHandle hDevice);

/**
 * Registers the POWER key handler and calls on power key event.
 *
 * @param Callback function to be called.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmRegisterPowerKey(
    NvOdmPowerKeyHandler Callback
    );

/**
 * Handles calls on power key events.
 *
 * @param Callback function to be called.
 * @param Events - Events to be handled
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvOdmHandlePowerKeyEvent(
    NvOdmPowerKeyEvents Events
    );

/**
 * Sets interrupts required for charging.
 *
 * @param hDevice A handle to the PMU.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
void NvOdmPmuSetChargingInterrupts(NvOdmPmuDeviceHandle hDevice);

/**
 * Unsets interrupts used for charging.
 *
 * @param hDevice A handle to the PMU.
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
void NvOdmPmuUnSetChargingInterrupts(NvOdmPmuDeviceHandle hDevice);

#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_PMU_H
