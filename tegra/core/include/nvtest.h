/*
 * Copyright (c) 2006 - 2007 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVTEST_H
#define INCLUDED_NVTEST_H

/**
 * NvTest is an api suite for test applications only.  These apis should not
 * be used in drivers.
 * 
 *
 *  H   H   OOO   W   W        TTTTT   OOO        U   U   SSSS  EEEE
 *  H   H  O   O  W W W          T    O   O       U   U  S      E
 *  HHHHH  O   O  W W W          T    O   O       U   U   SSS   EEE
 *  H   H  O   O  WWWWW          T    O   O       U   U      S  E
 *  H   H   OOO    W W           T     OOO         UUU   SSSS   EEEE
 *
 * A simple working example program which uses nvtest is located in
 *
 *    tests/transport/basic/basic.c
 *    tests/transport/basic/Makefile
 *
 * Use that as an example of how to use the macros and functions in this 
 * file.  The entry point in your test should be the NvTestMain() function
 * which you will write (instead of writing a main() function).
 *
 * You may also find the functions in these files useful in writing your test:
 *
 *    include/nvos.h
 *    include/nvutil.h
 *    include/nvapputil.h
 *    include/nvtestio.h
 *
 */


//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvcommon.h"
#include "nvos.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

/**---------------------------------------------------------------------------
 * Setup the test state - call once before running any tests
 */
#define NVTEST_INIT(appHandle) \
        { NvTestInitialize(0, 0, appHandle); }

/**---------------------------------------------------------------------------
 * Report results - call once after all tests have been run
 */
#define NVTEST_RESULT(appHandle) \
        return NvTestTerminate(*(appHandle))

/**---------------------------------------------------------------------------
 * Execute a test function with the given argument. An example:
 *
 * #include "nvtest.h"
 *
 * static void
 * testsuite_TestOne( NvTestApplicationHandle h )
 * {
 *     NvError err;
 *
 *     // do stuff
 *     err = sometest();     
 *     NVTEST_VERIFY( h, err == NvSuccess );
 * }
 *
 * NvError
 * NvTestMain( int argc, char *argv[] )
 * {
 *     NvTestApplication h;
 *
 *     NVTEST_INIT( &h );
 *
 *     // do any necessary driver init here
 *
 *     NVTEST_RUN( &h, testsuite_TestOne );     // run 1st test
 *     NVTEST_RUN( &h, testsuite_TestTwo );     // optional: run 2nd test
 *     // optional: add more tests here
 *
 *     NVTEST_RESULT( &h );
 * }
 *
 */
#define NVTEST_RUN( appHandle, testFunction )                               \
    {                                                                       \
        NvTestWriteFTrace("Running test " #testFunction "\n");              \
        if (NvTestSubtestBegin(*(appHandle), 0, #testFunction))             \
        {                                                                   \
            testFunction(appHandle);                                        \
        }                                                                   \
    }

#define NVTEST_RUN_ARG( appHandle, testFunction, arg )                      \
    {                                                                       \
        NvTestWriteFTrace("Running test " #testFunction "\n");              \
        if (NvTestSubtestBegin(*(appHandle), 0, #testFunction))             \
        {                                                                   \
            testFunction(appHandle, (arg) );                                \
        }                                                                   \
    }

/**---------------------------------------------------------------------------
 * Fails a test.  Output results and records some bookkeeping.
 * You must have called one of:
 *      NvTestSubtestBegin()
 *      NVTEST_RUN()
 *      NVTEST_RUN_ARG()
 * before calling NVTEST_FAIL().
 */
#define NVTEST_FAIL( appHandle )                                            \
        { NvTestSubtestFail(*(appHandle),0,0,__FILE__,__LINE__); }

/**---------------------------------------------------------------------------
 * Checks the expression result. Similar to assert, but does not compile out.
 * You must have called one of:
 *      NvTestSubtestBegin()
 *      NVTEST_RUN()
 *      NVTEST_RUN_ARG()
 * before calling NVTEST_VERIFY().
 */
#define NVTEST_VERIFY( appHandle, x )                                       \
    if( !(x) )                                                              \
        NVTEST_FAIL(appHandle)

/**---------------------------------------------------------------------------
 * Same as NVTEST_VERIFY, but will execute code on a failure, usually will
 * be a 'return' or 'goto clean' for cleanup, etc.
 * You must have called one of:
 *      NvTestSubtestBegin()
 *      NVTEST_RUN()
 *      NVTEST_RUN_ARG()
 * before calling NVTEST_VERIFY_CODE().
 */
#define NVTEST_VERIFY_CODE( appHandle, x, code )                            \
    if( !(x) )                                                              \
    {                                                                       \
        NVTEST_FAIL(appHandle);                                             \
        code;                                                               \
    }

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

#if defined(__cplusplus)
extern "C"
{
#endif

// Opaque handle for the test application
typedef struct NvTestAppRec **NvTestApplicationHandle;
typedef struct NvTestAppRec *NvTestApplication;

// Opaque handle to an instance of a test
typedef struct NvTestRec *NvTestHandle;

// Opaque handle for a subtest
typedef struct NvTestSubtestRec *NvTestSubtestHandle;


//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

/*------------------------------------------------------------------------*/
/**
 * Test application entry point.
 *
 * @param argc Number of arguments including executable name
 * @param argv Program aguments
 *
 * libnvtest exports the 'main' symbol.  Test applications should only export
 * 'NvTestMain'
 */
NvError
NvTestMain( int argc, char *argv[] );

/*------------------------------------------------------------------------*/
/**
 * Create an instance of the test.  Not all tests are required to support
 * multiple instances -- it is acceptable for NvTestCreate to fail if called
 * more than once.
 *
 * @param phTest Output parameter that is filled in with a handle to the newly
 *     created instance of the test.
 * @param argc Number of arguments including executable name
 * @param argv Program arguments.  This pointer may only be assumed to be valid
 *     during the execution of the NvTestCreate function.  It may be invalid if
 *     used later, e.g., during NvTestRun or NvTestDestroy.
 */
NvError NvTestCreate(NvTestHandle *phTest, int argc, char *argv[]);

/*------------------------------------------------------------------------*/
/**
 * Run the test and report the test's score, if any.
 *
 * @param hTest Handle to an instance of the test.
 * @param pScore Output parameter that is filled in with the score of the test.
 *     This is intended for use by performance tests, etc. where the test gives
 *     more than just "pass" or "fail" as its outcome.  For tests that are
 *     pass/fail only, scores of 0 and 1 should suffice (in addition to the
 *     NvError reporting the sort of failure).
 */
NvError NvTestRun(NvTestHandle hTest, NvU32 *pScore);

/*------------------------------------------------------------------------*/
/**
 * Destroy an instance of the test.
 *
 * @param hTest Handle to an instance of the test.
 *     This function should be a nop if hTest is NULL.
 */
void NvTestDestroy(NvTestHandle hTest);

/*------------------------------------------------------------------------*/
/**
 * Called once all shared libraries for a test have been loaded.  For setting
 * breakpoints.
 */
void NvLoad(void);

/*------------------------------------------------------------------------*/
/**
 * Called just before a subtest is run.  For setting breakpoints.
 *
 * @param testIndex index of subtest that is about to run.
 */
void NvRun(NvU32 testIndex);

/*------------------------------------------------------------------------*/
/** NvTestInitialize() call before calling any other functions
 *
 *  NvTestInitialize() may be called multiple times in a test,
 *  but each call to NvTestInitialize() must have a matching call to
 *  NvTestTerminate().
 *
 *  @param argc      - the program's argc
 *  @param argv      - the program's argv (or NULL)
 *  @param pAppHandle - handle is returned here
 */
void NvTestInitialize(
            int                 *argc,
            char                *argv[],
            NvTestApplication   *pApp);

/*------------------------------------------------------------------------*/
/** NvTestTerminate() call after all tests have been run.
 *
 *  This function displays results of all tests.
 *
 * @param app - the app handle returned by NvTestInitialize();
 *
 * @Returns NvSuccess if no tests have failed
 * @Returns NvError_TestApplicationFailed if any test failed
 */
NvError NvTestTerminate(NvTestApplication app);

/*------------------------------------------------------------------------*/
/** NvTestError() indicate that the test should return an error.
 *
 *  If called, the test will return an error.
 *  If not called, the test may still return an error (e.g. if a subtest fails)
 *
 * @param app - the app handle returned by NvTestInitialize();
 * @param err - the error to return (if NvSuccess, the app will return
 *                    NvError_TestApplicationFailed)
 */
void NvTestError(NvTestApplication app, NvError err);

/*------------------------------------------------------------------------*/
/** NvTestSubtestBegin() call before running a subtest
 *
 * @Returns FALSE if test should not be run
 * @Returns TRUE  if test should be run
 */
NvBool NvTestSubtestBegin(
            NvTestApplication app,
            NvTestSubtestHandle *pSubtest,
            const char *testNameFormat, ...);

/*------------------------------------------------------------------------*/
/** NvTestSubtestFail() call if a test fails (may be called multiple times)
 *  You must call NvTestSubtestBegin() before calling NvTestSubtestFail().
 *
 * @param appHandle - handle from NvTestInitialize() (may be NULL)
 * @param subHandle - handle from NvTestSubtestBegin() (may be NULL)
 * @param reason    - optional description of failure
 * @param file      - file where error occurred
 * @param line      - line in file where error occurred
 *
 */
void NvTestSubtestFail(
            NvTestApplication    app,
            NvTestSubtestHandle  subtest,
            const char          *reason,
            const char          *file,
            int                  line);

/*------------------------------------------------------------------------*/
/** NvTestSubtestSkip() call if a test should be skipped but
 *  NVTestSubtestBegin has already been called to start the test.
 *
 * @param appHandle - handle from NvTestInitialize() (may be NULL)
 * @param subHandle - handle from NvTestSubtestBegin() (may be NULL)
 * @param reason    - optional description for skipping test
 *
 */
#define NVTEST_HAS_SKIPSUBTEST_API 1
void NvTestSubtestSkip(
            NvTestApplication    app,
            NvTestSubtestHandle  subtest,
            const char          *reason);

/*------------------------------------------------------------------------*/
/** NvTestPrintf() printf output to host stdout
 */
void NvTestPrintf(const char *fmt, ...);

/*------------------------------------------------------------------------*/
/** NvTestVprintf() vprintf output to host stdout
 */
void NvTestVprintf(const char *format, va_list ap);

/*------------------------------------------------------------------------*/
/** NvTestReadline() read a line of text from stdin
 *
 *  Read up to and including the next end-of-line character from stdin.
 *  If the line is shorter than bufferLength-1 then the last character in buffer
 *  will be '\n'.  If the line is longer than bufferLength-1 then
 *  bufferLength-1 characters will be read into the buffer.  The buffer is
 *  always null-terminated.  The function writes to *count the number of
 *  characters placed into the buffer including the '\n' but not including the
 *  NULL terminator.  If no error occurs:
 *          1 <= *count <= bufferLength-1
 *  If an error occurs, *count will contain 0, buffer[0] will contain 0, and
 *  a nonzero error will be returned.
 *  
 * 
 * @param prompt       - If non-NULL, this is printed before waiting for input.
 * @param buffer       - buffer where line will be stored
 * @param bufferLength - size of buffer (bytes)
 * @param count        - if non-NULL, the number of bytes placed into buffer is
 *                          stored here (counting '\n' but not counting NULL
 *                          terminator).  If NULL this is ignored.
 *
 * @returns NvSuccess (0) for success
 * @returns a nonzero error code on failure (e.g. NvError_EndOfFile)
 */
NvError NvTestReadline(
                const char *prompt, 
                char *buffer, 
                size_t bufferLength, 
                size_t *count);

/*------------------------------------------------------------------------*/
/** NvTestGetFailureCount() returns number of failures so far
 *
 * @param appHandle - handle from NvTestInitialize() (may be NULL)
 */
NvU32 NvTestGetFailureCount(NvTestApplication app);

/*------------------------------------------------------------------------*/
/**
 * This allows the the executable to set the NvRun() function.
 *
 * @param func - this should point to the NvRun() function.
 */
void NvTestSetRunFunction(NvTestApplication app, void (*func)(NvU32));


/*------------------------------------------------------------------------*/
/** Subtest filter limits the subtests executed by returning false from
 *  subtest begin if the test name does not match filter.
 *  
 *  Filter can be a full test name to select single test or '<prefix>*'
 *  to select all tests with name starting with <prefix>.
 *
 */
void NvTestSetSubtestFilter(NvTestApplication app, const char* Filter);

/*------------------------------------------------------------------------*/
/** returns 2 strings which identify the chip and simulator.
 *   pChip:
 *     unk   - unknown
 *     ap15  - AP15
 *     ap16  - AP16
 *     t20   - T20 (AP20)
 *     t30   - T30
 *     etc
 *  
 *   pSim:
 *     unk   - unknown
 *     si    - silicon
 *     asim  - asim
 *     csim  - csim
 *     fpga  - fpga emulator
 *     etc
 */
void NvTestGetHwStrings(const char **pChip, const char **pSim);

/*------------------------------------------------------------------------*/
/** NvTestWrite() write data to host stdout --DEPRICATED - use NvTestPrintf()
 */
void NvTestWrite(const void *data, size_t len);

/*------------------------------------------------------------------------*/
/** NvTestWriteString() write string to stdout --DEPRICATED - use NvTestPrintf
 */
void NvTestWriteString(const char *str);

/*------------------------------------------------------------------------*/
/** Report test results in REST format
 */
void NvTestResultsForREST(NvTestApplication app, char* filename);

/*------------------------------------------------------------------------*/
/** NvTestWriteFTrace() write a string into ftrace
 */
void NvTestWriteFTrace(const char *str);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVTEST_H
