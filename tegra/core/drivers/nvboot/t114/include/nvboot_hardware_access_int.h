/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVBOOT_HARDWARE_ACCESS_INT_H
#define INCLUDED_NVBOOT_HARDWARE_ACCESS_INT_H

#include "nvcommon.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

#define NV_DEF_ENVIRONMENT_SUPPORTS_SIM 0

/**
 * NV_WRITE* and NV_READ* - low level read/write api to hardware.
 *
 * These macros should be used to read and write registers and memory
 * in NvDDKs so that the DDK will work on simulation and real hardware
 * with no changes.
 *
 * This is for hardware modules that are NOT behind the host.  Modules that
 * are behind the host should use nvrm_channel.h.
 *
 */

/**
 * NV_WRITE[8|16|32|64] - Writes N data bits to hardware.
 *
 *   @param a The address to write.
 *   @param d The data to write.
 */

/**
 * NV_READ[8|16|32|64] - Reads N bits from hardware.
 *
 * @param a The address to read.
 */

void NvWrite08( void *addr, NvU8 data );
void NvWrite16( void *addr, NvU16 data );
void NvWrite32( void *addr, NvU32 data );
void NvWrite64( void *addr, NvU64 data );
NvU8 NvRead08( void *addr );
NvU16 NvRead16( void *addr );
NvU32 NvRead32( void *addr );
NvU64 NvRead64( void *addr );

#define NV_WRITE08(a,d)     *((volatile NvU8 *)(a)) = (d)
#define NV_WRITE16(a,d)     *((volatile NvU16 *)(a)) = (d)
#define NV_WRITE32(a,d)     *((volatile NvU32 *)(a)) = (d)
#define NV_WRITE64(a,d)     *((volatile NvU64 *)(a)) = (d)
#define NV_READ8(a)         *((const volatile NvU8 *)(a))
#define NV_READ16(a)        *((const volatile NvU16 *)(a))
#define NV_READ32(a)        *((const volatile NvU32 *)(a))
#define NV_READ64(a)        *((const volatile NvU64 *)(a))

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVBOOT_HARDWARE_ACCESS_INT_H
