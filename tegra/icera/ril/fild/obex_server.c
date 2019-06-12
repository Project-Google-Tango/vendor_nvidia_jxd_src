/*************************************************************************************************
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup ObexLayer
 * @{
 */

/**
 * @file obex_server.c Obex server layer
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "obex.h"
#include "obex_server.h"
#include "obex_core.h"
#include "fild.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/

/* maximum number of opcodes */
#define OBEX_SERVER_MAX_OPCODES (20)

/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/

typedef struct
{
    obex_Opcode opcode;                       /* opcode to register for */
    obex_ServerOperationCallback callback;    /* application callback */
} obex_ServerOperationStruct;

typedef struct
{
    bool connected;                           /* boolean to determine if a client is connected */
    uint16 maximum_c2s_packet_length;         /* maximum client->server packet length */
    uint16 maximum_s2c_packet_length;         /* maximum server->client packet length */
    obex_Opcode processing_request;           /* used to indicate whether the server is currently busy processing a request */
} obex_ServerState;

typedef struct
{
    obex_ServerState state;                                            /* state */
    obex_ServerOperationStruct operations[OBEX_SERVER_MAX_OPCODES];    /* table of operations */
} obex_Server;

/*************************************************************************************************
 * Private function declarations (only used if absolutely necessary)
 ************************************************************************************************/

static obex_ResCode HandleConnectRequest(obex_PacketDesc *packet);
static obex_ResCode HandleDisconnectRequest(obex_PacketDesc *packet);

/*************************************************************************************************
 * Private variable definitions
 ************************************************************************************************/

/**
 * The main SERVER layer state variable (not declared 'static'
 * so as to ease debug)
 */
static obex_Server obex_server;

/**
 * Initial state of the OBEX server
 */
const obex_ServerState obex_server_initial_state = {
    false,                                   /* initially not connected */
    OBEX_CORE_BUFFER_SIZE,                   /* maximum_c2s_packet_length */
    OBEX_MINIMUM_MAX_PACKET_LENGTH,          /* maximum_s2c_packet_length */
    OBEX_OPS_NONE,                           /* initially not processing any request */
};

/**
 * Initial state of operations
 */
const obex_ServerOperationStruct obex_server_initial_operations[] = {
    /*  {opcode,              callback} */
    {OBEX_OPS_CONNECT,    HandleConnectRequest},
    {OBEX_OPS_DISCONNECT, HandleDisconnectRequest},
    {OBEX_OPS_NONE,       NULL}
};

/*************************************************************************************************
 * Public variable definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/**
 *  Go through the table of operations and find one that
 *  match the provided opcode
 *
 *  @opcode Opcode to find operation for
 *  @return The operation that was found (or NULL if none was
 *          found)
 */
static obex_ServerOperationStruct *FindOperation(obex_Opcode opcode)
{
    obex_ServerOperationStruct *operation = obex_server.operations;
    obex_ServerOperationStruct *return_value = NULL;

    /* make sure the last item in the table is an OBEX_OPS_NONE operation */
    if (OBEX_OPS_NONE != obex_server.operations[OBEX_SERVER_MAX_OPCODES-1].opcode)
    {
        ALOGE("%s: Invalid ops table 0x%x",
              __FUNCTION__,
              obex_server.operations[OBEX_SERVER_MAX_OPCODES-1].opcode);
        return return_value;
    }

    /* go through the table */
    while ( (operation->opcode != OBEX_OPS_NONE) && (NULL == return_value) )
    {
        /* opcode matches? */
        if (operation->opcode == opcode)
        {
            /* return the operation we've just found */
            return_value = operation;
        }
        else
        {
            /* move to next entry */
            operation++;
        }
    }

    if(!return_value)
    {
        obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_SERVER_FIND_CB_FOR_OPCODE_NOT_FOUND, opcode);
    }

    return return_value;
}


/**
 * Handle EOF detected on transport layer
 *
 * @return Return code
 */
static obex_ResCode HandleErrorEOF(void)
{
    return obex_CoreReInit();
}

/**
 * Handle an error
 *
 * @param res Error to handle
 * @return Return code
 */
static obex_ResCode HandleError(obex_ResCode res)
{
    obex_ResponseCodeVal response_code;

    switch (res)
    {
        case OBEX_OPS_UNSUPPORTED:
            response_code = OBEX_RESP_BAD_REQUEST_FBS;
            break;
        case OBEX_NOT_READY:
            response_code = OBEX_RESP_SERVICE_UNAVAIL_FBS;
            break;
        default:
            response_code = OBEX_RESP_INTERNAL_SERVER_ERR_FBS;
            break;
    }

    obex_CoreSendSimplePacket(response_code, NULL, 0);
    return OBEX_OK;
}

/**
 * Handle a CONNECT request
 *
 * @param packet Packet corresponding to the CONNECT request
 * @return Return code
 */
static obex_ResCode HandleConnectRequest(obex_PacketDesc *packet)
{
    obex_ResCode res;
    obex_ConnectData *connect_request_data;
    obex_ConnectData connect_response_data;
    obex_ResponseCodeVal response_code;
    void *tmp_ptr;

    /* prepare response now - the OBEX spec (section 3.4.1.8) states
    that we need to send version, flags, packet size information back
    even in case of a failure */
    connect_response_data.version_number = OBEX_VERSION_NUMBER;
    connect_response_data.flags = 0;
    connect_response_data.maximum_obex_packet_length_msb = OBEX_UINT16_MSB(OBEX_CORE_BUFFER_SIZE);
    connect_response_data.maximum_obex_packet_length_lsb = OBEX_UINT16_LSB(OBEX_CORE_BUFFER_SIZE);

    /* the following do{} while is used to avoid nested if/else statements */
    do
    {
        /* if we are already connected, throw an error response */
        if (obex_server.state.connected)
        {
            /* c.f. TC TestErrorOnTwoConsecutiveConnects */
            response_code = OBEX_RESP_PRECOND_FAILED_FBS;
            break;
        }

        /* read the request data */
        res = obex_CoreReadRequestData(packet, &tmp_ptr, OBEX_CONNECT_DATA_LEN);

        if (OBEX_OK != res)
        {
            /* c.f. TC TestErrorOnMissingRequestData */
            response_code = OBEX_RESP_BAD_REQUEST_FBS;
            break;
        }

        /* convert data to appropriate data structure */
        connect_request_data = (obex_ConnectData *)tmp_ptr;

        /* make sure there aren't any headers (as we do not know how to handle them) */
        obex_GenericHeader header;
        if (obex_CoreReadNextHeader(packet, &header) != OBEX_EOP)
        {
            /* c.f. TC TestErrorOnConnectWithHeader */
            response_code = OBEX_RESP_BAD_REQUEST_FBS;
            break;
        }

        obex_LogValue(OBEX_VERBOSE_DEBUG, OBEX_LOG_SERVER_CLIENT_VERSION_NUMBER, (uint32)connect_request_data->version_number );

        /* check OBEX version number */
        if (connect_request_data->version_number != OBEX_VERSION_NUMBER)
        {
            /* c.f. TC TestErrorOnConnectWithWrongVersionNumber */
            response_code = OBEX_RESP_HTTP_VER_NOT_SUPP_FBS;
            break;
        }

        /* check flags */
        if (connect_request_data->flags != 0)
        {
            /* c.f. TC TestErrorOnConnectWithWrongFlags */
            response_code = OBEX_RESP_FORBIDDEN_FBS;
            break;
        }

        /* handle maximum packet length */
        uint16 max_packet_length = OBEX_MSB_LSB_TO_UINT16(
                                                             connect_request_data->maximum_obex_packet_length_msb,
                                                             connect_request_data->maximum_obex_packet_length_lsb);

        obex_LogValue(OBEX_VERBOSE_INFO, OBEX_LOG_SERVER_CONNECT, (uint32)max_packet_length);

        if (max_packet_length < OBEX_MINIMUM_MAX_PACKET_LENGTH)
        {
            /* c.f. TC TestErrorOnConnectWithInvalidMaxPacketSize */
            response_code = OBEX_RESP_FORBIDDEN_FBS;
            break;
        }
        else
        {
            obex_server.state.maximum_s2c_packet_length = max_packet_length;
        }

        /* remember that we are connected to a client */
        obex_server.state.connected = true;

        /* all OK */
        response_code = OBEX_RESP_OK_FBS;
    } while (0);

    /* send response back */
    obex_CoreSendSimplePacket(response_code, (uint8*)&connect_response_data, OBEX_CONNECT_DATA_LEN);

    /*
     * return OBEX_OK regardless of the success of this operation - this is because the OBEX
     * spec requires a CONNECT response to be sent back to a CONNECT request, even if the CONNECT
     * request is badly formatted
     */
    return OBEX_OK;
}

/**
 * Handle a DISCONNECT request
 *
 * @param packet Packet corresponding to the DISCONNECT request
 * @return Return code
 */
static obex_ResCode HandleDisconnectRequest(obex_PacketDesc *packet)
{
    (void) packet;
    obex_LogMarker(OBEX_VERBOSE_INFO, OBEX_LOG_SERVER_DISCONNECT);

    /* remember that we are disconnected - this will allow
    the server to shut down gracefully */
    obex_server.state.connected = false;

    /* send response back */
    obex_CoreSendSimplePacket(OBEX_RESP_OK_FBS, NULL, 0);

    return OBEX_OK;
}

/**
 * Handle an unsupported request
 *
 * @param Packet corresponding to the unsupported operation
 * @return Return code
 */
static obex_ResCode HandleUnsupportedRequest(obex_PacketDesc *packet)
{
    obex_LogValue(OBEX_VERBOSE_DEBUG, OBEX_LOG_SERVER_UNKNOWN_OPCODE, packet->code);

    HandleError(OBEX_OPS_UNSUPPORTED);

    return OBEX_OK;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

void obex_ServerInitialise(void)
{
    /* initialise operations */
    memset(&obex_server.operations, 0, sizeof(obex_server.operations));
    memcpy(&obex_server.operations, &obex_server_initial_operations, sizeof(obex_server_initial_operations) );
}

obex_ResCode obex_ServerRun(void)
{
    obex_ResCode res;
    obex_PacketDesc packet;
    obex_ServerOperationStruct *operation;

    obex_LogMarker(OBEX_VERBOSE_INFO, OBEX_LOG_SERVER_START);

    /* initialise state of OBEX server */
    memcpy(&obex_server.state, &obex_server_initial_state, sizeof(obex_server.state) );

    do
    {
        /* wait for a packet */
        res = obex_CoreReadPacket(&packet);

        if(res == OBEX_EOF)
        {
            /** EOF on transport layer requires re-init... */
            res = HandleErrorEOF();
            if(res != OBEX_OK)
            {
                /** Fail to re-init transport layer: disconnect server... */
                obex_SetServerDisconnected();
            }
            /* skip rest and loop */
            continue;
        }

        fild_WakeLock();

        if (res != OBEX_OK)
        {
            /* cf TC TestErrorOnTooBigAPacketLength, TestErrorOnTooShortAPacketLength */
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_SERVER_ERR_INVALID_PACKET_READ, res);
            HandleError(res);

            /* skip rest and loop */
            fild_WakeUnLock();
            continue;
        }

        /*
         * the OBEX core layer has decoded the packet opcode and length and
         * written the information into the packet data structure.
         * Additionally, we know that the core layer has read the whole packet
         * so it is fine to throw an error response at any point if we are confused
         * by the contents of the packet.
         */

        /* if we are not connected, the only request we will accept is a CONNECT packet */
        if ((!obex_server.state.connected) && (packet.code != OBEX_OPS_CONNECT))
        {
            /* cf. TC TestErrorOnRequestBeforeDisconnect */

            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_SERVER_NON_CONNECT_WHILE_NOT_CONNECTED, packet.code);

            /* send error response */
            HandleError(OBEX_NOT_READY);
            /* skip rest and loop */
            fild_WakeUnLock();
            continue;
        }

        /* if we are already busy processing a command, make sure this request is the
        continuation of the previously received packets */
        if (obex_server.state.processing_request != OBEX_OPS_NONE)
        {
            if ((obex_server.state.processing_request | OBEX_FINAL_BIT_MASK) !=
                (packet.code | OBEX_FINAL_BIT_MASK))
            {
                /* cf. TC TestErrorOnNewRequestDuringRequest */
                obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_SERVER_ERR_INVALID_REQUEST);

                /* send error response */
                HandleError(OBEX_NOT_READY);
                /* skip rest and loop */
                fild_WakeUnLock();
                continue;
            }
        }

        /* check request */
        if ((operation=FindOperation(packet.code)) != NULL)
        {
            res = operation->callback(&packet);
            switch (res)
            {
            case OBEX_CONTINUE:
                /* the request needs to be continued */
                obex_server.state.processing_request = packet.code;
                break;
            case OBEX_OK:
                /* we are done with this request */
                obex_server.state.processing_request = OBEX_OPS_NONE;
                break;
            default:
                /* there was an error with this request */
                obex_server.state.processing_request = OBEX_OPS_NONE;
                HandleError(res);
                break;
            }
        }
        else /* if ((operation=FindOperation(packet.code)) != NULL) */
        {
            /* cf. TC TestErrorOnUnknownOpcode */
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_SERVER_ERR_UNSUPPORTED_REQUEST, packet.code);

            /* we do not know how to handle this type of request */
            obex_server.state.processing_request = OBEX_OPS_NONE;
            HandleUnsupportedRequest(&packet);
        }
        fild_WakeUnLock();
    } while (obex_server.state.connected);

    obex_LogMarker(OBEX_VERBOSE_INFO, OBEX_LOG_SERVER_STOP);

    return OBEX_OK;
}

obex_ResCode obex_ServerRegisterOperationCallback(obex_Opcode opcode, obex_ServerOperationCallback callback)
{
    uint32 i;

    /* sanity checks */
    if ((opcode == OBEX_OPS_NONE) || (callback == NULL))
    {
        ALOGE("%s: Invalid params 0x%x", __FUNCTION__, opcode);
        return OBEX_ERR;
    }

    /* find a free spot in the operations table */
    for (i=0; i<OBEX_SERVER_MAX_OPCODES; i++)
    {
        /* have we reached the last item? do we already have a callback
        for this opcode? */
        obex_Opcode this_opcode = obex_server.operations[i].opcode;
        if ((this_opcode==OBEX_OPS_NONE) || (this_opcode==opcode))
        {
            break;
        }
    }

    /* make sure there was a free spot left*/
    if(i < (OBEX_SERVER_MAX_OPCODES - 1))
    {
        /* move the end-of-table marker down the table (if necessary) */
        if (obex_server.operations[i].opcode==OBEX_OPS_NONE)
        {
            obex_server.operations[i+1].opcode = OBEX_OPS_NONE;
            obex_server.operations[i+1].callback = NULL;
        }

        /* register the new operation callback */
        obex_server.operations[i].opcode = opcode;
        obex_server.operations[i].callback = callback;

        return OBEX_OK;
    }
    else
    {
        obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_SERVER_ERR_REGISTRATION_FULL, opcode);
        return OBEX_ERR;
    }
}

uint16 obex_ServerMaximumS2CPacketSize(void)
{
    return obex_server.state.maximum_s2c_packet_length;
}

uint16 obex_ServerMaximumC2SPacketSize(void)
{
    return obex_server.state.maximum_c2s_packet_length;
}

void obex_SetServerDisconnected(void)
{
    obex_server.state.connected = false;
}
/** @} END OF FILE */
