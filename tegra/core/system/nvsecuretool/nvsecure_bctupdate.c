/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvflash_configfile.h"
#include "nvsecure_bctupdate.h"
#include "nvsecuretool.h"

#define BYTESPERSECTOR 4096
extern NvU32 g_BlockSize;
#define BLOCKSIZE (BYTESPERSECTOR * g_BlockSize)
#define RSA_KEY_SIZE 256
#define TF_TOS_MAGIC_ID_LEN 7

static NvFlashDevice *s_Devices;
static NvU32 s_NumDevices;

extern const NvSecureMode g_SecureMode;
extern NvSecureBctInterface g_BctInterface;

void
NvDevInfo(NvFlashDevice **Devices, NvU32 *NumDevices)
{
    *Devices    = s_Devices;
    *NumDevices = s_NumDevices;
}

NvError
NvSecureParseCfg(const char *FileName)
{
    NvError e                    = NvSuccess;
    NvFlashDevice *Devices       = NULL;
    NvU32 NumDevices             = 0;
    const char *err_str          = NULL;
    NvFlashConfigFileHandle hCfg = NULL;

    e = NvFlashConfigFileParse(FileName, &hCfg);
    VERIFY(e == NvSuccess,
           err_str = NvFlashConfigGetLastError(); goto fail);

    e = NvFlashConfigListDevices(hCfg, &NumDevices, &Devices);
    VERIFY(e == NvSuccess,
           err_str = NvFlashConfigGetLastError(); goto fail );

    VERIFY(NumDevices != 0, goto fail);
    VERIFY(Devices != NULL, goto fail);

    s_Devices    = Devices;
    s_NumDevices = NumDevices;

    goto clean;

fail:
    if (err_str)
    {
        NvAuPrintf("nvflash configuration file error: %s\n", err_str);
    }

clean:
    NvFlashConfigFileClose(hCfg);
    return e;
}

NvError
NvSecurePartitionOperation(
        PartitionOperation *Operation,
        void *Aux)
{
    NvError e = NvSuccess;
    NvU32 i;
    NvU32 j;
    NvFlashDevice *Device;
    NvFlashPartition *Partition;

    Device = s_Devices;
    for (i = 0; i < s_NumDevices; i++, Device = Device->next)
    {
        Partition = Device->Partitions;
        for (j = 0; j < Device->nPartitions; j++, Partition = Partition->next)
        {
           e = (*Operation) (Partition, Aux);
           if (e != NvSuccess)
               goto fail;
        }
    }

fail:
    return e;
}

NvError
NvSecureGetNumEnabledBootloader(
        NvFlashPartition *Partition,
        void *Aux)
{
    NvU32 NumBootloader;

    if (Aux == NULL)
        return NvError_BadParameter;

    NumBootloader = *(NvU32 *) Aux;
    if (Partition->Type == Nv3pPartitionType_Bootloader)
        NumBootloader++;

    *(NvU32 *) Aux = NumBootloader;

    return NvSuccess;
}

NvError
NvSecureUpdateBctWithBlInfo(
        NvFlashPartition *Partition,
        void *Aux)
{
    NvError e = NvSuccess;
    NvU32 NumSectors;
    NvU32 PageSize;
    NvU32 BlInstance;
    NvU32 StartPage;
    NvU32 StartBlock;
    NvU32 BlockSize;
    NvU32 Size;
    NvU32 PartitionSize;
    NvU8 *BlBuf = NULL;
    NvBctHandle SecureBctHandle;
    BctBlUpdateData *Data = (BctBlUpdateData *) Aux;
    NvU32 Version = 1;
    NvU32 BlSize;
    NvU32 LoadAddress;
    NvU32 EntryPoint;
    NvU32 Instance = 0;
    NvU32 NumSdramParams = 0;

    PageSize   = Data->PageSize;
    NumSectors = Data->NumSectors;
    BlInstance = Data->BlInstance;
    BlockSize  = Data->BlockSize;
    SecureBctHandle = Data->SecureBctHandle;
    PartitionSize = (NvU32)Partition->Size;

    if (Partition->Type == Nv3pPartitionType_Bootloader)
    {
        Size = sizeof(NvU32);
        /* Convert the logicalSectors into a startBlock and StartPage in terms
         * of the page/block size that the bootrom uses (i.e. in terms of the
         * log2 values stored in the bct)
         */
        StartBlock = (NumSectors * BYTESPERSECTOR) / BlockSize;
        StartPage  = (NumSectors * BYTESPERSECTOR) % BlockSize;
        StartPage /= PageSize;

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(SecureBctHandle, NvBctDataType_BootLoaderStartBlock,
                         &Size, &BlInstance, &StartBlock)
        );

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(SecureBctHandle, NvBctDataType_BootLoaderStartSector,
                         &Size, &BlInstance, &StartPage)
        );

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(SecureBctHandle, NvBctDataType_BootLoaderAttribute,
                         &Size, &BlInstance, &Partition->Id)
        );

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(SecureBctHandle, NvBctDataType_NumEnabledBootLoaders,
                         &Size, &Instance, &Data->NumBootloaders)
        );

        NV_CHECK_ERROR_CLEANUP(
            NvSecureReadFile(Partition->Filename, &BlBuf,
                             NV_TRUE, &BlSize, PageSize, 0)
        );

        if (*((NvU32 *)BlBuf + 1) == MICROBOOT_HEADER_SIGN)
        {
            LoadAddress = g_BctInterface.NvSecureGetMbLoadAddr();
            EntryPoint  = g_BctInterface.NvSecureGetMbEntryPoint();
            NV_CHECK_ERROR_CLEANUP(
                NvBctSetData(
                    SecureBctHandle,
                    NvBctDataType_NumValidSdramConfigs,
                    &Size, &Instance, &NumSdramParams));
        }
        else
        {
            LoadAddress = g_BctInterface.NvSecureGetBlLoadAddr();
            EntryPoint  = g_BctInterface.NvSecureGetBlEntryPoint();
        }

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(SecureBctHandle, NvBctDataType_BootLoaderLoadAddress,
                         &Size, &BlInstance, &LoadAddress)
        );

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(SecureBctHandle, NvBctDataType_BootLoaderEntryPoint,
                         &Size, &BlInstance, &EntryPoint)
        );

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(SecureBctHandle, NvBctDataType_BootLoaderVersion,
                         &Size, &BlInstance, &Version)
        );

        BlSize = BlSize - NV3P_AES_HASH_BLOCK_LEN - sizeof(NvU32);

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(SecureBctHandle, NvBctDataType_BootLoaderLength,
                         &Size, &BlInstance, &BlSize)
        );

        BlInstance--;
    }

    if (PartitionSize % BLOCKSIZE != 0)
        NumSectors += (((PartitionSize / BLOCKSIZE) + 1) * g_BlockSize);
    else
        NumSectors += ((PartitionSize / BLOCKSIZE) * g_BlockSize);

    Data->NumSectors = NumSectors;
    Data->BlInstance = BlInstance;

fail:
    if (BlBuf)
        NvOsFree(BlBuf);
    return e;
}

NvError
NvSecureEncryptSignBootloaders(
        NvFlashPartition *Partition,
        void *Aux)
{
    NvError e           = NvSuccess;
    NvU32 HashSize      = 0;
    NvU8 *HashBuf       = NULL;
    NvU8 *BlBuf         = NULL;
    NvU32 PageSize      = 0;
    NvU32 BlSize        = 0;
    NvU32 PaddedSize    = 0;
    NvU32 BlInstance    = 0;
    char *OutFile       = NULL;
    NvBctHandle SecureBctHandle;
    NvBctDataType BctDataType;
    NvSecureEncryptSignBlData *Data = (NvSecureEncryptSignBlData *) Aux;

    PageSize        = Data->PageSize;
    BlInstance      = Data->BlInstance;
    SecureBctHandle = Data->SecureBctHandle;

    if (Partition->Type == Nv3pPartitionType_Bootloader)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvSecureReadFile(Partition->Filename, &BlBuf,
                             NV_TRUE, &PaddedSize, PageSize, 0)
        );
        BlSize = PaddedSize - NV3P_AES_HASH_BLOCK_LEN - sizeof(NvU32);

        NV_CHECK_ERROR_CLEANUP(
            NvSecureSignEncryptBuffer(g_SecureMode, NULL, BlBuf,
                               BlSize, &HashBuf, &HashSize, NV_FALSE, NV_TRUE)
        );

        BctDataType = g_SecureMode == NvSecure_Pkc ?
                      NvBctDataType_BootloaderRsaPssSig :
                      NvBctDataType_BootLoaderCryptoHash;

        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(SecureBctHandle, BctDataType,
                         &HashSize, &BlInstance, HashBuf)
        );

        OutFile = NvSecureGetOutFileName(Partition->Filename, NULL);
        VERIFY(OutFile != NULL, e = NvError_InsufficientMemory; goto fail);
        e = NvSecureSaveFile(OutFile, BlBuf, BlSize);
        if (e != NvSuccess)
            goto fail;
        NvAuPrintf("%s is saved as %s\n", Partition->Filename, OutFile);

        BlInstance--;
    }

    Data->BlInstance = BlInstance;

fail:
    if (BlBuf)
        NvOsFree(BlBuf);
    if (HashBuf)
        NvOsFree(HashBuf);
    if (OutFile)
        NvOsFree(OutFile);
    return e;
}


NvError
NvSecureSignTrustedOSImage(
    NvFlashPartition *pPartition,
    void *pAux)
{
    NvError e           = NvSuccess;
    NvU32 HashSize      = 0;
    NvU8 *pHashBuf      = NULL;
    NvU8 *pBuf          = NULL;
    NvU32 DataStart     = 0;
    NvU32 ImageLen      = 0;
    NvU32 PaddedSize    = 0;
    NvU32 HashStart     = 0;
    char *pOutFile      = NULL;

    if (NvOsStrcmp(pPartition->Name, "TOS") == 0 &&
        pPartition->Filename != NULL)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvSecureReadFile(pPartition->Filename, &pBuf,
                             NV_FALSE, &PaddedSize, 0, RSA_KEY_SIZE)
        );

        ImageLen  = NvUStrtoul((char *)pBuf + TF_TOS_MAGIC_ID_LEN, NULL, 10);
        DataStart = 512;
        HashStart = DataStart + ImageLen;

        NV_CHECK_ERROR_CLEANUP(
            NvSecureSignEncryptBuffer(g_SecureMode, NULL, pBuf + DataStart,
                               ImageLen, &pHashBuf, &HashSize, NV_FALSE, NV_FALSE)
        );

        NvOsMemcpy(pBuf + HashStart, pHashBuf, HashSize);

        pOutFile = NvSecureGetOutFileName(pPartition->Filename, NULL);
        VERIFY(pOutFile != NULL, e = NvError_InsufficientMemory; goto fail);
        e = NvSecureSaveFile(pOutFile, pBuf, PaddedSize);
        if (e != NvSuccess)
            goto fail;
        NvAuPrintf("%s is saved as %s\n", pPartition->Filename, pOutFile);
    }

fail:
    if (pBuf)
        NvOsFree(pBuf);
    if (pHashBuf)
        NvOsFree(pHashBuf);
    if (pOutFile)
        NvOsFree(pOutFile);

    return e;
}

NvError
NvSecureSignWB0(
    NvFlashPartition *pPartition,
    void *pAux)
{
    NvError e            = NvSuccess;
    NvU8 *pBuf           = NULL;
    NvU32 FileSize       = 0;
    char *pOutFile       = NULL;

    if ((pPartition->Type == Nv3pPartitionType_WB0) &&
        (pPartition->Filename != NULL))
    {
        if (g_BctInterface.NvSecureSignWB0Image == NULL)
            return NvError_NotImplemented;

        NV_CHECK_ERROR_CLEANUP(
            NvSecureReadFile(pPartition->Filename, &pBuf,
                             NV_TRUE, &FileSize, 0, 0)
        );
        FileSize = FileSize - (NV3P_AES_HASH_BLOCK_LEN + sizeof(NvU32));
        g_BctInterface.NvSecureSignWB0Image(pBuf, FileSize);

        pOutFile = NvSecureGetOutFileName(pPartition->Filename, NULL);
        VERIFY(pOutFile != NULL, e = NvError_InsufficientMemory; goto fail);
        e = NvSecureSaveFile(pOutFile, pBuf, FileSize);
        if (e != NvSuccess)
            goto fail;
        NvAuPrintf("%s is saved as %s\n", pPartition->Filename, pOutFile);
    }

fail:
    if (pBuf)
        NvOsFree(pBuf);
    if (pOutFile)
        NvOsFree(pOutFile);
    return e;
}

