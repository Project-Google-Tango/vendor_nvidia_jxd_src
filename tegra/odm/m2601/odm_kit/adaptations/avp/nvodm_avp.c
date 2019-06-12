/*
 * Copyright (c) 2010-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nverror.h"
#include "nvodm_services.h"
#include "t30/arfuse.h"
#include "t30/ari2c.h"
#include "nvrm_hardware_access.h"

#define NV_FUSE_REGR(pFuse, reg) NV_READ32( (((NvUPtr)(pFuse)) + FUSE_##reg##_0))

/* Base adderss for arfuse.h registers */
#define FUSE_PA_BASE  0x7000F800

static NvS8
NvBlAvpGetAsicSku(void)
{
    NvU32 ChipSku = 0, GSpeedo = 0, i = 0;
    NvS32 Index = -1;
    NvS8 AsicSku = -1;

    /* FIXME: compiler optimize initialized array to use memcpy. If glibc build is optimize
     * for armv6, things will not go well. Using "static const" prevent compiler
     * to use memcpy. */
    static const NvU32 AsicSpeedoMap[][4] = {
        /* col 1 represents chip sku, col 2/3/4 represents max speedo value
         * for asicsku 0/1/2. Let say chipsku = 0x90 has speedo = 380, the speedo
         * will be compared (<=) with col2 first then col 3/4. In this example
         * 380 <= 430, so it belongs to asic sku1. */
        {0x90, 370, 430, 0},
        {0x91, 370, 430, 0},
        {0x93, 0,   0, 430},
        {0xB0, 370, 430, 0},
        {0xB1, 370, 430, 0}};

    /* Read the fuse register containing the chip skuinfo. */
    ChipSku = NV_FUSE_REGR(FUSE_PA_BASE, SKU_INFO);
    /* lsb 8 bits for chip sku */
    ChipSku &= 0x000000ff;

    /* Read the fuse register containing the chip skuinfo. */
    GSpeedo = NV_FUSE_REGR(FUSE_PA_BASE, SPEEDO_CALIB);
    /* Speedo G = Upper 16-bits Multiplied by 4 */
    GSpeedo = ((GSpeedo >> 16) & 0xFFFF) * 4;

    for (i = 0; i < NV_ARRAY_SIZE(AsicSpeedoMap); i++)
    {
        if (AsicSpeedoMap[i][0] == ChipSku)
        {
            Index = i;
            break;
        }
    }

    if (Index == -1)
        goto fail;

    for (i = 1; i < NV_ARRAY_SIZE(AsicSpeedoMap[0]); i++)
    {
        if (GSpeedo <= AsicSpeedoMap[Index][i])
        {
            AsicSku = i - 1;
            break;
        }
    }
fail:
    return AsicSku;
}

static NvBool
NvBlAvpQueryPmuGpio1()
{
    /* Keep NV_TRUE if require high gpio value.
     * Asic Sku0 = high, sku1 = low and sku2 = low.
     */
    NvBool MapAsicSkuGpioState[] = {NV_TRUE, NV_FALSE, NV_FALSE};
    NvS8 AsicSku;

    AsicSku = NvBlAvpGetAsicSku();
    if ((AsicSku < 0) || (AsicSku >= NV_ARRAY_SIZE(MapAsicSkuGpioState)))
    {
        /* Return low by default */
        return NV_FALSE;
    }

    return MapAsicSkuGpioState[(NvU8)AsicSku];
}

static NvOdmI2cStatus
NvAvpI2cWrite( NvOdmServicesI2cHandle hOdmI2c, NvU8 I2cDevAddress, NvU8 Len, NvU8 *Data)
{
    NvOdmI2cTransactionInfo TransactionInfo;

    TransactionInfo.Address  = I2cDevAddress;
    TransactionInfo.Buf      = Data;
    TransactionInfo.Flags    = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = Len;
    /* NvOdmI2cTransaction only 4 bytes read/write in single txn */
    return (NvOdmI2cTransaction(hOdmI2c, &TransactionInfo, 1, 400, 5));
}

void
NvOdmAvpPreCpuInit(void)
{
    NvOdmServicesI2cHandle hOdmI2c = NULL;
    NvU32 NvPmuRegVal = 0x0461 ;

    hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, 0x04);
    if (!hOdmI2c)
    {
        goto fail;
    }

    if (NvBlAvpQueryPmuGpio1(&NvPmuRegVal))
    {
        /* Make pmu gpio1 high */
        NvPmuRegVal = 0x0561;
    }

    NvAvpI2cWrite(hOdmI2c, 0x5A, 2, &NvPmuRegVal);

fail:
    NvOdmI2cClose(hOdmI2c);
}

void
NvOdmAvpPostCpuInit(void)
{
}

