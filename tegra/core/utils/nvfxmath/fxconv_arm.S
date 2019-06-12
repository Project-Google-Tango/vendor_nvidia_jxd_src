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

  EXPORT  NvSFxFloat2Fixed
  EXPORT  NvSFxFixed2Float

NvSFxFloat2Fixed COLON
#ifdef EABI_IS_HARD
  fmrs   r0, s0
//  vmod   r0, s0
#endif
  mov    r3, r0              // cvt.i
  mov    r2, r0, LSR #23     // ((unsigned)cvt.i >> 23)
  and    r1, r2, #0xff       // exp = ((unsigned)cvt.i >> 23) & 0xff
  cmp    r1, #0x6d           // (exp < (IEEE_SGL_EXPO_BIAS - 17)) ? 
  mov    r0, #0              // res = 0
  bxle   lr                  // if (exp < (IEEE_SGL_EXPO_BIAS - 17)) return res
  mov    r2, r3, LSL #8      // cvt.i << 8
  orr    r0, r2, #0x80000000 // res = (cvt.i << 8) | 0x80000000
  rsb    r2, r1, #0x8d       // IEEE_SGL_EXPO_BIAS + 14 - exp
  mov    ip, r0, LSR r2      // res=((unsigned)res)>>(IEEE_SGL_EXPO_BIAS+14-exp)
  add    r2, ip, #1          // res = ((unsigned int)(res + 1))
  mov    r0, r2, LSR #1      // res = ((unsigned int)(res + 1)) >> 1
  cmp    r3, #0              // (cvt.i >> 31) ?
  rsblt  r0, r0, #0          // (cvt.i >> 31) ? -res : res
  cmp    r1, #0x8d           // (exp >= (IEEE_SGL_EXPO_BIAS + 15) ?
  bxle   lr                  //  
  cmp    r3, #0              // (cvt.i >> 31) ? 
  mvnge  r0, #0x80000000     // 
  movlt  r0, #0x80000000     // (cvt.i >> 31) ? 0x80000000 : 0x7FFFFFFF
  bx     lr                  //

NvSFxFixed2Float COLON
  movs   r1, r0              // (s < 0) ?
  bxeq   lr                  // if (!s) return 0.0f;
  movmi  r0, #0xC7000000     // 
  movpl  r0, #0x47000000     // res = (s < 0) ? 0xC7000000 : 0x47000000
  rsbmi  r1, r1, #0          // u = (s < 0) ? -s : s;
  cntlz  r3, r1, r2          // lz(u)
  mov    r2, r1, LSL r3      // t = u << lz(u)
  sub    r0, r0, r3, LSL #23 // res -= (lz(u) << 23)
  and    r3, r2, #0xFF       // t & 0xFF
  cmp    r3, #0x80           // (t & 0xFF) == 0x80
  moveq  r1, r2, LSR #8      // u = (t >> 8)
  movne  r1, r2, LSR #7      // u = (t >> 7)
  and    r1, r1, #1          // u = u & 1
  add    r2, r2, r2          // t << 1
  add    r1, r1, r2, LSR #9  // u += ((t << 1) >> 9)
  add    r0, r0, r1          // res += u
#ifdef EABI_IS_HARD
  fmsr   s0, r0
//  vmov   s0, r0
#endif
  bx     lr                  // 

  END
