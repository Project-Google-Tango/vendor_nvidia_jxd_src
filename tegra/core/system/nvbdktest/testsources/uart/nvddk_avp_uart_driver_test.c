/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: DDK UART  Driver Testcase API</b>
 *
 * @b Description: Contains the testcases for the nvddk UART driver.
 */

#include "nvuart.h"
#include "testsources.h"

#define TOTAL_CHAR_COUNT 62
#define COUNT_ALPHABETS_LOWER_CASE 26
#define COUNT_ALPHABETS_LOWER_AND_UPPER_CASE 52


//Transfers(writes)  data of TransmitSize bytes and calculates data transmit(write) rate in KBps
static void UartWritePerf(NvU32 TransmitSize)
{
    NvBool b = NV_FALSE;
    NvError Error = NvSuccess;
    NvU32 LoopCount = 100;
    NvU8 *pWriteBuffer = NULL;
    NvU32 StartTime;
    NvU32 EndTime;
    NvU32 TimeTaken;
    NvU32 DataRate;
    NvU32 Index;
    NvU32 CharCount;
    char *test = "NvUartWritePerfTest";
    char *suite = "uart";
    NvOsDebugPrintf("UartWritePerf():Starting testcase The transmit Size is 0x%08x \n", TransmitSize);
    // Open the Uart
    NvAvpUartInit(PLLP_FIXED_FREQ_KHZ_408000);
    pWriteBuffer = NvOsAlloc(TransmitSize + 1);
    if (!pWriteBuffer)
    {
        Error = NvError_InsufficientMemory;
        NvOsDebugPrintf("UartWritetPerf():Memory allocation to write buffer failed \n");
        TEST_ASSERT_EQUAL(Error , NvSuccess, test, suite, "NvOsAlloc FAILED");
    }
    // This Loop writes Transmit Size bytes of data to debug console. Data contains A-Z, a-z,0-9  characters in sequence.
    for (Index = 0; Index < TransmitSize; ++Index)
    {
        CharCount = Index % TOTAL_CHAR_COUNT;
        if (CharCount < COUNT_ALPHABETS_LOWER_CASE)
        {
            pWriteBuffer[Index] = 'A' + (char)CharCount;
        }
        else if (( CharCount >= COUNT_ALPHABETS_LOWER_CASE) && (CharCount < COUNT_ALPHABETS_LOWER_AND_UPPER_CASE))
        {
            pWriteBuffer[Index] = 'a' + (char)CharCount - COUNT_ALPHABETS_LOWER_CASE;
        }
        else
        {
            pWriteBuffer[Index] = '0' + (char)CharCount - COUNT_ALPHABETS_LOWER_AND_UPPER_CASE;
        }
    }
    pWriteBuffer[TransmitSize] = '\0';
    NvOsDebugPrintf("UartWritePerf():Sending the data 3 times of size %d \n", TransmitSize);
    LoopCount = 1;
    while (LoopCount < 4)
    {
        StartTime = NvOsGetTimeMS();
        NvAvpUartWrite(pWriteBuffer);
        EndTime = NvOsGetTimeMS();
        TimeTaken = EndTime - StartTime;
        DataRate = (TransmitSize*1000)/TimeTaken;
        DataRate = DataRate/1024;
        NvOsDebugPrintf(
          "Trial Count %d Transfer Size %d KBytes, Time Taken %d MilliSecond and Data rate %d KBps \n",
             LoopCount, TransmitSize/1024, TimeTaken, DataRate);
        LoopCount++;
    }
fail:
    NvOsDebugPrintf("UartWritePerf(): Closing Uart handle\n");
    if(pWriteBuffer)
        NvOsFree(pWriteBuffer);
}

NvError NvUartWritePerfTest(void)
{
    UartWritePerf(16*1024);// Write 16KB  data
    UartWritePerf(32*1024);//write 32KB data
    UartWritePerf(64*1024);//write 64 KB data
    UartWritePerf(128*1024);//write 128KB data
    return NvSuccess;
}

//Writes testString 10 times on to Debug Console
NvError NvUartWriteVerifyTest(void)
{
    NvU8 i = 0;
    char *testString = "This is Uart Write Test\n";
    NvAvpUartInit(PLLP_FIXED_FREQ_KHZ_408000);
    while ( i < 10 )
    {
        NvAvpUartWrite(testString);
        i++;
    }
    return NvSuccess;
}

NvError uart_init_reg(void)
{
    NvBDKTest_pSuite pSuite  = NULL;
    NvBDKTest_pTest ptest1 = NULL;
    NvBDKTest_pTest ptest2 = NULL;
    NvError e = NvSuccess;
    const char* err_str = 0;
    e = NvBDKTest_AddSuite("uart", &pSuite);
    if (e != NvSuccess)
    {
        err_str = "Adding suite uart failed.";
        goto fail;
    }

    e = NvBDKTest_AddTest(pSuite, &ptest1, "NvUartWriteVerifyTest",
                        (NvBDKTest_TestFunc)NvUartWriteVerifyTest, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvUartWriteVerifyTest failed.";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest2, "NvUartWritePerfTest",
                        (NvBDKTest_TestFunc)NvUartWritePerfTest, "perf",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvUartWritePerfTest failed.";
        goto fail;
    }
fail:
    if (err_str)
        NvOsDebugPrintf("%s uart_init_reg failed : NvError 0x%x\n",
                        err_str , e);
    return e;
}
