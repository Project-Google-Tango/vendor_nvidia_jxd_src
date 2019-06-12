/*
 * Copyright (c) 2008-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbu.h"
#include "nvbit.h"
#include "nvbct.h"
#include "nvpartmgr.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"
#include "nvcrypto_hash.h"
#include "nvcrypto_cipher.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvsystem_utils.h"
#include "nvbu_private.h"
#include "nvddk_fuse.h"

#ifndef TEST_RECOVERY
#define TEST_RECOVERY 0
#endif

#if TEST_RECOVERY
//FIXME: Remove explicit BCT corruption before release
NvBool bIsInterruptedUpdate = NV_FALSE;
#endif
#define MICROBOOT_LOAD_REGION 0x40000000
#define MAX_BCT_SYNC_COPY 64

typedef struct NvBctBootLoaderInfoRec
{
    NvU32 Attribute;
    NvU32 Version;
    NvU32 StartBlock;
    NvU32 StartSector;
    NvU32 Length;
    NvU32 LoadAddress;
    NvU32 EntryPoint;
    NvU8 CryptoHash[NVCRYPTO_CIPHER_AES_BLOCK_BYTES];
} NvBctBootLoaderInfo;

// Forward declarations
extern NvU32 NvBitPrivGetChipId(void);

/*
 * local function declarations
 */

static
NvError
GetNumInstalledBls(
    NvBctHandle hBct,
    NvU32 *NumBootloaders);

static
NvError
GetBlInfo(
    NvBctHandle hBct,
    NvU32 Slot,
    NvBctBootLoaderInfo *pBlInfo);


static
NvError
SetBlInfo(
    NvBctHandle hBct,
    NvU32 Slot,
    NvBctBootLoaderInfo *pBlInfo);

static
NvError
GetBlSlot(
    NvBctHandle hBct,
    NvU32 PartitionId,
    NvBool *IsFound,
    NvU32 *Slot);

static
NvError
AllocateBlSlot(
    NvBctHandle hBct,
    NvU32 *Slot);

/**
 * Query which boot loader (BL) is actively running on the system
 *
 * @param hBit handle to BIT instance
 * @param pSlot pointer to slot (index) number in the BCT's BootLoader[] table
 *        corresponding to the BL that is currently running on the system
 *
 * @retval NvSuccess Active BL slot reported successfully
 * @retval NvError_InvalidAddress Illegal pointer value
 */

static
NvError
NvBuQueryActiveBlSlot(
    NvBitHandle hBit,
    NvU32 *pSlot);

/**
 * Determine the generation of the BL in the specified slot (in the BCT's table
 * of BL's)
 *
 * Note that the generation number is only defined for bootable BL's
 *
 * @param hBct handle to BCT instance
 * @param Slot index number in BCT's table of BL's
 * @param Generation pointer to generation number of the BL
 *
 * @retval NvSuccess Generation number reported successfully
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter BL in specified slot isn't bootable
 */

static
NvError
NvBuQueryBlGeneration(
    NvBctHandle hBct,
    NvU32 Slot,
    NvU32 *Generation);

/**
 * Determine the version number for the BL in the specified slot (in the BCT's
 * table of BL's)
 *
 * @param hBct handle to BCT instance
 * @param Slot index number in BCT's table of BL's
 * @param Version pointer to version number of the BL
 *
 * @retval NvSuccess Version number reported successfully
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter Illegal or out-of-range parameter
 */

static
NvError
NvBuQueryBlVersion(
    NvBctHandle hBct,
    NvU32 Slot,
    NvU32 *Version);


// WriteOneBct is passed block driver handle corresponding to
// partition id == 0
static
NvError
WriteOneBct(
    NvDdkBlockDevHandle hDev,
    NvU32 startSector, NvU32 numSectors,
    NvU32 startBlock, void *bctBuffer);

// ReadOneBct is passed block driver handle corresponding to
// partition id == 0
static
NvError
ReadOneBct(
    NvDdkBlockDevHandle hDev,
    NvU32 startSector,
    NvU32 numSectors,
    NvU32 PartSectorsPerBlock,
    NvU32 startOffset,
    void *readBctBuffer);

static
NvError
WriteAllBcts(
    NvBitHandle hBit,
    NvU32 bootromBlockSize,
    NvU32 bootromPageSize,
    NvU32 PartitionId,
    NvU32 size,
    void *buffer);

static
NvError
ComputeBootRomBlockPageSize(
    NvBctHandle hBct,
    NvU32 *BootromBlockSize,
    NvU32 *BootromPageSize);


void
LocateNextBct(
    const NvU32 * BytesPerSector,
    NvU32 BootromBlockSize,
    NvU32 BlockdevBlockSize,
    NvU32 *PreferredBctByteAddress,
    NvU32 *StartSector);

/**
 * Sign or Verify signature of the BCT
 *
 * @param Buffer Data to be signed.
 * @param Offset Offset in the data stored in param Buffer from where
 *         the signing should begin.
 * @param Size size of data to be signed.
 * @param IsHashVerify Flag to indicate the operation to be performed.
 *      NV_TRUE if signature of BCT is to be verified
 *      NV_FALSE if BCT is to be signed.
 * @param KeyType type of cipher key to be used for the operation.
 *
 * @retval NvSuccess BCT was signed or verified successfully.
 * @retval NvError_* An error occurred during the operation.
 */
NvError
BctSignVerify(
    NvU8 *Buffer,
    NvU32 Offset,
    NvU32 Size,
    NvBool IsHashVerify,
    NvCryptoCipherAlgoAesKeyType KeyType);

/*
 * function definitions
 */
NvError
BctEncryptDecrypt(
    NvU8 *Buffer,
    NvU32 Size,
    NvBool IsEncrypt,
    NvCryptoCipherAlgoAesKeyType KeyType,
    NvU8 *Key)
{
    NvError e;
    NvCryptoCipherAlgoHandle hCipher = 0;
    NvCryptoCipherAlgoParams cipher_params;
    // allocate cipher algorithm
    NV_CHECK_ERROR_CLEANUP(
        NvCryptoCipherSelectAlgorithm(NvCryptoCipherAlgoType_AesCbc,
                                      &hCipher)
        );

    // configure algorithm
    NvOsMemset(&cipher_params, 0, sizeof(cipher_params));
    cipher_params.AesCbc.IsEncrypt = IsEncrypt;
    cipher_params.AesCbc.KeyType = KeyType;
    cipher_params.AesCbc.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
    cipher_params.AesCbc.PaddingType =
        NvCryptoPaddingType_ExplicitBitPaddingOptional;
    if ((KeyType == NvCryptoCipherAlgoAesKeyType_UserSpecified) && (Key != NULL))
    {
        NvOsMemcpy(cipher_params.AesCbc.KeyBytes ,(NvU8 *)Key,16);
    }
    NV_CHECK_ERROR_CLEANUP(
        hCipher->SetAlgoParams( hCipher, &cipher_params )
        );

    // encrypt/decrypt BCT
    NV_CHECK_ERROR_CLEANUP(
        hCipher->ProcessBlocks( hCipher, Size, Buffer, Buffer,
                                NV_TRUE, NV_TRUE )
        );

fail:

    if( hCipher )
        hCipher->ReleaseAlgorithm( hCipher );
    return e;
}

NvError
BctSignVerify(
    NvU8 *Buffer,
    NvU32 Offset,
    NvU32 Size,
    NvBool IsHashVerify,
    NvCryptoCipherAlgoAesKeyType KeyType)
{
    NvError e;
    NvCryptoHashAlgoHandle hHash = 0;
    NvCryptoHashAlgoParams hash_params;
    NvU32 HashSize;
    NvU8 Hash[NVCRYPTO_CIPHER_AES_BLOCK_BYTES];
    NvBool isValidHash = NV_FALSE;
    NvU32 SignatureOffset = 0;

    NvOsMemset(&Hash[0], 0, sizeof(Hash));
    HashSize = NVCRYPTO_CIPHER_AES_BLOCK_BYTES;
    NV_CHECK_ERROR_CLEANUP(
        NvCryptoHashSelectAlgorithm( NvCryptoHashAlgoType_AesCmac,
                                     &hHash )
        );
    /* sign */
    NvOsMemset(&hash_params, 0, sizeof(hash_params));
    hash_params.AesCmac.IsCalculate = NV_TRUE;
    hash_params.AesCmac.KeyType = KeyType;
    hash_params.AesCmac.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
    hash_params.AesCmac.PaddingType =
        NvCryptoPaddingType_ExplicitBitPaddingOptional;

    hHash->SetAlgoParams( hHash, &hash_params );


    /* compute hash */
    NV_CHECK_ERROR_CLEANUP(
        hHash->ProcessBlocks( hHash, Size, Buffer + Offset,
                              NV_TRUE, NV_TRUE )
        );

    SignatureOffset = NvBctGetSignatureOffset();

    if (IsHashVerify)
    {
        /* verify signature */
        NvOsMemcpy(Hash, Buffer + SignatureOffset, HashSize);
        e = hHash->VerifyHash(hHash, Buffer + SignatureOffset, &isValidHash);
        if (!isValidHash)
        {
            e = NvError_InvalidState;
            NV_CHECK_ERROR_CLEANUP(e);
        }
    }
    else
    {
        NV_CHECK_ERROR_CLEANUP(
            hHash->QueryHash(hHash, &HashSize, Buffer + SignatureOffset)
        );
    }

fail:
    if( hHash )
        hHash->ReleaseAlgorithm( hHash );
    return e;
}

NvError
NvBuBctCryptoOps(
    NvBctHandle hBct,
    NvDdkFuseOperatingMode OperatingMode,
    NvU32 *Size,
    void *Buffer,
    NvBuBlCryptoOp CryptoOp)
{
    NvError e;
    NvU8 *pBuffer = (NvU8 *)Buffer;
    NvU32 HashDataOffset;
    NvU32 HashDataLength;
    NvU32 DataElemSize;
    NvU32 DataElemInstance;
    NvU32 BctSize = 0;
    NvCryptoCipherAlgoAesKeyType KeyType = NvCryptoCipherAlgoAesKeyType_Invalid;
    NvBool IsEncrypt = NV_FALSE;    /* Flag set based on the Bl operating mode */
    NV_ASSERT(CryptoOp <= NvBuBlCryptoOp_Num);

    if (!Size)
        return NvError_InvalidAddress;

    NV_CHECK_ERROR_CLEANUP(
        NvBctInit(&BctSize, NULL, NULL)
        );

    if (*Size == 0)
    {
        *Size = BctSize;

        if (Buffer)
            return NvError_InsufficientMemory;
        else
            return NvSuccess;
    }

    if (*Size < BctSize)
        return NvError_InsufficientMemory;

    if (!Buffer)
        return NvError_InvalidAddress;


    switch (OperatingMode)
    {
        case NvDdkFuseOperatingMode_Preproduction:
        case NvDdkFuseOperatingMode_NvProduction:
            IsEncrypt = NV_FALSE;
            KeyType = NvCryptoCipherAlgoAesKeyType_UserSpecified;
            break;

        case NvDdkFuseOperatingMode_OdmProductionOpen:
            IsEncrypt = NV_FALSE;
            KeyType = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
            break;

        case NvDdkFuseOperatingMode_OdmProductionSecure:
            IsEncrypt = NV_TRUE;
            KeyType = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
            break;

        case NvDdkFuseOperatingMode_OdmProductionSecurePKC:
            IsEncrypt = NV_FALSE;
            break;

        default:
            // unsupported modes
            return NvError_BadParameter;
    }

    /* query offset and length of data to hash */
    DataElemSize = sizeof(NvU32);
    DataElemInstance = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_HashDataOffset,
                     &DataElemSize, &DataElemInstance, &HashDataOffset)
        );

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_HashDataLength,
                     &DataElemSize, &DataElemInstance, &HashDataLength)
        );

    if (OperatingMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC)
    {
        NvU32 Instance =0;
        NvU32 Size = BctSize;

        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(hBct, NvBctDataType_FullContents,
                         &Size, &Instance, pBuffer)
        );
        return e;
    }

    switch (CryptoOp)
    {
        case NvBuBlCryptoOp_EncryptAndSign:
            // Get raw BCT
            DataElemSize = BctSize;
            DataElemInstance = 0;
            NV_CHECK_ERROR_CLEANUP(
                NvBctGetData(hBct, NvBctDataType_FullContents,
                             &DataElemSize, &DataElemInstance, pBuffer)
                );

            if( IsEncrypt )
            {
                NV_CHECK_ERROR_CLEANUP( BctEncryptDecrypt((pBuffer +
                           HashDataOffset), HashDataLength, NV_TRUE, KeyType, NULL)
                    );
            }

            NV_CHECK_ERROR_CLEANUP( BctSignVerify(pBuffer, HashDataOffset,
                        HashDataLength, NV_FALSE, KeyType) );
            break;

        case NvBuBlCryptoOp_VerifyAndDecrypt:
            NV_CHECK_ERROR_CLEANUP( BctSignVerify(pBuffer, HashDataOffset,
                        HashDataLength, NV_TRUE, KeyType) );

            if( IsEncrypt )
            {
                NV_CHECK_ERROR_CLEANUP( BctEncryptDecrypt((pBuffer +
                           HashDataOffset), HashDataLength, NV_FALSE, KeyType, NULL)
                    );
            }
            break;

        case NvBuBlCryptoOp_Verify:
                NV_CHECK_ERROR_CLEANUP( BctSignVerify(pBuffer, HashDataOffset,
                        HashDataLength, NV_TRUE, KeyType) );
            break;

        default:
            return NvError_BadParameter;
    }

fail:
    return e;
}

NvError
GetLastBlockWritten(NvU32 partId, NvU32 StartLogicalSector,
    NvU32 NumLogicalSectors, NvU32 *pLastLogicalBlk, NvU32 *pLastPhysicalBlk)
{
    NvU32 CurrSector;
    NvU32 MaxSector;
    NvU32 sectorsPerBlock, bytesPerSector;
    NvU32 PhysicalSector;
    NvBool FailedMapPhysical = NV_FALSE;
    NvBool FlagNoBlockUsed = NV_TRUE;
    MaxSector = (NumLogicalSectors + StartLogicalSector);
    for (CurrSector = StartLogicalSector; CurrSector < MaxSector; )
    {
        // Function to get physical sector number for given logical sector
        if (NvSysUtilGetDeviceInfo(partId, CurrSector, &sectorsPerBlock,
            &bytesPerSector, &PhysicalSector) == NV_FALSE)
        {
            // Failed to find the physical block for logical block
            // This is first logical block that is unused
            FailedMapPhysical = NV_TRUE;
            break;
        }
        else
        {
            // Found mapping of logical to physical
            // as block in partition has been written
            if (FlagNoBlockUsed == NV_TRUE)
                FlagNoBlockUsed = NV_FALSE;
            *pLastLogicalBlk = CurrSector / sectorsPerBlock;
            *pLastPhysicalBlk = PhysicalSector / sectorsPerBlock;
            CurrSector += sectorsPerBlock;
        }
    }
    // Check if any block in partition is unused
    if ((FailedMapPhysical == NV_TRUE) && (FlagNoBlockUsed == NV_TRUE))
    {
        // Consider error if
        return NvError_NandRegionIllegalAddress;
    }
    else
    {
        // Last known used block is already available in returned values
        return NvSuccess;
    }
}

// ReadOneBct is passed block driver handle corresponding to
// partition id == 0
// PartSectorsPerBlock - Sectors per block based on partition interleave count
NvError
ReadOneBct( NvDdkBlockDevHandle hDev,
            NvU32 startSector,
            NvU32 numSectors,
            NvU32 PartSectorsPerBlock,
            NvU32 startOffset,
            void *readBctBuffer)
{
    NvError e;
    NvDdkBlockDevIoctl_ReadPhysicalSectorInputArgs read_arg;
    NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs query_arg_in;
    NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs query_arg_out;
    NvU8 *tempBuffer = NULL, *bctPtr = NULL;
    NvDdkBlockDevInfo dinfo;
    NvU32 i;

    if (!readBctBuffer)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    hDev->NvDdkBlockDevGetDeviceInfo( hDev, &dinfo );

    // Do not use the sectors per block count for part id = 0
    query_arg_in.BlockNum = startSector / PartSectorsPerBlock;
    NV_CHECK_ERROR_CLEANUP(
        hDev->NvDdkBlockDevIoctl( hDev,
            NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus,
            sizeof(query_arg_in), sizeof(query_arg_out), &query_arg_in, &query_arg_out ));

    //If it's a bad block, skip it
    if (!query_arg_out.IsGoodBlock)
    {
        e = NvError_NandBadBlock;
        goto fail;
    }

    tempBuffer = NvOsAlloc(dinfo.BytesPerSector);
    if (!tempBuffer)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    bctPtr = readBctBuffer;

    read_arg.SectorNum = startSector;
    read_arg.NumberOfSectors = 1;
    read_arg.pBuffer = tempBuffer;
    NV_CHECK_ERROR_CLEANUP(
        hDev->NvDdkBlockDevIoctl( hDev,
            NvDdkBlockDevIoctlType_ReadPhysicalSector,
            sizeof(read_arg), 0, &read_arg, 0 ));

    NvOsMemcpy(bctPtr, tempBuffer+startOffset, dinfo.BytesPerSector-startOffset);
    bctPtr += dinfo.BytesPerSector-startOffset;

    for (i=startSector+1;i<startSector+numSectors;i++)
    {
        read_arg.SectorNum = i;
        read_arg.NumberOfSectors = 1;
        read_arg.pBuffer = tempBuffer;
        NV_CHECK_ERROR_CLEANUP(
            hDev->NvDdkBlockDevIoctl( hDev,
                NvDdkBlockDevIoctlType_ReadPhysicalSector,
                sizeof(read_arg), 0, &read_arg, 0 ));

        NvOsMemcpy(bctPtr, tempBuffer, dinfo.BytesPerSector);
        bctPtr += dinfo.BytesPerSector;
    }

    e = NvSuccess;
    goto clean;

fail:
clean:
    if (tempBuffer)
    {
        NvOsFree(tempBuffer);
        tempBuffer = NULL;
    }
    return e;
}

// WriteOneBct is passed block driver handle corresponding to
// partition id == 0
NvError
WriteOneBct( NvDdkBlockDevHandle hDev,
             NvU32 startSector, NvU32 numSectors,
             NvU32 startBlock, void *bctBuffer)
{
    NvError e;
    NvDdkBlockDevIoctl_WritePhysicalSectorInputArgs sector_arg;
    NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs query_arg_in;
    NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs query_arg_out;

    /* write the sectors, handle the remainder for the last sector */
    /* attempt to write to physical block/sector.
     * 1) erase the block(s)
     * 2) write sectors
     */

    if (!bctBuffer)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    query_arg_in.BlockNum = startBlock;
    NV_CHECK_ERROR_CLEANUP(
        hDev->NvDdkBlockDevIoctl( hDev,
            NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus,
            sizeof(query_arg_in), sizeof(query_arg_out), &query_arg_in, &query_arg_out ));

    //If it's a bad block, skip it
    if (!query_arg_out.IsGoodBlock)
    {
        e = NvSuccess;
        goto fail;
    }

    sector_arg.SectorNum = startSector;
    sector_arg.NumberOfSectors = numSectors;
    sector_arg.pBuffer = bctBuffer;
    NV_CHECK_ERROR_CLEANUP(
        hDev->NvDdkBlockDevIoctl( hDev,
                                  NvDdkBlockDevIoctlType_WritePhysicalSector,
                                  sizeof(sector_arg), 0, &sector_arg, 0 )
    );

    if (query_arg_out.IsGoodBlock)
    {
            NvDdkBlockDevInfo dinfo;
            NvU8 *readBctBuffer = NULL;
            hDev->NvDdkBlockDevGetDeviceInfo( hDev, &dinfo );
            readBctBuffer = NvOsAlloc(numSectors * dinfo.BytesPerSector);

            if (!readBctBuffer)
            {
                e = NvError_InsufficientMemory;
                goto fail;
            }

            NV_CHECK_ERROR_CLEANUP(
                ReadOneBct(hDev, startSector, numSectors,
                dinfo.SectorsPerBlock, 0, readBctBuffer)
                );

        //If the bct's match, return error
        if (!NvOsMemcmp(bctBuffer, readBctBuffer, numSectors* dinfo.BytesPerSector) == 0)
        {
            // Block driver expects buffer to be multiple of page size
            NvOsDebugPrintf("\n Bct read verify failed ");
            // cleanup even if read verify failed
            NvOsFree(readBctBuffer);
            NV_CHECK_ERROR_CLEANUP(
                NvError_BlockDriverWriteVerifyFailed
            );
        }
        NvOsFree(readBctBuffer);
    }

fail:
    return e;
}

void LocateNextBct(
    const NvU32 * BytesPerSector,
    NvU32 BootromBlockSize,
    NvU32 BlockdevBlockSize,
    NvU32 *preferredBctByteAddress,
    NvU32 *startSector
)
{
    *startSector = *preferredBctByteAddress / *BytesPerSector;
    *preferredBctByteAddress += BootromBlockSize;
}

NvError
ComputeBootRomBlockPageSize(
    NvBctHandle hBct,
    NvU32 *BootromBlockSize,
    NvU32 *BootromPageSize)
{
    NvError e = NvSuccess;
    NvU32 bootromBlockSizeLog2, bootromPageSizeLog2, length = sizeof(NvU32);
    NvU32 instance = 0;

    if (BootromBlockSize)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(hBct, NvBctDataType_BootDeviceBlockSizeLog2,
                         &length, &instance, &bootromBlockSizeLog2)
            );
        *BootromBlockSize = 1 << bootromBlockSizeLog2;
    }
    if (BootromPageSize)
    {
        NV_CHECK_ERROR_CLEANUP(NvBctGetData(hBct,
                NvBctDataType_BootDevicePageSizeLog2, &length,
                &instance, &bootromPageSizeLog2));
        *BootromPageSize = 1 << bootromPageSizeLog2;
    }

fail:

    return e;
}

NvError
WriteAllBcts(
             NvBitHandle hBit,
             NvU32 bootromBlockSize,
             NvU32 bootromPageSize,
             NvU32 PartitionId,
             NvU32 bctSize,
             void *buffer)
{
    NvU32 numSectors, startSector, numBlocks, startBlock = 0, partitionSize;
    NvU32 blockdevSlotOneSector,  bootromSlotOnePage;
    NvU32 blockdevBlockSize;
    NvDdkBlockDevInfo dinfo;
    NvError e = NvSuccess;
    NvU32 preferredBctByteAddress = 0;
    NvPartInfo PTInfo;
    NvU8 *writeBctBuffer = NULL;
    NvU32 Sector;
    NvU32 Size;
    NvU32 Instance = 0;
    NvFsMountInfo minfo;
    NvDdkBlockDevHandle hDev = 0;
    NvU32 GoodBctCount = 0;
    NvU32 BctCount = 0; //include both successfull and unsuccessfull bct writes
    NvU32 AllocSize = 0, i = 0;
    NvDdkBlockDevIoctl_ErasePhysicalBlockInputArgs erase_arg;

    Size = sizeof(NvU32);
    NV_CHECK_ERROR(
        NvBitGetData(hBit, NvBitDataType_ActiveBctSector, &Size,
                     &Instance, &Sector)
        );

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetFsInfo( PartitionId, &minfo )
    );

    // Except for read PT(we use part-id==0) we should open with partition id
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, PartitionId, &hDev )
    );

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetPartInfo( PartitionId, &PTInfo)
        );

    hDev->NvDdkBlockDevGetDeviceInfo( hDev, &dinfo );

    numSectors = ( (bctSize +  dinfo.BytesPerSector - 1)/ dinfo.BytesPerSector);
    blockdevBlockSize = dinfo.BytesPerSector * dinfo.SectorsPerBlock;
    AllocSize = numSectors * dinfo.BytesPerSector;

    writeBctBuffer  = NvOsAlloc(AllocSize);
    if (!writeBctBuffer)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(writeBctBuffer, 0,AllocSize);
    NvOsMemcpy(writeBctBuffer, buffer, bctSize);
    //Figure out where "slot 1" is in terms
    //of the bootrom page size.
    bootromSlotOnePage = (bctSize + bootromPageSize - 1)/ bootromPageSize;
    blockdevSlotOneSector = (bootromSlotOnePage * bootromPageSize) / dinfo.BytesPerSector;

    hDev->NvDdkBlockDevClose(hDev);

    // As an exception we need part-id==0 access as
    // BCT is written/read in raw mode by calling physical write/read IOCTLs
    // One reason we may be avoiding BCT write using FTL is we write Block 0
    // and Page 0 at the end when all other BCT copies have been written
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, 0, &hDev)
    );
    numBlocks = PTInfo.NumLogicalSectors / dinfo.SectorsPerBlock;

    /* erase the blocks before writing bct partition*/
    erase_arg.NumberOfBlocks = 1;
    erase_arg.IsTrimErase = NV_FALSE;
    erase_arg.IsSecureErase = NV_FALSE;
    for (i = 0; i < numBlocks; i++)
    {
        erase_arg.BlockNum = i;
        e = hDev->NvDdkBlockDevIoctl(hDev,
                NvDdkBlockDevIoctlType_ErasePhysicalBlock,
                sizeof(erase_arg), 0, &erase_arg, 0);
        // FIXME: should update bad block info into bct if erase fails for nand
        // ignore erase error for nand due to bad blocks otherwise sync will fail
        if (e && (minfo.DeviceId != NvDdkBlockDevMgrDeviceId_Nand))
            goto fail;
    }
    //If the active BCT is in block 0, slot 1, then
    //don't overwrite it. It's our known good bct.
#if TEST_RECOVERY
    //FIXME: Remove explicit BCT corruption before release
    // if (startBlock == 0)
    if (startBlock == 0 && bIsInterruptedUpdate)
#else
    if (startBlock == 0)
#endif
    {
        // slot 1 bct is written when slot 1 offset is multiple of sector size of block driver
        if (!((bootromSlotOnePage * bootromPageSize) % dinfo.BytesPerSector))
        {
            if ((e = WriteOneBct( hDev, blockdevSlotOneSector, numSectors,
                0, writeBctBuffer)) == NvSuccess) {
                // Successful bootup needs at least one good BCT copy
                GoodBctCount++;
            }
            BctCount++;
        }
    }
    // Removed map logical call as it is accessing area not part of partition.
    // For non-interleaved BCT partition, the 2nd chip not getting used.
    // In this case the map logical to physical with the BCT partition
    // actual size fails.
    partitionSize = (NvU32)PTInfo.NumLogicalSectors * dinfo.BytesPerSector;
    // below loop for bct sync should start from second block since
    // slot0 is written at last and/or slot1 as by above code.
    preferredBctByteAddress = bootromBlockSize;

    while (preferredBctByteAddress < partitionSize)
    {
        //Get the start of the bct
        LocateNextBct(&dinfo.BytesPerSector, bootromBlockSize,
            blockdevBlockSize, &preferredBctByteAddress, &startSector);

        //The block the bct belongs to (we need to erase the block)
        startBlock = startSector / dinfo.SectorsPerBlock;

        // Block driver expects buffer to be multiple of page size
        if ((e = WriteOneBct( hDev, startSector, numSectors,
            startBlock, writeBctBuffer)) == NvSuccess)
            GoodBctCount++;
        BctCount++;
        // should be matched with 63 copies since last one is written at end outside this loop
        if (BctCount == MAX_BCT_SYNC_COPY - 1)
            break;
    }

#if !TEST_RECOVERY
    //Finally, write to Block 0, Slot 0
    if ((e = WriteOneBct( hDev, 0, numSectors, 0, writeBctBuffer)) == NvSuccess)
        GoodBctCount++;
#endif
    // If no good BCT copy found return error
    if (!GoodBctCount)
    {
        NvOsDebugPrintf("\n Error Bct Verify: NO valid Bct found ");
        e = NvError_BlockDriverWriteVerifyFailed;
    }
    else
        // We needs atleast ONE valid BCT if there is one valid
        // BCT return success
        e = NvSuccess;
fail:
    if (hDev)
        hDev->NvDdkBlockDevClose(hDev);
    NvOsFree(writeBctBuffer);
    return e;
}

NvError
NvBuBctUpdate(NvBctHandle hBct,
              NvBitHandle hBit,
              NvU32 PartitionId,
              NvU32 size,
              void *buffer)
{
    NvError e = NvSuccess;
    NvU32 bootromBlockSize;
    NvU32 bootromPageSize;
    NvU32 BctSize = 0;

    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctSize, NULL, NULL));

    if (size != BctSize)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        ComputeBootRomBlockPageSize( hBct, &bootromBlockSize, &bootromPageSize)
    );

    NV_CHECK_ERROR_CLEANUP(
        WriteAllBcts(hBit, bootromBlockSize, bootromPageSize,
                     PartitionId, size, buffer)
    );

fail:
    return e;
}

NvError NvBuBctGetSize(NvBctHandle hBct,
               NvBitHandle hBit,
               NvU32 PartitionId, NvU32 *bufferSize)
{

    NvError e = NvSuccess;
    NvU32 bootromBlockSize, bootromPageSize;
    NvU32 BctSize = 0;
    NvU32 startOffset, endOffset;
    NvU32 byteStart;

    NvFsMountInfo minfo;
    NvDdkBlockDevHandle hDev = 0;
    NvDdkBlockDevInfo dinfo;

    NvU32 Block;
    NvU32 Sector;
    NvU32 Size;
    NvU32 Instance = 0;

    Size = sizeof(NvU32);
    NV_CHECK_ERROR(
        NvBitGetData(hBit, NvBitDataType_ActiveBctBlock, &Size,
                     &Instance, &Block)
        );

    Size = sizeof(NvU32);
    NV_CHECK_ERROR(
        NvBitGetData(hBit, NvBitDataType_ActiveBctSector, &Size,
                     &Instance, &Sector)
    );

    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctSize, NULL, NULL));


    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetFsInfo( PartitionId, &minfo )
    );

    // By default need partition id specific block driver handle
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, PartitionId, &hDev )
    );

    hDev->NvDdkBlockDevGetDeviceInfo( hDev, &dinfo );

    NV_CHECK_ERROR_CLEANUP(
        ComputeBootRomBlockPageSize( hBct, &bootromBlockSize, &bootromPageSize)
    );

    byteStart = (Sector * bootromPageSize) + (Block * bootromBlockSize);
    startOffset = byteStart % dinfo.BytesPerSector;
    endOffset = dinfo.BytesPerSector - (byteStart+BctSize) % dinfo.BytesPerSector;

    *bufferSize = startOffset + BctSize+endOffset;

fail:
    if (hDev)
        hDev->NvDdkBlockDevClose( hDev );

    return e;
}

/*
 * Code taken from ReadVerify
 **/
NvError NvBuBctRead(NvBctHandle hBct,
               NvBitHandle hBit,
               NvU32 PartitionId,NvU8 *buffer)
{
    NvError e = NvSuccess;
    NvU32 bootromBlockSize;

    NvU32 BctSize = 0;
    NvU32 Size;
    NvFsMountInfo minfo;
    NvDdkBlockDevInfo dinfo;
    NvDdkBlockDevHandle hDev = 0;
    NvDdkFuseOperatingMode OperatingMode;

    NvU32 numSectors, startSector,  partitionSize;
    NvU32 blockdevBlockSize;
    NvPartInfo PTInfo;
    NvU32 preferredBctByteAddress = 0;

    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctSize, NULL, NULL));

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetFsInfo( PartitionId, &minfo )
    );

    // By default need partition id specific block driver handle
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, PartitionId, &hDev )
    );

    hDev->NvDdkBlockDevGetDeviceInfo( hDev, &dinfo );

    NV_CHECK_ERROR_CLEANUP(
        ComputeBootRomBlockPageSize( hBct, &bootromBlockSize, NULL)
    );

    numSectors = ( (BctSize +  dinfo.BytesPerSector - 1)/ dinfo.BytesPerSector);

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetPartInfo( PartitionId, &PTInfo)
    );

    blockdevBlockSize = dinfo.BytesPerSector * dinfo.SectorsPerBlock;
    partitionSize = (NvU32)PTInfo.NumLogicalSectors * dinfo.BytesPerSector;

    // Close partition id specific block driver handle and get part-id==0 handle
    hDev->NvDdkBlockDevClose( hDev );

    // As an exception we need part-id==0 access as
    // BCT is written/read in raw mode by calling physical write/read IOCTLs
    // One reason we may be avoiding BCT write using FTL is we write Block 0
    // and Page 0 at the end when all other BCT copies have been written
    // FTL only allows write to increasing addresses
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, 0, &hDev )
    );

    Size =  sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&OperatingMode, (NvU32 *)&Size));

    if (OperatingMode == NvDdkFuseOperatingMode_Undefined)
        NV_CHECK_ERROR_CLEANUP(NvError_InvalidState);

    while (preferredBctByteAddress < partitionSize)
    {
        LocateNextBct(&dinfo.BytesPerSector, bootromBlockSize,
            blockdevBlockSize, &preferredBctByteAddress, &startSector);
        //Read the next BCT if read error, rather than returning
        e = ReadOneBct( hDev, startSector, numSectors,
                dinfo.SectorsPerBlock, 0, buffer);
        if (e != NvSuccess && e != NvError_NandErrorThresholdReached)
            continue;

        e = NvBuBctCryptoOps(hBct, OperatingMode, &BctSize, buffer, NvBuBlCryptoOp_Verify);
        if (e == NvSuccess)
            break;
    }

    if(preferredBctByteAddress >= partitionSize)
        e = NvError_InvalidState;

fail:
    if (hDev)
        hDev->NvDdkBlockDevClose( hDev );

    return e;
}

NvError
NvBuBctReadVerify(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 PartitionId,
    NvDdkFuseOperatingMode OperatingMode,
    NvU32 size)
{
    NvError e = NvSuccess;
    NvU32 bootromBlockSize;

    NvU32 BctSize = 0;
    NvFsMountInfo minfo;
    NvDdkBlockDevInfo dinfo;
    NvDdkBlockDevHandle hDev = 0;

    void *buffer = NULL;

    NvU32 numSectors, startSector,  partitionSize;
    NvU32 blockdevBlockSize;
    NvPartInfo PTInfo;
    NvU32 preferredBctByteAddress = 0;

    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctSize, NULL, NULL));

    if (size != BctSize)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetFsInfo( PartitionId, &minfo )
    );

    // By default need partition id specific block driver handle
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, PartitionId, &hDev )
    );

    hDev->NvDdkBlockDevGetDeviceInfo( hDev, &dinfo );

    NV_CHECK_ERROR_CLEANUP(
        ComputeBootRomBlockPageSize( hBct, &bootromBlockSize, NULL)
    );

    numSectors = ( (size +  dinfo.BytesPerSector - 1)/ dinfo.BytesPerSector);

    buffer = NvOsAlloc(numSectors * dinfo.BytesPerSector);

    if (!buffer)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetPartInfo( PartitionId, &PTInfo)
        );

    blockdevBlockSize = dinfo.BytesPerSector * dinfo.SectorsPerBlock;
    partitionSize = (NvU32)PTInfo.NumLogicalSectors * dinfo.BytesPerSector;

    // Close partition id specific block driver handle and get part-id==0 handle
    hDev->NvDdkBlockDevClose( hDev );

    // As an exception we need part-id==0 access as
    // BCT is written/read in raw mode by calling physical write/read IOCTLs
    // One reason we may be avoiding BCT write using FTL is we write Block 0
    // and Page 0 at the end when all other BCT copies have been written
    // FTL only allows write to increasing addresses
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, 0, &hDev )
    );

    while (preferredBctByteAddress < partitionSize)
    {
        LocateNextBct(&dinfo.BytesPerSector, bootromBlockSize,
            blockdevBlockSize, &preferredBctByteAddress, &startSector);

        NV_CHECK_ERROR_CLEANUP(
                ReadOneBct( hDev, startSector, numSectors,
                dinfo.SectorsPerBlock, 0, buffer)
            );

        e = NvBuBctCryptoOps(hBct, OperatingMode, &size, buffer, NvBuBlCryptoOp_Verify);
        if (e == NvSuccess)
            break;
    }

    if(preferredBctByteAddress >= partitionSize)
        e = NvError_InvalidState;

fail:

    if (hDev)
        hDev->NvDdkBlockDevClose( hDev );
    if(buffer)
        NvOsFree(buffer);

    return e;
}

NvError
NvBuBctRecover(NvBctHandle hBct,
               NvBitHandle hBit,
               NvU32 PartitionId)
{

    NvError e = NvSuccess;
    NvU32 bootromBlockSize, bootromPageSize;
    NvU32 BctSize = 0;
    NvU32 startOffset, endOffset, bufferSize;
    NvU32 numSlotOneSectors, startReadSector, byteStart;
#if TEST_RECOVERY
    NvU8 *tempPtr = NULL;
#endif
    NvU8 *buffer = NULL;
    NvFsMountInfo minfo;
    NvDdkBlockDevHandle hDev = 0;
    NvDdkBlockDevInfo dinfo;

    NvU32 Block;
    NvU32 Sector;
    NvU32 Size;
    NvU32 Instance = 0;

    Size = sizeof(NvU32);
    NV_CHECK_ERROR(
        NvBitGetData(hBit, NvBitDataType_ActiveBctBlock, &Size,
                     &Instance, &Block)
        );

    Size = sizeof(NvU32);
    NV_CHECK_ERROR(
        NvBitGetData(hBit, NvBitDataType_ActiveBctSector, &Size,
                     &Instance, &Sector)
        );

    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctSize, NULL, NULL));


    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetFsInfo( PartitionId, &minfo )
    );

    // By default need partition id specific block driver handle
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, PartitionId, &hDev )
    );

    hDev->NvDdkBlockDevGetDeviceInfo( hDev, &dinfo );


    NV_CHECK_ERROR_CLEANUP(
        ComputeBootRomBlockPageSize( hBct, &bootromBlockSize, &bootromPageSize)
    );

    //We need to figure out where, exactly, our active BCT is.
    //It might not be aligned with device sectors, so properly
    //figure out the startSector and offset within that sector.
    byteStart = Sector * bootromPageSize + Block * bootromBlockSize;
    startOffset = byteStart % dinfo.BytesPerSector;
    endOffset = dinfo.BytesPerSector - (byteStart+BctSize) % dinfo.BytesPerSector;
    startReadSector =  byteStart / dinfo.BytesPerSector;

    bufferSize = startOffset+BctSize+endOffset;
    numSlotOneSectors = bufferSize / dinfo.BytesPerSector;

    buffer = NvOsAlloc(bufferSize);

    if (!buffer)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    // Close partition id specific block driver handle and get part-id==0 handle
    hDev->NvDdkBlockDevClose( hDev );

    // As an exception we need part-id==0 access as
    // BCT is written/read in raw mode by calling physical write/read IOCTLs
    // One reason we may be avoiding BCT write using FTL is we write Block 0
    // and Page 0 at the end when all other BCT copies have been written
    // FTL only allows write to increasing addresses
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, 0, &hDev )
    );

    // Handle for raw device access is used from above as sectors/block is
    // not used in calculations. Note WriteOneBct and ReadOneBct need block
    // driver handle with partition id zero
    NV_CHECK_ERROR_CLEANUP(
        ReadOneBct( hDev, startReadSector, numSlotOneSectors,
            dinfo.SectorsPerBlock, startOffset, buffer)
        );

#if TEST_RECOVERY
    //FIXME: Remove explicit BCT corruption before release
    tempPtr = buffer;
    tempPtr[0]--;

    bIsInterruptedUpdate = NV_TRUE;
#endif
    // Write all bcts opens block driver handle inside implementation,
    // caller does not pass the block driver handle now
    NV_CHECK_ERROR_CLEANUP(
                WriteAllBcts(hBit, bootromBlockSize, bootromPageSize,
                        PartitionId, BctSize, buffer)
        );

fail:
    if (hDev)
        hDev->NvDdkBlockDevClose( hDev );
    NvOsFree(buffer);
    return e;
}

NvError
GetBlInfo(
    NvBctHandle hBct,
    NvU32 Slot,
    NvBctBootLoaderInfo *pBlInfo)
{
    NvError e;
    NvU32 Size;

    NV_ASSERT(hBct);
    NV_ASSERT(pBlInfo);

    Size = sizeof(NvU32);
    pBlInfo->Attribute = 0;
    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderAttribute,
                     &Size, &Slot, &pBlInfo->Attribute)
        );

    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderVersion,
                     &Size, &Slot, &pBlInfo->Version)
        );

    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderStartBlock,
                     &Size, &Slot, &pBlInfo->StartBlock)
        );

    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderStartSector,
                     &Size, &Slot, &pBlInfo->StartSector)
        );

    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderLength,
                     &Size, &Slot, &pBlInfo->Length)
        );

    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderLoadAddress,
                     &Size, &Slot, &pBlInfo->LoadAddress)
        );

    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderEntryPoint,
                     &Size, &Slot, &pBlInfo->EntryPoint)
        );

    Size = sizeof(pBlInfo->CryptoHash);
    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderCryptoHash,
                     &Size, &Slot, &pBlInfo->CryptoHash)
        );

    return NvSuccess;
}

NvError
SetBlInfo(
    NvBctHandle hBct,
    NvU32 Slot,
    NvBctBootLoaderInfo *pBlInfo)
{
    NvError e;
    NvU32 Size;
    NvU32 PartitionId;
    NvDdkFuseOperatingMode OpMode;

    NV_ASSERT(hBct);
    NV_ASSERT(pBlInfo);

    // Get current operating mode
    Size =  sizeof(NvU32);
    NV_CHECK_ERROR(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&OpMode, (NvU32 *)&Size));

    Size = sizeof(NvU32);
    NV_CHECK_ERROR(
        NvBctSetData(hBct, NvBctDataType_BootLoaderAttribute,
                     &Size, &Slot, &pBlInfo->Attribute)
        );

    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(hBct, NvBctDataType_BootLoaderStartBlock,
                     &Size, &Slot, &pBlInfo->StartBlock)
        );

    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(hBct, NvBctDataType_BootLoaderStartSector,
                     &Size, &Slot, &pBlInfo->StartSector)
        );

    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(hBct, NvBctDataType_BootLoaderLength,
                     &Size, &Slot, &pBlInfo->Length)
        );

    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(hBct, NvBctDataType_BootLoaderVersion,
                     &Size, &Slot, &pBlInfo->Version)
        );

    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(hBct, NvBctDataType_BootLoaderLoadAddress,
                     &Size, &Slot, &pBlInfo->LoadAddress)
        );

    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(hBct, NvBctDataType_BootLoaderEntryPoint,
                     &Size, &Slot, &pBlInfo->EntryPoint)
        );

    Size = sizeof(pBlInfo->CryptoHash);
    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(hBct, NvBctDataType_BootLoaderCryptoHash,
                     &Size, &Slot, &pBlInfo->CryptoHash)
        );

    return NvSuccess;

fail:

    // mark BL slot in BCT as not containing valid data
    Size = sizeof(NvU32);
    PartitionId = 0;
    (void) NvBctSetData(hBct, NvBctDataType_BootLoaderAttribute,
                        &Size, &Slot, &PartitionId);

    return e;
}

NvError
GetBlSlot(
    NvBctHandle hBct,
    NvU32 PartitionId,
    NvBool *IsFound,
    NvU32 *Slot)
{
    NvU32 MaxBl;
    NvU32 Size;
    NvU32 i;
    NvU32 Instance;
    NvU32 Id = 0;
    NvError e;

    NV_ASSERT(hBct);
    NV_ASSERT(IsFound);
    NV_ASSERT(Slot);

    *IsFound = NV_FALSE;

    Size = 0;
    MaxBl = 0;

    e = NvBctGetData(hBct, NvBctDataType_BootLoaderAttribute, &Size, &MaxBl, NULL);
    if (e != NvSuccess && e != NvError_InsufficientMemory)
        return e;

    for (i=0; i<MaxBl; i++)
    {
        Size = sizeof(NvU32);
        Instance = i;
        NV_CHECK_ERROR(
            NvBctGetData(hBct, NvBctDataType_BootLoaderAttribute,
                         &Size, &Instance, &Id)
            );

        if (Id == PartitionId)
        {
            *IsFound = NV_TRUE;
            *Slot = i;
            break;
        }
    }

    return NvSuccess;
}

NvError
AllocateBlSlot(
    NvBctHandle hBct,
    NvU32 *Slot)
{
    NvU32 MaxBl;
    NvU32 Size;
    NvU32 NumBootloaders;
    NvError e;

    NV_ASSERT(hBct);
    NV_ASSERT(Slot);

    Size = 0;
    MaxBl = 0;

    e = NvBctGetData(hBct, NvBctDataType_BootLoaderVersion, &Size, &MaxBl, NULL);
    if (e != NvSuccess && e != NvError_InsufficientMemory)
        return e;

    NV_CHECK_ERROR(GetNumInstalledBls(hBct, &NumBootloaders));

    if (NumBootloaders >= MaxBl)
        return NvError_BadParameter;

    *Slot = NumBootloaders;

    return NvSuccess;
}

NvError
GetNumInstalledBls(
    NvBctHandle hBct,
    NvU32 *NumBootloaders)
{
    NvError e;
    NvU32 i;
    NvU32 MaxBl;
    NvU32 Size;

    NvU32 Id = 0;

    NV_ASSERT(hBct);
    NV_ASSERT(NumBootloaders);

    Size = 0;
    MaxBl = 0;

    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderAttribute,
                     &Size, &MaxBl, NULL)
        );

    Size = sizeof(NvU32);

    for (i=0; i<MaxBl; i++)
    {
        NV_CHECK_ERROR(
            NvBctGetData(hBct, NvBctDataType_BootLoaderAttribute,
                         &Size, &i, &Id)
            );

        if (Id == 0)
            break;
    }

    *NumBootloaders = i;

    return NvSuccess;
}

NvError
NvBuQueryBlPartitionIdBySlot(
    NvBctHandle hBct,
    NvU32 Slot,
    NvU32 *PartitionId)
{
    NvError e;
    NvU32 Size = sizeof(NvU32);

    if (!hBct)
        return NvError_InvalidAddress;

    if (!PartitionId)
        return NvError_InvalidAddress;

    *PartitionId = 0;
    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderAttribute,
                     &Size, &Slot, PartitionId)
        );

    return NvSuccess;
}

NvError
NvBuQueryBlSlotByPartitionId(
    NvBctHandle hBct,
    NvU32 PartitionId,
    NvU32 *Slot)
{
    NvError e;
    NvBool IsFound = NV_FALSE;

    if (!hBct)
        return NvError_InvalidAddress;

    if (!Slot)
        return NvError_InvalidAddress;

    NV_CHECK_ERROR(
        GetBlSlot(hBct, PartitionId, &IsFound, Slot)
        );

    if (!IsFound)
        return NvError_BadParameter;

    return NvSuccess;
}

NvError
NvBuQueryBlVersion(
    NvBctHandle hBct,
    NvU32 Slot,
    NvU32 *Version)
{
    NvError e;
    NvU32 Size = sizeof(NvU32);

    if (!hBct)
        return NvError_InvalidAddress;

    if (!Version)
        return NvError_InvalidAddress;

    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_BootLoaderVersion,
                     &Size, &Slot, Version)
        );

    return NvSuccess;
}

NvError
NvBuQueryBlGeneration(
    NvBctHandle hBct,
    NvU32 Slot,
    NvU32 *Generation)
{
    NvError e;
    NvU32 Size = sizeof(NvU32);
    NvU32 Instance = 0;
    NvU32 EnabledBlSlots;
    NvU32 i;
    NvU32 PreviousVersion;
    NvU32 CurrentVersion;

    if (!hBct)
        return NvError_InvalidAddress;

    if (!Generation)
        return NvError_InvalidAddress;

    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_NumEnabledBootLoaders,
                     &Size, &Instance, &EnabledBlSlots)
        );

    if (Slot >= EnabledBlSlots)
        return NvError_BadParameter;

    NV_CHECK_ERROR(NvBuQueryBlVersion(hBct, 0, &PreviousVersion));

    *Generation = 0;

    for (i=1; i<EnabledBlSlots; i++)
    {
       NV_CHECK_ERROR(NvBuQueryBlVersion(hBct, i, &CurrentVersion));

       if (CurrentVersion != PreviousVersion)
       {
           (*Generation)++;
           PreviousVersion = CurrentVersion;
       }
    }

    return NvSuccess;
}

NvError
NvBuUpdateBlInfo(
    NvBctHandle hBct,
    NvU32 PartitionId,
    NvU32 Version,
    NvU32 StartBlock,
    NvU32 StartSector,
    NvU32 Length,
    NvU32 LoadAddress,
    NvU32 EntryPoint,
    NvU32 CryptoHashSize,
    NvU8 *CryptoHash)
{
    NvError e;
    NvU32 Slot;
    NvU32 Size;
    NvBool IsFound;
    NvBctBootLoaderInfo BlInfo;
    NvDdkFuseOperatingMode OpMode;

    if (!hBct)
        return NvError_BadParameter;

    Size =  sizeof(NvU32);
    NV_CHECK_ERROR(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&OpMode, (NvU32 *)&Size));

    if (OpMode != NvDdkFuseOperatingMode_OdmProductionSecure)
        if (!CryptoHash)
            return NvError_InvalidAddress;

    if (!PartitionId)
        return NvError_BadParameter;

    if (EntryPoint < LoadAddress)
        return NvError_BadParameter;

    if (EntryPoint >= LoadAddress + Length)
        return NvError_BadParameter;

    if (!CryptoHashSize)
        return NvError_BadParameter;

    if (CryptoHashSize != sizeof(BlInfo.CryptoHash))
        return NvError_BadParameter;

    NV_CHECK_ERROR(
        GetBlSlot(hBct, PartitionId, &IsFound, &Slot)
        );

    if (!IsFound)
        return NvError_BadParameter;

    // dont set version, load addres, entry point and hash of bootloader in bct,
    // in odm secure mode since these information are filled by nvsbktool utility
    // CryptoHash is made as null in odm secure mode
    if (!CryptoHash)
        NV_CHECK_ERROR(GetBlInfo(hBct, Slot, &BlInfo));
    else
    {
        BlInfo.Version = Version;
        BlInfo.LoadAddress = LoadAddress;
        BlInfo.EntryPoint = EntryPoint;
        NvOsMemcpy(BlInfo.CryptoHash, CryptoHash, sizeof(BlInfo.CryptoHash));
    }
    BlInfo.Attribute = PartitionId;
    BlInfo.StartBlock = StartBlock;
    BlInfo.StartSector = StartSector;
    BlInfo.Length = Length;

    NV_CHECK_ERROR(SetBlInfo(hBct, Slot, &BlInfo));

    return NvSuccess;
}

NvError
NvBuSetBlBootable(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 PartitionId)
{
    NvError e;
    NvU32 NumBl = 0;
    NvBuBlInfo *BlInfo = NULL;
    NvBuBlInfo BlInfoEntry;
    NvU32 BlIndex;
    NvBool IsFound = NV_FALSE;
    NvU32 i, j;
    NvU32 Slot = 0;
    NvU32 Size;
    NvU32 CurrBlLoadAddr = 0;
    NvU32 PartBlLoadAddr = 0;

    if (!hBct)
        return NvError_InvalidAddress;

    if (!hBit)
        return NvError_InvalidAddress;

    if (!PartitionId)
        return NvError_BadParameter;

    // get current boot order
    NV_CHECK_ERROR_CLEANUP(NvBuQueryBlOrder(hBct, hBit, &NumBl, NULL));

    BlInfo = NvOsAlloc(NumBl * sizeof(NvBuBlInfo));
    if( !BlInfo )
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(NvBuQueryBlOrder(hBct, hBit, &NumBl, BlInfo));

    // find specified partition in the boot order
    for (i=0; i<NumBl; i++)
    {
        if (BlInfo[i].PartitionId == PartitionId)
        {
            IsFound = NV_TRUE;
            break;
        }
    }

    if (!IsFound)
    {
        e = NvError_BadValue;
        goto fail;
    }

    BlIndex = i;

    // set partition as bootable
    BlInfo[BlIndex].IsBootable = NV_TRUE;

    NV_CHECK_ERROR(
        GetBlSlot(hBct, PartitionId, &IsFound, &Slot)
    );
    Size = sizeof(NvU32);
    if(IsFound)
    {
        NV_CHECK_ERROR(
            NvBctGetData(hBct, NvBctDataType_BootLoaderLoadAddress, &Size, &Slot, &CurrBlLoadAddr)
        );
    }

    for(j = 0; j < BlIndex; j++)
    {
        NV_CHECK_ERROR(
            GetBlSlot(hBct, BlInfo[j].PartitionId, &IsFound, &Slot)
        );
        Size = sizeof(NvU32);
        NV_CHECK_ERROR(
            NvBctGetData(hBct, NvBctDataType_BootLoaderLoadAddress, &Size, &Slot, &PartBlLoadAddr)
        );
        if(PartBlLoadAddr == CurrBlLoadAddr)
            break;
        else
        {
            if((CurrBlLoadAddr & MICROBOOT_LOAD_REGION) == MICROBOOT_LOAD_REGION)
                break;
            else
                continue;
        }
    }
    // move newly-bootable partition to head of list
    BlInfoEntry = BlInfo[BlIndex];

    for (i = BlIndex; i != j; i--)
    {
        BlInfo[i] = BlInfo[i-1];
    }

    BlInfo[j] = BlInfoEntry;

    // update boot order in BCT
    NV_CHECK_ERROR_CLEANUP(NvBuUpdateBlOrder(hBct, hBit, &NumBl, BlInfo));

    e = NvSuccess;
    // fall through

fail:
    if (BlInfo)
        NvOsFree(BlInfo);

    return e;
}

NvError
NvBuQueryIsInterruptedUpdate(
    NvBitHandle hBit,
    NvBool *pIsInterruptedUpdate)
{
    NvError e;

    NvU32 Block;
    NvU32 Sector;
    NvU32 Size;
    NvU32 Instance = 0;

    Size = sizeof(NvU32);
    NV_CHECK_ERROR(
        NvBitGetData(hBit, NvBitDataType_ActiveBctBlock, &Size,
                     &Instance, &Block)
        );

    Size = sizeof(NvU32);
    NV_CHECK_ERROR(
        NvBitGetData(hBit, NvBitDataType_ActiveBctSector, &Size,
                     &Instance, &Sector)
        );

    if (Block == 0 && Sector == 0)
        *pIsInterruptedUpdate = NV_FALSE;
    else
        *pIsInterruptedUpdate = NV_TRUE;

    return NvSuccess;
}

NvError
NvBuQueryActiveBlSlot(
    NvBitHandle hBit,
    NvU32 *pSlot)
{
    NvU32 i;
    NvU32 NumBl;
    NvU32 Size;
    NvU32 Instance;
    NvU32 BlStatus;
    NvBool BlFound = NV_FALSE;
    NvError e;

    if (!hBit)
        return NvError_BadParameter;

    if (!pSlot)
        return NvError_InvalidAddress;

    // get number of BL's
    Size = 0;
    Instance = 0;
    e = NvBitGetData(hBit, NvBitDataType_BlStatus, &Size, &Instance, NULL);
    if (e != NvSuccess && e != NvError_InsufficientMemory)
        return e;

    NV_ASSERT(Size == sizeof(NvU32));

    if (Size != sizeof(NvU32))
        return NvError_InvalidSize;

    NumBl = Instance;

    // check status of each BL, looking for the BL that was loaded successfully
    for (i=0; i<NumBl; i++)
    {
        Instance = i;
        NV_CHECK_ERROR(
            NvBitGetData(hBit, NvBitDataType_BlStatus, &Size, &Instance, &BlStatus)
            );

        if (NvBitGetBitStatusFromBootRdrStatus(BlStatus) == NvBitStatus_Success)
        {
            BlFound = NV_TRUE;
            *pSlot = i;
            break;
        }
    }

    NV_ASSERT(BlFound == NV_TRUE);
    if (!BlFound)
        return NvError_InvalidState;

    return NvSuccess;
}

NvError
NvBuAddBlPartition(
    NvBctHandle hBct,
    NvU32 PartitionId)
{
    NvError e;
    NvU32 Slot;
    NvU32 Size;

    if (!hBct)
        return NvError_InvalidAddress;

    if (!PartitionId)
        return NvError_BadParameter;

    // check is partition id already exists
    e = NvBuQueryBlSlotByPartitionId(hBct, PartitionId, &Slot);
    if (e == NvSuccess)
        return NvError_BadParameter;

    // allocate new slot
    NV_CHECK_ERROR(AllocateBlSlot(hBct, &Slot));

    // store partition id
    Size = sizeof(NvU32);

    NV_CHECK_ERROR(
        NvBctSetData(hBct, NvBctDataType_BootLoaderAttribute,
                     &Size, &Slot, &PartitionId)
        );

    return NvSuccess;
}

NvError
NvBuAddHashPartition(
    NvBctHandle hBct,
    NvU32 PartitionId)
{
    NvError e;
    NvU32 Slot;
    NvU32 Size;
    NvU32 Instance;
    NvU32 MaxPartition;
    NvU32 Id;
    NvU32 i;

    if (!hBct)
        return NvError_InvalidAddress;

    if (!PartitionId)
        return NvError_BadParameter;

    // check is partition id already exists
    Size = 0;
    MaxPartition = 0;

    e = NvBctGetData(hBct, NvBctDataType_HashedPartition_PartId, &Size, &MaxPartition, NULL);
    if (e != NvSuccess && e != NvError_InsufficientMemory)
        return e;

    for (i=0; i<MaxPartition; i++)
    {
        Size = sizeof(NvU32);
        Instance = i;
        NV_CHECK_ERROR(
            NvBctGetData(hBct, NvBctDataType_HashedPartition_PartId,
                         &Size, &Instance, &Id)
        );
        //If Partition is allocated and there should not be request for the same part-id again
        if (Id == PartitionId)
        {
            return NvError_BadParameter;
        }
        // End of the Allocation with Id = 0
        if (Id == 0)
        {
            Slot = i;
            break;
        }
    }
    if (i == MaxPartition)
        return NvError_OverFlow;
    // store partition id
    Size = sizeof(NvU32);

    NV_CHECK_ERROR(
        NvBctSetData(hBct, NvBctDataType_HashedPartition_PartId,
                     &Size, &Slot, &PartitionId)
    );

    return NvSuccess;
}

NvError
NvBuUpdateHashPartition(
    NvBctHandle hBct,
    NvU32 PartitionId,
    NvU32 HashSize,
    NvU8* Hash)
{
    NvError e;
    NvU32 Slot;
    NvU32 Size;
    NvU32 MaxPartition;
    NvU32 Instance;
    NvU32 Id;
    NvU32 i;

    if (!hBct)
        return NvError_InvalidAddress;

    if (!PartitionId)
        return NvError_BadParameter;

    // check is partition id already exists
    Size = 0;
    MaxPartition = 0;

    e = NvBctGetData(hBct, NvBctDataType_HashedPartition_PartId, &Size, &MaxPartition, NULL);
    if (e != NvSuccess )
        return e;

    for (i=0; i<MaxPartition; i++)
    {
        Size = sizeof(NvU32);
        Instance = i;
        NV_CHECK_ERROR(
            NvBctGetData(hBct, NvBctDataType_HashedPartition_PartId,
                         &Size, &Instance, &Id)
        );
        //If Partition is allocated and there should not be request for the same part-id again
        if (Id == PartitionId)
        {
            Slot = i;
            break;
        }
        // End of the Allocation with Id = 0
        if (Id == 0)
        {
            return NvError_NotInitialized;
        }
    }
    if (i == MaxPartition)
        return NvError_OverFlow;
    // Store the Hash
    NV_CHECK_ERROR(
        NvBctSetData(hBct, NvBctDataType_HashedPartition_CryptoHash,
                     &HashSize, &Slot, Hash)
    );
    return NvSuccess;
}

NvError NvBuQueryBlOrder(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 *NumBootloaders,
    NvBuBlInfo *BlInfo)
{
    NvError e;
    NvU32 NumBls;
    NvU32 NumEnabledBls;
    NvU32 Size = sizeof(NvU32);
    NvU32 Instance = 0;
    NvU32 ActiveSlot = 0;
    NvU32 i;

    if (!hBct || !hBit || !NumBootloaders)
        return NvError_InvalidAddress;

    NV_CHECK_ERROR(GetNumInstalledBls(hBct, &NumBls));

    if (*NumBootloaders < NumBls)
    {
        *NumBootloaders = NumBls;

        if (BlInfo)
            return NvError_InsufficientMemory;
        else
            return NvSuccess;
    }

    if (!BlInfo)
        return NvError_InvalidAddress;

    *NumBootloaders = NumBls;

    // get number of bootable BL's

    NV_CHECK_ERROR(
        NvBctGetData(hBct, NvBctDataType_NumEnabledBootLoaders,
                     &Size, &Instance, &NumEnabledBls)
        );

    // iterate over installed BL's, gathering properties

    for (i=0; i<NumBls; i++)
    {
        BlInfo[i].PartitionId = 0;
        NV_CHECK_ERROR(
            NvBctGetData(hBct, NvBctDataType_BootLoaderAttribute,
                         &Size, &i, &BlInfo[i].PartitionId)
            );

        NV_CHECK_ERROR(
            NvBctGetData(hBct, NvBctDataType_BootLoaderVersion,
                         &Size, &i, &BlInfo[i].Version)
            );

        NV_CHECK_ERROR(
            NvBctGetData(hBct, NvBctDataType_BootLoaderLoadAddress,
                         &Size, &i, &BlInfo[i].LoadAddress)
            );

        if (i<NumEnabledBls)
            BlInfo[i].IsBootable = NV_TRUE;
        else
            BlInfo[i].IsBootable = NV_FALSE;
    }

    // a new generation starts whenever the version changes

    BlInfo[0].Generation = 0;

    for (i=1; i<NumBls; i++)
    {
        BlInfo[i].Generation = BlInfo[i-1].Generation;

        if (BlInfo[i].Version == BlInfo[i-1].Version)
            BlInfo[i].Generation++;
    }

    /*
     * determine which BL's were used to boot the system
     *
     * the Boot ROM can assemble a working BL from several BL instances
     * belonging to the same generation (i.e., having the same version number
     * and residing in contiguous entries in the BCT's table of BL's)
     *
     * although not all instances of the given generation may have been needed
     * to construct the running BL, it probably doesn't make sense to be so
     * aggressive in identifying the BL's used to boot the system.
     *
     * modifying these "unused" BL instances would likely lead to confusion
     * should the update be interrupted, in which case the BCT would indicate a
     * higher level of redundancy than really exists on the system (since BL's
     * marked with the same version in the BCT would actually be different)
     *
     * hence, all BL instances belonging to the same generation as the running
     * BL will be marked as active here
     *
     * should some users prefer the more aggressive definition, the associated
     * code is also give below
     */

    for (i=0; i<NumBls; i++)
          BlInfo[i].IsActive = NV_FALSE;

    NV_CHECK_ERROR(NvBuQueryActiveBlSlot(hBit, &ActiveSlot));

#define AGGRESSIVE_ACTIVE_BL_DEFINITION 0

#if AGGRESSIVE_ACTIVE_BL_DEFINITION
    // determine which BL's were used to boot the system; this includes the base
    // BL plus any subsequent instances of the same generation that were used to
    // fill in missing and/or corrupted parts of the base BL

    BlInfo[ActiveSlot].IsActive = NV_TRUE;

    Size = sizeof(NvBool);

    for (i=ActiveSlot+1; i<NumEnabledBls; i++)
    {
        NvBool UsedForEccRecovery;

        NV_CHECK_ERROR(
            NvBitGetData(hBit, NvBitDataType_BlUsedForEccRecovery,
                         &Size, &i, &UsedForEccRecovery)
            );

        if (BlInfo[i].Generation != BlInfo[ActiveSlot].Generation)
            break;
        if (!UsedForEccRecovery)
            break;

        BlInfo[i].IsActive = NV_TRUE;
    }
#else
    // determine which BL's were used to boot the system; this includes the base
    // BL plus all other BL instances belonging to the same generation

    for (i=0; i<NumEnabledBls; i++)
    {
        if (BlInfo[i].Generation == BlInfo[ActiveSlot].Generation)
            BlInfo[i].IsActive = NV_TRUE;
    }
#endif

    return NvSuccess;

}


NvError
NvBuUpdateBlOrder(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvU32 *NumBootloaders,
    NvBuBlInfo *BlInfo)
{
    NvError e;
    NvU32 i;
    NvU32 NumInstalledBls;
    NvU32 NumEnabledBls;
    NvU32 Size = sizeof(NvU32);
    NvU32 Instance = 0;
    NvU32 Slot;
    NvBctBootLoaderInfo *TempBlInfo = NULL;

    if (!hBct || !hBit || !NumBootloaders)
        return NvError_InvalidAddress;

    NV_CHECK_ERROR(GetNumInstalledBls(hBct, &NumInstalledBls));

    if (*NumBootloaders != NumInstalledBls)
        return NvError_BadParameter;

    if (!BlInfo)
        return NvError_InvalidAddress;

    // check that at least first BL is bootable

    if (!BlInfo[0].IsBootable)
        return NvError_BadParameter;

    NumEnabledBls = 1;

    // count number of bootable BL's

    for (i=1; i<NumInstalledBls; i++)
    {
        if (BlInfo[i].IsBootable)
            NumEnabledBls++;
    }

    // iterate over installed BL's, checking for consistency

    for (i=1; i<NumInstalledBls; i++)
    {
        if (BlInfo[i].IsBootable && !BlInfo[i-1].IsBootable)
            return NvError_BadParameter;

    }

    // update BL ordering in BCT

    TempBlInfo = (NvBctBootLoaderInfo *)
        NvOsAlloc(NumInstalledBls * sizeof(NvBctBootLoaderInfo));
    if (!TempBlInfo)
        return NvError_InsufficientMemory;

    for (i=0; i<NumInstalledBls; i++)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvBuQueryBlSlotByPartitionId(hBct, BlInfo[i].PartitionId, &Slot)
            );

        NV_CHECK_ERROR_CLEANUP(
            GetBlInfo(hBct, Slot, &TempBlInfo[i])
            );
    }

    for (i=0; i<NumInstalledBls; i++)
    {
        NV_CHECK_ERROR_CLEANUP(
            SetBlInfo(hBct, i, &TempBlInfo[i])
            );
    }

    // update number of bootable BL's in BCT

    Instance = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(hBct, NvBctDataType_NumEnabledBootLoaders,
                     &Size, &Instance, &NumEnabledBls)
        );

    // fall through

fail:
    NvOsFree(TempBlInfo);

    return e;
}


NvError
NvBuQueryIsUpdateAllowed(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvBool *IsUpdateAllowed)
{
    NvError e;
    NvBool IsInterrupted;
    NvBool IsPrimary;

    if (!hBct || !hBit || !IsUpdateAllowed)
        return NvError_InvalidAddress;

    NV_CHECK_ERROR(NvBuQueryIsInterruptedUpdate(hBit, &IsInterrupted));

    if (IsInterrupted)
    {
        *IsUpdateAllowed = NV_FALSE;
        return NvSuccess;
    }

    NV_CHECK_ERROR(NvBuQueryIsPrimaryBlRunning(hBct, hBit, &IsPrimary));

    if (!IsPrimary)
    {
        *IsUpdateAllowed = NV_FALSE;
        return NvSuccess;
    }

    *IsUpdateAllowed = NV_TRUE;

    return NvSuccess;
}


NvError
NvBuQueryIsPrimaryBlRunning(
    NvBctHandle hBct,
    NvBitHandle hBit,
    NvBool *IsPrimaryBlRunning)
{
    NvError e;
    NvU32 ActiveSlot = 0;
    NvU32 Generation = 0;

    NV_CHECK_ERROR(
        NvBuQueryActiveBlSlot(hBit, &ActiveSlot)
        );

    NV_CHECK_ERROR(
        NvBuQueryBlGeneration(hBct, ActiveSlot, &Generation)
        );

    if (Generation == 0)
        *IsPrimaryBlRunning = NV_TRUE;
    else
        *IsPrimaryBlRunning = NV_FALSE;

    return NvSuccess;
}
