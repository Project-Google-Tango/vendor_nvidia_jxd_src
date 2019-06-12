/**
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvos.h"
#include "nvassert.h"
#include "nvpartmgr.h"
#include "nvpartmgr_int.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev_defs.h"

#include "nvcrypto_hash.h"
#include "nvcrypto_cipher_defs.h"
#include "nvcrypto_cipher.h"

#define PARTITION_TABLE_BUFFER_SIZE 65536
#define INVALID_DEVICE_INSTANCE 0xFFFFFFFF
#define INVALID_DEVICE_ID 0xFFFFFFFF

#if (NV_DEBUG)
#define NV_PARTMGR_TRACE(x) NvOsDebugPrintf x
#else
#define NV_PARTMGR_TRACE(x)
#endif // NV_DEBUG

/*
 * This enumeration defines the maximum number of devices
 * that can be stored in the device list. Each device in the
 * device list constitiutes the device and the corresponding
 * instance.
 * For example, device=emmc, instance=0 and
 * device=emmc and instance=1 are treated as two different
 * devices
 */
enum {NV_PARTMGR_MAX_DEVICES = 5};

/*
 * Local structures
 */
typedef struct NvPartMgrDeviceListRec
{
    NvU32 DeviceId;
    NvU32 DeviceInstance;
    NvBool IsDeviceOpen;
    NvDdkBlockDevHandle hBlockDevHandle;
    NvDdkBlockDevInfo BlockDevInfo;
} NvPartMgrDeviceList;

/*
 * Global Variables
 */
static NvPartitionTable gs_PartTable;
static NvPartMgrDeviceList *gs_pDevList;
static NvU32 gs_DevListIndex;
static NvBool gs_CreateStartFlag = NV_FALSE;
static NvOsMutexHandle gs_PartMgrLoadTableMutex;
static NvBool gs_IsTableLoaded = NV_FALSE;
static NvU8 *gs_pPartTableStart = NULL;
static NvU32 gs_NumPartitions = 0;
static NvU8* gs_PartTableVerifyBuffer = 0;
static NvU32 gs_PartMgrRefCount;

/*
 * Private APIs
 */
static void
CreatePartitionTableHeaders(NvPartitionTable *pPartTable)
{
    /// Copy the contents from local table to global table
    gs_PartTable.InsecureHeader.InsecureLength =
        pPartTable->InsecureHeader.InsecureLength;
    gs_PartTable.InsecureHeader.InsecureVersion =
        pPartTable->InsecureHeader.InsecureVersion;
    gs_PartTable.InsecureHeader.Magic =
        pPartTable->InsecureHeader.Magic;
    NvOsMemcpy(gs_PartTable.InsecureHeader.Signature,
                pPartTable->InsecureHeader.Signature,
                sizeof(gs_PartTable.InsecureHeader.Signature));
    gs_PartTable.SecureHeader.NumPartitions =
        pPartTable->SecureHeader.NumPartitions;
    gs_PartTable.SecureHeader.SecureLength =
        pPartTable->SecureHeader.SecureLength;
    gs_PartTable.SecureHeader.SecureMagic =
        pPartTable->SecureHeader.SecureMagic;
    gs_PartTable.SecureHeader.SecureVersion =
        pPartTable->SecureHeader.SecureVersion;
    NvOsMemcpy(gs_PartTable.SecureHeader.RandomData,
                pPartTable->SecureHeader.RandomData,
                sizeof(gs_PartTable.SecureHeader.RandomData));
    NvOsMemcpy(gs_PartTable.SecureHeader.Padding1,
                pPartTable->SecureHeader.Padding1,
                sizeof(gs_PartTable.SecureHeader.Padding1));
}

static NvError
AddNewDeviceEntryToList(NvFsMountInfo *pMountInfo)
{
    NvError e = NvSuccess;
    NvDdkBlockDevIoctl_PartitionOperationInputArgs PartOpArgs;
    /// No entry found, assuming new device
    /// Add to gs_pDevList
    gs_pDevList[gs_DevListIndex].DeviceId = pMountInfo->DeviceId;
    gs_pDevList[gs_DevListIndex].DeviceInstance =
        pMountInfo->DeviceInstance;
    NV_CHECK_ERROR(
            NvDdkBlockDevMgrDeviceOpen(
                (NvDdkBlockDevMgrDeviceId)pMountInfo->DeviceId,
                pMountInfo->DeviceInstance,
                0,
                &gs_pDevList[gs_DevListIndex].hBlockDevHandle));
    gs_pDevList[gs_DevListIndex].IsDeviceOpen = NV_TRUE;
    /// Next, get the device properties
    (gs_pDevList[gs_DevListIndex].hBlockDevHandle)->NvDdkBlockDevGetDeviceInfo(
        gs_pDevList[gs_DevListIndex].hBlockDevHandle,
        &gs_pDevList[gs_DevListIndex].BlockDevInfo);
    /// As we use BytesPerSector field of block dev info,
    /// make sure that it is non-zero
    if (gs_pDevList[gs_DevListIndex].BlockDevInfo.BytesPerSector == 0)
    {
        NV_PARTMGR_TRACE(("[NvPartMgrAddTableEntry]:"
                         "BytesPerSector=0 from NvDdkBlockDevHandle"));
        e = NvError_BadParameter;
        goto fail;
    }
    /// Now issue a PartitionOperation IOCTL to start the part operation
    PartOpArgs.Operation = NvDdkBlockDevPartitionOperation_Start;
    NV_CHECK_ERROR(
        (gs_pDevList[gs_DevListIndex].hBlockDevHandle)->NvDdkBlockDevIoctl(
            gs_pDevList[gs_DevListIndex].hBlockDevHandle,
            NvDdkBlockDevIoctlType_PartitionOperation,
            sizeof(NvDdkBlockDevIoctl_PartitionOperationInputArgs),
            0,
            &PartOpArgs,
            NULL));
fail:
    return e;
}

static NvError
PopulatePartitionEntryInfo(
    NvU32 DeviceIndex,
    NvU32 PartitionId,
    char *PartitionName,
    NvPartMgrPartitionType PartitionType,
    NvU32 PartitionAttr,
    NvPartAllocInfo *pAllocInfo,
    NvFsMountInfo *pMountInfo)
{
    NvError e = NvSuccess;
    NvU32 CurrentIndex = gs_PartTable.SecureHeader.NumPartitions-1;
    NvDdkBlockDevIoctl_AllocatePartitionInputArgs AllocInArgs;
    NvDdkBlockDevIoctl_AllocatePartitionOutputArgs AllocOutArgs;

    gs_PartTable.TableEntry[CurrentIndex].PartitionId = PartitionId;
    NvOsStrncpy(gs_PartTable.TableEntry[CurrentIndex].PartitionName,
                PartitionName,
                NVPARTMGR_PARTITION_NAME_LENGTH);
    /// Populate MountInfo
    gs_PartTable.TableEntry[CurrentIndex].MountInfo.DeviceId
            = pMountInfo->DeviceId;
    gs_PartTable.TableEntry[CurrentIndex].MountInfo.DeviceInstance
            = pMountInfo->DeviceInstance;
    gs_PartTable.TableEntry[CurrentIndex].MountInfo.DeviceAttr
            = pMountInfo->DeviceAttr;
    NvOsStrncpy(gs_PartTable.TableEntry[CurrentIndex].MountInfo.MountPath,
                pMountInfo->MountPath,
                NVPARTMGR_MOUNTPATH_NAME_LENGTH);
    gs_PartTable.TableEntry[CurrentIndex].MountInfo.FileSystemType
            = pMountInfo->FileSystemType;
    gs_PartTable.TableEntry[CurrentIndex].MountInfo.FileSystemAttr
            = pMountInfo->FileSystemAttr;

    /// As we retrieved the device info, use it to populate PartInfo
    /// NumLogicalSectors should not be populated if the value is
    /// 0xFFFFFFFFFFFFFFFF
    if (pAllocInfo->Size != (NvU64)(-1))
    {
        gs_PartTable.TableEntry[CurrentIndex].PartInfo.NumLogicalSectors
                    = ((pAllocInfo->Size) /
                      (gs_pDevList[DeviceIndex].BlockDevInfo.BytesPerSector));
        if (((pAllocInfo->Size) %
                      (gs_pDevList[DeviceIndex].BlockDevInfo.BytesPerSector)))
            gs_PartTable.TableEntry[CurrentIndex].PartInfo.NumLogicalSectors++;
    }
    else
    {
        /* magic value in the nand driver is actually (NvU32)-1.
         * NumLogicalSectors is a NvU32.
         */
        gs_PartTable.TableEntry[CurrentIndex].PartInfo.NumLogicalSectors =
            ((NvU32)-1);
    }

    gs_PartTable.TableEntry[CurrentIndex].PartInfo.PartitionAttr = PartitionAttr;
    gs_PartTable.TableEntry[CurrentIndex].PartInfo.PartitionType = PartitionType;
#ifdef NV_EMBEDDED_BUILD
    gs_PartTable.TableEntry[CurrentIndex].PartInfo.IsWriteProtected = pAllocInfo->IsWriteProtected;
#endif

    /// Fill in the information required for AllocatePartition IOCTL
    AllocInArgs.PartitionId = PartitionId;
    AllocInArgs.AllocationType =
        (NvDdkBlockDevPartitionAllocationType)pAllocInfo->AllocPolicy;
    AllocInArgs.StartPhysicalSectorAddress =
        (NvU32)((pAllocInfo->StartAddress) /
        (gs_pDevList[DeviceIndex].BlockDevInfo.BytesPerSector));
    AllocInArgs.NumLogicalSectors =
        (NvU32)gs_PartTable.TableEntry[CurrentIndex].PartInfo.NumLogicalSectors;
    AllocInArgs.PercentReservedBlocks = pAllocInfo->PercentReserved;
    AllocInArgs.PartitionAttribute = pAllocInfo->AllocAttribute;
    AllocInArgs.PartitionType = PartitionType;
    // FIXME: BCT and bootloader partitions need to be sequentially read
    AllocInArgs.IsSequencedReadNeeded = NV_TRUE;

    NV_CHECK_ERROR((gs_pDevList[DeviceIndex].hBlockDevHandle)->NvDdkBlockDevIoctl(
                            gs_pDevList[DeviceIndex].hBlockDevHandle,
                            NvDdkBlockDevIoctlType_AllocatePartition,
                            sizeof(NvDdkBlockDevIoctl_AllocatePartitionInputArgs),
                            sizeof(NvDdkBlockDevIoctl_AllocatePartitionOutputArgs),
                            &AllocInArgs,
                            &AllocOutArgs));
    gs_PartTable.TableEntry[CurrentIndex].PartInfo.StartLogicalSectorAddress
        = AllocOutArgs.StartLogicalSectorAddress;
    gs_PartTable.TableEntry[CurrentIndex].PartInfo.NumLogicalSectors
        = AllocOutArgs.NumLogicalSectors;

    gs_PartTable.TableEntry[CurrentIndex].PartInfo.StartPhysicalSectorAddress =
            AllocOutArgs.StartPhysicalSectorAddress;
    gs_PartTable.TableEntry[CurrentIndex].PartInfo.EndPhysicalSectorAddress =
            AllocOutArgs.StartPhysicalSectorAddress + AllocOutArgs.NumPhysicalSectors - 1;

    // WAR For absolute addresssing
    if (CurrentIndex && gs_PartTable.TableEntry[CurrentIndex].MountInfo.DeviceId == NvDdkBlockDevMgrDeviceId_Nand)
        gs_PartTable.TableEntry[CurrentIndex - 1].PartInfo.EndPhysicalSectorAddress =
            gs_PartTable.TableEntry[CurrentIndex].PartInfo.StartPhysicalSectorAddress - 1;
    return e;
}

static NvError
EncryptPartitionTable(NvU8 *pPartTableBuffer)
{
    NvError e =  NvError_NotInitialized;
    NvCryptoCipherAlgoHandle pAlgo = (NvCryptoCipherAlgoHandle)NULL;
    NvCryptoCipherAlgoParams Params;
    NvPartitionTable *pPartTable = (NvPartitionTable *)pPartTableBuffer;
    NvU32 paddingSize = 0;
    NvU32 bytesToEncrypt = pPartTable->InsecureHeader.InsecureLength -
                    sizeof(NvPartTableHeaderInsecure);

    // Random data is not generated right now.
    NvOsMemset(pPartTable->SecureHeader.RandomData,
               '\0',
               sizeof(pPartTable->SecureHeader.RandomData));

    NV_CHECK_ERROR_CLEANUP(NvCryptoCipherSelectAlgorithm(
                NvCryptoCipherAlgoType_AesCbc,
                &pAlgo));

    // configure algorithm
    Params.AesCbc.IsEncrypt = NV_TRUE;
    Params.AesCbc.KeyType = NvCryptoCipherAlgoAesKeyType_UserSpecified;
    Params.AesCbc.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
    NvOsMemset(Params.AesCbc.KeyBytes, 0, sizeof(Params.AesCbc.KeyBytes));
    NvOsMemset(Params.AesCbc.InitialVectorBytes, 0,
               sizeof(Params.AesCbc.InitialVectorBytes));
    Params.AesCbc.PaddingType = NvCryptoPaddingType_ExplicitBitPaddingOptional;
    NV_CHECK_ERROR_CLEANUP(pAlgo->SetAlgoParams(pAlgo, &Params));

    // Add padding, if required to align the data to AES block length
    NV_CHECK_ERROR_CLEANUP(pAlgo->QueryPaddingByPayloadSize(pAlgo, bytesToEncrypt,
                &paddingSize,
                ((NvU8 *)(pPartTableBuffer)) +
                sizeof(NvPartTableHeaderInsecure) +
                bytesToEncrypt));

    pPartTable->InsecureHeader.InsecureLength += paddingSize;
    pPartTable->SecureHeader.SecureLength += paddingSize;
    bytesToEncrypt += paddingSize;

    NV_CHECK_ERROR_CLEANUP(pAlgo->ProcessBlocks(pAlgo, bytesToEncrypt,
                 (void *)(pPartTableBuffer + sizeof(NvPartTableHeaderInsecure)),
                 (void *)(pPartTableBuffer + sizeof(NvPartTableHeaderInsecure)),
                 NV_TRUE, NV_TRUE));

    pAlgo->ReleaseAlgorithm(pAlgo);
    return NvSuccess;

fail:
    pAlgo->ReleaseAlgorithm(pAlgo);
    return e;
}

static NvError
DecryptPartitionTable(NvU8 *pPartTableBuffer)
{
    NvError e =  NvError_NotInitialized;
    NvCryptoCipherAlgoHandle pAlgo = (NvCryptoCipherAlgoHandle)NULL;
    NvCryptoCipherAlgoParams Params;
    NvPartitionTable *pPartTable = (NvPartitionTable *)pPartTableBuffer;
    NvU32 bytesToDecrypt = pPartTable->InsecureHeader.InsecureLength -
                    sizeof(pPartTable->InsecureHeader);

    NV_ASSERT( ((pPartTable->InsecureHeader.InsecureLength-
            sizeof(pPartTable->InsecureHeader)) %
            NV_PART_AES_HASH_BLOCK_LEN) == 0 );

    NV_CHECK_ERROR_CLEANUP(NvCryptoCipherSelectAlgorithm(
        NvCryptoCipherAlgoType_AesCbc,
        &pAlgo));

    // configure algorithm
    Params.AesCbc.IsEncrypt = NV_FALSE;
    Params.AesCbc.KeyType = NvCryptoCipherAlgoAesKeyType_UserSpecified;
    Params.AesCbc.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
    NvOsMemset(Params.AesCbc.KeyBytes, 0, sizeof(Params.AesCbc.KeyBytes));
    NvOsMemset(Params.AesCbc.InitialVectorBytes, 0,
               sizeof(Params.AesCbc.InitialVectorBytes));
    Params.AesCbc.PaddingType = NvCryptoPaddingType_ExplicitBitPaddingOptional;

    NV_CHECK_ERROR_CLEANUP(pAlgo->SetAlgoParams(pAlgo, &Params));

    NV_CHECK_ERROR_CLEANUP(pAlgo->ProcessBlocks(pAlgo, bytesToDecrypt,
             (void *)(pPartTableBuffer + sizeof(pPartTable->InsecureHeader)),
             (void *)(pPartTableBuffer + sizeof(pPartTable->InsecureHeader)),
                             NV_TRUE, NV_TRUE));

    pAlgo->ReleaseAlgorithm(pAlgo);
    return NvSuccess;

fail:
    pAlgo->ReleaseAlgorithm(pAlgo);
    return e;
}

static NvError
SignPartitionTable(NvU8 *pPartTableBuffer)
{
    // Generate signature from the data for the partition table
    //This implementation assumes InsecureLength is set already.
    NvError e =  NvError_NotInitialized;
    NvPartitionTable *pPartTable = (NvPartitionTable *)pPartTableBuffer;
    NvU32 bytesToSign = pPartTable->InsecureHeader.InsecureLength -
                    sizeof(NvPartTableHeaderInsecure);

    //Need to use a constant instead of a value
    NvU32 paddingSize = NV_PART_AES_HASH_BLOCK_LEN -
                            (bytesToSign % NV_PART_AES_HASH_BLOCK_LEN);

    NvU32 hashSize = NV_PART_AES_HASH_BLOCK_LEN;
    NvCryptoHashAlgoHandle pAlgo = (NvCryptoHashAlgoHandle)NULL;
    NvCryptoHashAlgoParams Params;

    NvOsMemset(&(pPartTable->InsecureHeader.Signature[0]), 0,
                        NV_PART_AES_HASH_BLOCK_LEN);

    NV_CHECK_ERROR_CLEANUP(NvCryptoHashSelectAlgorithm(
                                    NvCryptoHashAlgoType_AesCmac,
                                     &pAlgo));

    NV_ASSERT(pAlgo);

    // configure algorithm
    Params.AesCmac.IsCalculate = NV_TRUE;
    Params.AesCmac.KeyType = NvCryptoCipherAlgoAesKeyType_UserSpecified;
    Params.AesCmac.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
    NvOsMemset(Params.AesCmac.KeyBytes, 0, sizeof(Params.AesCmac.KeyBytes));
    Params.AesCmac.PaddingType = NvCryptoPaddingType_ExplicitBitPaddingOptional;

    NV_CHECK_ERROR_CLEANUP(pAlgo->SetAlgoParams(pAlgo, &Params));

    // Since Padding for the cmac algorithm is going to be decided at runtime,
    // we need to compute padding size on the fly and decide on the padding
    // size accordingly.
    // 1. Query the padding size. Padding size will be 0 if number of partitions
    //        is evenelse it's 8 bytes.
    // 2. Change the SecureLength, InsecureLength and bytesToSign by paddingSize
    NV_CHECK_ERROR_CLEANUP(pAlgo->QueryPaddingByPayloadSize(pAlgo, bytesToSign,
                                        &paddingSize,
                                        ((NvU8 *)(pPartTable)) +
                                        sizeof(NvPartTableHeaderInsecure) +
                                        bytesToSign));


    pPartTable->InsecureHeader.InsecureLength += paddingSize;
    pPartTable->SecureHeader.SecureLength += paddingSize;
    bytesToSign += paddingSize;

    // process data
    NV_CHECK_ERROR_CLEANUP(pAlgo->ProcessBlocks(pAlgo, bytesToSign,
                             (void *)(pPartTableBuffer +
                             sizeof(NvPartTableHeaderInsecure)),
                             NV_TRUE, NV_TRUE));

    //Read back the hash and only store 8 bytes in the signature
    // within the insecure header. This should be sufficient for ver-
    // ification purposes.
    NV_CHECK_ERROR_CLEANUP(pAlgo->QueryHash(pAlgo, &hashSize,
                                     pPartTable->InsecureHeader.Signature));


    pAlgo->ReleaseAlgorithm(pAlgo);
    return NvSuccess;

fail:

    pAlgo->ReleaseAlgorithm(pAlgo);
    return e;

}

static NvError
VerifyPartitionTableSignature(NvU8 *pPartTableBuffer)
{

    //This implementation assumes InsecureLength is set already.
    NvError e =  NvError_NotInitialized;
    NvPartitionTable *pPartTable = (NvPartitionTable *)pPartTableBuffer;
    NvU32 bytesToSign = pPartTable->InsecureHeader.InsecureLength -
                    sizeof(pPartTable->InsecureHeader);

    NvBool isValidHash = NV_FALSE;
    NvCryptoHashAlgoHandle pAlgo = (NvCryptoHashAlgoHandle)NULL;
    NvCryptoHashAlgoParams Params;

    NV_ASSERT( ((pPartTable->InsecureHeader.InsecureLength-
            sizeof(pPartTable->InsecureHeader)) %
            NV_PART_AES_HASH_BLOCK_LEN) == 0 );

    NV_CHECK_ERROR_CLEANUP(NvCryptoHashSelectAlgorithm(
                                    NvCryptoHashAlgoType_AesCmac,
                                     &pAlgo));

    // configure algorithm
    Params.AesCmac.IsCalculate = NV_TRUE;
    Params.AesCmac.KeyType = NvCryptoCipherAlgoAesKeyType_UserSpecified;
    Params.AesCmac.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
    NvOsMemset(Params.AesCmac.KeyBytes, 0, sizeof(Params.AesCmac.KeyBytes));
    Params.AesCmac.PaddingType = NvCryptoPaddingType_ExplicitBitPaddingOptional;

    NV_CHECK_ERROR_CLEANUP(pAlgo->SetAlgoParams(pAlgo, &Params));

    // process data
    NV_CHECK_ERROR_CLEANUP(pAlgo->ProcessBlocks(pAlgo, bytesToSign,
                             (void *)(pPartTableBuffer +
                             sizeof(pPartTable->InsecureHeader)),
                             NV_TRUE, NV_TRUE));

    // Verify the hash computed with the Signature of the Partition Table
    e = (pAlgo->VerifyHash(pAlgo,
                                        pPartTable->InsecureHeader.Signature,
                                        &isValidHash));
    if (!isValidHash)
    {
        e = NvError_BadValue;
        NV_CHECK_ERROR_CLEANUP(e);
    }

fail:

    pAlgo->ReleaseAlgorithm(pAlgo);
    return e;

}

/*
 * Public APIs
 */

 NvError
 NvPartMgrInit(void)
{
    NvError e = NvSuccess;

    /* Initialize PartitionManager*/
    if (gs_PartMgrRefCount == 0)
    {
        gs_PartTable.TableEntry = NULL;
        NV_CHECK_ERROR_CLEANUP(
            NvOsMutexCreate(&gs_PartMgrLoadTableMutex));
    }
    ++gs_PartMgrRefCount;
fail:
    return e;
}

void
NvPartMgrDeinit(void)
{
    NV_ASSERT(gs_PartMgrRefCount);

    --gs_PartMgrRefCount;
    if (gs_PartMgrRefCount == 0)
    {
        /* De-initialize PartitionManager */
        gs_IsTableLoaded = NV_FALSE;
        if (gs_pPartTableStart)
        {
            // Load Table Path
            NvOsFree(gs_pPartTableStart);
            gs_pPartTableStart = NULL;
            gs_PartTable.TableEntry = NULL;
        }
        else if (gs_PartTable.TableEntry)
        {
            // Create Table path
            NvOsFree(gs_PartTable.TableEntry);
            gs_PartTable.TableEntry = NULL;
        }
        NvOsMutexDestroy(gs_PartMgrLoadTableMutex);
    }
}

NvError
NvPartMgrLoadTable(
    NvDdkBlockDevMgrDeviceId DeviceId,
    NvU32 DeviceInstance,
    NvU32 StartLogicalSector,
    NvU32 NumLogicalSectors,
    NvBool IsSigned,
    NvBool IsEncrypted)
{
    NvError e = NvError_NotInitialized;
    NvDdkBlockDevHandle hBlockDevHandle;
    NvDdkBlockDevInfo BlockDevInfo;
    NvU32 i, j;
    NvU32 Size;
    NvPartitionTable *pPartTable = NULL;
    NvU8 *pPartTableBuffer = NULL;

    /// Begin Thread safe
    NvOsMutexLock(gs_PartMgrLoadTableMutex);

    // Check if PM init had been done earlier
    if(gs_PartMgrRefCount == 0)
    {
        NV_PARTMGR_TRACE(("[NvPartMgrLoadTable]:"
                          "PartMgr not initialized"));
        goto fail;
    }
    if (gs_IsTableLoaded == NV_TRUE)
    {
        e = NvSuccess;
        goto fail;
    }
    /// Open devmgr with partitionid = 0
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen(DeviceId,
                              DeviceInstance,
                              0,
                              &hBlockDevHandle));
    /// Get the device properties
    (hBlockDevHandle)->NvDdkBlockDevGetDeviceInfo(
                         hBlockDevHandle,
                         &BlockDevInfo);
    Size = sizeof(NvPartTableHeaderInsecure);
    pPartTableBuffer = NvOsAlloc(BlockDevInfo.BytesPerSector);
    if (pPartTableBuffer == NULL)
    {
        NV_PARTMGR_TRACE(("[NvPartMgrLoadTable]:"
                         "Memory allocation failed"));
        e = NvError_InsufficientMemory;
        goto CloseAndReturn;
    }
    for (i = StartLogicalSector;
         i < (StartLogicalSector + NumLogicalSectors);
         i++)
    {
        e = (hBlockDevHandle)->NvDdkBlockDevReadSector(hBlockDevHandle,
                                i,
                                pPartTableBuffer,
                                1);
        if (e != NvSuccess)
            continue;
        /// Verify the integrity of insecure header
        pPartTable = (NvPartitionTable *) pPartTableBuffer;
        if ((pPartTable->InsecureHeader.Magic == NV_PART_MAGIC) &&
            (pPartTable->InsecureHeader.InsecureVersion == NV_PART_VERSION))
        {
            /// We got a match
            break;
        }
    }
    if (i == (StartLogicalSector + NumLogicalSectors) ||
        (pPartTable == NULL))
    {
        /// Cannot find the PT, return
        NV_PARTMGR_TRACE(("[NvPartMgrLoadTable]:"
                         "Cannot locate PT"));
        e = NvError_BadParameter;
        NvOsFree(pPartTableBuffer);
        pPartTableBuffer = NULL;
        goto CloseAndReturn;
    }
    /// get the size of the partition table
    Size = pPartTable->InsecureHeader.InsecureLength;
    /// Compute number of sectors required
    if (Size % BlockDevInfo.BytesPerSector)
    {
        Size /= BlockDevInfo.BytesPerSector;
        Size++;
    }
    else
        Size /= BlockDevInfo.BytesPerSector;
    if(Size > 1)
    {
        NvOsFree(pPartTableBuffer);
        pPartTableBuffer = NULL;
        pPartTableBuffer = NvOsAlloc(Size * BlockDevInfo.BytesPerSector);
        if (pPartTableBuffer == NULL)
        {
            NV_PARTMGR_TRACE(("[NvPartMgrLoadTable]:"
                             "Memory allocation failed"));
            e = NvError_InsufficientMemory;
            goto CloseAndReturn;
        }

        e = (hBlockDevHandle)->NvDdkBlockDevReadSector(hBlockDevHandle,
                                i,
                                pPartTableBuffer,
                                Size);
        pPartTable = (NvPartitionTable *) pPartTableBuffer;
    }
    if (e == NvSuccess)
    {
        //compare backup partition table buffer with what we just read
        if(gs_PartTableVerifyBuffer &&
            (NvOsMemcmp(gs_PartTableVerifyBuffer, pPartTableBuffer,
                (Size * BlockDevInfo.BytesPerSector)) != 0))
        {
            e = NvError_NotSupported;
            goto fail;
        }

        if (IsSigned == NV_TRUE)
        {
            e = VerifyPartitionTableSignature(pPartTableBuffer);
            if (e != NvSuccess)
                goto CloseAndReturn;
        }

        if (IsEncrypted == NV_TRUE)
        {
            e = DecryptPartitionTable(pPartTableBuffer);
            if (e != NvSuccess)
                goto CloseAndReturn;
        }


        CreatePartitionTableHeaders((NvPartitionTable *) pPartTableBuffer);

        gs_pPartTableStart = pPartTableBuffer;
        // free table entry to avoid memory leak since it might have been
        // allocated in create table start and overwritten here.
        if (gs_PartTable.TableEntry)
        {
            NvOsFree(gs_PartTable.TableEntry);
            gs_PartTable.TableEntry = NULL;
        }
        gs_PartTable.TableEntry
            = (NvPartitionTableEntry *)((NvU32)pPartTableBuffer +
              (NvU32)sizeof(NvPartTableHeaderSecure) +
              (NvU32)sizeof(NvPartTableHeaderInsecure));
        gs_pPartTableStart = pPartTableBuffer;
        gs_PartTable.TableEntry
            = (NvPartitionTableEntry *)((NvU32)pPartTableBuffer +
              (NvU32)sizeof(NvPartTableHeaderSecure) +
              (NvU32)sizeof(NvPartTableHeaderInsecure));

        gs_pDevList =
            NvOsAlloc(NV_PARTMGR_MAX_DEVICES * sizeof(NvPartMgrDeviceList));
        if (gs_pDevList == NULL)
        {
            NV_PARTMGR_TRACE(("[NvPartMgrCreateTableStart]: Alloc Failed"));
            e = NvError_InsufficientMemory;
            goto fail;
        }
        for (i = 0; i < NV_PARTMGR_MAX_DEVICES; i++)
        {
            gs_pDevList[i].DeviceId = INVALID_DEVICE_ID;
            gs_pDevList[i].DeviceInstance = INVALID_DEVICE_INSTANCE;
            gs_pDevList[i].IsDeviceOpen = NV_FALSE;
        }
        gs_DevListIndex = 0;
        for (i = 0; i < gs_PartTable.SecureHeader.NumPartitions; i++)
        {
            for (j = 0;  j < gs_DevListIndex; j++)
            {
                if (gs_pDevList[j].DeviceId == gs_PartTable.TableEntry[i].MountInfo.DeviceId &&
                        gs_pDevList[j].DeviceInstance ==
                            gs_PartTable.TableEntry[i].MountInfo.DeviceInstance)
                    break;
            }
            if (gs_pDevList[gs_DevListIndex].DeviceId == INVALID_DEVICE_ID && j == gs_DevListIndex)
            {
                gs_pDevList[gs_DevListIndex].DeviceId =
			gs_PartTable.TableEntry[i].MountInfo.DeviceId;
                gs_pDevList[gs_DevListIndex].DeviceInstance =
			gs_PartTable.TableEntry[i].MountInfo.DeviceInstance;
                gs_DevListIndex++;
                if (gs_DevListIndex >= NV_PARTMGR_MAX_DEVICES )
                    break;
            }
        }

    }
    gs_IsTableLoaded = NV_TRUE;
CloseAndReturn:
    hBlockDevHandle->NvDdkBlockDevClose(hBlockDevHandle);
fail:
    if(gs_PartTableVerifyBuffer)
    {
        NvOsFree(gs_PartTableVerifyBuffer);
        gs_PartTableVerifyBuffer = 0;
    }
    /// End Thread safe
    NvOsMutexUnlock(gs_PartMgrLoadTableMutex);
    return e;
}

void
NvPartMgrUnLoadTable(void)
{
    NvOsMutexLock(gs_PartMgrLoadTableMutex);
    gs_IsTableLoaded = NV_FALSE;
    if (gs_pPartTableStart)
    {
        // Load Table Path
        NvOsFree(gs_pPartTableStart);
        gs_pPartTableStart = NULL;
        gs_PartTable.TableEntry = NULL;
    }
    else if (gs_PartTable.TableEntry)
    {
        // Create Table path
        NvOsFree(gs_PartTable.TableEntry);
        gs_PartTable.TableEntry = NULL;
    }
    NvOsMutexUnlock(gs_PartMgrLoadTableMutex);
}

// The following methods are available only from the single-threaded, non-interrupt bootloader environment.
// These routines are not thread-safe.

NvError
NvPartMgrCreateTableStart(NvU32 NumPartitions)
{
    NvError e = NvSuccess;
    NvU32 i;

    NV_ASSERT(NumPartitions);
    if (NumPartitions == 0)
    {
        e = NvError_BadParameter;
        goto fail;
    }
    if (gs_PartMgrRefCount == 0)
    {
        NV_PARTMGR_TRACE(("[NvPartMgrCreateTableStart]:"
                          "PartMgr not initialized"));
        e = NvError_NotInitialized;
        goto fail;
    }
    gs_PartTable.TableEntry =
        NvOsAlloc(NumPartitions * sizeof(NvPartitionTableEntry));
    NvOsMemset(gs_PartTable.TableEntry, 0, NumPartitions * sizeof(NvPartitionTableEntry));
    if (gs_PartTable.TableEntry == NULL)
    {
        NV_PARTMGR_TRACE(("[NvPartMgrCreateTableStart]: Alloc Failed"));
        e = NvError_InsufficientMemory;
        goto fail;
    }
    gs_pDevList =
        NvOsAlloc(NV_PARTMGR_MAX_DEVICES * sizeof(NvPartMgrDeviceList));
    if (gs_pDevList == NULL)
    {
        NV_PARTMGR_TRACE(("[NvPartMgrCreateTableStart]: Alloc Failed"));
        e = NvError_InsufficientMemory;
        goto fail;
    }
    gs_PartTable.SecureHeader.NumPartitions = 0;
    for (i = 0; i < NV_PARTMGR_MAX_DEVICES; i++)
    {
        gs_pDevList[i].DeviceId = -1;
        gs_pDevList[i].DeviceInstance = -1;
        gs_pDevList[i].IsDeviceOpen = NV_FALSE;
    }
    gs_CreateStartFlag = NV_TRUE;
    gs_NumPartitions = NumPartitions;
fail:
    return e;
}

NvError
NvPartMgrCreateTableFinish(void)
{
    NvError e = NvSuccess;
    NvU32 i;
    NvBool FailStatus = NV_FALSE;
    NvDdkBlockDevIoctl_PartitionOperationInputArgs PartOpArgs;
    if(gs_CreateStartFlag == NV_FALSE)
    {
        NV_PARTMGR_TRACE(("[NvPartMgrCreateTableFinish]:"
                          "CreateTableStart not called"));
        e = NvError_NotInitialized;
        goto fail;
    }
    PartOpArgs.Operation = NvDdkBlockDevPartitionOperation_Finish;
    for (i = 0; i < gs_DevListIndex; i++)
    {
        if (gs_pDevList[i].IsDeviceOpen == NV_TRUE)
        {
            /// Signal allocate partition end
            e = (gs_pDevList[i].hBlockDevHandle)->NvDdkBlockDevIoctl(
                    gs_pDevList[i].hBlockDevHandle,
                    NvDdkBlockDevIoctlType_PartitionOperation,
                    sizeof(NvDdkBlockDevIoctl_PartitionOperationInputArgs),
                    0,
                    &PartOpArgs,
                    NULL);
            if (e != NvSuccess)
            {
                NV_PARTMGR_TRACE(("[NvPartMgrCreateTableFinish]:"
                             "DeviceId %d's NvDdkBlockDevIoctl Failed: %X",
                             gs_pDevList[i].DeviceId, e));
                FailStatus = NV_TRUE;
            }
            (gs_pDevList[i].hBlockDevHandle)->NvDdkBlockDevClose(
                                gs_pDevList[i].hBlockDevHandle);
            gs_pDevList[i].IsDeviceOpen = NV_FALSE;
        }
    }
    /// Assert if ioctl failed
    if (FailStatus != NV_FALSE)
    {
        NV_ASSERT((FailStatus == NV_FALSE));
    }
    NvOsFree(gs_pDevList);
    gs_pDevList = NULL;
fail:
    return e;
}

/// Not currently thread-safe
NvError
NvPartMgrAddTableEntry(
    NvU32 PartitionId,
    char PartitionName[NVPARTMGR_PARTITION_NAME_LENGTH],
    NvPartMgrPartitionType PartitionType,
    NvU32 PartitionAttr,
    NvPartAllocInfo *pAllocInfo,
    NvFsMountInfo *pMountInfo)
{
    NvError e = NvSuccess;
    NvU32 DeviceIndex, i;

    /// Check if CreateTableStart had been called prior to this method
    if (gs_CreateStartFlag == NV_FALSE)
    {
         NV_PARTMGR_TRACE(("[NvPartMgrAddTableEntry]:"
                            "CreateTableStart not called"));
         e = NvError_InvalidState;
         goto fail;
    }

    if (gs_PartTable.SecureHeader.NumPartitions >= gs_NumPartitions)
    {
        NV_ASSERT(!"Number of table entries cannot exceed"
                   "number of partitions defined"
                   "by CreateTableStart");
        e = NvError_NotSupported;
        goto fail;
    }

    /// Check for sanity of the arguments
    if ((PartitionId == 0) || (pAllocInfo == NULL) || (pMountInfo == NULL))
    {
        NV_ASSERT(!"Invalid arguments");
        e = NvError_BadParameter;
        goto fail;
    }

    /// Check the uniqueness of PartitionId
    for (i = 0; i < gs_PartTable.SecureHeader.NumPartitions; i++)
    {
        if (PartitionId == gs_PartTable.TableEntry[i].PartitionId)
        {
            NV_PARTMGR_TRACE(("[NvPartMgrAddTableEntry]:"
                         "Partition with Id %d already defined",
                         PartitionId));
            e = NvError_BadParameter;
            goto fail;
        }
    }

    /// Scan through the device list and check
    /// if this devid and instance have been addressed before
    for (DeviceIndex = 0;
         DeviceIndex < NV_PARTMGR_MAX_DEVICES;
         DeviceIndex++)
    {
        if ((gs_pDevList[DeviceIndex].DeviceId == pMountInfo->DeviceId) &&
       (gs_pDevList[DeviceIndex].DeviceInstance == pMountInfo->DeviceInstance))
           break;
    }

    if (DeviceIndex == NV_PARTMGR_MAX_DEVICES)
    {
        NV_CHECK_ERROR(AddNewDeviceEntryToList(pMountInfo));
        DeviceIndex = gs_DevListIndex;
        gs_DevListIndex++;
    }

    /// As this is a valid partition, increment the number of partitions
    /// available and allocate / reallocate memory for TableEntry
    ++gs_PartTable.SecureHeader.NumPartitions;
    NV_CHECK_ERROR_CLEANUP(
        PopulatePartitionEntryInfo(
            DeviceIndex,
            PartitionId,
            PartitionName,
            PartitionType,
            PartitionAttr,
            pAllocInfo,
            pMountInfo));
fail:
    return e;
}

/// Not currently thread-safe
NvError
NvPartMgrSaveTable(
    NvU32 PartitionId,
    NvBool SignTable,
    NvBool EncryptTable)
{
    NvError e = NvSuccess;
    NvFsMountInfo FsMountInfo;
    NvPartInfo PartInfo;
    NvDdkBlockDevHandle hBlockDevHandle;
    NvDdkBlockDevInfo BlockDevInfo;
    // Number of sectors in one Partition Table
    NvU32 PTSectors = 0;
    NvU64 SectorNumber, TotalSectors;
    //contains only one copy of Partition Table
    NvU8 *pPartTableBuffer = NULL;
    NvBool IsWriteSuccessful;
    NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs ErasePartArgs;
    // Intermediate buffer to contain multiple copies of partition table
    NvU8 *PTIntermediateBuffer = NULL;
    // number of copies of Partiion Table in partition table Intermediate buffer
    NvU64 PartitionTableCopiesInPTBuffer;
    // number of sectors in Intermediate buffer(PTIntermediateBuffer)
    NvU64 PTBufferSectorsLimit;
    NvU64 TempSize;
    /// Complete the partition table
    PTSectors = sizeof(NvPartTableHeaderInsecure) +
           sizeof(NvPartTableHeaderSecure) +
           (gs_PartTable.SecureHeader.NumPartitions *
           sizeof(NvPartitionTableEntry));
    gs_PartTable.InsecureHeader.InsecureLength = PTSectors;
    gs_PartTable.SecureHeader.SecureLength = PTSectors;
    gs_PartTable.InsecureHeader.Magic = NV_PART_MAGIC;
    gs_PartTable.SecureHeader.SecureMagic = NV_PART_MAGIC;
    gs_PartTable.InsecureHeader.InsecureVersion = NV_PART_VERSION;
    gs_PartTable.SecureHeader.SecureVersion = NV_PART_VERSION;

    // Allocate a buffer of size of partition table and copy the contents of
    // the partition table there. This is required since the table entries are
    // on heap and changing table entries to be a variable sized array
    // results in errors during creation of partitions.
    if ((PTSectors % NV_PART_AES_HASH_BLOCK_LEN) != 0)
    {
        PTSectors += NV_PART_AES_HASH_BLOCK_LEN - (PTSectors %
                        NV_PART_AES_HASH_BLOCK_LEN);
    }

    /// Now write the table to disk
    /// Get the fs info for the partition table partitionid
    if (NvPartMgrGetFsInfo(PartitionId, &FsMountInfo) != NvSuccess)
    {
        NV_PARTMGR_TRACE(("[NvPartMgrSaveTable]:"
                        "Cannot find PartitionId: %d in table", PartitionId));
        e = NvError_BadParameter;
        goto fail;
    }
    // open the media block driver where the partition table resides
    e = NvDdkBlockDevMgrDeviceOpen(
        (NvDdkBlockDevMgrDeviceId)FsMountInfo.DeviceId,
        FsMountInfo.DeviceInstance, PartitionId, &hBlockDevHandle);
    if (e != NvSuccess)
    {
        NV_PARTMGR_TRACE(("[NvPartMgrSaveTable]:"
                         "NvDdkBlockDevMgrDeviceOpen failed: %X", e));
        goto fail;
    }
    /// Get the device info
    (hBlockDevHandle)->NvDdkBlockDevGetDeviceInfo(
                             hBlockDevHandle,
                             &BlockDevInfo);
    /// Compute number of sectors required
    if (PTSectors % BlockDevInfo.BytesPerSector)
    {
        PTSectors /= BlockDevInfo.BytesPerSector;
        PTSectors++;
    }
    else
        PTSectors /= BlockDevInfo.BytesPerSector;

    /// Issue write
    /// Write multiple copies of PT and fill this partition with copies of PT
    if (NvPartMgrGetPartInfo(PartitionId, &PartInfo) != NvSuccess)
    {
        NV_PARTMGR_TRACE(("[NvPartMgrSaveTable]:"
                        "Cannot find PartitionId: %d in table", PartitionId));
        e = NvError_BadParameter;
        goto fail;
    }
#ifdef NV_EMBEDDED_BUILD
    // This is the first erase of the block device.
    // We should unprotect all sectors. It is fine to not
    // check for the return status as some of the drivers
    // do not support this ioctl
    // Supported only for NOR
    hBlockDevHandle->NvDdkBlockDevIoctl(hBlockDevHandle,
            NvDdkBlockDevIoctlType_UnprotectAllSectors,
            0, 0, NULL, NULL);
#endif

    // Must erase the PT partition before write
    // FormatPartition is not called for PT paritition and plan is
    // to remove the Deleteall command in nvflash
    ErasePartArgs.StartLogicalSector =
        (NvU32)PartInfo.StartLogicalSectorAddress;
    ErasePartArgs.NumberOfLogicalSectors =
        (NvU32)PartInfo.NumLogicalSectors;
    ErasePartArgs.PTendSector = (NvU32)
        (PartInfo.StartLogicalSectorAddress +
        PartInfo.NumLogicalSectors);
    ErasePartArgs.IsPTpartition = NV_TRUE;
    ErasePartArgs.IsSecureErase = NV_FALSE;
    ErasePartArgs.IsTrimErase = NV_FALSE;

    e = hBlockDevHandle->NvDdkBlockDevIoctl(hBlockDevHandle,
            NvDdkBlockDevIoctlType_ErasePartition,
            sizeof(NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs), 0,
            &ErasePartArgs, NULL);
    if(e != NvSuccess)
    {
        // Format of PT partition failed
        NvOsDebugPrintf("\r\nError PT partition format sector start=%d, "
            "count=%d ", ErasePartArgs.StartLogicalSector,
            ErasePartArgs.NumberOfLogicalSectors);
        goto fail;
    }

    pPartTableBuffer = NvOsAlloc(PTSectors * BlockDevInfo.BytesPerSector);
    NvOsMemset(pPartTableBuffer, 0, PTSectors * BlockDevInfo.BytesPerSector);
    if(pPartTableBuffer == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    if(gs_PartTableVerifyBuffer == NULL)
        gs_PartTableVerifyBuffer = NvOsAlloc(PTSectors * BlockDevInfo.BytesPerSector);
    if(gs_PartTableVerifyBuffer == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    // Copy headers
    NvOsMemcpy(pPartTableBuffer, (NvU8 *)&gs_PartTable,
           sizeof(NvPartTableHeaderInsecure) + sizeof(NvPartTableHeaderSecure));

    // Copy table entries. Table entries are sequential on heap.
    NvOsMemcpy((pPartTableBuffer+ sizeof(NvPartTableHeaderInsecure) +
           sizeof(NvPartTableHeaderSecure)), (NvU8 *)&gs_PartTable.TableEntry[0],
           (gs_PartTable.SecureHeader.NumPartitions * sizeof(NvPartitionTableEntry)));

    if (EncryptTable == NV_TRUE)
    {
        /// TODO: Generate Random data and store in
        /// gs_PartTable.SecureHeader.RandomData
        NV_CHECK_ERROR(EncryptPartitionTable(pPartTableBuffer));
    }
    else
    {
        NvOsMemset(gs_PartTable.SecureHeader.RandomData,
                   '\0',
                   sizeof(gs_PartTable.SecureHeader.RandomData));
    }

    if (SignTable == NV_TRUE)
    {
        NV_CHECK_ERROR(SignPartitionTable(pPartTableBuffer));
    }
    else
    {
        NvOsMemset(gs_PartTable.InsecureHeader.Signature,
           '\0',
           sizeof(gs_PartTable.InsecureHeader.Signature));
    }

    // Now, write data to mass storage
    SectorNumber = PartInfo.StartLogicalSectorAddress;
    TotalSectors = SectorNumber + PartInfo.NumLogicalSectors;
    IsWriteSuccessful = NV_FALSE;

    // Take backup of complete partition table buffer
    NvOsMemcpy(gs_PartTableVerifyBuffer, pPartTableBuffer,
           (PTSectors * BlockDevInfo.BytesPerSector));
    // here PTBufferSectorsLimit is made multiple of number of sectors in
    // Partition Table so that partition table Intermediate buffer contains
    // integral number of copies of partion table
    PTBufferSectorsLimit = PARTITION_TABLE_BUFFER_SIZE /
                                      (NvU64)BlockDevInfo.BytesPerSector;
    PartitionTableCopiesInPTBuffer = PTBufferSectorsLimit / PTSectors;
    if (PartitionTableCopiesInPTBuffer <= 1)
    {
        // partition table Intermediate buffer is less than two times the
        // Partition Table so it can contain only one Partion Table and
        // thus no allocation required.
        PTBufferSectorsLimit = (NvU64)PTSectors;
        PTIntermediateBuffer = pPartTableBuffer;
        PartitionTableCopiesInPTBuffer = 1;
    }
    else
    {
        //here partition table Intermediate buffer contains two or more
        //integral copies of Partition Table
        PTBufferSectorsLimit = PartitionTableCopiesInPTBuffer * PTSectors;
        PTIntermediateBuffer = NvOsAlloc((NvU32)PTBufferSectorsLimit *
                                           BlockDevInfo.BytesPerSector);
        if (PTIntermediateBuffer == NULL)
        {
           e = NvError_InsufficientMemory;
           goto fail;
        }
        NvOsMemset(PTIntermediateBuffer, 0, (NvU32)PTBufferSectorsLimit *
                                           BlockDevInfo.BytesPerSector);
        // Filling the Intermediate buffer(PTIntermediateBuffer) with multiple
        // copies of Partition Table
        TempSize = 0;
        while (TempSize < PTBufferSectorsLimit)
        {
            NvU64 Offset;
            Offset = TempSize * BlockDevInfo.BytesPerSector;
            NvOsMemcpy(PTIntermediateBuffer + Offset, pPartTableBuffer,
                                       (PTSectors * BlockDevInfo.BytesPerSector));
            TempSize += (NvU64)PTSectors;
        }
    }
    // filling PT Partition in disk with partition table Intermediate buffer
    while (SectorNumber < TotalSectors)
    {
        // when number of sectors in partition table Intermediate buffer is
        // greater than number of sectors in PT partition that may be due to
        // either actual size of Intermediate buffer is greater than that of
        // PT partition or loop has reached to last part of PT partition
        // In such a case again modify the size of PTBufferSectorsLimit so that
        // it writes only integral copies of Partition Table in PT Partition.
        if (PTBufferSectorsLimit > (TotalSectors - SectorNumber))
        {
            PTBufferSectorsLimit = TotalSectors - SectorNumber;
            if (PTBufferSectorsLimit % PTSectors)
            {
                TempSize = PTBufferSectorsLimit % PTSectors;
                PTBufferSectorsLimit -= TempSize;
                // decreasing it so it could not iterate next time unnecesarily
                TotalSectors -= TempSize;
            }
        }
        e = (hBlockDevHandle)->NvDdkBlockDevWriteSector(hBlockDevHandle,
                                (NvU32)SectorNumber,
                                PTIntermediateBuffer,
                                (NvU32)PTBufferSectorsLimit);
        if(e == NvSuccess)
            IsWriteSuccessful = NV_TRUE;
        /// Even if Write fails, write multiple copies
        SectorNumber += PTBufferSectorsLimit;
    }
    /// If atleast one write of part table is successful, return success
    if(IsWriteSuccessful == NV_TRUE)
        e = NvSuccess;
    (hBlockDevHandle)->NvDdkBlockDevClose(hBlockDevHandle);

fail:

    if (pPartTableBuffer)
    {
        if(pPartTableBuffer == PTIntermediateBuffer)
            PTIntermediateBuffer = NULL;
        NvOsFree(pPartTableBuffer);
        pPartTableBuffer = NULL;
    }
    if (PTIntermediateBuffer)
    {
        NvOsFree(PTIntermediateBuffer);
        PTIntermediateBuffer = NULL;
    }

    return e;
}


NvError
NvPartMgrGetIdByName(
    const char *PartitionName,
    NvU32 *PartitionId)
{
    NvError e = NvError_NotInitialized;
    NvU32 i;
    NV_ASSERT(PartitionName);
    NV_ASSERT(PartitionId);
    if (gs_PartTable.TableEntry != NULL)
    {
        for (i = 0; i<gs_PartTable.SecureHeader.NumPartitions; i++)
        {
            if (NvOsStrncmp(PartitionName,
                           gs_PartTable.TableEntry[i].PartitionName,
                           NVPARTMGR_PARTITION_NAME_LENGTH) == 0)
            {
                *PartitionId = gs_PartTable.TableEntry[i].PartitionId;
                e = NvError_Success;
                break;
            }
        }
        if (i == gs_PartTable.SecureHeader.NumPartitions)
        {
            NV_PARTMGR_TRACE(("[NvPartMgrGetIdByName]:"
                         "Could not find a match for given partition name"));
            e = NvError_BadParameter;
        }
    }
    return e;
}

NvError
NvPartMgrGetNameById(
    NvU32 PartitionId,
    char *PartitionName)
{
    NvError e = NvError_NotInitialized;
    NvU32 i;
    NV_ASSERT(PartitionId);
    NV_ASSERT(PartitionName);
    if (gs_PartTable.TableEntry != NULL)
    {
        if (PartitionId != 0)
        {
            for (i = 0; i < gs_PartTable.SecureHeader.NumPartitions; i++)
            {
                if (PartitionId == gs_PartTable.TableEntry[i].PartitionId)
                {
                    NvOsStrncpy(PartitionName,
                        gs_PartTable.TableEntry[i].PartitionName,
                        NVPARTMGR_PARTITION_NAME_LENGTH);
                    e = NvError_Success;
                    break;
                }
            }
            if (i == gs_PartTable.SecureHeader.NumPartitions)
            {
                NV_PARTMGR_TRACE(("[NvPartMgrGetNameById]:"
                             "Could not find a match for given partition id"));
                e = NvError_BadParameter;
            }
        }
        else
            e = NvError_BadParameter;
    }
    return e;

}

NvError
NvPartMgrGetFsInfo(
    NvU32 PartitionId,
    NvFsMountInfo *pFsMountInfo)
{
    NvError e = NvError_NotInitialized;
    NvU32 i;
    NV_ASSERT(PartitionId);
    NV_ASSERT(pFsMountInfo);
    if (gs_PartTable.TableEntry != NULL)
    {
        if (PartitionId != 0)
        {
            for (i = 0; i < gs_PartTable.SecureHeader.NumPartitions; i++)
            {
                if (PartitionId == gs_PartTable.TableEntry[i].PartitionId)
                {
                    pFsMountInfo->DeviceAttr =
                        gs_PartTable.TableEntry[i].MountInfo.DeviceAttr;
                    pFsMountInfo->DeviceId =
                        gs_PartTable.TableEntry[i].MountInfo.DeviceId;
                    pFsMountInfo->DeviceInstance =
                        gs_PartTable.TableEntry[i].MountInfo.DeviceInstance;
                    pFsMountInfo->FileSystemAttr =
                        gs_PartTable.TableEntry[i].MountInfo.FileSystemAttr;
                    pFsMountInfo->FileSystemType =
                       gs_PartTable.TableEntry[i].MountInfo.FileSystemType;
                    NvOsStrncpy(pFsMountInfo->MountPath,
                            gs_PartTable.TableEntry[i].MountInfo.MountPath,
                            NVPARTMGR_MOUNTPATH_NAME_LENGTH);
                    e = NvError_Success;
                    break;
                }
            }
            if (i == gs_PartTable.SecureHeader.NumPartitions)
            {
                NV_PARTMGR_TRACE(("[NvPartMgrGetFsInfo]:"
                             "Could not find a match for given partition id"));
                e = NvError_BadParameter;
            }
        }
        else
            e = NvError_BadParameter;
    }
    return e;
}

NvError
NvPartMgrGetPartInfo(
    NvU32 PartitionId,
    NvPartInfo *pPartInfo)
{
    NvError e = NvError_NotInitialized;
    NvU32 i;
    NV_ASSERT(PartitionId);
    NV_ASSERT(pPartInfo);
    if (gs_PartTable.TableEntry != NULL)
    {
        if (PartitionId != 0)
        {
            for (i = 0; i < gs_PartTable.SecureHeader.NumPartitions; i++)
            {
                if (PartitionId == gs_PartTable.TableEntry[i].PartitionId)
                {
                    pPartInfo->NumLogicalSectors =
                        gs_PartTable.TableEntry[i].PartInfo.NumLogicalSectors;
                    pPartInfo->StartLogicalSectorAddress =
                        gs_PartTable.TableEntry[i].PartInfo.StartLogicalSectorAddress;
                    pPartInfo->PartitionAttr =
                        gs_PartTable.TableEntry[i].PartInfo.PartitionAttr;
                    pPartInfo->PartitionType = gs_PartTable.TableEntry[i].PartInfo.PartitionType;
                    pPartInfo->StartPhysicalSectorAddress =
                        gs_PartTable.TableEntry[i].PartInfo.StartPhysicalSectorAddress;
                    pPartInfo->EndPhysicalSectorAddress =
                        gs_PartTable.TableEntry[i].PartInfo.EndPhysicalSectorAddress;

                    e = NvError_Success;
                    break;
                }
            }
            if (i == gs_PartTable.SecureHeader.NumPartitions)
            {
                NV_PARTMGR_TRACE(("[NvPartMgrGetPartInfo]:"
                             "Could not find a match for given partition id"));
                e = NvError_BadParameter;
            }
        }
        else
            e = NvError_BadParameter;
    }
    return e;
}

NvU32
NvPartMgrGetNextId(
    NvU32 PartitionId)
{
    NvU32 i=0, PartId=0;
    if (gs_PartTable.TableEntry != NULL)
    {
        if (PartitionId == 0)
        {
            PartId = gs_PartTable.TableEntry[0].PartitionId;
        }
        else
        {
            for (i = 0; i < gs_PartTable.SecureHeader.NumPartitions; i++)
            {
                if (PartitionId == gs_PartTable.TableEntry[i].PartitionId)
                {
                    if ((i+1) < gs_PartTable.SecureHeader.NumPartitions)
                    {
                        PartId = gs_PartTable.TableEntry[i+1].PartitionId;
                    }
                    else
                        PartId = 0;
                    break;
                }
            }
            if (i == gs_PartTable.SecureHeader.NumPartitions)
            {
                PartId = 0;
            }
        }
    }
    return PartId;
}


NvU32
NvPartMgrGetNextIdForDeviceInstance(
    NvU32 DeviceId,
    NvU32 DeviceInstance,
    NvU32 PartitionId)
{
    NvU32 i = 0, PartId = 0, j;
    if (gs_PartTable.TableEntry != NULL)
    {
        for (i = 0; i < gs_PartTable.SecureHeader.NumPartitions; i++)
        {
            if (DeviceId == gs_PartTable.TableEntry[i].MountInfo.DeviceId &&
                    DeviceInstance == gs_PartTable.TableEntry[i].MountInfo.DeviceInstance)
            {
                /*
                 * Special Case were we don't know the partitionId,  so return the first partitionId
                 */
                if (PartitionId == 0 )
                {
                    PartId = gs_PartTable.TableEntry[i].PartitionId;
                    return PartId;
                }
                else if (PartitionId == gs_PartTable.TableEntry[i].PartitionId)
                {
                    for (j = i+1; j < gs_PartTable.SecureHeader.NumPartitions; j++)
                        if (DeviceId == gs_PartTable.TableEntry[j].MountInfo.DeviceId &&
                                DeviceInstance ==
                                    gs_PartTable.TableEntry[j].MountInfo.DeviceInstance)
                        {
                            PartId = gs_PartTable.TableEntry[j].PartitionId;
                            return PartId;
                        }
                }
            }
         }
         if (i == gs_PartTable.SecureHeader.NumPartitions)
             PartId = 0;

    }
    return PartId;
}

NvU32
NvPartMgrGetNextDeviceInstance(
        NvU32 DeviceId,
        NvU32 DeviceInstance,
        NvU32 *NextDeviceId,
        NvU32 *NextDeviceInstance)
{
    NvU32 j;

    if ( DeviceId == 0 && DeviceInstance == 0 )
    {
        *NextDeviceId = gs_pDevList[0].DeviceId;
        *NextDeviceInstance = gs_pDevList[0].DeviceInstance;
        return NvSuccess;
    }
    for (j = 0;  j < gs_DevListIndex; j++)
    {
        if (gs_pDevList[j].DeviceId == DeviceId &&
                gs_pDevList[j].DeviceInstance == DeviceInstance)
            break;
    }
    if ( j+1 < gs_DevListIndex )
    {
        *NextDeviceId = gs_pDevList[j+1].DeviceId;
        *NextDeviceInstance = gs_pDevList[j+1].DeviceInstance;
    }
    else
    {
        *NextDeviceId = 0;
        *NextDeviceInstance = 0;
    }
    return NvSuccess;
}

NvU32
NvPartMgrGetNumPartition()
{
    return  gs_PartTable.SecureHeader.NumPartitions;
}
