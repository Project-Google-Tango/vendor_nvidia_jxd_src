/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_services.h"

/* FIXME: Move these prototypes to a TBD common header */
NvOdmI2cStatus
NvOdmDispHdmiI2cTransaction(
    NvOdmServicesI2cHandle hOdmI2c,
    NvOdmI2cTransactionInfo *pTransaction,
    NvU32 NumOfTransactions,
    NvU32 ClockSpeedKHz,
    NvU32 WaitTimeoutInMs);
NvBool NvOdmDispHdmiI2cOpen(void);
void NvOdmDispHdmiI2cClose(void);

NvOdmI2cStatus
NvOdmDispHdmiI2cTransaction(
    NvOdmServicesI2cHandle hOdmI2c,
    NvOdmI2cTransactionInfo *pTransaction,
    NvU32 NumOfTransactions,
    NvU32 ClockSpeedKHz,
    NvU32 WaitTimeoutInMs)
{
    NvU8 lgwv4_edid[] =
    {
        0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x04,0x21,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x00,0x01,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
        0x01,0x01,0x01,0x01,0x01,0x01,0xFE,0x0C,0x20,0x00,0x31,0xE0,0x2D,0x10,0x40,0x40,
        0x43,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xAD
    };

    /*
     * p1852 sku2 has the following 2nd display path:
     * Tegra HDMI interface -> RGB converter -> LVDS serializer
     *
     * Since DDC reads are not possible in this configuration, a default EDID
     * (corresponding to the LGWV4 panel) is returned.
     */

    /* return default EDID only when first EDID read is performed */
    if (NumOfTransactions != 2 || !pTransaction)
        return NvOdmI2cStatus_InternalError;
    else if (!(pTransaction[0].Flags & NVODM_I2C_IS_WRITE) ||
             pTransaction[0].NumBytes != 1 || pTransaction[0].Buf[0] != 0)
        return NvOdmI2cStatus_InternalError;
    else if (pTransaction[1].Flags & NVODM_I2C_IS_WRITE ||
             pTransaction[1].NumBytes != sizeof(lgwv4_edid))
        return NvOdmI2cStatus_InternalError;

    NvOdmOsMemcpy(pTransaction[1].Buf, lgwv4_edid, sizeof(lgwv4_edid));
    return NvOdmI2cStatus_Success;
}

NvBool NvOdmDispHdmiI2cOpen(void)
{
    return NV_TRUE;
}

void NvOdmDispHdmiI2cClose(void)
{
}
