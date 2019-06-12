/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVMU_SHM_H__
#define __NVMU_SHM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "nvmu_types.h"

/**
 * \brief Shm handle type
 */
typedef void* NvMU_Shm;

/**
 * \brief Shm configuration
 */
typedef struct {
    /*! Does a new instance need to be created (used by server) */
    Bool bCreate;
    /*! Path for shared mem node */
    char *pPath;
    /*! Size of shared mem (input for server, output for client) */
    U32 uShmSize;
    /*! Size of meta information per entry */
    U32 uMetaSize;
} NvMU_ShmConfig;

/**
 * \brief Container type to read/write data
 */
typedef struct {
    /*! Pointer to data */
    void *pData;
    /*! Pointer to meta information */
    void *pMeta;
    /*! Data size */
    U32 uDataSize;
} NvMU_ShmData;

/**
 * \brief Creates a shared mem instance
 *
 * \param[out] phShm pointer to shm handle
 * \param[in] pConfig pointer to shm config
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_ShmOpen(NvMU_Shm *phShm, NvMU_ShmConfig *pConfig);

/**
 * \brief Close shared mem instance
 *
 * \param[in] phShm pointer to shm handle
 * \param[in] bDestroy Set to NVMU_TRUE if node needs to be destroyed (to be used by server)
 * \return \ref NVMU_STATUS_OK if successful
 */
NvMU_Status
NvMU_ShmClose(NvMU_Shm *phShm, Bool bDestroy);

/**
 * \brief Read from shared mem
 *
 * \param[in] hShm shm handle
 * \param[in] pData pointer to shm container to store read data
 * \param[in] uTimeoutMs Time to block for in milliseconds for operation to complete
 * \return \ref NVMU_STATUS_OK if successful
 * \return \ref NVMU_STATUS_NOT_READY if supplied buffer is not large enough
 * \return \ref NVMU_STATUS_TIMED_OUT if operation was not successfull within specified time
 */
NvMU_Status
NvMU_ShmRead(NvMU_Shm hShm, NvMU_ShmData *pData, U32 uTimeoutMs);

/**
 * \brief Write to shared mem
 *
 * \param[in] hShm shm handle
 * \param[in] pData pointer to shm container to store data to be written
 * \param[in] uTimeoutMs Time to block for in milliseconds for operation to complete
 * \return \ref NVMU_STATUS_OK if successful
 * \return \ref NVMU_STATUS_TIMED_OUT if operation was not successfull within specified time
 */
NvMU_Status
NvMU_ShmWrite(NvMU_Shm hShm, NvMU_ShmData *pData, U32 uTimeoutMs);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* __NVMU_SHM_H__ */
