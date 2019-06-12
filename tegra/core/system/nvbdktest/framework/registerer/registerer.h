/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_REGISTERER_H
#define INCLUDED_REGISTERER_H

#include "nvcommon.h"
#include "nvos.h"
#include "error_handler.h"

#if defined(__cplusplus)
extern "C"
{
#endif


// Registration record structure
typedef struct NvBDKTest_RegistrationRec
{
    NvU32 nSuites;
    NvU32 nTests;
    NvBDKTest_pSuite pSuite;
} NvBDKTest_Registration;
typedef NvBDKTest_Registration* NvBDKTest_pRegistration;



/**
 * Handles or drives the addition of a test(error checks/creation/insertion)
 *
 * @param pSuite The suite LL pointer into which the test node needs to be added to
 * @param pRetValue The test pointer which needs to be added to the suite
 * @param pStrName The test name string
 * @param pTestFunc The pointer to the test function
 * @param type The type string (eg: quick,stress,performance)
 *
 * @return Error code or NvSuccess if no failures
 */
NvError NvBDKTest_AddTest(NvBDKTest_pSuite pSuite,
                          NvBDKTest_pTest* pRetValue,
                          const char* pStrName,
                          NvBDKTest_TestFunc pTestFunc,
                          const char* type,
                          void *privateData,
                          NvBDKTest_FreePrivateFunc pFreeFunc);

/**
 * Handles or drives the addition of a suite(error checks/creation/insertion)
 *
 * @param pStrName The test name string
 *
 * @return Error code or NvSuccess if no failures
 */
NvError NvBDKTest_AddSuite(const char* pStrName,
                           NvBDKTest_pSuite* pRetValue);


/**
 * Searches a suite in the suite LL
 *
 * @param suiteName The suite name string
 *
 * @return Valid suite pointer if seach is successful else NULL
 */
NvBDKTest_pSuite NvBDKTest_GetSuitePtr(const char* suiteName);

/**
 * Searches a test in the test LL of a suite
 *
 * @param pSuite The suite name string
 * @param testName The test name string
 *
 * @return Valid test pointer if seach is successful else NULL
 */
NvBDKTest_pTest NvBDKTest_GetTestPtr(NvBDKTest_pSuite pSuite,
                                     const char* testName);

/**
 * Returns number of tests for a given test entry configuration
 *
 * @param pSuite The suite pointer
 * @param Argument The argument which specifies input choice(test type,testname,all)
.*
 * @return Number of tests
 */
NvU32 NvBDKTest_GetNumTests(const char* Suite, const char* Argument);

/*
 * Initates creation of default registration with all the suites/tests available
 */
NvError NvBDKTest_CreateDefaultRegistration(void);

/*
 * To get the name of all the suites
 *
 * @param buffer The suite_info buffer
 * @param size Size of the input buffer, by default it is 4 bytes
 *
 * @return Pointer for the entire suiteinfo
 */
NvU8* NvBDKTest_GetSuiteInfo(NvU8 *buffer, NvU32 *size);


#if defined(__cplusplus)
                     }
#endif

#endif // INCLUDED_REGISTERER_H
