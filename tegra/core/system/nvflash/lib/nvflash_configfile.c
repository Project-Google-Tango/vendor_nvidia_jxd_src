/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvflash_configfile.h"
#include "nvutil.h"
#include "nvassert.h"

typedef struct NvFlashConfigFileRec
{
    NvBool bError;
} NvFlashConfigFile;

typedef enum
{
    NvFlashParseTarget_Device = 1,
    NvFlashParseTarget_Partition,

    NvFlashParseTarget_Force32 = 0x7FFFFFFF,
} NvFlashParseTarget;

static NvU32 s_nDevices;
static NvFlashDevice *s_Devices;
static NvFlashPartition *s_CurrentPartition;
static NvFlashDevice *s_CurrentDevice;
static NvFlashConfigFile s_ConfigFile;
static NvFlashParseTarget s_ParseTarget;
const char *s_LastError;

#define NVFLASH_EXTFS_RANGE_START 0x40000000
#define NVFLASH_EXTFS_RANGE_END   0x7FFFFFFF


void NvFlashConfigFilePrivInit( NvFlashConfigFileHandle *hConfig )
{
    s_ParseTarget = NvFlashParseTarget_Device;

    s_Devices = 0;

    NvOsMemset( &s_ConfigFile, 0, sizeof(s_ConfigFile) );

    *hConfig = &s_ConfigFile;
    s_LastError = 0;
}

void NvFlashConfigFilePrivDecl( char *decl )
{
    NvFlashConfigFile *cfg = &s_ConfigFile;
    NvFlashDevice *d;
    NvFlashPartition *p;

    if( cfg->bError )
    {
        NvOsFree(decl);
        return;
    }

    if( NvOsStrcmp( decl, "device" ) == 0 )
    {
        /* add a new device to the list, set the current device */
        d = NvOsAlloc( sizeof(NvFlashDevice) );
        if( !d )
        {
            s_LastError = "memory allocation error";
            cfg->bError = NV_TRUE;
            NvOsFree(decl);
            return;
        }

        NvOsMemset( d, 0, sizeof(NvFlashDevice) );
        s_CurrentDevice = d;

        if( !s_Devices )
        {
            s_Devices = d;
        }
        else
        {
            NvFlashDevice *tmp = s_Devices;
            for( ; tmp; tmp = tmp->next )
            {
                if( !tmp->next )
                {
                    tmp->next = d;
                    break;
                }
            }
        }

        s_nDevices++;
        s_ParseTarget = NvFlashParseTarget_Device;
    }
    else if( NvOsStrcmp( decl, "partition" ) == 0 )
    {

        if( !s_CurrentDevice )
        {
            s_LastError = "partition without a device";
            cfg->bError = NV_TRUE;
            NvOsFree(decl);
            return;
        }

        p = NvOsAlloc( sizeof(NvFlashPartition) );
        if( !p )
        {
            s_LastError = "memory allocation failure";
            cfg->bError = NV_TRUE;
            NvOsFree(decl);
            return;
        }

        NvOsMemset( p, 0, sizeof(NvFlashPartition) );

        s_CurrentPartition = p;

        d = s_CurrentDevice;
        if( !d->nPartitions )
        {
            d->Partitions = p;
        }
        else
        {
            NvFlashPartition *tmp = d->Partitions;
            for( ; tmp; tmp = tmp->next )
            {
                if( !tmp->next )
                {
                    tmp->next = p;
                    break;
                }
            }
        }

        d->nPartitions++;
        s_ParseTarget = NvFlashParseTarget_Partition;
    }
    else
    {
        s_LastError = "unknown tag";
        cfg->bError = NV_TRUE;
    }

    NvOsFree(decl);
}

void NvFlashConfigFilePrivAttr( char *attr, char *val )
{
    NvFlashDevice *d;
    NvFlashPartition *p;
    NvFlashConfigFile *cfg = &s_ConfigFile;
    NvU32 FsType;

    if( cfg->bError )
    {
        NvOsFree(attr);
        NvOsFree(val);
        return;
    }

    d = s_CurrentDevice;
    p = s_CurrentPartition;

    if( s_ParseTarget == NvFlashParseTarget_Device )
    {
        if( NvOsStrcmp( attr, "type" ) == 0 )
        {
            if( NvOsStrcmp( val, "ide" ) == 0 )
            {
                d->Type = Nv3pDeviceType_Ide;
            }
            else if( NvOsStrcmp( val, "nand" ) == 0 )
            {
                d->Type = Nv3pDeviceType_Nand;
            }
            else if( NvOsStrcmp( val, "sdmmc" ) == 0 )
            {
                d->Type = Nv3pDeviceType_Emmc;
            }
            else if( NvOsStrcmp( val, "snor" ) == 0 )
            {
                d->Type = Nv3pDeviceType_Snor;
            }
            else if( NvOsStrcmp( val, "muxonenand" ) == 0 )
            {
                d->Type = Nv3pDeviceType_MuxOneNand;
            }
            else if( NvOsStrcmp( val, "mobilelbanand" ) == 0 )
            {
                d->Type = Nv3pDeviceType_MobileLbaNand;
            }
            else if( NvOsStrcmp( val, "spi" ) == 0 )
            {
                d->Type = Nv3pDeviceType_Spi;
            }
            else if( NvOsStrcmp(val, "usb3" ) == 0 )
            {
                d->Type = Nv3pDeviceType_Usb3;
            }
            else if( NvOsStrcmp( val, "sata" ) == 0 )
            {
                d->Type = Nv3pDeviceType_Sata;
            }
            else
            {
                s_LastError = "device/ type unknown";
                cfg->bError = NV_TRUE;
            }
        }
        else if( NvOsStrcmp( attr, "instance" ) == 0 )
        {
            d->Instance = NvUStrtoul( val, 0, 0 );
        }
#ifdef NV_EMBEDDED_BUILD
        else if(NvOsStrcmp( attr, "page_cache") == 0 )
        {
            d->PageCache = NvUStrtoul(val, 0, 0);
        }
#endif
        else
        {
            s_LastError = "device/ unknown attribute";
            cfg->bError = NV_TRUE;
        }
    }
    else if( s_ParseTarget == NvFlashParseTarget_Partition )
    {
        if( NvOsStrcmp( attr, "name" ) == 0 )
        {
            p->Name = NvOsAlloc(NvOsStrlen(val) + 1);

            if (p->Name)
                NvOsStrncpy(p->Name, val, NvOsStrlen(val) + 1);
        }
        else if( NvOsStrcmp( attr, "id" ) == 0 )
        {
            p->Id = NvUStrtoul( val, 0, 0 );
            if( p->Id > 255 )
            {
                s_LastError = "partition/ id too large";
                cfg->bError = NV_TRUE;
            }
        }
        else if( NvOsStrcmp( attr, "type" ) == 0 )
        {
            if( NvOsStrcmp( val, "boot_config_table" ) == 0 )
            {
                p->Type = Nv3pPartitionType_Bct;
            }
            else if( NvOsStrcmp( val, "bootloader" ) == 0 )
            {
                p->Type = Nv3pPartitionType_Bootloader;
            }
            else if( NvOsStrcmp( val, "bootloader_stage2" ) == 0 )
            {
                p->Type = Nv3pPartitionType_BootloaderStage2;
            }
            else if( NvOsStrcmp( val, "partition_table" ) == 0 )
            {
                p->Type = Nv3pPartitionType_PartitionTable;
            }
            else if( NvOsStrcmp( val, "master_boot_record" ) == 0 )
            {
                p->Type = Nv3pPartitionType_Mbr;
            }
            else if( NvOsStrcmp( val, "extended_boot_record" ) == 0 )
            {
                p->Type = Nv3pPartitionType_Ebr;
            }
             else if( NvOsStrcmp( val, "nv_data" ) == 0 )
            {
                p->Type = Nv3pPartitionType_NvData;
            }
            else if( NvOsStrcmp( val, "data" ) == 0 )
            {
                p->Type = Nv3pPartitionType_Data;
            }
            else if( NvOsStrcmp( val, "GP1" ) == 0 )
            {
                p->Type = Nv3pPartitionType_GP1;
            }
            else if( NvOsStrcmp( val, "GPT" ) == 0 )
            {
                p->Type = Nv3pPartitionType_GPT;
            }
            else if (NvOsStrcmp(val, "fuse_bypass") == 0)
            {
                p->Type = Nv3pPartitionType_FuseBypass;
            }
            else if (NvOsStrcmp(val, "config_table") == 0)
            {
                p->Type = Nv3pPartitionType_ConfigTable;
            }
            else if (NvOsStrcmp(val, "WB0") == 0)
            {
                p->Type = Nv3pPartitionType_WB0;
            }
            else
            {
                s_LastError = "partition/ unknown partition type";
                cfg->bError = NV_TRUE;
            }
        }
        else if( NvOsStrcmp( attr, "allocation_policy" ) == 0 )
        {
            if( NvOsStrcmp( val, "none" ) == 0 )
            {
                p->AllocationPolicy = Nv3pPartitionAllocationPolicy_None;
            }
            else if( NvOsStrcmp( val, "absolute" ) == 0 )
            {
                p->AllocationPolicy =
                    Nv3pPartitionAllocationPolicy_Absolute;
            }
            else if( NvOsStrcmp( val, "sequential" ) == 0 )
            {
                p->AllocationPolicy =
                    Nv3pPartitionAllocationPolicy_Sequential;
            }
            else
            {
                s_LastError = "partition/ allocation_policy invalid";
                cfg->bError = NV_TRUE;
            }
        }
        else if( NvOsStrcmp( attr, "filesystem_type" ) == 0 )
        {
            if( NvOsStrcmp( val, "basic" ) == 0 )
            {
                p->FileSystemType = Nv3pFileSystemType_Basic;
            }
#ifdef NV_EMBEDDED_BUILD
            else if( NvOsStrcmp( val, "enhanced" ) == 0 )
            {
                p->FileSystemType = Nv3pFileSystemType_Enhanced;
            }
            else if( NvOsStrcmp( val, "ext2" ) == 0 )
            {
                p->FileSystemType = Nv3pFileSystemType_Ext2;
            }
            else if( NvOsStrcmp( val, "yaffs2" ) == 0 )
            {
                p->FileSystemType = Nv3pFileSystemType_Yaffs2;
            }
            else if( NvOsStrcmp( val, "ext3" ) == 0 )
            {
                p->FileSystemType = Nv3pFileSystemType_Ext3;
            }
            else if( NvOsStrcmp( val, "ext4" ) == 0 )
            {
                p->FileSystemType = Nv3pFileSystemType_Ext4;
            }
            else if( NvOsStrcmp( val, "qnx" ) == 0 )
            {
                p->FileSystemType = Nv3pFileSystemType_Qnx;
            }
#endif
            else
            {
                FsType = NvUStrtoul( val, 0, 0 );
                if (FsType >= NVFLASH_EXTFS_RANGE_START &&
                    FsType < NVFLASH_EXTFS_RANGE_END)
                    p->FileSystemType = FsType;
                else
                {
                    s_LastError = "partition/ filesystem_type invalid";
                    cfg->bError = NV_TRUE;
                }
            }
        }
        else if( NvOsStrcmp( attr, "start_location" ) == 0 )
        {
            p->StartLocation = NvUStrtoull( val, 0, 0 );
        }
        else if( NvOsStrcmp( attr, "size" ) == 0 )
        {
            p->Size = NvUStrtoull( val, 0, 0 );
        }
        else if( NvOsStrcmp( attr, "file_system_attribute" ) == 0 )
        {
            p->FileSystemAttribute = NvUStrtoul( val, 0, 0 );
        }
        else if( NvOsStrcmp( attr, "partition_attribute" ) == 0 )
        {
            p->PartitionAttribute = NvUStrtoul( val, 0, 0 );
        }
        else if( NvOsStrcmp( attr, "allocation_attribute" ) == 0 )
        {
            p->AllocationAttribute = NvUStrtoul( val, 0, 0 );
        }
        else if( NvOsStrcmp( attr, "percent_reserved" ) == 0 )
        {
            p->PercentReserved = NvUStrtoul( val, 0, 0 );
        }
#ifdef NV_EMBEDDED_BUILD
        else if( NvOsStrcmp( attr, "write_protect" ) == 0 )
        {
            p->IsWriteProtected = NvUStrtoul( val, 0, 0 );
            if (p->IsWriteProtected > 1)
            {
                s_LastError = "Supported values for write_protect are 0, 1";
                cfg->bError = NV_TRUE;
            }
        }
#endif
        else if( NvOsStrcmp( attr, "filename" ) == 0 )
        {
#ifdef NV_EMBEDDED_BUILD
            if (p->Dirname)
            {
                s_LastError = "Dirname and Filename can't be defined at the same time";
                cfg->bError = NV_TRUE;
            }
            else
#endif
            {
                p->Filename = NvOsAlloc(NvOsStrlen(val) + 1);
                if (p->Filename)
                    NvOsStrncpy(p->Filename, val, NvOsStrlen(val) + 1);
            }
        }
#ifdef NV_EMBEDDED_BUILD
        else if(NvOsStrcmp(attr, "dirname") == 0)
        {
            if (p->Filename)
            {
                s_LastError = "Dirname and Filename can't be defined at the same time";
                cfg->bError = NV_TRUE;
            }
            else
            {
                p->Dirname = NvOsAlloc(NvOsStrlen(val) + 1);
                if (p->Dirname)
                    NvOsStrncpy(p->Dirname, val, NvOsStrlen(val) + 1);
            }

            NvOsStrncpy(p->Dirname, val, NvOsStrlen(val) + 1);
        }
        else if(NvOsStrcmp(attr, "imagepath") == 0)
        {
            p->ImagePath = NvOsAlloc(NvOsStrlen(val) + 1);

            if (p->ImagePath){
                NvOsStrncpy(p->ImagePath, val, NvOsStrlen(val) + 1);
            }
        }
        else if(NvOsStrcmp(attr, "os_args") == 0)
        {
            p->OS_Args = NvOsAlloc(NvOsStrlen(val) + 1);

            if (p->OS_Args)
                NvOsStrncpy(p->OS_Args, val, NvOsStrlen(val) + 1);

        }
        else if(NvOsStrcmp(attr, "ramdisk_path") == 0)
        {
            p->RamDiskPath = NvOsAlloc(NvOsStrlen(val) + 1);

            if (p->RamDiskPath)
                NvOsStrncpy(p->RamDiskPath, val, NvOsStrlen(val) + 1);

        }
        else if(NvOsStrcmp(attr, "os_load_address") == 0)
        {
            p->OS_LoadAddress = NvUStrtoull(val, 0, 0);
        }
        else if(NvOsStrcmp(attr, "ramdisk_load_address") == 0)
        {
            p->RamDisk_LoadAddress = NvUStrtoull(val, 0, 0);
        }
        else if (NvOsStrcmp(attr, "stream_decompression") == 0)
        {
            p->StreamDecompression = NvUStrtoull(val, 0, 0);
        }
        else if(NvOsStrcmp(attr, "decompression_algorithm") == 0)
        {
            p->DecompressionAlgo = NvOsAlloc(NvOsStrlen(val) + 1);

            if (p->DecompressionAlgo)
                NvOsStrncpy(p->DecompressionAlgo, val, NvOsStrlen(val) + 1);

        }
#endif
        else
        {
            s_LastError = "partition/ unknown attribute";
            cfg->bError = NV_TRUE;
        }
    }
    else
    {
        s_LastError = "unknown error!";
        cfg->bError = NV_TRUE; // something bad happened!
    }

    NvOsFree(attr);
    NvOsFree(val);
}

void
NvFlashConfigFileClose( NvFlashConfigFileHandle hConfig )
{
    // nothing to do
}

NvError
NvFlashConfigListDevices(
    NvFlashConfigFileHandle hConfig,
    NvU32 *nDevices,
    NvFlashDevice **devices )
{
    NV_ASSERT( hConfig );
    NV_ASSERT( nDevices );
    NV_ASSERT( devices );

    if( hConfig->bError )
    {
        // FIXME: better error code
        return NvError_BadParameter;
    }

    if( !s_Devices )
    {
        // FIXME: better error code
        return NvError_BadParameter;
    }

    *nDevices = s_nDevices;
    *devices = s_Devices;
    s_LastError = 0;
    return NvSuccess;
}

const char *
NvFlashConfigGetLastError( void )
{
    const char *ret;
    if( !s_LastError )
    {
        return "success";
    }

    ret = s_LastError;
    s_LastError = 0;
    return ret;
}
