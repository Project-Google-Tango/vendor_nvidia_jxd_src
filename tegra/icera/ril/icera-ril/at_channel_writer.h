/* Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved. */
/*
** Copyright 2010, Icera Inc.
*/

/**
* @{
*/

/**
* @file at_channel_writer.h The interface of the AT channel writer
*
*/

#ifndef __AT_CHANNEL_WRITER_H_
#define __AT_CHANNEL_WRITER_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*************************************************************************************************
* Project header files
************************************************************************************************/
#include "ril_request_queue.h"  /* struct _rilq_handle_t */

/*************************************************************************************************
* Standard header files
************************************************************************************************/
#include <stdlib.h> /* size_t */

/*************************************************************************************************
* Public Types
************************************************************************************************/
/**
 * forward declaration of the AT Channel Handle structure, which is used to
 *  identify the channel (AT Channel Writer instance)
 */
struct _at_channel_writer_handle_t;

typedef enum
{
      ATC_DEV_TYPE_UNKNOWN
    , ATC_DEV_TYPE_LOOPBACK
    , ATC_DEV_TYPE_TTY
    , ATC_DEV_TYPE_SOCKET
}
at_channel_device_type_t;

/* type definitions of supported callback functions */
/**
 * callback function which opens a device and returns the file descriptor of it
 *
 * @param device_path [IN] string containing the device name, address, etc
 *                         e.g. "/dev/ttyS0" or "127.0.0.1:12345"
 * @param device_type [IN] type of device as defined in at_channel_device_type_t
 * @param default_channel [IN] flag indicating if it's called for the default
 *                         channel (==1) or not (==0)
 *
 * @sa  at_channel_dev_close_callback_t
 */
typedef int (*at_channel_dev_open_callback_t)( char* device_path,
                                               at_channel_device_type_t device_type,
                                               int default_channel );

/**
 * callback function which is called when the device is closed
 *
 * @param fd [IN] file descriptor to close
 *
 * @sa  at_channel_dev_open_callback_t
 */
typedef void (*at_channel_dev_close_callback_t)( void* fd );

/**
 * the init callback is called in contect of the AT Channel and allows
 * channel specific initialisation. For example the default channel might
 * enable more unsolicited events than extension channels, so if creating
 * the default channel, here a special function could be given here
 */
typedef void (*at_channel_init_callback_t)( void );

/**
 * onUnsolicited() callback function pointer which is used by the reader thread
 * to pass unsolicited events to.
 * Note: it is not allowed to send AT commands in this function/context.
 */
typedef void (*at_channel_unsolicited_callback_t)( const char *s, const char *sms_pdu );

/**
 * Callback function pointer which points to a function that is executed in
 * context of the AT Channel writer thread and handles unsolicited events.
 * Note: it is allowed to send additional AT commands in this function/context
 */
typedef void (*at_channel_handle_unsolicited_callback_t)( const char *s, const char *sms_pdu );


/**
 * usual onATTimeout() callback function pointer
 */
typedef void (*at_channel_timeout_callback_t)( void );

/**
 * usual onATReaderClosed() callback function pointer
 */
typedef void (*at_channel_closed_callback_t)( void );

/**
 * Handler function which is called in contect of the AT Channel Writer and
 *  executes the RIL Request functions.
 */
typedef void (*at_channel_handle_request_callback_t)( int request, void* data, size_t datalen, void* ril_token );

/**
 * This structure gets all callback functions which have to be called by
 *  the AT Channel writer
 */
typedef struct _ril_at_channel_callback_funcs_t
{
    at_channel_dev_open_callback_t        callback_dev_open;         /**< device open function */
    at_channel_dev_close_callback_t       callback_dev_close;        /**< device close function */
    at_channel_init_callback_t            callback_init;             /**< callback function for specific
                                                                      channel initialization */
    at_channel_unsolicited_callback_t     callback_unsolicited;      /**< callback function for
                                                                      unsolicited events in AT channel reader context */
    at_channel_handle_unsolicited_callback_t  callback_handle_unsolicited;   /**< callback function which handles
                                                                       unsolicited events in context of the of the AT Channel writer */
    at_channel_timeout_callback_t         callback_timeout;          /**< callback function for timeouts */
    at_channel_closed_callback_t          callback_closed;           /**< callback function if the reader
                                                                      thread fo a channel died */
    at_channel_handle_request_callback_t  callback_handle_request;   /**< callback function which handles RIL
                                                                      requests in contect of the AT Channel writer */
}
at_channel_callback_funcs_t;

/*************************************************************************************************
* Public function declarations
************************************************************************************************/
/**
 * Create an AT Channel Writer
 *
 * This function creates a new At Channel Writer thread fo a specific device,
 *  which represents an AT Channel (or more precise RIL Request Channel)
 *
 * @param device_name [IN] zero terminated string describing the device, e.g. /dev/ttyS0
 * @param device_type [IN] device type as defined with at_channel_device_type_t
 * @param callback_funcs [IN] pointer to callback function struct
 * @param rilq_handle [IN] the RIL Request queue which is queried by this channel
 * @param default_channel [IN] indicates if it's the creation of the default at channel (==1)
 *                            or an extended channel (==0)
 * @param timeout [IN] When sending an AT command a timer is started. It expires after this
 *                       interval. usually the modem sends the final response before.
 *                       if this timer expires the error recovery mechanism has to be triggered.
 *                       The value has to be given in seconds [s].
 *
 * @return returns a new AT Channel Handle if successfull, NULL otherwise
 *
 * @sa at_channel_writer_destroy, _at_channel_handle_t
 */
struct _at_channel_writer_handle_t* at_channel_writer_create( const char* device_name,
                                                       at_channel_device_type_t device_type,
                                                       const at_channel_callback_funcs_t* callback_funcs,
                                                       struct _rilq_handle_t* rilq_handle,
                                                       int default_channel,
                                                       long long timeout );

/**
 * destroy an AT Channel Writer
 *
 * This function stops an AT Channel Writer thread and frees all allocated resources
 *
 * @param at_channel_handle [IN] Handle of the AT Channel Writer
 *
 * @return void
 *
 * @sa  at_channel_writer_create, _at_channel_handle_t
 */
void at_channel_writer_destroy(struct _at_channel_writer_handle_t* at_channel_handle) ;

/**
 * Set final response timeout for AT channel
 *
 * This function sets the timeout for a specific AT channel, which defines the maximum time until a
 * final response from the modem is received. If the final response is not received within this time
 * the error recovery mechanism is triggered, because it's assumed that the modem hangs.
 *
 * @param at_channel_handle [IN] Handle of the AT Channel Writer
 * @param timeout [IN] Timeout in [ms]
 *
 * @return void
 *
 * @sa  at_channel_writer_create, _at_channel_handle_t
 */
void at_channel_writer_set_at_cmd_timeout(struct _at_channel_writer_handle_t* at_channel_handle,
                                          long long timeout) ;

/*
 * Get the readiness of the given at channel
 * 
 * @param at_channel_writer_handle [IN] Handle of the AT channel writer being queried
 * 
 * @return boolean describing the the readiness of the given channel.  
 */
int at_channel_is_ready(struct _at_channel_writer_handle_t* at_channel_writer_handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __AT_CHANNEL_WRITER_H_ */

/** @} END OF FILE */
