/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
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
 *           NvDDK AP20 USB Host private functions</b>
 *
 * @b Description: Implements T30 USB Host controller private functions
 *
 */

#include "t30/arusb.h"
#include "nvddk_usbh_priv.h"
#include "nvrm_hardware_access.h"


#define USB_REG_RD(reg)\
    NV_READ32(pUsbHcdCtxt->UsbOtgVirAdr + ((USB2_CONTROLLER_USB2D_##reg##_0)/4))

// Read perticular field value from reg mentioned
#define USB_DRF_VAL(reg, field, value) \
    NV_DRF_VAL(USB2_CONTROLLER_USB2D, reg, field, value)

#define USB_REG_READ_VAL(reg, field) \
    USB_DRF_VAL(reg, field, USB_REG_RD(reg))


void UsbhPrivT30StoreRegOffsets(NvDdkUsbhHandle pUsbHcdCtxt)
{
    pUsbHcdCtxt->RegOffsetArray[UsbhRegId_HCCPARAMS] = USB2_CONTROLLER_USB2D_HCCPARAMS_0;
    pUsbHcdCtxt->RegOffsetArray[UsbhRegId_USBCMD] = USB2_CONTROLLER_USB2D_USBCMD_0;
    pUsbHcdCtxt->RegOffsetArray[UsbhRegId_USBSTS] = USB2_CONTROLLER_USB2D_USBSTS_0;
    pUsbHcdCtxt->RegOffsetArray[UsbhRegId_USBINTR] = USB2_CONTROLLER_USB2D_USBINTR_0;
    pUsbHcdCtxt->RegOffsetArray[UsbhRegId_ASYNCLISTADDR] = USB2_CONTROLLER_USB2D_ASYNCLISTADDR_0;
    pUsbHcdCtxt->RegOffsetArray[UsbhRegId_PORTSC1] = USB2_CONTROLLER_USB2D_PORTSC1_0;
    pUsbHcdCtxt->RegOffsetArray[UsbhRegId_OTGSC] = USB2_CONTROLLER_USB2D_OTGSC_0;
    pUsbHcdCtxt->RegOffsetArray[UsbhRegId_USBMODE] = USB2_CONTROLLER_USB2D_USBMODE_0;
    pUsbHcdCtxt->RegOffsetArray[UsbhRegId_HCSPARAMS] = USB2_CONTROLLER_USB2D_HCSPARAMS_0;
}

void UsbhPrivT30GetPortSpeed(NvDdkUsbhHandle pUsbHcdCtxt)
{
    pUsbHcdCtxt->UsbPortSpeed = (NvDdkUsbPortSpeedType)USB_REG_READ_VAL(HOSTPC1_DEVLC, PSPD)+1;
}

