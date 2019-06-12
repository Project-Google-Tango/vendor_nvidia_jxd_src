/*************************************************************************************************
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 *
 * @addtogroup ObexLayer
 * @{
 */

/**
 * @file obex_transport.h Obex transport interface.
 *
 */

#ifndef OBEX_TRANSPORT_H
#define OBEX_TRANSPORT_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

#include "obex.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/

/**
 * Type definition for the transport layer read function. This
 * function is registered through a call to
 * obex_TransportRegister() and called in the context of
 * obex_CoreReadPacket().
 *
 * This function should block until 'count' bytes have been read
 * from the OBEX interface
 *
 * @param buffer Buffer to write information into
 * @param count Number of bytes to read
 *
 * @return obex_ResCode
 */
typedef obex_ResCode (*obex_TransportReadFunc)(char *buffer, int count);

/**
 * Type definition for the transport layer write function. This
 * function is registered through a call to
 * obex_TransportRegister() and called in the context of
 * obex_CoreSendSimplePacket()/obex_CoreSendPacket().
 *
 * This function should block until 'count' bytes have been
 * written to the OBEX interface
 *
 * @param buffer Buffer to read information from
 * @param count Number of bytes to write
 *
 * @return obex_ResCode
 */
typedef obex_ResCode (*obex_TransportWriteFunc)(const char *buffer, int count);

/**
 * Type definition for the transport layer open function. This
 * function is registered through a call to
 * obex_TransportRegister() and called in the context of
 * handling error detected on transport layer.
 *
 * This function should block until it was possible to open
 * transport layer for file system access or file system server
 * explicitely disconnected.
 *
 */
typedef obex_ResCode (*obex_TransportOpenFunc)(void);

/**
 * Type definition for the transport layer close function. This
 * function is registered through a call to
 * obex_TransportRegister() and called in the context of
 * handling error detected on transport layer.
 *
 */
typedef void (*obex_TransportCloseFunc)(void);

/**
 * Type definition for the OBEX transport layer function
 * interface. An structure of this type must be passed to
 * obex_TransportRegister() at initialization time to
 * register read/write functions
 */
typedef struct
{
    obex_TransportOpenFunc open;              /**< open function */
    obex_TransportCloseFunc close;            /**< close function */
    obex_TransportReadFunc read;              /**< read function */
    obex_TransportWriteFunc write;            /**< write function */
} obex_TransportFuncs;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/
/**
 *
 *
 * @return obex_ResCode
 */
obex_ResCode obex_TransportOpen(void);

/**
 *
 *
 * @return obex_ResCode
 */
obex_ResCode obex_TransportClose(void);

/**
* Register the OBEX transport layer functional interface. It is
* necessary to call the function before calls to the main OBEX
* Core layer functions.
*
* @param funcs functional interface
* @see obex_TransportFuncs
*/
obex_ResCode obex_TransportRegister(const obex_TransportFuncs *funcs);

/**
* This function is meant to be called by the OBEX core layer in
* order to send data over the OBEX interface.
*
* This function blocks until all information have been sent.
*
* @param buffer Buffer to read information from
* @count Number of bytes to write
*/
obex_ResCode obex_TransportWrite(const char *buffer, int count);

/**
* This function is meant to be called by the OBEX core layer in
* order to read data from the OBEX interface.
*
* This function blocks until 'count' bytes have been read from
* the OBEX interface
*
* @param buffer Buffer to write information into
* @count Number of bytes to read
*/
obex_ResCode obex_TransportRead(char *buffer, int count);

#endif /* #ifndef OBEX_TRANSPORT_H */

/** @} END OF FILE */
