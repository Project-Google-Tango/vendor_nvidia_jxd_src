/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/** @file
 * @brief <b>SNOR Protocol Driver</b>
 *
 * @b Description: Contains the implementation of SNOR Protocol Driver API's .
 */

#include "nvddk_snor.h"
#include "nvrm_module.h"
#include "nvassert.h"
#include "snor_priv_driver.h"


#ifndef NV_OAL
#define NV_OAL 0
#endif

// macro to make string from constant
#define MACRO_GET_STR(x, y) \
    NvOsMemcpy(x, #y, (NvOsStrlen(#y) + 1))

typedef struct SnorPartTabRec
{
    // Data for each partition
    // Partition start sector address
    NvU32 StartLSA;
    // number of sectors in partition
    NvU32 NumOfSectors;
    // Remember the partition id for the partition/region
    NvU32 MinorInstance;
    // next entry pointer
    struct SnorPartTabRec * Next;
} SnorPartTab, *SnorPartTabHandle;

// This represents entire Device specific information
typedef struct SnorDevRec
{
    // major instance of device
    NvU32 Instance;
    // number of opened clients
    NvU32 RefCount;
    // Counter to keep track of power down clients
    NvU32 PowerUpCounter;
    // mutex for exclusive operations
    NvOsMutexHandle LockDev;
    // Ddk Snor handle
    NvDdkSnorHandle hDdkSnor;
    // NVTODO : No region table information for SNOR
    // Region Table for the device instance
    // SnorRegionTabHandle hHeadRegionList;

    // NVTODO : No interleave count
    // Interleave specific for device
    // NvU32 InterleaveCount;
    // keep device info for quick access
    NvDdkBlockDevInfo BlkDevInfo;
    // store log2 of pages per block
    NvU32 Log2PagesPerBlock;
    // store log2 of pages per block
    NvU32 Log2BytesPerPage;
    NvU32 NumOfBlocks;
    // store log2 of interleave count
    // NvU32 Log2InterleaveCount;
    // NvDdkSnorDeviceInfo SnorDeviceInfo;
    //SnorDeviceInfo SnorDevInfo;
    // Ddk returned number of devices, chip count
    NvU32 DdkNumDev;
    //Flag to indicate if the flash has been locked before
    NvBool IsFlashLocked;
    // Structure holding the pointers to the Ddk Snor functions
    // NvDdkSnorFunctionsPtrs SnorDdkFuncs;
    // Data for special partition part_id == 0 kept here
    // Block dev driver handle specific to a partition on a device
    // Buffer to hold the spare area data
    SnorPartTabHandle hHeadRegionList;
    NvU32 LogicalAddressStart;
    NvU8* pSpareBuf;
} *SnorDevHandle;

typedef struct SnorBlocDevRec {
    // This is partition specific information
    //
    // IMPORTANT: For code to work must ensure that  NvDdkBlockDev
    // is the first element of this structure
    // Block dev driver handle specific to a partition on a device
    NvDdkBlockDev BlockDev;
    // Flag to indicate the block driver is powered ON
    NvBool IsPowered;
    // Partition Id for partition corresponding to the block dev
    NvU32 MinorInstance;
    // Flag to indicate is write need to be read verified
    NvBool IsReadVerifyWriteEnabled;
    // This is device wide information
    //
    // One SnorDevHandle is shared between multiple partitions on same device
    // This is a pointer and is created once only
    SnorDevHandle hDev;
}SnorBlockDev, *SnorBlockDevHandle;


typedef struct SnorCommonInfoRec
{
    // For each snor controller we are maintaining device state in
    // SnorDevHandle. This data for a particular controller is also
    // visible when using the partition specific Snor Block Device Driver
    // Handle - SnorBlockDevHandle
    SnorDevHandle *SnorDevStateList;
    // List of locks for each device
    NvOsMutexHandle *SnorDevLockList;
    // Count of Snor controllers on SOC, queried from NvRM
    NvU32 MaxInstances;
    // Global RM Handle passed down from Device Manager
    NvRmDeviceHandle hRm;
    // Global boolean flag to indicate if init is done,
    // NV_FALSE indicates not initialized
    // NV_TRUE indicates initialized
    NvBool IsBlockDevInitialized;
    // Init snor block dev driver ref count
    NvU32 InitRefCount;
} SnorCommonInfo ;

// Global info shared by multiple snor controllers,
// and make sure we initialize the instance
SnorCommonInfo s_SnorCommonInfo = { NULL, NULL, 0, NULL, 0, 0 };

// Function used to free the region table created during
// partition creation. Region table is not to be used for normal boot.
static void
SnorFreePartTbl(SnorBlockDevHandle hSnorBlockDev)
{
    // Free each element in partition table
    SnorPartTabHandle hPartEntry;
    SnorPartTabHandle hPartEntryNext;
    hPartEntry = hSnorBlockDev->hDev->hHeadRegionList;
    if (hPartEntry)
    {
        while (hPartEntry->Next)
        {
            hPartEntryNext = hPartEntry->Next;
            NvOsFree(hPartEntry);
            // move to next entry
            hPartEntry = hPartEntryNext;
        }
        // Free the last entry
        NvOsFree(hPartEntry);
        hSnorBlockDev->hDev->hHeadRegionList = NULL;
    }
}

static NvError
SnorFormatDevice(SnorBlockDevHandle hSnorBlockDev)
{
    NvError e = NvSuccess;
    SnorDevHandle hSnor = hSnorBlockDev->hDev;
    NvU32 SnorStatus;
    NV_CHECK_ERROR_CLEANUP(NvDdkSnorFormatDevice(hSnor->hDdkSnor,
                                                 &SnorStatus));
fail:

    return e;
}

static NvError
SnorEraseSectors(SnorBlockDevHandle hSnorBlockDev,
                 NvU32 StartOffset,
                 NvU32 Length)
{
    NvError e = NvSuccess;
    SnorDevHandle hSnor = hSnorBlockDev->hDev;
    NV_CHECK_ERROR_CLEANUP(NvDdkSnorEraseSectors(hSnor->hDdkSnor,
                                                 StartOffset,
                                                 Length));

fail:
    return e;
}

static NvError SnorProtectSectors(SnorBlockDevHandle hSnorBlockDev,
                          NvU32 Offset, NvU32 Size, NvBool IsLastPartition)
{
    NvError e = NvSuccess;
    SnorDevHandle hSnor = hSnorBlockDev->hDev;
    NV_CHECK_ERROR_CLEANUP(NvDdkSnorProtectSectors(hSnor->hDdkSnor,
                                                   Offset, Size, IsLastPartition));
fail :
    return e;
}

#define SNOR_SECTOR_SIZE 0x200
static void
SnorGetDeviceInfo(
    NvDdkBlockDevHandle hBlockDev,
    NvDdkBlockDevInfo* pBlockDevInfo)
{
    volatile NvU32 temp;
    SnorBlockDevHandle hSnorBlkDev = (SnorBlockDevHandle)hBlockDev;
    SnorDevHandle hSnor = hSnorBlkDev->hDev;

    if ((hBlockDev == NULL) || (pBlockDevInfo == NULL))
        return;

    // Sector = Page (2K).
    // NVTODO : Is NumOfBlock redundant ? No, needed by partition manager
    pBlockDevInfo->BytesPerSector = (SNOR_SECTOR_SIZE << 2); // Match with Nand sector size
    pBlockDevInfo->SectorsPerBlock = 16;
    temp = pBlockDevInfo->BytesPerSector/SNOR_SECTOR_SIZE;
    temp *= pBlockDevInfo->SectorsPerBlock;
    // NVTODO : Test
    NvDdkSnorGetNumBlocks(hSnor->hDdkSnor, &hSnorBlkDev->hDev->NumOfBlocks);
    pBlockDevInfo->TotalBlocks = (hSnorBlkDev->hDev->NumOfBlocks)/temp; // Match with Nand
    pBlockDevInfo->DeviceType = NvDdkBlockDevDeviceType_Fixed;
}

// NVTODO : By blocks we mean sectors (or pages)
static NvError
SnorRead(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    void* const pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvSuccess;
    SnorBlockDevHandle hSnorBlkDev = (SnorBlockDevHandle)hBlockDev;
    SnorDevHandle hSnor = hSnorBlkDev->hDev;
    NvU8 *CurrentBuffer = (NvU8 *)pBuffer;
    NvDdkBlockDevInfo *pDeviceInfo;
    NvU32 TransferSize, StartOffset, SnorStatus;

    if ((hBlockDev == NULL) || (pBuffer == NULL) || (NumberOfSectors == 0))
    {
        e = NvError_BadParameter;
        goto fail;
    }

    // NVTODO : No support for Chip Select yet
    pDeviceInfo = &(hSnor->BlkDevInfo);
    TransferSize = (NumberOfSectors * pDeviceInfo->BytesPerSector);
    StartOffset = (SectorNum * pDeviceInfo->BytesPerSector);
    NV_CHECK_ERROR_CLEANUP(NvDdkSnorRead(hSnor->hDdkSnor,
                                         0,
                                         StartOffset,
                                         TransferSize,
                                         CurrentBuffer,
                                         &SnorStatus));

fail:

    return e;
}

static NvError
SnorWrite(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    const void* pBuffer,
    NvU32 NumberOfSectors)
{
    SnorBlockDevHandle hSnorBlkDev = (SnorBlockDevHandle)hBlockDev;
    SnorDevHandle hSnor = hSnorBlkDev->hDev;
    NvU32 Size,Offset ;
    NvU32 SnorStatus = 0;
    NvU8 *CurrentBuffer = (NvU8 *)pBuffer;
    NvDdkBlockDevInfo *pDeviceInfo;
    if ((hBlockDev == NULL) || (pBuffer == NULL) || (NumberOfSectors == 0))
    {
        return NvError_BadParameter;
    }

    pDeviceInfo = &(hSnor->BlkDevInfo);
    // NVTODO : Skipping chip select for now
    // Convert Everything into bytes.
    Size = NumberOfSectors * pDeviceInfo->BytesPerSector;
    Offset = SectorNum * pDeviceInfo->BytesPerSector;

    return NvDdkSnorWrite(hSnor->hDdkSnor,
                          0,
                          Offset,
                          Size,
                          CurrentBuffer,
                          &SnorStatus);
}

// Local function to open a Snor block device driver handle
static NvError
SnorOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    SnorBlockDevHandle* hSnorBlkDev)
{
    NvError e = NvSuccess;
    SnorBlockDevHandle hTempSnorBlockDev;
    SnorDevHandle hTempDev = NULL;

    NV_ASSERT(hSnorBlkDev);

    NvOsMutexLock((*(s_SnorCommonInfo.SnorDevLockList + Instance)));
    hTempSnorBlockDev = NvOsAlloc(sizeof(SnorBlockDev));
    if (!hTempSnorBlockDev)
    {
        NvOsMutexUnlock((*(s_SnorCommonInfo.SnorDevLockList + Instance)));
        return NvError_InsufficientMemory;
    }
    NvOsMemset(hTempSnorBlockDev, 0, sizeof(SnorBlockDev));
    // Allocate the data shared by all partitions on a device
    // This has to be created once.
    if (*(s_SnorCommonInfo.SnorDevStateList + Instance))
    {
        hTempSnorBlockDev->hDev =
            (*(s_SnorCommonInfo.SnorDevStateList + Instance));
    }
    else
    {
         // Allocate once for device
         if ((hTempDev =
            (SnorDevHandle)NvOsAlloc(sizeof(struct SnorDevRec))) == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        // Ensures that SnorDevHandle->hHeadRegionList
        // gets initialized to NULL
        NvOsMemset(hTempDev, 0, sizeof(struct SnorDevRec));
        // Use the lock create earlier
        hTempDev->LockDev = *(s_SnorCommonInfo.SnorDevLockList + Instance);

        // init Snor Ddk handle
        NV_CHECK_ERROR_CLEANUP(NvDdkSnorOpen(s_SnorCommonInfo.hRm,
                                             Instance,
                                             &hTempDev->hDdkSnor));

        // Save the device major instance
        hTempDev->Instance = Instance;

        // set the device instance state entry
        hTempSnorBlockDev->hDev = hTempDev;
        (*(s_SnorCommonInfo.SnorDevStateList + Instance)) =
            hTempSnorBlockDev->hDev;
        // Finished initializing device dependent data

        // save device info, after initialization of device specific data
        SnorGetDeviceInfo(
            &hTempSnorBlockDev->BlockDev, &hTempSnorBlockDev->hDev->BlkDevInfo);
    }
    // Initialize the partition id
    // hTempSnorBlockDev->MinorInstance = MinorInstance;
    hTempSnorBlockDev->IsPowered = NV_TRUE;

    // increment Dev ref count
    hTempSnorBlockDev->hDev->RefCount++;
    hTempSnorBlockDev->hDev->PowerUpCounter++;
    *hSnorBlkDev = hTempSnorBlockDev;

    NvOsMutexUnlock((*(s_SnorCommonInfo.SnorDevLockList + Instance)));

    return e;
fail:
    (*(s_SnorCommonInfo.SnorDevStateList + Instance)) = NULL;
    // Release resoures here.
    if (hTempDev)
    {
        // Close Snor Ddk Handle else get Snor already open
        NvDdkSnorClose(hTempDev->hDdkSnor);
        NvOsFree(hTempDev);
    }
    NvOsFree(hTempSnorBlockDev);
    *hSnorBlkDev = NULL;
    NvOsMutexUnlock((*(s_SnorCommonInfo.SnorDevLockList + Instance)));
    return e;
}

// Function to validate new partition arguments
static NvBool
SnorUtilIsPartitionArgsValid(
    SnorBlockDevHandle hSnorBlockDev,
    NvU32 StartLogicalSector, NvU32 EndLogicalSector)
{
    SnorPartTabHandle hCurrRegion;

    // NVTODO : Initialise the headregionlist
    hCurrRegion = hSnorBlockDev->hDev->hHeadRegionList;
    while (hCurrRegion)
    {
        // Check that there is no overlap in existing partition
        // with new partition
        if (((EndLogicalSector <= hCurrRegion->StartLSA)
            /* logical address of new partition finishes before start of
            compared partition */ ||
            (StartLogicalSector >=
            (hCurrRegion->StartLSA + (hCurrRegion->NumOfSectors)))
            /* logical address new partition starts next to or
            after compared partition */))
        {
            hCurrRegion = hCurrRegion->Next;
            continue;
        }
        else
            return NV_FALSE;
    }

    return NV_TRUE;
}
// Local function used to allocate a partition
// Percentage of replacement sectors sent as part of input argument
// is ignored while creating Snor partition - as there are no bad blocks
// in NOR. Hence, logical address equals physical address and size
// for Snor
static NvError
SnorUtilAllocPart(
    SnorBlockDevHandle hSnorBlockDev,
    NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn,
    NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut)
{
    SnorPartTabHandle hReg;
    SnorPartTabHandle pEntry;
    NvU32 StartLogicalSector;
    NvError e = NvSuccess;
    NvU32 RemainingSectorCount;
    NvDdkBlockDevInfo *pDeviceInfo;
    NvU32 NumOfBlocks;

    // Get device info for block dev handle
    pDeviceInfo = &(hSnorBlockDev->hDev->BlkDevInfo);
    // Set logical start block before bad block check
    StartLogicalSector = hSnorBlockDev->hDev->LogicalAddressStart;

    // NVTODO : No bad block, erase group for SNOR
    // Get start physical block number
    // Partition starts from superblock aligned addresses hence chip 0
    if (pAllocPartIn->AllocationType ==
        NvDdkBlockDevPartitionAllocationType_Absolute)
    {
        StartLogicalSector = pAllocPartIn->StartPhysicalSectorAddress;
        // change the next address as absolute address seen
        hSnorBlockDev->hDev->LogicalAddressStart =
            pAllocPartIn->StartPhysicalSectorAddress;

        // Check if new partition start is located within existing partitions
        if (SnorUtilIsPartitionArgsValid(hSnorBlockDev,
            StartLogicalSector, StartLogicalSector) == NV_FALSE)
        {
            // Partition start is within existing partitions
            return NvError_BlockDriverOverlappedPartition;
        }
    }

    // Check for NumLogicalSectors == -1 and its handling
    if (pAllocPartIn->NumLogicalSectors == 0xFFFFFFFF)
    {
        // This means allocate till end of device
        // Find number of blocks available
        RemainingSectorCount =
            (hSnorBlockDev->hDev->BlkDevInfo.TotalBlocks *
            hSnorBlockDev->hDev->BlkDevInfo.SectorsPerBlock) -
            hSnorBlockDev->hDev->LogicalAddressStart;
        pAllocPartIn->NumLogicalSectors = RemainingSectorCount;
    }
    // Else, only block size alignment is ensured
    // Get number of blocks to transfer, must be after
    // logic to take care of user partition size of -1
    NumOfBlocks = (pAllocPartIn->NumLogicalSectors / pDeviceInfo->SectorsPerBlock);
    // Ceil the size to next block
    NumOfBlocks += (((NumOfBlocks * pDeviceInfo->SectorsPerBlock)
        < pAllocPartIn->NumLogicalSectors)? 1 : 0);
    // validate the new partition args against existing partitions
    if (SnorUtilIsPartitionArgsValid(hSnorBlockDev,
        StartLogicalSector,
        (StartLogicalSector + pAllocPartIn->NumLogicalSectors)) == NV_FALSE)
        return NvError_InvalidAddress;

    // Update the logical start address for next partition
    hSnorBlockDev->hDev->LogicalAddressStart +=
        (NumOfBlocks * pDeviceInfo->SectorsPerBlock);

    // allocate a new region table entry and initialize
    // with the partition details
    pEntry = NvOsAlloc(sizeof(struct SnorPartTabRec));
    if (!pEntry)
        return NvError_InsufficientMemory;
    pEntry->Next = NULL;
    pEntry->MinorInstance = pAllocPartIn->PartitionId;
    pEntry->StartLSA = StartLogicalSector;
    pEntry->NumOfSectors = (NumOfBlocks * pDeviceInfo->SectorsPerBlock);

    // Return start logical sector address to upper layers
    pAllocPartOut->StartLogicalSectorAddress = StartLogicalSector;
    pAllocPartOut->NumLogicalSectors = pEntry->NumOfSectors;
    pAllocPartOut->StartPhysicalSectorAddress = StartLogicalSector;
    pAllocPartOut->NumPhysicalSectors = pEntry->NumOfSectors;

    hReg = hSnorBlockDev->hDev->hHeadRegionList;

    // goto region table link-list end
    if (hReg)
    {
        while (hReg->Next)
        {
            hReg = hReg->Next;
        }
    }
    // Add new region-table-entry to end-of-list
    if (hReg)
    {
        hReg->Next = pEntry;
    }
    else
    {
        hSnorBlockDev->hDev->hHeadRegionList = pEntry;
    }
    return e;
}
// Utility to return name of Ioctl given ioctl opcode
static void
UtilGetIoctlName(NvU32 Opcode, NvU8 *IoctlStr)
{
    switch(Opcode)
    {
        case NvDdkBlockDevIoctlType_ReadPhysicalSector:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ReadPhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_WritePhysicalSector:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_WritePhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors);
            break;
        case NvDdkBlockDevIoctlType_FormatDevice:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_FormatDevice);
            break;
        case NvDdkBlockDevIoctlType_PartitionOperation:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_PartitionOperation);
            break;
        case NvDdkBlockDevIoctlType_AllocatePartition:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_AllocatePartition);
            break;
        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_EraseLogicalSectors);
            break;
        case NvDdkBlockDevIoctlType_ErasePartition:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ErasePartition);
            break;
        case NvDdkBlockDevIoctlType_WriteVerifyModeSelect:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_WriteVerifyModeSelect);
            break;
        case NvDdkBlockDevIoctlType_LockRegion:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_LockRegion);
            break;
        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ErasePhysicalBlock);
            break;
        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus);
            break;
        case NvDdkBlockDevIoctlType_VerifyCriticalPartitions:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_VerifyCriticalPartitions);
            break;
        case NvDdkBlockDevIoctlType_UnprotectAllSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_UnprotectAllSectors);
            break;
        case NvDdkBlockDevIoctlType_ProtectSectors:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ProtectSectors);
            break;
        default:
            // Illegal Ioctl string
            MACRO_GET_STR(IoctlStr, UnknownIoctl);
            break;
    }
}

// Read and Write Physical sector function
static NvError
SnorUtilRdWrSector(SnorBlockDevHandle hSnorBlockDev,
    NvU32 Opcode, NvU32 InputSize, const void * InputArgs)
{
    NvError e = NvSuccess;
    NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *pRdPhysicalIn =
        (NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *)InputArgs;
    NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *pWrPhysicalIn =
        (NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *)InputArgs;

    NV_ASSERT(InputArgs);
    // Data is read from/written on chip block size at max each time
    if (Opcode == NvDdkBlockDevIoctlType_ReadPhysicalSector)
    {
        NV_ASSERT(InputSize == sizeof(NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs));
        // Read call
        e = SnorRead(&hSnorBlockDev->BlockDev,
                     pRdPhysicalIn->SectorNum,
                     pRdPhysicalIn->pBuffer,
                     pRdPhysicalIn->NumberOfSectors);
    }
    else if (Opcode == NvDdkBlockDevIoctlType_WritePhysicalSector)
    {
        NV_ASSERT(InputSize == sizeof(NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs));
        // Write call
        e = SnorWrite(&hSnorBlockDev->BlockDev,
                      pWrPhysicalIn->SectorNum,
                      pWrPhysicalIn->pBuffer,
                      pWrPhysicalIn->NumberOfSectors);
    }
    return e;
}

#define SNOR_BCT_BOUNDARY          (1*1024*1024)  // 1M
#define SNOR_BLOCK_NOT_ERASED      (0x00)
#define SNOR_BLOCK_ERASED          (0xFF)

static  NvError
SnorBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    SnorBlockDevHandle hSnorBlockDev = (SnorBlockDevHandle)hBlockDev;
    SnorDevHandle hSnor = hSnorBlockDev->hDev;
    static NvU8 *s_SnorBlockEraseStatus = NULL;
    NvError e = NvSuccess;


    // Lock before Ioctl
    NvOsMutexLock(hSnorBlockDev->hDev->LockDev);
    // Decode the Ioctl opcode
    switch (Opcode)
    {
        case NvDdkBlockDevIoctlType_UnprotectAllSectors :
            {
                e = NvDdkSnorUnprotectAllSectors(hSnor->hDdkSnor);
                break;
            }

        case NvDdkBlockDevIoctlType_ProtectSectors :
            {
                NvDdkBlockDevInfo devInfo;
                NvU32 Offset, Size;
                NvBool IsLastPartition;
                NvDdkBlockDevIoctl_ProtectSectorsInputArgs
                    *pProtectSectorInfo =
                    (NvDdkBlockDevIoctl_ProtectSectorsInputArgs *)
                    InputArgs;
                NV_ASSERT(InputSize ==
                        sizeof(NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs));
                NV_ASSERT(InputArgs);
                SnorGetDeviceInfo((NvDdkBlockDevHandle)hSnorBlockDev, &devInfo);
                Offset = pProtectSectorInfo->StartPhysicalSectorAddress * devInfo.BytesPerSector;
                Size = pProtectSectorInfo->NumPhysicalSectors * devInfo.BytesPerSector;
                IsLastPartition = pProtectSectorInfo->IsLastPartitionToProtect;
                SnorProtectSectors(hSnorBlockDev, Offset, Size, IsLastPartition);
                break;
            }

        case NvDdkBlockDevIoctlType_LockRegion:
            break;
        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
            {
                // erases physical block
                NvU32 StartDeviceBlock, StartOffset;
                NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs
                    *pErasePhysicalBlock =
                    (NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs *)
                    InputArgs;
                NvU32 EraseSize;
                NvDdkSnorGetEraseSize(hSnor->hDdkSnor, &EraseSize);

                /* If not already allocated, allocate space for the erase status array */
                if (!s_SnorBlockEraseStatus){
                    /* Allocating at run time rather than compile time due
                     * to lack of info at compilation */
                    s_SnorBlockEraseStatus = NvOsAlloc(sizeof(NvU8)*(SNOR_BCT_BOUNDARY/EraseSize));
                    if (!s_SnorBlockEraseStatus)
                        return NvError_InsufficientMemory;
                    NvOsMemset(s_SnorBlockEraseStatus, 0, sizeof(NvU8)*(SNOR_BCT_BOUNDARY/EraseSize));
                }

                NV_ASSERT(InputSize ==
                        sizeof(NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs));
                NV_ASSERT(InputArgs);

                /*
                 * Bootrom assumes block size of 32K, so need to map to corresponding Blocknum for driver.
                 */
                StartDeviceBlock = (pErasePhysicalBlock->BlockNum / (EraseSize >> 15));
                StartOffset =  StartDeviceBlock * EraseSize;
                /* Do not erase blocks beyond SNOR_BCT_BOUNDARY as those
                 * partitions are assumed to be formatted.
                 */
                if (StartDeviceBlock >= (SNOR_BCT_BOUNDARY/EraseSize))
                    break;
                if (SNOR_BLOCK_NOT_ERASED == s_SnorBlockEraseStatus[StartDeviceBlock])
                {
                    // FIXME : Assume length does not span across blocks
                    e = SnorEraseSectors(hSnorBlockDev,StartOffset, EraseSize);
                    s_SnorBlockEraseStatus[StartDeviceBlock] = SNOR_BLOCK_ERASED;
                }
            }
            break;

        case NvDdkBlockDevIoctlType_ReadPhysicalSector:
        case NvDdkBlockDevIoctlType_WritePhysicalSector:
        {
            e = SnorUtilRdWrSector(hSnorBlockDev,
                Opcode, InputSize, InputArgs);
        }
        break;

        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
        {
            // Return the same logical address
            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *pMapPhysicalSectorIn =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *pMapPhysicalSectorOut =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *)OutputArgs;

            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs));
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs));
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);

            pMapPhysicalSectorOut->PhysicalSectorNum =
                            pMapPhysicalSectorIn->LogicalSectorNum;
        }
        break;
        case NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors:
        {
            NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs
                *pLogicalBlockIn =
                (NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs *)
                InputArgs;
            NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs
                *pPhysicalBlockOut =
                (NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs *)
                OutputArgs;

            // No Boot Partition in case of SNOR
            // Hence, a direct assignment will do
            pPhysicalBlockOut->PartitionPhysicalSectorStart =
                pLogicalBlockIn->PartitionLogicalSectorStart;
            pPhysicalBlockOut->PartitionPhysicalSectorStop =
              pLogicalBlockIn->PartitionLogicalSectorStop ;
        }
        break;
        case NvDdkBlockDevIoctlType_AllocatePartition:
        {
            NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn =
                (NvDdkBlockDevIoctl_AllocatePartitionInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut =
                (NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *)OutputArgs;

            // Variable contains the start logical sector number for allocated partition
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_AllocatePartitionInputArgs));
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_AllocatePartitionOutputArgs));
            NV_ASSERT(OutputArgs);

            // Upper layer like partition manager keeps the partition table
            // Check for valid partitions - during creation
            // Check if partition create is done with part id == 0
            // Partition creation happens before NvDdkSnorOpen, so no locking needed
            e = SnorUtilAllocPart(hSnorBlockDev,
                pAllocPartIn, pAllocPartOut);
        }
        break;
        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
        {
            NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *pPhysicalQueryOut =
                (NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *)OutputArgs;
            NvU32 LockStatus = 0;

            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs));
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs));
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);

            // SNOR does not have concept of bad blocks
            pPhysicalQueryOut->IsGoodBlock= NV_TRUE;
            // Check block for lock mode
            // Lock status is 32 bit value - each bit indicating if
            // the WP_GROUP is locked.
            // The bit value for LSB indicates the lock status for the
            // sector number passed to IsBlock Locked function
            pPhysicalQueryOut->IsLockedBlock = LockStatus & 0x1;
        }
        break;

        case NvDdkBlockDevIoctlType_FormatDevice:
        e = SnorFormatDevice(hSnorBlockDev);
        break;

        case NvDdkBlockDevIoctlType_ErasePartition:
            {
                 NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *ip =
                     (NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *)InputArgs;
                 NvU32 StartOffset;
                 NvU32 Length;
                 NV_ASSERT(InputArgs);

                StartOffset = ip->StartLogicalSector * hSnorBlockDev->hDev->BlkDevInfo.BytesPerSector;
                Length = ip->NumberOfLogicalSectors * hSnorBlockDev->hDev->BlkDevInfo.BytesPerSector ;
                // Convert all parameters to bytes
                // In case of NOR, BootROM assumed values and
                // Device Parameters may be different.
                e = SnorEraseSectors(hSnorBlockDev,StartOffset, Length);
            }
        break;

        case NvDdkBlockDevIoctlType_PartitionOperation:
        {
            NvDdkBlockDevIoctl_PartitionOperationInputArgs *pPartOpIn;
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_PartitionOperationInputArgs));
            NV_ASSERT(InputArgs);
            pPartOpIn = (NvDdkBlockDevIoctl_PartitionOperationInputArgs *)
                InputArgs;
            // Check if partition create is done with part id == 0
            if (hSnorBlockDev->MinorInstance)
            {
                // Error as indicates block dev driver handle is
                // not part id == 0
                e = NvError_NotSupported;
                break;
            }
            if (pPartOpIn->Operation == NvDdkBlockDevPartitionOperation_Finish)
            {
                // Seems region table in memory should be updated using lock
                SnorFreePartTbl(hSnorBlockDev);
            }
        }
        break;
    }
    // unlock at end of ioctl
    NvOsMutexUnlock(hSnorBlockDev->hDev->LockDev);

    if (e != NvSuccess)
    {
        NvU8 IoctlName[80];
        // function to return name corresponding to ioctl opcode
        UtilGetIoctlName(Opcode, IoctlName);
        NvOsDebugPrintf("\r\nInst=%d, SNOR ioctl %s failed: error code=0x%x ",
            hSnorBlockDev->hDev->Instance, IoctlName, e);
    }
    return e;
}

static void SnorClose(NvDdkBlockDevHandle hBlockDev)
{
    // NVTODO : To be implemented
}

NvError
NvDdkSnorBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev)
{
    NvError e = NvSuccess;
    SnorBlockDevHandle hSnorBlkDev;

    *phBlockDev = NULL;

    NV_CHECK_ERROR_CLEANUP(SnorOpen(Instance, MinorInstance, &hSnorBlkDev));
    hSnorBlkDev->BlockDev.NvDdkBlockDevClose = &SnorClose;
    hSnorBlkDev->BlockDev.NvDdkBlockDevGetDeviceInfo = &SnorGetDeviceInfo;
    hSnorBlkDev->BlockDev.NvDdkBlockDevReadSector = &SnorRead;
    hSnorBlkDev->BlockDev.NvDdkBlockDevWriteSector = &SnorWrite;
    hSnorBlkDev->BlockDev.NvDdkBlockDevIoctl = &SnorBlockDevIoctl;
    *phBlockDev = &hSnorBlkDev->BlockDev;

fail:
    return e;
}

NvError
NvDdkSnorBlockDevInit(NvRmDeviceHandle hDevice)
{
    NvError e = NvSuccess;
    NvU32 i;

    if (!(s_SnorCommonInfo.IsBlockDevInitialized))
    {
        // Initialize the global state including creating mutex
        // The mutex to lock access

        // Get Snor device instances available
        s_SnorCommonInfo.MaxInstances =
            NvRmModuleGetNumInstances(hDevice, NvRmModuleID_SyncNor);
        // Allocate table of SnorDevHandle, each entry in this table
        // corresponds to a device
        s_SnorCommonInfo.SnorDevStateList = NvOsAlloc(
            sizeof(SnorDevHandle) * s_SnorCommonInfo.MaxInstances);
        if (!s_SnorCommonInfo.SnorDevStateList)
        {
            e = NvError_NotInitialized;
            goto fail;
        }
        NvOsMemset((void *)s_SnorCommonInfo.SnorDevStateList, 0,
            (sizeof(SnorDevHandle) * s_SnorCommonInfo.MaxInstances));

        // Create locks per device
        s_SnorCommonInfo.SnorDevLockList = NvOsAlloc(
            sizeof(NvOsMutexHandle) * s_SnorCommonInfo.MaxInstances);
        if (!s_SnorCommonInfo.SnorDevLockList)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        NvOsMemset((void *)s_SnorCommonInfo.SnorDevLockList, 0,
            (sizeof(SnorDevHandle) * s_SnorCommonInfo.MaxInstances));
        // Allocate the mutex for locking each device
        for (i = 0; i < s_SnorCommonInfo.MaxInstances; i++)
        {
            e = NvOsMutexCreate(&(*(s_SnorCommonInfo.SnorDevLockList + i)));
            if (e != NvSuccess)
            {
                e = NvError_NotInitialized;
                goto fail;
            }
        }

        // Save the RM Handle
        s_SnorCommonInfo.hRm = hDevice;
        s_SnorCommonInfo.IsBlockDevInitialized = NV_TRUE;
    }
    // increment Snor block dev driver init ref count
    s_SnorCommonInfo.InitRefCount++;
fail:
    if (e != NvSuccess)
    {
        if(s_SnorCommonInfo.SnorDevStateList)
            NvOsFree(s_SnorCommonInfo.SnorDevStateList);
        if(s_SnorCommonInfo.SnorDevLockList)
            NvOsFree(s_SnorCommonInfo.SnorDevLockList);
    }
    return e;
}

void
NvDdkSnorBlockDevDeinit(void)
{
    NvU32 i;
    if (s_SnorCommonInfo.IsBlockDevInitialized)
    {
        if (s_SnorCommonInfo.InitRefCount == 1)
        {
            // Dissociate from Rm Handle
            s_SnorCommonInfo.hRm = NULL;
            // Free allocated list of handle pointers
            // free mutex list
            for (i = 0; i < s_SnorCommonInfo.MaxInstances; i++)
            {
                NvOsFree((*(s_SnorCommonInfo.SnorDevStateList + i)));
                (*(s_SnorCommonInfo.SnorDevStateList + i)) = NULL;
                NvOsMutexDestroy((*(s_SnorCommonInfo.SnorDevLockList + i)));
            }
            NvOsFree(s_SnorCommonInfo.SnorDevLockList);
            s_SnorCommonInfo.SnorDevLockList = NULL;
            // free device instances
            NvOsFree(s_SnorCommonInfo.SnorDevStateList);
            s_SnorCommonInfo.SnorDevStateList = NULL;
            // reset the maximum Snor controller count
            s_SnorCommonInfo.MaxInstances = 0;
            s_SnorCommonInfo.IsBlockDevInitialized = NV_FALSE;
        }
        // decrement Snor dev driver init ref count
        s_SnorCommonInfo.InitRefCount--;
    }
}


