/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NV_LOGGER_H
#define INCLUDED_NV_LOGGER_H

/**
 * NvLogger provides a basic set of interfaces to abstract the logging mechanism
 * across all supported operating systems.
 */

#include "nvos.h"

#if !defined(NV_LOGGER_ENABLED)
#define NV_LOGGER_ENABLED 0
#endif

/**
 * Client supported by NvLogger.
 *
 *  @note Any new client should be added only at the end of list before NVLOG_MAX_CLIENTS.
 *  Also, the corresponding client string  has to be added at the end of client string table in nvmm_logger.c
 */
typedef enum
{
    NVLOG_MP3_DEC = 0,
    NVLOG_MP4_DEC, // = 1
    NVLOG_AVI_DEC, // = 2
    NVLOG_WMA_DEC, // = 3
    NVLOG_AAC_DEC, // =4
    NVLOG_WAV_DEC, // = 5
    NVLOG_VIDEO_DEC, // = 6
    NVLOG_VIDEO_RENDERER, // = 7
    NVLOG_MIXER, // = 8
    NVLOG_WMDRM, // = 9
    NVLOG_MP3_PARSER, // = 10
    NVLOG_MP4_PARSER, // = 11
    NVLOG_AVI_PARSER, // = 12
    NVLOG_ASF_PARSER, // = 13
    NVLOG_AAC_PARSER, //=14
    NVLOG_AMR_PARSER, //=15
    NVLOG_SUPER_PARSER, // = 16
    NVLOG_TRACK_LIST, // = 17
    NVLOG_MP4_WRITER, // = 18
    NVLOG_CONTENT_PIPE, // = 19
    NVLOG_NVMM_BUFFERING, // = 20
    NVLOG_NVMM_TRANSPORT, // = 21
    NVLOG_OMX_IL_CLIENT, // = 22
    NVLOG_NVMM_MANAGER, // = 23
    NVLOG_NVMM_BASEWRITER, // = 24
    NVLOG_MKV_PARSER, // = 25
    NVLOG_NEM_PARSER, // = 26
    NVLOG_DRM_PLAY, // = 27
    NVLOG_MAX_CLIENTS
}NvLoggerClient;

/**
 * Defines to filter/segregate logger data.
 */
typedef enum
{
    // Highest priority, No logging
    // EX: nothing is ever printed.
    NVLOG_NONE=0,

    // This should be used when system encountered a serious error from which it is unable to recover.
    // Ex: Something went really wrong with the AVP core - you had to reset it, ASSERT failures etc.
    NVLOG_FATAL,

    // This should be used to inform about any type logical failures.
    // Ex: Failure to open a critical device like OMX component, parse errors etc
    NVLOG_ERROR,

    // This should be used to log errors which are ignorable.
    // Ex: seeking past the EOF of file in content pipe, one of the multiple tracks in the stream is not valid etc
    NVLOG_WARN,

    // This should be used for tracing the code flow. All important functions and states which are touched should be logged under this.
    // Ex: open()->play->paused->close()
    NVLOG_INFO,

    // This should be used to log the context info helpful for understanding the code flow.
    // Ex:  track info, framing info etc.
    NVLOG_DEBUG,

    // Lowest priority, This should be used for logging large DEBUG info.
    // Ex: Timestamps, frame offsets, streaming information can be logged under this.
    NVLOG_VERBOSE
}NvLoggerLevel;

/**
 * The following options are supported through NvOsConfig registry to dynamically control
 * the logging mechanism
 *
 *  @note All the config values have to be provided as strings only
 *
 * KEY_WORD                  |  KEY_VALUE
 *--------------------------------
 * nvlog.level                    <LEVEL> sets all the clients to the given output level
 *
 * nvlog.<ClientID>.level <LEVEL> sets the client's output level for the given output level.
 *                                       ClientID stands for the client enum value defined in NvLoggerClient.
 *
 *  @note nvlog.<ClientID>.level takes precedence compared to nvlog.level
 *
 *<ANDROID>
 * Ex: 'adb shell setprop persist.tegra.nvlog.level 4' should enable debug traces for all clients upto INFO level
 * Ex: 'adb shell setprop persist.tegra.nvlog.14.level 5' should enable  debug traces for NVLOG_SUPER_PARSER client upto DEBUG level
 *
 *<WINCE>
 * Goto [HKEY_LOCAL_MACHINE\Software\NVIDIA Corporation\NvOs] in the registry to modify the nvlog levels.
 */


/** Logs the data for the given client.
 *
 * This function logs the information only if the given output level
 * matches with the nvlog.<ClientID>.level config value set in registry.
 * The client output level will be ignored if nvlog.enable.all
 * is set to 1 in the registry.
 *
 * @param ClientID A unique ID which identifies the logging client.
 *              This value is selected from NvLoggerClient.
 * @param Level Required to filter the output data.
 *               This value can be a single/combination of values defined in NvLoggerLevel.
 *
 */
void NvLoggerPrint(NvLoggerClient ClientID, NvLoggerLevel Level, const char* pFormat, ...);


#if !NV_IS_AVP
/** Gets the logger level for the given client dynamically.
 *
 * @note This API can be used only on CPU side.
 *
 * @param ClientID A unique ID which identifies the logging client.
 *              This value is selected from NvLoggerClient.
 * @return Level Required to filter the output data.
 *               This value can be a single/combination of values defined in NvLoggerLevel.
 *
 */
NvLoggerLevel NvLoggerGetLevel(NvLoggerClient ClientID);
#endif

#if NV_IS_AVP
/** Sets the logger level for the given client dynamically.
 *
 * @note This API can be used only on AVP side.
 *
 * @param ClientID A unique ID which identifies the logging client.
 *              This value is selected from NvLoggerClient.
 * @param Level Required to filter the output data.
 *               This value can be a single/combination of values defined in NvLoggerLevel.
 *
 */
void NvLoggerSetLevel(NvLoggerClient ClientID, NvLoggerLevel Level);
#endif

/** Macros for adding/removing logger API hooks to/from the client code
 *   based on the NV_LOGGER_ENABLED settings
 *
 * @note These macros can used in either debug/release mode
 *
 * Ex: set "LCDEFS += -DNV_LOGGER_ENABLED=1"
 * in client MakeFile enables logger hooks for that module
 *
 * alternatively, users can "#define NV_LOGGER_ENABLED 1"
 * in the respective source files to enable the logger hooks.
 *
 */
#if (NV_LOGGER_ENABLED)
    #define NV_LOGGER_PRINT(_x_) do { NvLoggerPrint _x_; } while (0)
#else
    #define NV_LOGGER_PRINT(_x_) do { } while (0)
#endif

#if (NV_LOGGER_ENABLED && !NV_IS_AVP)
    #define NV_LOGGER_GETLEVEL(_x_) NvLoggerGetLevel(_x_)
#else
    #define NV_LOGGER_GETLEVEL(_x_) NVLOG_NONE
#endif

#if (NV_LOGGER_ENABLED && NV_IS_AVP)
    #define NV_LOGGER_SETLEVEL(_x_, _y_) NvLoggerSetLevel(_x_, _y_)
#else
    #define NV_LOGGER_SETLEVEL(_x_, _y_)
#endif

#endif // INCLUDED_NV_LOGGER_H

