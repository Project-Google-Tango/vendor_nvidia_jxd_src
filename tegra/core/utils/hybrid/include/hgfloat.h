#ifndef __HGFLOAT_H
#define __HGFLOAT_H
/*------------------------------------------------------------------------
 *
 * Floating-point emulation package
 * --------------------------------
 *
 * (C) 2004-2005 Hybrid Graphics, Ltd.
 * All Rights Reserved.
 *
 * This file consists of unpublished, proprietary source code of
 * Hybrid Graphics, and is considered Confidential Information for
 * purposes of non-disclosure agreement. Disclosure outside the terms
 * outlined in signed agreement may result in irreparable harm to
 * Hybrid Graphics and legal action against the party in breach.
 *
 * $Id: //hybrid/libs/hg/main/hgFloat.h#6 $
 * $Date: 2007/01/22 $
 * $Author: jari $
 *
 *//**
 * \file
 * \brief       HG emulated floating-point math
 * \author      wili@hybrid.fi
 * \todo    Implement sin, cos, tan, acos with Tero method
 * \todo    byte->float conversion?
 * \todo    added hgFsignum, hgFdot4hi
 * \todo    degrees2radians, radians2degrees?
 * \todo
 *//*-------------------------------------------------------------------*/

#ifndef __HGDEFS_H
#       include "hgdefs.h"
#endif


#if (HG_COMPILER == HG_COMPILER_INTELC)
#       pragma warning (push)
#       pragma warning (disable:981)
#endif

/*----------------------------------------------------------------------*
 * Determine whether we use native floats or emulated ones. You can
 * always force emulation by defining HG_NO_NATIVE_FLOATS.
 *----------------------------------------------------------------------*/

#if !defined (HG_NATIVE_FLOATS)
#       if (HG_CPU == HG_CPU_X86)
#           define HG_NATIVE_FLOATS
#       elif (HG_CPU == HG_CPU_PPC)
#           define HG_NATIVE_FLOATS
#       elif (HG_CPU == HG_CPU_X86_64)
#           define HG_NATIVE_FLOATS
#       endif
#endif

#if defined (HG_NO_NATIVE_FLOATS)
#       undef HG_NATIVE_FLOATS
#endif

/*-------------------------------------------------------------------*//*!
 * \brief       Return bit pattern of a floating-point value as a 32-bit int
 * \param       f       Floating-point value
 * \return      32-bit unsigned integer
 *//*-------------------------------------------------------------------*/

HG_INLINE HGuint32 hgFloatBitPattern (HGfloat f)
{
    HGfloatInt fi;
    fi.f = f;
    return fi.i;

/*      return *((const HGuint32*)&f);*/
}

/*-------------------------------------------------------------------*//*!
 * \brief       Return floating-point value containing specified bit pattern
 * \param       x       32-bit unsigned int containing the bit pattern
 * \return      Corresponding floating-point value
 *//*-------------------------------------------------------------------*/

HG_INLINE float hgBitPatternToFloat (HGuint32 x)
{
    HGfloatInt fi;
    fi.i = x;
    return fi.f;
}

#include "hgfloat.inl"

#if !defined (HG_NATIVE_FLOATS)

/*-------------------------------------------------------------------------
 * FPU setup
 *-----------------------------------------------------------------------*/

HG_INLINE HGuint32  hgFsetfpumode(void)                                     {       return 0;       }
HG_INLINE void      hgFrestorefpumode(HGuint32 mode)                        {       HG_UNREF(mode); }

/*-------------------------------------------------------------------------
 * Float consistency checking
 *-----------------------------------------------------------------------*/

HG_INLINE HGbool    hgFisvalid      (float A)                               { return F_ISVALID(FIN(A));             }
HG_INLINE HGbool    hgFfinite       (float A)                               { return F_FINITE(FIN(A));              }
HG_INLINE float     hgFclean        (float A)                               { return FOUT(F_CLEAN(FIN(A)));         }
HG_INLINE float     hgFcleanfinite  (float A)                               { return FOUT(F_CLEANFINITE(FIN(A)));   }

/*-------------------------------------------------------------------------
 * Negation and absolute value
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFabs          (float A)                               { return FOUT(F_ABS(FIN(A)));   }
HG_INLINE float     hgFnabs         (float A)                               { return FOUT(F_NABS(FIN(A)));  }
HG_INLINE float     hgFneg          (float A)                               { return FOUT(F_NEG(FIN(A)));   }

/*-------------------------------------------------------------------------
 * Rounding operations + frac
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFfloor        (float A)                               { return FOUT(F_FLOOR(FIN(A))); }
HG_INLINE float     hgFceil         (float A)                               { return FOUT(F_NEG(F_FLOOR(F_NEG((FIN(A)))))); }
HG_INLINE float     hgFfrac         (float A)                               { return FOUT(F_FRAC(FIN(A)));          }

/*-------------------------------------------------------------------------
 * Conversion from (un)signed integers and fixed-point values into floats
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgItof          (HGint32 A)                             { return FOUT(F_ITOF(A));               }
HG_INLINE float     hgItofscale     (HGint32 A, HGint32 shift)              { return FOUT(F_ITOFSCALE(A, shift));   }
HG_INLINE float     hgI64tofscale   (HGint64 A, HGint32 shift)              { return FOUT(F_I64TOFSCALE(A, shift)); }
HG_INLINE float     hgUitof         (HGuint32 A)                            { return FOUT(F_UTOF(A));               }
HG_INLINE float     hgUtofscale     (HGuint32 A, HGint32 shift)             { return FOUT(F_UTOFSCALE(A, shift));   }

/*-------------------------------------------------------------------------
 * Conversions from floats into other data types
 *-----------------------------------------------------------------------*/

HG_INLINE HGint32       hgIchopscale    (float A, HGint32 shift)            { return F_ICHOPSCALE(FIN(A), shift);   }
HG_INLINE HGint64       hgI64chopscale  (float A, HGint32 shift)            { return F_I64CHOPSCALE(FIN(A), shift); }
HG_INLINE HGint32       hgIchop         (float A)                           { return F_ICHOP(FIN(A));               }
HG_INLINE HGint32       hgIceil         (float A)                           { return F_ICEIL(FIN(A));               }
HG_INLINE HGint32       hgIfloor        (float A)                           { return F_IFLOOR(FIN(A));              }
HG_INLINE HGint32       hgF2ubyte       (float A)                           { return F_F2UBYTE(FIN(A));             }

/*-------------------------------------------------------------------------
 * Addition and subtraction (two, three and four component ops)
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFadd          (float A, float B)                      { return FOUT(F_ADD(FIN(A),FIN(B))); }
HG_INLINE float     hgFsub          (float A, float B)                      { return FOUT(F_SUB(FIN(A),FIN(B))); }
HG_INLINE float     hgFadd3         (float A, float B, float C)             { return FOUT(F_ADD3(FIN(A),FIN(B),FIN(C)));    }
HG_INLINE float     hgFaddabs3      (float A, float B, float C)             { return FOUT(F_ADDABS3(FIN(A),FIN(B),FIN(C))); }
HG_INLINE float     hgFadd4         (float A, float B, float C, float D)    { return FOUT(F_ADD4(FIN(A),FIN(B),FIN(C),FIN(D)));     }
HG_INLINE float     hgFaddabs4      (float A, float B, float C, float D)    { return FOUT(F_ADDABS4(FIN(A),FIN(B),FIN(C),FIN(D)));  }

/*-------------------------------------------------------------------------
 * Multiplication, division, modulo + optimized variants
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFmul          (float A, float B)                      { return FOUT(F_MUL(FIN(A),FIN(B)));    }
HG_INLINE float     hgFimul         (float A, HGint32 B)                    { return FOUT(F_IMUL(FIN(A),B));        }
HG_INLINE float     hgFsqr          (float A)                               { return FOUT(F_SQR(FIN(A)));           }
HG_INLINE float     hgFrcp          (float A)                               { return FOUT(F_RCP(FIN(A)));           }
HG_INLINE float     hgFdiv          (float A, float B)                      { return FOUT(F_DIV(FIN(A),FIN(B)));    }
HG_INLINE float     hgFmod          (float A, float B)                      { return FOUT(F_MOD(FIN(A),FIN(B)));    }
HG_INLINE float     hgFshift        (float A, HGint32 B)                    { return FOUT(F_SHIFT(FIN(A),(B)));     }
HG_INLINE float     hgFtwice        (float A)                               { return FOUT(F_MULBY2(FIN(A)));        }
HG_INLINE float     hgFhalf         (float A)                               { return FOUT(F_DIVBY2(FIN(A)));        }

/*-------------------------------------------------------------------------
 * Logarithm and exponent (base 2)
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFlog2         (float A)                               { return FOUT(F_LOG2(FIN(A)));          }
HG_INLINE float     hgFexp2         (float A)                               { return FOUT(F_EXP2(FIN(A)));          }

/*-------------------------------------------------------------------------
 * Combos of multiplications and additions (dot products, determinants
 * etc.). The ones marked with 'hi' are higher-precision variants.
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFmadd         (float A, float B, float C)                 { return FOUT(F_MADD(FIN(A),FIN(B),FIN(C)));                    }
HG_INLINE float     hgFmsub         (float A, float B, float C)                 { return FOUT(F_MSUB(FIN(A),FIN(B),FIN(C)));                    }
HG_INLINE float     hgFdot2         (float A, float B, float C, float D)        { return FOUT(F_DOT2(FIN(A),FIN(B),FIN(C),FIN(D)));                 }
HG_INLINE float     hgFdot3         (float A, float B, float C, float D, float E, float F) { return hgFadd3(hgFmul(A,B), hgFmul(C,D), hgFmul(E,F)); }
HG_INLINE float     hgFdot4         (float A, float B, float C, float D, float E, float F, float G, float H) {  return FOUT(F_DOT4(FIN(A),FIN(B),FIN(C),FIN(D),FIN(E),FIN(F),FIN(G),FIN(H))); }
HG_INLINE float     hgFdot4hi       (float A, float B, float C, float D, float E, float F, float G, float H) {  return FOUT(F_DOT4HI(FIN(A),FIN(B),FIN(C),FIN(D),FIN(E),FIN(F),FIN(G),FIN(H))); }
HG_INLINE float     hgFdot3add      (float A, float B, float C, float D, float E, float F, float G) { return hgFdot4(A,B,C,D,E,F,G,1.0f); }
HG_INLINE float     hgFsqrlen3      (float A, float B, float C)                 { return hgFaddabs3(hgFsqr(A), hgFsqr(B), hgFsqr(C));           }
HG_INLINE float     hgFsqrlen4      (float A, float B, float C, float D)    { return hgFaddabs4(hgFsqr(A), hgFsqr(B), hgFsqr(C), hgFsqr(D));}
HG_INLINE float     hgFdet2         (float A, float B, float C, float D)    { return hgFmsub(A,D, hgFmul(B,C));                                 }

/*-------------------------------------------------------------------------
 * Square root, reciprocal square root and power function.
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFsqrt     (float A)                                   { return FOUT(F_SQRT(FIN(A)));          }
HG_INLINE float     hgFrsq      (float A)                                   { return FOUT(F_RSQ(FIN(A)));           }
HG_INLINE float     hgFpow      (float A, float B)                          { return FOUT(F_POW(FIN(A),FIN(B)));    }

/*-------------------------------------------------------------------------
 * Minimum/maximum ops + clamping
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFmin      (float A, float B)                          { return FOUT(F_MIN(FIN(A),FIN(B)));        }
HG_INLINE float     hgFmax      (float A, float B)                          { return FOUT(F_MAX(FIN(A),FIN(B)));        }
HG_INLINE float     hgFabsmin   (float A, float B)                          { return FOUT(F_ABSMIN(FIN(A),FIN(B)));     }
HG_INLINE float     hgFabsmax   (float A, float B)                          { return FOUT(F_ABSMAX(FIN(A),FIN(B)));     }
HG_INLINE float     hgFminZero  (float A)                                   { return FOUT(F_MINZERO(FIN(A)));           }
HG_INLINE float     hgFmaxZero  (float A)                                   { return FOUT(F_MAXZERO(FIN(A)));           }
HG_INLINE float     hgFclamp    (float A, float B, float C)                 { return FOUT(F_CLAMP(FIN(A),FIN(B),FIN(C))); }

/*-------------------------------------------------------------------------
 * Comparisons between two floats
 *-----------------------------------------------------------------------*/

HG_INLINE HGint32   hgFcmp      (float A, float B)                          { return F_CMP(FIN(A),FIN(B));      }
HG_INLINE HGbool    hgFcmpeq    (float A, float B)                          { return F_CMPEQ(FIN(A),FIN(B));    }
HG_INLINE HGbool    hgFcmpne    (float A, float B)                          { return F_CMPNE(FIN(A),FIN(B));    }
HG_INLINE HGbool    hgFcmple    (float A, float B)                          { return F_CMPLE(FIN(A),FIN(B));    }
HG_INLINE HGbool    hgFcmplt    (float A, float B)                          { return F_CMPLT(FIN(A),FIN(B));    }
HG_INLINE HGbool    hgFcmpge    (float A, float B)                          { return F_CMPGE(FIN(A),FIN(B));    }
HG_INLINE HGbool    hgFcmpgt    (float A, float B)                          { return F_CMPGT(FIN(A),FIN(B));    }

/*-------------------------------------------------------------------------
 * Comparisons against zero
 *-----------------------------------------------------------------------*/

HG_INLINE HGint32   hgFzcmp         (float A)                                   { return F_ZCMP(FIN(A));        }
HG_INLINE HGbool    hgFzcmpeq       (float A)                                   { return F_ZCMPEQ(FIN(A));      }
HG_INLINE HGbool    hgFzcmpne       (float A)                                   { return F_ZCMPNE(FIN(A));      }
HG_INLINE HGbool    hgFzcmple       (float A)                                   { return F_ZCMPLE(FIN(A));      }
HG_INLINE HGbool    hgFzcmplt       (float A)                                   { return F_ZCMPLT(FIN(A));      }
HG_INLINE HGbool    hgFzcmpge       (float A)                                   { return F_ZCMPGE(FIN(A));      }
HG_INLINE HGbool    hgFzcmpgt       (float A)                                   { return F_ZCMPGT(FIN(A));      }

/*-------------------------------------------------------------------------
 * Comparisons between a float and an integer
 *-----------------------------------------------------------------------*/

HG_INLINE HGint32   hgFicmp         (float A, HGint32 B)                    { return F_ICMP(FIN(A), B);                 }
HG_INLINE HGbool    hgFicmpeq       (float A, HGint32 B)                    { return (HGbool)(hgFicmp(A,B) == 0);       }
HG_INLINE HGbool    hgFicmpne       (float A, HGint32 B)                    { return (HGbool)(hgFicmp(A,B) != 0);       }
HG_INLINE HGbool    hgFicmple       (float A, HGint32 B)                    { return (HGbool)(hgFicmp(A,B) <= 0);       }
HG_INLINE HGbool    hgFicmplt       (float A, HGint32 B)                    { return (HGbool)(hgFicmp(A,B) <  0);       }
HG_INLINE HGbool    hgFicmpge       (float A, HGint32 B)                    { return (HGbool)(hgFicmp(A,B) >= 0);       }
HG_INLINE HGbool    hgFicmpgt       (float A, HGint32 B)                    { return (HGbool)(hgFicmp(A,B) >  0);       }

/*-------------------------------------------------------------------------
 * Trigonometric functions
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFsin      (float A)                                   { return FOUT(F_SIN(FIN(A)));  }
HG_INLINE float     hgFcos      (float A)                                   { return FOUT(F_COS(FIN(A)));  }
HG_INLINE float     hgFtan      (float A)                                   { return FOUT(F_TAN(FIN(A)));  }
HG_INLINE float     hgFacos     (float A)                                   { return FOUT(F_ACOS(FIN(A))); }

/*-------------------------------------------------------------------------
 * Syntactic sugar
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFlen3     (float A, float B, float C)             { return hgFsqrt(hgFsqrlen3(A,B,C));    }
HG_INLINE float     hgFlen4     (float A, float B, float C, float D)    { return hgFsqrt(hgFsqrlen4(A,B,C,D));  }

/*-------------------------------------------------------------------------
 * Miscellaneous functions
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFsignum   (float A)                               { return FOUT(F_SIGNUM(FIN(A)));                }
HG_INLINE float     hgFlerp     (float A, float B, float lerp)          { return hgFadd(hgFmul(hgFsub(B,A), lerp), A);  }
HG_INLINE float     hgFavg      (float A, float B)                      { return FOUT(F_AVG(FIN(A),FIN(B)));            }

/*-------------------------------------------------------------------------
 * Deprecated functions (these will disappear soon!)
 *-----------------------------------------------------------------------*/

/* deprecated -- use hgItofscale (x, -16) and hgF2fixed(x, 16) instead! */
HG_INLINE float     hgFixed2f       (HGint32 A)                                 { return FOUT(F_FIXED2F(A));            }
HG_INLINE HGint32   hgF2fixed       (float A)                                   { return F_ICHOPSCALE(FIN(A),16);       }

#else

#include <math.h>

#if (HG_OS != HG_OS_SYMBIAN)
#       include <float.h>
#endif

#if (HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_MSEC)
#       if (_MSC_VER >= 1400) && (HG_OS != HG_OS_WINCE)
#           define HG_HAVE_MSC_CONTROLFP_S
#else
#           define HG_HAVE_MSC_CONTROLFP
#endif
#elif defined(__CYGWIN__) || defined(__CYGWIN32__)
#       define HG_GCC_X86_ASM
#elif (HG_OS == HG_OS_SYMBIAN)
#   undef HG_HAVE_FENV_H
#elif (HG_OS==HG_OS_VANILLA && HG_COMPILER==HG_COMPILER_GCC)
#   undef HG_HAVE_FENV_H
#else
#       define HG_HAVE_FENV_H
#endif

#if defined(HG_HAVE_FENV_H)
#       include <fenv.h>
#endif

/* Define if <math.h> supports fabsf() and these are faster than fabs() */

#if (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_PPC)
#       define HG_HAVE_FLOAT_MATH_H_VARIANTS
#endif

/*-------------------------------------------------------------------------
 * FPU setup
 *-----------------------------------------------------------------------*/

HG_INLINE HGuint32 hgFsetfpumode(void)
{
    HGuint32 mode = 0;
#if defined(HG_HAVE_MSC_CONTROLFP_S)
    _controlfp_s(&mode, _RC_CHOP/*|_PC_24*/, _MCW_RC/*|_MCW_PC*/);
#elif defined(HG_HAVE_MSC_CONTROLFP)
    mode = _controlfp(0,0);
    _controlfp(_RC_CHOP/*|_PC_24*/, _MCW_RC/*|_MCW_PC*/);
#elif defined(HG_HAVE_FENV_H)
    int err;
    mode = (HGuint32)fegetround();
    err  = fesetround(FE_TOWARDZERO);
    HG_ASSERT(err == 0); /* Success? */
    HG_UNREF(err);
#elif defined(HG_GCC_X86_ASM)
    int temp;
    __asm__ __volatile__
    (
     "fstcw %0\n\t" /* Read the current state to %0 */
     "movl %0, %%ebx\n\t" /* Take copy for setting CHOP bits on */
     "orl $0xC00, %%ebx\n\t" /* Bits up! */
     "movl %%ebx, %1\n\t" /* Move to memory */
     "fldcw %1" /* Load the new control word */
     : "=m" (mode)
     : "m" (temp)
     : "%ebx"
    );
#elif (HG_OS == HG_OS_SYMBIAN)
#   if (HG_CPU == HG_CPU_ARM)
    hgSymbianSetFPMode(HG_TRUE);
#   endif
#elif (HG_OS == HG_OS_VANILLA && HG_COMPILER == HG_COMPILER_GCC)
#   if (HG_CPU == HG_CPU_ARM)
#      if (__GNUC__<4)
    /* FIXME: for some reason, warning-as-errors in GCC 4.x converts this to
     * an error */
#   warning "hgFsetfpumode not implemented for vanilla-arm-gcc"
#      endif
    HG_ASSERT(!"hgFsetfpumode not implemented");
#   endif
#else
#       error No implementation for hgFsetfpumode
#endif
    return mode;
}

HG_INLINE void hgFrestorefpumode(HGuint32 mode)
{
#if defined(HG_HAVE_MSC_CONTROLFP_S)
    _controlfp_s(HG_NULL, mode, _MCW_RC/*|_MCW_PC*/);
#elif defined(HG_HAVE_MSC_CONTROLFP)
    _controlfp(mode, _MCW_RC/*|_MCW_PC*/);
#elif defined(HG_HAVE_FENV_H)
    int err;
    err = fesetround((int)mode);
    HG_ASSERT(err == 0); /* Success? */
    HG_UNREF(err);
#elif defined(HG_GCC_X86_ASM)
    __asm__ __volatile__
    (
     "fldcw %0" /* Load the new control word */
     : /* No output */
     : "m" (mode)
    );
#elif (HG_OS == HG_OS_SYMBIAN)
    HG_UNREF(mode);
#   if (HG_CPU == HG_CPU_ARM)
    hgSymbianSetFPMode(HG_FALSE);
#   endif
#elif (HG_OS == HG_OS_VANILLA && HG_COMPILER == HG_COMPILER_GCC)
#   if (HG_CPU == HG_CPU_ARM)
#      if (__GNUC__<4)
    /* FIXME: for some reason, warning-as-errors in GCC 4.x converts this to
     * an error */
#   warning "hgFrestorefpumode not implemented for vanilla-arm-gcc"
#      endif
    HG_ASSERT(!"hgFrestorefpumode not implemented");
#   endif
#else
#       error No implementation for hgFrestorefpumode
#endif
}

/*-------------------------------------------------------------------------
 * Float consistency checking
 *-----------------------------------------------------------------------*/

HG_INLINE HGbool    hgFisvalid      (float A)                           { return F_ISVALID(FIN(A));             }
HG_INLINE HGbool    hgFfinite       (float A)                           { return F_FINITE(FIN(A));              }
HG_INLINE float     hgFclean        (float A)                           { return FOUT(F_CLEAN(FIN(A)));         }
HG_INLINE float     hgFcleanfinite  (float A)                           { return FOUT(F_CLEANFINITE(FIN(A)));   }

/*-------------------------------------------------------------------------
 * Negation and absolute value
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFabs      (float A)
{
#if defined (HG_HAVE_FLOAT_MATH_H_VARIANTS)
    return fabsf(A);
#else
    return (float)fabs(A);
#endif
}

HG_INLINE float     hgFnabs     (float A)                               { return -hgFabs(A);        }
HG_INLINE float     hgFneg      (float A)                               { return -A;                }

/*-------------------------------------------------------------------------
 * Rounding operations
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFfloor    (float A)                               { return FOUT(F_FLOOR(FIN(A))); }
HG_INLINE float     hgFceil     (float A)                               { return FOUT(F_NEG(F_FLOOR(F_NEG((FIN(A)))))); }
HG_INLINE float     hgFfrac     (float A)                               { return FOUT(F_FRAC(FIN(A)));          }

/*-------------------------------------------------------------------------
 * Conversion from integers into floats
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgUitof         (HGuint32 A)                        { return (float)(A);                    }
HG_INLINE float     hgItof          (HGint32  A)                        { return (float)(A);                    }
HG_INLINE float     hgItofscale     (HGint32 A, HGint32 shift)          { return FOUT(F_ITOFSCALE(A, shift));   }
HG_INLINE float     hgI64tofscale   (HGint64 A, HGint32 shift)          { return FOUT(F_I64TOFSCALE(A, shift)); }
HG_INLINE float     hgUtofscale     (HGuint32 A, HGint32 shift)         { return FOUT(F_UTOFSCALE(A, shift));   }

/*-------------------------------------------------------------------------
 * Conversions from floats into other data types
 *-----------------------------------------------------------------------*/

HG_INLINE HGint32       hgIchop         (float A)                       { return F_ICHOP(FIN(A));               }
HG_INLINE HGint32       hgIceil         (float A)                       { return F_ICEIL(FIN(A));               }
HG_INLINE HGint32       hgIfloor        (float A)                       { return F_IFLOOR(FIN(A));              }
HG_INLINE HGint32       hgF2ubyte       (float A)                       { return F_F2UBYTE(FIN(A));             }

HG_INLINE HGint32       hgIchopscale    (float A, HGint32 shift)        { return F_ICHOPSCALE(FIN(A), shift);   }
HG_INLINE HGint64       hgI64chopscale  (float A, HGint32 shift)        { return F_I64CHOPSCALE(FIN(A), shift); }

/*-------------------------------------------------------------------------
 * Addition and subtraction (two, three and four component ops)
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFadd      (float A, float B)                      { return A + B;     }
HG_INLINE float     hgFsub      (float A, float B)                      { return A - B;     }
HG_INLINE float     hgFadd3     (float A, float B, float C)             { return A + B + C; }
HG_INLINE float     hgFaddabs3  (float A, float B, float C)             { return (hgFabs(A) + hgFabs(B) + hgFabs(C)); }
HG_INLINE float     hgFadd4     (float A, float B, float C, float D)    { return A + B + C + D; }
HG_INLINE float     hgFaddabs4  (float A, float B, float C, float D)    { return (hgFabs(A) + hgFabs(B) + hgFabs(C) + hgFabs(D)); }

/*-------------------------------------------------------------------------
 * Multiplication and division
 * Note: if compiler/platform supports fast reciprocal computation,
 *       implement hgFrcp() using that.
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFmul      (float A, float  B)                     { return A * B;                         }
HG_INLINE float     hgFimul     (float A, HGint32 B)                    { return A * B;                         }
HG_INLINE float     hgFsqr      (float A)                               { return A * A;                         }

HG_INLINE float     hgFrcp      (float A)                               { return 1.0f / A;                      }
HG_INLINE float     hgFdiv      (float A, float B)                      { return A / B;                         }
#ifdef HG_USE_STD_MATH
HG_INLINE float     hgFmod      (float A, float B)                      { return (float)fmod(A,B);              }
#else
HG_INLINE float     hgFmod      (float A, float B)                      { return FOUT(F_MOD(FIN(A),FIN(B)));    }
#endif

HG_INLINE float     hgFshift    (float A, HGint32 B)                    { return FOUT(F_SHIFT(FIN(A),(B)));     }
HG_INLINE float     hgFtwice    (float A)                               { return A + A;                         }
HG_INLINE float     hgFhalf     (float A)                               { return A * 0.5f;                      }

/*-------------------------------------------------------------------------
 * Logarithms and exponents
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFlog2         (float A)                               { return FOUT(F_LOG2(FIN(A)));          }
HG_INLINE float     hgFexp2         (float A)                               { return FOUT(F_EXP2(FIN(A)));          }

/*-------------------------------------------------------------------------
 * Combos of multiplications and additions (dot products, determinants
 * etc.)
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFmadd     (float A, float B, float C)             { return A * B + C; }
HG_INLINE float     hgFmsub     (float A, float B, float C)             { return A * B - C; }
HG_INLINE float     hgFdot2     (float A, float B, float C, float D)    { return A * B + C * D; }
HG_INLINE float     hgFdot3     (float A, float B, float C, float D, float E, float F) { return A * B + C * D + E * F;  }
HG_INLINE float     hgFdot3add  (float A, float B, float C, float D, float E, float F, float G) { return A * B + C * D + E * F + G;  }
HG_INLINE float     hgFdot4     (float A, float B, float C, float D, float E, float F, float G, float H) { return A * B + C * D + E * F + G * H; }
HG_INLINE float     hgFdot4hi   (float A, float B, float C, float D, float E, float F, float G, float H) { return A * B + C * D + E * F + G * H; }
HG_INLINE float     hgFsqrlen3  (float A, float B, float C)             { return A * A + B * B + C * C;         }
HG_INLINE float     hgFsqrlen4  (float A, float B, float C, float D)    { return A * A + B * B + C * C + D * D; }
HG_INLINE float     hgFdet2     (float A, float B, float C, float D)    { return A * D - B * C;                 }

/*-------------------------------------------------------------------------
 * Square root, reciprocal square root and power function.
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFsqrt     (float A)                                   { return FOUT(F_SQRT(FIN(A)));          }
HG_INLINE float     hgFrsq      (float A)                                   { return FOUT(F_RSQ(FIN(A)));           }
HG_INLINE float     hgFpow      (float A, float B)                          { return FOUT(F_POW(FIN(A),FIN(B)));    }

/*-------------------------------------------------------------------------
 * Minimum/maximum ops
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFmin      (float A, float B)                          { return A < B ? A : B;                 }
HG_INLINE float     hgFmax      (float A, float B)                          { return A > B ? A : B;                 }
HG_INLINE float     hgFabsmin   (float A, float B)                          { return hgFabs(A) < hgFabs(B) ? A : B; }
HG_INLINE float     hgFabsmax   (float A, float B)                          { return hgFabs(A) > hgFabs(B) ? A : B; }
HG_INLINE float     hgFminZero  (float A)                                   { return A < 0.0f ? A : 0.0f;           }
HG_INLINE float     hgFmaxZero  (float A)                                   { return A > 0.0f ? A : 0.0f;           }
HG_INLINE float     hgFclamp    (float A, float B, float C)                 { return (A < B) ? B : (A > C) ? C : A; }

/*-------------------------------------------------------------------------
 * Comparisons between two floats
 *-----------------------------------------------------------------------*/

HG_INLINE HGint32   hgFcmp      (float A, float B)                          { return (A < B) ? -1 : (A > B) ? +1 : 0; }
HG_INLINE HGbool    hgFcmpeq    (float A, float B)                          { return (A == B);      }
HG_INLINE HGbool    hgFcmpne    (float A, float B)                          { return (A != B);      }
HG_INLINE HGbool    hgFcmple    (float A, float B)                          { return (A <= B);      }
HG_INLINE HGbool    hgFcmplt    (float A, float B)                          { return (A <  B);      }
HG_INLINE HGbool    hgFcmpge    (float A, float B)                          { return (A >= B);      }
HG_INLINE HGbool    hgFcmpgt    (float A, float B)                          { return (A >  B);      }

/*-------------------------------------------------------------------------
 * Comparisons against zero
 *-----------------------------------------------------------------------*/

HG_INLINE HGint32   hgFzcmp         (float A)                               { return (A < 0.0f) ? -1: (A > 0.0f) ? +1 : 0; }
HG_INLINE HGbool    hgFzcmpeq       (float A)                               { return A == 0.0f;     }
HG_INLINE HGbool    hgFzcmpne       (float A)                               { return A != 0.0f;     }
HG_INLINE HGbool    hgFzcmple       (float A)                               { return A <= 0.0f;     }
HG_INLINE HGbool    hgFzcmplt       (float A)                               { return A <  0.0f;     }
HG_INLINE HGbool    hgFzcmpge       (float A)                               { return A >= 0.0f;     }
HG_INLINE HGbool    hgFzcmpgt       (float A)                               { return A >  0.0f;     }

/*-------------------------------------------------------------------------
 * Comparisons between a float and an integer
 *-----------------------------------------------------------------------*/

HG_INLINE HGint32   hgFicmp         (float A, HGint32 B)                    { return (A < (double)(B)) ? -1 : (A > (double)(B)) ? +1 : 0; }
HG_INLINE HGbool    hgFicmpeq       (float A, HGint32 B)                    { return (A == (double)(B)); }
HG_INLINE HGbool    hgFicmpne       (float A, HGint32 B)                    { return (A != (double)(B)); }
HG_INLINE HGbool    hgFicmple       (float A, HGint32 B)                    { return (A <= (double)(B)); }
HG_INLINE HGbool    hgFicmplt       (float A, HGint32 B)                    { return (A <  (double)(B)); }
HG_INLINE HGbool    hgFicmpge       (float A, HGint32 B)                    { return (A >= (double)(B)); }
HG_INLINE HGbool    hgFicmpgt       (float A, HGint32 B)                    { return (A >  (double)(B)); }

/*-------------------------------------------------------------------------
 * Trigonometric Functions
 *-----------------------------------------------------------------------*/

#ifdef HG_USE_STD_MATH
HG_INLINE float     hgFsin      (float A)                               { return (float)(sin(A));   }
HG_INLINE float     hgFcos      (float A)                               { return (float)(cos(A));   }
HG_INLINE float     hgFtan      (float A)                               { return (float)(tan(A));   }
HG_INLINE float     hgFacos     (float A)                               { return (float)(acos(A));  }
#else
HG_INLINE float     hgFsin      (float A)                               { return FOUT(F_SIN(FIN(A)));  }
HG_INLINE float     hgFcos      (float A)                               { return FOUT(F_COS(FIN(A)));  }
HG_INLINE float     hgFtan      (float A)                               { return FOUT(F_TAN(FIN(A)));  }
HG_INLINE float     hgFacos     (float A)                               { return FOUT(F_ACOS(FIN(A))); }
#endif

/*-------------------------------------------------------------------------
 * Syntactic sugar
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFlen3     (float A, float B, float C)             { return hgFsqrt(hgFsqrlen3(A,B,C));    }
HG_INLINE float     hgFlen4     (float A, float B, float C, float D)    { return hgFsqrt(hgFsqrlen4(A,B,C,D));  }

/*-------------------------------------------------------------------------
 * Miscellaneous
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFsignum       (float A)                           { return FOUT(F_SIGNUM(FIN(A)));    }
HG_INLINE float     hgFlerp         (float A, float B, float lerp)      { return A + (B - A) * lerp;        }
HG_INLINE float     hgFavg          (float A, float B)                  { return 0.5f * A + 0.5f * B;       }

/*-------------------------------------------------------------------------
 * Deprecated
 *-----------------------------------------------------------------------*/

HG_INLINE float     hgFixed2f       (HGint32  A)                        { return (float)(A / 65536.0f); }
HG_INLINE HGint32   hgF2fixed       (float A)                           { return hgIchop(A * 65536.0f); }

#endif /* HG_NATIVE_FLOATS */

#if (HG_COMPILER == HG_COMPILER_INTELC)
#       pragma warning (pop)
#endif

/*----------------------------------------------------------------------*/
#endif /* __HGFLOAT_H */

