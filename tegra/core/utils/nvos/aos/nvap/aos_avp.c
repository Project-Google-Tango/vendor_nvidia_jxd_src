/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "aos_common.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "aos.h"
#include "aos_avp.h"
#include "arictlr.h"
#include "artimer.h"
#include "artimerus.h"
#include "arclk_rst.h"
#include "arapb_misc.h"
#include "arflow_ctlr.h"
#include "nvrm_drf.h"
#include "nvrm_module.h"
#include "common/nvrm_moduleids.h"
#include "nvos_internal.h"
#include "t30/aravp_cache.h"
#include "nvodm_pinmux_init.h"
#include "nvintr.h"
#include "nvrm_avp_swi_registry.h"
#include "aos_avp_board_info.h"
#include "nvnct.h"

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

/*
 * JTAG WAKEUP ENABLE:
 *  - Set to 0 if you don't want a JTAG event to wake up the AVP.
 *    This is the normal case for everyone except the the handfull
 *    of people who actually debug the AVP-side code.
 *  - Set to 1 if you are actively debugging the AVP code and
 *    you want a JTAG event to wake up the AVP. Note, if set to
 *    1 and you have JTAG connected, it may prevent the AVP from
 *    properly entering LP0.
 */
#define AVP_JTAG_WAKEUP_ENABLE  0

#ifndef FLOW_CTLR_HALT_COP_EVENTS_0_IRQ_1_RANGE
#define FLOW_CTLR_HALT_COP_EVENTS_0_IRQ_1_RANGE\
    FLOW_CTLR_HALT_COP_EVENTS_0_LIC_IRQ_RANGE
#endif

/* irq stack size must match the stack size in the linker script */
#define NVAOS_THUMB 1

NvOsInterruptHandler s_InterruptHandlers[NVAOS_MAX_HANDLERS];
NvBool               s_InterruptHandlersValid[NVAOS_MAX_HANDLERS];
NvU32                s_InterruptHandlerId[NVAOS_MAX_HANDLERS];

// for suspend/resume
NvU32 s_sysStackPtr = 0, g_saveIram = 0;
void *g_iramBufferAddress = NULL, *g_iramSourceAddress;
NvU32 g_iramSize;
volatile NvU32 g_syncAddr = 0;

/* for profiling */
NvU32 s_ExceptionAddr;

/* timer */
static NvU64 s_usec = 0;

// set to non-zero so that these won't go into the .bss
NvAosChip s_Chip       = NvAosChip_T30;
NvBool s_QuickTurnEmul = 0xa5;
NvBool s_Simulation    = 0xa5;  // set to non-zero so this won't be in .bss
NvBool s_FpgaEmulation = 0xa5;  // set to non-zero so this won't be in .bss
NvU32  s_Netlist       = 0xa5;  // set to non-zero so this won't be in .bss
NvU32  s_NetlistPatch  = 0xa5;  // set to non-zero so this won't be in .bss
NvU32  s_NumHwRegInt   = 5;

void *s_InterruptContext[NVAOS_MAX_HANDLERS];
static NvU32 s_IsIsr = 0;

// Define value of an invalid irq
static const IrqSource InvalidIrqSource = 0;

static volatile NvU32 *HwRegIntStatus[] = {
    (volatile NvU32 *)0x60004004,
    (volatile NvU32 *)0x60004104,
    (volatile NvU32 *)0x60004204,
    (volatile NvU32 *)0x60004304,
    (volatile NvU32 *)0x60004404
};

static volatile NvU32 *HwRegIntSet[] = {
    (volatile NvU32 *)0x60004034,
    (volatile NvU32 *)0x60004134,
    (volatile NvU32 *)0x60004234,
    (volatile NvU32 *)0x60004334,
    (volatile NvU32 *)0x60004434
};

static volatile NvU32 *HwRegIntClr[] = {
    (volatile NvU32 *)0x60004038,
    (volatile NvU32 *)0x60004138,
    (volatile NvU32 *)0x60004238,
    (volatile NvU32 *)0x60004338,
    (volatile NvU32 *)0x60004438
};

extern unsigned int Image$$ProgramStackRegion$$Base;
extern unsigned int Image$$ProgramStackRegionEnd$$Base;
extern unsigned int __end__;

// don't let this get into the .bss
MemoryMap s_MemoryMap = { 0xa5, 0xa5, 0xa5, 0xa5 };

void
nvaosGetMemoryMap( MemoryMap *map )
{
    map->ext_start = TXX_EXT_MEM_START;
    map->ext_end = TXX_EXT_MEM_END;
    map->mmio_start = TXX_MMIO_START;
    map->mmio_end = TXX_MMIO_END;
}

// All the info we know about a piece of hardware.
typedef struct
{
    NvRmModuleID Module;    // NvRm module type
    NvU8 Instance;          // Module instance
    NvU8 Index;             // Interrupt index within instance
    NvU8 Irq;               // Irq enable/disable bit position
    NvU8 ClockEnable;       // Clock enable bit position
    NvU8 Reset;             // reset bit position
    NvOsInterruptHandler hHandle; // Interrupt hander function
    void *context; // Interrupt handler argument
} Rm2IrqEntry;

// Can be sorted by highest priority Irq if necessary.
static Rm2IrqEntry s_Rm2Irqs[] =
{
    {NvRmModuleID_Timer,        0, 0,  0,  5, 0, 0, 0}, // TMR1 Primary Timer
    {NvRmModuleID_Timer,        1, 0,  1,  5, 0, 0, 0}, // TMR2 Secondary Timer
    {NvRmModuleID_ResourceSema, 0, 0,  4,  0, 0, 0, 0}, // IBF In Box Full
    {NvRmModuleID_ResourceSema, 0, 1,  5,  0, 0, 0, 0}, // IBE In Box Empty
    {NvRmModuleID_ResourceSema, 0, 2,  6,  0, 0, 0, 0}, // OBF Out Box Full
    {NvRmModuleID_ResourceSema, 0, 3,  7,  0, 0, 0, 0}, // OBE Out Box Empty
    {NvRmModuleID_Ucq,          0, 0,  8, 63, 0, 0, 0}, // UCQ UCQ Controller
    {NvRmModuleID_BseA,         0, 0,  9, 62, 0, 0, 0}, // BSE BSE Controller
    {NvRmModuleID_2D,           0, 0, 12, 21, 0, 0, 0}, // GE GE Controller
    {NvRmModuleID_Vcp,          0, 0, 25, 29, 0, 0, 0}, // VCP Reserved
    {NvRmPrivModuleID_Gpio,     0, 0, 32,  8, 0, 0, 0}, // GPIO1 Primary GPIO
    {NvRmPrivModuleID_Gpio,     1, 0, 33,  8, 0, 0, 0}, // GPIO2 Secondary GPIO
    {NvRmPrivModuleID_Gpio,     2, 0, 34,  8, 0, 0, 0}, // GPIO3 Tertiary GPIO
    {NvRmPrivModuleID_Gpio,     3, 0, 35,  8, 0, 0, 0}, // GPIO4 Quaternary GPIO
    {NvRmModuleID_Uart,         0, 0, 36,  6, 0, 0, 0}, // UART A UART A
    {NvRmModuleID_Uart,         1, 0, 37,  7, 0, 0, 0}, // UART B UART B
    {NvRmModuleID_Timer,        2, 0, 41,  5, 0, 0, 0}, // TMR3 Timer 3
    {NvRmModuleID_Timer,        3, 0, 42,  5, 0, 0, 0}, // TMR4 Timer 4
    {NvRmPrivModuleID_Gpio,     4, 0, 55,  8, 0, 0, 0}, // GPIO5 Quinary GPIO
    {NvRmModuleID_Vde,          0, 0, 9,   0, 0, 0, 0}, // VDE Sync Token Intr
    {NvRmModuleID_Vde,          0, 1, 10,  0, 0, 0, 0}, // VDE BSE-V Interrupt
    {NvRmModuleID_Vde,          0, 2, 11,  0, 0, 0, 0}, // VDE BSE-A Interrupt
    {NvRmModuleID_Vde,          0, 3, 12,  0, 0, 0, 0}, // VDE SXE Interrupt
    {NvRmModuleID_Vde,          0, 4, 8,   0, 0, 0, 0}, // VDE UCQ Error Intr
    {NvRmModuleID_Vde,          0, 5, 17,  0, 0, 0, 0}, // VDE Interrupt
    {NvRmModuleID_ArbitrationSema, 0, 0, 28,  0, 0, 0, 0}, // Arb Sema intr
    {NvRmModuleID_Usb2Otg,      3, 0, 97,  0, 0, 0, 0}, // Usb2Otg Intr
    {(NvRmModuleID)NvRmPrivModuleID_ApbDma, 0, 0, 60,  0, 0, 0, 0},
    {NvRmPrivModuleID_Gpio,     5, 0, 87,  8, 0, 0, 0}, // GPIO6 Senary GPIO
    {NvRmPrivModuleID_Gpio,     6, 0, 89,  8, 0, 0, 0}, // GPIO7 Septenary GPIO
    {NvRmPrivModuleID_Gpio,     7, 0, 125, 8, 0, 0, 0}, // GPIO8 Octonary GPIO
    {NvRmModuleID_GraphicsHost, 0, 0, 66,  0, 0, 0, 0}, // Host1X AVP
};

#if !__GNUC__
__asm NvU32 getCpsr( void )
{
    CODE32

    mrs r0, cpsr
    bx lr
}

__asm void setCpsr( NvU32 r )
{
    CODE32

    msr cpsr_csxf, r0
    bx lr
}

NvU32
disableInterrupts( void )
{
    NvU32 tmp;
    NvU32 intState;

    /* get current irq state */
    tmp = getCpsr();


    intState = tmp & (1<<7);  // get the interrupt enable bit
    tmp = tmp | (1<<7);

    /* disable irq */
    setCpsr( tmp );

    return intState;
}

void
restoreInterrupts( NvU32 state )
{
    NvU32 tmp;

    /* restore the old cpsr irq state */
    tmp = getCpsr();
    tmp = tmp & ~(1<<7);
    tmp = tmp | state;
    setCpsr( tmp );
}

void
enableInterrupts( void )
{
    NvU32 tmp;

    /* read/modify/write the cpsr */
    tmp = getCpsr();
    tmp = tmp & ~(1<<7); // clear the disable
    setCpsr( tmp );
}

/**
 * armv4 interrupt handling: do as much as possible in sys mode rather
 * than irq mode. don't try to atomically update cpsr and pc in one shot.
 * it will be ok to resore the cpsr with interrupts enabled just before
 * the thread register restore.
 */
__asm void
interruptHandler( void )
{
    PRESERVE8
    IMPORT g_CurrentThread
    IMPORT s_ExceptionAddr
    IMPORT nvaosProfTimerHandler
    IMPORT nvaosDecodeIrqAndDispatch

    CODE32

    /* switch to sys mode and spill most registers */
    msr cpsr_fxsc, #( MODE_DISABLE_INTR | MODE_SYS )
    stmfd sp!, {r0-r11,lr,pc} // leave the stack 8 byte aligned

    /* switch to irq mode, fixup lr, cpsr */
    msr cpsr_fxsc, #( MODE_DISABLE_INTR | MODE_IRQ )
    sub r0, lr, #4
    mrs r1, spsr

    /* save the exception address for profiler */
    ldr r2, =s_ExceptionAddr
    str lr, [r2]

    /* switch back to sys mode */
    msr cpsr_fxsc, #( MODE_DISABLE_INTR | MODE_SYS )

    /* fixup return address (overwrite pc in the spilled state) */
    str r0, [sp, #4 * 13]

    /* push the cpsr and r12 to round out the stack */
    stmfd sp!, {r1,r12}

    /* save the current thread's stack pointer */
    bl threadSave

    /* handle profiler */
    bl nvaosProfTimerHandler

    /* call the interrupt handler for the given interrupt */
    bl nvaosDecodeIrqAndDispatch

    /* interrupts are cleared at the source (timer handler, etc) */

    /* set the current thread state */
    bl threadSet

    /* unspill thread */
    ldmfd sp!, {r1,r12}

    /* check for thumb mode */
    tst r1, #(1 << 5)
    /* use bne to check for clear Z bit */
    bne thumb_unspill

    /* normal arm-mode unspill */
    msr cpsr_fxsc, r1
    ldmfd sp!, {r0-r11,lr,pc}

thumb_unspill
    /* clear the thumb bit from the cpsr, re-enable with bx */
    bic r1, r1, #(1 << 5)
    msr cpsr_fxsc, r1
    ldmfd sp!, {r0-r11,lr}

    /* switch to thumb mode */
    push {r0}
    add r0, pc, #1
    bx r0

    CODE16
    pop {r0,pc}
    ALIGN
}

__asm void
crashHandler( void )
{
    CODE32

    /* something bad happened */
crash
    b crash
}

/* prevent issues if there isn't a defined handler */
__weak void NvSwiHandler( int swi_num, int *pRegs )
{
}

/*
 * Lots of trickery in this function. An SVC call automatically
 * turns off interrupts. So in theory, anything called through
 * the SWIHandler should be atomic. But this is not the case
 * as SWI calls do things like wait on semaphores or grab locks
 * in the kernel causing context switches. As a result, we cannot
 * spill registers on the SVC stack at all (or we corrupt it
 * when context switches happen). To fix this, any stack operation
 * is done in SYS mode (ie we spill the registers right on the calling
 * thread's stack). Everything else, we continue to do in SVC mode.
 * The only other catch is that we need to be careful about restoring
 * the LR register for SYS and SVC as they are different.
 */
__asm void
swiHandler( void )
{
    IMPORT NvSwiHandler

    CODE32

    // Switch to SYS mode and spill the function args/scratch/return value
    msr cpsr_fsxc, #0xdf
    stmfd sp!, {r0-r3, r12, lr}

    // Store a pointer to the spilled registers in r1. This is the second
    // argument to the NvSwiHandler function
    mov r1, sp

    // Back to SVC mode
    msr cpsr_fsxc, #0xd3

    // Store the lr_svc register in r3
    mov r3, lr

    // Store SPSR
    mrs r0, spsr

    // Spill lr_svc and SPSR onto the SYS stack
    msr cpsr_fsxc, #0xdf
    stmfd sp!, {r0, r3}
    msr cpsr_fsxc, #0xd3

    // check for thumb mode, get the swi number (it's encoded into the
    // swi instruction). To be precise, the SWI is encoded in the
    // instruction stored at lr-2
    tst r0, #(1<<5)
    ldrneh r0, [lr,#-2]         // thumb instruction
    bicne r0, r0, #0xFF00
    ldreq r0, [lr,#-4]          // arm instruction
    biceq r0, r0, #0xFF000000

    /* if the swi is 0xab or 0x123456, then bail out */
    cmp r0, #0xab
    beq swi_unspill
    ldr r2, =0x123456
    cmp r0, r2
    beq swi_unspill

    // Switch to SYS mode and jump to the main SWI handler
    msr cpsr_fsxc, #0xdf
    bl NvSwiHandler

    // Restore the lr_svc and SPSR registers
    ldmfd   sp!, {r0, r3}

    // Switch to SVC mode
    msr cpsr_fsxc, #0xd3

    // patch SPSR
    msr spsr_cf, r0

    // patch lr_svc
    mov lr, r3

    // Back to SYS mode and restore everything else
    msr cpsr_fsxc, #0xdf
    ldmfd   sp!, {r0-r3, r12, lr}
    msr cpsr_fsxc, #0xd3

    // Done
    movs    pc, lr
swi_unspill

    // Switch to SYS and restore the lr_svc and SPSR registers
    msr cpsr_fsxc, #0xdf
    ldmfd   sp!, {r0, r3}          ; Get spsr from stack
    msr cpsr_fsxc, #0xd3
    msr spsr_cf, r0                ; Restore spsr spsr_cf
    mov lr, r3
    msr cpsr_fsxc, #0xdf
    ldmfd   sp!, {r0-r3, r12, lr}  ; Rest
    msr cpsr_fsxc, #0xd3
Swi_Address
    movs pc, lr ; We use this location as the SEMIHOST_VECTOR address
}

__asm void
undefHandler( void )
{
    CODE32

    /* an interrupt was raised without a handler */
hangsystem
    b hangsystem
}
#endif // !__GNUC__

void saveIram(void)
{
    if (g_iramBufferAddress)
        NvOsMemcpy(g_iramBufferAddress, g_iramSourceAddress, g_iramSize);

    g_saveIram = 0;
}

void restoreIram(void)
{
    if (g_iramBufferAddress)
        NvOsMemcpy(g_iramSourceAddress, g_iramBufferAddress, g_iramSize);

    g_iramBufferAddress = 0;
}

#if !__GNUC__
__asm void
suspend( void )
{
    IMPORT NvSwiHandler
    IMPORT nvaosWaitForInterrupt
    IMPORT s_sysStackPtr
    IMPORT NvOsDataCacheWriteback
    IMPORT saveIram
    IMPORT g_saveIram
    IMPORT g_iramSourceAddress
    IMPORT g_syncAddr
    PRESERVE8
    CODE32

    // Store everything in SYS mode
    stmfd sp!, {r0-r12, lr}

    // Now store the SVC registers: lr_svc and sp_svc

    // First switch to SVC mode
    msr cpsr_fsxc, #0xd3

    mov r0, lr
    mov r1, sp

    // switch back to SYS mode
    msr cpsr_fsxc, #0xdf

    // store the lr_svc and sp_svc regs
    stmfd sp!, {r0, r1}

    //Store the address of the current stackptr
    ldr r2, =s_sysStackPtr
    str sp, [r2]

    //Now, save IRAM if required (only in explicit LP0 case)

    //We use r4 to differentiate between implicit
    //and explicit LP0 cases. r4 = 0 is implicit
    //and r4 = 1 is explicit
    mov r4, #0
    ldr r0, =g_saveIram
    ldr r0, [r0]
    cmp r0, #0
    beq suspend_cont

    bl saveIram

    //Store the address of the resume function
    //at the start of IRAM
    ldr r0, =g_iramSourceAddress
    ldr r0, [r0]

    //Store the original contents
    //of the first word of iram in
    //case the explicit LP0 is aborted.
    ldr r3, [r0]

    ldr r1, =resume
    str r1, [r0]

    //Explicit LP0 => r4=1
    mov r4, #1

suspend_cont

    //Store some stuff we might
    //need in case we abort the
    //shutdown.
    stmfd sp!, {r0, r3, r4}

    //Flush the cache
    bl NvOsDataCacheWriteback

    //Check if we need to write
    //to sync address to notify CPU
    ldr r0, =g_syncAddr
    ldr r2, [r0]
    cmp r2, #0
    beq suspend_wfi

    //Notify the CPU that suspend is done
    //and then clear syncVal
    mov r1, #1
    str r1, [r2]
    mov r1, #0
    str r1, [r0]

suspend_wfi

    // jump into wfi. We're not coming back to nvaosSuspend
    // if it's a *real* suspend
    bl nvaosWaitForInterrupt

    // We're back here. So it wasn't a real suspend.
    // Abort the suspend operation.
    //If we're aborting an explicit suspend, then
    //we need to restore the first word of IRAM
    //that we overwrote with the resume function.
    //The restore value is stored in r3.
    ldmfd sp!, {r0, r3, r4}
    cmp r4, #0
    beq suspend_abort
    str r3, [r0]

suspend_abort

    // Blind restore svc regs. Who cares...
    ldmfd sp!, {r0, r1}

    // Restore the SYS regs
    ldmfd sp!, {r0-r12, lr}

    // done
    bx lr
}
#endif // !__GNUC__

void
nvaosSuspend( void )
{
#if NVAOS_SUSPEND_ENABLE
    g_SystemState = NvAosSystemState_Suspend;
    suspend();
    g_SystemState = NvAosSystemState_Running;
#endif
}

#if !__GNUC__
__asm
void resume( void )
{
    IMPORT nvaosTimerInit
    IMPORT s_sysStackPtr
    IMPORT initVectors
    IMPORT hackGetStack
    IMPORT avpCacheEnable
    IMPORT restoreIram
    PRESERVE8
    CODE32

    // We're back. Restore everything.

    //Restore the exception vectors
    bl initVectors

    //Switch to sys mode...
    msr cpsr_fsxc, #0xdf

    //Get a temporary stack
    bl hackGetStack
    mov sp, r0

    //Re-enable the cache
    bl avpCacheEnable

    //Restore the IRAM
    bl restoreIram

    // We need to switch to SYS mode and get back our real sp
    msr cpsr_fsxc, #0xdf

    ldr r0, =s_sysStackPtr
    ldr sp, [r0]

    // restore the SVC regs
    ldmfd sp!, {r0, r1}
    msr cpsr_fsxc, #0xd3
    mov lr, r0
    mov sp, r1
    msr cpsr_fsxc, #0xdf

    //Clear the sync address
    ldr r0, =g_syncAddr
    mov r1, #0
    str r1, [r0]

    // Notify CPU that AVP kernel is up.
    // CPU uses the reset vector as the first living sign of AVP kernel.
    // The value does not matter.
    ldr r0, =VECTOR_RESET
    ldr r1, =0xDECAF123
    str r1, [r0]

    // restore SYS regs
    ldmfd sp!, {r0-r12, lr}

    // done
    bx lr
}

__asm void
armSetupModes( NvU32 stack )
{
    CODE32

    /* save the return address */
    mov r3, lr

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

    bx r3
}

/* save the current thread's stack pointer
 */
__asm void
threadSave( void )
{
    IMPORT g_CurrentThread

    CODE32

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

    CODE32

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

    CODE32

    /* spill most of the registers */
    stmfd sp!, {r0-r11,lr,pc}

    /* save processor state */
    mrs r1, cpsr

    // Quick check to see if we're here in SYS mode
    // If we're not, then hang.
    tst r1, #(1 << 3) //If bit 3 == 0
    bne switch_continue

switch_hang
    b switch_hang

switch_continue
    /* make sure interrupts are disabled */
    orr r0, r1, #(1<<7)
    msr cpsr_fxsc, r0

    /* patch the return address (overwrite pc in the stack) */
    str lr, [sp, #4 * 13]

    /* spill cpsr and r12 (8 byte alignment) */
#if NVAOS_THUMB
    /* set the thumb bit in saved cpsr */
    orr r1, r1, #(1 <<5)
#endif
    stmfd sp!, {r1,r12}

    /* save the current thread's stack pointer */
    bl threadSave

    /* run the scheduler */
    bl nvaosScheduler

    /* set the thread stack */
    bl threadSet

    /* unspill thread registers */
    ldmfd sp!, {r1,r12}

    ldr lr, [sp, #4 * 13]

    /* check for thumb mode */
    tst r1, #(1 << 5)
    /* use bne to check for clear Z bit */
    bne threadswitch_thumb_unspill

    /* arm-mode unspill */
    msr cpsr_fxsc, r1
    ldmfd sp!, {r0-r11,lr,pc}

threadswitch_thumb_unspill
    /* clear the thumb bit from the cpsr, re-enable with bx */
    bic r1, r1, #(1 << 5)
    msr cpsr_fxsc, r1
    ldmfd sp!, {r0-r11,lr}

    /* switch to thumb mode */
    push {r0}
    add r0, pc, #1
    bx r0

    CODE16
    pop {r0,pc}
    ALIGN
}
#endif // !__GNUC__

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
nvaosGetStackSize( void )
{
    return NVAOS_STACK_SIZE;
}

// ARM V4 doesn't have CLZ :(
NvU32 NvAvpPlatBitFind(NvU32 Word)
{
    NvU32 Mask = 0x0000FFFF, LoBit = 0, Width = 16;

    // Mask from bottom till we find first bit.
    if (!Word)
        return 0;
    while (Width)
    {
        // If no one bits in low half, shift high-half down.
        if (!(Word & Mask))
        {
            Word = Word >> Width;
            LoBit += Width;
        }

        // Cut mask size in half
        Width /= 2;
        Mask = Mask >> Width;
    }

    return LoBit + 1;
}

// Make IRQ source handle.
static IrqSource MakeIrqSrc(NvU32 Irq, NvU32 Idx)
{
    return (IrqSource) (0x80000000 + (Irq << 8) + Idx);
}

// get table index for Irq number. Returns -1 if Irq not in table.
static NvS32 Irq2Idx(NvU32 Irq)
{
    // Could do some hashing here if table got too big.
    NvU32 i;
    for (i = 0; i < NV_ARRAY_SIZE(s_Rm2Irqs); i++)
    {
        // Look for the entry we need
        if (s_Rm2Irqs[i].Irq == Irq)
        {
            return i;
        }
    }

    return -1;
}

/**
 * calls the interrupt handler for the current interrupt.
 */
void
nvaosDecodeIrqAndDispatch( void )
{
    NvU32 IrqNum = 0;
    NvS32 IrqTableIdx;
    NvU32 Loop;

    for (Loop=0; Loop < s_NumHwRegInt; Loop++)
    {
        volatile NvU32 NewIrqs;
        NvU32          IrqBit;
        // Update current known IRQ status.
        NewIrqs = *HwRegIntStatus[Loop];

        // Add any new stuff to current and unreported tracking variables.
        IrqBit = NvAvpPlatBitFind(NewIrqs);
        if (IrqBit != 0)
        {
            IrqNum = IrqBit+32*Loop;
            break;
        }
    }

    if (IrqNum == 0)
        return;

    IrqNum--;   // make zero-based values

    IrqTableIdx = Irq2Idx(IrqNum);
    NV_ASSERT(IrqTableIdx >= 0);

    // flag this as isr time for semaphore implementation
    s_IsIsr = 1;

    // Signal semaphore for interrupts that have handlers
    if(s_Rm2Irqs[IrqTableIdx].hHandle)
    {
       s_Rm2Irqs[IrqTableIdx].hHandle(s_Rm2Irqs[IrqTableIdx].context);
    }

    s_IsIsr = 0;
}

#if !__GNUC__
void
nvaosThreadStackInit( NvOsThreadHandle thread, NvU32 *stackBase, NvU32
    words, NvBool exec )
{
    NvU32 *stack;
    NvU32 tmp;

    /* setup the thread stack, including the start function address */
    tmp = getCpsr();
    tmp |= MODE_SYS; // Set it forcibly to SYS mode if it isn't already

    /* need to manually enable interrupts (bit 7 in the cpsr) - 0 to enable,
     * 1 to disable.
     */
    tmp &= ~MODE_I;

    /* set thumb bit -- threadSwitch and the interruptHandler will never write
     * the cpsr with the thumb bit set. they will check for it and switch
     * to thumb mode with bx instead.
     */
#if NVAOS_THUMB
    tmp |= MODE_T;
#endif

    /* the initial stack is cpsr, r12, r0-r11, lr, pc */
    stack = stackBase + words;
    stack -= 16;
    /* clear registers */
    NvOsMemset( (unsigned char *)stack, 0, 16 * 4 );
    /* set the lr, pc, and cpsr */
    stack[14] = (NvU32)nvaosThreadKill;
    if( thread->wrap )
    {
        stack[15] = (NvU32)thread->wrap;
    }
    else
    {
        stack[15] = (NvU32)thread->function;
    }

    stack[0] = tmp;

    /* set r0 to the thread arg */
    stack[2] = (NvU32)(thread->args);

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
#endif // !__GNUC__

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
    NvU32 reg;

    /* need to use flow controller, no wfi support in arm7 */
    reg = NV_DRF_DEF(FLOW_CTLR, HALT_COP_EVENTS, MODE, FLOW_MODE_STOP_UNTIL_IRQ)
        | NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, IRQ_1, 1)
        | NV_DRF_NUM(FLOW_CTLR, HALT_COP_EVENTS, JTAG, AVP_JTAG_WAKEUP_ENABLE);
    AOS_REGW( FLOW_PA_BASE + FLOW_CTLR_HALT_COP_EVENTS_0, reg );
}

void nvaosThreadSwitch( void )
{
    threadSwitch();
}

/** assumes that interrupts are disabled */
void
ppiTimerInit( void )
{
    NvU32 dividend, divisor;
    NvU32 reg;

    /* must make sure 64bit variable is init to 0. Software keeping
     * track of upper 32bit works only if timer interrupt handling
     * occurs before lower 32bit overflows >1 time.
     */
    s_usec = 0;

    (void)measureT30OscFreq(&dividend, &divisor);

    /* enable clock to timer hardware */
    reg = 1 << 5; // tmr is bit 5
    AOS_REGW( CLK_RST_PA_BASE + CLK_RST_CONTROLLER_CLK_ENB_L_SET_0, reg );

    reg = NV_DRF_NUM( TIMERUS, USEC_CFG, USEC_DIVIDEND, dividend )
        | NV_DRF_NUM( TIMERUS, USEC_CFG, USEC_DIVISOR, divisor );
    AOS_REGW( TIMERUS_PA_BASE + TIMERUS_USEC_CFG_0, reg );

    /* use timer1 instead of timer0 (timer0 is for the main cpu)
     * start the timer - one shot
     */
    reg =   NV_DRF_DEF( TIMER, TMR_PTV, EN, ENABLE )
          | NV_DRF_NUM( TIMER, TMR_PTV, TMR_PTV, NVAOS_CLOCK_DEFAULT )
          |  NV_DRF_DEF( TIMER, TMR_PTV, PER, ENABLE ) ;
    AOS_REGW( TIMER1_PA_BASE, reg );

    // Freeze timers when in debug state.
    reg = AOS_REGR(TIMERUS_PA_BASE + TIMERUS_CNTR_FREEZE_0);
    reg = NV_FLD_SET_DRF_NUM(TIMERUS, CNTR_FREEZE, DBG_FREEZE_COP, 1, reg);
    AOS_REGW(TIMERUS_PA_BASE + TIMERUS_CNTR_FREEZE_0, reg);
}

void
ppiTimerHandler( void *context )
{
    NvU32 reg;

    // check if the interrupt is pending for us
    reg = AOS_REGR( LIC_INTERRUPT_PENDING(0) );
    if( (reg & (1 <<  AVP_IRQ_TIMER_1)) == 0 )
    {
        // timer interrupt isn't set, so bail out
        return;
    }

    // turn off the periodic interrupt and the timer temporarily
    reg = AOS_REGR(TIMER1_PA_BASE);
    reg &= ~(  NV_DRF_DEF( TIMER, TMR_PTV, PER, ENABLE )
             | NV_DRF_DEF( TIMER, TMR_PTV, EN, ENABLE  ));
    AOS_REGW(TIMER1_PA_BASE,reg);

    /* write a 1 to the intr_clr field to clear the interrupt */
    reg = NV_DRF_NUM( TIMER, TMR_PCR, INTR_CLR, 1 );
    AOS_REGW( TIMER1_PA_BASE + TIMER_TMR_PCR_0, reg );

    /* run the scheduler */
    nvaosScheduler();

    // If AOS is idle, don't enable the timer just suspend. When we resume,
    // we'll resume at this point and turn on the timer then
    if( g_SystemState == NvAosSystemState_Idle )
    {
        nvaosSuspend();
    }

    /* reset the timer */
    reg =  NV_DRF_DEF( TIMER, TMR_PTV, EN, ENABLE )
         | NV_DRF_NUM( TIMER, TMR_PTV, TMR_PTV, NVAOS_CLOCK_DEFAULT )
         | NV_DRF_DEF( TIMER, TMR_PTV, PER, ENABLE );
    AOS_REGW( TIMER1_PA_BASE, reg );
}

/* setup the ppi interrupt controller */
void
ppiInterruptSetup( void )
{
    NvU32 reg;

    reg = AOS_REGR( CLK_RST_PA_BASE + CLK_RST_CONTROLLER_RST_DEVICES_L_0 );
    if (NV_DRF_VAL( CLK_RST_CONTROLLER, RST_DEVICES_L, SWR_CPU_RST, reg ) )
    {
        /* clear everything if CPU is not running */
        reg = 0xFFFFFFFF;
        AOS_REGW( LIC_INTERRUPT_CLR(0), reg );
        AOS_REGW( LIC_INTERRUPT_CLR(1), reg );
    }

    /* enable timer interrupt */
    reg = (1 << AVP_IRQ_TIMER_1);
    AOS_REGW( LIC_INTERRUPT_SET(0), reg );
}

void avpCacheEnable( void )
{
    T30CacheEnable();
}

// Gets IRQ from an Irq source handle.
static NvU32 Src2Irq(IrqSource Irq)
{
    return (Irq >> 8) & 0xFF;
}

// Gets array index from an Irq source handle.
NvU32 Src2Idx(IrqSource Irq)
{
    return Irq & 0xFF;
}

IrqSource IrqSourceFromIrq(NvU32 Irq)
{
    NvU32 i;
    for (i = 0; i < NV_ARRAY_SIZE(s_Rm2Irqs); i++)
    {
        // Look for the entry we need
        if (s_Rm2Irqs[i].Irq == Irq)
        {
            return MakeIrqSrc(s_Rm2Irqs[i].Irq, i);
        }
    }
    return InvalidIrqSource;
}

/* install an interrupt handler for the given id */
void
nvaosInterruptInstall( NvU32 IrqArg, NvOsInterruptHandler Handle,
    void *context )
{
    NvU32 state;
    IrqSource Irq = IrqSourceFromIrq(IrqArg);

    // Determine which controller is needed.
    NvU32 IrqIdx = Src2Idx(Irq);
    NvU32 IrqNum = Src2Irq(Irq);
    NvU32 IrqCtrl = IrqNum / 32;

    // And which bit within the controller
    NvU32 IrqMask = 1 << (IrqNum % 32);

    state = disableInterrupts();

    NV_ASSERT(IrqCtrl < s_NumHwRegInt);

    if (Handle != 0)
    {
        s_Rm2Irqs[IrqIdx].hHandle = Handle;
        s_Rm2Irqs[IrqIdx].context = context;
        *HwRegIntSet[IrqCtrl] = IrqMask;
    }
    else
    {
        s_Rm2Irqs[IrqIdx].hHandle = 0;
        *HwRegIntClr[IrqCtrl] = IrqMask;
    }

    restoreInterrupts(state);
}

void
nvaosInterruptUninstall( NvU32 id )
{
    disableInterrupts();

    // uninstall the interrupt by installing a NULL handler
    nvaosInterruptInstall(id, (NvOsInterruptHandler)0, 0);

    enableInterrupts();
}

void nvaosCpuPreInit( void )
{
    NvU32 tmp;
    NvU32 id;
    NvU32 major;
    NvU32 minor;
    NvU32 stack;
    NvU32 irqStackSize;
    NvU32 i;

    // We'll give the IRQ/SVC stack 1/4th the static ProgramStackRegion
    irqStackSize = (((NvU32)&Image$$ProgramStackRegionEnd$$Base) -
        ((NvU32)&Image$$ProgramStackRegion$$Base)) >> 2;

    stack = ((NvU32)&Image$$ProgramStackRegion$$Base);

    // set SVC/IRQ stack to SYS_STACK+IRQ_SIZE-4
    stack += irqStackSize - 4;

    armSetupModes( stack );

    s_QuickTurnEmul = NV_FALSE;
    s_FpgaEmulation = NV_FALSE;
    s_Simulation = NV_FALSE;

    // check if we are on quickturn.
    tmp = AOS_REGR( MISC_PA_BASE + APB_MISC_GP_HIDREV_0 );
    id = NV_DRF_VAL( APB_MISC_GP, HIDREV, CHIPID, tmp );
    major = NV_DRF_VAL( APB_MISC_GP, HIDREV, MAJORREV, tmp );
    minor = NV_DRF_VAL( APB_MISC_GP, HIDREV, MINORREV, tmp );

    switch( id ) {
    case 0x30:
        s_Chip = NvAosChip_T30;
        s_NumHwRegInt = 5;
        break;
    case 0x35:
        s_Chip = NvAosChip_T114;
        s_NumHwRegInt = 5;
        break;
    case 0x40:
        s_Chip = NvAosChip_T124;
        s_NumHwRegInt = 5;
        break;
    default:
        NV_ASSERT( !"unknown chip!" );
        break;
    }

    /* Clear all the interrupt enable bits */
    for (i = 0; i < s_NumHwRegInt; i++)
    {
        *HwRegIntClr[i] = ~0;
    }

    tmp = AOS_REGR( MISC_PA_BASE + APB_MISC_GP_EMU_REVID_0);
    s_Netlist = NV_DRF_VAL( APB_MISC_GP, EMU_REVID, NETLIST, tmp);
    s_NetlistPatch = NV_DRF_VAL(APB_MISC_GP, EMU_REVID, PATCH, tmp);

    if( major == 0 )
    {
        if( s_Netlist == 0 )
        {
            s_Simulation = NV_TRUE;
        }
        else
        {
            if( minor == 0 )
            {
                s_QuickTurnEmul = NV_TRUE;
            }
            else
            {
                s_FpgaEmulation = NV_TRUE;
            }
        }
    }

    nvaosGetMemoryMap( &s_MemoryMap );

    /* interrupt init */
    ppiInterruptSetup();

    /* cache enable */
    avpCacheEnable();

    /* Enable JTAG */
    tmp = AOS_REGR(MISC_PA_BASE + APB_MISC_PP_CONFIG_CTL_0);
    tmp = NV_FLD_SET_DRF_DEF(APB_MISC, PP_CONFIG_CTL, JTAG, ENABLE, tmp);
    tmp = NV_FLD_SET_DRF_DEF(APB_MISC, PP_CONFIG_CTL, TBE, ENABLE, tmp);
    AOS_REGW(MISC_PA_BASE + APB_MISC_PP_CONFIG_CTL_0, tmp);
}

void nvaosCpuPostInit( void )
{
    /* install timer handler */
    nvaosInterruptInstall( AVP_IRQ_TIMER_1, ppiTimerHandler, 0 );
}

void nvaosTimerInit( void )
{
    ppiTimerInit();
}

void nvaosPageTableInit( void )
{
    /* no MMU on arm7 */
}

#if !__GNUC__
#include <rt_misc.h>

__value_in_regs  struct  __initial_stackheap
__user_initial_stackheap(unsigned R0, unsigned SP, unsigned R2,
    unsigned SL)
{
    // R0-R3 contain the return values
    // R0 - heap base
    // R1 - stack base
    // R2 - heap limit
    // R3 - stack limit

    struct __initial_stackheap config;

    config.heap_base   = (NvU32)&__end__;
    config.heap_limit  = ((NvU32)&Image$$ProgramStackRegion$$Base);
    config.heap_limit  -= 4;

    /* initial stack is 1k */
    config.stack_limit  = ((NvU32)&Image$$ProgramStackRegion$$Base);
    config.stack_base = ((NvU32)&Image$$ProgramStackRegionEnd$$Base);

    /* use the initial stack as the irq stack */
    //s_irqStack = (NvU32)config.stack_limit;

    return config;
}
#endif // !__GNUC__

void nvaosFinalSetup( void )
{
}

/** allocator */
Allocator s_Allocator;

void
nvaosAllocatorSetup( void )
{
    Allocator *a;
    NvU32 ExtMemStart;
    NvU32 ExtMemEnd;

    ExtMemStart = ((NvU32)&__end__) + 4;
    // make begin of allocation region 8-byte aligned.
    ExtMemStart = (ExtMemStart + 0x7) & ~0x7;
    ExtMemEnd = (NvU32)&Image$$ProgramStackRegion$$Base;

    a = &s_Allocator;
    a->size = ExtMemEnd - ExtMemStart;
    a->base = (NvU8 *)ExtMemStart;

    a->mspace = create_mspace_with_base( a->base, a->size, 0 );
}

void *
nvaosAllocate( NvU32 size, NvU32 align, NvOsMemAttribute attrib,
    NvAosHeap heap )
{
    Allocator *a = &s_Allocator;
    NvUPtr mem = 0;

    /* no allocations allowed in an interrupt handler */
    NV_ASSERT( !nvaosIsIsr() );

    if( !align )
    {
        align = 4;
    }

    /* check for valid alignment */
    NV_ASSERT( align == 4 || align == 8 || align == 16 || align == 32 );

    // FIXME: check attributes -- can only allocate uncached.

    if( heap == NvAosHeap_External )
    {
        NvU32 state;

        state = disableInterrupts();
        mem = (NvUPtr)mspace_memalign( a->mspace, align, size );
        restoreInterrupts(state);
    }
    else
    {
        NV_ASSERT( !"No internal memory" );
        return 0;
    }

    return (void*)mem;
}

/* This function should be used with caution.  It does not preserve the orignal
 * allocated alignment restrictions.  Use with care.
 */
void *
nvaosRealloc( void *ptr, NvU32 size )
{
    Allocator *a = &s_Allocator;
    NvUPtr mem;
    void *newptr;

    NV_ASSERT( ptr );

    mem = (NvUPtr)ptr;

    if( ( mem >= (NvU32)a->base ) &&
        ( mem <= (NvU32)a->base + a->size ) )
    {
        NvU32 state;

        state = disableInterrupts();
        newptr = mspace_realloc(a->mspace, ptr, size);
        restoreInterrupts(state);
    }
    else
    {
        NV_ASSERT( !"unknown memory aperture" );
        return 0;
    }

    return newptr;
}

void
nvaosFree( void *ptr )
{
    Allocator *a = &s_Allocator;
    NvUPtr mem;

    /* ptr may be null */
    if( ptr == 0 )
    {
        return;
    }

    mem = (NvUPtr)ptr;
    if( ( mem >= (NvU32)a->base ) &&
        ( mem <= (NvU32)a->base + a->size ) )
    {
        NvU32 state;

        state = disableInterrupts();
        mspace_free(a->mspace, ptr);
        restoreInterrupts(state);
        return;
    }

    NV_ASSERT( !"nvaosFree: freeing unknown memory pointer" );
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

NvS32
nvaosAtomicCompareExchange32(
    NvS32 *pTarget,
    NvS32 OldValue,
    NvS32 NewValue)
{
    NvS32 old;
    NvU32 state;

    state = nvaosDisableInterrupts();
    old = *pTarget;
    if (old == OldValue)
        *pTarget = NewValue;
    nvaosRestoreInterrupts(state);
    return old;
}

NvS32
nvaosAtomicExchange32(
    NvS32 *pTarget,
    NvS32 Value)
{
    NvS32 old;
    NvU32 state;

    state = nvaosDisableInterrupts();
    old = *pTarget;
    *pTarget = Value;
    nvaosRestoreInterrupts(state);
    return old;
}

NvS32
nvaosAtomicExchangeAdd32(
    NvS32 *pTarget,
    NvS32 Value)
{
    NvS32 old;
    NvU32 state;

    state = nvaosDisableInterrupts();
    old = *pTarget;
    *pTarget = old + Value;
    nvaosRestoreInterrupts(state);
    return old;
}

void
NvOsDataCacheWriteback( void )
{
    //See bug 428789
    NvOsDataCacheWritebackInvalidate();
}

void
NvOsDataCacheWritebackInvalidate( void )
{
    NvOsInstrCacheInvalidate();
}

void
NvOsDataCacheWritebackRange( void *start, NvU32 TotalLength )
{
}

void
NvOsDataCacheWritebackInvalidateRange( void *start, NvU32 TotalLength )
{
}

void
NvOsInstrCacheInvalidate( void )
{
    T30InstrCacheFlushInvalidate();
}

void
NvOsInstrCacheInvalidateRange( void *start, NvU32 length )
{
    // FIXME: implement?
}

void
NvOsFlushWriteCombineBuffer( void )
{
}

void
NvOsAVPSetIdle(NvU32 sourceAddress, NvU32 bufferAddress, NvU32 bufSize)
{
    g_SystemState = NvAosSystemState_Idle;
    g_saveIram = 1;
    g_iramSourceAddress = (void*)sourceAddress;
    g_iramBufferAddress = (void*)bufferAddress;
    g_iramSize = bufSize;
    g_syncAddr = bufferAddress + bufSize;
}

static void
thread_wrapper( void *v )
{
    NvOsThreadHandle t = g_CurrentThread;

    /* jump to user thread */
    t->function( t->args );

    /* threadKill/Yield will signal the join semaphore */
}


NvError
NvOsAVPThreadCreate(NvOsThreadFunction function, void *args,
    NvOsThreadHandle *thread, void *stackPtr, NvU32 stackSize)
{
    static NvU32 s_id = 2;

    NvOsThreadHandle t;
    NvU8 *stack;
    NvU32 state;

    NV_ASSERT( thread );
    NV_ASSERT( function );

    if (stackPtr == NULL)
    {
        return NvOsThreadCreateInternal(function, args, thread,
            NvOsThreadPriorityType_Normal);
    }

    NV_ASSERT( stackSize > 0 );

    t = NvOsAlloc(sizeof(NvOsThread));
    if( !t )
    {
        return NvError_InsufficientMemory;
    }

    stack = stackPtr;

    /* init thread members */
    nvaosThreadInit( t );

    t->wrap = thread_wrapper;
    t->function = function;
    t->args = args;
    t->magic = NVAOS_MAGIC;

    state = nvaosDisableInterrupts();

    /* init the stack */
    nvaosThreadStackInit( t, (NvU32 *)stack, stackSize / 4, NV_FALSE );

    t->id = s_id++;
    *thread = t;

    /* put the thread on the run queue */
    t->state = NvAosThreadState_Running;
    nvaosEnqueue( &t->sched_queue, &g_Runnable, NV_TRUE );

    nvaosRestoreInterrupts(state);

    return NvSuccess;
}

NvError
NvOsPhysicalMemMap( NvOsPhysAddr phys, size_t size,
    NvOsMemAttribute attrib, NvU32 flags, void **ptr )
{
    return T30PhysicalMemMap(phys, size, attrib, flags, ptr);
}

void
NvOsPhysicalMemUnmap( void *ptr, size_t size )
{
    T30PhysicalMemUnmap(ptr, size);
}

NvError
NvOsPageAlloc( size_t size, NvOsMemAttribute attrib,
    NvOsPageFlags flags, NvU32 protect, NvOsPageAllocHandle *descriptor )
{
    return NvError_NotImplemented;
}

void
NvOsPageFree( NvOsPageAllocHandle descriptor )
{
}

NvError
NvOsPageLock(void *ptr, size_t size, NvU32 protect,
    NvOsPageAllocHandle *descriptor)
{
    return NvError_NotImplemented;
}

NvError
NvOsPageMap( NvOsPageAllocHandle descriptor, size_t offset,
    size_t size, void **ptr )
{
    return NvError_NotImplemented;
}

NvError
NvOsPageMapIntoPtr( NvOsPageAllocHandle descriptor, void *pCallerPtr,
    size_t offset, size_t size)
{

    return NvError_NotImplemented;
}

void
NvOsPageUnmap(NvOsPageAllocHandle descriptor, void *ptr, size_t size)
{
}

NvOsPhysAddr
NvOsPageAddress( NvOsPageAllocHandle descriptor, size_t offset )
{
    return 0;
}

NvError
NvIntrRegister( const NvOsInterruptHandler* pIrqHandlerList,
    void* context, NvU32 IrqListSize,  const NvU32* pIrqList,
    NvOsInterruptHandle* handle, NvBool val)
{
    NvU32 i;
    NvU32 *h;

    if( !IrqListSize )
    {
        return NvError_BadParameter;
    }

    NV_ASSERT( pIrqList );
    NV_ASSERT( pIrqHandlerList );

    h = NvOsAlloc((IrqListSize + 1) * sizeof(NvU32));
    if (!h) return NvError_InsufficientMemory;

    h[0] = IrqListSize;
    for( i = 0; i < IrqListSize; i++ )
    {
        nvaosInterruptInstall( pIrqList[i], pIrqHandlerList[i], context );
        h[i+1] = pIrqList[i];
    }

    *handle = (NvOsInterruptHandle)h;
    return NvSuccess;
}

NvError NvIntrUnRegister(NvOsInterruptHandle hndl)
{
    NvU32 *h = (NvU32 *)hndl;
    NvU32 i;

    if(!hndl)
    {
        return NvError_BadParameter;
    }

    for( i = 0; i < h[0]; i++ )
    {
        nvaosInterruptUninstall( h[i+1] );
    }

    NvOsFree(h);
    return NvSuccess;
}

NvError
NvIntrSet(NvOsInterruptHandle hndl, NvBool val)
{
    return NvError_NotImplemented;
}
