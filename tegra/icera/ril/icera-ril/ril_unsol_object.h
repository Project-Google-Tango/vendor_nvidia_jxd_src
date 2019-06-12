/* Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved. */
/*
** Copyright 2010, Icera Inc.
*/

/**
* @{
*/

/**
* @file ril_unsol_object.h ...
*
*/

#ifndef __RIL_UNSOL_OBJECT_H_
#define __RIL_UNSOL_OBJECT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*************************************************************************************************
* Public Types
************************************************************************************************/

/**
 * The request object whoch stores all parameters which are passed to a onRequest()
 * function call. Note that the token field has type void* instead of RIL_Token to
 * avoid the include of ril.h here. However, RIL_token is a void* too.
 */
typedef struct _ril_unsol_object_t
{
    char* string;   /**< unsolicited event string */
    char* SmsPdu;  /**< SMS DPU data */
}
ril_unsol_object_t;

/*************************************************************************************************
* Public function declarations
************************************************************************************************/
/**
 * Create and initialize a RIL Unsolicited Object
 *
 * This function allocates memory for the ril_unsol_object_t and copies the
 * content of the calling parameter to the new data buffer, so that the input
 * parameters can be freed directly after the function returned.
 *
 * @param  unsolString [IN] unsolicited event string
 * @param  SmsPdu [IN] SmsPdu data
 *
 * @return returns a new ril_unsol_object if successfull, NULL otherwise
 *
 * @sa ril_unsol_object_destroy,  ril_unsol_object_t
 */
struct _ril_unsol_object_t* ril_unsol_object_create( const char* unsolString, const char* SmsPdu );

/**
 * Destroy a RIL Request Object
 *
 * This function frees all resources of the given ril_unsol_object
 *
 * @param  ril_unsol_obj [IN] RIL Unsolicited Event Object to be freed
 *
 * @sa ril_unsol_object_create,  ril_unsol_object_t
 */
void ril_unsol_object_destroy( struct _ril_unsol_object_t* ril_unsol_obj );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __RIL_UNSOL_OBJECT_H_ */

/** @} END OF FILE */
