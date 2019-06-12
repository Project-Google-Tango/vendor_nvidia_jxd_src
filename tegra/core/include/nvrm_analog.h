/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_ANALOG_H
#define INCLUDED_NVRM_ANALOG_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_module.h"
#include "nvrm_init.h"
#include "nvodm_query.h"

/**
 * List of controllable analog interfaces.  Multiple instances of any
 * particlar interface will be handled by the NVRM_ANALOG_INTERFACE macro
 * below.
 */
typedef enum
{
    NvRmAnalogInterface_Dsi,
    NvRmAnalogInterface_ExternalMemory,
    NvRmAnalogInterface_Hdmi,
    NvRmAnalogInterface_Lcd,
    NvRmAnalogInterface_Uart,
    NvRmAnalogInterface_Usb,
    NvRmAnalogInterface_Sdio,
    NvRmAnalogInterface_Tv,
    NvRmAnalogInterface_VideoInput,
    NvRmAnalogInterface_Num,
    NvRmAnalogInterface_Force32 = 0x7FFFFFFF
} NvRmAnalogInterface;

/**
 * Defines the USB Line state
 */
typedef enum
{
    NvRmUsbLineStateType_SE0 = 0,
    NvRmUsbLineStateType_SJ = 1,
    NvRmUsbLineStateType_SK = 2,
    NvRmUsbLineStateType_SE1 = 3,
    NvRmUsbLineStateType_Num,
    NvRmUsbLineStateType_Force32 = 0x7FFFFFFF
} NvRmUsbLineStateType;

/**
 * List of analog TV DAC type
 */
typedef enum
{
    NvRmAnalogTvDacType_CRT,
    NvRmAnalogTvDacType_SDTV,
    NvRmAnalogTvDacType_HDTV,
    NvRmAnalogTvDacType_Num,
    NvRmAnalogTvDacType_Force32 = 0x7FFFFFFF
} NvRmAnalogTvDacType;

/**
 * Create an analog interface id with multiple instances.
 */
#define NVRM_ANALOG_INTERFACE(id, instance) \
    ((NvRmAnalogInterface)((instance) << 16 | id))

/**
 * Get the interface id.
 */
#define NVRM_ANALOG_INTERFACE_ID(id) ((id) & 0xFFFF)

/**
 * Get the interface instance.
 */
#define NVRM_ANALOG_INTERFACE_INSTANCE(id) (((id) >> 16) & 0xFFFF)

/**
 * Control I/O pads, DACs, or PHYs, either enable or disable, with an optional
 * configuration structure, which may be defined per module.
 *
 * @param hDevice Handle to the RM device
 * @param Interface The physical interface to configure
 * @param Enable enable/disable bit
 * @param Config extra configuration options for each module, if necessary
 * @param ConfigLength the size in bytes of the configuration structure
 */
NvError NvRmAnalogInterfaceControl(
    NvRmDeviceHandle hDevice,
    NvRmAnalogInterface Interface,
    NvBool Enable,
    void* Config,
    NvU32 ConfigLength);

/**
 * Get TV DAC Configuration
 *
 * @param hDevice Handle to the RM device
 * @param Type The analog TV DAC type
 * @return The analog TV DAC Configuration value
 */
NvU8 NvRmAnalogGetTvDacConfiguration(
    NvRmDeviceHandle hDevice,
    NvRmAnalogTvDacType Type);

/**
 * Detect if USB is connected or not
 *
 * @param hDevice Handle to the RM device
 * @return TRUE means USB is connected
 */
NvBool NvRmUsbIsConnected(
    NvRmDeviceHandle hDevice);

/**
 * Detect charger type
 *
 * @param hDevice Handle to the RM device
 * @param wait Delay time and ready to get the correct charger type
 * @return USB charger type
 */
NvU32 NvRmUsbDetectChargerState(
    NvRmDeviceHandle hDevice,
    NvU32 wait);

/**
 * Extended configuration structures for NvRmAnalogInterfaceControl.
 */
typedef struct NvRmAnalogTvDacConfigRec
{
    /* The DAC input source, may be a Display controller or the TVO engine */
    NvRmModuleID Source;

    /* The DAC output amplitude */
    NvU8 DacAmplitude;
} NvRmAnalogTvDacConfig;

/**
 * List of USB analog status check parameters
 */
typedef enum
{
    NvRmAnalogUsbInputParam_CheckCableStatus,
    NvRmAnalogUsbInputParam_CheckChargerStatus,
    NvRmAnalogUsbInputParam_CheckIdStatus,
    NvRmAnalogUsbInputParam_WaitForPhyClock,
    NvRmAnalogUsbInputParam_ConfigureUsbPhy,
    NvRmAnalogUsbInputParam_ChargerDetection,
    NvRmAnalogUsbInputParam_SetUlpiNullTrimmers,
    NvRmAnalogUsbInputParam_ConfigureUlpiNullClock,
    NvRmAnalogUsbInputParam_SetNullUlpiPinMux,
    NvRmAnalogUsbInputParam_SetUlpiLinkTrimmers,
    NvRmAnalogUsbInputParam_VbusInterrupt,
    NvRmAnalogUsbInputParam_IdInterrupt,
    NvRmAnalogUsbInputParam_Num,
    NvRmAnalogUsbInputParam_Force32 = 0x7FFFFFFF
} NvRmAnalogUsbInputParam;

/**
 * Extended configuration structures for NvRmAnalogInterfaceControl for USB.
 */
typedef struct NvRmAnalogUsbConfigRec
{
    /* The USB Status check parameter */
    NvRmAnalogUsbInputParam InParam;
    NvBool UsbCableDetected;
    NvBool UsbChargerDetected;
    NvBool UsbIdDetected;
} NvRmAnalogUsbConfig;

#if defined(__cplusplus)
}
#endif

#endif
