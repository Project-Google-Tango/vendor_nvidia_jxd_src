/*
 * Copyright (c) 2008 - 2010 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b> Nv3p Byte Stream Specification</b>
 *
 * @b Description: Specifies the byte stream format for Nv3p.
 */

#ifndef INCLUDED_NV3P_BYTESTREAM_H
#define INCLUDED_NV3P_BYTESTREAM_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @defgroup nv3p_bytestream_group Nv3p Byte Stream Specification
 *
 * Specifies the byte stream format for Nv3p.
 * It is designed to be extensible and general purpose. No specific Nv3p
 * commands are defined here (see nv3p.h).
 *
 * Byte streams are little-endian. Packets may not be re-ordered or lost.
 * Packets are padded to 32 bits. Padding bits should be zero.
 *
 * The byte stream is network transport agnostic.
 * @ingroup nv3p_modules
 * @{
 */

/**
 * <h3>Header</h3>
 *
 * All Nv3p packets have the same basic header:
 * <pre>
 *
 *  0                   1                   2                   3   
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | version                                                       | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | packet type                                                   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | sequence number                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * </pre>
 *
 * @par version
 *
 * Defined as 0x1 for the first version.
 *
 * @par packet type
 *
 * Defined as one of:
 * - command--the protocol command. A command header follows the basic
 *     header.
 * - data--the data for the command. A data header follows the basic header.
 * - encrypted--the packet is encrypted. An encryption header follows the
 *     basic header.
 * - ack/nack--command, data, and encrypted packets must be ACKed (or NACKed,
 *     see the NACK packet definition). The ACK/NACK packet sequence number
 *     must match the packet that the receiver is ACKing. The NACK header
 *     follows the basic header.
 *
 * @par sequence number
 *
 *      Sender-sourced monatomically increasing packet number.
 *
 * <h3>Footer</h3>
 *
 * All Nv3p packets include the packet footer:
 * <pre>
 *
 *  0                   1                   2                   3   
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | checksum                                                      | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * </pre>
 *
 * @par checksum
 *
 *      The checksum over the entire packet, not including the footer.
 *
 * <h3>Command Packet</h3>
 *
 * Command packets may have zero or more parameters.
 *
 * @par Command Header
 *
 * <pre>
 *
 *  0                   1                   2                   3   
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | length                                                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | command number                                                |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | arguments ...                                                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * </pre>
 *
 * @par length
 *
 *      The number of argument bytes that follow the command number.
 *
 * @par command number
 *
 *      The command. See the Nv3p Protocol Specfication.
 *
 * @par arguments
 *
 *      Zero or more parameter bytes for the command.
 *
 * <h3>Data Packets</h3>
 *
 *      Data packets are the read or write data from a command. Write data
 *      follows the write command packet. Read data is sent from the receiver
 *      of the read command packet (there will be no command packet sent from
 *      the receiver).
 *
 *      For read return data, the command packet should be acked first, then
 *      the read data packet should follow.
 *
 *      Data may be split into multiple packets. The read or write command
 *      must specify the total data size. Each data packet has the payload
 *      length of it's own data only.
 *
 * @par Data Header
 *
 * <pre>
 *
 *  0                   1                   2                   3   
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | length                                                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | data ...                                                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * </pre>
 *
 * @par length
 *
 *      The number of bytes of data in this packet.
 *
 * @par data
 *
 *      The data bytes.
 *
 * <h3>Encrypted Packets</h3>
 *
 *      Encrypted packets are manditory when the ODM-Secure fuse is blown.
 *      The first nack when security is disabled will cause the system to
 *      shutdown.
 *
 * @par Encrypted Header
 *
 * <pre>
 *
 *  0                   1                   2                   3   
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | length                                                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * </pre>
 *
 * @par length
 *
 *      The number of bytes of packet payload.
 *
 * <h3>ACK Packets</h3>
 *
 *      Ack packets must be sent after each received command or data packet.
 *      The ack packet consists of only the basic header with the sequence
 *      number of the command, data, or encrypted packet to ack. If a packet
 *      is not processed (a command is invalid or not supported, a data write
 *      fails, etc), a nack must be sent to the sender.
 *
 * @par Nack Header
 *
 * <pre>
 * 
 *  0                   1                   2                   3   
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | error code                                                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * </pre>
 *
 * @par error code
 *
 *      The failing condition code (see nv3p_status.h for failure codes).
 */

/**
 * Defines sizes for packet headers in bytes.
 */
#define NV3P_PACKET_SIZE_BASIC      (3 * 4)
#define NV3P_PACKET_SIZE_COMMAND    (2 * 4)
#define NV3P_PACKET_SIZE_DATA       (1 * 4)
#define NV3P_PACKET_SIZE_ENCRYPTED  (1 * 4)
#define NV3P_PACKET_SIZE_FOOTER     (1 * 4)
#define NV3P_PACKET_SIZE_ACK        (0 * 4)
#define NV3P_PACKET_SIZE_NACK       (1 * 4)

/**
 * Defines the packet type in the Nv3p bytestream basic header.
 */
typedef enum
{
    Nv3pByteStream_PacketType_Command = 0x1,
    Nv3pByteStream_PacketType_Data,
    Nv3pByteStream_PacketType_Encrypted,
    Nv3pByteStream_PacketType_Ack,
    Nv3pByteStream_PacketType_Nack,

    Nv3pByteStream_PacketType_Force32 = 0x7FFFFFFF,
} Nv3pByteStream_PacketType;

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_NV3P_BYTESTREAM_H
