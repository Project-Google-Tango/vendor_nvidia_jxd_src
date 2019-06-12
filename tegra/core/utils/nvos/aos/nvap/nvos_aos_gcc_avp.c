/*
 * Copyright (c) 2010-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA CORPORATION
 * is strictly prohibited.
 */

#if __GNUC__

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "avp.h"
#include "aos.h"
#include "nvrm_drf.h"
#include "bootloader.h"
#include "aos_avp.h"
#include "nvrm_drf.h"
#include "nvbl_arm_cp15.h"
#include "arpg.h"

#define TIMERUS_PA_BASE 0x60005010
NvU32 g_MbTimeStamp = 11;
extern unsigned int __bss_start__, __bss_end__, __end__;

NvU32 timerusaddr = TIMERUS_PA_BASE;

void zInit(void);
void zInit(void)
{
    NvU8* cur = (NvU8*)&__bss_start__;
    NvU8* end = (NvU8*)&__bss_end__;

    // Clear any bytes up to the first word boundary.
    while ( (((NvU32)cur) & (sizeof(NvU32) - 1)) && (cur < end) )
    {
        *(cur++) = 0;
    }

    // Do 4-word write for as long as we can.
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

NV_NAKED void StoreTimeStamp(void)
{
    asm volatile(
    "mov     r0, %1                                                 \n"
    "ldr     r2, [r0, #0]                                           \n"
    "ldr     r0, =g_MbTimeStamp                                    \n"
    "str     r2, [r0]                                               \n"
    "bx lr                                                          \n"
    :
     "=&r"(g_MbTimeStamp)
    :
      "r"(timerusaddr)
    : "r0", "r2",  "cc", "memory"
    );
}

NV_NAKED void initVectors(void)
{
    asm volatile(
    /* install vectors */
    "ldr r0, =(0x6000F200 + 0)          \n" // VECTOR_RESET
    "ldr r1, =hackStartHandler          \n"
    "str r1, [r0]                       \n"

    "ldr r0, =(0x6000F200 + 4)          \n" // VECTOR_UNDEF
    "ldr r1, =crashHandler              \n"
    "str r1, [r0]                       \n"

    "ldr r0, =(0x6000F200 + 12)         \n" // VECTOR_PREFETCH_ABORT
    "ldr r1, =crashHandler              \n"
    "str r1, [r0]                       \n"

    "ldr r0, =(0x6000F200 + 16)         \n" // VECTOR_DATA_ABORT
    "ldr r1, =crashHandler              \n"
    "str r1, [r0]                       \n"

    "ldr r0, =(0x6000F200 + 24)         \n" // VECTOR_IRQ
    "ldr r1, =interruptHandler          \n"
    "str r1, [r0]                       \n"

    "ldr r0, =(0x6000F200 + 28)         \n" // VECTOR_FIQ
    "ldr r1, =crashHandler              \n"
    "str r1, [r0]                       \n"

    "ldr r0, =(0x6000F200 + 8)          \n" // VECTOR_SWI
    "ldr r1, =swiHandler                \n"
    "str r1, [r0]                       \n"

    /* enable the evt */
    "ldr r0, =(1 << 4);                 \n"
    "ldr r1, =(0x6000C000)              \n" // AVP_CACHE_PA_BASE
    "ldr r2, [r1]                       \n"
    "orr r2, r0                         \n"
    "str r2, [r1]                       \n"

    "bx lr                              \n"
    );
}

NV_NAKED __attribute__((section (".startsection"))) void _start(void);
NV_NAKED __attribute__((section (".startsection"))) void _start(void)
{
    asm volatile (
    "hackStartHandler:          \n"
    "b realstart               \n"
    /* writing signature in avp exe */
    ".word 0xdeadbeef           \n"
    "realstart:                \n"
    /* make sure we are in system mode with interrupts disabled */
    "msr cpsr_fsxc, %0          \n"

    /* set the stack in case it wasn't set */
    "bl initVectors             \n"
    "bl hackGetStack            \n"
    "mov sp, r0                 \n"
    "bl StoreTimeStamp          \n"
    "bl zInit                   \n"
    :
    :"I"(MODE_DISABLE_INTR | MODE_SYS )
    );

#if ENABLE_JTAG_DEBUG
    /* enable JTAG */
    asm volatile (
    "ldr r0, =%0                \n"
    "ldr r1, =%1                \n"
    "str r1, [r0]               \n"
    :
    :"I"(JtagReg)
    :"I"(JtagEnableBitField)
    );
#endif

    /* intialize AOS */
    asm volatile (
    "bl nvaosInit               \n"
    "b  main                    \n"
    );
}

NV_NAKED void
armSetupModes( NvU32 stack )
{
    asm volatile(
    /* save the return address */
    "mov r3, lr                 \n"

    /* svc mode */
    "msr cpsr_fsxc, %0          \n"
    "mov sp, r0                 \n"

    /* abt mode */
    "msr cpsr_fsxc, %1          \n"
    "mov sp, r0                 \n"

    /* undef mode */
    "msr cpsr_fsxc, %2          \n"
    "mov sp, r0                 \n"

    /* irq mode */
    "msr cpsr_fsxc, %3          \n"
    "mov sp, r0                 \n"

    /* system mode */
    "msr cpsr_fsxc, %4          \n"

    "bx r3                      \n"
    :
    :"I"(MODE_DISABLE_INTR | MODE_SVC)
    ,"I"(MODE_DISABLE_INTR | MODE_ABT)
    ,"I"(MODE_DISABLE_INTR | MODE_UND)
    ,"I"(MODE_DISABLE_INTR | MODE_IRQ)
    ,"I"(MODE_DISABLE_INTR | MODE_SYS)
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

void
enableInterrupts( void )
{
    NvU32 tmp;

    asm volatile (
    "mrs %[tmp], cpsr\n"
    :[tmp]"=r" (tmp)
    );

    tmp = tmp & ~(1<<7);

    asm volatile(
    "msr cpsr_csxf, %[tmp]\n"
    :
    :[tmp]"r" (tmp)
    );
}

NV_NAKED void
waitForInterrupt( void )
{
    //asm volatile("wfi\n");
    asm volatile("bx lr\n");
}

NV_NAKED void
interruptHandler( void )
{
    asm volatile(
    "           \n"
    /* switch to sys mode and spill most registers */
    "msr cpsr_fxsc, %0                  \n"
    "stmfd sp!, {r0-r11,lr,pc}          \n" // leave the stack 8 byte aligned

    /* switch to irq mode, fixup lr, cpsr */
    "msr cpsr_fxsc, %1                  \n"
    "sub r0, lr, #4                     \n"
    "mrs r1, spsr                       \n"

    /* save the exception address for profiler */
    "ldr r2, =s_ExceptionAddr           \n"
    "str lr, [r2]                       \n"

    /* switch back to sys mode */
    "msr cpsr_fxsc, %2                  \n"

    /* fixup return address (overwrite pc in the spilled state) */
    "str r0, [sp, #4 * 13]              \n"

    /* push the cpsr and r12 to round out the stack */
    "stmfd sp!, {r1,r12}                \n"

    /* save the current thread's stack pointer */
    "bl threadSave                      \n"

    /* handle profiler */
    // "bl nvaosProfTimerHandler                \n"

    /* call the interrupt handler for the given interrupt */
    "bl nvaosDecodeIrqAndDispatch       \n"

    /* interrupts are cleared at the source (timer handler, etc) */

    /* set the current thread state */
    "bl threadSet                       \n"

    /* unspill thread */
    "ldmfd sp!, {r1,r12}                \n"

    /* normal arm-mode unspill */
    "msr cpsr_fxsc, r1                  \n"
    "ldmfd sp!, {r0-r11,lr,pc}          \n"
    :
    : "I"(MODE_DISABLE_INTR | MODE_SYS)
    , "I"(MODE_DISABLE_INTR | MODE_IRQ)
    , "I"(MODE_DISABLE_INTR | MODE_SYS)
    );
}

NV_NAKED void
crashHandler( void )
{
    asm volatile("b .\n");
}

void NvSwiHandler( int swi_num, int *pRegs )
{
}

NV_NAKED void swiHandler(void)
{
    asm volatile(
    // Switch to SYS mode and spill the function args/scratch/return value
    "msr cpsr_fsxc, #0xdf               \n"
    "stmfd sp!, {r0-r3, r12, lr}        \n"

    // Store a pointer to the spilled registers in r1. This is the second
    // argument to the NvSwiHandler function
    "mov r1, sp                         \n"

    // Back to SVC mode
    "msr cpsr_fsxc, #0xd3               \n"

    // Store the lr_svc register in r3
    "mov r3, lr                         \n"

    // Store SPSR
    "mrs r0, spsr                       \n"

    // Spill lr_svc and SPSR onto the SYS stack
    "msr cpsr_fsxc, #0xdf               \n"
    "stmfd sp!, {r0, r3}                \n"
    "msr cpsr_fsxc, #0xd3               \n"

    // check for thumb mode, get the swi number (it's encoded into the
    // swi instruction). To be precise, the SWI is encoded in the
    // instruction stored at lr-2
    "tst r0, #(1<<5)                    \n"
    "ldrneh r0, [lr,#-2]                \n"     // thumb instruction
    "bicne r0, r0, #0xFF00              \n"
    "ldreq r0, [lr,#-4]                 \n"     // arm instruction
    "biceq r0, r0, #0xFF000000          \n"

    /* if the swi is 0xab or 0x123456, then bail out */
    "cmp r0, #0xab                      \n"
    "beq swi_unspill                    \n"
    "ldr r2, =0x123456                  \n"
    "cmp r0, r2                         \n"
    "beq swi_unspill                    \n"

    // Switch to SYS mode and jump to the main SWI handler
    "msr cpsr_fsxc, #0xdf               \n"
    "bl NvSwiHandler                    \n"

    // Restore the lr_svc and SPSR registers
    "ldmfd   sp!, {r0, r3}              \n"

    // Switch to SVC mode
    "msr cpsr_fsxc, #0xd3               \n"

    // patch SPSR
    "msr spsr_cf, r0                    \n"

    // patch lr_svc
    "mov lr, r3                         \n"

    // Back to SYS mode and restore everything else
    "msr cpsr_fsxc, #0xdf               \n"
    "ldmfd   sp!, {r0-r3, r12, lr}      \n"
    "msr cpsr_fsxc, #0xd3               \n"

    // Done
    "movs    pc, lr                     \n"
    "swi_unspill:                       \n"

    // Switch to SYS and restore the lr_svc and SPSR registers
    "msr cpsr_fsxc, #0xdf               \n"
    "ldmfd   sp!, {r0, r3}              \n"     // Get spsr from stack
    "msr cpsr_fsxc, #0xd3               \n"
    "msr spsr_cf, r0                    \n"     // Restore spsr spsr_cf
    "mov lr, r3         \n"
    "msr cpsr_fsxc, #0xdf               \n"
    "ldmfd   sp!, {r0-r3, r12, lr}      \n"     // Rest
    "msr cpsr_fsxc, #0xd3               \n"
    "Swi_Address:                       \n"
    "movs pc, lr                        \n"     // e use this location as the SEMIHOST_VECTOR address
    );
}

NV_NAKED void
undefHandler( void )
{
    /* an interrupt was raised without a handler */
    asm volatile("b .\n");
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
    /* spill most of the registers */
    "stmfd sp!, {r0-r11,lr,pc}          \n"

    /* save processor state */
    "mrs r1, cpsr                       \n"

    // Quick check to see if we're here in SYS mode
    // If we're not, then hang.
    "tst r1, #(1 << 3)                  \n" //If bit 3 == 0
    "bne switch_continue                \n"

    "switch_hang:                       \n"
    "b switch_hang                      \n"

    "switch_continue:                   \n"
    /* make sure interrupts are disabled */
    "orr r0, r1, #(1<<7)                \n"
    "msr cpsr_fxsc, r0                  \n"

    /* patch the return address (overwrite pc in the stack) */
    "str lr, [sp, #4 * 13]              \n"

    /* spill cpsr and r12 (8 byte alignment) */
    "stmfd sp!, {r1,r12}                \n"

    /* save the current thread's stack pointer */
    "bl threadSave                      \n"

    /* run the scheduler */
    "bl nvaosScheduler                  \n"

    /* set the thread stack */
    "bl threadSet                       \n"

    /* unspill thread registers */
    "ldmfd sp!, {r1,r12}                \n"

    "ldr lr, [sp, #4 * 13]              \n"

    /* arm-mode unspill */
    "msr cpsr_fxsc, r1                  \n"
    "ldmfd sp!, {r0-r11,lr,pc}          \n"
    );
}

void
nvaosThreadStackInit( NvOsThreadHandle thread, NvU32 *stackBase, NvU32
    words, NvBool exec )
{
    NvU32 *stack;
    NvU32 tmp;

    /* setup the thread stack, including the start function address */
    asm volatile("mrs %0, cpsr" : "=r" (tmp));
    tmp |= 0xf; //Set it forcibly to SYS mode if it isn't already

    /* need to manually enable interrupts (bit 7 in the cpsr) - 0 to enable,
     * 1 to disable.
     */
    tmp &= ~( 1 << 7 );

    /* set thumb bit -- threadSwitch and the interruptHandler will never write
     * the cpsr with the thumb bit set. they will check for it and switch
     * to thumb mode with bx instead.
     */
    //tmp |= (1 << 5 );

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

NV_NAKED void suspend(void)
{
    // Not implemented.
    for (;;)
        ;
}

#endif //__GNUC__
