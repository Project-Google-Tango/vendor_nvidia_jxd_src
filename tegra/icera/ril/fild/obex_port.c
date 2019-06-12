/*************************************************************************************************
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup ObexDriver
 * @{
 */

/**
 * @file drv_obex_port.c Obex abstraction layer
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "obex.h"
#include "obex_file.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Private function declarations (only used if absolutely necessary)
 ************************************************************************************************/

/*************************************************************************************************
 * Private variable definitions
 ************************************************************************************************/

static const char *log_point_map_table[OBEX_LOG_NUM_LOG_POINTS] ={

    /* Obex Core log points */
    [OBEX_LOG_CORE_CODE]                                      = "OBEX: Core: Received packet with code"
    ,[OBEX_LOG_CORE_PACKET_LENGTH]                            = "OBEX: Core: Received packet with packet length"
    ,[OBEX_LOG_CORE_HEADER_LENGTH]                            = "OBEX: Core: Received header - LENGTH"
    ,[OBEX_LOG_CORE_HEADER_BYTES]                             = "OBEX: Core: Received byte-sequence header - ID"
    ,[OBEX_LOG_CORE_HEADER_UNICODE]                           = "OBEX: Core: Received unicode header - ID"
    ,[OBEX_LOG_CORE_HEADER_1_BYTE]                            = "OBEX: Core: Received 1-byte header - ID"
    ,[OBEX_LOG_CORE_HEADER_4_BYTES]                           = "OBEX: Core: Received 4-byte header - ID"
    ,[OBEX_LOG_CORE_ERR_NOT_PACKET_LENGTH_TOO_SMALL]          = "OBEX: ERR: Packet length too small"
    ,[OBEX_LOG_CORE_ERR_NOT_PACKET_LENGTH_TOO_BIG]            = "OBEX: ERR: Packet length too big"
    ,[OBEX_LOG_CORE_ERR_NOT_ENOUGH_BYTES_LEFT_IN_PACKET]      = "OBEX: ERR: Not enough bytes left in packet"
    ,[OBEX_LOG_CORE_ERR_UNICODE_SEQUENCE_WITH_ODD_LENGTH]     = "OBEX: ERR: Unicode sequence with odd length"
    ,[OBEX_LOG_CORE_ERR_UNICODE_SEQUENCE_NOT_NULL_TERMINATED] = "OBEX: ERR: Unicode sequence not NULL terminated"

    /* Obex file log points */
    ,[OBEX_LOG_FILE_PUT]                                      = "OBEX: File: PUT request"
    ,[OBEX_LOG_FILE_PUT_FINAL]                                = "OBEX: File: PUT_FINAL request"
    ,[OBEX_LOG_FILE_GET]                                      = "OBEX: File: GET request"
    ,[OBEX_LOG_FILE_GET_FINAL]                                = "OBEX: File: GET_FINAL request"
    ,[OBEX_LOG_FILE_ACTION_FINAL]                             = "OBEX: File: ACTION_FINAL request"
    ,[OBEX_LOG_FILE_NAME_HEADER]                              = "OBEX: File: NAME header with filename length"
    ,[OBEX_LOG_FILE_DEST_NAME_HEADER]                         = "OBEX: File: DESTNAME header with filename length"
    ,[OBEX_LOG_FILE_BODY_HEADER]                              = "OBEX: File: BODY header with content length"
    ,[OBEX_LOG_FILE_TYPE_MD5_HEADER]                          = "OBEX: File: TYPE header for MD5 hash"
    ,[OBEX_LOG_FILE_LENGTH_HEADER]                            = "OBEX: File: LENGTH header with length"
    ,[OBEX_LOG_FILE_ACTIONID_HEADER]                          = "OBEX: File: ACTIONID header with id"
    ,[OBEX_LOG_FILE_END_OF_BODY_HEADER]                       = "OBEX: File: END_OF_BODY header with content length"
    ,[OBEX_LOG_FILE_APP_PARAMS_HEADER_START_OFFSET]           = "OBEX: File: APP_PARAMS header with start offset"
    ,[OBEX_LOG_FILE_APP_PARAMS_HEADER_LENGTH]                 = "OBEX: File: APP_PARAMS header with start length"
    ,[OBEX_LOG_FILE_RESET_GLOBAL_STATE]                       = "OBEX: File: Reset global state"
    ,[OBEX_LOG_DIR_EMPTY]                                     = "OBEX: File: Empty directory"
    ,[OBEX_LOG_FILE_ERR_NULL_HDR]                             = "OBEX: File: NULL header"
    ,[OBEX_LOG_FILE_ERR_NULL_FILENAME]                        = "OBEX: File: NULL filename"
    ,[OBEX_LOG_FILE_ERR_HDR_NOT_UNICODE_SEQUENCE]             = "OBEX: File: Not unicode sequence header"
    ,[OBEX_LOG_FILE_ERR_FILENAME_NOT_NULL_TERMINATED]         = "OBEX: File: Invalid filename"
    ,[OBEX_LOG_FILE_ERR_GET_GET_FINAL_EXPECTED]               = "OBEX: File: Error: expected GET or GET_FINAL"
    ,[OBEX_LOG_FILE_ERR_HDR_NOT_BYTE_SEQUENCE]                = "OBEX: File: Not byte sequence header"
    ,[OBEX_LOG_FILE_ERR_HDR_INVALID_MD5]                      = "OBEX: File: Invalid MD5 in header"
    ,[OBEX_LOG_FILE_ERR_NULL_PACKET]                          = "OBEX: File: NULL packet"
    ,[OBEX_LOG_FILE_ERR_ACTION_FINAL_EXPECTED]                = "OBEX: File: Error: expected ACTION_FINAL"
    ,[OBEX_LOG_FILE_ERR_HDR_NOT_4BYTES_SEQUENCE]              = "OBEX: File: Not 4bytes sequence header"
    ,[OBEX_LOG_FILE_ERR_HDR_INVALID_LENGTH]                   = "OBEX: File: Invalid header length"
    ,[OBEX_LOG_FILE_ERR_HDR_NOT_1BYTE_SEQUENCE]               = "OBEX: File: Not 1byte sequence header"
    ,[OBEX_LOG_FILE_ERR_PUT_PUT_FINAL_EXPECTED]               = "OBEX: File: Error: expected PUT or PUT_FINAL"
    ,[OBEX_LOG_FILE_ERR_HDR_NULL_BYTE_SEQUENCE]               = "OBEX: File: NULL byte sequence in header"
    ,[OBEX_LOG_FILE_ERR_PUT_PUT_FINAL_GET_GET_FINAL_EXPECTED] = "OBEX: File: Error: expected PUT PUT_FINAL GET or GET_FINAL"
    ,[OBEX_LOG_FILE_ERR_HDR_INVALID_BYTE_LENGTH]              = "OBEX: File: Invalid header byte length"
    ,[OBEX_LOG_FILE_ERR_PUT_PROCESSING_HEADERS]               = "OBEX: File: Processing headers failed"
    ,[OBEX_LOG_FILE_ERR_PUT_REMOVE_FAILED]                    = "OBEX: File: Remove failed"
    ,[OBEX_LOG_FILE_ERR_PUT_PACKET_WITH_NO_NAME]              = "OBEX: File: Packet with no name"
    ,[OBEX_LOG_FILE_ERR_PACKET_CREATION_FAILURE]              = "OBEX: File: Packet creation failed"
    ,[OBEX_LOG_FILE_ERR_HDR_APPEND_FAILURE]                   = "OBEX: File: Append header failed"
    ,[OBEX_LOG_FILE_ERR_PACKET_SEND_FAILURE]                  = "OBEX: File: Send packet failed"
    ,[OBEX_LOG_FILE_ERR_END_OF_PACKET_NOT_REACHED]            = "OBEX: File: End of packet not reached"
    ,[OBEX_LOG_FILE_ERR_NULL_FUNCTIONS]                       = "OBEX: File: NULL functions"
    ,[OBEX_LOG_FILE_ERR_TWO_NAME_HEADERS]                     = "OBEX: ERR: Two NAME headers"
    ,[OBEX_LOG_FILE_ERR_FILENAME_TOO_LONG]                    = "OBEX: ERR: File name too long"
    ,[OBEX_LOG_FILE_ERR_MISSING_FILENAME]                     = "OBEX: ERR: Missing file name"
    ,[OBEX_LOG_FILE_ERR_MISSING_DESTNAME]                     = "OBEX: ERR: Missing destination file name"
    ,[OBEX_LOG_FILE_ERR_MISSING_LENGTH]                       = "OBEX: ERR: Missing LENGTH header"
    ,[OBEX_LOG_FILE_ERR_FAILED_TO_OPEN]                       = "OBEX: ERR: Failed to open file"
    ,[OBEX_LOG_FILE_ERR_FAILED_TO_LGETSIZE]                   = "OBEX: ERR: Failed to get file size"
    ,[OBEX_LOG_FILE_ERR_START_OFF_TOO_BIG]                    = "OBEX: ERR: Start offset too big"
    ,[OBEX_LOG_FILE_ERR_CHUNK_EXCEEDS_FILE_BOUNDARY]          = "OBEX: ERR: Chunk exceeds file boundaries"
    ,[OBEX_LOG_FILE_ERR_FAILED_TO_SEEK_IN_FILE]               = "OBEX: ERR: Failed to seek in file"
    ,[OBEX_LOG_FILE_ERR_FAILED_TO_READ_FROM_FILE]             = "OBEX: ERR: Failed to read from file"
    ,[OBEX_LOG_FILE_ERR_FAILED_TO_WRITE_TO_FILE]              = "OBEX: ERR: Failed to write to file"
    ,[OBEX_LOG_FILE_ERR_FAILED_TO_TRUNCATE_FILE]              = "OBEX: ERR: Failed to truncate file"
    ,[OBEX_LOG_FILE_ERR_FAILED_TO_RENAME_FILE]                = "OBEX: ERR: Failed to rename file"
    ,[OBEX_LOG_FILE_ERR_FAILED_TO_COPY_FILE]                  = "OBEX: ERR: Failed to copy file"
    ,[OBEX_LOG_FILE_ERR_FILE_ALREADY_OPEN]                    = "OBEX: ERR: File already open"
    ,[OBEX_LOG_FILE_ERR_SAME_SOURCE_AND_DEST_NAMES]           = "OBEX: ERR: Source and Dest names are the same"
    ,[OBEX_LOG_FILE_ERR_INVALID_APP_PARAMS]                   = "OBEX: ERR: Invalid APP_PARAMS"
    ,[OBEX_LOG_FILE_ERR_UNSUPPORTED_ACTION_ID]                = "OBEX: ERR: Unsupported ACTION_ID"
    ,[OBEX_LOG_FILE_ERR_UNSUPPORTED_TYPE]                     = "OBEX: ERR: Unsupported TYPE"
    ,[OBEX_LOG_FILE_ERR_UNSUPPORTED_HEADER]                   = "OBEX: ERR: Unsupported header"
    ,[OBEX_LOG_FILE_ERR_INVALID_GET]                          = "OBEX: ERR: No obj for get operation"
    ,[OBEX_LOG_FILE_ERR_OBJ_STAT_FAILURE]                     = "OBEX: ERR: Obj stat failure"
    ,[OBEX_LOG_DIR_ERR_FAILED_TO_OPEN]                        = "OBEX: ERR: Failed to open dir"
    ,[OBEX_LOG_DIR_ERR_TOO_MUCH_ENTRIES]                      = "OBEX: ERR: Too much entries in dir"
    ,[OBEX_LOG_DIR_ERR_TOO_LONG_DIRNAME]                      = "OBEX: ERR: Too long dir name"

    /* Obex server log points */
    ,[OBEX_LOG_SERVER_NON_CONNECT_WHILE_NOT_CONNECTED]        = "OBEX: Server: Error - Received non-CONNECT request while disconnected - opcode"
    ,[OBEX_LOG_SERVER_CLIENT_VERSION_NUMBER]                  = "OBEX: Server: Client OBEX version number"
    ,[OBEX_LOG_SERVER_CONNECT]                                = "OBEX: Server: CONNECT Request with MAX packet length"
    ,[OBEX_LOG_SERVER_DISCONNECT]                             = "OBEX: Server: DISCONNECT Request"
    ,[OBEX_LOG_SERVER_START]                                  = "OBEX: Server: started"
    ,[OBEX_LOG_SERVER_STOP]                                   = "OBEX: Server: stopped"
    ,[OBEX_LOG_SERVER_UNKNOWN_OPCODE]                         = "OBEX: Server: Unknown opcode"
    ,[OBEX_LOG_SERVER_FIND_CB_FOR_OPCODE]                     = "OBEX: Server: Looking callback for opcode"
    ,[OBEX_LOG_SERVER_FIND_CB_FOR_OPCODE_NOT_FOUND]           = "OBEX: Server: Callback not found"
    ,[OBEX_LOG_SERVER_FIND_CB_FOR_OPCODE_FOUND]               = "OBEX: Server: Callbacks found"
    ,[OBEX_LOG_SERVER_ERR_INVALID_PACKET_READ]                = "OBEX: Server: Invalid packet read with res"
    ,[OBEX_LOG_SERVER_ERR_INVALID_REQUEST]                    = "OBEX: Server: Invalid request"
    ,[OBEX_LOG_SERVER_ERR_UNSUPPORTED_REQUEST]                = "OBEX: Server: Unsupported request"
    ,[OBEX_LOG_SERVER_ERR_INVALID_OPERATION]                  = "OBEX: Server: No operation found for this request"
    ,[OBEX_LOG_SERVER_ERR_OPERATION_FAILED]                   = "OBEX: Server: Operation failed"
    ,[OBEX_LOG_SERVER_ERR_REGISTRATION_FULL]                  = "OBEX: Server: No more room, fail to register callback for operation code"
    ,[OBEX_LOG_SERVER_ERR_REPORT]                             = "OBEX: Server: Report error"
};

/*************************************************************************************************
 * Public variable definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

/*************************************************************************************************
 * MD5
 ************************************************************************************************/
void obex_MD5Init(obex_MD5Context *context)
{
    (void) context;
    ALOGE("%s: Not supported !!!!\n", __FUNCTION__);
}

void obex_MD5Update(obex_MD5Context *context, unsigned char const *buf, int len)
{
    (void) context;
    (void) buf;
    (void) len;
    ALOGE("%s: Not supported !!!!\n", __FUNCTION__);
}

void obex_MD5Final(unsigned char digest[16], obex_MD5Context *context)
{
    (void) digest;
    (void) context;
    ALOGE("obex_MD5Update: Not supported !!!!\n");
}

/*************************************************************************************************
 * Logging
 ************************************************************************************************/
void obex_LogMarker(unsigned int level, unsigned int log_point)
{
    if ( ( log_point<OBEX_LOG_NUM_LOG_POINTS) && (0!=log_point_map_table[log_point]) )
    {
        switch(level)
        {
        case OBEX_VERBOSE_ERROR:
            ALOGE("%s\n", log_point_map_table[log_point]);
            break;

        case OBEX_VERBOSE_INFO:
            ALOGI("%s\n", log_point_map_table[log_point]);
            break;

        case OBEX_VERBOSE_WARNING:
            ALOGW("%s\n", log_point_map_table[log_point]);
            break;

        case OBEX_VERBOSE_VERBOSE:
            ALOGV("%s\n", log_point_map_table[log_point]);
            break;

        case OBEX_VERBOSE_DEBUG:
            ALOGD("%s\n", log_point_map_table[log_point]);
            break;
        }
    }
    else
    {
        ALOGE("%s: Unknown log point %d\n", __FUNCTION__, log_point);
    }
}

void obex_LogValue(unsigned int level, unsigned int log_point, unsigned int value)
{
    if ( ( log_point<OBEX_LOG_NUM_LOG_POINTS) && (0!=log_point_map_table[log_point]) )
    {
        switch(level)
        {
        case OBEX_VERBOSE_ERROR:
            ALOGE("%s: 0x%x\n", log_point_map_table[log_point], value);
            break;

        case OBEX_VERBOSE_INFO:
            ALOGI("%s: 0x%x\n", log_point_map_table[log_point], value);
            break;

        case OBEX_VERBOSE_WARNING:
            ALOGW("%s: 0x%x\n", log_point_map_table[log_point], value);
            break;

        case OBEX_VERBOSE_VERBOSE:
            ALOGV("%s: 0x%x\n", log_point_map_table[log_point], value);
            break;

        case OBEX_VERBOSE_DEBUG:
            ALOGD("%s: 0x%x\n", log_point_map_table[log_point], value);
            break;
        }
    }
    else
    {
        ALOGE("%s: Unknown log point %d with value: 0x%x\n", __FUNCTION__, log_point, value);
    }
}

/*************************************************************************************************
 * Dir operations
 ************************************************************************************************/
int obex_FormatDirEntry(obex_OsDirEnt *entry, void *buf)
{
    obex_DirEnt *client_entry = (obex_DirEnt *)buf;
    if(entry->d_reclen > MAX_FILENAME_LENGTH)
    {
        obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_DIR_ERR_TOO_LONG_DIRNAME, entry->d_reclen);
        return 0;
    }

    /* map OS specific dir entry struct with client's */
    strncpy(client_entry->d_name, entry->d_name, strlen(entry->d_name)+1);
    client_entry->d_fileno = entry->d_ino;
    client_entry->d_namlen = strlen(entry->d_name)+1;
    client_entry->d_type   = entry->d_type;

    return 1;
}

void obex_Sleep(unsigned int seconds)
{
    sleep(seconds);
    return;
}

/** @} END OF FILE */
