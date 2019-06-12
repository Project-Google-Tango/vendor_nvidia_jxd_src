/*
 * Copyright (c) 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "t30/armc.h"
#include "t30/arahb_arbc.h"
#include "nvrm_heap.h"
#include "nvrm_hwintf.h"
#include "nvcommon.h"
#include "nvrm_drf.h"
#include "../common/nvrm_smmu.h"

#define NVRM_SMMU_CHIP_NAME T30

#define SMMU_PTE_WIDTH  sizeof(NvU32)
#define SMMU_PDE_WIDTH  sizeof(NvU32)
#define SMMU_PTBL_SHIFT 12      // Address bit shift
#define SMMU_PDIR_SHIFT 12
#define SMMU_PAGE_SHIFT 12

#include "../common/nvrm_smmu_common.h"

#define LIST_MC_TRANSLATABLE_SWNAME(op)    \
    op(AFI, asid)   \
    op(AVPC, asid)  \
    op(DC, asid)    \
    op(DCB, asid)   \
    op(EPP, asid)   \
    op(G2, asid)    \
    op(HC, asid)    \
    op(HDA, asid)   \
    op(ISP, asid)   \
    op(MPE, asid)   \
    op(NV, asid)    \
    op(NV2, asid)   \
    op(PPCS, asid)  \
    op(SATA, asid)  \
    op(VDE, asid)   \
    op(VI, asid)

#define WRITE_ASID(SWID, asid) \
    NV_REGW(hDevice, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), \
        MC_SMMU_##SWID##_ASID_0, \
        NV_DRF_DEF(MC, SMMU_##SWID##_ASID, SWID##_SMMU_ENABLE, ENABLE) | \
            NV_DRF_NUM(MC, SMMU_##SWID##_ASID, SWID##_ASID, asid));

static void
initSMMUClients(NvRmDeviceHandle hDevice, NvU32 asid)
{
    NvU32 arb_xbar_ctl;

    LIST_MC_TRANSLATABLE_SWNAME(WRITE_ASID);
    arb_xbar_ctl = NV_REGR(hDevice,
                           NVRM_MODULE_ID(NvRmPrivModuleID_Ahb_Arb_Ctrl, 0),
                           AHB_ARBITRATION_XBAR_CTRL_0);
    NV_REGW(hDevice, NVRM_MODULE_ID(NvRmPrivModuleID_Ahb_Arb_Ctrl, 0),
        AHB_ARBITRATION_XBAR_CTRL_0,
        arb_xbar_ctl |
            NV_DRF_DEF(AHB, ARBITRATION_XBAR_CTRL, SMMU_INIT_DONE, DONE));

}
#undef WRITE_ASID

static void initLocal(struct SMMUChipOps *ops)
{
}
