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
 * @brief <b>NVIDIA Driver Development Kit: DDK SD Block Driver Testcase API</b>
 *
 * @b Description: Contains the testcases for the nvddk SD block driver.
 */

#include "nvddk_sdio.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"
#include "testsources.h"
#include "nvddk_sd_block_driver_test.h"


NvDdkBlockDevHandle hSdbdk = NULL;
NvDdkBlockDevInfo DeviceInfo;

#define SECTOR_INC_OFFSET_FULL_RW_TEST 59656
#define SECTOR_INC_MULTIPLIER 1

NvError SdInit(NvU32 Instance)
{
    NvError ErrStatus = NvSuccess;
    NvBool b = NV_TRUE;
    char *test = "SdInit";
    char *suite = "sd";
    ErrStatus = NvDdkBlockDevMgrInit();
    TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "NvDdkSDBlockDevinit FAILED");
    //Instance = 2 for enterprise and 0 for cardhu, Minor Instance = 0
    ErrStatus = NvDdkBlockDevMgrDeviceOpen(NvDdkBlockDevMgrDeviceId_SDMMC,
            Instance,0,&hSdbdk);
    TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "NvDdkSDBlockDevOpen FAILED");
    NvOsDebugPrintf("\n Sd INIT\n");
    if (hSdbdk->NvDdkBlockDevGetDeviceInfo)
    {
        hSdbdk->NvDdkBlockDevGetDeviceInfo(hSdbdk, &DeviceInfo);
    }
    NvOsDebugPrintf("Device Info:\n \
            Total Blocks: %d\n \
            DeviceType: %s\n \
            BytesPerSector: %d\n \
            SectorsPerBlock:%d\n", DeviceInfo.TotalBlocks,
            ((DeviceInfo.DeviceType == 1)?"Fixed":"Removable"),
            DeviceInfo.BytesPerSector,
            DeviceInfo.SectorsPerBlock);
fail:
    return ErrStatus;
}

NvError NvSdWriteReadVerifyTest(NvBDKTest_TestPrivData data)
{
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
        NvU32 Instance;
        NvBool b = NV_TRUE;
        char *test = "SdInit";
        char *suite = "sd";
        Instance = data.Instance;
        ErrStatus = SdInit(Instance);
        TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "SdInit FAILED");
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
                TEST_ASSERT_PTR_NULL(!pWriteBuffer, test, suite, "NvOsAlloc FAILED");
                // Allocate memory for read buffer
                pReadBuffer = NvOsAlloc(DataSize);
                TEST_ASSERT_PTR_NULL(!pReadBuffer, test, suite, "NvOsAlloc FAILED");
                // Fill the write buffer with the timestamp
                for(i=0;i<DataSize;i++)
                {
                    pWriteBuffer[i] = NvOsGetTimeUS();
                    pReadBuffer[i] = 0;
                }
                // Write the buffer to SD
                NvOsDebugPrintf("Writting the data ...\n");
                StartTime = NvOsGetTimeUS();
                ErrStatus = hSdbdk->NvDdkBlockDevWriteSector(hSdbdk,
                                                             StartSector,
                                                             pWriteBuffer,
                                          (DataSize/NV_BDK_SD_SECTOR_SIZE));
                EndTime = NvOsGetTimeUS();
                TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "Write FAILED");
                WriteTime = (NvU32)(EndTime - StartTime);
                // Read into the buffer from SD
                NvOsDebugPrintf("Reading the data ...\n");
                StartTime = NvOsGetTimeUS();
                ErrStatus = hSdbdk->NvDdkBlockDevReadSector(hSdbdk,
                                                            StartSector,
                                                            pReadBuffer,
                                           (DataSize / NV_BDK_SD_SECTOR_SIZE));
                EndTime = NvOsGetTimeUS();
                TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "REad FAILED");
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
        SdClose();
        return ErrStatus;
}

NvError NvSdVerifyWriteReadFull(NvBDKTest_TestPrivData data)
{
    NvError ErrStatus = NvSuccess;
    NvU64 DataSize = 0;
    NvU8 *pReadBuffer = NULL;
    NvU8 *pWriteBuffer = NULL;
    NvU32 StartSector = 0, NumSectors = 0;
    NvU32 i, j=0;
    NvU32 Instance;
    NvBool b = NV_TRUE;
    char *test = "NvSdVerifyWriteReadFull";
    char *suite = "sd";
    NumSectors = DeviceInfo.TotalBlocks * DeviceInfo.SectorsPerBlock;
    StartSector = 0;
    Instance = data.Instance;
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("Starting Write Read VerifyTest for entire card\n");
    NvOsDebugPrintf("==================================================\n");
    DataSize = NV_BDK_SD_SECTOR_SIZE;
    ErrStatus = SdInit(Instance);
    TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "SdInit FAILED");
    while (StartSector < NumSectors)
    {
        j = 0;
        // Allocate memory for write buffer
        pWriteBuffer = NvOsAlloc(DataSize);
        TEST_ASSERT_PTR_NULL(!pWriteBuffer, test, suite, "NvOsAlloc FAILED");
        // Allocate memory for read buffer
        pReadBuffer = NvOsAlloc(DataSize);
        TEST_ASSERT_PTR_NULL(!pReadBuffer, test, suite, "NvOsAlloc FAILED");
        // Fill the write buffer with known bytes
        for (i=0;i<DataSize;i++)
        {
            pWriteBuffer[i] = NvOsGetTimeUS();
            pReadBuffer[i] = 0;
        }

        // Write the buffer to SD
        NvOsDebugPrintf("Writting the data ...\n");
        ErrStatus = hSdbdk->NvDdkBlockDevWriteSector(hSdbdk,
                                                     StartSector,
                                                     pWriteBuffer,
                                                     1);
        TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "Write FAILED");


        // Read into the buffer from SD
        NvOsDebugPrintf("Reading the data ...\n");
        ErrStatus = hSdbdk->NvDdkBlockDevReadSector(hSdbdk,
                                                    StartSector,
                                                    pReadBuffer,
                                                    1);
        TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "Read FAILED");
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
        StartSector = StartSector + SECTOR_INC_OFFSET_FULL_RW_TEST * SECTOR_INC_MULTIPLIER;
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
    SdClose();
    return ErrStatus;
}

NvError NvSdVerifyWriteReadSpeed(NvBDKTest_TestPrivData data)
{
    NvError ErrStatus = NvSuccess;
    NvS32 DataSizes[] = {4, 8, 16, 32, 64, -1};
    NvU64 DataSize = 0;
    NvU32 SizeIndex = 0;
    NvU8 *pReadBuffer = NULL;
    NvU8 *pWriteBuffer = NULL;
    NvU32 StartSector = 0, NumSectors = 0, Instance;
    NvU64 StartTime=0, EndTime=0;
    NvU32 ReadTime=0, WriteTime=0;
    NvU32 Speed;
    NvU32 i, j=0;
    NvBool b = NV_TRUE;
    char *test = "NvSdVerifyWriteReadSpeed";
    char *suite = "sd";
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("         Starting Write Read Speed Test\n");
    NvOsDebugPrintf("==================================================\n");
    SizeIndex = 0;
    Instance = data.Instance;
    ErrStatus = SdInit(Instance);
    NumSectors = DeviceInfo.TotalBlocks * DeviceInfo.SectorsPerBlock;
    TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "SdInit FAILED");;
    while (DataSizes[SizeIndex] != -1)
    {
        NvOsDebugPrintf("=================================================\n");
        NvOsDebugPrintf("Testing with %d MB data\n", DataSizes[SizeIndex]);
        NvOsDebugPrintf("=================================================\n");

        DataSize = DataSizes[SizeIndex] * NV_BYTES_IN_MB;
        StartSector = NvOsGetTimeUS() % (NumSectors - 1000);
        j = 0;

        // Allocate memory for write buffer
        pWriteBuffer = NvOsAlloc(DataSize);
        TEST_ASSERT_PTR_NULL(!pWriteBuffer, test, suite, "NvOsAlloc FAILED");

        // Allocate memory for read buffer
        pReadBuffer = NvOsAlloc(DataSize);
        TEST_ASSERT_PTR_NULL(!pReadBuffer, test, suite, "NvOsAlloc FAILED");

        // Fill the write buffer with known bytes
        for (i = 0; i < DataSize; i++)
        {
            pWriteBuffer[i] = i;
            pReadBuffer[i] = 0;
        }

        // Write the buffer to SD
        NvOsDebugPrintf("Writting the data ...\n");
        StartTime = NvOsGetTimeUS();
        ErrStatus = hSdbdk->NvDdkBlockDevWriteSector(hSdbdk,
                                                     StartSector,
                                                     pWriteBuffer,
                                    (DataSize / NV_BDK_SD_SECTOR_SIZE));
        EndTime = NvOsGetTimeUS();
        TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "Write FAILED");
        WriteTime = (NvU32)(EndTime - StartTime);

        // Read into the buffer from SD
         NvOsDebugPrintf("Reading the data ...\n");
        StartTime = NvOsGetTimeUS();
        ErrStatus = hSdbdk->NvDdkBlockDevReadSector(hSdbdk,
                                                    StartSector,
                                                    pReadBuffer,
                                    (DataSize / NV_BDK_SD_SECTOR_SIZE));
        TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "Read FAILED");
        EndTime = NvOsGetTimeUS();
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
    SdClose();
    return ErrStatus;
}

NvError NvSdVerifyWriteReadSpeed_BootArea(NvBDKTest_TestPrivData data)
{
    NvError ErrStatus = NvSuccess;
    NvS32 DataSizes[] = {4, 8, 16, 64, 128, 512, -1};
    NvU64 DataSize = 0;
    NvU32 SizeIndex = 0;
    NvU8 *pReadBuffer = NULL;
    NvU8 *pWriteBuffer = NULL;
    NvU32 StartSector = 0, Instance;
    NvU64 StartTime=0, EndTime=0;
    NvU32 ReadTime=0, WriteTime=0;
    NvU32 Speed;
    NvU32 i, j=0;
    NvU32 passes =0, fails =0;
    NvBool b = NV_TRUE;
    char *test = "NvSdVerifyWriteReadSpeed_BootArea";
    char *suite = "sd";
    Instance = data.Instance;
    ErrStatus = SdInit(Instance);
    TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "SdInit FAILED");
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("Starting Write Read Boot Area VerifyTest\n");
    NvOsDebugPrintf("==================================================\n");
    while (DataSizes[SizeIndex] != -1)
    {
        DataSize = DataSizes[SizeIndex] * NV_BYTES_IN_KB;
        NvOsDebugPrintf("=============================================\n");
        NvOsDebugPrintf("Testing with %d KB data\n", DataSizes[SizeIndex]);
        NvOsDebugPrintf("=============================================\n");

       // Allocate memory for write buffer
        pWriteBuffer = NvOsAlloc(DataSize);
        TEST_ASSERT_PTR_NULL(!pWriteBuffer, test, suite, "NvOsAlloc FAILED");
        // Allocate memory for read buffer
        pReadBuffer = NvOsAlloc(DataSize);
        TEST_ASSERT_PTR_NULL(!pReadBuffer, test, suite, "NvOsAlloc FAILED");

        // Fill the write buffer with the timestamp
        for (i=0;i<DataSize;i++)
        {
            pWriteBuffer[i] = NvOsGetTimeUS();
            pReadBuffer[i] = 0;
        }

        // Write the buffer to SD
        StartTime = NvOsGetTimeUS();
        ErrStatus = hSdbdk->NvDdkBlockDevWriteSector(hSdbdk,
                                                     StartSector,
                                                     pWriteBuffer,
                                  (DataSize/NV_BDK_SD_SECTOR_SIZE));
        EndTime = NvOsGetTimeUS();
        TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "Write FAILED");


        WriteTime = (NvU32)(EndTime - StartTime);

        // Read into the buffer from SD
        StartTime = NvOsGetTimeUS();
        ErrStatus = hSdbdk->NvDdkBlockDevReadSector(hSdbdk,
                                                    StartSector,
                                                    pReadBuffer,
                                   (DataSize/NV_BDK_SD_SECTOR_SIZE));
        EndTime = NvOsGetTimeUS();
        TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "Read FAILED");
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

    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("Write Read Boot Area VerifyTest Completed\n");
    NvOsDebugPrintf("Total Tests = %d\nTotal Passes = %d\nTotal Fails = %d\n",
                           passes+fails, passes, fails);
    NvOsDebugPrintf("==================================================\n");
fail:
    if (pWriteBuffer)
        NvOsFree(pWriteBuffer);
    if (pReadBuffer)
        NvOsFree(pReadBuffer);
    SdClose();
    return ErrStatus;
}

NvError NvSdVerifyErase(NvBDKTest_TestPrivData data)
{
    NvError ErrStatus = NvSuccess;
    NvS32 DataSizes[] = {16, 32, 64, 128, 192, -1};
    NvU64 DataSize = 0;
    NvU32 SizeIndex = 0;
    NvU8 *pReadBuffer = NULL;
    NvU8 *pWriteBuffer = NULL;
    NvU32 StartSector = 0, NumSectors = 0, Instance;
    NvU64 StartTime=0, EndTime=0;
    NvU32 i, j=0;
    NvBool b = NV_TRUE;
    char *test = "NvSdVerifyErase";
    char *suite = "sd";
    NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs ErasePartArgs;
    Instance = data.Instance;
    ErrStatus = SdInit(Instance);
    TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,suite, "SdInit FAILED");
    NumSectors = DeviceInfo.TotalBlocks * DeviceInfo.SectorsPerBlock;
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("         Starting Erase Test\n");
    NvOsDebugPrintf("==================================================\n");
    SizeIndex = 0;
    while (DataSizes[SizeIndex] != -1)
    {
        NvOsDebugPrintf("=================================================\n");
        NvOsDebugPrintf("Testing with %d MB data\n", DataSizes[SizeIndex]);
        NvOsDebugPrintf("=================================================\n");

        DataSize = DataSizes[SizeIndex] * NV_BYTES_IN_MB;
        StartSector = NvOsGetTimeUS() % (NumSectors - 1000);
        j = 0;

        // Allocate memory for write buffer
        pWriteBuffer = NvOsAlloc(DataSize);
        TEST_ASSERT_PTR_NULL(!pWriteBuffer, test, suite, "NvOsAlloc FAILED");
        // Allocate memory for read buffer
        pReadBuffer = NvOsAlloc(DataSize);
        TEST_ASSERT_PTR_NULL(!pReadBuffer, test, suite, "NvOsAlloc FAILED");
        // Fill the write buffer with timestamps
        for (i = 0; i < DataSize; i++)
        {
            pWriteBuffer[i] = i;
            pReadBuffer[i] = 0;
        }

        // Write the buffer to SD
        NvOsDebugPrintf("Writing the data ...\n");
        ErrStatus = hSdbdk->NvDdkBlockDevWriteSector(hSdbdk,
                                                     StartSector,
                                                     pWriteBuffer,
                                    (DataSize / NV_BDK_SD_SECTOR_SIZE));
        TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,
            suite, "NvDdkBlockDevWriteSector FAILED");

        // Erase the data
        NvOsDebugPrintf("Erasing the data ...\n");
        ErasePartArgs.StartLogicalSector = (NvU32)StartSector;
        ErasePartArgs.NumberOfLogicalSectors =(NvU32)DataSize /
                                               NV_BDK_SD_SECTOR_SIZE;
        ErasePartArgs.IsPTpartition = NV_FALSE;
        ErasePartArgs.IsSecureErase = NV_FALSE;
        ErasePartArgs.IsTrimErase = NV_FALSE;

        StartTime = NvOsGetTimeUS();
        ErrStatus = hSdbdk->NvDdkBlockDevIoctl(hSdbdk,
                                    NvDdkBlockDevIoctlType_EraseLogicalSectors,
                    sizeof(NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs), 0,
                                    &ErasePartArgs, NULL);
        EndTime = NvOsGetTimeUS();
        TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,
            suite, "NvDdkBlockDevIoctl FAILED");


        // Read into the buffer from SD
        NvOsDebugPrintf("\nReading the data ...\n");
        ErrStatus = hSdbdk->NvDdkBlockDevReadSector(hSdbdk,
                                                    StartSector,
                                                    pReadBuffer,
                                    (DataSize / NV_BDK_SD_SECTOR_SIZE));
        TEST_ASSERT_EQUAL(ErrStatus, NvSuccess, test ,
            suite, "NvDdkBlockDevReadSector FAILED");


        // Verify the write and read data
        for (i = 0; i < DataSize; i++)
        {
            if (( pReadBuffer[i] != 0x00) && (pReadBuffer[i] != 0xFF) )
            {
                NvOsDebugPrintf("**ERROR: Erase Failed @ offset %d\n", i);
                j = 1;
                break;
            }
        }
        if (j == 0)
        {
            NvOsDebugPrintf("Verify Erase PASSED\n");
            NvOsDebugPrintf("Erase Time = %d us\n", (EndTime - StartTime));
        }
        else
        {
            NvOsDebugPrintf("Verify Erase FAILED\n");
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
    NvOsDebugPrintf("==================================================\n");
    NvOsDebugPrintf("            Erase Test Completed\n");
    NvOsDebugPrintf("==================================================\n");
fail:
    if (pWriteBuffer)
        NvOsFree(pWriteBuffer);
    if (pReadBuffer)
        NvOsFree(pReadBuffer);
    SdClose();
    return ErrStatus;
}

NvError sd_init_reg(void)
{
    NvBDKTest_pSuite pSuite  = NULL;
    NvBDKTest_pTest ptest1 = NULL;
    NvBDKTest_pTest ptest2 = NULL;
    NvBDKTest_pTest ptest3 = NULL;
    NvBDKTest_pTest ptest4 = NULL;
    NvBDKTest_pTest ptest5 = NULL;
    NvError e = NvSuccess;
    const char* err_str = 0;
    NvOsDebugPrintf("\n sd_init_reg done \n");
    e = NvBDKTest_AddSuite("sd", &pSuite);
    if (e != NvSuccess)
    {
        err_str = "Adding suite sd failed.";
        goto fail;
    }

    e = NvBDKTest_AddTest(pSuite, &ptest1, "NvSdWriteReadVerifyTest",
                        (NvBDKTest_TestFunc)NvSdWriteReadVerifyTest, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvSdWriteReadVerifyTest failed.";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest2, "NvSdVerifyWriteReadFull",
                        (NvBDKTest_TestFunc)NvSdVerifyWriteReadFull, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvSdVerifyWriteReadFull failed.";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest3, "NvSdVerifyWriteReadSpeed",
                        (NvBDKTest_TestFunc)NvSdVerifyWriteReadSpeed, "perf",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvSdVerifyWriteReadSpeed failed.";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest4, "NvSdVerifyWriteReadSpeed_BootArea",
                        (NvBDKTest_TestFunc)NvSdVerifyWriteReadSpeed_BootArea, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvSdVerifyWriteReadSpeed_BootArea failed.";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest5, "NvSdVerifyErase",
                        (NvBDKTest_TestFunc)NvSdVerifyErase, "basic",
                        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding test NvSdVerifyErase failed.";
        goto fail;
    }
fail:
    if(err_str)
        NvOsDebugPrintf("%s sd_init_reg failed : NvError 0x%x\n",
                        err_str , e);
    return e;
}

void SdClose(void)
{
    if (hSdbdk)
        hSdbdk->NvDdkBlockDevClose(hSdbdk);
    else
        NvOsDebugPrintf("Sd Not Initialised\n");
    NvDdkBlockDevMgrDeinit();
}

