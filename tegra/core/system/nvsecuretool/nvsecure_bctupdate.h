/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVSECURE_BCTUPDATE_H
#define INCLUDED_NVSECURE_BCTUPDATE_H

#include "nvsecuretool.h"
#include "nvflash_configfile.h"

/* Defines the structure passed to function for updating Bootloader
 * information into BCT.
 */
typedef struct BctBlUpdateDataRec
{
    NvU32 BlInstance;
    NvU32 NumSectors;
    NvU32 PageSize;
    NvU32 BlockSize;
    NvU32 NumBootloaders;
    NvBctHandle SecureBctHandle;
}BctBlUpdateData;

/* Define the structure passed to function for updating bootloader
 * hash into BCT.
 */
typedef struct NvSecureEncryptSignBlDataRec
{
    NvU32 BlInstance;
    NvU32 PageSize;
    NvBctHandle SecureBctHandle;
}NvSecureEncryptSignBlData;

/**
 * parse config file provided to nvsecuretool
 *
 * @param configuration filename
 * @param argv list of arguments in command line
 *
 * Returns NvSuccess if parsing is successful otherwise NvError
 */
NvError
NvSecureParseCfg( const char *filename);

/**
 * parses cfg strucuture and fills BL informationl
 *
 * @param BctHandle required for filling BCT
 * @param BctInterface structure
 *
 * Returns NvSuccess if parsing is successful otherwise NvError
 */
NvError
NvSecureParseAndFillBlInfo(
    NvBctHandle SecureBctHandle,
    NvSecureBctInterface SecureBctInterface);

void
NvDevInfo(NvFlashDevice **dev,NvU32 *nDev);

/* Defines the prototype of a function which is to be passed to
 * NvSecurePartitionOperation().
 */
typedef NvError
PartitionOperation (
        NvFlashPartition *Partition,
        void *Aux);

/* Iterates over all partitions and calls function passed having prototyps same
 * as PartitionOperation
 *
 * @param Operation Pointer to a function which is to be called for a partition
 * @param Aux Pointer to a data to be passed to function passed as Operation
 *
 * Returns NvSuccess if operation is successful
 */
NvError
NvSecurePartitionOperation(
        PartitionOperation *Operation,
        void *Aux);

/* Computes the number of bootloaders in cfg
 *
 * @param Partition Pointer to partition containing partition information
 * @param Aux Pointer to a NvU32 variable
 *
 * Returns NvSuccess if successful
 */
NvError
NvSecureGetNumEnabledBootloader(
        NvFlashPartition *Partition,
        void *Aux);

/* Fills the bootloader information in BCT
 *
 * @param Partition Pointer to partition containing partition information
 * @param Aux Pointer to structure NvSecureEncryptSignBlData
 *
 * Returns NvSuccess if parsing is successful otherwise NvError
 */
NvError
NvSecureUpdateBctWithBlInfo(
        NvFlashPartition *Partition,
        void *Aux);

/* In SBK mode, encrypts the bootloader and updates the hash in BCT
 * In PKC mode, updates the hash in BCT and signs warmboot code of bootloader
 *
 * @param Partition Pointer to partition containing partition information
 * @param Aux Pointer to structure NvSecureEncryptSignBlData
 *
 * Returns NvSuccess if parsing is successful otherwise NvError
 */
NvError
NvSecureEncryptSignBootloaders(
        NvFlashPartition *Partition,
        void *Aux);

/* In Secure mode, computes the trusted os image hash/sign and writes
  * hash/sign at the end of the image file
 *
 * @param Partition Pointer to partition containing partition information
 * @param Aux NULL
 *
 * Returns NvSuccess if Signing/Hashing is successful otherwise NvError
 */

NvError
NvSecureSignTrustedOSImage(
        NvFlashPartition *Partition,
        void *Aux);

/* In Secure mode, computes the Warmboot image hash/sign and fills the
 * header.
 *
 * @param Partition Pointer to partition containing partition information
 * @param Aux NULL
 *
 * Returns NvSuccess if Signing/Hashing is successful otherwise NvError
 */

NvError
NvSecureSignWB0(
        NvFlashPartition *Partition,
        void *Aux);

#endif//INCLUDED_NVSECURE_BCTUPDATE_H
