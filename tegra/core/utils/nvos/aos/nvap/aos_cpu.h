/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_AOS_CPU_H
#define INCLUDED_AOS_CPU_H

#include "nvcommon.h"
#include "aos.h"
#include "cpu.h"

#define NVAOS_CLOCK_DEFAULT 10000
#define NVAOS_STACK_SIZE (1024 * 8)

NvU32 disableInterrupts( void );
void restoreInterrupts( NvU32 oldState );
NV_NAKED void interruptHandler( void );
NV_NAKED void enableInterrupts( void );
NV_NAKED void waitForInterrupt( void );
NV_NAKED void undefHandler( void );
NV_NAKED void crashHandler( void );
NV_NAKED void cpuSetupModes( NvU32 stack );
NV_NAKED void threadSwitch( void );
NV_NAKED void threadSave( void );
NV_NAKED void threadSet( void );
NV_NAKED void initFpu(void);
NV_NAKED void swiHandler(void);
NV_NAKED void nvaosBreakPoint( void );
NV_NAKED void nvaosInvalidateL1DataCache(void);
NV_NAKED void nvaosCpuCacheSetWayOps(NvS32 op);
NV_NAKED void nvaosFlushCache(void);
void nvaosConfigureCache(void);

NvU32 nvaosT30GetOscFreq(NvU32 *dividend, NvU32 *divisor);

void ppiInterruptSetup( void );
void ppiTimerInit( void );
void ppiTimerHandler( void *context );

NvError NvOsPhysicalMemMapping(
    NvOsPhysAddr phys, size_t size,
    NvOsMemAttribute attrib, NvU32 flags, void **ptr);

#endif
