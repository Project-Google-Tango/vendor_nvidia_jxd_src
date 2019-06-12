/*
 * Copyright (c) 2008 - 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nv3p_transport_jtag.h"
#include "nv3p_transport_jtag_defs.h"

#define NV3P_REQUEST_FILE NV3P_APP_PC_REQUEST_FILE
#define NV3P_RESPONSE_FILE NV3P_APP_PC_RESPONSE_FILE

NvError Nv3pTransportReopenT30(Nv3pTransportHandle *hTrans)
{
    Nv3pTransportReopenJtag(hTrans);
    return NvSuccess;
}

NvError Nv3pTransportOpenT30(Nv3pTransportHandle *hTrans)
{
    return Nv3pTransportReopenT30(hTrans);
}

void Nv3pTransportCloseT30(Nv3pTransportHandle hTrans)
{
}

NvError
Nv3pTransportSendT30(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags )
{
    return Nv3pTransportSendJtag(hTrans, data, length, flags);
}

NvError
Nv3pTransportReceiveT30(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags )
{
    return Nv3pTransportReceiveJtag(hTrans, data, length, received,flags );
}

NvBool
Nv3pTransportUsbfCableConnected(
    Nv3pTransportHandle *hTrans,
    NvU32 instance)
{
    // Not Implemented
    return NV_FALSE;;
}

NvOdmUsbChargerType
Nv3pTransportUsbfGetChargerType(
    Nv3pTransportHandle *hTrans,
    NvU32 instance )
{
    // Not Implemented
    return NvOdmUsbChargerType_Dummy;
}

NvError Nv3pTransportReopenT1xx(Nv3pTransportHandle *hTrans)
{
    Nv3pTransportReopenJtag(hTrans);
    return NvSuccess;
}

NvError Nv3pTransportOpenT1xx(Nv3pTransportHandle *hTrans)
{
    return Nv3pTransportReopenT1xx(hTrans);
}

void Nv3pTransportCloseT1xx(Nv3pTransportHandle hTrans)
{
}

NvError
Nv3pTransportSendT1xx(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags )
{
    return Nv3pTransportSendJtag(hTrans, data, length, flags);
}

NvError
Nv3pTransportReceiveT1xx(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags )
{
    return Nv3pTransportReceiveJtag(hTrans, data, length, received,flags );
}

void
Nv3pTransportSetChargingMode(NvBool ChargingMode)
{
    // Not Applicable
}

#include "nv3p_transport_jtag_driver.c"
