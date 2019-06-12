/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "testsources.h"
#include "nvrm_hardware_access.h"
#include "nvbdk_pcb_api.h"
#include "nvbdk_pcb_interface.h"

void NvApiI2cTransfer(NvBDKTest_TestPrivData param)
{
    NvU32 instanceParam = param.Instance;
    PcbTestData *pPcbData = (PcbTestData *)param.pData;
    I2cTestPrivateData *pI2cData = (I2cTestPrivateData *)pPcbData->privateData;
    NvError e = NvSuccess;
    NvBool b;
    NvRmI2cHandle hI2c = NULL;
    NvRmI2cTransactionInfo i2cInfo;
    NvRmDeviceHandle rmHandle = NULL;
    NvU8 readBuf[I2C_MAX_READ_BUF_SIZE];
    NvU8 *buf = NULL;
    I2cTestPrivateData *p;
    NvU32 instance = 0xFFFFFFFF;
    char szAssertBuf[1024];

    e = NvRmOpen(&rmHandle, 0);
    if (e != NvSuccess)
        TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", "NvRmOpen Failed");

    for (p = pI2cData; p->command != _I2C_CMD_END_; p++)
    {
        if ((instance != p->instance) || !hI2c)
        {
            if (hI2c)
            {
                NvRmI2cClose(hI2c);
                hI2c = NULL;
            }

            e = NvRmI2cOpen(rmHandle, NvOdmIoModule_I2c, p->instance, &hI2c);
            if (e != NvSuccess)
                TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", "NvRmI2cOpen Failed");

            instance = p->instance;
        }

        switch (p->command) {
        case _I2C_CMD_WRITE_:
            i2cInfo.Flags = NVRM_I2C_WRITE;
            buf = (p->rwData).buf;
            break;
        case _I2C_CMD_READ_:
            i2cInfo.Flags = NVRM_I2C_READ;
            buf = (p->rwData).buf ? (p->rwData).buf : readBuf;
            break;
        default:
            break;
        }

        i2cInfo.Address = p->i2cAddr << 1;
        i2cInfo.NumBytes = (p->rwData).len;
        i2cInfo.Is10BitAddress = NV_FALSE;
        e = NvRmI2cTransaction(hI2c,
                               NvOdmIoModule_I2c,
                               NV_WAIT_INFINITE,
                               100,
                               buf,
                               (p->rwData).len,
                               &i2cInfo,
                               1);
        if (e != NvSuccess)
            TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name,
                                "pcb", "NvRmI2cTransaction Failed");

        if (p->command == _I2C_CMD_READ_)
        {
            if ((p->validationData).len > 0)
            {
                NvU32 i = 0;
                NvU8 deviceID = 0;
                if ((instanceParam > 0) && ((p->validationData).len <= 4))
                {
                    for (i = 0; i < (p->validationData).len; i++)
                    {
                        deviceID = (instanceParam >> (i * 8)) & 0xFF;
                        if (buf[i] != deviceID)
                        {
                            NvOsSnprintf(szAssertBuf, 1024,
                                         "(Checking deviceID Failed. "
                                         "%d-th deviceID[0x%02x] doesn't correspond with [0x%02x])",
                                         i + 1, buf[i], deviceID);
                            TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", szAssertBuf);
                        }
                    }
                }
                else
                {
                    for (i = 0; i < (p->validationData).len; i++)
                    {
                        deviceID = (p->validationData).buf[i];
                        if (buf[i] != deviceID)
                        {
                            NvOsSnprintf(szAssertBuf, 1024,
                                         "(Checking deviceID Failed. "
                                         "%d-th deviceID[0x%02x] doesn't correspond with [0x%02x])",
                                         i + 1, buf[i], deviceID);
                            TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", szAssertBuf);
                        }
                    }
                }
            }
        }
    }

fail:
    if (hI2c)
        NvRmI2cClose(hI2c);
    if (rmHandle) {
        NvRmClose(rmHandle);
        rmHandle = NULL;
    }
}
