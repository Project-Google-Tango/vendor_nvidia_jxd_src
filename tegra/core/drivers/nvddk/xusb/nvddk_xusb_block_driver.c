/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvassert.h"
#include "nvos.h"
#include "nvboot_bit.h"
#include "nvboot_clocks_int.h"
#include "nvboot_reset_int.h"
#include "nvrm_module.h"
#include "nvrm_memmgr.h"
#include "nvddk_xusb_block_driver.h"
#include "nvddk_xusb_context.h"
#include "nvddk_xusbh.h"
#include "iram_address_allocation.h"

#define BYTES_PER_BLOCK        (16384)
/* Instead of reading/writing in terms of sector size (i.e. 512 bytes),
   a *chunk* of bytes are sent to/received from the device. This is the maximum
   number of bytes that can be transacted with the driver
 */
#define RW_CHUNK_SIZE  (512*255)

typedef struct XusbhBlockDevRec
{
    NvDdkBlockDev BlockDev;
    NvddkXusbContext* Ctxt;
    NvU32 Instance;
    NvU32 MinorInstance;
    NvU32 PartitionAddr;
    NvU32 PartitionSectorIndex;
    NvU32 BytesPerSector;
    NvU32 SectorsPerBlock;
    NvU32 TotalBlocks;
    NvBool isOpen;
} XusbhBlockDev,*XusbhBlockDevHandle;

/* Structure to store the info related SPI block driver */
typedef struct XusbhBlockDevInfoRec
{
    /* RM Handle passed from block device manager */
    NvRmDeviceHandle hRm;

    /* Maximum USB Host controller instances on the SOC */
    NvU32 MaxInstances;

    /* Indicates if the block driver is initialized */
    NvBool IsInitialized;

    /* Pointer to the list of Usb devices */
    XusbhBlockDev *XusbhBlockDevList;

} XusbhBlockDevInfo;

static XusbhBlockDevInfo s_XusbhBlockDevInfo;

// macro to make string from constant
#define MACRO_GET_STR(x, y) \
    NvOsMemcpy(x, #y, (NvOsStrlen(#y) + 1))


/* Structure to store the info related SPI block driver */
typedef struct UsbhBlockDevInfoRec
{
    NvDdkBlockDev BlockDev;

    /* RM Handle passed from block device manager */
    NvRmDeviceHandle hRm;

} Usb3BlockDev;

NvError
NvBootUsb3ReadPage(
    const NvU32 Block,
    const NvU32 Page,
    NvU8 *Dest);


NvError
NvBootUsb3WritePage(
    const NvU32 Block,
    const NvU32 Page,
    NvU8 *Src);


static Usb3BlockDev s_Usb3BlockDev;


/*************************Utility Functions********************************/

static void
UtilGetIoctlName(
        NvU32 Opcode,
        NvU8 *IoctlStr)
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
            MACRO_GET_STR(IoctlStr,
                    NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector);
            break;
        case NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors:
            MACRO_GET_STR(IoctlStr,
                    NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors);
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
            MACRO_GET_STR(IoctlStr,
                    NvDdkBlockDevIoctlType_WriteVerifyModeSelect);
            break;
        case NvDdkBlockDevIoctlType_LockRegion:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_LockRegion);
            break;
        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
            MACRO_GET_STR(IoctlStr, NvDdkBlockDevIoctlType_ErasePhysicalBlock);
            break;
        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
            MACRO_GET_STR(IoctlStr,
                    NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus);
            break;
        case NvDdkBlockDevIoctlType_VerifyCriticalPartitions:
            MACRO_GET_STR(IoctlStr,
                    NvDdkBlockDevIoctlType_VerifyCriticalPartitions);
            break;
        default:
            // Illegal Ioctl string
            MACRO_GET_STR(IoctlStr, UnknownIoctl);
            break;
    }
}


/****************** Functions to populate the block driver*****************/

static void Usb3BlockDevClose(NvDdkBlockDevHandle hBlockDev)
{
   return;
}

static void
Usb3BlockDevGetDeviceInfo(
        NvDdkBlockDevHandle hBlockDev,
        NvDdkBlockDevInfo* pBlockDevInfo)
{
    NV_ASSERT(hBlockDev);
    NV_ASSERT(pBlockDevInfo);
    NvddkXusbContext* Ctxt = s_XusbhBlockDevInfo.XusbhBlockDevList->Ctxt;

    pBlockDevInfo->BytesPerSector = Ctxt->Usb3Status.BlockLenInByte;
    pBlockDevInfo->SectorsPerBlock = BYTES_PER_BLOCK / pBlockDevInfo->BytesPerSector;
    pBlockDevInfo->TotalBlocks = Ctxt->Usb3Status.LastLogicalBlkAddr;
    pBlockDevInfo->DeviceType = NvDdkBlockDevDeviceType_Fixed;
}

static void
Usb3BlockDevRegisterHotplugSemaphore(
        NvDdkBlockDevHandle hBlockDev,
        NvOsSemaphoreHandle hHotPlugSema)
{
    return;
}

static NvError
Usb3BlockDevRead(
        NvDdkBlockDevHandle hBlockDev,
        NvU32 SectorNum,
        void* pBuffer,
        NvU32 NumberOfSectors)
{
    NvError e = NvSuccess;
    NvU32 i = 0;
    NvU32 leftover = 0;
    NvddkXusbContext* Ctxt = s_XusbhBlockDevInfo.XusbhBlockDevList->Ctxt;
    NvU32 bytes_per_sector = Ctxt->Usb3Status.BlockLenInByte;
    NvU32 sectors_per_block = BYTES_PER_BLOCK / Ctxt->Usb3Status.BlockLenInByte;
    NvU8 *DataBuf = (NvU8*)(Ctxt->pCtxMemAddr + BUF_OFFSET);

    NvU32 NumOfChunks = (NumberOfSectors * bytes_per_sector)/RW_CHUNK_SIZE;
    for (i = 0; i < NumOfChunks; i++)
    {
        NvU32 BlockNum = SectorNum / sectors_per_block;
        NvU32 secnum = SectorNum % sectors_per_block;

        NV_CHECK_ERROR_CLEANUP(NvddkXusbReadPage(Ctxt, BlockNum, secnum, \
                                                          DataBuf, RW_CHUNK_SIZE));
        NvOsMemcpy((NvU8*)pBuffer + (i*RW_CHUNK_SIZE), (void*)DataBuf, RW_CHUNK_SIZE);

        SectorNum += (RW_CHUNK_SIZE/bytes_per_sector);
    }
    leftover = (NumberOfSectors * bytes_per_sector) % RW_CHUNK_SIZE;
    if (leftover)
    {
        NvU32 BlockNum = SectorNum / sectors_per_block;
        NvU32 secnum = SectorNum % sectors_per_block;
        NV_CHECK_ERROR_CLEANUP(NvddkXusbReadPage(Ctxt, BlockNum, secnum, \
                                                            DataBuf, leftover));
        NvOsMemcpy((NvU8*)pBuffer + (i*RW_CHUNK_SIZE), (void*)DataBuf, leftover);
    }

fail:
    return e;
}

static NvError
Usb3BlockDevWrite(
        NvDdkBlockDevHandle hBlockDev,
        NvU32 SectorNum,
        const void* pBuffer,
        NvU32 NumberOfSectors)
{
    NvError e = NvSuccess;
    NvU32 i = 0;
    NvU32 leftover = 0;
    NvddkXusbContext* Ctxt = s_XusbhBlockDevInfo.XusbhBlockDevList->Ctxt;
    NvU32 bytes_per_sector = Ctxt->Usb3Status.BlockLenInByte;
    NvU32 sectors_per_block = BYTES_PER_BLOCK / Ctxt->Usb3Status.BlockLenInByte;
    NvU8 *DataBuf = (NvU8*)(Ctxt->pCtxMemAddr + BUF_OFFSET);

    NvU32 NumOfChunks = (NumberOfSectors * bytes_per_sector)/RW_CHUNK_SIZE;
    for (i = 0; i < NumOfChunks; i++)
    {
        NvU32 BlockNum = SectorNum / sectors_per_block;
        NvU32 secnum = SectorNum % sectors_per_block;

        NvOsMemcpy((void*)DataBuf, (NvU8*)pBuffer + (i*RW_CHUNK_SIZE), RW_CHUNK_SIZE);
        NV_CHECK_ERROR_CLEANUP(NvddkXusbWritePage(Ctxt, BlockNum, secnum, \
                                                          DataBuf, RW_CHUNK_SIZE));

        SectorNum += (RW_CHUNK_SIZE/bytes_per_sector);
    }
    leftover = (NumberOfSectors * bytes_per_sector) % RW_CHUNK_SIZE;
    if (leftover)
    {
        NvU32 BlockNum = SectorNum / sectors_per_block;
        NvU32 secnum = SectorNum % sectors_per_block;
        NvOsMemcpy((void*)DataBuf, (NvU8*)pBuffer + (i*RW_CHUNK_SIZE), leftover);
        NV_CHECK_ERROR_CLEANUP(NvddkXusbWritePage(Ctxt, BlockNum, secnum, \
                                                            DataBuf, leftover));
    }

fail:
    return e;
}

static void
Usb3BlockDevPowerUp(
        NvDdkBlockDevHandle hBlockDev)
{
    /*Not Supported*/
}

static void
Usb3BlockDevPowerDown(
        NvDdkBlockDevHandle hBlockDev)
{
    /*Not Supported*/
}

static void Usb3BlockDevFlushCache(
        NvDdkBlockDevHandle hBlockDev)
{
    /*Not Supported*/
}

static NvError
Usb3BlockDevIoctl(
        NvDdkBlockDevHandle hBlockDev,
        NvU32 Opcode,
        NvU32 InputSize,
        NvU32 OutputSize,
        const void *InputArgs,
        void *OutputArgs)
{
    NvError e = NvSuccess;
    XusbhBlockDevHandle hHostBlkDev = (XusbhBlockDevHandle) hBlockDev;

    /* set the block dev params from global struct*/
    hHostBlkDev->BytesPerSector =
        s_XusbhBlockDevInfo.XusbhBlockDevList->Ctxt->Usb3Status.BlockLenInByte;
    hHostBlkDev->SectorsPerBlock = 1;
    hHostBlkDev->TotalBlocks =
    s_XusbhBlockDevInfo.XusbhBlockDevList->Ctxt->Usb3Status.LastLogicalBlkAddr+1;

#define MAX_IOCTL_STRING_LENGTH_BYTES 80
    NvU8 IoctlName[MAX_IOCTL_STRING_LENGTH_BYTES];

    /* Decode the IOCTL opcode */
    switch (Opcode)
    {
        case NvDdkBlockDevIoctlType_ReadPhysicalSector:
        {
            NV_ASSERT(InputArgs);
            e = Usb3BlockDevRead(hBlockDev,
                  ((NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs*)InputArgs)->SectorNum,
                  ((NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs*)InputArgs)->pBuffer,
                  ((NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs*)
                  InputArgs)->NumberOfSectors);
        }
        break;

        case NvDdkBlockDevIoctlType_WritePhysicalSector:
        {
            NV_ASSERT(InputArgs);
            e = Usb3BlockDevWrite(hBlockDev,
                  ((NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs*)InputArgs)->SectorNum,
                  ((NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs*)InputArgs)->pBuffer,
                  ((NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs*)
                  InputArgs)->NumberOfSectors);
        }
        break;

        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
        {
            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs
                *pMapPhysicalSectorIn =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs *) InputArgs;

            NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs
                *pMapPhysicalSectorOut =
                (NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs *) OutputArgs;

            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);
            NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs)
                 );

            NV_ASSERT(OutputSize ==
                    sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs)
                 );

            /* Physical and logical address is assumed to be the same for USB Drives */
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
            NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsInputArgs)
                 );

            NV_ASSERT(OutputSize ==
                    sizeof(NvDdkBlockDevIoctl_GetPartitionPhysicalSectorsOutputArgs)
                 );


            pPhysicalBlockOut->PartitionPhysicalSectorStart =
                pLogicalBlockIn->PartitionLogicalSectorStart;
            pPhysicalBlockOut->PartitionPhysicalSectorStop =
                pLogicalBlockIn->PartitionLogicalSectorStop;
        }
        break;

        case NvDdkBlockDevIoctlType_FormatDevice:
        {
            /*Not Necessary*/
            e = NvSuccess;
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
            if (pPartOpIn->Operation == NvDdkBlockDevPartitionOperation_Start)
            {
                hHostBlkDev->PartitionSectorIndex = 0;
            }
            e = NvSuccess;
        }
        break;

        case NvDdkBlockDevIoctlType_AllocatePartition:
        {
            NvU32 NumBlksRequired = 0;
            static NvU32 NumBlksToReduce = 0;
            NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn =
                (NvDdkBlockDevIoctl_AllocatePartitionInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut =
                (NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *)OutputArgs;

            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);
            NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_AllocatePartitionInputArgs));
            NV_ASSERT(OutputSize ==
                    sizeof(NvDdkBlockDevIoctl_AllocatePartitionOutputArgs));

            //check for partition attribute 0x800 to substract
            //the give size from available size
            if (pAllocPartIn->PartitionAttribute &
                NVDDK_BLOCKDEV_PART_MGMT_POLICY_REMAIN_SIZE)
            {
                //Calculate Remaining blocks in media
                NumBlksRequired = hHostBlkDev->TotalBlocks -
                                           (hHostBlkDev->PartitionSectorIndex /
                                            hHostBlkDev->SectorsPerBlock);

                NumBlksToReduce = pAllocPartIn->NumLogicalSectors /
                                     hHostBlkDev->SectorsPerBlock;
                if(pAllocPartIn->NumLogicalSectors %
                    hHostBlkDev->SectorsPerBlock)
                    NumBlksToReduce++;

                NumBlksRequired = NumBlksRequired - NumBlksToReduce;
            }
            //if partition is neither UDA or GPT
            else if(pAllocPartIn->NumLogicalSectors != (NvU32) -1)
            {
                NumBlksRequired = pAllocPartIn->NumLogicalSectors /
                                                hHostBlkDev->SectorsPerBlock;
                if(pAllocPartIn->NumLogicalSectors %
                    hHostBlkDev->SectorsPerBlock)
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

            pAllocPartOut->StartLogicalSectorAddress =
                hHostBlkDev->PartitionSectorIndex;
            pAllocPartOut->NumLogicalSectors = NumBlksRequired *
                                           hHostBlkDev->SectorsPerBlock;
            hHostBlkDev->PartitionSectorIndex +=
                pAllocPartOut->NumLogicalSectors;

            e = NvSuccess;
        }
        break;

        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
        {
            e = NvSuccess;
        }
        break;

        case NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus:
        {
            NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *pAllocPartOut =
                (NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs *)OutputArgs;
            pAllocPartOut->IsGoodBlock = NV_TRUE;
            pAllocPartOut->IsLockedBlock = NV_FALSE;
            e = NvSuccess;
        }
        break;

        case NvDdkBlockDevIoctlType_ErasePartition:
        {
            e = NvSuccess;
        }
        break;

        case NvDdkBlockDevIoctlType_ErasePhysicalBlock:
        {
            e = NvSuccess;
        }
        break;

        case NvDdkBlockDevIoctlType_VerifyCriticalPartitions:
        {
            e = NvSuccess;
        }
        break;

        default:
        {
            e = NvError_BlockDriverIllegalIoctl;
        }
        break;
    }

    if (e != NvSuccess)
    {
        // function to return name corresponding to ioctl opcode
        UtilGetIoctlName(Opcode, IoctlName);
    }

#undef  MAX_IOCTL_STRING_LENGTH_BYTES

    return e;
}


NvError NvDdkXusbhBlockDevInit(NvRmDeviceHandle hDevice)
{
    NvError e = NvSuccess;
    NvddkXusbContext* Ctxt = NULL;
    NV_ASSERT(hDevice);

    if (!s_XusbhBlockDevInfo.IsInitialized)
    {
        s_XusbhBlockDevInfo.MaxInstances =
            NvRmModuleGetNumInstances(hDevice, NvRmModuleID_XUSB_HOST);

        s_XusbhBlockDevInfo.XusbhBlockDevList = NvOsAlloc(
                sizeof(XusbhBlockDev) * s_XusbhBlockDevInfo.MaxInstances);

        if (!s_XusbhBlockDevInfo.XusbhBlockDevList)
        {
            e = NvError_InsufficientMemory;
            NvOsDebugPrintf("XUSBH ERROR: NvDdkXusbhBlockDevInit failed error[0x%x]..\n", e);
            goto fail;
        }

        NvOsMemset((void *)s_XusbhBlockDevInfo.XusbhBlockDevList, 0,
                (sizeof(XusbhBlockDev) * s_XusbhBlockDevInfo.MaxInstances));

        s_XusbhBlockDevInfo.XusbhBlockDevList->Ctxt =
                                            NvOsAlloc(sizeof(NvddkXusbContext));
        Ctxt = s_XusbhBlockDevInfo.XusbhBlockDevList->Ctxt;
        NvOsMemset((void *)Ctxt,0,sizeof(NvddkXusbContext));


        NV_CHECK_ERROR_CLEANUP(NvRmMemHandleAlloc(hDevice, NULL, 0,
            CTX_ALIGNMENT, NvOsMemAttribute_Uncached, CTX_SIZE, 0, 1,
            &Ctxt->hCtxMemHandle));

        Ctxt->pCtxMemAddr = (NvU8*)NvRmMemPin(Ctxt->hCtxMemHandle);

        if (Nvddk_xusbh_init(Ctxt) != NvSuccess)
        {
            NvOsDebugPrintf("XUSB: No device detected\n");
            s_XusbhBlockDevInfo.hRm = NULL;
            s_XusbhBlockDevInfo.IsInitialized = NV_FALSE;
            NvRmMemUnpin(Ctxt->hCtxMemHandle);
            NvRmMemHandleFree(Ctxt->hCtxMemHandle);
            e = NvSuccess;
        }
        else
        {
            /* Save the RM Handle */
            s_XusbhBlockDevInfo.hRm = hDevice;
            s_XusbhBlockDevInfo.IsInitialized = NV_TRUE;
        }
    }

fail:
    return e;
}

void NvDdkXusbhBlockDevDeinit(void)
{
    Nvddk_xusbh_deinit();

    if (s_XusbhBlockDevInfo.IsInitialized)
    {
        NvRmMemUnpin(s_XusbhBlockDevInfo.XusbhBlockDevList->Ctxt->hCtxMemHandle);
        NvRmMemHandleFree(s_XusbhBlockDevInfo.XusbhBlockDevList->Ctxt->hCtxMemHandle);
        NvOsFree(s_XusbhBlockDevInfo.XusbhBlockDevList);
        s_XusbhBlockDevInfo.XusbhBlockDevList = NULL;
        s_XusbhBlockDevInfo.hRm = NULL;
        s_XusbhBlockDevInfo.MaxInstances = 0;
        s_XusbhBlockDevInfo.IsInitialized = NV_FALSE;
    }
}

NvError NvDdkXusbhBlockDevOpen(
        NvU32 Instance,
        NvU32 MinorInstance,
        NvDdkBlockDevHandle *phBlockDev)
{
    NvError e = NvSuccess;

    NV_ASSERT(phBlockDev);
    if (s_XusbhBlockDevInfo.IsInitialized != NV_TRUE)
    {
        e = NvError_NotInitialized;
        return e;
    }

    s_Usb3BlockDev.BlockDev.NvDdkBlockDevClose = &Usb3BlockDevClose;
    s_Usb3BlockDev.BlockDev.NvDdkBlockDevGetDeviceInfo =
                                                     &Usb3BlockDevGetDeviceInfo;
    s_Usb3BlockDev.BlockDev.NvDdkBlockDevReadSector = &Usb3BlockDevRead;
    s_Usb3BlockDev.BlockDev.NvDdkBlockDevWriteSector = &Usb3BlockDevWrite;
    s_Usb3BlockDev.BlockDev.NvDdkBlockDevPowerUp = &Usb3BlockDevPowerUp;
    s_Usb3BlockDev.BlockDev.NvDdkBlockDevPowerDown = &Usb3BlockDevPowerDown;
    s_Usb3BlockDev.BlockDev.NvDdkBlockDevIoctl = &Usb3BlockDevIoctl;
    s_Usb3BlockDev.BlockDev.NvDdkBlockDevRegisterHotplugSemaphore =
                                          &Usb3BlockDevRegisterHotplugSemaphore;
    s_Usb3BlockDev.BlockDev.NvDdkBlockDevFlushCache = &Usb3BlockDevFlushCache;

    *phBlockDev = &(s_Usb3BlockDev.BlockDev);

    return e;
}
