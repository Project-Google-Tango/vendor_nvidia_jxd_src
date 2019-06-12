/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @defgroup ArchiveDriver Archive Services
 * @ingroup HighLevelServices
 */

/**
 * @addtogroup ArchiveDriver
 * @{
 */

/**
 * @file drv_arch_type.h Archive file utilities
 *
 */

#ifndef DRV_ARCH_TYPE_H
#define DRV_ARCH_TYPE_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/
#ifndef HOST_TESTING
#include "drv_fs_cfg.h"
#endif

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/
/* zlib arch utilities */
#define ZIP_ARCH_VER_1_0        0x1CE70100
#define VERIFY_DATA_IN_RAM      NULL
#define SKIP_ZIP_ARCH_CHECK     NULL
#define ZIP_MODE_NONE           0
#define ZIP_MODE_ZLIB           1
#define ZIP_MODE_LAST           ZIP_MODE_ZLIB

#define ARCH_ID_MASK            0x00FFFFFF
#define ZIP_MODE_MASK           0xFF000000
#define ZIP_MODE_REQ_SHIFT      24
#define GET_ARCH_ID(x)          ((x) & ARCH_ID_MASK)
#define GET_ZIP_MODE(x)         ((uint8)(((x) & ZIP_MODE_MASK) >> ZIP_MODE_REQ_SHIFT))

/* For archive acquisition from flash chunk per chunk */
#define DRV_ARCH_CHUNK  1024

/* Archive header constants */
#define SIZEOF_TLV_TAG          sizeof(unsigned int)
#define SIZEOF_TLV_LENGTH       sizeof(unsigned int)
/* Header RFU used for Extended header (ASN1 or full XML) */
#define ARCH_RFU_FIELD_SZ_ASN1  0x80
#define ARCH_RFU_FIELD_SZ_XML   0x120
#if defined (TARGET_DXP8060)
#define ARCH_RFU_FIELD_SZ       ARCH_RFU_FIELD_SZ_ASN1
#else
#define ARCH_RFU_FIELD_SZ       ARCH_RFU_FIELD_SZ_XML
#endif
#define ARCH_HEADER_MAX_SZ      (sizeof(tAppliFileHeader) - SIZEOF_TLV_TAG - SIZEOF_TLV_LENGTH)
#define ARCH_HEADER_BYTE_LEN    32

#define RSA_SIGNATURE_SIZE  256
#define NONCE_SIZE          256
#define KEY_ID_BYTE_LEN     4
#define SHA1_DIGEST_SIZE 	20
#define SHA2_DIGEST_SIZE 	32

#define ARCH_SIGN_TYPE__SHA1    0
#define ARCH_SIGN_TYPE__RSA     1 /* e.g SHA1-RSA signed */
#define ARCH_SIGN_TYPE__SHA2    2
#define ARCH_SIGN_TYPE__RSA2    3 /* e.g SHA2-RSA signed */

#define ARCH_TAG_8040       0x1CE8040A
#define ARCH_TAG_8060       0x1CE8060A
#define ARCH_TAG_9040       0x1CE9040A
#define ARCH_TAG_9140       0x1CE9140A

#define ARCH_TAG_BT2_TRAILER 0x1CEB72AB

#define IMEI_LENGTH 15 

/**
 * Errors returned by Arch functions.
 */
typedef enum
{
    ARCH_ERR_NO_ERROR
    ,ARCH_ERR_INVALID_SECURITY_STATE  /* Operation not allowed because platform security state is not valid. */
    ,ARCH_ERR_PROTECTED_ARCH          /* Operation not allowed because platform security state forbids it */
    ,ARCH_ERR_ARCH_NOT_FOUND          /* Parsing of arch_type table failed.*/
}ArchError;

/**
 *  HOW-TO ADD A NEW WRAPPED ARCH ENTRY:
 *
 *  1: Update ArchID enum with a new entry
 *  2: Define a new acronym string for the corresponding archive
 *  3: Create a new property entry for the corresponding archive
 *  (a property entry is a tArchFileProperty). 3: update
 *  DRV_ARCH_TABLE with a new entry above ARCH_LAST_DUMMY_FILE
 *
 *  NOTE: it is appreciated to define the new path in filesystem
 *  from tArchFileProperty in drivers/private/filesystem/public/drv_fs_cfg.h
 *  so that it can be easily shared with all modules.
 *
 *  Also see drv_arch_utils.c where there is a file path table
 */

/**
 *  Archive Id constants
 *
 *  Written in file header for identification by embedded
 *  applications.
 *
 */
typedef enum
{
    ARCH_ID_APP = 0
    ,ARCH_ID_BT2
    ,ARCH_ID_IFT
    ,ARCH_ID_LDR
    ,ARCH_ID_IMEI
    ,ARCH_ID_CUSTCFG
    ,ARCH_ID_ZEROCD
    ,ARCH_ID_MASS
    ,ARCH_ID_AUDIOCFG
    ,ARCH_ID_COMPAT
    ,ARCH_ID_PLATCFG
    ,ARCH_ID_SECCFG
    ,ARCH_ID_UNLOCK
    ,ARCH_ID_CALIB
    ,ARCH_ID_CALIB_PATCH
    ,ARCH_ID_SSL_CERT
    ,ARCH_ID_DEVICECFG
    ,ARCH_ID_PRODUCTCFG
    ,ARCH_ID_ROBCOUNTER
    ,ARCH_ID_FLASHDISK
    ,ARCH_ID_WEBUI_PACKAGE
    ,ARCH_ID_BT3
    ,ARCH_ID_ACT
    ,ARCH_ID_ACT_DATA
}ArchId;

/**
 *  Archive acronyms
 *
 *  Used by tools responsible for file generation/update
 *
 */
#define ARCH_ACR_APP             "MDM"
#define ARCH_ACR_BT2             "BT2"
#define ARCH_ACR_IFT             "IFT"
#define ARCH_ACR_LDR             "LDR"
#define ARCH_ACR_IMEI            "IMEI"
#define ARCH_ACR_CUSTCFG         "CUSTCFG"
#define ARCH_ACR_ZEROCD          "ZEROCD"
#define ARCH_ACR_MASS            "MASS"
#define ARCH_ACR_AUDIOCFG        "AUDIOCFG"
#define ARCH_ACR_COMPAT          "COMPAT"
#define ARCH_ACR_PLATCFG         "PLATCFG"
#define ARCH_ACR_SECCFG          "SECCFG"
#define ARCH_ACR_UNLOCK          "UNLOCK"
#define ARCH_ACR_CALIB           "CALIB"
#define ARCH_ACR_CALIB_PATCH     "CALIB_PATCH"
#define ARCH_ACR_SSL_CERT        "SSL_CERT"
#define ARCH_ACR_DEVICECFG       "DEVICECFG"
#define ARCH_ACR_PRODUCTCFG      "PRODUCTCFG"
#define ARCH_ACR_ROBCOUNTER      "ROBCOUNTER"
#define ARCH_ACR_FLASHDISK       "FLASHDISK"
#define ARCH_ACR_WEBUI_PACKAGE   "WEBUI_PACKAGE"
#define ARCH_ACR_BT3             "BT3"
#define ARCH_ACR_ACT             "ACT"
#define ARCH_ACR_ACT_DATA        "ACT_DATA"

/* Archive properties: see tArchFileProperty below */
#define ARCH_PROP_APP          {ARCH_ACR_APP,      ARCH_ID_APP,     ARCH_ICE_OEM_KEY_SET,  false, false, ARCH_TYPE_APPLI,true, false, false}
#define ARCH_PROP_BT2          {ARCH_ACR_BT2,      ARCH_ID_BT2,     ARCH_ICE_ICE_KEY_SET,  false, false, ARCH_TYPE_APPLI,true, false, false}
#define ARCH_PROP_IFT          {ARCH_ACR_IFT,      ARCH_ID_IFT,     ARCH_ICE_OEM_KEY_SET,  false, false, ARCH_TYPE_APPLI,true, false, false}
#define ARCH_PROP_LDR          {ARCH_ACR_LDR,      ARCH_ID_LDR,     ARCH_ICE_OEM_KEY_SET,  false, false, ARCH_TYPE_APPLI,true, false, false}
#define ARCH_PROP_BT3                 {ARCH_ACR_BT3,         ARCH_ID_BT3,         ARCH_ICE_OEM_KEY_SET,   false, false, ARCH_TYPE_APPLI, true,  false, false}
#define ARCH_PROP_ACT                 {ARCH_ACR_ACT,         ARCH_ID_ACT,         ARCH_ICE_OEM_KEY_SET,   false, false, ARCH_TYPE_APPLI, true,  false, false}

#define ARCH_PROP_ACT_DATA            {ARCH_ACR_ACT_DATA,    ARCH_ID_ACT_DATA,    ARCH_OEM_FACT_KEY_SET,  true,  false, ARCH_TYPE_DATA,  true,  false, false}

#define ARCH_PROP_IMEI_NO_AUTH        {ARCH_ACR_IMEI,        ARCH_ID_IMEI,        ARCH_NO_AUTH,           false, false, ARCH_TYPE_DATA,  true,  false, false}
#define ARCH_PROP_IMEI_EXT_AUTH       {ARCH_ACR_IMEI,        ARCH_ID_IMEI,        ARCH_EXT_AUTH,          false, false, ARCH_TYPE_DATA,  true,  false, false}
#define ARCH_PROP_IMEI_SOFT_ACTIVATION  {ARCH_ACR_IMEI,     ARCH_ID_IMEI,    ARCH_ACT_ACT_KEY_SET,  true,  false, ARCH_TYPE_DATA,true, false, false}
#define ARCH_PROP_IMEI         {ARCH_ACR_IMEI,     ARCH_ID_IMEI,    ARCH_OEM_FACT_KEY_SET, true,  false, ARCH_TYPE_DATA,true, false, false}

#define ARCH_PROP_CUSTCFG_NO_AUTH     {ARCH_ACR_CUSTCFG,     ARCH_ID_CUSTCFG,     ARCH_NO_AUTH,           false, false, ARCH_TYPE_DATA,  true,  false, false}
#define ARCH_PROP_CUSTCFG_EXT_AUTH    {ARCH_ACR_CUSTCFG,     ARCH_ID_CUSTCFG,     ARCH_EXT_AUTH,          false, false, ARCH_TYPE_DATA,  true,  false, false}
#define ARCH_PROP_CUSTCFG      {ARCH_ACR_CUSTCFG,  ARCH_ID_CUSTCFG, ARCH_OEM_FACT_KEY_SET, true,  false, ARCH_TYPE_DATA,true, false, false}

#define ARCH_PROP_ZEROCD_IGNORE_MAGIC {ARCH_ACR_ZEROCD,      ARCH_ID_ZEROCD,      ARCH_NO_AUTH,           false, true,  ARCH_TYPE_DATA,  true,  false, true}
#define ARCH_PROP_ZEROCD       {ARCH_ACR_ZEROCD,      ARCH_ID_ZEROCD,    ARCH_NO_AUTH,          false, true,  ARCH_TYPE_DATA,true, false, false}

#define ARCH_PROP_FLASHDISK    {ARCH_ACR_FLASHDISK,   ARCH_ID_FLASHDISK, ARCH_NO_AUTH,          false, true,  ARCH_TYPE_DATA,false, false, false}
#define ARCH_PROP_MASS         {ARCH_ACR_MASS,        ARCH_ID_MASS,      ARCH_ICE_OEM_KEY_SET,  false, false, ARCH_TYPE_APPLI,true, false, false}
#define ARCH_PROP_AUDIOCFG     {ARCH_ACR_AUDIOCFG,    ARCH_ID_AUDIOCFG,  ARCH_NO_AUTH,          false, false, ARCH_TYPE_DATA,true, false, false}
#define ARCH_PROP_COMPAT       {ARCH_ACR_COMPAT,      ARCH_ID_COMPAT,    ARCH_NO_AUTH,          false, false, ARCH_TYPE_DATA,true, false, false}
#define ARCH_PROP_PLATCFG      {ARCH_ACR_PLATCFG,     ARCH_ID_PLATCFG,   ARCH_NO_AUTH,          false, false, ARCH_TYPE_DATA,false, false, false}
#define ARCH_PROP_SECCFG       {ARCH_ACR_SECCFG,      ARCH_ID_SECCFG,    ARCH_ICE_FACT_KEY_SET, true,  false, ARCH_TYPE_DATA,true, false, false}

#define ARCH_PROP_CALIB_EXT_AUTH      {ARCH_ACR_CALIB,       ARCH_ID_CALIB,       ARCH_EXT_AUTH,          false, false, ARCH_TYPE_DATA,  false, false, false}
#define ARCH_PROP_CALIB      {ARCH_ACR_CALIB,    ARCH_ID_CALIB, ARCH_OEM_FACT_KEY_SET, false, false, ARCH_TYPE_DATA,false, false, false}
#define ARCH_PROP_CALIB_PATCH  {ARCH_ACR_CALIB_PATCH, ARCH_ID_CALIB_PATCH,  ARCH_OEM_FACT_KEY_SET, false, false, ARCH_TYPE_DATA,false, true, false}

#define ARCH_PROP_UNLOCK       {ARCH_ACR_UNLOCK,      ARCH_ID_UNLOCK,   ARCH_ICE_DBG_KEY_SET, true,  false, ARCH_TYPE_DATA,true, false, false}

#define ARCH_PROP_SSL_CERT     {ARCH_ACR_SSL_CERT,    ARCH_ID_SSL_CERT, ARCH_NO_AUTH,         false,  false, ARCH_TYPE_DATA,false, true, false}

#define ARCH_PROP_DEVICECFG_NO_AUTH   {ARCH_ACR_DEVICECFG,   ARCH_ID_DEVICECFG,   ARCH_NO_AUTH,           false, false, ARCH_TYPE_DATA,  true,  false, false}
#define ARCH_PROP_DEVICECFG_EXT_AUTH  {ARCH_ACR_DEVICECFG,   ARCH_ID_DEVICECFG,   ARCH_EXT_AUTH,          false, false, ARCH_TYPE_DATA,  true,  false, false}
#define ARCH_PROP_DEVICECFG  {ARCH_ACR_DEVICECFG,  ARCH_ID_DEVICECFG,  ARCH_OEM_FACT_KEY_SET,  true, false, ARCH_TYPE_DATA, true, false, false}

#define ARCH_PROP_PRODUCTCFG {ARCH_ACR_PRODUCTCFG, ARCH_ID_PRODUCTCFG, ARCH_OEM_FIELD_KEY_SET, false, false, ARCH_TYPE_DATA, true, false, false}

#define ARCH_PROP_ROBCOUNTER {ARCH_ACR_ROBCOUNTER, ARCH_ID_ROBCOUNTER, ARCH_ICE_CFG_KEY_SET, true, false, ARCH_TYPE_DATA, true, false, false}

#define ARCH_PROP_WEBUI_PACKAGE {ARCH_ACR_WEBUI_PACKAGE, ARCH_ID_WEBUI_PACKAGE, ARCH_NO_AUTH, false, false, ARCH_TYPE_DATA, false, true, false}

#ifndef __dxp__
/*
 * Archive type table FOR PC APPS ONLY
 * Not suitable for embedded applications
 */
#define DRV_ARCH_TYPE_TABLE_ON_PC                 \
{                                                 \
    ARCH_PROP_APP       \
    ,ARCH_PROP_BT2      \
    ,ARCH_PROP_IFT      \
    ,ARCH_PROP_LDR      \
    ,ARCH_PROP_IMEI     \
    ,ARCH_PROP_CUSTCFG  \
    ,ARCH_PROP_ZEROCD   \
    ,ARCH_PROP_MASS      \
    ,ARCH_PROP_AUDIOCFG \
    ,ARCH_PROP_COMPAT   \
    ,ARCH_PROP_PLATCFG   \
    ,ARCH_PROP_SECCFG   \
    ,ARCH_PROP_UNLOCK  \
    ,ARCH_PROP_CALIB \
    ,ARCH_PROP_CALIB_PATCH \
    ,ARCH_PROP_SSL_CERT \
    ,ARCH_PROP_DEVICECFG \
    ,ARCH_PROP_PRODUCTCFG \
    ,ARCH_PROP_ROBCOUNTER \
    ,ARCH_PROP_FLASHDISK  \
    ,ARCH_PROP_WEBUI_PACKAGE \
    ,ARCH_PROP_BT3 \
    ,ARCH_PROP_ACT \
    ,ARCH_PROP_ACT_DATA \
}
#endif

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/
/**
 * TLV encoded file header
 */
typedef struct
{
    unsigned int  tag;                                  /** DXP Tag: 0x1CE8040A or 0x1CE8060A */
    unsigned int  length;                               /** length of the whole File Header */

    /* Data field */
    unsigned int  file_size;                            /** size of file + size of signature */
    unsigned int  entry_address;                        /** application entry point in RAM */
    unsigned int  file_id;                              /** file identifier */
    unsigned int  sign_type;                            /** Signature type: 0=SHA1, 1=SHA1+RSA */
    unsigned int  checksum;                             /** checksum on TLV including Tag and Length */
    unsigned int  key_index;                            /** ICE-OEM key index used for acrchive signature */
    unsigned char rfu[ARCH_RFU_FIELD_SZ];               /** RFU + Padding (%32) */
} tAppliFileHeader;

/**
 * ZIP file header
 */
typedef struct
{
    unsigned int zip_arch_ver;                         /** Internal Icera Zip support version */
    unsigned int zip_header_length;                    /** length of the whole File Header */
    unsigned int zip_file_size;                        /** Size of the zipped arch (without padding bytes) */
} tZipAppliFileHeader;

/* Following type to define misc RSA key set */
typedef enum tagSigKeySet
{
    ARCH_ICE_ICE_KEY_SET
    ,ARCH_ICE_OEM_KEY_SET
    ,ARCH_OEM_FACT_KEY_SET
    ,ARCH_NO_AUTH         /* We might consider to update files with no security feature */
    ,ARCH_EXT_AUTH        /* We might consider to update files using external authentication mechanism */
    ,ARCH_ICE_FACT_KEY_SET
    ,ARCH_ICE_DBG_KEY_SET
    ,ARCH_OEM_FIELD_KEY_SET
    ,ARCH_ICE_CFG_KEY_SET
    ,ARCH_ACT_ACT_KEY_SET
}tSigKeySet;

typedef enum
{
    ARCH_TYPE_APPLI,
    ARCH_TYPE_DATA
}tArchFileType;

/**
 * Archive properties
 */
typedef struct tagArchFileProperty
{
    char *acr;                     /** Arch acronym used by external tools */
    int  arch_id;                  /** Arch ID set in arch header and used for file identification */
    tSigKeySet key_set;            /** Key set use to SHA1/RSA sign file */
    bool ppid_check;               /** Does the file embed public chip ID ? */
    bool write_file_during_auth;   /** Overwrite file during authentication:
                                    does not preserve former file. */
    tArchFileType type;            /** To distinguish between data and application file */
    bool keep_wrapped_info;        /** Is file wrapped info programmed in file system during file update */
    bool is_patch;                 /** This file must not be directly programmed but used to apply a patch */
    bool ignore_magic;             /** This archive can be used disregarding ARCH_TAG */
} tArchFileProperty;

/**
 * Type for generic patch handler.
 *
 * When applying a patch user needs: a pointer to archive header
 * and to data.
 *
 * Patch handler supposed to answer 0 on success, !=0 on error
 */
typedef int32 (*drv_archPatchHandler)(tAppliFileHeader *arch_hdr, uint8 *arch_start);

/**
 * Secondary boot archive map in memory.
 * 
 * When aqcuired, from bootROM or during AT%LOAD, data is found
 * at arch start in DMEM or in RAM. 
 *  
 * Data normally used by bootROM to copy BT2 code and data at 
 * boot time and re-used to access BT2 trailer when BT2 header 
 * is not available. 
 */
typedef struct
{
    uint32 imem_start_addr; /* start addr to copy BT2 code in IMEM */
    uint32 dmem_load_addr;  /* start addr to load BT2 code from DMEM */
    uint32 imem_size;       /* sizeof BT2 code */
} ArchBt2BootMap;

/**
 * Secondary boot extended trailer structure.
 */
typedef struct
{
    uint32 magic;   /* ARCH_TAG_BT2_TRAILER */
    uint32 size;    /* Size of data */
    uint8 *data;    /* Buffer of data */
} ArchBt2ExtTrailer;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

#endif /* #ifndef DRV_ARCH_TYPE_H */

/** @} END OF FILE */
