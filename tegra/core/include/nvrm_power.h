/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_POWER_H
#define INCLUDED_NVRM_POWER_H

#include "nvrm_module.h"
#include "nvrm_init.h"
#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           Resource Manager %Power Control APIs</b>
 *
 * @b Description: Declares module power management APIs.
 *
 */

/** @defgroup nvrm_power RM Power Control Services
 *
 * This file defines module power management APIs, which give DDK modules
 * access to SOC power management facilities. There are two groups:
 *
 * @par SOC-wide Resources Include
 *
 * - power planes
 * - processor clocks (CPU, AVP)
 * - memory clocks (MC, EMC)
 * - system bus clocks (PPSB, AHB, APB, and VDE video-pipe)
 *
 * @par Module Clocks Include
 *
 * - All individual module clocks other than specified above.
 *
 * The facilities in the first group are controlled by the RM Central Power
 * manager and Dynamic Frequency Scaling manager; DDK modules provide input
 * for RM policy decisions. The clocks in the second group are controlled by
 * the DDK modules themselves, while RM checks for (and tries to resolve)
 * possible conflicts with H/W clock tree limitations.
 *
 * <h3>Guidelines for DDK Driver Implementation</h3>
 *
 * A. During initialization the driver should:
 * - Register itself as power client via NvRmPowerRegister().
 * - Enable power to the module via NvRmPowerVoltageControl().
 * - Enable module clock via NvRmPowerModuleClockControl().
 *
 * B. During normal operation the driver can:
 * - Manage module clock (enable/disable) via \c NvRmPowerModuleClockControl.
 * - Configure the modules clocks via NvRmPowerModuleClockConfig().
 * - Request a gradual frequency increase for the clock in the first group via
 *   NvRmPowerStarvationHint(). The trigger condition for the request is module
 *   specific, and usually based on some type of "watermark" established by the
 *   driver. The starvation request shall be cancelled if trigger condition is
 *   resolved; the same API \c NvRmPowerStarvationHint is used for this purpose.
 * - Request an immediate frequency increase for the clock in the first group
 *   via NvRmPowerBusyHint(), usually, when the driver anticipates change in
 *   load.
 *
 * C. When entering low power Suspend state, after all other tasks
 *   (e.g., saving context) are completed, the driver should:
 * - Disable module clock via \c NvRmPowerModuleClockControl.
 * - Disable module power via \c NvRmPowerVoltageControl; as described in the
 *   voltage control header "Disable module power" means the driver is ready
 *   for module power down, which may happen at any time at RM discretion.
 * - (Optional) If the driver utilizes RM signaling for wake up, it may wait
 *   on wake semaphore, which was provided to RM during registration.
 *
 * D. When resuming from Suspend state, before any access to module H/W, the
 *   driver should:
 * - Enable power to the module via \c NvRmPowerVoltageControl.
 * - Enable module clock via \c NvRmPowerModuleClockControl.
 * - Check via NvRmPowerGetEvent() for SOC power management events happened
 *   while the driver was suspended and follow up with module specific resume
 *   procedure.
 *
 * E. During de-initialization the driver should:
 * - Disable module clock via \c NvRmPowerModuleClockControl.
 * - Disable module power via \c NvRmPowerVoltageControl.
 * - Unregister itself as power client via NvRmPowerUnRegister().
 *
 * @ingroup nvddk_rm
 * @{
 */

/**
 * Frequency data type, expressed in kHz.
 */
typedef NvU32 NvRmFreqKHz;

/**
 * Special value for an unspecified or default frequency.
 */
static const NvRmFreqKHz NvRmFreqUnspecified = 0xFFFFFFFF;

/**
 * Special value for the maximum possible frequency.
 */
static const NvRmFreqKHz NvRmFreqMaximum = 0xFFFFFFFD;

/**
 * Voltage data type, expressed in millivolts.
 */
typedef NvU32 NvRmMilliVolts;

/**
 * Special value for an unspecified or default voltage.
 */
static const NvRmMilliVolts NvRmVoltsUnspecified = 0xFFFFFFFF;

/**
 * Special value for the maximum possible voltage.
 */
static const NvRmMilliVolts NvRmVoltsMaximum = 0xFFFFFFFD;

/**
 * Special value for voltage/power disable.
 */
static const NvRmMilliVolts NvRmVoltsCycled = 0xFFFFFFFC;

/**
 * Special value for voltage/power disable.
 */
static const NvRmMilliVolts NvRmVoltsOff = 0;

/**
 * Defines possible power management events.
 */
typedef enum
{
    /// Specifies no outstanding events.
    NvRmPowerEvent_NoEvent = 1,

    /// Specifies wake from LP0.
    NvRmPowerEvent_WakeLP0,

    /// Specifies wake from LP1.
    NvRmPowerEvent_WakeLP1,
    NvRmPowerEvent_Num,
    NvRmPowerEvent_Force32 = 0x7FFFFFFF
} NvRmPowerEvent;

/**
 * Defines combined RM clients power state.
 */
typedef enum
{
    /// Specifies boot state ("RM is not open, yet").
    NvRmPowerState_Boot = 1,

    /// Specifies active state ("not ready-to-suspend").
    /// This state is entered if any client enables power to any module, other
    /// than \a NvRmPrivModuleID_System, via NvRmPowerVoltageControl() API.
    NvRmPowerState_Active,

    /// Specifies H/W autonomous state ("ready-to-core-power-on-suspend").
    /// This state is entered if all RM clients enable power only for
    /// \a NvRmPrivModuleID_System, via NvRmPowerVoltageControl() API.
    NvRmPowerState_AutoHw,

    /// Specifies idle state ("ready-to-core-power-off-suspend").
    /// This state is entered if none of the RM clients enables power
    /// to any module.
    NvRmPowerState_Idle,

    /// Specifies LP0 state ("main power-off suspend").
    NvRmPowerState_LP0,

    /// Specifies LP1 state ("main power-on suspend").
    NvRmPowerState_LP1,

    /// Specifies skipped LP0 state (set when LP0 entry error is
    /// detected, SoC resumes operations without entering LP0 state).
    NvRmPowerState_SkippedLP0,
    NvRmPowerState_Num,
    NvRmPowerState_Force32 = 0x7FFFFFFF
} NvRmPowerState;

/** Defines the clock configuration flags that are applicable for some modules.
 * Multiple flags can be OR'ed and passed to NvRmPowerModuleClockConfig().
 */
typedef enum
{
    /// Use external clock for the pads of the module.
    NvRmClockConfig_ExternalClockForPads = 0x1,

    /// Use internal clock for the pads of the module.
    NvRmClockConfig_InternalClockForPads = 0x2,

    /// Use external clock for the core of the module, or
    /// module is in slave mode.
    NvRmClockConfig_ExternalClockForCore = 0x4,

    /// Use internal clock for the core of the module, or
    /// module is in master mode.
    NvRmClockConfig_InternalClockForCore = 0x8,

    /// Use inverted clock for the module, i.e., the polarity of the clock
    /// used is inverted with respect to the source clock.
    NvRmClockConfig_InvertedClock = 0x10,

    /// Configure target module sub-clock:
    ///
    /// - Target Display: configure Display and TVDAC.
    /// - Target TVO: configure CVE and TVDAC only.
    /// - Target VI: configure VI_SENSOR only.
    /// - Target SPDIF: configure SPDIFIN only.
    NvRmClockConfig_SubConfig = 0x20,

    /// Select MIPI PLL as clock source:
    ///
    /// - Target Display:
    ///  - NvRmClockConfig_InternalClockForPads is also specified --
    ///      use MIPI PLL for pixel clock source, preserve PLL configuration.
    ///  - NvRmClockConfig_InternalClockForPads is not specified --
    ///      use MIPI PLL for pixel clock source, re-configure it if necessary.
    /// - Target HDMI: use MIPI PLL as HDMI clock source.
    NvRmClockConfig_MipiSync = 0x40,

    /// Adjust Audio PLL to match requested I2S or SPDIF frequency.
    NvRmClockConfig_AudioAdjust = 0x80,

    /// Disable TVDAC along with display configuration.
    NvRmClockConfig_DisableTvDAC = 0x100,

    /// Do not fail clock configuration request with specific target frequency
    /// above HW limit -- just configure clock at HW limit. @note Caller
    /// can request \a NvRmFreqMaximum to configure clock at HW limit, regardless
    /// of this flag's presence.
    NvRmClockConfig_QuietOverClock = 0x200,
    NvRmClockConfigFlags_Num,
    NvRmClockConfigFlags_Force32 = 0x7FFFFFFF
} NvRmClockConfigFlags;

/**
 * Defines SOC-wide clocks controlled by Dynamic Frequency Scaling (DFS)
 * that can be targeted by starvation and busy hints
 */
typedef enum
{
    /// Specifies CPU clock.
    NvRmDfsClockId_Cpu = 1,

    /// Specifies AVP clock.
    NvRmDfsClockId_Avp,

    /// Specifies system bus clock.
    NvRmDfsClockId_System,

    /// Specifies AHB bus clock.
    NvRmDfsClockId_Ahb,

    /// Specifies APB bus clock.
    NvRmDfsClockId_Apb,

    /// Specifies video pipe clock.
    NvRmDfsClockId_Vpipe,

    /// Specifies external memory controller clock.
    NvRmDfsClockId_Emc,
    NvRmDfsClockId_Num,
    NvRmDfsClockId_Force32 = 0x7FFFFFFF
} NvRmDfsClockId;

/**
 * Defines DFS manager run states.
 */
typedef enum
{
    /// DFS is in an invalid, not-initialized state.
    NvRmDfsRunState_Invalid = 0,

    /// DFS is in a disabled/not supported (terminal state).
    NvRmDfsRunState_Disabled = 1,

    /// DFS is stopped -- no automatic clock control. Starvation and busy hints
    /// are recorded but have no affect.
    NvRmDfsRunState_Stopped,

    /// DFS is running in closed loop -- full automatic control of SoC-wide
    /// clocks based on clock activity measuremnets. Starvation and busy hints
    /// are functional as well.
    NvRmDfsRunState_ClosedLoop,

    /// DFS is running in closed loop with profiling (cannot be set on non-
    /// profiling build).
    NvRmDfsRunState_ProfiledLoop,
    NvRmDfsRunState_Num,
    NvRmDfsRunState_Force32 = 0x7FFFFFFF
} NvRmDfsRunState;

/**
 * Defines DFS profile targets.
 */
typedef enum
{
    /// DFS algorithm within ISR.
    NvRmDfsProfileId_Algorithm = 1,

    /// DFS Interrupt service -- includes algorithm plus OS locking and
    ///  signaling calls; hence, includes blocking time (if any) as well.
    NvRmDfsProfileId_Isr,

    /// DFS clock control time -- includes PLL stabilazation time, OS locking
    /// and signalling calls; hence, includes blocking time (if any) as well.
    NvRmDfsProfileId_Control,
    NvRmDfsProfileId_Num,
    NvRmDfsProfileId_Force32 = 0x7FFFFFFF
} NvRmDfsProfileId;

/**
 * Defines voltage rails that are controlled in conjunction with dynamic
 * frequency scaling.
 */
typedef enum
{
    /// SoC core rail.
    NvRmDfsVoltageRailId_Core = 1,

    /// Dedicated CPU rail.
    NvRmDfsVoltageRailId_Cpu,
    NvRmDfsVoltageRailId_Num,
    NvRmDfsVoltageRailId_Force32 = 0x7FFFFFFF
} NvRmDfsVoltageRailId;

/**
 * Defines busy hint API synchronization modes.
 */
typedef enum
{
    /// Asynchronous mode (non-blocking API).
    NvRmDfsBusyHintSyncMode_Async = 1,

    /// Synchronous mode (blocking API).
    NvRmDfsBusyHintSyncMode_Sync,
    NvRmDfsBusyHintSyncMode_Num,
    NvRmDfsBusyHintSyncMode_Force32 = 0x7FFFFFFF
} NvRmDfsBusyHintSyncMode;

/**
 * Holds information on DFS clock domain utilization.
 */
typedef struct NvRmDfsClockUsageRec
{
    /// Minimum clock domain frequency.
    NvRmFreqKHz MinKHz;

    /// Maximum clock domain frequency.
    NvRmFreqKHz MaxKHz;

    /// Low corner frequency -- current low boundary for DFS control algorithm.
    /// Can be dynamically adjusted via APIs:
    /// - NvRmDfsSetLowCorner() for all DFS domains.
    /// - NvRmDfsSetCpuEnvelope() for CPU.
    /// - NvRmDfsSetEmcEnvelope() for EMC.
    ///
    /// When all DFS domains hit low corner, DFS stops waking up CPU
    ///  from low power state.
    NvRmFreqKHz LowCornerKHz;

    /// High corner frequency -- current high boundary for DFS control algorithm.
    /// Can be dynamically adjusted via APIs:
    /// - NvRmDfsSetCpuEnvelope() for Cpu.
    /// - NvRmDfsSetEmcEnvelope() for Emc.
    /// - NvRmDfsSetAvHighCorner() for other DFS domains.
    NvRmFreqKHz HighCornerKHz;

    /// Current clock domain frequency.
    NvRmFreqKHz CurrentKHz;

    /// Average frequency of domain \em activity (not average frequency). For
    /// domains that do not have activity monitors reported as unspecified.
    NvRmFreqKHz AverageKHz;
} NvRmDfsClockUsage;

/**
 * Holds information on DFS busy hint.
 */
typedef struct NvRmDfsBusyHintRec
{
    /// Target clock domain ID.
    NvRmDfsClockId ClockId;

    /// Requested boost duration, in milliseconds.
    NvU32 BoostDurationMs;

    /// Requested clock frequency level, in kHz.
    NvRmFreqKHz BoostKHz;

    /// Busy pulse mode indicator -- if NV_TRUE, busy boost is completely removed
    /// after busy time has expired; if NV_FALSE, DFS will gradually lower domain
    /// frequency after busy boost.
    NvBool BusyAttribute;
} NvRmDfsBusyHint;

/**
 * Holds information on DFS starvation hint.
 */
typedef struct NvRmDfsStarvationHintRec
{
    /// Target clock domain ID.
    NvRmDfsClockId ClockId;

    /// The starvation indicator for the target domain.
    NvBool Starving;
} NvRmDfsStarvationHint;

/**
 * This macro is used to convert ASCII 4-character codes
 * into the 32-bit tag that can be used to identify power manager clients for
 * logging purposes.
 */
#define NVRM_POWER_CLIENT_TAG(a,b,c,d) \
    ((NvU32) ((((a)&0xffUL)<<24UL) |  \
              (((b)&0xffUL)<<16UL) |  \
              (((c)&0xffUL)<< 8UL) |  \
              (((d)&0xffUL))))

/**
 * Registers RM power client.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param hEventSemaphore The client semaphore for power management event
 *  signaling. If null, no events will be signaled to the particular client.
 * @param pClientId A pointer to the storage that on entry contains client
 *  tag (optional), and on exit returns client ID, assigned by power manager.
 *
 * @retval NvSuccess If registration was successful.
 * @retval NvError_InsufficientMemory If failed to allocate memory for client
 *  registration.
 */
NvError NvRmPowerRegister(
    NvRmDeviceHandle hRmDeviceHandle,
    NvOsSemaphoreHandle hEventSemaphore,
    NvU32 * pClientId);

/**
 * Unregisters RM power client. Power and clock for the modules enabled by this
 * client are disabled and any starvation or busy requests are cancelled during
 * the unregistration.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ClientId The client ID obtained during registration.
 */
void NvRmPowerUnRegister(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 ClientId);

/**
 * Gets last detected and not yet retrieved power management event.
 * Returns no outstanding event if no events has been detected since the
 * client registration or the last call to this function.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ClientId The client ID obtained during registration.
 * @param pEvent Output storage pointer for power event identifier.
 *
 * @retval NvSuccess If event identifier was retrieved successfully.
 * @retval NvError_BadValue If specified client ID is not registered.
 */
NvError NvRmPowerGetEvent(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 ClientId,
    NvRmPowerEvent * pEvent);

/**
 * Notifies RM about power management event. Provides an interface for
 * OS power manager to report system power events to RM.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param Event The event RM power manager is to be aware of.
 */
void NvRmPowerEventNotify(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmPowerEvent Event);

/**
 * Gets combined RM clients power state.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param pState Output storage pointer for combined RM clients power state.
 *
 * @retval NvSuccess If power state was retrieved successfully.
 */
NvError NvRmPowerGetState(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmPowerState * pState);

/**
 * Gets SoC primary oscillator/input frequency.
 *
 * @param hRmDeviceHandle The RM device handle.
 *
 * @return Primary frequency in kHz.
 */
NvRmFreqKHz NvRmPowerGetPrimaryFrequency(
    NvRmDeviceHandle hRmDeviceHandle);

/**
 * Gets maximum frequency limit for the module clock.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ModuleId The combined module ID and instance of the target module.
 *
 * @return Module clock maximum frequency in kHz.
 */
NvRmFreqKHz NvRmPowerModuleGetMaxFrequency(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId);

/**
 *  Sets the clock configuration of the module clock.
 *  This API can also be used to query the existing configuration.
 *
 *  @par Usage example:
 *
 *  @code
 *
 *  NvError Error;
 *  NvRmFreqKHz MyFreqKHz = 0;
 *  ModuleId = NVRM_MODULE_ID(NvRmModuleID_Uart, 0);
 *
 *  // Get current frequency settings
 *  Error = NvRmPowerModuleClockConfig(RmHandle, ModuleId, ClientId,
 *                                      0, 0, NULL, 0, &MyFreqKHz, 0);
 *
 * // Set target frequency within HW defined limits
 * MyFreqKHz = TARGET_FREQ;
 * Error = NvRmPowerModuleClockConfig(RmHandle, ModuleId, ClientId,
 *                                    NvRmFreqUnspecified, NvRmFreqUnspecified,
 *                                    &MyFreqKHz, 1, &MyFreqKHz);
 *
 * @endcode
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ModuleId The combined module ID and instance of the target module.
 * @param ClientId The client ID obtained during registration.
 * @param MinFreq Requested minimum frequency for hardware module operation.
 *      - If the value is ::NvRmFreqUnspecified, RM uses the the min freq that
 *          this module can operate.
 *      - If the value specified is more than the HW minimum, passed value is
 *          used.
 *      - If the value specified is less than the HW minimum, it will be clipped
 *          to the HW minimum value.
 * @param MaxFreq Requested maximum frequency for hardware module operation.
 *      - If the value is ::NvRmFreqUnspecified, RM uses the the max freq that
 *          this module can run.
 *      - If the value specified is less than the HW maximum, that value is
 *           used.
 *      - If the value specified is more than the HW limit, it will be clipped
 *          to the HW maximum.
 * @param PrefFreqList Pointer to a list of preferred frequencies, sorted in the
 *      decresing order of priority. Use ::NvRmFreqMaximum to request HW maximum.
 * @param PrefFreqListCount Number of entries in the \a PrefFreqList array.
 * @param CurrentFreq Returns the current clock frequency of that module. NULL
 *      is a valid value for this parameter.
 * @param flags Module-specific flags. Thse flags are valid only for some
 *      modules. @sa nvrm_power::NvRmClockConfigFlags.
 *
 * @retval NvSuccess If clock control request completed successfully.
 * @retval NvError_ModuleNotPresent If the module ID or instance is invalid.
 * @retval NvError_NotSupported If failed to configure requested frequency (e.g.,
 *  output frequency for possible divider settings is outside specified range).
 */
NvError NvRmPowerModuleClockConfig(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvU32 ClientId,
    NvRmFreqKHz MinFreq,
    NvRmFreqKHz MaxFreq,
    const NvRmFreqKHz * PrefFreqList,
    NvU32 PrefFreqListCount,
    NvRmFreqKHz * CurrentFreq,
    NvU32 flags);

/**
 *  Gets the clock configuration of the module clock.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ModuleId The combined module ID and instance of the target module.
 * @param CurrentFreq Returns the current clock frequency of that module. NULL
 *      is a valid value for this parameter.
 *
 * @retval NvSuccess If get clock control request completed successfully.
 * @retval NvError_ModuleNotPresent If the module ID or instance is invalid.
 */
NvError NvRmPowerModuleGetClockConfig(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvRmFreqKHz * CurrentFreq);

/**
 *  Enables and disables the module clock.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ModuleId The combined module ID and instance of the target module.
 * @param ClientId The client ID obtained during registration.
 * @param Enable Enables/diables the module clock.
 *
 * @retval NvSuccess If the module is enabled.
 * @retval NvError_ModuleNotPresent If the module ID or instance is invalid.
 */
NvError NvRmPowerModuleClockControl(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvU32 ClientId,
    NvBool Enable);

/**
 * Requests the voltage range for a hardware module. As power planes are shared
 * between different modules, in the majority of cases the RM will choose the
 * appropriate voltage, and module owners only need to enable or disable power
 * for a module. Enable request is always completed (i.e., voltage is applied
 * to the module) before this function returns. Disable request just means that
 * the client is ready for module power down. Actually the power may be removed
 * within the call or any time later, depending on other client needs and power
 * plane dependencies with other modules.
 *
 * Assert encountered in debug mode if the module ID or instance is invalid.
 *
 * @par Usage example:
 *
 * @code
 *
 * NvError Error;
 * ModuleId = NVRM_MODULE_ID(NvRmModuleID_Uart, 0);
 *
 * // Enable module power
 * Error = NvRmPowerVoltageControl(RmHandle, ModuleId, ClientId,
 *          NvRmVoltsUnspecified, NvRmVoltsUnspecified,
 *          NULL, 0, NULL);
 *
 * // Disable module power
 * Error = NvRmPowerVoltageControl(RmHandle, ModuleId, ClientId,
 *          NvRmVoltsOff, NvRmVoltsOff,
 *          NULL, 0, NULL);
 *
 * @endcode
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ModuleId The combined module ID and instance of the target module.
 * @param ClientId The client ID obtained during registration.
 * @param MinVolts Requested minimum voltage for hardware module operation.
 * @param MaxVolts Requested maximum voltage for hardware module operation.
 *  Set to \a NvRmVoltsUnspecified when enabling power for a module, or to
 *  \a NvRmVoltsOff when disabling.
 * @param PrefVoltageList Pointer to a list of preferred voltages, ordered from
 *  lowest to highest, and terminated with a voltage of \a NvRmVoltsUnspecified.
 *  This parameter is optional -- ignored if NULL.
 * @param PrefVoltageListCount Number of entries in the \a PrefVoltageList
 *  array.
 * @param CurrentVolts Output storage pointer for resulting module voltage.
 *  - \a NvRmVoltsUnspecified is returned if module power is On and was not
 *     cycled, since the last voltage request with the same \a ClientId and \a
 *     ModuleId.
 *  - \a NvRmVoltsCycled is returned if module power is On but was powered down,
 *     since the last voltage request with the same ClientId and ModuleId.
 *  - \a NvRmVoltsOff is returned if module power is Off. This parameter is
 *     optional -- ignored  if NULL.
 *
 * @retval NvSuccess If voltage control request completed successfully.
 * @retval NvError_BadValue If specified client ID is not registered.
 * @retval NvError_InsufficientMemory If failed to allocate memory for
 *  voltage request.
 */
NvError NvRmPowerVoltageControl(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvU32 ClientId,
    NvRmMilliVolts MinVolts,
    NvRmMilliVolts MaxVolts,
    const NvRmMilliVolts * PrefVoltageList,
    NvU32 PrefVoltageListCount,
    NvRmMilliVolts * CurrentVolts);

/**
 * Lists modules registered by power clients for voltage control.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param pListSize Pointer to the list size. On entry specifies list size
 *  allocated by the caller, on exit the actual number of IDs returned. If
 *  entry size is 0, maximum list size is returned.
 * @param pIdList Pointer to the list of combined module ID/instance values
 *  to be filled in by this function. Ignored if input list size is 0.
 * @param pActiveList Pointer to the list of modules active attributes
 *  to be filled in by this function. Ignored if input list size is 0.
 */
void NvRmListPowerAwareModules(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 * pListSize,
    NvRmModuleID * pIdList,
    NvBool * pActiveList);

/**
 * Requests immediate frequency boost for SOC-wide clocks. In general, the RM
 * DFS manages SOC-wide clocks by measuring the average use of clock cycles,
 * and adjusting clock rates to minimize wasted clocks. It is preferable and
 * expected that modules consume clock cycles at a more-or-less constant rate.
 * Under some circumstances this will not be the case. For example, many cycles
 * may be consumed to prime a new media processing activity. If power client
 * anticipates such circumstances, it may sparingly use this API to alert the RM
 * that a temporary spike in clock usage is about to occur.
 *
 * @par Usage example:
 *
 * @code
 *
 * // Busy hint for CPU clock
 * NvError Error;
 * Error = NvRmPowerBusyHint(RmHandle, NvRmDfsClockId_Cpu, ClientId,
 *          BoostDurationMs, BoostFreqKHz);
 *
 * @endcode
 *
 * Clients should not call this API in an attempt to micro-manage a particular
 * clock frequency as that is the responsibility of the RM.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ClockId The DFS ID of the clock targeted by this hint.
 * @param ClientId The client ID obtained during registration.
 * @param BoostDurationMs The estimate of the boost duration in milliseconds.
 *  Use ::NV_WAIT_INFINITE to specify busy until canceled. Use 0 to request
 *  instantaneous spike in frequency and let DFS to scale down.
 * @param BoostKHz The requirements for the boosted clock frequency in kHz.
 *  Use \a NvRmFreqMaximum to request maximum domain frequency. Use 0 to cancel
 *  all busy hints reported by the specified client for the specified domain.
 *
 * @retval NvSuccess If busy request completed successfully.
 * @retval NvError_BadValue If specified client ID is not registered.
 * @retval NvError_InsufficientMemory If failed to allocate memory for
 *  busy hint.
 */
NvError NvRmPowerBusyHint(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsClockId ClockId,
    NvU32 ClientId,
    NvU32 BoostDurationMs,
    NvRmFreqKHz BoostKHz);

/**
 * Requests immediate frequency boost for multiple SOC-wide clock domains.
 * @sa NvRmPowerBusyHint() for detailed explanation of busy hint effects.
 *
 * @note It is recommended to use synchronous mode only when low frequency
 *  may result in functional failure. Otherwise, use asynchronous mode or
 *  \a NvRmPowerBusyHint API, which is always executed as non-blocking request.
 *  Synchronous mode must not be used by PMU transport.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ClientId The client ID obtained during registration.
 * @param pMultiHint Pointer to a list of busy hint records for
 *  targeted clocks.
 * @param NumHints Number of entries in \a pMultiHint array.
 * @param Mode Synchronization mode. In asynchronous mode this API returns to
 *  the caller after request is signaled to power manager (non-blocking call).
 *  In synchronous mode the API returns after busy hints are processed by power
 *  manager (blocking call).
 *
 * @retval NvSuccess If busy hint request completed successfully.
 * @retval NvError_BadValue If specified client ID is not registered.
 * @retval NvError_InsufficientMemory If failed to allocate memory for
 *  busy hints.
 */
NvError NvRmPowerBusyHintMulti(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 ClientId,
    const NvRmDfsBusyHint * pMultiHint,
    NvU32 NumHints,
    NvRmDfsBusyHintSyncMode Mode);

/**
 * Requests frequency increase for SOC-wide clock to avoid realtime starvation
 * conditions. Allows modules to contribute to the detection and avoidance of
 * clock starvation for DFS controlled clocks.
 *
 * This API should be called to indicate starvation threat and also to cancel
 * request when a starvation condition has eased.
 *
 * @note Although the RM DFS does its best to manage clocks without starving
 * the system for clock cycles, bursty clock usage can occasionally cause
 * short-term clock starvation. One solution is to leave a large enough clock
 * rate guard band such that any possible burst in clock usage will be absorbed.
 * This approach tends to waste clock cycles, and worsen power management.
 *
 * By allowing power clients to participate in the avoidance of system clock
 * starvation situations, detection responsibility can be moved closer to the
 * hardware buffers and processors where starvation occurs, while leaving the
 * overall dynamic clocking policy to the RM. A typical client would be a module
 * that manages media processing and is able to determine when it is falling
 * behind by watching buffer levels or some other module-specific indicator. In
 * response to the starvation request the RM increases gradually the respective
 * clock frequency until the request vis cancelled by the client.
 *
 * @par Usage example:
 *
 * @code
 *
 * NvError Error;
 *
 * // Request CPU clock frequency increase to avoid starvation
 * Error = NvRmPowerStarvationHint(
 *          RmHandle, NvRmDfsClockId_Cpu, ClientId, NV_TRUE);
 *
 * // Cancel starvation request for CPU clock frequency
 * Error = NvRmPowerStarvationHint(
 *          RmHandle, NvRmDfsClockId_Cpu, ClientId, NV_FALSE);
 *
 * @endcode
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ClockId The DFS ID of the clock targeted by this hint.
 * @param ClientId The client ID obtained during registration.
 * @param Starving The starvation indicator for the target module. If true,
 *  the client is requesting target frequency increase to avoid starvation.
 *  If false, the indication is that the imminent starvation is no longer a
 *  concern for this particular client.
 *
 * @retval NvSuccess If starvation request completed successfully.
 * @retval NvError_BadValue If specified client ID is not registered.
 * @retval NvError_InsufficientMemory If failed to allocate memory for
 *  starvation hint.
 */
NvError NvRmPowerStarvationHint(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsClockId ClockId,
    NvU32 ClientId,
    NvBool Starving);

/**
 * Requests frequency increase for multiple SOC-wide clock domains to avoid
 * realtime starvation conditions.
 * @sa NvRmPowerStarvationHint() for detailed explanation of starvation hint
 * effects.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ClientId The client ID obtained during registration.
 * @param pMultiHint Pointer to a list of starvation hint records for
 *  targeted clocks.
 * @param NumHints Number of entries in \a pMultiHint array.
 *
 * @retval NvSuccess If starvation hint request completed successfully.
 * @retval NvError_BadValue If specified client ID is not registered.
 * @retval NvError_InsufficientMemory If failed to allocate memory for
 *  starvation hints.
 */
NvError NvRmPowerStarvationHintMulti(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 ClientId,
    const NvRmDfsStarvationHint * pMultiHint,
    NvU32 NumHints);


 // TODO: Remove this API?

/**
 * Notifies the RM about DDK module activity.
 *
 * @note This function lets DDK modules notify the RM about interesting system
 * activities. Not all modules will need to make this indication, typically only
 * modules involved in user input or output activities. However, with current
 * SOC power management architecture such activities will be detected by the OS
 * adaptation layer, not RM. This API is not removed, just in case, we will find
 * out that RM still need to participate in user activity detection. In general,
 * modules should call this interface sparingly, no more than once every few
 * seconds.
 *
 * For cases when activity is a series of discontinuous events (keypresses, for
 * example), this parameter should simply be set to 1.
 *
 * For lengthy, continuous activities, this parameter is set to the estimated
 * length of the activity in milliseconds. This can reduce the number of calls
 * made to this API.
 *
 * A value of 0 in this parameter indicates that the module is not active and
 * can be used to signal the end of a previously estimated continuous activity.
 *
 * In current power management architecture user activity is handled by OS
 * (nor RM) power manager, and activity API is not used at all.
 *
 * Assert encountered in debug mode if the module ID or instance is invalid.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ModuleId The combined module ID and instance of the target module.
 * @param ClientId The client ID obtained during registration.
 * @param ActivityDurationMs The duration of the module activity.
 *   - For cases when activity is a series of discontinuous events (keypresses, for
 * example), this parameter should simply be set to 1.
 *   - For lengthy, continuous activities, this parameter is set to estimated
 * length of the activity in milliseconds. This can reduce the number of calls
 * made to this API.
 *   - A value of 0 in this parameter indicates that the module is not active
 *     and can be used to signal the end of a previously estimated continuous
 *     activity.
 *
 * @retval NvSuccess If clock control request completed successfully.
 */
NvError NvRmPowerActivityHint(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvU32 ClientId,
    NvU32 ActivityDurationMs);

/**
 * Gets DFS run sate.
 *
 * @param hRmDeviceHandle The RM device handle.
 *
 * @return Current DFS run state.
 */
NvRmDfsRunState NvRmDfsGetState(
    NvRmDeviceHandle hRmDeviceHandle);

/**
 * Gets information on DFS controlled clock utilization. If DFS is stopped
 * or disabled the average frequency is always equal to current frequency.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param ClockId The DFS ID of the clock targeted by this request.
 * @param pClockUsage Output storage pointer for clock utilization information.
 *
 * @return NvSuccess If clock usage information is returned successfully.
 */
NvError NvRmDfsGetClockUtilization(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsClockId ClockId,
    NvRmDfsClockUsage * pClockUsage);

/**
 * Sets DFS run state. Allows to stop or re-start DFS as well as switch
 * between open and closed loop operations.
 *
 * On transition to the DFS stopped state, the DFS clocks are just kept at
 * current frequencies. On transition to DFS run states, DFS sampling data
 * is re-initialized only if originally DFS was stopped. Transition between
 * running states has no additional effects, besides operation mode changes.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param NewDfsRunState The DFS run state to be set.
 *
 * @retval NvSuccess If DFS state was set successfully.
 * @retval NvError_NotSupported If DFS was disabled initially, in attempt
 * to disable initially enabled DFS, or in attempt to run profiled loop
 * on non profiling build.
 */
NvError NvRmDfsSetState(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsRunState NewDfsRunState);

/**
 * Sets DFS low corner frequencies -- low boundaries for DFS clocks when DFS
 * is running. If all DFS domains hit low corner, DFS will no longer wake
 * CPU from low power state.
 *
 * @note When CPU envelope is set via NvRmDfsSetCpuEnvelope() API the CPU
 *       low corner boundary can not be changed by this function.
 *       When EMC envelope is set via NvRmDfsSetEmcEnvelope() API the EMC
 *       low corner boundary can not be changed by this function.
 *
 * @par Usage example:
 *
 * @code
 *
 * NvError Error;
 * NvRmFreqKHz LowCorner[NvRmDfsClockId_Num];
 *
 * // Fill in low corner array
 * LowCorner[NvRmDfsClockId_Cpu] = NvRmFreqUnspecified;
 * LowCorner[NvRmDfsClockId_Avp] = ... ;
 * LowCorner[NvRmDfsClockId_System] = ...;
 * LowCorner[NvRmDfsClockId_Ahb] = ...;
 * LowCorner[NvRmDfsClockId_Apb] = ...;
 * LowCorner[NvRmDfsClockId_Vpipe] = ...;
 * LowCorner[NvRmDfsClockId_Emc] = ...;
 *
 * // Set new low corner for domains other than CPU, and preserve CPU boundary
 * Error = NvRmDfsSetLowCorner(RmHandle, NvRmDfsClockId_Num, LowCorner);
 *
 * @endcode
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param DfsFreqListCount Number of entries in the \a pDfsLowFreqList array.
 *  Must be always equal to ::NvRmDfsClockId_Num.
 * @param pDfsLowFreqList Pointer to a list of low corner frequencies, ordered
 *  according to nvrm_power::NvRmDfsClockId enumeration. If the list entry is
 *   set to \c NvRmFreqUnspecified, the respective low corner boundary is not modified.
 *
 * @retval NvSuccess If low corner frequencies were updated successfully.
 * @retval NvError_NotSupported If DFS is disabled.
 */
NvError NvRmDfsSetLowCorner(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 DfsFreqListCount,
    const NvRmFreqKHz * pDfsLowFreqList);

/**
 * Sets DFS target frequencies. If DFS is stopped clocks for the DFS domains
 * will be targeted with the specified frequencies. In any other DFS state
 * this function has no effect.
 *
 * @par Usage example:
 *
 * @code
 *
 * NvError Error;
 * NvRmFreqKHz Target[NvRmDfsClockId_Num];
 *
 * // Fill in target frequencies array
 * Target[NvRmDfsClockId_Cpu] = ... ;
 * Target[NvRmDfsClockId_Avp] = ... ;
 * Target[NvRmDfsClockId_System] = ...;
 * Target[NvRmDfsClockId_Ahb] = ...;
 * Target[NvRmDfsClockId_Apb] = ...;
 * Target[NvRmDfsClockId_Vpipe] = ...;
 * Target[NvRmDfsClockId_Emc] = ...;
 *
 * // Set new target
 * Error = NvRmDfsSetTarget(RmHandle, NvRmDfsClockId_Num, Target);
 *
 * @endcode
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param DfsFreqListCount Number of entries in the \a pDfsTargetFreqList array.
 *  Must be always equal to \c NvRmDfsClockId_Num.
 * @param pDfsTargetFreqList Pointer to a list of target frequencies, ordered
 *  according to \a NvRmDfsClockId enumeration. If the list entry is set to
 *  \a NvRmFreqUnspecified, the current domain frequency is used as a target.
 *
 * @retval NvSuccess If target frequencies were updated successfully.
 * @retval NvError_NotSupported If DFS is not stopped (disabled, or running).
 */
NvError NvRmDfsSetTarget(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 DfsFreqListCount,
    const NvRmFreqKHz * pDfsTargetFreqList);

/**
 * Sets DFS high and low boundaries for CPU domain clock frequency.
 *
 * Envelope parameters are clipped to the HW defined CPU domain range.
 * If envelope parameter is set to ::NvRmFreqUnspecified, the respective
 * CPU boundary is not modified, unless it violates the new setting for
 * the other boundary; in the latter case both boundaries are set to the
 * new specified value.
 *
 * @par Usage example:
 *
 * @code
 *
 * NvError Error;
 *
 * // Set CPU envelope boundaries to LowKHz : HighKHz
 * Error = NvRmDfsSetCpuEnvelope(RmHandle, LowKHz, HighKHz);
 *
 * // Change CPU envelope high boundary to HighKHz
 * Error = NvRmDfsSetCpuEnvelope(RmHandle, NvRmFreqUnspecified, HighKHz);
 *
 * // Release CPU envelope back to HW limits
 * Error = NvRmDfsSetCpuEnvelope(RmHandle, 0, NvRmFreqMaximum);
 *
 * @endcode
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param DfsCpuLowCornerKHz Requested low boundary in kHz.
 * @param DfsCpuHighCornerKHz Requested high limit in kHz.
 *
 * @retval NvSuccess If DFS envelope for for CPU domain was updated
 *  successfully.
 * @retval NvError_BadValue If reversed boundaries are specified.
 * @retval NvError_NotSupported If DFS is disabled.
 */
NvError NvRmDfsSetCpuEnvelope(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmFreqKHz DfsCpuLowCornerKHz,
    NvRmFreqKHz DfsCpuHighCornerKHz);

/**
 * Sets DFS high and low boundaries for EMC domain clock frequency.
 *
 * Envelope parameters are clipped to the ODM defined EMC configurations
 * within HW defined EMC domain range. If envelope parameter is set to
 * \a NvRmFreqUnspecified, the respective EMC boundary is not modified, unless
 * it violates the new setting for the other boundary; in the latter case
 * both boundaries are set to the new specified value.
 *
 * @par Usage example:
 *
 * @code
 *
 * NvError Error;
 *
 * // Set EMC envelope boundaries to LowKHz : HighKHz
 * Error = NvRmDfsSetEmcEnvelope(RmHandle, LowKHz, HighKHz);
 *
 * // Change EMC envelope high boundary to HighKHz
 * Error = NvRmDfsSetEmcEnvelope(RmHandle, NvRmFreqUnspecified, HighKHz);
 *
 * // Release EMC envelope back to HW limits
 * Error = NvRmDfsSetEmcEnvelope(RmHandle, 0, NvRmFreqMaximum);
 *
 * @endcode
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param DfsEmcLowCornerKHz Requested low boundary in kHz.
 * @param DfsEmcHighCornerKHz Requested high limit in kHz.
 *
 * @retval NvSuccess If DFS envelope for for EMC domain was updated
 *  successfully.
 * @retval NvError_BadValue If reversed boundaries are specified.
 * @retval NvError_NotSupported If DFS is disabled.
 */
NvError NvRmDfsSetEmcEnvelope(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmFreqKHz DfsEmcLowCornerKHz,
    NvRmFreqKHz DfsEmcHighCornerKHz);

/**
 * Sets DFS high boundaries for CPU and EMC.
 *
 * @note When either CPU or EMC envelope is set via \c NvRmDfsSetXxxEnvelope()
 * API, neither CPU nor EMC boundary is changed by this function.
 *
 * Requested parameters are clipped to the respective HW defined domain
 * ranges, as well as to ODM defined EMC configurations. If any parameter
 * is set to \a NvRmFreqUnspecified, the respective boundary is not modified.
 *
 * @par Usage example:
 *
 * @code
 *
 * NvError Error;
 *
 * // Set CPU subsystem clock limit to CpuHighKHz and Emc clock limit
 * // to EmcHighKHz
 * Error = NvRmDfsSetCpuEmcHighCorner(RmHandle, CpuHighKHz, EmcHighKHz);
 *
 * @endcode
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param DfsCpuHighKHz Requested high boundary in kHz for CPU.
 * @param DfsEmcHighKHz Requested high limit in kHz for EMC.
 *
 * @retval NvSuccess If high corner for AV subsystem was updated successfully.
 * @retval NvError_NotSupported If DFS is disabled.
 */
NvError NvRmDfsSetCpuEmcHighCorner(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmFreqKHz DfsCpuHighKHz,
    NvRmFreqKHz DfsEmcHighKHz);

/**
 * Sets DFS high boundaries for AV subsystem clocks.
 *
 * @note System bus clock limit must be always above \c DfsAvpHighKHz, and above
 * \c DfsVpipeHighKHz. Therefore it may be adjusted up, as a result of this
 * call, even though, it is marked unspecified.
 *
 * Requested parameter is clipped to the respective HW defined domain
 * range. If  parameter is set to \a NvRmFreqUnspecified, the respective
 * boundary is not modified.
 *
 * @par Usage example:
 *
 * @code
 *
 * NvError Error;
 *
 * // Set AVP clock limit to AvpHighKHz, Vde clock limit to VpipeHighKHz,
 * // and preserve System bus clock limit provided it is above requested
 * // AVP and Vpipe levels.
 * Error = NvRmDfsSetAvHighCorner(
 *          RmHandle, NvRmFreqUnspecified, AvpHighKHz, VpipeHighKHz);
 *
 * @endcode
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param DfsSystemHighKHz Requested high boundary in kHz for system bus.
 * @param DfsAvpHighKHz Requested high boundary in kHz for AVP.
 * @param DfsVpipeHighKHz Requested high limit in kHz for VDE pipe.
 *
 * @retval NvSuccess If high corner for AV subsystem was updated successfully.
 * @retval NvError_NotSupported If DFS is disabled.
 */
NvError NvRmDfsSetAvHighCorner(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmFreqKHz DfsSystemHighKHz,
    NvRmFreqKHz DfsAvpHighKHz,
    NvRmFreqKHz DfsVpipeHighKHz);

/**
 * Gets DFS profiling information.
 *
 * DFS profiling starts/re-starts every time \a NvRmDfsRunState_ProfiledLoop
 * state is set via NvRmDfsSetState(). DFS profiling stops when any other
 * sate is set.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param DfsProfileCount Number of DFS profiles. Must be always equal to
 *  \a NvRmDfsProfileId_Num.
 * @param pSamplesNoList Output storage pointer to an array of sample counts
 *  for each profile target ordered according to \c NvRmDfsProfileId enum.
 * @param pProfileTimeUsList Output storage pointer to an array of cummulative
 *  execution time in microseconds for each profile target ordered according
 *  to \c NvRmDfsProfileId enumeration.
 * @param pDfsPeriodUs Output storage pointer for average DFS sample
 *  period in microseconds.
 *
 * @retval NvSuccess If profile information is returned successfully.
 * @retval NvError_NotSupported If DFS is not ruuning in profiled loop.
 */
NvError NvRmDfsGetProfileData(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 DfsProfileCount,
    NvU32 * pSamplesNoList,
    NvU32 * pProfileTimeUsList,
    NvU32 * pDfsPeriodUs);

/**
 * Starts/re-starts NV DFS logging.
 *
 * @param hRmDeviceHandle The RM device handle.
 */
void NvRmDfsLogStart(
    NvRmDeviceHandle hRmDeviceHandle);

/**
 * Stops DFS logging and gets cumulative mean values of DFS domains frequencies
 *  over logging time.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param LogMeanFreqListCount Number of entries in the \a pLogMeanFreqList
 *  array. Must be always equal to \a NvRmDfsClockId_Num.
 * @param pLogMeanFreqList Pointer to a list filled with mean values of DFS
 *  frequencies, ordered according to \c NvRmDfsClockId enumeration.
 * @param pLogLp2TimeMs Pointer to a variable filled with cumulative time spent
 *  in LP2 in milliseconds.
 * @param pLogLp2Entries Pointer to a variable filled with cumulative number of
 *  LP2 mode entries.
 *
 * @retval NvSuccess If mean values are returned successfully.
 * @retval NvError_NotSupported If DFS is disabled.
 */
NvError NvRmDfsLogGetMeanFrequencies(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 LogMeanFreqListCount,
    NvRmFreqKHz * pLogMeanFreqList,
    NvU32 * pLogLp2TimeMs,
    NvU32 * pLogLp2Entries);

/**
 * Gets specified entry of the detailed DFS activity log.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param EntryIndex Log entrty index.
 * @param LogDomainsCount The size of activity arrays.
 *  Must be always equal to \a NvRmDfsClockId_Num.
 * @param pIntervalMs Pointer to a variable filled with sample interval time
 *  in milliseconds.
 * @param pLp2TimeMs Pointer to a variable filled with time spent in LP2
 *  in milliseconds.
 * @param pActiveCyclesList Pointer to a list filled with domain active cycles
 *  within sample interval.
 * @param pAveragesList Pointer to a list filled with average domain activity
 *  over DFS moving window.
 * @param pFrequenciesList Pointer to a list filled with instantaneous domains
 *  frequencies. All lists are ordered according to \c NvRmDfsClockId enumeration.
 *
 * @retval NvSuccess If log entry is retrieved successfully.
 * @retval NvError_InvalidAddress If requetsed entry is empty.
 * @retval NvError_NotSupported If DFS is disabled, or detailed logging
 *  is not supported.
 */
NvError NvRmDfsLogActivityGetEntry(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 EntryIndex,
    NvU32 LogDomainsCount,
    NvU32 * pIntervalMs,
    NvU32 * pLp2TimeMs,
    NvU32 * pActiveCyclesList,
    NvRmFreqKHz * pAveragesList,
    NvRmFreqKHz * pFrequenciesList);

/**
 * Gets specified entry of the detailed DFS starvation hints log.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param EntryIndex Log entrty index.
 * @param pSampleIndex Pointer to a variable filled with sample interval
 *  index in the activity log when this hint is associated with.
 * @param pClientId A pointer to the client ID.
 * @param pClientTag A pointer to the client tag.
 * @param pStarvationHint Pointer to a variable filled with starvation
 *  hint record.
 *
 * @retval NvSuccess If next entry is retrieved successfully.
 * @retval NvError_InvalidAddress If requetsed entry is empty.
 * @retval NvError_NotSupported If DFS is disabled, or detailed logging
 *  is not supported.
 */
NvError NvRmDfsLogStarvationGetEntry(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 EntryIndex,
    NvU32 * pSampleIndex,
    NvU32 * pClientId,
    NvU32 * pClientTag,
    NvRmDfsStarvationHint * pStarvationHint);

/**
 * Gets specified entry of the detailed DFS busy hints log.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param EntryIndex Log entrty index.
 * @param pSampleIndex Pointer to a variable filled with sample interval
 *  index in the activity log when this hint is associated with.
 * @param pClientId A pointer to the client ID.
 * @param pClientTag A pointer to the client tag.
 * @param pBusyHint Pointer to a variable filled with busy
 *  hint record.
 *
 * @retval NvSuccess If next entry is retrieved successfully.
 * @retval NvError_InvalidAddress If requetsed entry is empty.
 * @retval NvError_NotSupported If DFS is disabled, or detailed logging
 *  is not supported.
 */
NvError NvRmDfsLogBusyGetEntry(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 EntryIndex,
    NvU32 * pSampleIndex,
    NvU32 * pClientId,
    NvU32 * pClientTag,
    NvRmDfsBusyHint * pBusyHint);

/**
 * Gets low threshold and present voltage on the given rail.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param RailId The targeted voltage rail ID.
 * @param pLowMv Output storage pointer for low voltage threshold (in
 *  millivolt). \a NvRmVoltsUnspecified is returned if targeted rail does
 *  not exist on SoC.
 * @param pPresentMv Output storage pointer for present rail voltage (in
 *  millivolt). \a NvRmVoltsUnspecified is returned if targeted rail does
 *  not exist on SoC.
 */
void NvRmDfsGetLowVoltageThreshold(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsVoltageRailId RailId,
    NvRmMilliVolts * pLowMv,
    NvRmMilliVolts * pPresentMv);

/**
 * Sets low threshold for the given rail. The actual rail voltage is scaled
 *  to match SoC clock frequencies, but not below the specified threshold.
 *
 * @param hRmDeviceHandle The RM device handle.
 * @param RailId The targeted voltage rail ID.
 * @param LowMv Low voltage threshold (in millivolts) for the targeted rail.
 *  Ignored if targeted rail does not exist on SoC.
 */
void NvRmDfsSetLowVoltageThreshold(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsVoltageRailId RailId,
    NvRmMilliVolts LowMv);

/**
 * Notifies RM Kernel about entering Suspend state.
 *
 * @param hRmDeviceHandle The RM device handle.
 *
 * @retval NvSuccess if notifying RM entering Suspend state successfully.
 */
NvError NvRmKernelPowerSuspend(
    NvRmDeviceHandle hRmDeviceHandle);

/**
 * Notifies RM kernel about entering Resume state.
 *
 * @param hRmDeviceHandle The RM device handle.
 *
 * @retval NvSuccess if notifying RM entering Resume state successfully.
 */
NvError NvRmKernelPowerResume(
    NvRmDeviceHandle hRmDeviceHandle);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
