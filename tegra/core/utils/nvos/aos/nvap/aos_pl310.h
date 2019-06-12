/*
 * Copyright (c) 2008-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_AOS_PL310_H
#define INCLUDED_AOS_PL310_H

#include "nvcommon.h"

#if TARGET_SOC_T30
#include "arpl310.h"
#define PL310_BASE              0x50043000  //For T30
#endif

#define PL310_CACHE_LINE_SIZE   32

/* Pl310 is physically tagged and physically index cache. But, in AOS virtual
 * address == physical address,
 * so there is no need to convert to physical addresses, in any of the
 * cache operations.
 */

#if TARGET_SOC_T30
#define NV_PL310_REGR(reg)      AOS_REGR(PL310_BASE + PL310_##reg##_0)
#define NV_PL310_REGW(reg, val) AOS_REGW((PL310_BASE + PL310_##reg##_0), (val))

void nvaosPl310Init(void);
void nvaosPl310Enable(void);
void nvaosPl310Writeback(void);
void nvaosPl310WritebackInvalidate(void);
void nvaosPL310WritebackRange(void *start, NvU32 TotalLength);
void nvaosPl310WritebackInvalidateRange(void *start, NvU32 TotalLength);

#define nvaosPl310Sync() NV_PL310_REGW(CACHE_SYNC, 0)
#else
#define nvaosPl310Init() do {} while (0)
#define nvaosPl310Enable() do {} while(0)
#define nvaosPl310Writeback() do {} while (0)
#define nvaosPl310WritebackInvalidate() do {} while (0)
#define nvaosPL310WritebackRange(start, length) do {} while (0)
#define nvaosPl310WritebackInvalidateRange(start, length) do {} while (0)

#define nvaosPl310Sync() do {} while (0)
#endif

void nvaosPl310InvalidateDisable(void);

#endif // INCLUDED_AOS_PL310_H

