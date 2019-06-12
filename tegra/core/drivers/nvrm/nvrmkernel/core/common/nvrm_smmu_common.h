/*
 * Copyright (c) 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 * This file is included by t*rm_smmu_client.c for implementing common
 * per-chip low level functions. Source code is common, but each chip
 * may have its own #define's found in hwinc and other include files.
 */

static struct SMMUChipOps *s_ops;
static void initSMMUClients(NvRmDeviceHandle hDevice, NvU32 asid);

static void
flushSMMURegs(NvRmDeviceHandle hDevice, NvBool enableSMMU)
{
    NV_REGW(hDevice, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0),
        MC_SMMU_TLB_FLUSH_0,
        NV_DRF_DEF(MC, SMMU_TLB_FLUSH, TLB_FLUSH_VA_MATCH, ALL) |
            NV_DRF_DEF(MC, SMMU_TLB_FLUSH, TLB_FLUSH_ASID_MATCH, DISABLE));
    NV_REGW(hDevice, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0),
        MC_SMMU_PTC_FLUSH_0,
        NV_DRF_DEF(MC, SMMU_PTC_FLUSH, PTC_FLUSH_TYPE, ALL));

    if (enableSMMU && s_ops->useGART)
        NV_REGW(hDevice, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0),
            MC_SMMU_CONFIG_0,
            NV_DRF_DEF(MC, SMMU_CONFIG, SMMU_ENABLE, ENABLE));
    // do a read to flush out the writes
    (void)NV_REGR(hDevice, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0),
        MC_SMMU_CONFIG_0);
}

static void
setupSMMURegs(NvRmDeviceHandle hDevice, NvU32 asid, NvBool hit_miss)
{
    initSMMUClients(hDevice, asid);

    if (hit_miss)
    {
        NV_REGW(hDevice, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0),
            MC_SMMU_TLB_CONFIG_0,
            NV_DRF_DEF(MC, SMMU_TLB_CONFIG, TLB_STATS_ENABLE, ENABLE) |
            NV_DRF_DEF(MC, SMMU_TLB_CONFIG, TLB_HIT_UNDER_MISS, ENABLE) |
            NV_DRF_DEF(MC, SMMU_TLB_CONFIG, TLB_ACTIVE_LINES, DEFAULT));

        NV_REGW(hDevice, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0),
            MC_SMMU_PTC_CONFIG_0,
            NV_DRF_DEF(MC, SMMU_PTC_CONFIG, PTC_STATS_ENABLE, ENABLE) |
            NV_DRF_DEF(MC, SMMU_PTC_CONFIG, PTC_CACHE_ENABLE, ENABLE) |
            NV_DRF_DEF(MC, SMMU_PTC_CONFIG, PTC_INDEX_MAP, DEFAULT));
    }

    flushSMMURegs(hDevice, 1);
}

static void
setPDEbase(NvRmDeviceHandle hDevice, NvU32 *PDEbase, NvU32 asid)
{
    NV_REGW(hDevice, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0),
        MC_SMMU_PTB_ASID_0,
        NV_DRF_NUM(MC, SMMU_PTB_ASID, CURRENT_ASID, asid));
    NV_REGW(hDevice, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0),
        MC_SMMU_PTB_DATA_0,((NvU32)PDEbase >> SMMU_PAGE_SHIFT)|SMMU_PDE_ATTR);
}

static void initLocal(struct SMMUChipOps *);

// Define in 2 stages for complete argument expansion
#define PER_CHIP_INIT_CALLBACKtmp(name)  NvRmPriv##name##InitSMMUCallBack
#define PER_CHIP_INIT_CALLBACK(name)     PER_CHIP_INIT_CALLBACKtmp(name)
void PER_CHIP_INIT_CALLBACK(NVRM_SMMU_CHIP_NAME)(struct SMMUChipOps *ops, NvBool useGART);
void PER_CHIP_INIT_CALLBACK(NVRM_SMMU_CHIP_NAME)(struct SMMUChipOps *ops, NvBool useGART)
{
    ops->setupSMMURegs = setupSMMURegs;
    ops->flushSMMURegs = flushSMMURegs;
    ops->setPDEbase = setPDEbase;
    ops->smmu_pde_readable_field = SMMU_PDE_READABLE_FIELD;
    ops->smmu_pde_writable_field = SMMU_PDE_WRITABLE_FIELD;
    ops->smmu_pde_nonsecure_field = SMMU_PDE_NONSECURE_FIELD;
    ops->smmu_pde_next_field = SMMU_PDE_NEXT_FIELD;
    ops->smmu_pde_pa_shift = SMMU_PDE_PA_SHIFT;

    ops->smmu_pte_readable_field = SMMU_PTE_READABLE_FIELD;
    ops->smmu_pte_writable_field = SMMU_PTE_WRITABLE_FIELD;
    ops->smmu_pte_nonsecure_field = SMMU_PTE_NONSECURE_FIELD;
    ops->smmu_pte_pa_shift = SMMU_PTE_PA_SHIFT;

    ops->smmu_pdir_shift = SMMU_PDIR_SHIFT;
    ops->smmu_ptbl_shift = SMMU_PTBL_SHIFT;
    ops->smmu_page_shift = SMMU_PAGE_SHIFT;
    ops->smmu_pde_width  = SMMU_PDE_WIDTH;
    ops->smmu_pte_width  = SMMU_PTE_WIDTH;

    ops->useGART = useGART;

    initLocal(ops);

    s_ops = ops;
}

