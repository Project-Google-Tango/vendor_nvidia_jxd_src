/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVAPPUTIL_H
#define INCLUDED_NVAPPUTIL_H

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvcommon.h"
#include "nvos.h"

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

/*=========================================================================*/
/** NvAuRand16() - Returns a 16 bit pseudorandom number
 *
 * Returns a pseudorandom number in the range [0,65535].  This is implemented
 * using a very simple and very fast LCRNG.  The caller is responsible for
 * thread safety and determinism -- this API allows it to easily have a
 * separate PRNG per thread, for example.
 *
 * @param pRandState Points to an NvU32 containing the state of the PRNG.  This
 *     number will be updated as a side effect of this call.  Before you call
 *     this function for the first time, you should initialize this number to
 *     your random seed (can be as simple as setting it to 1).
 */
NvU16 NvAuRand16(NvU32 *pRandState);

/*=========================================================================*/
/** NvAuReadline() - DEPRECATED - use use NvTestReadline() instead
 *  @returns NvError_NotImplemented
 */
NvError
NvAuReadline(const char *, char **);

/*=========================================================================*/
/** NvAuGetOpt - Parses application command line options.
 *
 * @param argc             The program's argc
 * @param argv             The program's argv
 * @param optstring        The option string:
 *                            one letter for each option accepted
 *                            colon following each letter that takes an argument
 * @param index            pointer to int inialized to 0
 * @param returned_option  option letter is returned here
 * @param returned_value   argument (or NULL) is returned here
 * @param print_error      if nonzero print message on error
 *
 * This takes a string <optstring> with one letter for each accepted option.
 * Options which are followed by arguments on the command line are followed by
 * a colon in the <optstring>.
 *
 * The <index> should point to an integer which is initially set to 0.
 *
 * Each time NvTestGetOpt() is called it will find the next option and return.
 *
 * When an option not followed by a colon is found:
 *     *returned_option is set to the option character
 *     *returned_value  is set to NULL
 *     NvSuccess is returned
 *
 * When an option followed by a colon is found:
 *     *returned_option is set to the option character
 *     *returned_value  is set to the string following the option
 *     NvSuccess is returned
 *
 * When an argument is found that does not start with a dash:
 *     *returned_option is set 0
 *     *returned_value  is set to the argument
 *     NvSuccess is returned
 *
 * If any error occurs or an unknown option is found:
 *     *returned_option is set 0
 *     *returned_value  is set to the invalid argument
 *     NvError_TestCommandLineError is returned
 */
NvError
NvAuGetOpt(
            int          argc,
            char       **argv,
            const char *optstring,
            int         *index,
            int         *returned_option,
            char       **returned_value,
            int          print_error);

/*=========================================================================*/
/** NvAuGetArgValU32 - convert an argument string to an unsigned integer
 *
 * @param s            - the string
 * @param returned_val - value is returned here
 * @param print_error  - if nonzero print message on error
 *
 * If string is Null, empty, or does not contain an integer number then
 * TestCommandLineError is returned.  The string must contain just digits, or
 * hex digits followed by a leading 0x.
 */
NvError
NvAuGetArgValU32(const char *s, NvU32 *returned_val, int print_error);

/*=========================================================================*/
/** NvAuGetArgValS32 - convert an argument string to a signed integer
 *
 * @param s            - the string
 * @param returned_val - value is returned here
 * @param print_error  - if nonzero print message on error
 *
 * If string is Null, empty, or does not contain an integer number then
 * TestCommandLineError is returned.  The string must contain just digits, or
 * hex digits followed by a leading 0x, with an optional leading - sign.
 */
NvError
NvAuGetArgValS32(const char *s, NvS32 *returned_val, int print_error);

/*=========================================================================*/
/** NvAuGetArgValF32 - convert an argument string to a float
 *
 * @param s            - the string
 * @param returned_val - value is returned here
 * @param print_error  - if nonzero print message on error
 *
 * If string is Null, empty, or does not contain a real number then
 * TestCommandLineError is returned.
 * The number must consist of (in this order):
 *     - optional + or -
 *     - 1 or more digits
 *     - optional decimal point
 *     - 0 or more digits
 */
NvError
NvAuGetArgValF32(const char *s, float *returned_val, int print_error);

/*=========================================================================*/
/** NvAuStrtod - convert a string to a double (like strtod)
 *
 * @param s - the string
 * @param e - first character not used in conversion is returned here
 *
 * If string is Null, empty, or does not contain a real number then
 * 0.0 is returned and e will point to s.
 * The number must consist of (in this order with no spaces):
 *     - optional + or -
 *     - 1 or more digits
 *     - optional decimal point
 *     - 0 or more digits
 */
double NvAuStrtod(const char *s, char **e);

/*=========================================================================*/
/** NvAuErrorName - return string name for error code
 *
 * Returns the string name of the error or "???" if the <err> is unknown
 *
 * @param err - the error
 */
const char *NvAuErrorName(NvError err);


/**
 * Print a string to the application's stdout (whatever that may be).
 *
 * @param format The format string. Follows libc's printf.
 */
void
NvAuPrintf(const char *format, ...);

/**
 * Parses the given string; splits into tokens.  Only one delimiter.
 *
 * @param str The string to tokenize
 * @param delim The character to denote a token boundary
 * @param num Out param: number of tokens found
 * @param tokens Out param: the parsed tokens.
 */
NvError
NvAuTokenize( const char *str, char delim, NvU32 *num, char ***tokens );

/**
 * Frees the resources allocated from NvAuTokenize.
 *
 * @param num The number of tokens in the array
 * @param tokens The token array
 */
void
NvAuTokenFree( NvU32 num, char **tokens );

/**
 * Hashtable handle for use with NvAuHashtable functions.
 */
typedef struct NvAuHashtableRec *NvAuHashtableHandle;

/**
 * Generic string-indexed Hashtable.  Not thread safe.
 *
 * @param buckets The hashtable index width
 * @param hashtable The created hashtable
 */
NvError
NvAuHashtableCreate( NvU32 buckets, NvAuHashtableHandle *hashtable );

/**
 * Inserts an entry into the hashtable.
 *
 * @param hashtable The hashtable
 * @param key The entry key
 * @param data The entry data
 */
NvError
NvAuHashtableInsert( NvAuHashtableHandle hashtable, const char *key,
    void *data );

/**
 * Removes an entry from the hashtable.
 *
 * @param hashtable The hashtable
 * @param key The entry key
 */
void
NvAuHashtableDelete( NvAuHashtableHandle hashtable, const char *key );

/**
 * Finds an entry in the hashtable.
 *
 * @param hashtable The hashtable
 * @param key The entry key
 */
void *
NvAuHashtableFind( NvAuHashtableHandle hashtable, const char *key );

/**
 * Destroys the given hashtable.
 */
void
NvAuHashtableDestroy( NvAuHashtableHandle hashtable );

/**
 * Prepares the hashtable for iteration.
 *
 * @param hashtable The hashtable
 */
void
NvAuHashtableRewind( NvAuHashtableHandle hashtable );

/**
 * Iterate over hashtable entries of the hashtable specified by calling
 * NvAuHashtableRewind.  Successive calls will return pairs (key, data).
 * Behavior is undefined if hashtable is being changed between calls to
 * NvAuHashtableGetNext.
 *
 * @param hashtable The hashtable
 * @param key Current entry key
 * @param data Current entry data
 */
void
NvAuHashtableGetNext( NvAuHashtableHandle hashtable,
                      const char **key, void **data );


/**
 * Process handle for use with NvAuProcess functions.
 */
typedef struct NvAuProcessHandleRec *NvAuProcessHandle;

/**
 * Create a new process that runs the module pointed to by 'path'.
 *
 * The process will be created in suspended state. No code will be
 * executed by the process before the process is started by a call
 * to NvAuProcessStart().
 *
 * Note: do not provide the name of the executable as argv[0].
 * The implementation does this already.
 */
NvError
NvAuProcessCreate( const char* path, int argc, char* argv[],
                   NvAuProcessHandle* p );

/**
 * If the process is running, terminate it. Clean up all resources
 * associated to the process.
 */
void
NvAuProcessDestroy ( NvAuProcessHandle p );

/**
 * Get the process ID of a process, if applicable.
 */
NvU32
NvAuProcessGetPid ( NvAuProcessHandle p );

/**
 * Start running a process.
 */
NvError
NvAuProcessStart( NvAuProcessHandle p );

/**
 * Query whether a process has terminated.
 */
NvBool
NvAuProcessHasExited( NvAuProcessHandle p );


#endif // INCLUDED_NVAPPUTIL_H
