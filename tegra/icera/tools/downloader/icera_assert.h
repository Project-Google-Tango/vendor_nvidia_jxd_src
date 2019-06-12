/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @file icera_asssert.h Fundamental definitions.
 */

#ifndef ICERA_ASSERT_H
/**
 * Macro definition used to avoid recursive inclusion.
 */
#define ICERA_ASSERT_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

#ifndef __dxp__
#include <assert.h>         /* Needed for standard assertion macros. */
#endif

#if defined(__dxp__)
#include <icera_sdk.h>

/* TARGET_DXP variants have moved to ... */
#include "com_ice_variant.h"

/* Include unfixed GNATs defs */
#include "com_unfixed_gnats.h"

/* Checking of the toolchain compatibility */
#if defined (TARGET_DXP8060)
    #error "DXP8060 no longer supported."
#else
    #if !(ICERA_SDK_EV_AT_LEAST(4,9,'a'))
        #error "DXP toolchain too old to compile this software. Please upgrade your SDK."
        /* or comment the #error line if you really have to                       */
    #endif
#endif

#ifndef OVERRIDE_DXP_TOOLCHAIN_VERSION_CHECK
    #if ICERA_SDK_EV_AT_LEAST(4,9,'z')
        #error "DXP toolchain too recent. This software has not yet been validated with it."
    #endif
#endif

#endif

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/**
 * Attribute to declare that a function never returns
 *
 */
#ifdef __dxp__
#define DXP_NEVER_RETURNS   __attribute__((noreturn))
#else
#define DXP_NEVER_RETURNS
#endif

/**
 * @defgroup icera_assert Assertion support
 * @{
 */
#ifndef DXP_RELEASE_CODE

/**
 * Assertion macro, compiled only in non-release code.
 * @param cond Condition to check.
 */
    #define DEV_ASSERT(cond)    REL_ASSERT(cond)
    #define DEV_ASSERT_EXTRA(cond, n, ...)    REL_ASSERT_EXTRA(cond, n, __VA_ARGS__)
/**
 * Failure macro, compiled only in non-release code.
 * @param msg Message failure string.
 */
    #define DEV_FAIL(msg)       REL_FAIL(msg)
    #define DEV_FAIL_EXTRA(msg, n, ...)    REL_FAIL_EXTRA(msg, n, __VA_ARGS__)
#else
    #define DEV_ASSERT(cond)
    #define DEV_ASSERT_EXTRA(cond, n, ...)
    #define DEV_FAIL(msg)
    #define DEV_FAIL_EXTRA(msg, n, ...)
#endif

/**
 * We hope to label and suppress false positives from Klocwork static checking
 * by putting asserts into the code.
 * We use the UNVERIFIED form when we haven't done a serious code review.
 * In the code review we might choose to re-write (even for a false positive)
 * to improve the structure or legibility of the code.
 */
#define KW_ASSERT_FALSE_POSITIVE(cond)  DEV_ASSERT(cond)
#define KW_ASSERT_UNVERIFIED(cond)      DEV_ASSERT(cond)

#if (defined (__dxp__))
/**
 * Assertion macro, always compiled.
 * @param cond Condition to check.
 */
struct com_AssertInfo
{
    const int line;
    const char *filename;
    const char condition[];
};


#if (ICERA_SDK_EV_AT_LEAST(4,3,'a'))
/* EV4.3a only!

  SDK team implemented: Auto-generated thunks for invokation of assert function (requires EV4.3a compiler)

  - to reduce codesize in the caller of assert (and thus improve I-Cache performance or reduce I-Mem consumption)
    the code in the caller of assert is a simple fn call (no args are formed to the actual assert function)
  - the simple fn call is to an auto-generated thunk function which contains the invokation of the underlying assert fail code
  - the think is auto-generated using a new cpp extension (pragma deferred_output) so the source text for the thunk body
    is expanded *outside* the function which calls the assert macro (the alternative would be to use GCC nested
    functions, but these can't have attributes
  - it uses attributes to ensure its uncached (i.e. not in I-Mem and not close to important I-Cached functions),
    not instrumented, doesn't return etc. etc
  - the thunk's name is unique using another new cpp extension (__CANON_BASE_FILE__ and __CANON_LINE__)
    so that the thunk names are globally unique

 Updated by Eddy for EV4.5a: initialising the condition field is not allowed when condition is a
 flexible array member. So declare a local structure with exactly the right size. The same code is
 generated, but the debugging .size now includes the true length of the condition string rather than
 0 for a variable array element.
*/

#define INNER_NAME_OF_ASSERT_THUNK(canonBaseFileArg, canonLineArg) _AssertThunk_##canonBaseFileArg##_##canonLineArg

#define GENERATE_NAME_OF_ASSERT_THUNK(canonBaseFileArg, canonLineArg) INNER_NAME_OF_ASSERT_THUNK(canonBaseFileArg, canonLineArg)

#define INNER_GENERATE_REL_ASSERT_THUNK(cond, canonBaseFileArg, canonLineArg, fileArg, lineArg) \
void GENERATE_NAME_OF_ASSERT_THUNK(canonBaseFileArg, canonLineArg) (void)  __attribute__((noreturn, unused, noinline, no_instrument_function, placement(uncached))); \
void GENERATE_NAME_OF_ASSERT_THUNK(canonBaseFileArg, canonLineArg) (void) {\
struct sizedAssertInfo {const int line; const char *filename; const char condition[sizeof(#cond)];}; \
static const struct sizedAssertInfo    aInfoThunk __attribute__((placement(uncached))) ={.filename=fileArg, .line=lineArg, .condition=#cond};\
com_AssertFailStruct ((const struct com_AssertInfo*)&aInfoThunk); }

#define GENERATE_REL_ASSERT_THUNK(cond, canonBaseFileArg, canonLineArg, fileArg, lineArg) \
  INNER_GENERATE_REL_ASSERT_THUNK(cond, canonBaseFileArg, canonLineArg, fileArg, lineArg)

/* Icera-invented pragma (must occur *after* the assert define)
*/
#pragma GCC deferred_output GENERATE_REL_ASSERT_THUNK

#define INNER_GENERATE_REL_ASSERT_IF(cond, canonBaseFileArg, canonLineArg) \
  if ((cond)==0) {\
    void GENERATE_NAME_OF_ASSERT_THUNK(canonBaseFileArg, canonLineArg) (void) __attribute__((noreturn, unused, noinline, no_instrument_function, placement(uncached))); \
    GENERATE_NAME_OF_ASSERT_THUNK(canonBaseFileArg, canonLineArg) (); \
  }

#define GENERATE_REL_ASSERT_IF(cond, canonBaseFileArg, canonLineArg) \
  INNER_GENERATE_REL_ASSERT_IF(cond, canonBaseFileArg, canonLineArg)


#define INNER_REL_ASSERT(cond, canonBaseFileArg, canonLineArg) \
  GENERATE_REL_ASSERT_THUNK(cond, canonBaseFileArg, canonLineArg, __FILE__, __LINE__) \
  GENERATE_REL_ASSERT_IF(cond, canonBaseFileArg, canonLineArg)

/* Icera-invented __CANON_BASE_FILE__ to form a valid globally unique function name
*/
#define REL_ASSERT(cond) \
  INNER_REL_ASSERT(cond, __CANON_BASE_FILE__, __CANON_LINE__)


/* ASSERT_EXTRA routines take varargs, and these can potentially reference local variables
   - they wouldn't be in scope in the thunk, and thus the thunk-approach won't work.
   Thus we'll use a non-thunk approach for ASSERT_EXTRA ;-(
*/
#define REL_ASSERT_EXTRA(cond, n, ...)  if (unlikely(!(cond))) { \
struct sizedAssertInfo {const int line; const char *filename; const char condition[sizeof(#cond)];}; \
static const struct sizedAssertInfo ainfo __attribute__((placement(uncached))) = {.filename=__FILE__, .line=__LINE__, .condition=#cond};\
com_ExtendedAssertionStruct((const struct com_AssertInfo*)&ainfo, n, __VA_ARGS__);}

#else
/* SDK tweaked version of old assert macros
   - Using static data in a placement (internal) or overlay function *will place static data in D-Mem*. Thus we
     use an attribute here to ensure its in uncached memory (as its not performance critical) so it will interfere
     less with normal cached memory, and will avoid wasting D-Mem from internal functions.

   - Using const with (tweaks to the ainfo types) gets it into the best section and avoids duplication for dual-build
     compilation units

   - Further codespace could be saved by improving the com_ExtendedAssertionStruct interface to pass args differently, or
       to call another generated function to setup the args (this function *not* being in I-Mem!). See "auto-generated
       assert thunk" above for an idea of how we can improve this with EV4.3a
*/
  #define REL_ASSERT(cond)                if (unlikely(!(cond))) {static const struct com_AssertInfo ainfo __attribute__((placement(uncached))) = {.filename=__FILE__, .line=__LINE__, .condition=#cond}; com_AssertFailStruct(&ainfo);}

  #define REL_ASSERT_EXTRA(cond, n, ...)  if (unlikely(!(cond))) {static const struct com_AssertInfo ainfo  __attribute__((placement(uncached))) = {.filename=__FILE__, .line=__LINE__, .condition=#cond}; com_ExtendedAssertionStruct(&ainfo, n, __VA_ARGS__);}

#endif

#else //linux
    #define REL_ASSERT(cond)                assert(cond);
    #define REL_ASSERT_EXTRA(cond, n, ...)  REL_ASSERT(cond)
#endif

/**
 * Failure macro, always compiled.
 * @param msg Message failure string..
 */
#define REL_FAIL(msg)        REL_ASSERT(!(msg))
#define REL_FAIL_EXTRA(msg, n, ...)  REL_ASSERT_EXTRA(!(msg), n, __VA_ARGS__)

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/


/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/
 

/**
 * Assertion function, called on assertion failure. The struct version produces better code, but
 * the non-struct version is retained for compatibility
 *
 * @param filename  The name of the file containing the error
 * @param line_no   Line number
 * @param message   Text string of the assertion that failed
 */
extern void DXP_NEVER_RETURNS
com_AssertFail(const char *filename, int line_no, const char *message);
#ifdef __dxp__
extern void DXP_NEVER_RETURNS
com_AssertFailStruct(const struct com_AssertInfo *assert_info);
#endif

/**
 * Extended assertion function, called on assertion failure
 *
 * @param filename  The name of the file containing the error
 * @param line_no   Line number
 * @param message   Text string of the assertion that failed
 * @param n_extra_fields    Number of extra information fields
 * @param ...               int32s carrying information to help diagnose the error
 */
extern void DXP_NEVER_RETURNS
com_ExtendedAssertion(const char *filename,
                      int line_no,
                      const char *message,
                      int n_extra_fields,
                      ...);
#if (defined (__dxp__))
extern void DXP_NEVER_RETURNS
com_ExtendedAssertionStruct(const struct com_AssertInfo *assert_info,
                      int n_extra_fields,
                      ...);
#endif

#endif

/** @} END OF FILE */

