/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_AOS_AVP_H
#define INCLUDED_AOS_AVP_H

#include "nvcommon.h"
#include "aos.h"
#include "avp.h"

#define NVAOS_CLOCK_DEFAULT 5000
#define NVAOS_STACK_SIZE (1024 * 4)

//Some useful defines for the AVP interrupt API
typedef NvU32 IrqSource;

#if !__GNUC__
__asm void interruptHandler( void );
__asm void enableInterrupts( void );
NvU32 disableInterrupts( void );
void restoreInterrupts( NvU32 oldState );
__asm void waitForInterrupt( void );
__asm void undefHandler( void );
__asm void crashHandler( void );
__asm void cpuSetupModes( NvU32 stack );
__asm void threadSwitch( void );
__asm void threadSave( void );
__asm void threadSet( void );
__asm void suspend( void );
__asm void resume( void );
__asm void armSetupModes( NvU32 stack );
#else // !__GNUC__
NV_NAKED void interruptHandler( void );
void enableInterrupts( void );
NvU32 disableInterrupts( void );
void restoreInterrupts( NvU32 oldState );
NV_NAKED void waitForInterrupt( void );
NV_NAKED void undefHandler( void );
NV_NAKED void crashHandler( void );
NV_NAKED void threadSwitch( void );
NV_NAKED void threadSave( void );
NV_NAKED void threadSet( void );
NV_NAKED void suspend( void );
NV_NAKED void resume( void );
NV_NAKED void armSetupModes( NvU32 stack );
#endif // !__GNUC__

void ppiInterruptSetup( void );
void ppiTimerInit( void );
void ppiTimerHandler( void *context );

// FIXME: these names suck
void avpCacheEnable( void );
void restoreIram( void );
void saveIram( void );
NvU32 Src2Idx(IrqSource Irq);
NvU32 NvAvpPlatBitFind(NvU32 Word);
IrqSource IrqSourceFromIrq(NvU32 Irq);
NV_NAKED void StoreTimeStamp(void);
NV_NAKED void initVectors(void);
void NvSwiHandler(int swi_num, int *pRegs);
NV_NAKED void swiHandler(void);

void T30CacheEnable(void);
void T30InstrCacheFlushInvalidate(void);
NvError
T30PhysicalMemMap(NvOsPhysAddr phys,
    size_t size,
    NvOsMemAttribute attrib,
    NvU32 flags,
    void **ptr);
void T30PhysicalMemUnmap(void *ptr, size_t size);
NvU32 measureT30OscFreq(NvU32* dividend, NvU32* divisor);

#endif
