/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA CORPORATION
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "aos.h"
#include "avp.h"
#include "aos_avp.h"
#include "nvrm_drf.h"
#include "nvrm_module.h"
#include "common/nvrm_moduleids.h"
#include "nvos_internal.h"
#include "t30/aravp_cache.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"

#define ENABLE_SMMU_WINDOW_CACHING 1

#define CACHE_READ32(reg, offset) \
    NV_READ32( (T30_AVP_CACHE_BASE + AVP_CACHE_##reg##_0 + (offset)) );

#define CACHE_WRITE32(reg, offset, value) \
    do \
    { \
        NV_WRITE32( (T30_AVP_CACHE_BASE + AVP_CACHE_##reg##_0 + (offset)), (value) ); \
    } while (0)

typedef struct MmuSegDescRec
{
    NvU32 StartAddr;
    NvU32 EndAddr;
    NvU32 attributes;
    NvU32 rsvd;
} MmuSegDesc;

static NvOsIntrMutexHandle s_CacheLock = NULL;

static void ReadSegmentDesc(NvU32 index, MmuSegDesc* pMmuDesc)
{
    pMmuDesc->StartAddr = CACHE_READ32( MMU_MAIN_ENTRY, (index * 16) );
    pMmuDesc->EndAddr = CACHE_READ32( MMU_MAIN_ENTRY, ((index * 16) + 4) );
    pMmuDesc->attributes = CACHE_READ32( MMU_MAIN_ENTRY, ((index * 16) + 8) );
}

static void InsertSegmentDesc(NvU32 index, MmuSegDesc* pMmuDesc)
{
    NvU32 MmuCmd;
    NvU32 MmuShadowCopyMask;

    CACHE_WRITE32(MMU_SHADOW_ENTRY, (index * 16), pMmuDesc->StartAddr);
    CACHE_WRITE32(MMU_SHADOW_ENTRY, ((index * 16) + 4), pMmuDesc->EndAddr);
    CACHE_WRITE32(MMU_SHADOW_ENTRY, ((index * 16) + 8), pMmuDesc->attributes);

    MmuShadowCopyMask = 1 << index;
    CACHE_WRITE32(MMU_SHADOW_COPY_MASK_0, 0, MmuShadowCopyMask);
    MmuCmd = NV_DRF_DEF(AVP_CACHE, MMU_CMD, CMD, COPY_SHADOW);
    CACHE_WRITE32(MMU_CMD, 0, MmuCmd);
}

static void FlushInvalidateSegment(NvU32 StartAddr, NvU32 EndAddr)
{
    NvU32 i;
    NvU32 CacheMaint0;
    NvU32 CacheMaint2;
    NvU32 RawEvent;

    for (i = StartAddr; i <= EndAddr; i += AVP_CACHE_LINE_SIZE)
    {
        CACHE_WRITE32(INT_CLEAR, 0, 0x1);
        CacheMaint0 = NV_DRF_NUM(AVP_CACHE, MAINT_0, ADDR, i);
        CACHE_WRITE32(MAINT_0, 0, CacheMaint0);
        CacheMaint2 = NV_DRF_DEF(AVP_CACHE, MAINT_2, OPCODE, CLEAN_INVALID_PHY);
        CACHE_WRITE32(MAINT_2, 0, CacheMaint2);
        do {
            RawEvent = CACHE_READ32(INT_RAW_EVENT, 0);
        } while ( !(RawEvent & 0x1));
        CACHE_WRITE32(INT_CLEAR, 0, RawEvent);
    }
}

static NvS32 GetFreeMmuSegmentIndex(NvS32 StartIndex)
{
    NvS32 index;
    MmuSegDesc m;

    for (index = StartIndex; index < T30_MMU_MAX_SEGMENTS; index++)
    {
        ReadSegmentDesc(index, &m);
        if ( m.StartAddr > m.EndAddr)
            return index;
    }
    return -1;
}

static void RemoveOrSplitFromMmuSegment(NvU32 StartAddr, NvU32 EndAddr)
{
    NvU32 i;
    MmuSegDesc m;
    MmuSegDesc m2 = {0};
    NvS32 FreeIndex = -1;
    NvBool RemoveFromMiddle = NV_FALSE;

    NvOsIntrMutexLock(s_CacheLock);
    for (i = 0; i < T30_MMU_MAX_SEGMENTS; i++)
    {
        ReadSegmentDesc(i, &m);
        if ( (StartAddr == m.StartAddr) && (EndAddr == m.EndAddr) )
        {
            // Just Remove.
            m.StartAddr = 0xFFFFFFFF;
            m.EndAddr = 0;
            m.attributes = 0;
            break;
        }
        else if ( (StartAddr == m.StartAddr) && (EndAddr < m.EndAddr ) )
        {
            // Remove From Start
            m.StartAddr = EndAddr + AVP_CACHE_LINE_SIZE;
            break;
        }
        else if ( (StartAddr > m.StartAddr) && (EndAddr == m.EndAddr) )
        {
            // Remove From End;
            m.EndAddr = StartAddr - AVP_CACHE_LINE_SIZE;
            break;
        }
        else if ( (StartAddr > m.StartAddr) && (EndAddr < m.EndAddr) )
        {
            // Remove From Middle;
            RemoveFromMiddle = NV_TRUE;
            if (FreeIndex == -1)
                FreeIndex = GetFreeMmuSegmentIndex(i + 1);
            m2.StartAddr = EndAddr + AVP_CACHE_LINE_SIZE;
            m2.EndAddr = m.EndAddr;
            m2.attributes = T30_MMU_ATTRIBUTES;
            m.EndAddr = StartAddr - AVP_CACHE_LINE_SIZE;
            if (FreeIndex == -1)
                FlushInvalidateSegment(m2.StartAddr, m2.EndAddr);
            break;
        }
        else if ( (m.StartAddr > m.EndAddr) && (FreeIndex == -1) )
        {
            FreeIndex = i;
        }
    }
    if (i == T30_MMU_MAX_SEGMENTS)
        goto out;
    FlushInvalidateSegment(StartAddr, EndAddr);
    InsertSegmentDesc(i, &m);
    if (RemoveFromMiddle && (FreeIndex != -1))
        InsertSegmentDesc(FreeIndex, &m2);
out:
    NvOsIntrMutexUnlock(s_CacheLock);
}

static void AddOrMergeToMmuSegment(NvU32 StartAddr, NvU32 EndAddr)
{
    NvU32 i;
    MmuSegDesc m;
    NvS32 FreeIndex = -1;

    NvOsIntrMutexLock(s_CacheLock);
    for (i = 0; i < T30_MMU_MAX_SEGMENTS; i++)
    {
        ReadSegmentDesc(i, &m);
        if ( (EndAddr + AVP_CACHE_LINE_SIZE) == m.StartAddr )
        {
            m.StartAddr = StartAddr;
            InsertSegmentDesc(i, &m);
            goto out;
        }
        else if (StartAddr == (m.EndAddr + AVP_CACHE_LINE_SIZE))
        {
            m.EndAddr = EndAddr;
            InsertSegmentDesc(i, &m);
            goto out;
        }
        else if ( (m.StartAddr > m.EndAddr) && (FreeIndex == -1) )
        {
            FreeIndex = i;
        }
    }

    if (FreeIndex != -1)
    {
        m.StartAddr = StartAddr;
        m.EndAddr = EndAddr;
        m.attributes = T30_MMU_ATTRIBUTES;
        InsertSegmentDesc(FreeIndex, &m);
    }
    else
    {
        // Out of MMU segment descriptors. All 32 segment descriptor are in use.
        // Can't map the request memory to cached. Leave it uncached.
    }
out:
    NvOsIntrMutexUnlock(s_CacheLock);
}

extern unsigned int Image$$MainRegion$$Base;
extern unsigned int Image$$ProgramStackRegionEnd$$Base;

static void InitMmu(void)
{
    MmuSegDesc m;
    NvU32 MmuCmd;
    NvU32 MmuFallbackentry;
    NvU32 i;

    // StarBlockAddr > EndAddr is invalid entry.
    m.StartAddr = 0xFFFFFFFF;
    m.EndAddr = 0;
    m.attributes  = 0;

    // Init MMU.
    MmuCmd = NV_DRF_DEF(AVP_CACHE, MMU_CMD, CMD, INIT);
    CACHE_WRITE32(MMU_CMD, 0, MmuCmd);
    // Set Fallback Path for MMU as uncached.
    MmuFallbackentry =
        NV_DRF_DEF(AVP_CACHE, MMU_FALLBACK_ENTRY, CACHED, DISABLE) |
        NV_DRF_DEF(AVP_CACHE, MMU_FALLBACK_ENTRY, EXE_ENA, ENABLE) |
        NV_DRF_DEF(AVP_CACHE, MMU_FALLBACK_ENTRY, RD_ENA, ENABLE) |
        NV_DRF_DEF(AVP_CACHE, MMU_FALLBACK_ENTRY, WR_ENA, ENABLE);
    CACHE_WRITE32(MMU_FALLBACK_ENTRY, 0, MmuFallbackentry);
    // Setup MMU Descriptors.
    for (i = 0; i < T30_MMU_MAX_SEGMENTS; i++)
    {
        InsertSegmentDesc(i, &m);
    }
#if ENABLE_SMMU_WINDOW_CACHING
    m.StartAddr = (NvU32)(&Image$$MainRegion$$Base);
    m.EndAddr = (NvU32)(&Image$$ProgramStackRegionEnd$$Base);
    m.attributes  = T30_MMU_ATTRIBUTES;
    InsertSegmentDesc(0, &m);
#endif
}

static void CacheInvalidate(void)
{
    NvU32 RawEvent;
    NvU32 CacheMaint2;

    // Invalidate Cache.
    // As per HW bug 742002, cache shouldn't flushed before enabling it. So, flush
    // it after enabling cache.
    CACHE_WRITE32(INT_CLEAR, 0, 0x1);
    CacheMaint2 = NV_DRF_DEF(AVP_CACHE, MAINT_2, OPCODE, INVALID_WAY) |
                  NV_DRF_DEF(AVP_CACHE, MAINT_2, WAY_BITMAP, DEFAULT_MASK);
    CACHE_WRITE32(MAINT_2, 0, CacheMaint2);
    do {
        RawEvent = CACHE_READ32(INT_RAW_EVENT, 0);
    } while ( !(RawEvent & 1) );
    CACHE_WRITE32(INT_CLEAR, 0, RawEvent);
}

static void CacheEnable(void)
{
    NvU32 CacheConfig;

    // Enable Cache.
    CacheConfig = NV_DRF_DEF(AVP_CACHE, CONFIG, ENABLE_CACHE, TRUE) |
                  NV_DRF_NUM(AVP_CACHE, CONFIG, TAG_CHECK_ABORT_ON_ERROR, 1);
    CACHE_WRITE32(CONFIG, 0, CacheConfig);
}

static void SetTagCheckAbortError(NvBool set)
{
    NvU32 CacheConfig;

    if (set)
    {
        // Clear Tag check abort error.
        CacheConfig = NV_DRF_DEF(AVP_CACHE, CONFIG, ENABLE_CACHE, TRUE) |
                      NV_DRF_NUM(AVP_CACHE, CONFIG, TAG_CHECK_CLR_ERROR, 1);
        CACHE_WRITE32(CONFIG, 0, CacheConfig);
        // Enable Tag check abort error.
        CacheConfig = NV_DRF_DEF(AVP_CACHE, CONFIG, ENABLE_CACHE, TRUE) |
                      NV_DRF_NUM(AVP_CACHE, CONFIG, TAG_CHECK_ABORT_ON_ERROR, 1);
        CACHE_WRITE32(CONFIG, 0, CacheConfig);
    }
    else
    {
        CacheConfig = NV_DRF_DEF(AVP_CACHE, CONFIG, ENABLE_CACHE, TRUE) |
                      NV_DRF_NUM(AVP_CACHE, CONFIG, TAG_CHECK_ABORT_ON_ERROR, 0);
        CACHE_WRITE32(CONFIG, 0, CacheConfig);
    }
}

void T30CacheEnable(void)
{
    NV_ASSERT_SUCCESS(NvOsIntrMutexCreate(&s_CacheLock));
    InitMmu();
    CacheInvalidate();
    CacheEnable();
}

#if !__GNUC__
#pragma arm section code = "AvpCacheFlushRegion", rwdata = "AvpCacheFlushRegion", rodata = "AvpCacheFlushRegion", zidata ="AvpCacheFlushRegion"
#endif

NvU32 g_CacheMaint2;
NvU32 g_RawEvent;
void T30InstrCacheFlushInvalidate(void)
{
    NvOsIntrMutexLock(s_CacheLock);
    // Disable tag check abort error before flush operation.
    // Check HW bug 754551
    SetTagCheckAbortError(NV_FALSE);
    CACHE_WRITE32(INT_CLEAR, 0, 0x1);
    g_CacheMaint2 = NV_DRF_DEF(AVP_CACHE, MAINT_2, OPCODE, CLEAN_INVALID_WAY) |
                  NV_DRF_DEF(AVP_CACHE, MAINT_2, WAY_BITMAP, DEFAULT_MASK);
    CACHE_WRITE32(MAINT_2, 0, g_CacheMaint2);
    do {
        g_RawEvent = CACHE_READ32(INT_RAW_EVENT, 0);
    } while ( !(g_RawEvent & 1) );
    CACHE_WRITE32(INT_CLEAR, 0, g_RawEvent);
    SetTagCheckAbortError(NV_TRUE);
    NvOsIntrMutexUnlock(s_CacheLock);
}

#if !__GNUC__
#pragma arm section code, rwdata, rodata, zidata
#endif

NvError
T30PhysicalMemMap(NvOsPhysAddr phys,
    size_t size,
    NvOsMemAttribute attrib,
    NvU32 flags,
    void **ptr)
{
    NvU32 StartAddr, EndAddr;

    // If start address is in the middle of cache line, align it
    // to next cache line as the remaining bytes of cache line might
    // be uncached.
    StartAddr = (phys + AVP_CACHE_LINE_MASK) & (~AVP_CACHE_LINE_MASK);
    EndAddr = phys + size - 1;
    // if end address is in middle of a cached line, align it to prev
    // cache line.
    // AVP_CACHE_LINE_SIZE is 32(0x20) bytes
    // start line = 0, end line = 0, caches 0 to 31 bytes.
    // start line = 0, end line = 0x20, caches 0 to 63 bytes.
    EndAddr = (EndAddr - AVP_CACHE_LINE_MASK) & (~AVP_CACHE_LINE_MASK);

    if ( (attrib == NvOsMemAttribute_WriteBack) &&
         (size >= AVP_CACHE_LINE_SIZE) &&
         (EndAddr >= StartAddr))
    {
        if (!(phys + size <= TXX_MMIO_START || phys >= TXX_MMIO_END))
        {
            NvOsDebugPrintf("%s: WARNING: cached access to MMIO (%x-%x)\n",
                            __func__, phys, phys+size);
        }
        AddOrMergeToMmuSegment(StartAddr, EndAddr);
    }
    *ptr = (void*)phys;
    return NvSuccess;
}

void T30PhysicalMemUnmap(void *ptr, size_t size)
{
    NvU32 StartAddr, EndAddr;
    NvOsPhysAddr phys = (NvOsPhysAddr)ptr;

    StartAddr = (phys + AVP_CACHE_LINE_MASK) & (~AVP_CACHE_LINE_MASK);
    EndAddr = phys + size - 1;
    EndAddr = (EndAddr - AVP_CACHE_LINE_MASK)& (~AVP_CACHE_LINE_MASK);

    if ( (size >= AVP_CACHE_LINE_SIZE) &&
         (EndAddr >= StartAddr))
    {
        RemoveOrSplitFromMmuSegment(StartAddr, EndAddr);
    }
}

