/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "avp.h"
#include "aos.h"
#include "aos_common.h"

#if !__GNUC__
#pragma arm section code = "MainRegion", rwdata = "MainRegion", rodata = "MainRegion", zidata ="MainRegion"
#endif // !__GNUC__

extern unsigned int Image$$ProgramStackRegionEnd$$Base;

#define ENABLE_JTAG_DEBUG 0

#if ENABLE_JTAG_DEBUG
#define JtagEnableBitField 192
#define JtagReg 0x70000024
#endif

unsigned int hackGetStack( void );
unsigned int hackGetStack( void )
{
    unsigned int stack = (unsigned int)&Image$$ProgramStackRegionEnd$$Base;
    stack = stack - 4;
    return stack;
}

#if !__GNUC__
__asm void _start(void)
{
    PRESERVE8
    IMPORT __scatterload
    IMPORT nvaosInit
    ALIGN
    CODE32

hackStartHandler
    b realstart
    /* writing signature in avp executables */
    DCD 0xdeadbeef
realstart
    /* make sure we are in system mode with interrupts disabled */
    msr cpsr_fsxc, #( MODE_DISABLE_INTR | MODE_SYS )

    /* set the stack in case it wasn't set */
    bl initVectors
    bl hackGetStack
    mov sp, r0

#if ENABLE_JTAG_DEBUG
    ; enable JTAG
    ldr r0, =JtagReg
    ldr r1, =JtagEnableBitField
    str r1, [r0]
#endif

    /* intialize AOS */
    bl nvaosInit

    ldr  r1, =__scatterload
    bx   r1

}

__asm void initVectors(void)
{
    IMPORT hackGetStack
    IMPORT interruptHandler
    IMPORT swiHandler
    IMPORT crashHandler
    ALIGN
    CODE32

    /* VECTOR_RESET is modified later */
    /* CPU is polling it as the first living sign of AVP */

    /* install vectors */
    ldr r0, =VECTOR_UNDEF
    ldr r1, =crashHandler
    str r1, [r0]

    ldr r0, =VECTOR_PREFETCH_ABORT
    ldr r1, =crashHandler
    str r1, [r0]

    ldr r0, =VECTOR_DATA_ABORT
    ldr r1, =crashHandler
    str r1, [r0]

    ldr r0, =VECTOR_IRQ
    ldr r1, =interruptHandler
    str r1, [r0]

    ldr r0, =VECTOR_FIQ
    ldr r1, =crashHandler
    str r1, [r0]

    ldr r0, =VECTOR_SWI
    ldr r1, =swiHandler
    str r1, [r0]

    /* enable the evt */
    ldr r0, =(1 << 4);
    ldr r1, =AVP_CACHE_PA_BASE
    ldr r2, [r1]
    orr r2, r0
    str r2, [r1]

    bx lr
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
