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
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"
#include "nvbdk_pcb_interface.h"

void NvApiEmmcCheck(NvBDKTest_TestPrivData param)
{
    PcbTestData *pPcbData = (PcbTestData *)param.pData;
    EmmcTestPrivateData *pEmmcData = (EmmcTestPrivateData *)pPcbData->privateData;
    NvBool b;
    NvBool bInit = NV_FALSE;
    NvError e = NvSuccess;
    NvDdkBlockDevHandle h_sdbdk = NULL;
    NvDdkBlockDevInfo device_info;

    NvOsMemset(&device_info, 0, sizeof(device_info));

    e = NvDdkBlockDevMgrInit();
    if (e != NvSuccess)
        TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", "NvDdkSDBlockDevinit Failed");

    bInit = NV_TRUE;
    e = NvDdkBlockDevMgrDeviceOpen(NvDdkBlockDevMgrDeviceId_SDMMC,
                                   pEmmcData->instance, 0, &h_sdbdk);
    if (e != NvSuccess)
        TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", "NvDdkSDBlockDevOpen Failed");

    if (h_sdbdk->NvDdkBlockDevGetDeviceInfo) {
        h_sdbdk->NvDdkBlockDevGetDeviceInfo(h_sdbdk, &device_info);
    }

    if (pEmmcData->pValidationFunc)
        e = pEmmcData->pValidationFunc(device_info.Private);
    else
        e  = NvError_InvalidState;

    if (e != NvSuccess)
    {
        if (e == NvError_InvalidState)
        {
            TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name,
                                "pcb", "Cannot verify CID because of invalid status");
        }
        else
        {
            char szAssertBuf[1024];
            NvU32 cid[4];

            NvOsMemcpy(&cid, device_info.Private, sizeof(cid));
            NvOsSnprintf(szAssertBuf, 1024,
                         "(Checking CID Failed. "
                         "CID_PRODUCT_NAME_2[0x%08x] "
                         "CID_PRODUCT_NAME_1[0x%08x] "
                         "CID_MID_CBX_OID[0x%08x], "
                         "they don't correspond with the defined value)",
                         (cid[1] & 0xFFFF0000), cid[2], (cid[3] & 0x00FFFFFF));

            TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", szAssertBuf);
        }
    }

fail:
    if (h_sdbdk)
        h_sdbdk->NvDdkBlockDevClose(h_sdbdk);
    if (bInit)
        NvDdkBlockDevMgrDeinit();
}

