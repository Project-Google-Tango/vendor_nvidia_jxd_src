/*
 * Copyright (c) 2010-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "recovery_utils.h"
#include "nvstormgr.h"
#include "nvsystem_utils.h"
#include "nvstormgr.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"
#include "nvassert.h"
#include "nvbu.h"
#include "nvbu_private.h"
#include "nvcrypto_hash.h"
#include "nvfsmgr_defs.h"
#include "nvodm_system_update.h"
#include "nvodm_query_discovery.h"
#include "fastboot.h"
#include "nvos.h"
#include "nvaboot.h"
#include "nv_sha.h"
#include "nv_rsa.h"
#include "nv_secureblob.h"
#include "nvcrypto_cipher.h"
#include "nv3pserver.h"
#include "libfdt/libfdt.h"
#ifndef NV_LDK_BUILD
#include "publickey.h"
#endif
#include <string.h>

#define UPDATE_MAGIC       "MSM-RADIO-UPDATE"
#define UPDATE_MAGIC_SIZE  16
#define UPDATE_VERSION     0x00010000
#define PARTITION_NAME_LENGTH 4
#define UPDATE_CHUNK (10*1024*1024)
#define MISC_PAGES 3
#define MISC_COMMAND_PAGE 1
#define DATA_SIZE 134144
#define NV_AES_HASH_BLOCK_LEN (16)

#define GET_VERSION(x)   ((x) & 0x7FFFFFFF)
#define IS_ENCRYPTED(x)  (((x) & 0x80000000) >> (8*sizeof(NvU32)-1))

typedef enum
{
    RecoveryBitmapType_InstallingFirmware,
    RecoveryBitmapType_InstallationError,
    RecoveryBitmapType_Nums = 0x7fffffff
}RecoveryBitmapType;

typedef enum
{
    DtbPropertyType_Filename,
    DtbPropertyType_Boardids,
    DtbPropertyType_Nums = 0x7fffffff
}DtbPropertyType;

typedef struct
{
    unsigned char MAGIC[UPDATE_MAGIC_SIZE];
    unsigned Version;

    unsigned Size;

    unsigned EntriesOffset;
    unsigned NumEntries;

    unsigned BitmapWidth;
    unsigned BitmapHeight;
    unsigned BitmapBpp;

    unsigned BusyBitmapOffset;
    unsigned BusyBitmapLength;

    unsigned FailBitmapOffset;
    unsigned FailBitmapLength;
}UpdateHeader;

typedef struct
{
    char PartName[PARTITION_NAME_LENGTH];
    unsigned ImageOffset;
    unsigned ImageLength;
    unsigned Version;
}ImageEntry;

static NvBitHandle s_hBit;
static NvBctHandle s_hBct;
static NvU32 s_nBl;
static NvBuBlInfo *s_BlInfo = 0;
static char s_Status[32];
BootloaderMessage BootMessage;
static char *s_DtbProperty = NULL;
#ifdef NV_LDK_BUILD
/* LDK build doesn't use OTA */
static NvBigIntModulus RSAPublicKey;
NvS32 NvRSAVerifySHA1Dgst(NvU8 *Dgst, NvBigIntModulus *PubKeyN,
                          NvU32 *PubKeyE, NvU32 *Sign)
{
    return 1;
}
NvS32 NvSHA_Init(NvSHA_Data *pSHAData)
{
    return 1;
}
NvS32 NvSHA_Update(NvSHA_Data *pSHAData, NvU8 *pMsg, NvU32 MsgLen)
{
    return 1;
}
NvS32 NvSHA_Finalize(NvSHA_Data *pSHAData, NvSHA_Hash *pSHAHash)
{
    return 1;
}
#endif

static NvError
GetStorageGeometry( NvU32 PartitionID, NvU32 *start_block,
    NvU32 *start_page )
{
    NvBool b;
    NvError e = NvSuccess;
    NvPartInfo part_info;
    NvU32 sectors_per_block;
    NvU32 bytes_per_sector;
    NvU32 phys_sector;
    NvU32 size = 0;
    NvU32 inst = 0;
    NvU32 block_size_log2;
    NvU32 page_size_log2;

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetPartInfo( PartitionID, &part_info )
    );

    b = NvSysUtilGetDeviceInfo( PartitionID,
        (NvU32)part_info.StartLogicalSectorAddress,
        &sectors_per_block, &bytes_per_sector, &phys_sector );
    if (!b)
    {
        e = NvError_DeviceNotFound;
        goto fail;
    }

    size = 4;
    inst = 0;

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData( s_hBct, NvBctDataType_BootDeviceBlockSizeLog2, &size,
            &inst, &block_size_log2 )
    );

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData( s_hBct, NvBctDataType_BootDevicePageSizeLog2, &size,
            &inst, &page_size_log2 )
    );

    /* convert to what the bootrom thinks is right. */
    *start_block = (phys_sector * bytes_per_sector) /
        (1 << block_size_log2);
    *start_page = (phys_sector * bytes_per_sector) %
        (1 << block_size_log2);
    *start_page /= (1 << page_size_log2);

fail:
    return e;
}

static NvError GetBLInfo(void)
{
    NvError e = NvSuccess;

    if (!s_BlInfo)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvBitInit( &s_hBit )
        );

        NV_CHECK_ERROR_CLEANUP(
            NvBctInit( 0, 0, &s_hBct )
        );

        NV_CHECK_ERROR_CLEANUP(
            NvBuQueryBlOrder( s_hBct, s_hBit, &s_nBl, 0 )
        );

        s_BlInfo = NvOsAlloc( sizeof(NvBuBlInfo) * s_nBl );
        NV_ASSERT(s_BlInfo);
        if (!s_BlInfo)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }

        NV_CHECK_ERROR_CLEANUP(
            NvBuQueryBlOrder( s_hBct, s_hBit, &s_nBl, s_BlInfo )
        );
    }

fail:
    return e;
}

static NvError GetBCTHandle(NvStorMgrFileHandle hStagingFile, ImageEntry *Entry,
                                    NvBctHandle *pRcvBCTHandle, NvBool IsEncrypted)
{
    NvError e = NvSuccess;
    NvU8 *bctbuf = NULL;
    NvU32 bctbufsize = 0;
    NvU32 bytes = 0;
    NvCryptoCipherAlgoHandle hCipher = 0;
    NvCryptoCipherAlgoParams cipher_params;

    if (!hStagingFile || !Entry)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    bctbufsize = Entry->ImageLength;

    /*  read the BCT to a buffer */
    bctbuf = NvOsAlloc(bctbufsize);
    if (bctbuf == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileSeek(hStagingFile,
                          (Entry->ImageOffset)+ (sizeof(NvSignedUpdateHeader)),
                          NvOsSeek_Set)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hStagingFile, bctbuf, bctbufsize, &bytes)
    );
    if (bytes != bctbufsize)
    {
        e = NvError_FileReadFailed;
        goto fail;
    }

    if (IsEncrypted)
    {
        /* allocate cipher algorithm   */
        NV_CHECK_ERROR_CLEANUP(
            NvCryptoCipherSelectAlgorithm(NvCryptoCipherAlgoType_AesCbc,
                                          &hCipher)
        );

        /* configure algorithm  */
        NvOsMemset(&cipher_params, 0, sizeof(cipher_params));
        cipher_params.AesCbc.IsEncrypt = NV_FALSE;
        cipher_params.AesCbc.KeyType = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
        cipher_params.AesCbc.KeySize = NvCryptoCipherAlgoAesKeySize_128Bit;

        NvOsMemset(cipher_params.AesCbc.InitialVectorBytes, 0x0,
                   sizeof(cipher_params.AesCbc.InitialVectorBytes));
        cipher_params.AesCbc.PaddingType =
            NvCryptoPaddingType_ExplicitBitPaddingOptional;

        NV_CHECK_ERROR_CLEANUP(
            hCipher->SetAlgoParams( hCipher, &cipher_params )
        );

        /* decrypt bulk of BCT (in place decryption)  */
        NV_CHECK_ERROR_CLEANUP(
            hCipher->ProcessBlocks( hCipher, bctbufsize, bctbuf, bctbuf,
                                    NV_TRUE, NV_TRUE )
        );
    }

    NV_CHECK_ERROR_CLEANUP(
        NvBctInit(&bctbufsize, (void*)bctbuf, pRcvBCTHandle)
    );

fail:
    return e;
}

static NvError
UpdateBLParamsOfBCT(NvU32 Slot, NvU32 version, NvU32 start_block, NvU32 start_page,
                            NvU32 length, NvU32 hash_size, NvU8 *hash)
{
    NvError e = NvSuccess;
    NvU32 tempslot = Slot;
    NvU32 size = sizeof(version);

    /*  Update new Bootloader version & length & block & page*/
    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(s_hBct, NvBctDataType_BootLoaderVersion,
                    &size, &tempslot, (void*)(&version))
    );

    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(s_hBct, NvBctDataType_BootLoaderLength,
                    &size, &tempslot, (void*)(&length))
    );

    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(s_hBct, NvBctDataType_BootLoaderStartBlock,
                    &size, &tempslot, (void*)(&start_block))
    );

    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(s_hBct, NvBctDataType_BootLoaderStartSector,
                    &size, &tempslot, (void*)(&start_page))
    );

    /*  Update Bootloader hash value */
    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(s_hBct, NvBctDataType_BootLoaderCryptoHash,
                    &hash_size, &tempslot, hash)
    );

fail:
    return(e);
}

static NvError
UpdateParamsOfBCT(NvBctHandle RcvBCTHandle, NvBctDataType ParamNum,
                          NvBctDataType Param)
{
    NvError e = NvSuccess;
    NvU8  *buf = 0;
    NvU32 size = sizeof(NvU32);
    NvU32 Instance = 0, i = 0;
    NvU32 NumOfValidInstances = 0;
    NvU32 SetNumSdramSets = 0;

    /*  Get Number of Valid Instances */
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(RcvBCTHandle, ParamNum, &size, &Instance, &NumOfValidInstances)
    );
    /* Should not update NumSdramSets when using microboot */
    if (ParamNum == NvBctDataType_NumValidSdramConfigs)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(s_hBct, ParamNum, &size, &Instance, &SetNumSdramSets)
        );
        if (SetNumSdramSets != 0)
            SetNumSdramSets = NumOfValidInstances;
    } else
        SetNumSdramSets = NumOfValidInstances;
    /*  Update Number of Valid Instances to BCT */
    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(s_hBct, ParamNum, &size, &Instance, &SetNumSdramSets)
    );

    /*  Update All valid instances to BCT */
    size = 0;
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(RcvBCTHandle, Param, &size, &Instance, buf)
    );

    buf = NvOsAlloc(size);
    if (buf == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    for (i = 0 ; i < NumOfValidInstances ; i++)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(RcvBCTHandle, Param, &size, &i, buf)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(s_hBct, Param, &size, &i, buf)
        );
    }
fail:
    NvOsFree(buf);
    return(e);
}

static NvError UpdateBCTToDevice(void)
{
    NvDdkFuseOperatingMode op_mode;
    NvU32 size = 0;
    NvU32 OpModeSize;
    NvU32 bct_part_id;
    NvU8 *buf = 0;
    NvError e = NvSuccess;

    /* get the bct size */
    NV_CHECK_ERROR_CLEANUP(
        NvBctInit( &size, 0, 0 )
    );

    buf = NvOsAlloc( size );
    NV_ASSERT(buf);
    if (!buf)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetIdByName( "BCT", &bct_part_id )
    );

    OpModeSize =  sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&op_mode, (NvU32 *)&OpModeSize));

    /* re-sign and write the BCT */
    NV_CHECK_ERROR_CLEANUP(
        NvBuBctCryptoOps( s_hBct, op_mode, &size, buf,
            NvBuBlCryptoOp_EncryptAndSign )
    );
    if (op_mode == NvDdkFuseOperatingMode_OdmProductionSecurePKC)
    {
        T1xxUpdateCustDataBct(buf);
    }

    NV_CHECK_ERROR_CLEANUP(
        NvBuBctUpdate( s_hBct, s_hBit, bct_part_id, size, buf )
    );
fail:
    NvOsFree(buf);
    return e;
}

static NvError UpdateRedundantBLandMB(NvU8 *buf, NvU32 bufsize, NvU32 PartitionId,
                                      NvU32 Version, NvBool IsEncrypted)
{
    NvError e = NvSuccess;
    NvU8 hash[NVCRYPTO_CIPHER_AES_BLOCK_BYTES];
    NvU32 hash_size = 0;
    NvU32 i = 0;
    NvU32 size;
    NvU32 pad_length;
    NvU32 start_block;
    NvU32 start_page;
    NvDdkFuseOperatingMode op_mode;
    NvU32 bytesWritten = 0;
    NvStorMgrFileHandle hFile = NULL;
    char PartName[NVPARTMGR_PARTITION_NAME_LENGTH];

    NV_ASSERT(s_BlInfo);
    NV_ASSERT(s_hBct);

    size =  sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&op_mode, (NvU32 *)&size));

    hash_size = sizeof(hash);
    for (i = 0 ; i < s_nBl ; i++)
    {
        if (s_BlInfo[i].PartitionId == PartitionId)
        {
            if (op_mode != NvDdkFuseOperatingMode_OdmProductionSecurePKC)
            {
                NV_CHECK_ERROR_CLEANUP(
                    NvSysUtilEncyptedBootloaderWrite(buf,
                                                 bufsize,
                                                 PartitionId,
                                                 op_mode,
                                                 hash,
                                                 hash_size,
                                                 &pad_length,
                                                 IsEncrypted)
                );

                /* get obnoxious storage geometry stuff */
                start_block = 0;
                start_page = 0;
                NV_CHECK_ERROR_CLEANUP(
                    GetStorageGeometry(PartitionId, &start_block, &start_page)
                );

                /* finally update bl info */
                NV_CHECK_ERROR_CLEANUP(
                    UpdateBLParamsOfBCT(i, Version, start_block, start_page,
                                    pad_length, hash_size, hash)
                );

                /*  Write the infomation back to BCT */
                NV_CHECK_ERROR_CLEANUP(
                    UpdateBCTToDevice()
                );
            }
            else
            {
                /* write to partition */
                NV_CHECK_ERROR_CLEANUP(
                    NvPartMgrGetNameById(PartitionId, PartName));

                NV_CHECK_ERROR_CLEANUP(NvStorMgrFileOpen(PartName, NVOS_OPEN_WRITE,
                    &hFile));

                NV_CHECK_ERROR_CLEANUP(
                    NvStorMgrFileWrite(hFile, buf, bufsize, &bytesWritten));

                if (bytesWritten < bufsize)
                {
                    NvOsDebugPrintf("Could not update bootloader\n");
                    e = NvError_FileWriteFailed;
                }

                if (hFile != NULL)
                {
                    NvStorMgrFileClose(hFile);
                    hFile = NULL;
                }

            }
        }
    }

fail:
    if (hFile != NULL)
        NvStorMgrFileClose(hFile);
    return e;
}

static NvError
CheckAndUpdateBCT(NvStorMgrFileHandle hStagingFile, ImageEntry* Entry, NvBool *BCT)
{
    NvError e = NvSuccess;
    NvBctHandle RcvBCTHandle = NULL;
    NvDdkFuseOperatingMode op_mode;
    NvU32 OpModeSize;
    NV_ASSERT(BCT);

    OpModeSize =  sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&op_mode, (NvU32 *)&OpModeSize));
    if (! NvOsStrncmp(Entry->PartName,"BCT",PARTITION_NAME_LENGTH))
    {
        NV_CHECK_ERROR_CLEANUP(
            GetBCTHandle(hStagingFile, Entry, &RcvBCTHandle,
                                 IS_ENCRYPTED(Entry->Version))
        );
    }
    else
        goto fail;

    if (RcvBCTHandle != NULL)
    {
        if (op_mode != NvDdkFuseOperatingMode_OdmProductionSecurePKC)
        {
            /*  Update SDParam Info */
            NV_CHECK_ERROR_CLEANUP(
                UpdateParamsOfBCT(RcvBCTHandle,
                                      NvBctDataType_NumValidSdramConfigs,
                                      NvBctDataType_SdramConfigInfo)
            );

            /*  Update SDParam Info */
            NV_CHECK_ERROR_CLEANUP(
                UpdateParamsOfBCT(RcvBCTHandle,
                                      NvBctDataType_NumValidBootDeviceConfigs,
                                      NvBctDataType_BootDeviceConfigInfo)
            );

            /* Update type of device */
            NV_CHECK_ERROR_CLEANUP(
                UpdateParamsOfBCT(RcvBCTHandle,
                                      NvBctDataType_NumValidDevType,
                                      NvBctDataType_DevType)
            );
        }
        else
        {
           s_hBct = RcvBCTHandle;
        }

        /*  Write the infomation back to BCT */
        NV_CHECK_ERROR_CLEANUP(
            UpdateBCTToDevice()
        );
        *BCT = NV_TRUE;
        NvOsDebugPrintf("complete update BCT\n");
    }
    else
    {
        e = NvError_BadParameter;
        goto fail;
    }

fail:
    NvOsFree(RcvBCTHandle);
    return e;
}


static NvError
CheckAndUpdateBootloader(NvStorMgrFileHandle hStagingFile,
        ImageEntry* Entry, NvBool *Bootloader)
{
    NvError e = NvSuccess;
    NvU32 i;
    NvU32 bytes;
    NvU8 *buf = 0;
    NvU32 PartitionId = -1;

    NV_ASSERT(hStagingFile );
    NV_ASSERT(Entry );
    NV_ASSERT(Bootloader );

    if (!hStagingFile || !Entry || !Bootloader)
    {
        e = NvError_BadParameter;
        goto fail;
    }
    *Bootloader = NV_FALSE;
    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetIdByName( Entry->PartName, &PartitionId )
    );

    /* look for the partition id in the bootloader order array */
    for (i = 0; i < s_nBl; i++)
    {
        if (s_BlInfo[i].PartitionId == PartitionId)
        {
            /* version must be larger */
            NvBool result;
            result = NvOdmSysUpdateIsUpdateAllowed( NvOdmSysUpdateBinaryType_Bootloader,
                                s_BlInfo[i].Version, GET_VERSION(Entry->Version));
            if (result)
            {
                *Bootloader = NV_TRUE;
            }
            else
            {
                // It is a bootloader but current version is not supported
                *Bootloader = NV_TRUE;
                e = NvError_SysUpdateInvalidBLVersion;
                goto fail;
            }
            break;
        }
    }

    if (!*Bootloader)
    {
        /* not a bootloader, skip the rest */
        e = NvSuccess;
        goto fail;
    }

    buf = NvOsAlloc( (NvU32)Entry->ImageLength);
    NV_ASSERT(buf);
    if (!buf)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileSeek(hStagingFile,
                          (Entry->ImageOffset)+ (sizeof(NvSignedUpdateHeader)),
                          NvOsSeek_Set)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hStagingFile, buf, (NvU32)Entry->ImageLength, &bytes)
    );
    if (bytes != (NvU32)Entry->ImageLength)
    {
        e = NvError_FileReadFailed;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        UpdateRedundantBLandMB(buf, (NvU32)Entry->ImageLength, PartitionId,
                               GET_VERSION(Entry->Version),IS_ENCRYPTED(Entry->Version))
    );

fail:
    NvOsFree( buf );
    return e;

}

static NvError
UpdateSinglePartition(NvStorMgrFileHandle hReadFile, NvU8 *Image,
                      NvStorMgrFileHandle hWriteFile, NvU32 size)
{
    NvError e = NvSuccess;
    NvU8 *buf = Image;
    NvBool LookForSparseHeader = NV_TRUE;
    NvBool SparseImage = NV_FALSE;
    NvBool IsSparseStart = NV_FALSE;
    NvBool IsLastBuffer = NV_FALSE;
    NvU32 len = 0;
    NvU32 bytes = 0;
    NvFileSystemIoctl_WriteVerifyModeSelectInputArgs IoctlArgs;

    NV_ASSERT(hWriteFile);

    if (!buf)
    {
        buf = NvOsAlloc(UPDATE_CHUNK);
        if (!buf)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
    }

    // Enable write verify
    IoctlArgs.ReadVerifyWriteEnabled = NV_TRUE;
    NV_CHECK_ERROR_CLEANUP(NvStorMgrFileIoctl(
        hWriteFile,
        NvFileSystemIoctlType_WriteVerifyModeSelect,
        sizeof(IoctlArgs),
        0,
        &IoctlArgs,
        NULL));

    while (size)
    {
        len = (NvU32)NV_MIN(UPDATE_CHUNK, size);

        if (!Image)
        {
            NV_CHECK_ERROR_CLEANUP(
                NvStorMgrFileRead(hReadFile, buf, len, &bytes)
            );
            if (bytes != len)
            {
                e = NvError_FileReadFailed;
                goto fail;
            }
        }

        if (LookForSparseHeader)
        {
            if (NvSysUtilCheckSparseImage(buf, len))
            {
                SparseImage = NV_TRUE;
                IsSparseStart = NV_TRUE;
            }
            LookForSparseHeader = NV_FALSE;
        }

        if (!SparseImage)
        {
            NV_CHECK_ERROR_CLEANUP(
                NvStorMgrFileWrite( hWriteFile, buf, len, &bytes )
            );
            if (bytes != len)
            {
                e = NvError_InvalidSize;
                goto fail;
            }
        }
        else
        {
            if (!(size - len))
                IsLastBuffer = NV_TRUE;
            NV_CHECK_ERROR_CLEANUP(NvSysUtilUnSparse(hWriteFile,
                                                     buf,
                                                     len,
                                                     IsSparseStart,
                                                     IsLastBuffer,
                                                     NV_FALSE,
                                                     NV_FALSE,
                                                     NULL,
                                                     NULL));
            IsSparseStart = NV_FALSE;
        }

        if (Image)
            buf += len;

        size -= len;
    }
fail:
    // Disable write verify
    IoctlArgs.ReadVerifyWriteEnabled = NV_FALSE;
    if (hWriteFile)
    {
        (void)NvStorMgrFileIoctl(
            hWriteFile,
            NvFileSystemIoctlType_WriteVerifyModeSelect,
            sizeof(IoctlArgs),
            0,
            &IoctlArgs,
            NULL);
    }
    if (!Image)
        NvOsFree(buf);
    return e;
}

static NvError
UpdatePartitionsFromBlob(NvStorMgrFileHandle hStagingFile, ImageEntry *Entry)
{
    NvError e = NvSuccess;
    NvBool Bootloader = NV_FALSE;
    NvBool BCT = NV_FALSE;
    NvStorMgrFileHandle hWriteFile = 0;

    NV_ASSERT(hStagingFile );
    NV_ASSERT(Entry);

    if (!hStagingFile || !Entry)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    // check for bootloader update
    e = CheckAndUpdateBootloader(hStagingFile, Entry, &Bootloader);

    if (e != NvSuccess)
    {
        if (!s_hBit)
        {
            /* the bootrom didn't boot the system, this must be a test loaded
             * over jtag. continue happily.
             */
            Bootloader = NV_FALSE;
        }
        else
        {
            /* bootloader failed to install */
            if (e == NvError_SysUpdateInvalidBLVersion || e == NvError_SysUpdateBLUpdateNotAllowed)
            {
            }
            goto fail;
        }

    }
    if (Bootloader)
       goto fail;
    // there is no botloader to update. .check for BCT update

    NV_CHECK_ERROR_CLEANUP(
        CheckAndUpdateBCT(hStagingFile, Entry, &BCT)
    );
    if (BCT)
        goto fail;

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileSeek(hStagingFile,
                          (Entry->ImageOffset) + (sizeof(NvSignedUpdateHeader)),
                          NvOsSeek_Set)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileOpen(Entry->PartName, NVOS_OPEN_WRITE,
            &hWriteFile )
    );

    NV_CHECK_ERROR_CLEANUP(
        UpdateSinglePartition(hStagingFile, NULL, hWriteFile, (NvU32)Entry->ImageLength)
    );

fail:

    if (e != NvSuccess)
    {
        NvOsSnprintf(s_Status, sizeof(s_Status), "failed-update-%s", Entry->PartName);
    }

    (void)NvStorMgrFileClose( hWriteFile );
    return e;
}

static NvError VerifyBlobSignature(char *partname)
{
    NvSignedUpdateHeader *signedHeader = NULL;
    NvU32 *SignatureData = NULL;
    NvU8 *MessageBuffer = NULL;
    NvSHA_Data *SHAData = NULL;
    NvSHA_Hash *SHAHash = NULL;
    NvU32 *PubKeyE = NULL;
    NvStorMgrFileHandle hStagingFile = 0;
    NvU32 bytes = 0, so_far = 0;
    NvError e = NvSuccess;

    signedHeader = NvOsAlloc(sizeof(NvSignedUpdateHeader));
    if (signedHeader == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    SignatureData = NvOsAlloc(RSANUMBYTES);
    if (SignatureData == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    MessageBuffer = NvOsAlloc(SHA_1_CACHE_BUFFER_SIZE);
    if (MessageBuffer == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    SHAData = (NvSHA_Data*)NvOsAlloc(sizeof(NvSHA_Data));
    if (SHAData == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    SHAHash = (NvSHA_Hash*)NvOsAlloc(sizeof(NvSHA_Hash));
    if (SHAHash == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    PubKeyE = NvOsAlloc(RSANUMBYTES);
    if (PubKeyE == NULL){
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(PubKeyE, 0, RSANUMBYTES);
    PubKeyE[0] = RSA_PUBLIC_EXPONENT_VAL;

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileOpen(partname, NVOS_OPEN_READ, &hStagingFile)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hStagingFile, signedHeader, sizeof(NvSignedUpdateHeader), &bytes)
    );
    if (bytes != sizeof(NvSignedUpdateHeader))
    {
        e = NvError_FileReadFailed;
        goto fail;
    }

    // TODO: Look for any other validation which can be performed here
    if (NvOsStrncmp((char*)signedHeader->MAGIC, SIGNED_UPDATE_MAGIC, SIGNED_UPDATE_MAGIC_SIZE))
    {
        e = NvError_InvalidState;
        goto fail;
    }

    /*  Initializing the SHA-1 Engine */
    if (NvSHA_Init(SHAData) == -1)
    {
        e = NvError_InvalidState;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileSeek(hStagingFile, sizeof(NvSignedUpdateHeader), NvOsSeek_Set)
    );

    /*  Updating the SHA-1 engine hash value with the blob content */
    while (so_far < (signedHeader->ActualBlobSize))
    {
        NvU32 temp_size = SHA_1_CACHE_BUFFER_SIZE;
        if (((signedHeader->ActualBlobSize) - so_far) < temp_size)
            temp_size = (signedHeader->ActualBlobSize) - so_far;

        NV_CHECK_ERROR_CLEANUP(
            NvStorMgrFileRead(hStagingFile, MessageBuffer, temp_size, &bytes)
        );
        if (bytes != temp_size)
        {
            e = NvError_FileReadFailed;
            goto fail;
        }

        if (NvSHA_Update(SHAData, MessageBuffer, temp_size) == -1)
            goto fail;
        so_far += temp_size;
    }

    /*  Finalizing the SHA-1 engine hash value */
    if (NvSHA_Finalize(SHAData, SHAHash) == -1)
        goto fail;

    /*  Read Blob signature from file */
    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hStagingFile, SignatureData, signedHeader->SignatureSize, &bytes)
    );
    if (bytes != signedHeader->SignatureSize)
    {
        e = NvError_FileReadFailed;
        goto fail;
    }

    /*  Signature of the file has to be verified here   */
    if (NvRSAVerifySHA1Dgst(SHAHash->Hash, &RSAPublicKey, PubKeyE, SignatureData) == -1)
    {
        e = NvError_InvalidState;
        goto fail;
    }

fail:
    NvOsFree(signedHeader);
    NvOsFree(SignatureData);
    NvOsFree(MessageBuffer);
    NvOsFree(PubKeyE);
    NvOsFree(SHAData);
    NvOsFree(SHAHash);
    (void)NvStorMgrFileClose( hStagingFile );
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nBlob Signature Verification Failed\n");
    }
    return e;
}

NvError NvFastbootUpdateBin(NvU8 *Image, NvU32 ImageSize,
                            const PartitionNameMap *NvPart)
{
    NvError e = NvSuccess;
    NvU8 *buf = 0;
    NvU32 PartitionId = -1;
    NvU32 i = 0;
    NvU32 bytes;
    NvU32 OldKernelPages = 0;
    NvU32 OldRamdiskPages = 0;
    NvU32 OldSecondPages = 0;
    NvU32 NewKernelPages = 0;
    NvU32 NewRamdiskPages = 0;
    NvU8 *TempBuf = NULL;
    NvU32 TempBufSize = 0;
    NvBool IsZImageUpdate = NV_FALSE;
    NvBool IsRamdiskUpdate = NV_FALSE;
    AndroidBootImg BootImgHdr;
    NvStorMgrFileHandle hImgFile = NULL;

    if (!(NvOsStrncmp(NvPart->NvPartName, "NVC",sizeof("NVC")) &&
        NvOsStrncmp(NvPart->NvPartName, "EBT",sizeof("EBT"))))
    {
        NV_CHECK_ERROR_CLEANUP(
            NvPartMgrGetIdByName(NvPart->NvPartName, &PartitionId)
        );

        NV_CHECK_ERROR_CLEANUP(GetBLInfo());

        /* look for the partition id in the bootloader order array */
        for (i = 0; i < s_nBl; i++)
        {
            if (s_BlInfo[i].PartitionId == PartitionId)
                break;
        }
        buf = Image;

        NV_CHECK_ERROR_CLEANUP(
            UpdateRedundantBLandMB(buf, ImageSize, s_BlInfo[i].PartitionId, 1, NV_FALSE)
        );

    }   /*  end of bootloader updation */
    else
    {
        if (!NvOsStrncmp(NvPart->LinuxPartName, "zimage",sizeof("zimage")))
            IsZImageUpdate = NV_TRUE;
        else if (!NvOsStrncmp(NvPart->LinuxPartName,
                              "ramdisk",sizeof("ramdisk")))
            IsRamdiskUpdate = NV_TRUE;

        if (IsZImageUpdate || IsRamdiskUpdate)
        {
            NV_CHECK_ERROR_CLEANUP(
                NvStorMgrFileOpen(NvPart->NvPartName, NVOS_OPEN_READ, &hImgFile)
            );

            // Read header from boot image in boot partition
            NV_CHECK_ERROR_CLEANUP(
                NvStorMgrFileRead(hImgFile, (NvU8 *)&BootImgHdr,
                                  sizeof(AndroidBootImg), &bytes)
            );
            if (bytes != sizeof(AndroidBootImg))
            {
                e = NvError_FileReadFailed;
                goto fail;
            }

            // Verify if valid boot image is present
            if (NvOsMemcmp(BootImgHdr.magic, ANDROID_MAGIC, ANDROID_MAGIC_SIZE))
            {
                e = NvError_InvalidConfigVar;
                goto fail;
            }

            OldKernelPages = (BootImgHdr.kernel_size +
                              BootImgHdr.page_size - 1) / BootImgHdr.page_size;
            OldRamdiskPages = (BootImgHdr.ramdisk_size +
                               BootImgHdr.page_size - 1) / BootImgHdr.page_size;
            if (BootImgHdr.second_size)
                OldSecondPages = (BootImgHdr.second_size +
                               BootImgHdr.page_size - 1) / BootImgHdr.page_size;

            if (IsZImageUpdate)
            {
                NewKernelPages =
                    (ImageSize + BootImgHdr.page_size - 1) /
                    BootImgHdr.page_size;
                TempBufSize =
                    (1 + NewKernelPages + OldRamdiskPages + OldSecondPages) *
                    BootImgHdr.page_size;
                BootImgHdr.kernel_size = ImageSize;
            }
            else if (IsRamdiskUpdate)
            {
                NewRamdiskPages =
                    (ImageSize + BootImgHdr.page_size - 1) /
                    BootImgHdr.page_size;
                TempBufSize =
                    (1 + OldKernelPages + NewRamdiskPages + OldSecondPages) *
                    BootImgHdr.page_size;
                BootImgHdr.ramdisk_size = ImageSize;
            }

            // Allocate a temporary buffer to create the new boot image
            TempBuf = NvOsAlloc(TempBufSize);
            if (!TempBuf)
            {
                e = NvError_InsufficientMemory;
                goto fail;
            }
            NvOsMemset(TempBuf, 0, TempBufSize);

            // Place the boot header in the buffer
            NvOsMemcpy(TempBuf, &BootImgHdr, sizeof(AndroidBootImg));
            TempBuf += BootImgHdr.page_size;

            if (IsZImageUpdate)
            {
                // Append the new kernel image to the buffer
                NvOsMemcpy(TempBuf, Image, ImageSize);
                TempBuf += NewKernelPages * BootImgHdr.page_size;

                // Append the old ramdisk to the buffer
                NV_CHECK_ERROR_CLEANUP(
                    NvStorMgrFileSeek(
                        hImgFile,
                        (1 + OldKernelPages) * BootImgHdr.page_size,
                        NvOsSeek_Set)
                );
                NV_CHECK_ERROR_CLEANUP(
                    NvStorMgrFileRead(hImgFile, TempBuf,
                                      OldRamdiskPages * BootImgHdr.page_size,
                                      &bytes)
                );
                if (bytes != OldRamdiskPages * BootImgHdr.page_size)
                {
                    e = NvError_FileReadFailed;
                    goto fail;
                }
                TempBuf += OldRamdiskPages * BootImgHdr.page_size;
            }
            else if (IsRamdiskUpdate)
            {
                // Append the old kernel image to the buffer
                NV_CHECK_ERROR_CLEANUP(
                    NvStorMgrFileSeek(hImgFile,
                                      BootImgHdr.page_size, NvOsSeek_Set)
                );
                NV_CHECK_ERROR_CLEANUP(
                    NvStorMgrFileRead(hImgFile, TempBuf,
                                      OldKernelPages * BootImgHdr.page_size,
                                      &bytes)
                );
                if (bytes != OldKernelPages * BootImgHdr.page_size)
                {
                    e = NvError_FileReadFailed;
                    goto fail;
                }
                TempBuf += OldKernelPages * BootImgHdr.page_size;

                // Append the new ramdisk to the buffer
                NvOsMemcpy(TempBuf, Image, ImageSize);
                TempBuf += NewRamdiskPages * BootImgHdr.page_size;
            }

            // Append the old second image (if any) to the buffer
            if (OldSecondPages)
            {
                NV_CHECK_ERROR_CLEANUP(
                    NvStorMgrFileSeek(hImgFile,
                                      (1 + OldKernelPages + OldRamdiskPages) *
                                      BootImgHdr.page_size,
                                      NvOsSeek_Set)
                );
                NV_CHECK_ERROR_CLEANUP(
                    NvStorMgrFileRead(hImgFile, TempBuf,
                                      OldSecondPages * BootImgHdr.page_size,
                                      &bytes)
                );
                if (bytes != OldSecondPages * BootImgHdr.page_size)
                {
                    e = NvError_FileReadFailed;
                    goto fail;
                }
                TempBuf += OldSecondPages * BootImgHdr.page_size;
            }

            TempBuf -= TempBufSize;
            Image = TempBuf;
            ImageSize = TempBufSize;
            NV_CHECK_ERROR_CLEANUP(
                NvStorMgrFileClose(hImgFile)
            );
        }

        NV_CHECK_ERROR_CLEANUP(
            NvStorMgrFileOpen(NvPart->NvPartName, NVOS_OPEN_WRITE, &hImgFile)
        );
        NV_CHECK_ERROR_CLEANUP(
            UpdateSinglePartition(NULL, Image, hImgFile, ImageSize)
        );
    }/* end of else */

fail:
    if (TempBuf)
        NvOsFree(TempBuf);
    (void)NvStorMgrFileClose(hImgFile);
    return e;
}

NvError NvFastbootLoadBootImg(NvU32 StagingPartitionId, NvU8 **pImage,
                                NvU32 BootImgSize, NvU8 *BootImgSignature)
{
    NvError e = NvSuccess;
    char PartName[NVPARTMGR_PARTITION_NAME_LENGTH];
    NvU8 *MessageBuffer = NULL;
    NvU8 *tempImage = NULL;
    NvSHA_Data *SHAData = NULL;
    NvSHA_Hash *SHAHash = NULL;
    NvU32 *PubKeyE = NULL;
    NvU32 bytes = 0, so_far = 0;
    NvStorMgrFileHandle hStagingFile = 0;

    /*  verify the signature of the blob */
    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetNameById(StagingPartitionId, PartName)
    );

    *pImage = NvOsAlloc(BootImgSize);
    if (*pImage == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    tempImage = *pImage;

    MessageBuffer = NvOsAlloc(SHA_1_CACHE_BUFFER_SIZE);
    if (MessageBuffer == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    SHAData = (NvSHA_Data*)NvOsAlloc(sizeof(NvSHA_Data));
    if (SHAData == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    SHAHash = (NvSHA_Hash*)NvOsAlloc(sizeof(NvSHA_Hash));
    if (SHAHash == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    PubKeyE = NvOsAlloc(RSANUMBYTES);
    if (PubKeyE == NULL){
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(PubKeyE, 0, RSANUMBYTES);
    PubKeyE[0] = RSA_PUBLIC_EXPONENT_VAL;

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileOpen(PartName, NVOS_OPEN_READ, &hStagingFile)
    );

    /*  Initializing the SHA-1 Engine */
    if (NvSHA_Init(SHAData) == -1)
    {
        e = NvError_InvalidState;
        goto fail;
    }

    /*  Updating the SHA-1 engine hash value with the blob content */
    while (so_far < BootImgSize)
    {
        NvU32 temp_size = SHA_1_CACHE_BUFFER_SIZE;
        if ((BootImgSize - so_far) < temp_size)
            temp_size = BootImgSize - so_far;

        NV_CHECK_ERROR_CLEANUP(
            NvStorMgrFileRead(hStagingFile, MessageBuffer, temp_size, &bytes)
        );
        if (bytes != temp_size)
        {
            e = NvError_FileReadFailed;
            goto fail;
        }

        NvOsMemcpy(tempImage, MessageBuffer, bytes);
        tempImage += bytes;

        if (NvSHA_Update(SHAData, MessageBuffer, temp_size) == -1)
            goto fail;
        so_far += temp_size;
    }

    /*  Finalizing the SHA-1 engine hash value */
    if (NvSHA_Finalize(SHAData, SHAHash) == -1)
        goto fail;

    /*  Signature of the file has to be verified here   */
    if (NvRSAVerifySHA1Dgst(SHAHash->Hash, &RSAPublicKey, PubKeyE, (NvU32*)BootImgSignature) == -1)
    {
        e = NvError_InvalidState;
        goto fail;
    }

fail:
    NvOsFree(MessageBuffer);
    NvOsFree(PubKeyE);
    NvOsFree(SHAData);
    NvOsFree(SHAHash);
    (void)NvStorMgrFileClose( hStagingFile );
    if (e != NvSuccess)
    {
        NvOsFree(*pImage);
        *pImage= NULL;
    }
    return e;
}

static NvError UpdatePartitionById(
    NvStorMgrFileHandle hStagingFile,
    ImageEntry *entries,
    NvU32 NumEntries,
    NvU32 UpdatePartitionId)
{
    NvU32 e = NvSuccess;
    NvU32 i = 0;
    ImageEntry *TempEntry = NULL;
    NvU32 PartitionId = -1;

    TempEntry = entries;
    for (i = 0; i < NumEntries; i++)
    {
        PartitionId = -1;
        NV_CHECK_ERROR_CLEANUP(
            NvPartMgrGetIdByName(TempEntry->PartName, &PartitionId));
        if (PartitionId == UpdatePartitionId)
        {
            NvOsDebugPrintf("Start Updating %s\n", TempEntry->PartName);
            NV_CHECK_ERROR_CLEANUP(
                UpdatePartitionsFromBlob(hStagingFile, TempEntry)
            );
            NvOsDebugPrintf("End Updating %s\n", TempEntry->PartName);
            break;
        }
        TempEntry = TempEntry + 1;
    }
    /* Binary for this partition is not present in blob */
    if (i == NumEntries)
    {
        NvOsDebugPrintf("Partition %s not found in blob\n",
            TempEntry->PartName);
        e = NvError_BadParameter;
    }
fail:
    return e;
}

static NvError GetDtbInfo (NvStorMgrFileHandle hDtbFile, NvU32 DtbOffset,
                              DtbPropertyType PropertyType, char **Property,
                              NvU32 *PropertyLen)
{
    NvError e = NvSuccess;
    NvU32 bytes, i;
    void *DtbImage = NULL;
    NvU32 DtbSize = 0;
    char *property = NULL;
    const char *propertyLoc = NULL;
    int propertyLen = 0;
    int error;
    struct fdt_header *hdr = NULL;

    NV_ASSERT(hDtbFile && Property && PropertyLen);
    if (!hDtbFile || !Property || !PropertyLen)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    hdr = NvOsAlloc(sizeof(struct fdt_header));
    if (hdr == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileSeek(hDtbFile, DtbOffset, NvOsSeek_Set)
    );
    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hDtbFile, hdr, sizeof(struct fdt_header), &bytes)
    );
    if (bytes != sizeof(struct fdt_header))
    {
        e = NvError_FileReadFailed;
        goto fail;
    }

    error = fdt_check_header(hdr);
    if (error)
    {
        NvOsDebugPrintf("%s: Invalid fdt header. error: %s\n",
                        __func__, fdt_strerror(error));
        e = NvError_BadValue;
        goto fail;
    }

    DtbSize = fdt_totalsize(hdr);
    if (!DtbSize)
    {
        e = NvError_BadValue;
        goto fail;
    }

    DtbImage = NvOsAlloc(DtbSize);
    if (!DtbImage)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileSeek(hDtbFile, DtbOffset, NvOsSeek_Set)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hDtbFile, DtbImage, DtbSize, &bytes)
    );
    if (bytes != DtbSize)
    {
        e = NvError_FileReadFailed;
        goto fail;
    }

    switch (PropertyType)
    {
        case DtbPropertyType_Filename:
            propertyLoc = fdt_getprop((struct fdt_header *) DtbImage, 0,
                                      "nvidia,dtsfilename", &propertyLen);
            break;
        case DtbPropertyType_Boardids:
            propertyLoc = fdt_getprop((struct fdt_header *) DtbImage, 0,
                                      "nvidia,boardids", &propertyLen);
            break;
        default:
            break;
    }

    if (!propertyLoc)
    {
        e = NvError_NotSupported;
        goto fail;
    }

    if (PropertyType == DtbPropertyType_Filename)
    {
        for (i = propertyLen; i > 0; i--)
        {
            if ( *(propertyLoc + (i - 1)) == '/')
            {
                propertyLoc = propertyLoc + i;
                propertyLen = NvOsStrlen(propertyLoc) + 1;
                break;
            }
        }
    }

    property = NvOsAlloc(propertyLen);
    if (!property)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemcpy(property, propertyLoc, propertyLen);
    *Property = property;
    *PropertyLen = propertyLen;

fail:
    if (hdr)
        NvOsFree(hdr);
    if (DtbImage)
        NvOsFree(DtbImage);
    return e;
}

static NvError GetSubStrings(char *Property, NvU32 PropertyLen,
                                  char **SubStrings[], NvU32 *NumOfSubStrings)
{
    NvError e = NvSuccess;
    NvU32 i;
    char *tempPtr;
    char **subStrings;
    NvU32 strCount = 0;
    NvU32 numOfSubStrings = 0;

    NV_ASSERT(Property && SubStrings && NumOfSubStrings);
    if (!Property || !SubStrings || !NumOfSubStrings)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    tempPtr = Property;
    for (i = 0; i < PropertyLen; i++)
    {
        if (*tempPtr == '\0')
            numOfSubStrings++;
        tempPtr++;
    }
    subStrings = (char **) NvOsAlloc(numOfSubStrings * sizeof(char *));
    if (!subStrings)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    tempPtr = subStrings[0] = Property;
    strCount++;
    for (i = 0; i < PropertyLen; i++)
    {
        if (*tempPtr == '\0')
        {
            subStrings[strCount] = tempPtr + 1;
            strCount++;
            if (strCount == numOfSubStrings)
                break;
        }
        tempPtr++;
    }
    *SubStrings = subStrings;
    *NumOfSubStrings = numOfSubStrings;

fail:
    return e;
}

static NvError ConvertToInt(char *String, NvU32 Bytes, NvU32 *Num)
{
    NvError e = NvSuccess;
    NvU32 i;
    NvU32 num = 0;
    char *tempPtr;

    NV_ASSERT(String && Num);
    if (!String || !Num)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    tempPtr = String;
    for (i = 0; i < Bytes; i++)
    {
        num = num * 10 + ((*tempPtr) - '0');
        tempPtr = tempPtr + 1;
    }
    *Num = num;

fail:
    return e;
}

static NvError ParseSubString(char *SubString, NvOdmBoardInfo *BoardInfo)
{
    NvError e = NvSuccess;
    char *tempPtr1;
    char *tempPtr2;
    NvU32 boardInfo[3];
    NvU32 i;

    NV_ASSERT(SubString && BoardInfo);
    if (!SubString || !BoardInfo)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    tempPtr1 = SubString;
    for (i = 0; i < 3; i++)
    {
        tempPtr2 = strchr(tempPtr1, ':');
        if (tempPtr2 == NULL)
            tempPtr2= SubString + NvOsStrlen(SubString);
        NV_CHECK_ERROR_CLEANUP(
            ConvertToInt(tempPtr1, tempPtr2 - tempPtr1, &boardInfo[i])
        );
        tempPtr1 = tempPtr2 + 1;
    }

    BoardInfo->BoardID = (NvU16)boardInfo[0];
    BoardInfo->SKU = (NvU16)boardInfo[1];
    BoardInfo->Fab = (NvU8)boardInfo[2];

fail:
    return e;
}

static NvBool MatchBoardInfo(NvOdmBoardInfo BoardInfo1,
                                   NvOdmBoardInfo BoardInfo2)
{
    if (BoardInfo1.BoardID == BoardInfo2.BoardID &&
        BoardInfo1.SKU == BoardInfo2.SKU &&
        BoardInfo1.Fab == BoardInfo2.Fab)
        return NV_TRUE;

    return NV_FALSE;
}

#define NUM_BOARDS_SUPPORTED 3

static NvError MatchWithDeviceInfo(char *SubStrings[], NvU32 SubStringsNum,
                                          NvBool *Result)
{
    NvError e = NvSuccess;
    NvBool result = NV_FALSE;
    NvU32 i, j;
    NvOdmBoardInfo tempInfo;
    static NvOdmBoardInfo procInfo;
    static NvOdmPmuBoardInfo pmuInfo;
    static NvOdmDisplayBoardInfo dispInfo;
    static NvOdmBoardInfo *boardInfo[NUM_BOARDS_SUPPORTED];

    NV_ASSERT(SubStrings && Result);
    if (!SubStrings || !Result)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    if (!procInfo.BoardID)
    {
        NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_ProcessorBoard,
                                 &procInfo, sizeof(procInfo));
        boardInfo[0] = &procInfo;
    }

    if (!pmuInfo.BoardInfo.BoardID)
    {
        NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_PmuBoard,
                                 &pmuInfo, sizeof(pmuInfo));
        boardInfo[1] = &pmuInfo.BoardInfo;
    }

    if (!dispInfo.BoardInfo.BoardID)
    {
        NvOdmQueryGetBoardModuleInfo(NvOdmBoardModuleType_DisplayBoard,
                                 &dispInfo, sizeof(dispInfo));
        boardInfo[2] = &dispInfo.BoardInfo;
    }

    for (i = 0; i < SubStringsNum; i++)
    {
        if (SubStrings[i])
        {
            NV_CHECK_ERROR_CLEANUP(ParseSubString(SubStrings[i], &tempInfo));
            for (j = 0; j < NUM_BOARDS_SUPPORTED; j++)
            {
                if (MatchBoardInfo(*boardInfo[j], tempInfo))
                    break;
            }
            if (j == NUM_BOARDS_SUPPORTED)
                goto fail;
        }
    }
    result = NV_TRUE;

    *Result = result;

fail:
    return e;
}

static NvError CheckDtbInfo(NvStorMgrFileHandle hStagingFile,
                                     ImageEntry *DtbEntry,
                                     DtbPropertyType PropertyType,
                                     NvBool *PropertyMatch)
{
    NvError e = NvSuccess;
    NvBool propertyMatch = NV_FALSE;
    NvU32 i,j;
    NvU32 matchCount = 0;
    static NvU32 propertyLen = 0;
    static char **oldSubStrings = NULL;
    static NvU32 numOfOldSubStrings = 0;
    char *newProperty = NULL;
    NvU32 newPropertyLen = 0;
    char **newSubStrings = NULL;
    NvU32 numOfNewSubStrings = 0;
    NvStorMgrFileHandle hDtbFile = NULL;

    NV_ASSERT(hStagingFile && DtbEntry && PropertyMatch);
    if (!hStagingFile || !DtbEntry || !PropertyMatch)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    if (!s_DtbProperty)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvStorMgrFileOpen("DTB", NVOS_OPEN_READ, &hDtbFile)
        );
        NV_CHECK_ERROR_CLEANUP(
            GetDtbInfo(hDtbFile, 0, PropertyType, &s_DtbProperty, &propertyLen)
        );
        (void)NvStorMgrFileClose(hDtbFile);
        if (PropertyType == DtbPropertyType_Boardids)
        {
            NV_CHECK_ERROR_CLEANUP(
                GetSubStrings(s_DtbProperty, propertyLen,
                              &oldSubStrings, &numOfOldSubStrings)
            );
        }
    }

    e = GetDtbInfo(hStagingFile,
                   DtbEntry->ImageOffset + sizeof(NvSignedUpdateHeader),
                   PropertyType,
                   &newProperty,
                   &newPropertyLen);
    if (e == NvError_NotSupported)
    {
        e = NvSuccess;
        goto fail;
    }
    if (e != NvSuccess)
        goto fail;

    if (PropertyType == DtbPropertyType_Boardids)
    {
        NV_CHECK_ERROR_CLEANUP(
            GetSubStrings(newProperty, newPropertyLen,
                          &newSubStrings, &numOfNewSubStrings)
        );

        for (i = 0; i < numOfOldSubStrings; i++)
        {
            for (j= 0; j < numOfNewSubStrings; j++)
            {
                if (newSubStrings[j])
                {
                    if (!NvOsStrcmp(oldSubStrings[i], newSubStrings[j]))
                    {
                        matchCount++;
                        newSubStrings[j] = NULL;
                        break;
                    }
                }
            }
        }

        if (matchCount != numOfOldSubStrings)
            goto fail;

        if (numOfOldSubStrings == numOfNewSubStrings)
            propertyMatch = NV_TRUE;
        else
            NV_CHECK_ERROR_CLEANUP(
                MatchWithDeviceInfo(newSubStrings, numOfNewSubStrings,
                                    &propertyMatch)
            );
    }
    else if (newPropertyLen == propertyLen)
    {
        if (!NvOsMemcmp(s_DtbProperty, newProperty, propertyLen))
            propertyMatch = NV_TRUE;
    }

    *PropertyMatch = propertyMatch;

fail:
    if (newProperty)
        NvOsFree(newProperty);
    return e;
}

static NvError GetValidDtb(NvStorMgrFileHandle hStagingFile,
                               ImageEntry *BlobEntries,
                               NvU32 NumBlobEntries,
                               NvU32 *ValidDtbNum)
{
    NvError e = NvSuccess;
    NvU32 validDtbNum = 0;
    ImageEntry *tempBlobEntry;
    NvU32 i;
    NvBool propertyMatch = NV_FALSE;
    NvU32 dtbCount;
    NvU32 propertyType;

    NV_ASSERT(hStagingFile && BlobEntries && ValidDtbNum);
    if (!hStagingFile || !BlobEntries || !ValidDtbNum)
    {
        e = NvError_NotInitialized;
        goto fail;
    }

    for (propertyType = DtbPropertyType_Filename;
         propertyType <= DtbPropertyType_Boardids; propertyType++)
    {
        tempBlobEntry = BlobEntries;
        dtbCount = 0;
        for (i = 0; i < NumBlobEntries; i++, tempBlobEntry = tempBlobEntry + 1)
        {
            if (!NvOsStrcmp(tempBlobEntry->PartName, "DTB"))
            {
                dtbCount++;
                e = CheckDtbInfo(hStagingFile, tempBlobEntry,
                                 propertyType, &propertyMatch);
                if (e == NvError_NotSupported)
                {
                    e = NvSuccess;
                    break;
                }
                if (e != NvSuccess)
                    goto fail;

                if (propertyMatch)
                {
                    validDtbNum = dtbCount;
                    break;
                }
            }
        }
        if (s_DtbProperty)
            NvOsFree(s_DtbProperty);
        s_DtbProperty = NULL;
        if (validDtbNum)
            break;
    }

    *ValidDtbNum = validDtbNum;

fail:
    return e;
}

NvError NvInstallBlob(char *PartName)
{
    NvError e = NvSuccess;
    NvU32 bytes;
    NvU32 i, j;
    NvStorMgrFileHandle hStagingFile = 0;
    UpdateHeader *header = 0;
    NvU32 NumEntries = 0;
    ImageEntry *entries = 0;
    ImageEntry *TempEntry = 0;
    NvU32 SizeOfEntry = sizeof(ImageEntry);
    NvU32 Size;
    NvDdkFuseOperatingMode op_mode;
    NvU8 *IsBLOrBCT = NULL;
    NvU32 dtbPartId = 0;
    NvU32 numOfDtbs = 0;
    NvU32 validDtbNum = 0;
    NvU32 dtbCount = 0;

    // Memory Allocations
    header = NvOsAlloc(sizeof(UpdateHeader));
    if (header == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        VerifyBlobSignature(PartName)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileOpen(PartName, NVOS_OPEN_READ, &hStagingFile)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileSeek(hStagingFile, sizeof(NvSignedUpdateHeader), NvOsSeek_Set)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hStagingFile, header, sizeof(UpdateHeader), &bytes)
    );

    if (bytes != sizeof(UpdateHeader))
    {
        e = NvError_FileReadFailed;
        goto fail;
    }

    NumEntries = header->NumEntries;
    entries = NvOsAlloc(NumEntries * SizeOfEntry);
    NV_ASSERT(entries);
    if (!entries)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileSeek(hStagingFile,
                          (header->EntriesOffset)+(sizeof(NvSignedUpdateHeader)),
                          NvOsSeek_Set)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvStorMgrFileRead(hStagingFile, entries, NumEntries * SizeOfEntry, &bytes)
    );

    if (bytes != (NumEntries * SizeOfEntry))
    {
        e = NvError_FileReadFailed;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(GetBLInfo());

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetIdByName("DTB", &dtbPartId)
    );

    TempEntry = entries;
    for (i = 0; i < NumEntries; i++, TempEntry = TempEntry + 1)
    {
        if (!NvOsStrcmp(TempEntry->PartName, "DTB"))
            numOfDtbs++;
    }

    if (numOfDtbs > 1)
    {
        GetValidDtb(hStagingFile, entries, NumEntries, &validDtbNum);
    }

    Size =  sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_OpMode,
                (void *)&op_mode, (NvU32 *)&Size));

    /*  Update the partitions */
    if (op_mode == NvDdkFuseOperatingMode_OdmProductionSecurePKC)
    {
        NvU32 PartitionId = -1;
        NvU32 bct_part_id = -1;

        NV_CHECK_ERROR_CLEANUP(
            NvPartMgrGetIdByName( "BCT", &bct_part_id )
        );
        IsBLOrBCT = NvOsAlloc(s_nBl + 1);
        NvOsMemset(IsBLOrBCT, 0, s_nBl + 1);
        TempEntry = entries;
        for (i = 0; i < NumEntries; i++, TempEntry = TempEntry + 1)
        {
            e = NvPartMgrGetIdByName(TempEntry->PartName, &PartitionId);
            if (e != NvSuccess)
            {
                /* Ignore if blob has entry for non-existing partition */
                e = NvSuccess;
                continue;
            }
            if (PartitionId == bct_part_id)
                IsBLOrBCT[s_nBl] = 1;
            for (j = 0; j < s_nBl; j++)
            {
                if (s_BlInfo[j].PartitionId == PartitionId)
                    IsBLOrBCT[j] = 1;
            }
        }

        for (i = 0; i < s_nBl + 1; i++)
        {
            if (IsBLOrBCT[i] != 1)
                goto update_rest;
        }

        /* Update BL, MB, BCT */
        for (i = 0 ; i < s_nBl ; i += 2)
        {
            NV_CHECK_ERROR_CLEANUP(
                UpdatePartitionById(hStagingFile, entries, NumEntries,
                    s_BlInfo[i].PartitionId));
        }

        NV_CHECK_ERROR_CLEANUP(
            UpdatePartitionById(hStagingFile, entries, NumEntries,
                bct_part_id));

        for (i = 1 ; i < s_nBl ; i += 2)
        {
            NV_CHECK_ERROR_CLEANUP(
                UpdatePartitionById(hStagingFile, entries, NumEntries,
                    s_BlInfo[i].PartitionId));
        }

update_rest:
        /* Update Rest */
        TempEntry = entries;
        for (i = 0; i < NumEntries; i++, TempEntry = TempEntry + 1)
        {
            e = NvPartMgrGetIdByName(TempEntry->PartName, &PartitionId);
            if (e != NvSuccess)
            {
                /* Ignore if blob has entry for non-existing partition */
                e = NvSuccess;
                continue;
            }
            if (PartitionId == bct_part_id)
                continue;
            for (j = 0; j < s_nBl; j++)
            {
                if (s_BlInfo[j].PartitionId == PartitionId)
                    break;
            }

            if (j != s_nBl)
                continue;

            if (PartitionId == dtbPartId && numOfDtbs > 1)
            {
                dtbCount++;
                if (dtbCount != validDtbNum)
                    continue;
            }

            NV_CHECK_ERROR_CLEANUP(
                UpdatePartitionsFromBlob(hStagingFile, TempEntry)
            );
        }

        goto fail;
    }

    TempEntry = entries;
    for (i = 0; i < NumEntries; i++, TempEntry = TempEntry + 1)
    {
        NvU32 PartitionId = -1;
        if (IS_ENCRYPTED(TempEntry->Version) &&
            (op_mode != NvDdkFuseOperatingMode_OdmProductionSecure))
        {
            e = NvError_SysUpdateBLUpdateNotAllowed;
            NvOsDebugPrintf("\nUnsupported binary in blob\n");
            goto fail;
        }
        e = NvPartMgrGetIdByName(TempEntry->PartName, &PartitionId);
        if (e == NvSuccess)
        {
            if (PartitionId == dtbPartId && numOfDtbs > 1)
            {
                dtbCount++;
                if (dtbCount != validDtbNum)
                    continue;
            }

            NvOsDebugPrintf("\nStart Updating %s\n", TempEntry->PartName);
            NV_CHECK_ERROR_CLEANUP(
                UpdatePartitionsFromBlob(hStagingFile, TempEntry)
            );
            NvOsDebugPrintf("\nEnd Updating %s\n", TempEntry->PartName);
        }
        else
        {
            NvOsDebugPrintf("\nNon Existing %s\n", TempEntry->PartName);
        }
    }

fail:
    NvOsFree(header);
    NvOsFree(s_BlInfo);
    if (IsBLOrBCT)
    {
        NvOsFree(IsBLOrBCT);
        IsBLOrBCT = NULL;
    }
    s_BlInfo = NULL;
    s_nBl = 0;
    NvOsFree(entries);
    (void)NvStorMgrFileClose( hStagingFile );
    return e;
}

static NvError GetCommand(NvAbootHandle hAboot, CommandType *Command)
{
    NvError e = NvError_BadParameter;
    char command[32];

    NV_ASSERT(Command);
    if (Command)
    {
        *Command = CommandType_None;
        NV_CHECK_ERROR_CLEANUP(
            NvAbootBootfsReadNBytes(hAboot, "MSC", 0, sizeof(command),
                (NvU8 *)command)
        );

        if (!NvOsStrncmp(command, "boot-recovery", NvOsStrlen("boot-recovery")))
        {
            *Command = CommandType_Recovery;
        }
        else if (!NvOsStrncmp(command, "update", NvOsStrlen("update")))
        {
            *Command = CommandType_Update;
        }
        e= NvSuccess;
    }

fail:
    return e;
}

NvError
NvWriteCommandsForBootloader(
    NvAbootHandle hAboot, BootloaderMessage *Message)
{
    NvError e = NvError_BadParameter;
    NvStorMgrFileHandle hFile = 0;
    NvU32 bytes;
    NvU64 Start, Num;
    NvU32 SectorSize;
    NvU8 *temp = 0;
    NvFileSystemIoctl_WriteTagDataDisableInputArgs IoctlArgs;

    NV_CHECK_ERROR_CLEANUP(
        NvAbootGetPartitionParameters(hAboot,
            "MSC", &Start, &Num,
            &SectorSize)
    );
    temp = NvOsAlloc(SectorSize * MISC_PAGES);
    NV_ASSERT(temp);
    if (temp)
    {
        NvOsMemset(temp,0, (SectorSize * MISC_PAGES));

        NV_CHECK_ERROR_CLEANUP(
            NvStorMgrFileOpen( "MSC", NVOS_OPEN_WRITE, &hFile )
        );

        NvOsMemcpy(&temp[SectorSize * MISC_COMMAND_PAGE], Message,
            sizeof(BootloaderMessage));

        IoctlArgs.TagDataWriteDisable = NV_TRUE;
        NV_CHECK_ERROR_CLEANUP(NvStorMgrFileIoctl(
            hFile,
            NvFileSystemIoctlType_WriteTagDataDisable,
            sizeof(IoctlArgs),
            0,
            &IoctlArgs,
            NULL));

        NV_CHECK_ERROR_CLEANUP(
            NvStorMgrFileWrite(hFile, temp, (SectorSize * MISC_PAGES), &bytes)
        );
        if (bytes != (SectorSize * MISC_PAGES))
        {
            e = NvError_FileWriteFailed;
            goto fail;
        }
    }
    else
    {
        e = NvError_InsufficientMemory;
    }
fail:
    NvOsFree(temp);
    if (hFile)
        (void)NvStorMgrFileClose( hFile );
    return e;
}

static NvBool CheckStagingUpdate(NvAbootHandle hAboot, char* partName)
{
    NvBool PendingUpdate = NV_FALSE;
    NvError e = NvSuccess;
    NvSignedUpdateHeader *signedHeader =
        (NvSignedUpdateHeader *)NvOsAlloc(sizeof(NvSignedUpdateHeader));

    NV_CHECK_ERROR_CLEANUP(
        NvAbootBootfsReadNBytes(hAboot, partName, 0,
            sizeof(NvSignedUpdateHeader), (NvU8 *)signedHeader)
    );

    if (!NvOsStrncmp((char*)((signedHeader)->MAGIC),
            SIGNED_UPDATE_MAGIC, SIGNED_UPDATE_MAGIC_SIZE) )
        PendingUpdate = NV_TRUE;

fail:
    return PendingUpdate;
}

NvError NvCheckForUpdateAndRecovery(NvAbootHandle hAboot, CommandType *Command)
{
    NvError e = NvError_BadParameter;
    NvU32 StagingPartitionId = -1;
    NvBool StagingUpdate = NV_FALSE;
    char *partName = NULL;
    NV_ASSERT(hAboot);
    NV_ASSERT(Command);
    if (hAboot == 0 || Command == 0)
        goto fail;
    partName = NvOsAlloc(NVPARTMGR_PARTITION_NAME_LENGTH);
    if (partName == NULL)
    {
        goto fail;
    }
    NvOsMemset(partName, 0x0, NVPARTMGR_PARTITION_NAME_LENGTH);
    e = GetCommand(hAboot, Command);
    if (e == NvSuccess)
    {
        /* check for pending update in staging partition "USP"
         */
        if ((*Command) == CommandType_None)
        {
            /* Froyo/GB/ICS does not pass any info about update in the MSC partition
             * so we need to figure it out by reading USP parition
             */
            NvOsStrncpy(partName, "USP", NvOsStrlen("USP"));
            StagingUpdate = CheckStagingUpdate(hAboot, partName);
            if (StagingUpdate == NV_FALSE)
            {
                NvOsStrncpy(partName, "CAC", NvOsStrlen("CAC"));
                StagingUpdate = CheckStagingUpdate(hAboot, partName);
            }
            if (StagingUpdate == NV_TRUE)
            {
                e = NvPartMgrGetIdByName(partName, &StagingPartitionId);
                if (e != NvSuccess)
                {
                    // clear the staging partition
                    NvStorMgrFormat(partName);
                    // continue booting
                    *Command = CommandType_None;
                    e = NvSuccess;
                    goto fail;
                }
                *Command = CommandType_Update;
            }
        }
        if ((*Command) == CommandType_Update)
        {
            e = NvInstallBlob(partName);
            // clear the staging partition
            NvAbootBootfsFormat(partName);
            if( e != NvSuccess)
            {
                NvOsDebugPrintf("\nblob update failed\n");
                NvOsSleepMS(5000);
            }
            NvAbootReset(hAboot);
        }
    }

fail:
    NvOsFree(partName);
    return e;
}


