/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef _AVP_GPIO_H
#define _AVP_GPIO_H

#include "nverror.h"
#include "nvcommon.h"

/**
 * @brief Defines the possible GPIO pin modes.
 */
typedef enum
{
    /// Specifies GPIO pin mode as input and no interrupt configured.
    NvAvpGpioPinMode_InputData,

    /// Specifies GPIO pin mode as output.
    NvAvpGpioPinMode_Output,

    NvAvpGpioPinMode_Num,
    NvAvpGpioPinMode_Force32 = 0x7FFFFFFF
} NvAvpGpioPinMode;

/**
 * @brief Defines the pin state.
 */
typedef enum
{
   /// Pin state is low.
    NvAvpGpioPinState_Low = 0,

    /// Pin state is high.
    NvAvpGpioPinState_High,

    /// Pin is in tri-state.
    NvAvpGpioPinState_TriState,
    NvAvpGpioPinState_Num,
    NvAvpGpioPinState_Force32 = 0x7FFFFFFF
} NvAvpGpioPinState;

typedef struct NvAvpGpioCapsRec {
    NvU32 Instances;
    NvU32 PortsPerInstances;
    NvU32 PinsPerPort;
    NvU32 Features;
    NvU32 MaskedOffset;
} NvAvpGpioCaps;

typedef struct
{
    NvAvpGpioCaps *caps;
} NvAvpGpioHandle;

typedef struct
{
    NvU32 Instance;
    NvU32 Port;
    NvU32 PinNumber;
    NvU32 InstanceBaseaddr;
}NvAvpGpioPinHandle;

/* Bit mask of hardware features present in GPIO controller. */
typedef enum {
    NVRM_GPIO_CAP_FEAT_NONE = 0,
    NVRM_GPIO_CAP_FEAT_EDGE_INTR = 0x000000001
} NvAvpGpioCapFeatures;

NvError
NvAvpGpioOpen(
    NvAvpGpioHandle* phGpio);

NvError NvAvpGpioReleasePinHandle(
        NvAvpGpioHandle *phGpio,
        NvAvpGpioPinHandle *hPin);

NvError NvAvpGpioWritePin(
        NvAvpGpioHandle *phGpio,
        NvAvpGpioPinHandle *hPin,
        NvAvpGpioPinState *pPinState);

NvError NvAvpGpioReadPin(
        NvAvpGpioHandle *phGpio,
        NvAvpGpioPinHandle *hPin,
        NvAvpGpioPinState *pPinState);

NvError NvAvpGpioAcquirePinHandle(
        NvAvpGpioHandle *phGpio,
        NvU32 port,
        NvU32 pinNumber,
        NvAvpGpioPinHandle *phPin);

NvError NvAvpGpioConfigPin(
        NvAvpGpioHandle *phGpio,
        NvAvpGpioPinHandle *hPin,
        NvAvpGpioPinMode Mode);

void NvAvpGpioSuspend(void);

void NvAvpGpioResume(void);
#endif
