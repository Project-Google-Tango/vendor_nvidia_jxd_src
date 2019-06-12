/*
 * Copyright 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_DRF_ASM_H
#define INCLUDED_NVRM_DRF_ASM_H

/**
 *  @defgroup nvrm_drf RM DRF Macros
 *
 *  @ingroup nvddk_rm
 *
 * The following suite of macros are used from assembly language code for
 * generating values to write into hardware registers, or for extracting
 * fields from read registers. These macros are less sophistocated than
 * C counterparts because the assembler does not understand the C ternary
 * operator. In particular, depending on the version of the assember, there
 * may not be common C-style aliases to the native assembler operators. To
 * accomodate the lowest common denominator, only the native assember operators
 * (e.g., :SHL:, :AND:, etc.) are used.
 */

/**
 *  The default implementation of _MK_ENUM_CONST in the spec-generated headers
 *  appends the C unsigned long suffix operator (UL). This is incompatible with
 *  the assembler. Force _MK_ENUM_CONST to define just a plain constant.
 */
#if defined(_MK_ENUM_CONST)
#undef  _MK_ENUM_CONST
#define _MK_ENUM_CONST(_constant_) (_constant_)
#endif

//-----------------------------------------------------------------------------
// Abstract logical operations because some ARM assemblers don't
// understand C operators.
//-----------------------------------------------------------------------------

#if !defined(_AND_)
#if defined(_MSC_VER)       // Microsoft compiler/assember
#define _AND_ :AND:
#else
#define _AND_ &
#endif
#endif

#if !defined(_OR_)
#if defined(_MSC_VER)       // Microsoft compiler/assember
#define _OR_ :OR:
#else
#define _OR_ |
#endif
#endif

#if !defined(_SHL_)
#if defined(_MSC_VER)       // Microsoft compiler/assember
#define _SHL_ :SHL:
#else
#define _SHL_ <<
#endif
#endif


/** NV_DRF_OFFSET - Get a register offset

    @ingroup nvrm_drf

    @param d register domain (hardware block)
    @param r register name
 */
#define NV_DRF_OFFSET(d,r)  (d##_##r##_0)

/** NV_DRF_SHIFT - Get the shift count of a field

    @ingroup nvrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
 */
#define NV_DRF_SHIFT(d,r,f) (d##_##r##_0_##f##_SHIFT)

/** NV_DRF_MASK - Get the mask value of a field

    @ingroup nvrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
 */
#define NV_DRF_MASK(d,r,f)  ((d##_##r##_0_##f##_DEFAULT_MASK) _SHL_ (d##_##r##_0_##f##_SHIFT))

/** NV_DRF_DEF - define a new register value.

    @ingroup nvrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param c defined value for the field
 */
#define NV_DRF_DEF(d,r,f,c) \
    ((d##_##r##_0_##f##_##c) _SHL_ (d##_##r##_0_##f##_SHIFT))

/** NV_DRF_NUM - define a new register value.

    @ingroup nvrm_drf

    @param d register domain (hardware block)
    @param r register name
    @param f register field
    @param n numeric value for the field
 */
#define NV_DRF_NUM(d,r,f,n) \
    ((((n) _AND_ (d##_##r##_0_##f##_DEFAULT_MASK))) _SHL_ \
        (d##_##r##_0_##f##_SHIFT))

/** NV_RESETVAL - get the reset value for a register.

    @ingroup nvrm_drf

    @param d register domain (hardware block)
    @param r register name
 */
#define NV_RESETVAL(d,r)    (d##_##r##_0_RESET_VAL)

#endif // INCLUDED_NVRM_DRF_ASM_H
