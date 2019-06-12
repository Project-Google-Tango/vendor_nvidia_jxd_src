/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           NvDDK USB otg  private functions</b>
 *
 * @b Description: Defines USB otg driver private functions
 *
 */

#ifndef INCLUDED_NVDDK_USBOTG_PRIV_H
#define INCLUDED_NVDDK_USBOTG_PRIV_H

#include "nvddk_usbotg.h"
#include "nvrm_interrupt.h"
#include "nvodm_query_gpio.h"
#include "nvrm_gpio.h"
#include "nvddk_usbphy.h"

#if defined(__cplusplus)
extern "C"
{
#endif


// Minimum system frequency required for USB to work in High speed mode
#define USB_HW_MIN_SYSTEM_FREQ_KH   100000


// Host Controller Driver  Structure
typedef struct NvDdkUsbOtgRec
{
    // RM device handle
    NvRmDeviceHandle hRmDevice;

     // Holds USB Cable Connect status
     // B cable connection status
    volatile NvBool UsbCableConnected;
     // A Cable connection status
    volatile NvBool UsbIdDetected;
    NvRmGpioPinState IDPinStatus;
    NvRmGpioPinHandle CableIdGpioPin;
    NvOsInterruptHandle InterruptHandle;
    // Client semaphore
    NvOsSemaphoreHandle hClientSema;
    NvRmGpioHandle hGpio;
    const NvOdmGpioPinInfo *GpioPinInfo;
    NvRmGpioInterruptHandle GpioIntrHandle;

    // Is controller is suspended 
    NvBool IsSuspended;
    // USB Instance used for OTG
    NvU32 UsbInstance;
    NvDdkUsbPhyHandle hUsbPhy;
}NvDdkUsbOtg;


/***********************************************************
 ************* definitions for private functions ***********
 ***********************************************************/


#endif   // INCLUDED_NVDDK_USBOTG_PRIV_H

