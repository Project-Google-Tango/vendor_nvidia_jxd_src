/*
 * Copyright (c) 2005 - 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "fxmacros.h"

  SECTION TEXT

  EXPORT  NvSFxDiv
  EXPORT  NvSFxMod
  EXPORT  NvSFxRecip

  //---------------------------------------------------------------------------
  // S15.16 fixed-point mod routine
  //
  // C prototype: NvSFx NvSFxMod(NvSFx numerator, NvSFx divisor);
  //
  // input:  r0 = dividend
  //         r1 = divisor
  // output: r0 = mod(dividend, divisor)
  //
  // About 97 cycles per call on a PXA-255.
  //
  // This function is equivalent to the ANSI-C library function fmod(). The 
  // mod function is implemented via a call to the core division routine 
  // followed by subtracting the product of the integer part of the quotient 
  // and the divisor from the dividend.
  //
  // No attempt is made to handle overflow or division by zero. If overflow or 
  // division by 0 occurs during division, the result is undefined.
  //---------------------------------------------------------------------------
NvSFxMod COLON
  str   lr, [sp, #-4]!      // save link register on stack
  str   r0, [sp, #-4]!      // save dividend
  str   r1, [sp, #-4]!      // save divisor

  bl    NvSFxDiv            // 

  ldr   r2, [sp], #4        // restore divisor
  ldr   r3, [sp], #4        // restore dividend

  eor   r1, r0, r0, ASR #31 // compute absolute value
  sub   r0, r1, r0, ASR #31 //  of quotient
  eor   r1, r2, r2, ASR #31 // compute absolute value
  sub   r2, r1, r2, ASR #31 //  of divisor

  cmp   r3, #0x80000000     // dividend < 0 ?
  mov   r0, r0, LSR #16     // trunc(abs(quotient))
  mul   r1, r0, r2          // (trunc(abs(quot)) * (abs(divisor)))
  ldr   lr, [sp], #4        // return address stored in top of stack
  addcs r0, r3, r1          // rem = dividend+(trunc(abs(quot))*(abs(divisor)))
  subcc r0, r3, r1          // rem = dividend-(trunc(abs(quot))*(abs(divisor)))

  bx    lr                  // return

  //---------------------------------------------------------------------------
  // S15.16 fixed-point reciprocal routine
  //
  // C prototype: NvSFx NvSFxRecip(NvSFx divisor);
  //
  // input:  r0 = divisor
  // output: r0 = quotient
  //
  // No attempt is made to handle overflow or division by zero. If overflow or
  // division by 0 occurs, the result is undefined. The rounding mode "round to
  // zero" is applied to the result, i.e. truncation.
  //
  // Executes in 55 cycles per call on PXA-255.
  //
  // The underlying algorithm is Newton-Raphson iteration for the reciprocal:
  // x = x * (2 - d * x). Using a 9-bit initial approximation, a full precision
  // result is achieved in two iterations. This result is exact, except for
  // divisors of 2, for which it is too small by 1. An on-the-fly correction
  // is applied in these cases.
  //
  //---------------------------------------------------------------------------

NvSFxRecip COLON
  orrs   r3, r0, r0          // r3<31> = reciprocal sign = divisor sign
  rsbmi  r0, r0, #0          // oldy = labs (divisor)
  cntlz  r1, r0, r2          // lz = clz(divisor)
  movs   r0, r0, LSL r1      // y = oldy << lz; check whether y == 0
  addne  r2, pc, r0, LSR #24 // offset into table
  ldrneb r2, [r2, #LBLFWD(1)-LBLFWD(3)-128] // table lookup on top divisor bits
3 COLON
  bxeq   lr                  // if (!y) return y
  orr    r2, r2, #0x100      // r = rtab[(y >> 24) - 128] | 0x100
  str    lr, [sp, #-4]!      // save link register on stack
  mov    r2, r2, LSL #23     // r << 23
  umull  ip, lr, r2, r0      // prod = ((unsigned __int64)y) * (r << 23)
  rsb    lr, lr, #0          // f = -(unsigned)(prod >> 32)
  umull  ip, lr, r2, lr      // prod = ((unsigned __int64)(r << 24)) * f
  mov    r2, ip, LSR #31     //
  orr    r2, r2, lr, LSL #1  // r = (unsigned)(prod >> 31)
  umull  ip, lr, r2, r0      // prod = ((unsigned __int64)y) * r
  rsb    lr, lr, #0          // f = -(unsigned)(prod >> 32)
  umull  ip, lr, r2, lr      // prod = ((unsigned __int64)r) * f
  mov    r2, r0, LSR r1      // oldy = y >> lz
  rsb    r1, r1, #30         // 30 - lz
  sub    ip, r2, #1          // oldy - 1
  mov    r0, lr, LSR r1      // r = (unsigned)(prod >> 32); r = r >> (30 - lz)
  ldr    lr, [sp], #4        // retrieve return address stored in top of stack
  ands   r2, r2, ip          // oldy & (oldy - 1)
  addeq  r0, r0, #1          // if (!(oldy & (oldy - 1))) r++

  eor    r0, r0, r3, ASR #31 // negate reciprocal
  sub    r0, r0, r3, ASR #31 //  if divisor was negative
  
  bx     lr                  // return 


// Table entries below are constructed as follows:
//
// for (k = 0; k < 128; k++) {
//    rtab[k]=(unsigned char)((((1.0/(1.0+(k+0.5)/128.0)))-0.5)*512+0.5);
// }
//
// The above code partitions the interval [1,2) into 128 subintervals of
// equal size, and stores a 9-bit approximation of the reciprocal of the
// the subinterval's midpoint. Since the result is guaranteed to be in
// (0.5, 1), the most significant bit (of numeric weight 0.5) does not 
// need to be stored since it is always set and thus stored implicitly.
// 
// NOTE: For the reciprocal code, we could probably make do with an 8-bit 
// approximation, in which case we would use:
//
// for (k = 0; k < 128; k++) {
//    rtab[k]=(unsigned char)((((1.0/(1.0+(k+0.5)/128.0))))*256+0.5);
// }


1 COLON
  DB 0xfe
  DB 0xfa
  DB 0xf6
  DB 0xf2
  DB 0xef
  DB 0xeb
  DB 0xe7
  DB 0xe4
  DB 0xe0
  DB 0xdd
  DB 0xd9
  DB 0xd6
  DB 0xd2
  DB 0xcf
  DB 0xcc
  DB 0xc9
  DB 0xc6
  DB 0xc2
  DB 0xbf
  DB 0xbc
  DB 0xb9
  DB 0xb6
  DB 0xb3
  DB 0xb1
  DB 0xae
  DB 0xab
  DB 0xa8
  DB 0xa5
  DB 0xa3
  DB 0xa0
  DB 0x9d
  DB 0x9b
  DB 0x98
  DB 0x96
  DB 0x93
  DB 0x91
  DB 0x8e
  DB 0x8c
  DB 0x8a
  DB 0x87
  DB 0x85
  DB 0x83
  DB 0x80
  DB 0x7e
  DB 0x7c
  DB 0x7a
  DB 0x78
  DB 0x75
  DB 0x73
  DB 0x71
  DB 0x6f
  DB 0x6d
  DB 0x6b
  DB 0x69
  DB 0x67
  DB 0x65
  DB 0x63
  DB 0x61
  DB 0x5f
  DB 0x5e
  DB 0x5c
  DB 0x5a
  DB 0x58
  DB 0x56
  DB 0x54
  DB 0x53
  DB 0x51
  DB 0x4f
  DB 0x4e
  DB 0x4c
  DB 0x4a
  DB 0x49
  DB 0x47
  DB 0x45
  DB 0x44
  DB 0x42
  DB 0x40
  DB 0x3f
  DB 0x3d
  DB 0x3c
  DB 0x3a
  DB 0x39
  DB 0x37
  DB 0x36
  DB 0x34
  DB 0x33
  DB 0x32
  DB 0x30
  DB 0x2f
  DB 0x2d
  DB 0x2c
  DB 0x2b
  DB 0x29
  DB 0x28
  DB 0x27
  DB 0x25
  DB 0x24
  DB 0x23
  DB 0x21
  DB 0x20
  DB 0x1f
  DB 0x1e
  DB 0x1c
  DB 0x1b
  DB 0x1a
  DB 0x19
  DB 0x17
  DB 0x16
  DB 0x15
  DB 0x14
  DB 0x13
  DB 0x12
  DB 0x10
  DB 0x0f
  DB 0x0e
  DB 0x0d
  DB 0x0c
  DB 0x0b
  DB 0x0a
  DB 0x09
  DB 0x08
  DB 0x07
  DB 0x06
  DB 0x05
  DB 0x04
  DB 0x03
  DB 0x02
  DB 0x01

  //---------------------------------------------------------------------------
  // S15.16 fixed-point division routine
  //
  // C prototype: NvSFx NvSFxDiv(NvSFx numerator, NvSFx divisor);
  //
  // input:  r0 = dividend
  //         r1 = divisor
  // output: r0 = quotient
  //
  // No attempt is made to handle overflow or division by zero. If overflow or
  // division by 0 occurs, the result is undefined. The rounding mode "round to
  // zero" is applied to the result, i.e. truncation.
  //
  // Executes in 74 cycles per call on PXA-255.
  //
  // The underlying algorithm is Newton-Raphson iteration for the reciprocal:
  // x = x * (2 - d * x). Using a 9-bit initial approximation, a full precision
  // reciprocal result in U1.31 format is achieved in two iterations. This 
  // reciprocal is multiplied by the dividend and the result is normalized to 
  // U16.16 format. This results in an approximation to the quotient that has 
  // error of 0, -1, or -2 ulps. The quotient approximation is incremenented by
  // one and multiplied by the divisor, the resulting product subtracted from
  // the dividend. If the remainder is negative, we need to decrement quotient,
  // if it is greater or equal to the divisor, we need to increment quotient.
  // 
  // The very clever code for the second Netwon iteration is adapted from 
  // "ARM System Developer's Guide" by A.N. Sloss, D. Symes, and C. Wright. 
  // It performs a 34-bit by 31-bit multiply with the minimum number of
  // instructions and registers (I was unable to improve on that code).
  //
  //---------------------------------------------------------------------------

NvSFxDiv COLON
  eor    r2, r1, r1, ASR #31 // compute absolute value
  sub    r2, r2, r1, ASR #31 //  of divisor
  eor    r3, r0, r1          // r3<31> = quotient sign

  cntlz  r1, r2, ip          // clz(abs(divisor))

  // r0 = dividend, r1 = clz(abs(divisor)), r2 = abs(divisor), r3 = quot sign

  movs   r2, r2, LSL r1      // normalize divisor
  bxeq   lr                  // return if divisor is 0
  str    lr, [sp, #-4]!      // save link register on stack
  add    ip, pc, r2, LSR #24 // offset into table
  ldrb   ip, [ip, #LBLBCK(1)-LBLFWD(2)-128] // table lookup on top divisor bits
2 COLON
  orrs   r0, r0, r0          // dividend negative ?
  rsbmi  r0, r0, #0          // abs(dividend)
  add    ip, ip, #0x100      // 9-bit estimate of reciprocal
  mul    lr, ip, ip          // recip*recip
  str    r0, [sp, #-4]!      // save abs(dividend) on stack

  // r2 = abs(divisor), lr = recip*recip, ip=recip
  umull  r0, lr, r2, lr      // lr:r0 = recip*recip*divisor [fits in 49 bits]
  rsb    r0, lr, ip, LSL #9  // recip = 2*recip - recip*recip*divisor [U1.16]

  umull  ip, lr, r0, r0      // lr:ip = recip*recip
  movs   ip, ip, LSR #1      // a = recip*recip/2
  adc    ip, ip, lr, LSL #31 //  rounded
  movs   lr, lr, LSL #30     //
  umull  lr, ip, r2, ip      // a=a*d
  addmi  ip, ip, r2          // if (N) += 2^32*d
  rsc    r0, ip, r0, LSL #16 // recip = 2*recip - recip*recip*divisor

  ldr    ip, [sp], #4        // abs (dividend)
  mov    r2, r2, LSR r1      // abs (divisor)
  rsb    r1, r1, #31         // 31 - clz(divisor)

  // r0 = U1.31 estimate to 1/divisor
  // r1 = clz(abs(divisor))
  // r2 = abs(divisor)
  // r3 = quotient sign
  // ip = abs(dividend)

  umull  r0, lr, ip, r0      // lr:r0 = approx = dividend * recip(divisor)
  
  mov    r0, r0, LSR #16     // t = (approx >> 16)
  orr    r0, r0, lr, LSL #16 //
  mov    lr, lr, LSR #16     // 
  mov    r0, r0, LSR r1      // quot = t >> (31 - clz(divisor))
  rsb    r1, r1, #32         //
  mov    lr, lr, LSL r1      //
  orr    r0, r0, lr          //

  // r0 = quotient, ip = abs(dividend), r2 = abs(divisor)

  add    r0, r0, #1          // quot + 1
  umull  lr, r1, r2, r0      // r1:lr = (quot + 1) * abs(divisor)

  rsbs   lr, lr, ip, LSL #16 //
  rscs   r1, r1, ip, LSR #16 // dividend - (quot + 1) * divisor
  sbc    r0, r0, #0          // if (dividend-(quot+1)*divisor) < 0, quot--

  subcss lr, lr, r2          // rem -= divisor
  adccs  r0, r0, #0          // if (dividend-(quot+1)*divisor)>= divisor,quot++
  ldr    lr, [sp], #4        // return address stored in top of stack

  eor    r0, r0, r3, ASR #31 // negate quotient
  sub    r0, r0, r3, ASR #31 //  if negative

  bx     lr                  // return to caller

  END
