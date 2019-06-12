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
#include "testsources.h"
#include "nvsystem_utils.h"

// Global test registry variable
NvBDKTest_pRegistration s_pReg = NULL;

/* Query Functions */
/**
 * Checks if a test already exists in the default registration list
 */
static NvBool NvBDKTest_TestExists(NvBDKTest_pSuite pSuite,
                                   const char* pStrName);

/**
 * Checks if a suite already exists in the default registration list
 */
static NvBool NvBDKTest_SuiteExists(NvBDKTest_pRegistration pReg,
                                    const char* pStrName);

/**
 * Creates a test node with the input test data/details
 */
static NvError NvBDKTest_CreateTest(NvBDKTest_pTest* pRetValue,
                                    const char* pStrName,
                                    NvBDKTest_TestFunc pTestFunc,
                                    const char* type,
                                    void *pPrivateData,
                                    NvBDKTest_FreePrivateFunc pFreeFunc);

/**
 * Creates a suite node with the input test data/details
 */
static NvError NvBDKTest_CreateSuite(const char* pStrName,
                                     NvBDKTest_pSuite* pRetValue);

/**
 * Inserts a test node into the default suite LL
 */
static void NvBDKTest_InsertTest(NvBDKTest_pSuite pSuite,
                                 NvBDKTest_pTest pTest);

/**
 * Inserts a suite node into the default registry LL
 */
static void NvBDKTest_InsertSuite(NvBDKTest_pRegistration pReg,
                                  NvBDKTest_pSuite pSuite);

/**
 * Frees the supplied test pointer
 */
static NvError NvBDKTest_CleanTest(NvBDKTest_pTest pTest);

/**
 * Frees the supplied suite pointerLOCAL_SRC_FILES += fuse/nvddk_fuse_test.c
 */
static NvError NvBDKTest_CleanSuite(NvBDKTest_pSuite pSuite);

/**
 * Frees the supplied registry pointer
 */
static NvError NvBDKTest_CleanRegistration(NvBDKTest_pRegistration pReg);

/**
 * Returns number of tests in the LL of a particular suite
 */
static NvU32 NvBDKTest_GetSuiteTestNum(NvBDKTest_pSuite pSuite);

/**
 * Returns number of tests of a particular type in a specific suite LL
 */
static NvU32 NvBDKTest_GetSingleTypeTestNum(NvBDKTest_pSuite pSuite,
                                            const char* type);

/**
 * Returns 1 if the test is present in the suite
 */
static NvU32 NvBDKTest_GetSingleNameTestNum(NvBDKTest_pSuite pSuite,
                                            const char* test_name);


/*
 * Creates a new registration with all the suites/tests available
 */
static NvBDKTest_pRegistration NvBDKTest_CreateNewRegistration(void);


NvBDKTest_pSuite NvBDKTest_GetSuitePtr(const char* suiteName)
{
    NvBDKTest_pSuite pSuite = NULL;

    if (suiteName)
    {
        // Getting the suite LL pointer
        pSuite = s_pReg->pSuite;
        while (pSuite != NULL)
        {
            if (!NvOsStrcmp(pSuite->pSuiteName, suiteName))
                break;
            else
                pSuite = pSuite->pNext;
        }
    }
    return pSuite;
}

NvBDKTest_pTest NvBDKTest_GetTestPtr(NvBDKTest_pSuite pSuite,
                                     const char* testName)
{
    NvBDKTest_pTest pTest = NULL;

    if (pSuite || testName)
    {
        // Getting the test LL pointer
        pTest = pSuite->pTest;
        while (pTest != NULL)
        {
            if (!NvSysUtilsIStrcmp(pTest->pTestName, testName))
                break;
            else
                pTest = pTest->pNext;
        }
    }
    return pTest;
}

static NvError NvBDKTest_CleanTest(NvBDKTest_pTest pTest)
{
    char *err_str = 0;
    NvError e = NvSuccess;

    if (pTest->pPrivateData && pTest->pFreePrivateFunc)
    {
        (*pTest->pFreePrivateFunc)(pTest->pPrivateData);
        pTest->pPrivateData = NULL;
    }

    if (pTest->pTestName)
    {
        NvOsFree(pTest->pTestName);
        pTest->pTestName = NULL;
    }
    else
    {
        err_str = "NvBDKTest_CleanTest failed. Test does not exist";
        e = NvError_BDKTestUnknownTest;
        goto fail;
    }
    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_CleanTest failed : NvError 0x%x\n",
                        err_str , e);
clean:
    NvOsFree(pTest);
    pTest = NULL;
    return e;
}

static NvError NvBDKTest_CreateTest(NvBDKTest_pTest* pRetValue,
                                    const char* pStrName,
                                    NvBDKTest_TestFunc pTestFunc,
                                    const char* type,
                                    void *pPrivateData,
                                    NvBDKTest_FreePrivateFunc pFreeFunc)
{
    NvError e = NvSuccess;
    const char* err_str = 0;

    *pRetValue = (NvBDKTest_pTest)NvOsAlloc(sizeof(NvBDKTest_Test));

    if ((*pRetValue) != NULL)
    {
        (*pRetValue)->pTestName = (char *)NvOsAlloc(NvOsStrlen(pStrName)+1);
        if ((*pRetValue)->pTestName == NULL)
        {
            e = NvError_InsufficientMemory;
            err_str = "Insufficient Memory to allocate for TestName";
            goto fail;
        }

        (*pRetValue)->pTestType = (char *)NvOsAlloc(NvOsStrlen(type)+1);
        if ((*pRetValue)->pTestType == NULL)
        {
            e = NvError_InsufficientMemory;
            err_str = "Insufficient Memory to allocate for TestType";
            goto fail;
        }

        if ((*pRetValue)->pTestName != NULL)
        {
            NvOsStrcpy((*pRetValue)->pTestName, pStrName);
            (*pRetValue)->isActive = NV_FALSE;
            (*pRetValue)->pTestFunc = pTestFunc;
            NvOsStrcpy((*pRetValue)->pTestType, type);
            (*pRetValue)->pPrivateData = pPrivateData;
            (*pRetValue)->pFreePrivateFunc = pFreeFunc;
            (*pRetValue)->pNext = NULL;
            (*pRetValue)->pPrev = NULL;
        }
        else
        {
            NvOsFree(*pRetValue);
            *pRetValue = NULL;
        }
    }

    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_CreateTest failed: NvError 0x%x\n",
                        err_str , e);
clean:
    return e;
}

static NvBool NvBDKTest_TestExists(NvBDKTest_pSuite pSuite, const char* pStrName)
{
    NvBDKTest_pTest pTest = NULL;

    // Getting the test LL pointer
    pTest = pSuite->pTest;
    while (pTest != NULL)
    {
        if ((pTest->pTestName != NULL) &&
            !(NvOsStrcmp(pStrName, pTest->pTestName)))
            return NV_TRUE;
       pTest = pTest->pNext;
    }

    return NV_FALSE;
}

static void NvBDKTest_InsertTest(NvBDKTest_pSuite pSuite, NvBDKTest_pTest pTest)
{
    NvBDKTest_pTest pCurTest = NULL;

    pCurTest = pSuite->pTest;
    pTest->pNext = NULL;

    // First test node of the LL
    if (pCurTest == NULL)
    {
        pSuite->pTest = pTest;
        pTest->pPrev = NULL;
    }
    // Add a test node to the end of LL
    else
    {
        while (pCurTest->pNext != NULL)
            pCurTest = pCurTest->pNext;
        pCurTest->pNext = pTest;
        pTest->pPrev = pCurTest;
    }
    pSuite->nTests++;
}

NvError NvBDKTest_AddTest(NvBDKTest_pSuite pSuite,
                          NvBDKTest_pTest* pRetValue,
                          const char* pStrName,
                          NvBDKTest_TestFunc pTestFunc,
                          const char* type,
                          void *pPrivateData,
                          NvBDKTest_FreePrivateFunc pFreeFunc)
{
    NvError e = NvSuccess;
    const char* err_str = 0;

    if (s_pReg == NULL)
    {
        e = NvError_BDKTestNoRegistration;
        err_str = "Test Registry is not initialized";
        goto fail;
    }
    else if (pSuite == NULL)
    {
        e = NvError_BDKTestUnknownSuite;
        err_str = "Invalid suite being registered or run";
        goto fail;
    }
    else if (pStrName == NULL)
    {
        e = NvError_BDKTestUnknownTest;
        err_str = "Invalid test being registered or run";
        goto fail;
    }
    else if (pTestFunc == NULL)
    {
        e = NvError_BDKTestNullTestFuncPtr;
        err_str = "The test function pointer is null";
        goto fail;
    }
    else
    {
        // Check if the test already exists
        if (NvBDKTest_TestExists(pSuite, pStrName) == NV_TRUE)
        {
            e = NvError_BDKTestDupeTest;
            err_str = "Test with the name already exists";
            goto fail;
        }
        else
        {
            // Create a test node
            e = NvBDKTest_CreateTest(pRetValue, pStrName, pTestFunc, type,
                                     pPrivateData, pFreeFunc);
            if (e != NvSuccess)
            {
                err_str = "CreateTest failed.";
                goto fail;
            }
            else
            {
                NvBDKTest_InsertTest(pSuite, *pRetValue);
                s_pReg->nTests++;
            }
        }
    }

    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_AddTest failed: NvError 0x%x\n",
                        err_str , e);
clean:
    return e;
}

NvError NvBDKTest_AddSuite(const char* pStrName,
                           NvBDKTest_pSuite* pRetValue)
{
    NvError e = NvSuccess;
    const char* err_str = 0;

    if (s_pReg == NULL)
    {
        e = NvError_BDKTestNoRegistration;
        err_str = "Test Registry is not initialized";
        goto fail;
    }
    else if (pStrName == NULL)
    {
        e = NvError_BDKTestUnknownSuite;
        err_str = "Invalid suite being registered or run";
        goto fail;
    }
    else
    {
        // Check if it exists
        if (NvBDKTest_SuiteExists(s_pReg, pStrName) == NV_TRUE)
        {
            e = NvError_BDKTestDupeSuite;
            err_str = "Suite with the name already exists";
            goto fail;
        }
        else
        {
            // Create a suite node
            e = NvBDKTest_CreateSuite(pStrName, pRetValue);
            if (e != NvSuccess)
            {
                err_str = "Create suite failed";
                goto fail;
            }
            else
            {
                NvBDKTest_InsertSuite(s_pReg, *pRetValue);
            }
        }
    }

    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_AddSuite failed:vNvError 0x%x\n",
                        err_str , e);
clean:
    return e;
}

static NvError NvBDKTest_CreateSuite(const char* pStrName,
                                     NvBDKTest_pSuite* pRetValue)
{
    NvError e = NvSuccess;
    const char* err_str = 0;

    *pRetValue = (NvBDKTest_pSuite)NvOsAlloc(sizeof(NvBDKTest_Suite));
    if ((*pRetValue) == NULL)
    {
        e = NvError_InsufficientMemory;
        err_str = "Insufficient Memory to allocate for a Suite node";
        goto fail;
    }

    if ((*pRetValue) != NULL)
    {
        (*pRetValue)->pSuiteName = (char *)NvOsAlloc(NvOsStrlen(pStrName)+1);
        if ((*pRetValue)->pSuiteName == NULL)
        {
            e = NvError_InsufficientMemory;
            err_str = "Insufficient Memory to allocate for a SuiteName";
            goto fail;
        }

        if ((*pRetValue)->pSuiteName != NULL)
        {
            NvOsStrcpy((*pRetValue)->pSuiteName, pStrName);
            (*pRetValue)->isActive = NV_FALSE;
            (*pRetValue)->nTests = 0;
            (*pRetValue)->pNext = NULL;
            (*pRetValue)->pPrev = NULL;
            (*pRetValue)->pTest = NULL;
        }
        else
        {
            NvOsFree(*pRetValue);
            *pRetValue = NULL;
        }
    }

    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_CreateSuite failed:vNvError 0x%x\n",
                        err_str , e);
clean:
    return e;

}

static NvBool NvBDKTest_SuiteExists(NvBDKTest_pRegistration pReg,
                                    const char* pStrName)
{
    NvBDKTest_pSuite pSuite = NULL;

    pSuite = pReg->pSuite;
    while (pSuite != NULL)
    {
        if ((pSuite->pSuiteName != NULL) &&
            !(NvOsStrcmp(pStrName, pSuite->pSuiteName)))
            return NV_TRUE;
       pSuite = pSuite->pNext;
    }

    return NV_FALSE;
}

static void NvBDKTest_InsertSuite(NvBDKTest_pRegistration pReg,
                                  NvBDKTest_pSuite pSuite)
{
    NvBDKTest_pSuite pCurSuite = NULL;

    pCurSuite = pReg->pSuite;
    pSuite->pNext = NULL;

    // First node of the LL
    if (pCurSuite == NULL)
    {
        pReg->pSuite = pSuite;
        pSuite->pPrev = NULL;
    }
    // Add a suite node to the end of LL
    else
    {
        while (pCurSuite->pNext != NULL)
            pCurSuite = pCurSuite->pNext;

        pCurSuite->pNext = pSuite;
        pSuite->pPrev = pCurSuite;
    }
    pReg->nSuites++;
}

// Clean all tests in the suite
static NvError NvBDKTest_CleanSuite(NvBDKTest_pSuite pSuite)
{
    NvBDKTest_pTest currTest = NULL, nextTest = NULL;
    NvError e = NvSuccess;
    const char *err_str = 0;;

    if (pSuite != NULL)
    {
        // To clear a suite, we need to clear
        // the memory of the child tests
        currTest = pSuite->pTest;
        while (currTest)
        {
            nextTest = currTest->pNext;
            // Clear current suite
            e = NvBDKTest_CleanTest(currTest);
            if (e != NvSuccess)
            {
                err_str = "NvBDKTest_CleanSuite failed.\n";
                goto fail;
            }
            currTest = nextTest;
        }
        if (pSuite->pSuiteName)
            NvOsFree(pSuite->pSuiteName);
        pSuite->pSuiteName = NULL;
        pSuite->pTest = NULL;
        pSuite->nTests = 0;
        pSuite->isActive = NV_FALSE;
    }

    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_CleanSuite failed : NvError 0x%x\n",
                        err_str , e);
clean:
    NvOsFree(pSuite);
    pSuite = NULL;
    return e;
}

static NvBDKTest_pRegistration NvBDKTest_CreateNewRegistration(void)
{
    NvBDKTest_pRegistration pReg = (NvBDKTest_pRegistration)
                                    NvOsAlloc(sizeof(NvBDKTest_Registration));
    if (pReg != NULL)
    {
        pReg->nSuites = 0;
        pReg->nTests = 0;
        pReg->pSuite = NULL;
    }

    return pReg;
}

static NvError NvBDKTest_CleanRegistration(NvBDKTest_pRegistration pReg)
{
    NvBDKTest_pSuite currSuite = NULL, nextSuite = NULL;
    NvError e = NvSuccess;
    const char *err_str = 0;

    // If Registration exists, clear it
    if (pReg != NULL)
    {
        // To clear a registry, we need to clear
        // the memory of the child suites
        currSuite = pReg->pSuite;
        while (currSuite)
        {
            nextSuite = currSuite->pNext;
            // Clear current suite
            e = NvBDKTest_CleanSuite(currSuite);
            if (e != NvSuccess)
            {
                err_str = "NvBDKTest_CleanRegistration failed.\n";
                goto fail;
            }
            currSuite = nextSuite;
        }
    }

    s_pReg = NULL;
    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_CleanRegistration failed : NvError 0x%x\n",
                        err_str , e);
clean:
    return e;
}

static NvU32 NvBDKTest_GetSuiteTestNum(NvBDKTest_pSuite pSuite)
{
    NvBDKTest_pTest pTest = NULL;
    NvU32 count = 0;

    pTest = pSuite->pTest;
    if (pTest)
    {
        while (pTest != NULL)
        {
            if (pTest->pTestFunc != NULL)
                count++;
            pTest = pTest->pNext;
        }
    }
    return count;
}
static NvU32 NvBDKTest_GetSingleTypeTestNum(NvBDKTest_pSuite pSuite,
                                            const char* type)
{
    NvBDKTest_pTest pTest = NULL;
    NvU32 count = 0;

    pTest = pSuite->pTest;
    if (pTest)
    {
        while (pTest != NULL)
        {
            if (pTest->pTestFunc != NULL
                && !(NvOsStrcmp(pTest->pTestType, type)))
                count++;
            pTest = pTest->pNext;
        }
    }
    return count;
}


static NvU32 NvBDKTest_GetSingleNameTestNum(NvBDKTest_pSuite pSuite,
                                            const char* test_name)
{
    NvBDKTest_pTest pTest = NULL;
    NvU32 count = 0;

    pTest = pSuite->pTest;
    if (pTest)
    {
        while (pTest != NULL)
        {
            if (pTest->pTestFunc != NULL &&
                !(NvSysUtilsIStrcmp(pTest->pTestName, test_name)))
                count++;
            pTest = pTest->pNext;
        }
    }
    return count;
}


NvU32 NvBDKTest_GetNumTests(const char* Suite, const char* Argument)
{
    NvBDKTest_pSuite pSuite = NULL;

    if (!NvOsStrcmp(Argument, "all"))
    {
        pSuite = NvBDKTest_GetSuitePtr(Suite);
        return NvBDKTest_GetSuiteTestNum(pSuite);
    }
    else if (NvOsStrcmp(Argument, "all") &&
        (Argument[0] != 'n' && Argument[1] != 'v'))
    {
        pSuite = NvBDKTest_GetSuitePtr(Suite);
        return NvBDKTest_GetSingleTypeTestNum(pSuite, Argument);
    }
    else if (NvOsStrcmp(Argument, "all") &&
        (Argument[0] == 'n' && Argument[1] == 'v'))
    {
        pSuite = NvBDKTest_GetSuitePtr(Suite);
        return NvBDKTest_GetSingleNameTestNum(pSuite, Argument);
    }
    else
    {
        return 0;
    }
}


NvU8* NvBDKTest_GetSuiteInfo(NvU8 *buffer, NvU32 *size)
{
    NvBDKTest_pSuite pSuit = NULL;
    NvU32 temp = 0;
    NvU8 *temp_buf = NULL;
    temp_buf = buffer;

    pSuit = s_pReg->pSuite;
    if (!buffer)
        return NULL;
    NvOsMemcpy(buffer, (NvU8*)(&s_pReg->nSuites), sizeof(NvU32));
    buffer += sizeof(NvU32);

    while (pSuit)
    {
        temp = NvOsStrlen(pSuit->pSuiteName) + 1;
        *size += temp;

        temp_buf = NvOsRealloc(temp_buf, *size);
        buffer = temp_buf;
        buffer = buffer + (*size - temp);
        NvOsStrcpy((char *)buffer, pSuit->pSuiteName);

        pSuit = pSuit->pNext;

    }
    return temp_buf;
}

NvError NvBDKTest_CreateDefaultRegistration(void)
{
    NvError e = NvSuccess;
    const char *err_str = 0;

    // If Registration exists call clean function
    if (s_pReg != NULL)
    {
         e = NvBDKTest_CleanRegistration(s_pReg);
         if (e != NvSuccess)
            goto fail;
    }
    if (s_pReg )
    {
        e = NvError_BDKTestRegistrationCleanupFailed;
        err_str = "Registration Cleanup Failed";
        goto fail;
    }

    s_pReg = NvBDKTest_CreateNewRegistration();
    if (s_pReg == NULL)
    {
        e = NvError_InsufficientMemory;
        err_str = "Memory could not be allocated for a new registry";
        goto fail;
    }

    // FIXME: Add init functions for all test modules
    // Call register functions for all tests
    //SE
    e = se_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module SE failed";
        goto fail;
    }
    //DSI
    e = dsi_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module DSI failed";
        goto fail;
    }
    //UART
    e = uart_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module UART failed";
        goto fail;
    }
    //I2C
    e = i2c_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module I2C failed";
        goto fail;
    }
    //SD
    e = sd_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module SD failed";
        goto fail;
    }
    //PMU
    e = pmu_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module PMU failed";
        goto fail;
    }
    //Fuse
    e = fuse_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module fuse failed";
        goto fail;
    }

    //PWM
    e = pwm_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module PWM failed";
        goto fail;
    }

    //USBf
    e = usbf_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module USBf failed";
        goto fail;
    }

    //BIF
#ifndef NVBDKTEST_T124
    e = mipibif_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module mipibif failed";
        goto fail;
    }
#endif

    //PCB
    e = pcb_init_reg();
    if (e != NvSuccess)
    {
        err_str = "Init of test module PCB failed";
        goto fail;
    }
    goto clean;
fail:
    if(err_str)
        NvOsDebugPrintf("%s NvBDKTest_CreateDefaultRegistration failed"
                        " : NvError 0x%x\n", err_str , e);
clean:
    return e;
}
