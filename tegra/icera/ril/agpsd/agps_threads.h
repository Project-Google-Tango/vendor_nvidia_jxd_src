/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup agps
 * @{
 */

/**
 * @file agps_threads.c NVidia aGPS Downlink and Uplink threads
 *
 */


#ifndef AGPS_THREADS_H
#define AGPS_THREADS_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

#include "agps_port.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/
typedef struct
{
    int agps_socket_fd;
    int client_socket_fd;
    int modem_tty_fd;
    int threads_must_quit;
    agps_PortMutex tty_port_mutex ;
    agps_PortMutex socket_port_mutex ;
} agps_PortHandlers;

typedef struct
{
    char * device_path;
    char * socket_path;

} agps_DeviceInfo;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

/**
 * Uplink thread: blocking read of data from socket/gps, and pass it to tty/modem
 *
 * @param arg Argument
 * @return this function should never return
 */
void *agps_uplinkThread(void *arg);

/**
 * Downlink thread: blocking read of data from tty/modem, and pass it to the socket/gps
 *
 * @param arg Argument
 * @return this function should never return
 */
void *agps_downlinkThread(void *arg);

/**
 * Test thread: Replaces the GPS daemon, it connects to the socket and sends/receives some AT commands
 *
 * @param arg Argument
 * @return this function should never return
 */
void *agps_testThreadAndroid(void *arg);


#endif /* AGPS_THREADS_H */

