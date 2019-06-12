/*
 * Copyright 2011-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 *
 * This module is a replacement for the target GCC toolchain libgcc.a
 *
 * It doesn't make sense to compile it with another compiler.
 * It must also be compiled for ARMv4T ARM, because it will be linked against
 * code targeted for AVP.
 */
#ifndef __GNUC__
#error This module must be compiled with a GCC toolchain!
#endif
#ifndef __ARMEL__
#error This module must be compiled with a GCC/ARM toolchain!
#endif
#ifdef __THUMBEL__
#error This module must be compiled in ARM mode, not Thumb mode!
#endif
#ifndef __ARM_ARCH_4T__
#error This module must be compiled for the ARMv4T architecture!
#endif

#include "nvcommon.h"
#include "nvassert.h"

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* referenced by inline assembler code directly */
static __attribute__((used)) void MyDivU32(NvU32 Dividend,
                                           NvU32 Divisor,
                                           NvU32 *pQuotient,
                                           NvU32 *pRemainder)
{
    NvU32 Shift = 0;
    NvU32 Quotient = 0;

    /* prevent division by zero */
    NV_ASSERT(Divisor != 0);
    if (Dividend < Divisor)
    {
        *pQuotient  = 0;
        *pRemainder = Dividend;
        return ;
    }

    /* prevent endless loop */
    NV_ASSERT(!((Divisor == 0x80000000) && (Dividend >= Divisor)));
    while (Dividend >= Divisor)
    {
        Divisor <<= 1;
        Shift++;
        if(Divisor & 0x80000000)
            break;
    }
    if (Dividend < Divisor)
    {
        Divisor >>= 1;
        Shift--;
    }

    do
    {
        Quotient <<= 1;
        if (Dividend >= Divisor)
        {
            Dividend -= Divisor;
            Quotient |= 1;
        }
        Divisor >>= 1;
    } while (Shift-- > 0);

    *pQuotient  = Quotient;
    *pRemainder = Dividend;
    return;
}

/* referenced by inline assembler code directly */
static __attribute__((used)) void MyDivI32(int Dividend,
                                           int Divisor,
                                           int *pQuotient,
                                           int *pRemainder)
{
  unsigned int dend, dor;
  unsigned int q, r;

  /* prevent division by zero */
  NV_ASSERT(Divisor != 0);

  dend = ABS(Dividend);
  dor  = ABS(Divisor);
  MyDivU32( dend, dor, &q, &r );

  *pQuotient = q;
  if (Dividend < 0) {
    *pRemainder = -r;
    if (Divisor > 0)
      *pQuotient = -q;
  }
  else { /* positive Dividend */
    *pRemainder = r;
    if (Divisor < 0)
      *pQuotient = -q;
  }

}

signed int __bswapsi2(signed int RegVal);
signed int
__bswapsi2(signed int RegVal)
{
  return ((((RegVal) & 0xff000000) >> 24)
          | (((RegVal) & 0x00ff0000) >>  8)
          | (((RegVal) & 0x0000ff00) <<  8)
          | (((RegVal) & 0x000000ff) << 24));
}

/* referenced by inline assembler code directly */
static __attribute__((used)) void MyDivUL64(NvU64 Dividend,
                                            NvU64 Divisor,
                                            NvU64 *pQuotient,
                                            NvU64 *pRemainder)
{
    NvU32 Shift = 0;
    NvU64 Quotient = 0;

    /* prevent division by zero */
    NV_ASSERT(Divisor != 0);
    if (Dividend < Divisor)
    {
        *pQuotient  = 0;
        *pRemainder = Dividend;
        return ;
    }

    /* prevent endless loop */
    NV_ASSERT(!((Divisor == 0x8000000000000000ULL) && (Dividend >= Divisor)));
    while (Dividend >= Divisor)
    {
        Divisor <<= 1;
        Shift++;
        if (Divisor & 0x8000000000000000ULL)
            break;
    }
    if (Dividend < Divisor)
    {
        Divisor >>= 1;
        Shift--;
    }

    do
    {
        Quotient <<= 1;
        if (Dividend >= Divisor)
        {
            Dividend -= Divisor;
            Quotient |= 1;
        }
        Divisor >>= 1;
    } while (Shift-- > 0);

    *pQuotient  = Quotient;
    *pRemainder = Dividend;
    return;
}

/* referenced by inline assembler code directly */
static __attribute__((used)) void MyDivIL64(long long Dividend,
                                            long long Divisor,
                                            long long *pQuotient,
                                            long long *pRemainder)
{
  unsigned long long dend, dor;
  unsigned long long q, r;

  /* prevent division by zero */
  NV_ASSERT(Divisor != 0);

  dend = ABS(Dividend);
  dor  = ABS(Divisor);
  MyDivUL64( dend, dor, &q, &r );

  *pQuotient = q;
  if (Dividend < 0) {
    *pRemainder = -r;
    if (Divisor > 0)
      *pQuotient = -q;
  }
  else { /* positive Dividend */
    *pRemainder = r;
    if (Divisor < 0)
      *pQuotient = -q;
  }

}

/*
 * Inline assembler routines
 */
void __aeabi_uidiv(void);
__attribute__((naked)) void __aeabi_uidiv(void)
{
  /*
   * 32-bit unsigned integer division
   *
   * in:
   * r0: numerator
   * r1: denominator
   *
   * MyDivU32() in:
   * r0: Dividend
   * r1: Divisor
   * r2: &pQuotient  (-> out WORD)
   * r3: &pRemainder (-> out WORD)
   "
   * out:
   * r0: quotient
   */
  asm volatile(
     "push {lr}          \n" // preserve lr                        sp = sp -  4
     "sub  sp, sp, #12   \n" // 2x32-bit + stack alignment padding sp = sp - 12
     "add  r2, sp, #4    \n" // &quotient  == sp + 4                == 16 bytes
     "mov  r3, sp        \n" // &remainder == sp
     "bl   MyDivU32      \n"
     "ldr  r0, [sp, #4]  \n" // retrieve quotient
     "add  sp, sp, #12   \n" // stack fixup
     "pop  {lr}          \n" // restore lr
     "bx   lr            \n" // return and restore previous state (ARM/Thumb)
  );
}

void __aeabi_idiv(void);
__attribute__((naked)) void __aeabi_idiv(void)
{
  /*
   * 32-bit signed integer division
   *
   * in:
   * r0: numerator
   * r1: denominator
   *
   * MyDivI32() in:
   * r0: Dividend
   * r1: Divisor
   * r2: &pQuotient  (-> out WORD)
   * r3: &pRemainder (-> out WORD)
   "
   * out:
   * r0: quotient
   */
  asm volatile(
     "push {lr}          \n" // preserve lr                        sp = sp -  4
     "sub  sp, sp, #12   \n" // 2x32-bit + stack alignment padding sp = sp - 12
     "add  r2, sp, #4    \n" // &quotient  == sp + 4                == 16 bytes
     "mov  r3, sp        \n" // &remainder == sp
     "bl   MyDivI32      \n"
     "ldr  r0, [sp, #4]  \n" // retrieve quotient
     "add  sp, sp, #12   \n" // stack fixup
     "pop  {lr}          \n" // restore lr
     "bx   lr            \n" // return and restore previous state (ARM/Thumb)
  );
}

void __aeabi_uldiv(void);
__attribute__((naked)) void __aeabi_uldiv(void)
{
  /*
   * 64-bit unsigned integer division
   *
   * in:
   * (r0, r1): numerator
   * (r2, r3): denominator
   *
   * MyDivUL64() in:
   * (r0, r1): Dividend
   * (r2, r3): Divisor
   * [sp, #0]: &pQuotient  (-> out DWORD)
   * [sp, #4]: &pRemainder (-> out DWORD)
   "
   * out:
   * (r0, r1): quotient
   */
  asm volatile(
     "push {lr}          \n" // preserve lr                        sp = sp -  4
     "sub  sp, sp, #28   \n" // 2x64-bit + 2xpointer + stack pad   sp = sp - 28
     "add  ip, sp, #16   \n" // &quotient  == sp + 16               == 32 bytes
     "str  ip, [sp]      \n"
     "add  ip, sp, #8    \n" // &remainder == sp +  8
     "str  ip, [sp, #4]  \n"
     "bl   MyDivUL64     \n"
     "ldr  r0, [sp, #16] \n" // retrieve quotient  - ldrd requires ARMv6!
     "ldr  r1, [sp, #20] \n"
     "add  sp, sp, #28   \n" // stack fixup
     "pop  {lr}          \n" // restore lr
     "bx   lr            \n" // return and restore previous state (ARM/Thumb)
  );
}

void __aeabi_ldiv(void);
__attribute__((naked)) void __aeabi_ldiv(void)
{
  /*
   * 64-bit signed integer division
   *
   * in:
   * (r0, r1): numerator
   * (r2, r3): denominator
   *
   * MyDivIL64() in:
   * (r0, r1): Dividend
   * (r2, r3): Divisor
   * [sp, #0]: &pQuotient  (-> out DWORD)
   * [sp, #4]: &pRemainder (-> out DWORD)
   "
   * out:
   * (r0, r1): quotient
   */
  asm volatile(
     "push {lr}          \n" // preserve lr                        sp = sp -  4
     "sub  sp, sp, #28   \n" // 2x64-bit + 2xpointer + stack pad   sp = sp - 28
     "add  ip, sp, #16   \n" // &quotient  == sp + 16               == 32 bytes
     "str  ip, [sp]      \n"
     "add  ip, sp, #8    \n" // &remainder == sp +  8
     "str  ip, [sp, #4]  \n"
     "bl   MyDivIL64     \n"
     "ldr  r0, [sp, #16] \n" // retrieve quotient  - ldrd requires ARMv6!
     "ldr  r1, [sp, #20] \n"
     "add  sp, sp, #28   \n" // stack fixup
     "pop  {lr}          \n" // restore lr
     "bx   lr            \n" // return and restore previous state (ARM/Thumb)
  );
}

void __aeabi_uidivmod(void);
__attribute__((naked)) void __aeabi_uidivmod(void)
{
  /*
   * 32-bit unsigned integer division with quotient & remainder result
   *
   * in:
   * r0: numerator
   * r1: denominator
   *
   * MyDivU32() in:
   * r0: Dividend
   * r1: Divisor
   * r2: &pQuotient  (-> out WORD)
   * r3: &pRemainder (-> out WORD)
   "
   * out:
   * r0: quotient
   * r1: remainder
   */
  asm volatile(
     "push {lr}          \n" // preserve lr                        sp = sp -  4
     "sub  sp, sp, #12   \n" // 2x32-bit + stack alignment padding sp = sp - 12
     "add  r2, sp, #4    \n" // &quotient  == sp + 4                == 16 bytes
     "mov  r3, sp        \n" // &remainder == sp
     "bl   MyDivU32      \n"
     "ldr  r1, [sp]      \n" // retrieve remainder
     "ldr  r0, [sp, #4]  \n" // retrieve quotient
     "add  sp, sp, #12   \n" // stack fixup
     "pop  {lr}          \n" // restore lr
     "bx   lr            \n" // return and restore previous state (ARM/Thumb)
  );
}

void __aeabi_idivmod(void);
__attribute__((naked)) void __aeabi_idivmod(void)
{
  /*
   * 32-bit signed integer division with quotient & remainder result
   *
   * in:
   * r0: numerator
   * r1: denominator
   *
   * MyDivI32() in:
   * r0: Dividend
   * r1: Divisor
   * r2: &pQuotient  (-> out WORD)
   * r3: &pRemainder (-> out WORD)
   "
   * out:
   * r0: quotient
   * r1: remainder
   */
  asm volatile(
     "push {lr}          \n" // preserve lr                        sp = sp -  4
     "sub  sp, sp, #12   \n" // 2x32-bit + stack alignment padding sp = sp - 12
     "add  r2, sp, #4    \n" // &quotient  == sp + 4                == 16 bytes
     "mov  r3, sp        \n" // &remainder == sp
     "bl   MyDivI32      \n"
     "ldr  r1, [sp]      \n" // retrieve remainder
     "ldr  r0, [sp, #4]  \n" // retrieve quotient
     "add  sp, sp, #12   \n" // stack fixup
     "pop  {lr}          \n" // restore lr
     "bx   lr            \n" // return and restore previous state (ARM/Thumb)
  );
}

void __aeabi_uldivmod(void);
__attribute__((naked)) void __aeabi_uldivmod(void)
{
  /*
   * 64-bit unsigned integer division with quotient & remainder result
   *
   * in:
   * (r0, r1): numerator
   * (r2, r3): denominator
   *
   * MyDivUL64() in:
   * (r0, r1): Dividend
   * (r2, r3): Divisor
   * [sp, #0]: &pQuotient  (-> out DWORD)
   * [sp, #4]: &pRemainder (-> out DWORD)
   "
   * out:
   * (r0, r1): quotient
   * (r2, r3): remainder
   */
  asm volatile(
     "push {lr}          \n" // preserve lr                        sp = sp -  4
     "sub  sp, sp, #28   \n" // 2x64-bit + 2xpointer + stack pad   sp = sp - 28
     "add  ip, sp, #16   \n" // &quotient  == sp + 16               == 32 bytes
     "str  ip, [sp]      \n"
     "add  ip, sp, #8    \n" // &remainder == sp +  8
     "str  ip, [sp, #4]  \n"
     "bl   MyDivUL64     \n"
     "ldr  r2, [sp, #8]  \n" // retrieve remainder - ldrd requires ARMv6!
     "ldr  r3, [sp, #12] \n"
     "ldr  r0, [sp, #16] \n" // retrieve quotient  - ldrd requires ARMv6!
     "ldr  r1, [sp, #20] \n"
     "add  sp, sp, #28   \n" // stack fixup
     "pop  {lr}          \n" // restore lr
     "bx   lr            \n" // return and restore previous state (ARM/Thumb)
  );
}

void __aeabi_ldivmod(void);
__attribute__((naked)) void __aeabi_ldivmod(void)
{
  /*
   * 64-bit signed integer division with quotient & remainder result
   *
   * in:
   * (r0, r1): numerator
   * (r2, r3): denominator
   *
   * MyDivIL64() in:
   * (r0, r1): Dividend
   * (r2, r3): Divisor
   * [sp, #0]: &pQuotient  (-> out DWORD)
   * [sp, #4]: &pRemainder (-> out DWORD)
   "
   * out:
   * (r0, r1): quotient
   * (r2, r3): remainder
   */
  asm volatile(
     "push {lr}          \n" // preserve lr                        sp = sp -  4
     "sub  sp, sp, #28   \n" // 2x64-bit + 2xpointer + stack pad   sp = sp - 28
     "add  ip, sp, #16   \n" // &quotient  == sp + 16               == 32 bytes
     "str  ip, [sp]      \n"
     "add  ip, sp, #8    \n" // &remainder == sp +  8
     "str  ip, [sp, #4]  \n"
     "bl   MyDivIL64     \n"
     "ldr  r2, [sp, #8]  \n" // retrieve remainder - ldrd requires ARMv6!
     "ldr  r3, [sp, #12] \n"
     "ldr  r0, [sp, #16] \n" // retrieve quotient  - ldrd requires ARMv6!
     "ldr  r1, [sp, #20] \n"
     "add  sp, sp, #28   \n" // stack fixup
     "pop  {lr}          \n" // restore lr
     "bx   lr            \n" // return and restore previous state (ARM/Thumb)
  );
}

void __aeabi_llsl(void);
__attribute__((naked)) void __aeabi_llsl(void)
{
  /*
   * 64-bit unsigned logical left shift
   *
   * in:
   * (r0, r1): input
   * r2:       count
   *
   * out:
   * (r0, r1): output
   */
  asm volatile(
     "b    2f         \n"
     "1:              \n"
     "adds r0, r0, r0 \n" // input <<= 1
     "adc  r1, r1     \n"
     "sub  r2, r2, #1 \n" // count--
     "2:              \n"
     "cmp  r2, #0     \n" // count == 0 ?
     "bne  1b         \n"
     "bx   lr         \n" // return and restore previous state (ARM/Thumb)
  );
}

void __aeabi_llsr(void);
__attribute__((naked)) void __aeabi_llsr(void)
{
  /*
   * 64-bit unsigned logical right shift
   *
   * in:
   * (r0, r1): input
   * r2:       count
   *
   * out:
   * (r0, r1): output
   */
  asm volatile(
     "b    2f         \n"
     "1:              \n"
     "lsrs r1, r1, #1 \n" // input >>= 1
     "rrx  r0, r0     \n"
     "sub  r2, r2, #1 \n" // count--
     "2:              \n"
     "cmp  r2, #0     \n" // count == 0 ?
     "bne  1b         \n"
     "bx   lr         \n" // return and restore previous state (ARM/Thumb)
  );
}

/*
 * The remainder can be handled with plain C code
 */
int __aeabi_ulcmp(unsigned long long x,
                  unsigned long long y);
int __aeabi_ulcmp(unsigned long long x,
                  unsigned long long y)
{
  long long diff = x - y;
  return (diff > 0) ? 1 : (diff ? -1 : 0);
}

#ifdef ANDROID
/*
 * The code against which this module is linked must be compiled without
 * exception handling (-fno-exceptions). So these functions should never
 * be called, but they are still required by the linker.
 */
void __aeabi_unwind_cpp_pr0(void);
void __aeabi_unwind_cpp_pr0(void) { }
void __aeabi_unwind_cpp_pr1(void);
void __aeabi_unwind_cpp_pr1(void) { }
void __aeabi_unwind_cpp_pr2(void);
void __aeabi_unwind_cpp_pr2(void) { }
#endif
