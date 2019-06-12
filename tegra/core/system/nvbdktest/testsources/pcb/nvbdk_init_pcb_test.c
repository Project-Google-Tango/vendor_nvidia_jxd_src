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
#include "nvbdk_pcb_interface.h"
#include "nvbl_memmap_nvap.h"
#include "nvodm_services.h"
#include "nvodm_query_gpio.h"

NvError pcb_init_reg(void)
{
    NvBDKTest_pSuite pSuite  = NULL;
    NvBDKTest_pTest pTest = NULL;
    NvError e = NvSuccess;
    const char *err_str = 0;
    PcbTestData *testList = NULL;
    int i;

    testList = NvGetPcbTestDataList();
    if (testList)
    {
        e = NvBDKTest_AddSuite("pcb", &pSuite);
        if (e != NvSuccess) {
            err_str = "Adding suite pcb failed.";
            goto fail;
        }

        for (i = 0; testList[i].type; i++) {
            e = NvBDKTest_AddTest(pSuite, &pTest, testList[i].name,
                                  testList[i].testFunc, testList[i].type,
                                  &(testList[i]), NULL);
            if (e != NvSuccess) {
                err_str = "Adding test func failed.";
                goto fail;
            }
        }
    }
    return e;

fail:
    if (err_str)
        NvOsDebugPrintf("%s %s failed : NvError 0x%x\n",
                        err_str , __func__, e);
    return e;
}
