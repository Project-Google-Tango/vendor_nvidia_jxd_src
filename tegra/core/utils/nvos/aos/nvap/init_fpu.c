/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "cpu.h"
#include "aos.h"

#if !__GNUC__
#pragma arm section code = "MainRegion", rwdata = "MainRegion", rodata = "MainRegion", zidata ="MainRegion"
#endif

#if NO_FPU
void
initFpu( void )
{
    return;
}
#else
#if !__GNUC__
NV_NAKED void
initFpu( void )
{
    PRESERVE8

    /* Configure co-processor access based upon VFP/NEON features. */
    mrc    p15, 0, r0, c1, c0, 2               // CPACR
    fmrx   r1, fpsid
    and    r1, r1, #(0xFF<<16)                 // FP Subarchitecture
    cmp    r1, #(3<<16)                        // VFPv3?
    fmrxge r1, mvfr0
    fmrxge r2, mvfr1
    movlt  r1, #0
    movlt  r2, #0
    tst    r1, #(1<<1)                         // 32 x 64-bit registers?
    bicne  r0, r0, #(1<<30)                    // Yes, enable d16-d31
    tst    r2, #(1<<8)                         // SIMD load/store?
    bicne  r0, r0, #(1<<31)                    // Yes, enable Adv. SIMD ops
    mcr    p15, 0, r0, c1, c0, 2               // CPACR

    /* enable the floating point unit */
    mov r0, #( 1 << 30 )
    fmxr fpexc, r0

    /* enable the "run fast" vfp to avoid floating point bugs:
     *  - clear exception bits
     *  - flush to zero enabled
     *  - default NaN enabled
     */
    fmrx r0, fpscr
    orr  r0, r0, #( 1 << 25 ) /* default NaN */
    orr  r0, r0, #( 1 << 24 ) /* flush to zero */
    mov  r1, #0x9F00
    bic r0, r1  /* clear floating point exceptions */
    fmxr fpscr, r0

    /* init the vfp registers */
    mov  r1, #0
    mov  r2, #0
    fmdrr d0, r1, r2
    fmdrr d1, r1, r2
    fmdrr d2, r1, r2
    fmdrr d3, r1, r2
    fmdrr d4, r1, r2
    fmdrr d5, r1, r2
    fmdrr d6, r1, r2
    fmdrr d7, r1, r2
    fmdrr d8, r1, r2
    fmdrr d9, r1, r2
    fmdrr d10, r1, r2
    fmdrr d11, r1, r2
    fmdrr d12, r1, r2
    fmdrr d13, r1, r2
    fmdrr d14, r1, r2
    fmdrr d15, r1, r2

#ifdef ARCH_ARM_HAVE_NEON
    fmrx r0,mvfr0
    tst  r0, #2
    bxeq lr
    fmdrr d16, r1, r2
    fmdrr d17, r1, r2
    fmdrr d18, r1, r2
    fmdrr d19, r1, r2
    fmdrr d20, r1, r2
    fmdrr d21, r1, r2
    fmdrr d22, r1, r2
    fmdrr d23, r1, r2
    fmdrr d24, r1, r2
    fmdrr d25, r1, r2
    fmdrr d26, r1, r2
    fmdrr d27, r1, r2
    fmdrr d28, r1, r2
    fmdrr d29, r1, r2
    fmdrr d30, r1, r2
    fmdrr d31, r1, r2
#endif
    bx lr
}
#endif
#endif

