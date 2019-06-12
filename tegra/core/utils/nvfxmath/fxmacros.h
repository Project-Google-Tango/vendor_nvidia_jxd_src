/*
 * Copyright (c) 2005 - 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#if !defined(NV_FXMACROS_H)
#define NV_FXMACROS_H

#if defined(__GNUC__)
#define DW        .word
#define DB        .byte
#define LABEL(N)  N:
#define LOCLBL(N) N:
#define LBLFWD(N) N##f
#define LBLBCK(N) N##b
#define END
#define EXPORT    .global
#define SECTION
#define TEXT      .text
#define COLON     :
#define IMPORT_SYM(symName)
#else
#define DW        DCD
#define DB        DCB
#define LABEL(N)  N
#define LOCLBL(N) N
#define LBLFWD(N) %f##N
#define LBLBCK(N) %b##N
#define END       END
#define EXPORT    EXPORT
#define SECTION   AREA
#define TEXT      |.text|,CODE
#define COLON
#define IMPORT_SYM(symName)  IMPORT symName
#endif

#if defined(__GNUC__) && defined(PROFILE_BUILD)
#define FUNCTION_PROFILER function_profiler
.macro function_profiler
  stmdb sp!, {lr}
  bl    mcount
  ldmia sp!, {lr}
.endm
#else
#define FUNCTION_PROFILER
#endif

#if defined(__GNUC__)
#if defined(ARMV4I)
.macro cntlz dst, src, tmp
  mov   \tmp, \src          
  movs  \dst, \src, LSR #16
  mov   \dst, #0          
  moveq \tmp, \tmp, LSL #16
  orreq \dst, \dst, #16    
  tst   \tmp, #0xff000000 
  moveq \tmp, \tmp, LSL #8 
  orreq \dst, \dst, #8     
  tst   \tmp, #0xf0000000 
  moveq \tmp, \tmp, LSL #4 
  orreq \dst, \dst, #4     
  tst   \tmp, #0xc0000000 
  moveq \tmp, \tmp, LSL #2 
  orreq \dst, \dst, #2     
  cmp   \tmp, #0           
  orrge \dst, \dst, #1     
  addeq \dst, \dst, #1
.endm
#else
.macro cntlz dst, src, tmp
  clz   \dst, \src
.endm
#endif
#else
#if defined(ARMV4I)
  MACRO
  cntlz $dst, $src, $tmp
  mov   $tmp, $src          
  movs  $dst, $src, LSR #16
  mov   $dst, #0          
  moveq $tmp, $tmp, LSL #16
  orreq $dst, $dst, #16    
  tst   $tmp, #0xff000000 
  moveq $tmp, $tmp, LSL #8 
  orreq $dst, $dst, #8     
  tst   $tmp, #0xf0000000 
  moveq $tmp, $tmp, LSL #4 
  orreq $dst, $dst, #4     
  tst   $tmp, #0xc0000000 
  moveq $tmp, $tmp, LSL #2 
  orreq $dst, $dst, #2     
  cmp   $tmp, #0           
  orrge $dst, $dst, #1     
  addeq $dst, $dst, #1
  MEND
#else
  MACRO
  cntlz $dst, $src, $tmp
  clz   $dst, $src
  MEND
#endif
#endif

#endif // !defined(NV_FXMACROS_H)

