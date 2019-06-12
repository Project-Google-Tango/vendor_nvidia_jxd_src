/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
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
 *           NvDDK USB-f APIs</b>
 *
 * @b Description: Defines Interface for NvDDK USB function module.
 *
 */

#include "nvddk_usbf.h"
#include "nvddk_usbf_priv.h"
#include "nvassert.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "nvodm_query_gpio.h"
#include "nvrm_analog.h"
#include "nvrm_pmu.h"
#include "nvrm_pinmux.h"

#define MAX_USB_INSTANCES 5

// USB function controller record stucture pointer
static NvDdkUsbFunction *s_pUsbFuncCtxt = NULL;
static NvOsMutexHandle   s_UsbMutex = NULL;

NvError
NvDdkUsbfOpen(
    NvRmDeviceHandle hRmDevice,
    NvDdkUsbfHandle* hUsbFunc,
    NvU32* pMaxTransferSize,
    NvOsSemaphoreHandle hUsbClientSema,
    NvU32 Instance)
{
    NvError e;
    NvU32 MaxInstances = 0;
    NvDdkUsbFunction *pUsb = NULL;
    NvOsMutexHandle UsbMutex = NULL;
    const NvOdmUsbProperty *pUsbProperty = NULL;
    NvRmModuleInfo info[MAX_USB_INSTANCES];
    NvU32 j;

    NV_ASSERT(hRmDevice);
    NV_ASSERT(hUsbFunc);
    NV_ASSERT(pMaxTransferSize);
    NV_ASSERT(Instance < MAX_USB_INSTANCES);

    NV_CHECK_ERROR(NvRmModuleGetModuleInfo( hRmDevice, NvRmModuleID_Usb2Otg, &MaxInstances, NULL ));
    if (MaxInstances > MAX_USB_INSTANCES)
    {
       // Ceil "instances" to MAX_USB_INSTANCES
       MaxInstances = MAX_USB_INSTANCES;
    }
    NV_CHECK_ERROR(NvRmModuleGetModuleInfo( hRmDevice, NvRmModuleID_Usb2Otg, &MaxInstances, info ));
    for (j = 0; j < MaxInstances; j++)
    {
    // Check whether the requested instance is present
        if(info[j].Instance == Instance)
            break;
    }
    // No match found return
    if (j == MaxInstances)
    {
        return NvError_ModuleNotPresent;
    }
    *hUsbFunc = NULL;

    if (!s_UsbMutex)
    {
        e = NvOsMutexCreate(&UsbMutex);
        if (e!=NvSuccess)
            return e;

        if (NvOsAtomicCompareExchange32(
                (NvS32*)&s_UsbMutex, 0, (NvS32)UsbMutex)!=0)
        {
            NvOsMutexDestroy(UsbMutex);
        }
    }

    NvOsMutexLock(s_UsbMutex);
    if (!s_pUsbFuncCtxt)
    {
        s_pUsbFuncCtxt = NvOsAlloc(MaxInstances * sizeof(NvDdkUsbFunction));
        if (s_pUsbFuncCtxt)
        {
            NvOsMemset(s_pUsbFuncCtxt,
                        0,
                        MaxInstances * sizeof(NvDdkUsbFunction));
        }
    }
    NvOsMutexUnlock(s_UsbMutex);
    if (!s_pUsbFuncCtxt)
        return NvError_InsufficientMemory;

    pUsb = &s_pUsbFuncCtxt[Instance];
    NvOsMutexLock(s_UsbMutex);
    if (pUsb->RefCount==0)
    {
        NvRmPhysAddr PhysAddr;
        NvOsMemset(pUsb, 0, sizeof(NvDdkUsbFunction));
        pUsb->Instance = Instance;
        pUsb->hRmDevice = hRmDevice;
        pUsb->RefCount = 1;

        NvRmModuleGetBaseAddress(hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_Usb2Otg, pUsb->Instance),
            &PhysAddr, &pUsb->UsbOtgBankSize);

        NV_CHECK_ERROR_CLEANUP(
            NvRmPhysicalMemMap(PhysAddr,
                pUsb->UsbOtgBankSize,
                NVOS_MEM_READ_WRITE,
                NvOsMemAttribute_Uncached,
                (void **)&pUsb->UsbOtgVirAdr));

        pUsb->IsUsbSuspend = NV_FALSE;
        pUsb->UsbfChargerType = NvOdmUsbChargerType_UsbHost;
        pUsb->ChargerDetected = NV_FALSE;
        pUsb->UsbPortSpeed = UsbPortSpeedType_Full;

        // Query for USB Interface Type
        pUsbProperty = NvOdmQueryGetUsbProperty(
                        NvOdmIoModule_Usb, pUsb->Instance);

        pUsb->UsbInterfaceType = pUsbProperty->UsbInterfaceType;

        pUsb->DummyChargerSupported = pUsbProperty->SupportedChargers &
                                      NvOdmUsbChargerType_Dummy ?
                                      NV_TRUE : NV_FALSE;

        if (hUsbClientSema)
        {
            NV_CHECK_ERROR_CLEANUP(
                NvOsSemaphoreClone(hUsbClientSema, &pUsb->hClientSema));
        }

        /* Initialize the USB phy */
        NV_CHECK_ERROR_CLEANUP(
            NvDdkUsbPhyOpen(hRmDevice, pUsb->Instance, &pUsb->hUsbPhy));

        /* Get the number of endpoints supported by the controller */
        pUsb->UsbfEndpointCount = UsbfPrivGetEndpointCount(pUsb);

        NV_CHECK_ERROR_CLEANUP(NvOsIntrMutexCreate(&pUsb->hIntrMutex));
        NV_CHECK_ERROR_CLEANUP(NvOsSemaphoreCreate(&pUsb->hTxferSema, 0));
        NV_CHECK_ERROR_CLEANUP(UsbPrivAllocateBuffers(pUsb, PhysAddr));
        NV_CHECK_ERROR_CLEANUP(UsbPrivRegisterInterrupts(pUsb));
        UsbPrivConfigureEndpoints(pUsb);

        NV_CHECK_ERROR_CLEANUP(NvDdkUsbfStart(pUsb));
    }
    else
    {
        pUsb->RefCount++;
    }
    *pMaxTransferSize = pUsb->UsbMaxTransferSize;
    *hUsbFunc = pUsb;
    NvOsMutexUnlock(s_UsbMutex);

    return NvSuccess;

fail:

    NvDdkUsbfClose(pUsb);
    NvOsMutexUnlock(s_UsbMutex);
    return e;
}

void NvDdkUsbfClose(NvDdkUsbfHandle hUsbf)
{
    NvDdkUsbFunction *pUsb = hUsbf;

    NV_ASSERT(s_UsbMutex != NULL);
    if (!hUsbf || !pUsb)
        return;

    NvOsMutexLock(s_UsbMutex);
    if (!pUsb->RefCount || !pUsb->hRmDevice)
    {
        NvOsMutexUnlock(s_UsbMutex);
        return;
    }

    --pUsb->RefCount;

    if (pUsb->RefCount)
    {
        NvOsMutexUnlock(s_UsbMutex);
        return;
    }

    NvRmInterruptUnregister(pUsb->hRmDevice, pUsb->InterruptHandle);
    NvOsPhysicalMemUnmap((void*)pUsb->UsbOtgVirAdr, pUsb->UsbOtgBankSize);
    UsbPrivDeAllocateBuffers(pUsb);
    NvOsSemaphoreDestroy(pUsb->hTxferSema);
    NvOsSemaphoreDestroy(pUsb->hClientSema);
    NvOsIntrMutexDestroy(pUsb->hIntrMutex);
    NvDdkUsbPhyClose(pUsb->hUsbPhy);

    NvOsMemset(pUsb, 0, sizeof(NvDdkUsbFunction));

    NvOsMutexUnlock(s_UsbMutex);
}

const NvDdkUsbfEndpoint *
NvDdkUsbfGetEndpointList(
    NvDdkUsbfHandle hUsbf,
    NvU32* epCount)
{
    NV_ASSERT(hUsbf && hUsbf->RefCount);
    NV_ASSERT(epCount);

    *epCount = hUsbf->UsbfEndpointCount;
    return &hUsbf->UsbfEndpoint[0];
}

NvError
NvDdkUsbfConfigureEndpoint(
    NvDdkUsbfHandle hUsbf,
    const NvDdkUsbfEndpoint *endpoint,
    NvBool configure)
{

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    if ((endpoint->handle == USB_EP_CTRL_OUT) ||
        (endpoint->handle == USB_EP_CTRL_IN))
    {
        // No need to store the control IN and OUT endpoint information
        // These two endpoints are fixed client have no rights to modify them
        // These two endpoints will be configured by driver.
        return NvSuccess;
    }
    if(!hUsbf->IsUsbSuspend)
        UsbPrivInitalizeEndpoints(hUsbf, endpoint, configure);

    return NvSuccess;
}

NvError
NvDdkUsbfTransmit(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint epIn,
    NvU8* dataBuf,
    NvU32* txfrBytes,
    NvBool EndTransfer,
    NvU32 WaitTimeoutMs)
{
    NvError retStatus = NvSuccess;
    NvU32 totalBytesToWrite;
    NvU32 endPoint;

    NV_ASSERT(hUsbf && hUsbf->RefCount);
    NV_ASSERT(dataBuf);
    NV_ASSERT(txfrBytes);

    if (!epIn.isIn)
    {
        return NvError_UsbInvalidEndpoint;
    }
    // Get the USB internal handle to the endpoint
    endPoint = epIn.handle;

    totalBytesToWrite = *txfrBytes;
    // Zero out the tranfer bytes before the transfer
    *txfrBytes = 0;

    // Check resquest size is more than the USB buffer size
    if (totalBytesToWrite > USB_MAX_TXFR_SIZE_BYTES)
    {
        totalBytesToWrite = USB_MAX_TXFR_SIZE_BYTES;
    }

    retStatus = UsbPrivEpTransmit(hUsbf, endPoint, dataBuf,
                    &totalBytesToWrite, EndTransfer, WaitTimeoutMs);

    // Assign the bytres actually transfered via USB
    *txfrBytes = totalBytesToWrite;

    return retStatus;
}

NvError
NvDdkUsbfReceiveStart(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint EpOut,
    NvU32 maxTxfrBytes,
    NvBool EnableShortPacketTermination)
{
    NvError retStatus = NvSuccess;
    NvU32 endPoint;
    NvU32 MemBufferAddr;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    if (EpOut.isIn)
    {
        return NvError_UsbInvalidEndpoint;
    }

    // Get the USB internal handle to the endpoint
    endPoint = EpOut.handle;

    // Check resquest size is more than the USB buffer size
    if (maxTxfrBytes > USB_MAX_TXFR_SIZE_BYTES)
    {
        maxTxfrBytes = USB_MAX_TXFR_SIZE_BYTES;
    }

    // Clear any pending transfers on this endpoint
    retStatus = UsbPrivTxfrClear(hUsbf, endPoint);
    if (retStatus != NvSuccess)
    {
        return retStatus;
    }

    MemBufferAddr = UsbPrivGetFreeMemoryBlock(hUsbf, maxTxfrBytes, endPoint);

    if (MemBufferAddr)
    {
        // Mark the buffer as used before starting the transaction
        UsbPrivTxfrLockBuffer(hUsbf, endPoint, MemBufferAddr, maxTxfrBytes);

        UsbPrivTxfrStart(hUsbf, endPoint, maxTxfrBytes,
            MemBufferAddr, NV_FALSE, EnableShortPacketTermination, NV_TRUE);
    }
    else
    {
        retStatus = NvError_InsufficientMemory;
    }


    return retStatus;
}

NvError
NvDdkUsbfTransmitStart(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint EpIn,
    NvU32 maxTxfrBytes,
    NvU8 *pBuffer,
    NvBool EndTransfer)
{
    NvError retStatus = NvSuccess;
    NvU32 endPoint;
    NvU32 MemBufferAddr = 0;
    NvU8* pVirtualBuffer = NULL;
    NvBool EnableZlt = NV_FALSE;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    // Allowing zero byte transfers, assert only if there is any bytes
    // to transfer and buffer is NULL
    if (maxTxfrBytes)
    {
        NV_ASSERT(pBuffer);
    }

    if (!EpIn.isIn)
    {
        return NvError_UsbInvalidEndpoint;
    }

    // Get the USB internal handle to the endpoint
    endPoint = EpIn.handle;

    // Check resquest size is more than the USB buffer size
    if (maxTxfrBytes > USB_MAX_TXFR_SIZE_BYTES)
    {
        maxTxfrBytes = USB_MAX_TXFR_SIZE_BYTES;
    }

    // Clear any pending transfers on this endpoint
    retStatus = UsbPrivTxfrClear(hUsbf, endPoint);
    if (retStatus != NvSuccess)
    {
        return retStatus;
    }

    MemBufferAddr = UsbPrivGetFreeMemoryBlock(hUsbf, maxTxfrBytes, endPoint);

    if (MemBufferAddr)
    {
        // Copy only if there is any data to transfer other than zero byte.
        if (maxTxfrBytes)
        {
            // If this is the end oftransfer for this transaction,
            // check for Zero packet enabling, if total bytes is multiple of
            // MaxPacket Size then enable zero packet at the end of transfer
            if ((EndTransfer) &&
                !(maxTxfrBytes % UsbPrivGetPacketSize(hUsbf, endPoint)))
            {
                EnableZlt = NV_TRUE;
            }
            // Mark the buffer as used before starting the transaction
            UsbPrivTxfrLockBuffer(hUsbf, endPoint, MemBufferAddr, maxTxfrBytes);
            pVirtualBuffer = UsbPrivGetVirtualBufferPtr(hUsbf, MemBufferAddr);
            NvOsMemcpy(pVirtualBuffer, pBuffer, maxTxfrBytes);
        }
        UsbPrivTxfrStart(hUsbf, endPoint, maxTxfrBytes,
            MemBufferAddr, NV_FALSE, EnableZlt, NV_TRUE);
    }
    else
    {
        retStatus = NvError_InsufficientMemory;
    }


    return retStatus;
}

NvError
NvDdkUsbfTransferWait(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint ep,
    NvU32 timeoutMs)
{
    NvError retStatus = NvSuccess;
    NvU32 endPoint;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    // Get the USB internal handle to the endpoint
    endPoint = ep.handle;

    retStatus = UsbPrivTxfrWait(hUsbf, endPoint, timeoutMs);

    return retStatus;
}
NvError
NvDdkUsbfTransferCancelAll(
    NvDdkUsbfHandle hUsbf)
{
    NvError retStatus = NvSuccess;
    NV_ASSERT(hUsbf && hUsbf->RefCount);
    if(!hUsbf->IsUsbSuspend)
    {
        retStatus =  UsbPrivTxfrClearAll(hUsbf);
    }
    return retStatus;
}

NvError
NvDdkUsbfTransferCancel(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint ep)
{
    NvU32 endPoint;
    NvError retStatus = NvSuccess;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    if(!hUsbf->IsUsbSuspend)
    {
        endPoint = ep.handle;
        retStatus = UsbPrivTxfrClear(hUsbf, endPoint);
    }
    return retStatus;
}

NvError
NvDdkUsbfEndpointStatus(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint ep)
{
    NvError retStatus = NvSuccess;
    NvU32 endPoint;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    if(!hUsbf->IsUsbSuspend)
    {
        endPoint = ep.handle;
        retStatus = UsbPrivEpGetStatus(hUsbf, endPoint);
    }
    return retStatus;
}

void
NvDdkUsbfEpSetStalledState(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint ep,
    NvBool stallEndpoint)
{
    NvU32 endPoint;

    NV_ASSERT(hUsbf && hUsbf->RefCount);
    endPoint = ep.handle;

    UsbPrivEpSetStalledState(hUsbf, endPoint, stallEndpoint);
}

NvError
NvDdkUsbfStart(NvDdkUsbfHandle hUsbf)
{
    NvError e = NvSuccess;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    if (!hUsbf->ControllerStarted)
    {
        NV_CHECK_ERROR_CLEANUP(
            UsbPrivIntializeController(hUsbf));
        hUsbf->ControllerStarted = NV_TRUE;
    }
    else
    {
        if (hUsbf->UsbResetDetected)
        {
            NV_DEBUG_PRINTF(("UsbEventType_Reset\n"));
            UsbPrivStatusClearOnReset(hUsbf);
            hUsbf->UsbResetDetected = NV_FALSE;
            NV_CHECK_ERROR_CLEANUP(
                UsbPrivEndPointFlush(hUsbf, s_USB_FLUSH_ALL_EPS));
        }
    }

fail:

    return e;
}

NvError
NvDdkUsbfReceive(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint epOut,
    NvU8* dataBuf,
    NvU32* rcvdBytes,
    NvBool EnableShortPacketTermination,
    NvU32 WaitTimeoutMs)
{
    NvError retStatus = NvSuccess;
    NvU32 totalBytesToRead;
    NvU32 endPoint;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    if(*rcvdBytes != 0)
    {
       NV_ASSERT(dataBuf);
    }
    if (epOut.isIn)
    {
        return NvError_UsbInvalidEndpoint;
    }

    endPoint = epOut.handle;
    totalBytesToRead = *rcvdBytes;

    if ((endPoint == USB_EP_CTRL_OUT) &&
        (totalBytesToRead == USB_SETUP_PKT_SIZE) &&
        (hUsbf->SetupPacketDetected))
    {
        NvOsMemcpy((void *)dataBuf,
                   (void *)&hUsbf->setupPkt, USB_SETUP_PKT_SIZE);
        hUsbf->SetupPacketDetected = NV_FALSE;
    }
    else
    {
        // Check resquest size is more than the USB buffer size
        if (totalBytesToRead > USB_MAX_TXFR_SIZE_BYTES)
        {
            totalBytesToRead = USB_MAX_TXFR_SIZE_BYTES;
        }
        retStatus = UsbPrivReceive(hUsbf, endPoint, dataBuf, &totalBytesToRead,
                        EnableShortPacketTermination, WaitTimeoutMs);
    }

    if (retStatus == NvSuccess)
    {
        *rcvdBytes = totalBytesToRead;
    }
    else
    {
        *rcvdBytes = 0;
    }

    return retStatus;
}

NvError
NvDdkUsbfAckEndpoint(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfEndpoint Endpoint)
{
    NvU32 endPoint;
    NvError retStatus = NvSuccess;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    endPoint = Endpoint.handle;
    if(!hUsbf->IsUsbSuspend)
        retStatus = UsbPrivAckEndpoint(hUsbf, endPoint);
    return retStatus;
}

void
NvDdkUsbfSetAddress(
    NvDdkUsbfHandle hUsbf,
    NvU32 Address)
{
    NV_ASSERT(hUsbf && hUsbf->RefCount);
    UsbPrivSetAddress(hUsbf, Address);
}

NvU64
NvDdkUsbfGetEvents(NvDdkUsbfHandle hUsbf)
{
    NvU64 UsbEvents = NvDdkUsbfEvent_None;
    NvU32 EndPoint;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    if (hUsbf->UsbfCableConnected)
    {
        UsbEvents |= NvDdkUsbfEvent_CableConnect;

        if (hUsbf->UsbResetDetected)
        {
            UsbEvents |= NvDdkUsbfEvent_BusReset;
            // Reset received, means we are connected to USB host
            hUsbf->UsbfChargerType = NvOdmUsbChargerType_UsbHost;
        }
        else if (hUsbf->ChargerDetected)
        {
            UsbEvents |= hUsbf->UsbfChargerType;
        }
        else
        {
            for (EndPoint=0; EndPoint<USB_EP_MISC; EndPoint++)
            {
                if (hUsbf->EpTxferCompleted[EndPoint] &&
                    hUsbf->UsbDescriptorBuf.EpConfigured[EndPoint])
                {
                    UsbEvents |= (1ULL<<(NvDdkUsbfEvent_EpTxfrCompleteBit+EndPoint));
                    if (USB_IS_EP_IN(EndPoint))
                    {
                        hUsbf->EpTxferCompleted[EndPoint] = NV_FALSE;
                        // IN transfer is completed now Unlock the buffers
                        UsbPrivTxfrUnLockBuffer(hUsbf, EndPoint);
                    }
                }
            }

            for (;EndPoint<hUsbf->UsbfEndpointCount; EndPoint++)
            {
                if (hUsbf->EpTxferCompleted[EndPoint])
                {
                    UsbEvents |= (1ULL<<(NvDdkUsbfEvent_EpTxfrCompleteBit+EndPoint));
                    if (USB_IS_EP_IN(EndPoint))
                    {
                        hUsbf->EpTxferCompleted[EndPoint] = NV_FALSE;
                        // IN transfer is completed now Unlock the buffers
                        UsbPrivTxfrUnLockBuffer(hUsbf, EndPoint);
                    }
                }
            }

            if (hUsbf->SetupPacketDetected)
                UsbEvents |= NvDdkUsbfEvent_SetupPacket;

            if (hUsbf->UsbPortSpeed == UsbPortSpeedType_High)
                UsbEvents |= NvDdkUsbfEvent_PortHighSpeed;
            else
                UsbEvents |= NvDdkUsbfEvent_PortFullSpeed;
        }
    }
    else
    {
        hUsbf->UsbfChargerType = NvOdmUsbChargerType_UsbHost;
        hUsbf->ControllerStarted = NV_FALSE;
    }

    if (hUsbf->PortSuspend)
    {
        UsbEvents |= NvDdkUsbfEvent_BusSuspend;
        hUsbf->PortSuspend = NV_FALSE;
    }

    return UsbEvents;
}

void NvDdkUsbfSuspend(NvDdkUsbfHandle hUsbf, NvBool IsDpd)
{
    NvDdkUsbFunction *pUsb = hUsbf;
    NvError ErrStatus = NvSuccess;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    if (hUsbf->IsUsbSuspend)
    {
        return;
    }

    // lock with ISR
    NvOsIntrMutexLock(pUsb->hIntrMutex);

    // Power down the Usb PHY
    ErrStatus = NvDdkUsbPhyPowerDown(hUsbf->hUsbPhy, IsDpd);
    if (ErrStatus == NvError_NotImplemented)
    {
        // unlock with ISR
        NvOsIntrMutexUnlock(pUsb->hIntrMutex);
        return;
    }

    // turn off the controller interrupts
    UsbfPrivDisableControllerInterrupts(hUsbf);

    // Enable VBUS interrupt for cable detection
    UsbfPrivConfigVbusInterrupt(hUsbf, NV_TRUE);

    if (hUsbf->UsbfCableConnected)
    {
        // If USB is suspended forcebly, while Cable is connected
        // Set the UsbfCableConnected to FALSE and Signal the client to read
        // the cable disconnect event through NvDdkUsbfGetEvents.
        hUsbf->UsbfCableConnected = NV_FALSE;
        if (hUsbf->hClientSema)
        {
            NvOsSemaphoreSignal(hUsbf->hClientSema);
        }
    }

    hUsbf->ControllerStarted = NV_FALSE;
    hUsbf->IsUsbSuspend = NV_TRUE;
    // unlock with ISR
    NvOsIntrMutexUnlock(pUsb->hIntrMutex);
}

void NvDdkUsbfResume(NvDdkUsbfHandle hUsbf, NvBool IsDpd)
{
    NvDdkUsbFunction *pUsb = hUsbf;

    NV_ASSERT(hUsbf && hUsbf->RefCount);

    if (!hUsbf->IsUsbSuspend)
        return;

    // lock  ISR
    NvOsIntrMutexLock(pUsb->hIntrMutex);

    if (!hUsbf->UsbfCableConnected)
    {
        /// Check cable is connected or not on resume
        if (UsbfPrivIsCableConnected(hUsbf))
        {
            hUsbf->UsbfCableConnected = NV_TRUE;
            // Since cable is not removed when system goes to Suspend,
            // VBUS state is not changed and hence interrupt will not
            // be generated. Signal the client to check the cable status.
            if (hUsbf->hClientSema)
            {
                NvOsSemaphoreSignal(hUsbf->hClientSema);
            }
        }
        else
        {
            /// Power down the USB phy if cable is not connected
            NV_ASSERT_SUCCESS(NvDdkUsbPhyPowerDown(hUsbf->hUsbPhy, NV_FALSE));
            // Enable VBUS interrupt for cable detection
            UsbfPrivConfigVbusInterrupt(hUsbf, NV_TRUE);
        }
        // unlock ISR
        NvOsIntrMutexUnlock(pUsb->hIntrMutex);
        return;
    }

    // Power Up the USB phy
    NV_ASSERT_SUCCESS(NvDdkUsbPhyPowerUp(hUsbf->hUsbPhy, IsDpd));

    hUsbf->IsUsbSuspend = NV_FALSE;

    // unlock ISR
    NvOsIntrMutexUnlock(pUsb->hIntrMutex);
}


void
NvDdkUsbfSetPowerLimit(
    NvDdkUsbfHandle hUsbf,
    NvU32 ChargingCurrentMa,
    NvU32 ChargerType)
{
    NV_ASSERT(hUsbf && hUsbf->RefCount);

    NvRmPmuSetChargingCurrentLimit(hUsbf->hRmDevice,
        NvRmPmuChargingPath_UsbBus, ChargingCurrentMa, ChargerType);
}

void
NvDdkUsbfSetTestMode(NvDdkUsbfHandle hUsbf,
                     NvU32 TestModeSelector)
{
    NV_ASSERT(hUsbf && hUsbf->RefCount);

    UsbPrivSetTestMode(hUsbf, TestModeSelector);
}

NvError
NvDdkUsbfGetCapabilities(
    NvDdkUsbfHandle hUsbf,
    NvDdkUsbfCapabilities *pUsbCap)
{
    NV_ASSERT(hUsbf);
    NV_ASSERT(pUsbCap);

    pUsbCap->EndPointCount = UsbfPrivGetEndpointCount(hUsbf);

    return NvSuccess;
}


NvOdmUsbChargerType NvDdkUsbfDetectCharger(NvDdkUsbfHandle hUsbf)
{
    NvOdmUsbChargerType ChargerType = NvOdmUsbChargerType_Dummy;
    NV_ASSERT(hUsbf && hUsbf->RefCount);

    if (!hUsbf->ChargerDetected)
    {
        UsbfPrivConfigureChargerDetection(hUsbf, NV_TRUE, NV_FALSE);
        if (UsbfPrivIsChargerDetected(hUsbf))
        {
           hUsbf->ChargerDetected = NV_TRUE;
           NvOsDebugPrintf("Dedicated Charger detected\n");
        }
        else
        {
           ChargerType = NvOdmUsbChargerType_Dummy;
           NvOsDebugPrintf("Dumb Charger detected\n");
        }
        UsbfPrivConfigureChargerDetection(hUsbf, NV_FALSE, NV_FALSE);
    }

    if (hUsbf->ChargerDetected)
    {
        switch(hUsbf->LineState)
        {
            case UsbLineStateType_SE0:
                ChargerType = NvOdmUsbChargerType_SE0;
                break;
            case UsbLineStateType_SJ:
                ChargerType = NvOdmUsbChargerType_SJ;
                break;
            case UsbLineStateType_SK:
                ChargerType = NvOdmUsbChargerType_SK;
                break;
            case UsbLineStateType_SE1:
                ChargerType = NvOdmUsbChargerType_SE1;
                break;
            default:
                ChargerType = NvOdmUsbChargerType_UsbHost;
                break;
        }
    }

    return ChargerType;
}


void
NvDdkUsbfBusyHintsOnOff(
    NvDdkUsbfHandle hUsbf,
    NvBool OnOff,
    NvU32 BoostDurationMs)
{
    NvDdkUsbPhyIoctl_UsbBusyHintsOnOffInputArgs BusyHint;

    NV_ASSERT(hUsbf);

    BusyHint.OnOff = OnOff;
    BusyHint.BoostDurationMs = BoostDurationMs;

    NV_ASSERT_SUCCESS(
        NvDdkUsbPhyIoctl(hUsbf->hUsbPhy,
            NvDdkUsbPhyIoctlType_UsbBusyHintsOnOff,
            &BusyHint, NULL));
}

