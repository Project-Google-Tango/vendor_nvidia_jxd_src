 /*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: DDK Fuse Read-Bypass Testcase API</b>
 *
 * @b Description: Contains the testcases for the nvddk Fuse read and Fuse bypass APIs
 */

#include "nvddk_fuse.h"
#include "arfuse.h"
#include "testsources.h"

NvError NvFuseReadTest(NvBDKTest_TestPrivData data)
{
    NvU32 Size;
    NvError e = NvSuccess;

    NvBool OdmProduction;
    NvU32 PackageInfo, DeviceKey, Sku;

    //Device key
    Size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_DeviceKey,
                          (void *)&DeviceKey, (NvU32 *)&Size));
    NvOsDebugPrintf("Device key : %d\n", DeviceKey);

    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_Sku,
                          (void *)&Sku, (void *)&Size));
    NvOsDebugPrintf("Sku: %d\n",Sku);

    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_PackageInfo,
                          (void *)&PackageInfo, (NvU32 *)&Size));
    NvOsDebugPrintf("PackageInfo: %d\n",PackageInfo);

    Size = sizeof(NvBool);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OdmProduction,
              (void *)&OdmProduction, (NvU32 *)&Size));
    if (OdmProduction)
        NvOsDebugPrintf("OdmProduction is true\n");
    else
        NvOsDebugPrintf("OdmProduction is false\n");

fail:

        return e;
}


NvError NvFuseBypassTest(NvBDKTest_TestPrivData data)
{
    NvU32 Size;
    NvError e = NvSuccess;

    NvBool OdmProduction;
    NvU32 BootDevConfig, Sku;
    NvFuseBypassInfo bypassInfo;

    //Device key
    NvOsDebugPrintf("=========Reading fuse values before bypass=========\n");
    Size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceConfig,
                          (void *)&BootDevConfig, (NvU32 *)&Size));
    NvOsDebugPrintf("BootDeviceConfig : %d\n", BootDevConfig);

    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_Sku,
                          (void *)&Sku, (void *)&Size));
    NvOsDebugPrintf("Sku: %d\n",Sku);

    Size = sizeof(NvBool);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OdmProduction,
              (void *)&OdmProduction, (NvU32 *)&Size));
    if (OdmProduction)
        NvOsDebugPrintf("OdmProduction is true\n");
    else
        NvOsDebugPrintf("OdmProduction is false\n");

    NvOsDebugPrintf("=========Bypassing fuses========\n");


    NvOsMemset(&bypassInfo , 0, sizeof(bypassInfo));
    bypassInfo.num_fuses = 3;

    bypassInfo.fuses[0].offset = FUSE_BOOT_DEVICE_INFO_0;
    bypassInfo.fuses[0].fuse_value[0] = 0x3;

    bypassInfo.fuses[1].offset = FUSE_SKU_INFO_0;
    bypassInfo.fuses[1].fuse_value[0] = 0x20;

    bypassInfo.fuses[2].offset = FUSE_SECURITY_MODE_0;
    bypassInfo.fuses[2].fuse_value[0] = 0x1;

    NV_CHECK_ERROR_CLEANUP(NvDdkFuseBypassSku(&bypassInfo));

    NvOsDebugPrintf("=========Reading fuse values after bypass=========\n");
    Size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceConfig,
                          (void *)&BootDevConfig, (NvU32 *)&Size));
    NvOsDebugPrintf("BootDeviceConfig : %d\n", BootDevConfig);

    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_Sku,
                          (void *)&Sku, (void *)&Size));
    NvOsDebugPrintf("Sku: %d\n",Sku);

    Size = sizeof(NvBool);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OdmProduction,
              (void *)&OdmProduction, (NvU32 *)&Size));
    if (OdmProduction)
        NvOsDebugPrintf("OdmProduction is true\n");
    else
        NvOsDebugPrintf("OdmProduction is false\n");

fail:
        return e;
}

NvError fuse_init_reg(void)
{
    NvBDKTest_pSuite pSuite  = NULL;
    NvBDKTest_pTest ptest1 = NULL;
    NvBDKTest_pTest ptest2 = NULL;
    NvError e = NvSuccess;
    const char* err_str = 0;

    e = NvBDKTest_AddSuite("fuse", &pSuite);
    if (e != NvSuccess)
    {
        err_str = "Adding suite fuse failed.";
        goto fail;
    }

    e = NvBDKTest_AddTest(pSuite, &ptest1, "NvFuseReadTest",
                        (NvBDKTest_TestFunc)NvFuseReadTest, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvFuseReadTest failed.";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest2, "NvFuseBypassTest",
                        (NvBDKTest_TestFunc)NvFuseBypassTest, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvFuseBypassTest failed.";
        goto fail;
    }

fail:
    if(err_str)
        NvOsDebugPrintf("%s fuse_init_reg failed : NvError 0x%x\n",
                        err_str , e);
    return e;
}
