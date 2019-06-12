/*
 * Copyright (c) 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_T14X_FUSE_PRIV_H
#define INCLUDED_NVDDK_T14X_FUSE_PRIV_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "FUSE_bitmap.ref"
#include "t14x/arfuse.h"

#define NVDDK_SECURE_BOOT_KEY_BYTES  (16)
#define NVDDK_RESERVED_ODM_BYTES  (32)
#define NVDDK_UID_BYTES (16)
#define NVDDK_DEVICE_KEY_BYTES       (4)
#define ARSE_SHA256_HASH_SIZE   256

// Number of words of fuses as defined by the hardware.
#define FUSE_WORDS 192

/*
 * Representation of the fuse data.
 *
 */
typedef struct NvDdkFuseDataRec
{
    // Specifies the Secure Boot Key (SBK).
    NvU8   SecureBootKey[NVDDK_SECURE_BOOT_KEY_BYTES];

    // Specifies the Device Key (DK).
    NvU8   DeviceKey[NVDDK_DEVICE_KEY_BYTES];

    // Specifies the JTAG Disable fuse value.
    NvBool JtagDisable;

    // Specifies the device selection value in terms of the API.
    NvU32  SecBootDeviceSelect;

    // Specifies the device selection value in the actual fuses.
    NvU32  SecBootDeviceSelectRaw;

    // Specifies the device configuration value (right aligned).
    NvU32  SecBootDeviceConfig;

    // Specifies the SwReserved value.
    NvU32  SwReserved;

    // Specifies the ODM Production fuse value.
    NvBool OdmProduction;

    // Specifies the SkipDevSelStraps value.
    NvU32  SkipDevSelStraps;

    // Specifies the Reserved Odm value
    NvU8    ReservedOdm[NVDDK_RESERVED_ODM_BYTES];

    // Flags that indicate what to program.
    NvU32  ProgramFlags;

    // Flag indicates whether reservedodm fuses are setting or not
    NvBool ReservedOdmFlag;

    // Specifies the Pkc_disable fuse value (Only used when security_mode has been set)
    NvBool PkcDisable;

    // Specifies the vp8_enable(enable Vp8 encode/decode) fuse value.
    NvBool Vp8Enable;

    // Specifies whether odm[3:0] bits are write protected or not. 1<->1 conditional mapping
    NvU32 OdmLock;

    // Specifies the Public key hash
    NvU8 PublicKeyHash[ARSE_SHA256_HASH_SIZE / 8];

    // Stores USB Pid and other odm info which is permanently burnt
    NvU32 OdmInfo;
} NvDdkFuseData;

typedef enum
{
    NvDdkFuseBootDevice_Sdmmc,
    NvDdkFuseBootDevice_SnorFlash,
    NvDdkFuseBootDevice_SpiFlash,
    NvDdkFuseBootDevice_NandFlash,
    NvDdkFuseBootDevice_Usb3,
    NvDdkFuseBootDevice_resvd_5,
    NvDdkFuseBootDevice_resvd_6, // for SATA on T40
    NvDdkFuseBootDevice_resvd_7,
    NvDdkFuseBootDevice_Max, /* Must appear after the last legal item */
    NvDdkFuseBootDevice_Force32 = 0x7fffffff
} NvDdkFuseBootDevice;


NvU32 NvDdkToFuseBootDeviceSelMap(NvU32 RegData);

NvU32 NvFuseDevToBootDevTypeMap(NvU32 RegData);

NvError NvDdkFusePrivGetTypeSize(NvDdkFuseDataType Type,
                                 NvU32 *pSize);

void NvDdkFusePrivMapDataToFuseArrays(void);

void StartRegulator(void);

void StopRegulator(void);

NvU32 NvDdkFuseGetFusePgmCycles(void);

NvError NvDdkFuseSetPriv(void** pData, NvDdkFuseDataType type,
                   NvDdkFuseData* p_FuseData, NvU32 Size);

void NvDdkFuseFillChipOptions(void);

void EnableVppFuse(void);

void DisableVppFuse(void);

#ifdef __cplusplus
}
#endif

#endif
