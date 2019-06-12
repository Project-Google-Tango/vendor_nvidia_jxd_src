/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVMU_HANDLE_H__
#define __NVMU_HANDLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "nvmu_types.h"
#include "nvmu_utils.h"

/**
 * \brief Opaque handle type for a handle
 */
typedef void* NvMU_Handle;

/**
 * \brief Opaque handle type for a handle pool
 */
typedef void* NvMU_HandlePool;

/**
 * \brief Empty pool callback
 *
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnPoolEmpty)(void *pApp);

/**
 * \brief Full pool callback
 *
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnPoolFull)(void *pApp);

/**
 * \brief Has space callback
 *
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnPoolHasSpace)(void *pApp);

/**
 * \brief Has Entries callback
 *
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnPoolHasEntries)(void *pApp);

/**
 * \brief lock handle callback
 *
 * \param[in] pBuffer Pointer to buffer that needs to be locked
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnLockHandle)(void *pBuffer, void *pApp);

/**
 * \brief unlock handle callback
 *
 * \param[in] pBuffer Pointer to buffer that needs to be unlocked
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnUnlockHandle)(void *pBuffer, void *pApp);

/**
 * \brief destroy handle callback
 *
 * \param[in] pBuffer Pointer to buffer that needs to be destroyed
 * \param[in] pApp Application handle
 */
typedef NvMU_Status
(*OnDestroyHandle)(void *pBuffer, void *pApp);

/**
 * \breif Pool Config
 */
typedef struct {
    /*! Number of items pool can store */
    U32 uPoolSize;

    /*! Emtpy pool callback */
    OnPoolEmpty hOnEmpty;
    /*! Full Pool callback */
    OnPoolFull hOnFull;
    /*! Has Entries callback */
    OnPoolHasEntries hOnHasEntries;
    /*! Has space callback */
    OnPoolHasSpace hOnHasSpace;

    /*! lock handle */
    OnLockHandle hLock;
    /*! unlock handle */
    OnUnlockHandle hUnlock;
    /*! destroy handle */
    OnDestroyHandle hDestroy;

    /*! Application handle */
    void *pApp;
} NvMU_HandlePoolConfig;

/**
 * \brief Creates a handle pool instance
 *
 * \param[out] phPool pointer to pool handle
 * \param[in]  pConfig pointer to the handle pool config
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_HandlePoolCreate(NvMU_HandlePool *phPool, NvMU_HandlePoolConfig *pConfig);

/**
 * \brief Destroys the handle pool instance
 *
 * \param[in] phPool pointer to pool handle
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_HandlePoolDestroy(NvMU_HandlePool *phPool);

/**
 * \brief Add a buffer to pool
 *
 * \param[in] hPool pool handle
 * \param[in] pBuffer void buffer pointer to add to pool
 * \param[out] phHandle pointer to handle encapsulating this buffer
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_HandlePoolAdd(NvMU_HandlePool hPool, void *pBuffer, NvMU_Handle *phHandle);

/**
 * \brief Get a handle from buffer pool
 *
 * \param[in] hPool pool handle
 * \param[out] phHandle pointer to handle
 * \param[in] uTimeoutMs Timeout to wait for a handle to be available
 * \return \ref NVMU_STATUS_OK if successful
 * \return \ref NVMU_STATUS_TIMED_OUT if operation timed out
 */
NvMU_Status
NvMU_HandlePoolGet(NvMU_HandlePool hPool, NvMU_Handle *phHandle,
                   U32 uTimeoutMs);

/**
 * \brief Get buffer pointer pointed to by handle
 *
 * \param[in] Handle Buffer handle
 * \param[out] ppBuffer pointer to the buffer
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_HandleGetBuffer(NvMU_Handle Handle, void **ppBuffer);

/**
 * \brief Increment reference count for handle
 *
 * \param[in] Handle handle for which reference needs to be incremented
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_HandleAddRef(NvMU_Handle Handle);

/**
 * \brief Decrement reference count for handle
 *
 * \param[in] Handle handle for which reference needs to be decremented
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_HandleDecRef(NvMU_Handle Handle);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* __NVMU_HANDLE_H__ */
