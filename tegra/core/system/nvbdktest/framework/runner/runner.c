/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "registerer.h"
#include "runner.h"
#include "nvsystem_utils.h"

// Global variables for Registration
extern NvBDKTest_pRegistration s_pReg;

// Global variables for a TestEntry
NvBDKTest_pRunnerLog s_pTestEntryRunnerLog;
NvBDKTest_pFailureLog s_pTestFailureLog;
NvBool s_TestEntryRunning;
NvU32 s_TestEntryStartTime;
Nv3pSocketHandle s_h3p;
NvBDKTest_TestEnv s_TestEnv;

/**
 * Runs all the tests of a particular type present in the suite test LL
 */
static
NvError NvBDKTest_RunSingleType(NvBDKTest_pSuite pSuite,
                                const char* type,
                                NvBDKTest_TestPrivData PrivDataStruc);

/**
 * Runs all the tests in the suite test LL
 */
static
NvError NvBDKTest_RunSingleSuite(NvBDKTest_pSuite pSuite,
                                 NvBDKTest_TestPrivData PrivDataStruc);

/**
 * Runs all the tests of a particular type
 */
static
NvError NvBDKTest_RunSingleTest(NvBDKTest_pTest pTest,
                                NvBDKTest_pSuite pSuite,
                                NvBDKTest_TestPrivData PrivDataStruc);


/**
 * Extracts the data from the test log structure and sends the result buffer to the host
 */
static NvError NvBDKTest_SendTestLog(NvBDKTest_pTestResultLog TestLog);

/**
 * Extracts the data from the global Test Entry runner log and sends the result buffer to the host
 */
static NvError NvBDKTest_SendTestRunnerLog(void);

/**
 * Initializes the global variables and the runner log for the test entry
 */
static NvError NvBDKTest_TestEntryInit(void);

/**
 * De-inits all test entry data
 */
static void NvBDKTest_TestEntryDeInit(void);

/**
 * Frees all memory in the temporary test result log structure
 */
static void NvBDKTest_FreeTestLog(NvBDKTest_pTestResultLog TestLog);

/**
  * Copy memory and return last pointer of dest
  */
static void *NvBDKTest_CopyMemIncAddr(void *dest, void *src, NvU32 len);

/**
  * Generate runner message with NvOsSnprint
  */
static NvU32
NvBDKTest_CopyRunnerMessage(NvBDKTest_pRunnerLog pRunnerLog,
                            char *buf,
                            NvU32 size);

/**
  * Generate test message
  */
static NvU32
NvBDKTest_CopyTestMessage(NvBDKTest_pTestResultLog pTestLog,
                          char *buf,
                          NvU32 size);

/**
  * Compact data to send to nvflash through nv3p
  */
static NvU8*
NvBDKTest_CreateCompactionData(NvBDKTest_pTestResultLog pTestLog,
                               NvU32 *pTotalSize);

/**
 * Delete thread data for BDKTest
 */
static
NvBDKTest_ThreadData*
NvBDKTest_DeleteThreadData(NvBDKTest_ThreadData *pThreadData);

/**
 * Create thread data for BDKTest
 */
static
NvBDKTest_ThreadData*
NvBDKTest_CreateThreadData(NvBDKTest_TestPrivData *pPrivDataStruc,
                           NvBDKTest_TestFunc pTestFunc);

/**
 * Thread function for BDKTest
 */
static
void NvBDKTest_TestRunner(void *v);

static NvBDKTest_ThreadData*
NvBDKTest_DeleteThreadData(NvBDKTest_ThreadData *pThreadData)
{
    if (pThreadData)
    {
        if (pThreadData->pThreadSem)
            NvOsSemaphoreDestroy(pThreadData->pThreadSem);
        if (pThreadData->pExitSem)
            NvOsSemaphoreDestroy(pThreadData->pExitSem);
        if (pThreadData)
            NvOsFree(pThreadData);
    }
    return NULL;
}

static NvBDKTest_ThreadData*
NvBDKTest_CreateThreadData(NvBDKTest_TestPrivData *pPrivDataStruc,
                           NvBDKTest_TestFunc pTestFunc)
{
    NvBDKTest_ThreadData *p = NULL;
    p = NvOsAlloc(sizeof(NvBDKTest_ThreadData));
    if (p)
    {
        NvError e = NvSuccess;
        NvOsMemset(p, 0, sizeof(NvBDKTest_ThreadData));
        NvOsMemcpy(&(p->PrivDataStruc),
                   pPrivDataStruc,
                   sizeof(NvBDKTest_TestPrivData));
        p->pTestFunc = pTestFunc;

        e = NvOsSemaphoreCreate(&p->pThreadSem, 0);
        if( e != NvSuccess )
        {
            NvOsDebugPrintf("%s cannot careate pThreadSem \n", __func__);
            goto fail;
        }

        e = NvOsSemaphoreCreate(&p->pExitSem, 0);
        if( e != NvSuccess )
        {
            NvOsDebugPrintf("%s cannot careate pExitSem \n", __func__);
            goto fail;
        }
    }
    else
    {
        NvOsDebugPrintf("%s cannot careate thread data \n", __func__);
        goto fail;
    }
    return p;

fail:
    NvBDKTest_DeleteThreadData(p);
    return NULL;
}

static
void NvBDKTest_TestRunner(void *v)
{
    NvBDKTest_ThreadData *pData = v;
    NvU32 i = 0;
    const NvU32 retryCnt = 100;
    if (pData)
    {
        NvError status;
        NvBDKTest_TestPrivData *pPrivDataStruc = &(pData->PrivDataStruc);

        (*pData->pTestFunc)(*pPrivDataStruc);

        NvOsSemaphoreSignal(pData->pThreadSem);
        for (i = 0; i < retryCnt; i++)
        {
            if (pData->bDetach)
                break;
            status = NvOsSemaphoreWaitTimeout(pData->pExitSem, 100);
            if (status == NvSuccess)
                break;
        }
        /* free private data */
        if (pPrivDataStruc->pData && pPrivDataStruc->pFreeDataFunc)
            (*pPrivDataStruc->pFreeDataFunc)(pPrivDataStruc->pData);

        /* free thread data */
        NvBDKTest_DeleteThreadData(pData);
    }
}

static
NvError NvBDKTest_RunSingleTest(NvBDKTest_pTest pTest,
                                NvBDKTest_pSuite pSuite,
                                NvBDKTest_TestPrivData PrivDataStruc)
{
    static NvBool s_bEmergencyFlag = NV_FALSE;
    NvError e = NvSuccess;
    const char *err_str = 0;
    NvU32 nStartAsserts, nStartAssertFailed, nStartTime;
    NvBDKTest_pTestResultLog pNewTestResult = NULL;
    NvError threadStatus = NvSuccess;
    NvU32 timeout = NV_WAIT_INFINITE;

    if (s_TestEnv.timeout)
        timeout = s_TestEnv.timeout * 1000;

    if (s_bEmergencyFlag)
    {
        NvBDKTest_TestResultLog tmpLog;

        /* pass this test */
        NvOsMemset(&tmpLog, 0, sizeof(tmpLog));
        tmpLog.nAssertsTested = 1;
        tmpLog.nAssertsFailed = 1;
        tmpLog.pTestName = pTest->pTestName;
        tmpLog.pSuiteName = pSuite->pSuiteName;
        tmpLog.status = Nv3pStatus_InvalidState;

        s_pTestEntryRunnerLog->nAssertsTested++;
        s_pTestEntryRunnerLog->nAssertsFailed++;
        s_pTestEntryRunnerLog->nTestsRun++;
        s_pTestEntryRunnerLog->nTestsFailed++;
        NvOsDebugPrintf("---------------------------------------\n");
        NvOsDebugPrintf("Test: %s , Not tested!\n", pTest->pTestName);
        NvOsDebugPrintf("---------------------------------------\n");

        e = NvBDKTest_SendTestLog(&tmpLog);
        if (e != NvSuccess)
        {
            err_str = "Failure in Sending Test log buffer";
            goto fail;
        }
        return NvSuccess;
    }

    // Initialize the failure record for a test as Null
    s_pTestFailureLog = NULL;

    // Set up some variables to update RunnerLog
    nStartAsserts = s_pTestEntryRunnerLog->nAssertsTested;
    nStartAssertFailed = s_pTestEntryRunnerLog->nAssertsFailed;
    nStartTime = NvOsGetTimeMS();

    if (pTest->pTestFunc)
    {
        NvBDKTest_ThreadData *pThreadData = NULL;
        // Call the test function and update the metadata
        PrivDataStruc.pData = pTest->pPrivateData;
        PrivDataStruc.pFreeDataFunc = pTest->pFreePrivateFunc;

        pThreadData = NvBDKTest_CreateThreadData(&PrivDataStruc,
                                                 pTest->pTestFunc);
        if (pThreadData)
        {
            e = NvOsThreadCreate(NvBDKTest_TestRunner,
                                 pThreadData,
                                 &(pThreadData->pThread));
            if( e != NvSuccess )
            {
                NvBDKTest_DeleteThreadData(pThreadData);
                goto fail;
            }
        }
        s_pTestEntryRunnerLog->nTestsRun++;

        /* Wait thread */
        threadStatus = NvOsSemaphoreWaitTimeout(pThreadData->pThreadSem,
                                                timeout);
        if (threadStatus == NvSuccess)
        {
            /* Exit thread */
            NvOsSemaphoreSignal(pThreadData->pExitSem);
        }
        else
        {
            /* Detach thread */
            pThreadData->bDetach = NV_TRUE;
        }

        /* thread will free private data */
        PrivDataStruc.pData = NULL;
        PrivDataStruc.pFreeDataFunc = NULL;

        // Allocate memory for capturing a test log
        pNewTestResult = (NvBDKTest_pTestResultLog)NvOsAlloc
                         (sizeof(NvBDKTest_TestResultLog));
        if (pNewTestResult == NULL)
        {
            e = NvError_InsufficientMemory;
            err_str = "Memory could not be allocated for the TestLog struc.";
            goto fail;
        }

        NvOsMemset(pNewTestResult, 0, sizeof(NvBDKTest_pTestResultLog));
        // Fill details into the Test Log structure once the test has been run and failure records
        // have been appended.
        if (pTest->pTestName)
        {
            pNewTestResult->pTestName =
                (char*)NvOsAlloc(NvOsStrlen(pTest->pTestName) + 1);
            if(pNewTestResult->pTestName == NULL)
            {
                e = NvError_InsufficientMemory;
                err_str = "Memory could not be allocated for TestLog.TestName";
                goto fail;
            }
            NvOsStrcpy(pNewTestResult->pTestName, pTest->pTestName);
        }
        if (pSuite->pSuiteName)
        {
            pNewTestResult->pSuiteName =
                (char*)NvOsAlloc(NvOsStrlen(pSuite->pSuiteName) + 1);
            if(pNewTestResult->pSuiteName == NULL)
            {
                e = NvError_InsufficientMemory;
                err_str = "Memory could not be allocated for TestLog.SuiteName";
                goto fail;
            }
            NvOsStrcpy(pNewTestResult->pSuiteName, pSuite->pSuiteName);
        }

        pNewTestResult->pFailureLog    = s_pTestFailureLog;
        pNewTestResult->nTimeTaken     = NvOsGetTimeMS() - nStartTime;
        pNewTestResult->nAssertsTested = s_pTestEntryRunnerLog->nAssertsTested
                                         - nStartAsserts;
        pNewTestResult->nAssertsFailed = s_pTestEntryRunnerLog->nAssertsFailed
                                         - nStartAssertFailed;
        pNewTestResult->pNext          = NULL;
        pNewTestResult->pPrev          = NULL;

        // Update Runner Log info after updating and test run and display result on UART
        // FIXME: To add display function for device Display of resutl
        NvOsDebugPrintf("#######################################\n");
        if (threadStatus != NvSuccess)
        {
            /* Stop all tests bacause the thread is stuck */
            s_bEmergencyFlag = NV_TRUE;

            pNewTestResult->status = Nv3pStatus_BDKTestTimeout;
            pNewTestResult->nAssertsFailed++;
            (s_pTestEntryRunnerLog->nTestsFailed)++;
            NvOsDebugPrintf("Test: %s , Timeout!\n", pTest->pTestName);
        }
        else if (pNewTestResult->nAssertsFailed != 0)
        {
            if (s_TestEnv.stopOnFail)
                s_bEmergencyFlag = NV_TRUE;

            pNewTestResult->status = Nv3pStatus_BDKTestRunFailure;
            (s_pTestEntryRunnerLog->nTestsFailed)++;
            NvOsDebugPrintf("Test: %s , Failed!\n", pTest->pTestName);
        }
        else
        {
            pNewTestResult->status = Nv3pStatus_Ok;
            NvOsDebugPrintf("Test: %s , Passed!\n", pTest->pTestName);
        }
        NvOsDebugPrintf("#######################################\n\n");
        s_pTestEntryRunnerLog->nTimeTaken += pNewTestResult->nTimeTaken;

        /* After the test run and updation of runner log,
           send the test output log to the host */
        e = NvBDKTest_SendTestLog(pNewTestResult);
        if (e != NvSuccess)
        {
            err_str = "Failure in Sending Test log buffer";
            goto fail;
        }

        // Free the local test log pointer
        NvBDKTest_FreeTestLog(pNewTestResult);
        pNewTestResult = NULL;
    }
    else
    {
        e = NvError_BDKTestNullTestFuncPtr;
        err_str = "No test function is assigned for the test";
        goto fail;
    }

fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_RunSingleSuite failed : NvError 0x%x\n",
                        err_str , e);
    return e;
}

static
NvError NvBDKTest_RunSingleSuite(NvBDKTest_pSuite pSuite,
                                 NvBDKTest_TestPrivData PrivDataStruc)
{
    NvError e = NvSuccess;
    const char *err_str = 0;
    NvBDKTest_pTest pTest = NULL;

    if (!pSuite)
    {
        e = NvError_BDKTestUnknownSuite;
        err_str = "Null Suite pointer received. Unknown suite";
        goto fail;
    }
    else
    {
        pTest = pSuite->pTest;
        if (pTest == NULL)
        {
            e = NvError_BDKTestNoTest;
            err_str = "No test has been registered. Suite is empty";
            goto fail;
        }
        else
        {
            while (pTest != NULL)
            {
                e = NvBDKTest_RunSingleTest(pTest, pSuite, PrivDataStruc);
                if (e != NvSuccess)
                {
                    err_str = "NvBDKTest_RunSingleTest failed";
                    goto fail;
                }
                pTest = pTest->pNext;
            }
        }
    }

    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_RunSingleSuite failed : NvError 0x%x\n",
                        err_str , e);
clean:
    return e;
}

static
NvError NvBDKTest_RunSingleType(NvBDKTest_pSuite pSuite,
                                const char* type,
                                NvBDKTest_TestPrivData PrivDataStruc)
{
    NvError e = NvSuccess;
    const char *err_str = 0;
    NvBDKTest_pTest pTest = NULL;
    NvU32 isTypeRegistered = NV_FALSE;

    if (!pSuite)
    {
        e = NvError_BDKTestUnknownSuite;
        err_str = "Null Suite pointer received. Unknown suite";
        goto fail;
    }
    else
    {
        pTest = pSuite->pTest;
        if (pTest == NULL)
        {
            e = NvError_BDKTestNoTest;
            err_str = "No test has been registered. Suite is empty";
            goto fail;
        }
        else
        {
            while (pTest != NULL)
            {
                if (!NvOsStrcmp(pTest->pTestType, type))
                {
                    isTypeRegistered = NV_TRUE;
                    e = NvBDKTest_RunSingleTest(pTest, pSuite, PrivDataStruc);
                    if (e != NvSuccess)
                    {
                        err_str = "NvBDKTest_RunSingleTest failed";
                        goto fail;
                    }
                }
                pTest = pTest->pNext;
            }
            if (!isTypeRegistered)
            {
                err_str = "Request for a test type which has not been registered";
                e = NvError_BDKTestUnknownType;
                goto fail;
            }
        }
    }

    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_RunSingleType failed : NvError 0x%x\n",
                        err_str , e);
clean:
    return e;
}

static void *NvBDKTest_CopyMemIncAddr(void *dest, void *src, NvU32 len)
{
    if ((len > 0) && src)
    {
        NvOsMemcpy(dest, src, len);
        return (void *)((NvU8 *)(dest) + len);
    }
    return dest;
}

static NvU32
NvBDKTest_CopyRunnerMessage(NvBDKTest_pRunnerLog pRunnerLog,
                            char *buf,
                            NvU32 size)
{
    NvU32 totalSize = 0;
    if (pRunnerLog)
    {
        totalSize = NvOsSnprintf(buf ,size,
                                 "========================\n"
                                 "Total Tests Run: %d\n"
                                 "Failed Tests: %d\n"
                                 "AssertsTested: %d\n"
                                 "AssertsFailed: %d\n"
                                 "TimeTaken: %dMs\n"
                                 "========================\n\n",
                                 pRunnerLog->nTestsRun,
                                 pRunnerLog->nTestsFailed,
                                 pRunnerLog->nAssertsTested,
                                 pRunnerLog->nAssertsFailed,
                                 pRunnerLog->nTimeTaken);
    }
    return totalSize;
}
static NvU32
NvBDKTest_CopyTestMessage(NvBDKTest_pTestResultLog pTestLog,
                          char *buf,
                          NvU32 size)
{

    NvU32 totalSize = 0;
    NvBDKTest_pFailureLog tempFlog = NULL;

    switch (pTestLog->status) {
    case Nv3pStatus_BDKTestTimeout:
        totalSize += NvOsSnprintf(buf, size,
                                  "[Timeout] %s:%s\n",
                                  pTestLog->pSuiteName, pTestLog->pTestName);
        break;
    case Nv3pStatus_BDKTestRunFailure:
        for (tempFlog = pTestLog->pFailureLog;
             tempFlog;
             tempFlog = tempFlog->pNext)
        {
            totalSize += NvOsSnprintf(buf, size,
                                      "[Fail] %s:%d "
                                      "Failure condition: %s",
                                      tempFlog->pFunctionName,
                                      tempFlog->lineNum,
                                      tempFlog->pCondition);
            if (buf)
            {
                buf += NvOsStrlen(buf);
                size -= NvOsStrlen(buf);
            }
        }
        break;
    case Nv3pStatus_InvalidState:
        totalSize += NvOsSnprintf(buf, size,
                                  "[Not Tested] %s:%s",
                                  pTestLog->pSuiteName, pTestLog->pTestName);
        break;
    case Nv3pStatus_Ok:
        totalSize += NvOsSnprintf(buf, size,
                                  "[Pass] No Failures/Asserts");
        break;
    default:
        totalSize += NvOsSnprintf(buf, size,
                                  "[Unknown] %s:%s",
                                  pTestLog->pSuiteName, pTestLog->pTestName);
        break;
    }

    return totalSize;
}

/*
 *  ------------------------------
 * | length of data |    data     |
 *  ------------------------------
 * The size of length is 4 bytes and the size of data is variable.
 *
 *  -------------------------
 * | length | [suite name]   |
 *  -------------------------
 * | length | [test name]    |
 *  -------------------------
 * | length | [status]       |
 *  -------------------------
 * | length | [elapsed time] |
 *  -------------------------
 * | length | [message]      |
 *  -------------------------
 */
static NvU8*
NvBDKTest_CreateCompactionData(NvBDKTest_pTestResultLog pTestLog,
                               NvU32 *pTotalSize)
{
    NvU8 *buf = NULL;
    if (pTestLog)
    {
        NvU32 length = 0;
        char *tempBuf = NULL;
        NvU32 size = NVBDKTEST_RESULT_BUFFER_SIZE;
        NvU32 msgLength = 0;

        tempBuf =(char*)NvOsAlloc(size);
        if (!tempBuf)
        {
            NvOsDebugPrintf("cannot allocate memory [%s:%d]\n",
                            __func__, __LINE__);
            return NULL;
        }

        NvBDKTest_CopyTestMessage(pTestLog, tempBuf, size);
        msgLength = NvOsStrlen(tempBuf) +1;

        *pTotalSize = (sizeof(NvU32) + NvOsStrlen(pTestLog->pSuiteName) + 1
                       + sizeof(NvU32) + NvOsStrlen(pTestLog->pTestName) + 1
                       + sizeof(NvU32) + sizeof(pTestLog->status)
                       + sizeof(NvU32) + sizeof(pTestLog->nTimeTaken)
                       + sizeof(NvU32) + msgLength);

        buf = NvOsAlloc(*pTotalSize);
        if (buf)
        {
            NvU8 *p = buf;

            length = NvOsStrlen(pTestLog->pSuiteName) + 1;
            p = (NvU8 *)NvBDKTest_CopyMemIncAddr(p, &length, sizeof(NvU32));
            p = (NvU8 *)NvBDKTest_CopyMemIncAddr(p, pTestLog->pSuiteName,
                                                 length);

            length = NvOsStrlen(pTestLog->pTestName) + 1;
            p = (NvU8 *)NvBDKTest_CopyMemIncAddr(p, &length, sizeof(NvU32));
            p = (NvU8 *)NvBDKTest_CopyMemIncAddr(p, pTestLog->pTestName,
                                                 length);

            length = sizeof(pTestLog->status);
            p = (NvU8 *)NvBDKTest_CopyMemIncAddr(p, &length, sizeof(NvU32));
            p = (NvU8 *)NvBDKTest_CopyMemIncAddr(p, &pTestLog->status, length);

            length = sizeof(pTestLog->nTimeTaken);
            p = (NvU8 *)NvBDKTest_CopyMemIncAddr(p, &length, sizeof(NvU32));
            p = (NvU8 *)NvBDKTest_CopyMemIncAddr(p, &(pTestLog->nTimeTaken),
                                                 length);


            p = (NvU8 *)NvBDKTest_CopyMemIncAddr(p, &msgLength, sizeof(NvU32));
            p = (NvU8 *)NvBDKTest_CopyMemIncAddr(p, tempBuf, msgLength);
        }

        NvOsFree(tempBuf);
    }
    return buf;
}

static void NvBDKTest_FreeTestLog(NvBDKTest_pTestResultLog TestLog)
{
    NvBDKTest_pTestResultLog pCur = NULL;
    NvBDKTest_pTestResultLog pNext = NULL;
    NvBDKTest_pFailureLog pFailureCur = NULL;
    NvBDKTest_pFailureLog pFailureNext = NULL;

    pCur = TestLog;
    while (pCur)
    {
        if (pCur->pSuiteName)
            NvOsFree(pCur->pSuiteName);
        if (pCur->pTestName)
            NvOsFree(pCur->pTestName);
        if (pCur->pFailureLog)
        {
            pFailureCur = pCur->pFailureLog;
            while (pFailureCur)
            {
                if (pFailureCur->pSuiteName)
                    NvOsFree(pFailureCur->pSuiteName);
                if (pFailureCur->pTestName)
                    NvOsFree(pFailureCur->pTestName);
                if (pFailureCur->pFileName)
                    NvOsFree(pFailureCur->pFileName);
                if (pFailureCur->pFunctionName)
                    NvOsFree(pFailureCur->pFunctionName);
                if (pFailureCur->pCondition)
                    NvOsFree(pFailureCur->pCondition);
                pFailureNext = pFailureCur->pNext;
                NvOsFree(pFailureCur);
                pFailureCur = pFailureNext;
            }
            pCur->pFailureLog = NULL;
        }
        pNext = pCur->pNext;
        NvOsFree(pCur);
        pCur = pNext;
    }
}

static NvError NvBDKTest_SendTestLog(NvBDKTest_pTestResultLog TestLog)
{
    NvError e = NvSuccess;
    const char *err_str = 0;
    NvU32 size = 0;
    NvU8 *buf = NULL;

    buf = NvBDKTest_CreateCompactionData(TestLog, &size);
    if (buf)
    {
        // Send result buf string to the host
        e = Nv3pDataSend(s_h3p, buf, size, 0);
        if (e != NvSuccess)
        {
            err_str = "Sending of Test Log buffer failed";
            goto fail;
        }
    }
    else
    {
        e = NvError_InsufficientMemory;
        err_str = "Memory could not be allocated for SendTestLogBuffer";
    }


fail:
    if (err_str)
        NvOsDebugPrintf("%s NvBDKTest_SendTestLog failed : NvError 0x%x\n",
                        err_str , e);
    if (buf)
        NvOsFree(buf);
    return e;
}

static NvError NvBDKTest_SendTestRunnerLog(void)
{
    NvError e = NvSuccess;
    NvU32 size = NVBDKTEST_RESULT_BUFFER_SIZE;
    const char *err_str = 0;
    char *tempBuf = 0;

    tempBuf =(char*)NvOsAlloc(size);
    if (!tempBuf)
    {
        NvOsDebugPrintf("cannot allocate memory [%s:%d]\n", __func__, __LINE__);
        return NvError_InsufficientMemory;
    }

    NvBDKTest_CopyRunnerMessage(s_pTestEntryRunnerLog, tempBuf, size);

    // Send TestEntry metadata to the host
    e = Nv3pDataSend(s_h3p, (NvU8 *)tempBuf, NvOsStrlen(tempBuf) + 1, 0);
    if (e != NvSuccess)
    {
        err_str = "Sending of Test Runner Log buffer failed";
        goto fail;
    }

fail:
    if (err_str)
        NvOsDebugPrintf("%s NvBDKTest_SendTestRunnerLog failed: NvError 0x%x\n",
                        err_str , e);
    if (tempBuf)
        NvOsFree(tempBuf);
    return e;
}

static NvError NvBDKTest_TestEntryInit(void)
{
    // Initialize RunnerLogList
    s_pTestEntryRunnerLog = (NvBDKTest_pRunnerLog) NvOsAlloc
                            (sizeof(NvBDKTest_RunnerLog));
    if (!s_pTestEntryRunnerLog)
        return NvError_InsufficientMemory;
    s_pTestEntryRunnerLog->nAssertsFailed = 0;
    s_pTestEntryRunnerLog->nAssertsTested = 0;
    s_pTestEntryRunnerLog->nSuitesFailed = 0;
    s_pTestEntryRunnerLog->nSuitesRun = 0;
    s_pTestEntryRunnerLog->nTestsFailed = 0;
    s_pTestEntryRunnerLog->nTestsRun = 0;
    s_pTestEntryRunnerLog->nTimeTaken = 0;

    // Set global variables
    s_TestEntryRunning = NV_TRUE;
    s_TestEntryStartTime = NvOsGetTimeMS();

    return NvSuccess;
}

static void NvBDKTest_TestEntryDeInit(void)
{
    if (s_pTestEntryRunnerLog)
        NvOsFree(s_pTestEntryRunnerLog);
    s_TestEntryRunning = NV_FALSE;
    s_TestEntryStartTime = NvOsGetTimeMS() - s_TestEntryStartTime;
}
NvError NvBDKTest_RunInputTestPlan(const char* Suite,
                                   const char* Argument,
                                   NvBDKTest_TestPrivData PrivDataStruc,
                                   Nv3pSocketHandle h3p)
{
    NvBDKTest_pSuite pSuite = NULL;
    NvBDKTest_pTest pTest = NULL;
    NvError e = NvSuccess;
    const char *err_str = 0;

    s_h3p = h3p;

    // Initiate Test Entry
    e = NvBDKTest_TestEntryInit();
    if (e != NvSuccess)
    {
        err_str = "Memory could not be allocated for the TestEntryRunnerLog"
                  "NvBDKTest_TestEntryInit failed";
        goto fail;
    }

// Fixme : Add later
// b = NvBDKTest_CheckValidInput(Suite, Argument);

    // Call respective runner functions based on module and argument(testtype, test name , all)
    if (!NvOsStrcmp(Argument, "all"))
    {
        pSuite = NvBDKTest_GetSuitePtr(Suite);
        e = NvBDKTest_RunSingleSuite(pSuite, PrivDataStruc);
    }
    else if (NvOsStrcmp(Argument, "all")
            && (Argument[0] != 'n' && Argument[1] != 'v'))
    {
        pSuite = NvBDKTest_GetSuitePtr(Suite);
        e = NvBDKTest_RunSingleType(pSuite, Argument, PrivDataStruc);
    }
    else
    {
        pSuite = NvBDKTest_GetSuitePtr(Suite);
        pTest = NvBDKTest_GetTestPtr(pSuite, Argument);
        e = NvBDKTest_RunSingleTest(pTest, pSuite, PrivDataStruc);
    }
    if (e != NvSuccess)
        goto fail;

    // After tests have been run and test logs have been sent , send metadata/runner log
    e = NvBDKTest_SendTestRunnerLog();
    if (e != NvSuccess)
    {
        err_str = "Failure in Sending Test Runner Log buffer";
        goto fail;
    }

    // De Initiate TestEntry
    NvBDKTest_TestEntryDeInit();

    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_RunInputTestPlan failed : NvError 0x%x\n",
                        err_str , e);
clean:
    return e;
}

NvError NvBDKTest_SetBDKTestEnv(NvBool stopOnFail, NvU32 timeout)
{
    s_TestEnv.stopOnFail = stopOnFail;
    s_TestEnv.timeout = timeout;

    return NvSuccess;
}
