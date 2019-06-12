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
 * @file obex_server.h Obex core interface.
 *
 */

#ifndef OBEX_CORE_H
#define OBEX_CORE_H

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

/**
 * Size of buffers to use for RX/TX transfers
 */
#define OBEX_CORE_BUFFER_SIZE (8192)

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

/**
* Wait for a packet and read it entirely, based on the
* information contained in the packet length field.
* This function decodes the packet opcode/response code, packet
* length and fills in the provided packet structure with these
* information
*
* @param packet The packet data structure to write packet
*               information into
* @return Return code
* @see obex.h
*/
obex_ResCode obex_CoreReadPacket(obex_PacketDesc *packet);

/**
* Read request data out of a packet that was previously read
* using the obex_CoreReadPacket() function
*
* @param packet The packet to read request data from
* @param data The pointer to write the data pointer into
* @param length Length of the request data
* @return Return code
* @see obex.h
*/
obex_ResCode obex_CoreReadRequestData(obex_PacketDesc *packet, void **data, uint32 length);

/**
* Read request data out of a packet that was previously read
* using the obex_CoreReadPacket() function
*
* @param packet The packet to read response data from
* @param data The pointer to write the data pointer into
* @param length Length of the response data
* @return Return code
* @see obex.h
*/
obex_ResCode obex_CoreReadResponseData(obex_PacketDesc *packet, void **data, uint32 length);

/**
* Read a header out of a packet that was previously read using
* the obex_CoreReadPacket() function. This function can be
* called multiple times on a given packet to retrieve all
* headers. This function returns OBEX_EOP when the End Of Packet
* is reached.
*
* @param packet Packet to read header from
* @param header Header data structure to write data into
* @return Return code
* @see obex.h
*/
obex_ResCode obex_CoreReadNextHeader(obex_PacketDesc *packet, obex_GenericHeader *header);

/**
* Send a simple packet (i.e. without optional headers )
*
* @param code Code of the packet to send
* @param data Request/response data. This parameter can be set
*             to NULL if there are no request/response data to
*             send
* @data_length Length of request/response data. This parameter
*              can be set to 0 if there are no request/response
*              data to send
* @return Return code
* @see obex.h
*/
obex_ResCode obex_CoreSendSimplePacket(uint8 code, uint8 *data, uint16 data_length);

/**
* Create a new packet for sending. A call to
* obex_CoreSendPacket() must be subsequently made for the
* packet to be sent over the transport layer
*
* @param packet Packet data stucture to write packet information
*               into
* @param code Code of the packet to send
* @param data Request/response data. This parameter can be set
*             to NULL if there are no request/response data to
*             send
* @param data_length Length of the Request/response data. This
*             parameter can be set to NULL if there are no
*             request/response data to send
* @return Return code
* @see obex.h
*/
obex_ResCode obex_CoreCreatePacketForSending(obex_PacketDesc *packet, uint8 code, uint8 *data, uint16 data_length);

/**
* Append header to an existing packet (previously created
* through a call to obex_CoreCreatePacketForSending() )
*
* @param packet Packet to append header to
* @param header header Header to append to packet
* @return Return code
* @see obex.h
*/
obex_ResCode obex_CoreAppendHeader(obex_PacketDesc *packet, obex_GenericHeader *header);

/**
* Send a packet which was previously created with a call to
* obex_CoreCreatePacketForSending() and populated with calls
* to obex_CoreAppendHeader()
*
* @param packet Packet to send
* @return Return code
* @see obex.h
*/
obex_ResCode obex_CoreSendPacket(obex_PacketDesc *packet);

/**
* Return the length of a packet prologue - this is useful for
* working out how big a packet will turn out
*
* @return The length (in bytes) of a packet prologue
*/
uint16 obex_CoreGetPacketPrologueLength(void);

/**
* Return the length of a header prologue - this is useful for
* working out how big a packet will turn out
*
* @param header_type Header type to get the length of.
* @return The length (in bytes) of a header of type header_type
* @see obex.h
*/
uint16 obex_CoreGetHeaderPrologueLength(obex_ObjHdrType header_type);

/**
 * Re-init OBEX transport layer
 *
 * @return obex_ResCode OBEX_OK unless call was done when server
 *         not initialised
 */
obex_ResCode obex_CoreReInit(void);
#endif /* #ifndef OBEX_CORE_H */

/** @} END OF FILE */
