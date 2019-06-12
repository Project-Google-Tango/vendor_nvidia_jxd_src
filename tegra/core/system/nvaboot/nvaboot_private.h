/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef NVABOOT_PRIVATE_INCLUDED_H
#define NVABOOT_PRIVATE_INCLUDED_H

#include "nvcommon.h"
#include "nvaboot.h"
#include "nvrm_init.h"
#include "nvos.h"
#include "nvddk_blockdev.h"
#include "nvddk_blockdevmgr_defs.h"
#include "nvbct.h"
#include "nvddk_fuse.h"
#include "nvcrypto_cipher.h"
#include "nvcrypto_hash.h"
#include "nvboot_warm_boot_0.h"
#include "nvodm_services.h"

#ifdef __cplusplus
extern "C"
{
#endif

//------------------------------------------------------------------------------
// Scratch Register Mapping Macros
//------------------------------------------------------------------------------
// NOTE:    These macros are used to extract fields from registers in
//          various modules and pack them into the PMC scratch regsiters
//          for LP0 exit initialization.
//------------------------------------------------------------------------------

/** NV_SDRF_NUM - define a new scratch register value.

    @param s scratch register name (APBDEV_PMC_s)
    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param n defined value for the field
 */
#define NV_SDRF_NUM(s,d,r,f,n) \
    (((n)& NV_FIELD_MASK(APBDEV_PMC_##s##_0_##d##_##r##_0_##f##_RANGE)) << \
        NV_FIELD_SHIFT(APBDEV_PMC_##s##_0_##d##_##r##_0_##f##_RANGE))


/** NV_FLD_SET_SDRF_NUM - modify a scratch register field.

    @param s scratch register name (APBDEV_PMC_s)
    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param n numeric field value
 */
#define NV_FLD_SET_SDRF_NUM(s,d,r,f,n) \
    ((s & ~NV_FIELD_SHIFTMASK(APBDEV_PMC_##s##_0_##d##_##r##_0_##f##_RANGE)) \
     | NV_SDRF_NUM(s,d,r,f,n))


/** NV_SF_NUM - define a new scratch register value.

    @param s scratch register name (APBDEV_PMC_s)
    @param f register field
    @param n defined value for the field
 */
#define NV_SF_NUM(s,f,n) \
    (((n)& NV_FIELD_MASK(APBDEV_PMC_##s##_0_##f##_RANGE)) << \
        NV_FIELD_SHIFT(APBDEV_PMC_##s##_0_##f##_RANGE))


/** NV_FLD_SET_SR_NUM - modify a scratch register field.

    @param s scratch register name (APBDEV_PMC_s)
    @param f register field
    @param n numeric field value
 */
#define NV_FLD_SET_SF_NUM(s,f,n) \
    ((s & ~NV_FIELD_SHIFTMASK(APBDEV_PMC_##s##_0_##f##_RANGE)) | NV_SF_NUM(s,f,n))


struct NvAbootRec
{
    NvRmDeviceHandle         hRm;
    NvOsMutexHandle          hStorageMutex;
    NvDdkBlockDevMgrDeviceId BlockDevId;
    NvU32                    BlockDevInstance;
    NvBctAuxInfo             OsImage;
    void                    *pIramPhys;
    NvBool                   BlockMgrInit;
};

/**
 * Obtain the physical start page/sector for a given logical partition start
 * page. This function *only* works on logical pages that are the start of
 * partitions. It cannot be used to obtain generic logical->physical mappings.
 * This function is used to pass the physical parameters of partitions on to
 * the kernel. The reason we can't use a simple LogicalToPhysical IOCTL for
 * this is that this IOCTL relies on NV format specific metadata. Once the
 * partition has been formatted to something else, like ext2, we can no longer
 * rely on the information returned by this IOCTL.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 * @param PartitionId Partition to which the start logical address corresponds
 * @param LogicalPage Logical page start of a given partition.
 * @param PhysicalPage This will contain the corresponding physical page.
 * @retval NvSuccess If successful, or the appropriate error code.
 */
NvError
NvAbootPrivGetStartPhysicalPage(
   NvAbootHandle hAboot,
   NvU32         PartitionId,
   NvU64         LogicalPage,
   NvU64         *PhysicalPage);

/**
 * Reads the signature of warmboot RecoveryHeader and stores it in the insecure
 * header
 *
 * @param pAlgo Aboot handle returned by NvAbootOpen().
 * @param pHashSize A pointer to an integer where the hash result size is to be stored
 * @param Source Address where the hash result bytes are to be stored.
 *
 * @retval NvSuccess Indicates hash result size and bytes have been
 *     successfully returned.
 * @retval NvError_InsufficientMemory Indicates the specified buffer size
 *     is too small to hold the hash result data.
 * @retval NvError_InvalidAddress Indicates a pointer address is illegal
 *     (NULL).
 * @retval NvError_InvalidState Indicates the hash result hasn't been
 *     computed yet.
 */
NvError
NvAbootReadSign(
    NvCryptoHashAlgoHandle pAlgo,
    NvU32 *pHashSize,
    NvU32 Source);

/**
 * Clear the signature in the destination code segment.
 *
 * @param pDstHeader Pointer to destination LP0 exit header.
 */
void
NvAbootClearSign(NvBootWb0RecoveryHeader *pDstHeader);

/**
 * Enumerants returned from NvAbootPrivGetChipSku
 */
typedef enum
{
    NvAbootChipSku_Inval = 0,
    NvAbootChipSku_T20,
    NvAbootChipSku_T30,
    NvAbootChipSku_T33,
    NvAbootChipSku_AP33,
    NvAbootChipSku_T37,
    NvAbootChipSku_AP37,
} NvAbootChipSku;

/**
 * Return ChipSku enum for chip.
 *
 * @param hAboot Aboot handle returned by NvAbootOpen().
 * @retval One of the NvAbootChipSku enums (may return
 *       NvAbootChipSku_Inval, if chip is unrecognized).
 */
NvAbootChipSku NvAbootPrivGetChipSku(NvAbootHandle hAboot);
NvDdkBlockDevMgrDeviceId NvAbootGetSecBootDevId(NvU32 * pInstance);
NvError NvAbootChipSpecificConfig(NvAbootHandle hAboot);
NvError NvAbootPrivSaveSdramParams(NvAbootHandle hAboot);
NvError NvAbootPrivSaveFusePrivateKeyDisable(NvAbootHandle hAboot);
NvBool NvAbootConfigChipSpecificMem(NvAbootHandle hAboot, NvOdmOsOsInfo *pOsInfo, NvU32 *pBankSize, NvU32 *pMemSize, NvU32 DramBottom);

void AbootPrivGicDisableInterrupts(NvAbootHandle hAboot);

NvError NvAbootLockSecureScratchRegs(NvAbootHandle hAboot);
void LockVprAperture(NvAbootHandle hAboot);
void LockTsecAperture(NvAbootHandle hAboot);
NvU64 NvAbootPrivGetDramRankSize(NvAbootHandle hAboot,
            NvU64 TotalRamSize, NvU32 RankNo);

#ifdef __cplusplus
}
#endif

#endif
