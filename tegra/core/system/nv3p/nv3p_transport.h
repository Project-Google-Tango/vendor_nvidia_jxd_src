/*
 * Copyright (c) 2008 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Platform Programming Protocol (Nv3p) Transport Layer</b>
 *
 * @b Description: Functions and structures for the lower-level
 * transport mechanism that nv3p.h uses.
 */

#ifndef INCLUDED_NV3P_TRANSPORT_H
#define INCLUDED_NV3P_TRANSPORT_H

#include "nvcommon.h"
#include "nvos.h"
#include "nv3p.h"
#include "nvodm_query.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nv3p_transport Nv3p Transport Layer
 * Provides the transport
 * layer of the NVIDIA Platform Programming Protocol (Nv3p), which is a
 * packet-based communication protocol for a host PC to an NVIDIA
 * Application Processor (NVAP).
 *
 * See nv3p.h for the full specification.
 * @sa nvodm_query_nv3p.h
 *
 * @ingroup nv3p_group
 * @{
 */

/**
 * Holds a transport record handle; calling ::Nv3pOpen() creates an
 * ::Nv3pSocketHandle structure that references this structure.
 */
typedef struct Nv3pTransportRec *Nv3pTransportHandle;


/**
 * Re-opens the Nv3p transport layer and re-initializes the USB.
 *
 * @param[out] hTrans A pointer to the handle of the transport layer.
 * @param[in] mode The Nv3p transport mode.
 * @param[in] instance The device instance number.
 */
NvError
Nv3pTransportReopen( Nv3pTransportHandle *hTrans, Nv3pTransportMode mode, NvU32 instance );

/**
 * Opens the Nv3p transport layer and initializes the USB.
 *
 * @param[out] hTrans A pointer to the handle of the transport layer.
 * @param[in] mode The Nv3p transport mode.
 * @param[in] instance [in] The device instance number.
 */
NvError
Nv3pTransportOpen( Nv3pTransportHandle *hTrans, Nv3pTransportMode mode, NvU32 instance );

/**
 * Waits for specified time before opening the Nv3p transport layer and
 * initializing the USB.
 *
 * @param[out] hTrans A pointer to the handle of the transport layer.
 * @param[in] mode The Nv3p transport mode.
 * @param[in] instance The device instance number.
 * @param[in] TimeOutMs Specifies the maximum time to wait for USB a cable
 *              to be connected. If \a TimeOutMs is \c NV_WAIT_INFINITE,
 *              this functions the same as the Nv3pTransportOpen() API.
 *
 * @retval NvSuccess On success.
 * @retval NvError_UsbfCableDisConnected If a USB cable is not connected
 *              within the \a TimeOutMs.
 */
NvError
Nv3pTransportOpenTimeOut(
    Nv3pTransportHandle *hTrans,
    Nv3pTransportMode mode,
    NvU32 instance,
    NvU32 TimeOutMs);

/**
 * Frees resources for the Nv3p transport layer.
 *
 * @param hTrans Handle to the transport layer
 */
void
Nv3pTransportClose(Nv3pTransportHandle hTrans);

/**
 * Sends data over the Nv3p transport.
 *
 * @note This is a blocking interface.
 *
 * @param hTrans Handle to the transport layer.
 * @param data A pointer to the data bytes to send.
 * @param length The number of bytes to send.
 * @param flags Reserved, must be zero.
 */
NvError
Nv3pTransportSend(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags);

/**
 * Receives data over the Nv3p transport.
 *
 * Must be called after Nv3pTransportReceiveTimeOut() with
 * parameter \a TimeOutMs equal to 0 (zero). This can be used only for
 * receiving data of less than or equal to (<=) 4K bytes.
 *
 * @note This is a non-blocking interface.
 *
 * @param hTrans Handle to the transport layer.
 * @param data A pointer to the data bytes to receive.
 * @param length The maximum number of bytes to be received.
 * @param received A pointer to the bytes received. \a received may be null.
 *
 * @retval NvError_UsbfCableDisConnected If USB cable is not connected.
 * @retval NvSuccess If USB cable is connected.
 */
NvError
Nv3pTransportReceiveIfComplete(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received);

/**
 * Receives data over the Nv3p transport.
 *
 * @note This is a blocking interface.
 *
 * @param hTrans Handle to the transport layer, which is created by the
 *        @c StartFastbootProtocol() function in main.c.
 * @param data A pointer to the data bytes to receive.
 * @param length The maximum number of bytes to to receive.
 * @param received A pointer to the bytes received. \a received may be null.
 * @param flags Reserved, must be zero.
 */
NvError
Nv3pTransportReceive(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags);

/**
 * Receives data over the Nv3p transport.
 *
 * @param hTrans Handle to the transport layer.
 * @param data A pointer to the data bytes to receive.
 * @param length The maximum number of bytes to to receive.
 * @param received A pointer to the bytes received.
 * @param flags Reserved, must be zero.
 * @param TimeOutMs Specifies the maximum time to wait for completion
 *              of the USB transaction. If \a TimeOutMs is \c NV_WAIT_INFINITE,
 *              this functions the same as the Nv3pTransportReceive() API.
 *
 * @retval NvSuccess On success.
 * @retval NvError_Timeout If transfer times out.
 */
NvError
Nv3pTransportReceiveTimeOut(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags,
    NvU32 TimeOutMs);

/**
 * Sends data over the Nv3p transport.
 *
 * @param hTrans Handle to the transport layer.
 * @param data A pointer to the data bytes to send.
 * @param length The number of bytes to send.
 * @param flags Reserved, must be zero.
 * @param TimeOutMs Specifies the maximum time to wait for completion
 *              of the USB transaction. Preferred value is 30 msec or more.
 *              If \a TimeOutMs is \c NV_WAIT_INFINITE,
 *              this functions the same as the Nv3pTransportSend() API.
 *
 * @retval NvSuccess On success.
 * @retval NvError_Timeout If transfer times out.
 */
NvError
Nv3pTransportSendTimeOut(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags,
    NvU32 TimeOutMs);

/**
 * Checks if a USB cable is connected (only for the device code).
 *
 * @param hTrans A pointer to a handle to the transport layer.
 * @param instance The USB instance.
 *
 * @retval NV_TRUE If cable is connected.
 * @retval NV_FALSE If cable not connected.
 */
NvBool
Nv3pTransportUsbfCableConnected(
    Nv3pTransportHandle *hTrans,
    NvU32 instance );

/**
 * Finds charger type (only for the device code).
 *
 * @param hTrans A pointer to a handle to the transport layer.
 * @param instance The USB instance.
 *
 * @retval NvOdmUsbChargerType_Dummy If dumb charger.
 * @retval NvOdmUsbChargerType_SE0 If both D+ and D- pins are 0.
 * @retval NvOdmUsbChargerType_SE1 If both D+ and D- pins are 1.
 * @retval NvOdmUsbChargerType_SJ If D+ is high and D- is low.
 * @retval NvOdmUsbChargerType_SK If D+ is low and D- is high.
 * @retval NvOdmUsbChargerType_UsbHost If USB host-based charging type.
 */
NvOdmUsbChargerType
Nv3pTransportUsbfGetChargerType(
    Nv3pTransportHandle *hTrans,
    NvU32 instance );

void Nv3pTransportSetChargingMode(NvBool ChargingMode);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NV3P_TRANSPORT_H
