/*
 * Copyright (c) 2006-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVUSBHOST_H
#define INCLUDED_NVUSBHOST_H

#include "nvos.h"

#define MAX_SUPPORTED_DEVICES 127

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    NvUsbPipePolicy_ShortPacketTerminate = 1,
    NvUsbPipePolicy_IgnoreShortPackets,
    NvUsbPipePolicy_AllowPartialReads,

    NvUsbPipePolicy_Force32 = 0x7fffffff

} NvUsbPipePolicy;

typedef struct NvUsbDeviceRec *NvUsbDeviceHandle;

/**  NvUsbDeviceSetBusNum - specifiy the bus number before open USB
 *
 *  @param  usbBusNum  string of the usb bus number.
 *
 *  All devices appear the same. No serial number to
 *  identify them right now. So the usb bus number is used
 *  to support multiple DUTs on the same host.
 *
 */
void NvUsbDeviceSetBusNum(const char* usbBusNum);

/**  NvUsbDeviceOpen - opens a USB device.
 *      
 *  @param  instance    instance of the USB device connected.
 *  @param  hDev        Pointer to the opaque handle for the USB device.
 *
 *  Clients should call this API for instance number 0 to some max value untill
 *  the API returns NvError_KernelDriverNotFound.
 *
 */
NvError NvUsbDeviceOpen(NvU32 instance, NvUsbDeviceHandle *hDev);

/** NvUsbDeviceQueryConnectionStatus - Determine if USB device is still connected
 */

NvError NvUsbDeviceQueryConnectionStatus(NvUsbDeviceHandle hDev, NvBool *IsConnected);

/** NvUsbDeviceWrite - Writes to the USB device
 */

NvError NvUsbDeviceWrite(NvUsbDeviceHandle hDev, const void *ptr, NvU32 size);

/** NvUsbDeviceRead - Reads from the device
 */

NvError NvUsbDeviceRead(NvUsbDeviceHandle hDev, void *ptr, NvU32 size, NvU32 *bytesRead);

/** NvUsbDeviceClose - Closes the USB device
 */
void NvUsbDeviceClose(NvUsbDeviceHandle hDev);

/** NvUsbDeviceAbort - Aborts transfer on both in and out pipe
*/
void NvUsbDeviceAbort(NvUsbDeviceHandle hDev);

/** NvUsbDeviceSetReadPolicy - Sets read pipe policy
*/
NvError NvUsbDeviceSetReadPolicy(NvUsbDeviceHandle hDev, NvUsbPipePolicy policy, NvBool enable);

/** NvUsbDeviceSetWritePolicy - Sets read pipe policy
*/
NvError NvUsbDeviceSetWritePolicy(NvUsbDeviceHandle hDev, NvUsbPipePolicy policy, NvBool enable);

/** NvUsbDeviceReadMode - Switches reads to assynchronous mode by providing
    a pointer to OVERLAPPED structure
*/
void NvUsbDeviceReadMode(NvUsbDeviceHandle hDev, void* ovl);

/** NvUsbDeviceReadDevId - Retrieves the product id value embedded in usb device descriptor
    structure 
*/
NvError NvUsbDeviceReadDevId(NvUsbDeviceHandle hDev, NvU32 *productId);
#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_NVUSBHOST_H
