/*
 * Copyright 2008-2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "aos.h"
#include "aos_common.h"
#include "nvrm_drf.h"
#include "nvrm_arm_cp.h"
#include "nvrm_hardware_access.h"
#include "aos_pl310.h"

extern NvBlCpuClusterId s_ClusterId;
extern NvBool s_UsePL310L2Cache;

NvBool s_pl310initialized = NV_FALSE;

static NvU32 NvAosPl310IsEnabled(void)
{
    NvU32   reg;
    NvU32   result = 0;

    if (s_UsePL310L2Cache)
    {
        reg = NV_PL310_REGR(CONTROL);
        result = NV_DRF_VAL(PL310, CONTROL, ENABLE, reg);
    }
    return result;
}

void
nvaosPl310Init(void)
{
    NvU32 reg;
    NvU32 AuxReg;

    if (!s_UsePL310L2Cache || s_pl310initialized) return;

    // Get the cache id register which contains the PL310 RTL release number.
    reg = NV_PL310_REGR(CACHE_ID);
    if (NV_DRF_VAL(PL310, CACHE_ID, RTL_RELEASE, reg) < 3)
    {
        NV_ASSERT(!"We don't support PL310 RTL release less than 3");
    }

    // Configure the cache
    reg = NV_DRF_DEF(PL310, AUXILIARY_CONTROL, EXCLUSIVE, DISABLED)
        | NV_DRF_DEF(PL310, AUXILIARY_CONTROL, ASSOCIATIVITY, ASSOC_8)
        | NV_DRF_DEF(PL310, AUXILIARY_CONTROL, EVENT_MONITOR_BUS, DISABLED)
        | NV_DRF_DEF(PL310, AUXILIARY_CONTROL, PARITY, DISABLED)
        | NV_DRF_DEF(PL310, AUXILIARY_CONTROL, SHARED_ATTRIBUTE_OVERRIDE,
            DISABLED)
        | NV_DRF_DEF(PL310, AUXILIARY_CONTROL, FORCE_WRITE_ALLOCATE, DISABLED)
        | NV_DRF_DEF(PL310, AUXILIARY_CONTROL, NON_SECURE_LOCKDOWN_WR, ENABLED)
        | NV_DRF_DEF(PL310, AUXILIARY_CONTROL, NON_SECURE_INTERRUPT_ACCESS,
            ENABLED)
        | NV_DRF_DEF(PL310, AUXILIARY_CONTROL, DATA_PREFETCH, DISABLED)
        | NV_DRF_DEF(PL310, AUXILIARY_CONTROL, INSTRUCTION_PREFETCH, ENABLED);
    // Preserve the waysize from the auxiliary register, if it is valid.
    AuxReg = NV_PL310_REGR(AUXILIARY_CONTROL);
    AuxReg = NV_DRF_VAL(PL310, AUXILIARY_CONTROL, WAY_SIZE, AuxReg);
    AuxReg = NV_DRF_NUM(PL310, AUXILIARY_CONTROL, WAY_SIZE, AuxReg);
    reg |= AuxReg ? AuxReg : NV_DRF_DEF(PL310, AUXILIARY_CONTROL, WAY_SIZE, DEFAULT);
    NV_PL310_REGW(AUXILIARY_CONTROL, reg);

    if (s_ClusterId == NvBlCpuClusterId_LP)
    {
        reg = NV_DRF_NUM(PL310, TAG_RAM_LATENCY, SETUP, 1)
            | NV_DRF_NUM(PL310, TAG_RAM_LATENCY, READ, 2)
            | NV_DRF_NUM(PL310, TAG_RAM_LATENCY, WRITE, 2);
        NV_PL310_REGW(TAG_RAM_LATENCY, reg);

        reg = NV_DRF_NUM(PL310, DATA_RAM_LATENCY, SETUP, 1)
            | NV_DRF_NUM(PL310, DATA_RAM_LATENCY, READ, 2)
            | NV_DRF_NUM(PL310, DATA_RAM_LATENCY, WRITE, 2);
        NV_PL310_REGW(DATA_RAM_LATENCY, reg);
    }
    else
    {
        reg = NV_DRF_NUM(PL310, TAG_RAM_LATENCY, SETUP, 1)
            | NV_DRF_NUM(PL310, TAG_RAM_LATENCY, READ, 3)
            | NV_DRF_NUM(PL310, TAG_RAM_LATENCY, WRITE, 3);
        NV_PL310_REGW(TAG_RAM_LATENCY, reg);

        reg = NV_DRF_NUM(PL310, DATA_RAM_LATENCY, SETUP, 1)
            | NV_DRF_NUM(PL310, DATA_RAM_LATENCY, READ, 4)
            | NV_DRF_NUM(PL310, DATA_RAM_LATENCY, WRITE, 4);
        NV_PL310_REGW(DATA_RAM_LATENCY, reg);
    }

    // Invalidate all cache ways.
    reg = NV_DRF_DEF(PL310, INVALIDATE_BY_WAY, WAY_BITMAP, ALL_WAYS);
    NV_PL310_REGW(INVALIDATE_BY_WAY, reg);

    // Poll for completion.
    while (NV_PL310_REGR(INVALIDATE_BY_WAY) != 0);

    // Mask all cache interrupts.
    reg = NV_DRF_DEF(PL310, INTERRUPT_MASK, ECNTR, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_MASK, PARRT, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_MASK, PARRD, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_MASK, ERRWT, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_MASK, ERRWD, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_MASK, ERRRT, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_MASK, ERRRD, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_MASK, SLVERR, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_MASK, DECERR, DEFAULT);
    NV_PL310_REGW(INTERRUPT_MASK, reg);

    // Clear any existing interrupts
    reg = NV_DRF_DEF(PL310, INTERRUPT_CLEAR, ECNTR, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_CLEAR, PARRT, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_CLEAR, PARRD, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_CLEAR, ERRWT, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_CLEAR, ERRWD, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_CLEAR, ERRRT, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_CLEAR, ERRRD, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_CLEAR, SLVERR, DEFAULT)
        | NV_DRF_DEF(PL310, INTERRUPT_CLEAR, DECERR, DEFAULT);
    NV_PL310_REGW(INTERRUPT_CLEAR, reg);

    s_pl310initialized = NV_TRUE;
}

void
nvaosPl310Enable(void)
{
    NvU32   reg;
    if (!s_UsePL310L2Cache || NvAosPl310IsEnabled()) return;

    reg = NV_DRF_DEF(PL310, CONTROL, ENABLE, ENABLED);
    NV_PL310_REGW(CONTROL, reg);
}

void
nvaosPl310InvalidateDisable( void )
{
    NvU32 reg;
    NvU32 way;
    NvU32 is_enabled;

    if (!s_UsePL310L2Cache) return;

    is_enabled = NvAosPl310IsEnabled();

    // If enabled, lock all the cache ways and clean and invalidate all ways.
    if (is_enabled)
    {
        for (way = 0; way < 8; way++)
        {
            NV_WRITE32(PL310_BASE + PL310_DATA_LOCKDOWN0_0 + (way*4),
                NV_DRF_DEF(PL310, DATA_LOCKDOWN0, WAY_BITMAP, ALL_WAYS));
            NV_WRITE32(PL310_BASE + PL310_INSTRUCTION_LOCKDOWN0_0 + (way*4),
                NV_DRF_DEF(PL310, INSTRUCTION_LOCKDOWN0, WAY_BITMAP, ALL_WAYS));
        }

        // writeback and invalidate all ways.
        reg = NV_DRF_DEF(PL310, CLEAN_AND_INVALIDATE_BY_WAY, WAY_BITMAP, ALL_WAYS);
        NV_PL310_REGW(CLEAN_AND_INVALIDATE_BY_WAY, reg);

        // Poll for completion.
        do
        {
            reg = NV_PL310_REGR(CLEAN_AND_INVALIDATE_BY_WAY);
            reg = NV_DRF_VAL(PL310, CLEAN_AND_INVALIDATE_BY_WAY, WAY_BITMAP, reg);
        }
        while (reg != 0);

        // Data Synchronization Barrier
        MCR(p15, 0, reg, c7, c10, 4);
    }
    else
    {
        // Invalidate all cache ways.
        reg = NV_DRF_DEF(PL310, INVALIDATE_BY_WAY, WAY_BITMAP, ALL_WAYS);
        NV_PL310_REGW(INVALIDATE_BY_WAY, reg);

        // Poll for completion.
        while (NV_PL310_REGR(INVALIDATE_BY_WAY) != 0);
    }

    // disable
    reg = NV_DRF_DEF(PL310, CONTROL, ENABLE, DISABLED);
    NV_PL310_REGW(CONTROL, reg);

    // If was enabled, unlock all the cache ways.
    if (is_enabled)
    {
        for (way = 0; way < 8; way++)
        {
            NV_WRITE32(PL310_BASE + PL310_DATA_LOCKDOWN0_0 + (way*4),
                NV_DRF_DEF(PL310, DATA_LOCKDOWN0, WAY_BITMAP, NO_WAYS));
            NV_WRITE32(PL310_BASE + PL310_INSTRUCTION_LOCKDOWN0_0 + (way*4),
                NV_DRF_DEF(PL310, INSTRUCTION_LOCKDOWN0, WAY_BITMAP, NO_WAYS));
        }
    }
}

void
nvaosPl310Writeback( void )
{
    const NvU32 zero = 0;
    NvU32 reg;

    if (!NvAosPl310IsEnabled()) return;

    // write back all ways.
    reg = NV_DRF_DEF(PL310, CLEAN_BY_WAY, WAY_BITMAP, ALL_WAYS);
    NV_PL310_REGW(CLEAN_BY_WAY, reg);

    // Poll for completion.
    do
    {
        reg = NV_PL310_REGR(CLEAN_BY_WAY);
        reg = NV_DRF_VAL(PL310, CLEAN_BY_WAY, WAY_BITMAP, reg);
    }
    while (reg != 0);

    // Data Synchronization Barrier
    MCR(p15, 0, zero, c7, c10, 4);
}

void
nvaosPl310WritebackInvalidate( void )
{
    const NvU32 zero = 0;
    NvU32 reg;

    if (!NvAosPl310IsEnabled()) return;

    // writeback and invalidate all ways.
    reg = NV_DRF_DEF(PL310, CLEAN_AND_INVALIDATE_BY_WAY, WAY_BITMAP, ALL_WAYS);
    NV_PL310_REGW(CLEAN_AND_INVALIDATE_BY_WAY, reg);

    // Poll for completion.
    do
    {
        reg = NV_PL310_REGR(CLEAN_AND_INVALIDATE_BY_WAY);
        reg = NV_DRF_VAL(PL310, CLEAN_AND_INVALIDATE_BY_WAY, WAY_BITMAP, reg);
    }
    while (reg != 0);

    // Data Synchronization Barrier
    MCR(p15, 0, zero, c7, c10, 4);
}

void
nvaosPL310WritebackRange( void *start, NvU32 TotalLength )
{
    NvU32 length;
    NvU32 wAddr;
    const NvU32 zero = 0;

    if (!NvAosPl310IsEnabled()) return;

    length = TotalLength;
    wAddr = (NvU32)start;


    length += wAddr & (PL310_CACHE_LINE_SIZE-1);
    length = (length + (PL310_CACHE_LINE_SIZE-1)) & ~(PL310_CACHE_LINE_SIZE-1);
    wAddr = wAddr & ~(PL310_CACHE_LINE_SIZE-1);

    while (length)
    {
        /* write back the cache line */
        NV_PL310_REGW(CLEAN_LINE_BY_PA, wAddr);

        length = length - PL310_CACHE_LINE_SIZE;
        wAddr  = wAddr + PL310_CACHE_LINE_SIZE;
    }

    // Data Synchronization Barrier
    MCR(p15, 0, zero, c7, c10, 4);
}

void
nvaosPl310WritebackInvalidateRange( void *start, NvU32 TotalLength )
{
    NvU32 length;
    NvU32 wAddr;
    const NvU32 zero = 0;

    if (!NvAosPl310IsEnabled()) return;

    length = TotalLength;
    wAddr = (NvU32)start;

    length += wAddr & (PL310_CACHE_LINE_SIZE-1);
    length = (length + (PL310_CACHE_LINE_SIZE-1)) & ~(PL310_CACHE_LINE_SIZE-1);
    wAddr = wAddr & ~(PL310_CACHE_LINE_SIZE-1);

    while (length)
    {
        /* write back and invalidate the cache line */
        NV_PL310_REGW(CLEAN_AND_INVALIDATE_LINE_BY_PA, wAddr);

        length = length - PL310_CACHE_LINE_SIZE;
        wAddr  = wAddr + PL310_CACHE_LINE_SIZE;
    }

    // Data Synchronization Barrier
    MCR(p15, 0, zero, c7, c10, 4);
}
