/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup apgs
 * @{
 */

/**
 * @file agps_port.h NVidia aGPS RIL OS Port Layer
 *
 */

#ifndef AGPS_PORT_H
#define AGPS_PORT_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/


/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

#include <semaphore.h>
#include <pthread.h>
#include <sys/un.h>
#define LOG_TAG "AGPSD"
#include <utils/Log.h>
#include <cutils/properties.h> /* system properties */

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/
typedef enum
{
    AGPS_OK,
    AGPS_ERROR_BUFFER_TOO_SMALL,
    AGPS_ERROR_OUT_OF_MEMORY,
    AGPS_ERROR_TOO_MANY_OPTIONS,
    AGPS_ERROR_INVALID_OPTION_SPEC,
    AGPS_ERROR_OPTION_PARSING,
} agps_ReturnCode;


/** type definition for a thread */
typedef pthread_t agps_PortThread;

/** type definition for a semaphore */
typedef sem_t agps_PortSem;

/** type definition for a semaphore */
typedef pthread_mutex_t agps_PortMutex;


/**
* Enumeration of option types
*/
typedef enum
{
    AGPS_OPTION_FLAG,                            /* no parameter */
    AGPS_OPTION_INT,                             /* integer parameter */
    AGPS_OPTION_STRING,                          /* string parameter */
} agps_PortOptionType;

/**
* type definition for an option
*/
typedef struct
{
    /** long option, e.g. --help */
    const char *long_opt;
    /** short option, e.g. -h */
    char short_opt;
    /** option type (flag, integer, string) */
    agps_PortOptionType type;
    /** pointer to pointer to variable. The nature of the variable
     *  depends on the option type.
     *  For flags, an (int*) must be provided
     *  For integers, an (int*) must be provided
     *  For stringsm a (char**) must be provided
     */
    void *var;
    /** help message */
    const char *help;
} agps_PortOption;


#define SOCKET_PATH_MAX_LENGTH 108

typedef struct
{
    sa_family_t sun_family;    /*AF_UNIX*/
    char sun_path[SOCKET_PATH_MAX_LENGTH];        /*path name */

} sockaddr_un;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

/**
 * Create a thread..
 *
 * @param thread         thread handler
 * @param start_routine  thread routine
 * @param arg            pointer to args passed to thread
 *                       routine.
 *
 * @return int 0 on success, value < 0 on failure.
 */
int agps_PortCreateThread(agps_PortThread *thread, void *(*start_routine)(void *), void *arg);

/**
 * Lock a mutex
 *
 * @param agps_PortMutex
 *
 * @return int 0 on success, or an error number shall be returned to indicate the error
 */
int agps_PortMutexLock(agps_PortMutex * mutex) ;

/**
 * Unlock a mutex
 *
 * @param agps_PortMutex
 *
 * @return int 0 on success, or an error number shall be returned to indicate the error
 */

int agps_PortMutexUnlock(agps_PortMutex * mutex);

/**
 * Return pointer to last error string
 *
 */
char * agps_reportLastStringError(void);

/**
* Print usage
* @param options Array of options
*/
void agps_PortUsage(agps_PortOption *options);

/**
* Parse arguments. Arguments from the command line are
* overwritten by Android properties
*
* Note that only AGPS_OPTION_FLAG type of options cannot be
* overwritten by Android properties
*
* All options must have a long format AND a short format
*
* @param options     Pointer to array of options
* @param prop_prefix Prefix to be used when looking for Android
*                    properties
* @param argc        Argument count (from command line)
* @param argv        Array of arguments (from command line)
* @return error code
*/
agps_ReturnCode agps_PortParseArgs(agps_PortOption *options, char *prop_prefix, int argc, char *argv[]);

/**
* Free memory previously allocated by agps_DaemonParseArgs()
* @param options Pointer to array of options
*/
void agps_PortCleanupArgs(agps_PortOption *options);

/**
* Create a socket endpoint
*/
int agps_PortCreateSocket(void);

/**
* Shutdown a socket endpoint
*/
int agps_PortShutdownSocket(int socket);


/**
* Connect to a socket
* @param socket     Socket handler
* @socket_path      Socket path
*/
int agps_PortConnectSocket(int socket, char * socket_path);

/**
* Check if socket_path starts with the right Android path = /dev/socket/
* @socket_path      Socket path
* returns 0 if Ok, -1 if not
*/
int agps_CheckSocketPath(char * socket_path);

/**
* Get control of the socket and wait for the connection from client
* @socket_path      Socket path
* returns socket fd (or -1 if error), and client_fd
*/
int agps_AcceptFirstSocketConnection(const char * socket_path, int * client_fd);

/**
* Wait for new client connection
* @socket_fd      socket descriptor
* returns client socket fd (-1 if error)
*/
int agps_AcceptNewSocketConnection(int socket_fd);

#endif /* AGPS_PORT_H */

