/* Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved. */
/*
** Copyright 2010, Icera Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** You may not use this file except in compliance with the License.
** You may obtain a copy of the License at:
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** imitations under the License.
*/
/*
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/**
* @{
*/

/**
* @file at_channel_handle.h The interface of the AT channel handle
*
*/

#ifndef __AT_CHANNEL_HANDLE_H_
#define __AT_CHANNEL_HANDLE_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*************************************************************************************************
* Project header files
************************************************************************************************/
#include "atchannel.h"  /* ATResponse and ATUnsolHandler */
#include "stdio.h"

/*************************************************************************************************
* Standard header files
************************************************************************************************/
#include <pthread.h>    /* pthread_t, pthread_mutex_t, pthread_cond_t, ... */

/*************************************************************************************************
* Macros
************************************************************************************************/
/** maximum number of characters of an AT command response */
#define MAX_AT_RESPONSE (8 * 1024)

/*************************************************************************************************
* Public Types
************************************************************************************************/
/**
 * AT Command type.
 * It has been moved from the RIL.h interface to this internal
 * interface. There is no need to see it on the public interface.
 * However the parameter is AT Channel specific, so we need the
 * declaration here.
 */
typedef enum {
    NO_RESULT,   /**< no intermediate response expected */
    NUMERIC,     /**< a single intermediate response starting with a 0-9 */
    SINGLELINE,  /**< a single intermediate response starting with a prefix */
    MULTILINE,   /**< multiple line intermediate response starting with a prefix */
    MULTILINE_NO_PREFIX, /**< multiple line intermediate response starting without a prefix */
    TOFILE       /**< save the AT command result into a file */
} ATCommandType;

/**
 * This structure represents the handle of the AT channel.
 * It contains all the AT channel specific data
 */
typedef struct _at_channel_handle_t
{
    pthread_t tid_reader;             /**< reader thread ID */
    int fd;                           /**< fd of the AT channels TTY device */

    char ATBuffer[MAX_AT_RESPONSE+1]; /**< buffer for AT responses */
    char *ATBufferCur;                /**< pointer to current AT buffer position */
    int readCount;                    /**< number of bytes read */

    const char *filename;             /**< file name */
    FILE *fileout;

    pthread_mutex_t commandmutex;     /**< lock for different write attemps on the same fd */
    pthread_cond_t  commandcond;      /**< AT send command blocks until this is signalled */

    char at_cmd[8];                   /**< store current AT cmd; longest expected is "AT+CGACT\0" */
    ATCommandType   type;             /**< AT command type as defined with ATCommandType */
    const char     *responsePrefix;   /**< response prefix */
    const char     *smsPDU;           /**< data buffer containing the SMS in PDU format */
    ATResponse     *response;         /**< data object which represents the response to
                                           an AT command */

    ATUnsolHandler unsolHandler;      /**< callback function for unsolicited modem events */
    void (*onTimeout)(void);          /**< callback function for timeouts while waitinf for the
                                           final response */
    void (*onReaderClosed)(void);     /**< callback function when errors in the reader thread
                                           occur */
    int readerClosed;                 /**< flag which stores the status of the reader thread */

    int isDefaultChannel;             /**< flag which indicates if it's the handle of the default
                                                channel (==1) or not (==0) */
    long long ATCmdTimeout;           /**< timeout [ms] until which a final response for the
                                           command is expected. If the timeout expires, the
                                           onTimeout() callback function is called */
    int isInErrorRecovery;            /**< if we are in error recovery we should no longer send
                                            AT commands to the modem. However this might happen
                                            in a very short period of time during shutdown process,
                                            therefore we need this flag for suppressing modem
                                            communication.
                                            The flag is zero if not in error recovery mode or
                                            contains a negative error value if error recovery is
                                            in progress */
}
at_channel_handle_t;

/*************************************************************************************************
* Public function declarations
************************************************************************************************/
/**
 * Create a pre-initialized AT channel handle
 *
 * Creates an AT Channel Handle object and initializes all fields with
 * default values.
 *
 * @return returns the new AT Channel Handle
 */
at_channel_handle_t* at_channel_handle_create( void );

/**
 * Destroy an AT channel handle
 *
 * Frees the allocated resources of the AT Channel handle itself.
 * It doesn't take care of buffers which have been allocated outside this
 * module. These have to be freed by the allocating instance.
 *
 * @param  at_channel_handle At Channel handle to destroy
 *
 * @return returns the new AT Channel Handle
 */
void at_channel_handle_destroy( at_channel_handle_t* at_channel_handle );

/**
 * Attache an AT handle structure to the current thread
 *
 * Usecase is the AT reader thread which reads from the same fd
 * as the write thread does and uses the same mutexes/conditions.
 *
 * @param at_channel_handle AT Channel handle to register in the current thread
 *
 * @return 0 if successfull, pthread error code otherwise
 */
int at_channel_handle_set(const at_channel_handle_t* at_channel_handle );

/**
 * Retrieve the AT handle structure of the current thread
 *
 * It can be used in the actual thread to get the AT channel specific parameters
 * for this AT channel.
 *
 * @return valid AT Channel handle if successfull, NULL otherwise
 */
at_channel_handle_t* at_channel_handle_get( void );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __AT_CHANNEL_HANDLE_H_ */

/** @} END OF FILE */
