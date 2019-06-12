/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#include "nvcommon.h"
#include "nvos.h"
#include "nvaboot.h"
#include "nvaboot_private.h"
#include "nvappmain.h"
#include "nvutil.h"
#include "nvrm_hardware_access.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_se_blockdev.h"
#include "nvpartmgr.h"
#include "nvbl_query.h"
#include "nvodm_query.h"
#include "arapbpm.h"
#include "nvboot_warm_boot_0.h"
#include "arclk_rst.h"
#include "nvcrypto_cipher.h"
#include "nvcrypto_hash.h"
#include "nvos.h"
#include "nvstormgr.h"
#include "nvbct.h"
#include "arfuse.h"
#include "nvddk_fuse.h"

#define NV_CHECK_ERROR_FUSE_BYPASS(expr) \
    do                                   \
    {                                    \
        e = (expr);                      \
        if (e != NvSuccess)              \
        {                                \
            goto skip;                   \
        }                                \
    } while (0)

//macro to write fuse values to their respective offset
#define NVFUSE_WRITE32(a,d)     *((volatile NvU32 *)(a)) = (d)
#include "nvddk_fuse.h"

#define ARSE_RSA_MAX_MODULUS_SIZE 2048
#define SE_RSA_KEY_PKT_KEY_SLOT_ONE _MK_ENUM_CONST(0)
//----------------------------------------------------------------------------------------------
// Configuration parameters
//----------------------------------------------------------------------------------------------

#define OBFUSCATE_PLAIN_TEXT        1       // Set nonzero to obfuscate the plain text LP0 exit code

//----------------------------------------------------------------------------------------------
// Definitions
//----------------------------------------------------------------------------------------------

// NOTE: If more than one of the following is enabled, only one of them will
//       actually be used. RANDOM takes precedence over PATTERN, and PATTERN
//       takes precedence over ZERO.
#define RANDOM_AES_BLOCK_IS_RANDOM  0       // Set nonzero to randomize the header RandomAesBlock
#define RANDOM_AES_BLOCK_IS_PATTERN 0       // Set nonzero to patternize the header RandomAesBlock
#define RANDOM_AES_BLOCK_IS_ZERO    1       // Set nonzero to clear the header RandomAesBlock


// Address at which TEGRA LP0 exit code runs
// The address must not overlap the bootrom's IRAM usage
#define TEGRA_LP0_EXIT_RUN_ADDRESS   0x40020000
#define TEGRA_IRAM_BASE              0x40000000

//----------------------------------------------------------------------------------------------
// Global External Labels
//----------------------------------------------------------------------------------------------

// Until this code is placed into the TrustZone hypervisor, it will be sitting
// as plain-text in the non-secure bootloader. Use random meaninless strings
// for the entry points to hide the true purpose of this code to make it a
// little harder to identify from a symbol table entry.

void adSklS9DjzHw6Iuy34J8o7dli4ueHy0jg(void);       // Start of LP0 exit assembly code
void xak81lsdmLKSqkl903zLjWpv1b3TfD78k3(void);      // End of LP0 exit assembly code


//----------------------------------------------------------------------------------------------
// Local Functions
//----------------------------------------------------------------------------------------------

static NvU32 GetPartitionByType(NvU32 PartitionType)
{
    NvU32 PartitionId = 0;
    NvError e=NvSuccess;
    NvPartInfo PartitionInfo;

    while(1)
    {
        PartitionId = NvPartMgrGetNextId(PartitionId);
        if(PartitionId == 0)
            return PartitionId;
        NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionId, &PartitionInfo));
        if(PartitionInfo.PartitionType == PartitionType)
            break;
    }
    return PartitionId;
fail:
    return 0;
}

#if RANDOM_AES_BLOCK_IS_RANDOM

#define NV_CAR_REGR(reg) NV_READ32((0x60006000 + CLK_RST_CONTROLLER_##reg##_0))

static NvU64 AbootPrivQueryRandomSeed(void)
{
    NvU32   Reg;        // Scratch register
    NvU32   LFSR;       // Linear Feedback Shift Register value
    NvU64   seed = 0;   // Result
    NvU32   i;          // Loop index

    for (i = 0;
         i < (sizeof(NvU64)*8)/NV_FIELD_SIZE(CLK_RST_CONTROLLER_PLL_LFSR_0_RND_RANGE);
         ++i)
    {
        // Read the Linear Feedback Shift Register.
        Reg = NV_CAR_REGR(PLL_LFSR);
        LFSR = NV_DRF_VAL(CLK_RST_CONTROLLER, PLL_LFSR, RND, Reg);

        // Shift the seed and or in this part of the value.
        seed <<= NV_FIELD_SIZE(CLK_RST_CONTROLLER_PLL_LFSR_0_RND_RANGE);
        seed |= LFSR;
    }

    return seed;
}
#endif

static void AbootPrivDetermineCryptoOptions(NvBool* IsEncrypted,
                                            NvBool* IsSigned,
                                            NvBool* UseZeroKey)
{
    NvU32 Size;
    NvDdkFuseOperatingMode OpMode;

    NV_ASSERT(IsEncrypted != NULL);
    NV_ASSERT(IsSigned    != NULL);
    NV_ASSERT(UseZeroKey  != NULL);

    Size =  sizeof(NvU32);
    NvDdkFuseGet(NvDdkFuseDataType_OpMode, (void *)&OpMode, (NvU32 *)&Size);

    switch (OpMode)
    {
        case NvDdkFuseOperatingMode_Preproduction:
        case NvDdkFuseOperatingMode_NvProduction:
            *IsEncrypted = NV_FALSE;
            *IsSigned    = NV_TRUE;
            *UseZeroKey  = NV_TRUE;
            break;

        case NvDdkFuseOperatingMode_OdmProductionOpen:
            *IsEncrypted = NV_FALSE;
            *IsSigned    = NV_TRUE;
            *UseZeroKey  = NV_FALSE;
            break;

        case NvDdkFuseOperatingMode_OdmProductionSecure:
            *IsEncrypted = NV_TRUE;
            *IsSigned    = NV_TRUE;
            *UseZeroKey  = NV_FALSE;
            break;

        case NvDdkFuseOperatingMode_Undefined:   // Preproduction or FA
        default:
            *IsEncrypted = NV_FALSE;
            *IsSigned    = NV_FALSE;
            *UseZeroKey  = NV_FALSE;
            break;
    }
}

//----------------------------------------------------------------------------------------------
static NvError AbootPrivEncryptCodeSegment(NvU32 Destination,
                                           NvU32 Source,
                                           NvU32 Length,
                                           NvBool UseZeroKey)
{
    NvError e = NvError_NotInitialized;         // Error code
    NvCryptoCipherAlgoHandle    pAlgo = NULL;   // Crypto algorithm handle
    NvCryptoCipherAlgoParams    Params;         // Crypto algorithm parameters
    const void*                 pSource;        // Pointer to source
    void*                       pDestination;   // Pointer to destination

    // Select crypto algorithm.
    NV_CHECK_ERROR_CLEANUP(
        NvCryptoCipherSelectAlgorithm(
            NvCryptoCipherAlgoType_AesCbc,
            &pAlgo));

    NvOsMemset(&Params, 0, sizeof(Params));

    // Configure crypto algorithm
    if (UseZeroKey)
    {
        Params.AesCbc.KeyType   = NvCryptoCipherAlgoAesKeyType_UserSpecified;
    }
    else
    {
        Params.AesCbc.KeyType   = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
    }
    Params.AesCbc.IsEncrypt     = NV_TRUE;
    Params.AesCbc.KeySize       = NvCryptoCipherAlgoAesKeySize_128Bit;
    Params.AesCbc.PaddingType   = NvCryptoPaddingType_ExplicitBitPaddingOptional;

    NV_CHECK_ERROR_CLEANUP(pAlgo->SetAlgoParams(pAlgo, &Params));

    // Calculate AES block parameters.
    pSource      = (const void*)(Source + offsetof(NvBootWb0RecoveryHeader, RandomAesBlock));
    pDestination = (void*)(Destination  + offsetof(NvBootWb0RecoveryHeader, RandomAesBlock));
    Length      -= offsetof(NvBootWb0RecoveryHeader, RandomAesBlock);

    // Perform encryption.
    e = pAlgo->ProcessBlocks(pAlgo, Length, pSource, pDestination, NV_TRUE, NV_TRUE);

fail:
    pAlgo->ReleaseAlgorithm(pAlgo);
    return e;
}

//----------------------------------------------------------------------------------------------
static NvError AbootPrivSignCodeSegment(NvU32 Source,
                                        NvU32 Length,
                                        NvBool UseZeroKey)
{
    NvError e = NvError_NotInitialized;         // Error code
    NvCryptoHashAlgoHandle      pAlgo = NULL;   // Crypto algorithm handle
    NvCryptoHashAlgoParams      Params;         // Crypto algorithm parameters
    const void*                 pSource;        // Pointer to source
    NvU32   hashSize = sizeof(NvBootHash);      // Size of hash signature

    // Select crypto algorithm.
    NV_CHECK_ERROR_CLEANUP(
        NvCryptoHashSelectAlgorithm(
            NvCryptoHashAlgoType_AesCmac,
            &pAlgo));

    NvOsMemset(&Params, 0, sizeof(Params));

    // Configure crypto algorithm
    if (UseZeroKey)
    {
        Params.AesCmac.KeyType  = NvCryptoCipherAlgoAesKeyType_UserSpecified;
    }
    else
    {
        Params.AesCmac.KeyType  = NvCryptoCipherAlgoAesKeyType_SecureBootKey;
    }
    Params.AesCmac.IsCalculate  = NV_TRUE;
    Params.AesCmac.KeySize      = NvCryptoCipherAlgoAesKeySize_128Bit;
    Params.AesCmac.PaddingType  = NvCryptoPaddingType_ImplicitBitPaddingOptional;
    NV_CHECK_ERROR_CLEANUP(pAlgo->SetAlgoParams(pAlgo, &Params));

    // Calculate AES block parameters.
    pSource = (const void*)(Source + offsetof(NvBootWb0RecoveryHeader, RandomAesBlock));
    Length -= offsetof(NvBootWb0RecoveryHeader, RandomAesBlock);

    // Calculate the signature.
    NV_CHECK_ERROR_CLEANUP(pAlgo->ProcessBlocks(pAlgo, Length, pSource, NV_TRUE, NV_TRUE));

    // Read back the signature and store it in the insecure header.
    e = NvAbootReadSign(pAlgo, &hashSize, Source);

fail:

    pAlgo->ReleaseAlgorithm(pAlgo);
    return e;

}

//----------------------------------------------------------------------------------------------
// Global Functions
//----------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------
NvError NvAbootInitializeCodeSegment(NvAbootHandle hAboot)
{
    // Until this code makes it into the TrustZone hypervisor, return NvError_NotInitialized
    // for everything except success to avoid giving anything away since this will be sitting
    // in a plain-text code segment.
    NvError e = NvError_NotInitialized;         // Error code

    NvU32                       SegStartAddr;   // Segment Start Address
    NvU32                       SegEndAddr;     // Segment end address
    NvU32                       SegLength;      // Segment length
    NvU32                       Start;          // Start of the actual code
    NvU32                       End;            // End of the actual code
    NvU32                       ActualLength;   // Length of the actual code
    NvU32                       Length;         // Length of the signed/encrypted code
    NvRmPhysAddr                SdramBase;      // Start of SDRAM
    NvU32                       SdramSize;      // Size of SDRAM
    NvBootWb0RecoveryHeader*    pSrcHeader;     // Pointer to source LP0 exit header
    NvBootWb0RecoveryHeader*    pDstHeader;     // Pointer to destination LP0 exit header
    NvBool                      IsEncrypted;    // Segment is encrypted
    NvBool                      IsSigned;       // Segment is signed
    NvBool                      UseZeroKey;     // Use key of all zeros
    NvU32                       Partitionid;
    NvU8 *customerFuseData=NULL;
    NvBctHandle BctHandle = NULL;
    NvBctAuxInfo *auxInfo = NULL;
    NvU32 size = 0;
    NvU32 instance = 0;
    NvU32 BytesRead = 0;
    NvU32 BctLength = 0;
    NvFuseBypassInfo FuseBypassInfo;
    NvU32 * pbuffer;
    NvU32 *num_fuse;                        //for storing the no. of valid fusebypass pairs
    NvU8 i;
    char FileName[NVPARTMGR_PARTITION_NAME_LENGTH];
    NvStorMgrFileHandle hFile = NULL;
    NvU64 MemBase;
    NvU64 MemSize;
    NvDdkFuseOperatingMode OpMode;
    // Get the actual code limits.
    Start  = (NvU32)adSklS9DjzHw6Iuy34J8o7dli4ueHy0jg;
    End    = (NvU32)xak81lsdmLKSqkl903zLjWpv1b3TfD78k3;
    ActualLength = End - Start;
    Length = ((ActualLength + 15) >> 4) << 4;

    // Must have a valid Aboot handle and memory handle.
    if (hAboot == NULL)
    {
        // e = NvError_BadParameter;
        goto fail;
    }

    // Get the base address and size of SDRAM.
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&(hAboot->hRm), 0));
    NvRmModuleGetBaseAddress(hAboot->hRm,
                             NVRM_MODULE_ID(NvRmPrivModuleID_ExternalMemory, 0),
                             &SdramBase, &SdramSize);
    NvRmClose(hAboot->hRm);
    SdramSize = NvOdmQueryMemSize(NvOdmMemoryType_Sdram);

    if (NV_TRUE != NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_lp0_vec,
                                                            &MemBase, &MemSize))
        goto fail;
    SegLength    = (NvU32)MemSize;
    SegStartAddr = (NvU32)MemBase;
    SegEndAddr   = SegStartAddr + SegLength - 1;

    // The region specified by SegAddress must be in SDRAM and must be
    // nonzero in length.
    if ((SegLength  == 0)
    ||  (SegStartAddr == 0)
    ||  (SegStartAddr < SdramBase)
    ||  (SegStartAddr >= (SdramBase + SdramSize - 1))
    ||  (SegEndAddr <= SdramBase)
    ||  (SegEndAddr > (SdramBase + SdramSize - 1)))
    {
        // e = NvError_BadParameter;
        goto fail;
    }

    // Things must be 16-byte aligned.
    if ((SegLength & 0xF) || (SegStartAddr & 0xF))
    {
        // e = NvError_BadParameter;
        goto fail;
    }

    // Will the code fit?
    if (SegLength < Length)
    {
        // e = NvError_InsufficientMemory;
        goto fail;
    }

    // Get a pointers to the source and destination region header.
    pSrcHeader = (NvBootWb0RecoveryHeader*)Start;
    pDstHeader = (NvBootWb0RecoveryHeader*)SegStartAddr;

#if HAS_PKC_BOOT_SUPPORT
    //Copy the code directly if it is PKC secure mode, because the header is
    //filled by nvscuretool. The padding is also not required.
    size =  sizeof(NvU32);
    NvDdkFuseGet(NvDdkFuseDataType_OpMode, (void *)&OpMode, (NvU32 *)&size);
    if (OpMode == NvDdkFuseOperatingMode_OdmProductionSecurePKC)
    {
        NvU32 Offset = NV_OFFSETOF(NvBootWb0RecoveryHeader, RandomAesBlock);
        //Verify Warmboot Signature in case PKC secure boot mod
        e = NvDdkSeRsaPssSignatureVerify(SE_RSA_KEY_PKT_KEY_SLOT_ONE,
                                       ARSE_RSA_MAX_EXPONENT_SIZE,
                                       (NvU32*) ((NvU8*)Start + Offset),
                                       NULL, pSrcHeader->LengthSecure - Offset,
                                       pSrcHeader->Signature.RsaPssSig.Signature,
                                       NvDdkSeShaOperatingMode_Sha256,
                                       NULL,
                                       NvDdkSeShaResultSize_Sha256 / 8);
        if (e)
            NvOsDebugPrintf("PKC Warmboot Signing Verification is Failed %x\n",e);
        else
            NvOsDebugPrintf("PKC Warmboot Signing Verification success \n");

        NvOsMemcpy(pDstHeader, pSrcHeader, ActualLength);
        e = NvSuccess;
        goto fail;
    }
#endif
    // Populate the RandomAesBlock as requested.
    #if RANDOM_AES_BLOCK_IS_RANDOM
    {
        NvU64*  pRandomAesBlock = (NvU64*)&(pSrcHeader->RandomAesBlock);
        NvU64*  pEnd            = (NvU64*)(((NvU32)pRandomAesBlock)
                                + sizeof(pSrcHeader->RandomAesBlock));
        do
        {
            *pRandomAesBlock++ = AbootPrivQueryRandomSeed();

        } while (pRandomAesBlock < pEnd);
    }
    #elif RANDOM_AES_BLOCK_IS_PATTERN
    {
        NvU32*  pRandomAesBlock = (NvU32*)&(pSrcHeader->RandomAesBlock);
        NvU32*  pEnd            = (NvU32*)(((NvU32)pRandomAesBlock)
                                + sizeof(pSrcHeader->RandomAesBlock));

        do
        {
            *pRandomAesBlock++ = RANDOM_AES_BLOCK_IS_PATTERN;

        } while (pRandomAesBlock < pEnd);
    }
    #elif RANDOM_AES_BLOCK_IS_ZERO
    {
        NvU32*  pRandomAesBlock = (NvU32*)&(pSrcHeader->RandomAesBlock);
        NvU32*  pEnd            = (NvU32*)(((NvU32)pRandomAesBlock)
                                + sizeof(pSrcHeader->RandomAesBlock));

        do
        {
            *pRandomAesBlock++ = 0;

        } while (pRandomAesBlock < pEnd);
    }
    #endif

    // Populate the header.
    pSrcHeader->LengthInsecure     = Length;
    pSrcHeader->LengthSecure       = Length;
    pSrcHeader->Destination        = TEGRA_LP0_EXIT_RUN_ADDRESS;
    pSrcHeader->EntryPoint         = TEGRA_LP0_EXIT_RUN_ADDRESS;
    pSrcHeader->RecoveryCodeLength = Length - sizeof(NvBootWb0RecoveryHeader);

    size = 0;
    NV_CHECK_ERROR_FUSE_BYPASS(NvBctInit(&BctLength, NULL, &BctHandle));
    NV_CHECK_ERROR_FUSE_BYPASS(
        NvBctGetData(BctHandle, NvBctDataType_AuxDataAligned,
                     &size, &instance, NULL)
    );
    customerFuseData = NvOsAlloc(size);
    NvOsMemset(customerFuseData, 0, size);
    NV_CHECK_ERROR_FUSE_BYPASS(
        NvBctGetData(BctHandle, NvBctDataType_AuxDataAligned,
                     &size, &instance, customerFuseData)
    );
    pbuffer = (NvU32 *)((NvU32) adSklS9DjzHw6Iuy34J8o7dli4ueHy0jg
            + sizeof(NvBootWb0RecoveryHeader) + 4);
    num_fuse = pbuffer;
    auxInfo = (NvBctAuxInfo*)customerFuseData;
    if (auxInfo->StartFuseSector == 0 && auxInfo->NumFuseSector == 0)
    {
        NVFUSE_WRITE32(num_fuse, 0);
        goto skip;
    }
    Partitionid = GetPartitionByType(NvPartMgrPartitionType_FuseBypass);
    if (Partitionid != 0)
    {
        NvBool production_mode_value = 0;
        NvU32  production_mode_set = NV_FALSE;
        NvU32  total_size = sizeof(FuseBypassInfo);
        NvU32  offset = 0;

        NV_CHECK_ERROR_FUSE_BYPASS(NvPartMgrGetNameById(Partitionid, FileName));
        NV_CHECK_ERROR_FUSE_BYPASS(
            NvStorMgrFileOpen(FileName, NVOS_OPEN_READ, &hFile)
        );

        while(total_size)
        {
            NV_CHECK_ERROR_FUSE_BYPASS(
                NvStorMgrFileRead(hFile, ((NvU8 *)(&FuseBypassInfo)) + offset,
                                  total_size, &BytesRead)
            );
            offset += BytesRead;
            total_size -= BytesRead;
        }

        for (i = 0; i < FuseBypassInfo.num_fuses; i++)
        {
            if (FuseBypassInfo.fuses[i].offset == FUSE_PRODUCTION_MODE_0)
            {
                production_mode_value = FuseBypassInfo.fuses[i].fuse_value[0];
                production_mode_set = NV_TRUE;
                continue;
            }
            pbuffer += 1;
            NVFUSE_WRITE32(pbuffer, FuseBypassInfo.fuses[i].offset);
            pbuffer += 1;
            NVFUSE_WRITE32(pbuffer, FuseBypassInfo.fuses[i].fuse_value[0]);
        }
        if (production_mode_set == NV_TRUE)
        {
            pbuffer += 1;
            NVFUSE_WRITE32(pbuffer, FUSE_PRODUCTION_MODE_0);
            pbuffer += 1;
            NVFUSE_WRITE32(pbuffer, production_mode_value);
        }
        NVFUSE_WRITE32(num_fuse, i);
    }

skip:
    if(hFile)
        NvStorMgrFileClose(hFile);

    // Determine crypto options.
    AbootPrivDetermineCryptoOptions(&IsEncrypted, &IsSigned, &UseZeroKey);
    // Need to encrypt?
    if (IsEncrypted)
    {
        // Yes, do it.
        NV_CHECK_ERROR_CLEANUP(
            AbootPrivEncryptCodeSegment(SegStartAddr, Start, Length, UseZeroKey)
        );
        pDstHeader->LengthInsecure = Length;
    }
    else
    {
        // No, just copy the code directly.
        NvOsMemcpy(pDstHeader, pSrcHeader, Length);
    }

    // Clear the signature in the destination code segment.
    NvAbootClearSign(pDstHeader);

    // Need to sign?
    if (IsSigned)
    {
        // Yes, do it.
        NV_CHECK_ERROR_CLEANUP(AbootPrivSignCodeSegment(SegStartAddr, Length, UseZeroKey));
    }

    // Pass the LP0 exit code segment address to the OS.

fail:

    return e;
}

void
NvAbootDestroyPlainLP0ExitCode(void)
{
#if OBFUSCATE_PLAIN_TEXT
    NvU32 Start;        // Start of the actual code
    NvU32 End;          // End of the actual code
    NvU32 ActualLength; // Length of the actual code

    // Get the actual code limits.
    Start  = (NvU32)adSklS9DjzHw6Iuy34J8o7dli4ueHy0jg;
    End    = (NvU32)xak81lsdmLKSqkl903zLjWpv1b3TfD78k3;
    ActualLength = End - Start;

    // Blast the plain-text LP0 exit code in the bootloader image.
    NvOsMemset((void*)Start, 0, ActualLength);

    // Clean up the caches.
    NvOsDataCacheWritebackInvalidate();
    NvOsInstrCacheInvalidate();
#endif
}
