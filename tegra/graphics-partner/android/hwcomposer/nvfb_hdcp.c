/*
 * Copyright (C) 2010-2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 */

#include "nvfb_hdcp.h"

#if NV_HDCP

#include <errno.h>
#include <pthread.h>
#include <sys/select.h>

#include <cutils/atomic.h>
#include <cutils/log.h>

#include "nvassert.h"
#include "hdcp_up.h"

/* Enable HDCP status logging */
#define HDCP_DEBUG 0

/* For debugging on an unsecured device */
#define HDCP_FAKE 0

/* Would be nice if this was defined in sys/time.h */
#define clock_timersub(a, b, res)                     \
    do {                                              \
        (res)->tv_sec  = (a)->tv_sec  - (b)->tv_sec;  \
        (res)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
        if ((res)->tv_nsec < 0) {                     \
            (res)->tv_nsec += 1000000000;             \
            (res)->tv_sec  -= 1;                      \
        }                                             \
    } while (0)

struct nvfb_hdcp {
    HDCP_CLIENT_HANDLE client;
    enum nvfb_hdcp_status status;
    pthread_t thread;
    int pipe[2];
};

enum hdcp_message {
    HDCP_QUIT    = 0,
    HDCP_ENABLE  = 1,
    HDCP_DISABLE = 2,
};

/* Interval of HDCP link polling, in seconds */
#ifndef NV_HDCP_POLL_INTERVAL
#define NV_HDCP_POLL_INTERVAL 5
#endif

/* It may take a few seconds after the connection is established
 * before we can determine if the video sink is HDCP compliant.  We
 * blank protected content while negotiating the link, but if we fail
 * to determine compliance before this timeout (in seconds), we assume
 * the link is not compliant and display a warning.
 */
#define HDCP_TIMEOUT 5

#if HDCP_DEBUG
#define HDCP_TRACE(...) ALOGD("HDCP: "__VA_ARGS__)
static char *HDCP_STATUS_STRINGS[] = {
    "HDCP_STATUS_UNKNOWN",
    "HDCP_STATUS_COMPLIANT",
    "HDCP_STATUS_NONCOMPLIANT",
};

static char *HDCP_RET_STRINGS[] = {
    "HDCP_RET_SUCCESS",
    "HDCP_RET_UNSUCCESSFUL",
    "HDCP_RET_INVALID_PARAMETER",
    "HDCP_RET_NO_MEMORY",
    "HDCP_RET_READS_FAILED",
    "HDCP_RET_READM_FAILED",
    "HDCP_RET_LINK_PENDING",
};
#else
#define HDCP_TRACE(...)
#endif

#if HDCP_FAKE
#define hdcp_open(a, b) HDCP_RET_SUCCESS
#define hdcp_close(a)   HDCP_RET_SUCCESS
#define hdcp_status hdcp_fake_status

static HDCP_RET_ERROR hdcp_fake_status(HDCP_CLIENT_HANDLE client)
{
    static int count = 0;

    if (count++ & 0x8) {
        return HDCP_RET_SUCCESS;
    } else {
        return HDCP_RET_UNSUCCESSFUL;
    }
}
#endif

enum nvfb_hdcp_status nvfb_hdcp_get_status(struct nvfb_hdcp *hdcp)
{
    if (hdcp) {
        return android_atomic_acquire_load((int32_t *) &hdcp->status);
    } else {
        return HDCP_STATUS_NONCOMPLIANT;
    }
}

static void nvfb_hdcp_set_status(struct nvfb_hdcp *hdcp,
                                 enum nvfb_hdcp_status new_status)
{
#if HDCP_DEBUG
    enum nvfb_hdcp_status old_status = nvfb_hdcp_get_status(hdcp);

    if (new_status != old_status) {
        HDCP_TRACE("%s -> %s",
             HDCP_STATUS_STRINGS[old_status],
             HDCP_STATUS_STRINGS[new_status]);
    }
#endif

    NV_ASSERT(hdcp);
    android_atomic_release_store(new_status, (int32_t *)&hdcp->status);
}

static void *hdcp_poll(void *arg)
{
    struct nvfb_hdcp *hdcp = (struct nvfb_hdcp *) arg;
    enum nvfb_hdcp_status link_status = HDCP_STATUS_UNKNOWN;
    struct timespec start;
    int enabled = 0;

    HDCP_TRACE("entering hdcp_poll");
    nvfb_hdcp_set_status(hdcp, HDCP_STATUS_UNKNOWN);

    while (1) {
        struct timeval tv, *timeout = NULL;
        int status, nfds = hdcp->pipe[0] + 1;
        fd_set rfds;

        if (enabled) {
            long interval = NV_HDCP_POLL_INTERVAL;

            /* If status is unknown, retry once per second until
             * HDCP_TIMEOUT is reached.
             */
            if (link_status == HDCP_STATUS_UNKNOWN) {
                interval = 1;
            }

            tv.tv_sec = interval;
            tv.tv_usec = 0;
            timeout = &tv;
        }

        FD_ZERO(&rfds);
        FD_SET(hdcp->pipe[0], &rfds);
        nfds = select(nfds, &rfds, NULL, NULL, timeout);

        if (nfds < 0) {
            if (errno != EINTR) {
                ALOGE("%s: select failed: %s", __func__, strerror(errno));
                pthread_exit((void*)-1);
            }
        } else if (nfds) {
            ssize_t bytes;

            if (FD_ISSET(hdcp->pipe[0], &rfds)) {
                uint8_t value = 0;

                ALOGD("%s: processing pipe fd", __func__);
                do {
                    bytes = read(hdcp->pipe[0], &value, sizeof(value));
                } while (bytes < 0 && errno == EINTR);

                switch (value) {
                case HDCP_QUIT:
                    ALOGD("%s: exiting", __func__);
                    goto quit;
                case HDCP_ENABLE:
                    enabled = 1;
                    link_status = HDCP_STATUS_UNKNOWN;
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    break;
                case HDCP_DISABLE:
                    enabled = 0;
                    link_status = HDCP_STATUS_UNKNOWN;
                    break;
                }
            }
        }

        if (enabled) {
            HDCP_RET_ERROR ret;

            /* Validate HDCP status */
            ret = hdcp_status(hdcp->client);
#if HDCP_DEBUG
            NV_ASSERT(ret < NV_ARRAY_SIZE(HDCP_RET_STRINGS));
#endif
            HDCP_TRACE("hdcp_status -> %s", HDCP_RET_STRINGS[ret]);

            /* If link status is successfully verified, drop to the
             * regular polling interval.
             */
            if (ret == HDCP_RET_SUCCESS) {
                link_status = HDCP_STATUS_COMPLIANT;
            }

            /* If status is unknown, retry once per second until
             * HDCP_TIMEOUT is reached, then assume NONCOMPLIANT.
             */
            else if (link_status == HDCP_STATUS_UNKNOWN) {
                struct timespec now, diff;

                clock_gettime(CLOCK_MONOTONIC, &now);
                clock_timersub(&now, &start, &diff);

                HDCP_TRACE("status unknown for %d seconds", (int)diff.tv_sec);
                if (diff.tv_sec >= HDCP_TIMEOUT) {
                    /* Transition to NONCOMPLIANT */
                    link_status = HDCP_STATUS_NONCOMPLIANT;
                }
            }

            /* If the status was compliant but is now unknown, restart
             * the timeout.
             */
            else if (link_status == HDCP_STATUS_COMPLIANT) {
                link_status = HDCP_STATUS_UNKNOWN;
                clock_gettime(CLOCK_MONOTONIC, &start);
            }

            nvfb_hdcp_set_status(hdcp, link_status);
        }
    }

quit:
    HDCP_TRACE("exiting hdcp_poll");
    nvfb_hdcp_set_status(hdcp, HDCP_STATUS_UNKNOWN);
    pthread_exit(0);

    return NULL;
}

static int hdcp_send(struct nvfb_hdcp *hdcp, enum hdcp_message message)
{
    uint8_t value = (uint8_t) message;

    /* Notify the hotplug thread to terminate */
    if (write(hdcp->pipe[1], &value, sizeof(value)) < 0) {
        ALOGE("hdcp thread: write failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

void nvfb_hdcp_enable(struct nvfb_hdcp *hdcp)
{
    if (hdcp) {
        if (!hdcp->thread) {
            if (pipe(hdcp->pipe) < 0) {
                ALOGE("pipe failed: %s", strerror(errno));
                return;
            }

            if (pthread_create(&hdcp->thread, NULL, hdcp_poll, hdcp) < 0) {
                ALOGE("Can't create hdcp thread: %s", strerror(errno));
                close(hdcp->pipe[0]);
                close(hdcp->pipe[1]);
                return;
            }
        }

        hdcp_send(hdcp, HDCP_ENABLE);
    }
}

void nvfb_hdcp_disable(struct nvfb_hdcp *hdcp)
{
    if (hdcp) {
        hdcp_send(hdcp, HDCP_DISABLE);
    }
}

int nvfb_hdcp_open(struct nvfb_hdcp **hdcp_ret, int deviceId)
{
    struct nvfb_hdcp *hdcp;
    HDCP_RET_ERROR err;

    hdcp = (struct nvfb_hdcp *) malloc(sizeof(struct nvfb_hdcp));
    if (!hdcp) {
        return -1;
    }

    memset(hdcp, 0, sizeof(struct nvfb_hdcp));

    /* Initialize HDCP state */
    err = hdcp_open(&hdcp->client, deviceId);
    if (err != HDCP_RET_SUCCESS) {
        ALOGE("HDCP: hdcp_open failed, error %d", err);
        goto fail;
    }

    *hdcp_ret = hdcp;
    return 0;

fail:
    nvfb_hdcp_close(hdcp);
    return -1;
}

void nvfb_hdcp_close(struct nvfb_hdcp *hdcp)
{
    if (hdcp) {
        if (hdcp->thread) {
            int status;

            hdcp_send(hdcp, HDCP_QUIT);
            status = pthread_join(hdcp->thread, NULL);
            if (status) {
                ALOGE("hdcp thread: thread_join: %d", status);
            }

            close(hdcp->pipe[0]);
            close(hdcp->pipe[1]);
        }

        if (hdcp->client) {
            hdcp_close(hdcp->client);
        }

        free(hdcp);
    }
}

#endif /* NV_HDCP */
