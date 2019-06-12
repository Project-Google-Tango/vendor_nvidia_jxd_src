/*
 * Copyright (c) 2008 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "nv3p.h"
#include "nv3p_transport.h"
#include "nv3p_transport_usb_host.h"
#include "nv3p_transport_sema_host.h"
#if NVODM_BOARD_IS_FPGA
    #include "nv3p_transport_jtag.h"
#endif
static Nv3pTransportMode gs_mode;


NvError
Nv3pTransportReopen( Nv3pTransportHandle *hTrans, Nv3pTransportMode mode, NvU32 instance )
{
    NvError e = NvError_BadParameter;
    gs_mode = mode;
    switch(mode)
    {
        case Nv3pTransportMode_default:
        case Nv3pTransportMode_Usb:
            e = Nv3pTransportReopenUsb(hTrans, instance);
            break;
        case Nv3pTransportMode_Sema:
            e = Nv3pTransportReopenSema(hTrans, instance);
            break;
#if NVODM_BOARD_IS_FPGA
        case Nv3pTransportMode_Jtag:
            e = Nv3pTransportReopenJtag(hTrans);
            break;
#endif
        default:
            e = NvError_BadParameter;
            break;
    }
    return e;
}

NvError
Nv3pTransportOpen( Nv3pTransportHandle *hTrans, Nv3pTransportMode mode, NvU32 instance )
{
    NvError e = NvError_BadParameter;
    gs_mode = mode;
    switch(mode)
    {
        case Nv3pTransportMode_default:
        case Nv3pTransportMode_Usb:
            e = Nv3pTransportOpenUsb(hTrans, instance);
            break;
        case Nv3pTransportMode_Sema:
            e = Nv3pTransportOpenSema(hTrans, instance);
            break;
#if NVODM_BOARD_IS_FPGA
        case Nv3pTransportMode_Jtag:
            e = Nv3pTransportOpenJtag(hTrans);
            break;
#endif
        default:
            e = NvError_BadParameter;
            break;
    }
    return e;
}

void
Nv3pTransportClose(Nv3pTransportHandle hTrans)
{
    switch(gs_mode)
    {
        case Nv3pTransportMode_default:
        case Nv3pTransportMode_Usb:
            Nv3pTransportCloseUsb(hTrans);
            break;
        case Nv3pTransportMode_Sema:
            Nv3pTransportCloseSema(hTrans);
            break;
#if NVODM_BOARD_IS_FPGA
        case Nv3pTransportMode_Jtag:
            Nv3pTransportCloseJtag(hTrans);
            break;
#endif
        default:
            break;
    }
}

NvError
Nv3pTransportSend(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags)
{
    NvError e = NvError_BadParameter;
    switch(gs_mode)
    {
        case Nv3pTransportMode_default:
        case Nv3pTransportMode_Usb:
            e = Nv3pTransportSendUsb(hTrans, data, length, flags);
            break;
        case Nv3pTransportMode_Sema:
            e = Nv3pTransportSendSema(hTrans, data, length, flags);
            break;
#if NVODM_BOARD_IS_FPGA
        case Nv3pTransportMode_Jtag:
            e = Nv3pTransportSendJtag(hTrans, data, length, flags);
            break;
#endif
        default:
            e = NvError_BadParameter;
            break;
    }
    return e;
}

NvError
Nv3pTransportReceive(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags)
{
    NvError e = NvError_BadParameter;
    switch(gs_mode)
    {
        case Nv3pTransportMode_default:
        case Nv3pTransportMode_Usb:
            e = Nv3pTransportReceiveUsb(hTrans, data, length, received, flags);
            break;
        case Nv3pTransportMode_Sema:
            e = Nv3pTransportReceiveSema(hTrans, data, length, received, flags);
            break;
#if NVODM_BOARD_IS_FPGA
        case Nv3pTransportMode_Jtag:
            e = Nv3pTransportReceiveJtag(hTrans, data, length, received, flags);
            break;
#endif
        default:
            e = NvError_BadParameter;
            break;
    }
    return e;
}

NvError
Nv3pTransportSendTimeOut(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags,
    NvU32 TimeOut)
{
    // Not Implimented
    return NvError_NotImplemented;
}

NvError
Nv3pTransportReceiveTimeOut(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags,
    NvU32 TimeOut)
{
    // Not Implimented
    return NvError_NotImplemented;
}

NvBool
Nv3pTransportUsbfCableConnected(
    Nv3pTransportHandle *hTrans,
    NvU32 instance)
{
    // Not Necessary for host
    return NV_FALSE;
}

NvOdmUsbChargerType
Nv3pTransportUsbfGetChargerType(
    Nv3pTransportHandle *hTrans,
    NvU32 instance )
{
    // Not Necessary for host
    return NvOdmUsbChargerType_Dummy;;
}

void
Nv3pTransportSetChargingMode(NvBool ChargingMode)
{
    // Not applicable
}
