/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVMU_TYPES_H__
#define __NVMU_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long long U64;
typedef signed long long   S64;
typedef unsigned int       U32;
typedef signed int         S32;
typedef unsigned short     U16;
typedef signed short       S16;
typedef unsigned char      U8;
typedef signed char        S8;
typedef unsigned char      Bool;

#define U64_MAX    (18446744073709551615ULL)
#define U64_MIN    (0)
#define S64_MAX    (9223372036854775807LL)
#define S64_MIN    (-S64_MAX - 1)
#define U32_MAX    (4294967295UL)
#define U32_MIN    (0)
#define S32_MAX    (2147483647L)
#define S32_MIN    (-S32_MAX - 1)
#define U16_MAX    (65535)
#define U16_MIN    (0)
#define S16_MAX    (32767)
#define S16_MIN    (-S16_MAX - 1)
#define U8_MAX     (255)
#define U8_MIN     (0)
#define S8_MAX     (127)
#define S8_MIN     (-S8_MAX - 1)
#define NVMU_TRUE  (1)
#define NVMU_FALSE (0)

#define NVMU_TIMEOUT_INFINITE (0xFFFFFFFF)

/*! Level information for display */
typedef enum {
    VERBOSE = 0,
    INFO,
    WARNING,
    ERROR,
    NUMLEVELS
} NvMU_InfoLevel;

/*! Return value enum */
typedef enum {
    NVMU_STATUS_OK = 0,                 /**< Success */
    NVMU_STATUS_NOOP,                   /**< Success, but nothing was done */
    NVMU_STATUS_SUSPEND,                /**< For Thread: Return this to put thread in suspend state */
    NVMU_STATUS_SLEEP,                  /**< For Thread: Return this to put thread in sleep state */
    NVMU_STATUS_QUIT,                   /**< For Thread: Return this to quit thread  */
    NVMU_STATUS_TIMED_OUT,              /**< Called operation has timed out */
    NVMU_STATUS_FALSE,                  /**< Operation completed with false result */
    NVMU_STATUS_FAIL = 0x80000000,      /**< General failure */
    NVMU_STATUS_QUEUE_FULL,             /**< Queue is Full */
    NVMU_STATUS_QUEUE_EMPTY,            /**< Queue is Empty */
    NVMU_STATUS_INVALID_POINTER,        /**< Invalid (e.g. null) pointer */
    NVMU_STATUS_INVALID_ARGUMENT,       /**< Invalid argument */
    NVMU_STATUS_INVALID_INDEX,          /**< Invalid array index */
    NVMU_STATUS_INVALID_VERSION,        /**< Invalid version */
    NVMU_STATUS_INVALID_STATE,          /**< Invalid state */
    NVMU_STATUS_OUT_OF_MEMORY,          /**< Out of memory */
    NVMU_STATUS_ERROR_DISCONNECTED,     /**< IPC connection lost */
    NVMU_STATUS_NVSOCKET_WOULDBLOCK,    /**< IPC call would block */
    NVMU_STATUS_NOT_IMPLEMENTED,        /**< Unimplemented functionality */
    NVMU_STATUS_NOT_SUPPORTED,          /**< Not supported */
    NVMU_STATUS_NOT_READY               /**< Not ready */
} NvMU_Status;

#ifdef __cplusplus
}
#endif

#endif /*__NVMU_TYPES_H__ */
