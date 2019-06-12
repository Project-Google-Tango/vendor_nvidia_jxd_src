/*
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Defines the parameters and data structure for USB3 devices.
 */

#ifndef INCLUDED_NVBOOT_USB3_PARAM_H
#define INCLUDED_NVBOOT_USB3_PARAM_H

#if defined(__cplusplus)
extern "C"
{
#endif

/// Params that are used to config and initialize Usb3 driver.
typedef struct NvBootUsb3ParamsRec
{
    NvU8 ClockDivider;
    /// Specifies the usbh root port number device is attached to
    NvU8 RootPortNumber;

    NvU8 PageSize2kor16k;

    /// used to config the OC pin on the port
    NvU8 OCPin;

    /// used to tri-state the associated vbus pad
    NvU8 VBusEnable;
} NvBootUsb3Params;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_USB3_PARAM_H */
