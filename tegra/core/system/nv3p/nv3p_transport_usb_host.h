/*
 * Copyright (c) 2006 - 2010 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NV3P_TRANSPORT_USB_HOST_H
#define INCLUDED_NV3P_TRANSPORT_USB_HOST_H

#include "nv3p_transport.h"
#include "nvusbhost.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Chip Id.
 */
typedef struct Nv3pTransportUsbThreadArgsRec
{
    NvBool IsThreadModeEnabled;
    NvBool IsThreadRunning;
    NvBool ExitThread;
    NvOsThreadHandle hThread;
    NvUsbDeviceHandle hUsb;
    NvU8 *data;
    NvU32 length;
    NvU32 *received;
    NvU32 flags;
    NvBool IsDataSendCmd;
    NvOsSemaphoreHandle CmdReadySema;
    NvOsSemaphoreHandle CmdDoneSema;
    NvError CmdStatus;
} Nv3pTransportUsbThreadArgs;

/**
 * Opens the nv3p transport layer and reinits the USB
 *
 * @param hTrans [out] pointer to the handle to the transport layer.
 * @param instance [in] usb instance number
 */
NvError
Nv3pTransportReopenUsb( Nv3pTransportHandle *hTrans, NvU32 instance );

/**
 * Opens the nv3p transport layer and inits the USB.
 *
 * @param hTrans [out] pointer to the handle to the transport layer.
 * @param instance [in] usb instance number
 */
NvError
Nv3pTransportOpenUsb( Nv3pTransportHandle *hTrans, NvU32 instance);

/**
 * Frees resources for the nv3p transport layer.
 *
 * @param hTrans Handle to the transport layer
 */
NvError
Nv3pTransportCloseUsb( Nv3pTransportHandle hTrans );

/**
 * Send data over the nv3p transport.
 *
 * @param hTrans Handle to the transport layer
 * @param data The data bytes to send
 * @param length The number of bytes to send
 * @param flags Reserved, must be zero
 * 
 * This is a blocking interface.
 */
NvError
Nv3pTransportSendUsb( 
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags );

/**
 * Receive data over the nv3p transport.
 *
 * @param hTrans Handle to the transport layer
 * @param data The data bytes to receive
 * @param length The maximum number of bytes to send
 * @param receieved The number of bytes received
 * @param flags Reserved, must be zero
 *
 * 'received' may be null. This is a blocking interface.
 */
NvError
Nv3pTransportReceiveUsb(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags );  
#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_NV3P_TRANSPORT_USB_HOST_H
