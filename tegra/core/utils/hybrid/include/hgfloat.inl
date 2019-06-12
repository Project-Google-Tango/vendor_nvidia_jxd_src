#ifndef __HGFLOAT_INL
#define __HGFLOAT_INL
/*------------------------------------------------------------------------
 *
 * M3G %%VERSION_STRING%%
 * -----------------------------------------
 *
 * (C) 2004 Hybrid Graphics, Ltd.
 * All Rights Reserved.
 *
 * This file consists of unpublished, proprietary source code of
 * Hybrid Graphics, and is considered Confidential Information for
 * purposes of non-disclosure agreement. Disclosure outside the terms
 * outlined in signed agreement may result in irrepairable harm to
 * Hybrid Graphics and legal action against the party in breach.
 *
 * $Id: //hybrid/libs/hg/main/hgFloat.inl#3 $
 * $Date: 2007/01/04 $
 * $Author: tero $ 
 *
 *//**
 * \file
 * \brief       Inline code for HG emulated floating-point math
 * \author      wili@hybrid.fi
 *//*-------------------------------------------------------------------*/

#ifndef __HGINT32_H
#       include "hgint32.h"
#endif

/*@-shiftimplementation@*/

typedef HGuint32 HGifloat;          /* Floating-point type used internally  */

/*-------------------------------------------------------------------------
 * Floating-point constants
 *-----------------------------------------------------------------------*/

#define F_ZERO              0x00000000u
#define F_HALF              0x3f000000u
#define F_ONE               0x3f800000u
#define F_THREE             0x40400000u
#define F_PI                0x40490fdau
#define F_HALF_PI           0x3fc90fdau
#define F_MINUS_ZERO        0x80000000u
#define F_FLOAT_MAX         0x7f7fffffu
#define F_FLOAT_INF         0x7f800000u
#define F_FLOAT_MIN         0x00800000u

/*-------------------------------------------------------------------------
 * Internal constants
 *-----------------------------------------------------------------------*/

#define F_SIGN_MASK             0x80000000u
#define F_EXPONENT_SHIFT    23
#define F_EXPONENT_BIAS     126
#define F_MANTISSA_MASK     ((1u << 23) - 1)
#define F_HIDDEN_BIT_MASK       (1u << 23)

/*-------------------------------------------------------------------------
 * Prototypes for functions with large bodies (implemented in the .c file)
 * Note that most of these are of type ATTRIBUTE_CONST (a good compiler
 * can reorganize expressions then).
 *-----------------------------------------------------------------------*/

/* TODO: make sure that all compilers support this properly */
#define F_CALL HG_REGCALL HG_ATTRIBUTE_CONST 

HG_EXTERN_C_BLOCK_BEGIN

HGint32     F_CALL F_ICMP       (HGifloat a, HGint32 b);
HGint32     F_CALL F_ICHOP      (HGifloat x);                   
HGint32     F_CALL F_ICHOPSCALE     (HGifloat x, int shift);        /* (int)(chop)(x * pow(2.0f,shift)) (clamped into integer range) */
HGint64     F_CALL F_I64CHOPSCALE   (HGifloat x, int shift);        /* (HGint64)(chop)(x * pow(2.0f,shift)) (clamped into 64-bit integer range) */
HGint32     F_CALL F_F2UBYTE        (HGifloat a);

HGifloat    F_CALL F_CLEAN          (HGifloat a);
HGifloat    F_CALL F_CLEANFINITE    (HGifloat a);
HGifloat    F_CALL F_ADD            (HGifloat a, HGifloat b);
HGifloat    F_CALL F_MUL            (HGifloat a, HGifloat b);
HGifloat    F_CALL F_IMUL           (HGifloat a, HGint32  b);
HGifloat    F_CALL F_DIV            (HGifloat a, HGifloat b);
HGifloat    F_CALL F_MOD            (HGifloat a, HGifloat b);
HGifloat    F_CALL F_SHIFT          (HGifloat a, int shift);
HGifloat    F_CALL F_SQR            (HGifloat a);                   /* a*a                          */
HGifloat    F_CALL F_FLOOR          (HGifloat x);
HGifloat    F_CALL F_FRAC           (HGifloat x);
HGifloat    F_CALL F_LOG2           (HGifloat x);
HGifloat    F_CALL F_EXP2           (HGifloat x);
HGifloat    F_CALL F_POW            (HGifloat a, HGifloat b);
HGifloat    F_CALL F_MADD           (HGifloat a, HGifloat b, HGifloat c);
HGifloat    F_CALL F_CLAMP          (HGifloat a, HGifloat mn, HGifloat mx); /* max(min(a,mx), mn) -- assert: (mn <= mx), NAN -> [mn,mx] value       */
HGifloat    F_CALL F_ADD3           (HGifloat a, HGifloat b, HGifloat c);
HGifloat    F_CALL F_ADD4           (HGifloat a, HGifloat b, HGifloat c, HGifloat d);
HGifloat    F_CALL F_ADDABS3        (HGifloat a, HGifloat b, HGifloat c);
HGifloat    F_CALL F_ADDABS4        (HGifloat a, HGifloat b, HGifloat c, HGifloat d);

HGifloat    F_CALL F_DOT2           (HGifloat a, HGifloat b, HGifloat c, HGifloat d);                           
HGifloat    F_CALL F_DOT4           (HGifloat a, HGifloat b, HGifloat c, HGifloat d, HGifloat e, HGifloat f, HGifloat g, HGifloat h);
HGifloat    F_CALL F_DOT4HI         (HGifloat a, HGifloat b, HGifloat c, HGifloat d, HGifloat e, HGifloat f, HGifloat g, HGifloat h);

HGifloat    F_CALL F_UTOF           (HGuint32 x);                   /* (float)((unsigned int)(a))                       */
HGifloat    F_CALL F_ITOF           (HGint32 x);                    /* (float)x                                             */              
HGifloat    F_CALL F_ITOFSCALE      (HGint32 x, HGint32 shift);         
HGifloat    F_CALL F_I64TOFSCALE    (HGint64 x, HGint32 shift);     
HGifloat    F_CALL F_UTOFSCALE      (HGuint32 x, HGint32 shift);    
HGifloat    F_CALL F_FIXED2F        (HGint32 x);                    /* (float)(x) / 65536.0f                            */
HGint32     F_CALL F_IFLOOR         (HGifloat x);                   /* (int)(floor(x));                                     */
HGint32     F_CALL F_ICEIL          (HGifloat x);                   /* (int)(ceil(x));                                      */

HGifloat    F_CALL F_SQRT           (HGifloat a);                   /* sqrt(a)                          */
HGifloat    F_CALL F_SIN            (HGifloat a);
HGifloat    F_CALL F_COS            (HGifloat a);
HGifloat    F_CALL F_TAN            (HGifloat a);
HGifloat    F_CALL F_ACOS           (HGifloat x);
HGifloat    F_CALL F_RCP            (HGifloat a);                   /* 1.0f / a                         */
HGifloat    F_CALL F_RSQ            (HGifloat f);                   /* 1.0f / sqrt(f)                   */

HG_EXTERN_C_BLOCK_END

/*-------------------------------------------------------------------------
 * Macros for converting the input and output parameters
 * \todo [wili 16/Oct/04]       These should be NOPs for all compilers, i.e.,
 *                              we may want to implement them using the 
 *                              union trick
 *-----------------------------------------------------------------------*/

HG_INLINE HGifloat FIN (float x)        
{ 
    return (HGifloat)hgFloatBitPattern(x);
}

HG_INLINE float FOUT (HGifloat x)       
{ 
    return hgBitPatternToFloat(x);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Converts a floating point into a signed integer value
 *              that allows direct comparison using signed integer
 *              comparison instructions
 * \param       a       Floating point value
 * \return      Integer value that can be compared directly
 * \note    Handles -0.0f properly
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 F_MAKE_COMPARABLE (HGifloat a)
{
    HGint32 x = (HGint32)(a);
    if (x < 0)
        x = (HGint32)(0x80000000) - x;
    return x;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Compares two floating-point values and returns their
 *              relative ordering [-1,+1]
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return      a < b -> -1, a == b -> 0, a > b -> 1
 *//*-------------------------------------------------------------------*/

HG_INLINE HGint32 F_CMP (
    HGifloat a, 
    HGifloat b)
{
    HGint32 x = F_MAKE_COMPARABLE(a);
    HGint32 y = F_MAKE_COMPARABLE(b);

    return hgCmp32(x,y);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a <= b
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return      a <= b -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_CMPLE (HGifloat a, HGifloat b) /*@modifies@*/
{ 
    HGint32 x = F_MAKE_COMPARABLE(a);
    HGint32 y = F_MAKE_COMPARABLE(b);

    return (int)(x <= y);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a < b
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return      a < b -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_CMPLT (HGifloat a, HGifloat b)          
{ 
    HGint32 x = F_MAKE_COMPARABLE(a);
    HGint32 y = F_MAKE_COMPARABLE(b);

    return (int)(x < y);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a >= b
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return      a >= b -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_CMPGE (HGifloat a, HGifloat b)          
{ 
    HGint32 x = F_MAKE_COMPARABLE(a);
    HGint32 y = F_MAKE_COMPARABLE(b);

    return (int)(x >= y);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a > b
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return      a > b -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_CMPGT (HGifloat a, HGifloat b)          
{ 
    HGint32 x = F_MAKE_COMPARABLE(a);
    HGint32 y = F_MAKE_COMPARABLE(b);

    return (int)(x > y);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a equals b
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return      a == b -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_CMPEQ (
    HGifloat a, 
    HGifloat b)             
{ 
    a ^= b;
    if (a == 0x80000000u)       
        a = b & 0x7fffffff;         
    return (int)(a == 0);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a is not equal to b
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return      a != b -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_CMPNE (
    HGifloat a, 
    HGifloat b)             
{ 
    a ^= b;
    if (a == 0x80000000u)       
        a = b & 0x7fffffff;         
    return (int)(a != 0);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Check if value is a valid floating-point (NaNs are not
 *              but infinities are)
 * \param       a       Floating point value
 * \return      1 if valid, 0 otherwise
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_ISVALID (HGifloat a) /*@modifies@*/
{ 
/*      if (a == 0xffc00000u) 
        return HG_TRUE;
    */
    return (int)((a & 0x7fffffffu) <= F_FLOAT_INF);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Check if value is a finite floating-point value
 * \param       a       Floating point value
 * \return      1 if finite, 0 otherwise
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_FINITE (HGifloat a) /*@modifies@*/
{ 
    return (int)((a & 0x7fffffffu) < F_FLOAT_INF);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns absolute value of HGifloat
 * \param       a       Floating point value
 * \return      Absolute value of a
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_ABS (HGifloat a)
{
    return a &~ F_SIGN_MASK;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns negative absolute value of HGifloat
 * \param       a       Floating point value
 * \return      Negative absolute value of a
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_NABS (HGifloat a)
{
    return a | F_SIGN_MASK;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Negates a HGifloat
 * \param       a       Floating point value
 * \return      -a
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_NEG (HGifloat a)
{
    return a ^ F_SIGN_MASK;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a is equal to zero
 * \param       a       Floating point value
 * \return      a == 0 -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_ZCMPEQ  (HGifloat a)                    
{ 
    return (int)((a<<1) == 0);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a is not equal to zero
 * \param       a       Floating point value
 * \return      a != 0 -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_ZCMPNE  (HGifloat a)                    
{ 
    return (int)((a<<1) != 0);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Compares floating-point value to zero
 * \param       a       Floating point value
 * \return      a < 0 -> -1, a == 0 -> 0, a > 0 -> 1 (integer value)
 * \note    See also the function F_SIGNUM() if you want floating point
 *              results.
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_ZCMP (HGifloat a)
{
    if ((a &~ F_SIGN_MASK) == F_ZERO)       /* 0.0f and -0.0f */
        return 0;
    return 1 - ((a >> 30) & 2);             /* either -1 or +1 depending on sign */
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a <= 0
 * \param       a       Floating point value
 * \return      a <= 0 -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_ZCMPLE  (HGifloat a)                    
{ 
    return (int)((HGint32)(a) <= 0);            /* negative or zero, handles -0.0f */
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a < 0
 * \param       a       Floating point value
 * \return      a > 0 -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_ZCMPLT  (HGifloat a)                    
{ 
    return (int)((HGuint32)(a) > 0x80000000u);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a >= 0
 * \param       a       Floating point value
 * \return      a > 0 -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_ZCMPGE  (HGifloat a)                    
{ 
    return (int)((HGuint32)(a) <= 0x80000000u);
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns 'true' if a > 0
 * \param       a       Floating point value
 * \return      a > 0 -> 1, else 0
 *//*-------------------------------------------------------------------*/

HG_INLINE int F_ZCMPGT (HGifloat a)                     
{ 
    return (int)((HGint32)(a) > 0);         /* handles +-0.0f correctly */
}


/*-------------------------------------------------------------------*//*!
 * \brief       Floating-point subtraction
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return  a-b
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_SUB (
    HGifloat a, 
    HGifloat b)             
{ 
    return F_ADD(a,F_NEG(b));                   
}

/*-------------------------------------------------------------------*//*!
 * \brief       Minimum operator
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return  min(a,b)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_MIN (
    HGifloat a, 
    HGifloat b)             
{
    return (F_MAKE_COMPARABLE(a) < F_MAKE_COMPARABLE(b)) ? a : b;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns value that has smaller absolute value
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return  fabs(a) < fabs(b) ? a : b
 * \note    Does not return the _absolute value_ itself!!!
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_ABSMIN (
    HGifloat a, 
    HGifloat b)             
{ 
    return ((a << 1) < (b << 1)) ? a : b;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns value that has larger absolute value
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return  fabs(a) > fabs(b) ? a : b
 * \note    Does not return the _absolute value_ itself!!!
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_ABSMAX (
    HGifloat a, 
    HGifloat b)             
{ 
    return ((a << 1) > (b << 1)) ? a : b;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Maximum operator
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \return  max(a,b)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_MAX (
    HGifloat a, 
    HGifloat b)             
{ 
    return (F_MAKE_COMPARABLE(a) > F_MAKE_COMPARABLE(b)) ? a : b;
}

/*-------------------------------------------------------------------*//*!
 * \brief       Multiply-and-subtract
 * \param       a       Floating point value
 * \param       b       Floating point value
 * \param       c       Floating point value
 * \return  a * b - c
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_MSUB       (
    HGifloat a, 
    HGifloat b, 
    HGifloat c) 
{ 
    return F_MADD(a,b,F_NEG(c));            
}


/*-------------------------------------------------------------------*//*!
 * \brief       Divides a floating point value by 2.0f
 * \param       a       Floating point value
 * \return  a/2.0f
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_DIVBY2 (HGifloat a)
{
    return (a << 1) < (2 << 24) ?
        F_ZERO :                        /* result would underflow       */
        a - (1 << F_EXPONENT_SHIFT);    /* apply new exponent       */
}

/*-------------------------------------------------------------------*//*!
 * \brief   Computes the average of two values
 * \param   a   Floating point value
 * \param   b   Floating point value
 * \return  0.5f * a + 0.5f * b
 * \note    The operation does not overflow even if (a+b) > FLT_MAX.
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_AVG (
    HGifloat a, 
    HGifloat b)
{
    return F_ADD(F_DIVBY2(a), F_DIVBY2(b)); 
}

/*-------------------------------------------------------------------*//*!
 * \brief   Multiplies a floating point value by 2.0f
 * \param   a   Floating point value
 * \return  a*2.0f
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_MULBY2 (HGifloat a)
{
    HGint32     exponent = (HGint32)((a+a)>>24);

    if (exponent)                               /* if non-zero originally       */
    {
        a += (1 << F_EXPONENT_SHIFT);
        if (exponent >= 254)                    /* overflow?                */
            a = (a | 0x7fffffffu) &~ (1<<23);   /* generate FLT_MAX + sign      */
    }

    return a;
}
/*-------------------------------------------------------------------*//*!
 * \brief       Returns F_MAX(a, 0.0f)
 * \param       a       Floating point value
 * \return  max(a,0.0f)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_MAXZERO (HGifloat a)
{
    return (HGifloat)hgMaxZero32((HGint32)(a));
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns F_MIN(a, 0.0f)
 * \param       a       Floating point value
 * \return  min(a,0.0f)
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_MINZERO (HGifloat a)
{
    return (HGifloat)( a & ((HGint32)(a) >> 31));
}

/*-------------------------------------------------------------------*//*!
 * \brief       Returns the signum of a
 * \param       a       Floating point value
 * \return  -1.0f if a < 0, 0.0f if a == 0.0f, +1.0f if a > 0.0f
 *//*-------------------------------------------------------------------*/

HG_INLINE HGifloat F_SIGNUM (HGifloat a)
{
    HGifloat res = F_ONE | ((HGuint32)(a) & F_SIGN_MASK);

    if (F_ZCMPEQ(a))
        res = F_ZERO;

    return res;
}
/*@=shiftimplementation@*/

/*----------------------------------------------------------------------*/
#endif /* HGFLOAT_INL */

