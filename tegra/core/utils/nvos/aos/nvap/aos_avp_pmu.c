/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "aos_avp_pmu.h"
#include "nvddk_i2c.h"

#define SLAVE_FREQUENCY           400   // KHz
#define SLAVE_TRANSACTION_TIMEOUT 1000  // Ms

static NvDdkI2cSlaveHandle s_hPmu;

static void NvBlPmuInit(NvU16 Address)
{
    static NvU16 s_LastAddress = 0;

    if (s_LastAddress != Address && s_hPmu != NULL)
    {
        NvDdkI2cDeviceClose(s_hPmu);
        s_hPmu = NULL;
    }

    if (s_hPmu == NULL)
    {
        NvDdkI2cDeviceOpen(NvDdkI2c5, Address, 0, SLAVE_FREQUENCY,
                           SLAVE_TRANSACTION_TIMEOUT, &s_hPmu);

        s_LastAddress = Address;
    }
}

void NvBlPmuWrite(NvU16 Addr, NvU8 Offset, NvU8 Data)
{
    NvBlPmuInit(Addr);
    if (s_hPmu != NULL)
        NvDdkI2cDeviceWrite(s_hPmu, Offset, &Data, 1);
}

NvU32 NvBlPmuRead(NvU16 Addr, NvU8 Offset, NvU8 NumBytes)
{
    NvU32 Ret = 0;

    NvBlPmuInit(Addr);
    if (NumBytes > 4 || s_hPmu == NULL)
        return 0;

    NvDdkI2cDeviceRead(s_hPmu, Offset, (NvU8 *) &Ret, NumBytes);

    return Ret;
}

void NvBlDeinitPmuSlaves(void)
{
    if (s_hPmu != NULL)
        NvDdkI2cDeviceClose(s_hPmu);
}
