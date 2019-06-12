/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVMU_THREAD_H__
#define __NVMU_THREAD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "nvmu_types.h"
#include "nvmu_utils.h"

#define NVMU_MAX_THREAD_CMD_SIZE (20)

/**
 * \brief Opaque handle type for a Thread
 */
typedef void* NvMU_Thread;

/**
 * \brief Thread Command handler
 */
typedef NvMU_Status
(*NvMU_ThreadCmdHandler)(void*, void*);

/**
 * \brief Thread command structure
 */
typedef struct {
    /*! Command id*/
    U32 uId;
    /*! Returned value */
    NvMU_Status *pRet;
    /*! Sync Command Semaphore */
    NvMU_Semaphore hSem;
    /*! Argument area */
    U8 Args[NVMU_MAX_THREAD_CMD_SIZE];
} NvMU_ThreadCmd;

/**
 * \brief Thread Configuration
 */
typedef struct {
    /*! Thread Name */
    char *pThreadName;
    /*! Thread priority*/
    S32 iPriority;
    /*! Command handlers */
    NvMU_ThreadCmdHandler *pCmdHandlers;
    /*! Number of command handlers */
    U32 uCmdNumHandlers;
    /*! Command queue parameters */
    U32 uCmdQueueLen;
    /*! Thread start state */
    Bool  bSuspendOnStart;
    /*! Diagnostics control */
    char *pDiagPath;
    /*! Looper function */
    NvMU_Status (*pFunc)(void* pParam);
    /*! Arguments to the looper function*/
    void *pParam;
    /*! Application Handle*/
    void *pCtx;
} NvMU_ThreadConfig;


/**
 * \brief Creates a thread instance and starts running the thread
 *
 * \param[out] phThread Pointer to the thread handle
 * \param[in] pThreadConfig Pointer to thread Configuration
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_ThreadCreate(NvMU_Thread *phThread, NvMU_ThreadConfig *pThreadConfig);

/**
 * \brief Gets the priority of the thread
 *
 * \param[in]  hThread Thread handle
 * \param[out] pPriority Pointer to thread priority
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_ThreadPriorityGet(NvMU_Thread hThread, S32 *pPriority);

/**
 * \brief Sets the priority of the thread
 *
 * \param[in] hThread Thread handle
 * \param[in] iPriority Thread priority
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_ThreadPrioritySet(NvMU_Thread hThread, S32 iPriority);

/**
 * \brief Waits for the thread to complete its execution,
 *        joins the thread and frees the resources allocated to the thread
 *        Blocking call.
 *
 * \param[in] phThread Pointer to thread handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_ThreadDestroy(NvMU_Thread *phThread);

/**
 * \brief Yields the execution of the calling thread and allows execution of the
 *        threads scheduled ahead in the scheduler
 *
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_ThreadYield(void);

/**
 * \brief Blocks the calling thread until the called thread is suspended
 *        This is a utility functionality return on top of command queue and
 *        uses \ref NvMU_ThreadQueueCmd internally with Id 0xFF.
 *        This should always be called from one thread.
 *
 * \param[in] hThread Thread handle
 * \param[in] uTimeoutMs Time to wait for in milliseconds for operation to complete
 *            This can be set to \ref NVMU_TIMEOUT_INFINITE to block forever.
 * \return \ref NVMU_STATUS_OK if successful
 * \return \ref NVMU_STATUS_TIMED_OUT if operation timed out
 */
NvMU_Status
NvMU_ThreadSuspend(NvMU_Thread hThread, U32 uTimeoutMs);

/**
 * \brief Trigger thread semaphore
 *
 * \param[in] hThread Thread handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_ThreadTrigger(NvMU_Thread hThread);

/**
 * \brief Resume the suspended thread
 *        This is a utility functionality return on top of command queue and
 *        uses \ref NvMU_ThreadQueueCmd internally with Id 0xFE.
 *        This should always be called from one thread.
 *
 * \param[in] hThread Thread handle
 * \param[in] uTimeoutMs Time to wait for in milliseconds for operation to complete
 *            This can be set to \ref NVMU_TIMEOUT_INFINITE to block forever.
 * \return \ref NVMU_STATUS_OK if successful
 * \return \ref NVMU_STATUS_TIMED_OUT if operation timed out
 */
NvMU_Status
NvMU_ThreadResume(NvMU_Thread hThread, U32 uTimeoutMs);

/**
 * \brief Get the name of thread. Can be used for debugging purposes. Memory is
 *        owned by implementation.
 *
 * \param[in] hThread Thread handle
 * \param[out] ppzThreadName pointer to a char pointer to return thread name
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_ThreadGetName(NvMU_Thread hThread, const char **ppzThreadName);

/**
 * \brief Queue commands to thread
 *
 * \param[in] hThread Thread handle
 * \param[in] pThreadCmd pointer to the thread command
 * \param[in] uTimeoutMs Time to wait for in milliseconds for operation to complete
 *            This can be set to \ref NVMU_TIMEOUT_INFINITE to block forever.
 * \return \ref NVMU_STATUS_OK if successful
 * \return \ref NVMU_STATUS_TIMED_OUT if operation timed out
 */
NvMU_Status
NvMU_ThreadQueueCmd(NvMU_Thread hThread, NvMU_ThreadCmd *pThreadCmd, U32 uTimeoutMs);

/**
 * \brief Set Thread Diagnostics State
 *        eg. SetDiagState(hThread, __func__, __LINE__);
 *
 * \param[in] hThread Thread handle
 * \param[in] pState State of thread
 * \param[in] uValue State value of thread
 */
NvMU_Status
NvMU_ThreadSetDiagState(NvMU_Thread hThread, char *pState, U32 uValue);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* __NVMU_THREAD_H__ */
