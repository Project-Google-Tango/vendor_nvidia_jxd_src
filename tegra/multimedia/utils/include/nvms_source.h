/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _NVMS_SOURCE_H_
#define _NVMS_SOURCE_H_

#include <nvms_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Opaque handle type for Streaming Source
 */
typedef void* NvMS_Source;

/**
 * \brief Opaque handle type Streaming Source application
 */
typedef void* NvMS_SourceApp;

/**
 * \brief Source callback message type enumeration
 */
typedef enum {
    NVMS_SOURCE_MSG_ERROR,
    NVMS_SOURCE_MSG_SERVER_DYING,
    NVMS_SOURCE_MSG_INFO,
    NVMS_SOURCE_MSG_ROUTE,
    NVMS_SOURCE_MSG_ABORT,
    NVMS_SOURCE_MSG_UND = 0x7FFFFFFF
} NvMS_SourceMsgType;

/**
 * \brief Callback message structure
 */
typedef struct {
    unsigned int uNumStreams;
    NvMS_RouteType eRoute[NVMS_MAX_NUM_STREAMS];
} NvMS_SourceMsgSetRoute;

/**
 * \brief Callback message structure
 */
typedef struct {
    void *pMsg;
    NvMS_SourceApp hAppHandle;
    NvMS_SourceMsgType eType;
} NvMS_SourceMsg;

/**
 * \brief Callback Interface for messages from media streamer source
 *
 * \param[in] hSource Streamer source handle
 * \param[in] pMsg Callback message from source
 * \return void
 */
typedef NvMS_Status
(*NvMS_SourceCb)(NvMS_SourceMsg *pMsg);

/**
 * \brief Source Configuration
 */
typedef struct {
    /*! Callback handler */
    NvMS_SourceCb hCb;
    /*! Application handle */
    NvMS_SourceApp hAppHandle;
    /*! Path for IPC communication */
    char *pPath;
    /*! Verbosity (Loud = 1/quiet = 0) */
    unsigned int uVerbose;
} NvMS_SourceConfig;

/**
 * \brief Create a new source instance
 *
 * \param[in] phSource Pointer to source handle
 * \param[in] pConfig Pointer to source configuration
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_SourceOpen(NvMS_Source *phSource,
                NvMS_SourceConfig *pConfig);


/**
 * \brief Destroy a source instance
 *
 * \param[in] pSource Pointer to source handle
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_SourceClose(NvMS_Source *phSource);

/**
 * \brief Present a buffer to be processed
 *
 * \param[in] hSource source handle
 * \param[in] pBuffer Pointer to buffer
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_SourceProcess(NvMS_Source hSource,
                   NvMS_Buffer *pBuffer, unsigned int uTimeoutMs);

/**
 * \brief Configure source
 *
 * \param[in] hSource source handle
 * \param[in] pConfig Pointer to source configuration
 * \return \ref NVMS_STATUS_OK if successful
 */
NvMS_Status
NvMS_SourceConfigure(NvMS_Source hSource,
                     NvMS_BufferConfig *pConfig);

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* _NVMS_SOURCE_H_ */

