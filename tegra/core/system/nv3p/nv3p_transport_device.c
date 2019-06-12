/*
 * Copyright (c) 2008 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvrm_drf.h"
#include "arapb_misc.h"
#include "nv3p_private.h"
#include "nv3p_transport.h"

#define NV3P_APB_MISC_BASE 0x70000000UL
#define TIMEOUT_MS 1000

static NvU32 Nv3pPrivGetChipId(void)
{
    volatile NvU8 *pApbMisc = NULL;
    NvU32 ChipId = 0;
    NvError e;
    NV_CHECK_ERROR_CLEANUP(
        NvOsPhysicalMemMap(NV3P_APB_MISC_BASE, 4096,
            NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE, (void**)&pApbMisc)
    );
    ChipId = *(volatile NvU32*)(pApbMisc + APB_MISC_GP_HIDREV_0);
    ChipId = NV_DRF_VAL(APB_MISC_GP, HIDREV, CHIPID, ChipId);

 fail:
    return ChipId;
}

NvError Nv3pTransportReopen(Nv3pTransportHandle *hTrans, Nv3pTransportMode mode, NvU32 instance )
{
    NvU32 ChipId = Nv3pPrivGetChipId();

    if (!hTrans)
        return NvError_InvalidAddress;

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        return Nv3pTransportReopenT30(hTrans);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        return Nv3pTransportReopenT1xx(hTrans);
#endif

    return NvError_NotSupported;
}

NvError Nv3pTransportOpen(Nv3pTransportHandle *hTrans, Nv3pTransportMode mode, NvU32 instance)
{
    NvU32 ChipId = Nv3pPrivGetChipId();

    if (!hTrans)
        return NvError_InvalidAddress;

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        return Nv3pTransportOpenT30(hTrans, TIMEOUT_MS);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        return Nv3pTransportOpenT1xx(hTrans, TIMEOUT_MS);
#endif

    return NvError_NotSupported;
}

void Nv3pTransportClose(Nv3pTransportHandle hTrans)
{
    NvU32 ChipId = Nv3pPrivGetChipId();

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        Nv3pTransportCloseT30(hTrans);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        Nv3pTransportCloseT1xx(hTrans);
#endif
}


NvError
Nv3pTransportSend(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags)
{
    NvU32 ChipId = Nv3pPrivGetChipId();

    if (!hTrans)
        return NvError_InvalidAddress;

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        return Nv3pTransportSendT30(hTrans, data, length, flags, NV_WAIT_INFINITE);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        return Nv3pTransportSendT1xx(hTrans, data, length, flags, NV_WAIT_INFINITE);
#endif

    return NvError_NotSupported;
}

NvError
Nv3pTransportReceiveIfComplete(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received)
{
    NvU32 ChipId = Nv3pPrivGetChipId();

    if (!hTrans)
        return NvError_InvalidAddress;

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        return Nv3pTransportReceiveIfCompleteT30(hTrans, data, length,
            received);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        return Nv3pTransportReceiveIfCompleteT1xx(hTrans, data, length,
            received);
#endif

    return NvError_NotSupported;
}
NvError
Nv3pTransportReceive(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 *received,
    NvU32 flags)
{
    NvU32 ChipId = Nv3pPrivGetChipId();

    if (!hTrans)
        return NvError_InvalidAddress;

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        return Nv3pTransportReceiveT30(hTrans, data, length, received, flags, NV_WAIT_INFINITE);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        return Nv3pTransportReceiveT1xx(hTrans, data, length, received, flags, NV_WAIT_INFINITE);
#endif

    return NvError_NotSupported;
}

NvError Nv3pTransportOpenTimeOut(
    Nv3pTransportHandle *hTrans,
    Nv3pTransportMode mode,
    NvU32 instance,
    NvU32 TimeOut)
{
    NvU32 ChipId = Nv3pPrivGetChipId();

    if (!hTrans)
        return NvError_InvalidAddress;

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        return Nv3pTransportOpenT30(hTrans, TimeOut);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        return Nv3pTransportOpenT1xx(hTrans, TimeOut);
#endif

    return NvError_NotSupported;
}


NvError
Nv3pTransportSendTimeOut(
    Nv3pTransportHandle hTrans,
    NvU8 *data,
    NvU32 length,
    NvU32 flags,
    NvU32 TimeOut)
{
    NvU32 ChipId = Nv3pPrivGetChipId();

    if (!hTrans)
        return NvError_InvalidAddress;

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        return Nv3pTransportSendT30(hTrans, data, length, flags, TimeOut);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        return Nv3pTransportSendT1xx(hTrans, data, length, flags, TimeOut);
#endif

    return NvError_NotSupported;
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
    NvU32 ChipId = Nv3pPrivGetChipId();

    if (!hTrans)
        return NvError_InvalidAddress;

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        return Nv3pTransportReceiveT30(hTrans, data, length, received, flags, TimeOut);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        return Nv3pTransportReceiveT1xx(hTrans, data, length, received, flags, TimeOut);
#endif

    return NvError_NotSupported;
}

NvBool
Nv3pTransportUsbfCableConnected(
    Nv3pTransportHandle *hTrans,
    NvU32 Instance)
{
    NvU32 ChipId = Nv3pPrivGetChipId();

    if (ChipId == 0x30)
#ifdef ENABLE_T30
        return Nv3pTransportUsbfCableConnectedT30(hTrans, 0);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        return Nv3pTransportUsbfCableConnectedT1xx(hTrans, 0);
#endif
    return NV_FALSE;
}


NvOdmUsbChargerType
Nv3pTransportUsbfGetChargerType(Nv3pTransportHandle *hTrans,NvU32 instance)
{
    NvU32 ChipId = Nv3pPrivGetChipId();

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        return Nv3pTransportUsbfGetChargerTypeT30(hTrans, 0);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        return Nv3pTransportUsbfGetChargerTypeT1xx(hTrans, 0);
#endif
    return NvOdmUsbChargerType_Dummy;
}

void
Nv3pTransportSetChargingMode(NvBool ChargingMode)
{
    NvU32 ChipId = Nv3pPrivGetChipId();

#ifdef ENABLE_T30
    if (ChipId == 0x30)
        Nv3pTransportSetChargingModeT30(ChargingMode);
#endif
#ifdef ENABLE_T1XX
    if ((ChipId == 0x35) || (ChipId == 0x40))
        Nv3pTransportSetChargingModeT1xx(ChargingMode);
#endif

}
