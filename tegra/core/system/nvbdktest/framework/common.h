/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_COMMON_H
#define INCLUDED_COMMON_H

#include "nvcommon.h"
#include "nvos.h"
#include "string.h"
#include "nvbdktest.h"
#include "nv3p.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* Size macros */
#define TEMP_STRING_SIZE 50
#define NVBDKTEST_RESULT_BUFFER_SIZE (1024 * 32)

/**
 * Prototype for test function pointer
 */
typedef void (*NvBDKTest_TestFunc)(NvBDKTest_TestPrivData);

/**
 * Thread data for single test
 */
typedef struct NvBDKTest_ThreadDataRec
{
    NvOsSemaphoreHandle pThreadSem;
    NvOsSemaphoreHandle pExitSem;
    NvOsThreadHandle pThread;
    NvBDKTest_TestPrivData PrivDataStruc;
    NvBDKTest_TestFunc pTestFunc;
    NvBool bDetach;
} NvBDKTest_ThreadData;

/**
 * Test node structure
.*
.* Holds test info data and pointer to the next test node
 */
typedef struct NvBDKTest_TestRec
{
    char* pTestName;
    NvBool isActive;
    char* pTestType;
    NvBDKTest_TestFunc pTestFunc;
    void *pPrivateData;
    NvBDKTest_FreePrivateFunc pFreePrivateFunc;

    struct NvBDKTest_TestRec* pNext;
    struct NvBDKTest_TestRec* pPrev;
} NvBDKTest_Test;
typedef NvBDKTest_Test* NvBDKTest_pTest;

/**
 * Test suite structure
.*
.* Holds suite info data and pointer to the next suite node
 */
typedef struct NvBDKTest_SuiteRec
{
    char* pSuiteName;
    NvBool isActive;
    NvU32 nTests;
    NvBDKTest_pTest pTest;

    struct NvBDKTest_SuiteRec* pNext;
    struct NvBDKTest_SuiteRec* pPrev;
} NvBDKTest_Suite;
typedef NvBDKTest_Suite* NvBDKTest_pSuite;

/**
 * Structure for capturing failure log for each test
 */
typedef struct NvBDKTest_FailureLogRec
{
    NvU32 lineNum;
    char* pCondition;
    char* pFileName;
    char* pFunctionName;
    char* pTestName;
    char* pSuiteName;

    struct NvBDKTest_FailureLogRec* pNext;
    struct NvBDKTest_FailureLogRec* pPrev;
} NvBDKTest_FailureLog;
typedef NvBDKTest_FailureLog* NvBDKTest_pFailureLog;

/**
 * Runner Log Structure
.*
.* Holds metadata of a test entry run
 */
typedef struct NvBDKTest_RunnerLog
{
    NvU32 nSuitesRun;
    NvU32 nSuitesFailed;
    NvU32 nTestsRun;
    NvU32 nTestsFailed;
    NvU32 nAssertsTested;
    NvU32 nAssertsFailed;
    NvU32 nTimeTaken;
} NvBDKTest_RunnerLog;
typedef NvBDKTest_RunnerLog* NvBDKTest_pRunnerLog;

/**
 * Test result structure
.*
.* Holds test result info and a pointer to the test failure LL
 */
typedef struct NvBDKTest_TestResultLogRec
{
    NvU32 nAssertsTested;
    NvU32 nAssertsFailed;
    NvU32 nTimeTaken;
    char* pTestName;
    char* pSuiteName;
    NvBDKTest_pFailureLog pFailureLog;
    struct NvBDKTest_TestResultLogRec* pNext;
    struct NvBDKTest_TestResultLogRec* pPrev;
    Nv3pStatus status;
} NvBDKTest_TestResultLog;
typedef NvBDKTest_TestResultLog* NvBDKTest_pTestResultLog;

typedef struct NvBDKTest_TestEnvRec
{
    NvBool stopOnFail;
    NvU32 timeout;
} NvBDKTest_TestEnv;

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_COMMON_H
