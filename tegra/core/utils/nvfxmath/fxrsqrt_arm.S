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

  EXPORT  NvSFxRecipSqrt
  EXPORT  NvSFxSqrt
  EXPORT  NvSFxNormalize3

NvSFxRecipSqrt COLON
  cntlz r1, r0, r2          // lz = clz(x)
  and   r1, r1, #0xfffffffe // lz = lz & 0xfffffffe
  movs  r2, r0, LSL r1      // x = x << lz
  movne r3, r2, LSR #25     // x >> 25
  addne r3, pc, r3, LSL #2  // 
  ldrne r3, [r3,#LBLFWD(5)-LBLFWD(3)-128] // r = f = cubeTab[(x >> 25) - 32]
3 COLON
  bxeq  lr                  // if (!x) return x
  mov   r1, r1, LSR #1      // lz >> 1
  umull r0, ip, r2, r3      // prod = ((unsigned __int64)x) * f
  add   r3, r3, r3, LSL #1  // 3 * r
  rsb   r3, ip, r3, LSL #22 // r = ((3 * r) << 22) - (unsigned)(prod >> 32)
  umull r0, ip, r3, r3      // prod = ((unsigned __int64)r) * r; s = (unsigned)(prod >> 32);
  rsb   r1, r1, #18         // 18 - (lz >> 1)
  umull r0, ip, r2, ip      // prod = ((unsigned __int64)x) * s
  rsb   ip, ip, #0x30000000 // f = 0x30000000 - (unsigned)(prod >> 32)
  umull r0, r3, ip, r3      // prod = ((unsigned __int64)r) * f; r = (unsigned)(prod >> 32)
  mov   r2, #1              // 1
  add   r0, r2, r3, LSR r1  // r >> (18 - (lz >> 1)) + 1
  mov   r0, r0, LSR #1      // ((r >> (18 - (lz >> 1))) + 1) >> 1
  bx    lr                  // return

  // The table below is computed by the following computation, which stores
  // an 8-bit approximation to 1/sqrt(x) as well as the 24-bit cube of that
  // approximation in a 32-bit word. The two least significant bits of the
  // cube are cleared to allow the use of the data without additional masking.
  //
  // for (k = 0; k < 32; k++) {
  //     x = (((1.0/sqrt(1.0+(k+0.5)/32.0)))*256+0.5);
  //     s = (unsigned char)x;
  //     c = (unsigned int)s * s * s;
  //     cubeTab[k] = ((c & (~3)) << 8) | s;
  //     printf ("  DW 0x%08x\n", cubeTab[k]);
  // }
  // for (k = 0; k < 64; k++) {
  //     x = (((1.0/sqrt(2.0+(k+0.5)/32.0)))*256+0.5);
  //     s = (unsigned char)x;
  //     c = (unsigned int)s * s * s;
  //     cubeTab[k+32] = ((c & (~3)) << 8) | s;
  //     printf ("  DW 0x%08x\n", cubeTab[k+32]);
  // }

5 COLON
  DW 0xfa0bf8fe
  DW 0xee6b28fa
  DW 0xe5f024f7
  DW 0xdaf268f3
  DW 0xd2f000f0
  DW 0xc890c0ec
  DW 0xc10378e9
  DW 0xb9a758e6
  DW 0xb4da40e4
  DW 0xadcea0e1
  DW 0xa6f278de
  DW 0xa279c0dc
  DW 0x9beb48d9
  DW 0x97a5c4d7
  DW 0x916340d4
  DW 0x8d4fc8d2
  DW 0x895000d0
  DW 0x8563b8ce
  DW 0x818ac0cc
  DW 0x7dc4e8ca
  DW 0x7a1200c8
  DW 0x7671d8c6
  DW 0x72e440c4
  DW 0x6f6908c2
  DW 0x6db240c1
  DW 0x6a523cbf
  DW 0x670424bd
  DW 0x6563c0bc
  DW 0x623028ba
  DW 0x609ce8b9
  DW 0x5d8364b7
  DW 0x5bfd18b6
  DW 0x58fd40b4
  DW 0x5783a8b3
  DW 0x560e48b2
  DW 0x533000b0
  DW 0x51c70caf
  DW 0x506238ae
  DW 0x4da4c0ac
  DW 0x4c4c10ab
  DW 0x4af768aa
  DW 0x49a6b8a9
  DW 0x485a00a8
  DW 0x471134a7
  DW 0x45cc58a6
  DW 0x434e40a4
  DW 0x4214f8a3
  DW 0x40df88a2
  DW 0x3fade0a1
  DW 0x3e8000a0
  DW 0x3d55dc9f
  DW 0x3c2f789e
  DW 0x3c2f789e
  DW 0x3b0cc49d
  DW 0x39edc09c
  DW 0x38d2609b
  DW 0x37baa89a
  DW 0x36a68899
  DW 0x35960098
  DW 0x34890497
  DW 0x34890497
  DW 0x337f9896
  DW 0x3279ac95
  DW 0x31774094
  DW 0x30784893
  DW 0x30784893
  DW 0x2f7cc892
  DW 0x2e84b091
  DW 0x2d900090
  DW 0x2d900090
  DW 0x2c9eac8f
  DW 0x2bb0b88e
  DW 0x2bb0b88e
  DW 0x2ac6148d
  DW 0x29dec08c
  DW 0x29dec08c
  DW 0x28fab08b
  DW 0x2819e88a
  DW 0x2819e88a
  DW 0x273c5889
  DW 0x273c5889
  DW 0x26620088
  DW 0x258ad487
  DW 0x258ad487
  DW 0x24b6d886
  DW 0x24b6d886
  DW 0x23e5fc85
  DW 0x23184084
  DW 0x23184084
  DW 0x224d9883
  DW 0x224d9883
  DW 0x21860882
  DW 0x21860882
  DW 0x20c18081
  DW 0x20c18081
  DW 0x20000080


  //---------------------------------------------------------------------------
  // S15.16 fixed-point square root implemented via reciprocal square root. The
  // reciprocal square root is computed by a table-based starting approximation
  // followed by two Newton-Raphson iterations. An 8-bit approximation "r" is
  // retrieved from the table, along with the 24-bit cube of the approximation,
  // "r_cubed". The table lookup uses some most significant bits of the input 
  // argument as the index. The first NR iteration computes a refined estimate 
  // r' = 1.5 * r - x * r_cubed. The second iteration then computes the final
  // reciprocal square root as r" = 0.5 * r' * (3 - x * r' * r'). The square 
  // root is then approximated by sqrt = x * r". The remainder is computed to 
  // check whether this square root is exact and the preliminary result is
  // adjusted if necessary.
  //---------------------------------------------------------------------------
NvSFxSqrt COLON
  cntlz r1, r0, r2          // lz = clz(x)
  and   r1, r1, #0xfffffffe // lz = lz & 0xfffffffe
  movs  r2, r0, LSL r1      // x = oldx << lz
  movne r3, r2, LSR #25     // x >> 25
  addne r3, pc, r3, LSL #2  // each table entry comprises 4 bytes
  ldrne r3, [r3,#LBLBCK(5)-LBLFWD(2)-128] // r = f = cubeTab[(x >> 25) - 32]
2 COLON
  umull r0, ip, r2, r3      // prod = ((unsigned __int64)x) * f
  add   r3, r3, r3, LSL #1  // 3 * r
  rsb   r3, ip, r3, LSL #22 // r = ((3 * r) << 22) - (unsigned)(prod >> 32)
  umull r0, ip, r3, r3      // prod = ((unsigned __int64)r) * r; s = (unsigned)(prod >> 32);
  umull r0, ip, r2, ip      // prod = ((unsigned __int64)x) * s
  rsb   ip, ip, #0x30000000 // f = 0x30000000 - (unsigned)(prod >> 32)
  umull r0, r3, ip, r3      // prod = ((unsigned __int64)r) * f; r = (unsigned)(prod >> 32)
  mov   ip, r2, LSR r1      // oldx = x >> lz
  umull r0, r3, r2, r3      // prod = ((unsigned __int64)x) * r
  mov   r1, r1, LSR #1      // lz >> 1
  mov   r2, #4              // 4
  add   r0, r2, r3, LSR r1  // (r >> (lz >> 1)) + 0x4
  mov   r0, r0, LSR #3      // r = ((r >> (lz >> 1)) + 0x4) >> 3
  mul   r1, r0, r0          // r * r
  rsb   r2, r1, ip, LSL #16 // rem = (oldx << 16) - (r * r)
  add   r0, r0, r2, ASR #31 // rem += (rem >> 31)
  bx    lr                  // return

  //---------------------------------------------------------------------------
  // NvSFxNormalize3 normalizes a 3-element input vector, i.e. produces a unit
  // length vector. There are no overflows in intermediate computation for all
  // S15.16 inputs. Each of the output elements has an error of at most one ulp
  // compared to a reference function that performs intermediate computations
  // in double precision floating-point. Zero length vectors are returned as is.
  //---------------------------------------------------------------------------
NvSFxNormalize3 COLON
  str   r4, [sp, #-4]!      // save according to calling convention
  str   r5, [sp, #-4]!      // save according to calling convention

  ldr   r2, [r0]            // iv[0]
  smull r3, r4, r2, r2      // (NV_FX_LONGLONG)iv[0]) * iv[0]
  ldr   r2, [r0, #4]        // iv[1]
  smlal r3, r4, r2, r2      // ((NV_FX_LONGLONG)iv[0]) * iv[0]) + ((NV_FX_LONGLONG)iv[1]) * iv[1])
  ldr   r2, [r0, #8]        // iv[2]
  smlal r3, r4, r2, r2      // ((NV_FX_LONGLONG)iv[0]) * iv[0]) + ((NV_FX_LONGLONG)iv[1]) * iv[1]) + ((NV_FX_LONGLONG)iv[2]) * iv[2])

  // r = (unsigned)(prod >> 32)
  // s = (unsigned)(prod & 0xffffffff)

  movs  r2, r4              // lz = r
  moveq r2, r3              // if (!r) lz = s
  cntlz r2, r2, ip          // lz = clz(lz)
  and   ip, r2, #0xfffffffe // f = lz & 0xfffffffe
  cmp   r4, #0              // r == 0 ?
  addeq r2, r2, #32         // if (r == 0) lz += 32
  moveq r3, r3, LSL ip      // if (r == 0) x = (s << f)
  movne r4, r4, LSL ip      // if (r != 0) r << f
  rsbne ip, ip, #32         // if (r != 0) 32 - f
  orrne r3, r4, r3, LSR ip  // if (r != 0) x = (r << f) | ((f) ? (s >> (32 - f)) : 0)

  // r2 = lz  r3 = x

  cmp   r3, #0              // x == 0 ? 
  mov   r4, r3, LSR #25     // x >> 25
  add   r4, pc, r4, LSL #2  //
  ldrne r4, [r4,#LBLBCK(5)-LBLFWD(6)-128] // if (x) { r = f = cubeTab[(x >> 25) - 32] }
6 COLON
  mov   r2, r2, LSR #1      // lz >> 1
  umull r5, ip, r3, r4      // prod = ((unsigned NV_FX_LONGLONG)x) * f
  add   r4, r4, r4, LSL #1  // 3 * r
  rsb   r4, ip, r4, LSL #22 // r = ((3 * r) << 22) - (unsigned)(prod >> 32)
  umull r5, ip, r4, r4      // prod = ((unsigned NV_FX_LONGLONG)r) * r; s = (unsigned)(prod >> 32);
  umull r5, ip, r3, ip      // prod = ((unsigned NV_FX_LONGLONG)x) * s
  rsb   ip, ip, #0x30000000 // f = 0x30000000 - (unsigned)(prod >> 32)
  umull r5, r3, ip, r4      // prod = ((unsigned NV_FX_LONGLONG)r) * f; r = (unsigned)(prod >> 32)

  // r0 = iv
  // r1 = ov
  // r2 = lz
  // r3 = r

  ldr   ip, [r0]            // iv[0]
  smull r4, r5, r3, ip      // ((NV_FX_LONGLONG)iv[0]) * (int)r)
  mov   ip, r5, LSL #22     //
  orr   r4, ip, r4, LSR #10 //
  mov   r5, r5, ASR #10     // sprod = (((NV_FX_LONGLONG)iv[0]) * (int)r) >> 10
  rsb   ip, r2, #32         // 32 - lz
  mov   r5, r5, LSL r2      //
  orr   r4, r5, r4, LSR ip  // (int)(sprod >> lz)
  add   r4, r4, #1          // ((int)(sprod >> lz)) + 1)
  mov   r4, r4, ASR #1      // (((int)(sprod >> lz)) + 1) >> 1
  str   r4, [r1]            // ov[0] = (((int)(sprod >> lz)) + 1) >> 1

  ldr   ip, [r0, #4]        // iv[1]
  smull r4, r5, r3, ip      // ((NV_FX_LONGLONG)iv[1]) * (int)r)
  mov   ip, r5, LSL #22     //
  orr   r4, ip, r4, LSR #10 //
  mov   r5, r5, ASR #10     // sprod = (((NV_FX_LONGLONG)iv[1]) * (int)r) >> 10
  rsb   ip, r2, #32         // 32 - lz
  mov   r5, r5, LSL r2      //
  orr   r4, r5, r4, LSR ip  // (int)(sprod >> lz)
  add   r4, r4, #1          // ((int)(sprod >> lz)) + 1)
  mov   r4, r4, ASR #1      // (((int)(sprod >> lz)) + 1) >> 1
  str   r4, [r1, #4]        // ov[1] = (((int)(sprod >> lz)) + 1) >> 1

  ldr   ip, [r0, #8]        // iv[1]
  smull r4, r5, r3, ip      // ((NV_FX_LONGLONG)iv[2]) * (int)r)
  mov   ip, r5, LSL #22     //
  orr   r4, ip, r4, LSR #10 //
  mov   r5, r5, ASR #10     // sprod = (((NV_FX_LONGLONG)iv[2]) * (int)r) >> 10
  rsb   ip, r2, #32         // 32 - lz
  mov   r5, r5, LSL r2      //
  orr   r4, r5, r4, LSR ip  // (int)(sprod >> lz)
  add   r4, r4, #1          // ((int)(sprod >> lz)) + 1)
  mov   r4, r4, ASR #1      // (((int)(sprod >> lz)) + 1) >> 1
  str   r4, [r1, #8]        // ov[2] = (((int)(sprod >> lz)) + 1) >> 1

  ldr   r5, [sp], #4        // restore saved register
  ldr   r4, [sp], #4        // restore saved register

  bx    lr                  // return

  END
