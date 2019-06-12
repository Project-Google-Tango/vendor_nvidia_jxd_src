/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NV3P_TRANSPORT_SEMA_HOST_H
#define INCLUDED_NV3P_TRANSPORT_SEMA_HOST_H

#include "nv3p_transport.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * Opens the nv3p transport layer and reinits the USB
 *
 * @param hTrans [out] pointer to the handle to the transport layer.
 */
NvError
Nv3pTransportReopenSema( Nv3pTransportHandle *hTrans, NvU32 instance);

/**
 * Opens the nv3p transport layer and inits the USB.
 *
 * @param hTrans [out] pointer to the handle to the transport layer.
 */
NvError
Nv3pTransportOpenSema( Nv3pTransportHandle *hTrans, NvU32 instance);

/**
 * Frees resources for the nv3p transport layer.
 *
 * @param hTrans Handle to the transport layer
 */
NvError
Nv3pTransportCloseSema( Nv3pTransportHandle hTrans );

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
Nv3pTransportSendSema(
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
Nv3pTransportReceiveSema(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags );
#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_NV3P_TRANSPORT_Sema_HOST_H
