/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVFLASH_UTIL_H
#define INCLUDED_NVFLASH_UTIL_H

#include "nvcommon.h"
#include "nv3p.h"
#include "nvddk_fuse.h"
#include "nvnct.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define NV3P_AES_HASH_BLOCK_LEN 16
#define RSA_KEY_SIZE 256
/**
 * Attempts to fix broken BCT files.
 */
NvBool
nvflash_util_bct_munge( const char *bct_file, const char *tmp_file );
/**
 * Gets the bct size
 */
NvU32
nvflash_get_bct_size( void );
/**
 * sets the device ID
 */
void
nvflash_set_devid( NvU32 devid );

/**
 * gets the device ID
 */
NvU32
nvflash_get_devid( void );

/**
 * gets default entry and load address for bootloader
 */
void
nvflash_get_bl_loadaddr( NvU32* entry, NvU32* addr);

typedef struct NvFlashBlobHeaderRec
{
    enum
    {
        NvFlash_Version = 0x1,
        NvFlash_QueryRcmVersion,
        NvFlash_SendMiniloader,
        NvFlash_BlHash,
        NvFlash_EnableJtag,
        NvFlash_Bct,
        NvFlash_Fuse,
        NvFlash_Bl,
        NvFlash_mb,
        NvFlash_oem,
        NvFlash_Force32 = 0x7FFFFFFF,
    } BlobHeaderType;
    NvU32 length;
    NvU32 reserved1;
    NvU32 reserved2;
    NvU8 resbuff[NV3P_AES_HASH_BLOCK_LEN];
} NvFlashBlobHeader;

/**
 * Defines the structure needed to parse the common cfg
 */
typedef struct NvFlashBctParseInfoRec
{
    NvU32 BoardId;
    NvU32 BoardFab;
    NvU32 BoardSku;
    NvU32 MemType;
    NvU32 MemFreq;
    NvU32 SkuType;
}NvFlashBctParseInfo;

typedef struct NvFlashBinaryVersionInfoRec
{
    NvU32 MajorNum;
    NvU32 MinorNum;
    NvU32 Branch;
} NvFlashBinaryVersionInfo;

typedef enum
{
    NvFlashPreprocType_DIO = 1,
    NvFlashPreprocType_StoreBin,

    NvFlashPreprocType_Force32 = 0x7FFFFFFF,
} NvFlashPreprocType;

/**
 * Converts a .dio or a .store.bin file to the new format (dio2) by inserting
 * the required compaction blocks.
 *
 * @param filename The dio or the .store.bin file to convert
 * @param type File type: dio or store.bin.
 * @param blocksize The blocksize of the media
 * @param new_filename The converted file name
 */
NvBool
nvflash_util_preprocess( const char *filename, NvFlashPreprocType type,
    NvU32 blocksize, char **new_filename );

NvError
nvflash_util_dumpbit(NvU8 * BitBuffer, const char *dumpOption);

int
nvflash_util_atoi(char *str, int len);

/**
 * This function converts input size into the nearest multiple
 * size.
 *
 * @param num input size to be rounded off to the nearest multiple
 * @param SizeMultiple the size multiple used to round off num
 */
NvU64
nvflash_util_roundsize(NvU64 Num, NvU32 SizeMultiple);

/**
 * This function aligns the size of bootloader to multiple of
 * 16 bytes.
 *
 * @param pFileName Bootloader file name
 * @param pPadSize Size of padded buffer
 * @param pPadBuf Buffer containing padded bytes
 *
 * @return NV_TRUE if successful else NV_FALSE
 */
NvBool
nvflash_align_bootloader_length(
    const char *pFileName,
    NvU32 *pPadSize,
    NvU8 **pPadBuf);

/**
 * Uses the libnvbuildbct library to create the bctfile from the cfgfile.
 *
 * @param CfgFile Input cfg file name
 * @param BctFile Output bct file name
 * @param BoardInfo Board information to process common cfg
 * @param BctBuffer If not null then this will be updated to point to buffer
 *                  containing BCT created from input cfg file
 * @param BctSize If not null then this Will be updated with size of BCT
 *
 * @return NvSuccess if successful else NvError
 */
NvError
nvflash_util_buildbct(
    const char *pCfgFile,
    const char *pBctFile,
    const NvFlashBctParseInfo *pBctParseInfo,
    NvU8 **pBctBuffer,
    NvU32 *pBctSize);

/**
 * sets the platform info pointer.
 */
void
nvflash_set_platforminfo( Nv3pCmdGetPlatformInfo *Info );


/**
 * Perform CRC32 on the given set of data
 */
NvU32
nvflash_crc32(NvU32 CrcInitVal, const NvU8 *buf, NvU32 size);

/**
 * parses the version string information and stores it in structure
 */
void
nvflash_parse_binary_version(char* VersionStr, NvFlashBinaryVersionInfo *VersionInfo);

/**
 * Validates whether the version string is as per formats
 */
NvBool
nvflash_validate_versionstr(char* VersionStr);

/**
 * Validates the 3pserver version with nvflash app version
 */
NvBool
nvflash_validate_nv3pserver_version(Nv3pSocketHandle s_hSock);

/**
 * Validates the nvsecuretool version with nvflash app version
 */
NvBool
nvflash_check_compatibility(void);

/**
 * Displays the detailed version info between 2 major versions
 */
NvBool
nvflash_display_versioninfo(NvU32 Major1 ,NvU32 Major2);

/**
 * Parses blob structure for particular header type
 */

NvBool
nvflash_parse_blob(
    const char *BlobFile,
    NvU8 **pMsgBuffer,
    NvU32 HeaderType,
    NvU32 *length);


/**
 * Sets passed BoardInfo in the CustomerData of BCT buffer.
 */
void
nvflash_set_nctinfoinbct(NctCustInfo NCTCustInfo, NvU8 * BctBuffer);


/**
* Signs the warmboot code present in the WB0 partition in case of
* non secure mode.
 */
NvBool nvflash_sign_wb0(const char *pFileName, NvU32 *Wb0Codelength,
                        NvU8 **Wb0Buf);

/**
 * sets the opmode in simulation
 */
void
nvflash_set_opmode(NvU32 opmode);

/**
 * gets the  opmode in simulation
 */
NvDdkFuseOperatingMode
nvflash_get_opmode(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVFLASH_UTIL_H */
