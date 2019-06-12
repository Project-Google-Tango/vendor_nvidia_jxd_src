#ifndef __HGINT32_H
#define __HGINT32_H
/*------------------------------------------------------------------------
 *
 * Hybrid Compatibility Header
 * -----------------------------------------
 *
 * (C) 2002-2003 Hybrid Graphics, Ltd.
 * All Rights Reserved.
 *
 * This file consists of unpublished, proprietary source code of
 * Hybrid Graphics, and is considered Confidential Information for
 * purposes of non-disclosure agreement. Disclosure outside the terms
 * outlined in signed agreement may result in irrepairable harm to
 * Hybrid Graphics and legal action against the party in breach.
 *
 * Description:         Portable routines for manipulation of 32-bit integers
 *
 * $Id: //hybrid/libs/hg/main/hgInt32.h#1 $
 * $Date: 2005/11/08 $
 * $Author: janne $ *
 *----------------------------------------------------------------------*/

#if !defined (__HGDEFS_H)
#       include "hgdefs.h"
#endif

/*----------------------------------------------------------------------*
 * 32-bit integer manipulation routines
 * \todo [wili 030301] should hgPop etc. take hgInt32 instead??
 *----------------------------------------------------------------------*/

#if 0

/* absolute values */
HG_INLINE       HGint32         hgAbs32                         (HGint32);                                      /* absolute value (note that absolute value for 0x80000000 == 0x80000000 !!)*/
HG_INLINE       HGint32         hgNabs32                        (HGint32);                                      /* negative absolute value */
HG_INLINE       HGint32         hgDiff32                        (HGint32 a, HGint32 b);         /* absolute difference */

/* bit counting */
HG_INLINE       HGint32         hgPop32                         (HGuint32);                                     /* returns number of set bits [0,32] ("population count")       */
HG_INLINE       HGint32         hgClz32                         (HGuint32);                                     /* number of leading zeroes [0,32]              */
HG_INLINE       HGint32         hgCtz32                         (HGuint32);                                     /* number of trailing zeroes [0,32]             */
HG_INLINE       HGbool          hgIsPowerOfTwo32        (HGint32);                                      /* returns IM_TRUE if value is a power of two */

/* sign extraction */
HG_INLINE       HGint32         hgSign32                        (HGint32);                                      /* return clamp(x,-1,1) */
HG_INLINE       HGint32         hgSignBit32                     (HGint32);                                      /* return (x >= 0) ? 0 : 1                              */
HG_INLINE       HGint32         hgSignMask32            (HGint32);                                      /* return (x >= 0) ? 0 : 0xffffffff     (=-1)*/
HG_INLINE       HGint32         hgBitMask32                     (HGint32 d, HGint32 i);         /* return (x[i]) ? 0xFFFFFFFF : 0, i=0 lowest bit */
HG_INLINE       HGint32         hgCopySign32            (HGint32 d, HGint32 s);         /* assert(d>=0); if (s < 0) d = -d; return d; */

/* comparisons */
HG_INLINE       HGint32         hgCmp32                         (HGint32 a, HGint32 b);         /* result of comparison [-1,+1] */
HG_INLINE       HGbool          hgInRange32                     (HGint32 a, HGint32 lo, HGint32 hi);/* return (a >= lo && a <= hi) */

/* long multiplication */
HG_INLINE       HGint32         hgMul64h                        (HGint32,  HGint32);            /* 32x32->64 (signed),          returns high 32 bits */
HG_INLINE       HGuint32        hgMulu64h                       (HGuint32, HGuint32);           /* 32x32->64 (unsigned),        returns high 32 bits */

/* rotations and byte swapping*/
HG_INLINE       HGint32         hgRol32                         (HGint32 a, HGint32 sh);        /* rotate left  [0,32]  */
HG_INLINE       HGint32         hgRor32                         (HGint32 a, HGint32 sh);        /* rotate right [0,32]  */
HG_INLINE       HGuint32        hgByteSwap32            (HGuint32);                                     /* swaps byte order (abcd -> dcba)                      */

/* shifting */
HG_INLINE       HGint32         hgAsr32                         (HGint32 x, HGint32 sh);        /* arithmetic right shift       */
HG_INLINE       HGint32         hgLsr32                         (HGint32 x, HGint32 sh);        /* logical right shift          */

/* min-maxing and clamping */
HG_INLINE       HGint32         hgMaxZero32                     (HGint32);                                      /* return max(a,0) */
HG_INLINE       HGint32         hgMax32                         (HGint32 a, HGint32 b);         /* return larger of (a,b) */
HG_INLINE       HGint32         hgMin32                         (HGint32 a, HGint32 b);         /* return smaller of (a,b) */
HG_INLINE       HGint32         hgClamp32                       (HGint32 v, HGint32 mn, HGint32 mx); /* clamp v to [mn,mx] range        */
HG_INLINE       HGint32         hgClampUByte32          (HGint32);                                      /* saturates value to [0,255] range             */
HG_INLINE       HGint32         hgClampUShort32         (HGint32);                                      /* saturates value to [0,65535] range   */

HG_INLINE       HGint32         hgClamp32_0_x           (HGint32 v, HGint32 mx)         /* clamp v to [0,mx] range      */


#endif

#include "hgint32.inl"


/*----------------------------------------------------------------------*/
#endif /* __HGINT32_H */
