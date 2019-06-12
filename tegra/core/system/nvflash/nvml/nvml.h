/*
 * Copyright (c) 2008 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVML_H
#define INCLUDED_NVML_H

#include "nvml_private.h"
#include "nvbct.h"
#include "nvddk_fuse.h"

#if NVCPU_IS_64_BITS
typedef NvU64 NvDatum;
#else
typedef NvU32 NvDatum;
#endif

#define BOOTROM_ZERO_BIT_WAR_390163 1

#define MINILOADER_SIGNATURE 0x5AFEADD8

//Defines stolen shamelessly from nvboot_strap.h from the hw tree
#define NVBOOT_STRAP_DEVICE_CONFIGURATION_SHIFT (2)
#define NVBOOT_STRAP_DEVICE_CONFIGURATION_MASK (0x3)

#define NVBOOT_STRAP_SDRAM_CONFIGURATION_SHIFT (0)
#define NVBOOT_STRAP_SDRAM_CONFIGURATION_MASK (0x3)

#define NV_ADDRESS_MAP_MISC_BASE 0x70000000
#define NV_ADDRESS_MAP_FUSE_BASE 0x7000F800
#define TIMER1 0x60005008
#define VECTOR_IRQ 0x6000F218
#define VECTOR_SWI 0x6000F208

#define NVBOOT_AES_BLOCK_LENGTH_BYTES (NVBOOT_AES_BLOCK_LENGTH * sizeof(NvU32))

NvU8 NvBootStrapSdramConfigurationIndex( void );
NvU8 NvBootStrapDeviceConfigurationIndex( void );
NvBool NvBootFuseIsOdmProductionModeFuseSet(void);
NvBool NvBootFuseIsNvProductionModeFuseSet(void);
void NvBootSskGenerate(NvBootAesEngine SbkEngine, NvBootAesKeySlot SbkEncryptSlot, NvU8 *ssk);
#if !__GNUC__
__asm void ExecuteBootLoader( NvU32 entry );
#else
NV_NAKED void ExecuteBootLoader( NvU32 entry );
#endif
Nv3pDkStatus CheckIfDkBurned( void );
NvBool CheckIfSbkBurned( void );
void ReadBootInformationTable(NvBootInfoTable **ppBitTable);
void server( void );
NvBootError NvMlBootGetBct(NvU8 **bct, NvDdkFuseOperatingMode Mode);
// set the user defined boot device config fuse value
void NvMlSetBootDeviceConfig(NvU32 ConfigIndex);
// get the user defined boot device config value
void NvMlGetBootDeviceConfig(NvU32 *pConfigVal);
// set the user defined boot device type fuse value
void NvMlSetBootDeviceType(NvBootFuseBootDevice DevType);
// get the user defined boot device type fuse value
void NvMlGetBootDeviceType(NvBootFuseBootDevice *pDevType);
// decrypts bct and bootloader(sign also) got from nvsbk utility in odm secure mode
NvBool NvMlDecryptValidateImage(NvU8 *pBootloader, NvU32 BlLength, NvBool IsFirstChunk,
        NvBool IsLastChunk, NvBool IsSign, NvBool IsBct, NvU8 * BlHash);
// Initializes the BctDeviceHandle for reading and validating the bct.
NvBootError NvMlBctDeviceHandleInit(void);

// Verify the address pins of data bus
NvDatum VerifySdramDataBus(volatile NvDatum *addr);

// Verify the address pins of address bus
NvBool VerifySdramAddressBus(volatile NvDatum *testBaseAddr, NvU32 testSize);

#endif//INCLUDED_NVML_H
