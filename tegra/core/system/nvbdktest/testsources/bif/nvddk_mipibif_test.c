/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/*
 * This driver is independent of rm and can be used across anywhere in
 * the bootloader
 */
#include "nvddk_mipibif.h"
#include "testsources.h"

static NvBool NvDdkMipiBifWriteTest(void)
{
    NvBool Ret = NV_FALSE;
    MipiBifHandle Handle;
    static NvU8 rbuf[20];
    NvU8 wbuf[] = "abcdefgh";

    Handle.DevAddr = 0x01;
    Handle.RegAddr = 0x0F09;
    Handle.Cmds = MIPI_BIF_WRITE;
    Handle.Buf = wbuf;
    Handle.Len = 8;
    Ret = NvDdkMipiBifProcessCmds(&Handle);
    NvOsDebugPrintf("Data written:%s\n", wbuf);
    Handle.Len = 8;
    Handle.Cmds = MIPI_BIF_READDATA;
    Handle.Buf = rbuf;
    Ret = NvDdkMipiBifProcessCmds(&Handle);
    NvOsDebugPrintf("Data read:%s\n", rbuf);
    return Ret;
}
#if 0
static NvBool NvDdkMipiBifReadTest(void)
{
    NvBool Ret = NV_FALSE;
    MipiBifHandle Handle;
    NvU8 rbuf[64];
    NvU32 i;

    Handle.DevAddr = 0x01;
    Handle.RegAddr = 0x0000;
    Handle.Cmds = MIPI_BIF_READDATA;
    Handle.Buf = rbuf;
    Handle.Len = 10;
    Ret = NvDdkMipiBifProcessCmds(&Handle);
    for ( i = 0; i < Handle.Len ; i++)
        NvOsDebugPrintf("rbuf[%d] - %x", i, rbuf[i]);
    return Ret;
}
#endif
NvU32 expected_data [] = {0x10 , 0x03 , 0x08 , 0x00 , 0x01 , 0x1A , 0x06 , 0x19 , 0x00 , 0x24};

static NvBool NvDdkMipiBifSanityTest(void)
{
    NvBool Ret = NV_FALSE;
    MipiBifHandle Handle;
    NvU8 rbuf[64];
    NvU32 i;
    NvDdkMipiBifInit();
    NvDdkMipiBifHardReset();
    NvDdkMipiBifSendBRESCmd();
    NvOsSleepMS(2);
    Handle.DevAddr = 0x01;
    Handle.RegAddr = 0x0000;
    Handle.Cmds = MIPI_BIF_READDATA;
    Handle.Buf = rbuf;
    Handle.Len = 10;
    Ret = NvDdkMipiBifProcessCmds(&Handle);
    NvOsDebugPrintf("sanity o/p\n");
    for ( i = 0; i < Handle.Len ; i++)
        NvOsDebugPrintf("rbuf[%d] - %x", i, rbuf[i]);
    for( i = 0; i < Handle.Len ; i++)
        if (rbuf[i] != expected_data[i])
        {
            return NV_FALSE;
        }
    return Ret;
}

static void NvDdkMipiBifStandByTest(void)
{
    MipiBifHandle Handle;
    NvDdkMipiBifInit();
    NvDdkMipiBifHardReset();
    Handle.DevAddr = 0x01;
    Handle.Cmds = MIPI_BIF_BUS_COMMAND_STBY;
    NvDdkMipiBifProcessCmds(&Handle);
}

static void NvDdkMipiBifActivateTest(void)
{
    MipiBifHandle Handle;
    NvDdkMipiBifInit();
    NvDdkMipiBifHardReset();
    Handle.DevAddr = 0x01;
    Handle.Cmds = MIPI_BIF_ACTIVATE;
    NvDdkMipiBifProcessCmds(&Handle);
}


static NvBool NvDdkMipiBifInterruptTest(void)
{
        MipiBifHandle Handle;
        NvU8 Wbuf[8];
        NvBool Ret;

        NvDdkMipiBifInit();

        Handle.DevAddr = 0x01;
        Handle.Cmds = MIPI_BIF_WRITE;
        Handle.RegAddr = 0x0C00;
        Wbuf[0] = 0x3;
        Handle.Buf = Wbuf;
        Handle.Len =1;
        Ret = NvDdkMipiBifProcessCmds(&Handle);

        Handle.Cmds = MIPI_BIF_WRITE;
        Handle.RegAddr = 0x0C03;
        Wbuf[0] = 0x3;
        Handle.Buf = Wbuf;
        Handle.Len =1;
        Ret = NvDdkMipiBifProcessCmds(&Handle);

        Handle.Cmds = MIPI_BIF_WRITE;
        Handle.RegAddr = 0x0F06;
        Wbuf[0] = 0x0;
        Wbuf[1] = 0x0;
        Wbuf[2] = 0x1;
        Wbuf[3] = 0xAA;
        Handle.Buf = Wbuf;
        Handle.Len = 4;
        Ret = NvDdkMipiBifProcessCmds(&Handle);

        Handle.Cmds = MIPI_BIF_INT_READ;
        Ret = NvDdkMipiBifProcessCmds(&Handle);

        return Ret;
}
static NvError NvDdkMipiBifTest(void)
{
    NvBool Ret;
    NvBool b= NV_FALSE;
    NvError e = NvSuccess;
    NvDdkMipiBifInit();
    NvDdkMipiBifHardReset();
    NvDdkMipiBifSendBRESCmd();
    NvOsSleepMS(2);
    NvOsDebugPrintf("Write-read test\n");
    Ret = NvDdkMipiBifWriteTest();
    if (Ret == NV_FALSE)
        NvOsDebugPrintf("Bif Write Test failed\n");
    else
        NvOsDebugPrintf("Bif Write Test passed\n");
    if (!Ret)
    {
        e = NvError_InvalidOperation;
        TEST_ASSERT_EQUAL(e, NvSuccess, "NvDdkMipiBifTest" ,
            "mipibif", "NvDdkMipiBifWriteTest FAILED");
        goto fail;
    }
    NvOsDebugPrintf("sanity test \n");
    Ret = NvDdkMipiBifSanityTest();
    if (Ret == NV_FALSE)
        NvOsDebugPrintf("NvDdkMipiBifSanityTest failed\n");
    else
        NvOsDebugPrintf("NvDdkMipiBifSanityTest passed\n");
    if (!Ret)
    {
        e = NvError_InvalidOperation;
        TEST_ASSERT_EQUAL(e, NvSuccess, "NvDdkMipiBifTest" ,
            "mipibif", "NvDdkMipiBifSanityTest FAILED");
        goto fail;
    }
    NvOsDebugPrintf("Interrupt test\n");
    Ret= NvDdkMipiBifInterruptTest();
    if (!Ret)
    {
        e = NvError_InvalidOperation;
        TEST_ASSERT_EQUAL(e, NvSuccess, "NvDdkMipiBifTest" ,
            "mipibif", "NvDdkMipiBifInterrupTest FAILED");
        goto fail;
    }

    NvOsDebugPrintf("standby test\n");
    NvDdkMipiBifStandByTest();
    NvDdkMipiBifActivateTest();
    Ret = NvDdkMipiBifWriteTest();
    if (Ret == NV_FALSE)
        NvOsDebugPrintf("NvDdkMipiBifStandByTest failed\n");
    else
        NvOsDebugPrintf("NvDdkMipiBifStandByTest passed\n");
    if (!Ret)
    {
        e = NvError_InvalidOperation;
        TEST_ASSERT_EQUAL(e, NvSuccess, "NvDdkMipiBifTest" ,
            "mipibif", "NvDdkMipiBifStandByTest FAILED");
        goto fail;
    }

fail:
    return e;
}

NvError mipibif_init_reg(void)
{
    NvBDKTest_pSuite pSuite  = NULL;
    NvBDKTest_pTest ptest1 = NULL;
     NvError e = NvSuccess;
    const char* err_str = 0;
    NvOsDebugPrintf("\n mipibif_init_reg done \n");
    e = NvBDKTest_AddSuite("mipibif", &pSuite);
    if (e != NvSuccess)
    {
        err_str = "Adding suite mipibif failed.";
        goto fail;
    }

    e = NvBDKTest_AddTest(pSuite, &ptest1, "NvDdkMipiBifTest",
                        (NvBDKTest_TestFunc)NvDdkMipiBifTest, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "NvDdkMipiBifTest failed.";
        goto fail;
    }
 fail:
    if(err_str)
        NvOsDebugPrintf("%s mipibif_init_reg failed : NvError 0x%x\n",
                        err_str , e);
    return e;
}
