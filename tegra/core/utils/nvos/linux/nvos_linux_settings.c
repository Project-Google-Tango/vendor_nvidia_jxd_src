/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

//
// Parts of this file are taken from code checked in by Robert Tray in
// egl/eglpersistsettings.c, and modified as needed. Eventually consolidate
// both pieces of code into a single location.
//

#if defined(__linux__)
/* Large File Support */
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include "nvos.h"
#include "nvos_internal.h"
#include "nvos_debug.h"
#include "nvassert.h"
#include "nvos_linux.h"
#include <unistd.h>
#include <sys/syscall.h>

/* Android has its own libc, Bionic, which is taken from BSD, so it doesn't
 * have System V stuff, etc.
 */
#if !defined(ANDROID)
#include <sys/shm.h>
#include <sys/ipc.h>
#endif

#ifdef ANDROID
#include <cutils/properties.h>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <unwind.h>
#include <cutils/ashmem.h>
#else
#include <execinfo.h>
#endif

#ifdef ANDROID

#define MAX_BINARY_VALUE_SIZE  1024  // Max value size for binary data

// File Path and name defines
#define NVOS_GLES_SETTINGS_FILE_PATH   "/data/data/com.nvidia.nvcpl/gles_state_tegra.txt"
#define NVOS_STEREO_SETTINGS_FILE_PATH "/data/data/com.nvidia.nvcpl/stereo_state_tegra.txt"
#define NVOS_STEREO_PROFILES_FILE_PATH "/data/data/com.nvidia.NvCPLSvc/files/stereoprofs_tegra.txt"

#include "nvutil.h"

// TODO: Defines and structure taken from nvos_config.c. Once we figure out how
// to share the file parsing and read/write functions between this file and
// nvos_config.c, also move the structure and defines below into a common header.

// This specifies the maximum number of key, value pairs that we can support
// in the profiles file
#define NVOS_MAX_CONFIG_VARS 1024

typedef struct
{
    char* buf;
    const char* names[NVOS_MAX_CONFIG_VARS];
    const char* values[NVOS_MAX_CONFIG_VARS];
    int num;
} NvOsConfig;

/**
 * Forward declarations. Some of the functions are picked from nvos_config.c.
 * TODO: Eventually merge the functions with the originals from nvos_config.c
 */
static NvError NvOsConfigFileReadState(const char *filePath, const char *name,
                    char *value, NvU32 size);
static NvError NvOsConfigFileWriteState(const char *filePath, const char *name,
                    const char* value);
static NvError ParseConfigFile(const char *filePath, NvOsConfig* conf);
static NvError DumpConfigFile(const char *filePath, NvOsConfig* conf);

// Functions to do base64 encode/decode. From Robert Tray's original checkin
static NvError NvOsBinary2String(const char *binaryBufPtr, NvU32 binaryBufSize,
                         char *outStringPtr, NvU32 outStringSize);
static NvError NvOsString2Binary(const char *inStringPtr, NvU32 inStringSize,
                         unsigned char *binaryBufPtr, NvU32 *binaryBufSize);

// Function to convert a hex number to a char value
static inline char NvOsHexToChar(char hexVal);

#endif // ANDROID

/**
 * NvOsConfigGetStateInternal(): Get the requested state
 *
 * Depending on the stateId, the state will be read from the appropriate
 * location (each stateId type can theoretically have state stored in
 * a separate file)
 */
NvError NvOsConfigGetStateInternal(int stateId, const char *name, char *value,
        int valueSize, int flags)
{
#if !defined(ANDROID)
    // Only supported for Android as of now
    return NvError_NotSupported;
#else

    NvError retVal = NvSuccess;
    int i = 0;

    // The type of state can be inferred from the stateId
    // All stateId values use String values, except for stereo profiles,
    // which use a binary type (structure containing the required data)
    // TODO: Can additionally use a Type parameter if so desired
    if ((name == NULL) || (value == NULL) || (valueSize == 0))
    {
        NV_DEBUG_PRINTF(("NvOsConfigGetStateInternal(): NULL Pointer detected"));
        return NvError_BadParameter;
    }

    // If the stateId is NvOsStateId_3dv_Profiles, then the data has to be returned
    // in a binary format (pointer to a structure containing the data), so we first
    // convert the string data in the file to binary. For all other ids, for now assume
    // that the data is to be returned in a String format (storage in the settings file
    // is always in a string format as a key, value pair)
    // All state types do not need to share the same file location. We can
    // use a different file for each state type
    switch (stateId)
    {
        case NvOsStateId_3dv_Controls:
        {
            // Do nothing, controls are read via NvOsGetSysConfigString() (see
            // nvos_config.c)
            NV_DEBUG_PRINTF(("NvOsConfigGetStateInternal(): Should not be called for 3DV Controls"));
            retVal = NvError_BadValue;
            break;
        }
        case NvOsStateId_3dv_Profiles:
        {
            // Do nothing. Use NvCplGetAppProfileSetting*() functions in libnvcpl for all app profile
            // access.
            NV_DEBUG_PRINTF(("NvOsConfigGetStateInternal(): Should not be called for 3DV Settings"));
            retVal = NvError_BadValue;
            break;
        }
        case NvOsStateId_3dv_Gles_Settings:
        default:
        {
            char *valueString = value;
            retVal = NvOsConfigFileReadState(NVOS_GLES_SETTINGS_FILE_PATH, name, valueString, valueSize);
            NV_DEBUG_PRINTF(("NvOsConfigGetStateInternal(): GLES setting %s read as %s, return code = %s",
                name, valueString, retVal));
            break;
        }

    }

    return retVal;

#endif // ANDROID

}

/**
 * NvOsConfigQueryStateInternal(): Query the keynames for the specified state
 *
 * For the specified state, query all the keynames and return them in the
 * specified buffer. The maximum size of each key and the maximum number of
 * keys to return are provided via maxKeySize and maxNumkeys respectively.
 */
NvError NvOsConfigQueryStateInternal(int stateId, const char **ppKeys,
        int *pNumKeys, int maxKeySize)
{
#if !defined(ANDROID)
    // Only supported for Android as of now
    return NvError_NotSupported;
#else

    NvError retVal = NvSuccess;
    NvOsConfig conf;
    int i = 0;

    // Only handled for 3DV Profiles for now
    if (stateId != NvOsStateId_3dv_Profiles) {
        return NvError_BadValue;
    }

    retVal = ParseConfigFile(NVOS_STEREO_PROFILES_FILE_PATH, &conf);

    if ((ppKeys == NULL) && (pNumKeys != NULL))
    {
        (*pNumKeys) = conf.num;
        return retVal;
    }

    // In the normal mode where we want to read all the key names
    if ((ppKeys == NULL) || (pNumKeys == NULL) || (maxKeySize == 0))
    {
        NV_DEBUG_PRINTF(("NvOsConfigQueryStateInternal(): Invalid input Arguments\n"));
        return NvError_BadParameter;
    }

    for (i = 0; i < conf.num; i++)
    {
        if (ppKeys[i] == NULL) {
            NV_DEBUG_PRINTF(("NvOsConfigQueryStateInternal(): ppKeys[%d] is NULL", i));
        } else {
            NvOsStrncpy((char *)ppKeys[i], conf.names[i], maxKeySize);
        }
    }

    NvOsFree(conf.buf);

    return retVal;

#endif // ANDROID

}

/**
 * NvOsConfigSetStateInternal(): Set the specified state
 *
 * Depending on the stateId, the state will be written to the appropriate
 * location (each stateId type can theoretically have state written to
 * a separate file)
 *
 */
NvError NvOsConfigSetStateInternal(int stateId, const char *name, const char *value,
        int valueSize, int flags)
{
#if !defined(ANDROID)
    // Only supported for Android as of now
    return NvError_NotSupported;
#else
    NvError retVal = NvSuccess;

    // The type of state can be inferred from the stateId
    // All stateId values use String values, except for stereo profiles,
    // which use a binary type (structure containing the required data)
    // TODO: Can additionally use a Type parameter if so desired
    if ((name == NULL) || (value == NULL) || (valueSize == 0))
    {
        NV_DEBUG_PRINTF(("NvOsConfigSetStateInternal(): NULL Pointer detected"));
        return NvError_BadParameter;
    }

    // If the stateId is NvOsStateId_3dv_Profiles, then the data has been sent
    // in a binary format (pointer to a structure containing the data). For all
    // other ids, for now assume that the data is sent in a String format
    // All state types do not need to share the same file location. We can
    // use a different file for each state type
    switch (stateId)
    {
        case NvOsStateId_3dv_Controls:
        {
            // Do nothing, controls are set via NvOsSetSysConfigString() (see nvos_config.c)
            NV_DEBUG_PRINTF(("NvOsConfigSetStateInternal(): Should not be called for 3DVControls"));
            retVal = NvError_BadValue;
            break;
        }
        case NvOsStateId_3dv_Profiles:
        {
            // Convert the binary data to a string and write to the file
            NvU8 *bufPtr = NULL;
            NvU32 bufSize;
            if (valueSize >  MAX_BINARY_VALUE_SIZE)
            {
                NV_DEBUG_PRINTF(("NvOsConfigSetStateInternal(): Profile binary data is too large"));
                retVal = NvError_InvalidSize;
                break;
            }

            // Allocate a buffer to hold the string data. Make it 33% larger than
            // the binary size plus 1 for string terminator
            bufSize = (valueSize * 4 + 2) / 3 + 1;
            bufPtr = (NvU8 *) NvOsAlloc(bufSize);
            if (bufPtr == NULL)
            {
                NV_DEBUG_PRINTF(("NvOsConfigSetStateInternal(): Failed to allocate string data buffer"));
                retVal = NvError_InsufficientMemory;
                break;
            }

            bufPtr[0] = 0;

            retVal = NvOsBinary2String((char *)value, valueSize,
                                      (char *)bufPtr, bufSize);
            NV_ASSERT(retVal == NvSuccess);

            if (retVal != NvSuccess)
            {
                NV_DEBUG_PRINTF(("NvOsConfigSetStateInternal(): NvOsBinary2String() failed"));
                NvOsFree(bufPtr);
                break;
            }
            retVal = NvOsConfigFileWriteState(NVOS_STEREO_PROFILES_FILE_PATH, name, (const char *)bufPtr);

            NvOsFree(bufPtr);

            break;
        }
        case NvOsStateId_3dv_Gles_Settings:
        default:
        {
            retVal = NvOsConfigFileWriteState(NVOS_GLES_SETTINGS_FILE_PATH, name, value);
            NV_DEBUG_PRINTF(("NvOsConfigSetStateInternal(): GLES setting write done, return error code = %s",
                retVal));
            break;
        }
    }

    if (retVal != NvSuccess)
    {
        NV_DEBUG_PRINTF(("NvOsConfigSetStateInternal(): Failed, retVal = 0x%x", retVal));
    }

    return retVal;

#endif // ANDROID

}

#if defined(ANDROID)

// Note: The functions NvOsBinary2String and NvOsString2Binary are from code checked
// in originally by Robert Tray. Attempting to merge in that functionality here

/**
 * Translation table for base64 encoding
 */
static const char b64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * NvOsBinary2String - Convert a binary data buffer to a string representation
 * Use base64 conversion right now to keep things simple.
 * (ParseConfigFile below has strict limits on allowable chars)
 * Could be enhanced to do 'yenc' which improves the size ratio
 */
NvError NvOsBinary2String(const char *binaryBufPtr, NvU32 binaryBufSize,
                         char *outStringPtr, NvU32 outStringSize)
{
    NvU32 i = 0;
    const char *in = binaryBufPtr;
    char *out = outStringPtr;

    // output buffer must be at least 4/3 the size of the input buffer
    NV_ASSERT(outStringSize >= (binaryBufSize * 4 + 2)/3);

    if ((in == NULL) || (out == NULL) || (binaryBufSize == 0) ||
        (outStringSize < (binaryBufSize * 4 + 2)/3))
    {
        return NvError_BadParameter;
    }

    while ((i+2) < binaryBufSize) // groups of 3
    {
        // 3 input chars equals 4 output chars
        out[0] = b64[ in[0] >> 2];
        out[1] = b64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
        out[2] = b64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ];
        out[3] = b64[ in[2] & 0x3f ];

        in += 3;
        out += 4;
        i += 3;
    }

    if (i < binaryBufSize) // last remaining 1 or 2 bytes
    {
        switch (binaryBufSize - i)
        {
            case 2:
                out[0] = b64[ in[0] >> 2];
                out[1] = b64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
                out[2] = b64[ ((in[1] & 0x0f) << 2) ];
                in += 2;
                out += 3;
                i += 2;
                break;

            case 1:
                out[0] = b64[ in[0] >> 2];
                out[1] = b64[ ((in[0] & 0x03) << 4) ];
                in += 1;
                out += 2;
                i += 1;
                break;

            default:
                // unexpected
                break;
        }
    }

    *out = '\0'; // terminating NULL

    NV_ASSERT(i == binaryBufSize);
    NV_ASSERT(out <= (outStringPtr + outStringSize));

    return NvSuccess;
}

/**
 * Translation table for base64 decoding
 */
static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

/**
 * NvOsString2Binary - Convert an encoded char buffer to binary
 * Undo what NvOsBinary2String did above
 */
NvError NvOsString2Binary(const char *inStringPtr, NvU32 inStringSize,
                         unsigned char *binaryBufPtr, NvU32 *binaryBufSize)
{
    NvU32 i = 0;
    const char *in = inStringPtr;
    unsigned char *out = (unsigned char *)binaryBufPtr;

    if ((in == NULL) || (binaryBufSize == NULL) || (inStringSize == 0))
    {
        return NvError_BadParameter;
    }

    if (out == NULL)
    {
        // Valid if the caller is requesting the size of the decoded
        // binary buffer so they know how much to alloc.
        *binaryBufSize = 0;
    }
    else
    {
        // Validate the size of the passed in binary data buffer.
        // In theory the binaryBufSize should be 3/4 the size of
        // the input string buffer.  But if there were some white
        // space chars in the buffer then it's possible that the
        // binary buffer is smaller.
        // We'll validate against 2/3rds the size of the inStringSize
        // here.  That allows for some slop.
        // Below we have code that makes sure we don't overrun the
        // output buffer.  (Which sort of makes this here irrelevant...)

        // NV_ASSERT(binaryBufSize >= (inStringSize * 3)/4);
        NV_ASSERT(*binaryBufSize >= (inStringSize * 2)/3);
        if (*binaryBufSize < (inStringSize * 2)/3)
        {
            return NvError_InsufficientMemory;
        }
    }

    // This loop is less efficient than it could be because
    // it's designed to tolerate bad characters (like whitespace)
    // in the input buffer.
    while (i < inStringSize)
    {
        int vlen = 0;
        unsigned char vbuf[4], v;  // valid chars

        // gather 4 good chars from the input stream
        while ((i < inStringSize) && (vlen < 4))
        {
            v = 0;
            while ((i < inStringSize) && (v == 0))
            {
                // This loop skips bad chars
                v = (unsigned char) in[i++];
                v = (unsigned char) ((v < 43 || v > 122) ? 0 : cd64[v - 43]);
                if (v != 0)
                {
                    v = (unsigned char) ((v == '$') ? 0 : v - 61);
                }
            }
            if (v != 0)
            {
                vbuf[vlen++] = (unsigned char)(v - 1);
            }
        }

        if (out == NULL)
        {
            // just measuring the size of the destination buffer
            if ((vlen > 1) && (vlen <= 4))   // only valid values
            {
                *binaryBufSize += vlen - 1;
            }
            continue;
        }

        NV_ASSERT((out + vlen - 1) <= (binaryBufPtr + *binaryBufSize));
        if ((out + vlen - 1) > (binaryBufPtr + *binaryBufSize))
        {
            return NvError_InsufficientMemory;
        }

        switch (vlen)
        {
            case 4:
                // 4 input chars equals 3 output chars
                out[2] = (unsigned char ) (((vbuf[2] << 6) & 0xc0) | vbuf[3]);
                // fall through
            case 3:
                // 3 input chars equals 2 output chars
                out[1] = (unsigned char ) (vbuf[1] << 4 | vbuf[2] >> 2);
                // fall through
            case 2:
                // 2 input chars equals 1 output char
                out[0] = (unsigned char ) (vbuf[0] << 2 | vbuf[1] >> 4);
                out += vlen - 1;
                break;
            case 1:
                // Unexpected
                NV_ASSERT(vlen != 1);
                break;
            case 0:
                // conceivable if white space follows the end of valid data
                break;
            default:
                // Unexpected
                NV_ASSERT(vlen <= 4);
                break;
        }
    }

    return NvSuccess;
}

/**
 * NvOsHexToChar - Convert a hex number to a char value.
 */
inline char NvOsHexToChar(char hexVal)
{
    char retVal = 0;

    if ((hexVal >= 'A') && (hexVal <= 'F')) {
        retVal = hexVal - 'A' + 10; // A ... F
    } else if ((hexVal >= 'a') && (hexVal <= 'f')) {
        retVal = hexVal - 'a' + 10; // a ... f
    } else if ((hexVal >= '0') && (hexVal <= '9')) {
        retVal = hexVal - '0'; // 0 ... 9
    } else {
        // Invalid character
        retVal = 0;
    }

    return retVal;
}

/**
 * Functions below are almost the same as the ones at the end of nvos_config.c
 * Update the path for the settings, and add a couple more chars (+ and /) to
 * the AllowedChar() check (taken from code originally checked in by Robert Tray)
 */

/**
 * The allowed characters for names and values
 */
static int AllowedChar(char c)
{
    return ((c>='0' && c<='9') ||
            (c>='A' && c<='Z') ||
            (c>='a' && c<='z') ||
            (c=='+' || c=='/') ||  // for base64 encoding
            (c == '_') || (c == '-') || (c == '.'));
}

static int Whitespace(char c)
{
    return (c == ' ' || c == '\t');
}

static int Newline(char c)
{
    return (c == '\n' || c == '\r');
}

static NvError ParseConfigFile(const char *filePath, NvOsConfig* conf)
{
    NvOsFileHandle file = NULL;
    NvError err;
    NvOsStatType stat;
    NvU32 read;
    char* p;
    int mode = 0;

    conf->buf = NULL;
    conf->num = 0;

    err = NvOsFopen(filePath, NVOS_OPEN_READ, &file);
    if (err) return err;

    err = NvOsFstat(file, &stat);
    if (err) goto clean;

    if (stat.type != NvOsFileType_File)
    {
        err = NvError_InvalidState;
        goto clean;
    }

    conf->buf = NvOsAlloc((NvU32)stat.size + 1);
    if (!conf->buf)
    {
        err = NvError_InsufficientMemory;
        goto clean;
    }

    err = NvOsFread(file, conf->buf, (NvU32)stat.size, &read);
    if (err) goto clean;

    NV_ASSERT(read == (NvU32)stat.size);

    NvOsFclose(file);
    file = NULL;

    for (p = conf->buf; p < (conf->buf + read); p++)
    {
        switch(mode)
        {
        case 0:
            if (AllowedChar(*p))
            {
                mode = 1;
                conf->names[conf->num] = p;
            }
            break;
        case 1:
            if (*p == '=')
            {
                mode = 2;
                *p = 0;
            }
            else if (Whitespace(*p))
            {
                *p = 0;
            }
            else if (!AllowedChar(*p))
            {
                err = NvError_BadValue;
                goto clean;
            }
            break;
        case 2:
            if (AllowedChar(*p))
            {
                mode = 3;
                conf->values[conf->num] = p;
            }
            break;
        case 3:
            if (Newline(*p) || (p == (conf->buf + read)))
            {
                mode = 0;
                *p = 0;
                conf->num++;
            }
            else if (Whitespace(*p))
            {
                *p = 0;
            }
            else if (!AllowedChar(*p))
            {
                err = NvError_BadValue;
                goto clean;
            }
            break;
        }
    }

 clean:
    if (err && conf->buf)
        NvOsFree(conf->buf);

    if (file)
        NvOsFclose(file);

    return err;
}

static NvError DumpConfigFile(const char *filePath, NvOsConfig* conf)
{
    NvOsFileHandle file = NULL;
    NvError err;
    int i;

    /* TODO: [ahatala 2010-03-05] what to do if writing fails?
       Should possibly write to a temp file first and copy? */

    err = NvOsFopen(filePath, NVOS_OPEN_WRITE|NVOS_OPEN_CREATE, &file);
    if (err) return err;

    for (i = 0; i < conf->num; i++)
    {
        err = NvOsFwrite(file, conf->names[i], NvOsStrlen(conf->names[i]));
        if (err) break;
        err = NvOsFwrite(file, " = ", 3);
        if (err) break;
        err = NvOsFwrite(file, conf->values[i], NvOsStrlen(conf->values[i]));
        if (err) break;
        err = NvOsFwrite(file, "\n", 1);
        if (err) break;
    }

    NvOsFclose(file);
    return err;
}

NvError NvOsConfigFileWriteState(const char *filePath, const char *name, const char* value)
{
    NvError err;
    NvOsConfig conf;
    int i = 0;

    if ((filePath == NULL) || (name == NULL) || (value == NULL))
    {
        return NvError_BadParameter;
    }

    err = ParseConfigFile(filePath, &conf);
    if (err && err != NvError_FileNotFound)
        return err;

    for (; i < conf.num; i++)
    {
        if (NvOsStrcmp(conf.names[i], name) == 0)
            break;
    }

    if (i == conf.num)
    {
        conf.num++;
        if (conf.num > NVOS_MAX_CONFIG_VARS)
        {
            NvOsFree(conf.buf);
            return NvError_InsufficientMemory;
        }
    }

    conf.names[i] = name;
    conf.values[i] = value;

    err = DumpConfigFile(filePath, &conf);

    NvOsFree(conf.buf);

    return err;
}

NvError NvOsConfigFileReadState(const char *filePath, const char *name, char *value, NvU32 size)
{
    NvError err;
    NvOsConfig conf;
    int i = 0;

    err = ParseConfigFile(filePath, &conf);
    if (err)
    {
        return (err == NvError_FileNotFound) ? NvError_ConfigVarNotFound : err;
    }

    for (; i < conf.num; i++)
    {
        if (NvOsStrcmp(conf.names[i], name) == 0)
            break;
    }

    if (i == conf.num)
        err = NvError_ConfigVarNotFound;
    else
        NvOsStrncpy(value, conf.values[i], size);

    NvOsFree(conf.buf);
    return err;
}
#endif // ANDROID

