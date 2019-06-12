/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#if __GNUC__

#include "cpu.h"
#include "aos.h"
#include "nvrm_drf.h"
#include "bootloader.h"
#include "aos_cpu.h"
#include "nvrm_drf.h"
#include "nvbl_arm_cp15.h"
#include "arpg.h"

#define TIMERUS_PA_BASE 0x60005010
extern NvU32 g_BlAvpTimeStamp, g_BlCpuTimeStamp;
extern unsigned int __bss_start__, __bss_end__, __end__;
NvU32 timerusaddr = TIMERUS_PA_BASE;
NvU32 proc_tag = PG_UP_TAG_0_PID_CPU _AND_ 0xFF;

// Can't mark these static because they're only used from asm code
void zInit(void);
NV_NAKED void StoreTimeStamp(void);

/* zInit - Simple function to zero out the bss.
 * We can skip calling libc_init since AOS does
 * everything that is needed.
 */
void zInit(void)
{
    NvU8* cur = (NvU8*)&__bss_start__;
    NvU8* end = (NvU8*)&__bss_end__;

    // Clear any bytes up to the first word boundary.
    while ( (((NvU32)cur) & (sizeof(NvU32) - 1)) && (cur < end) )
    {
        *(cur++) = 0;
    }

    // Do 4-word writes for as long as we can.
    while ((NvUPtr)(end - cur) >= sizeof(NvU32) * 4)
    {
        *((NvU32*)(cur+0)) = 0;
        *((NvU32*)(cur+4)) = 0;
        *((NvU32*)(cur+8)) = 0;
        *((NvU32*)(cur+12)) = 0;
        cur += sizeof(NvU32) * 4;
    }

    // Clear any bytes after the last 4-word boundary.
    while (cur < end)
    {
        *(cur++) = 0;
    }
}

// this function gets the timestamp and updates the respective global variable
// base on the current processor is avp or cpu
NV_NAKED void StoreTimeStamp(void)
{
    asm volatile(
    //;------------------------------------------------------------------
    //; Check current processor: CPU or AVP?
    //; If AVP, go to AVP boot code, else continue on.
    //;------------------------------------------------------------------

    "mov     r0, %2                                                 \n"
    "ldrb    r2, [r0, %3]                                           \n"
     //;are we the CPU?
    "cmp     r2, %4                                                 \n"
    //; yep, we are the CPU
    "beq    StoreCputimeStamp                                       \n"

    // AVP time stamp
    "mov     r0, %5                                                 \n"
    "ldr     r2, [r0, #0]                                           \n"
    "ldr     r0, =g_BlAvpTimeStamp                                      \n"
    "str     r2, [r0]                                               \n"
    "b       Done                                                   \n"

"StoreCputimeStamp:                                                 \n"
    // CPU time stamp
    "mov     r0, %5                                                 \n"
    "ldr     r2, [r0, #0]                                           \n"
    "ldr     r0, =g_BlCpuTimeStamp                                      \n"
    "str     r2, [r0]                                               \n"

"Done:                                                             \n"
    "bx lr                                                          \n"
    :
     "=&r"(g_BlAvpTimeStamp),
     "=&r"(g_BlCpuTimeStamp)
    :
     "I"(PG_UP_PA_BASE),
     "I"(PG_UP_TAG_0),
     "r"(proc_tag),
     "r"(timerusaddr)
    : "r0", "r2",  "cc", "memory"
    );
}

NV_NAKED __attribute__((section (".startsection"))) void _start(void);
NV_NAKED __attribute__((section (".startsection"))) void _start(void)
{
    asm volatile(
    "b AosRealStart                                 \n"

    // Vector code, and assocatiated constants that will be copied to the real
    // vector address.
"ArmV6VectorsStart:                                 \n"
    "ldr     pc, aosRealStartAddr                   \n"
    "ldr     pc, aosUndefAddr                       \n"
    "ldr     pc, aosSwiAddr                         \n"
    "ldr     pc, aosCodeAbortAddr                   \n"
    "ldr     pc, aosDataAbortAddr                   \n"
    "ldr     pc, aosReserveAddr                     \n"
    "ldr     pc, aosIrqAddr                         \n"
    "ldr     pc, aosFiqAddr                         \n"

"aosRealStartAddr:   .word AosRealStart             \n"
"aosUndefAddr:       .word crashHandler             \n"
"aosSwiAddr:         .word swiHandler               \n"
"aosCodeAbortAddr:   .word crashHandler             \n"
"aosDataAbortAddr:   .word crashHandler             \n"
"aosReserveAddr:     .word 0                        \n"
"aosIrqAddr:         .word interruptHandler         \n"
"aosFiqAddr:         .word crashHandler             \n"

#if AOS_MON_MODE_ENABLE
"MonitorVectorsStart:                               \n"
    "ldr     pc, MonitorResetAddr                   \n"
    "ldr     pc, MonitorUndefAddr                   \n"
    "ldr     pc, MonitorSmcAddr                     \n"
    "ldr     pc, MonitorPrefechAbortAddr            \n"
    "ldr     pc, MonitorDataAbortAddr               \n"
    "ldr     pc, MonitorReserveAddr                 \n"
    "ldr     pc, MonitorIrqAddr                     \n"
    "ldr     pc, MonitorFiqAddr                     \n"

"MonitorResetAddr:        .word crashHandler        \n"
"MonitorUndefAddr:        .word crashHandler        \n"
"MonitorSmcAddr:          .word SmcHandler          \n"
"MonitorPrefechAbortAddr: .word crashHandler        \n"
"MonitorDataAbortAddr:    .word crashHandler        \n"
"MonitorReserveAddr:      .word 0                   \n"
"MonitorIrqAddr:          .word interruptHandler    \n"
"MonitorFiqAddr:          .word crashHandler        \n"
#endif

"                    .global SOSKeyIndexOffset      \n"
"SOSKeyIndexOffset:  .word 0                        \n"
"AosRealStart:                                      \n"

    //***********************************************************************
    //* WARNING * WARNING * WARNING * WARNING * WARNING * WARNING * WARNING *
    //***********************************************************************
    //*                                                                     *
    //* DO NOT use ANY ARMv5, ARMv6, or ARMv7 instructions in this code     *
    //* until the decision of whether this is the CPU or AVP has been made. *
    //* You must confine all code until then to ARMv4T instructions.        *
    //*                                                                     *
    //***********************************************************************

    // Make sure we are in system mode with interrupts disabled
    "msr cpsr_fsxc, #0xdf                           \n"

    // Set the stack pointer
    "ldr r0, =__end__                               \n"
    "add sp, r0, #0x400                             \n"

     // Might be cold booting, call a bootloader. This must detect a cold boot
     // and initialize memory and take the CPU out of reset. This must
     // also be safe to call from the CPU.
    "bl zInit                                       \n"
    // get the timestamp for avp and cpu
    "bl StoreTimeStamp                              \n"
    "bl bootloader                                  \n"

    //***********************************************************************
    //*          It is now safe to use non-ARMv4T instructions.             *
    //***********************************************************************

    // Blow away any stale state that may exist in the processor
    "cpsid aif, #0x13                               \n" // supervisor
    "msr spsr_fsxc, #0                              \n"
    "cpsid aif, #0x11                               \n" // FIQ
    "msr spsr_fsxc, #0                              \n"
    "cpsid aif, #0x12                               \n" // IRQ
    "msr spsr_fsxc, #0                              \n"
    "cpsid aif, #0x17                               \n" // abort
    "msr spsr_fsxc, #0                              \n"
    "cpsid aif, #0x1B                               \n" // undefined
    "msr spsr_fsxc, #0                              \n"
    "cpsid aif, #0x1f                               \n" // system

    // Set up system control register
    "mrc  p15, 0, r4, c1, c0, 0                     \n" // Read SCTLR
    "bic  r4, r4, #(1<<12)                          \n" // Disable instruction cache
    "bic  r4, r4, #(1<<11)                          \n" // Disable branch predictor
    "bic  r4, r4, #(1<<2)                           \n" // Disable data cache
    "bic  r4, r4, #(1<<0)                           \n" // disable the mmu
    "mcr  p15, 0, r4, c1, c0, 0                     \n" // Write SCTLR

    "bl nvaosConfigureCache                         \n"

    // Read Chip ID register so we can figure out where to save the vectors
    "ldr r0, =0x70000000                            \n" // ldr r0, =MISC_PA_BASE
    "ldr r12, [r0, #0x804]                          \n" // ldr r12, [r0, #APB_MISC_GP_HIDREV_0]
    "and r1, r12, #0xFF00                           \n" // and r1, r12, #(APB_MISC_GP_HIDREV_0_CHIPID_DEFAULT_MASK << APB_MISC_GP_HIDREV_0_CHIPID_SHIFT)
    "cmp r1, #0x02000                               \n" // T20?

    // Copy vector table entries from bootloader binary to
    // remapped location
    // Remap offset = Kernel offset(0xA00000) - Hi-Vec size(0x1000)
    "ldr r11, =0X9FF000                             \n"
    "addne r11, r11, #0x80000000                    \n" // Add start of SDRAM for all chip except T20

    "mov r1, #0                                     \n" // Assume low vectors
    "tst r4, #(1<<13)                               \n" // SCTLR.V (high vectors)?
    "ldrne r1, =0xFFFF0000                          \n" // Use high vectors if SCTRL.V is set
    // Copy vector table from bootloader binary to high vector (remapped) location
    "ldr r0, =ArmV6VectorsStart                     \n"
    "ldm r0!, {r2-r9}                               \n"
    "stm r11!, {r2-r9}                              \n"
    "ldm r0!, {r2-r9}                               \n"
    "stm r11!, {r2-r9}                              \n"

    // set up MVBAR to point to the start of vector table
    // at this point:
    // r11 points to monitor mode vector table in the vector remap page
    // r1 points to one page after the start of vector table page
    // vector table entry point = r1&0xffff0000 + r11&0x00000fff
#if AOS_MON_MODE_ENABLE
    "ldr r0, =0xffff0000                            \n"
    "and r1, r1, r0                                 \n"
    "mov r12, r11                                   \n"
    "ldr r0, =0x00000fff                            \n"
    "and r12, r12, r0                               \n"
    "orr r1, r1, r12                                \n"
    "mcr p15, 0, r1, c12, c0, 1                     \n"

    // copy monitor mode vector table to the vector remapping page
    "ldr r0, =MonitorVectorsStart                   \n"
    "ldm r0!,  {r2-r9}                              \n"
    "stm r11!, {r2-r9}                              \n"
    "ldm r0!,  {r2-r9}                              \n"
    "stm r11!, {r2-r9}                              \n"
#endif

"done_vectors:                                      \n"
    // Enable co-processor access (floating point will crash without this)
    // Disable access to d16-d31 and NEON until we know we have them.
    "mvn r0, #0                                     \n"
    "mcr p15, 0, r0, c1, c0, 2                      \n" // Write CPACR
    // The following ISB is required for BUG 911874
#ifdef COMPILE_ARCH_V7
    "isb                                            \n"
#else
    ".word  0xF57FF06F                              \n" // !!!DELETEME!!! BUG 830289
#endif
    "bl  initFpu                                    \n"

    "mrc  p15, 0, r1, c1, c0, 0                     \n" // Read SCTLR
    "orr  r1, r1, #(1<<11)                          \n" // Enable branch predictor (SCTLR.Z)
    "orr  r1, r1, #(1<<1)                           \n" // Enable strict alignment (SCTLR.A)
    "mcr  p15, 0, r1, c1, c0, 0                     \n" // Write SCTLR

    // Initialize a basic thread package and interrupt handler
    "bl nvaosInit                                   \n"

    "b main                                         \n"
    :
    :
    );
}

NV_NAKED void initFpu(void)
{
    asm volatile(
    // Configure co-processor access based upon VFP/NEON features.
    "mrc    p15, 0, r0, c1, c0, 2              \n" // CPACR
    "fmrx   r1, fpsid                          \n"
    "and    r1, r1, #(0xFF<<16)                \n" // FP Subarchitecture
    "cmp    r1, #(3<<16)                       \n" // VFPv3?
    "fmrxge r1, mvfr0                          \n"
    "fmrxge r2, mvfr1                          \n"
    "movlt  r1, #0                             \n"
    "movlt  r2, #0                             \n"
    "tst    r1, #(1<<1)                        \n" // 32 x 64-bit registers?
    "bicne  r0, r0, #(1<<30)                   \n" // Yes, enable d16-d31
    "tst    r2, #(1<<8)                        \n" // SIMD load/store?
    "bicne  r0, r0, #(1<<31)                   \n" // Yes, enable Adv. SIMD ops
    "mcr    p15, 0, r0, c1, c0, 2              \n" // CPACR

    // Enable the floating point unit
    "mov r0, #( 1 << 30 )                      \n"
    "fmxr fpexc, r0                            \n"
    // Enable the 'run fast' vfp to avoid floating point bugs:
    // - clear exception bits
    // - flush to zero enabled
    // - default NaN enabled
    "fmrx r0, fpscr                           \n"
    "orr  r0, r0, #( 1 << 25 )                \n" // default NaN
    "orr  r0, r0, #( 1 << 24 )                \n" // flush to zero
    "mov  r1, #0x9F00                         \n" // VFP exception mask
    "bic  r0, r1                              \n" // clear floating point exceptions
    "fmxr fpscr, r0                           \n"
    // Init the vfp registers
    "mov  r1, #0                              \n"
    "mov  r2, #0                              \n"
    "fmdrr d0, r1, r2                         \n"
    "fmdrr d1, r1, r2                         \n"
    "fmdrr d2, r1, r2                         \n"
    "fmdrr d3, r1, r2                         \n"
    "fmdrr d4, r1, r2                         \n"
    "fmdrr d5, r1, r2                         \n"
    "fmdrr d6, r1, r2                         \n"
    "fmdrr d7, r1, r2                         \n"
    "fmdrr d8, r1, r2                         \n"
    "fmdrr d9, r1, r2                         \n"
    "fmdrr d10, r1, r2                        \n"
    "fmdrr d11, r1, r2                        \n"
    "fmdrr d12, r1, r2                        \n"
    "fmdrr d13, r1, r2                        \n"
    "fmdrr d14, r1, r2                        \n"
    "fmdrr d15, r1, r2                        \n"
#ifdef ARCH_ARM_HAVE_NEON
    // This code requires proper compiler options otherwise the compiler will
    // complain about the use of d16-d31 so it can't be determined at run-time.
    "fmrx r0,mvfr0                            \n"
    "tst  r0, #2                              \n"
    "bxeq lr                                  \n"
    "fmdrr d16, r1, r2                        \n"
    "fmdrr d17, r1, r2                        \n"
    "fmdrr d18, r1, r2                        \n"
    "fmdrr d19, r1, r2                        \n"
    "fmdrr d20, r1, r2                        \n"
    "fmdrr d21, r1, r2                        \n"
    "fmdrr d22, r1, r2                        \n"
    "fmdrr d23, r1, r2                        \n"
    "fmdrr d24, r1, r2                        \n"
    "fmdrr d25, r1, r2                        \n"
    "fmdrr d26, r1, r2                        \n"
    "fmdrr d27, r1, r2                        \n"
    "fmdrr d28, r1, r2                        \n"
    "fmdrr d29, r1, r2                        \n"
    "fmdrr d30, r1, r2                        \n"
    "fmdrr d31, r1, r2                        \n"
#endif
    "bx lr                                    \n"
    );
}

// we're hard coding the entry point for all AOS images
NvU32 cpu_boot_stack = NVAP_LIMIT_PA_IRAM_CPU_EARLY_BOOT_STACK;
NvU32 entry_point = NV_AOS_ENTRY_POINT;
NvU32 avp_cache_settings = 0x800;
NvU32 avp_cache_config = 0x50040000;
NvU32 avp_boot_stack = NVAP_LIMIT_PA_IRAM_AVP_EARLY_BOOT_STACK;
NvU32 deadbeef = 0xdeadbeef;

// Following are ARM Erratas
void ARM_ERRATA_743622(void)
{
    asm volatile(
    // (NVIDIA BUG #1212749)
    "mrc     p15, 0, r0, c15, c0, 1       \n"
    "orr     r0, r0, #0x40                \n"
    "mcr     p15, 0, r0, c15, c0, 1       \n"
    );
}

void ARM_ERRATA_751472(void)
{
    asm volatile(
    // (NVIDIA BUG #1212749)
    "mrc     p15, 0, r0, c15, c0, 1       \n"
    "orr     r0, r0, #0x800               \n"
    "mcr     p15, 0, r0, c15, c0, 1       \n"
    );
}

NvU32
disableInterrupts( void )
{
    NvU32 tmp;
    NvU32 intState;

    /* get current irq state */
    asm volatile (
    "mrs %[tmp], cpsr\n"
    :[tmp]"=r" (tmp)
    );

    intState = tmp & (1<<7);  // get the interrupt enable bit
    tmp = tmp | (1<<7);

    /* disable irq */
    asm volatile(
    "msr cpsr_csxf, %[tmp]\n"
    :
    :[tmp]"r" (tmp)
    );

    return intState;
}

void
restoreInterrupts( NvU32 state )
{
    NvU32 tmp;

    /* restore the old cpsr irq state */
    asm volatile (
    "mrs %[tmp], cpsr\n"
    :[tmp]"=r" (tmp)
    );
    tmp = tmp & ~(1<<7);
    tmp = tmp | state;

    asm volatile(
    "msr cpsr_csxf, %[tmp]\n"
    :
    :[tmp]"r" (tmp)
    );

}

NV_NAKED void
enableInterrupts( void )
{
    asm volatile(
    "cpsie i\n"
    "bx lr\n"
    );
}

NV_NAKED void
waitForInterrupt( void )
{
    asm volatile(
#ifdef COMPILE_ARCH_V7
        "dsb\n"
        "wfi\n"
#else
        ".word  0xF57FF04F\n" // !!!DELETEME!!! BUG 830289
        ".word  0xE320F003\n" // !!!DELETEME!!! BUG 830289
#endif
        "bx lr\n"
    );
}

NV_NAKED void
interruptHandler( void )
{
    asm volatile(
    //save state to the system mode's stack
    "sub lr, lr, #4                                     \n"
    "srsdb #0x1f!                                       \n"
    //save the exception address and handle profiler
    //"str lr, [%0]                                       \n"
    //change to system mode and spill some registers
    "msr cpsr_fxsc, #0xdf                               \n"
#ifdef ARCH_ARM_HAVE_NEON
    "vstmdb sp!,  {d0-d15}                              \n"
    "vstmdb sp!,  {d16-d31}                             \n"
#endif
    "stmfd sp!, {r0-r12,lr}                             \n"
    //save the current thread's stack pointer
    "bl threadSave                                      \n"
    //switch back to irq mode
    "msr cpsr_fxsc, #0xd2                               \n"

    //"bl nvaosProfTimerHandler                           \n"
    //call the interrupt handler for the given interrupt
    "bl nvaosDecodeIrqAndDispatch                       \n"
    //interrupts are cleared at the source (timer handler, etc)
    //switch to system mode, set the thread, and unspill regs
    "msr cpsr_fxsc, #0xdf                               \n"
    //set the current thread state
    "bl threadSet                                       \n"
    //spill the next thread's registers
    "ldmfd sp!, {r0-r12, lr}                            \n"
#ifdef ARCH_ARM_HAVE_NEON
    "vldm sp!, {d16-d31}                                \n"
    "vldm sp!, {d0-d15}                                 \n"
#endif
    //return from exception
    "rfefd sp!                                          \n"
    :
    ://"r"(s_ExceptionAddr)
    :"lr"
    );
}

NV_NAKED void
crashHandler( void )
{
    asm volatile("b .\n");
}

NV_NAKED void swiHandler(void)
{
    /* don't do anything */
    asm volatile("movs pc, r14\n");
}

NV_NAKED void
undefHandler( void )
{
    /* an interrupt was raised without a handler */
    asm volatile("b .\n");
}

NV_NAKED void
cpuSetupModes( NvU32 stack )
{
    asm volatile(
    //svc mode
    "msr cpsr_fsxc, #0xd3                              \n"
    "mov sp, r0                                        \n"

    //abt mode
    "msr cpsr_fsxc, #0xd7                              \n"
    "mov sp, r0                                        \n"

    //undef mode
    "msr cpsr_fsxc, #0xdb                              \n"
    "mov sp, r0                                        \n"

    //irq mode
    "msr cpsr_fsxc, #0xd2                              \n"
    "mov sp, r0                                        \n"

    //system mode
    "msr cpsr_fsxc, #0xdf                              \n"
    "bx lr\n"
    );
}

/* save the current thread's stack pointer
 */
NV_NAKED void
threadSave( void )
{
    asm volatile(

    //set the stack pointer
    "str sp, [%0]              \n"
    "bx lr\n"
    :
    :"r"(g_CurrentThread)
    :"memory"
    );
}

/* restore the current thread's stack pointer
 */
NV_NAKED void
threadSet( void )
{
    asm volatile(
    "ldr r0, [%0]  \n"
    //save the stack pointer, which is the first thing in the thread struct
    "mov sp, r0              \n"
    "bx lr\n"
    :
    :"r"(g_CurrentThread)
    :"r0"
    );
}

NV_NAKED void
threadSwitch( void )
{
    asm volatile(
    //save some registers for scratch space
    "stmfd sp!, {r10-r11}          \n"

    //save processor state
    "mrs r11, cpsr                 \n"

    //make sure interrupts are disabled
    "cpsid i                       \n"

    //enable interrupt bits in the cpsr
    "bic r11, r11, #(1<<6)         \n"
    "bic r11, r11, #(1<<7)         \n"

    //spill thread state:
    //stack is return address, cpsr, vfp, r0-12, lr
    //"add r10, pc, #24              \n"
    "ldr r10, =thread_switch_exit\n"
    "stmfd sp!, {r10-r11}          \n"
#ifdef ARCH_ARM_HAVE_NEON
    "vstmdb sp!,  {d0-d15}         \n"
    "vstmdb sp!,  {d16-d31}        \n"
#endif
    "stmfd sp!, {r0-r12,lr}        \n"

    //save the current thread's stack pointer
    "bl threadSave                 \n"

    //run the scheduler
    "bl nvaosScheduler             \n"

    //set the thread stack
    "bl threadSet                  \n"

    //unspill thread registers
    "ldmfd sp!, {r0-r12,lr}        \n"
#ifdef ARCH_ARM_HAVE_NEON
    "vldm sp!, {d16-d31}           \n"
    "vldm sp!, {d0-d15}            \n"
#endif
    "rfefd sp!                     \n"

    "thread_switch_exit:           \n"
    //restore registers
    "ldmfd sp!, {r10-r11}          \n"
    "bx lr\n"
    );
}

/*
 * GCC library interface for AOS.
 */
int raise(int __sig);
int raise(int __sig)
{
    NvOsDebugPrintf("Signal %d raised!\r\n", __sig);
    NvOsBreakPoint(__FILE__, __LINE__, NULL);
    return __sig;
}

#endif //__GNUC__
