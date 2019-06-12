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
 * @brief <b>SATA Block Device Driver </b>
 *
 * @b Description: Contains implementation of SATA Block Device Driver API's
 *         Support for multi-partition APIs is also added.
 */

#include "nvddk_blockdev.h"
#include "nvddk_sata.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvrm_module.h"
#include "nvbct.h"

#define SATA_MAX_READ_WRITE_SECTORS 0xFF
#define SATA_BYTES_PER_SECTOR 512
#define SATA_KERNEL_ERASE_SECTORS 8
#define SATA_SECTOR_PER_BLOCK 1
#define SATA_SECTOR_COUNT_INDEX 120
#define IS_ERASE_SUPPORTED_BYTE 166

// This represents entire Device specific information
typedef struct SataDevRec
{
    // Block dev driver handle specific to a partition on a device
    NvDdkBlockDev BlockDev;

    // major instance of device
    NvU32 Instance;
    // Partition Id for partition corresponding to the block dev
    NvU32 MinorInstance;
    // number of opened clients
    NvU32 RefCount;
    // mutex for exclusive operations
    NvOsMutexHandle SataLock;
    // handle for sata ddk driver
    NvU32 hDdkSata;

    /* Total number of sectors on the SATA */
    NvU32 TotalSectors;

    NvU32 BytesPerSector;

    NvU32 SectorsPerBlock;

    /* Number of bytes for each block in the SATA */
    NvU32 PartitionAddr;

    /* Indicates if the driver is opened or not */
    NvBool IsOpen;

    NvBool IsEraseSupported;

   // Data for special partition part_id == 0 kept here
    // Block dev driver handle specific to a partition on a device
} SataDev;

// Local type defined to store data common
// to multiple Sata Controller instances
typedef struct SataCommonInfoRec
{
    // Global RM Handle passed down from Device Manager
    NvRmDeviceHandle hRmSata;
    // Count of Satacontrollers on SOC, queried from NvRM
    NvU32 MaxInstances;
    // Global boolean flag to indicate if init is done,
    // NV_FALSE indicates not initialized
    // NV_TRUE indicates initialized
    NvBool IsBlockDevInitialized;
    // Init Sata block dev driver ref count
    NvU32 InitRefCount;
    // For each Sata controller we are maintaining device state in
    // SataDevHandle.
    // Handle - SataDevHandle
     SataDev *SataDevStateList;
    // List of locks for each device
    // NvOsMutexHandle *SataDevLockList;
} SataCommonInfo;

// Global info shared by multiple Sata controllers,
// and make sure we initialize the instance
static SataCommonInfo s_SataCommonInfo;

/*********************************************************/
/* Start Sata Block device driver Interface definitions */
/*********************************************************/

// Local function to return the block device driver information
static void
SataBlockDevGetDeviceInfo(
    NvDdkBlockDevHandle hBlockDev,
    NvDdkBlockDevInfo* pBlockDevInfo)
{
    SataDev* pSataDev = (SataDev* ) hBlockDev;

    NV_ASSERT(hBlockDev);
    NV_ASSERT(pBlockDevInfo);

    NvOsMutexLock(pSataDev->SataLock);

    pBlockDevInfo->BytesPerSector = pSataDev->BytesPerSector;
    pBlockDevInfo->SectorsPerBlock = pSataDev->SectorsPerBlock;
    pBlockDevInfo->TotalBlocks =
        pSataDev->TotalSectors / pSataDev->SectorsPerBlock;
    pBlockDevInfo->TotalSectors =   pSataDev->TotalSectors;
    pBlockDevInfo->DeviceType = NvDdkBlockDevDeviceType_Fixed;
    SATA_DEBUG_PRINT(("SataBlockDevGetDeviceInfo::TotalSectors = %d \
        SectorsPerBlock = %d::BytesPerSector=%d\n", pSataDev->TotalSectors, \
        pSataDev->SectorsPerBlock, pSataDev->BytesPerSector));

    NvOsMutexUnlock(pSataDev->SataLock);
}

// Local function to open a Sata block device driver handle
static NvError
SataBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    SataDev** hSataBlkDev)
{
    NvError e = NvSuccess;
    SataDev *pSataDev;
    NvU8 pBuffer[512];

    pSataDev = &s_SataCommonInfo.SataDevStateList[Instance];

    NvOsMutexLock(pSataDev->SataLock);

    SATA_DEBUG_PRINT(("SataBlockDevOpen\n"));
    if (pSataDev->IsOpen)
    {
        SATA_DEBUG_PRINT(("SataBlockDevOpen::alreadyopened\n"));
        pSataDev->RefCount++;
        *hSataBlkDev = pSataDev;
        NvOsMutexUnlock(pSataDev->SataLock);
        return e;
    }

    NV_CHECK_ERROR_CLEANUP(NvDdkSataInit());

    // Save the device major instance
    Instance = 0;
    MinorInstance = 0;
    pSataDev->Instance = Instance;
    pSataDev->MinorInstance = MinorInstance;
    pSataDev->PartitionAddr = 0;
    pSataDev->BytesPerSector = SATA_BYTES_PER_SECTOR;
    pSataDev->SectorsPerBlock = SATA_SECTOR_PER_BLOCK;

    NV_CHECK_ERROR_CLEANUP(NvDdkSataIdentifyDevice(pBuffer));

    pSataDev->TotalSectors = (pBuffer[SATA_SECTOR_COUNT_INDEX] |
        (pBuffer[SATA_SECTOR_COUNT_INDEX + 1] << 8) |
        (pBuffer[SATA_SECTOR_COUNT_INDEX + 2] << 16) |
        (pBuffer[SATA_SECTOR_COUNT_INDEX + 3] << 24));

    if ((pBuffer[IS_ERASE_SUPPORTED_BYTE] & 0x04))
        pSataDev->IsEraseSupported = NV_TRUE;
    else
        pSataDev->IsEraseSupported = NV_FALSE;

    pSataDev->IsOpen = NV_TRUE;
    pSataDev->RefCount++;

    *hSataBlkDev = pSataDev;

    // Release resoures here.
    NvOsMutexUnlock(pSataDev->SataLock);
fail:
    if (e != NvSuccess)
    {
        SATA_ERROR_PRINT(("SataBlockDevOpen Failed:: Error = %d\n", e));
    }
    return e;
}


// Local function to close a Sata block device driver handle
static void SataBlockDevClose(NvDdkBlockDevHandle hBlockDev)
{
    SataDev* pSataDev = (SataDev* ) hBlockDev;

    NV_ASSERT(hBlockDev);

    NvOsMutexLock(pSataDev->SataLock);

    SATA_DEBUG_PRINT(("SataBlockDevClose::IsOpen=%d::InitRefCount=%d\n", \
        pSataDev->IsOpen,pSataDev->RefCount));
    if (pSataDev->RefCount == 1)
    {
        pSataDev->PartitionAddr = 0;
        pSataDev->TotalSectors = 0;
        pSataDev->BytesPerSector = 0;
        pSataDev->IsOpen = NV_FALSE;
        pSataDev->RefCount = 0;
    }
    else
    {
        NV_ASSERT(pSataDev->IsOpen);
        pSataDev->RefCount--;
    }
    // Release resoures here.
    NvOsMutexUnlock(pSataDev->SataLock);
}

// Local Sata function to read requested sectors from a logical sector.
static NvError
SataBlockDevReadSector(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    void* const pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvSuccess;
    SataDev* pSataDev = (SataDev* ) hBlockDev;
    NvU32 SectorsToRead, SectorCount = NumberOfSectors;
    NvU8 *BufferPtr = (NvU8 *)pBuffer;
    NvU32 SectorNumber = SectorNum;

    SATA_DEBUG_PRINT(("SataBlockDevReadSector::SecNum=%d::Buffer = 0x%x \
        NO.Sectors = %d\n",SectorNum, pBuffer, NumberOfSectors));

    NV_ASSERT(hBlockDev);
    NV_ASSERT(pBuffer);
    NV_ASSERT(NumberOfSectors);

    /* Check if requested read sectors exceed the total sectors on the device */
    NV_ASSERT(NumberOfSectors <= pSataDev->TotalSectors);

    if ((SectorNum + NumberOfSectors) > pSataDev->TotalSectors)
    {
        /* Requested read sectors crossing the device boundary */
        NV_ASSERT(!"Trying to read more than SATA device size\n");

        return NvError_BadParameter;
    }

    NvOsMutexLock(pSataDev->SataLock);

    // Read physical sectors.

    while (SectorCount)
    {
        if (SectorCount > SATA_MAX_READ_WRITE_SECTORS)
            SectorsToRead = SATA_MAX_READ_WRITE_SECTORS;
        else
            SectorsToRead = SectorCount;

        NV_CHECK_ERROR_CLEANUP(
            NvDdkSataRead(SectorNumber, BufferPtr, SectorsToRead));

        SectorCount -= SectorsToRead;
        SectorNumber += SectorsToRead;
        BufferPtr += (SectorsToRead * pSataDev->BytesPerSector);
    }

fail:
    if (e != NvSuccess)
    {
        SATA_ERROR_PRINT(("SataBlockDevReadSector Failed::SecNum=%d \
            Buffer = 0x%x::NO.Sectors = %d::return = %d\n", \
            SectorNum, pBuffer, NumberOfSectors, e));
    }
    NvOsMutexUnlock(pSataDev->SataLock);
    return e;
}

// Local Sata function to write requested sectors at a logical sector.
static NvError
SataBlockDevWriteSector(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 SectorNum,
    const void* pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvSuccess;
    SataDev* pSataDev = (SataDev* ) hBlockDev;
    NvU32 SectorsToWrite, SectorCount = NumberOfSectors;
    NvU8 *BufferPtr = (NvU8 *)pBuffer;
    NvU32 SectorNumber = SectorNum;

    SATA_DEBUG_PRINT(("SataBlockDevWriteSector::SecNum=%d::Buffer = 0x%x:: \
        NO.Sectors = %d\n", SectorNum, pBuffer, NumberOfSectors));

    NV_ASSERT(hBlockDev);
    NV_ASSERT(pBuffer);
    NV_ASSERT(NumberOfSectors);

    /* Check if requested read sectors exceed the total sectors on the device */
    NV_ASSERT(NumberOfSectors <= pSataDev->TotalSectors);

    if ((SectorNum + NumberOfSectors) > pSataDev->TotalSectors)
    {
        /* Requested read sectors crossing the device boundary */
        NV_ASSERT(!"Trying to write more than SATA device size\n");
        return NvError_BadParameter;
    }

    NvOsMutexLock(pSataDev->SataLock);

    while (SectorCount)
    {
        if (SectorCount > SATA_MAX_READ_WRITE_SECTORS)
            SectorsToWrite = SATA_MAX_READ_WRITE_SECTORS;
        else
            SectorsToWrite = SectorCount;

        // Write  physical sectos.
        NV_CHECK_ERROR_CLEANUP(
            NvDdkSataWrite(SectorNumber, BufferPtr, SectorsToWrite));

        SectorCount -= SectorsToWrite;
        SectorNumber += SectorsToWrite;
        BufferPtr += (SectorsToWrite * pSataDev->BytesPerSector);
    }
fail:
    if (e != NvSuccess)
    {
        SATA_ERROR_PRINT(("SataBlockDevWriteSector Failed::SecNum=%d:: \
            Buffer = 0x%x::NO.Sectors = %d::return = %d\n", \
            SectorNum, pBuffer, NumberOfSectors, e));
    }
    NvOsMutexUnlock(pSataDev->SataLock);
    return e;
}


// Local Sata function to power up the block driver
static void SataBlockDevPowerUp(NvDdkBlockDevHandle hBlockDev)
{
    // Not supported
}

// Local Sata function to shut down the block driver
static void SataBlockDevPowerDown(NvDdkBlockDevHandle hBlockDev)
{
    // Not supported
}

// Local Sata function to flush driver cache if any
static void SataBlockDevFlushCache(NvDdkBlockDevHandle hBlockDev)
{
    // Unsupported function forSATA for now.
}


// Local function to handle removable media
static void
SataRegisterHotplugSemaphore(
    NvDdkBlockDevHandle hBlockDev,
    NvOsSemaphoreHandle hHotPlugSema)
{
    // Hot plugging is not supported.
}


// Local Sata function to perform the requested ioctl operation.
static  NvError
SataBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    NvError e = NvSuccess;
     SataDev* pSataDev = (SataDev* ) hBlockDev;
    NvU32 BlockCount = 0;
    NvU32 RemainingSectorCount = 0;

    NvOsMutexLock(pSataDev->SataLock);

    SATA_DEBUG_PRINT(("SataBlockDevIoctl::OpCode = %d\n", OpCode));

    // Decode the IOCTL opcode
    switch (Opcode)
    {
        case NvDdkBlockDevIoctlType_ReadPhysicalSector:
        {
            NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *pRdPhysicalIn =
                    (NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *)InputArgs;

            NV_CHECK_ERROR_CLEANUP(NvDdkSataRead(pRdPhysicalIn->SectorNum,
                pRdPhysicalIn->pBuffer, pRdPhysicalIn->NumberOfSectors));
        }
        break;
        case NvDdkBlockDevIoctlType_WritePhysicalSector:
        {
            NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *pWrPhysicalIn =
                (NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *)InputArgs;
            // Read physical sector.
            NV_CHECK_ERROR_CLEANUP(NvDdkSataWrite(pWrPhysicalIn->SectorNum,
                pWrPhysicalIn->pBuffer, pWrPhysicalIn->NumberOfSectors));
        }
        break;

        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
        {
            NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *pPhysicalQueryOut =
                    (NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *)OutputArgs;

            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs));
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs));
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);
            // Sata does not have concept of bad blocks
            pPhysicalQueryOut->IsGoodBlock= NV_TRUE;
        }
            break;

        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
        {
            // TODO:  Erases sector
        }
        break;

        case NvDdkBlockDevIoctlType_LockRegion:
        {
            // TODO:  Need to check.
        }
        break;

        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
        {
            // Return the same logical address
            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *pMapPhysicalSectorIn =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *pMapPhysicalSectorOut =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *)OutputArgs;
            NV_ASSERT(InputSize == sizeof(
                NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs));
            NV_ASSERT(OutputSize == sizeof(
                NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs));
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);
             // Check if partition create is done with part id == 0
            if (pSataDev->MinorInstance)
            {
                // Error as indicates block dev driver handle is
                // not part id == 0
                e = NvError_BlockDriverNoPartition;
                break;
            }
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
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);
            NV_ASSERT(InputSize == sizeof(
                NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs));
            NV_ASSERT(OutputSize == sizeof(
                NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs));
            pPhysicalBlockOut->PartitionPhysicalSectorStart =
                    pLogicalBlockIn->PartitionLogicalSectorStart;
            pPhysicalBlockOut->PartitionPhysicalSectorStop =
                    pLogicalBlockIn->PartitionLogicalSectorStop;
        }
        break;

        case NvDdkBlockDevIoctlType_FormatDevice:
            if (pSataDev->IsEraseSupported)
            {
                SATA_DEBUG_PRINT(("SataBlockDevIoctl:: FormatDevice:: \
                    IsEraseSupported = %d\n", pSataDev->IsEraseSupported));
                NV_CHECK_ERROR_CLEANUP(
                    NvDdkSataErase(0, pSataDev->TotalSectors));
            }
            break;

        case NvDdkBlockDevIoctlType_PartitionOperation:
        {
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_PartitionOperationInputArgs));
            NV_ASSERT(InputArgs);
            SATA_DEBUG_PRINT(("SataBlockDevIoctl::PartitionOperation:: \
                pPartOpIn::Operation=%d\n",
                ((NvDdkBlockDevIoctl_PartitionOperationInputArgs *)InputArgs)->Operation));
            // Check if partition create is done with part id == 0
            if (pSataDev->MinorInstance)
            {
                // Error as indicates block dev driver handle is
                // not part id == 0
                 SATA_ERROR_PRINT(("SataBlockDevIoctl::PartitionOperation: \
                     Minor Instance = %d\n",pSataDev->MinorInstance));
                //e = NvError_BlockDriverIllegalPartId;
                break;
            }
        }
        break;

        case NvDdkBlockDevIoctlType_AllocatePartition:
        {
            NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn =
                (NvDdkBlockDevIoctl_AllocatePartitionInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut =
                (NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *)OutputArgs;

            SATA_DEBUG_PRINT(("SataBlockDevIoctl::AllocatePartition: \
                pAllocPartIn::PartitionId=%d::StartPhysicalSectorAddress=%d:: \
                NumLogicalSectors=%d\n",pAllocPartIn->PartitionId, \
                pAllocPartIn->StartPhysicalSectorAddress, \
                pAllocPartIn->NumLogicalSectors));

            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_AllocatePartitionInputArgs));
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_AllocatePartitionOutputArgs));

            /* Check if partition create is done with part id equal to 0 */
            if (pSataDev->MinorInstance)
            {
                e = NvError_NotSupported;
                break;
            }

            if (pAllocPartIn->AllocationType ==
                    NvDdkBlockDevPartitionAllocationType_Absolute)
            {
                pAllocPartOut->StartLogicalSectorAddress =
                    pAllocPartIn->StartPhysicalSectorAddress;
                pAllocPartOut->NumLogicalSectors =
                    pAllocPartIn->NumLogicalSectors;
            }
            else if (pAllocPartIn->AllocationType ==
                        NvDdkBlockDevPartitionAllocationType_Relative)
            {

               // check for partition attribute 0x800 to substract the give
               // size from available size
                if(pAllocPartIn->PartitionAttribute &
                   NVDDK_BLOCKDEV_PART_MGMT_POLICY_REMAIN_SIZE)
                {
                    RemainingSectorCount = pSataDev->TotalSectors -
                        pSataDev->PartitionAddr;
                    pAllocPartIn->NumLogicalSectors = RemainingSectorCount -
                        pAllocPartIn->NumLogicalSectors;
                }
                // Check for NumLogicalSectors == -1 and its handling
                if (pAllocPartIn->NumLogicalSectors == 0xFFFFFFFF)
                {
                    pAllocPartIn->NumLogicalSectors = pSataDev->TotalSectors -
                        pSataDev->PartitionAddr;
                }

               pAllocPartOut->StartLogicalSectorAddress =
                   pSataDev->PartitionAddr;

                /* Address needs to be block aligned */
                BlockCount =
                    pAllocPartIn->NumLogicalSectors/pSataDev->SectorsPerBlock;
                if (BlockCount * pSataDev->SectorsPerBlock !=
                        pAllocPartIn->NumLogicalSectors)
                    BlockCount++;

                pAllocPartOut->NumLogicalSectors =
                        BlockCount * pSataDev->SectorsPerBlock;
                pSataDev->PartitionAddr += pAllocPartOut->NumLogicalSectors;

                pAllocPartOut->StartPhysicalSectorAddress =
                        pAllocPartOut->StartLogicalSectorAddress;
                pAllocPartOut->NumPhysicalSectors =
                        pAllocPartOut->NumLogicalSectors;
            }
            else
            {
                NV_ASSERT(!"SATA Block driver, Invalid allocation type");
            }
            if (pSataDev->IsEraseSupported == NV_FALSE)
            {
                NvU8 *Buf;
                Buf = NvOsAlloc(SATA_KERNEL_ERASE_SECTORS *
                    SATA_BYTES_PER_SECTOR);
                NvOsMemset(Buf, 0, SATA_KERNEL_ERASE_SECTORS *
                    SATA_BYTES_PER_SECTOR);
                NvDdkSataWrite(
                    pAllocPartOut->StartPhysicalSectorAddress, Buf,
                    SATA_KERNEL_ERASE_SECTORS);
            }
            SATA_DEBUG_PRINT(("SataBlockDevIoctl::AllocatePartition: \
                pAllocPartOut::PartitionId=%d::StartPhysicalSectorAddress=%d:: \
                NumLogicalSectors=%d\n",pAllocPartOut->PartitionId, \
                pAllocPartOut->StartPhysicalSectorAddress, \
                pAllocPartOut->NumLogicalSectors));

        }
        break;

        case NvDdkBlockDevIoctlType_ErasePartition:
        {
           // TODO: Erase partition.
            break;
        }
        case NvDdkBlockDevIoctlType_VerifyCriticalPartitions:
        {
           // TODO: Verify critical partitions.
           e = NvSuccess;
        }
            break;

        default:
            e = NvError_BlockDriverIllegalIoctl;
            break;
    }

fail:
    // unlock at end of ioctl
    NvOsMutexUnlock(pSataDev->SataLock);
    return e;
}


// Open block device driver handle API
NvError
NvDdkSataBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev)
{
    NvError e;
    SataDev *hSata = NULL;
    NV_ASSERT(phBlockDev);
    NV_ASSERT(s_SataCommonInfo.IsBlockDevInitialized);

    // open the block driver handle
    NV_CHECK_ERROR_CLEANUP(SataBlockDevOpen(Instance, MinorInstance, &hSata));
    SATA_DEBUG_PRINT(("NvDdkSataBlockDevOpen::hSata= 0x%x, Instance = %d \
        MinorInstance = %d\n", hSata, Instance, MinorInstance));
    // Initialize all the function pointers for the block driver handle
    hSata->BlockDev.NvDdkBlockDevClose = &SataBlockDevClose;
    hSata->BlockDev.NvDdkBlockDevGetDeviceInfo = &SataBlockDevGetDeviceInfo;
    hSata->BlockDev.NvDdkBlockDevReadSector = &SataBlockDevReadSector;
    hSata->BlockDev.NvDdkBlockDevWriteSector = &SataBlockDevWriteSector;
    hSata->BlockDev.NvDdkBlockDevPowerUp = &SataBlockDevPowerUp;
    hSata->BlockDev.NvDdkBlockDevPowerDown = &SataBlockDevPowerDown;
    hSata->BlockDev.NvDdkBlockDevRegisterHotplugSemaphore =
                                                &SataRegisterHotplugSemaphore;
    hSata->BlockDev.NvDdkBlockDevIoctl = &SataBlockDevIoctl;
    hSata->BlockDev.NvDdkBlockDevFlushCache = &SataBlockDevFlushCache;
    *phBlockDev = &(hSata->BlockDev);

fail:
    if (e != NvSuccess)
        SATA_ERROR_PRINT(("NvDdkSataBlockDevOpen::Error = 0x%x\n", e));
    return e;
}

// Initialize block device driver API
NvError NvDdkSataBlockDevInit(NvRmDeviceHandle hDevice)
{
    NvError e = NvSuccess;
    NvU32 i;

    if (!(s_SataCommonInfo.IsBlockDevInitialized))
    {
        // Get Sata device instances available
        s_SataCommonInfo.MaxInstances = NvRmModuleGetNumInstances(
            hDevice, NvRmModuleID_Sata);
        // Allocate table of SataDevHandle, each entry in this table
        // corresponds to a device
        s_SataCommonInfo.SataDevStateList = NvOsAlloc(
            sizeof(SataDev) * s_SataCommonInfo.MaxInstances);
        if (!s_SataCommonInfo.SataDevStateList)
            return NvError_InsufficientMemory;

        NvOsMemset((void *)s_SataCommonInfo.SataDevStateList, 0,
            (sizeof(SataDev) * s_SataCommonInfo.MaxInstances));

        // Allocate the mutex for locking each device
        for (i = 0; i < s_SataCommonInfo.MaxInstances; i++)
        {
             e = NvOsMutexCreate(
                     &s_SataCommonInfo.SataDevStateList[i].SataLock);
            if (e != NvSuccess)
                return e;
        }

        // Save the RM Handle
        s_SataCommonInfo.hRmSata = hDevice;
        s_SataCommonInfo.IsBlockDevInitialized = NV_TRUE;
    }
    // increment Sata block dev driver init ref count
    s_SataCommonInfo.InitRefCount++;
    return e;
}

// Deinitialize block device driver API
void NvDdkSataBlockDevDeinit(void)
{
    NvU32 i;
    if (s_SataCommonInfo.IsBlockDevInitialized)
    {
        if (s_SataCommonInfo.InitRefCount == 1)
        {
            /* Free the locks */
            for (i = 0; i < s_SataCommonInfo.MaxInstances; i++)
            {
                NvOsMutexDestroy(s_SataCommonInfo.SataDevStateList[i].SataLock);
            }
            NvOsFree(s_SataCommonInfo.SataDevStateList);

           s_SataCommonInfo.SataDevStateList = NULL;
            s_SataCommonInfo.hRmSata = NULL;
            // reset the maximum Sata controller count
            s_SataCommonInfo.MaxInstances = 0;
            s_SataCommonInfo.IsBlockDevInitialized = NV_FALSE;
        }
        // decrement Sata dev driver init ref count
        s_SataCommonInfo.InitRefCount--;
    }
}

