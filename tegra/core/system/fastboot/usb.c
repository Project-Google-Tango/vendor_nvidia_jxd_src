/*
 * Copyright (c) 2009 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvodm_query_nv3p.h"
#include "fastboot.h"

/* The USB driver implementation in NV3P defines several weakly-linked
 * ODM functions to return parameters for the interface descriptor and
 * product ID string.  The implementations below are for supporting both
 * the recovery mode protocol (3P Server) and Android's Fastboot.
 */

static NvBool s_Is3pServerMode = NV_FALSE;

void FastbootSetUsbMode(NvBool Is3p)
{
    s_Is3pServerMode = Is3p;
}

NvBool NvOdmQueryUsbConfigDescriptorInterface(
    NvU32 *pIfcClass,
    NvU32 *pIfcSubclass,
    NvU32 *pIfcProtocol)
{
    if (s_Is3pServerMode)
        return NV_FALSE;

    *pIfcClass = 0xff;
    *pIfcSubclass = 0x42;
    *pIfcProtocol = 0x03;
    return NV_TRUE;
}

NvBool NvOdmQueryUsbDeviceDescriptorIds(
    NvU32 *pVendorId,
    NvU32 *pProductId,
    NvU32 *pBcdDeviceId)
{
    if (s_Is3pServerMode)
        return NV_FALSE;

    *pVendorId = 0x0955;
    *pProductId = 0x7000;
    *pBcdDeviceId = 0;
    return NV_TRUE;
}

NvU8 *NvOdmQueryUsbProductIdString(void)
{
    NV_ALIGN(4) static NvU8 RecoveryString[] = {
        0x00, 0x03,
        'F', 0x00,
        'a', 0x00,
        's', 0x00,
        't', 0x00,
        'b', 0x00,
        'o', 0x00,
        'o', 0x00,
        't', 0x00};

    RecoveryString[0] = NV_ARRAY_SIZE(RecoveryString);

    if (s_Is3pServerMode)
        return NULL;
    else
    {
        return RecoveryString;
    }
}

/* This matches the serial string implementation from the Linux Kernel.
   Any changes need to the output need to be synchronized so that
   'adb devices' and 'fastboot devices' use the same values */
NvU8 *NvOdmQueryUsbSerialNumString(void)
{
    NvError e;
    NvU32 i;
    NvU32 SerialLen;
    char Buff[MAX_SERIALNO_LEN] = {0};
    NV_ALIGN(4) static NvU8 SerialString[MAX_SERIALNO_LEN * 2 + 2] =
    {
        0x0, 0x03
    };

    /* is serial no fetched already? */
    if (SerialString[0] != 0)
        goto fail;

    NV_CHECK_ERROR_CLEANUP(FastbootGetSerialNo(Buff, MAX_SERIALNO_LEN));

    SerialLen = NvOsStrlen(Buff);
    SerialString[0] = SerialLen * 2 + 2;

    for (i = 0; i < SerialLen; i++)
        SerialString[i * 2 + 2] = Buff[i];

fail:
    return SerialString;
}
