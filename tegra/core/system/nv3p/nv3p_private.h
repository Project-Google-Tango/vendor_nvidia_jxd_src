/*
 * Copyright (c) 2008 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 #ifndef INCLUDED_NV3P_PRIVATE_H
 #define INCLUDED_NV3P_PRIVATE_H

#include "nv3p_transport.h"
#if defined(__cplusplus)
extern "C"
{
#endif

NvError Nv3pTransportReopenT30(Nv3pTransportHandle *hTrans);

NvError Nv3pTransportOpenT30(Nv3pTransportHandle *hTrans, NvU32 TimeOut);

void Nv3pTransportCloseT30(Nv3pTransportHandle hTrans);

NvError
Nv3pTransportReceiveIfCompleteT30(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received);

NvError
Nv3pTransportSendT30(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags,
    NvU32 TimeOut);

NvError
Nv3pTransportReceiveT30(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags,
    NvU32 TimeOut);

NvOdmUsbChargerType
Nv3pTransportUsbfGetChargerTypeT30(
    Nv3pTransportHandle *hTrans,
    NvU32 instance );

NvBool
Nv3pTransportUsbfCableConnectedT30(
    Nv3pTransportHandle *hTrans,
    NvU32 instance );

void
Nv3pTransportSetChargingModeT30(
    NvBool ChargingMode);


NvError Nv3pTransportOpenT1xx(Nv3pTransportHandle *hTrans, NvU32 TimeOut);

NvError Nv3pTransportReopenT1xx(Nv3pTransportHandle *hTrans);


void Nv3pTransportCloseT1xx(Nv3pTransportHandle hTrans);

NvError
Nv3pTransportSendT1xx(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags,
    NvU32 TimeOut);

NvError
Nv3pTransportReceiveT1xx(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags,
    NvU32 TimeOut);

NvOdmUsbChargerType
Nv3pTransportUsbfGetChargerTypeT1xx(
    Nv3pTransportHandle *hTrans,
    NvU32 instance );

NvBool
Nv3pTransportUsbfCableConnectedT1xx(
    Nv3pTransportHandle *hTrans,
    NvU32 instance );

void
Nv3pTransportSetChargingModeT1xx(
    NvBool ChargingMode);

NvError
Nv3pTransportReceiveIfCompleteT1xx(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received);

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_NV3P_PRIVATE_H
