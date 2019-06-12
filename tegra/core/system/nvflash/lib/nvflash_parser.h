/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVFLASH_PARSER_H
#define INCLUDED_NVFLASH_PARSER_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvos.h"
#include "nvddk_fuse.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * Parser structure for storing <key, value> pair.
 */
typedef struct
{
    char* key;
    char* value;
} NvFlashSectionPair;

typedef struct
{
    NvU32 n;    // Number of values in array pointed by data
    NvU32 max;  // Maximum values that can be filled in data
    void *data; // pointer to array of any datatype
}NvFlashVector;

/*
 * Enums for parser.
 */
typedef enum
{
    P_KEY,      // Expecting key
    P_PKEY,     // Key paritally parsed
    P_VALUE,    // Expecting value
    P_PVALUE,   // Value partially parsed
    P_COMPLETE, // Parsing of a section complete
    P_CONTINUE, // Continue parsing next sections
    P_STOP,     // Stop parsing next sections
    P_ERROR,    // Parsing error
} NvFlashParserStatus;

/*
 * Remove space and newline from begining and end of string.
 *
 * string: String to be chomped
 */
NvError
nvflash_chomp_string(char **string);

/*
 * Get the part of string from given position upto delimiter character.
 *
 * string: Input string
 * pos: Offset in string from where to start
 * delims: String of delimiting characters
 * token: Part of string from pos upto one of the characters in delims
 */
NvError
nvflash_get_nextToken(char *string, NvU32 *pos, const char *delims, char **token);

/*
 * Prototype for callback function
 */
typedef NvFlashParserStatus
parse_callback(NvFlashVector *rec, void *aux);

/*
 * Callback structure for fuse_write
 */

typedef struct NvFuseWriteCallbackArgRec
{
    NvFuseWrite FuseInfo;
} NvFuseWriteCallbackArg;

/*
 * Concates the str2 into str1. Reallocates str1 as per required.
 */
NvError
nvflash_concate_strings(char **str1, char *str2);

/**
 * Prases the file and call callback on finding each section.
 *
 * hFile: Handle to file to be parsed
 * callback: Callback function which is to be called on each parsed section
 * offset: If not null then parser will update this value to the file offset
           after the current parsed section.
 * aux: Additional parameters to be passed to callback function
 */
NvError
nvflash_parser(NvOsFileHandle hFile, parse_callback *callback,
                      NvU64 *offset, void *aux);

NvBool nvflash_fuse_value_alloc(NvFuseWrite *fusedata,
                        NvFlashSectionPair *pairs, NvU8 index1, NvU8 index2);

NvFlashParserStatus
nvflash_fusewrite_parser_callback(NvFlashVector *rec, void *aux);

NvDdkFuseDataType
nvflash_get_fusetype(const char *fuse_name);

/**
 * Parse token with case Sensitive
 */
void NvParseCaseSensitive(NvBool format);

/**
 * Determines if a given cfg is common cfg or not.
 *
 * @param Cfgname configuration file name
 *
 * Returns NV_TRUE if the given file is common cfg,
 * NV_FALSE otherwise.
 */
NvBool nvflash_check_cfgtype(const char *pCfgName);


#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVFLASH_PARSER_H */
