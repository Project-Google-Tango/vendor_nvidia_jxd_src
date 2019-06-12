/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvsystem_utils.h"
#include "nvcrypto_hash.h"
#include "nvcrypto_cipher.h"
#include "nvpartmgr.h"
#include "nvstormgr.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"
#include "nvassert.h"
#include "nvbct.h"
#include "t30/arapb_misc.h"
#include "nvrm_init.h"
#include "nvrm_hardware_access.h"

#define NV3P_AES_HASH_BLOCK_LEN 16
#define QB_SIGNATURE "QUICKBOOT"

typedef struct QbInfoRec {
    NvU8 Sign[10];
    NvU32 EntryPoint;
    NvU32 Version;
}QbInfo;

static NvBool
NvSysUtilPrivGetPageSize( NvU32 id, NvU32 *page_size )
{
    NvBool b;
    NvError e;
    NvBctHandle BctHandle = NULL;
    NvU32 size = 0;
    NvU32 instance = 0;

    size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(
        NvBctInit(0, 0, &BctHandle)
    );
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(BctHandle, NvBctDataType_BootDevicePageSizeLog2,
                 &size, &instance, page_size)
    );

    // converting log2 of page size to actual page size
    *page_size = 1 << (*page_size);

    b = NV_TRUE;
    goto clean;

fail:
    *page_size = 0;
    b = NV_FALSE;

clean:
    if( BctHandle)
        NvBctDeinit(BctHandle);
    return b;
}

// This function returns the block size within given partition
// It reports the superblock size only if the partition is interleaved
NvBool
NvSysUtilGetDeviceInfo( NvU32 id, NvU32 logicalSector, NvU32 *sectorsPerBlock,
    NvU32 *bytesPerSector, NvU32 *physicalSector)
{
    NvBool b;
    NvError e;
    NvFsMountInfo minfo;
    NvDdkBlockDevInfo dinfo;
    NvDdkBlockDevHandle hDev = 0;
    NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs MapInArgs;
    NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs MapOutArgs;

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetFsInfo( id, &minfo )
    );

    // As an exception part-id==0 access needed here since
    // IOCTL MapLogicalToPhysicalSector in case of Nand can be called for
    // block driver handle corresponding to part-id==0
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, 0, &hDev )
    );

    MapInArgs.LogicalSectorNum = logicalSector;

    NV_CHECK_ERROR_CLEANUP(
        hDev->NvDdkBlockDevIoctl(hDev,
            NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector,
            sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorInputArgs),
            sizeof(NvDdkBlockDevIoctl_MapLogicalToPhysicalSectorOutputArgs),
            &MapInArgs,
            &MapOutArgs)
    );

    *physicalSector = MapOutArgs.PhysicalSectorNum;

    hDev->NvDdkBlockDevClose( hDev );

    // By default need partition id specific block driver handle
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, id, &hDev )
    );

    hDev->NvDdkBlockDevGetDeviceInfo( hDev, &dinfo );
    // for non-interleaved partitions the sectors per erasable block and
    // not super block is returned
    *bytesPerSector = dinfo.BytesPerSector;
    *sectorsPerBlock = dinfo.SectorsPerBlock;

    b = NV_TRUE;
    goto clean;

fail:
    *sectorsPerBlock = 0;
    *bytesPerSector = 0;
    *physicalSector = 0;
    b = NV_FALSE;

clean:
    //FIXME: Unbalanced close? Do not see Nand asserts now if we uncomment
    if( hDev )
        hDev->NvDdkBlockDevClose( hDev );
    return b;
}

NvError
NvSysUtilBootloaderWrite(NvU8 *bootloader, NvU32 length, NvU32 part_id,
    NvDdkFuseOperatingMode OperatingMode, NvU8 *hash, NvU32 hash_size, NvU32 *padded_length)
{
    return(NvSysUtilEncyptedBootloaderWrite(bootloader, length, part_id,
                        OperatingMode, hash, hash_size,padded_length, NV_FALSE));
}

NvError
NvSysUtilEncyptedBootloaderWrite(NvU8 *bootloader, NvU32 length, NvU32 part_id,
    NvDdkFuseOperatingMode OperatingMode, NvU8 *hash, NvU32 hash_size,
    NvU32 *padded_length, NvBool IsEncrypted)
{
    NvError e;
    NvU32 bytes;
    NvU32 page_size;
    NvU32 pad_size;
    NvU32 aes_blocks;
    NvU32 size;
    NvStorMgrFileHandle hFile = 0;
    char partition_name[NVPARTMGR_PARTITION_NAME_LENGTH];
    // worst case is 2 extra blocks
    NvU8 tmpbuff[NVCRYPTO_CIPHER_AES_BLOCK_BYTES * 2];
    NvU8 *tmp;
    NvCryptoHashAlgoHandle hHash = 0;
    NvCryptoHashAlgoParams hash_params;
    NvCryptoCipherAlgoHandle hCipher = 0;
    NvCryptoCipherAlgoParams cipher_params;
    NvCryptoCipherAlgoAesKeyType KeyType = NvCryptoCipherAlgoAesKeyType_Invalid;
    NvU32 NeedEncryption;
    NvU32 ChipId = 0;

    NV_ASSERT( bootloader );
    NV_ASSERT( length );
    NV_ASSERT( hash );
    NV_ASSERT( hash_size == NVCRYPTO_CIPHER_AES_BLOCK_BYTES );
    NV_ASSERT( padded_length );

    switch (OperatingMode)
    {
        case NvDdkFuseOperatingMode_Preproduction:
        case NvDdkFuseOperatingMode_NvProduction:
            NeedEncryption = NV_FALSE;
            KeyType = NvCryptoCipherAlgoAesKeyType_UserSpecified;
            break;

        case NvDdkFuseOperatingMode_OdmProductionOpen:
            NeedEncryption = NV_FALSE;
            KeyType = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
            break;

        case NvDdkFuseOperatingMode_OdmProductionSecure:
            NeedEncryption = NV_TRUE;
            KeyType = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
            break;

        default:
            // unsupported modes
            return NvError_BadParameter;
    }

    /* there's a bootrom bug. here's a work-around */
    if (!NvSysUtilPrivGetPageSize(part_id, &page_size))
    {
        e = NvError_BadValue;
        goto fail;
    }

    /* round to the nearest 16 (NVCRYPTO_CIPHER_AES_BLOCK_BYTES).
     * if result is a multiple of the page size, then add 16 (zero'd).
     */
    NvOsMemset( tmpbuff, 0, sizeof(tmpbuff) );
    aes_blocks = length / NVCRYPTO_CIPHER_AES_BLOCK_BYTES;

    // add padding to ensure that the bootloader length is a multiple of 16
    pad_size = ((length + (NVCRYPTO_CIPHER_AES_BLOCK_BYTES - 1)) /
        NVCRYPTO_CIPHER_AES_BLOCK_BYTES) * NVCRYPTO_CIPHER_AES_BLOCK_BYTES;

    // copy final partial block to temp buffer
    bytes = length - aes_blocks * NVCRYPTO_CIPHER_AES_BLOCK_BYTES;
    tmp = bootloader + aes_blocks * NVCRYPTO_CIPHER_AES_BLOCK_BYTES;
    NvOsMemcpy( tmpbuff, tmp, bytes );

    ChipId = NvSysUtilGetChipId();
    // Workaround for Boot ROM bug 378464 for ap20 and t30
    // add an additional 16 bytes of padding to ensure that the bootloader
    // length is NOT a multiple of the page size.
    if (ChipId == 0x20 || ChipId == 0x30)
    {
        if (pad_size % page_size == 0)
        {
            bytes += 16;
            pad_size += 16;
        }
    }

    // AES padding flag
    // BootLoader[ModifiedLength+1] = 0x80, where ModifiedLength is:
    // a) The original length if it is not a multiple of the page size
    // b) The original length + 16 if it is a multiple of the page size
    tmp = &tmpbuff[bytes];
    *tmp = 0x80;

    *padded_length = pad_size;

    /* is this already encryptd? */
    if (NeedEncryption && (!IsEncrypted))
    {
        // allocate cipher algorithm
        NV_CHECK_ERROR_CLEANUP(
            NvCryptoCipherSelectAlgorithm(NvCryptoCipherAlgoType_AesCbc,
                                          &hCipher)
            );

        // configure algorithm
        NvOsMemset(&cipher_params, 0, sizeof(cipher_params));
        cipher_params.AesCbc.IsEncrypt = NV_TRUE;
        cipher_params.AesCbc.KeyType = KeyType;
        cipher_params.AesCbc.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
        NvOsMemset(cipher_params.AesCbc.InitialVectorBytes, 0x0,
                   sizeof(cipher_params.AesCbc.InitialVectorBytes));
        cipher_params.AesCbc.PaddingType =
            NvCryptoPaddingType_ExplicitBitPaddingOptional;

        NV_CHECK_ERROR_CLEANUP(
            hCipher->SetAlgoParams( hCipher, &cipher_params )
            );

        // encrypt bulk of bootloader, excluding any trailing partial blocks
        size = NVCRYPTO_CIPHER_AES_BLOCK_BYTES * aes_blocks;
        NV_CHECK_ERROR_CLEANUP(
            hCipher->ProcessBlocks( hCipher, size, bootloader, bootloader,
                                    NV_TRUE, (pad_size == length) )
            );

        // encrypt the remaining partial blocks and padding, if any are present
        if (pad_size != length)
        {
            size = pad_size - (NVCRYPTO_CIPHER_AES_BLOCK_BYTES * aes_blocks);
            NV_CHECK_ERROR_CLEANUP(
                hCipher->ProcessBlocks( hCipher, size, tmpbuff, tmpbuff,
                                        NV_FALSE, NV_TRUE )
                );
        }
    }

    /* sign */
    NV_CHECK_ERROR_CLEANUP(
        NvCryptoHashSelectAlgorithm( NvCryptoHashAlgoType_AesCmac,
            &hHash )
    );

    NvOsMemset(&hash_params, 0, sizeof(hash_params));
    hash_params.AesCmac.IsCalculate = NV_TRUE;
    hash_params.AesCmac.KeyType = KeyType;
    hash_params.AesCmac.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
    hash_params.AesCmac.PaddingType =
        NvCryptoPaddingType_ExplicitBitPaddingOptional;

    NV_CHECK_ERROR_CLEANUP(
        hHash->SetAlgoParams( hHash, &hash_params )
        );

    // encrypt bulk of bootloader, excluding any trailing partial blocks
    size = NVCRYPTO_CIPHER_AES_BLOCK_BYTES * aes_blocks;
    NV_CHECK_ERROR_CLEANUP(
        hHash->ProcessBlocks( hHash, size, bootloader,
            NV_TRUE, (pad_size == length) )
    );

    // hash the remaining partial blocks and padding, if any are present
    if (pad_size != length)
    {
        size = pad_size - (NVCRYPTO_CIPHER_AES_BLOCK_BYTES * aes_blocks);
        NV_CHECK_ERROR_CLEANUP(
            hHash->ProcessBlocks( hHash, size, tmpbuff, NV_FALSE, NV_TRUE )
        );
    }

    NV_CHECK_ERROR_CLEANUP(
        hHash->QueryHash( hHash, &hash_size, hash )
    );

    /* write to partition */
    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetNameById( part_id, partition_name )
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileOpen( partition_name,
            NVOS_OPEN_WRITE, &hFile )
    );

    size = aes_blocks * NVCRYPTO_CIPHER_AES_BLOCK_BYTES;
    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileWrite( hFile, bootloader, size, &bytes )
    );

    size = pad_size - (NVCRYPTO_CIPHER_AES_BLOCK_BYTES * aes_blocks);
    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileWrite( hFile, tmpbuff, size, &bytes )
    );

    e = NvSuccess;
    goto clean;

fail:

clean:
    NvStorMgrFileClose( hFile );
    if (hHash)
        hHash->ReleaseAlgorithm( hHash );
    if (hCipher)
        hCipher->ReleaseAlgorithm( hCipher );
    return e;
}

NvError
NvSysUtilBootloaderReadVerify(NvU8 *bootloader, NvU32 part_id,
    NvDdkFuseOperatingMode OperatingMode, NvU64 size, NvU8 *ExpectedHash,
    NvU64 StagingBufSize)
{
    NvError e = NvError_NotInitialized;
    NvU32 bytesRead;
    NvStorMgrFileHandle hFile = 0;
    char partition_name[NVPARTMGR_PARTITION_NAME_LENGTH];

    NvCryptoHashAlgoHandle hHash = 0;
    NvCryptoHashAlgoParams hash_params;
    NvCryptoCipherAlgoAesKeyType KeyType = NvCryptoCipherAlgoAesKeyType_Invalid;
    NvU32 page_size;
    NvU32 bytesToRead;
    NvBool isValid = NV_FALSE;
    NvU64 BytesRemaining;
    NvBool StartMsg = NV_TRUE;
    NvBool EndMsg = NV_FALSE;
    NvU32 ChipId = 0;

    NV_ASSERT( bootloader );
    NV_ASSERT( size );
    NV_ASSERT( ExpectedHash );

    switch (OperatingMode)
    {
        case NvDdkFuseOperatingMode_Preproduction:
        case NvDdkFuseOperatingMode_NvProduction:
        case NvDdkFuseOperatingMode_OdmProductionSecurePKC:
            KeyType = NvCryptoCipherAlgoAesKeyType_UserSpecified;
            break;

        case NvDdkFuseOperatingMode_OdmProductionOpen:
            KeyType = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
            break;

        case NvDdkFuseOperatingMode_OdmProductionSecure:
            KeyType = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
            break;

        default:
            // unsupported modes
            return NvError_BadParameter;
    }

    // Workaround for Boot ROM bug 378464
    if( !NvSysUtilPrivGetPageSize( part_id, &page_size ) )
    {
        e = NvError_BadValue;
        goto fail;
    }

    BytesRemaining = ((size  + NVCRYPTO_CIPHER_AES_BLOCK_BYTES-1) /
        NVCRYPTO_CIPHER_AES_BLOCK_BYTES) * NVCRYPTO_CIPHER_AES_BLOCK_BYTES;

    ChipId = NvSysUtilGetChipId();
    // Workaround for Boot ROM bug 378464 for ap20 and t30
    if (ChipId == 0x20 || ChipId == 0x30)
    {
        if (BytesRemaining % page_size == 0 )
        {
            BytesRemaining += 16;
        }
    }

    /* vierfy signature */
    NV_CHECK_ERROR_CLEANUP(
        NvCryptoHashSelectAlgorithm( NvCryptoHashAlgoType_AesCmac,
            &hHash )
    );

    hash_params.AesCmac.IsCalculate = NV_TRUE;
    hash_params.AesCmac.KeyType = KeyType;
    hash_params.AesCmac.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
    NvOsMemset( hash_params.AesCmac.KeyBytes, 0,
        sizeof(hash_params.AesCmac.KeyBytes) );
    hash_params.AesCmac.PaddingType =
        NvCryptoPaddingType_ExplicitBitPaddingOptional;

    NV_CHECK_ERROR_CLEANUP(
        hHash->SetAlgoParams( hHash, &hash_params )
        );

    /* read data from partition */
    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetNameById( part_id, partition_name )
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileOpen(partition_name, NVOS_OPEN_READ, &hFile)
        );

    //Reading bootloader from its partition into staging buffer for its verification
    while (BytesRemaining)
    {
        bytesRead = 0;
        bytesToRead = (BytesRemaining > StagingBufSize) ?
                    (NvU32)StagingBufSize : (NvU32)BytesRemaining;
        NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead( hFile, bootloader, bytesToRead, &bytesRead)
        );

        if ((bytesRead < bytesToRead) ||
            ((bytesRead % NVCRYPTO_CIPHER_AES_BLOCK_BYTES) != 0))
        {
            e = NvError_InvalidSize;
            goto fail;
        }

        EndMsg = (BytesRemaining == (NvU64)bytesRead);
        NV_CHECK_ERROR_CLEANUP(
        hHash->ProcessBlocks(hHash, bytesRead, bootloader, StartMsg, EndMsg)
        );
        StartMsg = NV_FALSE;
        BytesRemaining -= bytesRead;
    }
    /* No need to check isValid since we know after all the operations
     * done so far, there is a valid hash with the hash algo handle
     */

    //verify bootloader
    e = hHash->VerifyHash(hHash, ExpectedHash, &isValid);
    if (!isValid)
    {
        e = NvError_InvalidState;
        NV_CHECK_ERROR_CLEANUP(e);
    }

fail:

    NvStorMgrFileClose( hFile );
    if( hHash )
        hHash->ReleaseAlgorithm( hHash );
    return e;
}

// FIXME: put this in a common spot -- nvflash_util needs this too
/**
 * Windows CE ROMHDR. -- copy/pased from msdn.microsoft.com, then converted
 * to use nvidia types.
 */
typedef struct ROMHDR {
  NvU32 dllfirst;
  NvU32 dlllast;
  NvU32 physfirst;
  NvU32 physlast;
  NvU32 nummods;
  NvU32 ulRAMStart;
  NvU32 ulRAMFree;
  NvU32 ulRAMEnd;
  NvU32 ulCopyEntries;
  NvU32 ulCopyOffset;
  NvU32 ulProfileLen;
  NvU32 ulProfileOffset;
  NvU32 numfiles;
  NvU32 ulKernelFlags;
  NvU32 ulFSRamPercent;
  NvU32 ulDrivglobStart;
  NvU32 ulDrivglobLen;
  NvU16 usCPUType;
  NvU16 usMiscFlags;
  void *pExtensions;
  NvU32 ulTrackingStart;
  NvU32 ulTrackingLen;
} ROMHDR;

static NvU8 * NvSysUtilTokAddr(const NvU8 *src, char *patt, NvU32 searchLen)
{
    NvU32 i, pattLen;

    pattLen = NvOsStrlen(patt);
    for ( i = 0; i <= (searchLen - pattLen); i++)
        if(!(NvOsMemcmp(&src[i], patt, pattLen)))
            return (NvU8 *)&src[i];

    return NULL;
}

#define ROM_SIGNATURE_OFFSET    (0x40)
#define ROM_SIGNATURE           (0x43454345)

NvError
NvSysUtilImageInfo( NvU32 PartitionId, NvU32 *LoadAddress,
    NvU32 *EntryPoint, NvU32 *version, NvU32 length )
{
    NvU32 tmp;
    NvU8 *pStagingBuffer = NULL;
    NvStorMgrFileHandle hFile = NULL;
    NvError e = NvSuccess;
    NvU32 qbSigSearchLength;
    QbInfo *qbInfo = NULL;
    NvU8 *qbSigSearchSpace = NULL;
    NvU32 offset;
    NvU32 bytes;
    char FileName[NVPARTMGR_PARTITION_NAME_LENGTH];
    NvU32 BytesRead;
    NvU32 BytesToRead = sizeof(ROMHDR);

    NV_ASSERT( PartitionId );
    NV_ASSERT( LoadAddress );
    NV_ASSERT( EntryPoint );
    NV_ASSERT( version );

    pStagingBuffer = NvOsAlloc(BytesToRead);
    // FIXME: figure out versioning
    *version = 1;
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetNameById(PartitionId, FileName));
    // open file
    NV_CHECK_ERROR_CLEANUP(NvStorMgrFileOpen(FileName,
                                                NVOS_OPEN_READ,
                                                &hFile));
    NV_CHECK_ERROR_CLEANUP(NvStorMgrFileRead(hFile,
                                                pStagingBuffer,
                                                BytesToRead,
                                                &BytesRead));

    tmp = *(NvU32 *)(&pStagingBuffer[ROM_SIGNATURE_OFFSET]);
    if( tmp == ROM_SIGNATURE )
    {
        ROMHDR *hdr;
        offset = *(NvU32 *)&pStagingBuffer[ROM_SIGNATURE_OFFSET + 8];

        NV_CHECK_ERROR_CLEANUP(NvStorMgrFileSeek(hFile,
                                                    offset,
                                                    NvOsSeek_Set));
        NV_CHECK_ERROR_CLEANUP(NvStorMgrFileRead(hFile,
                                                    pStagingBuffer,
                                                    sizeof(ROMHDR),
                                                    &bytes));

        NV_ASSERT( bytes == sizeof(ROMHDR) );

        hdr = (ROMHDR *)(&pStagingBuffer[0]);
        *LoadAddress = hdr->physfirst;
        *EntryPoint = hdr->physfirst;
    }
    else // Check if it's a quickboot image.
    {
        //FIXME FIXME FIXME FIXME
        // First decrypt the image when encryption is enabled, else the signature will never match.

        qbSigSearchLength = sizeof(QbInfo) + (2 * NV3P_AES_HASH_BLOCK_LEN);

        qbSigSearchSpace = NvOsAlloc(qbSigSearchLength);
        if(qbSigSearchSpace == NULL)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        if(length > qbSigSearchLength)
        {
            offset = length - qbSigSearchLength;
            NV_CHECK_ERROR_CLEANUP(NvStorMgrFileSeek(hFile,
                        offset,
                        NvOsSeek_Set));

            NV_CHECK_ERROR_CLEANUP(NvStorMgrFileRead(hFile,
                        qbSigSearchSpace,
                        qbSigSearchLength,
                        &bytes));
#if !NVODM_BOARD_IS_SIMULATION
            NV_ASSERT( bytes == qbSigSearchLength);
#endif

            /* The BL image can have max 2 * NV3P_AES_HASH_BLOCK_LEN bytes padding.
             * We have to search for qbsignature in last qbSigSearchLength
             * bytes of the aligned BL image.
             */
            qbInfo = (QbInfo *) NvSysUtilTokAddr(qbSigSearchSpace,
                                                QB_SIGNATURE,
                                                qbSigSearchLength);
            if(qbInfo)
            {
                *LoadAddress = qbInfo->EntryPoint;
                *EntryPoint = qbInfo->EntryPoint;
                *version = qbInfo->Version;
            }
            else
            {
                e = NvError_BadValue;
            }
        }
        else
        {
            e = NvError_BadValue;
        }
    }
fail:
    if(hFile)
        NvStorMgrFileClose(hFile);
    if(pStagingBuffer)
        NvOsFree(pStagingBuffer);
    if(qbSigSearchSpace)
        NvOsFree(qbSigSearchSpace);
    // FIXME: any error code is as good as any, eh?
    return e;
}

NvError
NvSysUtilSignData(const NvU8 *pBuffer, NvU32 bytesToSign, NvBool StartMsg,
    NvBool EndMsg, NvU8 *pHash, NvDdkFuseOperatingMode* pOpMode)
{
    // Generate signature from the data for the partition table
    //This implementation assumes InsecureLength is set already.
    NvError e =  NvError_NotInitialized;

    //Need to use a constant instead of a value
    NvU32 paddingSize = NV3P_AES_HASH_BLOCK_LEN -
                            (bytesToSign % NV3P_AES_HASH_BLOCK_LEN);
    const NvU8 *bufferToSign = pBuffer;
    NvU32 hashSize = NV3P_AES_HASH_BLOCK_LEN;
    static NvCryptoHashAlgoHandle pAlgo = (NvCryptoHashAlgoHandle)NULL;

    if (StartMsg)
    {
        NvCryptoHashAlgoParams Params;
        NvCryptoCipherAlgoAesKeyType KeyType = NvCryptoCipherAlgoAesKeyType_Invalid;
        if(pOpMode)
        {
            switch (*pOpMode)
            {
                case NvDdkFuseOperatingMode_Preproduction:
                case NvDdkFuseOperatingMode_NvProduction:
                case NvDdkFuseOperatingMode_OdmProductionSecurePKC:
                    KeyType = NvCryptoCipherAlgoAesKeyType_UserSpecified;
                    break;

                case NvDdkFuseOperatingMode_OdmProductionOpen:
                    KeyType = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
                    break;

                case NvDdkFuseOperatingMode_OdmProductionSecure:
                    KeyType = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
                    break;

                default:
                    // unsupported modes
                    e = NvError_BadParameter;
                    goto fail;
            }
        }
        else
        {
            KeyType = NvCryptoCipherAlgoAesKeyType_UserSpecified;
        }
        NV_CHECK_ERROR_CLEANUP(NvCryptoHashSelectAlgorithm(
                                        NvCryptoHashAlgoType_AesCmac,
                                         &pAlgo));

        NV_ASSERT(pAlgo);
        NvOsMemset(&Params, 0, sizeof(Params));

        // configure algorithm
        Params.AesCmac.IsCalculate = NV_TRUE;
        Params.AesCmac.KeyType = KeyType;
        Params.AesCmac.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
        Params.AesCmac.PaddingType = NvCryptoPaddingType_ExplicitBitPaddingOptional;

        NV_CHECK_ERROR_CLEANUP(pAlgo->SetAlgoParams(pAlgo, &Params));
    }
    else
    {
        NV_ASSERT(pAlgo);
    }

    // Since Padding for the cmac algorithm is going to be decided at runtime,
    // we need to compute padding size on the fly and decide on the padding
    // size accordingly.
    // These padding bytes are not intended to be stored on the media
    // Assumption:there's enough room for padding bytes in the incoming buffer
    NV_CHECK_ERROR_CLEANUP(pAlgo->QueryPaddingByPayloadSize(pAlgo, bytesToSign,
                                        &paddingSize,
                                        ((NvU8 *)(bufferToSign) + bytesToSign) ));

    bytesToSign += paddingSize;

    // process data
    NV_CHECK_ERROR_CLEANUP(pAlgo->ProcessBlocks(pAlgo, bytesToSign,
                             (void *)(bufferToSign),
                             StartMsg, EndMsg));
    if (EndMsg == NV_TRUE)
    {
    //Read back the hash and only store 8 bytes in the signature
    // within the insecure header. This should be sufficient for ver-
    // ification purposes.
    NV_CHECK_ERROR_CLEANUP(pAlgo->QueryHash(pAlgo, &hashSize,
                                     pHash));
        pAlgo->ReleaseAlgorithm(pAlgo);
    }

    return NvSuccess;

fail:

    if (EndMsg == NV_TRUE)
    {
        pAlgo->ReleaseAlgorithm(pAlgo);
    }
    return e;
}

NvError
NvSysUtilEncryptData(NvU8 *pBuffer, NvU32 bytesToSign, NvBool StartMsg,
    NvBool EndMsg, NvBool IsDecrypt, NvDdkFuseOperatingMode OpMode)
{
    // Generate signature from the data for the partition table
    //This implementation assumes InsecureLength is set already.
    NvError e =  NvSuccess;
    static NvCryptoCipherAlgoHandle hCipher = 0;
    static NvBool IsEncrypt = NV_FALSE;

    if(StartMsg)
    {
        NvCryptoCipherAlgoParams cipher_params;
        NvCryptoCipherAlgoAesKeyType KeyType = NvCryptoCipherAlgoAesKeyType_Invalid;
        switch (OpMode)
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
            // For encrypting & decrypting data in PKC mode using SBK.
            case NvDdkFuseOperatingMode_OdmProductionSecurePKC:
            case NvDdkFuseOperatingMode_OdmProductionSecureSBK:
                IsEncrypt = NV_TRUE;
                KeyType = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
                break;

            default:
                // unsupported modes
                e = NvError_BadParameter;
                goto fail;
        }

        if(IsEncrypt)
        {
            // allocate cipher algorithm
            NV_CHECK_ERROR_CLEANUP(
                NvCryptoCipherSelectAlgorithm(NvCryptoCipherAlgoType_AesCbc,
                                              &hCipher)
                );
            NvOsMemset(&cipher_params, 0, sizeof(cipher_params));

            // configure algorithm
            cipher_params.AesCbc.IsEncrypt = !IsDecrypt;
            cipher_params.AesCbc.KeyType = KeyType;
            cipher_params.AesCbc.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;
            NvOsMemset(cipher_params.AesCbc.InitialVectorBytes, 0x0,
                       sizeof(cipher_params.AesCbc.InitialVectorBytes));
            cipher_params.AesCbc.PaddingType =
                NvCryptoPaddingType_ExplicitBitPaddingOptional;

            NV_CHECK_ERROR_CLEANUP(
                hCipher->SetAlgoParams( hCipher, &cipher_params )
                );
        }
    }

    if(IsEncrypt)
    {
        NV_CHECK_ERROR_CLEANUP(
            hCipher->ProcessBlocks( hCipher, bytesToSign, pBuffer, pBuffer,
                                    StartMsg, EndMsg )
            );
    }

fail:

    if (EndMsg == NV_TRUE)
    {
        IsEncrypt = NV_FALSE;
        if( hCipher )
            hCipher->ReleaseAlgorithm( hCipher );
    }
    return e;
}

NvU32 NvSysUtilGetChipId(void)
{
    NvU32 ChipId = 0;
#if NVODM_BOARD_IS_SIMULATION
    ChipId = nvflash_get_devid();
#else
    NvRmPhysAddr MiscBase;
    NvRmDeviceHandle hRm = NULL;
    NvU32        MiscSize;
    NvU8        *pMisc;
    NvU32        reg;
    NvError      e = NvSuccess;

    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&hRm, 0));
    NvRmModuleGetBaseAddress(hRm,
        NVRM_MODULE_ID(NvRmModuleID_Misc, 0), &MiscBase, &MiscSize);

    NV_CHECK_ERROR_CLEANUP(NvOsPhysicalMemMap(MiscBase, MiscSize, NvOsMemAttribute_Uncached,
        NVOS_MEM_READ_WRITE, (void**)&pMisc));

    reg = NV_READ32(pMisc + APB_MISC_GP_HIDREV_0);
    ChipId = NV_DRF_VAL(APB_MISC_GP, HIDREV, CHIPID, reg);
    NvRmClose(hRm);

fail:
    if (e)
        NvOsDebugPrintf("failed to read the ChipId\n");
#endif
    return ChipId;
}

char NvSysUtilsToLower(const char character)
{
    if(character >= 'A' && character <= 'Z')
        return ('a' + character - 'A');

    return character;
}

NvS32 NvSysUtilsIStrcmp(const char *string1, const char *string2)
{
    NvS32 ret;

    for(; *string1 && *string2; string1++, string2++)
    {
        if(NvSysUtilsToLower(*string1) != NvSysUtilsToLower(*string2))
            break;
    }

    ret = NvSysUtilsToLower(*string1) - NvSysUtilsToLower(*string2);

    return ret;
}

