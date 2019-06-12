/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvos.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "t30/arvde2x_t30.h"
#include "t30/aravp_bsea_t30.h"
#include "t30/arapbpm.h"
#include "nvddk_aes_common.h"
#include "nvddk_aes_priv.h"
#include "nvddk_aes_hw.h"
#include "nvsecureservices.h"
#include "nvassert.h"
#include "nvrm_module.h"
#include "arahb_arbc.h"

//---------------------MACRO DEFINITIONS--------------------------------------

//////////////////////////////////////////////////////////////////////////////
//  Defines for accessing AES Hardware registers:
//  This nifty trick relies on several definitions being true:
//  1.  engine==0 for BSEV, engine==1 for BSEA.  This is because the
//      enum value is cast to an NvU32 with a subtraction by one to generate
//      a mask.
//         engine==0 => mask==0           (~(0-1)) => (~0xffffffff) => 0
//         engine==1 => mask==0xffffffff  (~(1-1)) => (~0) => 0xffffffff
//  2.  The fields inside each common BSEA & BSEV register are identical --
//      that is, the fields for BSEA_CMDQUE_CONTROL exactly match the fields
//      inside BSEV_CMDQUE_CONTROL.  Therefore, using the DRF macros for
//      either register will work for updates.
//  3.  The offsets for all common registers in the two BSEs are identical,
//      so any register address can be computed as the BSEV address plus
//      the offset to the BSEA address, if the BSEA module is desired.
//
//  These macros are sufficient for the Boot ROM's needs; however, general
//  programming of the BSE & VDE modules using these is not possible, due to
//  the unique registers inside each BSE.
///////////////////////////////////////////////////////////////////////////////

#define NV_ADDRESS_MAP_AHB_ARBC_BASE            1610661888

#define SECURE_HW_REGR(engine,reg,value) \
{ \
    if (engine == AesHwEngine_A) \
    { \
        (value) = NV_READ32((NvU32)(NvAesCoreGetEngineAddress(engine)) + \
            (ARVDE_BSEV_##reg##_0)); \
    } \
    else if (engine == AesHwEngine_B) \
    { \
        (value) = NV_READ32((NvU32)(NvAesCoreGetEngineAddress(engine)) + \
            (AVPBSEA_##reg##_0)); \
    } \
}

#define SECURE_HW_REGW(engine,reg,data) \
{ \
    if (engine == AesHwEngine_A) \
    { \
        NV_WRITE32((NvU32)(NvAesCoreGetEngineAddress(engine)) + \
            (ARVDE_BSEV_##reg##_0), (data)); \
    } \
    else if (engine == AesHwEngine_B) \
    { \
        NV_WRITE32((NvU32)(NvAesCoreGetEngineAddress(engine)) + \
            (AVPBSEA_##reg##_0), (data)); \
    } \
}

#define SECURE_DRF_SET_VAL(engine,reg,field,newData,value) \
{ \
    if (engine == AesHwEngine_A) \
    { \
        value = NV_FLD_SET_DRF_NUM(ARVDE_BSEV, reg, field, newData, value); \
    } \
    else if (engine == AesHwEngine_B) \
    { \
        value = NV_FLD_SET_DRF_NUM(AVPBSEA, reg, field, newData, value); \
    } \
}

#define SECURE_DRF_READ_VAL(engine,reg,field,regData,value) \
{ \
    if (engine == AesHwEngine_A) \
    { \
        value = NV_DRF_VAL(ARVDE_BSEV,reg,field,regData); \
    } \
    else if (engine == AesHwEngine_B) \
    { \
        value = NV_DRF_VAL(AVPBSEA,reg,field,regData); \
    } \
}

#define SECURE_DRF_NUM(engine,reg,field, num) \
    NV_DRF_NUM(ARVDE_BSEV, reg, field, num) \

#define SECURE_INDEXED_REGR(engine, index, value) \
{ \
    if (AesHwEngine_A == engine) \
    { \
        (value) = NV_READ32((NvU32)(NvAesCoreGetEngineAddress(engine)) + \
        ARVDE_BSEV_SECURE_SEC_SEL0_0 + ((index) * 4)); \
    } \
    else if (AesHwEngine_B == engine) \
    { \
        (value) = NV_READ32((NvU32)(NvAesCoreGetEngineAddress(engine)) + \
        AVPBSEA_SECURE_SEC_SEL0_0 + ((index) * 4)); \
    } \
}

#define SECURE_INDEXED_REGW(engine, index, value) \
{ \
    if (AesHwEngine_A == engine) \
    { \
        NV_WRITE32((NvU32)(NvAesCoreGetEngineAddress(engine)) + \
            (ARVDE_BSEV_SECURE_SEC_SEL0_0 + ((index) * 4)), (value)); \
    } \
    else if (AesHwEngine_B == engine) \
    { \
        NV_WRITE32((NvU32)(NvAesCoreGetEngineAddress(engine)) + \
            (AVPBSEA_SECURE_SEC_SEL0_0 + ((index) * 4)), (value)); \
    } \
}

#define NV_ADDRESS_MAP_IRAM_A_BASE 0x40000000
#define AES_TEGRA_ALL_REVS (~0ul)

/**
 *  Define Ucq opcodes required for AES operation
 */
typedef enum
{
    // Opcode for Block Start Engine command
    AesUcqOpcode_BlockStartEngine = 0x0E,
    // Opcode for Dma setup command
    AesUcqOpcode_DmaSetup = 0x10,
    // Opcode for Dma Finish Command
    AesUcqOpcode_DmaFinish = 0x11,
    // Opcode for Table setup command
    AesUcqOpcode_SetupTable = 0x15,
    // Opcode for AesSetupTable command
    AesUcqOpcode_AesSetupTable = 0x25,
    AesUcqOpcode_Force32 = 0x7FFFFFFF
} AesUcqOpcode;

/**
 *  Define Aes command values
 */
typedef enum
{

    // Command value for AES key select
    AesUcqCommand_KeySelect = 0,
    // Command value for AES Original Iv select
    AesUcqCommand_OriginalIvSelect = 0x1,
    // Command value for updated Iv select
    AesUcqCommand_UpdatedIvSelect = 0x2,
    // Command value for AES key and Iv select
    AesUcqCommand_KeyAndIVSelect = 0x3,
    // Command value for Keytable selection
    AesUcqCommand_KeyTableSelect = 0x8,
    // Command value for KeySchedule selection
    AesUcqCommand_KeySchedTableSelect = 0x4,
    // Command mask for ketable address mask
    AesUcqCommand_KeyTableAddressMask = 0x1FFFF,
    AesUcqCommand_Force32 = 0x7FFFFFFF
} AesUcqCommand;

/**
 * @brief Define AES Interactive command Queue commands Bit positions
 */
typedef enum
{
    // Define bit postion for command Queue Opcode
    AesIcqBitShift_Opcode = 26,
    // Define bit postion for AES Table select
    AesIcqBitShift_TableSelect = 24,
    // Define bit postion for AES Key Table select
    AesIcqBitShift_KeyTableId = 17,
    // Define bit postion for AES Key Table Address
    AesIcqBitShift_KeyTableAddr = 0,
    // Define bit postion for 128 bit blocks count
    AesIcqBitShift_BlockCount = 0,
    AesIcqBitShift_Force32 = 0x7FFFFFFF
} AesIcqBitShift;

static void
NvSecureAesProcessBuffer(
    AesHwEngine Engine,
    NvU32 SrcPhyAddress,
    NvU32 DstPhyAddrOrKeySlot,
    NvU32 NumBlocks,
    NvBool IsEncryption,
    NvU32 OpMode,
    NvBool IsM2SOperation);

NvBool NvSecureServiceAesDisableEngine(NvU32 Engine)
{
    NvU32 RegValue = 0;

    // Disable the AES engine
    SECURE_HW_REGR(Engine, SECURE_SECURITY, RegValue);
    SECURE_DRF_SET_VAL(Engine, SECURE_SECURITY, SECURE_ENG_DIS, 1, RegValue);
    SECURE_HW_REGW(Engine, SECURE_SECURITY, RegValue);

    SECURE_HW_REGW(Engine, SECURE_SECURITY, 0);
    SECURE_HW_REGR(Engine, SECURE_SECURITY, RegValue);

    // Check the SECURE_CONFIG register value is set to 0x00002000
    if (RegValue == SECURE_DRF_NUM(Engine, SECURE_SECURITY, SECURE_ENG_DIS, 1))
        return  NV_TRUE;
    return NV_FALSE;
}

NvBool NvSecureServiceAesIsEngineDisabled(NvU32 Engine)
{
    NvU32 RegValue = 0;
    NvU32 EngineStatus = 0;

    SECURE_HW_REGR(Engine, SECURE_SECURITY, RegValue);
    SECURE_DRF_READ_VAL(Engine, SECURE_SECURITY, SECURE_ENG_DIS, RegValue, EngineStatus);

    return (NvBool)EngineStatus;
}

NvBool NvSecureServiceAesIsEngineBusy(NvU32 Engine)
{
    NvU32 RegValue = 0;
    NvU32 EngBusy = 0;
    NvU32 IcqEmpty = 0;
    NvU32 DmaBusy = 0;
#if WAIT_ON_AHB_ARBC
    NvU32 AhbArbBusy = 0;
#endif

    SECURE_HW_REGR(Engine, INTR_STATUS, RegValue);

    // Extract the EngBusy, IcqEmpty and DmaBusy status
    SECURE_DRF_READ_VAL(Engine, INTR_STATUS, ENGINE_BUSY, RegValue, EngBusy);
    SECURE_DRF_READ_VAL(Engine, INTR_STATUS, ICQ_EMPTY, RegValue, IcqEmpty);
    SECURE_DRF_READ_VAL(Engine, INTR_STATUS, DMA_BUSY, RegValue, DmaBusy);

#if WAIT_ON_AHB_ARBC
    RegValue = NV_READ32(NV_ADDRESS_MAP_AHB_ARBC_BASE +
                         AHB_ARBITRATION_AHB_MEM_WRQUE_MST_ID_0);
    if (Engine == AesHwEngine_B)
        AhbArbBusy = (1 << ARAHB_MST_ID_BSEA) & RegValue;
    else if (Engine == AesHwEngine_A)
        AhbArbBusy = (1 << ARAHB_MST_ID_BSEV) & RegValue;
    // Check for engine busy, ICQ not empty and DMA busy
    if ((EngBusy) || (!IcqEmpty) || (DmaBusy) || (AhbArbBusy))
#else
    if ((EngBusy) || (!IcqEmpty) || (DmaBusy))
#endif
    {
        // Return TRUE if any of the condition is true
        return NV_TRUE;
    }

    // Return FALSE if engine is not doing anything
    return NV_FALSE;
}

void NvSecureServiceAesWaitTillEngineIdle(NvU32 Engine)
{
    while(NvSecureServiceAesIsEngineBusy(Engine));
}

void NvSecureServiceAesSetupTable(
    NvU32 KeyTablePhyAddr,
    NvU32 Engine,
    NvU32 Slot)
{
    NvU32 SetupTableCmd = ((AesUcqOpcode_SetupTable << AesIcqBitShift_Opcode) |
        (AesUcqCommand_KeyAndIVSelect << AesIcqBitShift_TableSelect) |
        ((AesUcqCommand_KeyTableSelect | Slot) << AesIcqBitShift_KeyTableId) |
        ((KeyTablePhyAddr & AesUcqCommand_KeyTableAddressMask) <<
        AesIcqBitShift_KeyTableAddr));
    NvSecureServiceAesWaitTillEngineIdle(Engine);

    // Issue the ICQ command to update the table to H/W registers
    SECURE_HW_REGW(Engine, ICMDQUE_WR, SetupTableCmd);

    NvSecureServiceAesWaitTillEngineIdle(Engine);
}

void
NvSecureServiceAesSetUpdatedIv(
    NvU32 KeyTablePhyAddr,
    NvU32 Engine,
    NvU32 Slot)
{
    NvU32 SetupTableCmd =
        ((AesUcqOpcode_AesSetupTable << AesIcqBitShift_Opcode) |
        (AesUcqCommand_UpdatedIvSelect << AesIcqBitShift_TableSelect) |
        ((AesUcqCommand_KeyTableSelect | Slot) << AesIcqBitShift_KeyTableId) |
        ((KeyTablePhyAddr & AesUcqCommand_KeyTableAddressMask) <<
        AesIcqBitShift_KeyTableAddr));
    NvSecureServiceAesWaitTillEngineIdle(Engine);

    // Issue the ICQ command to update the table to H/W registers
    SECURE_HW_REGW(Engine, ICMDQUE_WR, SetupTableCmd);

    NvSecureServiceAesWaitTillEngineIdle(Engine);
}

void
NvSecureServiceAesSetOriginalIv(
    NvU32 KeyTablePhyAddr,
    NvU32 Engine,
    NvU32 Slot)
{
    NvU32 SetupTableCmd =
        ((AesUcqOpcode_AesSetupTable << AesIcqBitShift_Opcode) |
        (AesUcqCommand_OriginalIvSelect << AesIcqBitShift_TableSelect) |
        ((AesUcqCommand_KeyTableSelect | Slot) << AesIcqBitShift_KeyTableId) |
        ((KeyTablePhyAddr & AesUcqCommand_KeyTableAddressMask) <<
        AesIcqBitShift_KeyTableAddr));
    NvSecureServiceAesWaitTillEngineIdle(Engine);

    // Issue the ICQ command to update the table to H/W registers
    SECURE_HW_REGW(Engine, ICMDQUE_WR, SetupTableCmd);

    NvSecureServiceAesWaitTillEngineIdle(Engine);
}

void
NvSecureServiceAesSetKey(NvU32 KeyTablePhyAddr, NvU32 Engine, NvU32 Slot)
{
    NvU32 SetupTableCmd =
        ((AesUcqOpcode_AesSetupTable << AesIcqBitShift_Opcode) |
        (AesUcqCommand_KeySelect << AesIcqBitShift_TableSelect) |
        ((AesUcqCommand_KeyTableSelect | Slot) << AesIcqBitShift_KeyTableId) |
        ((KeyTablePhyAddr & AesUcqCommand_KeyTableAddressMask) <<
        AesIcqBitShift_KeyTableAddr));
    NvSecureServiceAesWaitTillEngineIdle(Engine);

    // Issue the ICQ command to update the table to H/W registers
    SECURE_HW_REGW(Engine, ICMDQUE_WR, SetupTableCmd);

    NvSecureServiceAesWaitTillEngineIdle(Engine);
}

void
NvSecureServiceAesSetKeySize(NvU32 Engine, NvU32 KeySize)
{
    NvU32 RegValue = 0;

    SECURE_HW_REGR(Engine, SECURE_INPUT_SELECT, RegValue);
    // Update the key length value interms of bits
    SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_KEY_LENGTH, (KeySize<<3), RegValue);
    SECURE_HW_REGW(Engine, SECURE_INPUT_SELECT, RegValue);
}

void NvSecureServiceAesSelectKeyIvSlot(NvU32 Engine, NvU32 Slot)
{
    NvU32 RegValue = 0;

    // Select the KEY slot for updating the IV vectors
    SECURE_HW_REGR(Engine, SECURE_CONFIG, RegValue);
    // 2-bit index to select between the 4- keys
    SECURE_DRF_SET_VAL(Engine, SECURE_CONFIG, SECURE_KEY_INDEX, Slot, RegValue);
    // Update the AES config register
    SECURE_HW_REGW(Engine, SECURE_CONFIG, RegValue);
}

void NvSecureServiceAesGetIv(NvU32 Engine, NvU32 Slot, NvU32 Start, NvU32 End, NvU32 *pIvAddress)
{
    // The Iv is preserved within the driver because read permission is locked
    // down for dedicated key slots
}

void NvSecureServiceAesControlKeyScheduleGeneration(NvU32 Engine, NvBool Enable)
{
    NvU32 RegValue = 0;

    // Disable key schedule generation in hardware
    SECURE_HW_REGR(Engine, SECURE_CONFIG_EXT, RegValue);
    SECURE_DRF_SET_VAL(Engine, SECURE_CONFIG_EXT, SECURE_KEY_SCH_DIS, (Enable ? 1 : 0), RegValue);
    SECURE_HW_REGW(Engine, SECURE_CONFIG_EXT, RegValue);
}

void NvSecureServiceAesLockSskReadWrites(NvU32 Engine)
{
    NvU32 RegValue = 0;

    SECURE_HW_REGR(Engine, SECURE_SEC_SEL4, RegValue);
    SECURE_DRF_SET_VAL(Engine, SECURE_SEC_SEL4, KEYREAD_ENB4, 0, RegValue);
    SECURE_DRF_SET_VAL(Engine, SECURE_SEC_SEL4, KEYUPDATE_ENB4, 0, RegValue);
    SECURE_HW_REGW(Engine, SECURE_SEC_SEL4, RegValue);
}

void
NvSecureServiceAesProcessBuffer(
    NvU32 Engine,
    NvU32 SrcPhyAddress,
    NvU32 DstPhyAddress,
    NvU32 NumBlocks,
    NvBool IsEncryption,
    NvU32 OpMode)
{
    NvSecureAesProcessBuffer(
        Engine,
        SrcPhyAddress,
        DstPhyAddress,
        NumBlocks,
        IsEncryption,
        OpMode,
        NV_FALSE);
}

/**
 * Encrypt/Decrypt a specified number of blocks of data
 *
 * @param Engine AES engine
 * @param SrcPhyAddress Physical address of source buffer
 * @param DstPhyAddrOrKeySlot Physical address of destination buffer if
 *       IsM2SOperation is NV_FALSE. Destination key slot number if
 *       IsM2SOperation is NV_TRUE
 * @param NumBlocks Number of blocks in Source buffer
 * @param IsEncryption NV_TRUE if encryption else NV_FALSE
 * @param OpMode Operational mode
 * @param IsM2SOperation If NV_TRUE processed buffer is stored in memory
 *       else the decrypted buffer is stored in given key slot.
 */
void
NvSecureAesProcessBuffer(
    AesHwEngine Engine,
    NvU32 SrcPhyAddress,
    NvU32 DstPhyAddrOrKeySlot,
    NvU32 NumBlocks,
    NvBool IsEncryption,
    NvU32 OpMode,
    NvBool IsM2SOperation)
{
    NvU32 CommandQueueData[AES_HW_MAX_ICQ_LENGTH];
    NvU32 CommandQueueLength = 0;
    NvU32 RegValue = 0;
    NvU32 EngBusy = 0;
    NvU32 IcqEmpty = 0;
    NvU32 i;

    // In the current implementation, only ECB mode is supported for
    // M2S operation
    if (IsM2SOperation && (OpMode != NvDdkAesOperationalMode_Ecb))
    {
        return;
    }

    if (IsM2SOperation && IsEncryption)
    {
        return;
    }

    // Setup DMA command
    CommandQueueData[CommandQueueLength++] = (AesUcqOpcode_DmaSetup << AesIcqBitShift_Opcode);
    CommandQueueData[CommandQueueLength++] = SrcPhyAddress;

    // Setup Block Start Engine Command
    CommandQueueData[CommandQueueLength++] =
        ((AesUcqOpcode_BlockStartEngine << AesIcqBitShift_Opcode) |
        ((NumBlocks - 1) << AesIcqBitShift_BlockCount));

    // Setup DMA finish command
    CommandQueueData[CommandQueueLength++] = (AesUcqOpcode_DmaFinish << AesIcqBitShift_Opcode);

    // Wait for engine to become idle
    NvSecureServiceAesWaitTillEngineIdle(Engine);

    // Configure command Queue control register
    SECURE_HW_REGR(Engine, CMDQUE_CONTROL, RegValue);

    if (Engine == AesHwEngine_A)
    {
        RegValue |=
            // Source Stream interface select,
            // (SRC_STM_SEL = 0: through CIF (SDRAM)),
            // and (SRC_STM_SEL = 1: through AHB (SDRAM/IRAM)).
            SECURE_DRF_NUM(Engine, CMDQUE_CONTROL, SRC_STM_SEL, (SrcPhyAddress & NV_ADDRESS_MAP_IRAM_A_BASE) ? 1 : 0) |
            // Destination Stream interface select,
            // (DST_STM_SEL = 0: through CIF (SDRAM)),
            // and (DST_STM_SEL = 1: through AHB (SDRAM/IRAM)).
            SECURE_DRF_NUM(Engine, CMDQUE_CONTROL, DST_STM_SEL, (DstPhyAddrOrKeySlot & NV_ADDRESS_MAP_IRAM_A_BASE) ? 1 : 0);
    }
    else
    {
        RegValue |=
            // Source Stream interface select,
            // (SRC_STM_SEL = 1: through AHB (SDRAM/IRAM)).
            SECURE_DRF_NUM(Engine, CMDQUE_CONTROL, SRC_STM_SEL, 1) |
            // Destination Stream interface select,
            // (DST_STM_SEL = 1: through AHB (SDRAM/IRAM)).
            SECURE_DRF_NUM(Engine, CMDQUE_CONTROL, DST_STM_SEL, 1);
    }

    // Update the Command Queue control register.
    SECURE_HW_REGW(Engine, CMDQUE_CONTROL, RegValue);

    /* Configure BSE engine for AES stand alone mode*/
    // Decode/Encode mode select (need to setup before DMASetup and
    // BlockStartEngine Command) (BSE_MODE_SEL = 000??: AES standalone mode)
    RegValue = SECURE_DRF_NUM(Engine, BSE_CONFIG, BSE_MODE_SEL, 0) |
        // Endian conversion enable for the bit stream.
        // (ENDIAN_ENB = 1: little Endian stream).
        SECURE_DRF_NUM(Engine, BSE_CONFIG, ENDIAN_ENB, 1);

    // Update the BSE config register
    SECURE_HW_REGW(Engine, BSE_CONFIG, RegValue);

    // Read the AES config register
    SECURE_HW_REGR(Engine, SECURE_CONFIG_EXT, RegValue);

    // Set counter offset to '0'
    SECURE_DRF_SET_VAL(Engine, SECURE_CONFIG_EXT, SECURE_OFFSET_CNT, 0, RegValue);
    SECURE_HW_REGW(Engine, SECURE_CONFIG_EXT, RegValue);

    if (IsM2SOperation) {
        SECURE_HW_REGR(Engine, SECURE_CONFIG, RegValue);
        SECURE_DRF_SET_VAL(Engine, SECURE_CONFIG, SECURE_MTM2KEY_INDEX, DstPhyAddrOrKeySlot, RegValue);
        SECURE_HW_REGW(Engine, SECURE_CONFIG, RegValue);
    }
    else
        SECURE_HW_REGW(Engine, SECURE_DEST_ADDR, DstPhyAddrOrKeySlot);

    // Configure the AES extension register for KEY and IV select
    SECURE_HW_REGR(Engine, SECURE_INPUT_SELECT, RegValue);
    SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_IV_SELECT, 1, RegValue);
    SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_MTM2KEY_ENB, (IsM2SOperation ? 1 : 0), RegValue);

    /* Select AES operation mode for configuring the AES config register */
    //////////////////////////////////////////////////////////////////////////
    switch (OpMode)
    {
        case NvDdkAesOperationalMode_Cbc:
            // Configuration for CBC mode of operation:
            // ----------------------------------------------------------------------
            //                  |   Encryption              |       Decryption
            //-----------------------------------------------------------------------
            // SECURE_XOR_POS      |   10b (top)               |   11b (bottom)
            // SECURE_INPUT_SEL    |   00b (AHB)               |   00b (AHB)
            // SECURE_VCTRAM_SEL   |   10b (IV + AES Output)   |   11b (IV_PreAHB)
            // SECURE_CORE_SEL     |    1b (AES)               |    0b (inverse AES)
            //////////////////////////////////////////////////////////////////////////
            /* AES configuration for Encryption/Decryption */
            // AES XOR position 10b = '2': top, before AES
            // AES XOR position 11b = '3': bottom, after AES
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_XOR_POS, (IsEncryption ? 2 : 3), RegValue);
            // AES input select 0?b = '00' or '01': From AHB input vector
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_INPUT_SEL, 0, RegValue);
            // Vector RAM select 10b = '2': Init Vector for first round and AES
            // output for the rest rounds.
            // Vector RAM select 11b = '3': Init Vector for the first round
            // and previous AHB input for the rest rounds
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_VCTRAM_SEL, (IsEncryption ? 2 : 3), RegValue);
            // AES core selection 1b = '1': Encryption
            // Inverse AES core selection 0b = '0': Decryption
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_CORE_SEL, (IsEncryption ? 1 : 0), RegValue);
            // Disable the random number generator
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_RNG_ENB, 0, RegValue);
            break;
        case NvDdkAesOperationalMode_Ecb:
            // Configuration for ECB mode of operation:
            // ----------------------------------------------------------------------
            //                  |   Encryption              |       Decryption
            //-----------------------------------------------------------------------
            // SECURE_XOR_POS      |   0X (bypass)               |   0X (bypass)
            // SECURE_INPUT_SEL    |   00b (AHB)               |   00b (AHB)
            // SECURE_VCTRAM_SEL   |   XX(don’t care)   |   XX(don’t care)
            // SECURE_CORE_SEL     |    1b (AES)               |    0b (inverse AES)
            //////////////////////////////////////////////////////////////////////////
            /* AES configuration for Encryption/Decryption */
            // For ECB mode, XOR position shoud be bypassed
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_XOR_POS, 0, RegValue);
            // AES input select is zero
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_INPUT_SEL, 0, RegValue);
            // AES core selection 1b = '1': Encryption
            // Inverse AES core selection 0b = '0': Decryption
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_CORE_SEL, (IsEncryption ? 1 : 0), RegValue);
            // Disable the random number generator
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_RNG_ENB, 0, RegValue);
            break;
        case NvDdkAesOperationalMode_AnsiX931:
            // For RNG mode, XOR position shoud be bypassed
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_XOR_POS, 0, RegValue);
            // AES input select is zero
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_INPUT_SEL, 0, RegValue);
            // AES core selection 1b = '1': Encryption
            // Inverse AES core selection 0b = '0': Decryption
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_CORE_SEL, (IsEncryption ? 1 : 0), RegValue);
            // To generate a random number, enable the random number generator
            SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_RNG_ENB, 1, RegValue);
            break;
        default:
            return;
    }

    SECURE_DRF_SET_VAL(Engine, SECURE_INPUT_SELECT, SECURE_HASH_ENB, 0, RegValue);
    SECURE_HW_REGW(Engine, SECURE_INPUT_SELECT, RegValue);

    // Issue the AES commands to the ICQ
    for (i = 0; i < CommandQueueLength; i++)
    {
        // Wait till engine becomes IDLE before issuing any command
        do
        {
            SECURE_HW_REGR(Engine, INTR_STATUS, RegValue);
            SECURE_DRF_READ_VAL(Engine, INTR_STATUS, ENGINE_BUSY, RegValue, EngBusy);
            SECURE_DRF_READ_VAL(Engine, INTR_STATUS, ICQ_EMPTY, RegValue, IcqEmpty);
        } while ((EngBusy) && ~(IcqEmpty));
        // Write the command to the ICQ register
        SECURE_HW_REGW(Engine, ICMDQUE_WR, CommandQueueData[i]);
    }

    // Wait for engine to become idle
    NvSecureServiceAesWaitTillEngineIdle(Engine);
}

void NvSecureServiceAesLoadSskToSecureScratchAndLock(NvU32 PmicBaseAddr, NvU32 *pKey, size_t Size)
{
    NvU32 *pSecureScratch = NULL;
    NvU32 *pPmicBaseAddr = NULL;

    NvRmPhysicalMemMap(
        PmicBaseAddr,
        Size,
        NVOS_MEM_READ_WRITE,
        NvOsMemAttribute_Uncached,
        (void **)&pPmicBaseAddr);

    // Get the secure scratch base address
    pSecureScratch = pPmicBaseAddr + (APBDEV_PMC_SECURE_SCRATCH0_0 / 4);

    // If Key is supplied then load the key into the scratch registers
    if (pKey)
    {
        NvU32 i;

        // Load the SSK into the secure scratch registers
        for (i = 0; i < (NvDdkAesKeySize_128Bit/4); i++)
        {
            NV_WRITE32(pSecureScratch, pKey[i]);
            pSecureScratch++;
        }
    }

    // Disable write access to the secure scratch register
    NV_WRITE32((pPmicBaseAddr) + (APBDEV_PMC_SEC_DISABLE_0 / 4), (NV_DRF_DEF(APBDEV_PMC, SEC_DISABLE, WRITE, ON)));

    // UnMap the virtual address
    NvOsPhysicalMemUnmap(pPmicBaseAddr, Size);
}

void NvSecureServiceAesKeyReadDisable(NvU32 Engine, NvU32 Slot)
{
    NvU32 RegValue = 0;

    SECURE_INDEXED_REGR(Engine, Slot, RegValue);
    RegValue = NV_FLD_SET_DRF_NUM(ARVDE_BSEV, SECURE_SEC_SEL0, KEYREAD_ENB0, 0, RegValue);
    SECURE_INDEXED_REGW(Engine, Slot, RegValue);
}
void NvSecureServiceAesDecryptToSlot(
    AesHwEngine Engine,
    NvU32 SrcPhyAddress,
    NvU32 DestKeySlot,
    NvU32 NumBlocks)
{
    NvSecureAesProcessBuffer(
        Engine,
        SrcPhyAddress,
        DestKeySlot,
        NumBlocks,
        NV_FALSE,
        NvDdkAesOperationalMode_Ecb,
        NV_TRUE);
}
