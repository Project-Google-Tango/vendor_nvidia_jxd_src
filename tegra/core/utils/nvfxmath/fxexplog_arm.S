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

  EXPORT  NvSFxLog2
  EXPORT  NvSFxPow2
  EXPORT  NvSFxPow

NvSFxLog2 COLON
  cntlz r1, r0, r2          // lz = clz(x)
  add   r2, r1, #1          // lz + 1
  str   lr, [sp, #-4]!      // save link register on stack
  mov   r2, r0, LSL r2      // t = x << (lz + 1)
  and   r3, r2, #0x3FFFFFF  // dx = t & ((1 << (32 - 6)) - 1)
  mov   r2, r2, LSR #26     // idx = (unsigned)t >> (32 - 6);
  add   r2, pc, r2, LSL #2  // &ltab[idx]
  ldr   ip, [r2, #LBLFWD(1)-LBLFWD(6)]   // ltab[idx]
6 COLON    
  ldr   r0, [r2, #LBLFWD(1)-LBLBCK(6)+4] // ltab[idx+1]
  ldr   r2, [r2, #LBLFWD(1)-LBLBCK(6)+8] // ltab[idx+2]
  sub   r0, r0, ip          // ltab[idx+1] - ltab[idx]
  sub   r2, r2, ip          // f2 = ltab[idx+2] - ltab[idx]
  sub   r2, r2, r0, LSL #1  // a = f2 - ((ltab[idx+1] - ltab[idx]) << 1)
  rsb   r0, r2, r0, LSL #1  // b = ((ltab[idx+1] - ltab[idx]) << 1) - a
  smull r2, lr, r3, r2      // (long long)a) * dx)
  rsb   r1, r1, #15         // 15 - lz
  mov   r2, r2, LSR #26     //
  orr   r2, r2, lr, LSL #6  // (int)((((long long)a) * dx) >> (32 - 6))
  add   r2, r2, r0          // approx = (int)((((long long)a)*dx)>> (32-6)) + b
  smull r0, lr, r2, r3      // (long long)approx) * dx
  mov   r1, r1, LSL #16     // ((15 - lz) << 16)
  mov   r0, r0, LSR #27     //
  orr   r0, r0, lr, LSL #5  // approx=(int)((((long long)approx)*dx)>>(32-6+1))
  ldr   lr, [sp], #4        // return address stored in top of stack
  add   r0, r0, ip          // approx = ltab[idx] + approx
  orr   r0, r1, r0, LSR #15 // approx = ((15-lz)<<16)|(((unsigned)approx)>> 15)
  bx    lr                  // return

1 COLON
  DW 0x00000000 
  DW 0x02dcf2d1
  DW 0x05aeb4dd
  DW 0x08759c50
  DW 0x0b31fb7d
  DW 0x0de42120
  DW 0x108c588d
  DW 0x132ae9e2
  DW 0x15c01a3a
  DW 0x184c2bd0
  DW 0x1acf5e2e
  DW 0x1d49ee4c
  DW 0x1fbc16b9
  DW 0x22260fb6
  DW 0x24880f56
  DW 0x26e2499d
  DW 0x2934f098
  DW 0x2b803474
  DW 0x2dc4439b
  DW 0x30014ac6
  DW 0x32377512
  DW 0x3466ec15
  DW 0x368fd7ee
  DW 0x38b25f5a
  DW 0x3acea7c0 
  DW 0x3ce4d544
  DW 0x3ef50ad2
  DW 0x40ff6a2e
  DW 0x43041403
  DW 0x450327eb
  DW 0x46fcc47a
  DW 0x48f10751
  DW 0x4ae00d1d
  DW 0x4cc9f1ab
  DW 0x4eaecfeb
  DW 0x508ec1fa
  DW 0x5269e12f
  DW 0x5440461c
  DW 0x5612089a
  DW 0x57df3fd0
  DW 0x59a80239
  DW 0x5b6c65aa
  DW 0x5d2c7f59
  DW 0x5ee863e5
  DW 0x60a02757
  DW 0x6253dd2c
  DW 0x64039858
  DW 0x65af6b4b
  DW 0x675767f5
  DW 0x68fb9fce
  DW 0x6a9c23d6
  DW 0x6c39049b
  DW 0x6dd2523d
  DW 0x6f681c73
  DW 0x70fa728c
  DW 0x72896373
  DW 0x7414fdb5
  DW 0x759d4f81
  DW 0x772266ad
  DW 0x78a450b8
  DW 0x7a231ace
  DW 0x7b9ed1c7
  DW 0x7d17822f
  DW 0x7e8d3846
  DW 0x80000000
  DW 0x816fe50b

NvSFxPow2 COLON
  mov   r1, r0, ASR #16     // i = (x >> 16)
  str   lr, [sp, #-4]!      // save link register on stack
  sub   r3, r0, r1, LSL #16 // f = (x & 0xffff)
  mov   r2, r3, LSR #10     // idx = (unsigned)f >> (16 - 6);
  sub   r3, r3, r2, LSL #10 // dx = f - (idx << (16 - 6))
  add   r2, pc, r2, LSL #2  // &etab[idx]
  ldr   ip, [r2, #LBLFWD(3)-LBLFWD(2)]   // etab[idx]
2 COLON
  ldr   r0, [r2, #LBLFWD(3)-LBLBCK(2)+4] // etab[idx+1]
  ldr   r2, [r2, #LBLFWD(3)-LBLBCK(2)+8] // etab[idx+2]
  sub   r0, r0, ip          // etab[idx+1] - etab[idx]
  sub   r2, r2, ip          // f2 = etab[idx+2] - etab[idx]
  sub   r2, r2, r0, LSL #1  // a = f2 - ((etab[idx+1] - etab[idx]) << 1)
  rsb   r0, r2, r0, LSL #1  // b = ((etab[idx+1] - etab[idx]) << 1) - a
  smull r2, lr, r3, r2      // (long long)a) * dx
  rsb   r1, r1, #15         // 15 - i
  mov   r2, r2, LSR #10     //
  orr   r2, r2, lr, LSL #22 // (int)((((long long)a) * dx) >> (16 - 6))
  add   r2, r2, r0          // approx= (int)((((long long)a)*dx) >> (16-6)) + b
  smull r0, lr, r2, r3      // (long long)approx) * dx
  cmp   r1, #31             // (15-i) > 31 (underflow) ?
  mov   r0, r0, LSR #11     //
  orr   r0, r0, lr, LSL #21 // approx=(int)((((long long)approx)*dx)>>(16-6+1))
  ldr   lr, [sp], #4        // return address stored in top of stack
  add   r0, r0, ip          // approx = etab[idx] + approx
  orr   r0, r0, #0x80000000 // (etab[idx] + (unsigned)approx + 0x80000000U) 
  mov   r0, r0, LSR r1      // (etab[idx]+(unsigned)approx+0x80000000U)>>(15-i)
  movhi r0, #0              // if underflow, flush result to zero
  bx    lr                  // return

3 COLON
  DW 0
  DW 23384564
  DW 47023769
  DW 70920388
  DW 95077224
  DW 119497111
  DW 144182913
  DW 169137525
  DW 194363876
  DW 219864923
  DW 245643659
  DW 271703107
  DW 298046324
  DW 324676399
  DW 351596457
  DW 378809655
  DW 406319186
  DW 434128275
  DW 462240186
  DW 490658215
  DW 519385697
  DW 548426000
  DW 577782531
  DW 607458734
  DW 637458090
  DW 667784117
  DW 698440373
  DW 729430454
  DW 760757994
  DW 792426669
  DW 824440194
  DW 856802323
  DW 889516852
  DW 922587619
  DW 956018503
  DW 989813426
  DW 1023976351
  DW 1058511286
  DW 1093422282
  DW 1128713434
  DW 1164388881
  DW 1200452809
  DW 1236909446
  DW 1273763071
  DW 1311018005
  DW 1348678619
  DW 1386749330
  DW 1425234604
  DW 1464138955
  DW 1503466946
  DW 1543223192
  DW 1583412354
  DW 1624039148
  DW 1665108339
  DW 1706624743
  DW 1748593232
  DW 1791018728
  DW 1833906207
  DW 1877260700
  DW 1921087292
  DW 1965391125
  DW 2010177395
  DW 2055451355
  DW 2101218317
  DW -2147483648
  DW -2100714521

NvSFxPow COLON
  cntlz r3, r0, r2          // lz = clz(x)
  add   r2, r3, #1          // lz + 1
  str   lr, [sp, #-4]!      // save link register on stack
  mov   r2, r0, LSL r2      // t = x << (lz + 1)
  str   r1, [sp, #-4]!      // save y
  and   r1, r2, #0x3FFFFFF  // dx = t & ((1 << (32 - 6)) - 1)
  mov   r2, r2, LSR #26     // idx = (unsigned)t >> (32 - 6);
  add   r2, pc, r2, LSL #2  // &ltab[idx]
  ldr   ip, [r2, #LBLBCK(1)-LBLFWD(4)]   // ltab[idx]
4 COLON
  ldr   r0, [r2, #LBLBCK(1)-LBLBCK(4)+4] // ltab[idx+1]
  ldr   r2, [r2, #LBLBCK(1)-LBLBCK(4)+8] // ltab[idx+2]
  sub   r0, r0, ip          // ltab[idx+1] - ltab[idx]
  sub   r2, r2, ip          // f2 = ltab[idx+2] - ltab[idx]
  sub   r2, r2, r0, LSL #1  // a = f2 - ((ltab[idx+1] - ltab[idx]) << 1)
  rsb   r0, r2, r0, LSL #1  // b = ((ltab[idx+1] - ltab[idx]) << 1) - a
  smull r2, lr, r1, r2      // (long long)a) * dx)
  rsb   r3, r3, #15         // 15 - lz
  mov   r2, r2, LSR #26     //
  orr   r2, r2, lr, LSL #6  // (int)((((long long)a) * dx) >> (32 - 6))
  add   r2, r2, r0          // approx =(int)((((long long)a)*dx) >> (32-6)) + b
  smull r0, lr, r2, r1      // (long long)approx) * dx
  mov   r3, r3, LSL #27     // ((15 - lz) << 27)
  ldr   r2, [sp], #4        // get y
  mov   r0, r0, LSR #27     //
  orr   r0, r0, lr, LSL #5  // approx=(int)((((long long)approx)*dx)>>(32-6+1))
  add   r0, r0, ip          // approx = ltab[idx] + approx
  orr   r0, r3, r0, LSR #4  // approx = ((15-lz)<<27)|(((unsigned)approx)>> 4)
  smull r1, r3, r2, r0      // r3:r1 = y*log2(x)
  mov   r1, r1, LSR #11     // shift to make result into S31.32 format
  orr   r1, r1, r3, LSL #21 //  such that r3 = i (integer part of result)
  mov   r3, r3, ASR #11     //   and r1 = f (fractional part) of result
  mov   r2, r1, LSR #26     // idx = (unsigned)f >> (32 - 6);
  sub   r1, r1, r2, LSL #26 // dx = f - (idx << (32 - 6))
  add   r2, pc, r2, LSL #2  // &etab[idx]
  ldr   ip, [r2, #LBLBCK(3)-LBLFWD(5)]   // etab[idx]
5 COLON
  ldr   r0, [r2, #LBLBCK(3)-LBLBCK(5)+4] // etab[idx+1]
  ldr   r2, [r2, #LBLBCK(3)-LBLBCK(5)+8] // etab[idx+2]
  sub   r0, r0, ip          // etab[idx+1] - etab[idx]
  sub   r2, r2, ip          // f2 = etab[idx+2] - etab[idx]
  sub   r2, r2, r0, LSL #1  // a = f2 - ((etab[idx+1] - etab[idx]) << 1)
  rsb   r0, r2, r0, LSL #1  // b = ((etab[idx+1] - etab[idx]) << 1) - a
  smull r2, lr, r1, r2      // (long long)a) * dx
  rsb   r3, r3, #15         // 15 - i
  mov   r2, r2, LSR #26     //
  orr   r2, r2, lr, LSL #6  // (int)((((long long)a) * dx) >> (32 - 6))
  add   r2, r2, r0          // approx = (int)((((long long)a)*dx)>> (32-6)) + b
  smull r0, lr, r2, r1      // (long long)approx) * dx
  cmp   r3, #31             // (15-i) > 31 (underflow) ?
  mov   r0, r0, LSR #27     //
  orr   r0, r0, lr, LSL #5  // approx=(int)((((long long)approx)*dx)>>(32-6+1))
  ldr   lr, [sp], #4        // return address stored in top of stack
  add   r0, r0, ip          // approx = etab[idx] + approx
  orr   r0, r0, #0x80000000 // (etab[idx] + (unsigned)approx + 0x80000000U) 
  mov   r0, r0, LSR r3      // (etab[idx]+(unsigned)approx+0x80000000U)>>(15-i)
  movhi r0, #0              // if underflow, flush result to zero
  bx    lr                  // return

  END
