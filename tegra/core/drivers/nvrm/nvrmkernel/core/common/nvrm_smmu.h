/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvrm_heap.h"
#include "nvrm_heap_simple.h"
#include "nvrm_hwintf.h"
#include "ap15/ap15rm_private.h"
#include "nvassert.h"
#include "nvcommon.h"
#include "nvrm_drf.h"

struct SMMUChipOps {
    void (*setupSMMURegs)(NvRmDeviceHandle hDevice, NvU32 a, NvBool hit_miss);
    void (*flushSMMURegs)(NvRmDeviceHandle hDevice, NvBool enableSMMU);
    void (*setPDEbase)(NvRmDeviceHandle hDevice, NvU32 *PDEbase, NvU32 asid);
    NvU32 smmu_pde_readable_field;
    NvU32 smmu_pde_writable_field;
    NvU32 smmu_pde_nonsecure_field;
    NvU32 smmu_pde_next_field;
    NvU32 smmu_pde_pa_shift;

    NvU32 smmu_pte_readable_field;
    NvU32 smmu_pte_writable_field;
    NvU32 smmu_pte_nonsecure_field;
    NvU32 smmu_pte_pa_shift;

    NvU32 smmu_pdir_shift;
    NvU32 smmu_ptbl_shift;
    NvU32 smmu_page_shift;
    NvU32 smmu_pte_width;
    NvU32 smmu_pde_width;

    NvBool useGART;
};

#define SMMU_PDE_ATTR_READ   SMMU_PDE_READABLE_FIELD
#define SMMU_PDE_ATTR_WRITE  SMMU_PDE_WRITABLE_FIELD
#define SMMU_PDE_ATTR_NONSECURE SMMU_PDE_NONSECURE_FIELD
#define SMMU_PDE_ATTR_NEXT   SMMU_PDE_NEXT_FIELD

#define SMMU_PTE_ATTR_READ   SMMU_PTE_READABLE_FIELD
#define SMMU_PTE_ATTR_WRITE  SMMU_PTE_WRITABLE_FIELD
#define SMMU_PTE_ATTR_NONSECURE SMMU_PTE_NONSECURE_FIELD

#define SMMU_PDE_ATTR   \
        (SMMU_PDE_ATTR_READ|SMMU_PDE_ATTR_WRITE|SMMU_PDE_ATTR_NONSECURE)
#define SMMU_PDE_ATTR_PT   \
        (SMMU_PDE_ATTR_READ|SMMU_PDE_ATTR_WRITE|SMMU_PDE_ATTR_NONSECURE|SMMU_PDE_ATTR_NEXT)
#define SMMU_PTE_ATTR   \
        (SMMU_PTE_ATTR_READ|SMMU_PTE_ATTR_WRITE|SMMU_PTE_ATTR_NONSECURE)

#define SMMU_PAGE_SIZE  (1 << SMMU_PAGE_SHIFT)
#define SMMU_PTBL_SIZE  (SMMU_PAGE_SIZE/SMMU_PTE_WIDTH)    // PTEs per PTBL
#define SMMU_PDIR_SIZE  (SMMU_PAGE_SIZE/SMMU_PDE_WIDTH)    // PDEs per PDIR

