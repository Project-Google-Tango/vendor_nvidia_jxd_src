/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_blockdevmgr.h"
#include "nvapputil.h"
#include "nvassert.h"
#include "nvstormgr.h"
#include "nvddk_blockdev.h"
#include "nvflash_commands.h"


#define NVDDK_BLOCKDEV_PART_MGMT_POLICY_REMAIN_SIZE 0x800

typedef struct HostBlockDevDataRec
{
    //bct partition info
    NvU32 StartBctSector;
    NvU32 EndBctSector;
    NvU8* pStrMgTmpBuff;
    NvU32 ValidTmpBuffBytes;
    char PartName[NVPARTMGR_PARTITION_NAME_LENGTH];
} HostBlockDevData;


typedef struct HostBlockDevRec
{
    // This is partition specific information
    //
    // IMPORTANT: For code to work must ensure that  NvDdkBlockDev
    // is the first element of this structure
    // Block dev driver handle specific to a partition on a device
    NvDdkBlockDev BlockDev;
    NvU32 PartitionNumber;
    NvU32 PartitionSectorIndex;

    NvU32 BytesPerSector;
    NvU32 SectorsPerBlock;
    NvU32 TotalBlocks;

    NvOsFileHandle hFile;

} HostBlockDev, *HostBlockDevHandle;

HostBlockDevData g_HostBlockDevData;

void HostBlkDevClose(NvDdkBlockDevHandle hBlockDev)
{
    HostBlockDevHandle hHostBlkDev = (HostBlockDevHandle) hBlockDev;
    if(hHostBlkDev->hFile)
        NvOsFclose(hHostBlkDev->hFile);
    NvOsFree(hBlockDev);
}

static void
HostBlkDevGetDeviceInfo(
    NvDdkBlockDevHandle hBlockDev,
    NvDdkBlockDevInfo* pBlockDevInfo)
{
    HostBlockDevHandle hostBlkDevHandle = (HostBlockDevHandle) hBlockDev;
    pBlockDevInfo->BytesPerSector = hostBlkDevHandle->BytesPerSector;
    pBlockDevInfo->DeviceType = 1;
    pBlockDevInfo->SectorsPerBlock = hostBlkDevHandle->SectorsPerBlock;
    pBlockDevInfo->TotalBlocks = hostBlkDevHandle->TotalBlocks;
}

static NvError
HostBlkDevRead(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 BlockNum,
    void* const pBuffer,
    NvU32 NumberOfBlocks)
{
    NvError e = NvSuccess;
    HostBlockDevHandle hHostBlkDev = (HostBlockDevHandle) hBlockDev;
    NvU32 Bytes;
    if (!hHostBlkDev->PartitionNumber)
    {
            //raw access
            if(!hHostBlkDev->hFile)
                NV_CHECK_ERROR_CLEANUP(
                    NvOsFopen("PT", NVOS_OPEN_READ, &hHostBlkDev->hFile)
                );
            NV_CHECK_ERROR_CLEANUP(
                NvOsFread(hHostBlkDev->hFile,
                                    pBuffer,
                                    NumberOfBlocks * hHostBlkDev->BytesPerSector,
                                    &Bytes)
            );
    }
fail:
    return e;
}

static NvError
HostBlkDevWrite(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 BlockNum,
    const void* pBuffer,
    NvU32 NumberOfBlocks)
{
    NvError e = NvSuccess;
    HostBlockDevHandle hHostBlkDev = (HostBlockDevHandle) hBlockDev;

    if(!hHostBlkDev->hFile)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvOsFopen(g_HostBlockDevData.PartName,
                                NVOS_OPEN_CREATE | NVOS_OPEN_WRITE,
                                &hHostBlkDev->hFile)
        );
    }
    NV_CHECK_ERROR_CLEANUP(
        NvOsFwrite(hHostBlkDev->hFile,
                            pBuffer,
                            NumberOfBlocks * hHostBlkDev->BytesPerSector)
    );
fail:
    return e;
}

static void
HostBlkDevPowerUp(NvDdkBlockDevHandle hBlockDev)
{
}

static void
HostBlkDevPowerDown(NvDdkBlockDevHandle hBlockDev)
{
}

static void
HostBlkDevRegisterHotplugSemaphore(
    NvDdkBlockDevHandle hBlockDev,
    NvOsSemaphoreHandle hHotPlugSema)
{
    // Hot plugging is not supported yet.
}

static  NvError
HostBlkDevBlockDevIoctl(
    NvDdkBlockDevHandle hBlockDev,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    NvError e = NvSuccess;
    HostBlockDevHandle hHostBlkDev = (HostBlockDevHandle) hBlockDev;
    switch(Opcode)
    {
        case NvDdkBlockDevIoctlType_PartitionOperation:
        {
            NvDdkBlockDevIoctl_PartitionOperationInputArgs *pPartOpIn;
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_PartitionOperationInputArgs));
            NV_ASSERT(InputArgs);
            pPartOpIn = (NvDdkBlockDevIoctl_PartitionOperationInputArgs *)
                InputArgs;
            // Check if partition create is done with part id == 0
            if (hHostBlkDev->PartitionNumber)
            {
                // Error as indicates block dev driver handle is
                // not part id == 0
                e = NvError_NotSupported;
                break;
            }
            if (pPartOpIn->Operation == NvDdkBlockDevPartitionOperation_Start)
            {
                hHostBlkDev->PartitionSectorIndex = 0;
            }
        }
        break;

        case NvDdkBlockDevIoctlType_AllocatePartition:
        {
            NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn =
                (NvDdkBlockDevIoctl_AllocatePartitionInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut =
                (NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *)OutputArgs;

            NvU32 NumBlksRequired = 0;
            static NvU32 NumBlksToReduce = 0;
            // Variable contains the start logical sector number for allocated partition
            NV_ASSERT(InputSize ==
                sizeof(NvDdkBlockDevIoctl_AllocatePartitionInputArgs));
            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputSize ==
                sizeof(NvDdkBlockDevIoctl_AllocatePartitionOutputArgs));
            NV_ASSERT(OutputArgs);

            // Check if partition create is done with part id == 0
            if (hHostBlkDev->PartitionNumber)
            {
                // Error as indicates block dev driver handle
                // is not part id == 0
                e = NvError_NotSupported;
                break;
            }
            //check for partition attribute 0x800 to substract the give size from available size
            if (pAllocPartIn->PartitionAttribute & NVDDK_BLOCKDEV_PART_MGMT_POLICY_REMAIN_SIZE)
            {
                //Calculate Remaining blocks in media
                NumBlksRequired = hHostBlkDev->TotalBlocks -
                                            (hHostBlkDev->PartitionSectorIndex /
                                            hHostBlkDev->SectorsPerBlock);

                NumBlksToReduce = pAllocPartIn->NumLogicalSectors /
                                     hHostBlkDev->SectorsPerBlock;
                if(pAllocPartIn->NumLogicalSectors % hHostBlkDev->SectorsPerBlock)
                    NumBlksToReduce++;

                NumBlksRequired = NumBlksRequired - NumBlksToReduce;
            }
            //if partition is neither UDA or GPT
            else if(pAllocPartIn->NumLogicalSectors != (NvU32) -1)
            {
                NumBlksRequired = pAllocPartIn->NumLogicalSectors /
                                                hHostBlkDev->SectorsPerBlock;
                if(pAllocPartIn->NumLogicalSectors % hHostBlkDev->SectorsPerBlock)
                    NumBlksRequired++;
            }
            //if partition is last (GPT)
            else
            {
                if (!NumBlksToReduce)
                    NumBlksRequired = hHostBlkDev->TotalBlocks -
                                            (hHostBlkDev->PartitionSectorIndex /
                                            hHostBlkDev->SectorsPerBlock);
                else
                    NumBlksRequired = NumBlksToReduce;
            }

            if (pAllocPartIn->AllocationType == NvDdkBlockDevPartitionAllocationType_Absolute)
            {
                NvAuPrintf("\r\n NvDdkBlockDevPartitionAllocationType_Absolute ");
                NvAuPrintf("\r\n %d,%d,%d ",pAllocPartIn->StartPhysicalSectorAddress ,
                    hHostBlkDev->PartitionSectorIndex,hHostBlkDev->SectorsPerBlock );
                if((pAllocPartIn->StartPhysicalSectorAddress % hHostBlkDev->SectorsPerBlock) ||
                    (hHostBlkDev->PartitionSectorIndex > pAllocPartIn->StartPhysicalSectorAddress))
                {
                    e = NvError_BadParameter;
                    goto fail;
                }
                hHostBlkDev->PartitionSectorIndex = pAllocPartIn->StartPhysicalSectorAddress;
                NvAuPrintf("\r\n hHostBlkDev->PartitionSectorIndex= %d ",
                    hHostBlkDev->PartitionSectorIndex);
            }
            pAllocPartOut->StartLogicalSectorAddress = hHostBlkDev->PartitionSectorIndex;
            pAllocPartOut->NumLogicalSectors = NumBlksRequired *
                                           hHostBlkDev->SectorsPerBlock;
            hHostBlkDev->PartitionSectorIndex += pAllocPartOut->NumLogicalSectors;
        }
        break;
        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
        {
            NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *pAllocPartOut =
                (NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *)OutputArgs;
            pAllocPartOut->IsGoodBlock = NV_TRUE;
            pAllocPartOut->IsLockedBlock = NV_FALSE;
        }
        break;

        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
        {
            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *pAllocPartIn =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *pAllocPartOut =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *)OutputArgs;
            pAllocPartOut->PhysicalSectorNum = pAllocPartIn->LogicalSectorNum;
        }
        break;

        case NvDdkBlockDevIoctlType_ReadPhysicalSector:
        {
            NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *pAllocPartIn =
                (NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs *)InputArgs;
            NvU32 FilePosition = pAllocPartIn->SectorNum * hHostBlkDev->BytesPerSector;
            NvU32 BytesRead;
            if(!hHostBlkDev->hFile)
            {
                NvOsMemcpy(g_HostBlockDevData.PartName, "BCT",
                                    NVPARTMGR_PARTITION_NAME_LENGTH);
                NV_CHECK_ERROR_CLEANUP(
                    NvOsFopen(g_HostBlockDevData.PartName,
                                         NVOS_OPEN_READ,
                                        &hHostBlkDev->hFile)
                );
            }
            if(pAllocPartIn->SectorNum < g_HostBlockDevData.EndBctSector)
            {
                NvU64 position;
                NvOsFtell(hHostBlkDev->hFile,&position);
                NV_CHECK_ERROR_CLEANUP(
                    NvOsFseek(hHostBlkDev->hFile,FilePosition,NvOsSeek_Set)
                );
                NvOsFtell(hHostBlkDev->hFile,&position);
                NV_CHECK_ERROR_CLEANUP(
                    NvOsFread(hHostBlkDev->hFile,
                                    pAllocPartIn->pBuffer,
                                    pAllocPartIn->NumberOfSectors * hHostBlkDev->BytesPerSector,
                                    &BytesRead)
                );
                NvOsFtell(hHostBlkDev->hFile,&position);
            }
            else
            {
                NvOsMemset(pAllocPartIn->pBuffer, 0, pAllocPartIn->NumberOfSectors *
                                        hHostBlkDev->BytesPerSector);
            }
        }
        break;
        case NvDdkBlockDevIoctlType_WritePhysicalSector:
        {
            NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *pAllocPartIn =
                (NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs *)InputArgs;
            NvU32 FilePosition = pAllocPartIn->SectorNum * hHostBlkDev->BytesPerSector;
            NvU64 position;
            if(!hHostBlkDev->hFile)
            {
                NvOsMemcpy(g_HostBlockDevData.PartName, "BCT",
                                    NVPARTMGR_PARTITION_NAME_LENGTH);
                NV_CHECK_ERROR_CLEANUP(
                    NvOsFopen(g_HostBlockDevData.PartName,
                                        NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_WRITE,
                                        &hHostBlkDev->hFile)
                );
                g_HostBlockDevData.StartBctSector = pAllocPartIn->SectorNum;
            }
            g_HostBlockDevData.EndBctSector = pAllocPartIn->SectorNum +
                                                                        pAllocPartIn->NumberOfSectors;
            NvOsFtell(hHostBlkDev->hFile,&position);

            NV_CHECK_ERROR_CLEANUP(
                NvOsFseek(hHostBlkDev->hFile,FilePosition,NvOsSeek_Set)
            );
            NvOsFtell(hHostBlkDev->hFile,&position);
            NV_CHECK_ERROR_CLEANUP(
                NvOsFwrite(hHostBlkDev->hFile,pAllocPartIn->pBuffer,
                                    pAllocPartIn->NumberOfSectors * hHostBlkDev->BytesPerSector)
            );
            NvOsFtell(hHostBlkDev->hFile,&position);
        }
        break;
    }
fail:
    return e;
}

static void HostBlkDevFlushCache(NvDdkBlockDevHandle hBlockDev)
{
    // Unsupported function for HostBlkDev
}

NvError
HostBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev)
{
    NvError e = NvSuccess;
    HostBlockDevHandle hostBlkDev = NvOsAlloc(sizeof(HostBlockDev));
    NvFlashCmdDevParamOptions *devparams;

    *phBlockDev = NULL;

    hostBlkDev->BlockDev.NvDdkBlockDevClose = &HostBlkDevClose;
    hostBlkDev->BlockDev.NvDdkBlockDevGetDeviceInfo = &HostBlkDevGetDeviceInfo;
    hostBlkDev->BlockDev.NvDdkBlockDevReadSector = &HostBlkDevRead;
    hostBlkDev->BlockDev.NvDdkBlockDevWriteSector = &HostBlkDevWrite;
    hostBlkDev->BlockDev.NvDdkBlockDevPowerUp = &HostBlkDevPowerUp;
    hostBlkDev->BlockDev.NvDdkBlockDevPowerDown = &HostBlkDevPowerDown;
    hostBlkDev->BlockDev.NvDdkBlockDevRegisterHotplugSemaphore =
                                                &HostBlkDevRegisterHotplugSemaphore;
    hostBlkDev->BlockDev.NvDdkBlockDevIoctl = &HostBlkDevBlockDevIoctl;
    hostBlkDev->BlockDev.NvDdkBlockDevFlushCache = &HostBlkDevFlushCache;

    hostBlkDev->PartitionNumber = MinorInstance;
    hostBlkDev->hFile = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvFlashCommandGetOption( NvFlashOption_DevParams, (void *)&devparams)
    );
    hostBlkDev->BytesPerSector = devparams->PageSize;
    hostBlkDev->SectorsPerBlock = devparams->BlockSize/devparams->PageSize;
    hostBlkDev->TotalBlocks = devparams->TotalBlocks;

    if(!hostBlkDev->TotalBlocks || !hostBlkDev->TotalBlocks || !hostBlkDev->TotalBlocks)
    {
        e = NvError_BadParameter;
        goto fail;
    }
    *phBlockDev = &hostBlkDev->BlockDev;
fail:
    return e;
}

NvError
NvDdkBlockDevMgrDeviceOpen(
    NvDdkBlockDevMgrDeviceId DeviceId,
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev)
{
    NvError e = NvSuccess;
    e = HostBlockDevOpen(Instance,MinorInstance,phBlockDev);
    return e;
}

NvError
NvDdkBlockDevMgrInit(void)
{
    NvError e = NvSuccess;
    NvFlashCmdDevParamOptions *devparams;
    NvOsMemcpy(g_HostBlockDevData.PartName,
                        "PT",
                        NVPARTMGR_PARTITION_NAME_LENGTH);
    NV_CHECK_ERROR_CLEANUP(
        NvFlashCommandGetOption( NvFlashOption_DevParams, (void *)&devparams)
    );
    g_HostBlockDevData.ValidTmpBuffBytes = 0;
    g_HostBlockDevData.pStrMgTmpBuff = NvOsAlloc(devparams->PageSize);
    if(!g_HostBlockDevData.pStrMgTmpBuff)
        e = NvError_InsufficientMemory;
    NvOsMemset(g_HostBlockDevData.pStrMgTmpBuff, 0xFF, devparams->PageSize);
fail:
    return e;
}

void
NvDdkBlockDevMgrDeinit(void)
{
}

NvError NvStorMgrFormat(const char * PartitionName)
{
    NvError e = NvSuccess;
    return e;
}

NvError NvStorMgrFileQueryStat(const char *pFileName, NvFileStat* pStat)
{
    NvError e = NvSuccess;
    return e;
}

NvError
NvStorMgrFileOpen(
    const char* pFileName,
    NvU32 Flags,
    NvStorMgrFileHandle* phFile)
{
    NvError e = NvSuccess;
    NvOsMemcpy(g_HostBlockDevData.PartName,
                        pFileName,
                        NVPARTMGR_PARTITION_NAME_LENGTH);
    NvDdkBlockDevMgrDeviceOpen(0,0,0,(NvDdkBlockDevHandle *)phFile);
    return e;
}

NvError NvStorMgrFileRead(NvStorMgrFileHandle hStorMgrFH, void *pBuffer,
    NvU32 BytesToRead,NvU32 *BytesRead)
{
    NvError e = NvSuccess;
    return e;
}

static NvError
FlushTempBuff( NvDdkBlockDevHandle pBlkDev, NvU8 *pData, NvU32 BytesToWrite)
{
    NvError e = NvSuccess;
    NvFlashCmdDevParamOptions *devparams;
    NV_CHECK_ERROR_CLEANUP(
        NvFlashCommandGetOption( NvFlashOption_DevParams, (void *)&devparams)
    );

    if((g_HostBlockDevData.ValidTmpBuffBytes + BytesToWrite) > devparams->PageSize)
    {
        NvOsMemcpy(
                &g_HostBlockDevData.pStrMgTmpBuff[g_HostBlockDevData.ValidTmpBuffBytes],
                pData,
                (devparams->PageSize - g_HostBlockDevData.ValidTmpBuffBytes));
        NV_CHECK_ERROR_CLEANUP(
            pBlkDev->NvDdkBlockDevWriteSector(pBlkDev,
                                                                    0,g_HostBlockDevData.pStrMgTmpBuff,1)
        );
        pData += (devparams->PageSize - g_HostBlockDevData.ValidTmpBuffBytes);
        BytesToWrite -= (devparams->PageSize - g_HostBlockDevData.ValidTmpBuffBytes);
        g_HostBlockDevData.ValidTmpBuffBytes = 0;
        NvOsMemset(g_HostBlockDevData.pStrMgTmpBuff, 0xFF, devparams->PageSize);
    }
    NvOsMemcpy(
            &g_HostBlockDevData.pStrMgTmpBuff[g_HostBlockDevData.ValidTmpBuffBytes],
            pData,
            BytesToWrite);
    g_HostBlockDevData.ValidTmpBuffBytes = BytesToWrite;
fail:
    return e;
}

NvError NvStorMgrFileWrite( NvStorMgrFileHandle hStorMgrFH, void *pBuffer,
      NvU32 BytesToWrite, NvU32 *BytesWritten)
{
    NvError e = NvSuccess;
    NvDdkBlockDevHandle pBlkDev = (NvDdkBlockDevHandle ) hStorMgrFH;
    NvFlashCmdDevParamOptions *devparams;
    NvU8 *pData = (NvU8*) pBuffer;
    NvU32 SectorsToWrite = 0;
    *BytesWritten = BytesToWrite;

    NV_CHECK_ERROR_CLEANUP(
        NvFlashCommandGetOption( NvFlashOption_DevParams, (void *)&devparams)
    );
    SectorsToWrite = BytesToWrite/devparams->PageSize;

    if(BytesToWrite < devparams->PageSize)
    {
        NV_CHECK_ERROR_CLEANUP(FlushTempBuff(pBlkDev, pData, BytesToWrite));
    }
    else
    {
        NvU32 ResidualBytes = BytesToWrite % devparams->PageSize;
        NV_CHECK_ERROR_CLEANUP(
            pBlkDev->NvDdkBlockDevWriteSector(pBlkDev,
                                                                    0,pData,SectorsToWrite)
        );
        if(ResidualBytes)
        {
            pData += (SectorsToWrite * devparams->PageSize);
            BytesToWrite -= (SectorsToWrite * devparams->PageSize);
            NV_CHECK_ERROR_CLEANUP(FlushTempBuff(pBlkDev, pData, BytesToWrite));
        }
    }

fail:
    return e;
}

NvError
NvStorMgrFileSeek(NvStorMgrFileHandle hStorMgrFH, NvS64 ByteOffset, NvU32 Whence)
{
    NvError e = NvSuccess;
     return e;
}

NvError NvStorMgrFileClose(NvStorMgrFileHandle hStorMgrFH)
{
    NvError e = NvSuccess;
    NvDdkBlockDevHandle  hdev = (NvDdkBlockDevHandle ) hStorMgrFH;
    if(hdev)
    {
        if(g_HostBlockDevData.ValidTmpBuffBytes)
        {
            NV_CHECK_ERROR_CLEANUP(
                hdev->NvDdkBlockDevWriteSector(
                    hdev,
                    0,
                    g_HostBlockDevData.pStrMgTmpBuff,
                    1)
            );
            g_HostBlockDevData.ValidTmpBuffBytes = 0;
        }
        hdev->NvDdkBlockDevClose(hdev);
    }
fail:
    return e;
}

NvError
NvStorMgrPartitionQueryStat(
    const char *pFileName,
    NvPartitionStat *pStat)
{
    NvError e = NvSuccess;
     return e;
}
