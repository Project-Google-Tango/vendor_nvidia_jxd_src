/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbit.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvbct.h"
#include "nvbct_customer_platform_data.h"
#include "t12x/nvboot_bct.h"
#include "nvbct_private.h"

#define NV_BCT_LENGTH sizeof(NvBootConfigTable)
#define MOD_SIZE ARSE_RSA_MAX_MODULUS_SIZE / 8 / 4

#define UNPACK_NVU32(pData8, Data32) \
    do { \
       NV_ASSERT(sizeof(*(pData8))==1); \
       NV_ASSERT(sizeof(Data32)==4); \
       (pData8)[0] = (Data32) & 0xFF; \
       (pData8)[1] = ((Data32) >> 8) & 0xFF; \
       (pData8)[2] = ((Data32) >> 16) & 0xFF; \
       (pData8)[3] = ((Data32) >> 24) & 0xFF; \
    } while (0)

#define PACK_NVU32(Data32, pData8) \
    do { \
       NV_ASSERT(sizeof(*(pData8))==1); \
       NV_ASSERT(sizeof(Data32)==4); \
       (Data32) = ((pData8)[3] << 24) | \
                  ((pData8)[2] << 16) | \
                  ((pData8)[1] << 8) | \
                  (pData8)[0]; \
    } while (0)


NvU32 NvBctGetBctSizeT12x(void)
{
    return NV_BCT_LENGTH;
}

NvError NvBctPrivGetDataSizeT12x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances)
{
    NvBootConfigTable *bct;

    NV_ASSERT(hBct);
    NV_ASSERT(Size);
    NV_ASSERT(NumInstances);

    bct = (NvBootConfigTable*)hBct;

    switch (DataType)
    {
        case NvBctDataType_BootLoaderAttribute:
        case NvBctDataType_BootLoaderVersion:
        case NvBctDataType_BootLoaderStartBlock:
        case NvBctDataType_BootLoaderStartSector:
        case NvBctDataType_BootLoaderLength:
        case NvBctDataType_BootLoaderLoadAddress:
        case NvBctDataType_BootLoaderEntryPoint:
        case NvBctDataType_BctSize:
            *Size = sizeof(NvU32);
            *NumInstances = NVBOOT_MAX_BOOTLOADERS;
            break;
        case NvBctDataType_CryptoSalt:
        case NvBctDataType_CryptoHash:
            *Size = NVBOOT_CMAC_AES_HASH_LENGTH;
            *NumInstances = 1;
            break;

        case NvBctDataType_BootLoaderCryptoHash:
            *Size = NVBOOT_CMAC_AES_HASH_LENGTH;
            *NumInstances = NVBOOT_MAX_BOOTLOADERS;
            break;

        case NvBctDataType_BootDeviceConfigInfo:
            *Size = sizeof(NvBootDevParams);
            *NumInstances = NVBOOT_BCT_MAX_PARAM_SETS;
            break;

        case NvBctDataType_BadBlockTable:
            *Size = sizeof(NvBootBadBlockTable);
            *NumInstances = 1;
            break;

        case NvBctDataType_SdramConfigInfo:
            *Size = sizeof(NvBootSdramParams);
            *NumInstances = NVBOOT_BCT_MAX_SDRAM_SETS;
            break;

        case NvBctDataType_DevType:
             *Size = sizeof(NvBootDevType);
             *NumInstances = NVBOOT_BCT_MAX_PARAM_SETS;
             break;

        case NvBctDataType_AuxData:
            *Size = NVBOOT_BCT_CUSTOMER_DATA_SIZE -
                    sizeof(NvBctCustomerPlatformData);
            *NumInstances = 1;
            break;

        case NvBctDataType_AuxDataAligned:
            {
                NvU8 *ptr, *alignedPtr;
                NvU32 AuxDataSize;

                AuxDataSize = NVBOOT_BCT_CUSTOMER_DATA_SIZE -
                              sizeof(NvBctCustomerPlatformData);
                ptr = bct->CustomerData + sizeof(NvBctCustomerPlatformData);
                alignedPtr = (NvU8*)((((NvU32)ptr + sizeof(NvU32) - 1) >> 2)
                                     << 2);
                *Size = AuxDataSize - ((NvU32)alignedPtr - (NvU32)ptr);
                *NumInstances = 1;
            }
            break;

        case NvBctDataType_DevParamsType:
            *Size = sizeof(NvU32);
            *NumInstances = NVBOOT_BCT_MAX_PARAM_SETS;
            break;

        case NvBctDataType_FullContents:
            *Size = sizeof(NvBootConfigTable);
            *NumInstances = 1;
            break;

        case NvBctDataType_Reserved:
            *Size = sizeof(NvU8);
            *NumInstances = NVBOOT_BCT_RESERVED_SIZE;
            break;

        case NvBctDataType_RsaKeyModulus:
            *Size = MOD_SIZE;
            *NumInstances = 1;
            break;

        case NvBctDataType_RsaPssSig:
            *Size = MOD_SIZE;
            *NumInstances = 1;
            break;

        case NvBctDataType_BootloaderRsaPssSig:
            *Size = MOD_SIZE;
            *NumInstances = NVBOOT_MAX_BOOTLOADERS;
            break;

        case NvBctDataType_UniqueChipId:
            *Size = sizeof(NvBootECID);
            *NumInstances = 1;
            break;

        default:
            return NvError_NotImplemented;
    }
    return NvSuccess;
}

static NvError NvBctPrivGetDataT12x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data)
{
    NvBootConfigTable *bct;
    NvU8 *Data8 = (NvU8 *)Data;

    bct = (NvBootConfigTable*)hBct;
    if (!hBct)
        return NvError_BadParameter;

    if (!Size || !Instance)
        return NvError_InvalidAddress;

    if (!Data)
        return NvError_InvalidAddress;

    switch (DataType)
    {
        case NvBctDataType_BootLoaderAttribute:
        {
            NvOsMemcpy(Data, &(bct->BootLoader[*Instance].Attribute), *Size);
        }
        break;

        case NvBctDataType_BootLoaderVersion:
        {
            NvOsMemcpy(Data, &(bct->BootLoader[*Instance].Version), *Size);
        }
        break;

        case NvBctDataType_DevParamsType:
        {
            NvOsMemcpy(Data, &(bct->DevParams[*Instance]), *Size);
        }
        break;

        case NvBctDataType_RsaKeyModulus:
        {
            NvU32 i;
            NV_ASSERT(*Size == MOD_SIZE);

            for (i = 0; i < MOD_SIZE; i++)
            {
                UNPACK_NVU32(&Data8[i * sizeof(NvU32)],
                             bct->Key.Modulus[i]);
            }
        }
        break;

        case NvBctDataType_CryptoHash:
        {
            NvU32 i;
            NV_ASSERT(*Size == NVBOOT_CMAC_AES_HASH_LENGTH);

            for (i=0; i < NVBOOT_CMAC_AES_HASH_LENGTH; i++)
                UNPACK_NVU32(&Data8[i * sizeof(NvU32)],
                    bct->Signature.CryptoHash.hash[i]);
        }
        break;

        case NvBctDataType_BootLoaderCryptoHash:
        {
            NvU32 i;
            NV_ASSERT(*Size == NVBOOT_CMAC_AES_HASH_LENGTH);

            for (i=0; i < NVBOOT_CMAC_AES_HASH_LENGTH; i++)
                UNPACK_NVU32(&Data8[i * sizeof(NvU32)],
                     bct->BootLoader[*Instance].Signature.CryptoHash.hash[i]);
        }
        break;

        case NvBctDataType_RsaPssSig:
        {
            NvU32 i;
            NV_ASSERT(*Size == MOD_SIZE);

            for (i=0; i < MOD_SIZE; i++)
                UNPACK_NVU32(&Data8[i * sizeof(NvU32)],
                    bct->Signature.RsaPssSig.Signature[i]);
        }
        break;

        case NvBctDataType_BootloaderRsaPssSig:
        {
            NvU32 i;
            NV_ASSERT(*Size == MOD_SIZE);

            for (i=0; i < MOD_SIZE; i++)
                UNPACK_NVU32(&Data8[i * sizeof(NvU32)],
                     bct->BootLoader[*Instance].Signature.RsaPssSig.Signature[i]);
        }
        break;

        case NvBctDataType_UniqueChipId:
        {
            NvOsMemcpy(Data, &bct->UniqueChipId, *Size);
        }
        break;

        default:
            return NvError_NotImplemented;
    }
    return NvSuccess;
}

static NvError NvBctPrivSetDataT12x(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data)
{
    NvBootConfigTable *bct;
    NvU8 *Data8 = (NvU8 *)Data;

    bct = (NvBootConfigTable*)hBct;
    if (!hBct)
        return NvError_BadParameter;

    if (!Size || !Instance)
        return NvError_InvalidAddress;

    if (!Data)
        return NvError_InvalidAddress;

    switch (DataType)
    {
        case NvBctDataType_BootLoaderAttribute:
        {
            NvOsMemcpy(&(bct->BootLoader[*Instance].Attribute), Data, *Size);
        }
        break;

        case NvBctDataType_BootLoaderVersion:
        {
            //if (bct->EnableFailBack)
                NvOsMemcpy(&(bct->BootLoader[*Instance].Version), Data, *Size);
        }
        break;

        case NvBctDataType_CryptoHash:
        {
            NvU32 i;
            NV_ASSERT(*Size == NVBOOT_CMAC_AES_HASH_LENGTH);

            for (i=0; i < NVBOOT_CMAC_AES_HASH_LENGTH; i++)
                PACK_NVU32(bct->Signature.CryptoHash.hash[i],
                    &Data8[i * sizeof(NvU32)]);
        }
        break;

        case NvBctDataType_BootLoaderCryptoHash:
        {
            NvU32 i;
            NV_ASSERT(*Size == NVBOOT_CMAC_AES_HASH_LENGTH);

            for (i=0; i < NVBOOT_CMAC_AES_HASH_LENGTH; i++)
                PACK_NVU32(
                    bct->BootLoader[*Instance].Signature.CryptoHash.hash[i],
                           &Data8[i * sizeof(NvU32)]);
        }
        break;

        case NvBctDataType_RsaPssSig:
        {
            NvU32 i;
            NV_ASSERT(*Size == MOD_SIZE);

            for (i=0; i < MOD_SIZE; i++)
                PACK_NVU32(bct->Signature.RsaPssSig.Signature[i],
                    &Data8[i * sizeof(NvU32)]);
        }
        break;

        case NvBctDataType_RsaKeyModulus:
        {
            NvU32 i;
            NV_ASSERT(*Size == MOD_SIZE);

            for (i=0; i < MOD_SIZE; i++)
                PACK_NVU32(bct->Key.Modulus[i],
                        &Data8[i * sizeof(NvU32)]);
        }
        break;

        case NvBctDataType_BootloaderRsaPssSig:
        {
            NvU32 i;
            NV_ASSERT(*Size == MOD_SIZE);

            for (i=0; i < MOD_SIZE; i++)
                PACK_NVU32(
                    bct->BootLoader[*Instance].Signature.RsaPssSig.Signature[i],
                           &Data8[i * sizeof(NvU32)]);
        }
        break;

        default:
            return NvError_NotImplemented;
    }
    return NvSuccess;
}

NvU32
NvBctPrivGetSignatureOffsetT12x(void)
{
    NvU32 Offset = 0;
    NvBootConfigTable Bct;

    Offset = (NvU32)&Bct.Signature - (NvU32)&Bct;
    return Offset;
}

NvU32
NvBctPrivGetSignDataOffsetT12x(void)
{
    NvU32 Offset = 0;
    NvBootConfigTable Bct;

    Offset = (NvU32)&Bct.RandomAesBlock - (NvU32)&Bct;
    return Offset;
}

#define NVBCT_SOC(FN) FN##T12x
#include "nvbct_common.c"
#undef UNPACK_NVU32
#undef PACK_NVU32
#undef NVBCT_SOC
