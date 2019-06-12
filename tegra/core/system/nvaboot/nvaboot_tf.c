/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvcommon.h"
#include "nvos.h"
#include "nverror.h"
#include "nvodm_services.h"
#include "nvaboot_private.h"
#include "nvsystem_utils.h"
#include "nvaboot_tf.h"
#include "arclk_rst.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"

#if defined(CONFIG_TRUSTED_FOUNDATIONS)
#include "t11x/arapbpm.h"
#endif

#ifdef CONFIG_TRUSTED_LITTLE_KERNEL
#define EKS_INDEX_OFFSET 0
#else
#include "tf_include.h"
#include "eks_index_offset.h"
#endif

/* Part of the available Secure RAM is allocated for the Trusted Foundations
 * Software workspace memory.  Note that this range does not include all
 * the available Secure RAM on the platform, since some Secure RAM is
 * required to store the code of the Trusted Foundations.
 */
#define TF_WORKSPACE_OFFSET      0x100000
#define TF_WORKSPACE_SIZE        0x100000
#define TF_WORKSPACE_ATTRIBUTES  0x6
#define TZRAM_BASE_ADDR          0x7C010000

/* Tegra Crypto driver */
#define TF_CRYPTO_DRIVER_UID    "[770CB4F8-2E9B-4C6F-A538-744E462E150C]"

/* Tegra L2 Cache Controller driver*/
#define TF_CORE_DRIVER_UID      "[2C2727B8-0159-45EF-94F0-5D194DE37C17]"
#define TF_L2CC_DRIVER_UID      "[cb632260-32c6-11e0-bc8e-0800200c9a66]"

/* Tegra Trace driver*/
#define TF_TRACE_DRIVER_UID     "[e85b1505-67ac-4d14-8680-4d97c019942d]"

/* Tegra Device ID */
#define TLK_DEVICEID_UID         "[cb632260-0159-4c6f-bc8e-4d97c019942d]"

#define ADD_TF_BOOT_STRING_ARG(sptr, rem, vptr) \
    do {                                             \
        NvU32 idx;                                   \
        idx = NvOsSnprintf(sptr, rem, "%s\n", vptr); \
        rem  -= idx;                                 \
        sptr += idx;                                 \
    } while (0)

#define ADD_TF_BOOT_INT_ARG(sptr, rem, aptr, val) \
    do {                                                              \
        NvU32 idx;                                                    \
        idx = NvOsSnprintf(sptr, rem, aptr ": 0x%x\n", (NvU32)(val)); \
        rem  -= idx;                                                  \
        sptr += idx;                                                  \
    } while (0)

#define ADD_TF_BOOT_MEM_ARG(sptr, rem, size, base) \
    do {                                                   \
        NvU32 idx;                                         \
        idx = NvOsSnprintf(sptr, rem, "mem: %dM@%dM\n",    \
                            (NvU32)(size), (NvU32)(base)); \
        rem  -= idx;                                       \
        sptr += idx;                                       \
    } while (0)

/*
 * Copy TF image (and keys) to what will become secure memory (called
 * from NvAbootPrepareBoot_TF just prior to jumping to the monitor).
 */
void NvAbootCopyTFImageAndKeys(TFSW_PARAMS *pTFSWParams)
{
    extern NvU32 SOSKeyIndexOffset;
#ifdef CONFIG_TRUSTED_LITTLE_KERNEL
    NvU32 Reg = NV_READ32((void*)(pTFSWParams->pClkAddr +
                CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0));
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_TZRAM, ENABLE, Reg);
    NV_WRITE32((void*)(pTFSWParams->pClkAddr +
                CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0),
            Reg);
#endif

    /* Copy TF image to secure memory */
    NvOsMemcpy((void *)((NvU32)pTFSWParams->TFStartAddr),
            (const void *)pTFSWParams->pTFAddr, pTFSWParams->TFSize);

    /* Copy keys buffer to secure memory */
    NvOsMemcpy((void *)pTFSWParams->pTFKeysAddr,
            (const void *)pTFSWParams->pTFKeys, pTFSWParams->pTFKeysSize);

#if defined(CONFIG_TRUSTED_FOUNDATIONS)
    if (SOSKeyIndexOffset == 0) {
        NvUPtr encryptedKeys;
        NvU32 i, *destKeyPtr;

        /* Copy encrypted keys to secure memory */
        encryptedKeys = pTFSWParams->pEncryptedKeys;
        destKeyPtr    = (NvU32 *)((NvU32)pTFSWParams->TFStartAddr + EKS_INDEX_OFFSET);

        for (i = 0; i < pTFSWParams->nNumOfEncryptedKeys; i++)
        {
            NvU32 slotKeyIndex, keyLength;

            keyLength = *((NvU32 *)encryptedKeys);
            encryptedKeys += sizeof(NvU32);

            slotKeyIndex = *destKeyPtr++;
            if (slotKeyIndex)
            {
                NvOsMemcpy((void *)((NvU32)pTFSWParams->TFStartAddr + slotKeyIndex),
                        (const void *)encryptedKeys, keyLength);
            }
            encryptedKeys += keyLength;
        }
    }
#elif defined(CONFIG_TRUSTED_LITTLE_KERNEL)
    NvUPtr encryptedKeys;
    NvU32 i;

    /* Copy encrypted keys to secure memory */
    encryptedKeys = pTFSWParams->pEncryptedKeys;
    NvU32 keyaddrOffset = pTFSWParams->pTFKeysAddr + pTFSWParams->pTFKeysSize;;

    for (i = 0; i < pTFSWParams->nNumOfEncryptedKeys; i++)
    {
        NvU32 keyLength;

        keyLength = *((NvU32 *)encryptedKeys);
        encryptedKeys += sizeof(NvU32);

        NvU32 deadbeaf = 0xdeadbee0;
        deadbeaf = deadbeaf | i;
        NvOsMemcpy((void *)(keyaddrOffset), (const void *)&(deadbeaf), sizeof(NvU32));
        keyaddrOffset += sizeof(NvU32);
        NvOsMemcpy((void *)(keyaddrOffset), (const void *)&(keyLength), sizeof(NvU32));
        keyaddrOffset += sizeof(NvU32);
        if (keyLength) {
            NvOsMemcpy((void *)(keyaddrOffset), (const void *)encryptedKeys, keyLength);
            keyaddrOffset += keyLength;
        }
        encryptedKeys += keyLength;
    }
#endif
}

/* Initialize TF_BOOT_PARAMS with data computed by the bootloader and stored in TFSW_PARAM */
static void NvAbootTFGenerateBootArgs(NvAbootHandle hAboot,
                                      TFSW_PARAMS *pTFSWParams,
                                      NvU32 *pKernelRegisters)
{
    NvU32 Remain = sizeof(pTFSWParams->sTFBootParams.pParamString);
    char *ptr = (char *)pTFSWParams->sTFBootParams.pParamString;
    NvU64 MemBase = 0, MemSize;

    /* Update TF boot args struct with data computed in bootloader */
    pTFSWParams->sTFBootParams.nBootParamsHeader = 'TFBP';

    pTFSWParams->sTFBootParams.nDeviceVersion = pKernelRegisters[1];
    pTFSWParams->sTFBootParams.nNormalOSArg =   pKernelRegisters[2];

    pTFSWParams->sTFBootParams.nWorkspaceAddress =
            ( NvU32)pTFSWParams->TFStartAddr + TF_WORKSPACE_OFFSET;
    pTFSWParams->sTFBootParams.nWorkspaceSize = TF_WORKSPACE_SIZE;
    pTFSWParams->sTFBootParams.nWorkspaceAttributes = TF_WORKSPACE_ATTRIBUTES;

    ADD_TF_BOOT_STRING_ARG(ptr, Remain, "[Global]");

    /* specify DRAM ranges */
    NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_Primary, &MemBase, &MemSize);

#if defined(CONFIG_TRUSTED_FOUNDATIONS)
    /* pass only the primary range (i.e. memory below carveout) */
    ADD_TF_BOOT_INT_ARG(ptr, Remain, "normalOS.RAM.1.address", MemBase);
    ADD_TF_BOOT_INT_ARG(ptr, Remain, "normalOS.RAM.1.size", MemSize);
#elif defined(CONFIG_TRUSTED_LITTLE_KERNEL)
    /* pass the primary range of the form mem: <size>M@<base>M */
    ADD_TF_BOOT_MEM_ARG(ptr, Remain, (MemSize >> 20), (MemBase >> 20));

    /* check for DRAM existing beyond the primary (i.e. memory configs > 2GB) */
    NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_Extended, &MemBase, &MemSize);
    if (MemSize > 0)
    {
        ADD_TF_BOOT_MEM_ARG(ptr, Remain, (MemSize >> 20), (MemBase >> 20));
        MemSize = 0;
    }
#endif

    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "normalOS.ColdBoot.PA", pTFSWParams->ColdBootAddr);

    /* Init Crypto driver section */
    ADD_TF_BOOT_STRING_ARG(ptr, Remain, TF_CRYPTO_DRIVER_UID);
    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "config.s.mem.1.address", pTFSWParams->pTFKeysAddr);
    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "config.s.mem.1.size", pTFSWParams->pTFKeysSize);
#ifdef CONFIG_TRUSTED_LITTLE_KERNEL
    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "config.s.mem.2.address", pTFSWParams->pClkAddr);
    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "config.s.mem.2.size", pTFSWParams->pClkSize);

    NvU32 serialno[4];

    NvRmQueryChipUniqueId(hAboot->hRm, sizeof(serialno), serialno);

    ADD_TF_BOOT_STRING_ARG(ptr, Remain, TLK_DEVICEID_UID);
    ADD_TF_BOOT_INT_ARG(ptr, Remain, "config.uid0", serialno[0]);
    ADD_TF_BOOT_INT_ARG(ptr, Remain, "config.uid1", serialno[1]);
    ADD_TF_BOOT_INT_ARG(ptr, Remain, "config.uid2", serialno[2]);
    ADD_TF_BOOT_INT_ARG(ptr, Remain, "config.uid3", serialno[3]);
#endif

    /* Init Core driver section */
    if (NvSysUtilGetChipId() == 0x35)
        ADD_TF_BOOT_STRING_ARG(ptr, Remain, TF_CORE_DRIVER_UID);
    else
        ADD_TF_BOOT_STRING_ARG(ptr, Remain, TF_L2CC_DRIVER_UID);

    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "config.register.aux.value", 0x7E080001);
    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "config.register.lp.tag_ram_latency", pTFSWParams->L2TagLatencyLP);
    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "config.register.lp.data_ram_latency", pTFSWParams->L2DataLatencyLP);
    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "config.register.g.tag_ram_latency", pTFSWParams->L2TagLatencyG);
    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "config.register.g.data_ram_latency", pTFSWParams->L2DataLatencyG);

    if (NvSysUtilGetChipId() == 0x35)
    {
        /* power reg is only used on T3 */
        ADD_TF_BOOT_INT_ARG(ptr, Remain,
                "config.register.power.value", 0x3);
        /* debug reg is only used on T2 */
        ADD_TF_BOOT_INT_ARG(ptr, Remain,
                "config.register.debug.value", 0x3);
    }
    /* Init Trace driver section */
    ADD_TF_BOOT_STRING_ARG(ptr, Remain, TF_TRACE_DRIVER_UID);
    ADD_TF_BOOT_INT_ARG(ptr, Remain,
            "config.register.uart.id", pTFSWParams->ConsoleUartId);

    pTFSWParams->sTFBootParams.nParamStringSize =
            sizeof(pTFSWParams->sTFBootParams.pParamString) - Remain;
}

static void NvAbootTFGetMemoryParams(NvAbootHandle hAboot,
                                     TFSW_PARAMS *pTFSWParams)
{
    NvU64 MemBase = 0;

    NvAbootMemLayoutBaseSize(hAboot, NvAbootMemLayout_SecureOs, &MemBase, NULL);

    /* image expected to be addressable with 32bit phys addrs */
    NV_ASSERT(MemBase < 0xFFF00000);
    pTFSWParams->TFStartAddr = (NvU32)MemBase;
}

static void NvAbootTFGetL2Latencies(NvAbootHandle hAboot,
                                    TFSW_PARAMS *pTFSWParams)
{
    NvAbootChipSku sku;

    sku = NvAbootPrivGetChipSku(hAboot);
    if (sku == NvAbootChipSku_T20)
    {
        pTFSWParams->L2TagLatencyLP = 0x331;
        pTFSWParams->L2DataLatencyLP = 0x441;

        /* not expecting G values to be used */
        pTFSWParams->L2TagLatencyG = pTFSWParams->L2TagLatencyLP;
        pTFSWParams->L2DataLatencyG = pTFSWParams->L2DataLatencyLP;
        return;
    }

    /* T30 family uses same LP latencies */
    pTFSWParams->L2TagLatencyLP = 0x221;
    pTFSWParams->L2DataLatencyLP = 0x221;

    if (((sku == NvAbootChipSku_T33) || (sku == NvAbootChipSku_AP33)) ||
        ((sku == NvAbootChipSku_T37) || (sku == NvAbootChipSku_AP37)))
    {
        pTFSWParams->L2TagLatencyG = 0x442;
        pTFSWParams->L2DataLatencyG = 0x552;
    }
    else
    {
        /* AP30, T30 */
        pTFSWParams->L2TagLatencyG = 0x441;
        pTFSWParams->L2DataLatencyG = 0x551;
    }
    return;
}

static void NvAbootTFGetUartConsoleId(NvAbootHandle hAboot,
                                      TFSW_PARAMS *pTFSWParams)
{
    NvOdmDebugConsole DbgConsole;

    DbgConsole = NvOdmQueryDebugConsole();
    if (DbgConsole == NvOdmDebugConsole_None)
    {
        pTFSWParams->ConsoleUartId = 0;
#ifdef CONFIG_TRUSTED_LITTLE_KERNEL
    /* For TLK, ConsoleUartId == 0 means no UART */
        return;
#endif
    }
    else if (DbgConsole & NvOdmDebugConsole_Automation)
        pTFSWParams->ConsoleUartId =
                                   (DbgConsole & 0xF) - NvOdmDebugConsole_UartA;
    else
        pTFSWParams->ConsoleUartId = DbgConsole - NvOdmDebugConsole_UartA;

    /* trace driver expects 1-based indexes (not 0-based) */
    pTFSWParams->ConsoleUartId++;
}

static void NvAbootSetTOSImageKeyParams(NvAbootHandle hAboot,
        TFSW_PARAMS *pTFSWParams)
{
    extern void NvAbootLoadTOSFromPartition(TFSW_PARAMS *pTFSWParams);

    pTFSWParams->TFSize = 0;
    pTFSWParams->pTFAddr = 0;

    /*
     * The TrustedOS source image comes either from a TrustedOS partition
     * or from the BL image itself through an included header file.
     */
    NvAbootLoadTOSFromPartition(pTFSWParams);

#if defined(CONFIG_TRUSTED_FOUNDATIONS)
    if (!pTFSWParams->pTFAddr || !pTFSWParams->TFSize) {
        pTFSWParams->pTFAddr = (NvUPtr)trusted_foundations;
        pTFSWParams->TFSize  = trusted_foundations_size;
    }
#endif
    NV_ASSERT(pTFSWParams->pTFAddr && pTFSWParams->TFSize);

#if defined(CONFIG_TRUSTED_LITTLE_KERNEL)
    pTFSWParams->pTFKeysAddr = TZRAM_BASE_ADDR;

    NvRmModuleGetBaseAddress(hAboot->hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            &pTFSWParams->pClkAddr,
            &pTFSWParams->pClkSize);

#elif defined(CONFIG_TRUSTED_FOUNDATIONS)
    pTFSWParams->pTFKeysAddr = (NvU32)pTFSWParams->TFStartAddr +
        (NvOdmQuerySecureRegionSize() - TF_KEYS_BUFFER_SIZE);
#endif
    pTFSWParams->pTFKeysSize = TF_KEYS_BUFFER_SIZE;
}

#if defined(CONFIG_TRUSTED_FOUNDATIONS)
static void NvAbootSaveSecureOsStartAddress(NvAbootHandle hAboot,
                                  NvU32 SecureOsStartAddr)
{
    NvU32 Reg;

    // unlock PMC secure scratch register (22) for writing/reading
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE2_0);
    Reg &= ~APBDEV_PMC_SEC_DISABLE2_0_WRITE22_FIELD;
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE2_0, Reg);

    // write cpu reset vector address
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_SECURE_SCRATCH22_0, SecureOsStartAddr);

    // lock PMC secure scratch register (22) for writing/reading
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE2_0);
    Reg |= APBDEV_PMC_SEC_DISABLE2_0_WRITE22_FIELD;
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE2_0, Reg);
}
#endif

void NvAbootTFPrepareBootParams(NvAbootHandle hAboot,
                                TFSW_PARAMS *pTFSWParams,
                                NvU32 *pKernelRegisters)
{
    extern NvU32 a1dff673f0bb86260b5e7fc18c0460ad859d66dc3;
    extern void *TFSW_Params_BootParams, *TFSW_Params_StartAddr;
    extern NvU32 TFSW_Params_CarveoutSize;
#ifdef CONFIG_TRUSTED_FOUNDATIONS
    NvU32 *pLP0BootParams;
#endif

    NvAbootTFGetMemoryParams(hAboot, pTFSWParams);
    NvAbootTFGetL2Latencies(hAboot, pTFSWParams);
    NvAbootTFGetUartConsoleId(hAboot, pTFSWParams);

    NvAbootSetTOSImageKeyParams(hAboot, pTFSWParams);
    NvAbootTFGenerateBootArgs(hAboot, pTFSWParams, pKernelRegisters);

    /* copy params to locate bootparams, entry point and carveout size */
    TFSW_Params_BootParams   = (void *)&pTFSWParams->sTFBootParams;
    TFSW_Params_StartAddr    = (void *)((NvU32)pTFSWParams->TFStartAddr);
    TFSW_Params_CarveoutSize = NvOdmQuerySecureRegionSize();

#ifdef CONFIG_TRUSTED_FOUNDATIONS
    /* now, patch LP0 warmboot code with params */
    pLP0BootParams = &a1dff673f0bb86260b5e7fc18c0460ad859d66dc3;
    pLP0BootParams[1] = pTFSWParams->sTFBootParams.nBootParamsHeader;
    pLP0BootParams[2] = pTFSWParams->sTFBootParams.nDeviceVersion;
    pLP0BootParams[3] = pTFSWParams->sTFBootParams.nNormalOSArg;
    pLP0BootParams[4] = pTFSWParams->sTFBootParams.nWorkspaceAddress;
    pLP0BootParams[5] = pTFSWParams->sTFBootParams.nWorkspaceSize;
    pLP0BootParams[6] = pTFSWParams->sTFBootParams.nWorkspaceAttributes;

    NvAbootSaveSecureOsStartAddress(hAboot, ( NvU32)pTFSWParams->TFStartAddr + 0x8);
#endif
}
