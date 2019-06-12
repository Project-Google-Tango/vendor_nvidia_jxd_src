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
 *           NvDDK USB-Otg APIs</b>
 *
 * @b Description: Defines Interface for NvDDK USB Otg module.
 *
 */

#define NV_ENABLE_DEBUG_PRINTS 0

#include "nvddk_usbotg.h"
#include "nvddk_usbotg_priv.h"
#include "nvassert.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "nvodm_query_gpio.h"
#include "nvrm_analog.h"
#include "nvodm_query_discovery.h"
#include "nvrm_pmu.h"


#define USB_DEBOUNCE_TIME_MS 10

/* Staic local functions  definations */
static void UsbOtgISR(void *arg);
static NvError UsbOtgRegisterInterrupts(NvDdkUsbOtgHandle hUsbOtg);
static NvError UsbOtgUnregisterInterrupts(NvDdkUsbOtgHandle hUsbOtg);
static void UsbOtgAckInterrupts (NvDdkUsbOtgHandle hUsbOtg);

static void GpioInterruptHandler(void *arg);
static NvError UsbOtgRegisterIDPinInterrupts(NvDdkUsbOtgHandle hUsbOtg);
static NvError UsbOtgUnregisterIDPinInterrupts(NvDdkUsbOtgHandle hUsbOtg);

static void UsbOtgConfigVbusInterrupt(NvDdkUsbOtgHandle hUsbOtg, NvBool Enable);
static NvBool UsbOtgIsCableConnected(NvDdkUsbOtgHandle hUsbOtg);
static void UsbOtgConfigIdInterrupt(NvDdkUsbOtgHandle hUsbOtg, NvBool Enable);
static NvBool UsbOtgIsIdPinSetToLow(NvDdkUsbOtgHandle hUsbOtg);
static void UsbOtgNotifyPendingRequests(NvDdkUsbOtgHandle hUsbOtg);



NvError
NvDdkUsbOtgOpen(
    NvRmDeviceHandle hRmDevice,
    NvOsSemaphoreHandle hUsbClientSema,
    NvDdkUsbOtgHandle* hUsbOtg)
{
    NvError e = NvSuccess;
    NvDdkUsbOtgHandle pUsbOtgHandle = NULL;
    
    NV_ASSERT(hRmDevice);

    /*  Initilization of USB Otg Handle */
    pUsbOtgHandle = NvOsAlloc(sizeof(NvDdkUsbOtg));
    if (pUsbOtgHandle == NULL)
    {
        return NvError_InsufficientMemory;
    }
    else
    {
        NvOsMemset(pUsbOtgHandle, 0, sizeof(NvDdkUsbOtg));
        pUsbOtgHandle->IDPinStatus = NvRmGpioPinState_High;
        pUsbOtgHandle->UsbCableConnected = NV_FALSE;
        pUsbOtgHandle->hRmDevice = hRmDevice;
        pUsbOtgHandle->IsSuspended = NV_FALSE;
        // At this moment instance "0" will be used as OTG
        pUsbOtgHandle->UsbInstance = 0;

        if (hUsbClientSema)
        {
            pUsbOtgHandle->hClientSema = hUsbClientSema;
        }

        NV_CHECK_ERROR_CLEANUP(
            NvDdkUsbPhyOpen(hRmDevice, pUsbOtgHandle->UsbInstance, &pUsbOtgHandle->hUsbPhy));
        NV_CHECK_ERROR_CLEANUP(NvDdkUsbOtgRegisterInterrupts(pUsbOtgHandle,NV_TRUE));

    }

    *hUsbOtg = pUsbOtgHandle;
    return e;

fail:
    if(pUsbOtgHandle)
    {
        // unregister interrupts
        NV_ASSERT_SUCCESS(NvDdkUsbOtgRegisterInterrupts(pUsbOtgHandle, NV_FALSE));
        NvDdkUsbPhyClose(pUsbOtgHandle->hUsbPhy);
        NvOsFree(pUsbOtgHandle);
    }
    *hUsbOtg = NULL;
    return e;
}

void NvDdkUsbOtgClose(NvDdkUsbOtgHandle hUsbOtg)
{

    if (!hUsbOtg)
    {
        return;
    }
    
    NV_ASSERT_SUCCESS(NvDdkUsbOtgRegisterInterrupts(hUsbOtg, NV_FALSE));
    NvDdkUsbPhyClose(hUsbOtg->hUsbPhy);

    NvOsFree(hUsbOtg);
    hUsbOtg = NULL;
    
}

void NvDdkUsbOtgSuspend(NvDdkUsbOtgHandle hUsbOtg, NvBool IsDpd)
{
    NV_ASSERT(hUsbOtg);

    if(hUsbOtg->IsSuspended)
    {
        return;
    }
    else
    {
        // Power down the Usb PHY
        NV_ASSERT_SUCCESS(NvDdkUsbPhyPowerDown(hUsbOtg->hUsbPhy, IsDpd));

        // Enable VBUS interrupt for cable detection
        UsbOtgConfigVbusInterrupt(hUsbOtg, NV_TRUE);

        hUsbOtg->IsSuspended = NV_TRUE;
    }

}

void NvDdkUsbOtgResume(NvDdkUsbOtgHandle hUsbOtg, NvBool IsDpd)
{
    NV_ASSERT(hUsbOtg);

    if(!hUsbOtg->IsSuspended)
        return;

    // Power Up the USB phy
    NV_ASSERT_SUCCESS(NvDdkUsbPhyPowerUp(hUsbOtg->hUsbPhy, IsDpd));

    UsbOtgNotifyPendingRequests(hUsbOtg);

    hUsbOtg->IsSuspended = NV_FALSE;
}

NvError NvDdkUsbOtgRegisterInterrupts(NvDdkUsbOtgHandle hUsbOtg,NvBool enable)
{
    NvError e = NvSuccess;
    const NvOdmUsbProperty *pUsbProperty;

    pUsbProperty = NvOdmQueryGetUsbProperty(NvOdmIoModule_Usb, hUsbOtg->UsbInstance);
    if ((pUsbProperty != NULL) && 
        (pUsbProperty->ConMuxType == NvOdmUsbConnectorsMuxType_MicroAB_TypeA))
    {
        // For Mux connectors do not depend on interrupts
        return e;
    }
    
    if(enable)
    {
        e = UsbOtgRegisterInterrupts(hUsbOtg);
        if(e !=  NvSuccess)
            return e;
        e = UsbOtgRegisterIDPinInterrupts(hUsbOtg);

        UsbOtgNotifyPendingRequests(hUsbOtg);
    }
    else
    {
        e = UsbOtgUnregisterInterrupts(hUsbOtg);
        if(e !=  NvSuccess)
            return e;
        e = UsbOtgUnregisterIDPinInterrupts(hUsbOtg);
    }
    
    return e;
}

static void UsbOtgISR(void *arg)
{
    if(((NvDdkUsbOtgHandle)arg)->InterruptHandle)
    {
        UsbOtgAckInterrupts((NvDdkUsbOtgHandle)arg);
        NvRmInterruptDone(((NvDdkUsbOtgHandle)arg)->InterruptHandle);
    }
}

static void GpioInterruptHandler(void *arg)
{
     NvRmGpioPinState value = NvRmGpioPinState_High;
     NvError e;
     /* Read GPIO pin status */
     // Configure Cable ID Pin as Input Pin
    NV_CHECK_ERROR_CLEANUP(NvRmGpioConfigPins(((NvDdkUsbOtgHandle)arg)->hGpio, 
                                   &((NvDdkUsbOtgHandle)arg)->CableIdGpioPin, 1,
                                   NvRmGpioPinMode_InputData));
    NvRmGpioReadPins(((NvDdkUsbOtgHandle)arg)->hGpio, 
                                       &((NvDdkUsbOtgHandle)arg)->CableIdGpioPin, &value, 1);
    if(value == NvRmGpioPinState_Low)
    {
        if( ((NvDdkUsbOtgHandle)arg)->IDPinStatus != NvRmGpioPinState_Low)
        {
            if (((NvDdkUsbOtgHandle)arg)->hClientSema)
            {
                NvOsSemaphoreSignal(((NvDdkUsbOtgHandle)arg)->hClientSema);
            }
            ((NvDdkUsbOtgHandle)arg)->IDPinStatus = NvRmGpioPinState_Low;
        }
    }
    NvRmGpioInterruptDone(((NvDdkUsbOtgHandle)arg)->GpioIntrHandle);
fail:
    // Skip all operations and return if error encountered
    return;
}


static NvError UsbOtgRegisterInterrupts(NvDdkUsbOtgHandle hUsbOtg)
{
    NvRmDeviceHandle hDevice;
    NvU32 IrqList;
    NvOsInterruptHandler IntHandlers;
    
    if (hUsbOtg->InterruptHandle)
    {
        return NvSuccess;
    }
    hDevice = hUsbOtg->hRmDevice;
    IrqList = NvRmGetIrqForLogicalInterrupt(hDevice,
                                             NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbOtg->UsbInstance),
                                             0);
    IntHandlers = UsbOtgISR;

    return NvRmInterruptRegister(hDevice,
                                 1,
                                 &IrqList,
                                 &IntHandlers,
                                 hUsbOtg,
                                 &(hUsbOtg->InterruptHandle),
                                 NV_TRUE
                                 );
}

static NvError UsbOtgRegisterIDPinInterrupts(NvDdkUsbOtgHandle hUsbOtg)
{
    NvRmDeviceHandle hDevice;
    NvU32 IrqList;
    NvOsInterruptHandler IntHandlers;
    NvError e = NvSuccess;
    NvU32 PinCount;
    NvRmGpioHandle hGpio = NULL;
    NvOsInterruptHandler IntrHandler = (NvOsInterruptHandler)GpioInterruptHandler;
    const NvOdmUsbProperty *pUsbProperty;

    pUsbProperty = NvOdmQueryGetUsbProperty(NvOdmIoModule_Usb, hUsbOtg->UsbInstance);
    if ((pUsbProperty != NULL) && (pUsbProperty->IdPinDetectionType  == NvOdmUsbIdPinType_CableId))
    {
        if(!hUsbOtg->InterruptHandle)
        {
            hDevice = hUsbOtg->hRmDevice;
            IrqList = NvRmGetIrqForLogicalInterrupt(hDevice,
                                             NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, hUsbOtg->UsbInstance),
                                             0);
            IntHandlers = UsbOtgISR;

            e= NvRmInterruptRegister(hDevice,
                                 1,
                                 &IrqList,
                                 &IntHandlers,
                                 hUsbOtg,
                                 &(hUsbOtg->InterruptHandle),
                                 NV_TRUE
                                 );
        }
        // Control the USB Phy analog component for ensbling ID PIN  interrupt
        UsbOtgConfigIdInterrupt(hUsbOtg, NV_TRUE);
    }
    else
    {
        /* Register for ID pin interrupts */
        if(hUsbOtg->GpioIntrHandle) 
        {
                return NvSuccess;
        }
        NV_CHECK_ERROR(NvRmGpioOpen(hUsbOtg->hRmDevice, &hGpio));
    
        hUsbOtg->hGpio = hGpio;

        hUsbOtg->GpioPinInfo = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_Usb,
                                              0, &PinCount);

        if (hUsbOtg->GpioPinInfo != NULL)
        {

            NV_CHECK_ERROR(NvRmGpioAcquirePinHandle(hGpio,hUsbOtg->GpioPinInfo[NvOdmGpioPin_UsbCableId].Port,
                    hUsbOtg->GpioPinInfo[NvOdmGpioPin_UsbCableId].Pin, &hUsbOtg->CableIdGpioPin));

            e = NvRmGpioInterruptRegister(hGpio, hUsbOtg->hRmDevice, hUsbOtg->CableIdGpioPin,
                IntrHandler, NvRmGpioPinMode_InputInterruptLow,
                hUsbOtg, &hUsbOtg->GpioIntrHandle,USB_DEBOUNCE_TIME_MS);
            if (e == NvError_Success)
            {
                e = NvRmGpioInterruptEnable(hUsbOtg->GpioIntrHandle);
            }
        }
   }
    return e;
}

static NvError UsbOtgUnregisterInterrupts(NvDdkUsbOtgHandle hUsbOtg)
{

    // Unregister interrupt handle
    if(hUsbOtg->InterruptHandle)
    {
        NvRmInterruptUnregister(hUsbOtg->hRmDevice,
                hUsbOtg->InterruptHandle);
        
       hUsbOtg->InterruptHandle = NULL;
    }
     return NvSuccess;

}
static NvError UsbOtgUnregisterIDPinInterrupts(NvDdkUsbOtgHandle hUsbOtg)
{
    const NvOdmUsbProperty *pUsbProperty;
        
    pUsbProperty = NvOdmQueryGetUsbProperty(NvOdmIoModule_Usb, hUsbOtg->UsbInstance);
    if ((pUsbProperty != NULL) && (pUsbProperty->IdPinDetectionType  == NvOdmUsbIdPinType_CableId))
    {
        // Control the USB Phy analog component for ensbling ID PIN  interrupt
        UsbOtgConfigIdInterrupt(hUsbOtg, NV_FALSE);

        // Unregister interrupt handle
        if(hUsbOtg->InterruptHandle)
        {
            NvRmInterruptUnregister(hUsbOtg->hRmDevice,
                hUsbOtg->InterruptHandle);
        
            hUsbOtg->InterruptHandle = NULL;
        }
    }
    else if(hUsbOtg->GpioIntrHandle)
    {
        NvRmGpioInterruptUnregister(hUsbOtg->hGpio, hUsbOtg->hRmDevice, hUsbOtg->GpioIntrHandle);
        NvRmGpioReleasePinHandles(hUsbOtg->hGpio,&hUsbOtg->CableIdGpioPin,1);
        NvRmGpioClose(hUsbOtg->hGpio);
        hUsbOtg->GpioIntrHandle = NULL;
    }
    return NvSuccess;

}

static void UsbOtgAckInterrupts (NvDdkUsbOtgHandle  hUsbOtg)
{
    const NvOdmUsbProperty *pUsbProperty;

    // Assert if handle is not created
    NV_ASSERT(hUsbOtg);
    
    hUsbOtg->UsbCableConnected = UsbOtgIsCableConnected(hUsbOtg);

    if(hUsbOtg->UsbCableConnected)
    {
         // Disable VBUS interrupt
        UsbOtgConfigVbusInterrupt(hUsbOtg, NV_FALSE);

        if (hUsbOtg->hClientSema)
        {
            NvOsSemaphoreSignal(hUsbOtg->hClientSema);
        }
    }

    pUsbProperty = NvOdmQueryGetUsbProperty(NvOdmIoModule_Usb, hUsbOtg->UsbInstance);

    if ((pUsbProperty != NULL) && (pUsbProperty->IdPinDetectionType  == NvOdmUsbIdPinType_CableId))
    {
        hUsbOtg->UsbIdDetected = UsbOtgIsIdPinSetToLow(hUsbOtg);
        if(hUsbOtg->UsbIdDetected)
        {
            // Disable ID pin interrupt
            UsbOtgConfigIdInterrupt(hUsbOtg, NV_FALSE);
            if (hUsbOtg->hClientSema)
            {
                NvOsSemaphoreSignal(hUsbOtg->hClientSema);
            }
        }
    }

}

static void UsbOtgConfigVbusInterrupt(NvDdkUsbOtgHandle hUsbOtg, NvBool Enable)
{
    NvDdkUsbPhyIoctl_VBusInterruptInputArgs Vbus;

    Vbus.EnableVBusInterrupt = Enable;

    NV_ASSERT_SUCCESS(
        NvDdkUsbPhyIoctl(hUsbOtg->hUsbPhy, 
            NvDdkUsbPhyIoctlType_VBusInterrupt,
            &Vbus, NULL));
}


static NvBool UsbOtgIsCableConnected(NvDdkUsbOtgHandle hUsbOtg)
{
    NvDdkUsbPhyIoctl_VBusStatusOutputArgs Status;

    NV_ASSERT_SUCCESS(
        NvDdkUsbPhyIoctl(hUsbOtg->hUsbPhy, 
            NvDdkUsbPhyIoctlType_VBusStatus,
            NULL, &Status));

    return Status.VBusDetected;
}

static void UsbOtgConfigIdInterrupt(NvDdkUsbOtgHandle hUsbOtg, NvBool Enable)
{
    NvDdkUsbPhyIoctl_IdPinInterruptInputArgs Id;

    Id.EnableIdPinInterrupt = Enable;

    NV_ASSERT_SUCCESS(
        NvDdkUsbPhyIoctl(hUsbOtg->hUsbPhy, 
            NvDdkUsbPhyIoctlType_IdPinInterrupt,
            &Id, NULL));
}


static NvBool UsbOtgIsIdPinSetToLow(NvDdkUsbOtgHandle hUsbOtg)
{
    NvDdkUsbPhyIoctl_IdPinStatusOutputArgs IdStatus;

    NV_ASSERT_SUCCESS(
        NvDdkUsbPhyIoctl(hUsbOtg->hUsbPhy, 
            NvDdkUsbPhyIoctlType_IdPinStatus,
            NULL, &IdStatus));

    return IdStatus.IdPinSetToLow;
}
static void UsbOtgNotifyPendingRequests(NvDdkUsbOtgHandle hUsbOtg)
{
    const NvOdmUsbProperty *pUsbProperty;
    
    pUsbProperty = NvOdmQueryGetUsbProperty(NvOdmIoModule_Usb, hUsbOtg->UsbInstance);
    hUsbOtg->UsbCableConnected = UsbOtgIsCableConnected(hUsbOtg);

    if(hUsbOtg->UsbCableConnected)
    {
        if (hUsbOtg->hClientSema)
        {
            NvOsSemaphoreSignal(hUsbOtg->hClientSema);
        }
    }

    if ((pUsbProperty != NULL) && (pUsbProperty->IdPinDetectionType  == NvOdmUsbIdPinType_CableId))
    {
        hUsbOtg->UsbIdDetected = UsbOtgIsIdPinSetToLow(hUsbOtg);
        if(hUsbOtg->UsbIdDetected)
        {
            if (hUsbOtg->hClientSema)
            {
                NvOsSemaphoreSignal(hUsbOtg->hClientSema);
            }
        }
    }
    return;
}

NvDdkUsbOtgOperationModeType 
    NvDdkUsbOtgGetOperationMode(
    NvDdkUsbOtgHandle hUsbOtg)
{
    NvRmGpioPinState value = NvRmGpioPinState_High;
    const NvOdmUsbProperty *pUsbProperty;

    pUsbProperty = NvOdmQueryGetUsbProperty(NvOdmIoModule_Usb, hUsbOtg->UsbInstance);

    // For Muxed connectors,  if no external VBUS connected, then run the controller in Host mode
    if ((pUsbProperty != NULL) && 
        (pUsbProperty->ConMuxType == NvOdmUsbConnectorsMuxType_MicroAB_TypeA))
    {
        if (UsbOtgIsCableConnected(hUsbOtg))
        {
            return NvDdkUsbOtgOperationModeType_Function;
        }
    }

    // Check ID Pin for Host Connection
    if ((pUsbProperty != NULL) && 
        (pUsbProperty->IdPinDetectionType  == NvOdmUsbIdPinType_Gpio))
    {
        /* Read ID  pin status */
        NvRmGpioReadPins(hUsbOtg->hGpio, &hUsbOtg->CableIdGpioPin, &value, 1);
        /* If the pin is active state low means - A Cable is connected  */
        if(value ==  NvRmGpioPinState_Low)
        {
            return NvDdkUsbOtgOperationModeType_Host;
        }
    }
    else 
    {
        /* Cable ID Status if controller supports*/
        if(UsbOtgIsIdPinSetToLow(hUsbOtg))
        {
            return NvDdkUsbOtgOperationModeType_Host;
        }
    }

    // Check VBUS status for device connection
    if (UsbOtgIsCableConnected(hUsbOtg))
    {
        return NvDdkUsbOtgOperationModeType_Function;
    }


    return NvDdkUsbOtgOperationModeType_None;
}
