/* Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved. */
/*
** Copyright 2010, Icera Inc.
*/

/**
* @{
*/

/**
* @file ril_request_queue.h The interface of the RIL-Request-Queue which
* handles the exchange of RIL-Request object between threads via a FIFO mechanism
*/

#ifndef __RIL_REQUEST_QUEUE_H_
#define __RIL_REQUEST_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*************************************************************************************************
* Project header files
************************************************************************************************/
#include "ril_request_object.h"
#include "ril_unsol_object.h"

/*************************************************************************************************
* Public Types
************************************************************************************************/
/**
 * RIL Request Queue handle. There can be several instances of the queue, so a
 * handle to identify the right instance is needed. However, the fields of the
 * handle are only used internally, so a forward declaration is enough at this
 * interface.
 */
struct _rilq_handle_t;

/** enum specifying the supported object types of the queue*/
typedef enum
{
      RILQ_OBJECT_TYPE_UNDEFINED /**< undefined object type, usually used
                                        for initialization and error detection */
    , RILQ_OBJECT_TYPE_REQUEST   /**< object is a RIL Request */
    , RILQ_OBJECT_TYPE_REQUEST_LOWP /**< object is a low priority ril request */
    , RILQ_OBJECT_TYPE_UNSOL     /**< object is an Unsolicited Response */
}
rilq_object_type_t;

/*************************************************************************************************
* Public function declarations
************************************************************************************************/
/**
 * Create and initialize a RIL Request Queue and returns the handle
 *
 * @return handle to the RIL Request Queue if successfull, NULL otherwise
 *
 * @sa rilq_destroy, rilq_put_request, rilq_get_request, _rilq_handle_t
 */
struct _rilq_handle_t* rilq_create( void );

/**
 * Destroy a RIL object Queue
 *
 * This function destroys a RIL Request queue. The handle will ne longer be
 * valid if the function returned. All pending RIL Requests will be frees
 * also by this function.
 *
 * @param handle [IN] Handle to existing RIL Request Queue
 *
 * @return void
 *
 * @sa rilq_create, rilq_put_object, rilq_get_object, _rilq_handle_t
 */
void rilq_destroy( struct _rilq_handle_t* handle );

/**
 * Put an Object to the queue
 *
 * This function puts a Request or unsolicited response object into the queue
 *
 * @param handle [IN] Handle to existing queue
 * @param data   [IN] pointer to object data
 * @param type   [IN] object type (request or unsolicited response)
 *
 * @return zero if successfull, nonzero otherwise
 *
 * @sa rilq_create, rilq_get_object, _rilq_handle_t
 */
int rilq_put_object(struct _rilq_handle_t* handle, void* data,  rilq_object_type_t type);

/**
 * Get an Object from the queue
 *
 * This function gets a request or unsolicited response object from the queue
 *
 * @param handle [IN] Handle to existing queue
 * @param type   [OUT] object type (request or unsolicited response)
 *
 * @return pointer to object if successfull, NULL otherwise
 *
 * @sa rilq_create, rilq_put_object, _rilq_handle_t
 */
void* rilq_get_object( struct _rilq_handle_t* handle,  rilq_object_type_t* type);

/**
 * unblocks the rilq_get_request() function, e.g. when the calling thread
 * has to be terminated
 *
 * @param handle [IN] handle of queue to unblock
 *
 * return void
 */
void rilq_unblock( struct _rilq_handle_t* handle );

/**
 * removes all pending objects from the queue
 *
 * @param handle [IN] handle of queue to clear
 *
 * return void
 */
void rilq_clear( struct _rilq_handle_t* handle );

/**
 * returns the number of currently available RIL requests in the queue
 */
unsigned long rilq_get_req_count(const struct _rilq_handle_t* handle );

/**
 * returns the number of currently available unsolicited responses in the queue
 */
unsigned long rilq_get_unsol_count(const struct _rilq_handle_t* handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __RIL_REQUEST_QUEUE_H_ */

/** @} END OF FILE */
