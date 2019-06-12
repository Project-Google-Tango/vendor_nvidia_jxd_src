/*
 * Copyright (c) 2010-2012, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvbit.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvbct.h"
#include "nvbct_customer_platform_data.h"
#include "t30/nvboot_bct.h"
#include "nvbct_private.h"
#include "nvbct_nvinternal_t30.h"

#define NV_BCT_LENGTH sizeof(NvBootConfigTable)

#ifdef NV_EMBEDDED_BUILD
#define BCT_CUSTOMER_DATA_SIZE 1344
#endif

NvU32 NvBctGetBctSizeT30(void)
{
    return NV_BCT_LENGTH;
}

NvError NvBctPrivGetDataSizeT30(
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
#ifdef NV_EMBEDDED_BUILD
        case NvBctDataType_EnableFailback:
            *Size = sizeof(NvU8);
            *NumInstances = 1;
            break;
        case NvBctDataType_HashedPartition_PartId:
            *Size = sizeof(NvU32);
            *NumInstances = NV_BCT_NUM_HASHED_PARTITION;
            break;
        case NvBctDataType_HashedPartition_CryptoHash:
            *Size = NVBOOT_CMAC_AES_HASH_LENGTH_BYTES;
            *NumInstances = NV_BCT_NUM_HASHED_PARTITION;
            break;
        case NvBctDataType_InternalInfoOneTimeRaw:
            *Size = sizeof(NvInternalOneTimeRaw);
            *NumInstances = 1;
            break;
#endif
        default:
            return NvError_NotImplemented;
    }
    return NvSuccess;
}

static NvError NvBctPrivGetDataT30(
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
#ifdef NV_EMBEDDED_BUILD
        case NvBctDataType_EnableFailback:
        {
            NvOsMemcpy(Data, &(bct->EnableFailBack), *Size);
        }
        break;

        case NvBctDataType_HashedPartition_PartId:
        {
            NvInternalInfoRaw *InternalInfoRaw =
                (NvInternalInfoRaw *)
                (&(bct->CustomerData[BCT_CUSTOMER_DATA_SIZE]));
            NvBctHashedPartitionInfoRaw *BctHashedPartitionInfoRaw =
                InternalInfoRaw->BctHashedPartInfoRaw;
            NvOsMemcpy(Data,
                &(BctHashedPartitionInfoRaw[*Instance].PartitionId[0]), *Size);
        }
        break;

        case NvBctDataType_HashedPartition_CryptoHash:
        {
            NvInternalInfoRaw *InternalInfoRaw =
                (NvInternalInfoRaw *)
                (&(bct->CustomerData[BCT_CUSTOMER_DATA_SIZE]));
            NvBctHashedPartitionInfoRaw *BctHashedPartitionInfoRaw =
                InternalInfoRaw->BctHashedPartInfoRaw;
            NvOsMemcpy(Data,
                &(BctHashedPartitionInfoRaw[*Instance].CryptoHash[0]), *Size);
        }
        break;

        case NvBctDataType_InternalInfoOneTimeRaw:
        {
            NvInternalInfoRaw *InternalInfoRaw =
                (NvInternalInfoRaw *)
                (&(bct->CustomerData[BCT_CUSTOMER_DATA_SIZE]));
            NvOsMemcpy(Data, &(InternalInfoRaw->InternalOneTimeRaw), *Size);
        }
        break;
#endif

        case NvBctDataType_CryptoHash:
        {
            NvU32 i;
            NV_ASSERT(*Size == NVBOOT_CMAC_AES_HASH_LENGTH);

            for (i=0; i < NVBOOT_CMAC_AES_HASH_LENGTH; i++)
                UNPACK_NVU32(&Data8[i*sizeof(NvU32)], bct->CryptoHash.hash[i]);
        }
        break;

        case NvBctDataType_BootLoaderCryptoHash:
        {
            NvU32 i;
            NV_ASSERT(*Size == NVBOOT_CMAC_AES_HASH_LENGTH);

            for (i=0; i < NVBOOT_CMAC_AES_HASH_LENGTH; i++)
                UNPACK_NVU32(&Data8[i*sizeof(NvU32)],
                             bct->BootLoader[*Instance].CryptoHash.hash[i]);
        }
        break;

        default:
            return NvError_NotImplemented;
    }
    return NvSuccess;
}

static NvError NvBctPrivSetDataT30(
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

#ifdef NV_EMBEDDED_BUILD
        case NvBctDataType_EnableFailback:
        {
            NvOsMemcpy(&(bct->EnableFailBack), Data, *Size);
        }
        break;

        case NvBctDataType_HashedPartition_PartId:
        {
            NvInternalInfoRaw *InternalInfoRaw =
                (NvInternalInfoRaw *)
                (&(bct->CustomerData[BCT_CUSTOMER_DATA_SIZE]));
            NvBctHashedPartitionInfoRaw *BctHashedPartitionInfoRaw =
                InternalInfoRaw->BctHashedPartInfoRaw;
            NvOsMemcpy(&(BctHashedPartitionInfoRaw[*Instance].PartitionId[0]),
                Data, *Size);
        }
        break;

        case NvBctDataType_HashedPartition_CryptoHash:
        {
            NvU8 Version = NV_BCT_HASHED_PART_INFO_VERSION;
            // Flashing the NvInternal Version with CMAC. Since we needs to update the version for
            // nvinternal version each time we flash
            NvU32 NvInternalVersion = NVINTERNAL_ONE_TIME_RAW_VERSION;
            NvInternalInfoRaw *InternalInfoRaw =
                (NvInternalInfoRaw *)
                (&(bct->CustomerData[BCT_CUSTOMER_DATA_SIZE]));
            NvBctHashedPartitionInfoRaw *BctHashedPartitionInfoRaw =
                   InternalInfoRaw->BctHashedPartInfoRaw;
            NvOsMemcpy(&(BctHashedPartitionInfoRaw[*Instance].Version),
                    &Version, sizeof(Version));
            NvOsMemcpy(&(BctHashedPartitionInfoRaw[*Instance].CryptoHash[0]),
                    Data, *Size);
            NvOsMemcpy(&(InternalInfoRaw->InternalInfoVersionRaw),
                    &NvInternalVersion, sizeof(NvInternalInfoVersionRaw));
        }
        break;

        case NvBctDataType_InternalInfoOneTimeRaw:
        {
            NvInternalInfoRaw *InternalInfoRaw =
                (NvInternalInfoRaw *)
                (&(bct->CustomerData[BCT_CUSTOMER_DATA_SIZE]));
            NvOsMemcpy(&(InternalInfoRaw->InternalOneTimeRaw), Data, *Size);
        }
        break;
#endif

        case NvBctDataType_CryptoHash:
        {
            NvU32 i;
            NV_ASSERT(*Size == NVBOOT_CMAC_AES_HASH_LENGTH);

            for (i=0; i < NVBOOT_CMAC_AES_HASH_LENGTH; i++)
                PACK_NVU32(bct->CryptoHash.hash[i], &Data8[i*sizeof(NvU32)]);
        }
        break;

        case NvBctDataType_BootLoaderCryptoHash:
        {
            NvU32 i;
            NV_ASSERT(*Size == NVBOOT_CMAC_AES_HASH_LENGTH);

            for (i=0; i < NVBOOT_CMAC_AES_HASH_LENGTH; i++)
                PACK_NVU32(bct->BootLoader[*Instance].CryptoHash.hash[i],
                       &Data8[i*sizeof(NvU32)]);
        }
        break;

        default:
            return NvError_NotImplemented;
    }
    return NvSuccess;
}

#define NVBCT_SOC(FN) FN##T30
#include "nvbct_common.c"
#undef NVBCT_SOC
#undef UNPACK_NVU32
#undef PACK_NVU32
