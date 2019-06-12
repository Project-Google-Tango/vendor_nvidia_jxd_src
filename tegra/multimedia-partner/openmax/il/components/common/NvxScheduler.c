/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "common/NvxScheduler.h"
#include "common/NvxMutex.h"
#include <string.h>
#include <stdlib.h>
#include "NvxTrace.h"

#if defined(LINUX)
#include <sys/time.h>
#endif

#include "nvos.h"

typedef struct SNvxWorkerInt NvxWorkerInt;
struct SNvxWorkerInt {
    void *pWorkerData;           /**< pointer reserved for use by worker */
    union {
        struct {
            NvxWorkerInt  *pPrev;
            NvxWorkerInt  *pNext;
            NvxWorkerFunc pFunc;
            NvxErrorHandler pError;
            NvOsThreadHandle hThread;
            NvOsSemaphoreHandle hTriggerEvent;
            NvOsSemaphoreHandle hSchedulerEvent;
        } i;
        void *pSchedulerPtrs[8];     /**< set of pointers reserved for use by scheduler */
    } p;
    union {
        struct {
            OMX_U8 bTriggered;
            OMX_U8 bPaused;
            OMX_U8 bStopping;
            OMX_U8 bStopped;
            OMX_U32 uTickLastRun;
            OMX_U32 uTickNextRun;
        } i;
        OMX_U32 pSchedulerData[4];   /**< data area reserved for use by scheduler */
    } state;
};

static OMX_HANDLETYPE s_hSchedMutex = 0;
static struct SNvxWorkerInt s_oHead = { 0, {{0}}, {{0}} };

//static void atexit_handler(void)
//{
//    NvxSchedulerShutdown(100);
//}

OMX_ERRORTYPE NvxSchedulerInit(void)
{
    OMX_ERRORTYPE e = OMX_ErrorNone;
    if (s_oHead.p.i.pNext != NULL)
        return OMX_ErrorNone;

    e = NvxMutexCreate(&s_hSchedMutex);
    if (e == NVX_SuccessNoThreading) {
        s_hSchedMutex = 0;
        e = OMX_ErrorNone;
    }

    NvOsMemset(&s_oHead, 0, sizeof(s_oHead));
    s_oHead.p.i.pPrev = &s_oHead;
    s_oHead.p.i.pNext = &s_oHead;

    //atexit(atexit_handler);
    return e;
}

OMX_ERRORTYPE NvxSchedulerShutdown(OMX_IN NvxTimeMs uTimeOut)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxWorkerInt *pWorkerInt;
    int bMoreWorkers = OMX_TRUE;

    while (bMoreWorkers)
    {
        NvxMutexLock(s_hSchedMutex);
        pWorkerInt = s_oHead.p.i.pNext;
        bMoreWorkers = (pWorkerInt != &s_oHead && pWorkerInt != NULL);
        NvxMutexUnlock(s_hSchedMutex);

        if (bMoreWorkers) {
            NvxCheckError( eError, NvxWorkerDeinit((void *)pWorkerInt, uTimeOut) );
        }
    }

    NvxMutexDestroy(s_hSchedMutex);
    s_hSchedMutex = 0;
    NvOsMemset(&s_oHead, 0, sizeof(s_oHead));

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE NvxRunWorkerFuncOnce(NvxWorkerInt* pWorkerInt, NvxTimeMs uMaxTime, NvxTimeMs* puMaxMsecToNextCall)
{
    OMX_ERRORTYPE e;
    OMX_BOOL bMoreWork;
    NvxWorker* pWorker = (NvxWorker*) pWorkerInt;

    NvxWorkerFunc pFunc = pWorkerInt->p.i.pFunc;
    NvxErrorHandler pError = pWorkerInt->p.i.pError;
    NVXTRACE((NVXT_WORKER,NVXT_SCHEDULER, "NvxRunWorkerFuncOnce"));
    pWorkerInt->state.i.bTriggered = 0;

    bMoreWork = OMX_FALSE;
    if (pFunc && pWorker)
        e = pFunc(pWorker, uMaxTime, &bMoreWork, puMaxMsecToNextCall);
    else
        e = OMX_ErrorUndefined;

    if (NvxIsError(e) && pError)
        e = pError(pWorker, e);

    /* Mask out OMX_ErrorNoMore since we don't want to kill the process/thread
     * if we're just waiting for more buffers to become available */
    if(OMX_ErrorNoMore == e)
        e = OMX_ErrorNone;

    /* Ignore errors for now.. */
#if 0
    /* This doesn't work.  Causes deadlocks.  */
    if (NvxIsError(e))
    {
        NvxWorkerDeinit(pWorker, NVX_TIMEOUT_NEVER);
        return OMX_ErrorUndefined;
    }
#endif

    if (bMoreWork && !pWorkerInt->state.i.bStopping)
        pWorkerInt->state.i.bTriggered = 1;

    return OMX_ErrorNone;
}


static NvU32 NvxRunWorkerFunc( void * pWorker)
{
    NvxWorkerInt* pWorkerInt = (NvxWorkerInt *)pWorker;
    NvxTimeMs uMaxMsecToNextCall = NVX_TIMEOUT_NEVER;
    NvOsSemaphoreHandle hSchedulerEvent = pWorkerInt->p.i.hSchedulerEvent;

    NvOsSemaphoreSignal(hSchedulerEvent);

    NVXTRACE((NVXT_WORKER,NVXT_SCHEDULER, "NvxRunWorkerFunc"));

    while (1)
    {
        if (pWorkerInt->state.i.bStopping)
            break;

        if (!pWorkerInt->state.i.bPaused && pWorkerInt->state.i.bTriggered)
        {
            if (NvxIsError(NvxRunWorkerFuncOnce(pWorkerInt, NVX_TIMEOUT_NEVER, &uMaxMsecToNextCall)))
                break;
        }

       if (!pWorkerInt->state.i.bTriggered)
       {
            if (uMaxMsecToNextCall == 0)
                uMaxMsecToNextCall = NV_WAIT_INFINITE;

            NvOsSemaphoreWaitTimeout(pWorkerInt->p.i.hTriggerEvent, uMaxMsecToNextCall);
            if (!pWorkerInt->state.i.bStopped)
               pWorkerInt->state.i.bTriggered = 1;
       }
    }

    pWorkerInt->state.i.bStopped = 1;
    hSchedulerEvent = pWorkerInt->p.i.hSchedulerEvent;
    NvOsSemaphoreSignal(hSchedulerEvent);

    return 0;
}


/* NvxCCGetWallTime - get the current wall time */
static OMX_ERRORTYPE NvxSchedulerGetWallTime(NvxTimeMs *pMs)
{
    *pMs = NvOsGetTimeMS();
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxSchedulerRun(OMX_OUT OMX_BOOL* pbMoreWork, OMX_IN NvxTimeMs uTimeLimit, OMX_OUT NvxTimeMs *puTimeSpent, OMX_OUT NvxTimeMs *puTimeUntilNextCall )
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxWorkerInt *pWorkerInt;
    NvxTimeMs uStartTime, uMaxTime;
    NvxTimeMs uEndLastCall;
    OMX_BOOL bMoreWorkLastIter = OMX_TRUE;

    if (s_oHead.p.i.pNext == NULL)
        return OMX_ErrorInvalidState;
    if (s_hSchedMutex != NULL)
        return OMX_ErrorNotImplemented;  /* this call is not implemented for MT environment (should only be called in ST environment) */

    NvxMutexLock(s_hSchedMutex);

    /* TODO: real scheduling algorithm (current one just runs things in order, starting from the first) */

    *pbMoreWork = OMX_FALSE;
    *puTimeUntilNextCall = NVX_TIMEOUT_NEVER;

    NvxSchedulerGetWallTime(&uStartTime);
    uEndLastCall = uStartTime;
    if (uTimeLimit > 0)
        uMaxTime = uStartTime + uTimeLimit;
    else
        uMaxTime = uStartTime + uTimeLimit + 1;

    while (bMoreWorkLastIter && uEndLastCall < uMaxTime) {
        bMoreWorkLastIter = *pbMoreWork = OMX_FALSE;
        pWorkerInt = s_oHead.p.i.pNext;
        while (pWorkerInt != &s_oHead && pWorkerInt) {
            if (pWorkerInt->state.i.uTickNextRun < uEndLastCall)
                pWorkerInt->state.i.bTriggered = OMX_TRUE;
            pWorkerInt = pWorkerInt->p.i.pNext;
        }

        pWorkerInt = s_oHead.p.i.pNext;
        while (pWorkerInt != &s_oHead && pWorkerInt && uEndLastCall < uMaxTime)
        {
            NvxWorkerInt *pNext = pWorkerInt->p.i.pNext;
            if (!pWorkerInt->state.i.bStopped) {
                OMX_BOOL bRunWorker;
                NvxTimeMs uMaxMsecToNextCall = NVX_TIMEOUT_NEVER;
                if (pWorkerInt->state.i.bStopping) {
                    pWorkerInt->state.i.bStopped = 1;
                    continue;
                }

                bRunWorker = (!pWorkerInt->state.i.bPaused && pWorkerInt->state.i.bTriggered);
                if (bRunWorker)
                    bMoreWorkLastIter = *pbMoreWork = OMX_TRUE;

                NvxMutexUnlock(s_hSchedMutex);
                if (bRunWorker) {
                    NvxCheckError( eError, NvxRunWorkerFuncOnce(pWorkerInt, uMaxTime - uEndLastCall, &uMaxMsecToNextCall) );
                    NvxSchedulerGetWallTime(&uEndLastCall);
                }

                NvxMutexLock(s_hSchedMutex);
                pWorkerInt->state.i.uTickLastRun = uEndLastCall;
                if (bRunWorker) {
                    if (uMaxMsecToNextCall == NVX_TIMEOUT_NEVER) {
                        pWorkerInt->state.i.uTickNextRun = NVX_TIMEOUT_NEVER;
                    }
                    else {
                        pWorkerInt->state.i.uTickNextRun = uEndLastCall+uMaxMsecToNextCall;
                    }
                }
                if (pWorkerInt->state.i.uTickNextRun !=  NVX_TIMEOUT_NEVER)
                {
                    if (pWorkerInt->state.i.uTickNextRun < *puTimeUntilNextCall)
                        *puTimeUntilNextCall = pWorkerInt->state.i.uTickNextRun;
                    /* still more work for scheduler to do, its just not immediate */
                }
            }
            pWorkerInt = pNext;
        }
    }

    *puTimeSpent = uEndLastCall - uStartTime;
    if (*puTimeUntilNextCall != NVX_TIMEOUT_NEVER)
    {
        if (*puTimeUntilNextCall < uEndLastCall)
        {
            *puTimeUntilNextCall = NVX_TIMEOUT_NEVER;
            *pbMoreWork = OMX_TRUE;
        }
        else
            *puTimeUntilNextCall -= uEndLastCall;
   }

    NvxMutexUnlock(s_hSchedMutex);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxWorkerRun(OMX_IN NvxWorker* pWorker)
{
    NvxWorkerInt* pWorkerInt = (NvxWorkerInt *)pWorker;
    NvxTimeMs uMaxMsec;

    if (pWorker == NULL)
        return OMX_ErrorBadParameter;
    NVXTRACE((NVXT_WORKER,NVXT_SCHEDULER, "NvxWorkerRun"));

    if (s_hSchedMutex != NULL)
        return OMX_ErrorNotImplemented;  /* this call is not implemented for MT environment (should only be called in ST environment) */

    return NvxRunWorkerFuncOnce(pWorkerInt, 10, &uMaxMsec);
}

OMX_ERRORTYPE NvxSchedulerIsMultithreaded(OMX_OUT OMX_BOOL *pbIsMultithreaded)
{
    if (s_oHead.p.i.pNext == NULL)
        return OMX_ErrorInvalidState;
    *pbIsMultithreaded = (s_hSchedMutex != NULL);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE NvxWorkerInit(OMX_OUT NvxWorker* pWorker, NvxWorkerFunc pFunc, void *pWorkerData, NvxErrorHandler pErrorHandler)
{
    NvError e = NvSuccess;
    NvxWorkerInt* pWorkerInt = (NvxWorkerInt *)pWorker;

    if (pWorker == NULL || pFunc == NULL)
        return OMX_ErrorBadParameter;
    NVXTRACE((NVXT_WORKER,NVXT_SCHEDULER, "NvxWorkerInit"));
    if (s_oHead.p.i.pNext == NULL)
        NvxSchedulerInit();

    NvOsMemset(pWorker, 0, sizeof(*pWorker));

    NvxMutexLock(s_hSchedMutex);
    pWorker->pWorkerData = pWorkerData;
    pWorkerInt->p.i.pPrev = s_oHead.p.i.pPrev; // when complete, this worker will be the last in the chain before the head
    pWorkerInt->p.i.pNext = &s_oHead;
    pWorkerInt->p.i.pFunc = pFunc;
    pWorkerInt->p.i.pError = pErrorHandler;
    pWorkerInt->state.i.bTriggered = 1;

    if (s_hSchedMutex != NULL) {
        e = NvOsSemaphoreCreate(&pWorkerInt->p.i.hTriggerEvent, 0);
        
        if (e == NvSuccess)
            e = NvOsSemaphoreCreate(&pWorkerInt->p.i.hSchedulerEvent, 0);

        if (e == NvSuccess)
        {
            e = NvOsThreadCreate((NvOsThreadFunction)NvxRunWorkerFunc, 
                                 pWorker, &pWorkerInt->p.i.hThread);
        }
    }

    if (e == NvSuccess) {
        // insert into doubly link list of workers
        pWorkerInt->p.i.pPrev->p.i.pNext = pWorkerInt;
        pWorkerInt->p.i.pNext->p.i.pPrev = pWorkerInt;
    }

    if (s_hSchedMutex != NULL) {
        if (e == NvSuccess)
            NvOsSemaphoreWait(pWorkerInt->p.i.hSchedulerEvent);
    }

    NvxMutexUnlock(s_hSchedMutex);
    return (e != NvSuccess) ? OMX_ErrorInsufficientResources : OMX_ErrorNone;
}

OMX_ERRORTYPE NvxWorkerDeinit(OMX_IN NvxWorker* pWorker, OMX_IN NvxTimeMs uTimeOut)
{
    NvError e = NvSuccess;
    NvxWorkerInt* pWorkerInt = 0;

    NVXTRACE((NVXT_WORKER,NVXT_SCHEDULER, "NvxWorkerDeinit"));

    if (uTimeOut == 0)
        uTimeOut = NV_WAIT_INFINITE;

    if (pWorker == NULL)
        return OMX_ErrorBadParameter;

    pWorkerInt = (NvxWorkerInt *)pWorker;

    NvxMutexLock(s_hSchedMutex);

    if (pWorkerInt->p.i.pNext == NULL || pWorkerInt->p.i.pPrev == NULL || pWorkerInt->state.i.bStopped)
    {
        NvxMutexUnlock(s_hSchedMutex);
        return OMX_ErrorIncorrectStateOperation;
    }

    pWorkerInt->state.i.bStopping = 1;

    if (s_hSchedMutex != NULL)
    {
        NvOsSemaphoreHandle hTriggerEvent = pWorkerInt->p.i.hTriggerEvent;
        NvOsSemaphoreHandle hSchedulerEvent = pWorkerInt->p.i.hSchedulerEvent;
        int bStopped = pWorkerInt->state.i.bStopped;
        if (!bStopped)
            pWorkerInt->state.i.bStopping = 1;
        e = NvSuccess;
        while (e == NvSuccess && !bStopped) {
            if (e == NvSuccess) {
                NvOsSemaphoreSignal(hTriggerEvent);
                e = NvSuccess;
            }
            if (e == NvSuccess) {
                e = NvOsSemaphoreWaitTimeout(hSchedulerEvent, uTimeOut);
                bStopped = pWorkerInt->state.i.bStopped;
                if (!bStopped && e == NvSuccess) {
                    e = NvSuccess;
                    pWorkerInt->state.i.bStopping = 1;
                }
            }
        }

        if (!bStopped && e == NvError_Timeout) {
            bStopped = pWorkerInt->state.i.bStopped;
            if (!bStopped)
                return OMX_ErrorTimeout;
        }

        NvOsThreadJoin(pWorkerInt->p.i.hThread);
        pWorkerInt->p.i.hThread = NULL;

        NvOsSemaphoreDestroy(pWorkerInt->p.i.hTriggerEvent);
        NvOsSemaphoreDestroy(pWorkerInt->p.i.hSchedulerEvent);
        pWorkerInt->p.i.hTriggerEvent = NULL;
        pWorkerInt->p.i.hSchedulerEvent = NULL;
    }

    NvxMutexUnlock(s_hSchedMutex);

    if (pWorkerInt->p.i.pPrev && pWorkerInt->p.i.pNext) {
        pWorkerInt->p.i.pNext->p.i.pPrev = pWorkerInt->p.i.pPrev;
        pWorkerInt->p.i.pPrev->p.i.pNext = pWorkerInt->p.i.pNext;
        pWorkerInt->p.i.pNext = NULL;
        pWorkerInt->p.i.pPrev = NULL;
    }

    pWorkerInt->p.i.hTriggerEvent = 0;
    pWorkerInt->p.i.hSchedulerEvent = 0;
    pWorkerInt->state.i.bTriggered = 0;
    pWorkerInt->state.i.bPaused  = 1;
    pWorkerInt->state.i.uTickLastRun = ~0;

    return (e != NvSuccess) ? OMX_ErrorUndefined : OMX_ErrorNone;
}

/* TODO: move Pause/Resume to public header if we decide we want them.
   Also, reenable code in schedulerTest.c that tests them if we do that.
 */

/** Pause worker (blocks until paused).
    @param[in] pWorker  the worker
    @param[in] uTimeOut maximum time to wait for pause

    @returns OMX_ErrorIncorrectStateOperation if already paused
    @returns OMX_ErrorTimeout if timed out while waiting for a non-zero timeout */
OMX_ERRORTYPE NvxWorkerPause(OMX_IN NvxWorker* pWorker, OMX_IN NvxTimeMs uTimeOut);

/** Resume running worker after a pause (blocks until resumed).
    @param[in] pWorker  the worker
    @param[in] uTimeOut maximum time to wait for
    @returns OMX_ErrorIncorrectStateOperation if not paused
    @returns OMX_ErrorTimeout if timed out while waiting for a non-zero timeout */
OMX_ERRORTYPE NvxWorkerResume(OMX_IN NvxWorker* pWorker, OMX_IN NvxTimeMs uTimeOut);


OMX_ERRORTYPE NvxWorkerPause(OMX_IN NvxWorker* pWorker, OMX_IN NvxTimeMs uTimeOut)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxWorkerInt* pWorkerInt = (NvxWorkerInt *)pWorker;
    NVXTRACE((NVXT_WORKER,NVXT_SCHEDULER, "NvxWorkerPause"));
    if (pWorker == NULL)
        return OMX_ErrorBadParameter;

    if (pWorkerInt->state.i.bPaused)
        eError = OMX_ErrorIncorrectStateOperation;
    else
        pWorkerInt->state.i.bPaused = 1;

    return eError;
}

OMX_ERRORTYPE NvxWorkerResume(OMX_IN NvxWorker* pWorker, OMX_IN NvxTimeMs uTimeOut)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    NvxWorkerInt* pWorkerInt = (NvxWorkerInt *)pWorker;
    NVXTRACE((NVXT_WORKER,NVXT_SCHEDULER, "NvxWorkerResume"));
    if (pWorker == NULL)
        return OMX_ErrorBadParameter;

    if (!pWorkerInt->state.i.bPaused)
        eError = OMX_ErrorIncorrectStateOperation;
    else
        pWorkerInt->state.i.bPaused = 0;

    if (NvxIsSuccess(eError)) {
        NvOsSemaphoreSignal(pWorkerInt->p.i.hTriggerEvent);
        eError = OMX_ErrorNone;
    }
    return eError;
}

OMX_ERRORTYPE NvxWorkerTrigger(OMX_IN NvxWorker* pWorker)
{
    NvxWorkerInt* pWorkerInt = (NvxWorkerInt *)pWorker;
    NVXTRACE((NVXT_WORKER,NVXT_SCHEDULER, "NvxWorkerTrigger"));
    if (pWorker == NULL)
        return OMX_ErrorBadParameter;

    if (!pWorkerInt->state.i.bStopped)
        pWorkerInt->state.i.bTriggered = 1;
    else
        return OMX_ErrorIncorrectStateOperation;

    if (pWorkerInt->p.i.hTriggerEvent == NULL)
        return OMX_ErrorNone;

    NvOsSemaphoreSignal(pWorkerInt->p.i.hTriggerEvent);
    return OMX_ErrorNone;
}

