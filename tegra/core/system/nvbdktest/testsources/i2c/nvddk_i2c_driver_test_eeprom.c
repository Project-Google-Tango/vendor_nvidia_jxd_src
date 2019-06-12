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
 * @brief <b>NVIDIA Driver Development Kit: I2c  Driver Testcase API</b>
 *
 * @b Description: Contains the testcases for the I2c driver.
 */

#include "nvrm_i2c.h"
#include "testsources.h"

#define EEPROM_I2C_SLAVE_ADDRESS  0xAC
#define EEPROM_PAGE_SIZE 8
#define EEPROM_TEST_DATA_1 0xBB
#define EEPROM_TEST_DATA_2 0xCC
#define EEPROM_TEST_DATA_3 0xDD
#define EEPROM_TEST_DATA_1_WRITE_OFFSET 64
#define EEPROM_TEST_DATA_2_WRITE_OFFSET 72
#define EEPROM_TEST_DATA_3_WRITE_OFFSET 80
#define EEPROM_BOARDID_OFFSET       0x4
#define EEPROM_WRITE_CYCLE_TIME_MS  0x5

enum { NVEEPROM_UTIL_I2C_TEST_CLOCK_FREQ_KHZ = 400 };

//Verifies the I2c Driver Write / Read by checking Write and Read data to EEPROM.
NvError NvI2cEepromWRTest(void)
{
    NvBool b = NV_FALSE;
    NvError e = NvSuccess;
    NvRmI2cHandle hI2c;
    NvRmDeviceHandle hRmDev;
    NvU8 *i2cRbuffer = NULL;
    NvRmI2cTransactionInfo *i2cRtransinfo = NULL;
    NvU8 i2cRstack_buffer[9];
    NvS8 i=0;
    const char *test = "NvI2cEepromWRTest";
    const char *suite = "i2c";
    NvU8 *i2cWbuffer = NULL;
    NvError err= NvSuccess;
    NvRmI2cTransactionInfo *i2cWtransinfo = NULL;
    NvU8 i2cWstack_buffer[9];
    i2cRbuffer = i2cRstack_buffer;
    i2cRtransinfo = (NvRmI2cTransactionInfo *)NvOsAlloc(sizeof(NvRmI2cTransactionInfo));
    i2cRtransinfo->Flags = NVRM_I2C_READ;
    i2cRtransinfo->NumBytes = 4;
    i2cRtransinfo->Is10BitAddress = NV_FALSE;
    i2cRtransinfo->Address = EEPROM_I2C_SLAVE_ADDRESS;

    NvOsMemset(i2cWstack_buffer, EEPROM_TEST_DATA_1, EEPROM_PAGE_SIZE + 1);
    i2cWstack_buffer[0] = EEPROM_TEST_DATA_1_WRITE_OFFSET;
    i2cWbuffer = i2cWstack_buffer;
    i2cWtransinfo = (NvRmI2cTransactionInfo *)NvOsAlloc(sizeof(NvRmI2cTransactionInfo));
    i2cWtransinfo->Flags = NVRM_I2C_WRITE;
    i2cWtransinfo->NumBytes = EEPROM_PAGE_SIZE + 1;
    i2cWtransinfo->Is10BitAddress = NV_FALSE;
    i2cWtransinfo->Address = EEPROM_I2C_SLAVE_ADDRESS;
    NvOsMemset(i2cRstack_buffer, 0, EEPROM_PAGE_SIZE + 1);
    i2cRbuffer = i2cRstack_buffer;
    e = NvRmOpen(&hRmDev, 0);
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\n NvRmOpen I2C Failed\n ");
        TEST_ASSERT_EQUAL(e, NvSuccess, test, suite, "NvRmOpen FAILED");
    }
    e = NvRmI2cOpen(hRmDev,NvOdmIoModule_I2c, 0, &hI2c);
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\n NvRmI2cOpen I2C  Failed\n");
        TEST_ASSERT_EQUAL(e, NvSuccess, test, suite, "NvRmI2cOpen FAILED");
    }
    //This is write transaction. First byte of i2cWbuffer hold offset address and
    //remaining bytes are data to be written on to eeprom.
    NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
            NV_WAIT_INFINITE,
            NVEEPROM_UTIL_I2C_TEST_CLOCK_FREQ_KHZ,
            i2cWbuffer,
            EEPROM_PAGE_SIZE + 1,
            i2cWtransinfo,
            1);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("\n Write failed I2C \n");
        TEST_ASSERT_EQUAL(err, NvSuccess, test, suite, "NvRmI2cTransaction FAILED");
    }
    // Wait for EEPROM to complete internal write before sending next read/write
    // command. EEPROM will not acknowledge the slave address for next commands,
    // if it is busy with internal write.
    NvOsSleepMS(EEPROM_WRITE_CYCLE_TIME_MS);
    NvOsMemset(i2cWstack_buffer, EEPROM_TEST_DATA_2, EEPROM_PAGE_SIZE + 1);
    i2cWstack_buffer[0] = EEPROM_TEST_DATA_2_WRITE_OFFSET;
    i2cWbuffer = i2cWstack_buffer;
    err = NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
                        NV_WAIT_INFINITE,
                        NVEEPROM_UTIL_I2C_TEST_CLOCK_FREQ_KHZ,
                        i2cWbuffer,
                        EEPROM_PAGE_SIZE + 1,
                        i2cWtransinfo,
                        1);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("\n Write failed I2C \n");
        TEST_ASSERT_EQUAL(err, NvSuccess, test, suite, "NvRmI2cTransaction FAILED");
    }
    // Wait for EEPROM to complete internal write before sending next read/write
    // command.
    NvOsSleepMS(EEPROM_WRITE_CYCLE_TIME_MS);
    NvOsMemset(i2cWstack_buffer, EEPROM_TEST_DATA_3, EEPROM_PAGE_SIZE + 1);
    i2cWstack_buffer[0] = EEPROM_TEST_DATA_3_WRITE_OFFSET;
    i2cWbuffer = i2cWstack_buffer;
    err=NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
            NV_WAIT_INFINITE,
            NVEEPROM_UTIL_I2C_TEST_CLOCK_FREQ_KHZ,
            i2cWbuffer,
            EEPROM_PAGE_SIZE + 1,
            i2cWtransinfo,
            1);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("\n Write failed I2C \n");
        TEST_ASSERT_EQUAL(err, NvSuccess, test, suite, "NvRmI2cTransaction FAILED");
    }
    // Wait for EEPROM to complete internal write before sending next read/write
    // command.
    NvOsSleepMS(EEPROM_WRITE_CYCLE_TIME_MS);
    i2cWstack_buffer[0] = EEPROM_TEST_DATA_1_WRITE_OFFSET;
    i2cWtransinfo->NumBytes = 1;
    err=NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
                    NV_WAIT_INFINITE,
                    NVEEPROM_UTIL_I2C_TEST_CLOCK_FREQ_KHZ,
                    i2cWbuffer,
                    1,
                    i2cWtransinfo,
                    1);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("\n Write failed I2C \n");
        TEST_ASSERT_EQUAL(err, NvSuccess, test, suite, "NvRmI2cTransaction FAILED");
    }
    err = NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
            NV_WAIT_INFINITE,
            NVEEPROM_UTIL_I2C_TEST_CLOCK_FREQ_KHZ,
            i2cRbuffer,
            EEPROM_PAGE_SIZE,
            i2cRtransinfo,
            1);
    for (i=0; i < EEPROM_PAGE_SIZE; i++ )
    {
        NvOsDebugPrintf("DATA_1 =  0x%X\n", i2cRbuffer[i]);
    }
    i2cWstack_buffer[0] = EEPROM_TEST_DATA_2_WRITE_OFFSET;
    i2cWtransinfo->NumBytes = 1;
    err=NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
                    NV_WAIT_INFINITE,
                    NVEEPROM_UTIL_I2C_TEST_CLOCK_FREQ_KHZ,
                    i2cWbuffer,
                    1,
                    i2cWtransinfo,
                    1);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("\n Write failed I2C \n");
        TEST_ASSERT_EQUAL(err, NvSuccess, test, suite, "NvRmI2cTransaction FAILED");
    }
    err = NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
            NV_WAIT_INFINITE,
            NVEEPROM_UTIL_I2C_TEST_CLOCK_FREQ_KHZ,
            i2cRbuffer,
            EEPROM_PAGE_SIZE,
            i2cRtransinfo,
            1);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("\n Read failed I2C \n");
        TEST_ASSERT_EQUAL(err, NvSuccess, test, suite, "NvRmI2cTransaction FAILED");
    }
    for (i=0; i < EEPROM_PAGE_SIZE; i++ )
    {
        NvOsDebugPrintf("DATA_2 =  0x%X\n", i2cRbuffer[i]);
    }
    i2cWstack_buffer[0] = EEPROM_TEST_DATA_3_WRITE_OFFSET;
    i2cWtransinfo->NumBytes = 1;
    //This is write transaction before read.
    //We write only one byte, which is address offset to start read.
    err=NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
                    NV_WAIT_INFINITE,
                    NVEEPROM_UTIL_I2C_TEST_CLOCK_FREQ_KHZ,
                    i2cWbuffer,
                    1,
                    i2cWtransinfo,
                    1);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("\n Write failed I2C \n");
        TEST_ASSERT_EQUAL(err, NvSuccess, test, suite, "NvRmI2cTransaction FAILED");
    }
    //This is a read transaction, i2cRbuffer holds read data.
    err = NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
            NV_WAIT_INFINITE,
            NVEEPROM_UTIL_I2C_TEST_CLOCK_FREQ_KHZ,
            i2cRbuffer,
            EEPROM_PAGE_SIZE,
            i2cRtransinfo,
            1);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("\n Read failed I2C \n");
        TEST_ASSERT_EQUAL(err, NvSuccess, test, suite, "NvRmI2cTransaction FAILED");
    }
    for (i=0; i < EEPROM_PAGE_SIZE; i++ )
    {
        NvOsDebugPrintf("DATA_3 =  0x%X\n", i2cRbuffer[i]);
    }
fail:
    if (err != NvSuccess)
        NvOsDebugPrintf(" NvI2cEepromWRTest FAILED");
    if (i2cRtransinfo)
        NvOsFree(i2cRtransinfo);
    if (i2cWtransinfo)
        NvOsFree(i2cWtransinfo);
    return err;
}

// Verifies I2c READ by reading board id of an EEPROM.
NvError NvReadBoardID(void)
{
    NvBool b = NV_FALSE;
    NvError e = NvSuccess;
    NvRmI2cHandle hI2c;
    NvRmDeviceHandle hRmDev;
    NvU8 *i2cRbuffer = NULL;
    NvRmI2cTransactionInfo *i2cRtransinfo = NULL;
    NvU8 i2cRstack_buffer[9];
    NvError err= NvSuccess;
    const char *test = "NvReadBoardID";
    const char *suite = "i2c";
    NvU8 *i2cWbuffer = NULL;
    NvRmI2cTransactionInfo *i2cWtransinfo = NULL;
    NvU8 i2cWstack_buffer[9];

    i2cWstack_buffer[0] = 64;
    i2cRbuffer = i2cRstack_buffer;
    i2cRtransinfo = (NvRmI2cTransactionInfo *)NvOsAlloc(sizeof(NvRmI2cTransactionInfo));
    i2cRtransinfo->Flags = NVRM_I2C_READ;
    i2cRtransinfo->NumBytes = 4;
    i2cRtransinfo->Is10BitAddress = NV_FALSE;
    i2cRtransinfo->Address = EEPROM_I2C_SLAVE_ADDRESS;
    i2cWbuffer = i2cWstack_buffer;
    i2cWtransinfo = (NvRmI2cTransactionInfo *)NvOsAlloc(sizeof(NvRmI2cTransactionInfo));
    i2cWtransinfo->Flags = NVRM_I2C_WRITE;
    i2cWtransinfo->Is10BitAddress = NV_FALSE;
    i2cWtransinfo->Address = EEPROM_I2C_SLAVE_ADDRESS;
    NvOsMemset(i2cRstack_buffer, 0, EEPROM_PAGE_SIZE + 1);
    i2cRbuffer = i2cRstack_buffer;

    NvOsDebugPrintf("\n inside  I2C \n");
    e = NvRmOpen(&hRmDev, 0);
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\n NvRmOpen I2C Failed \n ");
        TEST_ASSERT_EQUAL(e, NvSuccess, test, suite, "NvRmOpen FAILED");
    }
    e = NvRmI2cOpen(hRmDev,NvOdmIoModule_I2c,0, &hI2c);
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\n NvRmI2cOpen I2C Failed\n");
        TEST_ASSERT_EQUAL(e, NvSuccess, test, suite, "NvRmI2cOpen FAILED");
    }

    i2cWstack_buffer[0] = EEPROM_BOARDID_OFFSET;
    i2cWtransinfo->NumBytes = 1;
    err=NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
                NV_WAIT_INFINITE,
                100,
                i2cWbuffer,
                1,
                i2cWtransinfo,
                1);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("\n Write failed I2C \n");
        TEST_ASSERT_EQUAL(err, NvSuccess, test, suite, "NvRmI2cTransaction FAILED");
    }
    err = NvRmI2cTransaction(hI2c ,NvOdmIoModule_I2c,
                NV_WAIT_INFINITE,
                100,
                i2cRbuffer,
                2,
                i2cRtransinfo,
                1);
    if (err != NvSuccess)
    {
        NvOsDebugPrintf("\n Read failed I2C \n");
        TEST_ASSERT_EQUAL(err, NvSuccess, test, suite, "NvRmI2cTransaction FAILED");
    }
    NvOsDebugPrintf("\n EEPROM_BOARDID = 0x%X%X \n", i2cRbuffer[1],i2cRbuffer[0]);
fail:
    if (err != NvSuccess)
        NvOsDebugPrintf(" NvReadBoardIDTest FAILED");
    if (i2cRtransinfo)
        NvOsFree(i2cRtransinfo);
    if (i2cWtransinfo)
        NvOsFree(i2cWtransinfo);
    return err;
}

NvError i2c_init_reg(void)
{
    NvBDKTest_pSuite pSuite  = NULL;
    NvBDKTest_pTest ptest1 = NULL;
    NvBDKTest_pTest ptest2 = NULL;
    NvError e = NvSuccess;
    const char* err_str = 0;

    e = NvBDKTest_AddSuite("i2c", &pSuite);
    if (e != NvSuccess)
    {
        err_str = "Adding suite i2c failed.";
        goto fail;
    }

    e = NvBDKTest_AddTest(pSuite, &ptest1, "NvI2cEepromWRTest",
                        (NvBDKTest_TestFunc)NvI2cEepromWRTest, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvI2cEepromWRTest failed.";
        goto fail;
    }
        e = NvBDKTest_AddTest(pSuite, &ptest2, "NvReadBoardID",
                        (NvBDKTest_TestFunc)NvReadBoardID, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvReadBoardID failed.";
        goto fail;
    }
fail:
    if (err_str)
        NvOsDebugPrintf("%s i2c_init_reg failed : NvError 0x%x\n",
                        err_str , e);
    return e;
}

