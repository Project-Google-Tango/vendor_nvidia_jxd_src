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
 * @file obex_server.h Obex server interface.
 *
 */

#ifndef OBEX_SERVER_H
#define OBEX_SERVER_H

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
 * OBEX Server Operation Callback
 *
 * Application layers can register a callback to the OBEX server
 * module to handle a specific opcode
 *
 * @param packet The packet to handle
 * @return Callbacks may return OBEX_OK to indicate that the
 *         application layer is done with this request.
 *         OBEX_CONTINUE may be returned if the operation
 *         requires subsequent packets to be received. In that
 *         case, the server layer will ensure that no other
 *         operations are interleaved with the current
 *         operation. Callbacks may return any other response
 *         code in case of an error.
 */
typedef obex_ResCode (*obex_ServerOperationCallback)(obex_PacketDesc *packet);

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

/**
* Initialise server
*/
void obex_ServerInitialise(void);

/**
* Start and run the OBEX server. This function blocks until a
* client fails to connect properly or a client disconnects from
* the server.
*
* @return Return code
* @see obex.h
*/
obex_ResCode obex_ServerRun(void);

/**
 * Register a callback for an operation. If a callback is
 * already registered for the operation, this callback will
 * replace the existing one
 *
 * @param opcode Operation to register to
 * @callback Callback to call when the server receives a packet
 *           with the provided opcode
 * @return Return code
 * @see obex.h
 */
obex_ResCode obex_ServerRegisterOperationCallback(obex_Opcode opcode, obex_ServerOperationCallback callback);

/**
 * Return the maximum packet size for server->client packets.
 * This function can be called to work out what the maximum
 * packet size is.
 *
 * @return Maximum server->client packet size
 */
uint16 obex_ServerMaximumS2CPacketSize(void);

/**
 * Return the maximum packet size for client->server packets.
 * This function can be called to work out what the maximum
 * packet size is.
 *
 * @return Maximum client->server packet size
 */
uint16 obex_ServerMaximumC2SPacketSize(void);

/**
 * Disconnect server from client
 */
void obex_SetServerDisconnected(void);

#endif /* #ifndef OBEX_SERVER_H */

/** @} END OF FILE */
