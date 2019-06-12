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
#include "nvassert.h"
#include "nvos.h"

#if NV_TIO_DUMP_CT_ENABLE || NV_TIO_DEBUG_ENABLE
#include "nvapputil.h"
#endif

//###########################################################################
//############################### CODE ######################################
//###########################################################################

#if NV_TIO_DUMP_CT_ENABLE || NV_TIO_DEBUG_ENABLE
//===========================================================================
// NvTioGotError()
//===========================================================================
NvError NvTioGotError(NvError err);
NvError NvTioGotError(NvError err)
{ 
    return err;
}
#endif

//===========================================================================
// NvTioShowError()
//===========================================================================
NvError NvTioShowError(
                    NvError err, 
                    const char *file, 
                    int line)
{
#if NV_TIO_DUMP_CT_ENABLE || NV_TIO_DEBUG_ENABLE
    if (err) {
#if !NV_TIO_DEBUG_SHOW_TIMEOUT
        if (err == NvError_Timeout)
            return err;
#endif
#if NV_TIO_DEBUG_ENABLE
        NvTioDebugf("Warning: NvE**or %08x (%s) at %s:%d\n",
            err, 
            NvAuErrorName(err), 
            file, 
            line);
#endif
#if NV_TIO_DUMP_CT_ENABLE
        if (NvTioGlob.enableDebugDump) {
            NvTioDebugDumpf("Warning: NvE**or %08x (%s) at %s:%d\n",
                err, 
                NvAuErrorName(err), 
                file, 
                line);
            NvTioDebugDumpf(0); // fflush()
        }
#endif
        NvTioGotError(err);
    }
#endif
    return err;
}

#if NV_TIO_DUMP_CT_ENABLE
//===========================================================================
// NvTioStreamDebugIsAscii() - print if >=minAsciiChars consecutive are printbl
//===========================================================================
static void NvTioStreamDebugIsAscii(
                    const char *start,
                    const char *end,
                    const char *s,
                    size_t len,
                    int minAsciiChars)
{
    int i;
    int n=0;
    int d=0;
    int ccnt=0;

    if (!minAsciiChars) 
        NvTioDebugDumpf("%s",start);

    for (i=0; i<(int)len; i++) {
        NvU32 c = (NvU8) s[i];
        n <<= 4;
        d++;
        if (c >= '0' && c<= '9') {
            n |= c - '0';
        } else if (c >= 'a' && c <= 'f') {
            n |= c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            n |= c - 'A' + 10;
        } else {
            n = 0;
            d = 0;
        }
        if (d==2) {
            if (n<' ' || n>= 0x7f)
                n = '.';
            if (!minAsciiChars) {
                NvTioDebugDumpf("  %c",n);
            } else if (n != '.') {
                ccnt++;
                if (ccnt >= minAsciiChars) {
                    NvTioStreamDebugIsAscii(start,end,s,len,0);
                    return;
                }
            } else {
                ccnt = 0;
            }
            n = 0;
            d = 0;
        } else if (!minAsciiChars) {
            NvTioDebugDumpf("   ");
        } else if (d==0) {
            ccnt = 0;
        }
    }
    if (!minAsciiChars) 
        NvTioDebugDumpf("%s",end);
}
#endif

#if NV_TIO_DUMP_CT_ENABLE
//===========================================================================
// NvTioStreamDebug() - dump io to file
//===========================================================================
void NvTioStreamDebug(
                    int isWrite,
                    int fd, 
                    const void *ptr,
                    size_t len)
{
    NvU32 i;
    const char *p;

    if (!NvTioGlob.enableDebugDump)
        return;

    p = ptr;
    if (isWrite < 0) {
        if (isWrite == -1) {
            NvTioDebugDumpf( " %02d = open(%s)\n",fd,p);
        } else if (isWrite == -2) {
            if (len) {
                NvTioDebugDumpf( " %02d poll readable  events=%08x\n",fd,len);
            }
        } else if (isWrite == -3) {
            if (len) {
                NvTioDebugDumpf( "    Error %08x (%s) at %s:%d\n",
                    fd,
                    NvAuErrorName(fd),
                    (const char*)ptr,
                    len);
            }
        } else if (isWrite == -4) {
            NvTioDebugDumpf( "DEBUG: %s\n", (const char*)ptr);
        }
    } else {
        NvTioDebugDumpf( "%s%02d:",
                isWrite?
                    (NvTioGlob.isHost?"--> W":"<-- W") :
                    (NvTioGlob.isHost?"<-- R":"--> R"),
                fd);

        if (len==0) {
            NvTioDebugDumpf(" End-Of-File\n");
        } else if ((int)len < 0) {
            NvTioDebugDumpf(" Error!!\n");
        } else {
            for (i=0; i<len; i++) {
                int c = p[i];
                if (c<=' ' || c>= 0x7f)
                    c='.';
                NvTioDebugDumpf("  %c",c);
            }

            NvTioDebugDumpf("\n        ");
            for (i=0; i<len; i++) {
                NvU32 c = (NvU8) p[i];
                NvTioDebugDumpf(" %02x",c);
            }
            NvTioDebugDumpf("\n");

            // if ascii then print chars too
            NvTioStreamDebugIsAscii(
                    "        ",
                    "\n",
                    p,
                    len,
                    len>3?2:1);
        }
    }

    NvTioDebugDumpf(0); // fflush()
}
#endif

//===========================================================================
// NvTioShowBytes()
//===========================================================================
void NvTioShowBytes(const NvU8 *ptr, NvU32 len)
{
    NvU32 i;
    NvTioDebugf("    chars: ");
    for (i=0; i<len; i++) {
        int c = ptr[i];
        if (c<=' ' || c>= 0x7f)
            c='.';
        NvTioDebugf("  %c",c);
    }
    NvTioDebugf("\n");

    NvTioDebugf("    hex:   ");
    for (i=0; i<len; i++) {
        int c = ptr[i];
        NvTioDebugf(" %02x",c);
    }
    NvTioDebugf("\n");
}

//===========================================================================
// NvTioRemoteSetStateDebug() - set state
//===========================================================================
void NvTioRemoteSetStateDebug(
                    NvTioRemote *remote, 
                    NvTioTargetState state,
                    const char *file,
                    int line)
{
#if NV_TIO_DUMP_CT_ENABLE
    if (NvTioGlob.enableDebugDump) {
        static char *stateStr[] = {
            "Disconnected",
            "Unknown",
            "Running",
            "Breakpoint",
            "BreakpointCmd",
            "FileWait",
            "FileWaitCmd",
            "RunningEOF",
            "Exit",
        };
        char buf[120];
        NvOsSnprintf(
            buf, 
            sizeof(buf), 
            "Switching from state %d (%s)  -->  %d (%s)  at %s:%d\n",
            remote->state,
            remote->state < NV_ARRAY_SIZE(stateStr) ? 
                    stateStr[remote->state] : "???",
            state,
            state < NV_ARRAY_SIZE(stateStr) ? 
                    stateStr[state] : "???",
            file,
            line);

        //DB(("%s\n",buf));
        DB_STR(buf);
    }
#endif
    remote->state = state;
}

//===========================================================================
// NvTioDisabledClose() - close disabled stream
//===========================================================================
static void NvTioDisabledClose(NvTioStreamHandle stream)
{
    stream->f.remote->oldStream = 0;
    stream->f.remote  = 0;
}

//===========================================================================
// NvTioRemoteConnect() - connect to remote machine
//===========================================================================
//
// Assumes remote has been zeroed by caller.
//
NvError NvTioRemoteConnect(
                    NvTioRemote *remote,
                    NvTioStream *connection, 
                    void        *buf,
                    NvU32        bufSize)
{
    static NvTioStreamOps disable_ops = {0};

    NV_ASSERT(connection->magic == NV_TIO_STREAM_MAGIC);
    NV_ASSERT(connection->ops   != &disable_ops);
    NV_ASSERT(buf);
    
    remote->oldStream = connection;
    remote->stream    = *connection;

    remote->buf    = buf;
    remote->bufLen = bufSize;
    remote->cmdLen = -1;
    remote->dst    = -1;

    NvTioRemoteSetState(remote, NvTioTargetState_Running);

    //
    // prevent connection from being used except for the connection
    //
    disable_ops.sopName  = "host-target connection (disabled for regular use)";
    disable_ops.sopClose = NvTioDisabledClose;
    connection->ops = &disable_ops;
    connection->f.remote = remote;

    return NvSuccess;
}

//===========================================================================
// NvTioRemoteDisconnect() - disconnect from remote machine
//===========================================================================
void NvTioRemoteDisconnect(NvTioRemote *remote)
{
    NV_ASSERT(remote->stream.magic == NV_TIO_STREAM_MAGIC);
    NV_ASSERT(remote->oldStream->magic == NV_TIO_STREAM_MAGIC);
    NV_ASSERT(remote->oldStream->f.remote == remote);
    
    //
    // terminate debug protocol
    //
    if (remote->terminate)
        remote->terminate(remote);

    // enable old stream
    remote->oldStream->ops  = remote->stream.ops;
    remote->oldStream->f.fp = remote->stream.f.fp;

    NvTioRemoteSetState(remote, NvTioTargetState_Disconnected);

    // disable remote stream
    remote->stream.magic = NV_TIO_FREE_MAGIC;
}

