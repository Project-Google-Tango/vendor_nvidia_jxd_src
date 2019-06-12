/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* NvxScheduler.h */

#ifndef NVXSCHEDULER_H
#define NVXSCHEDULER_H

/* Scheduler abstraction to allow for components to run either single threaded or multithreaded */

#include <OMX_Core.h>
#include <NVOMX_IndexExtensions.h>

#include  <common/NvxError.h>

/** a worker object */
typedef struct SNvxWorker NvxWorker;
struct SNvxWorker {
    void *pWorkerData;           /**< pointer reserved for use by worker */
    void *pSchedulerPtrs[8];     /**< set of pointers reserved for use by scheduler */
    OMX_U32 pSchedulerData[4];   /**< data area reserved for use by scheduler */
};

/** Typedef for error handler */
typedef OMX_ERRORTYPE (*NvxErrorHandler)(OMX_IN NvxWorker *, OMX_IN OMX_ERRORTYPE);

/** Typedef for worker functions
    @param[in] pWorker      is the worker structure for this worker.  You can get the worker data from that.
    @param[in] uMaxTime     is the maximum amount of time the worker is allotted.  Workers can use this to guage how much work to do.
    @param[out] pbMoreWork  set by worker to true if there is more work to do
    @returns OMX_ErrrorNone   if successful
    @returns NVX_ErrorInsufficientTime if uMaxTime is not sufficient for any work to be done
    @returns other OMX_Error  if an error should be reported via the OMX event mechanism
 */
typedef OMX_ERRORTYPE (*NvxWorkerFunc)(OMX_IN NvxWorker* pWorker, OMX_IN NvxTimeMs uMaxTime, OMX_INOUT OMX_BOOL* pbMoreWork, OMX_INOUT NvxTimeMs* puMaxMsecToNextCall);

/** Initialize the scheduler, if not already initialized.  This will
    be called automatically by worker functions on initialized
    workers.
    @returns OMX_ErrorInsufficientResources if it can't acquire memory or GFSDK handles
 */
OMX_ERRORTYPE NvxSchedulerInit(void);

/** Return true in pbIsMultithreaded if scheduler is using a multithreaded implemeentation.
    @returns OMX_ErrorInvalidState if the scheduler has been shutdown ore never initialized
 */
OMX_ERRORTYPE NvxSchedulerIsMultithreaded(OMX_OUT OMX_BOOL *pbIsMultithreaded);

/** Tell the scheduler to shutdown.  Stops all workers.
    @returns OMX_ErrorTimeout if timed out while waiting
 */
OMX_ERRORTYPE NvxSchedulerShutdown(OMX_IN NvxTimeMs uTimeOut);

/** Tell the scheduler to run workers for a while.  If there is no
    work to be done, waits at least uMinWait.

    @param[out] pbMoreWork set to true if there is more work to be done
                           by the scheduler
    @param[in] uTimeLimit desired amount of time to spend servicing
                          workers in a single threaded environment
    @param[out] puTimeSpent amount of time to spent servicing workers
    @param[out] puTimeUntilNextCall the amount of time until the scheduler
                                    needs to run again, if pbMoreWork is false
                          
    @returns OMX_ErrorInsufficientResources if uMaxTime is not sufficient for any work to be done */
OMX_ERRORTYPE NvxSchedulerRun(OMX_OUT OMX_BOOL* pbMoreWork, OMX_IN NvxTimeMs uTimeLimit, OMX_OUT NvxTimeMs *puTimeSpent, OMX_OUT NvxTimeMs *puTimeUntilNextCall );

/** Initialize a worker, add it to the scheduler, and start it running */
OMX_ERRORTYPE NvxWorkerInit(OMX_OUT NvxWorker* pWorker, NvxWorkerFunc pFunc, void *pWorkerData, NvxErrorHandler pErrorHandler);

/** Tell worker to stop running, wait for it to shutdown, and 
    remove it from scheduler.
    @param[in] pWorker  the worker
    @param[in] uTimeOut maximum time to wait for worker to stop

    @returns OMX_ErrorIncorrectStateOperation if already stopped
    @returns OMX_ErrorTimeout if timed out while waiting for a non-zero timeout */
OMX_ERRORTYPE NvxWorkerDeinit(OMX_IN NvxWorker* pWorker, OMX_IN NvxTimeMs uTimeOut);

/** Trigger worker to run.  A paused worker will not run again until
    unpaused.  In a multithreaded environment, the worker may start
    running immmediatly. but in a single threaded environment it won't
    run again until the NvsScheduler gives it a time slot while
    executing NvsSchedulerRun.

    @param[in] pWorker  the worker to trigger */
OMX_ERRORTYPE NvxWorkerTrigger(OMX_IN NvxWorker* pWorker);

/** In single threaded mode only, run the specified worker

    @param[in] pWorker  the worker to run */
OMX_ERRORTYPE NvxWorkerRun(OMX_IN NvxWorker* pWorker);

#endif /* NVXSCHEDULER_H */

