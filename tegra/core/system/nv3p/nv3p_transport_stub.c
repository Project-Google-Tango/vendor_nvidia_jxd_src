/*
 * Copyright (c) 2009 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nv3p_transport.h"
#include "nvassert.h"

NvError
Nv3pTransportReopen( Nv3pTransportHandle *hTrans, Nv3pTransportMode mode, NvU32 instance )
{
    return NvError_NotImplemented;
}

NvError
Nv3pTransportOpen( Nv3pTransportHandle *hTrans, Nv3pTransportMode mode, NvU32 instance )
{
    return NvError_NotImplemented;
}

void
Nv3pTransportClose(Nv3pTransportHandle hTrans)
{
}

NvError
Nv3pTransportSend(Nv3pTransportHandle hTrans, NvU8 *data, NvU32 length,
    NvU32 flags)
{
    return NvError_NotImplemented;
}

NvError
Nv3pTransportReceive(Nv3pTransportHandle hTrans, NvU8 *data, NvU32 length,
    NvU32 *received,  NvU32 flags)
{
    return NvError_NotImplemented;
}

NvError
Nv3pTransportSendTimeOut(Nv3pTransportHandle hTrans, NvU8 *data, NvU32 length,
    NvU32 flags, NvU32 TimeOut)
{
    return NvError_NotImplemented;
}

NvError
Nv3pTransportReceiveTimeOut(Nv3pTransportHandle hTrans, NvU8 *data, NvU32 length,
    NvU32 *received, NvU32 flags, NvU32 TimeOut)
{
    return NvError_NotImplemented;
}

