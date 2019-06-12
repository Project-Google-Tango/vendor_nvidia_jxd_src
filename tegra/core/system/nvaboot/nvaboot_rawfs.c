/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** nvaboot_rawfs.c
 *
 * NvAboot support for reading and writing raw data to NV storage manager
 * partitions on block devices.
 */

#include "nvcommon.h"
#include "crc32.h"
#include "nvpartmgr.h"
#include "nvaboot_blockdev_nice.h"
#include "nverror.h"
#include "nvaboot_private.h"
#include "nvos.h"
#include "nvrm_module.h"
#include "nvfsmgr.h"
#include "nvbl_query.h"

#define MBR_SIZE 512
#define EBR_SIZE 512
#define MBR_PART_RECORD_OFFSET 446
#define EBR_PART_RECORD_OFFSET 446
#define DISK_BLOCK_SIZE 512
#define MAX_PRIMARY_MBR_PARTITIONS 4
#define MAX_EBR_PARTITIONS 100
#define GPT_BLOCK_SIZE 512
#define GPT_HEADER_SIGNATURE 0x5452415020494645ULL
#define GPT_HEADER_REVISION_V1 0x00010000
#define GPT_PRIMARY_PARTITION_TABLE_LBA 1
#define GPT_PART_NAME_LENGTH 16
#define GPT_UUID_LENGTH 16
#define GPT_HEADER_SIZE 0x5c
#define GPT_MAX_PART_ENTRIES 128
#define GPT_SIGN_OFFSET_FROM_END_BUFF 512

char Ebr[EBR_SIZE];
char Mbr[MBR_SIZE];

typedef enum
{
    Ebr_Not_Found,
    Ebr_Header_Found,
    Ebr_Data_Found,
    Ebr_Done,
    Ebr_Force32 = 0x7fffffffUL,
} EbrStatus;

typedef struct PrimaryPartRec
{
    NvU8 status;
    NvU8 head_f;
    NvU8 sector_f;
    NvU8 cyl_f;
    NvU8 part_type;
    NvU8 head_l;
    NvU8 sector_l;
    NvU8 cyl_l;
    NvU32 lba_f;
    NvU32 num_blocks;
} PrimaryPartitionRecord;

typedef struct LogicalPartRec
{
    NvU8 status;
    NvU8 head_f;
    NvU8 sector_f;
    NvU8 cyl_f;
    NvU8 part_type;
    NvU8 head_l;
    NvU8 sector_l;
    NvU8 cyl_l;
    NvU32 lba_f;
    NvU32 num_blocks;
} LogicalPartitionRecord;

PrimaryPartitionRecord PartRecord;
LogicalPartitionRecord FirstEntry, SecondEntry;
typedef struct {
    NvU8 b[16];
} efi_guid_t;


#define EFI_GUID(a,b,c,d0,d1,d2,d3,d4,d5,d6,d7) \
{ (a) & 0xff, ((a) >> 8) & 0xff, ((a) >> 16) & 0xff, ((a) >> 24) & 0xff, \
  (b) & 0xff, ((b) >> 8) & 0xff, \
  (c) & 0xff, ((c) >> 8) & 0xff, \
  (d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7) }


#define PARTITION_SYSTEM_GUID(x) \
    NvU8 x[16]  = EFI_GUID( 0xC12A7328, 0xF81F, 0x11d2, \
              0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B)
#define LEGACY_MBR_PARTITION_GUID(x) \
    NvU8 x[16]  = EFI_GUID( 0x024DEE41, 0x33E7, 0x11d3, \
              0x9D, 0x69, 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F)
#define PARTITION_MSFT_RESERVED_GUID(x) \
    NvU8 x[16]  = EFI_GUID( 0xE3C9E316, 0x0B5C, 0x4DB8, \
              0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE)
#define PARTITION_BASIC_DATA_GUID(x) \
    NvU8 x[16]  = EFI_GUID( 0xEBD0A0A2, 0xB9E5, 0x4433, \
              0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7)
#define PARTITION_LINUX_RAID_GUID(x) \
    NvU8 x[16]  = EFI_GUID( 0xa19d880f, 0x05fc, 0x4d3b, \
              0xa0, 0x06, 0x74, 0x3f, 0x0f, 0x84, 0x91, 0x1e)
#define PARTITION_LINUX_SWAP_GUID(x) \
    NvU8 x[16]  = EFI_GUID( 0x0657fd6d, 0xa4ab, 0x43c4, \
              0x84, 0xe5, 0x09, 0x33, 0xc8, 0x4b, 0x4f, 0x4f)
#define PARTITION_LINUX_LVM_GUID(x) \
    NvU8 x[16]  = EFI_GUID( 0xe6d6d379, 0xf507, 0x44c2, \
              0xa2, 0x3c, 0x23, 0x8f, 0x2a, 0x3d, 0xf9, 0x28)

typedef struct GptHeaderRec {
        NvU64 signature;
        NvU32 revision;
        NvU32 header_size;
        NvU32 header_crc32;
        NvU32 reserved1;
        NvU64 my_lba;
        NvU64 alternate_lba;
        NvU64 first_usable_lba;
        NvU64 last_usable_lba;
        efi_guid_t disk_guid;
        NvU64 partition_entry_lba;
        NvU32 num_partition_entries;
        NvU32 sizeof_partition_entry;
        NvU32 partition_entry_array_crc32;
        NvU8 reserved2[GPT_BLOCK_SIZE - 92];
}  GptHeaderRecord;

typedef struct GptEntryAttributesRec {
        NvU64 required_to_function:1;
        NvU64 reserved:47;
        NvU64 type_guid_specific:16;
}  GptEntryAttributesRecord;

typedef struct GptEntryRec {
        efi_guid_t partition_type_guid;
        efi_guid_t unique_partition_guid;
        NvU64 starting_lba;
        NvU64 ending_lba;
        GptEntryAttributesRecord attributes;
        NvS16 partition_name[72 / sizeof (NvS16)];
} GptEntryRecord;
//PACKED

GptHeaderRecord     g_GptH1;
GptHeaderRecord     g_GptH2;
GptEntryRecord      g_GptEntries[128];
NvU32               Extra_Blocks_End;// extra blocks at the end which is not multiple of 2 MB a


static NvU32 GetPartitionByType(NvU32 PartitionType)
{
    NvU32 PartitionId = 0;
    NvError e=NvSuccess;
    NvPartInfo PartitionInfo;
    while(1)
    {
        PartitionId = NvPartMgrGetNextId(PartitionId);
        if(PartitionId == 0)
            return PartitionId;
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionId, &PartitionInfo));
        if(PartitionInfo.PartitionType == PartitionType)
            break;
    }
    return PartitionId;
fail:
    return 0;
}


static NvError FillGPT1Header(NvAbootHandle hAboot, NvU32 SectorSize)
{
    NvError e = NvSuccess;
    NvU32 PartitionIdGp1;
    NvU32 PartitionId;
    NvU64 PartitionStart, PartitionSize;
    NvU32 TempSectorSize;
    NvU8 pUuid[GPT_UUID_LENGTH];
    NvU32 PartitionIdGpt2 = 0;

    //signature
    g_GptH1.signature = GPT_HEADER_SIGNATURE;
    //92 bytes header size
    g_GptH1.header_size = GPT_HEADER_SIZE;
    g_GptH1.revision = 0x10000;
    //CRC for header 1 assigned later
    g_GptH1.header_crc32 = 0x00;
    //reserver byte set to 0
    g_GptH1.reserved1 = 0x00;

    if((PartitionIdGp1 = GetPartitionByType(NvPartMgrPartitionType_GP1)) == 0)
        return NvError_BadParameter;
    NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
            PartitionIdGp1, &PartitionStart, &PartitionSize, &TempSectorSize));

    //location of this header copy
    g_GptH1.my_lba = (PartitionStart + PartitionSize) * (SectorSize / GPT_BLOCK_SIZE) - 1;

    //Partition entries starting LBA (always 2 in primary copy)
    g_GptH1.partition_entry_lba = g_GptH1.my_lba - (128 * 128) / GPT_BLOCK_SIZE;

    if((PartitionIdGpt2 = GetPartitionByType(NvPartMgrPartitionType_GPT)) == 0)
        return NvError_BadParameter;
    NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
            PartitionIdGpt2, &PartitionStart, &PartitionSize, &TempSectorSize));

    //backup LBA location of other header copy. End of device -1 sector
    g_GptH1.alternate_lba = (PartitionStart + PartitionSize + Extra_Blocks_End) * (SectorSize / GPT_BLOCK_SIZE) - 1;

    //Last usable LBA (secondary partition table first LBA - 1)
    //GPT partition start -1 sector address
    g_GptH1.last_usable_lba = PartitionStart * (SectorSize / GPT_BLOCK_SIZE) - 1;

    //NV_CHECK_ERROR_CLEANUP(NvPartMgrGetIdByName("GP1", &PartitionIdGp1));
    if((PartitionIdGp1=GetPartitionByType(NvPartMgrPartitionType_GP1)) == 0)
        return NvError_BadParameter;
    PartitionId = NvPartMgrGetNextId(PartitionIdGp1);
    if (PartitionId == 0)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
            PartitionId, &PartitionStart, &PartitionSize, &TempSectorSize));

    //First usable LBA for partitions (primary partition table last LBA + 1)
    //first sector of partition after GP1
    g_GptH1.first_usable_lba = PartitionStart * (SectorSize / GPT_BLOCK_SIZE) ;

    //Disk GUID
    NV_CHECK_ERROR_CLEANUP(
        NvRmGetRandomBytes(hAboot->hRm, 16, pUuid)
    );
    NvOsMemcpy((NvU8*)&g_GptH1.disk_guid, (NvU8*)&g_GptH2.disk_guid, sizeof(pUuid));

    //num of partition entries
    //g_GptH1.num_partition_entries = GetPartEntriesCount();
    //size of partition entries
    g_GptH1.sizeof_partition_entry = GPT_MAX_PART_ENTRIES;

fail:
    return e;
}

static NvError FillGPT2Header(NvAbootHandle hAboot, NvU32 SectorSize)
{
    NvError e = NvSuccess;
    NvU32 PartitionIdGp1;
    NvU32 PartitionIdGpt2;
    NvU32 PartitionId = 0;
    NvU64 PartitionStart, PartitionSize;
    NvU32 TempSectorSize;
    NvU8 pUuid[GPT_UUID_LENGTH];
    NvFsMountInfo FsMountInfo;
    NvDdkBlockDevHandle hBlockDevHandle;
    NvDdkBlockDevInfo BlockDevInfo;
    NvU32 PartitionIdGPT;

    //signature
    g_GptH2.signature = GPT_HEADER_SIGNATURE;
    //92 bytes header size
    g_GptH2.header_size = GPT_HEADER_SIZE;
    g_GptH2.revision = 0x10000;
    //CRC for header 2 assigned later
    g_GptH2.header_crc32 = 0x00;
    //reserver byte set to 0
    g_GptH2.reserved1 = 0x00;

    // code to get the TotalSector (Blocks 4096) of secondary media
    if ((PartitionIdGPT = GetPartitionByType(NvPartMgrPartitionType_GPT)) == 0)
        return NvError_BadParameter;

      NvPartMgrGetFsInfo(PartitionIdGPT, &FsMountInfo);

    // open the media block driver where the partition table resides
    e = NvDdkBlockDevMgrDeviceOpen(
        (NvDdkBlockDevMgrDeviceId)FsMountInfo.DeviceId,
        FsMountInfo.DeviceInstance, PartitionId, &hBlockDevHandle);

    // Get the device info
    (hBlockDevHandle)->NvDdkBlockDevGetDeviceInfo(
                             hBlockDevHandle,
                             &BlockDevInfo);
    if (FsMountInfo.DeviceId == NvDdkBlockDevMgrDeviceId_Sata)
    {
        // There are no extra blocks on sata
        Extra_Blocks_End = 0;
    }
    else
    {
        // get the no of sectors which are not multiple of 512
        Extra_Blocks_End = (BlockDevInfo.TotalSectors % GPT_BLOCK_SIZE);
    }

    if((PartitionIdGp1 = GetPartitionByType(NvPartMgrPartitionType_GP1)) == 0)
        return NvError_BadParameter;
    NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
            PartitionIdGp1, &PartitionStart, &PartitionSize, &TempSectorSize));

    //backup LBA location of first header copy
    g_GptH2.alternate_lba =  (PartitionStart + PartitionSize) * (SectorSize / GPT_BLOCK_SIZE) - 1;

    if((PartitionIdGpt2 = GetPartitionByType(NvPartMgrPartitionType_GPT)) == 0)
        return NvError_BadParameter;
    NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
            PartitionIdGpt2, &PartitionStart, &PartitionSize, &TempSectorSize));

    //location of this header copy. End of device
    g_GptH2.my_lba= (PartitionStart + PartitionSize + Extra_Blocks_End) * (SectorSize / GPT_BLOCK_SIZE) - 1;

    //Last usable LBA (secondary partition table first LBA - 1)
    //GPT partition start -1 sector address
    g_GptH2.last_usable_lba = PartitionStart * (SectorSize / GPT_BLOCK_SIZE) - 1;

    //NV_CHECK_ERROR_CLEANUP(NvPartMgrGetIdByName("GP1", &PartitionIdGp1));
    if((PartitionIdGp1=GetPartitionByType(NvPartMgrPartitionType_GP1)) == 0)
        return NvError_BadParameter;
    PartitionId = NvPartMgrGetNextId(PartitionIdGp1);
    if (PartitionId == 0)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
            PartitionId, &PartitionStart, &PartitionSize, &TempSectorSize));

    //First usable LBA for partitions (primary partition table last LBA + 1)
    //first sector of partition after MBR partition
    g_GptH2.first_usable_lba = PartitionStart * (SectorSize / GPT_BLOCK_SIZE) ;

    //Disk GUID
    NV_CHECK_ERROR_CLEANUP(
        NvRmGetRandomBytes(hAboot->hRm, 16, pUuid));

    NvOsMemcpy((NvU8*)&g_GptH2.disk_guid, pUuid, sizeof(pUuid));
    //partition count
    //g_GptH2.num_partition_entries = GetPartEntriesCount();
    //size of partition entries
    g_GptH2.sizeof_partition_entry = GPT_MAX_PART_ENTRIES;
    g_GptH2.partition_entry_lba = g_GptH2.my_lba - (128 * 128) / GPT_BLOCK_SIZE;

fail:
    return e;
}

static NvError FillGptPartitionDetails(NvAbootHandle hAboot, NvU32 SectorSize)
{
    NvError e = NvSuccess;
    NvU32   PartitionId;
    char PartName[NVPARTMGR_PARTITION_NAME_LENGTH];
    NvU8 pUuid[GPT_UUID_LENGTH];
    NvU32 CrcTableEntries = 0;
    NvU32 i =0;
    NvU64 PartitionStart, PartitionSize;
    NvU32 TempSectorSize;
    NvU32 PartCount = 0;
    NvU32 len = 0;
    NvU32 j = 0;
    NvPartInfo PartitionInfo;

    PARTITION_BASIC_DATA_GUID(temp);

    NvOsMemset(g_GptEntries, 0, sizeof(g_GptEntries));
    //NV_CHECK_ERROR_CLEANUP(NvPartMgrGetIdByName("GP1", &PartitionId));
    if((PartitionId=GetPartitionByType(NvPartMgrPartitionType_GP1)) == 0)
        return NvError_BadParameter;

    //fill in the entries of partition after MBR partition
    //for( i =0; i<PartCount; i++)
    while(1)
    {
        PartitionId = NvPartMgrGetNextId(PartitionId);
        if(PartitionId  == 0)
            break;
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionId, &PartitionInfo));
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetNameById(PartitionId, PartName));

        NvAbootGetPartitionParametersbyId(hAboot, PartitionId,
            &PartitionStart, &PartitionSize, &TempSectorSize);

        g_GptEntries[i].starting_lba = (PartitionStart ) * (SectorSize /GPT_BLOCK_SIZE);
        g_GptEntries[i].ending_lba = (PartitionStart + PartitionSize) * (SectorSize /GPT_BLOCK_SIZE) -1;

        // prevent partition from overlapping with secondary GPT header and partition table
        if (hAboot->BlockDevId != NvDdkBlockDevMgrDeviceId_SDMMC)
        {
            if (g_GptEntries[i].starting_lba >= g_GptH2.partition_entry_lba)
            {
                g_GptEntries[i].starting_lba = 0;
                g_GptEntries[i].ending_lba = 0;
                break;
            }
            if (g_GptEntries[i].ending_lba >= g_GptH2.partition_entry_lba)
                g_GptEntries[i].ending_lba = g_GptH2.partition_entry_lba - 1;
        }

        //convert to unicode char
        len = NvOsStrlen(PartName);
        for (j = 0; j <= len; j++)
            g_GptEntries[i].partition_name[j] = PartName[j];
        NvOsMemcpy(g_GptEntries[i].partition_type_guid.b, temp, sizeof(g_GptEntries[i].partition_type_guid.b));
        NV_CHECK_ERROR_CLEANUP(
        NvRmGetRandomBytes(hAboot->hRm, 16, pUuid)
        );
        NvOsMemcpy((NvU8*)&g_GptEntries[i].unique_partition_guid, pUuid, sizeof(pUuid));
        i = i +1;
        //attribute to do
    }
    //partition count
    if (hAboot->BlockDevId == NvDdkBlockDevMgrDeviceId_SDMMC)
    {
        g_GptH1.num_partition_entries = i;//GetPartEntriesCount();
        g_GptH2.num_partition_entries = i;//GetPartEntriesCount();
        PartCount = i;
    }
    else
    {
        g_GptH1.num_partition_entries = GPT_MAX_PART_ENTRIES;
        g_GptH2.num_partition_entries = GPT_MAX_PART_ENTRIES;
        PartCount = GPT_MAX_PART_ENTRIES;
    }
    //calculate CRC for table entries
    CrcTableEntries = NvComputeCrc32(0, (const NvU8 *)&g_GptEntries[0], sizeof(GptEntryRecord)*PartCount);

    //fill in table entries CRC in GPT header1 and header2
    g_GptH1.partition_entry_array_crc32 = CrcTableEntries;
    g_GptH2.partition_entry_array_crc32 = CrcTableEntries;

    //calculate CRC for GPT header1 and header 2
    g_GptH1.header_crc32 =0;
    g_GptH2.header_crc32 =0;
    g_GptH1.header_crc32 = NvComputeCrc32(0, (const NvU8 *)&g_GptH1, g_GptH1.header_size);
    g_GptH2.header_crc32 = NvComputeCrc32(0, (const NvU8 *)&g_GptH2, g_GptH2.header_size);

fail:
    return e;

}

static NvError WriteGptPartitiontable(NvAbootHandle hAboot, NvU32 DeviceId,
                                      NvU32 DeviceInstance, NvU32 SectorSize)
{
    NvError e = NvSuccess;
    NvU32 offset = MBR_PART_RECORD_OFFSET;
    NvU8 *buff = NULL;
    NvU32 size = 0, GptPartitionId = 0, Gp1PartitionId = 0;
    NvU32 SectorToWrite = 0;
    NvU64 LogicalStartSector;
    NvPartInfo PartitionGpt, PartitionGp1;
    NvDdkBlockDevHandle hDev = NULL;
    NvU32 SizeOfData;

    NvOsMemset(&g_GptH1, 0, sizeof(g_GptH1));
    NvOsMemset(&g_GptH2, 0, sizeof(g_GptH2));

    NV_CHECK_ERROR_CLEANUP(FillGPT2Header(hAboot, SectorSize));
    NV_CHECK_ERROR_CLEANUP(FillGPT1Header(hAboot, SectorSize));
    NV_CHECK_ERROR_CLEANUP(FillGptPartitionDetails(hAboot, SectorSize));

    size = sizeof(Mbr) + sizeof(g_GptH1) + sizeof(g_GptEntries);
    size = (size + SectorSize -1) / SectorSize;
    buff = NvOsAlloc(size * SectorSize);
    if(!buff)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    //write Entries and GPT2
    NvOsMemset(buff,0, sizeof(buff));
    //NV_CHECK_ERROR_CLEANUP(NvPartMgrGetIdByName("GPT", &GptPartitionId));
    if((GptPartitionId=GetPartitionByType(NvPartMgrPartitionType_GPT)) == 0)
        return NvError_BadParameter;
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(GptPartitionId, &PartitionGpt));

    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen(DeviceId,
            DeviceInstance, GptPartitionId, &hDev)
    );

    SizeOfData = sizeof(g_GptH2) + sizeof(g_GptEntries);
    offset = (size * SectorSize) - SizeOfData;
    NvOsMemset(buff, 0, offset);
    NvOsMemcpy(buff + offset, g_GptEntries, sizeof(g_GptEntries));
    NvOsMemcpy(buff + offset + sizeof(g_GptEntries), &g_GptH2, sizeof(g_GptH2));
    SectorToWrite = size;
    LogicalStartSector = (NvU32)(PartitionGpt.StartLogicalSectorAddress + Extra_Blocks_End + PartitionGpt.NumLogicalSectors - SectorToWrite);
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevWriteSector(hDev, LogicalStartSector, buff, SectorToWrite, NV_TRUE)
    );

   if((GptPartitionId=GetPartitionByType(NvPartMgrPartitionType_GP1)) == 0)
        return NvError_BadParameter;
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(GptPartitionId, &PartitionGpt));

    NvOsMemset(buff,0, sizeof(buff));
    SizeOfData = sizeof(g_GptH1) + sizeof(g_GptEntries);
    offset = (size * SectorSize) - SizeOfData;
    NvOsMemset(buff, 0, offset);
    NvOsMemcpy(buff + offset, g_GptEntries, sizeof(g_GptEntries));
    NvOsMemcpy(buff + offset + sizeof(g_GptEntries), &g_GptH1, sizeof(g_GptH1));
    SectorToWrite = size;
    LogicalStartSector = (NvU32)(PartitionGpt.StartLogicalSectorAddress + PartitionGpt.NumLogicalSectors - SectorToWrite);

    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevWriteSector(hDev, (NvU32)LogicalStartSector, buff, SectorToWrite, NV_TRUE)
    );

    // write GP1
    PartRecord.sector_f = 0x2; // CHS address of first absolute sector in partition (counting starts with 1)
    PartRecord.part_type = 0xee; // type for protective MBR
    PartRecord.head_l = 0xff; // CHS address of last absolute sector in partition (0xff for entire disk)
    PartRecord.sector_l = 0xff;
    PartRecord.cyl_l = 0xff;
    PartRecord.lba_f = 0x1; // LBA of first absolute sector in the partition
    PartRecord.num_blocks = PartitionGpt.StartLogicalSectorAddress*(SectorSize/DISK_BLOCK_SIZE) - 1;
    NvOsMemcpy(&Mbr[MBR_PART_RECORD_OFFSET], &PartRecord, sizeof(PartRecord));
    Mbr[510] = 0x55;
    Mbr[511] = 0xaa;
    NvOsMemset(buff, 0, size*SectorSize);
    NvOsMemcpy(buff, Mbr, sizeof(Mbr));
    NvOsMemcpy(buff + sizeof(Mbr), &g_GptH1, sizeof(g_GptH1));
    NvOsMemcpy(buff + sizeof(Mbr) + sizeof(g_GptH1), g_GptEntries, sizeof(g_GptEntries));
    if ((Gp1PartitionId = GetPartitionByType(NvPartMgrPartitionType_GP1)) == 0)
        return NvError_BadParameter;
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(Gp1PartitionId, &PartitionGp1));
    LogicalStartSector = PartitionGp1.StartLogicalSectorAddress;
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevWriteSector(hDev, (NvU32)LogicalStartSector, buff, SectorToWrite, NV_TRUE)
    );

fail:
    return e;
}

NvError NvAbootRawErasePartition(
    NvAbootHandle hAboot,
    const char *NvPartitionName)
{
    NvPartInfo Partition;
    NvU32      PartitionId;
    NvU8      *pZero = NULL;
    NvError    e;
    NvDdkBlockDevHandle hDev = NULL;
    NvDdkBlockDevInfo BdInfo;
    const NvU32 SectorsPerWrite = 16;

    if (!hAboot || !NvPartitionName)
        return NvError_BadParameter;

    NvOsMutexLock(hAboot->hStorageMutex);

    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetIdByName(NvPartitionName, &PartitionId));
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionId, &Partition));

    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen(hAboot->BlockDevId,
            hAboot->BlockDevInstance, PartitionId, &hDev)
    );

    NvDdkBlockDevGetDeviceInfo(hDev, &BdInfo);

    pZero = NvOsAlloc(BdInfo.BytesPerSector * SectorsPerWrite);
    NvOsMemset(pZero, 0xff, BdInfo.BytesPerSector * SectorsPerWrite);

    while (Partition.NumLogicalSectors)
    {
        NvU32 Remain = NV_MIN(SectorsPerWrite, Partition.NumLogicalSectors);

        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevWriteSector(hDev,
                Partition.StartLogicalSectorAddress, pZero, Remain, NV_TRUE)
        );

        Partition.StartLogicalSectorAddress += Remain;
        Partition.NumLogicalSectors -= Remain;
    }

 fail:
    NvDdkBlockDevClose(hDev);

    NvOsMutexUnlock(hAboot->hStorageMutex);

    if (pZero)
        NvOsFree(pZero);

    return e;
}

NvError NvAbootRawWritePartition(
    NvAbootHandle hAboot,
    const char   *NvPartitionName,
    const NvU8   *pBuff,
    NvU64         BuffSize,
    NvU64         PartitionOffset)
{
    NvPartInfo Partition;
    NvU32      PartitionId;
    NvU32      NumBlocks;
    NvU32      StartBlock;
    NvU8      *pTempSector = NULL;
    NvError    e;
    NvDdkBlockDevHandle hDev = NULL;
    NvDdkBlockDevInfo BdInfo;

    if (!hAboot || !NvPartitionName)
        return NvError_BadParameter;

    NvOsMutexLock(hAboot->hStorageMutex);

    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetIdByName(NvPartitionName, &PartitionId));
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionId, &Partition));

    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen(hAboot->BlockDevId,
            hAboot->BlockDevInstance, PartitionId, &hDev)
    );

    NvDdkBlockDevGetDeviceInfo(hDev, &BdInfo);
    if (BdInfo.BytesPerSector & (BdInfo.BytesPerSector-1))
    {
        e = NvError_NotSupported;
        goto fail;
    }

    StartBlock = NvDiv64(PartitionOffset, BdInfo.BytesPerSector);
    StartBlock += Partition.StartLogicalSectorAddress;
    if ((NvU64)Partition.NumLogicalSectors * (NvU64)BdInfo.BytesPerSector <=
        PartitionOffset + BuffSize)
    {
        e = NvError_WriterFileSizeLimitExceeded;
        goto fail;
    }

    /*  If the star and/or end address isn't aligned to the underlying sector
     *  size, perform a copy-back of the sector to ensure that no data
     *  corruption occurs */
    if (PartitionOffset & (BdInfo.BytesPerSector-1))
    {
        NvU32 Bytes = PartitionOffset & (BdInfo.BytesPerSector-1);
        Bytes = BdInfo.BytesPerSector - Bytes;
        Bytes = NV_MIN(Bytes, BuffSize);

        pTempSector = NvOsAlloc(BdInfo.BytesPerSector);
        if (!pTempSector)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        NvOsMemset(pTempSector, 0, BdInfo.BytesPerSector);
        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevReadSector(hDev, StartBlock, pTempSector, 1, NV_TRUE)
        );
        NvOsMemcpy(&pTempSector[PartitionOffset & (BdInfo.BytesPerSector-1)],
            pBuff, Bytes);
        pBuff += Bytes;
        BuffSize -= Bytes;
        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevWriteSector(hDev, StartBlock, pTempSector, 1, NV_TRUE)
        );
        StartBlock++;
    }

    NumBlocks = NvDiv64(BuffSize, BdInfo.BytesPerSector);

    if (BuffSize & (BdInfo.BytesPerSector-1))
    {
        NvU32 LastBlock = StartBlock + NumBlocks;

        if (!pTempSector)
            pTempSector = NvOsAlloc(BdInfo.BytesPerSector);

        if (!pTempSector)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        NvOsMemset(pTempSector, 0, BdInfo.BytesPerSector);

        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevReadSector(hDev, LastBlock, pTempSector, 1, NV_TRUE)
        );

        NvOsMemcpy(pTempSector,
            &pBuff[(NvU64)NumBlocks*(NvU64)BdInfo.BytesPerSector],
            BuffSize & (BdInfo.BytesPerSector-1));

        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevWriteSector(hDev, LastBlock, pTempSector, 1, NV_TRUE)
        );

    }

    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevWriteSector(hDev, StartBlock, pBuff, NumBlocks, NV_TRUE)
    );

 fail:
    NvDdkBlockDevClose(hDev);

    NvOsMutexUnlock(hAboot->hStorageMutex);

    if (pTempSector)
        NvOsFree(pTempSector);
    return e;
}

static NvError NvAbootRawWriteEBRPartition(
    NvAbootHandle hAboot,
    NvU32 DeviceId,
    NvU32 DeviceInstance,
    NvU64 ExtendedPhysicalStartSector,
    NvU32 EBRPartitionId,
    NvU32 NextEbrPartitionId)
{
    NvPartInfo Partition;
    NvU32 EBRPartitionSize;
    NvU32 EbrFsPartitionId;
    NvU64 PartitionStart, PartitionSize, LogicalStartSector;
    NvU32 offset = EBR_PART_RECORD_OFFSET, SectorSize;
    NvDdkBlockDevHandle hDev = NULL;
    NvError    e = NvSuccess;
    NvPartInfo PartitionInfo;
    NvFsMountInfo FsMountInfo;
    NvU32 NextEbrFsPartitionId;

    if (!hAboot)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    EbrFsPartitionId = NvPartMgrGetNextIdForDeviceInstance(
                            DeviceId, DeviceInstance, EBRPartitionId);
    if (!EbrFsPartitionId)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(EbrFsPartitionId, &PartitionInfo));
    if (PartitionInfo.PartitionType != NvPartMgrPartitionType_Data)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    NvOsMemset(&FirstEntry, 0, sizeof(FirstEntry));
    NvOsMemset(&SecondEntry, 0, sizeof(SecondEntry));
    NvOsMemset(Ebr, 0, EBR_SIZE);
    NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
        EBRPartitionId, &PartitionStart, &PartitionSize,
        &SectorSize));
    EBRPartitionSize = PartitionSize;
    NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
        EbrFsPartitionId, &PartitionStart, &PartitionSize,
        &SectorSize));
    // Populate First Entry of the EBR.
    FirstEntry.lba_f = EBRPartitionSize;
    FirstEntry.lba_f = FirstEntry.lba_f * (SectorSize / DISK_BLOCK_SIZE);
    FirstEntry.num_blocks = PartitionSize ;
    FirstEntry.num_blocks = FirstEntry.num_blocks * (SectorSize / DISK_BLOCK_SIZE);

    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetFsInfo(EbrFsPartitionId, &FsMountInfo));
#ifdef NV_EMBEDDED_BUILD
    // Set 0x83 for Linux, 0xB1 for QNX.
    if (FsMountInfo.FileSystemType == NvFsMgrFileSystemType_Qnx)
        FirstEntry.part_type = 0xB1;
    else
#endif
        FirstEntry.part_type = 0x83;

    if(NextEbrPartitionId) // It is not the last partition, so populate second entry also.
    {
        NextEbrFsPartitionId = NvPartMgrGetNextIdForDeviceInstance(
                                    DeviceId, DeviceInstance, NextEbrPartitionId);
        if (!NextEbrFsPartitionId)
        {
            e = NvError_BadParameter;
            goto fail;
        }
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(NextEbrFsPartitionId, &PartitionInfo));
        if (PartitionInfo.PartitionType != NvPartMgrPartitionType_Data)
        {
            e = NvError_BadParameter;
            goto fail;
        }
        NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
            NextEbrPartitionId, &PartitionStart, &PartitionSize,
            &SectorSize));
        EBRPartitionSize = PartitionSize;

        // We need the info about the extended partition not the Nvflash EBR partition
        // which is there before each extended partition
        NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
            NextEbrFsPartitionId, &PartitionStart, &PartitionSize, &SectorSize)
        );

        SecondEntry.lba_f = (PartitionStart - EBRPartitionSize)- ExtendedPhysicalStartSector ;
        SecondEntry.lba_f = SecondEntry.lba_f * (SectorSize / DISK_BLOCK_SIZE);
        SecondEntry.num_blocks = PartitionSize + EBRPartitionSize;
        SecondEntry.num_blocks = SecondEntry.num_blocks *
            (SectorSize / DISK_BLOCK_SIZE);
        SecondEntry.part_type = 0x5;
    }

    NvOsMemcpy(&Ebr[offset], &FirstEntry, sizeof(FirstEntry));
    offset += sizeof(FirstEntry);
    NvOsMemcpy(&Ebr[offset], &SecondEntry, sizeof(SecondEntry));

    //Write out EBR magic value.
    Ebr[510] = 0x55;
    Ebr[511] = 0xAA;

    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(EBRPartitionId, &Partition));
    LogicalStartSector = (NvU32)Partition.StartLogicalSectorAddress;

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetFsInfo(EBRPartitionId, &FsMountInfo)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen(FsMountInfo.DeviceId,
            FsMountInfo.DeviceInstance, EBRPartitionId, &hDev)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevWriteSector(hDev, LogicalStartSector, Ebr, 1, NV_TRUE)
    );
fail:
    if(hDev)
        NvDdkBlockDevClose(hDev);
    return e;
}


static NvBool NvIsGPTAvailable(NvU32 DeviceId, NvU32 DeviceInstance)
{
    NvError    e = NvSuccess;
    NvBool retval = NV_FALSE;
    NvU32 GPTPartID = 0;
    NvPartInfo GPTPartInfo;
    NvDdkBlockDevHandle hDev = NULL;
    NvDdkBlockDevInfo BlockDevInfo;
    NvU8 *buff = NULL;
    GPTPartID = GetPartitionByType(NvPartMgrPartitionType_GPT);
    if(GPTPartID != 0)
    {
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(GPTPartID, &GPTPartInfo));
        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevMgrDeviceOpen(DeviceId,
                DeviceInstance, GPTPartID, &hDev)
        );
        NvDdkBlockDevGetDeviceInfo(hDev, &BlockDevInfo);
        buff = NvOsAlloc(BlockDevInfo.BytesPerSector);
        if (!buff)
        {
            retval = NV_FALSE;
            goto fail;
        }
        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevReadSector(hDev, GPTPartInfo.StartLogicalSectorAddress +
                    GPTPartInfo.NumLogicalSectors -1, buff, 1, NV_TRUE)
        );
        if((NvOsMemcmp(&buff[BlockDevInfo.BytesPerSector-GPT_SIGN_OFFSET_FROM_END_BUFF],
                    "EFI PART", 8)) == 0)
        {
            retval = NV_TRUE;
        }
    }
fail:
    if(hDev)
        NvDdkBlockDevClose(hDev);
    if (buff)
        NvOsFree(buff);
    return retval;
}

static NvBool NvIsMBRAvailable(NvAbootHandle hAboot)
{
    NvError    e = NvSuccess;
    NvBool retval = NV_FALSE;
    NvU32 MBRPartID = 0;
    NvU32 CurrentPartitionId = 0;
    NvFsMountInfo FsMountInfo;
    NvPartInfo MBRPartInfo, PartitionInfo;
    NvDdkBlockDevHandle hDev = NULL;
    NvDdkBlockDevInfo BlockDevInfo;
    NvU8 *buff = NULL;
    char PartName[16];
    while(1)
    {
        CurrentPartitionId = NvPartMgrGetNextId(CurrentPartitionId);
        if(CurrentPartitionId == 0 )
            break;
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(CurrentPartitionId, &PartitionInfo));
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetNameById(CurrentPartitionId, PartName));
        if((PartitionInfo.PartitionType == NvPartMgrPartitionType_Mbr) ||
            (NvOsMemcmp(PartName, "MBR", 3) == 0))
        {
            MBRPartID = CurrentPartitionId;
            break;
        }
    }
    if(MBRPartID != 0)
    {
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(MBRPartID, &MBRPartInfo));
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetFsInfo(MBRPartID, &FsMountInfo));
        NV_CHECK_ERROR_CLEANUP(
             NvDdkBlockDevMgrDeviceOpen(FsMountInfo.DeviceId,
                FsMountInfo.DeviceInstance, MBRPartID, &hDev)
        );
        NvDdkBlockDevGetDeviceInfo(hDev, &BlockDevInfo);
        buff = NvOsAlloc(BlockDevInfo.BytesPerSector);
        if (!buff)
        {
            retval = NV_FALSE;
            goto fail;
        }
        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevReadSector(hDev, MBRPartInfo.StartLogicalSectorAddress, buff, 1, NV_TRUE)
        );
        if((buff[MBR_SIZE-2] == 0x55) && (buff[MBR_SIZE-1] = 0xAA))
        {
            retval = NV_TRUE;
        }
    }
fail:
    if(hDev)
        NvDdkBlockDevClose(hDev);
    if (buff)
        NvOsFree(buff);
    return retval;
}

static NvError NvAbootRawWriteMBRPartitionPerDevice(
    NvAbootHandle hAboot,
    NvU32 DeviceId,
    NvU32 DeviceInstance)
{
    NvU32 MbrPartitionId = 0;
    NvU32 CurrentPartitionId = 0;
    EbrStatus EbrPartitionStatus = Ebr_Not_Found;
    NvU32 ExtendedPartSize = 0;
    NvPartInfo PartitionInfo;
    char PartName[16];
    NvU64 PartitionStart, PartitionSize;
    NvU32 offset = MBR_PART_RECORD_OFFSET, SectorSize;
    NvU32 EbrPartitions[MAX_EBR_PARTITIONS];
    NvU32 NumEbrPartitions = 0;
    NvU32 EbrPartitionCount = 0;
    NvU32 NumPartEntries = 0;
    NvU64 LogicalStartSector, MbrLogicalStartSector = 0;
    NvU64 PhysicalStartSector;
    NvU64 MbrPhysicalStartSector = 0, EbrPhysicalStartSector = 0;
    NvU32 EbrEntryOffest = 0;
    NvDdkBlockDevHandle hDev = NULL;
    NvError    e = NvSuccess;
    NvFsMountInfo FsMountInfo;
    NvU32 GPTPartitionID = 0;

    if (!hAboot)
    {
        return NvError_BadParameter;
    }

    NvOsMutexLock(hAboot->hStorageMutex);

    NvOsMemset(&PartRecord, 0, sizeof(PartRecord));
    NvOsMemset(Mbr, 0, MBR_SIZE);
    NvOsMemset(EbrPartitions, 0, MAX_EBR_PARTITIONS);

    CurrentPartitionId = 0;
    if((GPTPartitionID = GetPartitionByType(NvPartMgrPartitionType_GPT)) != 0)
    {
        NvU32 Recoverymode = 0;
        NvU32 SecondaryBootDevice = 0;
        NvU32 DevInstance = 0;
        Recoverymode = (NvU32)NvBlQueryGetNv3pServerFlag();
        SecondaryBootDevice = NvAbootGetSecBootDevId(&DevInstance);
        if (!(Recoverymode &&
            ((SecondaryBootDevice == NvDdkBlockDevMgrDeviceId_Usb3) ||
             (SecondaryBootDevice == NvDdkBlockDevMgrDeviceId_Spi))))

        {
            if(NvIsGPTAvailable(DeviceId, DeviceInstance) == NV_TRUE)
            {
                e = NvSuccess;
                goto end;
            }
        }

        if (NvAbootGetPartitionParametersbyId(hAboot,
                    GPTPartitionID, &PartitionStart, &PartitionSize,
                    &SectorSize) == NvSuccess)
            WriteGptPartitiontable(hAboot, DeviceId,
                DeviceInstance, SectorSize);
    }
    else
    {
        if(NvIsMBRAvailable(hAboot) == NV_TRUE)
        {
            e = NvSuccess;
            goto end;
        }
        NvOsMemset(&PartRecord, 0, sizeof(PartRecord));
        NvOsMemset(Mbr, 0, MBR_SIZE);
        NvOsMemset(EbrPartitions, 0, MAX_EBR_PARTITIONS);
        while(1)
        {
            CurrentPartitionId = NvPartMgrGetNextIdForDeviceInstance(
                                     DeviceId,
                                     DeviceInstance,
                                     CurrentPartitionId);
            if(CurrentPartitionId == 0 )
            {
                if(EbrPartitionStatus == Ebr_Data_Found)
                    EbrPartitionStatus = Ebr_Done;
                break;
            }

            NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(CurrentPartitionId, &PartitionInfo));
            NV_CHECK_ERROR_CLEANUP(NvPartMgrGetFsInfo(CurrentPartitionId, &FsMountInfo));
            NV_CHECK_ERROR_CLEANUP(NvPartMgrGetNameById(CurrentPartitionId, PartName));
            if(!MbrPartitionId &&
                (!((PartitionInfo.PartitionType == NvPartMgrPartitionType_Mbr) ||
                (!NvOsMemcmp(PartName, "MBR", 3)))))
            {
                continue;
            }
            NV_CHECK_ERROR_CLEANUP(NvAbootGetPartitionParametersbyId(hAboot,
                CurrentPartitionId, &PartitionStart, &PartitionSize, &SectorSize)
            );

            LogicalStartSector = (NvU32)PartitionInfo.StartLogicalSectorAddress;
            NV_CHECK_ERROR_CLEANUP(
                NvAbootPrivGetStartPhysicalPage(hAboot, CurrentPartitionId,
                     LogicalStartSector, &PhysicalStartSector)
            );

            if(!MbrPartitionId)
            {
                MbrPartitionId = CurrentPartitionId;
                MbrPhysicalStartSector = PhysicalStartSector;
                MbrLogicalStartSector = LogicalStartSector;
                continue;
            }
            if(PartitionInfo.PartitionType == NvPartMgrPartitionType_NvData)
            {
                //if we find a non fs data partition, then there shouldnt be any MBR partitions.
                //end the search here.
                if(EbrPartitionStatus == Ebr_Data_Found)
                {
                    EbrPartitionStatus = Ebr_Done;
                }
                else if(EbrPartitionStatus == Ebr_Header_Found)
                {
                    e = NvError_BadParameter;
                    goto fail;
                }
                break;
            }
            else if(PartitionInfo.PartitionType == NvPartMgrPartitionType_Data)
            {
                //if we find any data partition which is associated with a filesystem,
                //this partition can be a primary MBR partition or an EBR fs_data partition.
                if(EbrPartitionStatus == Ebr_Data_Found)
                {
                    EbrPartitionStatus = Ebr_Done;
                }
                if(EbrPartitionStatus == Ebr_Header_Found)
                {
                    EbrPartitionStatus = Ebr_Data_Found;
                    ExtendedPartSize += PartitionSize;
                }
                else
                {
                    // It's a primary partition, so add its entry to MBR.
                    PartRecord.lba_f = (NvU32)(PartitionStart - MbrPhysicalStartSector);
                    PartRecord.lba_f = PartRecord.lba_f *
                                            (SectorSize / DISK_BLOCK_SIZE);
                    PartRecord.num_blocks = (NvU32)PartitionSize;
                    PartRecord.num_blocks = PartRecord.num_blocks *
                                                (SectorSize / DISK_BLOCK_SIZE);

                    //0x83 is the id for linux partitions (ext2/ext3)
#ifdef NV_EMBEDDED_BUILD
                    //0xB1 is the id for QNX partitions
                    if (FsMountInfo.FileSystemType == NvFsMgrFileSystemType_Qnx)
                        PartRecord.part_type = 0xB1;
                    else
#endif
                        PartRecord.part_type = 0x83;

                    NvOsMemcpy(&Mbr[offset], &PartRecord, sizeof(PartRecord));
                    offset += sizeof(PartRecord);
                    NumPartEntries++;
                     //we reached maximum partitions MBR can support..so quit.
                     if(NumPartEntries > MAX_PRIMARY_MBR_PARTITIONS)
                    {
                        e = NvError_BadParameter;
                        goto fail;
                    }
                }
            }
            else if(PartitionInfo.PartitionType == NvPartMgrPartitionType_Ebr)
            {
                if(EbrPartitionStatus == Ebr_Not_Found)
                {
                    EbrPartitionStatus = Ebr_Header_Found;
                    EbrPhysicalStartSector = PhysicalStartSector;
                    EbrEntryOffest = offset;
                    PartRecord.lba_f = (NvU32)(PartitionStart - MbrPhysicalStartSector);
                    PartRecord.lba_f = PartRecord.lba_f *
                                            (SectorSize / DISK_BLOCK_SIZE);
                    PartRecord.num_blocks = (NvU32)PartitionSize;
                    PartRecord.num_blocks = PartRecord.num_blocks *
                                                (SectorSize / DISK_BLOCK_SIZE);
                    PartRecord.part_type = 0x5;
                    NvOsMemcpy(&Mbr[offset], &PartRecord, sizeof(PartRecord));
                    offset += sizeof(PartRecord);
                     if(NumPartEntries >= MAX_PRIMARY_MBR_PARTITIONS)
                    {
                        e = NvError_BadParameter;
                        goto fail;
                    }
                     NumPartEntries++;
                }
                else if(EbrPartitionStatus == Ebr_Data_Found)
                {
                    EbrPartitionStatus = Ebr_Header_Found;
                }
                else
                {
                    e = NvError_BadParameter;
                    goto fail;
                }
                EbrPartitions[NumEbrPartitions++] = CurrentPartitionId;
                ExtendedPartSize += PartitionSize;
                if(NumEbrPartitions >= MAX_EBR_PARTITIONS)
                {
                    e = NvError_BadParameter;
                    goto fail;
                }
            }
            else
            {
                e = NvError_BadParameter;
                goto fail;
            }
        }

        if(!MbrPartitionId)
        {
            e = NvError_BadParameter;
            goto fail;
        }

        if(EbrPartitionStatus == Ebr_Done)
        {
            EbrPartitionCount = 0;
            //fill EBR size info in MBR
            NvOsMemcpy(&PartRecord, &Mbr[EbrEntryOffest], sizeof(PartRecord));
            PartRecord.num_blocks = (NvU32)ExtendedPartSize;
            PartRecord.num_blocks = PartRecord.num_blocks * (SectorSize / DISK_BLOCK_SIZE);
            NvOsMemcpy(&Mbr[EbrEntryOffest], &PartRecord, sizeof(PartRecord));
            //write EBR partitions
            while(EbrPartitionCount < NumEbrPartitions)
            {
                NV_CHECK_ERROR_CLEANUP(
                    NvAbootRawWriteEBRPartition(hAboot, DeviceId, DeviceInstance,
                        EbrPhysicalStartSector,
                        EbrPartitions[EbrPartitionCount],
                        EbrPartitions[EbrPartitionCount + 1])
                );
                EbrPartitionCount++;
            }
        }

        //write MBR partition
        //Write out MBR magic value.
        Mbr[510] = 0x55;
        Mbr[511] = 0xAA;

        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetFsInfo(MbrPartitionId, &FsMountInfo));
        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevMgrDeviceOpen(FsMountInfo.DeviceId,
                FsMountInfo.DeviceInstance, MbrPartitionId, &hDev)
        );

        NV_CHECK_ERROR_CLEANUP(
            NvDdkBlockDevWriteSector(hDev, MbrLogicalStartSector, Mbr, 1, NV_TRUE)
        );
    }

fail:
end:
    if(hDev)
        NvDdkBlockDevClose(hDev);

    NvOsMutexUnlock(hAboot->hStorageMutex);

    return e;
}

NvError NvAbootRawWriteMBRPartition(NvAbootHandle hAboot)
{
    NvError e = NvSuccess;
    NvU32 DeviceId = 0, NextDeviceId = 0;
    NvU32 DeviceInstance = 0, NextDeviceInstance = 0;

    while(1)
    {
        NvPartMgrGetNextDeviceInstance(DeviceId, DeviceInstance,
            &NextDeviceId, &NextDeviceInstance);
        if (NextDeviceId == 0 && NextDeviceInstance == 0 )
            break;

        if (NextDeviceId == NvDdkBlockDevMgrDeviceId_Usb3 ||
            NextDeviceId == NvDdkBlockDevMgrDeviceId_SDMMC ||
            NextDeviceId == NvDdkBlockDevMgrDeviceId_Sata)
            {
                NV_CHECK_ERROR(
                    NvAbootRawWriteMBRPartitionPerDevice(hAboot,
                        NextDeviceId,
                        NextDeviceInstance));
            }
        DeviceId = NextDeviceId;
        DeviceInstance = NextDeviceInstance;
    }

    return e;
}

#if 0
// Call this function from fastboot/main.c if require to verify
// if MBR written is correct
// Use only for debugging purpose
NvError NvAbootRawReadMBRPartition(NvAbootHandle hAboot)
{
    NvU32 DeviceId = 0, NextDeviceId = 0;
    NvU32 DeviceInstance = 0, NextDeviceInstance = 0;
    NvU32 CurrentPartitionId = 0;
    NvDdkBlockDevHandle hDev = NULL;
    NvError    e = NvSuccess;
    NvFsMountInfo FsMountInfo;
    NvU32 MbrLogicalStartSector;
    NvPartInfo PartitionInfo;
    char PartName[NVPARTMGR_PARTITION_NAME_LENGTH];
    while(1)
    {
        NvPartMgrGetNextDeviceInstance(DeviceId, DeviceInstance,
            &NextDeviceId, &NextDeviceInstance);
        FastbootStatus("DeviceId:%d Intance:%d \n\r",
            NextDeviceId, NextDeviceInstance);
        if (NextDeviceId == 0 && NextDeviceInstance == 0)
            break;
        if (NextDeviceId == NvDdkBlockDevMgrDeviceId_SDMMC)
        {
            CurrentPartitionId = 0;
            while (1){
                CurrentPartitionId = NvPartMgrGetNextIdForDeviceInstance(
                                         NextDeviceId,
                                         NextDeviceInstance,
                                         CurrentPartitionId);
                FastbootStatus("DeviceId:%d Instance:%d PartId:%d\n\r",
                    NextDeviceId,
                    NextDeviceInstance,
                    CurrentPartitionId);
                if(CurrentPartitionId == 0 )
                {
                    break;
                }
                NV_CHECK_ERROR_CLEANUP(
                    NvPartMgrGetNameById(CurrentPartitionId, PartName));
                if(!NvOsMemcmp(PartName, "MBR", 3))
                {
                    int i = 0;
                    NV_CHECK_ERROR_CLEANUP(
                            NvPartMgrGetFsInfo(CurrentPartitionId, &FsMountInfo)
                    );

                    NV_CHECK_ERROR_CLEANUP(
                            NvDdkBlockDevMgrDeviceOpen(FsMountInfo.DeviceId,
                                FsMountInfo.DeviceInstance,
                                CurrentPartitionId, &hDev)
                    );

                    NV_CHECK_ERROR_CLEANUP(
                        NvPartMgrGetPartInfo(CurrentPartitionId,
                            &PartitionInfo)
                    );
                    MbrLogicalStartSector =
                        (NvU32)PartitionInfo.StartLogicalSectorAddress;

                    NV_CHECK_ERROR_CLEANUP(
                            NvDdkBlockDevReadSector(hDev,
                                MbrLogicalStartSector, Mbr, 1, NV_TRUE)
                    );

                    for (i = MBR_PART_RECORD_OFFSET; i<512; i +=8)
                    {
                        FastbootStatus("%x %x %x %x %x %x %x %x\n\r",
                            Mbr[i],Mbr[i+1],Mbr[i+2],Mbr[i+3],
                            Mbr[i+4],Mbr[i+5],Mbr[i+6],Mbr[i+7]);
                    }
                    if(hDev)
                        NvDdkBlockDevClose(hDev);
                    break;
                }
            }
        }
        DeviceId = NextDeviceId;
        DeviceInstance = NextDeviceInstance;
    }
fail:
    return e;
}
#endif

