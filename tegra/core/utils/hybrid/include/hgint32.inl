/*------------------------------------------------------------------------
 *
 * Hybrid Compatibility Header
 * -----------------------------------------
 *
 * (C) 2002-2005 Hybrid Graphics, Ltd.
 * All Rights Reserved.
 *
 * This file consists of unpublished, proprietary source code of
 * Hybrid Graphics, and is considered Confidential Information for
 * purposes of non-disclosure agreement. Disclosure outside the terms
 * outlined in signed agreement may result in irrepairable harm to
 * Hybrid Graphics and legal action against the party in breach.
 *
 * Description:     Inline code for hgInt32.h
 * Note:            This file is included _only_ by hgInt32.h
 *
 * $Id: //hybrid/libs/hg/main/hgInt32.inl#9 $
 * $Date: 2007/03/14 $
 * $Author: antti $ *
 *
 * \todo add bitReverse?
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_MSC)
#       pragma warning(disable:4035)        /* don't whine about no return value */
#       include <stdlib.h>                      /* _rotl() and _rotr() intrinsics    */
#endif

#if (HG_COMPILER == HG_COMPILER_HITACHI)
#       include <machine.h>
#endif

#if (HG_COMPILER == HG_COMPILER_IAR)
#       include <intrinsics.h>
#endif

/*lint -save -e701 -e702 */                 /* suppress some LINT warnings      */
/*@-shiftnegative -shiftimplementation -boolops@*/

/* x86 inline assembler sequences */
#if ((HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_INTELC)) && (HG_CPU == HG_CPU_X86) && defined(HG_SUPPORT_ASM)
#       define HG_X86_INT32_ASM
#endif

/*-------------------------------------------------------------------*//*!
 * \brief       Performs an arithmetic (signed) right shift for an integer
 * \param       x       Value to be shifted
 * \param       sh      Shift range [0,31]
 * \return      Resulting integer
 * \todo    [wili 030301] should we assert here the value of sh?
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgAsr32 (
    HGint32 x, 
    HGint32 sh)
{
    return x >> sh;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs a logical (unsigned) right shift for an integer
 * \param       x       Value to be shifted
 * \param       sh      Shift range [0,31]
 * \return      Resulting integer
 * \note    This routine is provided so that logical shifts can be applied
 *              to signed integers without having to perform two casts in the
 *              code.
 * \todo    [wili 030301] should we assert here the value of sh?
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgLsr32 (
    HGint32 x, 
    HGint32 sh)
{
    return (HGint32)((HGuint32)(x) >> sh);
}

#if (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_PPC || HG_CPU == HG_CPU_X86_64 || HG_CPU == HG_CPU_X86 || HG_CPU == HG_CPU_ARM) && (__GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
#       define HG_HAVE_GCC_BUILTIN_CLZ
#endif

/*-------------------------------------------------------------------*//*!
 * \brief       Counts number of leading zeroes in an integer
 * \param       value   Integer value
 * \return      Number of leading zeroes [0,32]
 * \note    Example outputs: clz(0)=32, clz(1)=31, clz(-1)=0
 * \todo    [wili 030225] We want to generate clz instruction for ARM
 *              if supported by the CPU
 * \todo    [wili 29/Jan/04] Write a better sequence for SH3 - the current
 *              C code does not generate very nice results.
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgClz32 (HGuint32 value)
{
#if defined (HG_X86_INT32_ASM)
    __asm
    {
        mov         eax,0x3f
        bsr         eax,value
        xor         eax,0x1f
    }
#elif defined(HG_HAVE_GCC_BUILTIN_CLZ)
#       if (HG_CPU == HG_CPU_X86_64 || HG_CPU == HG_CPU_X86)
    HGint32 ret = __builtin_clz (value);
    if (!value)
        ret = 32;
    return ret;
#       else
    return __builtin_clz (value);   /* on PPC the compiler-generated clz returns correctly 32 for value == 0 */
#       endif
#elif (HG_COMPILER == HG_COMPILER_INTELC)
    return _CountLeadingZeros(value);
#elif (HG_COMPILER == HG_COMPILER_MSEC) && (_WIN32_WCE > 0x500)
    return _CountLeadingZeros(value);
#elif (HG_COMPILER == HG_COMPILER_MWERKS) && (HG_CPU == HG_CPU_PPC)
    return __cntlzw((int)value);
#elif (HG_COMPILER == HG_COMPILER_GHS)
    return __CLZ32(value);
    /* \todo [sampo 16/Mar/04] add support for clz in ARMv5 */
#elif (HG_COMPILER == HG_COMPILER_GCC) && defined(HG_SUPPORT_WMMX)
    return __builtin_clz(value);
#elif defined(HG_COMPILER_ADS_OR_RVCT) && (HG_CPU == HG_CPU_ARM) && defined(HG_SUPPORT_ASM) && (defined(__TARGET_ARCH_5) || defined(__TARGET_ARCH_5T) || defined(__TARGET_ARCH_5E) || defined(__TARGET_ARCH_6) || defined(__TARGET_ARCH_6T)) 
    HGint32 r;
    __asm
    {
        clz         r,value
    }
    return r;
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return 31 - _msb (value);
#elif (HG_COMPILER == HG_COMPILER_IAR) && (HG_CPU == HG_CPU_ARM) && (__CORE__ >= __ARM5__) && (__CPU_MODE__ == 2)
    return __CLZ(value);
#else
    int c = 0; 
#define OP(Y) if (value<(1u<<(Y))) c+=(Y); else value >>= (Y);
    OP (16);
    OP (8);
    OP (4);
    OP (2);
    c += 2 >> value;
#undef OP
    return c;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Counts number of trailing zeroes in an integer
 * \param       x       Integer value
 * \return      Number of trailing zeroes [0,32]
 * \note    Example outputs: ctz(0)=32, ctz(1)=0, ctz(2)=1, ctz(-1)=0
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgCtz32 (HGuint32 x)
{
#if defined (HG_X86_INT32_ASM)
    __asm
    {
        mov         eax,32
        bsf         eax,x
    }
#elif defined(HG_HAVE_BUILTIN_CTZ)
#       if (HG_CPU == HG_CPU_X86_64 || HG_CPU == HG_CPU_X86)
    HGint32 ret = __builtin_ctz (value);
    if (!value)
        ret = 32;
    return ret;
#       else 
    return __builtin_ctz (x);
#       endif   
#else
    return 32 - hgClz32(~x & (x-1));
#endif
}
/*-------------------------------------------------------------------*//*!
 * \brief       Reverse byte order of a 32-bit unsigned integer
 * \param       x       32-bit unsigned integer
 * \return      Unsigned integer where the byte order has been reversed, i.e.,
 *              0x12345678 -> 0x78563412
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGuint32 hgByteSwap32 (HGuint32 x)
{
#if (HG_COMPILER == HG_COMPILER_INTELC)
    /* NB: our hand-written asm equivalent is shorter/faster, but can't
       currently get the Intel C inline assembler to work */
    return  _byteswap_ulong(x);
#elif defined (HG_X86_INT32_ASM)
    __asm
    {
        mov         eax,[x]
        bswap   eax
    }

#elif defined(HG_COMPILER_ADS_OR_RVCT) && (HG_CPU == HG_CPU_ARM) && defined(HG_SUPPORT_ASM) 
    HGint32 t,c;
    __asm
    {
        EOR     t,x,x, ROR 16
        BIC t,t, 0xff0000
        MOV c,x, ROR 8
        EOR c,c,t,LSR 8
    }
    return c;
#else
    return (x>>24) | (((x >> 16)&0xFFu)<<8) | (((x >> 8)&0xFFu) << 16) | (x << 24);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns sign [-1,0,+1] of value
 * \param       x       32-bit integer value
 * \return      x>0 -> 1, x < 0 -> -1, x = 0 -> 0
 * \note    Written on multiple lines (otherwise ADS generates invalid code!!)
 * \note    This is effectively the same (except faster) than
 *              hgCmp(value,0)
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgSign32 (HGint32 value)
{
#if (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _min(_max(value, -1),+1);
#elif (HG_CPU == HG_CPU_X86_64)
    if (value >= 1)
        value = 1;
    if (value <= -1)
        value = -1;
    return value;
#else
    HGuint32 t = (HGuint32)(-value);
    t >>= 31;
    return (HGint32)((value >> 31) | t);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns result [-1,0,+1] of signed comparison
 * \param       a       First 32-bit value
 * \param       b       Second 32-bit value
 * \return      a<b -> -1, a=b -> 0, a>b -> +1
 * \note    The code below compiles into four ARM instructions (no
 *              branches). If you want to modify the code, please specialize
 *              it for ARM using the construct below.
 * \todo    On some platforms this could be implementing by _clamping_
 *              the result to range [-1,+1] 
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgCmp32 (
    HGint32 a, 
    HGint32 b)
{
    HGint32 r = (a < b) ? -1 : 0;
    if (a > b)  
        r++;
    return r;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 0 if x >= 0, -1 otherwise
 * \param       x       32-bit integer value
 * \return      x>=0 -> 0, x < 0 -> -1
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgSignMask32 (HGint32 x)
{
    return (x >> 31);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 0 if x >= 0, 1 otherwise
 * \param       x       32-bit integer value
 * \return      x>=0 -> 0, x < 0 -> 1
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgSignBit32 (HGint32 x)
{
    return (HGint32)((HGuint32)(x) >> 31);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns population count (i.e., number of set bits) of 
 *              an integer value
 * \param       a       32-bit integer value
 * \return      Number of set bits [0,32].
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgPop32 (HGuint32 a)
{
#if (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _count(a);
#else
    a = ((a >> 1) & 0x55555555u) + (a & 0x55555555u);
    a = ((a >> 2) & 0x33333333u) + (a & 0x33333333u);
    a += a >> 4;
    a &= 0x0f0f0f0fu;
    a += a >> 8;
    a += a >> 16;
    a &= 0xff;
    return (HGint32)(a);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns boolean (0/1) indicating whether a is in the
 *              range [lo,hi] (signed)
 * \param       a       32-bit integer value to be tested
 * \param       lo      Interval minimum inclusive
 * \param       hi      Interval maximum inclusive
 * \return      1 if a is in the range, 0 otherwise
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGbool hgInRange32 (
    HGint32 a, 
    HGint32 lo, 
    HGint32 hi)
{
    HG_ASSERT(lo <= hi);
    HG_ASSERT((HGbool)((HGuint32)(a-lo) <= (HGuint32)(hi-lo)) == (HGbool)(a >= lo && a <= hi));
    return (HGbool)((HGuint32)(a-lo) <= (HGuint32)(hi-lo));
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns -1 if specified bit set, 0 otherwise
 * \param       x       Integer value
 * \param       i       Bit offset [0,31]
 * \return      (x & (1<<i)) ? -1 : 0
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgBitMask32 (
    HGint32 x, 
    HGint32 i)
{
    HG_ASSERT( i >= 0 && i <= 31);

    return (x<<(31-i)) >> 31;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns smaller of two signed values
 * \param       a       First 32-bit integer value
 * \param       b       Second 32-bit integer value
 * \return      Smaller of the two values
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgMin32 (
    HGint32 a, 
    HGint32 b)
{
#if (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _min(a, b);
#else
    if (b < a)
        a = b;
    return a;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns larger of two signed values
 * \param       a       First 32-bit integer value
 * \param       b       Second 32-bit integer value
 * \return      Larger of the two values
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgMax32 (
    HGint32 a, 
    HGint32 b)
{
#if (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _max(a, b);
#else
    if (b > a)
        a = b;
    return a;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns smaller of two unsigned values
 * \param       a       First 32-bit unsigned integer value
 * \param       b       Second 32-bit unsigned integer value
 * \return      Smaller of the two values
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGuint32 hgMinu32 (
    HGuint32 a, 
    HGuint32 b)
{
    if (b < a)
        a = b;
    return a;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns larger of two unsigned values
 * \param       a       First 32-bit unsigned integer value
 * \param       b       Second 32-bit unsigned integer value
 * \return      Larger of the two values
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGuint32 hgMaxu32 (
    HGuint32 a, 
    HGuint32 b)
{
    if (b > a)
        a = b;
    return a;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns max (a,0), i.e., clamps a to zero
 * \param       a       32-bit integer value
 * \return      max (a,0)
 * \note    Compiles into a single instruction on ARM!
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgMaxZero32 (HGint32 a)
{
/* apparently GCC does not handle this optimally without a little asm.. */
#if (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_ARM) && defined (HG_SUPPORT_ASM)
    __asm__ ("bic %[a], %[a], %[a], asr #31\n"
             : [a] "+r" (a) );                      
    return a;
#elif (HG_CPU == HG_CPU_X86_64)
    return (a < 0) ? 0 : a;
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _max (a, 0);
#else
    return a & ~(a >> 31);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns absolute value of an integer
 * \param       a       32-bit input value
 * \return      abs(a)
 * \todo    [wili 030218] On many platforms it might be best to
 *              use native abs() ?
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgAbs32 (HGint32 a) /*@modifies@*/
{
#if (HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_INTELC)
    return abs(a);
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _abs(a);
#elif (HG_COMPILER == HG_COMPILER_MWERKS) && (HG_CPU == HG_CPU_PPC)
    return __abs(a);
#elif defined(HG_COMPILER_ADS_OR_RVCT) && (HG_CPU == HG_CPU_ARM) && defined(HG_SUPPORT_ASM)
    __asm
    {
        movs    a,a             /* in some cases this is really redundant */
        rsbmi   a,a,#0
    }
    return a;
#else
    return (a >= 0) ? a : -a;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns negative absolute value of an integer
 * \param       a       32-bit input value
 * \return      -abs(a)
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgNabs32 (HGint32 a)
{
#if defined(HG_COMPILER_ADS_OR_RVCT) && (HG_CPU == HG_CPU_ARM) && defined(HG_SUPPORT_ASM)
    __asm
    {
        movs    a,a             /* in some cases this is really redundant */
        rsbpl   a,a,#0
    }
    return a;
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return -_abs(a);
#else
    return ((a > 0) ? -a : a);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns absolute difference of two values
 * \param       a 32-bit input value
 * \param       b 32-bit input value
 * \return      |a-b|
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgDiff32 (HGint32 a, HGint32 b)
{
#if defined(HG_COMPILER_ADS_OR_RVCT) && (HG_CPU == HG_CPU_ARM) && defined(HG_SUPPORT_ASM)
    __asm
    {
        subs    a,a,b       /* in some cases this is really redundant */
        rsbmi   a,a,#0
    }
    return a;
#else
    return hgAbs32(a-b);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Rotates 32-bit value to the left
 * \param       a       32-bit value
 * \param       sh      Shift value [0,32]
 * \return      Rotated value
 * \note    Works even if sh = 32!
 * \todo    [wili 02/Oct/03] optimize for ARM?
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgRol32 (
    HGint32 a, 
    HGint32 sh)
{
    HG_ASSERT (sh >= 0 && sh <= 32);

#if (HG_COMPILER == HG_COMPILER_MSC) && (HG_CPU == HG_CPU_X86) && (HG_OS == HG_OS_WIN32)
    return (HGint32)_rotl((HGuint32)a,sh);
#elif (HG_COMPILER == HG_COMPILER_INTELC)
    return _rotl(a,sh);
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _ror(a, 32 - sh);
#else
    return (a << sh) | ((HGuint32)(a) >> (32-sh));
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Rotates 32-bit value to the right
 * \param       a       32-bit value
 * \param       sh      Shift value [0,32]
 * \return      Rotated value
 * \note    Works even if sh = 32!
 * \todo    [wili 02/Oct/03] optimize for ARM?
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgRor32 (
    HGint32 a, 
    HGint32 sh)
{
    HG_ASSERT (sh >= 0 && sh <= 32);
#if (HG_COMPILER == HG_COMPILER_MSC) && (HG_CPU == HG_CPU_X86) && (HG_OS == HG_OS_WIN32)
    return (HGint32)_rotr((HGuint32)a,sh);
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _ror(a, sh);
#elif (HG_COMPILER == HG_COMPILER_INTELC)
    return _rotr(a,sh);
#else
    return (HGint32)((HGuint32)(a) >> sh) | (a << (32-sh));
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Clamps value to be in range [mn,mx]
 * \param       v       Input value
 * \param       mn      Interval minimum (inclusive)
 * \param       mx      Interval maximum (inclusive)
 * \return      Clamped value
 * \note    mn *must* be <= mx
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgClamp32 (
    HGint32 v, 
    HGint32 mn, 
    HGint32 mx)
{
    HG_ASSERT (mn <= mx);
#if (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _min(_max(v, mn), mx);
#else   
    if (v < mn) 
        v = mn;
    if (v > mx) 
        v = mx;
    
    return v;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Clamps input value to [0,255] range
 * \param       a       32-bit integer
 * \return      Clamped input value
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgClampUByte32 (HGint32 a)
{
#if (HG_COMPILER == HG_COMPILER_INTELC)
    if (a < 0)
        a = 0;
    if (a > 255)
        a = 255;
    return a;
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _max(_min(a,255),0);
#else
    a = hgMaxZero32(a);
    if (a > 255)
        a = 255;
    return a;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Clamps input value to [0,65535] range
 * \param       x       32-bit inter
 * \return      Clamped input value
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgClampUShort32 (HGint32 x)
{
#if (HG_COMPILER == HG_COMPILER_INTELC)
    if (x >= 65536)
        x = 65535;
    if (x < 0)
        x = 0;
    return x;
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _max(_min(x,65535),0);
#else
    x = hgMaxZero32(x);
    if (x > 65535)
        x = 65535;
    return x;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Clamps value to be in range [0,mx]
 * \param       v       Input value
 * \param       mx      Interval maximum (inclusive, must be >= 0)
 * \return      Clamped value in range [0,mx]
 * \note    mx >= 0
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgClamp32_0_x (
    HGint32 v, 
    HGint32 mx)
{
#if (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _max(_min(v,mx),0);
#else
    HG_ASSERT (mx >= 0);

    if (v > mx)
        v = mx;

    v = hgMaxZero32(v);

    return v;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns boolean indicating whether input value is a power
 *              of two (note that 0 is considered a power of two here!)
 * \param       x       Input 32-bit value (unsigned)
 * \return      1 if x is a power of two, 0 otherwise
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGbool hgIsPowerOfTwo32 (HGuint32 x)
{
    return (HGbool)!(x & (x-1));
}

/*-------------------------------------------------------------------*//*!
 * \brief       Assumes that 'd' is positive
 * \todo    [wili 030224] should this func behave more like FORTRAN's
 *              ISIGN? i.e. sign is always applied?
 * \todo    [wili 030224] Perhaps a better idea is to perform an actual
 *              comparison (see code commented out)
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgCopySign32 (
    HGint32 d, 
    HGint32 s)
{
    HG_ASSERT (d >= 0);
    
#if (HG_CPU == HG_CPU_X86) || (HG_CPU == HG_CPU_PPC) || (HG_CPU == HG_CPU_VC02) /* avoid branches */
    s>>=31;
    return (d ^ s) - s;
#else
    if (s < 0)          
        d = -d;
    return d;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs signed 32x32->64 multiplication, returns high 32 bits
 * \param       a       First 32-bit signed integer
 * \param       b       Second 32-bit signed integer
 * \return      ((int64)(a)*b)>>32
 * \note    THIS IS A CRITICAL FUNCTION TO OPTIMIZE!!
 * \todo    [wili 02/Oct/03] Check if there are special register rules
 *              for smull (i.e. is it ok if compiler maps 'h' to either
 *              'a' or 'b'??
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGint32 hgMul64h (
    HGint32 a, 
    HGint32 b)
{
#if (HG_COMPILER == HG_COMPILER_HITACHI)
    return (HGint32)(dmuls_h(a,b));
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _mulhd_ss (a, b);
#elif (HG_COMPILER == HG_COMPILER_INTELC)
    return (HGint32)( ((__int64)(a) * b) >> 32);
#elif defined (HG_X86_INT32_ASM)
    __asm
    {
        mov         eax,[a]
        imul    [b]
        mov         eax,edx
    }
#elif (HG_COMPILER == HG_COMPILER_MWERKS) && (HG_CPU == HG_CPU_PPC)
    return __mulhw(a,b);
#elif (HG_COMPILER == HG_COMPILER_RVCT) && (HG_CPU == HG_CPU_ARM) && defined(HG_SUPPORT_ASM) && defined (__TARGET_FEATURE_MULTIPLY)
    __asm { smull a, b, a, b }
    return b;
#elif (HG_COMPILER == HG_COMPILER_ADS) && (HG_CPU == HG_CPU_ARM) && defined(HG_SUPPORT_ASM) && defined (__TARGET_FEATURE_MULTIPLY)
    HGint32 h,dummy;
    __asm { smull dummy,h,a,b }
    return h;
#elif (HG_COMPILER == HG_COMPILER_GHS)
    return __MULSH(a,b);
#elif (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_ARM) && defined (HG_SUPPORT_ASM)
    HGint32 h, dummy;
    __asm__ ("smull %[dummy],%[h],%[a],%[b]\n\t"
             : [dummy] "=r&" (dummy),
               [h]     "=r&" (h)
             : [a]     "r"  (a),
               [b]     "r"  (b)
             );
    return h;
#elif (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_SH) && defined (HG_SUPPORT_ASM)
    HGint32 h;
    __asm__ ("dmuls.l %[a], %[b]\n\t"
     "sts     mach,%[h]\n\t"
      : [h]     "=r" (h)
      : [a] "r" (a), [b] "r"(b)
          : "mach", "macl" );
    return h;
#elif defined (HG_64_BIT_INTS)
    return (HGint32)( ((HGint64)(a) * b) >> 32);
#else
    HGuint32 hh         = (HGuint32)((a>>16)   *(b>>16));
    HGuint32 lh         = (HGuint32)((a&0xFFFF)*(b>>16));
    HGuint32 hl         = (HGuint32)((a>>16)   *(b&0xFFFF));
    HGuint32 ll         = (HGuint32)((a&0xFFFF)*(b&0xFFFF));
    HGuint32 oldlo;

    hh += (HGint32)(lh)>>16;
    hh += (HGint32)(hl)>>16;

    oldlo   =  ll;
    ll          += lh<<16;
    if (ll < oldlo)
        hh++;

    oldlo   =  ll;
    ll          += hl<<16;
    if (ll < oldlo)
        hh++;

    return (HGint32)hh;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs unsigned 32x32->64 multiplication, returns high
 *              32 bits
 * \param       a       First 32-bit  unsigned integer
 * \param       b       Second 32-bit unsigned integer
 * \return      ((unsigned int64)(a)*b)>>32
 * \note    THIS IS A CRITICAL FUNCTION TO OPTIMIZE!!
 * \todo    [wili 02/Oct/03] Check if there are special register rules
 *              for umull (i.e. is it ok if compiler maps 'h' to either
 *              'a' or 'b'??
 *//*-------------------------------------------------------------------*/

HG_INLINE HG_ATTRIBUTE_CONST HGuint32 hgMulu64h (
    HGuint32 a, 
    HGuint32 b)
{

#if (HG_COMPILER == HG_COMPILER_HITACHI)
    return (HGuint32)(dmulu_h(a,b));
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
    return _mulhd_uu (a, b);
#elif (HG_COMPILER == HG_COMPILER_INTELC)
    return (HGuint32)( ((unsigned __int64)(a) * b) >> 32);
/*      return _MulUnsignedHigh(a,b);*/
#elif defined (HG_X86_INT32_ASM)
    __asm
    {
        mov         eax,[a]
        mul         [b]
        mov         eax,edx
    }
#elif (HG_COMPILER == HG_COMPILER_MWERKS) && (HG_CPU == HG_CPU_PPC)
    return __mulhwu(a,b);
#elif (HG_COMPILER == HG_COMPILER_RVCT) && (HG_CPU == HG_CPU_ARM) && defined(HG_SUPPORT_ASM) && defined (__TARGET_FEATURE_MULTIPLY)
    __asm { umull a,b,a,b }
    return b;
#elif (HG_COMPILER == HG_COMPILER_ADS)  && (HG_CPU == HG_CPU_ARM) && defined(HG_SUPPORT_ASM) && defined (__TARGET_FEATURE_MULTIPLY)
    HGuint32 h,dummy;
    __asm { umull dummy,h,a,b }
    return h;
#elif (HG_COMPILER == HG_COMPILER_GHS)
    return __MULUH(a,b);
#elif (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_ARM) && defined (HG_SUPPORT_ASM)
    HGuint32 h, dummy;
    __asm__ ("umull %[dummy],%[h],%[a],%[b]\n\t"
             : [dummy] "=r&" (dummy),
               [h]     "=r&" (h)
             : [a]     "r"  (a),
               [b]     "r"  (b)
             );
    return h;
#elif (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_SH) && defined (HG_SUPPORT_ASM)
    HGint32 h;
    __asm__ ("dmulu.l %[a], %[b]\n\t"
     "sts     mach,%[h]\n\t"
      : [h]     "=r" (h)
      : [a] "r" (a), [b] "r"(b)
          : "mach", "macl" );
    return h;
#elif defined (HG_64_BIT_INTS)
    return (HGuint32)( ((HGuint64)(a) * b) >> 32);
#else
    HGuint32 hh = (a>>16)   * (b>>16);
    HGuint32 lh = (a&0xFFFF)* (b>>16);
    HGuint32 hl = (a>>16)   * (b&0xFFFFu);
    HGuint32 ll = (a&0xFFFF)* (b&0xFFFFu);
    HGuint32 oldlo;

    hh += (lh>>16);
    hh += (hl>>16);

    oldlo = ll;
    ll += lh<<16;
    if (ll < oldlo)
        hh++;

    oldlo = ll;
    ll += hl<<16;
    if (ll < oldlo)
        hh++;

    return hh;
#endif
}

/*----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_MSC)
#       pragma warning(default:4035)    
#endif

#undef HG_X86_INT32_ASM

/*lint -restore */
/*@=shiftnegative =shiftimplementation =boolops@*/

/*----------------------------------------------------------------------*/

