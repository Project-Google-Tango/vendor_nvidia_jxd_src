/*************************************************************************************************
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 *
 * @defgroup ObexLayer OBEX Layer
 * @{
 */

/**
 * @file obex.h Obex interface.
 *
 */

#ifndef OBEX_H
#define OBEX_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

#include "obex_port.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

#define OBEX_GET_HEADER_MEANING(HD) ((HD) >> 6)

/** minimum negotiable max packet size as defined in the OBEX
 *  spec (3.4.1.4) */
#define OBEX_MINIMUM_MAX_PACKET_LENGTH (255)

/** OBEX version number */
#define OBEX_VERSION_NUMBER (0x10) /* 1.0 */

/** Final Bit Mask */
#define OBEX_FINAL_BIT_MASK (0x80)

/** Length of CONNECT packet data */
#define OBEX_CONNECT_DATA_LEN      (sizeof(obex_ConnectData))

/** Length of APP_PARAMS header data */
#define OBEX_APP_PARAMS_DATA_LEN      (sizeof(obex_AppParamsData))

/**
* Type definition to use in order to indicate that an MD5
* message digest is requested
*/
#define OBEX_MD5_HEADER_TYPE ("md5")

/**
* Start Offset Parameter ID in an APP_PARAMS header
*/
#define OBEX_APP_PARAMS_OFFSET_ID (0)

/**
* Length Parameter ID in an APP_PARAMS header
*/
#define OBEX_APP_PARAMS_LENGTH_ID (1)

/** Retrieve LSB out of a 16-bit number */
#define OBEX_UINT16_LSB(n) ((uint8)((n)&0xFF))

/** Retrieve MSB out of a 16-bit number */
#define OBEX_UINT16_MSB(n) ((uint8)(((n)>>8)&0xFF))

/** Convert MSB and LSB into a single 16-bit number */
#define OBEX_MSB_LSB_TO_UINT16(msb,lsb) (((((msb)&0xFF))<<8)|((lsb)&0xFF))

/** Convert a big-endian series of 4 bytes into a single 32-bit
 *  number */
#define OBEX_MSB_msb_lsb_LSB_TO_UINT32(MSB,msb,lsb,LSB) (\
     (((MSB)&0xFF)<<24) \
    |(((msb)&0xFF)<<16) \
    |(((lsb)&0xFF)<<8)  \
    |(((LSB)&0xFF)))

/** Retrieve most significant byte out of a 32-bit number */
#define OBEX_UINT32_MSB(n) (((n)>>24)&0xFF)

/** Retrieve 2nd most significant byte out of a 32-bit number */
#define OBEX_UINT32_msb(n) (((n)>>16)&0xFF)

/** Retrieve 2nd least significant byte out of a 32-bit
 *  number */
#define OBEX_UINT32_lsb(n) (((n)>>8)&0xFF)

/** Retrieve least significant byte out of a 32-bit number */
#define OBEX_UINT32_LSB(n) ((n)&0xFF)

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/

/**
 * Describe the OBEX operations.
 * FBS stands for Final Bit Set (indicates final packet of request).
 */
typedef enum
{
    OBEX_OPS_NONE                        = 0x00, /**< Dummy request - used for control flow               */
    OBEX_OPS_CONNECT                     = 0x80, /**< Choose your partner, negotiate capabilities         */
    OBEX_OPS_DISCONNECT                  = 0x81, /**< Signal the end of the session                       */
    OBEX_OPS_PUT                         = 0x02, /**< Send an object                                      */
    OBEX_OPS_PUT_FINAL                   = 0x82, /**< Send an object - FBS                                */
    OBEX_OPS_GET                         = 0x03, /**< Get an object                                       */
    OBEX_OPS_GET_FINAL                   = 0x83, /**< Get an object - FBS                                 */
    OBEX_OPS_SET_PATH                    = 0x85, /**< Modifies the current path on the receiving side     */
    OBEX_OPS_ACTION                      = 0x06, /**< Common actions to be performed by the target        */
    OBEX_OPS_ACTION_FINAL                = 0x86, /**< Common actions to be performed by the target - FBS  */
    OBEX_OPS_SESSION                     = 0x87, /**< Used for reliable session support                   */
    OBEX_OPS_ABORT                       = 0xFF, /**< Abort the current operation                         */
} obex_Opcode;

/**
 * Enumeration of header types
 */
typedef enum
{
    OBEX_OBJHDTYPE_UNICODE_SEQUENCE=0,           /**< Null terminated Unicode text, length prefixed with 2 byte unsigned interger */
    OBEX_OBJHDTYPE_BYTE_SEQUENCE=1,              /**< Byte sequence, length prefixed with 2 byte unsigned interger */
    OBEX_OBJHDTYPE_1BYTE=2,                      /**< 1 byte quantity */
    OBEX_OBJHDTYPE_4BYTES=3,                     /**< 4 byte quantity */
} obex_ObjHdrType;

/**
 * Describe the obex oject header identifiers
 */
typedef enum
{
    OBEX_OBJHDID_COUNT                    = 0xC0,    /**< Number of objects (used by Connect)                                        */
    OBEX_OBJHDID_NAME                     = 0x01,    /**< Name of the object (often a file name)                                     */
    OBEX_OBJHDID_TYPE                     = 0x42,    /**< Type of object - e.g. text, html, binary, manufacturer specific            */
    OBEX_OBJHDID_LENGTH                   = 0xC3,    /**< The length of the object in bytes                                          */
    OBEX_OBJHDID_TIME_ISO8601             = 0x44,    /**< Date/time stamp ­ ISO 8601 version - preferred                             */
    OBEX_OBJHDID_TIME_4BYTES              = 0xC4,    /**< Date/time stamp ­ 4 byte version (for compatibility only)                  */
    OBEX_OBJHDID_DESCRIPTION              = 0x05,    /**< Text description of the object                                             */
    OBEX_OBJHDID_TARGET                   = 0x46,    /**< Name of service that operation is targeted to                              */
    OBEX_OBJHDID_HTTP                     = 0x47,    /**< An HTTP 1.x header                                                         */
    OBEX_OBJHDID_BODY                     = 0x48,    /**< A chunk of the object body.                                                */
    OBEX_OBJHDID_END_OF_BODY              = 0x49,    /**< the final chunk of the object body                                         */
    OBEX_OBJHDID_WHO                      = 0x4A,    /**< Identifies the OBEX application, used to tell if talking to a peer         */
    OBEX_OBJHDID_CONNECT_ID               = 0xCB,    /**< An identifier used for OBEX connection multiplexing                        */
    OBEX_OBJHDID_APP_PARAMS               = 0x4C,    /**< Extended application request & response information                        */
    OBEX_OBJHDID_AUTH_CHAL                = 0x4D,    /**< Authentication digest-challenge                                            */
    OBEX_OBJHDID_AUTH_RESP                = 0x4E,    /**< Authentication digest-response                                             */
    OBEX_OBJHDID_CREATOR_ID               = 0xCF,    /**< Indicates the creator of an object                                         */
    OBEX_OBJHDID_WAN_UUID                 = 0x50,    /**< Uniquely identifies the network client (OBEX server)                       */
    OBEX_OBJHDID_OBJECT_CLASS             = 0x51,    /**< OBEX Object class of object                                                */
    OBEX_OBJHDID_SESSION_PARAMS           = 0x52,    /**< Parameters used in session commands/responses                              */
    OBEX_OBJHDID_SESSION_SEQ_NUM          = 0x93,    /**< Sequence number used in each OBEX packet for reliability                   */
    OBEX_OBJHDID_ACTION_ID                = 0x94,    /**< Specifies the action to be performed (used in ACTION operation)            */
    OBEX_OBJHDID_DESTNAME                 = 0x15,    /**< The destination object name (used in certain ACTION operations)            */
    OBEX_OBJHDID_PERMISSIONS              = 0xD6,    /**< 4-byte bit mask for setting permissions                                    */
    OBEX_OBJHDID_SINGLE_RESP_MODE         = 0x97,    /**< 1-byte value to setup Single Response Mode (SRM)                           */
    OBEX_OBJHDID_SINGLE_RESP_MODE_PARAMS  = 0x98,    /**< 1-byte value for setting parameters used during Single Response Mode (SRM) */
} obex_ObjHdrId;

/**
 * Describe the obex session parameter tag
 */
typedef enum
{
    /**
     * The device address of the device sending the header. If running over IrDA
     * this is the 32-bit device address. For Bluetooth this is the 48-bit Bluetooth
     * address. If Running over TCP/IP this is the IP address.
     */
    OBEX_SPHD_DEVICE_ADDR               = 0x00,
    /**
     * The nonce is a value provided by the device, which will be used in the
     * creation of the session ID. This number must be unique for each session
     * created by the device. One method for creating the nonce is to start with a
     * random number then increment the value each time a new session is
     * created. The Nonce should be at least 4 bytes and at most 16 bytes in size.
     */
    OBEX_SPHD_NONCE                     = 0x01,
    /**
     * This is a 16-byte value, which is formed by taking the device
     * address and nonce from the client and server and running the MD5 algorithm
     * over the resulting string. The Session ID is created as follows: MD5("Client
     * Device Address" "Client Nonce" "Server Device Address" "Server Nonce")
     */
    OBEX_SPHD_SESSION_ID                = 0x02,
    /**
     * This is a one-byte value sent by the server, which indicates the next
     * sequence number expected when the session is resumed.
     */
    OBEX_SPHD_NEXT_SEQUENCE_NUMBER      = 0x03,
    /**
     * 0x04 Timeout This is a 4-byte value that contains the number of seconds a session can be
     * in suspend mode before it is considered closed. The value of 0xffffffff
     * indicates a timeout of infinity. This is the default timeout. If a device does not
     * send a timeout field then it can be assumed that the desired timeout is
     * infinity. The timeout in affect is the smallest timeout sent by the client or
     * server.
     */
    OBEX_SPHD_TIMEOUT                   = 0x04,
    /**
     * The session opcode is a 1-byte value sent by the client to indicate the
     * Session command that is to be performed. This tag-length-value is only sent
     * in SESSION commands and must be the first tag in the Session-
     * Parameters header. The session opcode definitions are defined in the
     * bulleted list below.
     */
    OBEX_SPHD_SESSION_OPCODE            = 0x05,
} obex_SessionParamsHdrId;

/**
 * Describe the session opcodes
 */
typedef enum
{
    OBEX_SESSION_OPCODE_CREATE_SESSION   = 0x00,
    OBEX_SESSION_OPCODE_CLOSE_SESSION    = 0x01,
    OBEX_SESSION_OPCODE_SUSPEND_SESSION  = 0x02,
    OBEX_SESSION_OPCODE_RESUME_SESSION   = 0x03,
    OBEX_SESSION_OPCODE_SET_TIMEOUT      = 0x04,
} obex_SessionOpcodeId;

/**
 * Describe the actions
 */
typedef enum
{
    OBEX_ACTION_COPY                     = 0x00,
    OBEX_ACTION_MOVE_RENAME              = 0x01,
    OBEX_ACTION_SET_PERM                 = 0x02,
    OBEX_ACTION_RESIZE                   = 0x80, /* Proprietary action */
} obex_ObjectActionId;

/**
 * OBEX response codes values
 */
typedef enum
{
    OBEX_RESP_CONT                       = 0x10, /**< Continue                                                */
    OBEX_RESP_CONT_FBS                   = 0x90, /**< Continue - FBS                                          */
    OBEX_RESP_OK                         = 0x20, /**< OK, Success                                             */
    OBEX_RESP_OK_FBS                     = 0xA0, /**< OK, Success - FBS                                       */
    OBEX_RESP_CREATED                    = 0x21, /**< Created                                                 */
    OBEX_RESP_CREATED_FBS                = 0xA1, /**< Created - FBS                                           */
    OBEX_RESP_ACCEPTED                   = 0x22, /**< Accepted                                                */
    OBEX_RESP_ACCEPTED_FBS               = 0xA2, /**< Accepted - FBS                                          */
    OBEX_RESP_NON_AUTH_INF               = 0x23, /**< Non-Authoritative Information                           */
    OBEX_RESP_NON_AUTH_INF_FBS           = 0xA3, /**< Non-Authoritative Information - FBS                     */
    OBEX_RESP_NO_CONTENT                 = 0x24, /**< No Content                                              */
    OBEX_RESP_NO_CONTENT_FBS             = 0xA4, /**< No Content - FBS                                        */
    OBEX_RESP_RESET_CONTENT              = 0x25, /**< Reset Content                                           */
    OBEX_RESP_RESET_CONTENT_FBS          = 0xA5, /**< Reset Content - FBS                                     */
    OBEX_RESP_PARTIAL_CONTENT            = 0x26, /**< Partial Content                                         */
    OBEX_RESP_PARTIAL_CONTENT_FBS        = 0xA6, /**< Partial Content - FBS                                   */

    OBEX_RESP_MULTIPLE_CHOICES           = 0x30, /**< Multiple Choices                                        */
    OBEX_RESP_MULTIPLE_CHOICES_FBS       = 0xB0, /**< Multiple Choices - FBS                                  */
    OBEX_RESP_MOVED_PERM                 = 0x31, /**< Moved Permanently                                       */
    OBEX_RESP_MOVED_PERM_FBS             = 0xB1, /**< Moved Permanently - FBS                                 */
    OBEX_RESP_MOVED_TEMP                 = 0x32, /**< Moved temporarily                                       */
    OBEX_RESP_MOVED_TEMP_FBS             = 0xB2, /**< Moved temporarily - FBS                                 */
    OBEX_RESP_SEE_OTHER                  = 0x33, /**< See Other                                               */
    OBEX_RESP_SEE_OTHER_FBS              = 0xB3, /**< See Other - FBS                                         */
    OBEX_RESP_NOT_MODIFIED               = 0x34, /**< Not modified                                            */
    OBEX_RESP_NOT_MODIFIED_FBS           = 0xB4, /**< Not modified - FBS                                      */
    OBEX_RESP_USE_PROXY                  = 0x35, /**< Use Proxy                                               */
    OBEX_RESP_USE_PROXY_FBS              = 0xB5, /**< Use Proxy - FBS                                         */

    OBEX_RESP_BAD_REQUEST                = 0x40, /**< Bad Request - server couldn't understand request        */
    OBEX_RESP_BAD_REQUEST_FBS            = 0xC0, /**< Bad Request - server couldn't understand request - FBS  */
    OBEX_RESP_UNAUTHORIZED               = 0x41, /**< Unauthorized                                            */
    OBEX_RESP_UNAUTHORIZED_FBS           = 0xC1, /**< Unauthorized - FBS                                      */
    OBEX_RESP_PAYMENT_REQUIRED           = 0x42, /**< Payment required                                        */
    OBEX_RESP_PAYMENT_REQUIRED_FBS       = 0xC2, /**< Payment required - FBS                                  */
    OBEX_RESP_FORBIDDEN                  = 0x43, /**< Forbidden - operation is understood but refused         */
    OBEX_RESP_FORBIDDEN_FBS              = 0xC3, /**< Forbidden - operation is understood but refused - FBS   */
    OBEX_RESP_NOT_FOUND                  = 0x44, /**< Not Found                                               */
    OBEX_RESP_NOT_FOUND_FBS              = 0xC4, /**< Not Found - FBS                                         */
    OBEX_RESP_METHOD_NOT_ALLOWED         = 0x45, /**< Method not allowed                                      */
    OBEX_RESP_METHOD_NOT_ALLOWED_FBS     = 0xC5, /**< Method not allowed - FBS                                */
    OBEX_RESP_NOT_ACCEPTABLE             = 0x46, /**< Not Acceptable                                          */
    OBEX_RESP_NOT_ACCEPTABLE_FBS         = 0xC6, /**< Not Acceptable - FBS                                    */
    OBEX_RESP_PROXY_AUTH_REQUIRED        = 0x47, /**< Proxy Authentication required                           */
    OBEX_RESP_PROXY_AUTH_REQUIRED_FBS    = 0xC7, /**< Proxy Authentication required - FBS                     */
    OBEX_RESP_REQUEST_TIMEOUT            = 0x48, /**< Request Time Out                                        */
    OBEX_RESP_REQUEST_TIMEOUT_FBS        = 0xC8, /**< Request Time Out - FBS                                  */
    OBEX_RESP_CONFLICT                   = 0x49, /**< Conflict                                                */
    OBEX_RESP_CONFLICT_FBS               = 0xC9, /**< Conflict - FBS                                          */
    OBEX_RESP_GONE                       = 0x4A, /**< Gone                                                    */
    OBEX_RESP_GONE_FBS                   = 0xCA, /**< Gone - FBS                                              */
    OBEX_RESP_LENGTH_REQUIRED            = 0x4B, /**< Length Required                                         */
    OBEX_RESP_LENGTH_REQUIRED_FBS        = 0xCB, /**< Length Required - FBS                                   */
    OBEX_RESP_PRECOND_FAILED             = 0x4C, /**< Precondition failed                                     */
    OBEX_RESP_PRECOND_FAILED_FBS         = 0xCC, /**< Precondition failed - FBS                               */
    OBEX_RESP_REQ_ENTITY_TOO_LARGE       = 0x4D, /**< Requested entity too large                              */
    OBEX_RESP_REQ_ENTITY_TOO_LARGE_FBS   = 0xCD, /**< Requested entity too large - FBS                        */
    OBEX_RESP_REQ_URL_TOO_LARGE          = 0x4E, /**< Request URL too large                                   */
    OBEX_RESP_REQ_URL_TOO_LARGE_FBS      = 0xCE, /**< Request URL too large - FBS                             */
    OBEX_RESP_UNSUP_MEDIA_TYPE           = 0x4F, /**< Unsupported media type                                  */
    OBEX_RESP_UNSUP_MEDIA_TYPE_FBS       = 0xCF, /**< Unsupported media type - FBS                            */

    OBEX_RESP_INTERNAL_SERVER_ERR        = 0x50, /**< Internal Server Error                                   */
    OBEX_RESP_INTERNAL_SERVER_ERR_FBS    = 0xD0, /**< Internal Server Error - FBS                             */
    OBEX_RESP_NOT_IMPLEMENTED            = 0x51, /**< Not Implemented                                         */
    OBEX_RESP_NOT_IMPLEMENTED_FBS        = 0xD1, /**< Not Implemented - FBS                                   */
    OBEX_RESP_BAD_GATEWAY                = 0x52, /**< Bad Gateway                                             */
    OBEX_RESP_BAD_GATEWAY_FBS            = 0xD2, /**< Bad Gateway - FBS                                       */
    OBEX_RESP_SERVICE_UNAVAIL            = 0x53, /**< Service Unavailable                                     */
    OBEX_RESP_SERVICE_UNAVAIL_FBS        = 0xD3, /**< Service Unavailable - FBS                               */
    OBEX_RESP_GATEWAY_TIMEOUT            = 0x54, /**< Gateway Timeout                                         */
    OBEX_RESP_GATEWAY_TIMEOUT_FBS        = 0xD4, /**< Gateway Timeout - FBS                                   */
    OBEX_RESP_HTTP_VER_NOT_SUPP          = 0x55, /**< HTTP version not supported                              */
    OBEX_RESP_HTTP_VER_NOT_SUPP_FBS      = 0xD5, /**< HTTP version not supported - FBS                        */

    OBEX_RESP_DB_FULL                    = 0x60, /**< Database Full                                           */
    OBEX_RESP_DB_FULL_FBS                = 0xE0, /**< Database Full - FBS                                     */
    OBEX_RESP_DB_LOCKED                  = 0x61, /**< Database Locked                                         */
    OBEX_RESP_DB_LOCKED_FBS              = 0xE1, /**< Database Locked - FBS                                   */
} obex_ResponseCodeVal;


/**
 * OBEX status codes
 */
typedef enum
{
    OBEX_OK=0,
    OBEX_ERR,
    OBEX_CONTINUE,                           /**< The currently processed operation needs to be continued */
    OBEX_NOT_READY,                          /**< Initialization incomplete or missing */
    OBEX_DATA_TOO_BIG,                       /**< Packet larger than we can cope with */
    OBEX_INVALID,                            /**< The obex packet format is invalid. Should not occur */
    OBEX_HDR_UNKNOWN,                        /**< Unknown header */
    OBEX_HDR_UNSUPPORTED,                    /**< This header is not yet supported. Requires new dev. */
    OBEX_OPS_UNKNOWN,                        /**< Unknown operation */
    OBEX_OPS_UNSUPPORTED,                    /**< This operation is not yet supported. Requires new dev. */
    OBEX_EOP,                                /**< End Of Packet reached */
    OBEX_EOF,                                /**< End Of File reached on transport layer */
} obex_ResCode;

/**
 * Type definition for a unicode character (network byte order)
 */
typedef struct
{
    uint8 msb;
    uint8 lsb;
} obex_UnicodeCharacter;

/**
 * Generic type for a header
 */
typedef struct
{
    obex_ObjHdrId id;                         /**< Header id */
    obex_ObjHdrType type;                     /**< Header type */
    union                                        /**< Header value */
    {
        uint8 val_1b;
        uint32 val_4b;
        struct
        {
            uint32 byte_length;
            uint8 *ptr;
        } byte_sequence;
        struct
        {
            uint32 character_length;
            obex_UnicodeCharacter *ptr;
        } unicode_sequence;
    } value;                                     /**< Header value (union) */
} obex_GenericHeader;

/**
 * obex_ packet
 */
typedef struct
{
    uint8      code;                             /**< Obex Operation or Response Code */
    uint16     packet_length;                    /**< Packet Length */
    uint16     payload_length;                   /**< Packet Length */
} obex_PacketDesc;

/**
 * OBEX CONNECT packet data
 */
typedef struct
{
    uint8      version_number;                   /**< version number, must be OBEX_VERSION_NUMBER */
    uint8      flags;                            /**< flags, must be 0 */
    uint8      maximum_obex_packet_length_msb;   /**< max packet length, most significant byte */
    uint8      maximum_obex_packet_length_lsb;   /**< max packet length, least significant byte */
} obex_ConnectData;

/**
 * OBEX APP_PARAMS header data (cf.
 * FS005_OBEX_server_for_firmware_update)
 */
typedef struct
{
    uint8      start_offset_parameter_id;        /**< start offset param id, must be OBEX_APP_PARAMS_OFFSET_ID*/
    uint8      start_offset_parameter_length;    /**< start offset param length, must be 4 */
    uint8      start_offset_byte0;               /**< offset, most significant byte */
    uint8      start_offset_byte1;               /**< offset, 2nd most significant byte */
    uint8      start_offset_byte2;               /**< offset, 2nd least significant byte */
    uint8      start_offset_byte3;               /**< offset, least significant byte */
    uint8      length_parameter_id;              /**< length param id, must be OBEX_APP_PARAMS_LENGTH_ID*/
    uint8      length_parameter_length;          /**< length param length, must be 4 */
    uint8      length_byte0;                     /**< length, most significant byte */
    uint8      length_byte1;                     /**< length, 2nd most significant byte */
    uint8      length_byte2;                     /**< length, 2nd least significant byte */
    uint8      length_byte3;                     /**< length, least significant byte */
} obex_AppParamsData;

/**
 * Dir entry struct used by OBEX code.
 */
typedef struct
{
    char          d_name[MAX_FILENAME_LENGTH];  /** file name */
    unsigned int  d_fileno;                     /** file serial number */
    int           d_namlen;                     /** length of the file name */
    int           d_type;                       /** type of file */
} obex_DirEnt;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/


#endif /* #ifndef OBEX_H */

/** @} END OF FILE */
