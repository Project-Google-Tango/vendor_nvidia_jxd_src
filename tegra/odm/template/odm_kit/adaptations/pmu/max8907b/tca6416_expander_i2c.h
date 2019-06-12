/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TCA6416_EXPANDER_I2C_H
#define INCLUDED_TCA6416_EXPANDER_I2C_H

#include "nvodm_pmu.h"
#include "max8907b.h"

#if defined(__cplusplus)
extern "C"
{
#endif



/**
 * @brief Defines the possible modes.
 */

typedef enum
{

    /**
     * Specifies the gpio pin as not in use. 
     */
    GpioPinMode_Inactive = 0,

    /// Specifies the gpio pin mode as input.
    GpioPinMode_InputData,

    /// Specifies the gpio pin mode as output.
    GpioPinMode_Output,

    GpioPinMode_Num,
    GpioPinMode_Force32 = 0x7FFFFFFF
} GpioPinMode;


/** 
 * @brief Defines the pin state
 */

typedef enum
{
   // Pin state high 
    GpioPinState_Low = 0,
    // Pin is high
    GpioPinState_High,
    GpioPinState_Num,
    GpioPinState_Force32 = 0x7FFFFFFF
} GpioPinState;


NvBool Tca6416ConfigPortPin(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 portNo,
    NvU32 pinNo,
    GpioPinMode Mode);

NvBool Tca6416WritePortPin(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 portNo,
    NvU32 pinNo,
    GpioPinState data);

NvBool Tca6416ReadPortPin(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 portNo,
    NvU32 pinNo, 
    GpioPinState*State);


#if defined(__cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_TCA6416_EXPANDER_I2C_H
