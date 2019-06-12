/**
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <windows.h>
#include <winusb.h>
#include <initguid.h>
#include <setupapi.h>
#include <strsafe.h>
#include "nvusbhost.h"
#include "nvapputil.h"
#include "nvassert.h"

// {EAD8C4F6-6102-45c7-AA66-36E6D7204600} This value should match the value
// from the .inf file which is used to install winusb.sys file.

DEFINE_GUID(GUID_DEVINTERFACE_NVBOOT_USB, 
0xEAD8C4F6, 0x6102, 0x45c7, 0xaa, 0x66, 0x36, 0xe6, 0xd7, 0x20, 0x46, 0x00);

#define MAX_DEVPATH_LENGTH  (256)

/**
 * Currently, following functions are used by this code:
 *
 * WinUsb_Initialize
 * WinUsb_Free
 * WinUsb_QueryInterfaceSettings
 * WinUsb_QueryDeviceInformation
 * WinUsb_QueryPipe
 * WinUsb_ReadPipe
 * WinUsb_WritePipe
 * WinUsb_SetPipePolicy
 * WinUsb_AbortPipe
 */
static FARPROC NvWinUsb_Initialize                   = NULL;
static FARPROC NvWinUsb_Free                         = NULL;
static FARPROC NvWinUsb_QueryInterfaceSettings       = NULL;
static FARPROC NvWinUsb_QueryDeviceInformation       = NULL;
static FARPROC NvWinUsb_QueryPipe                    = NULL;
static FARPROC NvWinUsb_ReadPipe                     = NULL;
static FARPROC NvWinUsb_WritePipe                    = NULL;
static FARPROC NvWinUsb_SetPipePolicy                = NULL;
static FARPROC NvWinUsb_AbortPipe                    = NULL;
static FARPROC NvWinUsb_GetDescriptor               = NULL;

// this precludes reentrancy. it might be ok.
static NvOsLibraryHandle hWinUsbDll = 0;

typedef struct NvUsbDeviceRec
{
    HANDLE                  hUsbDevice;
    WINUSB_INTERFACE_HANDLE hWinUsb;
    UCHAR speed;
    USB_INTERFACE_DESCRIPTOR ifaceDescriptor;
    USB_DEVICE_DESCRIPTOR devDescriptor;
    WINUSB_PIPE_INFORMATION  bulkInPipe;
    WINUSB_PIPE_INFORMATION  bulkOutPipe;
    OVERLAPPED* rOverlapped;
} NvUsbDevice;

static NvError GetDevicePath(LPGUID InterfaceGuid,
                   NvU32 instance,
                   PCHAR DevicePath,
                   size_t BufLen);

static NvError GetDevicePath(LPGUID InterfaceGuid,
                   NvU32 instance,
                   PCHAR DevicePath,
                   size_t BufLen)
{
    BOOL bResult = FALSE;
    HDEVINFO deviceInfo;
    SP_DEVICE_INTERFACE_DATA interfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = NULL;
    ULONG length;
    ULONG requiredLength=0;
    HRESULT hr;
    NvError err = NvSuccess;

    deviceInfo = SetupDiGetClassDevs(InterfaceGuid,
                     NULL, NULL,
                     DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfo == INVALID_HANDLE_VALUE)
    {
        NvOsDebugPrintf("SetupDiGetClassDevs failed - GetLastError returned (%lu)\n", GetLastError());
        NvOsDebugPrintf("Make sure that the Nvidia recovery mode driver is installed on this system\n");
        return NvError_KernelDriverNotFound;
    }

    ZeroMemory(&interfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    bResult = SetupDiEnumDeviceInterfaces(deviceInfo,
                                        NULL,
                                        InterfaceGuid,
                                        instance,
                                        &interfaceData);
    if (bResult == FALSE)
    {
        NvOsDebugPrintf("SetupDiEnumDeviceInterfaces" 
                        "failed GetLastError returned (%lu)\n",
                        GetLastError());
        NvOsDebugPrintf("Make sure that the AP1x system is plugged into the system and is in recovery mode\n");
        err = NvError_DeviceNotFound;
        goto fail;
    }

    SetupDiGetDeviceInterfaceDetail(deviceInfo,
                                  &interfaceData,
                                  NULL, 0,
                                  &requiredLength,
                                  NULL);

    detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)
        LocalAlloc(LMEM_FIXED, requiredLength);
    if (NULL == detailData)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }

    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    length = requiredLength;

    bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
                                           &interfaceData,
                                           detailData,
                                           length,
                                           &requiredLength,
                                           NULL);
    if (FALSE == bResult)
    {
        err = NvError_BadParameter; /* FIXME what is the right error value */
        goto fail;
    }

    hr = StringCchCopy(DevicePath,
                     BufLen,
                     detailData->DevicePath);
    if (FAILED(hr))
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }

fail:

    if (deviceInfo != INVALID_HANDLE_VALUE)
    {
        SetupDiDestroyDeviceInfoList(deviceInfo);
    }

    if (detailData)
    {
        LocalFree(detailData);
    }
    return err;
}

#define GET_ENTRYPOINT(x)                                               \
    NvWinUsb_ ## x = NvOsLibraryGetSymbol(hWinUsbDll, "WinUsb_" #x);
#define CLEAR_ENTRYPOINT(x)                     \
    NvWinUsb_ ## x = NULL;

static void NvWinUsbCleanUp()
{
    CLEAR_ENTRYPOINT(Initialize);
    CLEAR_ENTRYPOINT(Free);
    CLEAR_ENTRYPOINT(QueryInterfaceSettings);
    CLEAR_ENTRYPOINT(QueryDeviceInformation);
    CLEAR_ENTRYPOINT(QueryPipe);
    CLEAR_ENTRYPOINT(ReadPipe);
    CLEAR_ENTRYPOINT(WritePipe);
    CLEAR_ENTRYPOINT(SetPipePolicy);
    CLEAR_ENTRYPOINT(AbortPipe);
    CLEAR_ENTRYPOINT(GetDescriptor);

    NvOsLibraryUnload(hWinUsbDll);
    hWinUsbDll = 0;
}

static NvError NvLoadWinUsbDll()
{
    /* 
     * the DLL loading takes place here. It's assumed that this
     * function "NvUsbDeviceOpen" shall always be called before any
     * other USB function is used.
     *
     * If NvUsbDeviceWrite, NvUsbDeviceClose,
     */
    NvError e;

    if (hWinUsbDll == 0)
    {
        NV_CHECK_ERROR_CLEANUP(NvOsLibraryLoad("winusb.dll", &hWinUsbDll));
        
        GET_ENTRYPOINT(Initialize);
        GET_ENTRYPOINT(Free);
        GET_ENTRYPOINT(QueryInterfaceSettings);
        GET_ENTRYPOINT(QueryDeviceInformation);
        GET_ENTRYPOINT(QueryPipe);
        GET_ENTRYPOINT(ReadPipe);
        GET_ENTRYPOINT(WritePipe);
        GET_ENTRYPOINT(SetPipePolicy);
        GET_ENTRYPOINT(AbortPipe);
        GET_ENTRYPOINT(GetDescriptor);

        if (!(NvWinUsb_Initialize &&
              NvWinUsb_Free &&
              NvWinUsb_QueryInterfaceSettings &&
              NvWinUsb_QueryDeviceInformation &&
              NvWinUsb_QueryPipe &&
              NvWinUsb_ReadPipe &&
              NvWinUsb_WritePipe &&
              NvWinUsb_SetPipePolicy &&
              NvWinUsb_AbortPipe &&
              NvWinUsb_GetDescriptor))
        {
            e = NvError_SymbolNotFound;
            goto fail;
        }
    }
    return NvSuccess;

fail:

    NvWinUsbCleanUp();
    return e;
}

NvError NvUsbDeviceQueryConnectionStatus(
    NvUsbDeviceHandle hDev, 
    NvBool *IsConnected)
{
    ULONG length;
    UCHAR speed;
    BOOL bResult = FALSE;

    *IsConnected = NV_FALSE;
    
    if (hDev == NULL)
        return NvError_BadParameter;

    /* 
     * If this function is called before NvUsbDeviceOpen, or if NvUsbDeviceOpen
     * failed, the driver is not loaded successfully yet.
     */
    if (hWinUsbDll == 0)
        return NvError_KernelDriverNotFound;

    /*
     * Attempt to query device info as a means to determining whether
     * the device is still connected
     */
    length = sizeof(UCHAR);
    bResult = NvWinUsb_QueryDeviceInformation(hDev->hWinUsb,
                                              DEVICE_SPEED,
                                              &length,
                                              &speed);
    if (bResult == FALSE)
    {
        // experiments indicate that this condition occurs when the device is
        // no longer connected
        return NvSuccess;
    }
    
    *IsConnected = NV_TRUE;
    return NvSuccess;
}

// dummy implementation to avoid build error
void NvUsbDeviceSetBusNum(const char* usbBusNum)
{
    return;
}

NvError NvUsbDeviceOpen(NvU32 instance, NvUsbDeviceHandle *handle)
{
    HANDLE hDev;
    TCHAR devicePath[MAX_DEVPATH_LENGTH];
    NvError err;
    BOOL bResult = FALSE;
    ULONG length;
    NvU32 i;
    WINUSB_PIPE_INFORMATION  pipeInfo;
    NvUsbDevice *dev = NULL;
    ULONG descriptorLength;

    if (instance > MAX_SUPPORTED_DEVICES)
    {
        err = NvError_NotImplemented;
        goto fail;
    }

    err = NvLoadWinUsbDll();
    if (err != NvSuccess)
        goto fail;

    err = GetDevicePath( (LPGUID) &GUID_DEVINTERFACE_NVBOOT_USB,
                         instance,
                         devicePath,
                         sizeof(devicePath));
    if (err != NvSuccess)
    {
        goto fail;
    }

    dev = malloc(sizeof(NvUsbDevice));
    if (dev == NULL)
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }
    memset(dev, 0, sizeof(NvUsbDevice));


    hDev = CreateFile(devicePath,
                    GENERIC_WRITE | GENERIC_READ,
                    FILE_SHARE_WRITE | FILE_SHARE_READ,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                    NULL);
    if (hDev == INVALID_HANDLE_VALUE)
    {
        NvOsDebugPrintf("CreateFile failed - GetLastError returned (%lu)\n",
                GetLastError());
        err= NvError_DeviceNotFound;
        goto fail;
    }
    dev->hUsbDevice = hDev;

    bResult = NvWinUsb_Initialize(hDev, &dev->hWinUsb);
    if (bResult == FALSE)
    {
        NvOsDebugPrintf("WinUsb_Initialize failed GetLastError returned (%lu)\n",
                GetLastError());
        err = NvError_DeviceNotFound; 
        goto fail;
    }

    length = sizeof(UCHAR);
    bResult = NvWinUsb_QueryDeviceInformation(dev->hWinUsb,
                                        DEVICE_SPEED,
                                        &length,
                                        &dev->speed);
    if (bResult == FALSE)
    {
        err = NvError_DeviceNotFound;
        goto fail;
    }

    bResult = NvWinUsb_GetDescriptor(dev->hWinUsb,
                        USB_DEVICE_DESCRIPTOR_TYPE,
                        0,
                        0,
                        &dev->devDescriptor,
                        sizeof(USB_DEVICE_DESCRIPTOR),
                        &descriptorLength);

    if (bResult == FALSE)
    {
        err = NvError_DeviceNotFound;
        goto fail;
    }

    bResult = NvWinUsb_QueryInterfaceSettings(dev->hWinUsb,
                                            0,
                                            &dev->ifaceDescriptor);
    if (bResult == FALSE)
    {
        err = NvError_DeviceNotFound;
        goto fail;
    }

    if (dev->ifaceDescriptor.bNumEndpoints != 2)
    {
        err = NvError_DeviceNotFound;
        goto fail;
    }

    for (i=0; i<dev->ifaceDescriptor.bNumEndpoints; i++)
    {
        bResult = NvWinUsb_QueryPipe(dev->hWinUsb,
                                 0,
                                 (UCHAR) i,
                                 &pipeInfo);
        if (pipeInfo.PipeType == UsbdPipeTypeBulk &&
                  USB_ENDPOINT_DIRECTION_IN(pipeInfo.PipeId))
        {
            memcpy(&dev->bulkInPipe, &pipeInfo, 
                    sizeof(WINUSB_PIPE_INFORMATION));
        }
        else if (pipeInfo.PipeType == UsbdPipeTypeBulk &&
                  USB_ENDPOINT_DIRECTION_OUT(pipeInfo.PipeId))
        {
            memcpy(&dev->bulkOutPipe, &pipeInfo, 
                    sizeof(WINUSB_PIPE_INFORMATION));
        } else
        {
            return NvError_DeviceNotFound;
        }
    }
    
    *handle = dev;
    return NvSuccess;

fail:

    if (dev)
    {
        if (dev->hWinUsb)
        {
            NvWinUsb_Free(dev->hWinUsb);
        }
        CloseHandle(hDev);
        free(dev);
    }

    NvWinUsbCleanUp();

    *handle = 0;
    return err;
}


NvError NvUsbDeviceWrite(NvUsbDeviceHandle hDev, const void *ptr, NvU32 size)
{
    BOOL bResult;
    ULONG bytesWritten;

    if (hDev == NULL)
        return NvError_BadParameter;

    /* 
     * If this function is called before NvUsbDeviceOpen, or if NvUsbDeviceOpen
     * failed, the driver is not loaded successfully yet.
     */
    if (hWinUsbDll == 0)
        return NvError_KernelDriverNotFound;

    /* FIXME There is a possibility that the winsub driver can send less, 
     * then this API should retry! */
    bResult = NvWinUsb_WritePipe(hDev->hWinUsb, hDev->bulkOutPipe.PipeId, 
            (PUCHAR)ptr, size, &bytesWritten, NULL);
    if (bResult == FALSE || size != bytesWritten)
    {
        NvOsDebugPrintf("NvUsbDeviceWrite failed GetLastError() returned (%lu)",
                GetLastError());
        return NvError_KernelDriverNotFound;
    }
    return NvSuccess;
}

NvError NvUsbDeviceRead(NvUsbDeviceHandle hDev, void *ptr, NvU32 size, 
        NvU32 *bytesRead)
{
    BOOL bResult;
    ULONG lbytesRead = 0;
    ULONG* pbytesRead = hDev->rOverlapped ? NULL : &lbytesRead;

    /* 
     * If this function is called before NvUsbDeviceOpen, or if NvUsbDeviceOpen
     * failed, the driver is not loaded successfully yet.
     */
    if (hWinUsbDll == 0)
        return NvError_KernelDriverNotFound;

    bResult = NvWinUsb_ReadPipe(hDev->hWinUsb, hDev->bulkInPipe.PipeId, 
                              ptr, size, 
                              pbytesRead, hDev->rOverlapped);
    if (bResult == FALSE)
    {
        if (GetLastError() != ERROR_IO_PENDING)
        {
            NvOsDebugPrintf("NvUsbDeviceRead failed GetLastError() returned (%lu)",
                GetLastError());
            return NvError_KernelDriverNotFound;
        }
        return NvError_Busy;
    }

    if (bytesRead)
        *bytesRead = (NvU32)lbytesRead;

    return NvSuccess;
}

void NvUsbDeviceClose(NvUsbDeviceHandle hDev)
{
    if (hDev == NULL)
        return;

    /* 
     * If this function is called before NvUsbDeviceOpen, or if NvUsbDeviceOpen
     * failed, the driver is not loaded successfully yet.
     */
    if (hWinUsbDll == 0)
        return;

    NV_ASSERT(hDev->hWinUsb);
    CloseHandle(hDev->hUsbDevice);
    NvWinUsb_Free(hDev->hWinUsb);

    free(hDev);
    NvWinUsbCleanUp();

    return;
}

void NvUsbDeviceAbort(NvUsbDeviceHandle hDev)
{
    if (hDev == NULL)
        return;
 
    /* 
     * If this function is called before NvUsbDeviceOpen, or if NvUsbDeviceOpen
     * failed, the driver is not loaded successfully yet.
     */
    if (hWinUsbDll == 0)
        return;

    NV_ASSERT(hDev->hWinUsb);
    NvWinUsb_AbortPipe(hDev->hWinUsb, hDev->bulkInPipe.PipeId);
    NvWinUsb_AbortPipe(hDev->hWinUsb, hDev->bulkOutPipe.PipeId);
    return;
}

NvError NvUsbDeviceSetPolicy(NvUsbDeviceHandle hDev, UCHAR interfaceHandle,
                             NvUsbPipePolicy policy, NvBool enable)
{
    ULONG policyType;
    UCHAR policyEnable = enable;
    ULONG valueLength;

    switch (policy)
    {
        case NvUsbPipePolicy_ShortPacketTerminate:
            policyType = SHORT_PACKET_TERMINATE;
            valueLength = sizeof(policyEnable);
            break;
        case NvUsbPipePolicy_IgnoreShortPackets:
            policyType = IGNORE_SHORT_PACKETS;
            valueLength = sizeof(policyEnable);
            break;
        case NvUsbPipePolicy_AllowPartialReads:
            policyType = ALLOW_PARTIAL_READS;
            valueLength = sizeof(policyEnable);
            break;
        default:
            return NvError_BadParameter;
    }

    if (!NvWinUsb_SetPipePolicy(hDev->hWinUsb,
                                interfaceHandle,
                                policyType,
                                valueLength,
                                &policyEnable))
        return NvError_FileOperationFailed;

    return NvSuccess;
}

NvError NvUsbDeviceSetReadPolicy(NvUsbDeviceHandle hDev, NvUsbPipePolicy policy,
                                 NvBool enable)
{
    return NvUsbDeviceSetPolicy(hDev, hDev->bulkInPipe.PipeId, policy, enable);
}

NvError NvUsbDeviceSetWritePolicy(NvUsbDeviceHandle hDev, NvUsbPipePolicy policy,
                                  NvBool enable)
{
    return NvUsbDeviceSetPolicy(hDev, hDev->bulkOutPipe.PipeId, policy, enable);
}

// FIXME: If needed, implement clean switch back to synchronous state
void NvUsbDeviceReadMode(NvUsbDeviceHandle hDev, OVERLAPPED* ovl)
{
    hDev->rOverlapped = ovl;
}
// Get product id.
NvError
NvUsbDeviceReadDevId(NvUsbDeviceHandle hDev, NvU32 *productId)
{
    // idVendor;
    if ( !hDev )
    {
        return NvError_BadParameter;
    }
    else
    {
        *productId = hDev->devDescriptor.idProduct;
        return NvSuccess;
    }
}

