/*
 * Copyright (c) 2010-2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_HDCP_UP_H
#define INCLUDED_HDCP_UP_H

#if defined(__cplusplus)
extern "C"
{
#endif

// HDCP Return Error Code
typedef enum
{
    HDCP_RET_SUCCESS            = 0,
    HDCP_RET_UNSUCCESSFUL       = 1,
    HDCP_RET_INVALID_PARAMETER  = 2,
    HDCP_RET_NO_MEMORY          = 3,
    HDCP_RET_READS_FAILED       = 4,
    HDCP_RET_READM_FAILED       = 5,
    HDCP_RET_LINK_PENDING       = 6,    // downstream in process, not real failure
} HDCP_RET_ERROR;

/**
 * Client Handle for an HDCP connection
 */
typedef void * HDCP_CLIENT_HANDLE;

/**
 * hdcp_open
 * Opens an HDCP upstream client interface to a display.
 * A client handle is allocated and returned from this function.
 * The caller must release the handle by calling hdcp_close.
 *
 * @param hClientRef A pointer to HDCP_CLIENT_HANDLE.
 * @param fbIndex Specifies the framebuffer instance to open.
 * @retval HDCP_RET_SUCCESS on success, one of HDCP_RET_ERROR otherwise.
 * @returns hClientRef Receives a HDCP_CLIENT_HANDLE on success; NULL on error.
 */
HDCP_RET_ERROR hdcp_open(HDCP_CLIENT_HANDLE *hClientRef, int fbIndex);

/**
 * hdcp_close
 * Closes an HDCP upstream client interface.
 *
 * @param hClient A HDCP_CLIENT_HANDLE returned from a prior successful call to hdcp_open.
 * @retval HDCP_RET_SUCCESS on success, one of HDCP_RET_ERROR otherwise.
 */
HDCP_RET_ERROR hdcp_close(HDCP_CLIENT_HANDLE hClient);

/**
 * hdcp_status
 * Performs a check of the HDCP link status and reports if it is valid.
 *
 * @param hClient A HDCP_CLIENT_HANDLE returned from a prior successful call to hdcp_open.
 * @retval HDCP_RET_SUCCESS on success, one of HDCP_RET_ERROR otherwise.
 */
HDCP_RET_ERROR hdcp_status(HDCP_CLIENT_HANDLE hClient);

#if defined(__cplusplus)
}
#endif

#endif
