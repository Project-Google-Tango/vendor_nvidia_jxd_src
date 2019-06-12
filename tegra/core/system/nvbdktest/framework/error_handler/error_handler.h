/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_ERROR_HANDLER_H
#define INCLUDED_ERROR_HANDLER_H

#include "nvcommon.h"
#include "nvos.h"
#include "common.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// Test failure/success macros
#define TEST_BOOL_TRUE 1
#define TEST_BOOL_FALSE 0

#define TEST_ASSERT_MESSAGE(value, test, suite, message) \
    do \
    { \
        b = NvBDKTest_TestAssert((value), (message) \
            , test, suite , __FILE__, __FUNCTION__,__LINE__); \
        if (!b) \
        { \
            NvOsDebugPrintf("[%s:%d] <%s> %s \n", __FUNCTION__, __LINE__, test, message); \
            NvOsDebugPrintf(" at %s \n", __FILE__); \
            goto fail; \
        } \
    } while (0)

#define TEST_ASSERT(value, test, suite, message) \
    do \
    { \
        b = NvBDKTest_TestAssert((value), ("ASSERT(" #value,")=>",#message) \
            , test, suite , __FILE__, __FUNCTION__,__LINE__); \
        if (!b) \
        { \
            NvOsDebugPrintf("%s \n",message); \
            goto fail; \
        } \
    } while (0)

#define TEST_ASSERT_EQUAL(actual, expected, test, suite, message) \
    do \
    { \
       b = NvBDKTest_TestAssert(((actual) == (expected)) \
         , ("ASSERT_EQUAL(" #actual "," #expected ")=>" #message), test, suite \
          , __FILE__, __FUNCTION__,__LINE__); \
       if (!b) \
       { \
           NvOsDebugPrintf("%s \n",message); \
           goto fail; \
       } \
    } while (0)

#define TEST_ASSERT_PTR_NULL(value, test, suite, message) \
    do \
    { \
        b = NvBDKTest_TestAssert((NULL == (void*)(value)) \
           , ("ASSERT_PTR_NULL(" #value")(" #message ")"), test, suite \
           , __FILE__, __FUNCTION__,__LINE__); \
        if (!b) \
        { \
            NvOsDebugPrintf("%s \n",message); \
            goto fail; \
        } \
    } while (0)

#define TEST_ASSERT_STRING_EQUAL(actual, expected, test, suite, message) \
    do \
    { \
        b = NvBDKTest_TestAssert(!(NvOsStrcmp((const char*)(actual) \
            , (const char*)(expected))) \
            , ("ASSERT_STRING_EQUAL(" #actual "," #expected ") =>" #message) \
            , test, suite, __FILE__, __FUNCTION__,__LINE__); \
        if (!b) \
        { \
            NvOsDebugPrintf("%s \n",message); \
            goto fail; \
        } \
    } while (0)


/**
 * Registers the incoming test asert info to the failure log
 *
 * @param value The assert check value : TEST_BOOL_TRUE or TEST_BOOL_FALSE
 * @param pCondition The string of the condition which was checked ( eg: "i == 0")
 * @param pTestName The test name string
 * @param pSuiteName The suite name string
 * @param pFileName The file name string
 * @param pFunctionName The function name string
 * @param lineNum The line number at which failure occured
 *
 * @return NV_FALSE if asserts fail else NV_TRUE
 */
NvBool NvBDKTest_TestAssert(NvU32 value,
                           const char* pCondition,
                           const char* pTestName,
                           const char* pSuiteName,
                           const char* pFileName,
                           const char* pFunctionName,
                           NvU32 lineNum);

#if defined(__cplusplus)
                     }
#endif

#endif // INCLUDED_ERROR_HANDLER_H
