/*
 * Copyright (c) 2007-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * nvfxmath.h
 *
 * The primary intent of this library is to provide efficient
 * implementations of commonly used S15.16 fixed point math routines.
 * One consequence of this design choice is that macros are used in
 * some places to avoid function call overhead.  This forces the
 * library to be delivered as a header file and a static library.
 */

#if !defined(NVFXMATH_H)
#define NVFXMATH_H

#include <stdlib.h>  /* for labs() prototype */

#ifdef __cplusplus
extern "C" {
#endif

/* no support for 'long long' until MS VC7 */
#if defined(_WIN32) && defined(_WIN32) && (_MSC_VER < 1300)
#define NV_FX_LONGLONG __int64
#else
#define NV_FX_LONGLONG long long
#endif

typedef int NvSFx;


/* Constants */
#define NV_SFX_ZERO               ((NvSFx)0x00000000)
#define NV_SFX_ONE                ((NvSFx)0x00010000)
#define NV_SFX_TWO                ((NvSFx)0x00020000)
#define NV_SFX_THREE              ((NvSFx)0x00030000)
#define NV_SFX_EIGHT              ((NvSFx)0x00080000)
#define NV_SFX_TEN                ((NvSFx)0x000A0000)
#define NV_SFX_FIFTEEN            ((NvSFx)0x000F0000)
#define NV_SFX_7_POINT_9375       ((NvSFx)0x0007F000)
#define NV_SFX_HALF               ((NvSFx)0x00008000)
#define NV_SFX_MINUS_ONE          ((NvSFx)0xFFFF0000)
#define NV_SFX_MINUS_TWO          ((NvSFx)0xFFFE0000)
#define NV_SFX_MINUS_THREE        ((NvSFx)0xFFFD0000)
#define NV_SFX_ONE_HUNDRED_EIGHTY ((NvSFx)0x00B40000)
#define NV_SFX_POINT_TWO          ((NvSFx)0x00003333)
#define NV_SFX_POINT_EIGHT        ((NvSFx)0x0000CCCD)
#define NV_SFX_1000               ((NvSFx)0x03E80000)
#define NV_SFX_MINUS_1000         ((NvSFx)0xFC180000)
#define NV_SFX_128                ((NvSFx)0x00800000)
#define NV_SFX_NINETY             ((NvSFx)0x005A0000)
#define NV_SFX_POINT_125          ((NvSFx)0x00002000)
#define NV_SFX_63                 ((NvSFx)0x003F0000)
#define NV_SFX_63_POINT_375       ((NvSFx)0x003F6000)
#define NV_SFX_HIGH               ((NvSFx)0x7FFFFFFF)
#define NV_SFX_LOW                ((NvSFx)0x80000000)
#define NV_SFX_COSMOLOGICAL       ((NvSFx)0x0000002A)
#define NV_SFX_L2E                ((NvSFx)0x00017154)
#define NV_SFX_LN2                ((NvSFx)0x0000B172)
#define NV_SFX_255                ((NvSFx)0x00FF0000)

/* Operations */

/*
 * NvSFxAbs returns the absolute value of its parameter.  This is currently
 * implemented as a call to labs(), which the compiler will turn into
 * efficient asm.
 */
#define NV_SFX_ABS(A)   (labs((NvSFx)(A)))

/*
 * NvSFxFloor returns the floor of its parameter.
 */
#define NV_SFX_FLOOR(A) ((((NvSFx)(A)) >> 16) << 16)

/*
 * NvSFxCeil returns the ceiling of its parameter.  The result is
 * undefined if input > 7FFF.0000
 */
#define NV_SFX_CEIL(A)  (-NV_SFX_FLOOR(-((NvSFx)(A))))

/*
 * NvSFxAdd returns the sum of its parameters.  The result is undefined
 * if sum > 7FFF.FFFF or sum < 8000.0000
 */
#define NV_SFX_ADD(A,B) (((NvSFx)(A)) + ((NvSFx)(B)))

/*
 * NvSFxSub returns the difference of its parameters.  The result is
 * undefined if difference > 7FFF.FFFF or difference < 8000.0000
 */
#define NV_SFX_SUB(A,B) (((NvSFx)(A)) - ((NvSFx)(B)))

/*
 * NvSFxMul returns the product of its parameters.  The result is
 * undefined if product > 7FFF.FFFF or product < 8000.0000
 */
#define NV_SFX_MUL(A,B) \
    ((NvSFx)((((NV_FX_LONGLONG)(NvSFx)(A)) * ((NvSFx)(B))) >> 16))

/*
 * NvSFxDot4, NvSFxDot3, and NvSFxDot2 implement four element, three element,
 * and two element dot products, respectively. These macros provide additional
 * accuracy by accumulating the double wide results and doing rounded conversion
 * back to S15.16 format only at the end.
 */
#define NV_SFX_DOT4(a0,b0,a1,b1,a2,b2,a3,b3)               \
    (((((((NV_FX_LONGLONG)(a0)) * (b0))   +                \
        (((NV_FX_LONGLONG)(a1)) * (b1)))  +                \
       ((((NV_FX_LONGLONG)(a2)) * (b2))   +                \
        (((NV_FX_LONGLONG)(a3)) * (b3)))) + 0x8000) >> 16)

#ifndef SYNERGY
#define NV_SFX_DOT3(a0,b0,a1,b1,a2,b2)                     \
    ((((((NV_FX_LONGLONG)(a0)) * (b0))  +                  \
       (((NV_FX_LONGLONG)(a1)) * (b1))  +                  \
       (((NV_FX_LONGLONG)(a2)) * (b2))) + 0x8000) >> 16)
#else
// HACKHACKHACK look at: synergy_path.c
#define NV_SFX_DOT3(a0,b0,a1,b1,a2,b2) (NvSFxDot3(a0,b0,a1,b1,a2,b2))
NvSFx NvSFxDot3(int a0, int b0, int a1, int b1, int a2, int b2);
#endif

#define NV_SFX_DOT2(a0,b0,a1,b1)                           \
    ((((((NV_FX_LONGLONG)(a0)) * (b0))  +                  \
       (((NV_FX_LONGLONG)(a1)) * (b1))) + 0x8000) >> 16)

#define NV_GL_X_DOT3_SQR(a0,a1,a2) \
    (NV_SFX_DOT3(a0,a0,a1,a1,a2,a2))

#define NV_SFX_NEG(a)             (-(a))

/* Macros of other operations that need function calls */
#define NV_SFX_SQR(a)             (NV_SFX_MUL((a),(a)))
#define NV_SFX_DIV(a,b)           (NvSFxDiv((a),(b)))
#define NV_SFX_RECIP(a)           (NvSFxRecip(a))
#define NV_SFX_SQRT(a)            (NvSFxSqrt(a))
#define NV_SFX_INV_SQRT(a)        (NvSFxRecipSqrt(a))
#define NV_SFX_SIND(a)            (NvSFxSinD(a))
#define NV_SFX_COSD(a)            (NvSFxCosD(a))
#define NV_SFX_SIND_FULL_RANGE(a) (NvSFxSinDFullRange(a))
#define NV_SFX_COSD_FULL_RANGE(a) (NvSFxCosDFullRange(a))
#define NV_SFX_POW(a,b)           (NvSFxPow((a),(b)))
#define NV_SFX_EXP(a)             (NvSFxPow2(NV_SFX_MUL(NV_SFX_L2E,a))) 
#define NV_SFX_LOG(a)             (NV_SFX_MUL(NV_SFX_LN2,NvSFxLog2(a)))
#define NV_SFX_MOD(a,b)           (NvSFxMod((a),(b))
#define NV_SFX_POW2(a)            (NvSFxPow2(a))
#define NV_SFX_LOG2(a)            (NvSFxLog2(a))
#define NV_SFX_ASIND(a)           (NvSFxAsinD(a))
#define NV_SFX_ACOSD(a)           (NvSFxAcosD(a))
#define NV_SFX_ATAN2D(a,b)        (NvSFxAtan2D((a),(b)))
#define NV_SFX_ATAND(a)           (NvSFxAtan2D(a,NV_SFX_ONE))
#define NV_SFX_NORM3(iv,ov)       (NvSFxNormalize3((iv),(ov)))

// Special-case operations where we know something about "a" and/or "b".
// Divide of "a" by integer constant "b".
#define NV_SFX_DIV_BY_WHOLE(a,b) ((a)/(b))
// Multiply of "a" by integer constant "b". Ditto.
#define NV_SFX_MUL_BY_WHOLE(a,b) ((a)*(b))

/* Combined operations */
#define NV_SFX_MUL3(a,b,c)    NV_SFX_MUL(NV_SFX_MUL(a,b),c)
#define NV_SFX_SIGN(v)        (((v)>>31)&1)

/* Conditional tests */
#define NV_SFX_EQ_ZERO(v)     ((v) == NV_SFX_ZERO)
#define NV_SFX_NE_ZERO(v)     ((v) != NV_SFX_ZERO)
#define NV_SFX_EQ_ONE(v)      ((v) == NV_SFX_ONE)
#define NV_SFX_NE_ONE(v)      ((v) != NV_SFX_ONE)
#define NV_SFX_LT_ZERO(v)     ((v) < NV_SFX_ZERO)
#define NV_SFX_LE_ZERO(v)     ((v) <= NV_SFX_ZERO)
#define NV_SFX_GT_ZERO(v)     ((v) > NV_SFX_ZERO)
#define NV_SFX_GE_ZERO(v)     ((v) >= NV_SFX_ZERO)
#define NV_SFX_LT_ONE(v)      ((v) < NV_SFX_ONE)
#define NV_SFX_LE_ONE(v)      ((v) <= NV_SFX_ONE)
#define NV_SFX_GT_ONE(v)      ((v) > NV_SFX_ONE)
#define NV_SFX_GE_ONE(v)      ((v) >= NV_SFX_ONE)
#define NV_SFX_LT(a,b)         ((a) < (b))
#define NV_SFX_LE(a,b)         ((a) <= (b))
#define NV_SFX_EQ(a,b)         ((a) == (b))
#define NV_SFX_NE(a,b)         ((a) != (b))
#define NV_SFX_GT(a,b)         ((a) > (b))
#define NV_SFX_GE(a,b)         ((a) >= (b))

// Min / Max operation
#define NV_SFX_MIN(a, b)       (NV_SFX_LT(a,b) ? (a) : (b))
#define NV_SFX_MAX(a, b)       (NV_SFX_GT(a,b) ? (a) : (b))

// conversions
#define NV_SFX_WHOLE_TO_FX(i)       ((NvSFx)((i) << 16))
#define NV_SFX_FP_TO_FX(f)          (NvSFxFloat2Fixed(f))
#define NV_SFX_TO_FP(s)             (NvSFxFixed2Float(s))
#define NV_SFX_TO_WHOLE(s)          ((((NvSFx)(s)) + 0x8000) >> 16)
#define NV_SFX_TRUNCATE_TO_WHOLE(s) (((NvSFx)(s)) >> 16)  
// This slow conversion macro should only be used for compile-time,
// such as const initialization.
#define NV_SFX_FP_TO_FX_CT(c)       ((NvSFx)(c * 65536.0f + \
                                    ((c<0.0f) ? -0.5f : 0.5f)))


/* Operations implemented as functions */

/* 
 * NvSFxPow approximates A^B (where "^" denotes "raised to the power of"). 
 * In general, the result is undefined if A is not a non-zero, non-negative 
 * number. I.e. A must be > 0000.0000. However pow(0,0) is 0001.0000 as 
 * demanded by OpenGL-ES. The result is undefined if pow(A,B) > 7FFF.FFFF.
 * The result is also undefined if log2(A)*B is not in range [8000.0000, 
 * 7FFF.FFFF].
 *
 * While this pow() function works well in many situations (e.g. for
 * lighting), there are numerical shortcomings when it is used as a general
 * purpose pow() function, since significantly reduced accuracy can occur
 * for certain pairs of arguments. Errors up to about 0.5% have been
 * observed. The basic issue when using pow(x,y) = exp(y*log(x)) is that
 * a small error in the logarithm can lead to a massive error in the
 * function value. This effect is more pronounced with a fixed point
 * implementation than a floating-point one.  The worst errors occur when
 * A is a number slightly less than 1, and B is a negative number of
 * fairly large magnitude such that the result is large but does not
 * overflow.
 */
NvSFx NvSFxPow (NvSFx A, NvSFx B);

/* 
 * NvSFxDiv returns the quotient of its arguments.  Division is exact
 * and the result is equal to the quotient of dividend and divisor
 * rounded towards 0, i.e. truncated after 16 fraction bits.  The
 * quotient is undefined if abs(dividend) > (7FFF.FFFF * abs(divisor)).
 * The divisor must satisfy abs(divisor) <= 7FFF.FFFF.
 */
NvSFx NvSFxDiv (NvSFx dividend, NvSFx divisor);

/* 
 * NvSFxRecip returns the reciprocal of its argument.  The reciprocal
 * is exact and the result is equal to the quotient of 1 / divisor 
 * rounded towards 0, i.e. truncated after 16 fraction bits.  The
 * quotient is undefined if abs(dividend) > (7FFF.FFFF * abs(divisor)).
 */
NvSFx NvSFxRecip (NvSFx divisor);

/*
 * NvSFxMod is the fixed-point equivalent of the ANSI-C function fmod(). It 
 * computes dividend - trunc(dividend / divisor) * divisor, where trunc(x) = 
 * (x >= 0.0) ? floor(x) : ceil(x)). The remainder has the same sign as the
 * dividend. The result is undefined if 
 * abs(dividend) > (7FFF.FFFF * abs(divisor)).
 */
NvSFx NvSFxMod(NvSFx dividend, NvSFx divisor);

/* 
 * NvSFxCosD computes an approximation to the cosine of deg, where deg is an
 * angle given in degrees (a full circle having 360 degrees). The input must
 * be >= 0 and less than 360 degrees, i.e. 0000.0000 <= deg <= 0167.FFFF. The
 * function returns 0 for arguments outside this range. The magnitude of the
 * approximation error does not exceed 4/65536 over the entire supported 
 * argument range.
 */ 
NvSFx NvSFxCosD(NvSFx deg);

/* 
 * NvSFxSinD computes an approximation to the sine of deg, where deg is an
 * angle given in degrees (a full circle having 360 degrees). The input must
 * be >= 0 and less than 360 degrees, i.e. 0000.0000 <= deg <= 0167.FFFF. The
 * function returns 0 for arguments outside this range. The magnitude of the
 * approximation error does not exceed 4/65536 over the entire supported 
 * argument range.
 */
NvSFx NvSFxSinD(NvSFx deg);

/* 
 * NvSFxCosDFullRange computes an approximation to the cosine of deg,
 * where deg is an angle given in degrees (a full circle having 360
 * degrees).  The input is allowed to be any fixed point number.  The
 * magnitude of the approximation error does not exceed 4/65536 over
 * the entire supported argument range.
 *
 * This function is slower than NvSFxCosD because it first transforms
 * its parameter to be >= 0 and less than 360 degrees.  As such, this
 * function is provided for convenience and is not recommended for
 * performance sensitive applications.
 */ 
NvSFx NvSFxCosDFullRange (NvSFx deg);

/* 
 * NvSFxSinDFullRange computes an approximation to the sine of deg,
 * where deg is an angle given in degrees (a full circle having 360
 * degrees).  The input is allowed to be any fixed point number.  The
 * magnitude of the approximation error does not exceed 4/65536 over
 * the entire supported argument range.
 *
 * This function is slower than NvSFxSinD because it first transforms
 * its parameter to be >= 0 and less than 360 degrees.  As such, this
 * function is provided for convenience and is not recommended for
 * performance sensitive applications.
 */ 
NvSFx NvSFxSinDFullRange (NvSFx deg);

/*
 * NvSFxSqrt returns the square root of its parameter.  The square root
 * is exact and the result is equal to sqrt (radicand) rounded towards 0,
 * i.e. truncated after 16 fraction bits.  The result is undefined if the
 * radicand does not satisfy 0000.0000 <= radicand < 7FFF.FFFF, i.e. the
 * radicand must be greater than or equal to zero.
 */
NvSFx NvSFxSqrt(NvSFx radicand);

/*
 * NvSFxRecipSqrt computes an approximation to 1/sqrt(radicand). The result is
 * undefined if radicand does not satisfy 0000.0000 < radicand < 7FFF.FFF, i.e
 * the argument must be a non-zero non-negative number. Over the supported
 * argument range all results either match the reference function, or are off
 * by at most +/-1/65536. No larger errors were observed in an exhaustive test.
 */
NvSFx NvSFxRecipSqrt(NvSFx radicand);

/*
 * NvSFxLog2 computes an approximation to the logarithm base 2 of x. The result
 * is undefined if the argument x does not satisfy 0000.0000 < x < 7FFF.FFFF,
 * i.e. argument must be a non-zero non-negative number. Over the supported
 * argument range all results either match the reference function, or are off
 * by at most +/-1/65536. No larger errors were observed in an exhaustive test.
 */
NvSFx NvSFxLog2(NvSFx x);

/*
 * NvSFxPow2 computes an approximation to 2^x. The result is undefined if the 
 * argument x is > 000E.FFFF (about 14.99998). The approximation provides 
 * about 23.5 bits of accuracy. The result is not off by more than +/-2/65536
 * (as compared to a floating-point reference converted to S15.16) if the 
 * argument is less than 0008.2000. The magnitude of the approximation error 
 * increases for larger arguments, but does not exceed 176/65536 over the 
 * entire supported argument range.
 */
NvSFx NvSFxPow2(NvSFx x);

/*
 * NvSFxFloat2Fixed converts the floating-point number f into a S15.16 fixed
 * point number. The result is identical to ((int)(f * 65536.0f + 0.5f)) for
 * f >= 0, and ((int)(f * 65536.0f - 0.5f)) for f < 0. Using this function 
 * avoids floating-point computation which is slow without FPU. The result is 
 * undefined if f does not satisfy -32768.0 < f < 32767.99998.
 */
NvSFx NvSFxFloat2Fixed(float f);

/*
 * NvSFxFixed2Float converts the fixed-point number x into IEEE-754 single
 * precision floating- point number. The result is identical to x/65536.0f.
 * Using this function avoids floating-point computation which is slow 
 * without an FPU.
 */
float NvSFxFixed2Float(NvSFx x);

/*
 * NvSFxAtan2D computes an approximation to the principal value of arctangent 
 * of y/x, using the signs of both arguments to determine the quadrant of the
 * return value. The value of the arctangent returned is in degrees (where a
 * full circle comprises 360 degrees) and is in [-180, 180]. Zero is returned 
 * if both arguments are zero. The magnitude of the error should not exceed 
 * 2/65536 (determined by comparing a large number of results to a floating-
 * point reference).
 */
NvSFx NvSFxAtan2D (NvSFx y, NvSFx x);

/*
 * NvSFxAsinD computes an approximation to the principal value of arcsine of 
 * x. The value of the arc sine returned is in degrees (where a full circle 
 * comprises 360 degrees) and is in [-90, 90]. The result is undefined if the 
 * input does not satisfy -1 <= x <= 1. The magnitude of the approximation 
 * error does not exceed 3/65536 over the entire legal argument range.
 */
NvSFx NvSFxAsinD (NvSFx x);

/*
 * NvSFxAcosD computes an approximation to the principal value of arcsine of 
 * x. The value of the arc cosine returned is in degrees (where a full circle 
 * comprises 360 degrees) and is in [0, 180]. The result is undefined if the 
 * input does not satisfy -1 <= x <= 1. The magnitude of the approximation 
 * error does not exceed 3/65536 over the entire legal argument range.
 */
NvSFx NvSFxAcosD (NvSFx x);

/*
 * NvSFxNormalize3 normalizes a 3-element input vector, i.e. produces a unit
 * length vector. There are no overflows in intermediate computation for all
 * S15.16 inputs. Each of the output elements has an error of at most one ulp
 * compared to a reference function that performs intermediate computations
 * in double precision floating-point. Zero length vectors are returned as is.
 */
void NvSFxNormalize3 (NvSFx const *iv, NvSFx *ov);

#ifdef __cplusplus
}
#endif

#endif /* NVFXMATH_H */
