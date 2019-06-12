/*------------------------------------------------------------------------
 *
 * Hybrid Compatibility Header
 * -----------------------------------------
 *
 * (C) 2002-2004 Hybrid Graphics, Ltd.
 * All Rights Reserved.
 *
 * This file consists of unpublished, proprietary source code of
 * Hybrid Graphics, and is considered Confidential Information for
 * purposes of non-disclosure agreement. Disclosure outside the terms
 * outlined in signed agreement may result in irrepairable harm to
 * Hybrid Graphics and legal action against the party in breach.
 *
 * Description:         Inline code for hgInt64.h
 * Note:                        This file is included _only_ by hgInt64.h
 *
 * $Id: //hybrid/libs/hg/main/hgInt64Native.inl#2 $
 * $Date: 2005/11/08 $
 * $Author: janne $ *
 *
 * \note This file contains the "native" implementation of 64 bit
 *               integers for platforms that have a reasonably decent
 *               support for 64-bit data types.
 *
 *----------------------------------------------------------------------*/

#if !defined(HG_64_BIT_INTS)
#       error You should include hgInt64Emulated.inl instead!
#endif

/*@-shiftnegative -shiftimplementation -predboolint -boolops*/
#if defined (__thumb)
#       error This version does not work for THUMB!
#endif

#if (HG_COMPILER == HG_COMPILER_INTELC)
#       pragma warning (push)
#       pragma warning (disable:981)
#endif

/*----------------------------------------------------------------------*
 * Internal #defines if inline assembler is supported.
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_ARM) && defined (HG_SUPPORT_ASM)
#       define HG_GCC_ARM_ASSEMBLER
#endif

#if (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_SH) && defined (HG_SUPPORT_ASM)
#       define HG_GCC_SH_ASSEMBLER
#endif

#if (HG_COMPILER == HG_COMPILER_MSC) && defined (HG_SUPPORT_ASM)
#       define HG_MSC_X86_ASSEMBLER
#endif

#if (HG_COMPILER == HG_COMPILER_RVCT) && defined (HG_SUPPORT_ASM)
#       define HG_RVCT_ARM_ASSEMBLER
#endif

/*----------------------------------------------------------------------*
 * For some compilers we want to override certain 64-bit functions
 * by usion the "union trick", i.e., manipulating the 64-bit integer
 * components directly as union members.
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_GCC) || (HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_RVCT) || (HG_COMPILER == HG_COMPILER_INTELC) || (HG_COMPILER == HG_COMPILER_HCVC)
#       define HG_USE_UNIONS_64
#endif

#if (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_ARM)
#       undef HG_USE_UNIONS_64
#endif

#if (HG_CPU == HG_CPU_X86_64)
#       undef HG_USE_UNIONS_64
#endif

/*----------------------------------------------------------------------*
 * If we have a modulo-256 shifter (ARM) and we're using GCC/RVCT,
 * we want to use specialized shifter operations
 *----------------------------------------------------------------------*/

#if ((HG_COMPILER == HG_COMPILER_GCC) || (HG_COMPILER == HG_COMPILER_RVCT)) && (HG_CPU == HG_CPU_ARM) 
#       define HG_ARM_SHIFTERS
#endif

/*----------------------------------------------------------------------*
 * If we have a modulo-32 shifter (x86 or SH3), we want to use 
 * hand-written shifter operations.
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_MSC) || ((HG_COMPILER == HG_COMPILER_GCC && ((HG_CPU == HG_CPU_SH) || (HG_CPU == HG_CPU_X86))))
#       define HG_MODULO_32_SHIFTERS
#endif

/*-------------------------------------------------------------------*//*!
 * \brief       Casts an unsigned 64-bit integer into signed
 * \param       a       Unsigned 64-bit integer
 * \return      64-bit signed integer
 * \note        When properly inlined, this is a no-op
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgToSigned64 (const HGuint64 a)
{
        return (HGint64)(a);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Casts a signed 64-bit integer into unsigned
 * \param       a       Signed 64-bit integer
 * \return      64-bit unsigned integer
 * \note        When properly inlined, this is a no-op
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint64 hgToUnsigned64 (const HGint64 a)
{
        return (HGuint64)(a);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Sign-extends a 32-bit signed integer into a HGint64
 * \param       l       Signed 32-bit integer
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgSet64s (HGint32 l)
{
        return (HGint64)(l);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Constructs a 64-bit unsigned integer from two 32-bit values
 * \param       h       High 32 bits
 * \param       l       Low  32 bits
 * \return      Resulting 64-bit unsigned integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint64 hgSet64u (
        HGuint32 h,
        HGuint32 l)
{
#if defined (HG_USE_UNIONS_64)
/*@-usedef*/
        HGuint64_s r;
        r.c.l = l;
        r.c.h = h;
        return r.r; 
/*@=usedef*/
#else
        return (((HGuint64)(h))<<32) | l;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Constructs a 64-bit integer from two 32-bit value
 * \param       h       High 32 bits
 * \param       l       Low 32 bits
 * \return      Resulting 64-bit signed integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgSet64 (
        HGint32 h,
        HGint32 l)
{
        
#if defined (HG_USE_UNIONS_64)
/*@-usedef*/
        HGint64_s r;
        r.c.h = h;
        r.c.l = l;
        return r.r;
/*@=usedef*/
#else
        HGuint64 r;
        r = (HGuint32)(h);
        r <<= 32;
        r |= (HGuint32)(l);
        return (HGint64)r;
#endif
}


/*-------------------------------------------------------------------*//*!
 * \brief       Returns a 64-bit integer with the value 0
 * \return      64-bit integer with the value 0
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgZero64 (void)
{
#if defined (HG_USE_UNIONS_64)
/*@-usedef*/
        HGint64_s r;
        r.c.l = 0;
        r.c.h = 0;
        return r.r;
/*@=usedef*/
#else
        return (HGint64)(0);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Assigns 32-bit integer into high 32 bits of a 64-bit
 *                      integer and sets low part to zero
 * \param       a       32-bit input value
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgSet64x (HGint32 hi)
{
#if defined (HG_USE_UNIONS_64)
/*@-usedef*/
        HGint64_s r;
        r.c.h = hi;
        r.c.l = 0;
        return r.r;
/*@=usedef*/
#else
        return ((HGint64)(hi))<<32;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns high 32 bits of a 64-bit integer
 * \param       a       64-bit integer
 * \return      High 32 bits
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgGet64h (const HGint64 a)
{
#if defined (HG_USE_UNIONS_64)
        HGint64_s r;
        r.r = a;
        return r.c.h;
#else
        return (HGint32)(((HGuint64)(a)) >> 32);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns high 32 bits of a 64-bit unsigned integer
 * \param       a       64-bit unsigned integer
 * \return      High 32 bits
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint32 hgGet64uh (const HGuint64 a)
{
#if defined (HG_USE_UNIONS_64)
        HGuint64_s r;
        r.r = a;
        return r.c.h;
#else
        return (HGuint32)(a >> 32);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns low 32 bits of a 64-bit unsigned integer
 * \param       a       64-bit unsigned integer
 * \return      Low 32 bits
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint32 hgGet64ul (const HGuint64 a)
{
        return (HGuint32)(a);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns low 32 bits of a 64-bit integer
 * \param       a       64-bit integer
 * \return      Low 32 bits (note that return value is signed!)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgGet64l (const HGint64 a)
{
#if (HG_COMPILER == HG_COMPILER_RVCT)
        HGint64_s r;
        r.r = a;
        return r.c.l;
#else
        return (HGint32)(a);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns Nth bit of a 64-bit integer
 * \param       x       64-bit integer
 * \param       offset  Bit offset [0,63]
 * \return      offset'th bit [0,1]
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgGetBit64 (
        HGint64         x,
        int                     offset)
{
        HG_ASSERT (offset >= 0 && offset <= 63);
#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_extract_pi32(_mm_srli_si64 (_mm_cvtsi64_m64(x), offset),0) & 1;
#elif (HG_COMPILER == HG_COMPILER_GCC && (HG_CPU != HG_CPU_X86_64)) || (HG_COMPILER == HG_COMPILER_RVCT) || (HG_COMPILER == HG_COMPILER_INTELC)
        {
                HGint64_s r;
                r.r = x;
                return (offset >= 32) ? ((r.c.h >> (offset&31)) & 1) : ((r.c.l >> offset) & 1);
        }
#elif (HG_COMPILER == HG_COMPILER_MSC)
        {
                HGint32 mask = (offset >= 32) ? 0 : -1;
                HGint32 a    = hgGet64h(x) + ((hgGet64l(x)-hgGet64h(x)) & mask);
                return (a >> offset) & 1;
        }
#else
        return (HGint32)((x >> offset)&1);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Sets specified bit of a 64-bit integer
 * \param       x               64-bit integer
 * \param       offset  Bit offset
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgSetBit64 (
        HGint64         x,
        int                     offset)
{
        HG_ASSERT (offset >= 0 && offset <= 63);
        {
#if (HG_COMPILER == HG_COMPILER_GCC && (HG_CPU != HG_CPU_X86_64)) || (HG_COMPILER == HG_COMPILER_RVCT)  || (HG_COMPILER == HG_COMPILER_INTELC)
        {
                HGint64_s       r;
                HGint32         ofsMask = 1<<(offset & 31);
                r.r = x;
                if (offset & 0x20)
                        r.c.h |= ofsMask;
                else
                        r.c.l |= ofsMask;
                return r.r;
        }
#elif (HG_COMPILER == HG_COMPILER_MSC)

                HGint32 h   = hgGet64h(x);
                HGint32 l   = hgGet64l(x);
                HGint32 foo = 1 << offset;

                if (offset & 32)
                        return hgSet64(h | foo, l);
                return hgSet64(h, l | foo);

#else
                return x | ((HGint64)(1) << offset);
#endif
        }
}

/*-------------------------------------------------------------------*//*!
 * \brief       Clears specified bit of a 64-bit integer
 * \param       x               64-bit integer
 * \param       offset  Bit offset [0,63]
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgClearBit64 (
        HGint64 x,
        int             offset)
{
#if (HG_COMPILER == HG_COMPILER_GCC && (HG_CPU != HG_CPU_X86_64)) || (HG_COMPILER == HG_COMPILER_RVCT) || (HG_COMPILER == HG_COMPILER_INTELC)
                HGint64_s       r;
                HGint32         ofsMask = 1<<(offset & 31);
                r.r = x;
                if (offset & 0x20)
                        r.c.h &=~ofsMask;
                else
                        r.c.l &=~ofsMask;
                return r.r;
#elif (HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_HCVC)
                HGuint32 m              = (offset >= 32) ? 0xffffffffu : 0;
                HGuint32 ofsMask = ~(1u << offset);

                return hgSet64(hgGet64h(x) & (ofsMask | ~m),hgGet64l(x) & (ofsMask | m));

#else
        return x &~ ((HGint64)(1) << offset);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs logical or between two 64-bit integer
 * \param       a       First 64-bit integer
 * \param       b       Second 64-bit integer
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgOr64 (
        const HGint64 a,
        const HGint64 b)
{
        return a | b;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs logical eor between two 64-bit integer
 * \param       a       First 64-bit integer
 * \param       b       Second 64-bit integer
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgXor64 (
        const HGint64 a,
        const HGint64 b)
{
        return a^b;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs logical and between two 64-bit integer
 * \param       a       First 64-bit integer
 * \param       b       Second 64-bit integer
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgAnd64 (
        const HGint64 a,
        const HGint64 b)
{
        return a & b;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs 64-bit addition
 * \param       a       First 64-bit value
 * \param       b       Second 64-bit value
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgAdd64 (
        const HGint64 a,
        const HGint64 b)
{
        return a+b;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Increments 64-bit value by one
 * \param       a       64-bit value
 * \return      64-bit value
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgInc64 (
        HGint64 a)
{
#if defined(HG_GCC_ARM_ASSEMBLER)
        HGint64_s r;

        r.r = a;

    __asm__ ("adds  %[l], %[l], #1\n\t"\
                         "adc   %[h], %[h], #0\n\t"\
                         : [h] "+r"  (r.c.h),
                           [l] "+r"  (r.c.l)
                         :
                         : "cc"
                         );

        return r.r;
#elif defined (HG_RVCT_ARM_ASSEMBLER)
        HGint32 h = hgGet64h(a);
        HGint32 l = hgGet64l(a);

        __asm
        {
                ADDS l,l,1
                ADC  h,h,0
        }

        return hgSet64(h,l);
#else
        return a+1;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Decrements 64-bit value by one
 * \param       a       64-bit value
 * \return      64-bit value
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgDec64 (HGint64 a)
{
#if defined(HG_GCC_ARM_ASSEMBLER)
        HGint64_s r;
        r.r = a;
    __asm__ ("subs  %[l], %[l], #1\n\t"\
                         "sbc   %[h], %[h], #0\n\t"\
                         : [h] "+r"  (r.c.h),
                           [l] "+r"  (r.c.l)
                         :
                         : "cc"
                         );
        return r.r;
#elif defined (HG_RVCT_ARM_ASSEMBLER)
        HGint32 h = hgGet64h(a);
        HGint32 l = hgGet64l(a);
        __asm
        {
                SUBS l,l,1
                SBC  h,h,0
        }
        return hgSet64(h,l);
#else
        return a - 1;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       64-bit integer subtraction
 * \param       a       First 64-bit integer
 * \param       b       Second 64-bit integer
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgSub64 (
        const HGint64 a,
        const HGint64 b)
{
        return a-b;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns boolean indicating whether a < b
 * \param       a       First 64-bit integer
 * \param       b       Second 64-bit integer
 * \return      a < b -> 1, else 0
 * \todo        [wili 29/Jan/03] Optimize for SH3!!
 *//*-------------------------------------------------------------------*/

HG_INLINE HGbool hgLessThan64 (
        const HGint64 a,
        const HGint64 b)
{
#if defined(HG_GCC_ARM_ASSEMBLER)
        HGint64_s aa,bb;

        aa.r = a;
        bb.r = b;

    __asm__ ("subs  %[al], %[al], %[bl]\n\t"\
                         "sbcs  %[al], %[ah], %[bh]\n\t"\
                         "movlt %[al], #1\n\t"\
                         "movge %[al], #0\n\t"

                         : [al] "+r" (aa.c.l)                   /* a.l is both read and written by the instruction (indicated by +) */
                         : [ah] "r" (aa.c.h),
                           [bl] "r" (bb.c.l),
                           [bh] "r" (bb.c.h)
                         : "cc"
                         );
        return aa.c.l;
#elif defined (HG_MSC_X86_ASSEMBLER)
        __asm
        {
                xor             eax,eax
                mov             ecx,dword ptr [a + 0]
                sub             ecx,dword ptr [b + 0]
                mov             ecx,dword ptr [a + 4]
                sbb             ecx,dword ptr [b + 4]
                setl    al
        }
        /* return value in eax */

#elif (HG_COMPILER == HG_COMPILER_GCC && HG_CPU == HG_CPU_SH)

        HGint32 r0,r1,r2;

        r0 = (hgGet64h(a) == hgGet64h(b));
        r2 = (hgGet64h(a)  < hgGet64h(b));
        r1 = ((HGuint32)hgGet64l(a) < (HGuint32)hgGet64l(b));

        return (r0 & r1) | r2;
#elif defined (HG_RVCT_ARM_ASSEMBLER)
        HGint32 ah = hgGet64h(a);
        HGint32 al = hgGet64l(a);
        HGint32 bh = hgGet64h(b);
        HGint32 bl = hgGet64l(b);
        HGint32 tmp;

        __asm {
                SUBS    tmp, al, bl
                SBCS    tmp, ah, bh
                MOVLT   tmp,#1
                MOVGE   tmp,#0
        }

        return tmp;

#else
        return (HGbool)(a < b);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns boolean indicating whether a < b (unsigned)
 * \param       a       First 64-bit unsigned integer
 * \param       b       Second 64-bit unsigned integer
 * \return      a < b -> 1, else 0
 * \note        THIS IS A CRITICAL ROUTINE (as all other unsigned comparison
 *                      routines use this)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGbool hgLessThan64u (
        HGuint64 a,
        HGuint64 b)
{
#if defined (HG_RVCT_ARM_ASSEMBLER)

        HGint32 ah = hgGet64h(a);
        HGint32 al = hgGet64l(a);
        HGint32 bh = hgGet64h(b);
        HGint32 bl = hgGet64l(b);
        HGint32 tmp;

        __asm {
                SUBS    tmp, al, bl
                SBCS    tmp, ah, bh
                MOVLO   tmp,#1
                MOVHS   tmp,#0
        }

        return tmp;
#elif defined(HG_GCC_ARM_ASSEMBLER)
        HGint64_s aa,bb;

        aa.r = a;
        bb.r = b;

    __asm__ ("subs  %[al], %[al], %[bl]\n\t"\
                         "sbcs  %[al], %[ah], %[bh]\n\t"\
                         "movlo %[al], #1\n\t"\
                         "movhs %[al], #0\n\t"

                         : [al] "+r" (aa.c.l)                   /* a.l is both read and written by the instruction (indicated by +) */
                         : [ah] "r" (aa.c.h),
                           [bl] "r" (bb.c.l),
                           [bh] "r" (bb.c.h)
                         : "cc"
                         );
        return aa.c.l;

#elif defined (HG_MSC_X86_ASSEMBLER)
        __asm
        {
                xor             eax,eax
                mov             ecx,dword ptr [a + 0]
                sub             ecx,dword ptr [b + 0]
                mov             ecx,dword ptr [a + 4]
                sbb             ecx,dword ptr [b + 4]
                setb    al
        }
        /* return value in eax */
#elif (HG_COMPILER == HG_COMPILER_GCC && HG_CPU == HG_CPU_SH)

        HGint32 r0,r1,r2;

        r0 = (hgGet64uh(a) == hgGet64uh(b));
        r2 = (hgGet64uh(a) < hgGet64uh(b));
        r1 = (hgGet64ul(a) < hgGet64ul(b));

        return (r0 & r1) | r2;

#else
        return (HGbool)(a < b);
#endif

}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns boolean indicating whether a < b (unsigned)
 * \param       a       64-bit unsigned integer
 * \param       b       32-bit unsigned integer
 * \return      a < b -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE HGbool hgLessThan64_32u (
        HGuint64 a,
        HGuint32 b)
{
#if defined (HG_RVCT_ARM_ASSEMBLER)

        HGint32 ah = hgGet64h(a);
        HGint32 al = hgGet64l(a);
        HGint32 tmp;

        __asm {
                SUBS    tmp, al, b
                SBCS    tmp, ah, #0
                MOVLO   tmp,#1
                MOVHS   tmp,#0
        }

        return tmp;
#elif defined(HG_GCC_ARM_ASSEMBLER)
        HGint64_s aa;

        aa.r = a;

    __asm__ ("subs  %[al], %[al], %[bl]\n\t"\
                         "sbcs  %[al], %[ah], #0\n\t"\
                         "movlo %[al], #1\n\t"\
                         "movhs %[al], #0\n\t"

                         : [al] "+r" (aa.c.l)
                         : [ah] "r" (aa.c.h),
                           [bl] "r" (b)
                         : "cc"
                         );
        return aa.c.l;
#else
        return (HGbool)(a < (HGuint64)(b));
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns boolean indicating whether a > b
 * \param       a       First 64-bit integer
 * \param       b       Second 64-bit integer
 * \return      a > b -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE HGbool hgGreaterThan64 (
        const HGint64 a,
        const HGint64 b)
{
        return hgLessThan64(b,a);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns boolean indicating whether a > b (unsigned)
 * \param       a       First 64-bit unsigned integer
 * \param       b       Second 64-bit unsigned integer
 * \return      a > b -> 1, else 0
 * \note        [wili 02/Oct/03] Fixed bug here!
 *//*-------------------------------------------------------------------*/

HG_INLINE HGbool hgGreaterThan64u (
        const HGuint64 a,
        const HGuint64 b)
{
        return hgLessThan64u(b,a);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns boolean value indicating whether 64-bit value is
 *                      zero
 * \param       a       64-bit integer
 * \return      a = 0 -> 1 else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE HGbool hgIsZero64 (const HGint64 a)
{
#if (HG_COMPILER == HG_COMPILER_INTELC)
        return ((hgGet64h(a) | hgGet64l(a)) == 0);
#elif defined(HG_USE_UNIONS_64)
        return (HGbool)(!(hgGet64h(a) | hgGet64l(a)));
#else
        return (HGbool)(a == 0);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns boolean value indicating whether 64-bit value is
 *                      less than zero (i.e. if the value is negative)
 * \param       a       64-bit integer
 * \return      a < 0 ? 1 : 0
 *//*-------------------------------------------------------------------*/

HG_INLINE HGbool hgLessThanZero64 (const HGint64 a)
{
        return (HGbool)((HGuint32)(hgGet64h(a)) >> 31);         /* sign bit set? */
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns boolean value indicating whether two 64-bit values
 *                      are equal
 * \param       a       First 64-bit integer
 * \param       b       Second 64-bit integer
 * \return      a = b -> 1 else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE HGbool hgEq64 (
        HGint64 a,
        HGint64 b)
{
#if (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU != HG_CPU_X86_64)
        HGint64_s aa,bb;
        aa.r = a;
        bb.r = b;

        return (HGbool)(((aa.c.h^bb.c.h)|(aa.c.l^bb.c.l))==0);
#elif (HG_COMPILER == HG_COMPILER_RVCT)
        HGint64_s aa,bb;
        aa.r = a;
        bb.r = b;
        return (aa.c.h == bb.c.h && aa.c.l == bb.c.l);

#elif (HG_COMPILER == HG_COMPILER_MSC)                  /* non-branching variant */
        a^=b;
        return (hgGet64h(a) | hgGet64l(a)) == 0;
#else
        return (HGbool)(a == b);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns smaller of two 64-bit value (signed comparison)
 * \param       a       First 64-bit value
 * \param       b       Second 64-bit value
 * \return      Smaller of the two values
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgMin64 (
        HGint64 a,
        HGint64 b)
{
#if defined (HG_GCC_ARM_ASSEMBLER)
        HGint64_s aa,bb;
        HGint32   tmp;

        aa.r = a;
        bb.r = b;

        __asm__ ("subs  %[tmp],%[al], %[bl]\n\t"        \
                         "sbcs  %[tmp],%[ah], %[bh]\n\t"        \
                         "movge %[ah], %[bh]\n\t"                       \
                         "movge %[al], %[bl]\n\t"                        :
                         [tmp] "=r&" (tmp),
                         [ah]  "+r"  (aa.c.h),
                         [al]  "+r"  (aa.c.l)  :
                         [bh]  "r"   (bb.c.h),
                         [bl]  "r"   (bb.c.l) :
                         "cc");

        return aa.r;
#elif (HG_COMPILER == HG_COMPILER_RVCT)

        HGint32 ah = hgGet64h(a);
        HGint32 al = hgGet64l(a);
        HGint32 bh = hgGet64h(b);
        HGint32 bl = hgGet64l(b);

        HGint32 tmp;

        HG_UNREF(tmp);

        __asm {
                SUBS    tmp, al, bl
                SBCS    tmp, ah, bh
                MOVGE   ah, bh
                MOVGE   al, bl
        }

        return hgSet64(ah,al);
#elif (HG_COMPILER == HG_COMPILER_INTELC)
        return a < b ? a : b;
#else
        return (hgLessThan64(a,b)) ? a : b;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns larger of two 64-bit value (signed comparison)
 * \param       a       First 64-bit value
 * \param       b       Second 64-bit value
 * \return      Larger of the two values
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgMax64 (
        HGint64 a,
        HGint64 b)
{
#if defined (HG_GCC_ARM_ASSEMBLER)
        HGint64_s aa,bb;
        HGint32   tmp;

        aa.r = a;
        bb.r = b;

        __asm__ ("subs  %[tmp],%[al], %[bl]\n\t"        \
                         "sbcs  %[tmp],%[ah], %[bh]\n\t"        \
                         "movlt %[ah], %[bh]\n\t"                       \
                         "movlt %[al], %[bl]\n\t"                        :
                         [tmp] "=r&" (tmp),
                         [ah]  "+r"  (aa.c.h),
                         [al]  "+r"  (aa.c.l)  :
                         [bh]  "r"   (bb.c.h),
                         [bl]  "r"   (bb.c.l) :
                         "cc");

        return aa.r;
#elif (HG_COMPILER == HG_COMPILER_RVCT)

        HGint32 ah = hgGet64h(a);
        HGint32 al = hgGet64l(a);
        HGint32 bh = hgGet64h(b);
        HGint32 bl = hgGet64l(b);
        HGint32 tmp;

        HG_UNREF(tmp);

        __asm {
                SUBS    tmp, al, bl
                SBCS    tmp, ah, bh
                MOVLT   ah, bh
                MOVLT   al, bl
        }

        return hgSet64(ah,al);
#elif (HG_COMPILER == HG_COMPILER_INTELC)
        return a >= b ? a : b;
#else
        return (hgLessThan64(a,b)) ? b : a;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns max(a,0)
 * \param       a       64-bit integer
 * \return      max(a,0)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgMaxZero64 (HGint64 a)
{
#if (HG_COMPILER == HG_COMPILER_GCC && HG_CPU != HG_CPU_X86_64) || (HG_COMPILER == HG_COMPILER_INTELC && HG_CPU == HG_CPU_ARM) || (HG_CPU == HG_CPU_VC02)
        if (hgGet64h(a) < 0)
                a = 0;
        return a;
#elif (HG_COMPILER == HG_COMPILER_RVCT) 
        HGint64_s r;
        r.r = a;
        r.c.l = r.c.l &~(r.c.h>>31);                            /* single instruction in ARM */
        r.c.h = r.c.h &~(r.c.h>>31);
        return r.r;
#else
        return a & ~(a>>63);
#endif
}


/*-------------------------------------------------------------------*//*!
 * \brief       Rotate 64-bit value to the left
 * \param       a       64-bit integer
 * \param       sh      Rotation value (internally modulated by 64)
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgRol64 (
        const HGint64   a,
        HGint32                 sh)
{
#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_cvtm64_si64(_mm_rori_si64(_mm_cvtsi64_m64(a),(64 - sh) & 63));
#else
        /* NB: don't use _rotl64() implementation of Intel C compiler,
           as it produces worse code 
        */

        HGint32 hi,lo;

        hi = hgGet64h(a);
        lo = hgGet64l(a);

        if (sh & 0x20)                          /* sh = [32,63] */
        {
                hi = hgGet64l(a);
                lo = hgGet64h(a);
        }

        sh &= 31;


#if (HG_SHIFTER_MODULO <= 32)
        if (sh)
#endif
        {
                HGint32 tlo = (HGint32)((HGuint32)(hi) >> (32-sh));
                HGint32 thi = (HGint32)((HGuint32)(lo) >> (32-sh));
                hi = thi | (hi << sh);
                lo = tlo | (lo << sh);
        }

        return hgSet64(hi, lo);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Rotate 64-bit value to the right
 * \param       a       64-bit integer
 * \param       sh      Rotation value (internally modulated by 64)
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgRor64 (
        const HGint64   a,
        HGint32                 sh)
{
#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_cvtm64_si64(_mm_rori_si64(_mm_cvtsi64_m64(a),sh));
#else
        /* NB: don't use _rotr64() implementation of Intel C compiler,
           as it produces worse code 
        */
        HGint32 hi,lo;

        if (sh & 0x20)                          /* sh = [32,63] */
        {
                hi = hgGet64l(a);
                lo = hgGet64h(a);
        } else
        {
                hi = hgGet64h(a);
                lo = hgGet64l(a);
        }

        sh &= 31;

#if (HG_SHIFTER_MODULO <= 32)
        if (sh)
#endif
        {
                HGint32 tlo = hi << (32-sh);
                HGint32 thi = lo << (32-sh);
                hi = thi | (HGint32)((HGuint32)(hi) >> sh);
                lo = tlo | (HGint32)((HGuint32)(lo) >> sh);
        }

        return hgSet64(hi, lo);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Logical shift left for 64-bit integers
 * \param       a       64-bit integer
 * \param       sh      Shift value [0,63]
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgLsl64 (
        HGint64         a,
        HGint32         sh)
{
        HG_ASSERT(sh >= 0 && sh <= 63);

#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_cvtm64_si64(_mm_slli_si64(_mm_cvtsi64_m64(a),sh));
#elif defined (HG_ARM_SHIFTERS)
        
        /* We have separate code paths for handling constant-propagated cases. These
           always generate the optimal code sequences. Note that the code below
           relies on a modulo-256 shifter!
        */

        if (HG_BUILTIN_CONSTANT(sh))
        {
                if (sh == 0)
                        return a;
                else if (sh == 1)
                        return hgAdd64(a,a);
                else if (sh >= 32)
                {
                        HGint64_s aa;
                        aa.r = a;
                        aa.c.h = aa.c.l << (sh-32);
                        aa.c.l = 0;
                        return aa.r;
                } else
                {
                        HGint64_s aa;
                        aa.r = a;

                        aa.c.h = (aa.c.h << sh) | ((HGuint32)(aa.c.l) >> (32-sh));
                        aa.c.l = aa.c.l << sh;

                        return aa.r;
                }
        }

        /* Implementation of the generic shifter (6 instructions for ARM)  */
        {
                HGint64_s       aa;
                HGint32         sh2 = 32 - sh;

                aa.r    = a;
                aa.c.h  = (HGint32)((aa.c.h << sh) | ((HGuint32)(aa.c.l) >> sh2) | (aa.c.l << (-sh2)));
                aa.c.l  = aa.c.l << sh;

                return aa.r;
        }

#elif defined(HG_MODULO_32_SHIFTERS)

        /* written this way because a number of compilers (MSC, Intel C) have bugs
           in their shifter constant propagation
        */
        
        {
                HGint32 h = hgGet64h(a);
                HGint32 l = hgGet64l(a);

                if (sh & 32)
                {
                        h   = l;
                        l   = 0;
                        sh &= 31;
                }

                if (sh)
                {
                        h       = (h << sh) + ((HGuint32)(l) >> (32-sh)); 
                        l       = (l << sh);
                }

                return hgSet64 (h,l);
        }

#else
        return a << sh;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Logical shift left for 64-bit integers, returns high 32 bits
 * \param       a       64-bit integer
 * \param       sh      Shift value [0,63]
 * \return      High 32 bits of a 64-bit integer
 **//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgLsl64h (
        const HGint64   a,
        HGint32                 sh)
{
        HG_ASSERT((int)sh >= 0 && sh <= 63);

#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_extract_pi32(_mm_slli_si64(_mm_cvtsi64_m64(a),sh),1);
#elif defined (HG_ARM_SHIFTERS)
        if (HG_BUILTIN_CONSTANT(sh))
        {
                if (sh == 0)
                        return hgGet64h(a);
                else if (sh >= 32)
                {
                        HGint64_s aa;
                        aa.r = a;
                        return aa.c.l << (sh-32);
                } else
                {
                        HGint64_s aa;
                        aa.r = a;
                        return (aa.c.h << sh) | ((HGuint32)(aa.c.l) >> (32-sh));
                }
        }

        /* Implementation of the generic shifter (5 instructions for ARM)  */
        {
                HGint64_s       aa;
                HGint32         sh2 = 32 - sh;

                aa.r    = a;
                return (HGint32)((aa.c.h << sh) | ((HGuint32)(aa.c.l) >> sh2) | (aa.c.l << (-sh2)));
        }
#elif defined (HG_MODULO_32_SHIFTERS)
        {
                HGint32         h               = hgGet64h(a);
                HGint32         l               = hgGet64l(a);
                HGint32         mask    = (sh >= 32) ? 0 : -1;
                HGuint32        l2;

                h       = (l & ~mask) | (h & mask);
                l2      = ((HGuint32)(l)>>1);
                l2      &= mask;
                h       = (h << sh) | (l2 >> (sh^31)); /*lint !e504*/ /* unusual shift */
                return h;
        }
#else
        return (HGint32)((a << sh)>>32);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs logical left shift of a 64-bit value, returns
 *                      high 32 bits
 * \param       a       64-bit integer value
 * \param       sh      Shift value [0,31]
 * \return      High 32 bits of resulting 64-bit value
 * \note        This function has a limited shift range !!!! [0,31]
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgLsl64h_0_31 (
        const HGint64   a,
        HGint32                 sh)
{
        HG_ASSERT (sh >= 0 && sh <= 31);

#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_extract_pi32(_mm_slli_si64(_mm_cvtsi64_m64(a),sh),1);
#elif defined (HG_ARM_SHIFTERS)
        {
                HGint64_s r;
                r.r = a;
                return ((r.c.h << sh) | ((HGuint32)(r.c.l) >> (32-sh)));
        }
#elif defined (HG_MODULO_32_SHIFTERS)
        {
                HGint32 ah = hgGet64h(a);
                HGint32 al = hgGet64l(a);
                HGint32 mask = (sh == 0) ? 0 : al;

                return ((ah << sh) | (((HGuint32)(mask)) >> (32-sh)));
        }
#else
        return (HGint32)((a << sh)>>32);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Logical shift right for 64-bit integers
 * \param       a       64-bit integer
 * \param       sh      Shift value [0,63]
 * \return      Resulting 64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgLsr64 (
        const HGint64   a,
        HGint32                 sh)
{
        HG_ASSERT(sh >= 0 && sh <= 63);

#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_cvtm64_si64(_mm_srli_si64(_mm_cvtsi64_m64(a),sh));
#elif defined (HG_ARM_SHIFTERS)

        /* handle constant cases */
        /* note: relies on MOD 256 shifter! */

        if (HG_BUILTIN_CONSTANT(sh))
        {
                if (sh == 0)
                        return a;
                else if (sh >= 32)
                {
                        HGint64_s       r;

                        r.r   = a;
                        r.c.l = (HGuint32)(r.c.h) >> (sh-32);
                        r.c.h = 0;

                        return r.r;
                } else
                {
                        HGint64_s r;

                        r.r             = a;
                        r.c.l   = ((HGuint32)(r.c.l) >> sh) | (r.c.h << (32-sh));
                        r.c.h   = ((HGuint32)(r.c.h) >> sh);

                        return r.r;
                }
        }

        /* generic case (6 instructions) */
        {
                HGint64_s       r;
                HGint32         sh2 = 32-sh;

                r.r             = a;
                r.c.l   = ((HGuint32)(r.c.l) >> sh) | (r.c.h << sh2) | ((HGuint32)(r.c.h) >> (-sh2));
                r.c.h   = (HGuint32)(r.c.h) >> sh;

                return r.r;
        }

#elif defined (HG_MODULO_32_SHIFTERS)
        {
                HGuint32        hi              = hgGet64h(a);
                HGuint32        lo              = hgGet64l(a);
                HGint32         m               = (sh >= 32) ? 0 : -1;  /* handle shifters [32,63] */

                lo += (hi-lo) &~m;
                hi &= m;

                {
                        HGint32  mask = (sh == 0) ? 0 : hi;
                        int      s    = sh & 31;
                        lo = ((HGuint32)(lo) >> s) | (mask << (32-sh));
                        hi >>= s;
                }

                return hgSet64((HGint32)hi,(HGint32)lo);
        }

#else
        return (HGint64)((HGuint64)(a) >> sh);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Logical shift right for 64-bit integers (with limited
 *                      shifter range)
 * \param       a       64-bit integer
 * \param       sh      Shift value [0,31]
 * \return      Resulting 64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgLsr64_0_31 (
        const HGint64   a,
        HGint32                 sh)
{
        HG_ASSERT (sh >= 0 && sh <= 31);

#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_cvtm64_si64(_mm_srli_si64(_mm_cvtsi64_m64(a),sh));
#elif defined (HG_ARM_SHIFTERS)

        if (HG_BUILTIN_CONSTANT(sh) && sh == 0)
                return a;
        else
        {
                HGint64_s       r;
                HGint32         sh2 = 32-sh;

                r.r             = a;
                r.c.l   = ((HGuint32)(r.c.l) >> sh) | (r.c.h << sh2);
                r.c.h   = (HGuint32)(r.c.h) >> sh;

                return r.r;
        }
#elif defined (HG_MODULO_32_SHIFTERS)
        {
                HGuint32        hi              = hgGet64h(a);
                HGuint32        lo              = hgGet64l(a);

                {
                        HGint32  mask = (sh == 0) ? 0 : hi;
                        lo = ((HGuint32)(lo) >> sh) | (mask << (32-sh));
                        hi >>= sh;
                }

                return hgSet64((HGint32)hi,(HGint32)lo);
        }

#else
        return (HGint64)((HGuint64)(a) >> sh);
#endif
}


/*-------------------------------------------------------------------*//*!
 * \brief       Performs arithmetic shift right for a 64-bit value
 * \param       a       64-bit integer
 * \param       sh      Shift value [0,63]
 * \return      Resulting 64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgAsr64 (
        const HGint64   a,
        HGint32                 sh)
{

#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_cvtm64_si64(_mm_srai_si64(_mm_cvtsi64_m64(a),sh));
#elif defined (HG_ARM_SHIFTERS)
        /* handle constant cases that are not compiled optimally by GCC */
        if (HG_BUILTIN_CONSTANT(sh))
        {
                if (sh == 0)
                        return a;
                else if (sh >= 32)
                {
                        HGint64_s r;

                        r.r   = a;
                        r.c.l = r.c.h >> (sh-32);
                        r.c.h = r.c.h >> 31;

                        return r.r;
                } else
                {
                        HGint64_s r;

                        r.r             = a;
                        r.c.l   = ((HGuint32)(r.c.l) >> sh) | (r.c.h << (32-sh));
                        r.c.h   = r.c.h >> sh;

                        return r.r;
                }
        }

        /* generic shifter */
        {

                HGint64_s       r;
                HGint32         sh2 = 32-sh;

                r.r             = a;
                r.c.l   = ((HGuint32)(r.c.l) >> sh) | (r.c.h << sh2);

                if (sh > 32)
                        r.c.l = r.c.h >> (-sh2);

                r.c.h   = r.c.h >> sh;

                return r.r;
        }

#elif defined (HG_MODULO_32_SHIFTERS)
        {
                HGint32 hi              = hgGet64h(a);
                HGint32 lo              = hgGet64l(a);
                HGint32 m       = 0;
                HGint32 ss      = 0;

                if(sh >= 32)
                {
                        m  = -1;
                        ss = 31;
                }

                lo += (hi-lo) & m;
                hi = hi >> ss;                                                  /* remember: modulo-32 */

                {
                        int      s    = sh & 31;
                        HGint32  mask = s ? hi : 0;
                        lo = ((HGuint32)lo >> s) | (mask << ((32-sh)&31));
                        hi >>= s;
                }

                return hgSet64((HGint32)hi,(HGint32)lo);
        }
#else
        HG_ASSERT(sh >= 0 && sh <= 63);
        return (a >> sh);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs arithmetic shift right for a 64-bit value
 * \param       a       64-bit integer
 * \param       sh      Shift value [0,31]
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgAsr64_0_31 (
        const HGint64   a,
        HGint32                 sh)
{

#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_cvtm64_si64(_mm_srai_si64(_mm_cvtsi64_m64(a),sh));
#elif defined (HG_ARM_SHIFTERS)
        HGint64_s       r;

        HG_ASSERT(sh >= 0 && sh <= 31);

        r.r        = a;
        r.c.l  = ((HGuint32)(r.c.l) >> sh) | (r.c.h << (32-sh));
        r.c.h  = r.c.h >> sh;

        return r.r;

#elif defined (HG_MODULO_32_SHIFTERS)
        {
                HGint32 hi              = hgGet64h(a);
                HGint32 lo              = hgGet64l(a);

                {
                        HGint32  mask = (sh&31) ? hi : 0;
                        lo = ((HGuint32)(lo) >> sh) | (mask << (32-sh));
                        hi >>= sh;
                }

                return hgSet64((HGint32)hi,(HGint32)lo);
        }
#else
        HG_ASSERT(sh >= 0 && sh <= 31);
        return (a >> sh);
#endif
}


/*-------------------------------------------------------------------*//*!
 * \brief       Performs arithmetic shift right and returns bottom 32
 *                      bits of the result
 * \param       a       64-bit integer
 * \param       sh      Shift value [0,63]
 * \return      Low 32 bits of a 64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgAsr64l (
        const HGint64   a,
        HGint32                 sh)
{
#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_extract_pi32(_mm_srai_si64(_mm_cvtsi64_m64(a),sh),0);
#elif defined (HG_MODULO_32_SHIFTERS)
        {
                HGint32 hi              = hgGet64h(a);
                HGint32 lo              = hgGet64l(a);
                HGint32 m       = 0;
                HGint32 ss      = 0;

                if(sh >= 32)
                {
                        m  = -1;
                        ss = 31;
                }

                lo += (hi-lo) & m;
                hi = hi >> ss;                                                  /* remember: modulo-32 */

                {
                        int      s    = sh & 31;
                        HGint32  mask = s ? hi : 0;
                        lo = ((HGuint32)lo >> s) | (mask << ((32-sh)&31));
                }

                return lo;
        }
#else
        HG_ASSERT(sh <= 63);
        return hgGet64l(hgAsr64(a,sh));
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs 64-bit shift operation, returns low 32 bits
 * \param       a       64-bit integer
 * \param       sh      Shift value [0,63]
 * \return      Low 32 bits of a 64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgLsr64l (
        const HGint64   a,
        HGint32                 sh)
{
        HG_ASSERT(sh >= 0 && sh <= 63);

#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _mm_extract_pi32(_mm_srli_si64(_mm_cvtsi64_m64(a),sh),0);
#elif defined (HG_ARM_SHIFTERS)
        {
                HGint64_s       r;
                HGint32         sh2 = 32-sh;

                r.r     = a;
                return ((HGuint32)(r.c.l) >> sh) | (r.c.h << sh2) | ((HGuint32)(r.c.h) >> (-sh2));
        }
#elif defined (HG_MODULO_32_SHIFTERS)
        {
                HGuint32        hi              = hgGet64h(a);
                HGuint32        lo              = hgGet64l(a);
                HGint32         m               = (sh >= 32) ? 0 : -1;  /* handle shifters [32,63] */

                lo += (hi-lo) &~ (HGuint32)m;
                hi &= (HGuint32)m;

                {
                        HGint32  mask = (sh == 0) ? 0 : hi;
                        lo = ((HGuint32)(lo) >> (sh&31)) | (mask << (32-sh));
                }

                return lo;
        }
#else
        return (HGint32)(((HGuint64)(a))>>sh);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Negates a 64-bit value
 * \param       a       Input 64-bit integer
 * \return      64-bit integer after negation
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgNeg64 (const HGint64 a)
{
#if defined (HG_RVCT_ARM_ASSEMBLER)

        int h = hgGet64h(a);
        int l = hgGet64l(a);

        __asm {
                RSBS l, l, #0
                RSC  h, h, #0
        }

        return hgSet64(h,l);

#else
        return -a;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs logical not for a 64-bit integer
 * \param       a       Input 64-bit integer
 * \return      64-bit integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgNot64 (const HGint64 a)
{
        return ~a;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns absolute value of 64-bit integer
 * \param       a       Input 64-bit integer
 * \return      Absolute value of 64-bit integer
 * \note        If high 32 bits of a are 0x80000000, the result is still
 *                      negative!
 * \todo        Improve for RVCT
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgAbs64 (HGint64 a)
{

#if (HG_COMPILER == HG_COMPILER_RVCT)
        HGint64 b = hgNeg64(a);
        if (hgLessThanZero64(a))
                a = b;
        return a;
#elif (HG_COMPILER == HG_COMPILER_INTELC)
        return _abs64(a);
#elif (HG_COMPILER == HG_COMPILER_MSC)
        HGint64 r = hgNeg64(a);
        if (hgGet64h(a) < 0)
                a = r;
        return a;
#else
        if (a < 0)
                return -a;
        return a;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns negative absolute value of 64-bit integer
 * \param       a       Input 64-bit integer
 * \return      Negative absolute value of 64-bit integer
 * \todo        Improve for RVCT
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgNabs64 (HGint64 a)
{
#if (HG_COMPILER == HG_COMPILER_GCC) || (HG_COMPILER == HG_COMPILER_RVCT) || (HG_COMPILER == HG_COMPILER_INTELC)
        HGint64 r = hgNeg64(a);
        if (!(hgGet64h(a) < 0))
                a = r;

        return a;
#else
        if (a >= 0)
                return -a;
        return a;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       32x32->64-bit signed multiplication
 * \param       a       32-bit signed integer
 * \param       b       32-bit signed integer
 * \return      64-bit signed result of the multiplication
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgMul64 (
        HGint32 a,
        HGint32 b)
{
#if defined (HG_GCC_ARM_ASSEMBLER) 
    HGint64_s r;

        __asm__ (" smull %[rlo], %[rhi], %[a], %[b]\n"
                         : [rlo] "=r&" (r.c.l),
                           [rhi] "=r&" (r.c.h)
                         : [a]   "r"   (a),
                           [b]   "r"   (b)
                         );

        return r.r;
#elif defined (HG_GCC_SH_ASSEMBLER)
        HGint32 h, l;
    __asm__ ("dmuls.l %[a], %[b]\n\t"
                         "sts     macl,%[l]\n\t"
                         "sts     mach,%[h]\n\t"
                         : [h] "=r" (h), [l] "=r" (l)
                         : [a] "r" (a), [b] "r"(b)
                         : "mach", "macl" );
        return hgSet64(h, l);
#elif defined (HG_MSC_X86_ASSEMBLER)
        HGint64_s r;
        __asm
        {
                mov             eax,a
                imul    b
                mov             [r.c.h],edx
                mov             [r.c.l],eax
        }
        return r.r;
#else
        return (HGint64)(a) * (HGint32)(b);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       32x32->64-bit unsigned multiplication
 * \param       a       32-bit unsigned integer
 * \param       b       32-bit unsigned integer
 * \return      64-bit unsigned result of the multiplication
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint64 hgMulu64 (
        HGuint32 a,
        HGuint32 b)
{
#if defined (HG_GCC_ARM_ASSEMBLER) 
    HGuint64_s r;

        __asm__ (" umull %[rlo], %[rhi], %[a], %[b]\n"
                         : [rlo] "=r&" (r.c.l),
                           [rhi] "=r&" (r.c.h)
                         : [a]   "r"   (a),
                           [b]   "r"   (b)
                         );

        return r.r;
#elif defined (HG_MSC_X86_ASSEMBLER)
        HGint64_s r;
        __asm
        {
                mov             eax,a
                mul             b
                mov             [r.c.h],edx
                mov             [r.c.l],eax
        }
        return r.r;
#elif 0 /* defined (HG_GCC_SH_ASSEMBLER)*/
        HGint32 h, l;
    __asm__ ("dmulu.l %[a], %[b]\n\t"
                         "sts     macl,%[l]\n\t"
                         "sts     mach,%[h]\n\t"
                         : [h] "=r" (h), [l] "=r" (l)
                         : [a] "r" (a), [b] "r"(b)
                         : "mach", "macl" );
        return hgSet64(h, l);
#else
        return ((HGuint64)(a) * (HGuint32)(b));
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       64x64->64-bit signed multiplication (returns lowest 64-bits)
 * \param       a       64-bit signed integer
 * \param       b       64-bit signed integer
 * \return      64-bit signed result of the multiplication (lowest bits)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgMul64x64 (
        HGint64 a,
        HGint64 b)
{
#if defined (HG_MSC_X86_ASSEMBLER)
        HGint64_s r;

        __asm
        {
                mov             ecx,dword ptr [a+0]
                imul    ecx,dword ptr [b+4]
                mov             eax,dword ptr [a+4]
                imul    eax,dword ptr [b+0]
                add             ecx,eax
                mov             eax,dword ptr [a+0]
                mul             dword ptr [b+0]
                add             edx,ecx
                mov             [r.c.h],edx
                mov             [r.c.l],eax
        }
        return r.r;
#elif !defined (HG_NATIVE_64_BIT_INTS)
        HGint64 res = hgToSigned64(hgMulu64((HGuint32)hgGet64l(a), (HGuint32)hgGet64l(b)));
        res = hgSet64(hgGet64h(res) + hgGet64h(a) * hgGet64l(b), hgGet64l(res));
        res = hgSet64(hgGet64h(res) + hgGet64h(b) * hgGet64l(a), hgGet64l(res));
        return res;
#else
        return a * b;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       64x64->64-bit unsigned multiplication (returns lowest 64-bits)
 * \param       a       64-bit unsigned integer
 * \param       b       64-bit unsigned integer
 * \return      64-bit unsigned result of the multiplication (lowest bits)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint64 hgMulu64x64 (
        HGuint64 a,
        HGuint64 b)
{
#if !defined (HG_NATIVE_64_BIT_INTS)
        HGint64 res = hgToSigned64(hgMulu64(hgGet64ul(a), hgGet64ul(b) ));
        res = hgSet64(hgGet64h(res) + hgGet64uh(a) * hgGet64ul(b), hgGet64l(res));
        res = hgSet64(hgGet64h(res) + hgGet64uh(b) * hgGet64ul(a), hgGet64l(res));
        return hgToUnsigned64(res);
#else
        return a * b;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       64x32->64-bit signed multiplication (returns lowest 64-bits)
 * \param       a       64-bit signed integer
 * \param       b       32-bit signed integer
 * \return      64-bit signed result of the multiplication (lowest bits)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgMul64x32 (
        HGint64 a,
        HGint32 b)
{
#if defined (HG_MSC_X86_ASSEMBLER)
        HGint64_s r;
        __asm
        {
                mov             ecx,dword ptr [b]
                mov             eax,dword ptr [a+4]
                imul    eax,ecx
                mov             ebx,eax                                 /* ebx = ah*b                                   */
                sar             ecx,31                                  /* ecx = b >> 31                                */
                mov             eax,dword ptr [a+0]
                and             ecx,eax                                 /* ecx = al & (b>>31)                   */
                sub             ebx,ecx                                 /* ebx = ah*b - (al & (b>>31))  */
                mul             dword ptr [b]                   /* edx:eax = al * b                             */
                add             edx,ebx                                 /* edx += ebx                                   */
                mov             [r.c.h],edx
                mov             [r.c.l],eax
        }
        return r.r;
#elif !defined (HG_NATIVE_64_BIT_INTS)
        HGint64 res = hgToSigned64(hgMulu64((HGuint32)hgGet64l(a), (HGuint32)b));
        res = hgSet64(hgGet64h(res) + (hgGet64h(a) * b) - (hgGet64l(a)&(b>>31)),        hgGet64l(res));
        return res;
#else
        return a * b;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       64x32->64-bit unsigned multiplication (returns lowest 64-bits)
 * \param       a       64-bit unsigned integer
 * \param       b       32-bit unsigned integer
 * \return      64-bit unsigned result of the multiplication (lowest 64 bits)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint64 hgMulu64x32 (
        HGuint64 a,
        HGuint32 b)
{
#if defined (HG_MSC_X86_ASSEMBLER)
        HGuint64_s r;

        __asm
        {
                mov             eax,dword ptr [a+4]
                mov             ebx,dword ptr [b]
                mul             ebx                                             /* edx:eax = ah * b */
                mov             ecx,eax                                 /* ecx = lo(ah*b)   */
                mov             eax,dword ptr [a+0]
                mul             ebx                                             /* edx:eax = al * b */
                add             edx,ecx
                mov             [r.c.h],edx
                mov             [r.c.l],eax
        }
        return r.r;
#elif !defined (HG_NATIVE_64_BIT_INTS)
        HGint64 res = hgToSigned64(hgMulu64(hgGet64ul(a), b ));
        res = hgSet64(hgGet64h(res) + hgGet64uh(a) * b, hgGet64l(res));
        return hgToUnsigned64(res);
#else
        return a * b;
#endif
}


/*-------------------------------------------------------------------*//*!
 * \brief       Performs a 32*32 -> 64 signed multiplication followed
 *                      by a 64-bit addition
 * \param       a               64-bit signed integer
 * \param       b               32-bit signed integer
 * \param       c               32-bit signed integer
 * \return      a+(b*c)
 * \note        This function produces the same results as
 *                      hgAdd64(a,hgMul64(b,c)) but is provided as a separate
 *                      function because some platforms have custom instructions
 *                      for implementing this.
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgMadd64 (
        HGint64 a,
        HGint32 b,
        HGint32 c)
{
#if defined (HG_GCC_ARM_ASSEMBLER) 
    HGint64_s r;
        r.r = a;

        __asm__ (" smlal %[rlo], %[rhi], %[b], %[c]\n"
                         : [rlo] "+r&" (r.c.l),
                           [rhi] "+r&" (r.c.h)
                         : [b]   "r"   (b),
                           [c]   "r"   (c)
                         );

        return r.r;
#elif defined (HG_MSC_X86_ASSEMBLER)
        __asm
        {
                mov             eax,b
                imul    c
                add             dword ptr [a+0],eax
                adc             dword ptr [a+4],edx
        }
        return a;
#else
        return hgAdd64(a,hgMul64(b,c));
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs a 32*32 -> 64 signed multiplication followed
 *                      by a 64-bit addition, return high 32-bits
 * \param       a               64-bit signed integer
 * \param       b               32-bit signed integer
 * \param       c               32-bit signed integer
 * \return      (a+(b*c))>>32
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgMadd64h (
        HGint64 a,
        HGint32 b,
        HGint32 c)
{
#if defined (HG_GCC_ARM_ASSEMBLER) 

    HGint64_s r;

        r.r = a;

        __asm__ (" smlal %[rlo], %[rhi], %[b], %[c]\n"
                         : [rlo] "+r" (r.c.l),
                           [rhi] "+r" (r.c.h)
                         : [b]   "r"   (b),
                           [c]   "r"   (c)
                         );

        return r.c.h;

#elif defined (HG_MSC_X86_ASSEMBLER)
        int r;
        __asm
        {
                mov             eax,b
                imul    c
                add             eax,dword ptr [a+0]
                adc             edx,dword ptr [a+4]
                mov             r,edx
        }
        return r;
#else
        return hgGet64h(hgAdd64(a,hgMul64(b,c)));
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs a 32*32 -> 64 unsigned multiplication followed
 *                      by a 64-bit addition
 * \param       a               64-bit unsigned integer
 * \param       b               32-bit unsigned integer
 * \param       c               32-bit unsigned integer
 * \return      a+(b*c)
 * \note        This function produces the same results as
 *                      hgAdd64(hgMulu64(a,b),c) but is provided as a separate
 *                      function because some platforms have custom instructions
 *                      for implementing this.
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint64 hgMaddu64 (
        HGuint64 a,
        HGuint32 b,
        HGuint32 c)
{
#if defined (HG_GCC_ARM_ASSEMBLER) 
    HGuint64_s r;
        r.r = a;

        __asm__ (" umlal %[rlo], %[rhi], %[b], %[c]\n"
                         : [rlo] "+r" (r.c.l),
                           [rhi] "+r" (r.c.h)
                         : [b]   "r"   (b),
                           [c]   "r"   (c)
                         );

        return r.r;
#elif defined (HG_MSC_X86_ASSEMBLER)
        __asm
        {
                mov             eax,b
                mul             c
                add             dword ptr [a+0],eax
                adc             dword ptr [a+4],edx
        }
        return a;
#else
        return hgToUnsigned64(hgAdd64(hgToSigned64(a),hgToSigned64(hgMulu64(b,c))));
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs a 32x32->64 signed multiplication followed by
 *                      a shift to the right
 * \param       a               32-bit signed integer
 * \param       b               32-bit signed integer
 * \param       shift   Shift value [0,63]
 * \return      64-bit integer
 * \note        This function is somewhat obsolete as exactly the same
 *                      code is generated by mul+asr combination. It is provided
 *                      mainly so that assembly optimizations could be performed
 *                      for some platforms.
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgMulAsr64 (
        HGint32         a,
        HGint32         b,
        HGint32         shift)
{
#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return  _mm_cvtm64_si64(_mm_srai_si64(_mm_mia_si64 (_mm_setzero_si64(), a, b), shift));
#else
        HG_ASSERT (shift <= 63);
        return hgAsr64(hgMul64(a,b),shift);
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns number of leading zeroes for a 64-bit integer
 * \param       a       Input 64-bit integer
 * \return      Number of leading zeroes [0,64]
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgClz64 (const HGint64 a)
{

#if (HG_COMPILER == HG_COMPILER_INTELC)
        return _CountLeadingZeros64(a);
#elif (HG_COMPILER == HG_COMPILER_MSC) && defined (HG_SUPPORT_ASM)

        __asm
        {
                mov             eax,0x3f
                bsr             eax,dword ptr [a+4]
                xor             eax,0x1f
                
                cmp             eax,32
                jl              outta

                mov             ecx,0x3f
                bsr             ecx,dword ptr [a+0]
                xor             ecx,0x1f
                add             eax,ecx
outta:
        }

#else
        HGuint32        x               = (HGuint32)hgGet64h(a);
        int         bits        = 0;

        if (x == 0)
        {
                x               = (HGuint32)hgGet64l(a);
                bits    = 32;
        }

        return bits + hgClz32(x);
#endif
}


/*-------------------------------------------------------------------*//*!
 * \brief       Returns number of trailing zeroes for a 64-bit integer
 * \param       a       Input 64-bit integer
 * \return      Number of trailing zeroes [0,64]
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgCtz64 (const HGint64 a)
{
        HGuint32        x               = (HGuint32)hgGet64l(a);
        int         bits        = 0;

        if (x == 0)
        {
                x               = (HGuint32)hgGet64h(a);
                bits    = 32;
        }

        return bits + hgCtz32(x);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Population count of a 64-bit integer
 * \param       a       64-bit input integer
 * \return      Population count [0,64] (i.e., number of set bits)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgPop64 (const HGint64 a)
{
        return hgPop32((HGuint32)hgGet64h(a)) +
                   hgPop32((HGuint32)hgGet64l(a));
}


/*-------------------------------------------------------------------*//*!
 * \brief       Performs addition, saturates signed sum to
 *          0x80000000 and 0x7fffffff
 * \param       a       First 32-bit signed integer
 * \param       b       Second 32-bit signed integer
 * \return      saturated sum
 * \todo        move to hgInt32.h
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 hgAdds32 (
        HGint32 a,
        HGint32 b)
{
#if (HG_COMPILER == HG_COMPILER_INTELC) && defined (HG_SUPPORT_WMMX)
        return _AddSatInt (a,b);
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
        return _adds (a,b);
#elif defined (HG_GCC_ARM_ASSEMBLER) 
        const HGint32 t = 0x80000000;

    __asm__ ("adds  %[a], %[a], %[b]\n\t" \
                         "eorvs %[a], %[t], %[a], ASR #31\n\t" :
                         [a] "+r" (a) :
                         [b] "r" (b),
                         [t] "r" (t)  :
                         "cc"
                         );

        return a;

#elif defined (HG_RVCT_ARM_ASSEMBLER)

        const HGint32 t = 0x80000000;

        __asm
        {
                adds    a,a,b
                eorvs   a,t,a,ASR #31
        }
        return a;
#elif defined (HG_MSC_X86_ASSEMBLER)

        __asm
        {
                mov             eax,dword ptr [a]
                xor             ebx,ebx
                add             eax,dword ptr [b]
                mov             ecx,eax
                seto    bl                      /* ebx = 1 if overflows  */
                neg             ebx                     /* ebx = -1 if overflows */
                sar             ecx,31
                xor             ecx,0x80000000
                sub             ecx,eax
                and             ecx,ebx
                add             eax,ecx
        }
#else
        HGint32 v = hgGet64h(hgAdd64(hgSet64s(a),hgSet64s(b)));
        a += b;

        if((v ^ a) < 0) /* different sign? */
        {
                a = v^0x7fffffff;
        }

        return a;
#endif
}


/*-------------------------------------------------------------------*//*!
 * \brief       Internal routine for performing division
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint32 hgDiv32Internal (
        HGuint32        cio[1], 
        HGuint64        aio[1], 
        HGuint32        b, 
        HGint32         n)
{
        HG_ASSERT(n >= 0 && n <= 31);
        HG_ASSERT(n == 31 || cio[0] == 0);
        {
                HGuint32        q;
                HGuint32        ah = hgGet64uh(aio[0]);
                HGuint32        al = hgGet64ul(aio[0]);
                HGuint32        carry = cio[0];

#if ((HG_COMPILER == HG_COMPILER_ADS) || (HG_COMPILER == HG_COMPILER_RVCT)) && defined(HG_SUPPORT_ASM)

        HGint32         tmp;

        __asm
        {
                and             tmp,n,#0x18

                movs    carry,carry,LSR #1
                /* TST preserves _shifter_ carry out */

            tst     n,#4
                bne             _t1xx
                tst             n,#2
                bne             _t01x
                tst             n,#1
                bne             s1
                b               s0
_t1xx:  tst             n,#2
                bne             _t11x
                tst             n,#1
                bne             s5
                b               s4
_t01x:  tst             n,#1
        bne             s3
                b               s2
_t11x:  tst             n,#1
                bne             s7
        b               s6

lp:             movs    carry,carry,LSR #1

s7:
/* 7 */
                cmpcc   ah,b
                subcs   ah,ah,b
                adcs    al,al,al
                adcs    ah,ah,ah
s6:
/* 6 */
                cmpcc   ah,b
                subcs   ah,ah,b
                adcs    al,al,al
                adcs    ah,ah,ah
s5:
/* 5 */
                cmpcc   ah,b
                subcs   ah,ah,b
                adcs    al,al,al
                adcs    ah,ah,ah
s4:
/* 4 */
                cmpcc   ah,b
                subcs   ah,ah,b
                adcs    al,al,al
                adcs    ah,ah,ah
s3:
/* 3 */
                cmpcc   ah,b
                subcs   ah,ah,b
                adcs    al,al,al
                adcs    ah,ah,ah
s2:
/* 2 */
                cmpcc   ah,b
                subcs   ah,ah,b
                adcs    al,al,al
                adcs    ah,ah,ah
s1:
/* 1 */
                cmpcc   ah,b
                subcs   ah,ah,b
                adcs    al,al,al
                adcs    ah,ah,ah
s0:
/* 0 */
                cmpcc   ah,b
                subcs   ah,ah,b
                adcs    al,al,al
                adcs    ah,ah,ah

                adc             carry,carry,carry

                subs    tmp,tmp,#8
                bpl             lp

                mov             tmp,#2
                mov             tmp,tmp,LSL n
                sub             tmp,tmp,#1
                and             q,al,tmp
                bic             al,al,tmp               /* may not be necessary to clear out the quotient bits */
    }

#else
                HGuint64        a;

                q = 0;
                do
                {
                        if(!carry)      carry = (ah >= b);
                        if(carry)       ah -= b;
                        q = q+q+carry;
                        carry = (ah >= 0x80000000u ? 1 : 0);
                        a = hgToUnsigned64(hgAdd64(hgSet64(ah,al),hgSet64(ah,al)));
                        al = hgGet64ul(a);
                        ah = hgGet64uh(a);
                } while(n--);
#endif
                aio[0] = hgSet64u(ah,al);
                cio[0] = carry;
                return q;
        }

}

/*-------------------------------------------------------------------*//*!
 * \brief       Performs 64:32 unsigned division
 * \param       a       64-bit unsigned integer to be divided
 * \param       b       32-bit unsigned integer divisor
 * \return      (a/b) (64-bit unsigned)
 * \note        Division by zero is asserted in debug build
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint64 hgDivu64_32 (
        HGuint64 a,
        HGuint32 b)
{
        HG_ASSERT (b != 0);

        {
#if defined (HG_MSC_X86_ASSEMBLER)
        HGuint64 r;
        __asm
        {
                xor             edx, edx
                mov             ecx, dword ptr [b]
                mov             eax, dword ptr [a+4]
                div             ecx
                mov             dword ptr [r+4],eax
                mov             eax,dword ptr [a+0]
                div             ecx
                mov             dword ptr [r+0],eax
        }

        return r;
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
        return a / b;
#elif (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_PPC)
        return a / b;
#elif !defined (HG_NATIVE_64_BIT_INTS)

        HGuint32        qh, ql;
        HGint32         sha, shb;
        HGuint32        carry;

        if(hgLessThan64_32u(a,b))
                return hgSet64u(0,0);

        shb = hgClz32(b);
        b <<= shb;

        sha = hgClz64(hgToSigned64(a));
        a = hgToUnsigned64(hgLsl64(hgToSigned64(a),sha));

        carry = 0;
        qh = 0;

        if((shb-sha) >= 0)
                qh = hgDiv32Internal(&carry, &a, b, shb-sha);

        ql = hgDiv32Internal(&carry, &a, b, hgMin32(32+shb-sha,31));

        return hgSet64u(qh,ql);

#else
        return (a / b);
#endif
        }
}


/*-------------------------------------------------------------------*//*!
 * \brief       Performs 64:32 signed division
 * \param       a       64-bit signed integer to be divided
 * \param       b       32-bit signed integer divisor
 * \return      (a/b) (64-bit)
 * \note        Division by zero is asserted in debug build
 * \todo        Optimize for MSC6
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint64 hgDiv64_32 (
        HGint64 a,
        HGint32 b)
{
#if (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_PPC)
        return a / b;
#elif (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
        return a / b;
#elif !defined (HG_NATIVE_64_BIT_INTS)
        HGbool          sign = (HGbool)(((HGuint32)(hgGet64h(a)^b))>>31);
        HGint64         res;

        HG_ASSERT (b != 0);

        res = hgToSigned64(hgDivu64_32(hgToUnsigned64(hgAbs64(a)),(HGuint32)hgAbs32(b)));

        if (sign)                                       /* todo: avoid comparison?      */
                res = hgNeg64(res);

        return res;
#else
        HG_ASSERT (b != 0);
        return a / b;
#endif
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 0xffffffff:ffffffff / b
 * \param       b       32-bit unsigned integer divisor
 * \return      (0xffffffff:ffffffff/b) (64-bit unsigned)
 * \note        Division by zero is asserted in debug build
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint64 hgRcpu64 (
        HGuint32 b)
{
#if defined (HG_MSC_X86_ASSEMBLER)
        HGuint64 r;
        __asm
        {
                xor             edx, edx
                mov             ecx, dword ptr [b]
                mov             eax, -1
                div             ecx
                mov             dword ptr [r+4],eax
                mov             eax, -1
                div             ecx
                mov             dword ptr [r+0],eax
        }

        return r;
#else
        return hgDivu64_32(hgSet64u(0xffffffffu,0xffffffffu),b);
#endif
}

#undef HG_USE_UNIONS_64
#undef HG_ARM_SHIFTERS
#undef HG_GCC_ARM_ASSEMBLER
#undef HG_MSC_X86_ASSEMBLER
#undef HG_RVCT_ARM_ASSEMBLER
#undef HG_GCC_SH_ASSEMBLER

#if (HG_COMPILER == HG_COMPILER_INTELC)
#       pragma warning (pop)
#endif

/*@=shiftnegative =shiftimplementation =predboolint =boolops*/

/*----------------------------------------------------------------------*/


