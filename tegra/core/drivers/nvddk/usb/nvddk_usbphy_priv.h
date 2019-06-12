/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
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
 *           NvDDK USB PHY functions</b>
 *
 * @b Description: Defines USB PHY private functions
 *
 */

#ifndef INCLUDED_NVDDK_USBPHY_PRIV_H
#define INCLUDED_NVDDK_USBPHY_PRIV_H

#include "nvddk_usbphy.h"
#include "nvodm_query.h"
#include "nvodm_query_discovery.h"
#include "nvodm_usbulpi.h"
#include "nvrm_power.h"
#include "nvassert.h"
#include "nvrm_memmgr.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Minimum system frequency required for USB to work in High speed mode
 */
enum {USB_HW_MIN_SYSTEM_FREQ_KH = 100000};

/**
 * Wait time(1 second) for controller H/W status to change before giving up.
 */
enum {USB_PHY_HW_TIMEOUT_US = 1000000};

/**
 * USB Phy capabilities structure
 */
typedef struct NvDdkUsbPhyCapabilitiesRec
{
    /// Inidcates USB phy share the common clock and reset.
    NvBool PhyRegInController;
    /// Inidcates USB phy share the common clock and reset.
    NvBool CommonClockAndReset;
    /// Indicates USB phy pll programming is in usb reg space.
    NvBool PllRegInController;
} NvDdkUsbPhyCapabilities;

/**
 * USB UTMIP Pad config control structure
 */
typedef struct NvDdkUsbPhyUtmiPadConfigRec
{
    // Usb controller virtual address
    volatile NvU32 *pVirAdr;
    // Usb controller bank size
    NvU32 BankSize;
    // Utmi Pad On reference count
    NvU32 PadOnRefCount;
} NvDdkUsbPhyUtmiPadConfig;

/**
 * Usb Phy record structure.
 */
typedef struct NvDdkUsbPhyRec
{
    // RM device handle
    NvRmDeviceHandle hRmDevice;
    // Usb controller virtual address
    volatile NvU32 *UsbVirAdr;
    // Usb controller bank size
    NvU32 UsbBankSize;
    // Misc virtual address
    volatile NvU32 *MiscVirAdr;
    // Misc bank size
    NvU32 MiscBankSize;
    // Usb phy open reference count
    NvU32  RefCount;
    // instance number
    NvU32 Instance;
    // capabilities structure
    NvDdkUsbPhyCapabilities Caps;
    // Usb odm property
    const NvOdmUsbProperty *pProperty;
    // Power Manager semaphore.
    NvOsSemaphoreHandle hPwrEventSem;
    // Id returned from driver's registration with Power Manager
    NvU32 RmPowerClientId;
    // Ulpi Handle
    NvOdmUsbUlpiHandle hOdmUlpi;
    // guid
    NvU64 Guid;
    // peripheral connectivity
    NvOdmPeripheralConnectivity const *pConnectivity;
    // Indicates Phy is Powered up
    NvBool IsPhyPoweredUp;
    // Utmpi Pad Config control structure
    NvDdkUsbPhyUtmiPadConfig *pUtmiPadConfig;
    // Set of function pointers to access the usb phy hardware interface.
    // Pointer to the h/w specific PowerUp function.
    NvError (*PowerUp)(NvDdkUsbPhyHandle hUsbPhy);
    // Pointer to the h/w specific PowerDown function.
    NvError (*PowerDown)(NvDdkUsbPhyHandle hUsbPhy);
    // Pointer to the h/w specific WaitForStableClock function.
    NvError (*WaitForStableClock)(NvDdkUsbPhyHandle hUsbPhy);
    // Pointer to the h/w specific CloseHwInterface function.
    void (*CloseHwInterface)(NvDdkUsbPhyHandle hUsbPhy);
    // Pointer to the h/w specific Ioctl function.
    NvError (*Ioctl)(NvDdkUsbPhyHandle hUsbPhy,
        NvDdkUsbPhyIoctlType IoctlType,
        const void *pInputArgs,
        void *pOutputArgs);
} NvDdkUsbPhy;

/**
 * Opens the T124 specifi H/W Usb Phy interface.
 *
 * @param hUsbPhy handle to the USB phy.
 *
 * @retval None
 */
void T124UsbPhyOpenHwInterface(NvDdkUsbPhy *pUsbPhy);

#if defined(__cplusplus)
}
#endif

/** @}*/
#endif // INCLUDED_NVDDK_USBPHY_H

