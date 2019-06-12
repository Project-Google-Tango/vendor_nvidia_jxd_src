/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvml_aes.h"
#include "nvml_aes_local.h"
#include "nvrm_drf.h"
#include "t30/aravp_bsea.h"
#include "t30/arvde2x.h"
#include "t30/arahb_arbc.h"
#include "nvboot_ahb_int.h"
#include "nvboot_clocks_int.h"
#include "nvboot_codecov_int.h"
#include "t30/nvboot_config.h"
#include "t30/nvboot_error.h"
#include "nvboot_hardware_access_int.h"
#include "nvboot_reset_int.h"
#include "nvboot_util_int.h"


#define NV_ADDRESS_MAP_BSEA_BASE 0x60011000
#define NV_ADDRESS_MAP_VDE_BASE 0x6001A000
/* The SBK keys are kept in "nonsecure" key slots because only read
 * protection is needed; however, SSK keys are kept in "secure" key
 * slots because both read and write protection is needed.
 *
 * Slots 0 and 1 are "nonsecure" slots, meaning that only read
 *   protection can be provided.
 * Slots 2 and 3 are "secure" slots, meaning that read and/or write
 *   protection can be provided.
 *
 * Check the SBK keys and SSK keys are in the proper slots.
 *
 * Note: if this code is changed, then the permissions lockdown code
 *       at the bottom of this routine must also be reviewed and
 *       updated.
 */
NV_CT_ASSERT( (NVBOOT_SBK_ENCRYPT_SLOT == NVBOOT_SBK_DECRYPT_SLOT) );
NV_CT_ASSERT( (NVBOOT_SBK_ENCRYPT_SLOT >= NvBootAesKeySlot_0) &&
              (NVBOOT_SBK_ENCRYPT_SLOT <= NvBootAesKeySlot_3) );

NV_CT_ASSERT( (NVBOOT_SSK_ENCRYPT_SLOT == NVBOOT_SSK_DECRYPT_SLOT) );
NV_CT_ASSERT( (NVBOOT_SSK_ENCRYPT_SLOT >= NvBootAesKeySlot_4) &&
              (NVBOOT_SSK_ENCRYPT_SLOT <= NvBootAesKeySlot_7) );

//---------------------MACRO DEFINITIONS--------------------------------------
#define AES_REG_WR_OPCODE                  0x7C

//////////////////////////////////////////////////////////////////////////////
//  Defines for accessing AES Hardware registers:
//  This nifty trick relies on several definitions being true:
//  1.  engine==NvBootAesEngine_A for BSEV, engine==NvBootAesEngine_B for BSEA.
//      This is because theenum value is cast to an NvU32 with a subtraction by
//       one to generatea mask.
//  2.  The fields inside each common BSEA & BSEV register are identical --
//      that is, the fields for BSEA_CMDQUE_CONTROL exactly match the fields
//      inside BSEV_CMDQUE_CONTROL.  Therefore, using the DRF macros for
//      either register will work for updates.
//  3.  The offsets for all common registers in the two BSEs are identical,
//      so any register address can be computed as the BSEV address plus
//      the offset to the BSEA address, if the BSEA module is desired.
//
///////////////////////////////////////////////////////////////////////////////
#define BSE_SELECT_ENGINE(engine) \
    ((~((NvU32)(engine)-1))&((NvU32) ((NV_ADDRESS_MAP_BSEA_BASE + AVPBSEA_ICMDQUE_WR_0) - \
            (NV_ADDRESS_MAP_VDE_BASE + ARVDE_BSEV_ICMDQUE_WR_0))))

#define SECURE_REG_ADDR(engine,reg) \
    (ARVDE_BSEV_##reg##_0 + BSE_SELECT_ENGINE(engine))

#define SECURE_HW_REGR(engine,reg,value) \
    (value) = NV_READ32(NV_ADDRESS_MAP_VDE_BASE + SECURE_REG_ADDR(engine,reg));

#define SECURE_HW_REGW(engine,reg,data) \
    NV_WRITE32(NV_ADDRESS_MAP_VDE_BASE + SECURE_REG_ADDR(engine,reg), (data));

#define SECURE_DRF_SET_VAL(engine,reg,field,newData,value) \
    value = NV_FLD_SET_DRF_NUM(ARVDE_BSEV, reg, field, newData, value);

#define SECURE_DRF_READ_VAL(engine,reg,field,regData,value) \
    value = NV_DRF_VAL(ARVDE_BSEV,reg,field,regData);

#define SECURE_DRF_DEF(engine,reg,field,def) \
    NV_DRF_DEF(ARVDE_BSEV, reg, field, def)

#define SECURE_DRF_NUM(engine,reg,field, num) \
        (NV_DRF_NUM(ARVDE_BSEV, reg, field, num))

#define SECURE_HW_KEY_TAB_REGR(engine, reg, index,value) \
{ \
    if (engine == NvBootAesEngine_A) \
        { \
            (value) = NV_READ32(NV_ADDRESS_MAP_VDE_BASE + \
                        (ARVDE_BSEV_##reg##_##0) + index*4); \
        } \
    else if (engine == NvBootAesEngine_B) \
        { \
            (value) = NV_READ32(NV_ADDRESS_MAP_BSEA_BASE + \
                        (AVPBSEA_##reg##_##0) + index*4 ); \
        } \
}
#define SECURE_IRAM_CFG_REGW(engine,reg,data) \
{ \
    if (engine == NvBootAesEngine_A) \
        { \
            NV_WRITE32(NV_ADDRESS_MAP_VDE_BASE + \
                        (ARVDE_BSEV_##reg##_##0), data); \
        } \
    else if (engine == NvBootAesEngine_B) \
        { \
            NV_WRITE32(NV_ADDRESS_MAP_BSEA_BASE + \
                        (AVPBSEA_##reg##_##0), data); \
        } \
}
#define SECURE_IRAM_ACCESS_CFG_INTADDR(engine, addr) \
{ \
    if (engine == NvBootAesEngine_A) \
        { \
            addr = ARVDE_BSEV_IRAM_ACCESS_CFG_0 & 0x00FF; \
        } \
    else if (engine == NvBootAesEngine_B) \
        { \
            addr = AVPBSEA_IRAM_ACCESS_CFG_0 & 0x00FF; \
        } \
}

// -------------------Global Variables Decleration-----------------------------
/**
 * Keytable array, this must be in the IRAM
 * Address must be aligned to 4 bytes for T30 Security Engine
 */
NV_ALIGN(4) static NvU32 s_KeyTable[NVBOOT_AES_KEY_TABLE_LENGTH];

/**
 * Aes context record stucture and its pointer.
 */
static NvBootAesContext s_AesCtxt;
static NvBootAesContext *s_pAesCtxt = NULL;

// -------------------Private Functions Declaration---------------------------


/**
 * Setup the ICQ commands required for the AES encryption/decryption operation.
 */
static void
AesSetupIcqCommands_Int(NvBootAesEngine engine, NvU8 * SrcPhyAddress,
                                 NvU32 BlockCount);

/**
 * Initializes the selected AES HW engine by programming the registers.
 */
static void
AesStartHwEngine_Int(NvBootAesEngine engine, NvU8 *pDestPhyAddress,
                               NvU8 *pSrcPhyAddress, NvBool IsEncryption);

/**
 * Sets the Setup Table command required for the AES engine.
 */
static void
AesSetupTable_Int(NvBootAesEngine engine, NvBootAesKeySlot slot,
                            NvBool updateIVOnly);


// --------------------Public Functions Definitions----------------------------


void
NvMlAesInitializeEngine(void)
{
    NvU32 Engine;

    PROFILE();
    // Check Engine is already initialized or not. If it is already
    // initialized then we will have non NULL pointer.
    if (s_pAesCtxt == NULL)
    {
        // Initialize both AES engines (BSEV and BSEA).
        // Enable clock and then reset the controllers
        // Note: Due to the Bug#353286 VDE needs to be reset
        // when ever BSEV is issued soft reset.

        // Set the clock rate for BSEV.
        // BSEV is supposed to use one of the following clock sources:
        // PLLP_OUT0,PLLC_OUT0, PLLM_OUT0 AND CLK_M
        //Use <=100MHz clock source for BSEV since BSEV is known to
        // work reliably in this frequency range
        NvBootClocksConfigureClock(NvBootClocksClockId_VdeId,
             NVBOOT_CLOCKS_7_1_DIVIDER_BY_2,
             CLK_RST_CONTROLLER_CLK_SOURCE_VDE_0_VDE_CLK_SRC_PLLP_OUT0);

        // Enable clock to the controller
        NvBootClocksSetEnable(NvBootClocksClockId_VdeId, NV_TRUE);
        NvBootClocksSetEnable(NvBootClocksClockId_BsevId, NV_TRUE);
        NvBootClocksSetEnable(NvBootClocksClockId_BseaId, NV_TRUE);
        // Reset the controllers
        NvBootResetSetEnable(NvBootResetDeviceId_VdeId, NV_TRUE);
        NvBootResetSetEnable(NvBootResetDeviceId_BsevId, NV_TRUE);
        NvBootResetSetEnable(NvBootResetDeviceId_BseaId, NV_TRUE);
        NvBootResetSetEnable(NvBootResetDeviceId_VdeId, NV_FALSE);
        NvBootResetSetEnable(NvBootResetDeviceId_BsevId, NV_FALSE);
        NvBootResetSetEnable(NvBootResetDeviceId_BseaId, NV_FALSE);
        for (Engine = 0; Engine< NvBootAesEngine_Num;Engine++)
        {
            s_AesCtxt.isHashingEnabled[Engine] = NV_FALSE;
            s_AesCtxt.UseOriginalIv   [Engine] = NV_FALSE;
        }
        /* Initialize the AES context pointer with AES structure */
        s_pAesCtxt = &s_AesCtxt;
    }
}


void
NvMlAesClearKeyAndIv(
    NvBootAesEngine engine,
    NvBootAesKeySlot slot)
{
    /* Validating the parameters */
    NV_ASSERT(s_pAesCtxt);
    NV_ASSERT(engine < NvBootAesEngine_Num);
    NV_ASSERT(slot < NvBootAesKeySlot_Num);

    NvBootUtilMemset((void *)&s_KeyTable[0], 0,
                        NVBOOT_AES_KEY_TABLE_LENGTH_BYTES);

    // Setup the Key table in the H/W to update
    // the cleared key in H/W registers
    AesSetupTable_Int(engine, slot, NV_FALSE);
}

void
NvMlAesClearIv(
    NvBootAesEngine engine,
    NvBootAesKeySlot slot)
{


    NvBootUtilMemset((void *)&s_KeyTable[0], 0,
                        (NVBOOT_AES_IV_LENGTH * 4));
    NvMlAesStartEngine(
           engine,
           1,
           (NvU8 *)&s_KeyTable[0],
           (NvU8 *)&s_KeyTable[0],
           NV_FALSE);
    while ( NvMlAesIsEngineBusy(engine) )
                ;

}

void
NvMlAesSetKeyAndIv(
    NvBootAesEngine engine,
    NvBootAesKeySlot slot,
    NvBootAesKey *pKey,
    NvBootAesIv *pIv,
    NvBool IsEncryption)
{
    NvU32 RegValue = 0;
    /* Validating the parameters */
    NV_ASSERT(s_pAesCtxt);
    NV_ASSERT(pKey);
    NV_ASSERT(engine < NvBootAesEngine_Num);
    NV_ASSERT(slot < NvBootAesKeySlot_Num);

    NvBootUtilMemcpy(&s_KeyTable[0],
                    &pKey->key[0],
                    sizeof(NvBootAesKey));

    // Copy Initial Vector at the next 8 words of KeyTable
    NvBootUtilMemcpy(&s_KeyTable
                    [NVBOOT_AES_KEY_TABLE_LENGTH - NVBOOT_AES_IV_LENGTH],
                    &pIv->iv[0],
                    sizeof(NvBootAesIv));

    // Enable key schedule generation in hardware - this should be preferably
    // done once only, probably at the time of initialization of the engine
    SECURE_HW_REGR(engine, SECURE_CONFIG_EXT, RegValue);
    // Set 1 to disable the AES engine
    SECURE_DRF_SET_VAL(engine, SECURE_CONFIG_EXT, SECURE_KEY_SCH_DIS, 0, RegValue);
    // update the register value
    SECURE_HW_REGW(engine, SECURE_CONFIG_EXT, RegValue);

    // Setup the Key table to store the key
    // schedule data in the H/W registres
    AesSetupTable_Int(engine, slot, NV_FALSE);

}

void
NvMlAesSetIv(
    NvBootAesEngine engine,
    NvBootAesKeySlot  slot,
    NvBootAesIv *pIv)
{

   NvBootUtilMemcpy((void *)&s_KeyTable[0],
                                    pIv,
                                    (NVBOOT_AES_IV_LENGTH * 4));

    NvMlAesStartEngine(
            engine,
            1,
            (NvU8 *)&s_KeyTable[0],
            (NvU8 *)&s_KeyTable[0],
            NV_FALSE);
      while ( NvMlAesIsEngineBusy(engine) )
                    ;
   NvBootUtilMemcpy((void *)&s_KeyTable[0],
                                    0,
                                    (NVBOOT_AES_IV_LENGTH * 4));
}


void
NvMlAesSelectKeyIvSlot(
    NvBootAesEngine engine,
    NvBootAesKeySlot slot)
{
    NvU32 RegValue = 0;

    /* Validating the parameters */
    NV_ASSERT(s_pAesCtxt);
    NV_ASSERT(engine < NvBootAesEngine_Num);
    NV_ASSERT(slot < NvBootAesKeySlot_Num);

    // Wait till engine becomes IDLE.
    while (NvMlAesIsEngineBusy(engine))
            ;
    // Start encrypting/decrypting with the original IV.
    s_pAesCtxt->UseOriginalIv[engine] = NV_TRUE;
    // Select the KEY slot for updating the IV vectors
    SECURE_HW_REGR(engine, SECURE_CONFIG, RegValue);
    // 5-bit index to select between the 8- keys
    SECURE_DRF_SET_VAL(engine, SECURE_CONFIG, SECURE_KEY_INDEX, slot, RegValue);
    // Update the AES config register
    SECURE_HW_REGW(engine, SECURE_CONFIG, RegValue);
}

void
NvMlAesStartEngine(
    NvBootAesEngine engine,
    NvU32 nblocks,
    NvU8 *src,
    NvU8 *dst,
    NvBool IsEncryption)
{
    /* Validating the parameters */
    NV_ASSERT(s_pAesCtxt);
    NV_ASSERT(engine < NvBootAesEngine_Num);
    NV_ASSERT(src);
    NV_ASSERT(dst);
    // Number of blocks must be non-zero value and less than 0xFFFFF
    NV_ASSERT(!((nblocks == 0) || (nblocks > 0xFFFFF)));

    // Setup ICQ commands
    AesSetupIcqCommands_Int(engine, src, nblocks);

    // Start HW AES engine
    AesStartHwEngine_Int(engine, dst, src, IsEncryption);

    // If the decyrption address is to external memory, make sure the writes to
    // memory are coherent.
    if(NvBootAhbCheckIsExtMemAddr((NvU32 *)dst))
    {
        NvBootAhbWaitCoherency((engine == NvBootAesEngine_A) ? ARAHB_MST_ID_BSEV :
                                                            ARAHB_MST_ID_BSEA);
    }
}

NvBool
NvMlAesIsEngineBusy(NvBootAesEngine engine)
{
    NvBool Status = NV_FALSE;
    NvU32 RegValue = 0;
    NvU32 EngBusy = 0;
    NvU32 IcqEmpty = 0;
    NvU32 DmaBusy = 0;

    /* Validating the parameters */
    NV_ASSERT(s_pAesCtxt);
    NV_ASSERT(engine < NvBootAesEngine_Num);

    // Read the AES Status register
    SECURE_HW_REGR(engine, INTR_STATUS, RegValue);
    // Extract the EngBusy, IcqEmpty and DmaBusy status
    SECURE_DRF_READ_VAL(engine, INTR_STATUS, ENGINE_BUSY, RegValue, EngBusy);
    SECURE_DRF_READ_VAL(engine, INTR_STATUS, ICQ_EMPTY, RegValue, IcqEmpty);
    SECURE_DRF_READ_VAL(engine, INTR_STATUS, DMA_BUSY, RegValue, DmaBusy);

    // Check for engine busy, ICQ not empty and DMA busy
    if ((EngBusy) || (!IcqEmpty) || (DmaBusy))
    {
        Status = NV_TRUE;
    }

    return Status;
}

void
NvMlAesEnableHashingInEngine(
    NvBootAesEngine engine,
    NvBool enbHashing)
{
    NvU32 RegValue = 0;

    SECURE_HW_REGR(engine, SECURE_INPUT_SELECT, RegValue);
    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, SECURE_HASH_ENB,
                   enbHashing, RegValue);
    SECURE_HW_REGW(engine, SECURE_INPUT_SELECT, RegValue);
    s_AesCtxt.isHashingEnabled[engine] = enbHashing;
}

NvBool
NvMlAesIsHashingEnabled(NvBootAesEngine engine)
{
    NV_ASSERT(engine < NvBootAesEngine_Num);

    return s_AesCtxt.isHashingEnabled[engine];
}

void
NvMlAesReadHashOutput(
    NvBootAesEngine engine,
    NvU8 *pHashResult)
{
    // Whether Hashing is enabled in the engine or not is to be bound to the
    // engine. Else we may end up reading HASH_RESULT0-3 for 1 engine
    // when the hashing is enabled for the other engine.
    if (s_AesCtxt.isHashingEnabled[engine])
    {
        NvU32 *pHashRes = (NvU32 *)&pHashResult[0];
        SECURE_HW_REGR(engine, SECURE_HASH_RESULT0, pHashRes[0]);
        SECURE_HW_REGR(engine, SECURE_HASH_RESULT1, pHashRes[1]);
        SECURE_HW_REGR(engine, SECURE_HASH_RESULT2, pHashRes[2]);
        SECURE_HW_REGR(engine, SECURE_HASH_RESULT3, pHashRes[3]);
    }
    else
    {
        NvBootUtilMemset((void *)&pHashResult[0], 0, 16);
    }
}

// ---------------------Private Functions Definitions----------------------------


/**
 * Initializes the selected AES HW engine by programming the registers
 *
 * @param engine For which AES engine to start for encryption/decryption
 * @param pDstPhyAddr destination address for output of AES
 * @param pSrcPhyAddr Source address for input of the AES
 * @param IsEncryption if set NV_TRUE is for encryption else for decryption
 *
 * @return None
 */
static void
AesStartHwEngine_Int(
    NvBootAesEngine engine,
    NvU8 *pDstPhyAddr,
    NvU8 *pSrcPhyAddr,
    NvBool IsEncryption)
{
    NvU32 RegValue = 0;
    NvU32 EngBusy = 0;
    NvU32 IcqEmpty = 0;
    NvU32 i;

    SECURE_IRAM_CFG_REGW(engine, IRAM_ACCESS_CFG, 0x0);
    /* Configuring command Queue control register*/
    // Interactive command queue enable (ICMDQUE_ENB = 1: ICMQ enable)
    RegValue = SECURE_DRF_NUM(engine, CMDQUE_CONTROL, ICMDQUE_ENB, 1) |
               // UCQ command queue enable (UCMDQUE_ENB = 1: UCQ enable)
               SECURE_DRF_NUM(engine, CMDQUE_CONTROL, UCMDQUE_ENB, 1) |
               // Source Stream interface select,
               // (SRC_STM_SEL = 0: through CIF (SDRAM)),
               // and (SRC_STM_SEL = 1: through AHB (SDRAM/IRAM)).
               SECURE_DRF_NUM(engine, CMDQUE_CONTROL, SRC_STM_SEL, 1)|
               // Destination Stream interface select,
               // (DST_STM_SEL = 0: through CIF (SDRAM)),
               // and (DST_STM_SEL = 1: through AHB (SDRAM/IRAM)).
               SECURE_DRF_NUM(engine, CMDQUE_CONTROL, DST_STM_SEL, 1);
    // Update the Command Queue control register.
    SECURE_HW_REGW(engine, CMDQUE_CONTROL, RegValue);


    /* Configure BSE engine for AES stand alone mode*/
    // Decode/Encode mode select (need to setup before DMASetup and
    // BlockStartEngine Command) (BSE_MODE_SEL = 000??: AES standalone mode)
    RegValue = SECURE_DRF_NUM(engine, BSE_CONFIG, BSE_MODE_SEL, 0) |
               // Endian conversion enable for the bit stream.
               // (ENDIAN_ENB = 1: little Endian stream).
               SECURE_DRF_NUM(engine, BSE_CONFIG, ENDIAN_ENB, 1);
    // Update the BSE config register.
    SECURE_HW_REGW(engine, BSE_CONFIG, RegValue);


    /* Configure AES registers */
    // Read the AES congig register
    SECURE_HW_REGR(engine, SECURE_CONFIG_EXT, RegValue);
    // since it is not counter mode set counter offset to '0'
    SECURE_DRF_SET_VAL(engine, SECURE_CONFIG_EXT, SECURE_OFFSET_CNT,
                    0, RegValue);
    // since it is not counter mode set counter incriment to '0'
    SECURE_DRF_SET_VAL(engine, SECURE_CONFIG_EXT, SECURE_CTR_CNTN, 0, RegValue);
    SECURE_HW_REGW(engine, SECURE_CONFIG_EXT, RegValue);

    SECURE_HW_REGR(engine, SECURE_INPUT_SELECT, RegValue);
    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, SECURE_INPUT_ALG_SEL,
                    1, RegValue);
    // Round select must be '0' for 128 bit key based on which the engine
    // shall perform 10 rounds of encryption/decryption
    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, SECURE_KEY_LENGTH,
                    0x80, RegValue);

    /* Select AES operation mode for configuring the AES config register */
    //////////////////////////////////////////////////////////////////////////
    // Configuration for CBC mode of operation:
    // ----------------------------------------------------------------------
    //                  |   Encryption              |       Decryption
    //-----------------------------------------------------------------------
    // AES_XOR_POS      |   10b (top)               |   11b (bottom)
    // AES_INPUT_SEL    |   00b (AHB)               |   00b (AHB)
    // AES_VCTRAM_SEL   |   10b (IV + AES Output)   |   11b (IV_PreAHB)
    // AES_CORE_SEL     |    1b (AES)               |    0b (inverse AES)
    //////////////////////////////////////////////////////////////////////////
    /* AES configuration for Encryption/Decryption */
    // AES XOR position 10b = '2': top, before AES
    // AES XOR position 11b = '3': bottom, after AES
    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, SECURE_RNG_ENB,
                   0, RegValue);
    // Init vector select: 0: original IV for first round and updated IV
    // for the rest of them in BSE internal register
    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, SECURE_IV_SELECT,
                    (s_pAesCtxt->UseOriginalIv[engine] ? 0 : 1), RegValue);

    // Don't return to the original IV until restarting encryption/decryption.
    s_pAesCtxt->UseOriginalIv[engine] = NV_FALSE;

    // AES core selection 1b = '1': Encryption
    // Inverse AES core selection 0b = '0': Decryption
    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, SECURE_CORE_SEL,
                   (IsEncryption ? 1 : 0), RegValue);
    // Vector RAM select 10b = '2': Init Vector for first round and AES
    // output for the rest rounds.
    // Vector RAM select 11b = '3': Init Vector for the first round
    // and previous AHB input for the rest rounds
    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, SECURE_VCTRAM_SEL,
                   (IsEncryption ? 2 : 3), RegValue);
    // AES input select 0?b = '00' or '01': From AHB input vector
    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, SECURE_INPUT_SEL,
                    0, RegValue);

    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, SECURE_XOR_POS,
                   (IsEncryption ? 2 : 3), RegValue);
    // This is set to 1/0 based on whether hashing is to be enabled in the engine.
    // Defaults to 0 being set.
    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, SECURE_HASH_ENB,
                   s_AesCtxt.isHashingEnabled[engine], RegValue);
    // Enable AES decryption On the fly mode when audio/video decode mode
    // (Set 1: enable on the fly mode) (Set "0" when AES standalone mode).
    // Our AES engiine is curenlty configured for Stand alone mode.
    SECURE_DRF_SET_VAL(engine, SECURE_INPUT_SELECT, ON_THE_FLY_ENB,
                    0, RegValue);

    // Update the AES configuration value to the AES register
    SECURE_HW_REGW(engine, SECURE_INPUT_SELECT, RegValue);
    // Update the AES destination physical address
    SECURE_HW_REGW(engine, SECURE_DEST_ADDR, PTR_TO_ADDR(pDstPhyAddr));

    // Issue the AES commands to the ICQ
    for (i = 0; i < s_pAesCtxt->CommandQueueLength; i++)
    {
        // Wait till engine becomes IDLE before issuing any command
        do {
            SECURE_HW_REGR(engine, INTR_STATUS, RegValue);
            SECURE_DRF_READ_VAL(engine, INTR_STATUS, ENGINE_BUSY,
                                RegValue, EngBusy);
            SECURE_DRF_READ_VAL(engine, INTR_STATUS, ICQ_EMPTY,
                                RegValue, IcqEmpty);
        } while ((EngBusy) && ~(IcqEmpty));

        // Write the command to the ICQ register
        SECURE_HW_REGW(engine, ICMDQUE_WR, s_pAesCtxt->CommandQueueData[i]);

    }
}

/**
 * Setup the ICQ commands required for the AES encryption/decryption operation
 *
 * @param engine which AES engine to setup the ICQ commands
 * @param SrcPhyAddress pointer to the source address
 * @param BlockCount number of 128 bit blocks to be processed
 *
 * @return None
 */
static void
AesSetupIcqCommands_Int(
    NvBootAesEngine engine,
    NvU8 * SrcPhyAddress,
    NvU32 BlockCount)
{
    /* Form the ICQ commands into the queue format */

    // Intialize the queue length
    s_pAesCtxt->CommandQueueLength = 0;

    /// Setup DMA command
    s_pAesCtxt->CommandQueueData[s_pAesCtxt->CommandQueueLength++] =
                            (AesUcqOpcode_DmaSetup << AesIcqBitShift_Opcode);
    s_pAesCtxt->CommandQueueData[s_pAesCtxt->CommandQueueLength++] =
                                                  PTR_TO_ADDR(SrcPhyAddress);

    ///Setup Block Start Engine Command
    s_pAesCtxt->CommandQueueData[s_pAesCtxt->CommandQueueLength++] =
            ((AesUcqOpcode_BlockStartEngine << AesIcqBitShift_Opcode) |
             ((BlockCount - 1) << AesIcqBitShift_BlockCount));

    /// Setup DMA finish command
    s_pAesCtxt->CommandQueueData[s_pAesCtxt->CommandQueueLength++] =
                        (AesUcqOpcode_DmaFinish << AesIcqBitShift_Opcode);

}

/**
 * Sets the Setup Table command required for the AES engine
 *
 * @param engine which AES engine to setup the Key table
 * @param slot For which AES Key slot to use for setting up the key table
 *
 * @return None
 */
static void
AesSetupTable_Int(
    NvBootAesEngine engine,
    NvBootAesKeySlot slot,
    NvBool updateIVOnly)
{
    /* IRAM_ACCESS_CFG is at different offsets in the two engines
    * (ref. bug#493656) in T30. Therefore, a separate macro has
    * has been used to write to this register.
    * This register is supposed to be used in conjunction with the
    * SetupTable command only.  SetupTable command has 17 bits
    * for TableAddr, specifying the word-aligned byte offset for the
    * key table base address in IRAM,
    * Word address in IRAM = (BASE_ADR << 2) + (Setuptable_adr[16:2] <<
    * INCR_SHIFT))
    */
    NvU32 SetupTable = 0;
    NvU32 IramAccessCfg = 0;
    NvU32 IramAccessCfgAddrLo = 0;
    NvU32 IramAccessCfgAddrHi = 0;

    /* WAR for bug#677240. */
    SECURE_IRAM_ACCESS_CFG_INTADDR(engine, IramAccessCfgAddrLo);

    IramAccessCfgAddrHi = IramAccessCfgAddrLo + 2;
    SetupTable = ((AesUcqOpcode_SetupTable << AesIcqBitShift_Opcode) |
            (AesUcqCommand_TableSelect << AesIcqBitShift_TableSelect) |
            ((AesUcqCommand_KeyTableSelect | slot) << AesIcqBitShift_KeyTableId) |
            ((PTR_TO_ADDR(s_KeyTable) & 0xF) << AesIcqBitShift_KeyTableAddr));

    IramAccessCfg = (AES_REG_WR_OPCODE << 24) |
                    (IramAccessCfgAddrLo << 16) |
                    ((NvU32)(&s_KeyTable[0]) & 0xFFF0);

    while (NvMlAesIsEngineBusy(engine))
        ;

    // Issue RwgW to internal register IRAM_ACCESS_CFG[15:0]
    SECURE_HW_REGW(engine, ICMDQUE_WR, IramAccessCfg);

    IramAccessCfg = (AES_REG_WR_OPCODE << 24) |
                    (IramAccessCfgAddrHi << 16) |
                    (((NvU32)(&s_KeyTable[0]) & 0x0FFF0000) >> 16) ;

    while (NvMlAesIsEngineBusy(engine))
        ;

    // Issue RegW to internal register IRAM_ACCESS_CFG[31:16]
    SECURE_HW_REGW(engine, ICMDQUE_WR, IramAccessCfg);

    while (NvMlAesIsEngineBusy(engine))
        ;

    // Issue the ICQ Setup table command to finally setup the key table
    SECURE_HW_REGW(engine, ICMDQUE_WR, SetupTable);

    while (NvMlAesIsEngineBusy(engine))
        ;
}

