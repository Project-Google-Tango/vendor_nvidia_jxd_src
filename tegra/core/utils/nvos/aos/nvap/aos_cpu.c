/*
 * Copyright (c) 2007-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvaboot.h"
#include "nvrm_drf.h"
#include "nvrm_init.h"
#include "nvrm_arm_cp.h"
#include "nvrm_heap.h"
#include "aos_common.h"
#include "aos.h"
#include "aos_ap.h"
#include "armc.h"
#include "nvap/aos_pl310.h"
#include "t11x/areic_proc_if.h"
#include "t11x/arsysctr0.h"
#include "artimer.h"
#include "artimerus.h"
#include "arclk_rst.h"
#include "arapb_misc.h"
#include "arfic_dist.h"
#include "arfic_proc_if.h"
#include "t30/armselect.h"
#include "t30/arflow_ctlr.h"
#include "nvodm_query.h"
#include "nvbl_arm_processor.h"
#include "nvbl_arm_cp15.h"
#include "nvintr.h"
#include <stdlib.h>
#include <string.h>
#if !__GNUC__
#include <rt_misc.h>
#endif
#include "aos.h"
#include "aos_cpu.h"
#include "aos_mon_mode.h"
#include "aos_ns_mode.h"

/* following are bit positions in SCTLR */
#define TEX_BIT0  6
#define CACHE_EN  3
#define BUFFER_EN 2
#define TRE_EN    28

#define SMALL_PAGE_BIT  1 /* the position of page size in PDE */
#define SHARABLE_BIT   10

/* this defines a mask for memory attribute bits */
#define MEM_ATTRIB_MASK ((1<<TEX_BIT0) | (1<<CACHE_EN) | (1<<BUFFER_EN))

/* this corresponds to enum NvOsMemAttribute */
#define UNCACHED        (0<<TEX_BIT0|0<<CACHE_EN|0<<BUFFER_EN)
#define WRITEBACK       (1<<TEX_BIT0|1<<CACHE_EN|1<<BUFFER_EN)
#define WRITECOMBINE    (1<<BUFFER_EN)
#define INNERWRITEBACK  (1<<TEX_BIT0|1<<BUFFER_EN)

extern unsigned int __end__;

NvOsInterruptHandler s_InterruptHandlers[NVAOS_MAX_HANDLERS];
NvBool s_InterruptHandlersValid[NVAOS_MAX_HANDLERS];
NvU32 s_InterruptHandlerId[NVAOS_MAX_HANDLERS];
void *s_InterruptContext[NVAOS_MAX_HANDLERS];
NvU32 s_IsIsr;

// smaller stack for irq stuff
static NvU32 s_IrqStack[2048 / sizeof(NvU32)];

#if !__GNUC__
static NvU32 s_LibcStack[NVAOS_STACK_SIZE / sizeof(NvU32)];
#endif

/*  Adds a new virtual->physical DRAM mapping range into the page tables.
 *  BottomOfMem, TopOfMem and Virt must be megabyte aligned.  Entire range
 *  is mapped linearly.  Optionally cleans the data cache after writing
 *  page tables and invalidates the TLB to ensure coherency.  Mappings
 *  can be cached (outer/inner WBWA) or uncached (shared device).
 */
enum
{
    AosPte_Dram = 0,
    AosPte_LastMeg,
    AosPte_Num
};

static NvU32  memAttrib[5]={UNCACHED, WRITEBACK, WRITECOMBINE, INNERWRITEBACK, UNCACHED};

static NvU32 *FirstLevelPageTable = NULL;
static NvU32 *SecondLevelPageTable[AosPte_Num] = { NULL };
static NvUPtr BottomOfDram;

static NvUPtr nvaos_GetProgramEndAddress(void)
{
    return ((NvUPtr)&__end__) + sizeof(unsigned int);
}

// don't let this get into the .bss
MemoryMap s_MemoryMap = { 0xa5, 0xa5, 0xa5, 0xa5 };

NvUPtr nvaos_GetMemoryStart( void );
NvUPtr nvaos_GetMemoryStart( void )
{
    return s_MemoryMap.ext_start;
}

// this isn't needed yet, but keep it here for fun
NvUPtr nvaos_GetMemoryEnd( void );
NvUPtr nvaos_GetMemoryEnd( void )
{
    return s_MemoryMap.ext_end;
}

NvU32 *nvaos_GetFirstLevelPageTableAddr(void)
{
    return FirstLevelPageTable;
}

// needed by Linux bootloaders to properly specify the ram regions
// surrounding carveout, on systems with large memory configs
NvU32 nvaos_GetMaxMemorySize( void );
NvU32 nvaos_GetMaxMemorySize( void )
{
    return s_MemoryMap.ext_end - s_MemoryMap.ext_start;
}

static NvUPtr nvaos_GetMmioStart( void )
{
    return s_MemoryMap.mmio_start;
}

static NvUPtr nvaos_GetMmioEnd( void )
{
    return s_MemoryMap.mmio_end;
}

static NvU32 s_CpuCacheLineSize = 0;
static NvU32 s_CpuCacheLineMask = 0;

#define AOS_ALIGN(X, A)     (((X) + ((A)-1)) & ~((A)-1))
#define AOS_END_ADDR        (nvaos_GetProgramEndAddress())
#define AOS_END_OFFS        ((AOS_END_ADDR) - (nvaos_GetMemoryStart()))
#define AOS_END_SAFE(X,Y)   (AOS_ALIGN(AOS_END_##X,(Y)) + (Y))

// bottom of initial dynamic allocation region
#define AOS_HI_VEC_REMAP_SIZE (0x1000)
#define AOS_INIT_ALLOC_BOTTOM (AOS_END_SAFE(ADDR, s_CpuCacheLineSize))
#define AOS_INIT_ALLOC_SIZE (nvaos_GetMemoryStart() + ZIMAGE_START_ADDR - AOS_INIT_ALLOC_BOTTOM - AOS_HI_VEC_REMAP_SIZE)
#define AOS_HI_VEC_REMAP_BOTTOM (AOS_INIT_ALLOC_BOTTOM + AOS_INIT_ALLOC_SIZE)
#define AOS_RESERVE_FOR_KERNEL_IMAGE_BOTTOM (AOS_HI_VEC_REMAP_BOTTOM + AOS_HI_VEC_REMAP_SIZE)
#define AOS_RESERVE_FOR_RAMDISK_IMAGE_BOTTOM (AOS_RESERVE_FOR_KERNEL_IMAGE_BOTTOM + AOS_RESERVE_FOR_KERNEL_IMAGE_SIZE)
#define AOS_REMAIN_ALLOC_BOTTOM (AOS_RESERVE_FOR_RAMDISK_IMAGE_BOTTOM + RAMDISK_MAX_SIZE + DTB_MAX_SIZE)

Allocator s_InitAllocator = { 0 };
Allocator s_RemainAllocator = { 0 };
Allocator s_UncachedAllocator = { 0 };
Allocator s_WriteCombinedAllocator = { 0 };
Allocator s_SecuredAllocator = { 0 };
static Allocator *s_Allocators[] = { &s_InitAllocator, &s_RemainAllocator, NULL };
Allocator *s_UncachedAllocators[] = { &s_UncachedAllocator, NULL };
Allocator *s_WriteCombinedAllocators[] = { &s_WriteCombinedAllocator, NULL };
Allocator *s_SecuredAllocators[] = { &s_SecuredAllocator, NULL };

NvOsInterruptHandler gMainHndlr = NULL;

/* for profiling */
NvU32 s_ExceptionAddr;

NvAosChip s_Chip         = NvAosChip_T30;
NvBool s_IsCortexA9      = 0xa5;  // set to non-zero so this won't be in .bss
NvBool s_QuickTurnEmul   = 0xa5;  // set to non-zero so this won't be in .bss
NvBool s_UsePL310L2Cache = 0xa5;  // set to non-zero so this won't be in .bss
NvBool s_UseLPAE         = 0xa5;  // set to non-zero so this won't be in .bss
NvBool s_Simulation      = 0xa5;  // set to non-zero so this won't be in .bss
NvBool s_FpgaEmulation   = 0xa5;  // set to non-zero so this won't be in .bss
NvU32  s_Netlist         = 0xa5;  // set to non-zero so this won't be in .bss
NvU32  s_NetlistPatch    = 0xa5;  // set to non-zero so this won't be in .bss
NvBlCpuClusterId s_ClusterId = NvBlCpuClusterId_G;

// 64bit usec counter
static NvU64 s_usec = 0;

#if NVOS_TRACE || NV_DEBUG
#undef NvOsPhysicalMemMap
#undef NvOsPhysicalMemUnmap
#undef NvOsPageAlloc
#undef NvOsPageFree
#undef NvOsPageMap
#undef NvOsPageMapIntoPtr
#undef NvOsPageUnmap
#undef NvOsPageAddress
#endif

static void nvaos_TimerHandler( void *context );

#if !__GNUC__

void $Sub$$_fp_init(void)
{
    /* AOS doesn't support FPU yet */
    return;
}

NvU32
disableInterrupts( void )
{
    NvU32 tmp;
    NvU32 intState;

    /* get current irq state */
    __asm("mrs tmp, cpsr");

    intState = tmp & (1<<7);  // get the interrupt enable bit
    tmp = tmp | (1<<7);

    /* disable irq */
    __asm("msr cpsr_csxf, tmp");
    return intState;
}

void
restoreInterrupts( NvU32 state )
{
    NvU32 tmp;

    /* restore the old cpsr irq state */
    __asm("mrs tmp, cpsr");
    tmp = tmp & ~(1<<7);
    tmp = tmp | state;
    __asm("msr cpsr_csxf, tmp");
}

__asm void
enableInterrupts( void )
{
    cpsie i
    bx lr
}

__asm void
waitForInterrupt( void )
{
    wfi
    bx lr
}

__asm void
interruptHandler( void )
{
    PRESERVE8
    IMPORT g_CurrentThread
    IMPORT s_ExceptionAddr
    IMPORT nvaosProfTimerHandler
    IMPORT nvaosDecodeIrqAndDispatch

    /* save state to the system mode's stack */
    sub lr, lr, #4
    srsfd #0x1f!

    /* change to system mode and spill some registers */
    msr cpsr_fxsc, #( MODE_DISABLE_INTR | MODE_SYS )
    stmfd sp!, {r0-r12,lr}

    /* save the current thread's stack pointer */
    bl threadSave

    /* switch back to irq mode */
    msr cpsr_fxsc, #( MODE_DISABLE_INTR | MODE_IRQ )

    /* save the exception address and handle profiler */
    ldr r0, =s_ExceptionAddr
    str lr, [r0]
    bl nvaosProfTimerHandler

    /* call the interrupt handler for the given interrupt */
    bl nvaosDecodeIrqAndDispatch

    /* interrupts are cleared at the source (timer handler, etc) */

    /* switch to system mode, set the thread, and unspill regs */
    msr cpsr_fxsc, #( MODE_DISABLE_INTR | MODE_SYS )

    /* set the current thread state */
    bl threadSet

    /* spill the next thread's registers */
    ldmfd sp!, {r0-r12,lr}

    /* return from exception */
    rfefd sp!
}

__asm void
crashHandler( void )
{
    /* something bad happened */
crash
    b crash
}

__asm void
swiHandler( void )
{
    /* don't do anything */
    movs pc, r14
}

__asm void
undefHandler( void )
{
    /* an interrupt was raised without a handler */
hangsystem
    b hangsystem
}

/* save the current thread's stack pointer
 */
__asm void
threadSave( void )
{
    IMPORT g_CurrentThread

    ldr r0, =g_CurrentThread
    ldr r0, [r0]

    /* save the stack pointer, which is the first thing in the thread struct */
    str sp, [r0]
    bx lr
}

/* restore the current thread's stack pointer
 */
__asm void
threadSet( void )
{
    IMPORT g_CurrentThread

    ldr r0, =g_CurrentThread
    ldr r0, [r0]

    /* set the stack pointer */
    ldr sp, [r0]
    bx lr
}

__asm void
threadSwitch( void )
{
    PRESERVE8
    IMPORT nvaosScheduler

    /* save some registers for scratch space */
    stmfd sp!, {r10-r11}

    /* save processor state */
    mrs r11, cpsr

    /* make sure interrupts are disabled */
    cpsid i

    /* enable interrupt bits in the cpsr */
    bic r11, r11, #(1<<6)
    bic r11, r11, #(1<<7)

    /* spill thread state:
     * stack is return address, cpsr, r0-12, lr
     */
    ldr r10, =thread_switch_exit
    stmfd sp!, {r10-r11}
    stmfd sp!, {r0-r12,lr}

    /* save the current thread's stack pointer */
    bl threadSave

    /* run the scheduler */
    bl nvaosScheduler

    /* set the thread stack */
    bl threadSet

    /* unspill thread registers */
    ldmfd sp!, {r0-r12,lr}
    rfefd sp!

thread_switch_exit
    /* restore registers */
    ldmfd sp!, {r10-r11}
    bx lr
}

#endif //__GNUC__

NvUPtr nvaos_GetTZMemoryStart( void );
NvUPtr nvaos_GetTZMemoryStart( void )
{
    return (NvUPtr)s_SecuredAllocator.base;
}

NvUPtr nvaos_GetTZMemoryEnd( void );
NvUPtr nvaos_GetTZMemoryEnd( void )
{
    return (NvUPtr)(s_SecuredAllocator.base + s_SecuredAllocator.size);
}

void nvaosSuspend( void )
{
    // FIXME: implement?
}

/**
 * flag for interrupt service time for semaphore implementation.
 */
NvU32 nvaosIsIsr( void )
{
    return s_IsIsr;
}

/**
 * returns the last exception exception address
 */
NvU32
nvaosGetExceptionAddr( void )
{
    return s_ExceptionAddr;
}
NvU32
nvaosReadPmcScratch( void )
{
    return nvaosGetOdmData();
}

#define ARM_VERSION_CORTEX_A15 0xC0F

void
nvaosHaltCpu(void)
{
#if defined(__GNUC__)
    NvU32 midr;

#ifdef ANDROID
#define _ARM_DATA_AND_INSTRUCTION_BARRIER \
        asm volatile("dsb"); \
        asm volatile("isb")
#else
        /* Current L4T toolchain doesn't understand the instructions */
#if defined(__THUMBEL__) || defined(__THUMBEB__)
#define _ARM_DATA_AND_INSTRUCTION_BARRIER \
        asm volatile(".hword 0xF3BF,0X8F4F"); /* dsb */ \
        asm volatile(".hword 0xF3BF,0x8F6F")  /* isb */
#else
#define _ARM_DATA_AND_INSTRUCTION_BARRIER \
        asm volatile(".word  0xF57FF04F");    /* dsb */ \
        asm volatile(".word  0xF57FF06F")     /* isb */
#endif
#endif

    asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r" (midr));
    midr = (midr & 0x0000FFF0) >> 4;
    if (midr == ARM_VERSION_CORTEX_A15)
    {
        *(volatile NvU32 *)0x70050FB0 = 0xC5ACCE55;
        _ARM_DATA_AND_INSTRUCTION_BARRIER;
        *(volatile NvU32 *)0x70050090 = 0x1;
        _ARM_DATA_AND_INSTRUCTION_BARRIER;
    }
    else
    {
        asm volatile("mcr p14, 0, %0, c0, c4, 2" :: "r"(1));
        _ARM_DATA_AND_INSTRUCTION_BARRIER;
    }

#undef _ARM_DATA_AND_INSTRUCTION_BARRIER
#endif
}

NvU32
nvaosGetStackSize( void )
{
    return NVAOS_STACK_SIZE;
}

void
nvaosGetMemoryMap( MemoryMap *map )
{
    map->ext_start = TXX_EXT_MEM_START;
    map->ext_end = TXX_EXT_MEM_END;
    map->mmio_start = TXX_MMIO_START;
    map->mmio_end = TXX_MMIO_END;
}

/**
 * calls the interrupt handler for the current interrupt.
 */
void
nvaosDecodeIrqAndDispatch( void )
{
    /* flag this as isr time for semaphore implementation */
    s_IsIsr = 1;

    if(gMainHndlr)
        (gMainHndlr)( 0 );

    s_IsIsr = 0;
}

void
nvaosThreadStackInit( NvOsThreadHandle thread, NvU32 *stackBase, NvU32
    words, NvBool exec )
{
    NvU32 *stack;
    NvU32 tmp;

    /* setup the thread stack, including the start function address */
    /* read the cpsr - need to put it on the stack before the pc */
    GET_CPSR(tmp);

    /* need to manually enable interrupts (bits 6 and 7 in the cpsr) - 0
     * or enable, 1 to disable.
     */
    tmp &= ~( 3 << 6 );

    stack = stackBase + words;
#ifdef ARCH_ARM_HAVE_NEON
    /* the initial stack is r0-r12, lr, vfp, pc, cpsr */
    /* vfp: 32 double words == 64 words, 64 + 16 = 80 */
    stack -= 80;
    /* clear registers r0-r12, lr, vfp */
    NvOsMemset( (unsigned char *)stack, 0, 77 * 4 );
#else
    /* the initial stack is r0-r12, lr, pc, cpsr */
    stack -= 16;
    /* clear registers r0-r12, lr */
    NvOsMemset( (unsigned char *)stack, 0, 13 * 4 );
#endif
    stack += 13;
    /* set the lr, pc, and cpsr */
    stack[0] = (NvU32)nvaosThreadKill;
    if( thread->wrap )
    {
#ifdef ARCH_ARM_HAVE_NEON
        stack[65] = (NvU32)thread->wrap;
#else
        stack[1] = (NvU32)thread->wrap;
#endif
    }
    else
    {
#ifdef ARCH_ARM_HAVE_NEON
        stack[65] = (NvU32)thread->function;
#else
        stack[1] = (NvU32)thread->function;
#endif
    }
#ifdef ARCH_ARM_HAVE_NEON
    stack[66] = tmp;
#else
    stack[2] = tmp;
#endif
    stack -= 13;

    /* set r0 to the thread arg */
    stack[0] = (NvU32)(thread->args);

    thread->stack = (unsigned char *)stackBase;
    thread->sp = (NvU32)stack;

    if( exec )
    {
        /* set the stack pointer */
        threadSet();

        /* execute the thread */
        if( thread->wrap )
        {
            thread->wrap( thread->args );
        }
        else
        {
            thread->function( thread->args );
        }
    }
}

void nvaosEnableInterrupts( void )
{
    enableInterrupts();
}

NvU32 nvaosDisableInterrupts( void )
{
    return disableInterrupts();
}

void nvaosRestoreInterrupts( NvU32 state )
{
    restoreInterrupts(state);
}

void nvaosWaitForInterrupt( void )
{
    waitForInterrupt();
}

void nvaosThreadSwitch( void )
{
    threadSwitch();
}

/** assumes that interrupts are disabled */
void nvaosTimerInit( void )
{
    NvU32 id;
    NvU32 dividend, divisor;
    NvU32 reg;

    switch( s_Chip ) {
    case NvAosChip_T30: id = 0x30; break;
    case NvAosChip_T114: id = 0x35; break;
    case NvAosChip_T124 : id = 0x40; break;
    default: id = 0x30; break;
    }

    NvIntrInit( id, (NvOsInterruptHandler*)(&gMainHndlr),
        (NvOsInterruptHandler)(nvaos_TimerHandler));

    /* must make sure 64bit variable is init to 0. Software keeping
     * track of upper 32bit works only if timer interrupt handling
     * occurs before lower 32bit overflows >1 time.
     */
    s_usec = 0;
    (void)nvaosT30GetOscFreq(&dividend, &divisor);

    reg = NV_DRF_NUM( TIMERUS, USEC_CFG, USEC_DIVIDEND, dividend )
        | NV_DRF_NUM( TIMERUS, USEC_CFG, USEC_DIVISOR, divisor );
    AOS_REGW( TIMERUS_PA_BASE + TIMERUS_USEC_CFG_0, reg );

    /* start the timer - one shot */
    reg = NV_DRF_DEF( TIMER, TMR_PTV, EN, ENABLE )
        | NV_DRF_NUM( TIMER, TMR_PTV, TMR_PTV, NVAOS_CLOCK_DEFAULT );
    AOS_REGW( TIMER0_PA_BASE, reg );

    // Freeze timers when in debug state.
    reg = AOS_REGR(TIMERUS_PA_BASE + TIMERUS_CNTR_FREEZE_0);
    reg = NV_FLD_SET_DRF_NUM(TIMERUS, CNTR_FREEZE, DBG_FREEZE_CPU0, 0, reg);
    AOS_REGW(TIMERUS_PA_BASE + TIMERUS_CNTR_FREEZE_0, reg);
}

static void
nvaos_TimerHandler( void *context )
{
    NvU32 reg;

    /* write a 1 to the intr_clr field to clear the interrupt */
    reg = NV_DRF_NUM( TIMER, TMR_PCR, INTR_CLR, 1 );
    AOS_REGW( TIMER0_PA_BASE + TIMER_TMR_PCR_0, reg );

    /* reset the timer */
    reg = NV_DRF_DEF( TIMER, TMR_PTV, EN, ENABLE )
        | NV_DRF_NUM( TIMER, TMR_PTV, TMR_PTV, NVAOS_CLOCK_DEFAULT );
    AOS_REGW( TIMER0_PA_BASE, reg );

    /* run the scheduler */
    nvaosScheduler();

    AOS_REGW(GIC_INTERRUPT_INT_SET(CPU_IRQ_TIMER_0 /
            IRQS_PER_INTR_CTLR),
            (1 << (CPU_IRQ_TIMER_0 % IRQS_PER_INTR_CTLR)));
}

void
nvaos_ConfigureInterruptController(void)
{
    NvU32 core;
    NvU32 reg;
    NvU32 i;
    NvU32 SpiLines;
    NvU32 FirstReg;
    NvU32 LastReg;

    /* Initialize the GIC (Generalized Interrupt Controller).
       Get the number of SPI lines (there are 32 interrupts per line). */
    SpiLines = AOS_REGR(GIC_PA_BASE + FIC_DIST_IC_TYPE_0);
    SpiLines = NV_DRF_VAL(FIC_DIST, IC_TYPE, IT_LINES_NUMBER, SpiLines);

    /* Disable all the interrupt enables (32 per register, 'SpiLine'
       external interrupt registers + 1 register for IPI/PPI interrupts). */
    FirstReg = GIC_PA_BASE + FIC_DIST_ENABLE_CLEAR_0_0;
    LastReg  = FirstReg + ((SpiLines + 1) * sizeof(NvU32));
    for (i = FirstReg; i < LastReg; i += sizeof(NvU32))
    {
        AOS_REGW( i, ~0);
    }

    /* Set the interrupt affinity of only to the CPU that this code is currently
     * running on.
     *
     * Set affinity of all external interrupts interrupts (4 per register).
     */
    MRC(p15, 0, core, c0, c0, 5);
    core &= 0x3; // Bottom 2 bits are the cpu number

    core = 1 << core;
    core = ((core << 24) | (core << 16) | (core << 8) | core);

    FirstReg = GIC_PA_BASE + FIC_DIST_SPI_TARGET_4_0;
    LastReg  = FirstReg + (SpiLines * 8 * sizeof(NvU32));
    for (i = FirstReg; i < LastReg; i += sizeof(NvU32))
    {
        AOS_REGW( i, core);
    }

    /* Enable the GIC distributor. */
    reg = AOS_REGR( GIC_PA_BASE + FIC_DIST_DISTRIBUTOR_ENABLE_0);
    reg = NV_FLD_SET_DRF_NUM(FIC_DIST, DISTRIBUTOR_ENABLE,
        INTERRUPT_IN_ENABLE, 1, reg);
    AOS_REGW( GIC_PA_BASE + FIC_DIST_DISTRIBUTOR_ENABLE_0, reg);

    /* Configure group1 interrupts for non-secure interrupts */
#if AOS_MON_MODE_ENABLE
    FirstReg = GIC_PA_BASE + FIC_DIST_INTERRUPT_SECURITY_0_0;
    LastReg = FirstReg + sizeof(NvU32) * 6;
    for ( i = FirstReg; i < LastReg; i+= sizeof(NvU32) )
    {
        AOS_REGW( i, 0xFFFFFFFF );
    }
#endif

    if (s_IsCortexA9)
    {
        /* Enable the CPU interface to get interrupts */
        reg = AOS_REGR(GIC_PA_BASE + FIC_PROC_IF_CONTROL_0);
        reg = NV_FLD_SET_DRF_NUM(FIC_PROC_IF, CONTROL, ENABLE_S, 1, reg);
        AOS_REGW( GIC_PA_BASE + FIC_PROC_IF_CONTROL_0, reg);

        /* Set the priorty to the lowest */
        reg = NV_DRF_NUM(FIC_PROC_IF, PRIORITY_MASK, PRIORITY, ~0);
        AOS_REGW( GIC_PA_BASE +  FIC_PROC_IF_PRIORITY_MASK_0, reg);
    }
    else
    {
        /* Enable the CPU interface to get interrupts */
        reg = AOS_REGR(GIC_PA_BASE + EIC_PROC_IF_GICC_CTLR_0);
        reg = NV_FLD_SET_DRF_NUM(EIC_PROC_IF, GICC_CTLR, EnableGrp0, 1, reg);
        AOS_REGW( GIC_PA_BASE + EIC_PROC_IF_GICC_CTLR_0, reg);

        /* Set the priorty to the lowest */
        reg = NV_DRF_NUM(EIC_PROC_IF, GICC_PMR, Priority, ~0);
        AOS_REGW( GIC_PA_BASE +  EIC_PROC_IF_GICC_PMR_0, reg);
    }
}

void
nvaos_EnableTimer( void )
{
    NvU32 offset;

    /* Enable the interrupt */
    offset = (CPU_IRQ_TIMER_0 >> 5) * 4 + FIC_DIST_ENABLE_SET_0_0;
    AOS_REGW(GIC_PA_BASE + offset,
        (1 << (CPU_IRQ_TIMER_0 & 0x1f)));

    AOS_REGW08(GIC_PA_BASE + FIC_DIST_SPI_TARGET_0_0 +
        CPU_IRQ_TIMER_0, 0x3);
}

/* install an interrupt handler for the given id */
void
nvaosInterruptInstall( NvU32 id, NvOsInterruptHandler handler, void *context )
{
    int   i;
    NvU32 state;

    state = disableInterrupts();

    // find an unsused handler
    for (i=0; i < NVAOS_MAX_HANDLERS; ++i)
    {
        if (s_InterruptHandlersValid[i] == NV_FALSE)
            break;
    }

    if (i == NVAOS_MAX_HANDLERS)
    {
        NV_ASSERT(!"Attempt to register too many handlers...\n");
        return;
    }

    s_InterruptHandlers[i] = handler;
    s_InterruptContext[i] = context;
    s_InterruptHandlersValid[i] = NV_TRUE;
    s_InterruptHandlerId[i] = id;

    restoreInterrupts(state);
}

void
nvaosInterruptUninstall( NvU32 id )
{
    int i;
    NvU32 intState;

    for (i = 0; i < NVAOS_MAX_HANDLERS; ++i)
    {
        if ( s_InterruptHandlersValid[i] &&  s_InterruptHandlerId[i] == id)
            break;
    }

    if (i == NVAOS_MAX_HANDLERS)
    {
        NV_ASSERT(!"Attempt to unregister an interrupt that was not "
            "registered.\n");
        return;
    }

    intState = disableInterrupts();

    s_InterruptHandlers[i] = 0;
    s_InterruptHandlerId[i] = 0;
    s_InterruptContext[i] = 0;
    s_InterruptHandlersValid[i] = NV_FALSE;

    restoreInterrupts(intState);
}

#if !__GNUC__
__asm void
cpuSetupModes( NvU32 stack )
{
    /* svc mode */
    msr cpsr_fsxc, #( MODE_DISABLE_INTR | MODE_SVC )
    mov sp, r0

    /* abt mode */
    msr cpsr_fsxc, #( MODE_DISABLE_INTR | MODE_ABT )
    mov sp, r0

    /* undef mode */
    msr cpsr_fsxc, #( MODE_DISABLE_INTR | MODE_UND )
    mov sp, r0

    /* irq mode */
    msr cpsr_fsxc, #( MODE_DISABLE_INTR | MODE_IRQ )
    mov sp, r0

    /* system mode */
    msr cpsr_fsxc, #( MODE_DISABLE_INTR | MODE_SYS )

    bx lr
}
#endif

void
nvaos_ConfigureCpu(void)
{
    NvU32 stack;

    /* CPU mode setup */
    stack = (NvU32)s_IrqStack;
    stack += 2048 - 4;

    cpuSetupModes( stack );
}

void nvaos_ConfigureA15(void)
{
    if (s_IsCortexA9 == NV_FALSE)
    {
        NvU32 tmp;

        MRC(p15, 1, tmp, c9, c0, 2);
        tmp &= ~7;
        tmp |= 2;
        MCR(p15, 1, tmp, c9, c0, 2);

        MRC(p15, 1, tmp, c15, c0, 0);
        tmp |= (1<<7);
        MCR(p15, 1, tmp, c15, c0, 0);

        /* Enable non-cacheable streaming enhancement */
        MRC(p15, 0, tmp, c1, c0, 1);
        tmp |= (1 << 24);
        MCR(p15, 0, tmp, c1, c0, 1);

#if !defined(ENABLE_NVTBOOT)
        tmp = AOS_REGR(SYSCTR0_PA_BASE + SYSCTR0_CNTFID0_0);
        MCR(p15, 0, tmp, c14, c0, 0);

        tmp = AOS_REGR(SYSCTR0_PA_BASE + SYSCTR0_CNTCR_0);
        tmp |= 3;
        AOS_REGW(SYSCTR0_PA_BASE + SYSCTR0_CNTCR_0, tmp);
        AOS_REGW(SYSCTR0_PA_BASE + SYSCTR0_COUNTERID0_0, 0);
        AOS_REGW(SYSCTR0_PA_BASE + SYSCTR0_COUNTERID1_0, 0);
        AOS_REGW(SYSCTR0_PA_BASE + SYSCTR0_COUNTERID2_0, 0);
        AOS_REGW(SYSCTR0_PA_BASE + SYSCTR0_COUNTERID3_0, 0);
        AOS_REGW(SYSCTR0_PA_BASE + SYSCTR0_COUNTERID4_0, 0);
        AOS_REGW(SYSCTR0_PA_BASE + SYSCTR0_COUNTERID8_0, 0);
        AOS_REGW(SYSCTR0_PA_BASE + SYSCTR0_COUNTERID9_0, 0);
        AOS_REGW(SYSCTR0_PA_BASE + SYSCTR0_COUNTERID10_0, 0);
        AOS_REGW(SYSCTR0_PA_BASE + SYSCTR0_COUNTERID11_0, 0);
#endif
    }
}
void nvaosCpuPreInit( void )
{
    NvU32 tmp;
    NvU32 id;
    NvU32 major;
    NvU32 minor;

    nvaos_ConfigureCpu();

    s_IsCortexA9 = NV_TRUE;
    s_Simulation = NV_FALSE;
    s_FpgaEmulation = NV_FALSE;
    s_QuickTurnEmul = NV_FALSE;
    s_UseLPAE = NV_FALSE;

    // check if we are in quickturn or fpga, only turn on L2 for quickturn
    tmp = AOS_REGR( MISC_PA_BASE + APB_MISC_GP_HIDREV_0 );
    id = NV_DRF_VAL( APB_MISC_GP, HIDREV, CHIPID, tmp );
    major = NV_DRF_VAL( APB_MISC_GP, HIDREV, MAJORREV, tmp );
    minor = NV_DRF_VAL( APB_MISC_GP, HIDREV, MINORREV, tmp );

    switch( id ) {
    case 0x30:
        s_Chip = NvAosChip_T30;
        tmp = AOS_REGR(FLOW_PA_BASE + FLOW_CTLR_CLUSTER_CONTROL_0);
        if (NV_DRF_VAL(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, tmp))
        {
            s_ClusterId = NvBlCpuClusterId_LP;
        }
        break;
    case 0x35:
        s_Chip = NvAosChip_T114;
        tmp = AOS_REGR(FLOW_PA_BASE + FLOW_CTLR_CLUSTER_CONTROL_0);
        if (NV_DRF_VAL(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, tmp))
        {
            s_ClusterId = NvBlCpuClusterId_LP;
        }
        break;
    case 0x40:
        s_Chip = NvAosChip_T124;
        tmp = AOS_REGR(FLOW_PA_BASE + FLOW_CTLR_CLUSTER_CONTROL_0);
        if (NV_DRF_VAL(FLOW_CTLR, CLUSTER_CONTROL, ACTIVE, tmp))
        {
            s_ClusterId = NvBlCpuClusterId_LP;
        }
        break;
    default:
        NV_ASSERT( !"unknown chip!" );
        break;
    }

    tmp = AOS_REGR( MISC_PA_BASE + APB_MISC_GP_EMU_REVID_0);
    s_Netlist = NV_DRF_VAL( APB_MISC_GP, EMU_REVID, NETLIST, tmp);
    s_NetlistPatch = NV_DRF_VAL(APB_MISC_GP, EMU_REVID, PATCH, tmp);
    if (major == 0)
    {
        if (s_Netlist == 0)
        {
            s_Simulation = NV_TRUE;
        }
        else
        {
            if (minor == 0)
            {
                s_QuickTurnEmul = NV_TRUE;
            }
            else
            {
                s_FpgaEmulation = NV_TRUE;
                s_UsePL310L2Cache = NV_FALSE;
            }
        }
    }

    // Is this a Cortex A9 or Cortex A15?  Configure ourselves accordingly
    MRC(p15, 0, tmp, c0, c0, 0);
    if ((tmp & 0x0000FFF0) != 0x0000C090)
    {
        // No
        s_IsCortexA9 = NV_FALSE;
        s_UsePL310L2Cache = NV_FALSE;
        if ((tmp & 0x0000FFF0) == 0x0000C0F0)
            nvaos_ConfigureA15();
    }

    nvaosGetMemoryMap( &s_MemoryMap );

#ifndef ENABLE_NVTBOOT
    nvaosInitPmcScratch(s_Chip);
#endif

    // GIC interrupt controller
    nvaos_ConfigureInterruptController();

    //---------------------------------------------------------------------
    // Disable the PCIE aperture in MSELECT. Touching its address range
    // whether through an invalid pointer access or through a JTAG debugger
    // is lethal. The PCIE driver will have to re-enable the aperture.
    //---------------------------------------------------------------------
    tmp = AOS_REGR(MSELECT_PA_BASE + MSELECT_CONFIG_0);
    tmp = NV_FLD_SET_DRF_NUM(MSELECT, CONFIG, ENABLE_PCIE_APERTURE, 0,
        tmp);
    AOS_REGW(MSELECT_PA_BASE + MSELECT_CONFIG_0, tmp);

    /* timer interrupt init */
    nvaos_EnableTimer();
}

void nvaosCpuPostInit( void )
{
    nvaosMonModeInit();
    nvaosNSModeInit();
}

#define CPU_DATA_CACHE_SET_WAY_INVALIDTE        (-1)
#define CPU_DATA_CACHE_SET_WAY_CLEAN            (0)
#define CPU_DATA_CACHE_SET_WAY_CLEAN_INVALIDATE (1)

#if __GNUC__

NV_NAKED void nvaosInvalidateL1DataCache(void)
{
    asm volatile(
        "mov    r9, #0                          \n" // r9 = current cache level
        "mcr    p15, 2, r9, c0, c0, 0           \n" // CSSELR
    );
}

NV_NAKED void nvaosCpuCacheSetWayOps(NvS32 op)
{
    // Register usage:
    //
    //  R0  = cache operation
    //  R1  = Cache Size Id Register (CCSIDR), or scratch
    //  R2  = cache level * 3, or cache line length
    //  R3  = number of cache levels * 2
    //  R4  = maximum way number
    //  R5  = bit position of way increment
    //  R6  = Cache Level Id Register (CLIDR)
    //  R7  = maximum set number
    //  R8  = current way number
    //  R9  = current cache level * 2

    // NOTE: The interior loops of this function should not use
    //       any instructions that cause data memory operations.

    asm volatile(
        "stmfd  sp!, {r4-r9, lr}                \n"

        // If the data cache is enabled, we can't invalidate the cache.
        // Turn invalidate requests into clean and invalidate.
        "cmp    r0, #0                          \n" // op
        "mrcmi  p15, 0, r1, c1, c0, 0           \n" // SCTLR
        "movge  r1, #0                          \n"
        "tst    r1, #4                          \n" // D-cache on?
        "movne  r0, #1                          \n"

        "mrc    p15, 1, r6, c0, c0, 1           \n" // CLIDR
        "ands   r3, r6, #0x07000000             \n"
        "mov    r3, r3, lsr #23                 \n" // R3 = number of cache levels * 2
        "beq    finished                        \n"
        "mov    r9, #0                          \n" // r9 = current cache level
"loop1:                                         \n"
        // Get the cache type at this level
        "add    r2, r9, r9, lsr #1              \n" // r2 = current cache level * 3
        "mov    r1, r6, lsr r2                  \n" // bottom 3 bits are cache type for this level
        "and    r1, r1, #7                      \n"
        "cmp    r1, #2                          \n"
        "blt    skip                            \n" // no cache or only instruction cache at this level

        // Get the cache attributes at this level
        "mcr    p15, 2, r9, c0, c0, 0           \n" // CSSELR
#ifdef COMPILE_ARCH_V7
        "isb                                    \n"
#else
        ".word  0xF57FF06F                      \n" // !!!DELETEME!!! BUG 830289
#endif
        "mrc    p15, 1, r1, c0, c0, 0           \n" // CCSIDR
        "and    r2, r1, #7                      \n" // cache line length
        "add    r2, r2, #4                      \n" // +4 for line length offset (log2 16 bytes)
#ifdef COMPILE_ARCH_V7
        "movw   r4, #0x3FF                      \n"
#else
        "mov    r4, #0xFF                       \n" // !!!DELETEME!!! BUG 830289
        "orr    r4, r4, #0x300                  \n" // !!!DELETEME!!! BUG 830289
#endif
        "ands   r4, r4, r1, lsr #3              \n" // r4 = max way number (right aligned)
        "clz    r5, r4                          \n" // r5 = bit positino of way size increment
#ifdef COMPILE_ARCH_V7
        "movw   r7, #0x7FFF                     \n"
#else
        "mov    r7, #0xFF                       \n" // !!!DELETEME!!! BUG 830289
        "orr    r7, r7, #0x7F00                 \n" // !!!DELETEME!!! BUG 830289
#endif
        "ands   r7, r7, r1, lsr #13             \n" // r7 = max set number (right aligned)

        // Iterate by set/way at this level
"loop2:                                         \n"
        "mov    r8, r4                          \n" // r8 = working copy of max way size
"loop3:                                         \n"
        "orr    r1, r9, r8, lsl r5              \n" // R1 = way number and cache number
        "orr    r1, r1, r7, lsl r2              \n" // R1 = way, set, cache numbers
        "cmp    r0, #0                          \n" // Determine requested operation:
        "mcrmi  p15, 0, r1, c7, c6, 2           \n" //  < 0: invalidate by set/way
        "mcreq  p15, 0, r1, c7, c10, 2          \n" //  = 0: clean by set/way
        "mcrgt  p15, 0, r1, c7, c14, 2          \n" //  > 0: clean & invaldiate by set/way
        "subs   r8, r8, #1                      \n" // decrement way
        "bge    loop3                           \n"
        "subs   r7, r7, #1                      \n" // decrement set
        "bge    loop2                           \n"
"skip:                                          \n"
        "add    r9, r9, #2                      \n" // increment cache level
        "cmp    r3, r9                          \n"
        "bgt    loop1                           \n"
"finished:                                      \n"
#ifdef COMPILE_ARCH_V7
        "dsb                                    \n"
#else
        ".word  0xF57FF04F                      \n" // !!!DELETEME!!! BUG 830289
#endif
        "ldmfd  sp!, {r4-r9, pc}                \n"
    );
}
#else
#pragma arm
__asm void nvaosCpuCacheSetWayOps(NvS32 op)
{
    // Register usage:
    //
    //  R0  = cache operation
    //  R1  = Cache Size Id Register (CCSIDR), or scratch
    //  R2  = cache level * 3, or cache line length
    //  R3  = number of cache levels * 2
    //  R4  = maximum way number
    //  R5  = bit position of way increment
    //  R6  = Cache Level Id Register (CLIDR)
    //  R7  = maximum set number
    //  R8  = current way number
    //  R9  = current cache level * 2

    // NOTE: The interior loops of this function should not use
    //       any instructions that cause data memory operations.

        stmfd   sp!, {r4-r9, lr}

        // If the data cache is enabled, we can't invalidate the cache.
        // Turn invalidate requests into clean and invalidate.
        cmp     r0, #0                          // op
        mrcmi   p15, 0, r1, c1, c0, 0           // SCTLR
        movge   r1, #0
        tst     r1, #4                          // D-cache on?
        movne   r0, #1

        mrc     p15, 1, r6, c0, c0, 1           // CLIDR
        ands    r3, r6, #0x07000000
        mov     r3, r3, lsr #23                 // R3 = number of cache levels * 2
        beq     finished
        mov     r9, #0                          // r9 = current cache level
loop1
        // Get the cache type at this level
        add     r2, r9, r9, lsr #1              // r2 = current cache level * 3
        mov     r1, r6, lsr r2                  // bottom 3 bits are cache type for this level
        and     r1, r1, #7
        cmp     r1, #2
        blt     skip                            // no cache or only instruction cache at this level

        // Get the cache attributes at this level
        mcr     p15, 2, r9, c0, c0, 0           // CSSELR
#ifdef COMPILE_ARCH_V7
        isb
#else
        DCD     0xF57FF06F                      // !!!DELETEME!!! BUG 830289
#endif
        mrc     p15, 1, r1, c0, c0, 0           // CCSIDR
        and     r2, r1, #7                      // cache line length
        add     r2, r2, #4                      // +4 for line length offset (log2 16 bytes)
#ifdef COMPILE_ARCH_V7
        movw    r4, #0x3FF
#else
        mov     r4, #0xFF                       // !!!DELETEME!!! BUG 830289
        orr     r4, r4, #0x300                  // !!!DELETEME!!! BUG 830289
#endif
        ands    r4, r4, r1, lsr #3              // r4 = max way number (right aligned)
        clz     r5, r4                          // r5 = bit positino of way size increment
#ifdef COMPILE_ARCH_V7
        movw    r4, #0x7FFF
#else
        mov     r7, #0xFF                       // !!!DELETEME!!! BUG 830289
        orr     r7, r7, #0x7F00                 // !!!DELETEME!!! BUG 830289
#endif
        ands    r7, r7, r1, lsr #13             // r7 = max set number (right aligned)

        // Iterate by set/way at this level
loop2
        mov     r8, r4                          // r8 = working copy of max way size
loop3
        orr     r1, r9, r8, lsl r5              // R1 = way number and cache number
        orr     r1, r1, r7, lsl r2              // R1 = way, set, cache numbers
        cmp     r0, #0                          // Determine requested operation:
        mcrmi   p15, 0, r1, c7, c6, 2           //  < 0: invalidate by set/way
        mcreq   p15, 0, r1, c7, c10, 2          //  = 0: clean by set/way
        mcrgt   p15, 0, r1, c7, c14, 2          //  > 0: clean & invaldiate by set/way
        subs    r8, r8, #1                      // decrement way
        bge     loop3
        subs    r7, r7, #1                      // decrement set
        bge     loop2
skip
        add     r9, r9, #2                      // increment cache level
        cmp     r3, r9
        bgt     loop1
finished
#ifdef COMPILE_ARCH_V7
        dsb
#else
        DCD     0xF57FF04F                      // !!!DELETEME!!! BUG 830289
#endif
        ldmfd   sp!, {r4-r9, pc}
}
#endif

void nvaosConfigureCache(void)
{
    const NvU32 zero = 0;
    NvU32 cache_size_id;
    NvU32 cache_level_id;
    NvU32 num_levels;
    NvU32 reg;
    NvU32 intState;
    NvU32 prrr = 0xff0a89a8, nmrr = 0xc0e044e0;

    intState = disableInterrupts();

    /*
     * memory mapping table
     * TEX[0]  C  B  n  TRn  IRn ORn  type
     * 0       0  0  0  00   00  00   strongly ordered
     * 0       0  1  1  10   00  00   normal memory, non-cacheable, bufferable
     * 0       1  0  2  10   10  10   normal memory, WTNW, WTNW
     * 0       1  1  3  10   11  11   normal memory, WBRA, WBRA
     * 1       0  0  4  01   00  00   device
     * 1       0  1  5  10   01  00   normal memory, WBWA, Non-cacheable
     * 1       1  0  6       00  00   reserved
     * 1       1  1  7  10   01  11   normal memory, WBWA, WBRA
     */

    /*
     * we use 111 for normal cacheable memory,       WriteBack
     * we use 001 for writecombine                   WriteCombined
     * we use 100 for device memory,                 device
     * we use 000 for "uncached"                     Uncached/Unbuffered
     * we use 101 for inner WBWA, outer non-cached   InnerWriteBack
     */
    MRC(p15, 0, reg, c1, c0, 0);
    reg |= (1<<TRE_EN);
    MCR(p15, 0, reg, c1, c0, 0);   /* enable TRE */
    MCR(p15, 0, prrr, c10, c2, 0); /* memory types */
    MCR(p15, 0, nmrr, c10, c2, 1); /* memory attributes */

    /* Get the number of architectural cache levels */
    MRC(p15, 1, cache_level_id, c0, c0, 1); /* CLIDR */
    num_levels = ((cache_level_id >> 24) & 7);
    NV_ASSERT(num_levels >= 1);

    /* Get the cache attributes at level 0 (data cache). */
    MCR(p15, 2, zero, c0, c0, 0);           /* CCSELR: LSB is D/I selector */
    MCR(p15, 0, zero, c7, c5, 4);           /* ISB */
    MRC(p15, 1, cache_size_id, c0, c0, 0);  /* CCSIDR */

    s_CpuCacheLineSize = 1 << ((cache_size_id & 7) + 4);
    s_CpuCacheLineMask = s_CpuCacheLineSize - 1;

    /* Invalidate instruction cache */
    MCR(p15, 0, zero, c7, c5, 0);

    /* Invalidate the TLBs */
    MCR(p15, 0, zero, c8, c5, 0);
    MCR(p15, 0, zero, c8, c6, 0);
    MCR(p15, 0, zero, c7, c10, 4);

    /* enable instruction cache */
    MRC(p15, 0, reg, c1, c0, 0);
    reg |= M_CP15_C1_C0_0_I;
    MCR(p15, 0, reg, c1, c0, 0);

    /* Enable SMP mode */
    MRC(p15, 0, reg, c1, c0, 1);
    reg |= 0x40;
    if (num_levels == 1)
    {
        /* Also enable maintenance operation broadcast on systems with
           only a single level of architectural cache. */
        reg |= 1;
    }
    MCR(p15, 0, reg, c1, c0, 1);

    /* Systems with an architectural L2 cache must not use the PL310. */
    if (num_levels > 1)
    {
        s_UsePL310L2Cache = NV_FALSE;
    }
    else
    {
        s_UsePL310L2Cache = NV_TRUE;
    }

    /* Invalidate the data cache. */
    nvaosCpuCacheSetWayOps(CPU_DATA_CACHE_SET_WAY_INVALIDTE);

    restoreInterrupts(intState);
}

#if !__GNUC__
/* inform the ARM libc that we know what we are doing -- don't do the usual
 * check for stack == heap limit for out-of-memory
 */
#pragma import(__use_two_region_memory)

__value_in_regs  struct  __initial_stackheap
    __user_initial_stackheap(unsigned R0, unsigned SP, unsigned R2,
        unsigned SL)
{
    struct __initial_stackheap config;

    // R0-R3 contain the return values
    // R0 - heap base
    // R1 - stack base
    // R2 - heap limit
    // R3 - stack limit

    config.heap_base = AOS_END_SAFE(ADDR, s_CpuCacheLineSize);
    config.heap_limit = AOS_REMAIN_ALLOC_BOTTOM;

    config.stack_limit = (unsigned int) &s_LibcStack[0];
    config.stack_base =
        (unsigned int)&(s_LibcStack[NV_ARRAY_SIZE(s_LibcStack)]) ;

    return config;
}
#endif

void nvaosAllocatorSetup( void )
{
    s_InitAllocator.base = (NvU8 *)AOS_INIT_ALLOC_BOTTOM;
    s_InitAllocator.size = AOS_INIT_ALLOC_SIZE;
    s_InitAllocator.mspace =
        create_mspace_with_base(s_InitAllocator.base, s_InitAllocator.size, 0);
}

static NvBool
nvaos_IsUncached(NvU32 addr)
{
    /* figure out the PTE entry */
    NvU32 PTEbase = (addr - BottomOfDram) >> 12;
    NvU32 *pPte = &(SecondLevelPageTable[AosPte_Dram][PTEbase]);
    if ((pPte[0] & MEM_ATTRIB_MASK) == UNCACHED)
    {
        return NV_TRUE;
    }
    return NV_FALSE;
}

static NvBool
nvaos_IsWriteBack(NvU32 addr)
{
    /* figure out the PTE entry */
    NvU32 PTEbase = (addr - BottomOfDram) >> 12;
    NvU32 *pPte = &(SecondLevelPageTable[AosPte_Dram][PTEbase]);
    if ((pPte[0] & MEM_ATTRIB_MASK) == WRITEBACK)
    {
        return NV_TRUE;
    }
    return NV_FALSE;
}

static NvBool
nvaos_IsWriteCombined(NvU32 addr)
{
    /* figure out the PTE entry */
    NvU32 PTEbase = (addr - BottomOfDram) >> 12;
    NvU32 *pPte = &(SecondLevelPageTable[AosPte_Dram][PTEbase]);
    if ((pPte[0] & MEM_ATTRIB_MASK) == WRITECOMBINE)
    {
        return NV_TRUE;
    }
    return NV_FALSE;
}


static NvBool
nvaos_IsInnerWriteBack(NvU32 addr)
{
    /* figure out the PTE entry */
    NvU32 PTEbase = (addr - BottomOfDram) >> 12;
    NvU32 *pPte = &(SecondLevelPageTable[AosPte_Dram][PTEbase]);
    if ((pPte[0] & MEM_ATTRIB_MASK) == INNERWRITEBACK)
    {
        return NV_TRUE;
    }
    return NV_FALSE;
}

static  NvU32  ConfigurePageTableNS(NvU32 pte)
{
#if     AOS_MON_MODE_ENABLE
        return pte | 0x9;
#else
        return pte | 0x1;
#endif
}

static  void  ConfigureSectionNS(void)
{
#if     AOS_MON_MODE_ENABLE
    NvU32  i;
    NvU32  start;
    NvU32  end;

    start = nvaos_GetMemoryStart() / 0x100000;
    end = nvaos_GetMemoryEnd() / 0x100000;

    // Set ext mem first level section descriptor NS=1
    // so that it is accessible from non-secure state.
    for (i = start; i < end; i++)
    {
        FirstLevelPageTable[i] |= 0x80000;
    }
#endif
}

static NvBool
nvaos_IsSecured( NvU32 addr )
{

    if( (addr >= (NvUPtr)s_SecuredAllocator.base) &&
        (addr < ((NvUPtr)(s_SecuredAllocator.base + s_SecuredAllocator.size))))
        return NV_TRUE;

    return NV_FALSE;
}

static void
nvaos_AddMemoryMapping(
    NvAosChip Chip,
    NvUPtr BottomOfMem,
    NvUPtr TopOfMem,
    void  *Virt,
    NvOsMemAttribute Attrib,
    NvBool UserAccess,
    NvBool Clean)
{
    NvUPtr Len  = (TopOfMem - BottomOfMem) >> 20;
    NvU32  Idx  = ((NvUPtr)Virt - BottomOfDram)>>12;
    NvU32 *pPde = &(FirstLevelPageTable[((NvUPtr)Virt)>>20]);
    NvU32 *pPte = &(SecondLevelPageTable[AosPte_Dram][Idx]);
    NvU32  j;
    NvU32  Attributes;

    // Virt must be at 1MB boundary
    NV_ASSERT(!((NvU32)Virt & 0xffff));

    // Setup of the loop-invariant part of the PTE here because
    // on debug FPGA builds this just takes too freakin' long.
    Attributes = (1<<SMALL_PAGE_BIT)         // 4KB small pages
               | memAttrib[Attrib];
    Attributes |= UserAccess ? (0x3<<4) : (0x1<<4);

    if (Attrib == NvOsMemAttribute_WriteBack ||
        Attrib == NvOsMemAttribute_InnerWriteBack)
    {
        Attributes |= (1<<SHARABLE_BIT);      // shareable
    }

    for (j = 0; j < Len ; j++, pPte += 256)
    {
        NvU32 i;

        if (Attrib == NvOsMemAttribute_Secured)
            pPde[j] = (NvU32)pPte | 0x1;
        else
            pPde[j] = ConfigurePageTableNS((NvU32)pPte);

        for (i=0; i<256; i++, BottomOfMem += 4096)
            pPte[i] = BottomOfMem | Attributes;
        if (Clean)
            NvOsDataCacheWritebackRange(pPte, 256*sizeof(*pPte));
    }
    if (Clean)
    {
        NvUPtr Mva = (NvUPtr)Virt;
        NvOsDataCacheWritebackRange(pPde, Len*sizeof(*pPde));
        for (j = 0; j <  Len; j++, Mva+=4096)
            MCR(p15, 0, Mva, c8, c7, 3);    /* TLBIMVAA */
    }
}

void   nvaosEnableMMU(NvU32 TtbAddr)
{
    NvU32 dacr;
    NvU32 sctlr;

    if (s_UseLPAE)
    {
        // Systems with LPAE require the use of 64-bit MCRR/MRRC instructions
        // for certain CP15 register accesses even if the configured physical
        // address can be completely contained within 4GB.
        NvU32 ttbcr;
        NvU64 ttbr0;

        // Make sure that the ARMv7-A non-LPAE page table format is selected.
        ttbcr = 0;
        MCR(p15, 0, ttbcr, c2, c0, 2);  // TTBCR

        // Set the Translation Table Base Register (TTBR0) with the
        // non-LPAE layout and enable cacheable page table walks
        // (inner/outer shared write-back/write-allocate).
        // NOTE: Double cast required to suppress warnings.
        ttbr0 = (NvU64)(TtbAddr);
        ttbr0 |= (1 << 6) | (1 << 3) | (1 << 1);
        MCRR_U64(p15, 0, ttbr0, c2);    // TTBR0
    }
    else
    {
        //  Set the Translation Table Base Register (TTBR0)
        MCR(p15, 0, TtbAddr, c2, c0, 0);
    }

    // Set Domain Access Control Register (DACR) to client
    dacr = 1;
    MCR(p15, 0, dacr, c3, c0, 0);   // DACR

    // Update the System Control Register:
    //  - Enable the data cache
    //  - Enable the MMU.
    MRC(p15, 0, sctlr, c1, c0, 0);  // SCTLR
    sctlr |= SCTLR_M | SCTLR_C;
    MCR(p15, 0, sctlr, c1, c0, 0);  // SCTLR
}

// NOTE: this has to be called with the cache disabled
static void
nvaos_CreateInitialPageTable(
    NvAosChip Chip,
    NvUPtr BottomOfMem,
    NvUPtr TopOfMem,
    NvUPtr BottomOfMmio)
{
    NvU32 i;
    NvU32 Pde, Pte;
    NvU32 clidr;
    NvU32 mmfr0;
    NvU32 sctlr;
    NvBool UseHighVector = NV_FALSE;

    // split the memory in half for cached/uncached
    NV_ASSERT( (TopOfMem - BottomOfMem) <= nvaos_GetMaxMemorySize() );
    NV_ASSERT( (NvU32)&__end__ < TopOfMem );

// Physical memory layout of SDRAM. Only supports upto 512MB of SDRAM.
//
// page 0x00:               reserved
// page 0x01:               reserved (This page was earlier used for hi-vec remap)
//                          (Current hi-vec remap is located one page before kernel load address)
// page 0x02 - page 0x81:   2nd level page table for sdram cached.
// page 0x82 - page 0x101:  2nd level page table for sdram uncached.
// page 0x102:              maps first meg virtual address space, to catch
//                          NULL pointer exceptions.
// page 0x103:              last megabyte pagetable, to remap vectors low
// page 0x104 - page 0x107: First level page table - 4 pages for 4 GIG virtual
//                          memory aperture
// page 0x108 - page 0x1ffff: Remainder of 512MB RAM used by program and data.
//
// LD script should load the program starting from page 0x108

    // Does this CPU support the Large Physical Address Extension (LPAE)?
    MRC(p15,0,mmfr0,c0,c1,4);   // MMFR0
    if ((mmfr0 & 0xF) >= 5)     // VMSA
    {
        // FIXME: Changes in the bootloader virtual address map require
        //        the use of the TEX remap registers. Use of the TEX
        //        remap registers is incompatible with LPAE which require
        //        the use of MAIR0/1 instead. For now do not enable LPAE.
        s_UseLPAE = NV_FALSE;
    }

    // Get the number of architectural cache levels
    MRC(p15, 1, clidr, c0, c0, 1); // CLIDR
    if (((clidr >> 24) & 7) > 1)
    {
        // Systems with an architectural L2 cache must not use the PL310
        // or the SCU (they don't exist).
        s_UsePL310L2Cache = NV_FALSE;
    }

    MRC(p15, 0, sctlr, c1, c0, 0);  // SCTLR
    if (sctlr & SCTLR_V)
    {
        UseHighVector = NV_TRUE;
    }

    FirstLevelPageTable                 =(NvU32*)(BottomOfMem + (0x104<<12));
    SecondLevelPageTable[AosPte_Dram]   =(NvU32*)(BottomOfMem + (0x02<<12));
    SecondLevelPageTable[AosPte_LastMeg]=(NvU32*)(BottomOfMem + (0x103<<12));

    BottomOfDram = BottomOfMem;

    NvOsMemset(&FirstLevelPageTable[0], 0,
        ((NvU8*)&FirstLevelPageTable[4096])
         - ((NvU8*)&FirstLevelPageTable[0]));

    NvOsMemset(&SecondLevelPageTable[AosPte_Dram][0], 0,
        ((NvU8*)&SecondLevelPageTable[AosPte_Dram][256*1024])
         -((NvU8*)&SecondLevelPageTable[AosPte_Dram][0]));

    // Remap the last megabyte of VA so that high-vector accesses go to SDRAM
    // physical page 1.
    Pde = (NvU32)SecondLevelPageTable[AosPte_LastMeg];
    Pde = ConfigurePageTableNS((NvU32)Pde);

    if (UseHighVector)
        FirstLevelPageTable[0xfff] = Pde;
    else
        FirstLevelPageTable[0] = Pde;

    for (i=0; i<256; i++)
    {
        Pte = (0x3<<4) | 0x2;
        if ((UseHighVector && i == 0xf0) || (!UseHighVector && i == 0))
        {
            Pte |= (AOS_HI_VEC_REMAP_BOTTOM);
            Pte |= memAttrib[NvOsMemAttribute_WriteBack];
        }
        else
        {
            Pte |= (((0xfff<<8) | i) << 12);
            Pte |= 0x1;  // no-execute
            Pte |= memAttrib[NvOsMemAttribute_Uncached];
        }
        SecondLevelPageTable[AosPte_LastMeg][i] = Pte;
    }

    // Map all of MMIO space, except for the already-mapped last megabyte
    for (i=(BottomOfMmio)>>20; i<0xfff; i++)
    {
        Pde  = (i<<20) | 0x2;           // 1MB device section as strongly ordered
        Pde |= (3<<10) | (1<<4);        // Full r/w permissions, no execute
        FirstLevelPageTable[i] = Pde;
    }

    ConfigureSectionNS();

    nvaos_AddMemoryMapping(Chip, BottomOfMem, TopOfMem,
        (void *)BottomOfMem, NvOsMemAttribute_WriteBack, NV_TRUE, NV_FALSE);

    // Disable user access to the reserved pages
    for (i=0; i<0x108; i++)
    {
        Pte = SecondLevelPageTable[AosPte_Dram][i];
        Pte &= ~(0x3<<4);
        Pte |= (0x1<<4);
        SecondLevelPageTable[AosPte_Dram][i] = Pte;
        //  Repeat for the uncached aperture.
        Pte = SecondLevelPageTable[AosPte_Dram][0x80*1024 + i];
        Pte &= ~(0x3<<4);
        Pte |= (0x1<<4);
        SecondLevelPageTable[AosPte_Dram][0x80*1024 + i] = Pte;
    }
}

static void nvaosSetupTZMemory(void)
{
#if AOS_MON_MODE_ENABLE
    NvU32 TZBottom = nvaos_GetTZMemoryStart();
    NvU32 TZTop = nvaos_GetTZMemoryEnd();

    AOS_REGW( MC_PA_BASE + MC_SECURITY_CFG0_0, TZBottom );
    AOS_REGW( MC_PA_BASE + MC_SECURITY_CFG1_0, (TZTop-TZBottom)>>20 );
#endif
}

void nvaosPageTableInit(void)
{
    NvU32 Bottom = nvaos_GetMemoryStart();
    NvU32 Top = AOS_REMAIN_ALLOC_BOTTOM;

    nvaos_CreateInitialPageTable(s_Chip, Bottom, Top, nvaos_GetMmioStart());

    nvaosEnableMMU((NvU32)FirstLevelPageTable);

    if (s_UsePL310L2Cache)
    {
        nvaosPl310Init();
        nvaosPl310Enable();
    }
}

void nvaosFinalSetup(void)
{
    NvRmDeviceHandle hRm;
    NvU32 CarveoutSize;
    NvU32 MemorySize;
    NvU32 intState;
    NvU32 Bottom;
    NvU32 CachedTop;
    NvU32 WriteCombinedTop;
    NvU32 UncachedTop;
    NvU32 CarveoutTop;
    NvU32 SecuredTop;
    NvU32 Top;
    NvU32 UncachedSize;
    NvU32 WriteCombinedSize;
    NvU32 SecuredSize;

    // Explicitly open RM before querying the memory size because there
    // is an incestuous relationship between NvOdmQueryMemSize() and
    // NvRmOpen(). There is a call to NvOdmQueryMemSize() from within
    // NvRmOpen() that will not report the correct memory size as set
    // by the customer option if RM is not already open.
    NV_ASSERT_SUCCESS(NvRmOpen(&hRm, 0));

    MemorySize = NV_MIN(nvaos_GetMaxMemorySize(),
                        NvOdmQueryMemSize(NvOdmMemoryType_Sdram));
    CarveoutSize = NvOdmQueryCarveoutSize();
    SecuredSize = NvOdmQuerySecureRegionSize();
    UncachedSize = MemorySize >> 3;
    // Align UncachedSize to 1 MB
    UncachedSize = AOS_ALIGN(UncachedSize, 0x100000);
    WriteCombinedSize = MemorySize >> 4;
    WriteCombinedSize = AOS_ALIGN(WriteCombinedSize, 0x100000);

    Top = nvaos_GetMemoryStart() + MemorySize;
    SecuredTop = Top;

    CarveoutTop = SecuredTop - SecuredSize;
    UncachedTop = CarveoutTop - CarveoutSize;
    WriteCombinedTop = UncachedTop - UncachedSize;
    CachedTop = WriteCombinedTop - WriteCombinedSize;

    Bottom = AOS_REMAIN_ALLOC_BOTTOM;

    intState = nvaosDisableInterrupts();
    nvaos_AddMemoryMapping(s_Chip, Bottom, CachedTop, (void*)Bottom,
        NvOsMemAttribute_WriteBack, NV_TRUE, NV_TRUE);

    nvaos_AddMemoryMapping(s_Chip, CachedTop, WriteCombinedTop,
        (void*)CachedTop, NvOsMemAttribute_WriteCombined, NV_TRUE, NV_TRUE);

    nvaos_AddMemoryMapping(s_Chip, WriteCombinedTop, UncachedTop,
        (void*)WriteCombinedTop, NvOsMemAttribute_Uncached, NV_TRUE, NV_TRUE);

    nvaos_AddMemoryMapping(s_Chip, UncachedTop, CarveoutTop,
        (void*)UncachedTop, NvOsMemAttribute_Uncached, NV_TRUE, NV_TRUE);

    nvaos_AddMemoryMapping(s_Chip, CarveoutTop, SecuredTop,
        (void*)CarveoutTop, NvOsMemAttribute_Secured, NV_TRUE, NV_TRUE);

    nvaosRestoreInterrupts(intState);

    intState = nvaosDisableInterrupts();

    /*
     * The heap memory layout is
     *
     * s_InitAllocator              :                              AOS_INIT_ALLOC_BOTTOM -- nvaos_GetMemoryStart() + 0x9FFFFF
     * (reserved for kernel image)  :                  nvaos_GetMemoryStart() + 0xA00000 -- nvaos_GetMemoryStart() + 0x15FFFFF
     * (reserved for ramdisk image) :                 nvaos_GetMemoryStart() + 0x2000000 -- nvaos_GetMemoryStart() + 0x33FFFFF
     * s_RemainAllocator            :                 nvaos_GetMemoryStart() + 0x3400000 -- (start of s_WriteCombinedAllocator - 1)
     * s_WriteCombinedAllocator     : ("start of s_UncachedAllocator" - MemorySize / 16) -- ("start of s_UncachedAllocator" - 1)
     * s_UncachedAllocator          :    ("start of Carveout" - MemorySize / 8)          -- ("start of Carveout" - 1)
     * Carveout Memory              : ("end of ext. memory - Securedsize - CarveoutSize" -- "end of ext. memory - Securedsize - 1")
     * s_SecuredAllocator           :  ("end of ext. memory - Securedsize")              -- "end of available ext. memory"
     */

    s_RemainAllocator.base = (NvU8 *)AOS_REMAIN_ALLOC_BOTTOM;
    s_RemainAllocator.size = MemorySize - \
                             SecuredSize - \
                             CarveoutSize - \
                             UncachedSize - \
                             WriteCombinedSize - \
                             (AOS_REMAIN_ALLOC_BOTTOM - nvaos_GetMemoryStart()) ;
    s_RemainAllocator.mspace =
        create_mspace_with_base(s_RemainAllocator.base,
            s_RemainAllocator.size, 0);

    s_WriteCombinedAllocator.base = s_RemainAllocator.base + s_RemainAllocator.size;
    s_WriteCombinedAllocator.size = WriteCombinedSize;
    s_WriteCombinedAllocator.mspace =
        create_mspace_with_base(s_WriteCombinedAllocator.base,
            s_WriteCombinedAllocator.size, 0);

    s_UncachedAllocator.base = s_WriteCombinedAllocator.base + s_WriteCombinedAllocator.size;
    s_UncachedAllocator.size = UncachedSize;
    s_UncachedAllocator.mspace =
        create_mspace_with_base(s_UncachedAllocator.base,
            s_UncachedAllocator.size, 0);

    s_SecuredAllocator.base = s_UncachedAllocator.base + s_UncachedAllocator.size + CarveoutSize;
    s_SecuredAllocator.size = SecuredSize;
    s_SecuredAllocator.mspace =
        create_mspace_with_base(s_SecuredAllocator.base,
            s_SecuredAllocator.size, 0);

    nvaosSetupTZMemory();
    if (s_SecuredAllocator.size)
        NvRmPrivHeapSecuredInit(s_SecuredAllocator.size,
            (NvRmPhysAddr)s_SecuredAllocator.base);

    nvaosRestoreInterrupts(intState);
}

NvS32
nvaosAtomicCompareExchange32(
    NvS32 *pTarget,
    NvS32 OldValue,
    NvS32 NewValue)
{
    NvS32 old;
    NvU32 state;
    NvU32 result = 0;
    const NvU32 zero = 0;

    if ( nvaos_IsUncached((NvU32)pTarget) )
    {
        state = nvaosDisableInterrupts();
        old = *pTarget;
        if (old == OldValue)
            *pTarget = NewValue;
        nvaosRestoreInterrupts(state);
    }
    else
    {
        //Data Memory Barrier
        MCR(p15, 0, zero, c7, c10, 5);
#if !__GNUC__
        do
        {
            // Get exclusive access.
            __asm("ldrex old, [pTarget]");
            if (old != OldValue)
                NewValue = old;
            // It could be a dummy write to remove exclusive access.
            __asm("strex result, NewValue, [pTarget]");
        } while (result);
#else
        do
        {
            // Get exclusive access.
            asm volatile(
            "ldrex %1, [%2] \n"
            "mov %0, #0     \n"
            "teq %1, %3     \n"
            "strexeq %0, %4, [%2] \n"
            :"=&r" (result), "=&r" (old)
            :"r" (pTarget), "r" (OldValue), "r" (NewValue)
            :"cc"
            );
        } while (result);
#endif
        //Data Memory Barrier
        MCR(p15, 0, zero, c7, c10, 5);
    }

    return old;
}

NvS32
nvaosAtomicExchange32(
    NvS32 *pTarget,
    NvS32 Value)
{
    NvS32 old;
    NvU32 state;
    NvU32 result;
    const NvU32 zero = 0;

    if ( nvaos_IsUncached((NvU32)pTarget) )
    {
        state = nvaosDisableInterrupts();
        old = *pTarget;
        *pTarget = Value;
        nvaosRestoreInterrupts(state);
    }
    else
    {
        //Data Memory Barrier
        MCR(p15, 0, zero, c7, c10, 5);
#if !__GNUC__
        do
        {
            __asm("ldrex old, [pTarget]");
            __asm("strex result, Value, [pTarget]");
        } while (result);
#else
        do
        {
            // Get exclusive access.
            asm volatile(
            "ldrex %1, [%2] \n"
            "strex %0, %3, [%2] \n"
            :"=&r" (result), "=&r" (old)
            :"r" (pTarget), "r" (Value)
            );
        } while (result);
#endif
        //Data Memory Barrier
        MCR(p15, 0, zero, c7, c10, 5);
    }

    return old;
}

NvS32
nvaosAtomicExchangeAdd32(
    NvS32 *pTarget,
    NvS32 Value)
{
    NvS32 old;
    NvU32 state;
    NvU32 result;
    NvS32 NewValue;
    const NvU32 zero = 0;

    if ( nvaos_IsUncached((NvU32)pTarget) )
    {
        state = nvaosDisableInterrupts();
        old = *pTarget;
        *pTarget = old + Value;
        nvaosRestoreInterrupts(state);
    }
    else
    {
        //Data Memory Barrier
        MCR(p15, 0, zero, c7, c10, 5);
#if !__GNUC__
        do
        {
            __asm("ldrex old, [pTarget]");
            NewValue = old + Value;
            __asm("strex result, NewValue, [pTarget]");
        } while (result);
#else
        do
        {
            // Get exclusive access.
            asm volatile(
            "ldrex %1, [%3] \n"
            "add %2, %1, %4 \n"
            "strex %0, %2, [%3] \n"
            :"=&r" (result), "=&r" (old), "=&r" (NewValue)
            :"r" (pTarget), "r" (Value)
            );
        } while (result);
#endif
        //Data Memory Barrier
        MCR(p15, 0, zero, c7, c10, 5);
    }
    return old;
}

void *
nvaosAllocate( NvU32 size, NvU32 align, NvOsMemAttribute attrib,
    NvAosHeap heap )
{
    Allocator **a = &s_Allocators[0];
    NvUPtr mem = 0;

    /* no allocations allowed in an interrupt handler */
    NV_ASSERT( !nvaosIsIsr() );

    if( !align )
    {
        align = 4;
    }

    /* check for valid alignment */
    NV_ASSERT( align == 4 || align == 8 || align == 16 || align == 32 );

    if ( attrib == NvOsMemAttribute_Uncached )
    {
        // for uncached memory, round the size up to a multiple number of
        // cache lines
        size = (size + s_CpuCacheLineMask) & ~s_CpuCacheLineMask;
        align = 32;
        a = &s_UncachedAllocators[0];
    }
    if ( attrib == NvOsMemAttribute_WriteCombined )
    {
        // for uncached memory, round the size up to a multiple number of
        // cache lines
        size = (size + s_CpuCacheLineMask) & ~s_CpuCacheLineMask;
        align = 32;
        a = &s_WriteCombinedAllocators[0];
    }

    if ( attrib == NvOsMemAttribute_Secured )
    {
        // for uncached memory, round the size up to a multiple number of
        // cache lines
        size = (size + s_CpuCacheLineMask) & ~s_CpuCacheLineMask;
        align = 32;
        a = &s_SecuredAllocators[0];
    }

    /* actually try to allocate some memory */
    if (heap == NvAosHeap_External)
    {
        NvU32 intState = nvaosDisableInterrupts();
        for (; !mem && (*a)!=NULL; a++)
        {
            if ((*a)->base)
                mem = (NvUPtr)mspace_memalign( (*a)->mspace, align, size );
        }
        nvaosRestoreInterrupts(intState);
    }
    else
    {
        NV_ASSERT( !"No internal memory" );
        return 0;
    }

    if( !mem )
    {
        return 0;
    }

    /* Make sure no stale data exists in the cache for the allocated range */
    NvOsDataCacheWritebackInvalidateRange( (void *)mem, size );

    return (void*)mem;
}

/* This function should be used with caution.  It does not preserve the orignal
 * allocated alignment restrictions.  Use with care.
 */
static void *
nvaos_DangerousRealloc(void *Orig, NvU32 Size)
{
    void *NewPtr;
    NvOsMemAttribute Attr;

    if (nvaos_IsUncached((NvU32)Orig))
    {
        Attr = NvOsMemAttribute_Uncached;
    }
    else if (nvaos_IsWriteCombined((NvU32)Orig))
    {
        Attr = NvOsMemAttribute_WriteCombined;
    }
    else
    {
        Attr = 0;
    }

    NewPtr = nvaosAllocate(Size, s_CpuCacheLineSize, Attr,
        NvAosHeap_External);
    if (!NewPtr)
    {
        return NULL;
    }

    /* FIXME: If NewPtr is larger than Orig, and Orig is allocated at the end
     * of addressable memory, this will cause a page fault.  There isn't any
     * easy way to fix this.
     */
    NvOsMemcpy(NewPtr, Orig, Size);
    nvaosFree(Orig);
    return NewPtr;
}

void *
nvaosRealloc( void *ptr, NvU32 size )
{
    Allocator **a;
    NvUPtr mem;
    NvU32 intState;
    void *newptr = NULL;

    /* no allocations allowed in an interrupt handler */
    NV_ASSERT(!nvaosIsIsr());
    NV_ASSERT(ptr);

    //  Uncached addresses always use the unsafe reallocator.
    if (nvaos_IsUncached((NvU32)ptr) || nvaos_IsWriteCombined((NvU32)ptr))
    {
        return nvaos_DangerousRealloc(ptr, size);
    }

    mem = (NvUPtr)ptr;
    for (a=&s_Allocators[0]; (*a); a++)
    {
        if ((mem >= (NvUPtr)(*a)->base) &&
            (mem < ((NvUPtr)(*a)->base + (*a)->size)))
        {
            break;
        }
    }

    NV_ASSERT((*a) && "Unknown mspace!");

    /* If the current mspace can't support the reallocation request, try any
     * available mspace with the dangerous reallocator
     */
    intState = nvaosDisableInterrupts();
    newptr = mspace_realloc((*a)->mspace, ptr, size);
    nvaosRestoreInterrupts(intState);

    if (!newptr)
    {
        return nvaos_DangerousRealloc(ptr, size);
    }

    return newptr;
}

void
nvaosFree( void *ptr )
{
    Allocator **a = &s_Allocators[0];
    NvUPtr mem;
    NvU32 intState;

    /* no allocations allowed in an interrupt handler */
    NV_ASSERT( !nvaosIsIsr() );

    /* ptr may be null */
    if( ptr == 0 )
    {
        return;
    }

    /* check for uncached address */
    if (nvaos_IsUncached((NvU32)ptr))
    {
        a = &s_UncachedAllocators[0];
    }else if(nvaos_IsWriteCombined((NvU32)ptr))
    {
        a = &s_WriteCombinedAllocators[0];
    }
    else if(nvaos_IsSecured((NvU32)ptr))
    {
        a = &s_SecuredAllocators[0];
    }

    mem = (NvUPtr)ptr;

    for( ; (*a); a++ )
    {
        if ((mem >= (NvU32)(*a)->base) &&
            (mem < (NvU32)(*a)->base + (*a)->size))
        {
            break;
        }
    }

    NV_ASSERT((*a) && "Unknown mspace!");

    intState = nvaosDisableInterrupts();
    mspace_free((*a)->mspace, ptr);
    nvaosRestoreInterrupts(intState);
}

NvU64
nvaosGetTimeUS( void )
{
    NvU32 usecTmp;

    usecTmp = AOS_REGR( TIMERUS_PA_BASE );
    if ( usecTmp < (NvU32)s_usec )
    {
        s_usec += (0x1ULL << 32);
    }
    s_usec &= (0xFFFFFFFFULL << 32);
    s_usec |= (NvU64)usecTmp;

    return s_usec;
}

void
NvOsDataCacheWriteback( void )
{
    NvU32 intState;

    intState = disableInterrupts();

    /* Clean the entire cache by set and way. */
    nvaosCpuCacheSetWayOps(CPU_DATA_CACHE_SET_WAY_CLEAN);

    if (s_UsePL310L2Cache)
    {
        nvaosPl310Writeback();
    }

    restoreInterrupts(intState);
}

void
NvOsDataCacheWritebackInvalidate( void )
{
    NvU32 intState;

    intState = disableInterrupts();

    /* Clean and envalidate the entire cache by set and way. */
    nvaosCpuCacheSetWayOps(CPU_DATA_CACHE_SET_WAY_CLEAN_INVALIDATE);

    if (s_UsePL310L2Cache)
    {
        nvaosPl310WritebackInvalidate();
    }

    restoreInterrupts(intState);
}

void
NvOsDataCacheWritebackRange( void *start, NvU32 TotalLength )
{
    NvU32 length = TotalLength;
    NvU32 wAddr = (NvU32)start;
    length += wAddr & s_CpuCacheLineMask;
    length = (length + s_CpuCacheLineMask) &
        ~s_CpuCacheLineMask;

    wAddr = wAddr & ~s_CpuCacheLineMask;

    while (length)
    {
        MCR(p15, 0, wAddr, c7, c10, 1);
        length = length - s_CpuCacheLineSize;
        wAddr  = wAddr + s_CpuCacheLineSize;
    }

    MCR(p15, 0, length, c7, c10, 4);

    if (s_UsePL310L2Cache)
    {
        nvaosPL310WritebackRange(start, TotalLength );
    }
}

void
NvOsDataCacheWritebackInvalidateRange( void *start, NvU32 TotalLength )
{
    NvU32 length = TotalLength;
    NvU32 wAddr = (NvU32)start;

    length += wAddr & s_CpuCacheLineMask;
    length = (length + s_CpuCacheLineMask) &
        ~s_CpuCacheLineMask;

    wAddr = wAddr & ~s_CpuCacheLineMask;

    while (length)
    {
        MCR(p15, 0, wAddr, c7, c14, 1);
        length = length - s_CpuCacheLineSize;
        wAddr  = wAddr + s_CpuCacheLineSize;
    }

    if (s_UsePL310L2Cache)
    {
        nvaosPl310WritebackInvalidateRange(start, TotalLength );
    }
}

void
NvOsInstrCacheInvalidate( void )
{
    const NvU32 zero = 0;

    MCR(p15, 0, zero, c7, c5, 0);
    // JN: we should not have to do anything to the l2-cache here.
}

void
NvOsInstrCacheInvalidateRange( void *start, NvU32 length )
{
    NvU32 wAddr = (NvU32)start;

    length += wAddr & s_CpuCacheLineMask;
    length = (length+ s_CpuCacheLineMask) &
        ~s_CpuCacheLineMask;

    wAddr = wAddr & ~s_CpuCacheLineMask;

    while (length)
    {
        MCR(p15, 0, wAddr, c7, c5, 1);
        length = length - s_CpuCacheLineSize;
        wAddr    = wAddr + s_CpuCacheLineSize;
    }

    // JN: we should not have to do anything to the l2-cache here.
}

NvBool
NvOsIsMemoryOfGivenType(NvOsMemAttribute Attrib, NvU32 Addr)
{
    NvBool IsSame;

    if (Attrib == NvOsMemAttribute_Uncached)
        IsSame = nvaos_IsUncached(Addr);
    else if (Attrib == NvOsMemAttribute_WriteBack)
        IsSame = nvaos_IsWriteBack(Addr);
    else if (Attrib == NvOsMemAttribute_WriteCombined)
        IsSame = nvaos_IsWriteCombined(Addr);
    else if (Attrib == NvOsMemAttribute_InnerWriteBack)
        IsSame = nvaos_IsInnerWriteBack(Addr);
    else if (Attrib == NvOsMemAttribute_Secured)
        IsSame = nvaos_IsSecured(Addr);
    else
        IsSame = NV_FALSE;

    return IsSame;
}

void
NvOsFlushWriteCombineBuffer( void )
{
    NvU32 tmp = 0;

    /* use Data Synchronization Barrier */
    MCR(p15, 0, tmp, c7, c10, 4);
    if (s_UsePL310L2Cache == NV_TRUE)
    {
        nvaosPl310Sync();
    }
}

static NvError
nvaos_PhysicalMemMap( NvOsPhysAddr phys, size_t size,
    NvOsMemAttribute attrib, NvU32 flags, void **ptr, NvBool doMapping)
{
    if ((phys >= nvaos_GetMemoryStart()) &&
        (phys < nvaos_GetMemoryStart() + nvaos_GetMaxMemorySize()))
    {
        void* va;

        va = (void*)phys;

        if (doMapping)
        {
            NvU32 intState = nvaosDisableInterrupts();
            nvaos_AddMemoryMapping(s_Chip, phys, phys+size,
                va, attrib, NV_TRUE, NV_TRUE);
            nvaosRestoreInterrupts(intState);
        }
        *ptr = va;
    }
    else if ((phys >= nvaos_GetMmioStart()) && (phys < nvaos_GetMmioEnd()))
        *ptr = (void*)phys;
    else
        return NvError_BadParameter;

    return NvSuccess;
}

// May add actual mapping
NvError
NvOsPhysicalMemMap( NvOsPhysAddr phys, size_t size,
    NvOsMemAttribute attrib, NvU32 flags, void **ptr )
{
    return nvaos_PhysicalMemMap(phys, size, attrib, flags, ptr, NV_TRUE);
}

// Query only
NvError
NvOsPhysicalMemMapping( NvOsPhysAddr phys, size_t size,
    NvOsMemAttribute attrib, NvU32 flags, void **ptr )
{
    return nvaos_PhysicalMemMap(phys, size, attrib, flags, ptr, NV_FALSE);
}

void
NvOsPhysicalMemUnmap( void *ptr, size_t size )
{
}

#define NVAOS_PAGE_SIZE (1 << 12)

#if !__GNUC__
NV_NAKED void nvaosFlushCache(void)
{
    PRESERVE8

    /* invalidate the i&d tlb entries */
    mov r1, #0
    mcr p15, 0, r1, c8, c5, 0
    mcr p15, 0, r1, c8, c6, 0
    bx lr
}
#else
NV_NAKED void nvaosFlushCache(void)
{
    asm volatile(
    //invalidate the i&d tlb entries
    "mov r1, #0                                     \n"
    "mcr p15, 0, r1, c8, c5, 0                      \n"
    "mcr p15, 0, r1, c8, c6, 0                      \n"
    "bx lr                                          \n"
    );
}
#endif // __GNUC__

NvError
NvOsPageAlloc( size_t size, NvOsMemAttribute attrib,
    NvOsPageFlags flags, NvU32 protect, NvOsPageAllocHandle *descriptor )
{
    void *ptr;

    /* Allocate NVAOS_PAGE_SIZE bytes more, so that we can align ourselves */
    ptr = nvaosAllocate(size + NVAOS_PAGE_SIZE, 0, attrib,
            NvAosHeap_External);

    if (ptr != NULL)
    {
        *descriptor = (NvOsPageAllocHandle)ptr;
        return NvSuccess;
    } else
    {
        *descriptor = NULL;
        return NvError_InsufficientMemory;
    }
}

void
NvOsPageFree( NvOsPageAllocHandle descriptor )
{
    nvaosFree((void *)descriptor);
}

NvError
NvOsPageLock(void *ptr, size_t size, NvU32 protect,
    NvOsPageAllocHandle *descriptor)
{
    return NvSuccess;
}

NvError
NvOsPageMap( NvOsPageAllocHandle descriptor, size_t offset,
    size_t size, void **ptr )
{
    NvU32 PageAlignedStart;

    NV_ASSERT(descriptor != NULL);
    NV_ASSERT(ptr != NULL);
    PageAlignedStart = ((NvU32)descriptor + NVAOS_PAGE_SIZE - 1) &
                                            ~(NVAOS_PAGE_SIZE - 1);
    *ptr = (void *)(PageAlignedStart + offset);

    return NvSuccess;
}

NvError
NvOsPageMapIntoPtr( NvOsPageAllocHandle descriptor, void *pCallerPtr,
    size_t offset, size_t size)
{
    /* There is no kernel and user mode on AOS, so there is no need for
     * this API
     */
    return NvError_NotImplemented;
}

void
NvOsPageUnmap(NvOsPageAllocHandle descriptor, void *ptr, size_t size)
{
    /* Just return as all pages are mapped on allocation */
}

NvOsPhysAddr
NvOsPageAddress( NvOsPageAllocHandle descriptor, size_t offset )
{
    NvU32 PageAlignedStart;

    NV_ASSERT(descriptor != NULL);
    PageAlignedStart = ((NvU32)descriptor + NVAOS_PAGE_SIZE - 1) &
                                            ~(NVAOS_PAGE_SIZE - 1);
    /* Cached virtual == Physical on AOS */
    return PageAlignedStart + offset;
}

// Placeholder for some future work.  ifdef'ing out for now
#if 0
// spinlocks for AP20 with dual ARMv7's
void
nvaosIntrMutexLock(NvOsIntrMutexHandle h)
{
    NvU32 val=0;
    NvU32 newval=1;

    h->SaveIntState = disableInterrupts();
    __asm
    {
spinhere:
            ldrex val, [&h->lock]
            cmp val, #0
            strexeq val, newval, [&h->lock]
            cmp val, #0
            beq spinhere
    }
}

void
nvaosIntrMutexUnlock(NvOsIntrMutexHandle h)
{
    h->lock = 0;  // release lock
    restoreInterrupts(h->SaveIntState);
}

NvError
nvaosIntrMutexCreate(NvOsIntrMutexHandle *h)
{
    NvOsIntrMutexHandle NewMut;

    NewMut = NvOsAlloc( sizeof(*NewMut) );
    if (!NewMut)
        return NvError_InsufficientMemory;

    NvOsMemset(NewMut, 0, sizeof(*NewMut));
    *h = NewMut;
    return NvSuccess;
}

void
nvaosIntrMutexDestroy(NvOsIntrMutexHandle h)
{
    NvOsFree(h);
}

#endif

