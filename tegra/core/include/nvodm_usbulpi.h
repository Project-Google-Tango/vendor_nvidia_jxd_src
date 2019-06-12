/*
 * Copyright (c) 2009 NVIDIA Corporation.  All Rights Reserved.
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
 *         USB ULPI Interface</b>
 *
 * @b Description: Defines the ODM interface for USB ULPI device.
 */

#ifndef INCLUDED_NVODM_USBULPI_H
#define INCLUDED_NVODM_USBULPI_H


/**
 * @defgroup nvodm_usbulpi USB ULPI Adaptation Interface
 *
 * This is the USB ULPI ODM adaptation interface, which
 * handles the abstraction of opening and closing of the USB ULPI device.
 * For NVIDIA Driver Development Kit (NvDDK) clients, USB ULPI device
 * means a USB controller connected to a ULPI interface that has an
 * external phy. This API allows NvDDK clients to open the USB ULPI device by
 * setting the ODM specific clocks to ULPI controller or external phy, so that USB ULPI
 * device can be used.
 *
 * @ingroup nvodm_adaptation
 * @{
 */
#if defined(_cplusplus)
extern "C"
{
#endif

#include "nvcommon.h"


/**
 * Defines the USB ULPI context.
 */
typedef struct NvOdmUsbUlpiRec * NvOdmUsbUlpiHandle;

/**
 * Opens the USB ULPI device by setting the ODM-specific clocks 
 * and/or settings related to USB ULPI controller and external phy.
 * @param Instance The ULPI instance number.
 * @return A USB ULPI device handle on success, or NULL on failure.
*/
NvOdmUsbUlpiHandle  NvOdmUsbUlpiOpen(NvU32 Instance);

/** 
 * Closes the USB ULPI device handle by clearing 
 * the related ODM-specific clocks and settings.
 * @param hUsbUlpi A handle to USB ULPI device.
*/
void NvOdmUsbUlpiClose(NvOdmUsbUlpiHandle hUsbUlpi);


#if defined(_cplusplus)
}
#endif

/** @} */

#endif // INCLUDED_NVODM_USBULPI_H

