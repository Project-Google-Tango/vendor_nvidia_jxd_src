/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "nvimager.h"

#if NVOS_IS_LINUX || NVOS_IS_UNIX
#include <sys/mount.h>
#include <unistd.h>
#endif

extern Nv3pSocketHandle s_hSock;
extern NvFlashDevice *s_Devices;
extern NvU32 s_nDevices;
extern NvBool s_Resume;
extern NvBool s_Quiet;
extern NvOsSemaphoreHandle s_FormatSema;
extern NvBool s_FormatSuccess;

#define FILENAME_SIZE 20
#define NV_FLASH_SYSTEM_FN_CMDLINE 1024
#define NV_FLASH_SYSTEM_KERNEL_CMDLINE 512
#define NV_BLOCK_SIZE_EXT2_EXT3_EXT4 4096
#define NV_FILESYSTEM_TYPE 8

#define VERIFY(exp, code) \
    if(!(exp)) { \
        code; \
    }

#define NVFLASH_APPEND_STRING(cmdPtr, remains, format, param) \
    do{ \
        if (param) \
            NvOsSnprintf(cmdPtr, remains, format, param); \
        else \
            NvOsSnprintf(cmdPtr, remains, format, " NONE "); \
        Idx = NvOsStrlen(cmdPtr); \
        remains -= Idx; \
        cmdPtr += Idx; \
    }while(0)

#if NVOS_IS_LINUX || NVOS_IS_UNIX
NvU32 nvflash_create_filesystem_image()
{
    NvOsFileHandle hFile = 0;
    NvU8 filesystemtype[NV_FILESYSTEM_TYPE];
    NvError e = NvSuccess;
    NvFlashDevice *d;
    NvFlashPartition *p;
    NvU32 i,j;
    NvU8 *nvflash_base_dir;

    NvU8 block_size_buffer[NV_BLOCK_SIZE_EXT2_EXT3_EXT4];
    NvU32  num_blocks = 0;
    NvOsStatType stat;
    NvU8 cmd[NV_FLASH_SYSTEM_FN_CMDLINE];
    const char *err_str = 0;

    NvFlashCommandGetOption(NvFlashOption_NvFlash_BaseDir, (void *)&nvflash_base_dir);
    d = s_Devices;
    for(i = 0; i < s_nDevices; i++, d = d->next)
    {
        p = d->Partitions;
        for(j = 0; j < d->nPartitions; j++, p = p->next)
        {
            if(p->Dirname)
            {
                NvAuPrintf("Creating Filesystem image for partition %s\n",p->Name);
                NvOsSnprintf(cmd , NV_FLASH_SYSTEM_FN_CMDLINE,
                    "src_size=`du -sb %s`;src_size=`echo ${src_size} | cut -d ' ' -f1`;size=`echo %llu *.9 | bc`;size=`echo $size | cut -d . -f1`;if [ $src_size -gt $size ];then echo exiting>/tmp/out;exit 1;fi;",
                    p->Dirname, p->Size);
                e = system(cmd);
                VERIFY(e == NvSuccess,
                    err_str = "Source directory size is more than the 90% of partition size";
                    goto fail);

                switch(p->FileSystemType)
                {
                    case Nv3pFileSystemType_Ext2:
                    case Nv3pFileSystemType_Ext3:
                    case Nv3pFileSystemType_Ext4:
                        memset(block_size_buffer, '\0', NV_BLOCK_SIZE_EXT2_EXT3_EXT4);
                        p->Filename = NvOsAlloc(FILENAME_SIZE);

                        if(p->FileSystemType == Nv3pFileSystemType_Ext2)
                        {
                            NvOsSnprintf(filesystemtype, NV_FILESYSTEM_TYPE, "ext2");
                        }
                        else if(p->FileSystemType == Nv3pFileSystemType_Ext4)
                        {
                            NvOsSnprintf(filesystemtype, NV_FILESYSTEM_TYPE, "ext4");
                        }
                        else
                        {
                            NvOsSnprintf(filesystemtype, NV_FILESYSTEM_TYPE, "ext3");
                        }
                        NvOsSnprintf(p->Filename, FILENAME_SIZE,
                            "%s.%s", p->Name, filesystemtype);

                        e = NvOsFopen(p->Filename, NVOS_OPEN_WRITE | NVOS_OPEN_CREATE, &hFile);
                        VERIFY(e == NvSuccess,
                            err_str = "Opening filesystem image failed"; goto fail);
                        for (num_blocks =0 ;
                               num_blocks < (p->Size)/NV_BLOCK_SIZE_EXT2_EXT3_EXT4;
                               num_blocks++){
                            e = NvOsFwrite(hFile, block_size_buffer,
                                    NV_BLOCK_SIZE_EXT2_EXT3_EXT4);
                            VERIFY(e == NvSuccess,
                                err_str = "Filesystem image creation failed"; goto fail);
                        }
                        NvOsFclose(hFile);

                        NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                            "%s/mkfs.%s -F -q %s",
                            nvflash_base_dir, filesystemtype, p->Filename);
                        e = system(cmd);
                        VERIFY(e == NvSuccess, err_str = "mkfs failed"; goto fail);

                        NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                            "/tmp/%s", p->Filename);
                        umount2(cmd, MNT_FORCE);

                        NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                            "rm -rf /tmp/%s", p->Filename);
                        system(cmd);

                        NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                            "/tmp/%s", p->Filename);
                        e = mkdir(cmd, 0777);
                        VERIFY(e == NvSuccess, err_str = "mkdir failed"; goto fail);

                        NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                            "sudo mount %s /tmp/%s -o loop", p->Filename, p->Filename);
                        e = system(cmd);
                        VERIFY(e == NvSuccess,
                            err_str = "loop device mount failed"; goto fail);

                        NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                            "cp -a %s/* /tmp/%s", p->Dirname, p->Filename);
                        e = system(cmd);
                        VERIFY(e == NvSuccess,
                            err_str = "Copying filesystem files failed"; goto fail);

                        e = system("sync");
                        VERIFY(e == NvSuccess, err_str = "sync failed"; goto fail);

                        NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                            "umount -d /tmp/%s", p->Filename);
                        e = system(cmd);
                        VERIFY(e == NvSuccess,
                            err_str = "loop device umount failed"; goto fail);

                        NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                            "/tmp/%s", p->Filename);
                        e = rmdir(cmd);
                        VERIFY(e == NvSuccess,
                            err_str = "Removal of mount point failed"; goto fail);

                        break;
                    case Nv3pFileSystemType_Yaffs2:
                        p->Filename = NvOsAlloc(FILENAME_SIZE);
                        NvOsSnprintf(filesystemtype, NV_FILESYSTEM_TYPE, "yaffs2");
                        NvOsSnprintf(p->Filename, FILENAME_SIZE,
                            "%s.%s", p->Name, filesystemtype);
                        NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                            "rm -rf %s", p->Filename);
                        system(cmd);

                        if (d->PageCache)
                        {
                            NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                                "%s/mkyaffs2image %s %s pagecache",
                                nvflash_base_dir, p->Dirname, p->Filename);
                        }
                        else
                        {
                            NvOsSnprintf(cmd, NV_FLASH_SYSTEM_FN_CMDLINE,
                                "%s/mkyaffs2image %s %s",
                                nvflash_base_dir, p->Dirname, p->Filename);
                        }
                        e = system(cmd);
                        VERIFY(e == NvSuccess,
                            err_str = "mkyaffs2image failed"; goto fail);

                        e = NvOsFopen(p->Filename, NVOS_OPEN_READ, &hFile);
                        VERIFY(e == NvSuccess,
                            err_str = "Opening Filesystem image failed"; goto fail);
                        NvOsFstat(hFile, &stat);
                        NvOsFclose(hFile);

                        if (stat.size > (p->Size - p->Size/10)){
                            e = NvError_InsufficientMemory;
                            VERIFY(e == NvSuccess,
                                err_str = "Filesystem image created is too large"; goto fail);
                        }

                        break;
                    default:
                        VERIFY(NV_FALSE,
                            err_str = "No support for creating FileSystem"; goto fail);
                }
            }
        }
    }
fail:
    if(err_str)
    {
        NvAuPrintf("%s\n", err_str);
    }
    return e;
}

#else

NvU32 nvflash_create_filesystem_image(){
    NvU32 i,j;
    NvFlashDevice *d;
    NvFlashPartition *p;

    d = s_Devices;
    for(i = 0; i < s_nDevices; i++, d = d->next)
    {
        p = d->Partitions;
        for(j = 0; j < d->nPartitions; j++, p = p->next)
        {
            if(p->Dirname)
            {
                NvAuPrintf("FileSystem image creation is not supported");
                return NvError_NotSupported;
            }
        }
    }
    return NvSuccess;
}

#endif

NvU32 nvflash_create_kernel_image()
{
    NvError e = NvSuccess;
    NvFlashDevice *d;
    NvFlashPartition *p;
    NvU32 i,j;
    NvU8 *nvflash_base_dir;
    NvU32 remains = NV_FLASH_SYSTEM_FN_CMDLINE + NV_FLASH_SYSTEM_KERNEL_CMDLINE, Idx = 0;
    NvU8 cmd[NV_FLASH_SYSTEM_FN_CMDLINE + NV_FLASH_SYSTEM_KERNEL_CMDLINE];
    NvU8 *cmdPtr;
    const char *err_str = 0;
    NvU8 compression_flag = 0;

    NvFlashCommandGetOption(NvFlashOption_NvFlash_BaseDir, (void *)&nvflash_base_dir);
    d = s_Devices;

    for(i = 0; i < s_nDevices; i++, d = d->next)
    {
        p = d->Partitions;
        for(j = 0; j < d->nPartitions; j++, p = p->next)
        {
            remains = NV_FLASH_SYSTEM_FN_CMDLINE + NV_FLASH_SYSTEM_KERNEL_CMDLINE;
            cmd[0] = '\0';
            cmdPtr = cmd;
            if(p->ImagePath)
            {
                NvAuPrintf("Creating OS Image for partition %s\n",p->Name);
                if (p->StreamDecompression && p->DecompressionAlgo)
                {
                    if(NvOsStrcmp(p->DecompressionAlgo, "lzf") &&
                            NvOsStrcmp(p->DecompressionAlgo, "zlib"))
                    {
                        e = NvError_BadParameter;
                        err_str = "Unknown decompression algorithm";
                        goto fail;
                    }
                    NvOsSnprintf(cmdPtr, remains, "%s/compress_%s ",
                            nvflash_base_dir, p->DecompressionAlgo);
                    Idx = NvOsStrlen(cmdPtr);
                    remains -= Idx;
                    cmdPtr += Idx;

                    if(!NvOsStrcmp(p->DecompressionAlgo, "lzf"))
                    {
                        if (d->Type == Nv3pDeviceType_Snor)
                        {
                            NvOsSnprintf(cmdPtr, remains,
                                    "-b %s -c %s -r > kernel.def", "65535",
                                    p->ImagePath);
                            Idx = NvOsStrlen(cmdPtr);
                            remains -= Idx;
                            cmdPtr += Idx;
                        }
                        else
                        {
                            NvOsSnprintf(cmdPtr, remains,
                                    "-b %s -c %s -r > kernel.def", "2047",
                                    p->ImagePath);
                            Idx = NvOsStrlen(cmdPtr);
                            remains -= Idx;
                            cmdPtr += Idx;
                        }
                    }

                    else
                    {
                        NVFLASH_APPEND_STRING(cmdPtr, remains,
                                " %s > kernel.def", p->ImagePath);
                    }
                    e = system(cmd);
                    VERIFY(e == NvSuccess,
                            err_str = "Failed to create Compressed OS Image"; goto fail);
                    compression_flag = 1;
                }
                else if ((!(p->StreamDecompression)) ^ (!(p->DecompressionAlgo)))
                {
                    e = NvError_ConfigVarNotFound;
                    err_str = "stream_decompression & decompression_algorithm must be defined together";
                    goto fail;
                }

                remains = NV_FLASH_SYSTEM_FN_CMDLINE + NV_FLASH_SYSTEM_KERNEL_CMDLINE;
                cmd[0] = '\0';
                cmdPtr = cmd;
                Idx = 0;

                NvOsSnprintf(cmdPtr, remains, "%s/mkbootimg ", nvflash_base_dir);
                Idx = NvOsStrlen(cmdPtr);
                remains -= Idx;
                cmdPtr += Idx;

                if(compression_flag == 1)
                    NVFLASH_APPEND_STRING(cmdPtr,remains,"--kernel %s ", "kernel.def");
                else
                    NVFLASH_APPEND_STRING(cmdPtr,remains,"--kernel %s ", p->ImagePath);

                NVFLASH_APPEND_STRING(cmdPtr,remains,"--cmdline %s ", p->OS_Args);
                NVFLASH_APPEND_STRING(cmdPtr,remains,"--ramdisk %s ", p->RamDiskPath);
                p->Filename = NvOsAlloc(FILENAME_SIZE);
                NvOsSnprintf(p->Filename, FILENAME_SIZE, "%s.img",p->Name);
                NVFLASH_APPEND_STRING(cmdPtr,remains,"--output %s ", p->Filename);
                NVFLASH_APPEND_STRING(cmdPtr,remains,
                        "--kern_addr 0x%x ", p->OS_LoadAddress);
                NVFLASH_APPEND_STRING(cmdPtr,remains,
                        "--ramdisk_addr 0x%x ", p->RamDisk_LoadAddress);
                if(compression_flag == 1)
                    NVFLASH_APPEND_STRING(cmdPtr,remains,
                            "--bluncompress %s ", p->DecompressionAlgo);

                e = system(cmd);
                VERIFY(e == NvSuccess, err_str = "mkbootimg failed"; goto fail);
            }
        }
    }
fail:
    if(err_str)
    {
        NvAuPrintf("%s\n", err_str);
    }
    return e;
}
