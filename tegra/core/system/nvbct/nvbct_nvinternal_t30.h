/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef _INCLUDE_NVBCT_NVINTERNAL_H
#define _INCLUDE_NVBCT_NVINTERNAL_H

#include "nvbct_customer_platform_data.h"
#include "nvbct.h"
#define EMBEDDED_P1852_PROJECT 61852

/* 128 bit (16 bytes) AES is used */
#define NVBOOT_CMAC_AES_HASH_LENGTH_BYTES 16
#define NV_BCT_NUM_HASHED_PARTITION 10

#define IS_ANY_PAD_IN_INTERNALINFO(x)                                                 \
    if(sizeof(NvInternalInfoRaw) == (sizeof(NvInternalInfoVersionRaw) +               \
                sizeof(NvBctAuxInternalInfoRaw) + sizeof(NvBctAuxInfoRaw) +           \
                (sizeof(NvBctHashedPartitionInfoRaw) * NV_BCT_NUM_HASHED_PARTITION) + \
                sizeof(NvBoardSerialNumDataRaw) + sizeof(NvMacIdInfoDataRaw) +        \
                (sizeof(NvSkuInfoRaw) * 2) ))                                         \
        x = NV_FALSE;                                                                 \
    else                                                                              \
        x = NV_TRUE

#define NV_BCT_AUX_INFO_VERSION 1
typedef struct NvBctAuxInfoRawRec
{
    NvU8 Version;
    /// BCT page table starting sector
    NvU8      StartLogicalSector[2];

    /// BCT page table number of logical sectors
    NvU8      NumLogicalSectors[2];
    NvU8 Reserved[4];

} NvBctAuxInfoRaw;

#define NV_BCT_AUX_INTERNAL_INFO_VERSION 1
typedef struct NvBctAuxInternalInfoRawRec
{
    NvU8 Version;
    NvU8 CustomerOption[4];
    NvU8 BctPartitionId;
    NvU8 Reserved[4];
} NvBctAuxInternalInfoRaw;

typedef struct NvBctHashedPartitionInfoRec
{
    NvU32 PartitionId;
    NvU32 CryptoHash[NVBOOT_CMAC_AES_HASH_LENGTH_BYTES / 4];
} NvBctHashedPartitionInfo;

#define INTERFACE_NAME_LEN 5
#define ETH_ALEN 6
typedef struct MacIdArrayRec {
    NvS8 Interface[INTERFACE_NAME_LEN];
    NvU8 MacId[ETH_ALEN];
} NvMacIdInfoData;

#define NV_MAC_ID_INFO_DATA_VERSION 1
typedef struct MacIdArrayRawRec {
    NvU8 Version;
    NvS8 Interface[INTERFACE_NAME_LEN];
    NvU8 MacId[ETH_ALEN];
    NvU8 Reserved[8];
} NvMacIdInfoDataRaw;

#define NV_BOARD_SERIAL_NUM_DATA_VERSION 1
typedef struct BoardSerialNumRawRec {
    NvU8 Version;
    NvU8 SerialNum[8];
    NvU8 Reserved[4];
} NvBoardSerialNumDataRaw;

#define NV_BCT_HASHED_PART_INFO_VERSION 1
typedef struct NvBctHashedPartitionInfoRawRec
{
    NvU8 Version;
    NvU8 PartitionId[4];
    NvU8 CryptoHash[NVBOOT_CMAC_AES_HASH_LENGTH_BYTES];
    NvU8 Reserved[8];
} NvBctHashedPartitionInfoRaw;

#define CURRRENT_SKUINFO_VERSION_T30 1
typedef struct NvSkuInfoRawRec
{
    NvU8 Version;
    NvU8 TestConfig;
    NvU8 BomPrefix[2];
    NvU8 Project[4];
    NvU8 SkuId[4];
    NvU8 Revision[2];
    NvU8 SkuVersion[2];
    NvU8 Reserved[8];
} NvSkuInfoRaw;

#define CURRRENT_PRODUCT_INFO_VERSION_T30 1
typedef NvSkuInfoRaw NvProdInfoRaw;

typedef struct NvInternalInfoVersionRec
{
    NvU32 InternalInfoVersion;
} NvInternalInfoVersion;

#define CURRRENT_INTERNALINFO_VERSION_T30 1
typedef struct NvInternalInfoVersionRawRec
{
    NvU8 InternalInfoVersion[4];
} NvInternalInfoVersionRaw;

#define NVINTERNAL_ONE_TIME_RAW_VERSION 1
typedef struct NvInternalOneTimeRawRec
{
    NvBoardSerialNumDataRaw BoardSerialNumDataRaw;
    NvMacIdInfoDataRaw MacIdInfoDataRaw;
    NvSkuInfoRaw SkuInfoRaw;
    NvProdInfoRaw ProductInfoRaw;
    NvU8 Reserved[40];
    NvU8 Version;
}NvInternalOneTimeRaw;

typedef struct NvInternalInfoRawRec
{
    NvInternalInfoVersionRaw  InternalInfoVersionRaw;
    NvBctAuxInternalInfoRaw BctAuxInternalInfoRaw;
    NvBctAuxInfoRaw BctAuxInfoRaw;
    NvBctHashedPartitionInfoRaw BctHashedPartInfoRaw[NV_BCT_NUM_HASHED_PARTITION];
    NvInternalOneTimeRaw InternalOneTimeRaw;
} NvInternalInfoRaw;

#endif
