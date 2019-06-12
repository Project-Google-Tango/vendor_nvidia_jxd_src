/*
 * Copyright (c) 2006 - 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVERROR_H
#define INCLUDED_NVERROR_H

/**
 * @defgroup nverror Error Handling
 *
 * nverror.h contains our error code enumeration and helper macros.
 *
 * @{
 */

/**
 * The NvError enumeration contains ALL return / error codes.  Error codes
 * are specifically explicit to make it easy to identify where an error
 * came from.
 *
 * All error codes are derived from the macros in nverrval.h.
 * @ingroup nv_errors
 */
typedef enum
{
#define NVERROR(_name_, _value_, _desc_) NvError_##_name_ = _value_,
    /* header included for macro expansion of error codes */
    #include "nverrval.h"
#undef NVERROR

    // An alias for success
    NvSuccess = NvError_Success,

    NvError_Force32 = 0x7FFFFFFF
} NvError;

/**
 * A helper macro to check a function's error return code and propagate any
 * errors upward.  This assumes that no cleanup is necessary in the event of
 * failure.  This macro does not locally define its own NvError variable out of
 * fear that this might burn too much stack space, particularly in debug builds
 * or with mediocre optimizing compilers.  The user of this macro is therefore
 * expected to provide their own local variable "NvError e;".
 */
#define NV_CHECK_ERROR(expr) \
    do \
    { \
        e = (expr); \
        if (e != NvSuccess) \
            return e; \
    } while (0)

/**
 * A helper macro to check a function's error return code and, if an error
 * occurs, jump to a label where cleanup can take place.  Like NV_CHECK_ERROR,
 * this macro does not locally define its own NvError variable.  (Even if we
 * wanted it to, this one can't, because the code at the "fail" label probably
 * needs to do a "return e;" to propagate the error upwards.)
 */
#define NV_CHECK_ERROR_CLEANUP(expr) \
    do \
    { \
        e = (expr); \
        if (e != NvSuccess) \
            goto fail; \
    } while (0)


/**
 * Prints err if it is an error (does nothing if err==NvSuccess).
 * Always returns err unchanged
 * never prints anything if err==NvSuccess)
 *
 * NOTE: Do not use this with errors that are expected to occur under normal
 * situations.
 *
 * @param err - the error to return
 * @returns err
 */
#define NV_SHOW_ERRORS  NV_DEBUG
#if     NV_SHOW_ERRORS
#define NV_SHOW_ERROR(err)  NvOsShowError(err,__FILE__,__LINE__)
#else
#define NV_SHOW_ERROR(err)  (err)
#endif


/** @} */

#endif // INCLUDED_NVERROR_H
