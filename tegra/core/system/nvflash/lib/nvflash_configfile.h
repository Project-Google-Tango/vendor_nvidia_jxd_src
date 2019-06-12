/*
 * Copyright (c) 2008-2012, NVIDIA CORPORATION. All rights reserved.
 * 
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVFLASH_CONFIGFILE_H
#define INCLUDED_NVFLASH_CONFIGFILE_H

#include "nvcommon.h"
#include "nvos.h"
#include "nv3p.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * NvFlash's configuration file format:
 *
 * [device] - specifies a mass-storage device
 * <attr>=<val> - attribute/value pairs for the device
 * [partition] - specifies a partition in the current device
 * <attr>=<val> - attribute/value pairs for the partition
 * # - comments
 */

typedef struct NvFlashConfigFileRec *NvFlashConfigFileHandle;

typedef struct NvFlashPartitionRec
{
    char *Name;
    NvU32 Id;
    Nv3pPartitionType Type;
    Nv3pPartitionAllocationPolicy AllocationPolicy;
    Nv3pFileSystemType FileSystemType;
    NvU64 StartLocation;
    NvU64 Size;
    NvU32 FileSystemAttribute;
    NvU32 PartitionAttribute;
    NvU32 AllocationAttribute;
    NvU32 PercentReserved;
    char *Filename;
#ifdef NV_EMBEDDED_BUILD
    NvU32 IsWriteProtected;
    char *Dirname;
    char *ImagePath;
    char *OS_Args;
    char *RamDiskPath;
    NvU32 OS_LoadAddress;
    NvU32 RamDisk_LoadAddress;
    NvU32 StreamDecompression;
    char *DecompressionAlgo;
#endif
    struct NvFlashPartitionRec *next;
} NvFlashPartition;

typedef struct NvFlashDeviceRec
{
    Nv3pDeviceType Type;
    NvU32 Instance;
    NvU32 nPartitions;
    NvFlashPartition *Partitions;
#ifdef NV_EMBEDDED_BUILD
    NvU32 PageCache;
#endif
    struct NvFlashDeviceRec *next;
} NvFlashDevice;

/**
 * Opens and parses the given configuration file. This must be called first.
 *
 * @param filename The configuration file to parse.
 * @param hConfig Pointer to the configuration handle.
 */
NvError
NvFlashConfigFileParse(
    const char *filename,
    NvFlashConfigFileHandle *hConfig );

/**
 * Closes the file resources.
 */
void
NvFlashConfigFileClose( NvFlashConfigFileHandle hConfig );

/**
 * List the mass-storage devices. Each device has a list of partitions.
 *
 * @param hConfig The configuration file handle
 * @param nDevice Pointer to the number of devices
 * @param devices Pointer to the devices
 *
 * Nothing should be free'd. Device and partition pointers are in contiguous
 * memory (array of pointers).
 */
NvError
NvFlashConfigListDevices(
    NvFlashConfigFileHandle hConfig,
    NvU32 *nDevices,
    NvFlashDevice **devices );

/**
 * Retrieve the last encountered error, if any. Returns "success" if nothing
 * bad happened.
 */
const char *
NvFlashConfigGetLastError( void );

/** private functions for the parser */
void NvFlashConfigFilePrivInit( NvFlashConfigFileHandle *hConfig );
void NvFlashConfigFilePrivDecl( char *decl );
void NvFlashConfigFilePrivAttr( char *attr, char *val );

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVFLASH_CONFIGFILE_H
