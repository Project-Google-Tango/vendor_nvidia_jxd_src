/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVMU_QUEUE_H__
#define __NVMU_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "nvmu_types.h"
#include "nvmu_utils.h"

/**
 * \brief Opaque handle type for a Queue
 */
typedef void* NvMU_Queue;

/**
 * \brief Emtpy queue callback
 *
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnQueueEmpty)(void *pApp);

/**
 * \brief Full queue callback
 *
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnQueueFull)(void *pApp);

/**
 * \brief Has space callback
 *
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnQueueHasSpace)(void *pApp);

/**
 * \brief Has Entries callback
 *
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnQueueHasEntries)(void *pApp);

/**
 * \brief Queue Config
 */
typedef struct {
    /*! Size of individual items in queue */
    U32 uItemSize;
    /*! Number of items queue can store */
    U32 uQueueSize;
    /*! Empty queue callback */
    OnQueueEmpty hOnEmpty;
    /*! Full queue callback */
    OnQueueFull hOnFull;
    /*! On space callback */
    OnQueueHasSpace hOnHasSpace;
    /*! Has entries callback */
    OnQueueHasEntries hOnHasEntries;
    /*! Application handle */
    void *pApp;
} NvMU_QueueConfig;

/**
 * \brief Creates a queue instance
 *
 * \param[out] phQueue pointer to queue handle
 * \param[in] pQueueConfig pointer to queue config
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_QueueCreate(NvMU_Queue *phQueue, NvMU_QueueConfig *pQueueConfig);

/**
 * \brief Destroys queue instance
 *
 * \param[in] phQueue pointer to queue handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_QueueDestroy(NvMU_Queue *phQueue);

/**
 * \brief Checks if queue is full
 *
 * \param[in]  hQueue Queue handle
 * \param[out] pIsFull pointer to a boolean indicating the status of queue
 *             NVMU_TRUE = Queue is full
 *             NVMU_FALSE = Queue is not full
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_QueueIsFull(NvMU_Queue hQueue, Bool *pIsFull);

/**
 * \brief Checks if queue is empty
 *
 * \param[in]  hQueue Queue handle
 * \param[out] pIsEmpty pointer to a boolean indicating the status of queue
 *             NVMU_TRUE = Queue is empty
 *             NVMU_FALSE = Queue is not empty
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_QueueIsEmpty(NvMU_Queue hQueue, Bool *pIsEmpty);

/**
 * \brief Get/Pop element from Queue
 *
 * \param[in]  hQueue Queue handle
 * \param[out] pItem pointer to element to be popped from queue
 * \param[out] pItemSize pointer to the size of element popped from queue
 * \param[in]  uTimeoutMs timeout for this operation
 *             This can be set to \ref NVMU_TIMEOUT_INFINITE to block forever.
 * \return \ref NVMU_STATUS_OK if successful
 * \return \ref NVMU_STATUS_TIMED_OUT if operation timed out
 */
NvMU_Status
NvMU_QueueGet(NvMU_Queue hQueue, void *pItem, U32 *pItemSize, U32 uTimeoutMs);

/**
 * \brief Put/Push element to Queue
 *
 * \param[in] hQueue Queue handle
 * \param[in] pItem pointer to element to be pushed to queue
 * \param[in] uItemSize size of item to be pushed to queue
 * \param[in] uTimeoutMs timeout for this operation
 *             This can be set to \ref NVMU_TIMEOUT_INFINITE to block forever.
 * \return \ref NVMU_STATUS_OK if successful
 * \return \ref NVMU_STATUS_TIMED_OUT if operation timed out
 */
NvMU_Status
NvMU_QueuePut(NvMU_Queue hQueue, void *pItem, U32 uItemSize, U32 uTimeoutMs);

/**
 * \brief Get the size/depth of Queue. Number of elements a queue can contain
 *
 * \param[in]  hQueue Queue handle
 * \param[out] puSize pointer to variable containing the size of queue
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_QueueGetSize(NvMU_Queue hQueue, U32 *puSize);

/**
 * \brief Peek element from Queue. Element continues to live in Queue.
 *
 * \param[in]  hQueue Queue handle
 * \param[out] pItem pointer to element peeked from queue
 * \param[out] puItems pointer to variable indicating the current size of queue
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_QueuePeek(NvMU_Queue hQueue, void *pItem, U32 *pItemSize, U32 *puItems);

/**
 * \brief Abort Queue. Following this all Queue Put operations fail, but
 *        Queue Get operations are successfull.
 *
 * \param[in]  hQueue Queue handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_QueueAbort(NvMU_Queue hQueue);

/**
 * \brief Resume Queue. Undoes Queue Abort.
 *
 * \param[in]  hQueue Queue handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_QueueResume(NvMU_Queue hQueue);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* __NVMU_QUEUE_H__ */
