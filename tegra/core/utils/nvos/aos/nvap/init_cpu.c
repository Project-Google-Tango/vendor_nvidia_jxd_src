/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "cpu.h"
#include "aos.h"
#include "aos_common.h"
#include "nvrm_drf.h"

#if !__GNUC__
#pragma arm
#pragma arm section code = "MainRegion", rwdata = "MainRegion", rodata = "MainRegion", zidata ="MainRegion"
/* linker will put this at the end of the program, hopefully. see below. */
extern unsigned int __end__;
#endif

extern int main( int argc, char *argv[] );

extern void initFpu(void);
extern void nvaosConfigureCache(void);

#if !__GNUC__
NV_NAKED void _start(void)
{
    PRESERVE8
    IMPORT __end__
    IMPORT __scatterload
    IMPORT bootloader
    IMPORT nvaosInit
    IMPORT interruptHandler
    IMPORT swiHandler
    IMPORT crashHandler
    IMPORT initFpu
    IMPORT nvaosConfigureCache
    IMPORT SmcHandler

    b AosRealStart

    /* vector code, and assocatiated constants that will be copied to the real
     * vector address.
     */
ArmV6VectorsStart
    ldr     pc, aosRealStartAddr
    ldr     pc, aosUndefAddr
    ldr     pc, aosSwiAddr
    ldr     pc, aosCodeAbortAddr
    ldr     pc, aosDataAbortAddr
    ldr     pc, aosReserveAddr
    ldr     pc, aosIrqAddr
    ldr     pc, aosFiqAddr

aosRealStartAddr   DCD     AosRealStart
aosUndefAddr       DCD     crashHandler
aosSwiAddr         DCD     swiHandler
aosCodeAbortAddr   DCD     crashHandler
aosDataAbortAddr   DCD     crashHandler
aosReserveAddr     DCD     0
aosIrqAddr         DCD     interruptHandler
aosFiqAddr         DCD     crashHandler

#if AOS_MON_MODE_ENABLE
MonitorVectorsStart
    ldr     pc, MonitorResetAddr
    ldr     pc, MonitorUndefAddr
    ldr     pc, MonitorSmcAddr
    ldr     pc, MonitorPrefechAbortAddr
    ldr     pc, MonitorDataAbortAddr
    ldr     pc, MonitorReserveAddr
    ldr     pc, MonitorIrqAddr
    ldr     pc, MonitorFiqAddr

MonitorResetAddr           DCD     crashHandler
MonitorUndefAddr           DCD     crashHandler
MonitorSmcAddr             DCD     SmcHandler
MonitorPrefechAbortAddr    DCD     crashHandler
MonitorDataAbortAddr       DCD     crashHandler
MonitorReserveAddr         DCD     0
MonitorIrqAddr             DCD     interruptHandler
MonitorFiqAddr             DCD     crashHandler
#endif

AosRealStart

    /***********************************************************************/
    /* WARNING * WARNING * WARNING * WARNING * WARNING * WARNING * WARNING */
    /***********************************************************************/
    /*                                                                     */
    /* DO NOT use ANY ARMv5, ARMv6, or ARMv7 instructions in this code     */
    /* until the decision of whether this is the CPU or AVP has been made. */
    /* You must confine all code until then to ARMv4T instructions.        */
    /*                                                                     */
    /***********************************************************************/

    /* Make sure we are in system mode with interrupts disabled */
    msr cpsr_fsxc, #( MODE_DISABLE_INTR | MODE_SYS )

    /* Set the stack pointer */
    ldr r0, =__end__
    add sp, r0, #0x400

    /* Might be cold booting, call a bootloader. This must detect a cold boot
     * and initialize memory and take the CPU out of reset. This must
     * also be safe to call from the CPU.
     */
    bl bootloader

    /***********************************************************************/
    /*          It is now safe to use non-ARMv4T instructions.             */
    /***********************************************************************/

    /* Blow away any stale state that may exist in the processor */
    cpsid aif, #MODE_SVC
    msr spsr_fsxc, #0
    cpsid aif, #MODE_FIQ
    msr spsr_fsxc, #0
    cpsid aif, #MODE_IRQ
    msr spsr_fsxc, #0
    cpsid aif, #MODE_ABT
    msr spsr_fsxc, #0
    cpsid aif, #MODE_UND
    msr spsr_fsxc, #0
    cpsid aif, #MODE_SYS

    mrc  p15, 0, r4, c1, c0, 0  /* Read SCTLR */
    bic  r1, r4, #SCTLR_I       /* Disable instruction cache */
    bic  r1, r4, #SCTLR_Z       /* Disable branch predictor */
    bic  r1, r4, #SCTLR_C       /* Disable data cache */
    bic  r1, r4, #SCTLR_M       /* disable the mmu */
    mcr  p15, 0, r4, c1, c0, 0  /* Write SCTLR */

    /* invalidate d-cache, enable i-cache, SMP. Does not enable d-cache */
    bl nvaosConfigureCache

    /* For ARMv7, we have to copy the entire 4KB page from 0xVVVV:0000 where
     * the vectors are, to where they will be remapped into memory. VVVV will
     * be 0xFFFF for high vectors (SCTLR.V set) or 0x0000 for low vectors
     * (SCTLR.V clear). This is a huge hack for now, once AOS supports an mmap
     * type interface, this code can be removed.
     */

    ldr r11, =_start
    ldr r0, =0xffffff
    bic r11, r11, r0
    ldr r1, =VECTOR_RESET
    add r11, r11, r1
    mov r0, r11
    mov r1, #0                  /* Assume low vectors */
    tst r4, #SCTLR_V            /* SCTLR.V (high vectors)? */
    ldrne r1, =0xFFFF0000       /* Use high vectors */
    mov r2, #32

copyloop
    /* each of thse ldm/stm pairs does 8 4-byte registers.  32-bytes a shot.
     * loop is unrolled 4 times, for 128 bytes.  4096/128=32
     */
    ldm r1!, {r3-r10}
    stm r0!, {r3-r10}
    ldm r1!, {r3-r10}
    stm r0!, {r3-r10}
    ldm r1!, {r3-r10}
    stm r0!, {r3-r10}
    ldm r1!, {r3-r10}
    stm r0!, {r3-r10}
    adds r2, r2, #-1
    bne copyloop

    ldr r0, =ArmV6VectorsStart
    ldm r0!,  {r2-r9}
    stm r11!, {r2-r9}
    ldm r0!,  {r2-r9}
    stm r11!, {r2-r9}

    // set up MVBAR to point to the start of vector table
    // at this point:
    // r11 points to monitor mode vector table in the vector remap page
    // r1 points to one page after the start of vector table page
#if AOS_MON_MODE_ENABLE
    // vector table entry point = r1&0xffff0000 + r11&0x00000fff
    ldr r0, =0xffff0000
    and r1, r1, r0
    mov r12, r11
    ldr r0, =0x00000fff
    and r12, r12, r0
    orr r1, r1, r12
    mcr p15, 0, r1, c12, c0, 1

    // copy monitor mode vector table to the vector remapping page
    ldr r0, =MonitorVectorsStart
    ldm r0!,  {r2-r9}
    stm r11!, {r2-r9}
    ldm r0!,  {r2-r9}
    stm r11!, {r2-r9}
#endif

done_vectors
    /* enable co-processor access (floating point will crash without this) */
    mvn r0, #0
    mcr p15, 0, r0, c1, c0, 2  /* Write CPACR */
    /* The following ISB is required for BUG 911874 */
#ifdef COMPILE_ARCH_V7
    isb
#else
    DCD     0xF57FF06F                      // !!!DELETEME!!! BUG 830289
#endif
    bl  initFpu

    mrc  p15, 0, r1, c1, c0, 0 /* Read SCTLR */
    orr  r1, r1, #SCTLR_Z      /* Enable branch predictor */
    orr  r1, r1, #SCTLR_A      /* Turn on strict alignment checking */
    mcr  p15, 0, r1, c1, c0, 0 /* Write SCTLR */

    /* intialize a basic thread package and interrupt handler */
    bl nvaosInit

    /*
     * __scatterload  handles the zero initialization of the bss and then
     * jumps to __rt_entry.
     *
     * __rt_entry handles the stdlib init and then jumps to main.
     */

    ldr  r1, =__scatterload
    bx   r1
}

#pragma arm section code, rwdata, rodata, zidata

#pragma arm section code = "EndRegion", rwdata = "EndRegion", rodata = "EndRegion", zidata ="EndRegion"

/* There seems to be a bug in the ARM linker for the end of region symbols
 * that are emitted from the linker.  By placing a symbol at the end of
 * the program region, we can manually get the address of the end of the
 * program.  The scatter file will place this region after the program region.
 */
unsigned int __end__;

#pragma arm section code, rwdata, rodata, zidata
#endif // !__GNUC__
