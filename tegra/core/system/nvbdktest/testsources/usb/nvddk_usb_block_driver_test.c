/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: DDK USB Block Driver Testcase API</b>
 *
 * @b Description: Contains the testcases for the nvddk USB HOST block driver.
 */


#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"
#include "testsources.h"
#include "nvddk_usb_block_driver_test.h"

NvDdkBlockDevHandle hDdkUsb = NULL;
NvDdkBlockDevInfo DeviceInfo;

NvError UsbTestInit(NvU32 Instance)
{
    NvError ErrStatus = NvSuccess;
    ErrStatus = NvDdkBlockDevMgrInit();
    if (ErrStatus)
    {
        NvOsDebugPrintf("NvDdkUsbBlockDevinit FAILED\n");
        return ErrStatus;
    }
    //Instance = 2 for enterprise and 0 for cardhu, Minor Instance = 0
    ErrStatus = NvDdkBlockDevMgrDeviceOpen(NvDdkBlockDevMgrDeviceId_Usb3, Instance, 0,
                                        &hDdkUsb);
    if (ErrStatus)
    {
        NvOsDebugPrintf("NvDdkUsbBlockDevOpen FAILED\n");
        return ErrStatus;
    }
    if (hDdkUsb->NvDdkBlockDevGetDeviceInfo)
    {
        hDdkUsb->NvDdkBlockDevGetDeviceInfo(hDdkUsb, &DeviceInfo);
    }
    NvOsDebugPrintf("Device Info:\n \
            Total Blocks: %d\n \
            DeviceType: %s\n \
            BytesPerSector: %d\n \
            SectorsPerBlock:%d\n", DeviceInfo.TotalBlocks,
            ((DeviceInfo.DeviceType == 1)?"Fixed":"Removable"),
            DeviceInfo.BytesPerSector,
            DeviceInfo.SectorsPerBlock);
    return ErrStatus;
}


NvError NvUsbVerifyWriteRead(NvBDKTest_TestPrivData data)
{
    NvBool b = NV_FALSE;
    NvError ErrStatus = NvSuccess;
    NvS32 DataSizes[] = {4, 64, 128, 512, 1024, 2048, -1};
    NvU64 DataSize = 0;
    NvU32 SizeIndex = 0;
    NvU8 *pReadBuffer = NULL;
    NvU8 *pWriteBuffer = NULL;
    NvU32 StartSector = 0, NumSectors = 0;
    NvU64 StartTime=0, EndTime=0;
    NvU32 ReadTime=0, WriteTime=0;
    NvU32 Speed;
    NvU32 i, j=0, k=0;
    NvU32 passes =0, fails =0;
    char *test = "NvUsbVerifyWriteRead";
    char *suite = "usb";
    NvU32 Instance;
    Instance = data.Instance;
    ErrStatus = UsbTestInit(Instance);
    TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "UsbTestInit FAILED");
    NumSectors = DeviceInfo.TotalBlocks * DeviceInfo.SectorsPerBlock;
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("          Starting Write Read VerifyTest\n");
    NvOsDebugPrintf("==================================================\n");
    for (k = 0; k < 20; k++ )
    {
        StartSector = NvOsGetTimeUS() % (NumSectors - 1000);
        NvOsDebugPrintf("Testing with start sector = %d\n", StartSector);
        SizeIndex = 0;
        while (DataSizes[SizeIndex] != -1)
        {
            DataSize = DataSizes[SizeIndex] * NV_BYTES_IN_KB;
            NvOsDebugPrintf("=============================================\n");
            NvOsDebugPrintf("Testing with %d KB data\n", DataSizes[SizeIndex]);
            NvOsDebugPrintf("=============================================\n");
           // Allocate memory for write buffer
            pWriteBuffer = NvOsAlloc(DataSize);
            if (!pWriteBuffer)
            {
                NvOsDebugPrintf("NvOsAlloc FAILED\n");
                TEST_ASSERT_EQUAL(!pWriteBuffer, NvSuccess,
                    test, suite, "NvOsAlloc FAILED");
            }
            // Allocate memory for read buffer
            pReadBuffer = NvOsAlloc(DataSize);
            if (!pReadBuffer)
            {
                NvOsDebugPrintf("NvOsAlloc FAILED\n");
                TEST_ASSERT_EQUAL(!pReadBuffer, NvSuccess,
                    test, suite, "NvOsAlloc FAILED");
            }
            // Fill the write buffer with the timestamp
            for (i=0;i<DataSize;i++)
            {
                pWriteBuffer[i] = NvOsGetTimeUS();
                pReadBuffer[i] = 0;
            }
            // Write the buffer to USB
            StartTime = NvOsGetTimeUS();
            ErrStatus = hDdkUsb->NvDdkBlockDevWriteSector(hDdkUsb,
                                                         StartSector,
                                                         pWriteBuffer,
                                      (DataSize/NVBDKTEST_USB_SECTOR_SIZE));
            EndTime = NvOsGetTimeUS();
            if (ErrStatus)
            {
                NvOsDebugPrintf("Write FAILED\n");
                TEST_ASSERT_EQUAL(ErrStatus, NvSuccess,
                    test, suite, "NvDdkBlockDevWriteSector FAILED");
            }
            WriteTime = (NvU32)(EndTime - StartTime);
            // Read into the buffer from USB
            NvOsDebugPrintf("Reading the data ...x%08X\n", hDdkUsb);
            StartTime = NvOsGetTimeUS();
            ErrStatus = hDdkUsb->NvDdkBlockDevReadSector(hDdkUsb,
                                                        StartSector,
                                                        pReadBuffer,
                                       (DataSize / NVBDKTEST_USB_SECTOR_SIZE));
            EndTime = NvOsGetTimeUS();
            if (ErrStatus)
            {
                NvOsDebugPrintf("Read FAILED\n");
                TEST_ASSERT_EQUAL(ErrStatus, NvSuccess,
                    test, suite, "NvDdkBlockDevReadSector FAILED");
            }
            ReadTime = (NvU32)(EndTime - StartTime);
            // Verify the write and read data
            NvOsDebugPrintf("Write-Read-Verify ...\n");
            j = 0;
            for (i = 0; i < DataSize; i++)
            {
                if (pWriteBuffer[i] != pReadBuffer[i])
                {
                    NvOsDebugPrintf("**ERROR: Compare Failed @ offset %d\n", i);
                    j = 1;
                    break;
                }
            }
            if (j == 0)
            {
                NvOsDebugPrintf("Write-Read-Verify PASSED\n");
                // Calulcate and print the write time and speed
                Speed = ((DataSize /NV_BYTES_IN_KB) * NV_MICROSEC_IN_SEC) /
                         WriteTime;
                NvOsDebugPrintf("Write Time = %d us\n", WriteTime);
                NvOsDebugPrintf("Write Speed = %d KB/s\n", Speed);

                // Calculate and print the write time and speed
                Speed = ((DataSize /NV_BYTES_IN_KB) * NV_MICROSEC_IN_SEC) /
                         ReadTime;
                NvOsDebugPrintf("Read Time  = %d us\n", ReadTime);
                NvOsDebugPrintf("Read Speed = %d KB/s\n", Speed);
                passes++;
            }
            else
            {
                NvOsDebugPrintf("Write-Read-Verify FAILED\n");
                fails++;
                TEST_ASSERT_EQUAL(!NvSuccess, NvSuccess,
                    test, suite, " Write-Read-Verify FAILED ");
            }
            // Free the allocated buffer
            if (pWriteBuffer)
                NvOsFree(pWriteBuffer);
            if (pReadBuffer)
                NvOsFree(pReadBuffer);
            pReadBuffer = NULL;
            pWriteBuffer = NULL;
            // Change index to verify with next data size
            SizeIndex++;
        }
    }
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("Write Read VerifyTest Completed\n");
    NvOsDebugPrintf("Total Tests = %d\nTotal Passes = %d\nTotal Fails = %d\n",
                           passes+fails, passes, fails);
    NvOsDebugPrintf("==================================================\n");
fail:
    if (pWriteBuffer)
        NvOsFree(pWriteBuffer);
    if (pReadBuffer)
        NvOsFree(pReadBuffer);
    UsbTestClose();
    return ErrStatus;
}


NvError NvUsbVerifyWriteReadFull(NvBDKTest_TestPrivData data)
{
    NvError ErrStatus = NvSuccess;
    NvU64 DataSize = 0;
    NvU8 *pReadBuffer = NULL;
    NvU8 *pWriteBuffer = NULL;
    NvU32 StartSector = 0, NumSectors = 0;
    NvU32 i, j=0;
    NvBool b = NV_FALSE;
    char *test = "NvUsbVerifyWriteReadFull";
    char *suite = "usb";
    NvU32 Instance;
    Instance = data.Instance;
    NumSectors = DeviceInfo.TotalBlocks * DeviceInfo.SectorsPerBlock;
    StartSector = 0;
    ErrStatus = UsbTestInit(Instance);
    TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "UsbTestInit FAILED");
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("Starting Write Read VerifyTest for entire card\n");
    NvOsDebugPrintf("==================================================\n");
    DataSize = NVBDKTEST_USB_SECTOR_SIZE;
    while (StartSector < NumSectors)
    {
        j = 0;
        // Allocate memory for write buffer
        pWriteBuffer = NvOsAlloc(DataSize);
        if (!pWriteBuffer)
        {
            NvOsDebugPrintf("NvOsAlloc FAILED\n");
            TEST_ASSERT_EQUAL(!pWriteBuffer, NvSuccess,
                test, suite, "NvOsAlloc FAILED");
        }

        // Allocate memory for read buffer
        pReadBuffer = NvOsAlloc(DataSize);
        if (!pReadBuffer)
        {
            TEST_ASSERT_EQUAL(!pReadBuffer, NvSuccess,
                test, suite, "NvOsAlloc FAILED");
        }

        // Fill the write buffer with known bytes
        for (i=0;i<DataSize;i++)
        {
            pWriteBuffer[i] = NvOsGetTimeUS();
            pReadBuffer[i] = 0;
        }

        // Write the buffer to Usb
        NvOsDebugPrintf("Writting the data ...\n");
        ErrStatus = hDdkUsb->NvDdkBlockDevWriteSector(hDdkUsb,
                                                     StartSector,
                                                     pWriteBuffer,
                                                     1);
        if (ErrStatus)
        {
            NvOsDebugPrintf("Write FAILED\n");
            TEST_ASSERT_EQUAL(ErrStatus, NvSuccess,
                test, suite, "NvDdkBlockDevWriteSector FAILED");
        }
        // Read into the buffer from Usb
        NvOsDebugPrintf("Reading the data ...\n");
        ErrStatus = hDdkUsb->NvDdkBlockDevReadSector(hDdkUsb,
                                                    StartSector,
                                                    pReadBuffer,
                                                    1);
        if (ErrStatus)
        {
            NvOsDebugPrintf("Read FAILED\n");
            TEST_ASSERT_EQUAL(ErrStatus, NvSuccess,
                test, suite, "NvDdkBlockDevReadSector FAILED");
        }
        // Verify the write and read data
        for (i=0;i<DataSize;i++)
        {
            if(pWriteBuffer[i] != pReadBuffer[i])
            {
                NvOsDebugPrintf("**ERROR: Compare Failed @ offset %d\n", i);
                j = 1;
                break;
            }
        }
        if (j == 0)
            NvOsDebugPrintf("Write-Read-Verify for Sector %d PASSED\n", StartSector);
        else
            NvOsDebugPrintf("Write-Read-Verify for Sector %d FAILED\n", StartSector);
       // Free the allocated buffer
        if (pWriteBuffer)
            NvOsFree(pWriteBuffer);
        if (pReadBuffer)
            NvOsFree(pReadBuffer);
        pReadBuffer = NULL;
        pWriteBuffer = NULL;
        // Change index to verify with next data size
        StartSector++;
    }
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("Write Read VerifyTest for entire card Completed\n");
    NvOsDebugPrintf("==================================================\n");
fail:
    if (pWriteBuffer)
        NvOsFree(pWriteBuffer);
    if (pReadBuffer)
        NvOsFree(pReadBuffer);
    UsbTestClose();
    return ErrStatus;
}


NvError NvUsbVerifyWriteReadSpeed(NvBDKTest_TestPrivData data)
{
    NvBool b = NV_FALSE;
    NvError ErrStatus = NvSuccess;
    NvS32 DataSizes[] = {4, 8, 16, 32, 64, -1};
    NvU64 DataSize = 0;
    NvU32 SizeIndex = 0;
    NvU8 *pReadBuffer = NULL;
    NvU8 *pWriteBuffer = NULL;
    NvU32 StartSector = 0, NumSectors = 0;
    NvU64 StartTime=0, EndTime=0;
    NvU32 ReadTime=0, WriteTime=0;
    NvU32 Speed;
    NvU32 i, j=0;
    char *test = "NvUsbVerifyWriteReadSpeed";
    char *suite = "usb";
    NvU32 Instance;
    Instance = data.Instance;
    ErrStatus = UsbTestInit(Instance);
    TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "UsbTestInit FAILED");
    NumSectors = DeviceInfo.TotalBlocks * DeviceInfo.SectorsPerBlock;
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("         Starting Write Read Speed Test\n");
    NvOsDebugPrintf("==================================================\n");
    SizeIndex = 0;

    while(DataSizes[SizeIndex] != -1)
    {
        NvOsDebugPrintf("=================================================\n");
        NvOsDebugPrintf("Testing with %d MB data\n", DataSizes[SizeIndex]);
        NvOsDebugPrintf("=================================================\n");

        DataSize = DataSizes[SizeIndex] * NV_BYTES_IN_MB;
        StartSector = NvOsGetTimeUS() % (NumSectors - 1000);
        j = 0;

        // Allocate memory for write buffer
        pWriteBuffer = NvOsAlloc(DataSize);
        if (!pWriteBuffer)
        {
            NvOsDebugPrintf("NvOsAlloc FAILED\n");
            TEST_ASSERT_EQUAL(!pWriteBuffer, NvSuccess, test, suite, "NvOsAlloc FAILED");
        }

        // Allocate memory for read buffer
        pReadBuffer = NvOsAlloc(DataSize);
        if (!pReadBuffer)
        {
            NvOsDebugPrintf("NvOsAlloc FAILED\n");
            TEST_ASSERT_EQUAL(!pReadBuffer, NvSuccess, test, suite, "NvOsAlloc FAILED");
        }

        // Fill the write buffer with known bytes
        for (i = 0; i < DataSize; i++)
        {
            pWriteBuffer[i] = i;
            pReadBuffer[i] = 0;
        }

        // Write the buffer to Usb
        NvOsDebugPrintf("Writting the data ...\n");
        StartTime = NvOsGetTimeUS();
        ErrStatus = hDdkUsb->NvDdkBlockDevWriteSector(hDdkUsb,
                                                     StartSector,
                                                     pWriteBuffer,
                                    (DataSize / NVBDKTEST_USB_SECTOR_SIZE));
        EndTime = NvOsGetTimeUS();
        if (ErrStatus)
        {
            NvOsDebugPrintf("Write FAILED\n");
            TEST_ASSERT_EQUAL(ErrStatus, NvSuccess,
                test, suite, " NvDdkBlockDevWriteSector FAILED ");
        }
        WriteTime = (NvU32)(EndTime - StartTime);

        // Read into the buffer from Usb
         NvOsDebugPrintf("Reading the data ...\n");
        StartTime = NvOsGetTimeUS();
        ErrStatus = hDdkUsb->NvDdkBlockDevReadSector(hDdkUsb,
                                                    StartSector,
                                                    pReadBuffer,
                                    (DataSize / NVBDKTEST_USB_SECTOR_SIZE));
        EndTime = NvOsGetTimeUS();

        if (ErrStatus)
        {
            NvOsDebugPrintf("Read FAILED\n");
            TEST_ASSERT_EQUAL(ErrStatus, NvSuccess,
                test, suite, " NvDdkBlockDevReadSector FAILED ");
        }
        ReadTime = (NvU32)(EndTime - StartTime);

#if NV_WRITE_READ_VERIFY_NEEDED_FOR_SPEED_TEST
        // Verify the write and read data
        NvOsDebugPrintf("Write-Read-Verify ...\n");
        j = 0;
        for (i = 0; i < DataSize; i++)
        {
            if (pWriteBuffer[i] != pReadBuffer[i])
            {
                NvOsDebugPrintf("**ERROR: Compare Failed @ offset %d\n", i);
                j = 1;
                break;
            }
        }
        if (j == 0)
        {
            NvOsDebugPrintf("Write-Read-Verify PASSED\n");
#endif
            // Calulcate and print the write time and speed
            Speed = ((DataSize /NV_BYTES_IN_KB) * NV_MICROSEC_IN_SEC) /
                     WriteTime;
            NvOsDebugPrintf("Write Time = %d us\n", WriteTime);
            NvOsDebugPrintf("Write Speed = %d KB/s\n", Speed);

            // Calculate and print the write time and speed
            Speed = ((DataSize /NV_BYTES_IN_KB) * NV_MICROSEC_IN_SEC) /
                     ReadTime;
            NvOsDebugPrintf("Read Time  = %d us\n", ReadTime);
            NvOsDebugPrintf("Read Speed = %d KB/s\n", Speed);
#if NV_WRITE_READ_VERIFY_NEEDED_FOR_SPEED_TEST
        }
        else
        {
            NvOsDebugPrintf("Write-Read-Verify FAILED\n");
            TEST_ASSERT_EQUAL(!NvSuccess, NvSuccess,
                test, suite, " Write-Read-Verify FAILED");
       }
#endif
       // Free the allocated buffer
       if (pWriteBuffer)
            NvOsFree(pWriteBuffer);
       if (pReadBuffer)
            NvOsFree(pReadBuffer);
        pReadBuffer = NULL;
        pWriteBuffer = NULL;

        // Change index to verify with next data size
        SizeIndex++;
    }
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("        Write Read Speed Test Completed\n");
    NvOsDebugPrintf("==================================================\n");
fail:
    if (pWriteBuffer)
        NvOsFree(pWriteBuffer);
    if (pReadBuffer)
        NvOsFree(pReadBuffer);
    UsbTestClose();
    return ErrStatus;
}

void UsbTestClose(void)
{
    if (hDdkUsb)
        hDdkUsb->NvDdkBlockDevClose(hDdkUsb);
    NvDdkBlockDevMgrDeinit();
}

NvError Usb_init_reg(void)
{
    NvBDKTest_pSuite pSuite  = NULL;
    NvBDKTest_pTest ptest1 = NULL;
    NvBDKTest_pTest ptest2 = NULL;
    NvError e = NvSuccess;
    const char* err_str = 0;
    NvOsDebugPrintf("\nUsb_init_reg\n");
    e = NvBDKTest_AddSuite("usb", &pSuite);
    if (e != NvSuccess)
    {
        err_str = "Adding suite usb failed.";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest1, "NvUsbVerifyWriteRead",
                        (NvBDKTest_TestFunc)NvUsbVerifyWriteRead, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvUsbVerifyWriteRead failed.";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest2, "NvUsbVerifyWriteReadFull",
                        (NvBDKTest_TestFunc)NvUsbVerifyWriteReadFull, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvUsbVerifyWriteReadFull failed.";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest2, "NvUsbVerifyWriteReadSpeed",
                        (NvBDKTest_TestFunc)NvUsbVerifyWriteReadSpeed, "perf",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvUsbVerifyWriteReadSpeed failed.";
        goto fail;
    }
fail:
    if (err_str)
        NvOsDebugPrintf("%s Usb_init_reg failed : NvError 0x%x\n",
                        err_str , e);
    return e;
}

