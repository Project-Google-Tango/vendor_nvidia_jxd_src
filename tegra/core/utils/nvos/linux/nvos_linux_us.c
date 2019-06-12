/*
 * Copyright (c) 2013-2014,  NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef ANDROID
#include <cutils/log.h>
#endif

#include "nvos.h"

#ifndef UNIFIED_SCALING
#define UNIFIED_SCALING 0
#endif

#ifdef ANDROID
#if UNIFIED_SCALING
#include <sys/socket.h>
#include <sys/un.h>
#endif /* UNIFIED_SCALING */
#else
#include <execinfo.h>
#endif

#ifdef ANDROID
#if UNIFIED_SCALING
/*
 * Line protocol for unified scaling socket comm: [char] [payload]
 *
 * '#' <string> '\n'
 *
 *   Client identification string. Not required. Used for diagnostics and
 *   debugging prints if sent.
 *
 * '!' <str> '=' <str> '\n'
 *
 *   Property update. The first string is the key and the second string is
 *   the value. Writes these into ussrd's configuration properties: this has
 *   the same effect as running "adb shell setprop key val" but only updates
 *   ussrd's internal copy of the property.
 *
 * '%' <int32> '\n'
 *
 *   [To be deprecated] Throughput hint in range of [1..2000]: 1000 means
 *   we're at target throughput. 500 means we're at half-throughput and 2000
 *   means we're double throughput. This has the implied usecase "graphics".
 *
 * '@' <int32> '\n'
 *
 *   [To be deprecated] Framerate hint in range of [0..inf]: Ask ussrd to
 *   use the value as the target framerate for scaling. Sending 0 (zero)
 *   means use the default target framerate for the system, scaler, or
 *   application. This has the implied usecase "graphics".
 *
 * '=' <str> ' ' <uint32> ' ' <uint32> ' ' <uint32> [ ' ' <uint32> [ ' ' <uint32> ] ]'\n'
 *
 *   Usecase and any other throughput hint: the first string is the usecase
 *   token (string of [a-z] only), the first uint is the hint type
 *   [0..UINT_MAX], the second uint is the hint value [0..UINT_MAX], and the
 *   third is the timeout in milliseconds. Hint types 0 and 1 are the same
 *   as '%' and '@', respectively. The last two uints are optional and may
 *   carry undocumented out-of-band data.
 *
 *   To cancel hints, the client sends a nasty hint type of INT_MAX
 *   (NvOsTPHintType_Force32) to the unified scaler. That shouldn't be in
 *   any legitimate use, I suppose.
 */

/* Control socket path. This should be platform-specific. */
#define UNIFIED_SCALING_SOCKET "/data/misc/phs/phsd.ctl"

/* On error, socket reconnect timeout. */
#define UNIFIED_SCALING_RECONNECT_BUMP_US (1000000)         /* 1s */
#define UNIFIED_SCALING_RECONNECT_TIMEOUT_US (10*1000000)   /* 10s */

/* For locking the API functions. */
static pthread_mutex_t hint_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Socket to ussrd. */
static int scaler_fd = -1;
static NvU32 max_hint_timeout = INT_MAX;

/* Usecase list to provide indexes for canonical access. */
#define MAX_USECASES 8
#define MAX_NAME_LENGTH 16
static char usecases[MAX_USECASES][MAX_NAME_LENGTH];

/* Local helper prototypes. */

static int NvGetUsecase (const char *usecase, NvBool create);
static int NvValidateUsecase (const char *usecase);
static int NvSendHints (NvU32 client_tag, va_list args);
static int NvConnectSocket (void);
static int NvDoHandshake (NvU32 *value);

#   endif /* UNIFIED_SCALING */
#endif /* ANDROID */

/*
 * Interface functions.
 */

int NvOsSendThroughputHint (const char *usecase, NvOsTPHintType type, NvU32 value, NvU32 timeout_ms)
{
    int rv = 0;

#ifdef ANDROID
#if UNIFIED_SCALING
    rv = NvOsVaSendThroughputHints(NVOSTPHINT_DEFAULT_TAG, usecase, type, value, timeout_ms, NULL);
#endif
#endif

    return rv;
}

int NvOsVaSendThroughputHints (NvU32 client_tag, ...)
{
    int rv = 0;

#ifdef ANDROID
#if UNIFIED_SCALING
    va_list args;

    va_start(args, client_tag);
    {
        pthread_mutex_lock(&hint_mutex);
        rv = NvSendHints(client_tag, args);
        pthread_mutex_unlock(&hint_mutex);
    }
    va_end(args);
#endif
#endif

    return rv;
}

void NvOsCancelThroughputHint (const char *usecase)
{
#ifdef ANDROID
#if UNIFIED_SCALING

    /* Cancel ALL hints: just drop the fd. */
    if (usecase == NULL) {
        pthread_mutex_lock(&hint_mutex);
        if (scaler_fd != -1) {
            close(scaler_fd);
            scaler_fd = -1;
        }
        pthread_mutex_unlock(&hint_mutex);
    }

    /* Cancel a specific usecase: send a special cancellation hint. */
    else {
        NvOsVaSendThroughputHints(NVOSTPHINT_DEFAULT_TAG,
                                  usecase, NvOsTPHintType_Force32, 0, NVOSTPHINT_TIMEOUT_ONCE,
                                  NULL);
    }
#endif
#endif
}

/*
 * Local helper functions.
 */

#ifdef ANDROID
#if UNIFIED_SCALING
int NvGetUsecase (const char *usecase, NvBool create)
{
    int start, i;

    /* Use the first letter as hash. */
    i = start = (*usecase) % MAX_USECASES;

    do {
        /* Existing slot. */
        if (strncmp(usecase, usecases[i], MAX_NAME_LENGTH - 1) == 0) {
            return i;
        }

        /* Empty slot. */
        else if (*usecases[i] == 0) {
            if (create) {
                if (NvValidateUsecase(usecase)) {
                    strcpy(usecases[i], usecase);
                    return i;
                }
            }
            return -1;
        }

        if (++i == MAX_USECASES)
            i = 0;
    } while (i != start);

    return -1;
}

int NvValidateUsecase (const char *usecase)
{
    int len = strlen(usecase);

    /* Must not be an empty string and must fit in max size. */
    if (len > 0 && len < MAX_NAME_LENGTH - 1) {
        /* Only accept letters [a-z] and a NUL terminator in the usecase. */
        for (;;) {
            if (*usecase >= 'a' && *usecase <= 'z'){
                usecase++;
                continue;
            }
            if (*usecase == '\0') {
                return 1;
            }

            break;
        }
    }
    return 0;
}

int NvSendHints (NvU32 client_tag, va_list args)
{
    int rv = -2;

    if (NvConnectSocket()) {
        char buffer[1024];
        int len;
        char *string;
        int usecase;
        NvOsTPHintType type;
        NvU32 value;
        NvU32 timeout_ms;
        int space = sizeof(buffer) - 1;
        int written = 0;

        for (;;) {
            /* Check for terminator. */
            string = va_arg(args, char *);
            if (string == NULL)
                break;

            /* Unpack parameters. */
            usecase     = NvGetUsecase(string, NV_TRUE);
            type        = va_arg(args, NvOsTPHintType);
            value       = va_arg(args, NvU32);
            timeout_ms  = va_arg(args, NvU32);

            /* Validate. */
            if (usecase < 0)
                return -1;
            if (timeout_ms > max_hint_timeout)
                return max_hint_timeout;

            /* Append hint. */
            len = snprintf(&buffer[written], space, "=%s %d %u %u %u\n",
                           usecases[usecase],
                           type,
                           value,
                           timeout_ms,
                           client_tag);
            if (len >= space) {
                ALOGD("too many throughput hints: buffer full, hints not sent");
                return -3;
            }

            /* Update cursors. */
            space -= len;
            written += len;
        }
        buffer[written] = '\0';

        if (send(scaler_fd, buffer, written, MSG_NOSIGNAL) != -1) {
            rv = 0;
        } else {
            ALOGD("sending throughput hint to unified scaler failed: %s", strerror(errno));
            close(scaler_fd);
            scaler_fd = -1;
        }
    }

    return rv;
}

int NvConnectSocket (void)
{
    static int failure = 0;
    static NvU64 last_try = 0;
    static NvU64 reconn_timeout = UNIFIED_SCALING_RECONNECT_BUMP_US;
    NvU64 t = NvOsGetTimeUS();

    /* To avoid excess overhead make sure that we won't try to reconnect to
     * unified scaler too often. */
    if (failure) {
        if (reconn_timeout < UNIFIED_SCALING_RECONNECT_TIMEOUT_US)
            reconn_timeout += UNIFIED_SCALING_RECONNECT_BUMP_US;

        if (t - last_try < reconn_timeout)
            return 0;

        last_try = t;
    }

    /* Try to open a local socket to the unified scaler. */
    if (scaler_fd == -1) {
        failure = 1;
        scaler_fd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (scaler_fd != -1) {
            struct sockaddr_un addr;

            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path,
                    UNIFIED_SCALING_SOCKET,
                    sizeof(addr.sun_path) - 1);

            if (connect(scaler_fd,
                        (struct sockaddr *)&addr,
                        sizeof(addr)) != -1)
            {
                if (NvDoHandshake(&max_hint_timeout))
                    failure = 0;
            }
        }
    }

    /* Update failure status. */
    if (failure) {
        if (scaler_fd != -1) {
            close(scaler_fd);
            scaler_fd = -1;
        }

        ALOGD("cannot connect to power hinting service");
        return 0;
    }

    /* Connected! Reset reconnect timeout. */
    reconn_timeout = UNIFIED_SCALING_RECONNECT_BUMP_US;
    return 1;
}

int NvDoHandshake (NvU32 *rv)
{
    char buffer[256];
    char procname[128];
    char *endptr;
    NvU32 value;
    int bytes;

    NvOsGetProcessInfo(procname, sizeof(procname));

    if (send(scaler_fd,
             buffer,
             snprintf(buffer, sizeof(buffer), "#%s\n", procname),
             MSG_NOSIGNAL) == -1)
    {
        ALOGD("cannot send hello handshake to power hinting service");
        return 0;
    }

    bytes = recv(scaler_fd, buffer, sizeof(buffer) - 1, MSG_NOSIGNAL);
    buffer[sizeof(buffer) - 1] = '\0';
    if (bytes <= 0)
    {
        ALOGD("cannot read hello response from power hinting service");
        return 0;
    }


    if (sscanf(buffer, "%u\n", &value) != 1) {
        ALOGD("invalid hello response from power hinting service");
        return 0;
    }

    *rv = value;
    return 1;
}
#endif
#endif
