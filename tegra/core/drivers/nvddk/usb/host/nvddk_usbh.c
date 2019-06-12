/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All rights reserved.
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
 *           NvDDK USB-Host APIs</b>
 *
 * @b Description: Defines Interface for NvDDK USB Host module.
 *
 */

#define NV_ENABLE_DEBUG_PRINTS 0

#include "nvddk_usbh_priv.h"
#include "nvassert.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "nvrm_pinmux.h"
#include "nvodm_pmu.h"
#include "nvrm_pmu.h"
#include "nvodm_query_gpio.h"

NvError
NvDdkUsbhOpen(
    NvRmDeviceHandle hRmDevice,
    NvDdkUsbhHandle* hUsbh,
    NvOsSemaphoreHandle UsbEventSema,
    NvU32 Instance)
{
    NvError e = NvSuccess;
    NvU32 MaxInstances = 0;
    NvDdkUsbhHandle pUsbhHandle = NULL;
    const NvOdmGpioPinInfo *pUsbhGpioPinInfo  = NULL;
    NvU32 GpioPinCount = 0;
    const NvOdmUsbProperty *pUsbProperty = NULL;
    NvRmPhysAddr PhysAddr;

    NV_ASSERT(hRmDevice);

    MaxInstances = NvRmModuleGetNumInstances(hRmDevice, NvRmModuleID_Usb2Otg);
    NV_ASSERT(Instance < MaxInstances);

    // Initilization of USB HCD Handle
    pUsbhHandle = NvOsAlloc(sizeof(NvDdkUsbHcd));
    if (pUsbhHandle == NULL)
        return NvError_InsufficientMemory;
    NvOsMemset(pUsbhHandle, 0,sizeof(NvDdkUsbHcd));

    pUsbhHandle->hRmDevice = hRmDevice;
    pUsbhHandle->Instance = Instance;

    // Query for USB Interface Type
    pUsbProperty =
             NvOdmQueryGetUsbProperty(NvOdmIoModule_Usb, pUsbhHandle->Instance);
    pUsbhHandle->UsbInterfaceType = pUsbProperty->UsbInterfaceType;

    // Get the controller base address
    NvRmModuleGetBaseAddress(
            hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, Instance),
            &PhysAddr,
            &pUsbhHandle->UsbOtgBankSize);

    NV_CHECK_ERROR_CLEANUP(NvRmPhysicalMemMap(
                PhysAddr,
                pUsbhHandle->UsbOtgBankSize,
                NVOS_MEM_READ_WRITE,
                NvOsMemAttribute_Uncached,
                (void **)&pUsbhHandle->UsbOtgVirAdr));

    pUsbhHandle->UsbhCableConnected = NV_FALSE;
    pUsbhHandle->ControllerStarted = NV_FALSE;
    pUsbhHandle->TranferDone = NV_FALSE;
    pUsbhHandle->Addr0ControlPipeAdded = NV_FALSE;
    pUsbhHandle->IsUsbSuspend = NV_FALSE;
    pUsbhHandle->InitializeModeFull = NV_FALSE;

    if (pUsbProperty->IdPinDetectionType == NvOdmUsbIdPinType_Gpio)
    {
        // Get GPIO handle
        NV_CHECK_ERROR_CLEANUP(NvRmGpioOpen(pUsbhHandle->hRmDevice,
                               &(pUsbhHandle->hUsbHcdGpioHandle)));

        pUsbhGpioPinInfo = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_Usb,
                                                Instance,
                                                &GpioPinCount);

     /* GpioPinCount or pUsbhGpioPinInfo should not be null, if
      * "IdPinDetectionType" is "GPIO". So need to modify
      * "NvOdmQueryGpioPinMap", hence returning errror as "Not implemented"
      */
        if (!GpioPinCount)
        {
            e = NvError_NotImplemented;
            NV_DEBUG_PRINTF(("Not implemented\n"));
            goto fail;
        }
    }

    if (UsbEventSema)
        pUsbhHandle->EventSema = UsbEventSema;

    UsbhPrivGetCapabilities(pUsbhHandle);
    UsbhPrivEnableVbus(pUsbhHandle, pUsbhHandle->Instance);

    NV_CHECK_ERROR_CLEANUP(NvDdkUsbPhyOpen(hRmDevice,
                           pUsbhHandle->Instance,
                           &pUsbhHandle->hUsbPhy));
    *hUsbh = pUsbhHandle;
    return e;

fail:

    // Clean up all memories allocated
    // UnMap the virtual Address
    NvOsPhysicalMemUnmap(pUsbhHandle->UsbOtgVirAdr,
                 pUsbhHandle->UsbOtgBankSize);
    NvRmGpioClose(pUsbhHandle->hUsbHcdGpioHandle);
    NvDdkUsbPhyClose(pUsbhHandle->hUsbPhy);

    NvOsMemset(pUsbhHandle, 0, sizeof(NvDdkUsbHcd));
    return e;
}

NvError NvDdkUsbhStart(NvDdkUsbhHandle hUsbh, NvBool IsInitialize)
{
    NvError e = NvSuccess;

    NV_ASSERT(hUsbh);

    if (!UsbPrivIsACableConnected(hUsbh))
    {
    /**
      * Tempararily use this.
      * Change NvError messages such that those should be usable by USBH and
      * USBF code
      */
        return NvError_UsbfCableDisConnected;
    }

    hUsbh->InitializeModeFull = IsInitialize;

    if (!hUsbh->ControllerStarted)
    {
        UsbhPrivConfigVbusInterrupt(hUsbh, NV_FALSE);
        // inform the PMU about host mode

// This API is not implemented for some PMICs Hence Commented out.
// TODO: Need to uncomment later.
#if 0
        NvRmPmuSetChargingCurrentLimit(
                hUsbh->hRmDevice,
                NvRmPmuChargingPath_UsbBus,
                NVODM_USB_HOST_MODE_LIMIT,
                NvOdmUsbChargerType_UsbHost);
#endif

        // Enabling the controller means coming out of suspend mode incase if it is in suspend mode
        hUsbh->IsUsbSuspend = NV_FALSE;

        if (IsInitialize)
        {
            e = UsbhPrivInitializeController(hUsbh);
            if (e != NvSuccess)
            {
                NV_DEBUG_PRINTF(
                    ("NvDdkUsbhStart:: UsbhPrivInitContrller Failed\n"));
            }
        }
    }
    if (e == NvSuccess)
    {
        hUsbh->ControllerStarted = NV_TRUE;
    }

    if (IsInitialize)
    {
        /* program port Speed Connect,incase  if force the port speed to FULL  */
        // USB_REG_UPDATE_NUM(PORTSC1, PFSC, hUsbh->portSpeedLimit);
        UsbhPrivAllocateBuffers(hUsbh);
        // enable the root hub port power
        usbhPrivConfigPortPower(hUsbh, NV_TRUE);
        // enable Async Schedule Park mode Enable functionality
        UsbhPrivEnableAsyncScheduleParkMode(hUsbh);
        // Clear all usb interrupts
        UsbhPrivClearInterrupts(hUsbh);
        // Register with Interrupt listener and enable the interrupt for USB.
        UsbhPrivRegisterInterrupts(hUsbh);

        // Write the register to enable USB int, error int, and port change ints.
        UsbhPrivEnableInterrupts(hUsbh);

        if (UsbhPrivRunController(hUsbh) != NvSuccess)
        {
            return NvError_Timeout;
        }
    }

    NV_DEBUG_PRINTF(("NvDdkUsbhStart:: start Success\n"));
    return e;

}

void NvDdkUsbhClose(NvDdkUsbhHandle hUsbh, NvBool IsInitialize)
{
    if (!hUsbh)
    {
        return;
    }
    if (IsInitialize)
    {
        /* Stop the host controller */
        if (UsbhPrivStopController(hUsbh) != NvSuccess)
        {
            NV_DEBUG_PRINTF(
                    ("NvDdkUsbhClose:: Error while stopping the controller\n"));
        }

        // Clean up all memories allocated
        UsbhPrivDeAllocateBuffers(hUsbh);

        NvRmInterruptUnregister(hUsbh->hRmDevice, hUsbh->InterruptHandle);
        hUsbh->InterruptHandle = NULL;
    }
    hUsbh->InitializeModeFull = NV_FALSE;

    hUsbh->ControllerStarted = NV_FALSE;

    // disabling VBUS interrupt
    UsbhPrivConfigVbusInterrupt(hUsbh, NV_FALSE);

    // UnMap the virtual Address
    NvOsPhysicalMemUnmap(hUsbh->UsbOtgVirAdr, hUsbh->UsbOtgBankSize);
    NvRmGpioClose(hUsbh->hUsbHcdGpioHandle);

    NvDdkUsbPhyClose(hUsbh->hUsbPhy);
    NvOsFree(hUsbh);
}

NvError NvDdkUsbhAddPipe(NvDdkUsbhHandle hUsbh, NvDdkUsbhPipeInfo *pAddPipe)
{
    UsbHcdQh *pNewHcdQh;
    NvError err = NvSuccess;
    NvU32 pipe;

    NV_ASSERT(hUsbh);
    NV_ASSERT(pAddPipe);
    NV_DEBUG_PRINTF(("USBHDDK::AddPipe::START\n"));

    pipe = CREATE_PIPE(pAddPipe->DeviceAddr, pAddPipe->EpNumber,
                       pAddPipe->EndpointType, pAddPipe->PipeDirection);

    switch (USB_PIPETYPE(pipe))
    {
        case NvDdkUsbhPipeType_Control:
        case NvDdkUsbhPipeType_Bulk:
            /** If addpipe is requested for ep 0 & Dev add 0,
                for more than once just return  with success **/
            if (!pAddPipe->DeviceAddr && !pAddPipe->EpNumber &&
                    (pAddPipe->EndpointType == NvDdkUsbhPipeType_Control))
            {
                if (hUsbh->Addr0ControlPipeAdded)
                {
                    return err;
                }
                else
                {
                    hUsbh->Addr0ControlPipeAdded = NV_TRUE;
                }
            }

            // Disable Async Shedule incase if it is enabled
            if (UsbhPrivIsAsyncSheduleEnabled(hUsbh))
            {
                // Stop Executing before modifiing the AS list
                UsbhPrivDisableAsyncShedule(hUsbh);
            }
            // Allocate memory for Queue Head
            if (UsbhPrivAllocateQueueHead(hUsbh, &pNewHcdQh) == NvSuccess)
            {
                NV_DEBUG_PRINTF(
                    ("USBHDDK::AddPipe:pNewHcdQh = 0x%x\n", pNewHcdQh));
                // Configure Queue head
                UsbhPrivConfigureQH(hUsbh, pNewHcdQh,
                                    pipe,
                                    pAddPipe->SpeedOfDevice,
                                    pAddPipe->MaxPktLength);
                // Add the new Queue head(mapped onto hardware) to the ASYNC LIST
                UsbhPrivAddToAsyncList(hUsbh, pNewHcdQh);
                // Set the decriptor to the new ED
                pAddPipe->EpPipeHandler = (NvU32)pNewHcdQh;

                NV_DEBUG_PRINTF(
                   ("USBHDDK::AddPipe::EpPipeHandler = 0x%x::NewHcdQh = 0x%x\n",
                                 pAddPipe->EpPipeHandler, pNewHcdQh));
                // Enable Asynchronous schedule
                UsbhPrivEnableAsyncShedule(hUsbh);
            }
            break;

        case NvDdkUsbhPipeType_Interrupt:
        case NvDdkUsbhPipeType_Isoc:
        default:
            // Need to work out
            break;
    }
    return NvSuccess;
}

NvError NvDdkUsbhDeletePipe(NvDdkUsbhHandle hUsbh, NvU32 DeleteEpPipeHandler)
{
    // Disable async shedule incase if it is enabled
    // Clear PIPE
    // if Td_Head is not null call abort transfer and
    // clear any pending transfers
    // set used  one as 0
    // Reach the given HcQH in the aysnc list  and remove this HcQh from the
    // list
    // Then enable the Async shedule
    NV_ASSERT(hUsbh);
    NV_ASSERT(DeleteEpPipeHandler);

    if (UsbhPrivIsAsyncSheduleEnabled(hUsbh))
    {
        // Stop Executing before modifiing the AS list
        UsbhPrivDisableAsyncShedule(hUsbh);
    }

    // Delete from async list
    UsbhPrivDeleteFromAsyncList(hUsbh, (UsbHcdQh *)DeleteEpPipeHandler);
    // Enable Asynchronous schedule
    UsbhPrivEnableAsyncShedule(hUsbh);

    return NvSuccess;
}

NvError NvDdkUsbhGetPortSpeed(NvDdkUsbhHandle hUsbh, NvU32 *pGetPortSpeed)
{
    *pGetPortSpeed = hUsbh->UsbPortSpeed;
    return NvSuccess;
}

NvError
NvDdkUsbhSubmitTransfer(
    NvDdkUsbhHandle hUsbh,
    NvDdkUsbhTransfer* pTransfer)
{
    UsbHcdQh* pHcdQh = NULL;

    NvU32 pipe = 0;
    NvError err = NvSuccess;
    NvU32 UsbEvents = NvDdkUsbhEvent_None;
    NvDdkUsbhTransfer *pCompletedTransfer = 0;

    NV_ASSERT(pTransfer);

    pHcdQh = (UsbHcdQh*)(pTransfer->EpPipeHandler);
    pipe = pHcdQh->Pipe;

    NV_DEBUG_PRINTF(("USBHDDK::SubmitTrans11:HPTRANSFER = 0x%x\n", pTransfer));

    switch (USB_PIPETYPE(pipe))
    {
        case NvDdkUsbhPipeType_Control:
        case NvDdkUsbhPipeType_Bulk:
            UsbhPrivAsyncSubmitTransfer(hUsbh, pTransfer, pipe);
            break;
        case NvDdkUsbhPipeType_Interrupt:
        case NvDdkUsbhPipeType_Isoc:
        default:
            break;
    }
    // Blocking mode of this funtion need to be handled incase client give
    while (1)
    {
        err = NvOsSemaphoreWaitTimeout(hUsbh->EventSema, 100);
        UsbEvents = NvDdkUsbhGetEvents(hUsbh, &pCompletedTransfer);

        if (
                (UsbEvents & NvDdkUsbhEvent_TransferDone) &&
                             (pTransfer == pCompletedTransfer))
        {
            break;
        }
    }
    return err;
}

NvError
NvDdkUsbhAbortTransfer(
    NvDdkUsbhHandle hUsbh,
    NvDdkUsbhTransfer* pTransfer)
{
    NvError errStatus = NvSuccess;
    return errStatus;
}

NvU32
NvDdkUsbhGetEvents(
    NvDdkUsbhHandle hUsbh,
    NvDdkUsbhTransfer **pTransfer)
{
    // NvError errStatus = NvSuccess;
    NvU32 UsbEvents = NvDdkUsbhEvent_None;

    NV_ASSERT(hUsbh);
    // Fill  all events generated    and return them.
    if (UsbPrivIsACableConnected(hUsbh))
    {
        if (hUsbh->UsbhCableConnected)
        {
            UsbEvents |= NvDdkUsbhEvent_CableConnect;
        }

        if (hUsbh->PortEnabled)
        {
            UsbEvents |= NvDdkUsbhEvent_PortEnabled;
        }
    }
    if (hUsbh->UsbhCableDisConnected)
    {
        UsbEvents |= NvDdkUsbhEvent_CableDisConnect;
    }

    if (hUsbh->UsbhCableConnected)
    {
        if (hUsbh->TranferDone)
        {
            UsbEvents |= NvDdkUsbhEvent_TransferDone;
            // If tranfer pointer is NULL
            if (pTransfer)
                *pTransfer = hUsbh->pCompeltedTransferListHead;
            hUsbh->TranferDone = NV_FALSE;
        }
    }
    return UsbEvents;
}

void NvDdkUsbhSuspend(NvDdkUsbhHandle hUsbh)
{

    NV_ASSERT(hUsbh);
    if ((hUsbh->Instance == ULPI_INSTANCE) &&
        (hUsbh->UsbInterfaceType == NvOdmUsbInterfaceType_UlpiNullPhy))
    {
        return;
    }
    /* Check whether it is already in suspend mode.
       SUSPEND only if it is not already in suspend. */
    if (hUsbh->IsUsbSuspend == NV_FALSE)
    {
        if (hUsbh->InitializeModeFull)
        {
            // suspend the port..
            UsbhPrivSuspendPort(hUsbh,NV_FALSE);
        }
        // Power down the Usb PHY
        NvDdkUsbPhyPowerDown(hUsbh->hUsbPhy, NV_TRUE);

        hUsbh->IsUsbSuspend = NV_TRUE;
        hUsbh->ControllerStarted = NV_FALSE;
        hUsbh->Addr0ControlPipeAdded = NV_FALSE;
    }

}

void NvDdkUsbhResume(NvDdkUsbhHandle hUsbh)
{
    NV_ASSERT(hUsbh);

    if ((hUsbh->Instance == ULPI_INSTANCE) &&
        (hUsbh->UsbInterfaceType == NvOdmUsbInterfaceType_UlpiNullPhy))
    {
        return;
    }

    // Resume only if  controller is suspended already
    if (hUsbh->IsUsbSuspend == NV_TRUE)
    {
        if (hUsbh->UsbInterfaceType == NvOdmUsbInterfaceType_Utmi)
        {
            UsbhPrivConfigVbusInterrupt(hUsbh, NV_FALSE);
        }
        // Power Up the USB phy
        NvDdkUsbPhyPowerUp(hUsbh->hUsbPhy, NV_TRUE);

        if (hUsbh->InitializeModeFull)
        {
            UsbhPrivInitializeController(hUsbh);
        }
        usbhPrivConfigPortPower(hUsbh, NV_TRUE);
        if (hUsbh->InitializeModeFull)
        {
            UsbhPrivEnableInterrupts(hUsbh);
            UsbhPrivRunController(hUsbh);
        }
        hUsbh->IsUsbSuspend = NV_FALSE;
    }
}


void NvDdkUsbhBusyHintsOnOff(NvDdkUsbhHandle hUsbh, NvBool OnOff)
{
    NvDdkUsbPhyIoctl_UsbBusyHintsOnOffInputArgs BusyHint;

    NV_ASSERT(hUsbh);
    BusyHint.OnOff = OnOff;
    BusyHint.BoostDurationMs = NV_WAIT_INFINITE;

    NV_ASSERT_SUCCESS(
            NvDdkUsbPhyIoctl(hUsbh->hUsbPhy,
            NvDdkUsbPhyIoctlType_UsbBusyHintsOnOff,
            &BusyHint, NULL));
}
