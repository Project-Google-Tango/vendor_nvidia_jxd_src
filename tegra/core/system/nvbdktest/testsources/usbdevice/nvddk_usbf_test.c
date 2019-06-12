/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: Usbf  Driver Testcase API</b>
 *
 * @b Description: Contains the testcases for the Usbf driver.
 */
#include "testsources.h"
#include "nv3p.h"
#include "nverror.h"
#include "nvos.h"

#define DATA_CHUNK_SIZE_1MB (1024 * 1024)

Nv3pTransportMode TransportMode = Nv3pTransportMode_Usb;
NvU32 devinstance = 0;
Nv3pSocketHandle s_hSock;

// This test sends 1024 bytes of data(each containing integer value 99) to host ,
//calculates the datarate after a check for successful data send and receive
NvError NvUsbfPerfTest(void)
{
    NvBool b = NV_FALSE;
    NvError Error = NvSuccess;
    NvU32 StartTime;
    NvU32 EndTime;
    NvU32 TimeTakenToSend = 1;
    NvU32 DataSendRate;
    NvU32 DataReceiveRate;
    char *test = "NvUsbfPerfTest";
    char *suite = "usbf";
    NvU8 *send_buf = 0;
    NvU8 *rec_buf = 0;
    char *err_str = 0;
    NvU32 bytes;
    send_buf = NvOsAlloc(DATA_CHUNK_SIZE_1MB);
    if (send_buf ==  NULL)
    {
        NvOsDebugPrintf("Out of memory Error\n");
        Error = NvError_InsufficientMemory;
        goto fail;
    }
    rec_buf = NvOsAlloc(DATA_CHUNK_SIZE_1MB);
    if (rec_buf ==  NULL)
    {
        NvOsDebugPrintf("Out of memory Error\n");
        Error = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(send_buf, 99, DATA_CHUNK_SIZE_1MB);
    NvOsMemset(rec_buf, 0, DATA_CHUNK_SIZE_1MB);
    Error = Nv3pReOpen(&s_hSock, TransportMode, devinstance);
    TEST_ASSERT_EQUAL(Error , NvSuccess, test, suite, "Nv3pOpen FAILED");
    StartTime = NvOsGetTimeMS();
    Error = Nv3pDataSend(s_hSock, send_buf, DATA_CHUNK_SIZE_1MB, 0);
    EndTime = NvOsGetTimeMS();
    TimeTakenToSend = EndTime - StartTime;
    TEST_ASSERT_EQUAL(Error , NvSuccess, test, suite, "Nv3pDataSend FAILED");
    StartTime = NvOsGetTimeMS();
    Error = Nv3pDataReceive(s_hSock, rec_buf, DATA_CHUNK_SIZE_1MB, &bytes, 0);
    EndTime = NvOsGetTimeMS();
    TEST_ASSERT_EQUAL(Error , NvSuccess, test, suite, "Nv3pDataReceive FAILED");
    if (!NvOsMemcmp(send_buf, rec_buf, DATA_CHUNK_SIZE_1MB))
    {
        DataSendRate = DATA_CHUNK_SIZE_1MB / (TimeTakenToSend * 1000);
        DataReceiveRate =  DATA_CHUNK_SIZE_1MB / (TimeTakenToSend * 1000);
        NvOsDebugPrintf("\nUSBDevice Test:Data Send/Receive Successful\n");
        NvOsDebugPrintf("DataSendRate = %dMBps\nDataReceiveRate = %dMBps",
                    DataSendRate, DataReceiveRate);
    }
    goto clean ;
fail:
    if (err_str)
        NvOsDebugPrintf("Usbf Perf Test Failed. Error: %s\n", err_str);
clean:
    if (send_buf)
        NvOsFree(send_buf);
    if (rec_buf)
        NvOsFree(rec_buf);
    return Error;
}

NvError usbf_init_reg(void)
{
    NvBDKTest_pSuite pSuite  = NULL;
        NvBDKTest_pTest ptest1 = NULL;
    NvError e = NvSuccess;
    const char* err_str = 0;
    e = NvBDKTest_AddSuite("usbf", &pSuite);
    if (e != NvSuccess)
    {
        err_str = "Adding suite usbf failed.";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest1, "NvUsbfPerfTest",
                        (NvBDKTest_TestFunc)NvUsbfPerfTest, "perf",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvUsbfPerfTest failed.";
        goto fail;
    }
fail:
    if (err_str)
        NvOsDebugPrintf("%s usb_init_reg failed : NvError 0x%x\n",
                        err_str , e);
    return e;
}
