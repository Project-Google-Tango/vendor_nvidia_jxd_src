/*************************************************************************************************
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup ObexLayer
 * @{
 */

/**
 * @file obex_core.c OBEX core layer
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "obex.h"
#include "obex_core.h"
#include "obex_transport.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <string.h>
#ifndef __dxp__
#include <stdarg.h>
#endif

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/

/** size of the OBEX packet prologue (opcode or response code
 *  or ID + 16-bit packet length) */
#define OBEX_CORE_PACKET_PROLOGUE_LENGTH (sizeof(obex_Prologue))

/** size of the OBEX sequence header prologue (opcode or
 *  response code or ID + 16-bit packet length) */
#define OBEX_CORE_SEQUENCE_HEADER_PROLOGUE_LENGTH (sizeof(obex_Prologue))

/** size of the OBEX prologue for 1 or bytes quantities */
#define OBEX_CORE_1_OR_4_BYTE_HEADER_PROLOGUE_LENGTH (1)

/* bytes left in RX buffer */
#define BYTES_LEFT_IN_RX_BUFFER(packet) (packet->payload_length-obex_core.rx_buffer.offset)

/* bytes left in TX buffer */
#define BYTES_LEFT_IN_TX_BUFFER(packet) (OBEX_CORE_BUFFER_SIZE-obex_core.tx_buffer.offset)


/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/

/**
 *  An OBEX packet starts with a 1-byte opcode and a 2-byte
 *  packet length. Some headers (byte sequence or unicode) have
 *  a similar prologue.
 */
typedef struct
{
    uint8 code;                                  /**< code (8 bits) */
    uint8 length_msb;                            /**< length_msb (8 bits) */
    uint8 length_lsb;                            /**< length_lsb (8 bits) */
} obex_Prologue;

/**
* Data structure for an OBEX core buffer. The core layer makes
* use of two buffers, one for each of the RX/TX directions.
*
* The offset is maintained to keep track of how much of the
* buffer has been consumed (how many bytes written in the case
* of the TX buffer, how many bytes processed in the case of the
* RX buffer)
*/
typedef struct
{
    uint32 offset;                               /**< read offset (RX buffer) / write offset (TX buffer) */
    uint8 buffer[OBEX_CORE_BUFFER_SIZE];     /**< main buffer */
} obex_CoreBuffer;

/**
* Data structure for the OBEX core layer state
*/
typedef struct
{
    obex_CoreBuffer rx_buffer;                /**< read buffer */
    obex_CoreBuffer tx_buffer;                /**< write buffer */
} obex_CoreState;

/*************************************************************************************************
 * Private function declarations (only used if absolutely necessary)
 ************************************************************************************************/

/*************************************************************************************************
 * Private variable definitions
 ************************************************************************************************/

/**
 * OBEX Core layer state (not declared 'static' so as to ease
 * debug)
 */
static obex_CoreState obex_core;

static char *obex_log_prefix = "OBEX: Core";

/*************************************************************************************************
 * Public variable definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/**
 * Read request or response data from a packet - we are making
 * the assumption that these data immediately follow the packet*
 * prologue i.e. there is no interleaved header between the
 * prologue and the request/response data
 *
 * @param packet Packet to read data from
 * @param data Pointer to write data pointer into
 * @length Lenght of the data to read (this is operation
 *         dependent)
 * @return Return code
 */
static obex_ResCode ReadPacketData(obex_PacketDesc *packet, void **data, uint32 length)
{
    /* sanity checks */

    if (NULL == packet)
    {
        return OBEX_ERR;
    }

    /* make sure we have enough data to read */
    if (packet->packet_length < OBEX_CORE_PACKET_PROLOGUE_LENGTH + length)
    {
        return OBEX_INVALID;
    }

    /* update packet data structure */
    *data = (void*)obex_core.rx_buffer.buffer;

    /* update RX pointer */
    obex_core.rx_buffer.offset += length;

    return OBEX_OK;

}

/**
* Write a packet prologue and request/response data into the TX
* buffer. Data will be handed over to the transport layer later
*
* @param code Packet opcode/response code
* @param data Request/response data. This parameter may be set
*             to NULL if there are no request/response data to
*             send
* @param data_length  Length of Request/response data. This
*             parameter may be set to 0 if there are no
*             request/response data to send
* @return Return code
*/
static obex_ResCode WritePacketPrologueAndDataToTXBuffer(uint8 code, uint8 *data, uint16 data_length)
{
    obex_Prologue *packet_prologue;
    uint16 packet_length;

    /* sanity checks */
    if ((data_length>0) && (NULL == data))
    {
        return OBEX_ERR;
    }

    /* make sure we are not already sending a packet */
    if (obex_core.tx_buffer.offset != 0)
    {
        return OBEX_ERR;
    }

    packet_length = OBEX_CORE_PACKET_PROLOGUE_LENGTH + data_length;

    /* don't try to send packets larger than we can deal with */
    if (packet_length > OBEX_CORE_BUFFER_SIZE)
    {
        return OBEX_ERR;
    }

    /* prologue will be written from the beginning of the TX buffer */
    packet_prologue = (obex_Prologue *)obex_core.tx_buffer.buffer;

    /* fill in prologue with appropriate data */
    packet_prologue->code = code;
    packet_prologue->length_msb = OBEX_UINT16_MSB(packet_length);
    packet_prologue->length_lsb = OBEX_UINT16_LSB(packet_length);

    /* append packet data */
    memcpy( obex_core.tx_buffer.buffer + OBEX_CORE_PACKET_PROLOGUE_LENGTH, data, data_length);

    /* move write pointer forward */
    obex_core.tx_buffer.offset += packet_length;

    return OBEX_OK;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

obex_ResCode obex_CoreReadPacket(obex_PacketDesc *packet)
{
    obex_ResCode res;
    obex_Prologue prologue;
    uint16 packet_length;
    uint16 payload_length;

    /* reset RX pointer */
    obex_core.rx_buffer.offset = 0;

    /* read prologue */
    res = obex_TransportRead( (char*)&prologue, OBEX_CORE_PACKET_PROLOGUE_LENGTH );

    if (res != OBEX_OK)
    {
        return res;
    }

    /* decode packet length */
    packet_length = OBEX_MSB_LSB_TO_UINT16(prologue.length_msb,prologue.length_lsb);

    /* sanity checks */
    if (packet_length < OBEX_CORE_PACKET_PROLOGUE_LENGTH)
    {
        /* c.f. TC TestErrorOnTooShortAPacketLength */
        obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_CORE_ERR_NOT_PACKET_LENGTH_TOO_SMALL, (uint32)packet_length);
        return OBEX_INVALID;
    }

    /* work out the payload length */
    payload_length = packet_length - OBEX_CORE_PACKET_PROLOGUE_LENGTH;

    if (packet_length > OBEX_CORE_BUFFER_SIZE)
    {
        uint16 n_bytes_read = 0;

        /* c.f. TC TestErrorOnTooBigAPacketLength */
        obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_CORE_ERR_NOT_PACKET_LENGTH_TOO_BIG, (uint32)packet_length);

        /* flush data out of buffer */
        do
        {
            /* how many bytes can we read? */
            uint16 bytes_to_read = COM_MIN( OBEX_CORE_BUFFER_SIZE,
                                            payload_length - n_bytes_read );

            /* read them */
            res = obex_TransportRead( (char*)obex_core.rx_buffer.buffer, bytes_to_read);
            if (res != OBEX_OK)
            {
                return res;
            }

            /* update number of bytes read so far */
            n_bytes_read += bytes_to_read;

        } while (n_bytes_read < payload_length );

        return OBEX_DATA_TOO_BIG;
    }

    /* read payload (if any) */
    if (payload_length > 0)
    {
        /* now, read the rest of the packet */
        res = obex_TransportRead( (char*)obex_core.rx_buffer.buffer, payload_length );

        if (res != OBEX_OK)
        {
            return res;
        }
    }

    /* fill in the packet with the information we have so far */
    packet->code = prologue.code;
    packet->packet_length = packet_length;
    packet->payload_length = payload_length;

    return OBEX_OK;
}

obex_ResCode obex_CoreReadRequestData(obex_PacketDesc *packet, void**data, uint32 length)
{
    return ReadPacketData(packet, data, length);
}

obex_ResCode obex_CoreReadResponseData(obex_PacketDesc *packet, void **data, uint32 length)
{
    return ReadPacketData(packet, data, length);
}

obex_ResCode obex_CoreSendSimplePacket(uint8 code, uint8 *data, uint16 data_length)
{
    obex_ResCode ret;

    ret = WritePacketPrologueAndDataToTXBuffer(code, data, data_length);

    if (ret == OBEX_OK)
    {
        /* hand packet over to the transport layer - note that the write pointer equals
        the packet size */
        ALOGD("%s: %s: 0x%x 0x%x",
              obex_log_prefix,
              __FUNCTION__,
              code,
              obex_core.tx_buffer.offset);
        ret = obex_TransportWrite( (const char *)obex_core.tx_buffer.buffer, obex_core.tx_buffer.offset );
    }

    /* reset write pointer */
    obex_core.tx_buffer.offset = 0;

    return ret;
}

obex_ResCode obex_CoreReadNextHeader(obex_PacketDesc *packet, obex_GenericHeader *header)
{
    uint16 header_length = 0;                 /* for byte_sequence and unicode headers */
    uint16 payload_length;                    /* for byte_sequence and unicode headers */
    obex_Prologue *prologue;                  /* for byte_sequence and unicode headers */
    uint8 *rx_buf_ptr = (uint8*)obex_core.rx_buffer.buffer;
    uint8 header_id;
    uint8 header_type;

    /* sanity checks */
    if ((NULL == packet) || (NULL==header))
    {
        return OBEX_ERR;
    }

    /* anything left? */
    if (BYTES_LEFT_IN_RX_BUFFER(packet) < 1)
    {
        /* we have reached the end of the packet */
        return OBEX_EOP;
    }

    /* read header ID */
    header_id = *(rx_buf_ptr + obex_core.rx_buffer.offset);
    header_type = OBEX_GET_HEADER_MEANING(header_id);

    /* fill in header structure */
    header->id = header_id;
    header->type = header_type;

    switch (header_type)
    {
    case OBEX_OBJHDTYPE_UNICODE_SEQUENCE:
    case OBEX_OBJHDTYPE_BYTE_SEQUENCE:
        /* we nead at least two bytes left to read the header length */
        if (BYTES_LEFT_IN_RX_BUFFER(packet) < 2)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_CORE_ERR_NOT_ENOUGH_BYTES_LEFT_IN_PACKET);
            return OBEX_INVALID;
        }

        /* cast buffer to a prologue */
        prologue = (obex_Prologue*)(rx_buf_ptr + obex_core.rx_buffer.offset);

        /* read header length - next 2 bytes are header length MSB and LSB */
        header_length = OBEX_MSB_LSB_TO_UINT16(
                                                  prologue->length_msb,
                                                  prologue->length_lsb);
        payload_length = header_length - OBEX_CORE_SEQUENCE_HEADER_PROLOGUE_LENGTH;

        /* move read pointer forward */
        obex_core.rx_buffer.offset += OBEX_CORE_SEQUENCE_HEADER_PROLOGUE_LENGTH;

        /* we need at least header_length bytes lest to read the header value */
        if (BYTES_LEFT_IN_RX_BUFFER(packet) < payload_length)
        {
            /* c.f. TC TestErrorOnShorterPacketThanIndicatedBySequenceHeader */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_CORE_ERR_NOT_ENOUGH_BYTES_LEFT_IN_PACKET);
            return OBEX_INVALID;
        }

        /* save pointer to header value into header data structure */
        if (OBEX_OBJHDTYPE_BYTE_SEQUENCE==header_type)
        {
            header->value.byte_sequence.byte_length = payload_length;
            header->value.byte_sequence.ptr = rx_buf_ptr + obex_core.rx_buffer.offset;
        }
        else
        {
            /* make sure the length divides by two! */
            if (payload_length & 1)
            {
                obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_CORE_ERR_UNICODE_SEQUENCE_WITH_ODD_LENGTH, (uint32)payload_length);
                return OBEX_INVALID;
            }
            header->value.unicode_sequence.character_length = payload_length >> 1;
            header->value.unicode_sequence.ptr = (obex_UnicodeCharacter *)(rx_buf_ptr + obex_core.rx_buffer.offset);
            /* make sure the unicode sequence is NULL terminated */
            if ( (header->value.unicode_sequence.ptr[header->value.unicode_sequence.character_length-1].msb != 0)
                 || (header->value.unicode_sequence.ptr[header->value.unicode_sequence.character_length-1].lsb != 0))
            {
                /* c.f. TC TestErrorOnPutWithNotNullTerminatedFileName */
                obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_CORE_ERR_UNICODE_SEQUENCE_NOT_NULL_TERMINATED);
                return OBEX_INVALID;
            }
        }
        /* move read pointer forward */
        obex_core.rx_buffer.offset += payload_length;
        break;
    case OBEX_OBJHDTYPE_1BYTE:
        /* move read pointer forward */
        obex_core.rx_buffer.offset++;

        /* we need at least 1 byte left */
        if (BYTES_LEFT_IN_RX_BUFFER(packet) < 1)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_CORE_ERR_NOT_ENOUGH_BYTES_LEFT_IN_PACKET);
            return OBEX_INVALID;
        }
        /* fill in header struct */
        header->value.val_1b = *(rx_buf_ptr + obex_core.rx_buffer.offset);

        /* move read pointer forward */
        obex_core.rx_buffer.offset++;
        break;
    case OBEX_OBJHDTYPE_4BYTES:
        /* move read pointer forward */
        obex_core.rx_buffer.offset++;

        /* we need at least 4 bytes left */
        if (BYTES_LEFT_IN_RX_BUFFER(packet) < 4)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_CORE_ERR_NOT_ENOUGH_BYTES_LEFT_IN_PACKET);
            return OBEX_INVALID;
        }

        /* retrieve value and adjust read pointer - we are temporarily borrowing
        the existing rx_buf_ptr to point to the header value */
        rx_buf_ptr = rx_buf_ptr + obex_core.rx_buffer.offset;
        header->value.val_4b = OBEX_MSB_msb_lsb_LSB_TO_UINT32(
                                                                 rx_buf_ptr[0],
                                                                 rx_buf_ptr[1],
                                                                 rx_buf_ptr[2],
                                                                 rx_buf_ptr[3]);

        /* move read pointer forward */
        obex_core.rx_buffer.offset += 4;
        break;
    default:
        /* we are utterly confused */
        return OBEX_ERR;
    }

    ALOGD("%s: Received packet: 0x%x 0x%x 0x%x 0x%x 0x%x",
          obex_log_prefix,
          (uint32)packet->code,
          (uint32)packet->packet_length,
          (uint32)packet->payload_length,
          (uint32)header_length,
          (uint32)header_id);

    return OBEX_OK;
}

obex_ResCode obex_CoreCreatePacketForSending(obex_PacketDesc *packet, uint8 code, uint8 *data, uint16 data_length)
{
    obex_ResCode ret;

    /* sanity checks */
    if (NULL == packet)
    {
        return OBEX_ERR;
    }

    ret = WritePacketPrologueAndDataToTXBuffer(code, data, data_length);
    if (ret != OBEX_OK)
    {
        return ret;
    }

    /* initialise packet */
    packet->code = code;
    packet->packet_length = (uint16)obex_core.tx_buffer.offset;
    packet->payload_length = data_length;

    return OBEX_OK;
}

obex_ResCode obex_CoreAppendHeader(obex_PacketDesc *packet, obex_GenericHeader *header)
{
    uint16 header_length;

    /* sanity checks */
    if ((NULL == packet) || (NULL == header))
    {
        return OBEX_ERR;
    }

    /* add header ID */
    obex_core.tx_buffer.buffer[obex_core.tx_buffer.offset++] = header->id;

    /* treat the header differently depending on its type */
    switch (header->type)
    {
    case OBEX_OBJHDTYPE_UNICODE_SEQUENCE:
        /* append header length */
        header_length = (uint16)((header->value.unicode_sequence.character_length * 2) + OBEX_CORE_SEQUENCE_HEADER_PROLOGUE_LENGTH);
        obex_core.tx_buffer.buffer[obex_core.tx_buffer.offset++] = (uint8)OBEX_UINT16_MSB(header_length);
        obex_core.tx_buffer.buffer[obex_core.tx_buffer.offset++] = (uint8)OBEX_UINT16_LSB(header_length);
        /* append header value */
        memcpy(obex_core.tx_buffer.buffer + obex_core.tx_buffer.offset,    /* dest */
               header->value.unicode_sequence.ptr,    /* src */
               header->value.unicode_sequence.character_length * 2    /* byte_length */
              );
        /* move write pointer forward */
        obex_core.tx_buffer.offset += header->value.unicode_sequence.character_length * 2;
        break;
    case OBEX_OBJHDTYPE_BYTE_SEQUENCE:
        /* append header length */
        header_length = (uint16)(header->value.byte_sequence.byte_length + OBEX_CORE_SEQUENCE_HEADER_PROLOGUE_LENGTH);
        obex_core.tx_buffer.buffer[obex_core.tx_buffer.offset++] = (uint8)OBEX_UINT16_MSB(header_length);
        obex_core.tx_buffer.buffer[obex_core.tx_buffer.offset++] = (uint8)OBEX_UINT16_LSB(header_length);
        /* append header value */
        memcpy(obex_core.tx_buffer.buffer + obex_core.tx_buffer.offset,    /* dest */
               header->value.byte_sequence.ptr,  /* src */
               header->value.byte_sequence.byte_length    /* byte_length */
              );
        /* move write pointer forward */
        obex_core.tx_buffer.offset += header->value.byte_sequence.byte_length;
        break;
    case OBEX_OBJHDTYPE_1BYTE:
        /* append header value */
        obex_core.tx_buffer.buffer[obex_core.tx_buffer.offset++] = header->value.val_1b;
        break;
    case OBEX_OBJHDTYPE_4BYTES:
        /* append header value */
        obex_core.tx_buffer.buffer[obex_core.tx_buffer.offset++] = (uint8)OBEX_UINT32_MSB(header->value.val_4b);
        obex_core.tx_buffer.buffer[obex_core.tx_buffer.offset++] = (uint8)OBEX_UINT32_msb(header->value.val_4b);
        obex_core.tx_buffer.buffer[obex_core.tx_buffer.offset++] = (uint8)OBEX_UINT32_lsb(header->value.val_4b);
        obex_core.tx_buffer.buffer[obex_core.tx_buffer.offset++] = (uint8)OBEX_UINT32_LSB(header->value.val_4b);
        break;
    default:
        /* unknown header type - move write pointer back to remove the header ID */
        obex_core.tx_buffer.offset--;
        return OBEX_ERR;
    }

    return OBEX_OK;
}

obex_ResCode obex_CoreSendPacket(obex_PacketDesc *packet)
{
    obex_ResCode ret;
    obex_Prologue *prologue;
    uint16 packet_length;

    /* sanity checks */
    if (NULL == packet)
    {
        return OBEX_ERR;
    }

    /* update prologue */
    prologue = (obex_Prologue *)obex_core.tx_buffer.buffer;
    packet_length = (uint16)obex_core.tx_buffer.offset;
    prologue->length_msb = OBEX_UINT16_MSB(packet_length);
    prologue->length_lsb = OBEX_UINT16_LSB(packet_length);

    /* hand packet over to the transport layer - note that the write pointer equals
       the packet size */
    ALOGD("%s: %s: 0x%x 0x%x",
        obex_log_prefix,
        __FUNCTION__,
        packet->code,
        packet_length);
    ret = obex_TransportWrite( (const char *)obex_core.tx_buffer.buffer, packet_length );

    /* reset write pointer */
    obex_core.tx_buffer.offset = 0;

    return ret;
}

uint16 obex_CoreGetPacketPrologueLength(void)
{
    return OBEX_CORE_PACKET_PROLOGUE_LENGTH;
}

/**
* Return the size of a header prologue - this is useful for
* working out how big a packet will turn out
*/
uint16 obex_CoreGetHeaderPrologueLength(obex_ObjHdrType header_type)
{
    uint16 ret;

    switch (header_type)
    {
    case OBEX_OBJHDTYPE_UNICODE_SEQUENCE:
    case OBEX_OBJHDTYPE_BYTE_SEQUENCE:
        ret = OBEX_CORE_SEQUENCE_HEADER_PROLOGUE_LENGTH;
        break;
    case OBEX_OBJHDTYPE_1BYTE:
    case OBEX_OBJHDTYPE_4BYTES:
        ret = OBEX_CORE_1_OR_4_BYTE_HEADER_PROLOGUE_LENGTH;
        break;
    default:
        ALOGE("%s: %s: Unknown OBEX header type 0x%x",
              obex_log_prefix,
              __FUNCTION__,
              header_type);
        ret = 0;
        break;
    }

    return ret;
}

obex_ResCode obex_CoreReInit(void)
{
    int ret = OBEX_OK;

    ALOGD("%s: %s", obex_log_prefix, __FUNCTION__);
    ret = obex_TransportClose();
    if (ret == OBEX_OK)
    {
        obex_Sleep(1);
        ret = obex_TransportOpen();
    }
    return ret;
}

/** @} END OF FILE */
