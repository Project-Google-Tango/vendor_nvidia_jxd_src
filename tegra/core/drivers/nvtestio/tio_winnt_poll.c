/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "tio_local.h"
#include "tio_winnt.h"
#include "nvassert.h"
#include "nvos.h"

#if NVOS_IS_WINDOWS
#include <io.h>
#include <winsock2.h>
#endif

#if NVOS_IS_UNIX
#include <sys/poll.h>
#endif

//###########################################################################
//############################### CODE ######################################
//###########################################################################


//===========================================================================
// NvTioPoll() - Poll for streams which can be read without blocking
//===========================================================================
NvError NvTioPoll(NvTioPollDesc *tioDesc, NvU32 cnt, NvU32 timeout_msec)
{
    NvTioFdEvent osDesc[NV_TIO_POLL_MAX];
    int  map1[NV_TIO_POLL_MAX]; // i = map1[j]
    int  map2[NV_TIO_POLL_MAX]; // j = map2[i]
    HANDLE waitEvents[NV_TIO_POLL_MAX];
    NvError err = NvSuccess;
    NvTioStream *stream;
    int i;  // index into tioDesc[]
    int j;  // index into osDesc[] (and final count of osDesc)
    int k;  // index into osDesc[]
    NvU32 n;  // waited event index
    int timeout = (timeout_msec==NV_WAIT_INFINITE) ? INFINITE : (int)timeout_msec;

    NV_ASSERT(cnt < NV_TIO_POLL_MAX);

    //
    // Initial condition for checking streams with buffered data
    //
    NvOsMemset(map2, 0, sizeof(map1));
    osDesc[0].revents = 0;
    osDesc[0].fd = -1;

    for (;;) {
        //
        // Check for any streams that are ready to be read
        //
        // First time through loop:
        //    All entries in tioDesc[] map to osDesc[0] which has
        //    osDesc[0]==-1, so no reading will be performed.  
        //
        //
        // Second time through loop:
        //    Streams that are ready to read will have osDesc[j].revents set to
        //    POLLIN.
        //
        n = 0;
        for (i=0; i<(int)cnt; i++) {
            tioDesc[i].returnedEvents = 0;
            if (!(tioDesc[i].requestedEvents & NvTrnPollEvent_In))
                continue;
            stream = tioDesc[i].stream;
            if (!stream ||
                !stream->ops ||
                !stream->ops->sopPollPrep ||
                !stream->ops->sopPollCheck) {
                DB(("NvTioPoll() called with unsupported stream"));
                err = NvError_NotSupported;
                goto fail;
            }
            j = map2[i];
            if (j<0)
                continue;
            err = stream->ops->sopPollCheck(&tioDesc[i], &osDesc[j]);
            if (err)
                goto fail;

            // is it ready to read?
            if (tioDesc[i].returnedEvents & NvTrnPollEvent_In)
                n++;

            // this ensures the same file desc does not get read multiple times
            // per poll (which could block)
            osDesc[j].fd = -1;
            osDesc[j].revents = 0;
        }

        // are any streams readable?
        if (n)
            return NvSuccess;

        //
        // prepare to poll
        //
        for (j=i=0; i<(int)cnt; i++) {
            map2[i] = -1;
            if (!(tioDesc[i].requestedEvents & NvTrnPollEvent_In))
                continue;
            stream = tioDesc[i].stream;

#if 0
            if (!stream ||
                !stream->ops ||
                !stream->ops->sopPollPrep ||
                !stream->ops->sopPollCheck) {
                DB(("NvTioPoll() called with unsupported stream"));
                err = NvError_NotSupported;
                goto fail;
            }
#endif

            osDesc[j].fd        = -1;
            osDesc[j].events[0] = NULL;
            osDesc[j].revents   = 0;

            // fill in osDesc[j]
            err = stream->ops->sopPollPrep(&tioDesc[i], &osDesc[j]);
            if (err)
                goto fail;
            
            // only want 1 of each file descriptor in tioDesc[]
            for (k=0; k<j; k++) {
                if (osDesc[j].fd == osDesc[k].fd) {
                    map2[i] = map2[map1[k]];
                    osDesc[j].fd = -1;
                    break;
                }
            }

            // if no descriptor (or duplicate) then skip this one
            if (osDesc[j].fd == -1)
                continue;

            waitEvents[j] = osDesc[j].events[0];
            map1[j] = i;
            map2[i] = j;
            j++;
        }

        DBD(("WaitForMultipleObjects starts\n"));
        n = WaitForMultipleObjects(j, &waitEvents[0], FALSE, timeout);

        if (n-WAIT_OBJECT_0 >= (NvU32)j)
            break;

        DBD(("WaitForMultipleObjects returned object %d\n", n-WAIT_OBJECT_0));
        osDesc[n-WAIT_OBJECT_0].revents |= POLLIN;

        // loop to see if anything is ready to read
    }

    if (n==WAIT_TIMEOUT)
    {
        DBD(("WaitForMultipleObjects timed out\n"));
        err = NvError_Timeout;
    }
    else
    {
        // WAIT_FAILED, WAIT_ABANDONED, or other error
        DBD(("WaitForMultipleObjects failed\n"));
        err = NvError_FileOperationFailed;
    }

fail:
    return DBERR(err);
}
