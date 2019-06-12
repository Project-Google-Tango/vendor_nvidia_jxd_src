/*
 * Copyright (c) 2010-2013 NVIDIA Corporation.  All Rights Reserved.
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

#if 0
#define SMMUDPRINTF(a) NvOsDebugPrintf a
#else
#define SMMUDPRINTF(a)
#endif

#define setupSMMURegs(hDevice, a, h_m) s_ChipCB.setupSMMURegs(hDevice, a, h_m)
#define flushSMMURegs(hDevice, enable) s_ChipCB.flushSMMURegs(hDevice, enable)
#define setPDEbase(hDevice, p, a)      s_ChipCB.setPDEbase(hDevice, p, a)
#define SMMU_PDE_READABLE_FIELD        s_ChipCB.smmu_pde_readable_field
#define SMMU_PDE_WRITABLE_FIELD        s_ChipCB.smmu_pde_writable_field
#define SMMU_PDE_NONSECURE_FIELD       s_ChipCB.smmu_pde_nonsecure_field
#define SMMU_PDE_NEXT_FIELD            s_ChipCB.smmu_pde_next_field
#define SMMU_PDE_PA_SHIFT              s_ChipCB.smmu_pde_pa_shift
#define SMMU_PTE_READABLE_FIELD        s_ChipCB.smmu_pte_readable_field
#define SMMU_PTE_WRITABLE_FIELD        s_ChipCB.smmu_pte_writable_field
#define SMMU_PTE_NONSECURE_FIELD       s_ChipCB.smmu_pte_nonsecure_field
#define SMMU_PTE_PA_SHIFT              s_ChipCB.smmu_pte_pa_shift
#define SMMU_PDIR_SHIFT                s_ChipCB.smmu_pdir_shift
#define SMMU_PTBL_SHIFT                s_ChipCB.smmu_ptbl_shift
#define SMMU_PAGE_SHIFT                s_ChipCB.smmu_page_shift
#define SMMU_PDE_WIDTH                 s_ChipCB.smmu_pte_width
#define SMMU_PTE_WIDTH                 s_ChipCB.smmu_pte_width

#include "../common/nvrm_smmu.h"
static struct SMMUChipOps s_ChipCB;

/**
 * Simulate GART with SMMU
 */
#define NV_MC_SMMU_NUM_ASIDS 4

extern NvBool           gs_GartInited;
extern NvRmHeapSimple   gs_GartAllocator;
extern NvU32            *gs_GartSave;

static NvU32 *s_PAllocbase;
static NvU32 *s_PAA[NV_MC_SMMU_NUM_ASIDS];
static NvU32 *s_PDEbase;
static NvU32 *s_PTEbase;
static NvU32 s_NumPTEs;
static NvU32 s_SMMUAddr;
static NvU32 s_SMMUAddrAligned;
static NvU32 s_SMMUSize;
static NvU32 s_PTEOffset;
static NvU32 s_PDEOffset;
static NvU32 s_allocSize;

#ifdef ALIGN
#undef ALIGN
#endif
#define ALIGN(n,m) ((n) & ~((m)-1))
#define ALIGNDN(n,m) ALIGN((n),(m))
#define ALIGNUP(n,m) ALIGN((n)+(m)-1,(m));

/* Address space ID to use */
#ifndef NVRM_T30_SMMUASID
#define NVRM_T30_SMMUASID 0
#endif
static NvU32 s_ASID = NVRM_T30_SMMUASID;

/* Enable stat counters */
#ifndef SMMU_HIT_MISS_STAT
#define SMMU_HIT_MISS_STAT 0
#endif
static NvU32 s_hit_miss_stat = SMMU_HIT_MISS_STAT;

extern NvBool g_useGART;

extern void NvRmPrivT30InitSMMUCallBack(struct SMMUChipOps *ops, NvBool useGT);
/**
 * Initializes all of the TLB entries in the SMMU and enables SMMU translation
 * All SMMU-range entries are initially marked "invalid" and other locations
 * are marked "through."
 *
 * @param hDevice The RM device handle.
 */
static NvError
NvRmPrivT30InitSMMU(NvRmDeviceHandle hDevice)
{
    NvU32 nPTE;
    NvU32 nPDE;
    NvU32 SMMUAddrCeiling;
    NvU32 n;

    NV_ASSERT(hDevice != NULL);

    switch(hDevice->ChipId.Id)
    {
    default:
        // Latest - falls through to the latest chip
    case 0x30:
        NvRmPrivT30InitSMMUCallBack(&s_ChipCB, g_useGART);
        break;
    }

    s_SMMUAddr = hDevice->GartMemoryInfo.base;
    s_SMMUSize = hDevice->GartMemoryInfo.size;

    NV_ASSERT(!(s_SMMUAddr % SMMU_PAGE_SIZE));
    NV_ASSERT(!(s_SMMUSize % SMMU_PAGE_SIZE));

    s_SMMUAddrAligned = ALIGNDN(s_SMMUAddr, SMMU_PAGE_SIZE * SMMU_PTBL_SIZE);
    SMMUAddrCeiling = ALIGNUP(s_SMMUAddr + s_SMMUSize,
                              SMMU_PAGE_SIZE * SMMU_PTBL_SIZE);


    // Both addresses are aligned at PTBL boundary
    s_NumPTEs = (SMMUAddrCeiling - s_SMMUAddrAligned) / SMMU_PAGE_SIZE;

    s_PTEOffset = (s_SMMUAddr - s_SMMUAddrAligned) / SMMU_PAGE_SIZE;
    s_PDEOffset = s_SMMUAddrAligned / (SMMU_PAGE_SIZE * SMMU_PTBL_SIZE);

    // PDE/PTE allocation must align at PDIR/PTBL allocation boundary.
    s_allocSize = SMMU_PDE_WIDTH * SMMU_PDIR_SIZE + SMMU_PTE_WIDTH * s_NumPTEs;

    // For A02 and above, reserve a dummy page for AVP vectors.
    if (s_SMMUAddrAligned < 0x80000000)
    {
        s_allocSize += SMMU_PAGE_SIZE;

        // We need PT for page zero
        if (s_SMMUAddrAligned != 0x0)
            s_allocSize += SMMU_PAGE_SIZE;
    }

    gs_GartSave = NvOsAlloc(s_allocSize*NV_MC_SMMU_NUM_ASIDS + SMMU_PAGE_SIZE);
    if (!gs_GartSave)
        return NvError_InsufficientMemory;
    s_PAllocbase = (NvU32 *)ALIGNUP((NvU32)gs_GartSave, SMMU_PAGE_SIZE);

    for (n = 0; n < NV_MC_SMMU_NUM_ASIDS; n++)
        s_PAA[n] = (NvU32 *)((NvU32)s_PAllocbase + n * s_allocSize);
    s_PDEbase = s_PAA[s_ASID];
    s_PTEbase = (NvU32 *)((NvU32)s_PDEbase + SMMU_PDE_WIDTH * SMMU_PDIR_SIZE);

    // Initialize PDEs to "through mode"
    for (nPDE = 0; nPDE < SMMU_PDIR_SIZE; nPDE++)
        s_PDEbase[nPDE]    = SMMU_PDE_ATTR | (nPDE << SMMU_PDE_PA_SHIFT);

    // Initialize PTEs to "through mode" and make PDEs point to PTBLs
    for (nPTE = 0; nPTE < s_NumPTEs; nPTE++)
    {
        NvU32 *pte = &s_PTEbase[nPTE];

        *pte = SMMU_PTE_ATTR |
               ((s_SMMUAddrAligned + nPTE*SMMU_PAGE_SIZE) >> SMMU_PTBL_SHIFT);
        if (!(nPTE % SMMU_PTBL_SIZE))
             s_PDEbase[s_PDEOffset + nPTE / SMMU_PTBL_SIZE] =
                 SMMU_PDE_ATTR_PT | ((NvU32)pte >> SMMU_PTBL_SHIFT);
    }

    // Reserve a dummy page for AVP vectors at address zero pointing to
    // the extra allocation.
    if (s_SMMUAddrAligned < 0x80000000)
    {
        NvU32 *AVPvectorPTE;
        if (s_SMMUAddrAligned == 0x0)
            AVPvectorPTE = s_PTEbase;
        else
        {
            AVPvectorPTE = (NvU32 *)((NvU32)s_PAllocbase + s_allocSize
                                      - SMMU_PAGE_SIZE * 2);
            for (nPTE = 0; nPTE < SMMU_PTBL_SIZE; nPTE++)
                AVPvectorPTE[nPTE] = SMMU_PTE_ATTR |
                                     ((nPTE*SMMU_PAGE_SIZE) >> SMMU_PTBL_SHIFT);
            s_PDEbase[0] =
                 SMMU_PDE_ATTR_PT | ((NvU32)AVPvectorPTE >> SMMU_PTBL_SHIFT);
        }

        AVPvectorPTE[0] = SMMU_PTE_ATTR |
            (((NvU32)s_PAllocbase + s_allocSize - SMMU_PAGE_SIZE) >>
                SMMU_PTBL_SHIFT);
    }

    NvOsDataCacheWritebackRange(s_PDEbase, s_allocSize);
    setPDEbase(hDevice, s_PDEbase, s_ASID);
    setupSMMURegs(hDevice, s_ASID, s_hit_miss_stat);
    return NvSuccess;
}

#if NVOS_IS_LINUX
static NvError
NvOsPhysicalMemMapping(NvOsPhysAddr PA, size_t size,
    NvOsMemAttribute attrib, NvU32 flags, void **VAp)
{
    NV_ASSERT(!"Not implemented");
    return NvError_NotSupported;
}
#else
NvError NvOsPhysicalMemMapping(NvOsPhysAddr PA, size_t size,
    NvOsMemAttribute attrib, NvU32 flags, void **VAp);
#endif

#ifndef NV_USE_AOS
#define WITHIN_OS_MANAGED_RANGE(addr) 1
#else
extern NvU32 nvaos_GetMemoryStart(void);
extern NvU32 nvaos_GetMemoryEnd(void);
#define WITHIN_OS_MANAGED_RANGE(addr) \
    (nvaos_GetMemoryStart() <= (NvU32)(addr) && \
    (NvU32)(addr) < nvaos_GetMemoryEnd())
#endif

NvError
NvRmPrivT30HeapSMMUAlloc(
    NvRmDeviceHandle hDevice,
    NvOsPageAllocHandle hPageHandle,
    NvU32 NumberOfPages,
    NvRmPhysAddr *ioVAp)
{
    NvError result;
    NvU32 nPage;
    NvU32 FirstPageAddr;
    NvU32 PTEOffset;
    NvU32 basePAddr;

    NV_ASSERT(hDevice);
    NV_ASSERT(hPageHandle);

    NvOsMutexLock(hDevice->mutex);

    if (gs_GartInited == NV_FALSE)
    {
        result = NvRmPrivT30InitSMMU(hDevice);
        if (NvSuccess != result)
            goto fail;
        gs_GartInited = NV_TRUE;
    }
    NvOsMutexUnlock(hDevice->mutex);

    result = NvRmPrivHeapSimpleAlloc(
        &gs_GartAllocator,
        NumberOfPages * SMMU_PAGE_SIZE,
        SMMU_PAGE_SIZE,
        &FirstPageAddr);

    if (result != NvSuccess)
        return result;

    *ioVAp = FirstPageAddr;

    NvOsMutexLock(hDevice->mutex);
    /* Check that the SMMU address exists and is page aligned */
    NV_ASSERT(FirstPageAddr != 0xFFFFFFFF);
    NV_ASSERT(!(FirstPageAddr % SMMU_PAGE_SIZE));

    PTEOffset = s_PTEOffset + (FirstPageAddr - s_SMMUAddr)/SMMU_PAGE_SIZE;
    NV_ASSERT(PTEOffset + NumberOfPages <= s_NumPTEs);

    /* If within AOS-managed memory address range, get mapped physical
     * address. Otherwise, use given address.
     * Simulator always uses mapped physical address.
     */
    basePAddr = WITHIN_OS_MANAGED_RANGE(hPageHandle)
                ? NvOsPageAddress(hPageHandle, 0)
                : (NvU32)hPageHandle;
    for (nPage = 0; nPage < NumberOfPages; nPage++)
    {
        s_PTEbase[PTEOffset + nPage] =
            SMMU_PTE_ATTR |
            ((basePAddr + nPage * SMMU_PAGE_SIZE) >> SMMU_PAGE_SHIFT);
        ((NvU32 *)(basePAddr + nPage * SMMU_PAGE_SIZE))[0] =
                        FirstPageAddr + nPage * SMMU_PAGE_SIZE;
        ((NvU32 *)(basePAddr + nPage * SMMU_PAGE_SIZE))[1] =
                        basePAddr + nPage * SMMU_PAGE_SIZE;
        NvOsDataCacheWritebackRange(
                        (void *)(basePAddr + nPage * SMMU_PAGE_SIZE),
                                 sizeof(NvU32) *2);
    }

    NvOsDataCacheWritebackRange(&s_PTEbase[PTEOffset],
                                SMMU_PTE_WIDTH * NumberOfPages);
    flushSMMURegs(hDevice, 0);
    SMMUDPRINTF(("%s: ioVA=0x%x NumberOfPages=0x%x paddr=0x%x\n",
        __FUNCTION__, *ioVAp,NumberOfPages, basePAddr));
fail:
    NvOsMutexUnlock(hDevice->mutex);

    return result;
}

void
NvRmPrivT30HeapSMMUFree(
    NvRmDeviceHandle hDevice,
    NvRmPhysAddr ioVA,
    NvU32 NumberOfPages)
{
    NvU32 nPage;
    NvU32 PTEOffset = s_PTEOffset + (ioVA - s_SMMUAddr)/SMMU_PAGE_SIZE;
    NV_ASSERT(hDevice);

    if (!ioVA || !NumberOfPages)
        return;

    NV_ASSERT(!(ioVA % SMMU_PAGE_SIZE));
    NV_ASSERT(s_SMMUAddr <= ioVA);

    for (nPage = 0; nPage < NumberOfPages; nPage++)
    {
        s_PTEbase[PTEOffset + nPage] =
            SMMU_PTE_ATTR |
                ((ioVA + nPage * SMMU_PAGE_SIZE) >> SMMU_PAGE_SHIFT);
    }
    NvOsDataCacheWritebackRange(&s_PTEbase[PTEOffset],
                                SMMU_PTE_WIDTH * NumberOfPages);
    flushSMMURegs(hDevice, 0);
    NvRmPrivHeapSimpleFree(&gs_GartAllocator, ioVA);
    SMMUDPRINTF(("%s: ioVA=0x%x NumberOfPages=0x%x\n",
        __FUNCTION__, ioVA, NumberOfPages));

}

void
NvRmPrivT30SMMUSuspend(NvRmDeviceHandle hDevice)
{
    // Nothing to do
}

void
NvRmPrivT30SMMUResume(NvRmDeviceHandle hDevice)
{
    NvOsMutexLock(hDevice->mutex);
    setPDEbase(hDevice, s_PDEbase, s_ASID);
    setupSMMURegs(hDevice, s_ASID, s_hit_miss_stat);
    NvOsMutexUnlock(hDevice->mutex);
}

NvError
NvRmPrivT30SMMUMap(NvOsPhysAddr ioVA, size_t size, NvOsMemAttribute attrib,
                   NvU32 flags, void **cpuVAp)
{
    NvU32 ioVAAligned = ALIGNDN(ioVA, SMMU_PAGE_SIZE);
    NvU32 offset = ioVA - ioVAAligned;
    NvOsPhysAddr physAddr;

    NV_ASSERT((s_SMMUAddr <= ioVA) && (ioVA + size <= s_SMMUAddr + s_SMMUSize));
    physAddr = (s_PTEbase[(ioVAAligned - s_SMMUAddrAligned) >> SMMU_PAGE_SHIFT]
                    << SMMU_PAGE_SHIFT) + offset;
    SMMUDPRINTF(("%s: ioVA=0x%x physAddr=0x%x\n", __FUNCTION__,ioVA,physAddr));
    return NvOsPhysicalMemMapping(physAddr, size, attrib, flags, cpuVAp);
}
