/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_usbh_block_driver.h"
#include "nvddk_blockdev_defs.h"
#include "nvassert.h"
#include "nvddk_blockdev.h"
#include "nvddk_usbh.h"
#include "nvrm_power.h"
#include "nvos.h"
#include "nvodm_query.h"
#include "nvddk_usbh_scsi_driver.h"


#define EP_ADDR_MASK 0x0F

#define NvError_Failure (NvError)(2)

#define MACRO_GET_STR(x, y) \
    NvOsMemcpy(x, #y, (NvOsStrlen(#y) + 1))

#if 0
#define NVUSBH_DEBUG_PRINTF(x) NvOsDebugPrintf x
#else
#define NVUSBH_DEBUG_PRINTF(x) {}
#endif

#define BYTES_PER_BLOCK 512
#define SECTORS_PER_BLOCK 1

/* Structure to store the info related to the USB Block devices */
typedef struct UsbhBlockDevRec
{
    NvDdkBlockDev BlockDev;
    UsbHostContext* Ctxt;
    DeviceInfo* devInfo;
    IOEndpointHandle ioHandle;
    NvU32 Instance;
    NvU32 MinorInstance;
    NvU32 PartitionAddr;
    NvU32 SectorsPerBlock;
    NvBool isOpen;
} UsbhBlockDev;

/* Structure to store the info related SPI block driver */
typedef struct UsbhBlockDevInfoRec
{
    /* RM Handle passed from block device manager */
    NvRmDeviceHandle hRm;

    /* Maximum USB Host controller instances on the SOC */
    NvU32 MaxInstances;

    /* Indicates if the block driver is initialized */
    NvBool IsInitialized;

    /* Pointer to the list of Usb devices */
    UsbhBlockDev *UsbhBlockDevList;

    /* Io module */
    NvU32 IoModule;

} UsbhBlockDevInfo;

static UsbhBlockDevInfo s_UsbhBlockDevInfo;

void delayfunction(NvU32 x, NvU32 y);
void delayfunction(NvU32 x, NvU32 y)
{
    NvU32 a, b, c, i, j;
    for(i = 0; i < x; i++)
    {
        for(j = 0;j < y; j++)
            a = b;
        b = c;
        c = a;
        a = b *c;
    }
}

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

static void UsbhBlockDevClose(NvDdkBlockDevHandle hBlockDev)
{
    UsbhBlockDev* pUsbhBlkDev = (UsbhBlockDev* ) hBlockDev;
    NV_ASSERT(hBlockDev);

    if(!pUsbhBlkDev->isOpen)
    {
        NvOsDebugPrintf("Trying to close driver without open\n");
        goto end;
    }
    NvRmClose(pUsbhBlkDev->Ctxt->RmHandle);
    NvDdkUsbhClose(pUsbhBlkDev->Ctxt->hUsbHost,NV_TRUE);
    NvOsFree(pUsbhBlkDev->Ctxt);
    NvOsFree(pUsbhBlkDev->ioHandle);

end:

    return;
}

static void
UsbhBlockDevGetDeviceInfo(
        NvDdkBlockDevHandle hBlockDev,
        NvDdkBlockDevInfo* pBlockDevInfo)
{
    UsbhBlockDev* pUsbhBlkDev = (UsbhBlockDev* ) hBlockDev;

    NV_ASSERT(hBlockDev);
    NV_ASSERT(pBlockDevInfo);

    pBlockDevInfo->BytesPerSector = BYTES_PER_BLOCK;
    pBlockDevInfo->SectorsPerBlock = SECTORS_PER_BLOCK;
    pBlockDevInfo->TotalBlocks =
        pUsbhBlkDev->devInfo->RdCapacityData
        [pUsbhBlkDev->devInfo->MediaLun].RLogicalBlockAddress;
    pBlockDevInfo->DeviceType = NvDdkBlockDevDeviceType_Removable;
    NVUSBH_DEBUG_PRINTF(("UsbhBlockDevGetDeviceInfo..BytesPerSector = %d"
        "::SectorsPerBlock=%d::TotalBlocks=%d\n",
        pBlockDevInfo->BytesPerSector,
        pBlockDevInfo->SectorsPerBlock,
        pBlockDevInfo->TotalBlocks));
}

static void
UsbhBlockDevRegisterHotplugSemaphore(
        NvDdkBlockDevHandle hBlockDev,
        NvOsSemaphoreHandle hHotPlugSema)
{
    NVUSBH_DEBUG_PRINTF(("UsbhBlockDevRegisterHotplugSemaphore::Not Implemented\n"));
    return;
}

static NvError
UsbhBlockDevRead(
        NvDdkBlockDevHandle hBlockDev,
        NvU32 SectorNum,
        void* const pBuffer,
        NvU32 NumberOfSectors)
{
    NvError err = NvSuccess;
    UsbhBlockDev* pUsbhBlkDev = (UsbhBlockDev* ) hBlockDev;
    NvDdkUsbhHandle ghUsbh = pUsbhBlkDev->Ctxt->hUsbHost;
    DeviceInfo *devInfo = pUsbhBlkDev->devInfo;
    IOEndpointHandle ioHandle = pUsbhBlkDev->ioHandle;
    NVUSBH_DEBUG_PRINTF(("UsbhBlockDevRead::Reading From device...\n"));

    //Assuming lba = SectorNum
    err = scsiRead(ghUsbh,
            devInfo,
            ioHandle->inEndpoint,
            ioHandle->OutEndpoint,
            devInfo->MediaLun,
            SectorNum,
            NumberOfSectors,
            (NvU8*)pBuffer);
    return err;
}

static NvError
UsbhBlockDevWrite(
        NvDdkBlockDevHandle hBlockDev,
        NvU32 SectorNum,
        const void* pBuffer,
        NvU32 NumberOfSectors)
{
    NvError err = NvSuccess;
    UsbhBlockDev* pUsbhBlkDev = (UsbhBlockDev* ) hBlockDev;
    NvDdkUsbhHandle ghUsbh = pUsbhBlkDev->Ctxt->hUsbHost;
    DeviceInfo *devInfo = pUsbhBlkDev->devInfo;
    IOEndpointHandle ioHandle = pUsbhBlkDev->ioHandle;
    NVUSBH_DEBUG_PRINTF(("UsbhBlockDevWrite::Writing To device...\n"));
    //Assuming lba = SectorNum
    err = scsiWrite(ghUsbh,
            devInfo,
            ioHandle->inEndpoint,
            ioHandle->OutEndpoint,
            devInfo->MediaLun,
            SectorNum,
            NumberOfSectors,
            (NvU8*)pBuffer);

    return err;
}

static void
UsbhBlockDevPowerUp(
        NvDdkBlockDevHandle hBlockDev)
{
    NVUSBH_DEBUG_PRINTF(("UsbhBlockDevPowerUp::Not supported\n"));
    /*Not Supported*/
}

static void
UsbhBlockDevPowerDown(
        NvDdkBlockDevHandle hBlockDev)
{
    NVUSBH_DEBUG_PRINTF(("UsbhBlockDevPowerUp::Not supported\n"));
    /*Not Supported*/
}

static void UsbhBlockDevFlushCache(
        NvDdkBlockDevHandle hBlockDev)
{
    NVUSBH_DEBUG_PRINTF(("Flush Cache::Not implemented\n"));
    /*Not Supported*/
}

static  NvError
UsbhBlockDevIoctl(
        NvDdkBlockDevHandle hBlockDev,
        NvU32 Opcode,
        NvU32 InputSize,
        NvU32 OutputSize,
        const void *InputArgs,
        void *OutputArgs)
{
    NvError e = NvSuccess;
    UsbhBlockDev* pUsbhBlkDev = (UsbhBlockDev* ) hBlockDev;
#define MAX_IOCTL_STRING_LENGTH_BYTES 80
    NvU8 IoctlName[MAX_IOCTL_STRING_LENGTH_BYTES];
    NvU32 BlockCount = 0;

    /* Decode the IOCTL opcode */
    switch (Opcode)
    {
        case NvDdkBlockDevIoctlType_ReadPhysicalSector:
        {
            NVUSBH_DEBUG_PRINTF(("ReadPhysicalSector...\n"));
            NV_ASSERT(InputArgs);
            e = UsbhBlockDevRead(hBlockDev,
                    ((NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs*) InputArgs)->SectorNum,
                    ((NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs*) InputArgs)->pBuffer,
                    ((NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs*)
                     InputArgs)->NumberOfSectors);
        }
        break;

        case NvDdkBlockDevIoctlType_WritePhysicalSector:
        {
            NVUSBH_DEBUG_PRINTF(("WritePhysicalSector...\n"));
            NV_ASSERT(InputArgs);
            e = UsbhBlockDevWrite(hBlockDev,
                    ((NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs*) InputArgs)->SectorNum,
                    ((NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs*) InputArgs)->pBuffer,
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

            NVUSBH_DEBUG_PRINTF(
                    ("NvDdkBlockDevIoctlType_GetPartitionPhysicalSectors::"
                     "PartitionPhysicalSectorStart[%d], "
                     "PartitionPhysicalSectorStop[%d]\n",
                     pLogicalBlockIn->PartitionLogicalSectorStart,
                     pLogicalBlockIn->PartitionLogicalSectorStop));

            pPhysicalBlockOut->PartitionPhysicalSectorStart =
                pLogicalBlockIn->PartitionLogicalSectorStart;
            pPhysicalBlockOut->PartitionPhysicalSectorStop =
                pLogicalBlockIn->PartitionLogicalSectorStop;
        }
        break;

        case NvDdkBlockDevIoctlType_FormatDevice:
        {
            e = scsiFormatUnit(pUsbhBlkDev->Ctxt->hUsbHost,
                    pUsbhBlkDev->devInfo,
                    pUsbhBlkDev->ioHandle->inEndpoint,
                    pUsbhBlkDev->ioHandle->OutEndpoint,
                    pUsbhBlkDev->devInfo->MediaLun,
                    0,0,0,0);

            if (e != NvSuccess)
                NvOsDebugPrintf("Format Unit Command failed\n");
        }
        break;

        case NvDdkBlockDevIoctlType_PartitionOperation:
        {
            NvDdkBlockDevIoctl_PartitionOperationInputArgs *pPartOpIn;
            NVUSBH_DEBUG_PRINTF
                (("DevIoctl:: NvDdkBlockDevIoctlType_PartitionOperation\n"));
            NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_PartitionOperationInputArgs));
            NV_ASSERT(InputArgs);
            pPartOpIn = (NvDdkBlockDevIoctl_PartitionOperationInputArgs *)
                InputArgs;
            // Check if partition create is done with part id == 0
            if (pUsbhBlkDev->MinorInstance)
            {
                // Error as indicates block dev driver handle is
                // not part id == 0
                NVUSBH_DEBUG_PRINTF(("Part ID can only be 0\n"));
                e = NvError_NotSupported;
                break;
            }
            if (pPartOpIn->Operation == NvDdkBlockDevPartitionOperation_Finish)
            {
                pUsbhBlkDev->PartitionAddr = 0;
            }
        }
        break;

        case NvDdkBlockDevIoctlType_AllocatePartition:
        {
            NvDdkBlockDevIoctl_AllocatePartitionInputArgs *pAllocPartIn =
                (NvDdkBlockDevIoctl_AllocatePartitionInputArgs *)InputArgs;
            NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *pAllocPartOut =
                (NvDdkBlockDevIoctl_AllocatePartitionOutputArgs *)OutputArgs;

            NVUSBH_DEBUG_PRINTF(("NvDdkBlockDevIoctlType_AllocatePartition.I/P::"
                        "StartPhysicalSectorAddress = "
                        "%d::NumLogicalSectors=%d\n",pAllocPartIn->StartPhysicalSectorAddress,
                        pAllocPartIn->NumLogicalSectors));

            NV_ASSERT(InputArgs);
            NV_ASSERT(OutputArgs);
            NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_AllocatePartitionInputArgs));
            NV_ASSERT(OutputSize ==
                    sizeof(NvDdkBlockDevIoctl_AllocatePartitionOutputArgs));

            /* Check if partition create is done with part id equal to 0 */
            if (pUsbhBlkDev->MinorInstance)
            {
                NVUSBH_DEBUG_PRINTF(("NvDdkBlockDevIoctlType_AllocatePartition::"
                            "MinorInstance[%d]..\n", pUsbhBlkDev->MinorInstance));
                e = NvError_NotSupported;
                break;
            }
            if (pAllocPartIn->AllocationType ==
                    NvDdkBlockDevPartitionAllocationType_Absolute)
            {
                NVUSBH_DEBUG_PRINTF(
                        ("NvDdkBlockDevPartitionAllocationType_Absolute::"
                         "StartPhysicalSectorAddress[%d], NumLogicalSectors[%d]..\n",
                         pAllocPartIn->StartPhysicalSectorAddress,
                         pAllocPartIn->NumLogicalSectors));

                pAllocPartOut->StartLogicalSectorAddress =
                    pAllocPartIn->StartPhysicalSectorAddress;
                pAllocPartOut->NumLogicalSectors =
                    pAllocPartIn->NumLogicalSectors;
            }

            else if (pAllocPartIn->AllocationType ==
                    NvDdkBlockDevPartitionAllocationType_Relative)
            {
                NVUSBH_DEBUG_PRINTF(
                        ("NvDdkBlockDevPartitionAllocationType_Relative"
                         "StartPhysicalSectorAddress[%d], NumLogicalSectors[%d]..\n",
                         pAllocPartIn->StartPhysicalSectorAddress,
                         pAllocPartIn->NumLogicalSectors));

                pAllocPartOut->StartLogicalSectorAddress =
                    pUsbhBlkDev->PartitionAddr;
                /* Address needs to be block aligned */
                BlockCount =
                    pAllocPartIn->NumLogicalSectors/pUsbhBlkDev->SectorsPerBlock;
                if (BlockCount * pUsbhBlkDev->SectorsPerBlock !=
                        pAllocPartIn->NumLogicalSectors)
                    BlockCount++;
                pAllocPartOut->NumLogicalSectors =
                    BlockCount * pUsbhBlkDev->SectorsPerBlock;
                pUsbhBlkDev->PartitionAddr += pAllocPartOut->NumLogicalSectors;
                NVUSBH_DEBUG_PRINTF(
                        ("NvDdkBlockDevPartitionAllocationType_Relative: Partition allocated"
                         "StartLogicalSectorAddress[%d], NumLogicalSectors[%d]..\n",
                         pAllocPartOut->StartLogicalSectorAddress,
                         pAllocPartOut->NumLogicalSectors));
            }
            else
            {
                NVUSBH_DEBUG_PRINTF(("USBH Block driver, Invalid allocation type\n"));
                NV_ASSERT(!"USBH Block driver, Invalid allocation type");
            }
        }
        break;

        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
        {
/*            NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *ip =
                (NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *)InputArgs;

            NV_ASSERT(InputArgs);
            NV_ASSERT(InputSize ==
                    sizeof(NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs));

            NVUSBH_DEBUG_PRINTF(
                    ("NvDdkBlockDevIoctlType_EraseLogicalSectors..\n"));

            NV_ASSERT(ip->NumberOfLogicalSectors);

            NVUSBH_DEBUG_PRINTF(("NvDdkBlockDevIoctlType_EraseLogicalSectors \
                        StartLogicalSector[%d] NumberOfLogicalSectors[%d]\n",
                        ip->StartLogicalSector,
                        ip->NumberOfLogicalSectors));

            NvU8 *buffer = NvOsAlloc(ip->NumberOfLogicalSectors * BYTES_PER_BLOCK);
            e = scsiWrite(pUsbhBlkDev->Ctxt->hUsbHost,
                    pUsbhBlkDev->devInfo,
                    pUsbhBlkDev->ioHandle->inEndpoint,
                    pUsbhBlkDev->ioHandle->OutEndpoint,
                    pUsbhBlkDev->devInfo->MediaLun,
                    ip->StartLogicalSector,
                    ip->NumberOfLogicalSectors,
                    buffer);*/
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

        default:
        {
            NVUSBH_DEBUG_PRINTF(("USBH ERROR: Illegal block driver Ioctl..\n"));
            e = NvError_BlockDriverIllegalIoctl;
        }
        break;
    }

    if (e != NvSuccess)
    {
        NVUSBH_DEBUG_PRINTF(("USBH ERROR: UsbhBlockDevIoctl failed error[0x%x]..\n", e));
        // function to return name corresponding to ioctl opcode
        UtilGetIoctlName(Opcode, IoctlName);
        NvOsDebugPrintf("\r\nInst=%d, USBH ioctl %s failed: error code=0x%x ",
                pUsbhBlkDev->Instance, IoctlName, e);
    }

#undef  MAX_IOCTL_STRING_LENGTH_BYTES

    return e;
}

static NvError
UsbhBlockDevOpen(
        NvU32 Instance,
        NvU32 MinorInstance,
        UsbhBlockDev** hUsbhBlkDev)
{
    NvError err = NvSuccess;
    UsbhBlockDev *pUsbhBlkDev;

    NvU8 i;
    NvU32 portEvents = 0;
    NvU32 blkInEpHndle,blkOutEpHndle;
    NvDdkUsbhPipeInfo PipeInfo;
    NvU8 blkInEndptAddr, blkOutEndptAddr;
    DeviceInfo *pdevInfo = NvOsAlloc(sizeof(DeviceInfo));
    UsbHostContext *Ctxt = NvOsAlloc(sizeof(UsbHostContext));
    IOEndpointHandle ioHandle = NvOsAlloc(sizeof(IOEndpoint));

    NV_ASSERT(Instance < s_UsbhBlockDevInfo.MaxInstances);
    NvOsDebugPrintf("Line: %d func: %s \n", __LINE__, __func__);

    pUsbhBlkDev = &s_UsbhBlockDevInfo.UsbhBlockDevList[Instance];
    if(pUsbhBlkDev->isOpen) {
        *hUsbhBlkDev = pUsbhBlkDev;
        return NvSuccess;
    }

    NvOsMemset(ioHandle,0,sizeof(IOEndpointHandle));
    NvOsMemset(Ctxt,0,sizeof(UsbHostContext));
    NvOsMemset(pdevInfo,0,sizeof(DeviceInfo));

    pdevInfo->CBW_tag = 0x11223344;
    pdevInfo->DevAddr = 1;
    pdevInfo->MediaLun = 0xFF;
    err = NvRmOpenNew(&Ctxt->RmHandle);
    if(err != NvSuccess) {
        NVUSBH_DEBUG_PRINTF(("NvDdkUsbh_Test>NvRmOpen Failed\n"));
    } else
    {
        NVUSBH_DEBUG_PRINTF(("NvDdkUsbh_Test>NvRmOpen Success\n"));
    }

    err = NvOsSemaphoreCreate(&Ctxt->Semaphore, 0);
    if (err != NvSuccess)
    {
        NVUSBH_DEBUG_PRINTF(("NvDdkUsbh_Test>NvOsSemaphoreCreate Failed\n"));
    }
    NvOsDebugPrintf("Line: %d func: %s \n", __LINE__, __func__);

    err = NvDdkUsbhOpen(
            Ctxt->RmHandle,
            &(Ctxt->hUsbHost),
            Ctxt->Semaphore, Instance);

    if(err!=NvSuccess) {
        NVUSBH_DEBUG_PRINTF(("NvDdkUsbhOpen Failed\n"));
    } else
        NVUSBH_DEBUG_PRINTF(("NvDdkUsbhOpen Success\n"));

    err = NvDdkUsbhStart(Ctxt->hUsbHost, NV_TRUE);
    if(err == NvDdkUsbhEvent_CableDisConnect)
    {
        NVUSBH_DEBUG_PRINTF(("Connect usb MSD device \n"));
        do
        {
            delayfunction(10,100);
            err = NvDdkUsbhStart(Ctxt->hUsbHost,NV_TRUE);
        }while (err == NvDdkUsbhEvent_CableDisConnect);
    }
    if (err != NvSuccess)
    {
        NVUSBH_DEBUG_PRINTF(("NvDdkUsbhStart Failed\n"));
        goto ErrorHandle;
    }


    do
    {
        delayfunction(10, 200);
        portEvents = NvDdkUsbhGetEvents(Ctxt->hUsbHost, NULL);
    } while (!(portEvents & NvDdkUsbhEvent_PortEnabled));

    if (portEvents & NvDdkUsbhEvent_CableConnect)
        NVUSBH_DEBUG_PRINTF(("Received cable connect event\n"));

    if (portEvents & NvDdkUsbhEvent_PortEnabled)
        NVUSBH_DEBUG_PRINTF(("Received Port Enable Event\n"));

    // Cable has been detected then now enumerate the device
    NVUSBH_DEBUG_PRINTF(("Enumerating Device\n"));

    err = EnumerateDevice(Ctxt->hUsbHost, pdevInfo);
    if(err!=NvSuccess) {
        NVUSBH_DEBUG_PRINTF(("Enumeration Failed\n"));
    }else
        NVUSBH_DEBUG_PRINTF(("Enumeration completed successfully\n"));

    NVUSBH_DEBUG_PRINTF(("Creating in and out Endpoint Handles\n"));

    blkInEndptAddr   = pdevInfo->InEndpoint.BEndpointAddress & EP_ADDR_MASK;
    blkOutEndptAddr = pdevInfo->OutEndpoint.BEndpointAddress & EP_ADDR_MASK;

    // Adding Out pipe
    PipeInfo.DeviceAddr = pdevInfo->DevAddr;
    PipeInfo.EndpointType = NvDdkUsbhPipeType_Bulk;
    PipeInfo.EpNumber = blkOutEndptAddr;
    PipeInfo.PipeDirection = NvDdkUsbhDir_Out;
    PipeInfo.MaxPktLength =  (pdevInfo->OutEndpoint).BMaxPacketSize;
    NvDdkUsbhGetPortSpeed(Ctxt->hUsbHost, &PipeInfo.SpeedOfDevice);
    NvDdkUsbhAddPipe(Ctxt->hUsbHost, &PipeInfo);
    blkOutEpHndle = PipeInfo.EpPipeHandler;

    //Adding In pipe
    PipeInfo.DeviceAddr = pdevInfo->DevAddr;
    PipeInfo.EndpointType = NvDdkUsbhPipeType_Bulk;
    PipeInfo.EpNumber = blkInEndptAddr;
    PipeInfo.PipeDirection = NvDdkUsbhDir_In;
    PipeInfo.MaxPktLength = (pdevInfo->InEndpoint).BMaxPacketSize;
    NvDdkUsbhGetPortSpeed(Ctxt->hUsbHost, &PipeInfo.SpeedOfDevice);
    NvDdkUsbhAddPipe(Ctxt->hUsbHost, &PipeInfo);
    blkInEpHndle = PipeInfo.EpPipeHandler;

    ioHandle->OutEndpoint = blkOutEpHndle;
    ioHandle->inEndpoint = blkInEpHndle;

    // MSC device..
    //obtain media specific information for each lun
    for (i = 0; i <= pdevInfo->MaxLuns; i++)
    {
        err = GetMediaInfo
            (Ctxt->hUsbHost, pdevInfo, blkInEpHndle, blkOutEpHndle, i);
        if (err == NvSuccess)
        {
            if(pdevInfo->MediaLun == 0xFF)
                pdevInfo->MediaLun = i;
            pdevInfo->MediaPresent++;
            NVUSBH_DEBUG_PRINTF(("Media present on lun %d\n", i));
        }
    }

    NVUSBH_DEBUG_PRINTF(("Number of accessible Media present on device %d\n",
                pdevInfo->MediaPresent));

    pUsbhBlkDev->Ctxt = Ctxt;
    pUsbhBlkDev->devInfo = pdevInfo;
    pUsbhBlkDev->ioHandle = ioHandle;
    pUsbhBlkDev->PartitionAddr = 0;
    pUsbhBlkDev->Instance = Instance;
    pUsbhBlkDev->MinorInstance = MinorInstance;
    pUsbhBlkDev->isOpen = NV_TRUE;
    pUsbhBlkDev->SectorsPerBlock = 1;

    *hUsbhBlkDev = pUsbhBlkDev;

    NVUSBH_DEBUG_PRINTF(("USB Block Device Open successful\n"));
    return err;

ErrorHandle:
    NvRmClose(Ctxt->RmHandle);
    if (Ctxt->hUsbHost)
    {
        NvDdkUsbhClose(Ctxt->hUsbHost, NV_TRUE);
    }
    NvOsFree((void *)Ctxt);
    NvOsFree((void *)pdevInfo);
    return err;
}

/***************** Public Functions ************************/

NvError NvDdkUsbhBlockDevInit(NvRmDeviceHandle hDevice)
{

    NvError e = NvSuccess;

    NvOsDebugPrintf("NvDdkUsbhBlockDevInit..\n");

    NV_ASSERT(hDevice);

    if (!s_UsbhBlockDevInfo.IsInitialized)
    {
        s_UsbhBlockDevInfo.MaxInstances =
            NvRmModuleGetNumInstances(hDevice, NvRmModuleID_Usb2Otg);

        if (s_UsbhBlockDevInfo.MaxInstances)
            s_UsbhBlockDevInfo.IoModule = NvOdmIoModule_Usb;

        s_UsbhBlockDevInfo.UsbhBlockDevList = NvOsAlloc(
                sizeof(UsbhBlockDev) * s_UsbhBlockDevInfo.MaxInstances);
        if (!s_UsbhBlockDevInfo.UsbhBlockDevList)
        {
            e = NvError_InsufficientMemory;
            NvOsDebugPrintf
                ("USBH ERROR: NvDdkUsbhBlockDevInit failed error[0x%x]..\n", e);
            goto fail;
        }

        NvOsMemset((void *)s_UsbhBlockDevInfo.UsbhBlockDevList, 0,
                (sizeof(UsbhBlockDev) * s_UsbhBlockDevInfo.MaxInstances));

        /* Save the RM Handle */
        s_UsbhBlockDevInfo.hRm = hDevice;
        s_UsbhBlockDevInfo.IsInitialized = NV_TRUE;
    }

fail:
    return e;
}

void NvDdkUsbhBlockDevDeinit(void)
{

    if (s_UsbhBlockDevInfo.IsInitialized)
    {
        NvOsFree(s_UsbhBlockDevInfo.UsbhBlockDevList);
        s_UsbhBlockDevInfo.UsbhBlockDevList = NULL;
        s_UsbhBlockDevInfo.hRm = NULL;
        s_UsbhBlockDevInfo.MaxInstances = 0;
        s_UsbhBlockDevInfo.IsInitialized = NV_FALSE;
    }
}

NvError NvDdkUsbhBlockDevOpen(
        NvU32 Instance,
        NvU32 MinorInstance,
        NvDdkBlockDevHandle *phBlockDev)
{
    NvError e = NvSuccess;
    UsbhBlockDev *hUsbhBlkDev = NULL;

    NV_ASSERT(Instance < s_UsbhBlockDevInfo.MaxInstances);
    NV_ASSERT(phBlockDev);
    NV_ASSERT(s_UsbhBlockDevInfo.IsInitialized);
    NV_CHECK_ERROR_CLEANUP(
            UsbhBlockDevOpen(Instance, MinorInstance,&hUsbhBlkDev));

    hUsbhBlkDev->BlockDev.NvDdkBlockDevClose = &UsbhBlockDevClose;
    hUsbhBlkDev->BlockDev.NvDdkBlockDevGetDeviceInfo =
                                    &UsbhBlockDevGetDeviceInfo;
    hUsbhBlkDev->BlockDev.NvDdkBlockDevReadSector = &UsbhBlockDevRead;
    hUsbhBlkDev->BlockDev.NvDdkBlockDevWriteSector = &UsbhBlockDevWrite;
    hUsbhBlkDev->BlockDev.NvDdkBlockDevPowerUp = &UsbhBlockDevPowerUp;
    hUsbhBlkDev->BlockDev.NvDdkBlockDevPowerDown = &UsbhBlockDevPowerDown;
    hUsbhBlkDev->BlockDev.NvDdkBlockDevIoctl = &UsbhBlockDevIoctl;
    hUsbhBlkDev->BlockDev.NvDdkBlockDevRegisterHotplugSemaphore =
        &UsbhBlockDevRegisterHotplugSemaphore;
    hUsbhBlkDev->BlockDev.NvDdkBlockDevFlushCache = &UsbhBlockDevFlushCache;

    *phBlockDev = &(hUsbhBlkDev->BlockDev);

fail:
    return e;
}

