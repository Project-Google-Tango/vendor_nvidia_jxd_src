/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvddk_operatingmodes.h"
#include "nvddk_fuse.h"
#include "nvrm_hardware_access.h"
#include "nvddk_fuse_common.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvddk_bootdevices.h"
#include "nvddk_fuse_write_priv.h"
#include "nvrm_init.h"
#include "nvrm_pmu.h"
#include "nvodm_query_discovery.h"


#define FLAGS_PROGRAM_ILLEGAL         0x80000000

#define FUSE_PGM_CYCLES_MAX 16

// The following has not appeared in any netlist headers.
#ifndef FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0
#define FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0       0x0
#define FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0_DATA  0:0
#define FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0_WIDTH 1
#define FUSE_ENABLE_FUSE_PROGRAM__RED_ALIAS_0       0x1
#define FUSE_ENABLE_FUSE_PROGRAM__RED_ALIAS_0_DATA  0:0
#define FUSE_ENABLE_FUSE_PROGRAM__RED_ALIAS_0_WIDTH 1
#endif
#ifndef FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0_SHIFT
#define FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0_SHIFT 0
#define FUSE_ENABLE_FUSE_PROGRAM__RED_ALIAS_0_SHIFT 0
#endif

NvDdkFuseData s_FuseData = {{0}};

// Storage for the array of fuse words & mask words.
NvU32 s_FuseArray   [FUSE_WORDS] = { 0 };
NvU32 s_MaskArray   [FUSE_WORDS] = { 0 };
NvU32 s_TempFuseData[FUSE_WORDS];

static NvError MapDataToFuseArrays(void)
{
    NvU32 *Src;
    NvU32  Data;
    NvU32  MaskData;

    // Start by clearing the arrays.
    fusememset(s_FuseArray, 0, sizeof(s_FuseArray));
    fusememset(s_MaskArray, 0, sizeof(s_MaskArray));
    // Set ENABLE_FUSE_PROGRAM bit to 1
    SET_FUSE_BOTH(ENABLE_FUSE_PROGRAM, 0, 1);

    // Reserved Odm fuse
    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_RESERVED_ODM)
    {
        Src = (NvU32*)(s_FuseData.ReservedOdm);
        Data = Src[0];
        SET_FUSE_BOTH(RESERVED_ODM0, 0, Data);
#ifdef FUSE_RESERVED_ODM0__PRI_ALIAS_1
        UPDATE_DATA  (RESERVED_ODM0, 0, Data);
        SET_FUSE_BOTH(RESERVED_ODM0, 1, Data);
#endif

        Data = Src[1];
        SET_FUSE_BOTH(RESERVED_ODM1, 0, Data);
#ifdef FUSE_RESERVED_ODM1__PRI_ALIAS_1
        UPDATE_DATA  (RESERVED_ODM1, 0, Data);
        SET_FUSE_BOTH(RESERVED_ODM1, 1, Data);
#endif

        Data = Src[2];
        SET_FUSE_BOTH(RESERVED_ODM2, 0, Data);
#ifdef FUSE_RESERVED_ODM2__PRI_ALIAS_1
        UPDATE_DATA  (RESERVED_ODM2, 0, Data);
        SET_FUSE_BOTH(RESERVED_ODM2, 1, Data);
#endif

        Data = Src[3];
        SET_FUSE_BOTH(RESERVED_ODM3, 0, Data);
#ifdef FUSE_RESERVED_ODM3__PRI_ALIAS_1
        UPDATE_DATA  (RESERVED_ODM3, 0, Data);
        SET_FUSE_BOTH(RESERVED_ODM3, 1, Data);
#endif

        Data = Src[4];
        SET_FUSE_BOTH(RESERVED_ODM4, 0, Data);
#ifdef FUSE_RESERVED_ODM4__PRI_ALIAS_1
        UPDATE_DATA  (RESERVED_ODM4, 0, Data);
        SET_FUSE_BOTH(RESERVED_ODM4, 1, Data);
#endif

        Data = Src[5];
        SET_FUSE_BOTH(RESERVED_ODM5, 0, Data);
#ifdef FUSE_RESERVED_ODM5__PRI_ALIAS_1
        UPDATE_DATA  (RESERVED_ODM5, 0, Data);
        SET_FUSE_BOTH(RESERVED_ODM5, 1, Data);
#endif

        Data = Src[6];
        SET_FUSE_BOTH(RESERVED_ODM6, 0, Data);
#ifdef FUSE_RESERVED_ODM6__PRI_ALIAS_1
        UPDATE_DATA  (RESERVED_ODM6, 0, Data);
        SET_FUSE_BOTH(RESERVED_ODM6, 1, Data);
#endif

        Data = Src[7];
        SET_FUSE_BOTH(RESERVED_ODM7, 0, Data);
#ifdef FUSE_RESERVED_ODM7__PRI_ALIAS_1
        UPDATE_DATA  (RESERVED_ODM7, 0, Data);
        SET_FUSE_BOTH(RESERVED_ODM7, 1, Data);
#endif
    }

    // If Secure mode is set, we can not burn any other fuses, so return.
    if (NvDdkFuseIsOdmProductionModeFuseSet() &&
        !(s_FuseData.ProgramFlags & FLAGS_PROGRAM_RESERVED_ODM))
    {
        return NvError_SecurityModeBurn;
    }

    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_JTAG_DISABLE)
    {
        Data = (NvU32)(s_FuseData.JtagDisable);
        SET_FUSE_BOTH(ARM_DEBUG_DIS, 0, Data);
    }

    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_ODM_PRODUCTION)
    {
        Data = (NvU32)(s_FuseData.OdmProduction);
        SET_FUSE_BOTH(SECURITY_MODE, 0, Data);
    }

    // SBK & DK
    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_SECURE_BOOT_KEY)
    {
        Src = (NvU32*)(s_FuseData.SecureBootKey);
        Data = Src[0];
        SET_FUSE_BOTH(PRIVATE_KEY0, 0, Data);
        UPDATE_DATA  (PRIVATE_KEY0, 0, Data);
        SET_FUSE_BOTH(PRIVATE_KEY0, 1, Data);

        Data = Src[1];
        SET_FUSE_BOTH(PRIVATE_KEY1, 0, Data);
        UPDATE_DATA  (PRIVATE_KEY1, 0, Data);
        SET_FUSE_BOTH(PRIVATE_KEY1, 1, Data);

        Data = Src[2];
        SET_FUSE_BOTH(PRIVATE_KEY2, 0, Data);
        UPDATE_DATA  (PRIVATE_KEY2, 0, Data);
        SET_FUSE_BOTH(PRIVATE_KEY2, 1, Data);

        Data = Src[3];
        SET_FUSE_BOTH(PRIVATE_KEY3, 0, Data);
        UPDATE_DATA  (PRIVATE_KEY3, 0, Data);
        SET_FUSE_BOTH(PRIVATE_KEY3, 1, Data);
    }

    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_DEVICE_KEY)
    {
        Src = (NvU32*)(s_FuseData.DeviceKey);
        Data = Src[0];
        SET_FUSE_BOTH(PRIVATE_KEY4, 0, Data);
        UPDATE_DATA  (PRIVATE_KEY4, 0, Data);
        SET_FUSE_BOTH(PRIVATE_KEY4, 1, Data);
    }

    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_BOOT_DEV_CONFIG)
    {
        // BootDeviceConfig is 14 bits so mask other bits
        Data = (s_FuseData.SecBootDeviceConfig  & 0x3FFF);
        SET_FUSE_BOTH(BOOT_DEVICE_INFO, 0, Data);
    }

    // Assemble RESERVED_SW
    if ((s_FuseData.ProgramFlags & FLAGS_PROGRAM_BOOT_DEV_SEL       ) ||
        (s_FuseData.ProgramFlags & FLAGS_PROGRAM_SKIP_DEV_SEL_STRAPS) ||
        (s_FuseData.ProgramFlags & FLAGS_PROGRAM_SW_RESERVED        ))
    {
        Data = 0;
        MaskData = 0;

        if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_BOOT_DEV_SEL)
        {
            Data     |= (s_FuseData.SecBootDeviceSelectRaw & 0x7);
            MaskData |= 0x7;
        }

        if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_SKIP_DEV_SEL_STRAPS)
        {
            Data     |= ((s_FuseData.SkipDevSelStraps & 0x1) << 3);
            MaskData |= (0x1 << 3);
        }

        if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_SW_RESERVED)
        {
            Data     |= ((s_FuseData.SwReserved & 0xF) << 4);
            MaskData |= (0xF <<  4);
        }

        SET_FUSE_BOTH_WITH_MASK(RESERVED_SW, 0, Data, MaskData);
    }

    NvDdkFusePrivMapDataToFuseArrays();
    return NvError_Success;
}


// Wait for completion (state machine goes idle).
static void WaitForFuseIdle(void)
{
    NvU32 RegData;

    do
    {
        NvFuseUtilWaitUS(1);
        RegData = FUSE_NV_READ32(FUSE_FUSECTRL_0);
    } while (NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, RegData) !=
             FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);
}

 /*
  * Read a word from the fuses.
  * Note: The fuses must already have been sensed, and
  * the programming power should be off.
  */
static NvU32 ReadFuseWord(NvU32 Addr)
{
    NvU32 RegData;

    WaitForFuseIdle();
    // Prepare the data
    FUSE_NV_WRITE32(FUSE_FUSEADDR_0, Addr);

    // Trigger the read
    RegData = FUSE_NV_READ32(FUSE_FUSECTRL_0);
    RegData = NV_FLD_SET_DRF_DEF(FUSE, FUSECTRL, FUSECTRL_CMD, READ, RegData);
    FUSE_NV_WRITE32(FUSE_FUSECTRL_0, RegData);

    // Wait for completion (state machine goes idle).
    WaitForFuseIdle();

    RegData = FUSE_NV_READ32(FUSE_FUSERDATA_0);

    return RegData;
}

/*
 * Write a word to the fuses.
 * Note: The programming power should be on, and the only non-zero
 * bits should be fuses that have not yet been blown.
 */
static void WriteFuseWord(NvU32 Addr, NvU32 Data)
{
    NvU32 RegData;

    if (Data == 0) return;

    WaitForFuseIdle();
    // Prepare the data
    FUSE_NV_WRITE32(FUSE_FUSEADDR_0,  Addr);
    FUSE_NV_WRITE32(FUSE_FUSEWDATA_0, Data);

    // Trigger the write
    RegData = FUSE_NV_READ32(FUSE_FUSECTRL_0);
    RegData = NV_FLD_SET_DRF_DEF(FUSE, FUSECTRL, FUSECTRL_CMD, WRITE, RegData);
    FUSE_NV_WRITE32(FUSE_FUSECTRL_0, RegData);

    // Wait for completion (state machine goes idle).
    WaitForFuseIdle();
}

// Sense the fuse array & wait until done.
static void SenseFuseArray(void)
{
    NvU32 RegData;

    WaitForFuseIdle();
    RegData = FUSE_NV_READ32(FUSE_FUSECTRL_0);
    RegData = NV_FLD_SET_DRF_DEF(FUSE,
                                 FUSECTRL,
                                 FUSECTRL_CMD,
                                 SENSE_CTRL,
                                 RegData);
    FUSE_NV_WRITE32(FUSE_FUSECTRL_0, RegData);

    // Wait for completion (state machine goes idle).
    WaitForFuseIdle();
}

static void
NvDdkFuseProgramFuseArray(
                                NvU32 *FuseData,
                                NvU32 *MaskData,
                                NvU32  NumWords,
                                NvU32  TProgramCycles)
{
    NvU32  RegData;
    NvU32  i;
    NvU32  Addr = 0;
    NvU32 *Data = NULL;
    NvU32 *Mask = NULL;
    NvU32  FuseWord0; // Needed for initially enabling fuse programming.
    NvU32  FuseWord1; // Needed for initially enabling fuse programming.


    // Make all registers visible first
    SetFuseRegVisibility(1);

    // Read the first two fuse words.
    SenseFuseArray();
    FuseWord0 = ReadFuseWord(FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0);
    FuseWord1 = ReadFuseWord(FUSE_ENABLE_FUSE_PROGRAM__RED_ALIAS_0);

    // also indicate power good to start the regulator and wait (again)
    StartRegulator();

    /*
     *In AP20, the fuse macro is a high density macro, with a completely
    * different interface from the procedure used in AP15.  Fuses are
    * burned using an addressing mechanism, so no need to prepare
    * the full list, but more write to control registers are needed, not
    * a big deal
    * The only bit that can be written at first is bit 0, a special write
    * protection bit by assumptions all other bits are at 0
    * Note that the bit definitions are no more part of arfuse.h
    * (unfortunately). So we define them here
    *
    * Modify the fuse programming time.
    *
    * For AP20, the programming pulse must have a precise width of
    * [9000, 11000] ns.
    */
    if (TProgramCycles > 0)
    {
        RegData = NV_DRF_NUM(FUSE,
                             FUSETIME_PGM2,
                             FUSETIME_PGM2_TWIDTH_PGM,
                             TProgramCycles);
        FUSE_NV_WRITE32(FUSE_FUSETIME_PGM2_0, RegData);
    }

    /* FuseWord0 and FuseWord1 should be left with only the Fuse Enable fuse
     * set to 1, and then only if this fuse has not yet been burned.
     */
    FuseWord0 = (0x1 << FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0_SHIFT) &
      ~FuseWord0;
    FuseWord1 = (0x1 << FUSE_ENABLE_FUSE_PROGRAM__RED_ALIAS_0_SHIFT) &
      ~FuseWord1;

    WriteFuseWord(FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0, FuseWord0);
    WriteFuseWord(FUSE_ENABLE_FUSE_PROGRAM__RED_ALIAS_0, FuseWord1);

    // Remove power good as we may toggle fuse_src in the model, wait
    StopRegulator();

    /*
     * Sense the fuse macro, this will allow programming of other fuses
     * and the reading of the existing fuse values
     */
    SenseFuseArray();

    /*
     * Clear out all bits in FuseData that have already been burned
     * or that have been masked out.
     */

    fusememcpy(s_TempFuseData, FuseData, sizeof(NvU32) * FUSE_WORDS);

    Addr = 0;
    Data = s_TempFuseData;
    Mask = MaskData;

    for (i = 0; i < NumWords; i++, Addr++, Data++, Mask++)
    {
        RegData = ReadFuseWord(Addr);
        *Data = (*Data & ~RegData) & *Mask;
    }

    // Enable power good
    StartRegulator();

    /*
      * Finally loop on all fuses, program the non zero ones
     * Words 0 and 1 are written last and they contain control fuses and we
     * need a sense after writing to a control word (with the exception of
     * the master enable fuse) this is also the reason we write them last
     */
    Addr = 2;
    Data = s_TempFuseData + Addr;
    for (; Addr < NumWords; Addr++, Data++)
    {
        WriteFuseWord(Addr, *Data);
    }

    // write the two control words, we need a sense after each write
    Addr = 0;
    Data = s_TempFuseData;
    for (; Addr < NV_MIN(2, NumWords); Addr++, Data++)
    {
        WriteFuseWord(Addr, *Data);
        WaitForFuseIdle();
        // Remove power good as we may toggle fuse_src in the model, wait
        StopRegulator();
        SenseFuseArray();
        StartRegulator();
    }

    StopRegulator();

    /*
     * Wait until done (polling)
     * this one needs to use fuse_sense done, the FSM follows a periodic
     * sequence that includes idle
     */
    do {
         NvFuseUtilWaitUS(1);
         RegData = FUSE_NV_READ32(FUSE_FUSECTRL_0);
    } while (NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_FUSE_SENSE_DONE, RegData) != 0x1);
}

static NvBool NvDdkFuseIsOdmProductionMode(void)
{
    if (! NvDdkFuseIsFailureAnalysisMode()
         && NvDdkFuseIsOdmProductionModeFuseSet())
    {
        return NV_TRUE;
    }
    return NV_FALSE;
}

static NvError
NvDdkFuseVerifyFuseArray(
                                    NvU32 *FuseData,
                                    NvU32 *MaskData,
                                    NvU32 NumWords)
{
    NvU32        RegData;
    NvU32        i;
    NvU32        Addr = 0;
    NvU32       *Data = NULL;
    NvU32       *Mask = NULL;
    NvError  e   = NvSuccess;

    // Make all registers visible first
    SetFuseRegVisibility(1);

    // Sense the fuse array
    SenseFuseArray();

    // Loop over the data, checking fuse registers.
    Addr = 0;
    Data = FuseData;
    Mask = MaskData;
    for (i = 0; i < NumWords; i++, Addr++, Data++, Mask++)
    {
        RegData = (ReadFuseWord(Addr) & (*Mask));

        /*
          * Once Odm production mode fuse word(0th word) is set, it can
         * not set any other fuse word  in the fuse cell, including the
         * redundent fuse word (1st word).
         * Hence it makes no sense to verify it.
         */
        if ((i == 1) && NvDdkFuseIsOdmProductionMode())
        {
            continue;
        }

        // Check the results.
        if (RegData != ((*Data) & (*Mask)))
        {
            // Abort with an error on a miscompare
            e = NvError_BadValue;
            goto fail;
        }
    }

    // Fallthrough on sucessful completion.

 fail:
    // Hide back registers
    SetFuseRegVisibility(1);
    return e;
}

/*
 *************************************
 * PUBLIC API
 *************************************
 */

/**
 * Clears the cache of fuse data.
 */
void NvDdkFuseClear(void)
{
    fusememset(&s_FuseData, 0, sizeof(NvDdkFuseData));
}

/**
 * Read the current fuse data into the fuse registers.
 */
void NvDdkFuseSense(void)
{
    SetFuseRegVisibility(1);
    SenseFuseArray();
    SetFuseRegVisibility(0);
}

/**
 * Macro's to swap byte order for an NvU32
 */

#define EXTRACT_BYTE_NVU32(Data32, ByteNum) \
    (((Data32) >> ((ByteNum)*8)) & 0xFF)

#define SWAP_BYTES_NVU32(Data32)                    \
    do {                                            \
        NV_ASSERT(sizeof(Data32)==4);               \
        (Data32) =                                  \
            (EXTRACT_BYTE_NVU32(Data32, 0) << 24) | \
            (EXTRACT_BYTE_NVU32(Data32, 1) << 16) | \
            (EXTRACT_BYTE_NVU32(Data32, 2) <<  8) | \
            (EXTRACT_BYTE_NVU32(Data32, 3) <<  0);  \
    } while (0)

/**
 * Schedule fuses to be programmed to the specified values when the next
 * NvDdkFuseProgram() operation is performed
 *
 * By passing a size of zero, the caller is requesting to be told the
 * expected size.
 */
#define FUSE_SET(name, flag)                                              \
    case NvDdkFuseDataType_##name:                                         \
    {                                                                     \
        NvU8 *p = (NvU8 *)&(p_FuseData.name);                           \
        NvU32 i;                                                          \
        /* read existing fuse value */                                    \
        NV_CHECK_ERROR(NvDdkFuseGet(NvDdkFuseDataType_##name, p, &Size));   \
        /* check consistency between existing and desired fuse values. */ \
        /* fuses cannot be unburned, so desired value cannot specify   */ \
        /* any unburned (0x0) bits where the existing value already    */ \
        /* contains burned (0x1) bits.                                 */ \
        if (NvDdkFuseDataType_##name != NvDdkFuseDataType_SecureBootKey && \
            NvDdkFuseDataType_##name != NvDdkFuseDataType_DeviceKey)       \
            for (i=0; i<Size; i++)                                         \
                if ((p[i] | ((NvU8*)pData)[i]) != ((NvU8*)pData)[i])      \
                    NV_CHECK_ERROR(NvError_InvalidState);                  \
        /* consistency check passed; schedule fuses to be burned */         \
        fusememcpy(&(s_FuseData.name), pData, Size);                       \
        s_FuseData.ProgramFlags |= FLAGS_PROGRAM_##flag;                   \
    }                                                                      \
    break


static NvError NvDdkFuseGetFuseTypeSize(NvDdkFuseDataType Type,
                                        NvU32 *pSize, NvU32 ChipId)
{
    if (!pSize)
        return NvError_InvalidAddress;

    switch (Type)
    {
        case NvDdkFuseDataType_DeviceKey:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_JtagDisable:
            *pSize = sizeof(NvBool);
            break;

        case NvDdkFuseDataType_KeyProgrammed:
            *pSize = sizeof(NvBool);
            break;

        case NvDdkFuseDataType_OdmProduction:
            *pSize = sizeof(NvBool);
            break;

        case  NvDdkFuseDataType_SecBootDeviceConfig:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_Sku:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_SwReserved:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_SkipDevSelStraps:
            *pSize = sizeof(NvBool);
            break;

        case NvDdkFuseDataType_PackageInfo:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_SecBootDeviceSelect:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_SecBootDeviceSelectRaw:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_OpMode:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_SecBootDevInst:
            *pSize = sizeof(NvU32);
            break;

        case NvDdkFuseDataType_WatchdogEnabled:
            *pSize = sizeof(NvBool);
            break;

        case NvDdkFuseDataType_UsbChargerDetectionEnabled:
            *pSize = sizeof(NvBool);
            break;

        case NvDdkFuseDataType_PkcDisable:
            *pSize = sizeof(NvBool);
            break;

        case NvDdkFuseDataType_Vp8Enable:
            *pSize = sizeof(NvBool);
            break;

        case NvDdkFuseDataType_OdmLock:
           *pSize = sizeof(NvU32);
            break;

        default:
        {
            return NvDdkFusePrivGetTypeSize(Type, pSize);
         }
    }

         return NvSuccess;
}

NvError NvDdkFuseSet(NvDdkFuseDataType Type, void *pData, NvU32 *pSize)
{
    NvError e = NvError_Success;
    NvU32 Size, RegData, ChipId;
    NvDdkFuseData p_FuseData;

    if (Type == NvDdkFuseDataType_None)
            return NvError_BadValue;
    if (pSize == NULL || pData == NULL) return NvError_BadParameter;

    // Get Chip Id
    RegData = NV_READ32(AR_APB_MISC_BASE_ADDRESS + APB_MISC_GP_HIDREV_0);
    ChipId = NV_DRF_VAL(APB_MISC, GP_HIDREV, CHIPID, RegData);

    // Get min size of the Fuse type
    NV_CHECK_ERROR(NvDdkFuseGetFuseTypeSize(Type, &Size, ChipId));

    // Error checks
    if (*pSize == 0)
    {
        *pSize = Size;
         return NvError_Success;
    }

    if (*pSize < Size) return NvError_InsufficientMemory;
    if (pData == NULL) return NvError_BadParameter;

    /*
     * If Disable Reg program is set, chip can not be fused, so return as
     *  "Access denied".
     */
    if (NvDdkPrivIsDisableRegProgramSet())
        return NvError_AccessDenied;

    // Only reserved odm fuses are allowed to burn in secure mode
    if (NvDdkFuseIsOdmProductionModeFuseSet() &&
                    (Type != NvDdkFuseDataType_ReservedOdm))
    {
        return NvError_BadValue;
    }

    if (NvDdkFuseSetPriv(&pData, Type, &p_FuseData, Size))
    {

        switch (Type)
        {
            FUSE_SET(DeviceKey,                 DEVICE_KEY              );
            FUSE_SET(JtagDisable,                JTAG_DISABLE           );
            FUSE_SET(OdmProduction,          ODM_PRODUCTION     );
            FUSE_SET(SecBootDeviceConfig, BOOT_DEV_CONFIG    );

            case NvDdkFuseDataType_SecBootDeviceSelect:
            case NvDdkFuseDataType_SecBootDeviceSelectRaw:
                {
                    NvU8 *p = (NvU8 *)&(s_FuseData.SecBootDeviceSelectRaw);
                    NvU32 ApiData;  // API symbolic value for device selection
                    NvU32 FuseData; // Raw fuse data for the device selection


                    if (Type == NvDdkFuseDataType_SecBootDeviceSelect)
                    {
                        /* Read out the argument. */
                        fusememcpy(&(ApiData), pData, Size);
                        if (ApiData == 0 ||
                                ApiData >= NvDdkSecBootDeviceType_Max)
                        {
                            return NvError_BadValue;
                        }

                        /* Map the symbolic value to a fuse value. */
                        FuseData = NvDdkToFuseBootDeviceSelMap(ApiData);
                    }
                    else
                    {
                        /*
                        * Type == NvDdkFuseDataType_SecBootDeviceSelectRaw
                        * Read out the argument.
                        */
                        fusememcpy(&(FuseData), pData, Size);
                        if (FuseData >= NvDdkFuseBootDevice_Max)
                        {
                            return NvError_BadValue;
                        }

                        /* Map the fuse value to a symbolic value */
                        ApiData = NvFuseDevToBootDevTypeMap(FuseData);
                    }

                    /* read existing fuse value */
                    NV_CHECK_ERROR(
                        NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceSelectRaw,
                                p, &Size));


                   /*
                    * Check consistency between existing & desired fuse values.
                    * fuses cannot be unburned, so desired value cannot specify
                    * any unburned (0x0) bits where the existing value already
                    * contains burned (0x1) bits.
                    */
                    if ((s_FuseData.SecBootDeviceSelectRaw | FuseData)
                                                            != FuseData)
                        return NvError_InvalidState;

                    /* Consistency check passed; schedule fuses to be burned */
                    s_FuseData.SecBootDeviceSelect    = ApiData;
                    s_FuseData.SecBootDeviceSelectRaw = FuseData;
                    s_FuseData.ProgramFlags |= FLAGS_PROGRAM_BOOT_DEV_SEL;
                }
                break;

            FUSE_SET(SecureBootKey,      SECURE_BOOT_KEY       );
            FUSE_SET(SwReserved,          SW_RESERVED               );
            FUSE_SET(SkipDevSelStraps,  SKIP_DEV_SEL_STRAPS  );
            FUSE_SET(ReservedOdm,        RESERVED_ODM             );

            default:
                return(NvError_BadValue);
        }
    }

    // Set the ReservOdm flag if it present
    if (Type == NvDdkFuseDataType_ReservedOdm)
    {
        s_FuseData.ReservedOdmFlag = NV_TRUE;
    }

    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_ILLEGAL)
        return NvError_InvalidCombination;

    return NvError_Success;
}

void NvDdkFuseClearArrays(void)
{
    NvOsMemset((NvU8*)&s_FuseData, 0,sizeof(s_FuseData));
    NvOsMemset((NvU8*)&s_FuseArray, 0,sizeof(s_FuseArray));
    NvOsMemset((NvU8*)&s_MaskArray, 0,sizeof(s_MaskArray));
    NvOsMemset((NvU8*)&s_TempFuseData, 0,sizeof(s_TempFuseData));
}

/**
 * Program all fuses based on cache data changed via the NvDdkFuseSet() API.
 *
 * Caller is responsible for supplying valid fuse programming voltage prior to
 * invoking this routine.
 *
 * NOTE: All values that are not intended to be programmed must have
 *       value 0. There is no need to conditionalize this code based on which
 *       logical values were actually set through the API.
 */
NvError NvDdkFuseProgram(void)
{
    NvU32 PgmCycles = 0;
    NvU32 RegData;
    NvError e = NvError_Success;

    /*
     * Validate the current state.
     * If Secure mode is already burned, only reserved odm fuses are allowed to burn
     */
    if (NvDdkFuseIsOdmProductionModeFuseSet() && (!s_FuseData.ReservedOdmFlag))
        return NvError_InvalidState;

    /*
     * If Disable Reg program is set, chip can not be fused, so return as
     *  "Access denied".
     */
    if (NvDdkPrivIsDisableRegProgramSet())
        return NvError_AccessDenied;


    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_ILLEGAL)
        return NvError_InvalidState;

    /* enable software writes to the fuse registers */
    RegData = NV_DRF_DEF(FUSE, WRITE_ACCESS_SW, WRITE_ACCESS_SW_CTRL, READWRITE);
    FUSE_NV_WRITE32(FUSE_WRITE_ACCESS_SW_0, RegData);

    /* Enable external programming voltage */
    EnableVppFuse();

    /* Add delay of 1 milli sec.after fuse programming voltage */
    NvFuseUtilWaitUS(1000);

    /* calculate the number of program cycles from the oscillator freq */
    PgmCycles = NvDdkFuseGetFusePgmCycles();

    if (!PgmCycles)
        return NvError_BadValue;

    /* Map data onto a fuse array.*/
    NV_CHECK_ERROR(MapDataToFuseArrays());

    NvDdkFuseProgramFuseArray(s_FuseArray, s_MaskArray, FUSE_WORDS,
                              PgmCycles);

    /* Clear reserved odm flag */
    s_FuseData.ReservedOdmFlag = NV_FALSE;

    /* Disable programming voltage */
    DisableVppFuse();

    /* Add delay of 1 milli sec.after disabling fuse programming voltage */
    NvFuseUtilWaitUS(1000);

    /* disable software writes to the fuse registers */
    RegData = NV_DRF_DEF(FUSE, WRITE_ACCESS_SW, WRITE_ACCESS_SW_CTRL, READONLY);
    FUSE_NV_WRITE32(FUSE_WRITE_ACCESS_SW_0, RegData);

    /* apply the fuse values immediately instead of resetting the chip */
    NvDdkFuseSense();

    /* Read all data into the chip options
       NOTE: For T114 and T148, Programming  FUSE_PRIV2INTFC_START_0 causes a hang.
       Refer Bug 1172524. Hence, users have to use reset/coldboot/LP0 exit for immediate
       reflectance of burnt values via NvDdkFuseRead */
    NvDdkFuseFillChipOptions();

    do {
       NvFuseUtilWaitUS(1000);
       RegData = FUSE_NV_READ32(FUSE_FUSECTRL_0);
       } while (NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_FUSE_SENSE_DONE, RegData) != 0x1
               || NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, RegData) !=
               FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

    return e;
}

/**
 * Verify all fuses scheduled via the NvDdkFuseSet*() API's
 */
NvError NvDdkFuseVerify(void)
{
    NvError e = NvError_Success;

    // Map data onto a fuse array.
    MapDataToFuseArrays();

    // Check to see if the data matches the real fuses.
    e = NvDdkFuseVerifyFuseArray(s_FuseArray, s_MaskArray, FUSE_WORDS);

    if (e != NvError_Success)
        e = NvError_InvalidState;

    return e;
}

/*
 * NvDdkDisableFuseProgram API disables the fuse programming until the next
 * next system reset.
 */
void NvDdkDisableFuseProgram(void)
{
    NvU32 RegData;

    RegData = NV_DRF_DEF(FUSE, DISABLEREGPROGRAM, DISABLEREGPROGRAM_VAL, ENABLE);
    FUSE_NV_WRITE32(FUSE_DISABLEREGPROGRAM_0, RegData);

}
