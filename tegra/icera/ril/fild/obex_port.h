/*************************************************************************************************
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 *
 * @addtogroup ObexLayer
 * @{
 */

/**
 * @file obex_server.h Obex os abs layer.
 *
 */

#ifndef OBEX_PORT_H
#define OBEX_PORT_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/
#ifdef __dxp__
#include "icera_global.h"
#include "drv_security.h"
#else
#include <stdbool.h>  /** bool */
#include <dirent.h>   /** DIR,... */

#include "fild_port.h"
#endif

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

#ifndef __dxp__
/* Some defines to remove small drivers code dependencies */
#define MD5_DIGEST_SIZE     16
#define DXP_CACHED_UNI1
#define MAX_FILENAME_LENGTH 128
#define DRV_FS_SEPARATOR "/"
#endif

/** Redefine COM_MIN to avoid common code dependency */
#define COM_MIN(A,B)    (((A)<(B))?(A):(B))

#ifndef __S_IFMT
#define __S_IFMT 0170000
#endif
#ifndef __S_IFDIR
#define __S_IFDIR 0040000
#endif
#ifndef __S_ISTYPE
#define __S_ISTYPE(mode, mask) (((mode) & __S_IFMT) == (mask))
#endif

#define OBEX_ISDIR(stat) __S_ISTYPE((stat.st_mode), __S_IFDIR)

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/
#ifdef __dxp__
typedef struct xMD5Context obex_MD5Context;
#else
typedef int obex_MD5Context;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

typedef DIR obex_Dir;
typedef struct dirent obex_OsDirEnt;
typedef struct stat obex_Stat;
#endif

/**
 * enumeration of OBEX log levels
 */
typedef enum
{
    OBEX_VERBOSE_ERROR,
    OBEX_VERBOSE_INFO,
    OBEX_VERBOSE_WARNING,
    OBEX_VERBOSE_VERBOSE,
    OBEX_VERBOSE_DEBUG,
} obex_LogLevel;

/**
 * enumeration of OBEX log points
 */
typedef enum
{
    /* Obex Core log points */
     OBEX_LOG_CORE_CODE = 0
    ,OBEX_LOG_CORE_PACKET_LENGTH
    ,OBEX_LOG_CORE_HEADER_LENGTH
    ,OBEX_LOG_CORE_HEADER_BYTES
    ,OBEX_LOG_CORE_HEADER_UNICODE
    ,OBEX_LOG_CORE_HEADER_1_BYTE
    ,OBEX_LOG_CORE_HEADER_4_BYTES
    ,OBEX_LOG_CORE_ERR_NOT_PACKET_LENGTH_TOO_SMALL
    ,OBEX_LOG_CORE_ERR_NOT_PACKET_LENGTH_TOO_BIG
    ,OBEX_LOG_CORE_ERR_NOT_ENOUGH_BYTES_LEFT_IN_PACKET
    ,OBEX_LOG_CORE_ERR_UNICODE_SEQUENCE_WITH_ODD_LENGTH
    ,OBEX_LOG_CORE_ERR_UNICODE_SEQUENCE_NOT_NULL_TERMINATED

    /* Obex file log points */
    ,OBEX_LOG_FILE_NAME_HEADER
    ,OBEX_LOG_FILE_TYPE_MD5_HEADER
    ,OBEX_LOG_FILE_LENGTH_HEADER
    ,OBEX_LOG_FILE_ACTIONID_HEADER
    ,OBEX_LOG_FILE_BODY_HEADER
    ,OBEX_LOG_FILE_END_OF_BODY_HEADER
    ,OBEX_LOG_FILE_APP_PARAMS_HEADER_START_OFFSET
    ,OBEX_LOG_FILE_APP_PARAMS_HEADER_LENGTH
    ,OBEX_LOG_FILE_PUT
    ,OBEX_LOG_FILE_PUT_FINAL
    ,OBEX_LOG_FILE_GET
    ,OBEX_LOG_FILE_GET_FINAL
    ,OBEX_LOG_FILE_ACTION_FINAL
    ,OBEX_LOG_FILE_DEST_NAME_HEADER
    ,OBEX_LOG_FILE_RESET_GLOBAL_STATE
    ,OBEX_LOG_DIR_EMPTY
    ,OBEX_LOG_FILE_ERR_TWO_NAME_HEADERS
    ,OBEX_LOG_FILE_ERR_FILENAME_TOO_LONG
    ,OBEX_LOG_FILE_ERR_FILE_ALREADY_OPEN
    ,OBEX_LOG_FILE_ERR_UNSUPPORTED_TYPE
    ,OBEX_LOG_FILE_ERR_MISSING_FILENAME
    ,OBEX_LOG_FILE_ERR_FAILED_TO_OPEN
    ,OBEX_LOG_FILE_ERR_FAILED_TO_LGETSIZE
    ,OBEX_LOG_FILE_ERR_START_OFF_TOO_BIG
    ,OBEX_LOG_FILE_ERR_FAILED_TO_SEEK_IN_FILE
    ,OBEX_LOG_FILE_ERR_FAILED_TO_WRITE_TO_FILE
    ,OBEX_LOG_FILE_ERR_INVALID_APP_PARAMS
    ,OBEX_LOG_FILE_ERR_UNSUPPORTED_HEADER
    ,OBEX_LOG_FILE_ERR_FAILED_TO_READ_FROM_FILE
    ,OBEX_LOG_FILE_ERR_CHUNK_EXCEEDS_FILE_BOUNDARY
    ,OBEX_LOG_FILE_ERR_MISSING_LENGTH
    ,OBEX_LOG_FILE_ERR_FAILED_TO_TRUNCATE_FILE
    ,OBEX_LOG_FILE_ERR_MISSING_DESTNAME
    ,OBEX_LOG_FILE_ERR_SAME_SOURCE_AND_DEST_NAMES
    ,OBEX_LOG_FILE_ERR_FAILED_TO_COPY_FILE
    ,OBEX_LOG_FILE_ERR_FAILED_TO_RENAME_FILE
    ,OBEX_LOG_FILE_ERR_UNSUPPORTED_ACTION_ID
    ,OBEX_LOG_FILE_ERR_NULL_HDR
    ,OBEX_LOG_FILE_ERR_NULL_FILENAME
    ,OBEX_LOG_FILE_ERR_HDR_NOT_UNICODE_SEQUENCE
    ,OBEX_LOG_FILE_ERR_FILENAME_NOT_NULL_TERMINATED
    ,OBEX_LOG_FILE_ERR_GET_GET_FINAL_EXPECTED
    ,OBEX_LOG_FILE_ERR_HDR_NOT_BYTE_SEQUENCE
    ,OBEX_LOG_FILE_ERR_HDR_INVALID_MD5
    ,OBEX_LOG_FILE_ERR_NULL_PACKET
    ,OBEX_LOG_FILE_ERR_ACTION_FINAL_EXPECTED
    ,OBEX_LOG_FILE_ERR_HDR_NOT_4BYTES_SEQUENCE
    ,OBEX_LOG_FILE_ERR_HDR_INVALID_LENGTH
    ,OBEX_LOG_FILE_ERR_HDR_NOT_1BYTE_SEQUENCE
    ,OBEX_LOG_FILE_ERR_PUT_PUT_FINAL_EXPECTED
    ,OBEX_LOG_FILE_ERR_HDR_NULL_BYTE_SEQUENCE
    ,OBEX_LOG_FILE_ERR_PUT_PUT_FINAL_GET_GET_FINAL_EXPECTED
    ,OBEX_LOG_FILE_ERR_HDR_INVALID_BYTE_LENGTH
    ,OBEX_LOG_FILE_ERR_PUT_PROCESSING_HEADERS
    ,OBEX_LOG_FILE_ERR_PUT_REMOVE_FAILED
    ,OBEX_LOG_FILE_ERR_PUT_PACKET_WITH_NO_NAME
    ,OBEX_LOG_FILE_ERR_PACKET_CREATION_FAILURE
    ,OBEX_LOG_FILE_ERR_HDR_APPEND_FAILURE
    ,OBEX_LOG_FILE_ERR_PACKET_SEND_FAILURE
    ,OBEX_LOG_FILE_ERR_END_OF_PACKET_NOT_REACHED
    ,OBEX_LOG_FILE_ERR_NULL_FUNCTIONS
    ,OBEX_LOG_FILE_ERR_INVALID_GET
    ,OBEX_LOG_FILE_ERR_OBJ_STAT_FAILURE
    ,OBEX_LOG_DIR_ERR_FAILED_TO_OPEN
    ,OBEX_LOG_DIR_ERR_TOO_MUCH_ENTRIES
    ,OBEX_LOG_DIR_ERR_TOO_LONG_DIRNAME


    /* Obex server log points */
    ,OBEX_LOG_SERVER_CLIENT_VERSION_NUMBER
    ,OBEX_LOG_SERVER_CONNECT
    ,OBEX_LOG_SERVER_DISCONNECT
    ,OBEX_LOG_SERVER_UNKNOWN_OPCODE
    ,OBEX_LOG_SERVER_START
    ,OBEX_LOG_SERVER_NON_CONNECT_WHILE_NOT_CONNECTED
    ,OBEX_LOG_SERVER_STOP
    ,OBEX_LOG_SERVER_FIND_CB_FOR_OPCODE
    ,OBEX_LOG_SERVER_FIND_CB_FOR_OPCODE_NOT_FOUND
    ,OBEX_LOG_SERVER_FIND_CB_FOR_OPCODE_FOUND
    ,OBEX_LOG_SERVER_ERR_INVALID_PACKET_READ
    ,OBEX_LOG_SERVER_ERR_INVALID_REQUEST
    ,OBEX_LOG_SERVER_ERR_UNSUPPORTED_REQUEST
    ,OBEX_LOG_SERVER_ERR_INVALID_OPERATION
    ,OBEX_LOG_SERVER_ERR_OPERATION_FAILED
    ,OBEX_LOG_SERVER_ERR_REGISTRATION_FULL
    ,OBEX_LOG_SERVER_ERR_REPORT

    ,OBEX_LOG_NUM_LOG_POINTS

} obex_LogPoints;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

/**
 * Initialise an MD5 context
 *
 * @param context Pointer to obex_MD5Context data structure to
 *                initialise
 */
void obex_MD5Init(obex_MD5Context *context);

/**
 * Update an MD5 context (previously initialised through a call
 * to obex_MD5Init) with the buffer passed in the parameter list
 *
 * @param context Pointer to the initialised MD5 context
 * @param buf Buffer to update the context with
 * @param len Size (in bytes) of the buffer
 */
void obex_MD5Update(obex_MD5Context *context, unsigned char const *buf, int len);

/**
 * Finalise an MD5 context, previously initialised with a call
 * to obex_MD5Init() and populated with calls to obex_MD5Update()
 *
 * @param digest Pointer to a 16-byte buffer that will be
 *               populated with the MD5 hash
 * @param context Pointer to the MD4 context
 */
void obex_MD5Final(unsigned char digest[16], obex_MD5Context *context);

/**
 *
 *
 * @param level
 * @param log_point
 */
void obex_LogMarker(unsigned int level, unsigned int log_point);

/**
 *
 *
 * @param level
 * @param log_point
 * @param value
 */
void obex_LogValue(unsigned int level, unsigned int log_point, unsigned int value);

/**
 * Sleep a given amount of seconds
 *
 * @param seconds
 */
void obex_Sleep(unsigned int seconds);

#endif /* #ifndef OBEX_PORT_H */

/** @} END OF FILE */
