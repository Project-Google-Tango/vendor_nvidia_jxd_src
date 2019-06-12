/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "error_handler.h"

extern NvBDKTest_pFailureLog s_pTestFailureLog;
extern NvBDKTest_pRunnerLog s_pTestEntryRunnerLog;


/**
 * Logs the failure/fail or pass assert to the Failure log list
 */
static NvError NvBDKTest_LogFailure(NvBDKTest_pFailureLog* pFailureLog,
                             NvBDKTest_pRunnerLog* pRunnerInfo,
                             const char* pSuiteName,
                             const char* pTestName,
                             const char* pCondition,
                             const char* pFileName,
                             const char* pFunctionName,
                             NvU32 lineNum);



static NvError NvBDKTest_LogFailure(NvBDKTest_pFailureLog* pFailureLog,
                             NvBDKTest_pRunnerLog* pRunnerInfo,
                             const char* pSuiteName,
                             const char* pTestName,
                             const char* pCondition,
                             const char* pFileName,
                             const char* pFunctionName,
                             NvU32 lineNum)
{
    NvBDKTest_pFailureLog pFailureNew = NULL;
    NvBDKTest_pFailureLog pTemp = NULL;
    NvError e = NvSuccess;
    const char *err_str = 0;

    // Initialize a new failure node
    pFailureNew = (NvBDKTest_pFailureLog)NvOsAlloc(sizeof(NvBDKTest_FailureLog));
    if (pFailureNew == NULL)
    {
        e = NvError_InsufficientMemory;
        err_str = "Cannot allocate memory for a new Failure Log record";
        goto fail;
    }

    // Fill details
    if (pSuiteName)
    {
        pFailureNew->pSuiteName = (char*)NvOsAlloc(NvOsStrlen(pSuiteName) + 1);
        if(pFailureNew->pSuiteName == NULL)
        {
            e = NvError_InsufficientMemory;
            err_str = "Cannot allocate memory for SuiteName";
            goto fail;
        }
        NvOsStrcpy(pFailureNew->pSuiteName, pSuiteName);
    }
    if (pTestName)
    {
        pFailureNew->pTestName = (char*)NvOsAlloc(NvOsStrlen(pTestName) + 1);
        if(pFailureNew->pTestName == NULL)
        {
            e = NvError_InsufficientMemory;
            err_str = "Cannot allocate memory for TestName";
            goto fail;
        }
        NvOsStrcpy(pFailureNew->pTestName, pTestName);
    }
    if (pCondition)
    {
        pFailureNew->pCondition = (char*)NvOsAlloc(NvOsStrlen(pCondition) + 1);
        if(pFailureNew->pCondition == NULL)
        {
            e = NvError_InsufficientMemory;
            err_str = "Cannot allocate memory for Condition";
            goto fail;
        }
        NvOsStrcpy(pFailureNew->pCondition, pCondition);
    }
    if (pFileName)
    {
        pFailureNew->pFileName = (char*)NvOsAlloc(NvOsStrlen(pFileName) + 1);
        if(pFailureNew->pFileName == NULL)
        {
            e = NvError_InsufficientMemory;
            err_str = "Cannot allocate memory for FileName";
            goto fail;
        }
        NvOsStrcpy(pFailureNew->pFileName, pFileName);
    }
    if (pFunctionName)
    {
        pFailureNew->pFunctionName = (char*)NvOsAlloc(NvOsStrlen(pFunctionName) + 1);
        if(pFailureNew->pFunctionName == NULL)
        {
            e = NvError_InsufficientMemory;
            err_str = "Cannot allocate memory for FunctionName";
            goto fail;
        }
        NvOsStrcpy(pFailureNew->pFunctionName, pFunctionName);
    }

    pFailureNew->lineNum = lineNum;
    pFailureNew->pNext = NULL;
    pFailureNew->pPrev = NULL;

    pTemp = *pFailureLog;
    // Insert at end of the failure log LLs
    if (pTemp)
    {
        while (pTemp->pNext)
        {
            pTemp = pTemp->pNext;
        }
        pTemp->pNext = pFailureNew;
        pFailureNew->pPrev = pTemp;
    }
    // First node in the failure log LLs
    else
    {
        *pFailureLog = pFailureNew;
    }

    goto clean;
fail:
    if (pFailureNew->pSuiteName)
        NvOsFree(pFailureNew->pSuiteName);
    if (pFailureNew->pTestName)
        NvOsFree(pFailureNew->pTestName);
    if (pFailureNew->pCondition)
        NvOsFree(pFailureNew->pCondition);
    if (pFailureNew->pFileName)
        NvOsFree(pFailureNew->pFileName);
    if (pFailureNew->pFunctionName)
        NvOsFree(pFailureNew->pFunctionName);
    if (pFailureNew)
        NvOsFree(pFailureNew);
    if(err_str)
        NvOsDebugPrintf( "%s NvError 0x%x\n", err_str , e);
clean:
    return e;
}

NvBool NvBDKTest_TestAssert(NvU32 value,
                            const char* pCondition,
                            const char* pTestName,
                            const char* pSuiteName,
                            const char* pFileName,
                            const char* pFunctionName,
                            NvU32 lineNum)
{
    // Update no of asserts tested in the TestEntry runner log
    s_pTestEntryRunnerLog->nAssertsTested++;

    if (value == TEST_BOOL_FALSE)
    {
        s_pTestEntryRunnerLog->nAssertsFailed++;
         NvBDKTest_LogFailure(&s_pTestFailureLog, &s_pTestEntryRunnerLog,
                              pSuiteName, pTestName, pCondition,
                              pFileName, pFunctionName, lineNum);
         return NV_FALSE;
    }
    return NV_TRUE;
}
