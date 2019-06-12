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
 * @file obex_file.c Obex (FILE) application layer
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

#include "obex.h"
#include "obex_server.h"
#include "obex_file.h"
#include "obex_core.h"
#include "fild.h" /* fild_LastError() */

#ifdef __dxp__
#include "drv_fs.h"
#include "drv_fs_cfg.h"
#endif

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

#if !defined(__dxp__) && !defined(ANDROID)
#include <stdint.h>  /* INT32_MAX */
#endif

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/

#define INVALID_HANDLE (-1)

/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/

/**
 * Type definition for the OBEX File layer state
 */
typedef struct
{
    obex_FileFuncs funcs;                        /**< functional file interface */
    obex_DirFuncs d_funcs;                       /**< functional dir interface */
    char inbox[MAX_FILENAME_LENGTH+1];           /**< folder containing files to manipulate */
    struct
    {
        char name[MAX_FILENAME_LENGTH+1];        /**< current obj name */
        int handle;                              /**< handle to the file (if open) */
        obex_Dir *dir;                            /**< handle to the dir (if open) */
    } objstate;                                  /**< state of the file currently being manipulated */
    struct
    {
        int objsize;                             /**< file size (only relevant to GET operations) */
        int read_offset;                         /**< current offset in obj (only relevant to GET operations) */
        obex_OsDirEnt *dirent;                   /**< current available dir entry (when open) */
        bool md5_requested;                      /**< flag to indicate if an MD5 message digest has been requested (through a TYPE header) */
    } getstate;
    struct
    {
        int start_offset;                        /**< offset to start reading from (specified in an APP_PARAMS header) */
        int length;                              /**< length of the chunk to read (specified in an APP_PARAMS header)  - ignored if equal to 0 - ignored on a PUT operation */
    } chunkinfo;
    struct
    {
        int actionid;                                /**< the id of the action to perform (e.g. resize) */
        int length;                                  /**< content of the LENGTH header */
        char destname[MAX_FILENAME_LENGTH+1];        /**< destination obj name */
    } actionstate;
    char buffer[OBEX_CORE_BUFFER_SIZE];      /**< buffer for read operations - TO DO: could be optimised away!!! */
} obex_FileSystem;

/*************************************************************************************************
 * Private function declarations (only used if absolutely necessary)
 ************************************************************************************************/

/*************************************************************************************************
 * Private variable definitions
 ************************************************************************************************/

/**
 *  The initial state to apply during registration or after
 *  completion of a multi-step GET/PUT
 */
const obex_FileSystem obex_file_system_initial_state = {
    {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL}, /* initial functional file interface */
    {NULL,NULL,NULL},                                         /* initial functional dir interface */
#ifdef __dxp__
    DRV_FS_APP_DIR_NAME,                     /* default 'inbox' is the APP folder */
#else
#ifdef _WIN32
    ".",    /* On a win32 OS, 'inbox' is the default folder where server runs... */
#else
    "",     /* On Unix OS, 'inbox' will be root of file system */
#endif
#endif
    {
        "",                                  /* no file name initially */
        INVALID_HANDLE,                      /* 'NULL' handle */
        NULL,                                /* NULL dir stream */
    },
    {
        0,                                   /* read file size */
        0,                                   /* read offset */
        NULL,                                /* NULL dir entry */
        false                                /* MD5 not requested */
    },
    {
        0,                                   /* chunk start offset */
        0                                    /* chunk length */
    },
    {
        0,                                   /* action id */
        -1,                                  /* length = -1 */
        ""                                   /* destination file name */
    },
    ""                                       /* read buffer */
};

/**
 * The main file system layer state variable (not declared
 * 'static' so as to ease debug)
 */
static obex_FileSystem obex_filesystem;

static char *obex_log_prefix = "OBEX: File";

/*************************************************************************************************
 * Public variable definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/**
 * Close file (if it's already open)
 */
static void CloseFile(void)
{
    /* close file */
    if (obex_filesystem.objstate.handle != INVALID_HANDLE)
    {
        obex_filesystem.funcs.close(obex_filesystem.objstate.handle);
    }

    /* forget handle */
    obex_filesystem.objstate.handle = INVALID_HANDLE;
}

/**
 * Close dir (if it's already open)
 */
static void CloseDir(void)
{
    /* close dir */
    if (obex_filesystem.objstate.dir)
    {
        obex_filesystem.d_funcs.closedir(obex_filesystem.objstate.dir);
    }

    /* forget dir handle */
    obex_filesystem.objstate.dir = NULL;
}


/**
 * Reset global state to the initial values
 */
static void ResetFileSystemGlobalState(void)
{
    /* close file if necessary */
    CloseFile();

    /* close dir if necessary */
    CloseDir();

    /* copy contents obex_file_system_initial_state except the functional interface
    and inbox */
#ifdef __dxp__
    strcpy( obex_filesystem.inbox, obex_file_system_initial_state.inbox);
#endif
    memcpy( (void*)&obex_filesystem.objstate, (void*)&obex_file_system_initial_state.objstate, sizeof(obex_filesystem.objstate) );
    memcpy( (void*)&obex_filesystem.getstate, (void*)&obex_file_system_initial_state.getstate, sizeof(obex_filesystem.getstate) );
    memcpy( (void*)&obex_filesystem.chunkinfo, (void*)&obex_file_system_initial_state.chunkinfo, sizeof(obex_filesystem.chunkinfo) );
    memcpy( (void*)&obex_filesystem.actionstate, (void*)&obex_file_system_initial_state.actionstate, sizeof(obex_filesystem.actionstate) );

    return;
}

/**
 * Handle a NAME or DESTNAME header
 * @param name Location of the buffer to write name into
 * @param header Header to read the file name from
 * @return The return code
 */
static obex_ResCode HandleNameOrDestNameHeader(char *name, obex_GenericHeader *header)
{
    char *tmp_ptr;
    uint32 i;
    uint32 name_character_length;
    obex_ResCode ret = OBEX_OK;

    /* the following do{} while(0) is used to prevent using nested if() statements */
    do
    {
        if (NULL == header)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_HDR);
            ret = OBEX_ERR;
            break;
        }

        if(NULL==name)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_FILENAME);
            ret = OBEX_ERR;
            break;
        }

        /* do we already have a obj name? */
        if (strcmp(name,"") != 0)
        {
            /* c.f. TC TestPutWithTwoNameHeaders */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_TWO_NAME_HEADERS);

            /* a name has already been registered, meaning we have
            received multiple NAME headers, or there is a problem in
            the logic. This is an error */
            ret = OBEX_ERR;
            break;
        }

        /* check the header is what we think this is */
        if (header->type != OBEX_OBJHDTYPE_UNICODE_SEQUENCE)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_NOT_UNICODE_SEQUENCE, header->type);
            ret = OBEX_ERR;
            break;
        }

        /* make sure the length of the absolute obj name ('inbox'+separator+ name) is less
        than we can cope with */
        name_character_length = header->value.unicode_sequence.character_length;
        if ((strlen(obex_filesystem.inbox) + name_character_length)
            > MAX_FILENAME_LENGTH)
        {
            /* c.f. TC TestErrorOnPutWithTooLongFileName */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_FILENAME_TOO_LONG);
            ret = OBEX_ERR;
            break;
        }

        /* remember file name - extract 8-bit character string out of
        the 16-bit unicode string */
        strcpy(name, obex_filesystem.inbox);
        tmp_ptr = name + strlen(name);
        for (i=0; i<name_character_length; i++)
        {
            *tmp_ptr++ = header->value.unicode_sequence.ptr[i].lsb;
        }
        /* move one character back */
        tmp_ptr--;

        /* make sure the name ends with '\0' */
        if (*tmp_ptr!='\0')
        {
            /* this should have been caught by the CORE layer (?!?) */

            /* reset file name */
            name[0] = '\0';
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_FILENAME_NOT_NULL_TERMINATED);
            ret = OBEX_ERR;
            break;
        }

        if ((header->id != OBEX_OBJHDID_NAME) && (header->id != OBEX_OBJHDID_DESTNAME))
        {
            ALOGE("%s: %s: Unknown header id 0x%x",
                  obex_log_prefix,
                  __FUNCTION__,
                  header->id);
            ret = OBEX_ERR;
        }
    } while (0);

    return ret;
}

/**
 * Handle the TYPE header
 *
 * @param packet Packet to handle
 * @param header The header to handle
 * @return Return code
 */
static obex_ResCode HandleTypeHeader(obex_PacketDesc *packet, obex_GenericHeader *header)
{
    obex_ResCode ret = OBEX_OK;
    uint32 i;

    /* the following do{} while(0) is used to prevent using nested if() statements */
    do
    {
        if (NULL == header)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_HDR);
            ret = OBEX_ERR;
            break;
        }

        /* we should only be dealing with GET/GET_FINAL operations here */
        if ((packet->code | OBEX_FINAL_BIT_MASK) != OBEX_OPS_GET_FINAL)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_GET_GET_FINAL_EXPECTED, (packet->code | OBEX_FINAL_BIT_MASK));
            ret = OBEX_ERR;
            break;
        }

        /* check the header is what we think this is */
        if (header->type != OBEX_OBJHDTYPE_BYTE_SEQUENCE)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_NOT_BYTE_SEQUENCE, header->type);
            ret = OBEX_ERR;
            break;
        }

        /* the file should not be open yet */
        if (obex_filesystem.objstate.handle != INVALID_HANDLE)
        {
            obex_LogMarker(OBEX_VERBOSE_INFO, OBEX_LOG_FILE_ERR_FILE_ALREADY_OPEN);
            ret = OBEX_ERR;
            break;
        }

        /* make sure the length is sensible - since the only TYPE we support there
        is the MD5 type, the byte length should be the length of MD5_HEADER_TYPE */
        if (header->value.byte_sequence.byte_length != strlen(OBEX_MD5_HEADER_TYPE))
        {
            /* c.f. TC TestGetWithWrongTypeHeader2 */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_UNSUPPORTED_TYPE);
            ret = OBEX_ERR;
            break;
        }

        /* make sure the type is what we expect */
        for (i=0; i<strlen(OBEX_MD5_HEADER_TYPE); i++)
        {
            if (header->value.byte_sequence.ptr[i] != OBEX_MD5_HEADER_TYPE[i])
            {
                obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_INVALID_MD5);
                ret = OBEX_ERR;
                break;
            }
        }
        if (ret != OBEX_OK)
        {
            /* c.f. TC TestGetWithWrongTypeHeader */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_UNSUPPORTED_TYPE);
            break;
        }

        /* everything's fine - let us remember we need to compute an MD5 message digest */
        obex_filesystem.getstate.md5_requested = true;
    } while (0);

    return ret;
}

/**
 * Handle the LENGTH header
 *
 * @param packet Packet containing the header
 * @param header The header to handle
 * @return Return code
 */
static obex_ResCode HandleLengthHeader(obex_PacketDesc *packet, obex_GenericHeader *header)
{
    obex_ResCode ret = OBEX_OK;

    /* the following do{} while(0) is used to prevent using nested if() statements */
    do
    {
        if (NULL == header)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_HDR);
            ret = OBEX_ERR;
            break;
        }

        if (NULL==packet)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_PACKET);
            ret = OBEX_ERR;
            break;
        }

        /* we should only be dealing with ACTION_FINAL operations here */
        if ((packet->code) != OBEX_OPS_ACTION_FINAL)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_ACTION_FINAL_EXPECTED, packet->code);
            ret = OBEX_ERR;
            break;
        }

        /* check the header is what we think this is */
        if (header->type != OBEX_OBJHDTYPE_4BYTES)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_NOT_4BYTES_SEQUENCE, header->type);
            ret = OBEX_ERR;
            break;
        }

        /* make sure the length is sensible */
        if (header->value.val_4b > INT32_MAX)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_INVALID_LENGTH, header->value.val_4b);
            ret = OBEX_ERR;
            break;
        }

        /* Remember the length */
        obex_filesystem.actionstate.length = (int)header->value.val_4b;
    } while (0);

    return ret;
}

/**
 * Handle the ACTIONID header
 *
 * @param packet Packet containing the header
 * @param header The header to handle
 * @return Return code
 */
static obex_ResCode HandleActionIdHeader(obex_PacketDesc *packet, obex_GenericHeader *header)
{
    obex_ResCode ret = OBEX_OK;

    /* the following do{} while(0) is used to prevent using nested if() statements */
    do
    {
        if (NULL == header)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_HDR);
            ret = OBEX_ERR;
            break;
        }

        if (NULL==packet)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_PACKET);
            ret = OBEX_ERR;
            break;
        }

        /* we should only be dealing with ACTION_FINAL operations here */
        if ((packet->code) != OBEX_OPS_ACTION_FINAL)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_ACTION_FINAL_EXPECTED, packet->code);
            ret = OBEX_ERR;
            break;
        }

        /* check the header is what we think this is */
        if (header->type != OBEX_OBJHDTYPE_1BYTE)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_NOT_1BYTE_SEQUENCE, header->type);
            ret = OBEX_ERR;
            break;
        }

        /* Remember the ACTION_ID */
        obex_filesystem.actionstate.actionid = header->value.val_1b;
    } while (0);

    return ret;
}

/**
 * Handle BODY/END_OF_BODY headers
 *
 * @param packet Packet containing the header
 * @param header The header to handle
 * @return Return code
 */
static obex_ResCode HandleBodyEndOfBodyHeaders(obex_PacketDesc *packet, obex_GenericHeader *header)
{
    int handle;
    int write_ret;
    int file_size;

    obex_ResCode ret = OBEX_OK;

    /* the following do{} while(0) is used to prevent using nested if() statements */
    do
    {
        if (NULL == header)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_HDR);
            ret = OBEX_ERR;
            break;
        }

        if (NULL==packet)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_PACKET);
            ret = OBEX_ERR;
            break;
        }

        /* check the header is what we think this is */
        if (header->type != OBEX_OBJHDTYPE_BYTE_SEQUENCE)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_NOT_BYTE_SEQUENCE, header->type);
            ret = OBEX_ERR;
            break;
        }
        /* we should only be dealing with PUT/PUT_FINAL operations here */
        if ((packet->code|OBEX_FINAL_BIT_MASK) != OBEX_OPS_PUT_FINAL)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PUT_PUT_FINAL_EXPECTED, (packet->code|OBEX_FINAL_BIT_MASK));
            ret = OBEX_ERR;
            break;
        }
        /* make sure we have a obj name - the NAME header must be
        received before BODY/END_OF_BODY headers */
        if (strcmp(obex_filesystem.objstate.name,obex_file_system_initial_state.objstate.name) == 0)
        {
            /* c.f. TC TestErrorOnPutWithNoFileName */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_MISSING_FILENAME);
            ret = OBEX_ERR;
            break;
        }

        /* file not opened ? : open it */
        if (obex_filesystem.objstate.handle==INVALID_HANDLE)
        {
            /** open/create file (for writing):
             *  if name is a dir path, this will fail as it is not
             *  supported */
#ifdef _WIN32
            handle = obex_filesystem.funcs.open(obex_filesystem.objstate.name, O_CREAT | O_WRONLY | O_BINARY, S_IREAD | S_IWRITE | S_IEXEC);
#else
            handle = obex_filesystem.funcs.open(obex_filesystem.objstate.name, O_CREAT | O_WRONLY, DEFAULT_CREAT_FILE_MODE);
#endif
            if (handle < 0)
            {
                /* c.f. TC TestPutErrorOnFailureToOpen */
                ALOGE("%s: %s: Fail to open %s. %s",
                    obex_log_prefix,
                    __FUNCTION__,
                    obex_filesystem.objstate.name,
                    fild_LastError());
                ret = OBEX_ERR;
                break;
            }

            /* update global state */
            obex_filesystem.objstate.handle = handle;

            /* if start offset specified through a APP_PARAMS header,
            let us move the write pointer */
            if (obex_filesystem.chunkinfo.start_offset != 0)
            {
                /* retrieve file size */
                file_size = obex_filesystem.funcs.lgetsize(handle);
                if (file_size == -1)
                {
                    /* c.f. TC TestErrorOnPutChunkWithFailureInGetSize */
                    ALOGE("%s: %s: Fail to get %s size. %s",
                        obex_log_prefix,
                        __FUNCTION__,
                        obex_filesystem.objstate.name,
                        fild_LastError());
                    ret = OBEX_ERR;
                    break;
                }

                /* make sure the start offset does not extend beyond the file
                boundaries */
                if (obex_filesystem.chunkinfo.start_offset > file_size)
                {
                    /* c.f. TC TestPutChunkWithTooBigStartOffset */
                    obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_START_OFF_TOO_BIG, (uint32)obex_filesystem.chunkinfo.start_offset );
                    ret = OBEX_ERR;
                    break;
                }

                /* now move the write pointer */
                if (obex_filesystem.funcs.lseek(handle, obex_filesystem.chunkinfo.start_offset, SEEK_SET) == -1)
                {
                    /* c.f. TC TestErrorOnPutChunkWithFailureInLseek */
                    ALOGE("%s: %s: Fail to seek in %s. %s",
                        obex_log_prefix,
                        __FUNCTION__,
                        obex_filesystem.objstate.name,
                        fild_LastError());
                    ret = OBEX_ERR;
                    break;
                }
            }
        }
        /* any contents in this header? */
        if (header->value.byte_sequence.byte_length>0)
        {
            /* make sure the header was a valid value pointer */
            if (NULL==header->value.byte_sequence.ptr)
            {
                obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_NULL_BYTE_SEQUENCE);
                ret = OBEX_ERR;
                break;
            }
            /* write data */
            ALOGI("%s: %s: write in %s", obex_log_prefix, __FUNCTION__,  obex_filesystem.objstate.name);
            write_ret = obex_filesystem.funcs.write(
                                                 obex_filesystem.objstate.handle,    /* handle */
                                                 header->value.byte_sequence.ptr,    /* source pointer */
                                                 header->value.byte_sequence.byte_length);    /* length */

            if ((uint32)write_ret != header->value.byte_sequence.byte_length)
            {
                /* c.f. TC TestErrorOnPutWithFailureInWrite */
                ALOGE("%s: %s: Fail to write %d in %s. %s",
                    obex_log_prefix,
                    __FUNCTION__,
                    (uint32)header->value.byte_sequence.byte_length,
                    obex_filesystem.objstate.name,
                    fild_LastError());
                ret = OBEX_ERR;
                break;
            }
        }
    } while (0);

    if(ret != OBEX_OK)
    {
        ALOGE("%s: %s: error %d", obex_log_prefix, __FUNCTION__, ret);
    }

    return ret;
}

/**
 * Handle APP_PARAMS header - this header is used in PUT/GET
 * requests to specify the chunk of the file to read/write
 *
 * @param packet Packet containing the header
 * @param header The header to handle
 * @return Return code
 */
static obex_ResCode HandleAppParamsHeaders(obex_PacketDesc *packet, obex_GenericHeader *header)
{
    obex_AppParamsData *app_params_data;
    obex_ResCode ret = OBEX_OK;

    /* the following do{} while(0) is used to prevent using nested if() statements */
    do
    {
        if (NULL == header)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_HDR);
            ret = OBEX_ERR;
            break;
        }

        if (NULL==packet)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_PACKET);
            ret = OBEX_ERR;
            break;
        }

        /* check the header is what we think this is */
        if (header->type != OBEX_OBJHDTYPE_BYTE_SEQUENCE)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_NOT_BYTE_SEQUENCE, header->type);
            ret = OBEX_ERR;
            break;
        }
        /* we should only be dealing with PUT/PUT_FINAL/GET/GET_FINAL operations here */
        if (((packet->code|OBEX_FINAL_BIT_MASK) != OBEX_OPS_PUT_FINAL) &&
            ((packet->code|OBEX_FINAL_BIT_MASK) != OBEX_OPS_GET_FINAL))
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PUT_PUT_FINAL_GET_GET_FINAL_EXPECTED, (packet->code|OBEX_FINAL_BIT_MASK));
            ret = OBEX_ERR;
            break;
        }
        /* make sure the file is not open yet - the  APP_PARAMS header must come in the first
        packet of the multi-step PUT/GET operation, and before the BODY headers */
        if (obex_filesystem.objstate.handle != INVALID_HANDLE)
        {
            /* c.f. TC TestErrorOnPutChunkWithAppParamsAfterBody */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_FILE_ALREADY_OPEN);
            ret = OBEX_ERR;
            break;
        }
        /* check header length is meaningful */
        if (header->value.byte_sequence.byte_length != OBEX_APP_PARAMS_DATA_LEN)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_INVALID_BYTE_LENGTH, header->value.byte_sequence.byte_length);
            ret = OBEX_ERR;
            break;
        }
        /* cast header data to the appropriate structure */
        app_params_data = (obex_AppParamsData *)header->value.byte_sequence.ptr;
        /* check IDs and lengths */
        if ((app_params_data->start_offset_parameter_id != OBEX_APP_PARAMS_OFFSET_ID)
            || (app_params_data->start_offset_parameter_length != 4)
            || (app_params_data->length_parameter_id != OBEX_APP_PARAMS_LENGTH_ID)
            || (app_params_data->length_parameter_length != 4))
        {
            /* c.f. TC TestErrorOnInvalidAppParams */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_INVALID_APP_PARAMS);
            ret = OBEX_ERR;
            break;
        }
        /* retrieve start offset */
        obex_filesystem.chunkinfo.start_offset = OBEX_MSB_msb_lsb_LSB_TO_UINT32(
                                                                                 app_params_data->start_offset_byte0,
                                                                                 app_params_data->start_offset_byte1,
                                                                                 app_params_data->start_offset_byte2,
                                                                                 app_params_data->start_offset_byte3 );
        /* retrieve length */
        obex_filesystem.chunkinfo.length = OBEX_MSB_msb_lsb_LSB_TO_UINT32(
                                                                           app_params_data->length_byte0,
                                                                           app_params_data->length_byte1,
                                                                           app_params_data->length_byte2,
                                                                           app_params_data->length_byte3 );
        ALOGD("%s: APP_PARAMS: 0x%x 0x%x",
              obex_log_prefix,
              obex_filesystem.chunkinfo.start_offset,
              obex_filesystem.chunkinfo.length);
        /* not much we can check from the start_offset, length params, we're done! */
    } while (0);

    return ret;
}


/**
 * Handle a PUT/PUT_FINAL request
 *
 * @param packet Packet to be handled
 * @return Return code
 */
static obex_ResCode HandlePutRequest(obex_PacketDesc *packet)
{
    obex_GenericHeader header;
    obex_ResCode read_next_header_res;
    obex_ResCode handle_header_res;
    obex_ResCode ret=OBEX_ERR;

    /* the following do{} while(0) is used to prevent using nested if() statements */
    do
    {
        /* sanity checks */
        if (NULL==packet)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_PACKET);
            ret = OBEX_ERR;
            break;
        }
        if ((OBEX_OPS_PUT_FINAL != packet->code) && (OBEX_OPS_PUT != packet->code))
        {
            ALOGE("%s: %s: Unsupported opcode 0x%x",
                  obex_log_prefix,
                  __FUNCTION__,
                  (uint32)packet->code);
            ret = OBEX_ERR;
            break;
        }

        /* loop through all headers - there must be at least one for this
        packet to qualify as a valid PUT/PUT_FINAL packet */
        do
        {
            /* get header */
            read_next_header_res = obex_CoreReadNextHeader(packet, &header);

            handle_header_res = OBEX_OK;

            if (read_next_header_res == OBEX_OK)
            {
                switch (header.id)
                {
                case OBEX_OBJHDID_NAME:
                    /* the file name is encoded in this header */
                    handle_header_res = HandleNameOrDestNameHeader(obex_filesystem.objstate.name, &header);
                    break;
                case OBEX_OBJHDID_BODY:
                case OBEX_OBJHDID_END_OF_BODY:
                    handle_header_res = HandleBodyEndOfBodyHeaders(packet, &header);
                    break;
                case OBEX_OBJHDID_APP_PARAMS:
                    /* chunk information (start_offset, length) are encoded in this header */
                    handle_header_res = HandleAppParamsHeaders(packet, &header);
                    break;
                case OBEX_OBJHDID_CONNECT_ID:
                case OBEX_OBJHDID_LENGTH:
                    /* ignore these headers instead of throwing an error response -
                    some OBEX client (e.g. OpenOBEX) use them but we do not */
                    handle_header_res = OBEX_OK;
                    break;
                default:
                    /* c.f. TC TestErrorOnPutWithUnknownHeader */
                    obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_UNSUPPORTED_HEADER, (uint32)header.id);
                    /* throw an error */
                    handle_header_res = OBEX_HDR_UNSUPPORTED;
                }

                /* handle errors */
                if (handle_header_res != OBEX_OK)
                {
                    break;
                }
            }
        } while (read_next_header_res == OBEX_OK);

        /* if there was an error processing headers, stop */
        if (handle_header_res != OBEX_OK)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PUT_PROCESSING_HEADERS, handle_header_res);
            ret = handle_header_res;
            break;
        }

        /* if we did not reach the end of the packet (without errors) when reading
           headers, stop */
        if (read_next_header_res != OBEX_EOP)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_END_OF_PACKET_NOT_REACHED, read_next_header_res);
            ret = read_next_header_res;
            break;
        }

        if (packet->code == OBEX_OPS_PUT_FINAL)
        {
            /* we should be done with this file */

            /* if the file was open, close it */
            if (obex_filesystem.objstate.handle!=INVALID_HANDLE)
            {
                /* Reset state */
                ResetFileSystemGlobalState();
            }
            else if (strcmp(obex_filesystem.objstate.name,obex_file_system_initial_state.objstate.name) != 0)
            {
                /* the file was not open, which means we got a packet with no
                BODY/END_OF_BODY headers - this is a request to delete the
                file */

                /* delete file */
                ALOGI("%s: %s: removing %s", obex_log_prefix, __FUNCTION__, obex_filesystem.objstate.name);
                if (obex_filesystem.funcs.remove(obex_filesystem.objstate.name) != 0)
                {
                    ALOGE("%s: %s: Fail to remove %s. %s",
                        obex_log_prefix,
                        __FUNCTION__,
                        obex_filesystem.objstate.name,
                        fild_LastError());
                    ret = OBEX_ERR;
                    break;
                }

                /* Reset state */
                ResetFileSystemGlobalState();
            }
            else
            {
                /* c.f. TC TestErrorOnPutWithNoFileNameOrBody */
                /* packet with no NAME or BODY/END_OF_BODY headers - this is an error */
                obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PUT_PACKET_WITH_NO_NAME);
                ret = OBEX_ERR;
                break;
            }

            /* send OK_FBS back */
            ret = obex_CoreSendSimplePacket(OBEX_RESP_OK_FBS,NULL,0);
            break;
        }

        ret = obex_CoreSendSimplePacket(OBEX_RESP_CONT_FBS,NULL,0);
        break;
    } while (0);

    if (OBEX_OK != ret)
    {
        /* reset state */
        ResetFileSystemGlobalState();
        ALOGE("%s: %s: error %d", obex_log_prefix, __FUNCTION__, ret);
    }
    return ret;
}

/**
 * Read file contents and send a packet back to the client after
 * all headers have been processed by HandleGetRequest().
 * @return OBEX_CONTINUE if there are more contents to
 *         return, OBEX_OK if there are no more contents to
 *         return - other codes indicate an error
 */
static obex_ResCode HandleGetFile(void)
{
    obex_ResCode ret;
    uint32 max_packet_size;
    uint32 max_body_payload_size;
    uint32 bytes_left_in_file;
    uint32 bytes_in_next_packet, bytes_read;
    uint8 packet_code;
    uint8 header_code;
    obex_PacketDesc response_packet;
    obex_GenericHeader header;

    do
    {

        /* this is where we actually return file contents */

        /* what is the maximum packet size? we have constraints on both server and client sides */
        max_packet_size = COM_MIN(
                                 obex_ServerMaximumC2SPacketSize(),
                                 obex_ServerMaximumS2CPacketSize() );

        /* how many bytes can we fit in a BODY/END_OF_BODY packet, given that we also need to
        fit in the packet prologue and the header prologue? */
        max_body_payload_size =
            max_packet_size
            - obex_CoreGetPacketPrologueLength()
            - obex_CoreGetHeaderPrologueLength(OBEX_OBJHDTYPE_BYTE_SEQUENCE);

        /* how many bytes left to read in the file? */
        bytes_left_in_file = obex_filesystem.getstate.objsize - obex_filesystem.getstate.read_offset;

        /* how many bytes can we fit in the next packet? */
        bytes_in_next_packet = COM_MIN(max_body_payload_size, bytes_left_in_file);

        /* read from file */
        ALOGI("%s: %s: read in %s", obex_log_prefix, __FUNCTION__, obex_filesystem.objstate.name);
        bytes_read = obex_filesystem.funcs.read(
            obex_filesystem.objstate.handle,    /* handle */
            obex_filesystem.buffer,    /* destination buffer */
            bytes_in_next_packet);    /* number of bytes to read */
        if ( bytes_read != bytes_in_next_packet)
        {
            /* c.f. TC TestErrorOnGetWithFailureToRead */
            ALOGE("%s: %s: Fail to read in %s.%s",
                obex_log_prefix,
                __FUNCTION__,
                obex_filesystem.objstate.name,
                fild_LastError());
            ret = OBEX_ERR;
            break;
        }

        /* update global state */
        obex_filesystem.getstate.read_offset += bytes_in_next_packet;

        /* find out which packet/header codes we are about to send */
        packet_code = obex_filesystem.getstate.read_offset < obex_filesystem.getstate.objsize ? OBEX_RESP_CONT_FBS : OBEX_RESP_OK_FBS;
        header_code = obex_filesystem.getstate.read_offset < obex_filesystem.getstate.objsize ? OBEX_OBJHDID_BODY : OBEX_OBJHDID_END_OF_BODY;

        /* Create new packet  */
        ret = obex_CoreCreatePacketForSending(&response_packet,packet_code,NULL,0);
        if (ret!=OBEX_OK)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PACKET_CREATION_FAILURE, ret);
            break;
        }

        /* add header */
        header.id = header_code;
        header.type = OBEX_GET_HEADER_MEANING(header.id);
        header.value.byte_sequence.byte_length = bytes_in_next_packet;
        header.value.byte_sequence.ptr = (uint8*)obex_filesystem.buffer;
        ret = obex_CoreAppendHeader(&response_packet, &header);
        if (ret!=OBEX_OK)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_APPEND_FAILURE, ret);
            break;
        }

        /* send packet */
        ret = obex_CoreSendPacket(&response_packet);
        if (ret!=OBEX_OK)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PACKET_SEND_FAILURE, ret);
            break;
        }

        /* are we done? */
        if (obex_filesystem.getstate.read_offset == obex_filesystem.getstate.objsize)
        {
            /* we're done with this operation */
            ret = OBEX_OK;
        }
        else
        {
            /* we're not done with this operation */
            ret = OBEX_CONTINUE;
        }
    } while (0);

    if((ret != OBEX_OK) && (ret != OBEX_CONTINUE))
    {
        ALOGE("%s: %s: error %d", obex_log_prefix, __FUNCTION__, ret);
    }

    return ret;
}

/**
 * Send dir entries back to the client after they have been read
 * and processed by HandleGetRequest().
 * @return OBEX_CONTINUE if there are more contents to
 *         return, OBEX_OK if there are no more contents to
 *         return - other codes indicate an error
 */
static obex_ResCode HandleGetDir(void)
{
    obex_ResCode ret=OBEX_OK;
    uint32 max_packet_size;
    uint32 max_body_payload_size;
    uint32 bytes_in_next_packet, dir_entry_bytes;
    uint8 packet_code;
    uint8 header_code;
    obex_PacketDesc response_packet;
    obex_GenericHeader header;
    uint32 bytes_left_in_buffer;
    int buff_offset=0;

    do
    {
        if (obex_filesystem.getstate.objsize==0)
        {
            /* empty directory... */
            obex_LogMarker(OBEX_VERBOSE_DEBUG, OBEX_LOG_DIR_EMPTY);
            dir_entry_bytes=0;
            packet_code = OBEX_RESP_OK_FBS;
            header_code = OBEX_OBJHDID_END_OF_BODY;
        }
        else
        {
            /* return dir content:
               obex_filesystem.buffer filled with consecutive dir entries data */

            /* what is the maximum packet size? we have constraints on both server and client sides */
            max_packet_size = COM_MIN(obex_ServerMaximumC2SPacketSize(),
                                      obex_ServerMaximumS2CPacketSize() );

            /* how many bytes can we fit in a BODY/END_OF_BODY packet, given that we also need to
            fit in the packet prologue and the header prologue? */
            max_body_payload_size = max_packet_size
                - obex_CoreGetPacketPrologueLength()
                - obex_CoreGetHeaderPrologueLength(OBEX_OBJHDTYPE_BYTE_SEQUENCE);

            /* how many bytes left to read in dir entries buffer ? */
            bytes_left_in_buffer = obex_filesystem.getstate.objsize - obex_filesystem.getstate.read_offset;

            /* how many bytes can we fit in the next packet? */
            bytes_in_next_packet = COM_MIN(max_body_payload_size, bytes_left_in_buffer);

            /* update global state */
            obex_filesystem.getstate.read_offset += bytes_in_next_packet;

            /* find out which packet/header codes we are about to send */
            packet_code = obex_filesystem.getstate.read_offset < obex_filesystem.getstate.objsize ? OBEX_RESP_CONT_FBS : OBEX_RESP_OK_FBS;
            header_code = obex_filesystem.getstate.read_offset < obex_filesystem.getstate.objsize ? OBEX_OBJHDID_BODY : OBEX_OBJHDID_END_OF_BODY;

            /* Create new packet  */
            ret = obex_CoreCreatePacketForSending(&response_packet,packet_code,NULL,0);
            if (ret!=OBEX_OK)
            {
                obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PACKET_CREATION_FAILURE, ret);
                break;
            }

            /* add header */
            header.id = header_code;
            header.type = OBEX_GET_HEADER_MEANING(header.id);
            header.value.byte_sequence.byte_length = bytes_in_next_packet;
            header.value.byte_sequence.ptr = (uint8*)obex_filesystem.buffer+buff_offset;
            buff_offset+=bytes_in_next_packet;
            ret = obex_CoreAppendHeader(&response_packet, &header);
            if (ret!=OBEX_OK)
            {
                obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_APPEND_FAILURE, ret);
                break;
            }

            /* send packet */
            ret = obex_CoreSendPacket(&response_packet);
            if (ret!=OBEX_OK)
            {
                obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PACKET_SEND_FAILURE, ret);
                break;
            }

            /* are we done? */
            if (obex_filesystem.getstate.read_offset == obex_filesystem.getstate.objsize)
            {
                /* we're done with this operation */
                ret = OBEX_OK;
            }
            else
            {
                /* we're not done with this operation */
                ret = OBEX_CONTINUE;
            }
        }
     } while (0);

    return ret;
}

/**
 * Handle sent back of data to client.
 * Can send file content or dir entries.
 *
 * @return OBEX_CONTINUE if there are more contents to
 *         return, OBEX_OK if there are no more contents to
 *         return - other codes indicate an error
 */
static obex_ResCode HandleGetObj(void)
{
    if(obex_filesystem.objstate.handle != INVALID_HANDLE)
    {
        return HandleGetFile();
    }
    else if(obex_filesystem.objstate.dir)
    {
        return HandleGetDir();
    }

    obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_INVALID_GET);
    return OBEX_ERR;
}

/**
 * Create an MD5 message digest of the file and return it to the
 * client
 * @return Return code (OBEX_OK if successful)
 */
static obex_ResCode HandleGetMD5(void)
{
    obex_ResCode ret = OBEX_OK;
    obex_MD5Context md5_context;
    uint32 bytes_left_in_file;
    uint32 max_bytes_to_read;
    uint8 md5_digest[MD5_DIGEST_SIZE];
    obex_PacketDesc response_packet;
    obex_GenericHeader header;

    /* initialise MD5 context */
    obex_MD5Init(&md5_context);

    /*
       the following do{} while (0) is used to avoid using returns and
       nested if() in the middle of the function
    */
    do
    {
        /* read file contents and update MD5 */
        while (obex_filesystem.getstate.read_offset < obex_filesystem.getstate.objsize)
        {
            bytes_left_in_file = obex_filesystem.getstate.objsize - obex_filesystem.getstate.read_offset;
            max_bytes_to_read = COM_MIN(bytes_left_in_file, sizeof(obex_filesystem.buffer) );

            ALOGI("%s: %s: read in %s", obex_log_prefix, __FUNCTION__, obex_filesystem.objstate.name);
            if ((uint32)obex_filesystem.funcs.read(
                                        obex_filesystem.objstate.handle,    /* handle */
                                        obex_filesystem.buffer,    /* destination buffer */
                                        max_bytes_to_read    /* number of bytes to read */
                                        ) != max_bytes_to_read)
            {
                /* c.f. TC TestErrorOnGetMD5HashWithFailureToRead */
                ALOGE("%s: %s: Fail to read in %s.%s",
                      obex_log_prefix,
                      __FUNCTION__,
                      obex_filesystem.objstate.name,
                      fild_LastError());
                ret = OBEX_ERR;
                break;
            }

            /* update read offset */
            obex_filesystem.getstate.read_offset += max_bytes_to_read;

            obex_MD5Update(&md5_context, (uint8*)obex_filesystem.buffer, max_bytes_to_read);
        }

        /* break if there has been an error */
        if (ret != OBEX_OK)
        {
            break;
        }

        /* finalise MD5 message digest */
        obex_MD5Final(md5_digest, &md5_context);

        /* Create new packet  */
        ret = obex_CoreCreatePacketForSending(&response_packet,OBEX_RESP_OK_FBS,NULL,0);
        if (ret!=OBEX_OK)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PACKET_CREATION_FAILURE, ret);
            break;
        }

        /* add header */
        header.id = OBEX_OBJHDID_END_OF_BODY;
        header.type = OBEX_GET_HEADER_MEANING(header.id);
        header.value.byte_sequence.byte_length = MD5_DIGEST_SIZE;
        header.value.byte_sequence.ptr = md5_digest;
        ret = obex_CoreAppendHeader(&response_packet, &header);
        if (ret!=OBEX_OK)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_APPEND_FAILURE, ret);
            break;
        }

        /* send packet */
        ret = obex_CoreSendPacket(&response_packet);
        if (ret!=OBEX_OK)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PACKET_SEND_FAILURE, ret);
            break;
        }
    } while (0);

    if((ret != OBEX_OK))
    {
        ALOGE("%s: %s: error %d", obex_log_prefix, __FUNCTION__, ret);
    }

    return ret;
}

/**
 * Read all available entries found in currently opened
 * directory.
 * Entry is formatted from obex_OsDirEnt to obex_OsDirEnt and
 * appended to dir data buffer that might be sent to client.
 * This function called each time a dir is opened to get the
 * length of the dir data content.
 *
 * @return OBEX_ERR in case of err or OBEX_OK
 */
static obex_ResCode GetDirEntries(void)
{
    obex_ResCode ret = OBEX_OK;
    obex_OsDirEnt *entry;
    obex_DirEnt client_entry;
    int size=0, ans;

    /* read dir entry */
    entry = obex_filesystem.d_funcs.readdir(
        obex_filesystem.objstate.dir);

    while(entry)
    {
        /* format entry for client */
        ans = obex_FormatDirEntry(entry, (uint8 *)&client_entry);
        if (ans == 0)
        {
            ret = OBEX_ERR;
            break;
        }

        /* copy formatted entry in obj buffer if possible */
        if ((size+sizeof(obex_DirEnt)) > OBEX_CORE_BUFFER_SIZE)
        {
            /* it will not be able to store this entry... */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_DIR_ERR_TOO_MUCH_ENTRIES);
            ret = OBEX_ERR;
            break;
        }
        memcpy(obex_filesystem.buffer+size, &client_entry, sizeof(obex_DirEnt));
        size+=sizeof(obex_DirEnt);

        /* read next dir entry */
        entry = obex_filesystem.d_funcs.readdir(
            obex_filesystem.objstate.dir);
    }

    if (ret==OBEX_OK)
    {
        /* update global state */
        obex_filesystem.getstate.objsize = size ;
    }

    return ret;
}

/**
 * Handle first GET/GET_FINAL response
 *
 * @param
 */
static obex_ResCode HandleFirstGetResponse(void)
{
    obex_ResCode ret;
    obex_PacketDesc response_packet;
    int handle;
    int file_size;
    obex_GenericHeader header;
    obex_Stat stat;
    int ans;
    obex_Dir *dir;

    /*
       the following do{} while (0) is used to avoid using returns and
       nested if() in the middle of the function
    */
    do
    {
        /* at this point, the client should have provided a NAME header
                to identify the dir or file being read. make sure this is the case */
        if (strcmp(obex_filesystem.objstate.name,obex_file_system_initial_state.objstate.name) == 0)
        {
            /* c.f. TC TestErrorOnGetWithNoFileName */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_MISSING_FILENAME);
            ret = OBEX_ERR;
            break;
        }

        /* check if we have to read a dir or a file */
        ans = obex_filesystem.funcs.stat(obex_filesystem.objstate.name, &stat);
        if(ans == -1)
        {
            ALOGI("%s: %s: File %s not found.", obex_log_prefix, __FUNCTION__, obex_filesystem.objstate.name);
            ret = OBEX_ERR;
            break;
        }
        else
        {
            if(OBEX_ISDIR(stat))
            {
                /* open dir (for reading) */
                dir = obex_filesystem.d_funcs.opendir(obex_filesystem.objstate.name);
                if(dir==NULL)
                {
                    ALOGE("%s: %s: Fail to open %s.%s",
                          obex_log_prefix,
                          __FUNCTION__,
                          obex_filesystem.objstate.name,
                          fild_LastError());
                    ret = OBEX_ERR;
                    break;
                }

                /* update global state */
                obex_filesystem.objstate.dir = dir;

                /* need to read all entries now to communicate length to client... */
                ret=GetDirEntries();
                if (ret!=OBEX_OK)
                {
                    break;
                }

                obex_filesystem.getstate.read_offset = 0;
            }
            else
            {
                /* open file (for reading) */
#ifdef _WIN32
                handle = obex_filesystem.funcs.open(obex_filesystem.objstate.name, O_RDONLY | O_BINARY, 0);
#else
                handle = obex_filesystem.funcs.open(obex_filesystem.objstate.name, O_RDONLY, 0);
#endif
                if (handle < 0)
                {
                    /* c.f. TC TestErrorOnGetWithFailureToOpen */
                    ALOGE("%s: %s: Fail to open %s.%s",
                          obex_log_prefix,
                          __FUNCTION__,
                          obex_filesystem.objstate.name,
                          fild_LastError());
                    ret = OBEX_ERR;
                    break;
                }

                /* update global state */
                obex_filesystem.objstate.handle = handle;

                /* retrieve file size */
                file_size = obex_filesystem.funcs.lgetsize(handle);
                if (file_size == -1)
                {
                    /* c.f. TC TestErrorOnGetWithFailureToGetSize */
                    ALOGE("%s: %s: Fail to get %s size.%s",
                          obex_log_prefix,
                          __FUNCTION__,
                          obex_filesystem.objstate.name,
                          fild_LastError());
                    ret = OBEX_ERR;
                    break;
                }

                /* update global state */
                obex_filesystem.getstate.objsize = file_size;
                obex_filesystem.getstate.read_offset = 0;

                /* if chunkinfo was specified through a APP_PARAMS header,
                let us move the read pointer to the start offset and shorten the objsize
                to the sum of the chunk start offset and length */
                if ((obex_filesystem.chunkinfo.start_offset != 0) || (obex_filesystem.chunkinfo.length != 0))
                {
                    /* make sure the chunk does not extend beyond the file
                    boundaries */
                    if (obex_filesystem.chunkinfo.start_offset + obex_filesystem.chunkinfo.length > file_size)
                    {
                        /* c.f. TC TestErrorOnGetChunkBeyondEndOfFile */
                        obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_CHUNK_EXCEEDS_FILE_BOUNDARY);
                        ret = OBEX_ERR;
                        break;
                    }

                    /* now move the read pointer */
                    if (obex_filesystem.funcs.lseek(handle, obex_filesystem.chunkinfo.start_offset, SEEK_SET) == -1)
                    {
                        /* c.f. TC TestErrorOnGetChunkWithFailureToSeek */
                        ALOGE("%s: %s: Fail to seek in %s.%s",
                              obex_log_prefix,
                              __FUNCTION__,
                              obex_filesystem.objstate.name,
                              fild_LastError());
                        ret = OBEX_ERR;
                        break;
                    }

                    /* update file information to reflect the chunk to read */
                    obex_filesystem.getstate.read_offset = obex_filesystem.chunkinfo.start_offset;
                    obex_filesystem.getstate.objsize = obex_filesystem.chunkinfo.start_offset + obex_filesystem.chunkinfo.length;
                }
            }
        }

        /* Create new packet (CONT_FBS) */
        ret = obex_CoreCreatePacketForSending(&response_packet,OBEX_RESP_CONT_FBS,NULL,0);
        if (ret!=OBEX_OK)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PACKET_CREATION_FAILURE, ret);
            break;
        }

        /* add OBEX_OBJHDID_LENGTH header */
        header.id = OBEX_OBJHDID_LENGTH;
        header.type = OBEX_GET_HEADER_MEANING(header.id);
        if (true == obex_filesystem.getstate.md5_requested)
        {
            header.value.val_4b = MD5_DIGEST_SIZE;
        }
        else
        {
            header.value.val_4b = obex_filesystem.getstate.objsize - obex_filesystem.getstate.read_offset;
        }
        ret = obex_CoreAppendHeader(&response_packet, &header);
        if (ret!=OBEX_OK)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_HDR_APPEND_FAILURE, ret);
            break;
        }

        /* send packet */
        ret = obex_CoreSendPacket(&response_packet);
        if (ret!=OBEX_OK)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_PACKET_SEND_FAILURE, ret);
            break;
        }

        ret = OBEX_CONTINUE;
    } while (0);

    return ret;
}

/**
 * Handle a GET/GET_FINAL request
 *
 * @param packet Packet to be handled
 * @return Return code
 */
static obex_ResCode HandleGetRequest(obex_PacketDesc *packet)
{
    obex_GenericHeader header;
    obex_ResCode read_next_header_res;
    obex_ResCode handle_header_res;
    obex_ResCode ret;

    /* the following do{} while(0) is used to prevent using nested if() statements */
    do
    {

        /* sanity checks */
        if (NULL==packet)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_PACKET);
            ret = OBEX_ERR;
            break;
        }

        /* loop through all headers - there must be at least one for this
        packet to qualify as a valid PUT/PUT_FINAL packet */
        do
        {
            /* get header */
            read_next_header_res = obex_CoreReadNextHeader(packet, &header);

            handle_header_res = OBEX_OK;

            if (read_next_header_res == OBEX_OK)
            {
                switch (header.id)
                {
                case OBEX_OBJHDID_NAME:
                    /* the obj name is encoded in this header */
                    handle_header_res = HandleNameOrDestNameHeader(obex_filesystem.objstate.name, &header);
                    break;
                case OBEX_OBJHDID_APP_PARAMS:
                    /* chunk information (start_offset, length) are encoded in this header */
                    handle_header_res = HandleAppParamsHeaders(packet, &header);
                    break;
                case OBEX_OBJHDID_TYPE:
                    /* the MD5 message digest is requested through this header */
                    handle_header_res = HandleTypeHeader(packet, &header);
                    break;
                case OBEX_OBJHDID_CONNECT_ID:
                    /* ignore these headers instead of throwing an error response -
                    some OBEX client (e.g. OpenOBEX) use them but we do not */
                    handle_header_res = OBEX_OK;
                    break;
                case OBEX_OBJHDID_LENGTH:
                    /* LENGTH header not supported - clients should use the APPLICATION_PARAMETER
                    header to specify the start offset / length of the chunk to be read */
                    handle_header_res = OBEX_HDR_UNSUPPORTED;
                    break;
                default:
                    /* c.f. TC TestErrorOnGetWithUnknownHeader */
                    obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_UNSUPPORTED_HEADER, (uint32)header.id);
                    /* throw an error */
                    handle_header_res = OBEX_HDR_UNSUPPORTED;
                }

                /* handle errors */
                if (handle_header_res != OBEX_OK)
                {
                    obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_UNSUPPORTED_HEADER, handle_header_res);
                    break;
                }
            }                                    /* if (res == OBEX_OK) */
        } while (read_next_header_res == OBEX_OK);

        /* if there was an error processing headers, stop */
        if (handle_header_res != OBEX_OK)
        {
            ret = handle_header_res;
            break;
        }

        /* if we did not reach the end of the packet (without errors) when reading
           headers, stop */
        if (read_next_header_res != OBEX_EOP)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_END_OF_PACKET_NOT_REACHED, read_next_header_res);
            ret = read_next_header_res;
            break;
        }

        /* if the file has not been opened yet, open it now, send CONTINUE_FBS
        response back with LENGTH header to indicate the file size - we will
        only do this once per multi-step GET operation */
        if (obex_filesystem.objstate.handle==INVALID_HANDLE && obex_filesystem.objstate.dir==NULL)
        {
            ret = HandleFirstGetResponse();
        }
        else                                     /* if (obex_filesystem.objstate.handle==INVALID_HANDLE) */
        {
            if (true == obex_filesystem.getstate.md5_requested)
            {
                /* return the MD5 message digest */
                ret = HandleGetMD5();
            }
            else
            {
                /* return the obj contents (as many as we can fit in the next packet) */
                ret = HandleGetObj();
            }
        }
    } while (0);

    if (OBEX_CONTINUE != ret)
    {
        ResetFileSystemGlobalState();
    }

    return ret;
}

/**
* Handle the RESIZE ACTION
* @return The return code
*/
static obex_ResCode HandleActionResize(void)
{
    obex_ResCode ret = OBEX_OK;
    int handle = -1;
    int file_size;
    int write_ret;
    int number_of_bytes_to_write;

    do
    {
        /* make sure we have a length (i.e. length differs from initial value) */
        if (obex_filesystem.actionstate.length == obex_file_system_initial_state.actionstate.length)
        {
            /* c.f. TC TestErrorOnResizeWithMissingLength */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_MISSING_LENGTH);
            ret = OBEX_ERR;
            break;
        }

        /* open file (for writing) */
#ifdef _WIN32
        handle = obex_filesystem.funcs.open(obex_filesystem.objstate.name, O_WRONLY|O_APPEND|O_BINARY, 0);
#else
        handle = obex_filesystem.funcs.open(obex_filesystem.objstate.name, O_WRONLY|O_APPEND, 0);
#endif
        if (handle < 0)
        {
            /* c.f. TC TestErrorOnResizeWithFailureToOpen */
            ALOGE("%s: %s: Fail to open %s.%s",
                  obex_log_prefix,
                  __FUNCTION__,
                  obex_filesystem.objstate.name,
                  fild_LastError());
            ret = OBEX_ERR;
            break;
        }

        /* update global variable */
        obex_filesystem.objstate.handle = handle;

        /* retrieve original file size */
        file_size = obex_filesystem.funcs.lgetsize(handle);
        if (file_size == -1)
        {
            /* c.f. TC TestErrorOnResizeWithFailureToGetSize */
            ALOGE("%s: %s: Fail to get %s size.%s",
                  obex_log_prefix,
                  __FUNCTION__,
                  obex_filesystem.objstate.name,
                  fild_LastError());
            ret = OBEX_ERR;
            break;
        }

        /* resized length less than the original file size? */
        if (obex_filesystem.actionstate.length <= file_size)
        {
            /* close file */
            CloseFile();

            /* we need to truncate the file */
            ALOGI("%s: %s: truncating %s", obex_log_prefix, __FUNCTION__, obex_filesystem.objstate.name);
            if (obex_filesystem.funcs.truncate(obex_filesystem.objstate.name, obex_filesystem.actionstate.length) == -1)
            {
                /* c.f. TC TestErrorOnTruncateWithFailureToTruncate */
                ALOGE("%s: %s: Fail to truncate %s (%d/%d).%s",
                      obex_log_prefix,
                      __FUNCTION__,
                      obex_filesystem.objstate.name,
                      obex_filesystem.actionstate.length,
                      file_size,
                      fild_LastError());
                obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_FAILED_TO_TRUNCATE_FILE);
                ret = OBEX_ERR;
                break;
            }
        }
        else
        {
            /* we need to add 'zeroes' at the end of the file */
            memset(obex_filesystem.buffer, 0, sizeof(obex_filesystem.buffer) );

            /* how many bytes do we need to write? */
            number_of_bytes_to_write = obex_filesystem.actionstate.length - file_size;

            do
            {
                uint32 bytes_to_write_now;

                /* how many bytes can we write in one shot? */
                bytes_to_write_now = COM_MIN((uint32)number_of_bytes_to_write, sizeof(obex_filesystem.buffer) );

                /* write bytes */
                ALOGI("%s: %s: writting in %s", obex_log_prefix, __FUNCTION__, obex_filesystem.objstate.name);
                write_ret = obex_filesystem.funcs.write(
                                                     handle,    /* handle */
                                                     obex_filesystem.buffer,    /* source pointer */
                                                     bytes_to_write_now);    /* length */
                if (write_ret == -1)
                {
                    /* c.f. TC TestErrorOnExtendWithFailureToWrite */
                    ALOGE("%s: %s: Fail to write in %s (%d).%s",
                          obex_log_prefix,
                          __FUNCTION__,
                          obex_filesystem.objstate.name,
                          bytes_to_write_now,
                          fild_LastError());
                    ret = OBEX_ERR;
                    break;
                }

                /* update number of bytes to write */
                number_of_bytes_to_write -= bytes_to_write_now;
            } while (number_of_bytes_to_write > 0);

            if (OBEX_OK != ret)
            {
                break;
            }
        }

        /* send OK_FBS back */
        ret = obex_CoreSendSimplePacket(OBEX_RESP_OK_FBS,NULL,0);
    } while (0);

    /* reset state */
    ResetFileSystemGlobalState();

    if(ret != OBEX_OK)
    {
        ALOGE("%s: %s: error %d", obex_log_prefix, __FUNCTION__, ret);
    }

    return ret;
}

/**
* Handle the COPY or MOVE actions
* @param packet The packet to handle
* @return The return code
*/
static obex_ResCode HandleActionCopyOrMove(void)
{
    obex_ResCode ret = OBEX_OK;
    int res = 0;

    do
    {
        /* the caller should have made sure we received a NAME header, let us
        check if we have received a DESTINATION header */
        if (strcmp(obex_filesystem.actionstate.destname,"") == 0)
        {
            /* c.f. TC TestErrorOnCopyWithMissingDestName */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_MISSING_DESTNAME);
            ret = OBEX_ERR;
            break;
        }

        /* make sure the source and destination names differ */
        if (strcmp(obex_filesystem.objstate.name,obex_filesystem.actionstate.destname) == 0)
        {
            /* c.f. TC TestErrorOnCopyWithSameSourceAndDestNames */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_SAME_SOURCE_AND_DEST_NAMES);
            ret = OBEX_ERR;
            break;
        }

        /* copy or rename file, depending on action id */
        if (OBEX_ACTION_COPY == obex_filesystem.actionstate.actionid)
        {
            ALOGI("%s: %s: copying file %s to %s", obex_log_prefix, __FUNCTION__, obex_filesystem.objstate.name, obex_filesystem.actionstate.destname);
            res = obex_filesystem.funcs.copy(obex_filesystem.objstate.name, obex_filesystem.actionstate.destname);

            if (res != 0)
            {
                /* c.f. TestErrorOnCopyWithFailureToCopy */
                ALOGE("%s: %s: Fail to copy %s to %s.%s",
                      obex_log_prefix,
                      __FUNCTION__,
                      obex_filesystem.objstate.name,
                      obex_filesystem.actionstate.destname,
                      fild_LastError());
                ret = OBEX_ERR;
                break;
            }
        }
        else if (OBEX_ACTION_MOVE_RENAME == obex_filesystem.actionstate.actionid)
        {
            ALOGI("%s: %s: renaming file %s to %s", obex_log_prefix, __FUNCTION__, obex_filesystem.objstate.name, obex_filesystem.actionstate.destname);
            res = obex_filesystem.funcs.rename(obex_filesystem.objstate.name, obex_filesystem.actionstate.destname);

            if (res != 0)
            {
                /* c.f. TC TestErrorOnRenameWithFailureToRename */
                ALOGE("%s: %s: Fail to rename %s to %s.%s",
                      obex_log_prefix,
                      __FUNCTION__,
                      obex_filesystem.objstate.name,
                      obex_filesystem.actionstate.destname,
                      fild_LastError());
                ret = OBEX_ERR;
                break;
            }
        }
        else
        {
            ALOGE("%s: %s: Unknown ACTION ID 0x%x",
                  obex_log_prefix,
                  __FUNCTION__,
                  obex_filesystem.actionstate.actionid);
            ret = OBEX_ERR;
            break;
        }

        /* send OK_FBS back */
        ret = obex_CoreSendSimplePacket(OBEX_RESP_OK_FBS,NULL,0);
    } while (0);

    return ret;

}

/**
 * Handle an ACTION request
 *
 * @param packet Packet to be handled
 * @return Return code
 */
static obex_ResCode HandleActionRequest(obex_PacketDesc *packet)
{
    obex_GenericHeader header;
    obex_ResCode read_next_header_res;
    obex_ResCode handle_header_res;
    obex_ResCode ret = OBEX_OK;

    /* the following do{} while(0) is used to prevent using nested if() statements */
    do
    {
        /* sanity checks */
        if (NULL==packet)
        {
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_PACKET);
            ret = OBEX_ERR;
            break;
        }
        if (OBEX_OPS_ACTION_FINAL != packet->code)
        {
            ALOGE("%s: %s: Unsupported opcode 0x%x",
                  obex_log_prefix,
                  __FUNCTION__,
                  (uint32)packet->code);
            ret = OBEX_ERR;
            break;
        }

        /* loop through all headers - there must be at least one for this
        packet to qualify as a valid PUT/PUT_FINAL packet */
        do
        {
            /* get header */
            read_next_header_res = obex_CoreReadNextHeader(packet, &header);

            handle_header_res = OBEX_OK;

            if (read_next_header_res == OBEX_OK)
            {
                switch (header.id)
                {
                case OBEX_OBJHDID_NAME:
                    /* the file name is encoded in this header */
                    handle_header_res = HandleNameOrDestNameHeader(obex_filesystem.objstate.name, &header);
                    break;
                case OBEX_OBJHDID_DESTNAME:
                    /* the destination file name is encoded in this header */
                    handle_header_res = HandleNameOrDestNameHeader(obex_filesystem.actionstate.destname, &header);
                    break;
                case OBEX_OBJHDID_ACTION_ID:
                    /* the type of action to perform is encoded in this header */
                    handle_header_res = HandleActionIdHeader(packet, &header);
                    break;
                case OBEX_OBJHDID_LENGTH:
                    /* the new file size is encoded in this header */
                    handle_header_res = HandleLengthHeader(packet, &header);
                    break;
                default:
                    obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_UNSUPPORTED_HEADER, (uint32)header.id);
                    /* throw an error */
                    handle_header_res = OBEX_HDR_UNSUPPORTED;
                }

                /* handle errors */
                if (handle_header_res != OBEX_OK)
                {
                    obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_UNSUPPORTED_HEADER, handle_header_res);
                    break;
                }
            }
        } while (read_next_header_res == OBEX_OK);

        /* if there was an error processing headers, stop */
        if (handle_header_res != OBEX_OK)
        {
            ret = handle_header_res;
            break;
        }

        /* if we did not reach the end of the packet (without errors) when reading
           headers, stop */
        if (read_next_header_res != OBEX_EOP)
        {
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_END_OF_PACKET_NOT_REACHED, read_next_header_res);
            ret = read_next_header_res;
            break;
        }

        /* make sure we have a name (NAME header) */
        if (strcmp(obex_filesystem.objstate.name,"") == 0)
        {
            /* c.f. TC TestActionWithNoFileName */
            obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_MISSING_FILENAME);
            ret = OBEX_ERR;
            break;
        }

        switch (obex_filesystem.actionstate.actionid)
        {
        case OBEX_ACTION_RESIZE:
            ret = HandleActionResize();
            break;
        case OBEX_ACTION_COPY:
        case OBEX_ACTION_MOVE_RENAME:
            ret = HandleActionCopyOrMove();
            break;
        default:
            /* c.f. TC TestActionWithErrorOnActionId */
            obex_LogValue(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_UNSUPPORTED_ACTION_ID, (uint32)obex_filesystem.actionstate.actionid);
            ret = OBEX_ERR;
            break;
        }
    } while (0);

    /* Reset state */
    ResetFileSystemGlobalState();

    return ret;
}


/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
void obex_SetInbox(char *inbox)
{
    strcpy(obex_filesystem.inbox, inbox);
    return;
}

obex_ResCode obex_FileRegister(const obex_FileFuncs *funcs, const obex_DirFuncs *d_funcs)
{
    /* sanity checks */
    if (NULL == funcs)
    {
        obex_LogMarker(OBEX_VERBOSE_ERROR, OBEX_LOG_FILE_ERR_NULL_FUNCTIONS);
        return OBEX_ERR;
    }

    /* make sure all fields of the functional file interface are populated with
    a sensible value */
    if ((NULL==funcs->open)
        || (NULL==funcs->close)
        || (NULL==funcs->read)
        || (NULL==funcs->write)
        || (NULL==funcs->lseek)
        || (NULL==funcs->truncate)
        || (NULL==funcs->lgetsize)
        || (NULL==funcs->remove)
        || (NULL==funcs->copy)
        || (NULL==funcs->rename)
        || (NULL==funcs->stat))
    {
        ALOGE("%s: %s: One of the callbacks from the OBEX file functional interface is NULL",
              obex_log_prefix,
              __FUNCTION__);
        return OBEX_ERR;
    }

    /* save obex_ File functional interface */
    memcpy(&obex_filesystem.funcs, funcs, sizeof(obex_FileFuncs) );

    /* make sure all fields of the functional dir interface are populated with
    a sensible value */
    if ((NULL==d_funcs->opendir)
        || (NULL==d_funcs->closedir)
        || (NULL==d_funcs->readdir))
    {
        ALOGE("%s: %s: One of the callbacks from the OBEX dir functional interface is NULL",
            obex_log_prefix,
            __FUNCTION__);
        return OBEX_ERR;
    }

    /* save obex dir functional interface */
    memcpy(&obex_filesystem.d_funcs, d_funcs, sizeof(obex_DirFuncs) );

    /* register operations */
    obex_ServerRegisterOperationCallback(OBEX_OPS_PUT, HandlePutRequest);
    obex_ServerRegisterOperationCallback(OBEX_OPS_PUT_FINAL, HandlePutRequest);
    obex_ServerRegisterOperationCallback(OBEX_OPS_GET, HandleGetRequest);
    obex_ServerRegisterOperationCallback(OBEX_OPS_GET_FINAL, HandleGetRequest);
    obex_ServerRegisterOperationCallback(OBEX_OPS_ACTION_FINAL, HandleActionRequest);

    /* apply initial state */
    obex_filesystem.objstate.handle = INVALID_HANDLE;
    ResetFileSystemGlobalState();

    return OBEX_OK;
}

/** @} END OF FILE */
