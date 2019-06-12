/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVMU_UTILS_H__
#define __NVMU_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "nvmu_types.h"

#ifndef NVMU_TAG
#define NVMU_TAG "nvmu"
#endif

#define NVLOGV(pszFormat, ...)  NvMU_Trace(VERBOSE, __FILE__, __FUNCTION__, __LINE__, NVMU_TAG, pszFormat, ##__VA_ARGS__);
#define NVLOGI(pszFormat, ...)  NvMU_Trace(INFO, __FILE__, __FUNCTION__, __LINE__, NVMU_TAG, pszFormat, ##__VA_ARGS__);
#define NVLOGW(pszFormat, ...)  NvMU_Trace(WARNING, __FILE__, __FUNCTION__, __LINE__, NVMU_TAG, pszFormat, ##__VA_ARGS__);
#define NVLOGE(pszFormat, ...)  NvMU_Trace(ERROR, __FILE__, __FUNCTION__, __LINE__, NVMU_TAG, pszFormat, ##__VA_ARGS__);

/**
 * \brief Opaque handle type for an Event
 */
typedef void* NvMU_Event;

/**
 * \brief Opaque handle type for a Semaphore
 */
typedef void* NvMU_Semaphore;

/**
 * \brief Opaque handle type for a Mutex
 */
typedef void* NvMU_Mutex;

/**
 * \brief Allocate memory
 *
 * \param[in] uSize Memory size to be allocated
 * \return    pointer to the allocated memory if successful
 *            0 if the memory allocation fails or size == 0
 */
void *
NvMU_Malloc(U32 uSize);

/**
 * \brief Allocate memory initialized to 0
 *
 * \param[in] uNum Number of elements to be allocated
 * \param[in] uSize Memory size to be allocated
 * \return    pointer to the allocated memory if successful.
 *            0 if the memory allocation fails or
 *            uSize == 0 or uNum == 0
 */
void *
NvMU_Calloc(U32 uNum, U32 uSize);

/**
 * \brief Free the memory allocated earlier
 *
 * \param[in] ptr pointer to the memory to be freed
 */
void
NvMU_Free(void *ptr);

/**
 * \brief Get the current time
 *
 * \return time with microsecond granularity
 */
U64
NvMU_GetClock(void);

/**
 * \brief Prints trace with various levels of information
 *
 * \return void
 */
void
NvMU_Trace(NvMU_InfoLevel eInfoLevel, const char *pszFile, const char *pszFunction,
           S32 iLine, const char *pTag, const char *pszFormat, ...);

/**
 * \brief Creates an Event instance
 *
 * \param[out] phEvent pointer to the Event Handler
 * \param[in]  bManual triggering mode manual or automatic
 * \param[in]  bSet Initital state set or reset
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_EventCreate(NvMU_Event *phEvent, Bool bManual, Bool bSet);

/**
 * \brief Wait on an Event (Blocking event)
 *
 * \param[in] hEvent Event Handler
 * \param[in] uTimeoutMs Time out in ms
 *            This can be set to \ref NVMU_TIMEOUT_INFINITE to block forever.
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_EventWait(NvMU_Event hEvent, U32 uTimeoutMs);

/**
 * \brief Set the event
 *
 * \param[in] hEvent Event Handler
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_EventSet(NvMU_Event hEvent);

/**
 * \brief Reset the event
 *
 * \param[in] hEvent Event Handler
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_EventReset(NvMU_Event hEvent);

/**
 * \brief Delete an Event instance
 *
 * \param[in] *phEvent pointer to Event Handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_EventDestroy(NvMU_Event *phEvent);

/**
 * \brief Creates a Mutex instance
 *
 * \param[out] phMutex pointer to the mutex Handler
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_MutexCreate(NvMU_Mutex *phMutex);

/**
 * \brief Delete the Mutex instance
 *
 * \param[in] phMutex pointer to Mutex Handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_MutexDestroy(NvMU_Mutex *phMutex);

/**
 * \brief Acquire the Mutex(lock the mutex)
 *
 * \param[in] hMutex Mutex Handler
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_MutexAcquire(NvMU_Mutex hMutex);

/**
 * \brief Release the Mutex(unlock the mutex)
 *
 * \param[in] hMutex Mutex Handler
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_MutexRelease(NvMU_Mutex hMutex);

/**
 * \brief Creates a Semaphore instance
 *
 * \param[out] phSem pointer to the Semaphore Handler
 * \param[in]  uInitCount initial count of the semaphore if created successfully
 * \param[in]  uMaxCount maximum count that the semaphore can allow
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SemaphoreCreate(NvMU_Semaphore *phSem, U32 uInitCount, U32 uMaxCount);

/**
 * \brief Increment the semaphore count(analogous to sem_post)
 *
 * \param[in] hSem Semaphore Handler
 * \param[out] pVal Pointer to current sem value
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SemaphoreIncrement(NvMU_Semaphore hSem, U32 *pVal);

/**
 * \brief Decrement the semaphore count(analogous to sem_timedwait)
 *
 * \param[in] hSem Semaphore Handler
 * \param[out] pVal Pointer to current sem value
 * \param[in] uTimeOutMs time to wait before relinquishing trying further
 *            This can be set to \ref NVMU_TIMEOUT_INFINITE to block forever.
 * \return \ref NVMU_STATUS_OK if successful
 * \return \ref NVMU_STATUS_TIMED_OUT if operation timed out
 */
NvMU_Status
NvMU_SemaphoreDecrement(NvMU_Semaphore hSem, U32 *pVal, U32 uTimeoutMs);

/**
 * \brief Destroys the Semaphore instance
 *
 * \param[in]  phSem pointer to Semaphore Handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_SemaphoreDestroy(NvMU_Semaphore *phSem);

#ifdef __cplusplus
};
#endif

#endif /* __NVMU_UTILS_H__ */
