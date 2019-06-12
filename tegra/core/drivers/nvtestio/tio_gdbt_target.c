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

#include "tio_gdbt.h"
#include "tio_local_target.h"
#include "tio_sdram.h"
#include "tio_shmoo.h"
#include "nvassert.h"
#include "nvutil.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

#define NV_TIO_MX_CMD_OVERHEAD      19

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

static NvU8           s_msgBuf[NV_TIO_TARGET_CONNECTION_BUFFER_SIZE];
static NvTioGdbtParms s_parms;

extern NvTioSdramData NvTioSdramDataFromHost;
extern NvTioBlitPatternData NvTioBlitPatternDataFromHost;

static NvTioStreamOps s_TioGdbtHostFileOps = {0};

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioGdbtProcessSdramParams() - handle a command that transfers sdram params
//===========================================================================
static NvError
NvTioGdbtProcessSdramParams(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32  NumBytes = 0;
    NvU32  i;
    NvU8  *src;
    NvU8  *dst;
    NvU8   byte;

    NumBytes = parms->parm[1].val;
    if (NumBytes > sizeof(NvTioSdramData))
        return NvError_BadParameter;

    src      = (NvU8*)parms->parm[2].str;
    dst      = (NvU8*)&NvTioSdramDataFromHost;
    for (i = 0; i < NumBytes; i++)
    {
        byte   = (NvU8)NvUStrtoul((const char*)src, (char **)&src, 16);
        *dst++ = byte;
        src++; /* Skip the trailing space. */
    }

    if ((NvTioSdramDataFromHost.Magic   == NVTIO_SDRAM_MAGIC) &&
        (NvTioSdramDataFromHost.Version <= NvTioSdramVersion_Current))
    {
        /* Return OK to indicate success */
        NvTioGdbtCmdString(cmd, "OK");
        return NvSuccess;
    }
    else
    {
        return NvError_BadParameter;
    }
}

//===========================================================================
// NvTioGdbtProcessBlitPatternParams()
// -handle a command that transfers blit pattern params
//===========================================================================
static NvError
NvTioGdbtProcessBlitPatternParams(NvTioGdbtParms *parms, NvTioGdbtCmd *cmd)
{
    NvU32  NumBytes = 0;
    NvU32  i;
    NvU8  *src;
    NvU8  *dst;
    NvU8   byte;

    NumBytes = parms->parm[1].val;

    src      = (NvU8*)parms->parm[2].str;
    dst      = (NvU8*)&NvTioBlitPatternDataFromHost;
    for (i = 0; i < NumBytes; i++)
    {
        if (i == 4) /* change dst to store pattern data */
        {
            dst = (NvU8*)NvOsAlloc(NvTioBlitPatternDataFromHost.Number * 4);
            if (dst == 0)
                return NvError_InsufficientMemory;
            NvTioBlitPatternDataFromHost.Params = (NvU32*)dst;
        }
        byte   = (NvU8)NvUStrtoul((const char*)src, (char **)&src, 16);
        *dst++ = byte;
        src++; /* Skip the trailing space. */
    }

    /* Return OK to indicate success */
    NvTioGdbtCmdString(cmd, "OK");
    return NvSuccess;
}

//===========================================================================
// NvTioGdbtHandleCommand() - handle a gdbtarget command
//===========================================================================
// NOTE: command starts at: &host->remote.buf[0]
//       command ends at:   &host->remote.buf[host->remote.cmdLen]
//       null terminated:    host->remote.buf[host->remote.cmdLen]==0
static NvError NvTioGdbtHandleCommand(NvTioHost *host)
{
    NvError err;
    NvError err2;
    NvTioGdbtCmd cmd;

    NV_ASSERT(host->remote.cmdLen >= 0);

#if NV_TIO_DEBUG_ENABLE
    NvTioDebugf("Got Command (len=%d):\n",host->remote.cmdLen);
    NvTioShowBytes((const NvU8*)host->remote.buf, host->remote.cmdLen);
#endif

    NvTioGdbtCmdBegin(&cmd, s_msgBuf, sizeof(s_msgBuf));

    err = NvTioGdbtCmdParse(&s_parms, host->remote.buf, host->remote.cmdLen);
    if (err)
        goto reply_err;

    switch(s_parms.cmd)
    {
        case 'm':
        {
            void *addr = (void*)s_parms.parm[0].val;
            size_t cnt = s_parms.parm[1].val;
            size_t maxcnt;
            if (s_parms.parmCnt != 2 ||
                s_parms.parm[0].isHex     == 0   ||
                s_parms.parm[0].separator != ',' ||
                s_parms.parm[1].isHex     == 0   ||
                s_parms.parm[1].separator != 0)
                goto reply_err;

            maxcnt = ((sizeof(s_msgBuf) - NV_TIO_GDBT_MSG_OVERHEAD) / 2) & ~3UL;
            if (cnt > maxcnt)
                cnt = maxcnt;

            NvTioGdbtCmdHexDataAligned(&cmd, addr, cnt);
            goto reply_send;
        }

        case 'M':
        {
            void *addr = (void*)s_parms.parm[0].val;
            char *src  = s_parms.parm[2].str;
            size_t cnt = s_parms.parm[1].val;
            if (s_parms.parmCnt != 3 ||
                s_parms.parm[0].isHex     == 0   ||
                s_parms.parm[0].separator != ',' ||
                s_parms.parm[1].isHex     == 0   ||
                s_parms.parm[1].separator != ':' ||
                s_parms.parm[2].isHex     == 0   ||
                s_parms.parm[2].separator != 0   ||
                s_parms.parm[2].len != cnt*2)
                goto reply_err;

            err = NvTioHexToMemAlignedDestructive(addr, src, cnt);
            if (err)
                goto reply_err;
            goto reply_ok;
        }

        case 'X':
        {
            void *addr = (void*)s_parms.parm[0].val;
            char *src  = s_parms.parm[2].str;
            size_t cnt = s_parms.parm[1].val;
            if (s_parms.parmCnt != 3 ||
                s_parms.parm[0].isHex     == 0   ||
                s_parms.parm[0].separator != ',' ||
                s_parms.parm[1].isHex     == 0   ||
                s_parms.parm[1].separator != ':' ||
                s_parms.parm[2].isHex     == 0   ||
                s_parms.parm[2].separator != 0) 
                goto reply_err;

            NvOsMemcpy(addr, src, cnt);
            goto reply_ok;
        }

        case '_':
        {
            if (!NvOsStrcmp("sdramparams", s_parms.parm[0].str))
            {
                err = NvTioGdbtProcessSdramParams(&s_parms, &cmd);
            }
            else if (!NvOsStrcmp("blitpatternparams", s_parms.parm[0].str))
            {
                err = NvTioGdbtProcessBlitPatternParams(&s_parms, &cmd);
            }
            else
            {
                err = NvTioShmooProcessCommand(&s_parms, &cmd);
            }
            if (err)
            {
                /* TODO: Distinguish between execution errors and unsupported
                 *       features.
                 */
                goto reply_err;
            }
            else
            {
                goto reply_send;

            }
        }

        default:
            goto reply_null;
    }

reply_null:
    //
    // bad or unknown command - send null response
    //
    goto reply_send;

reply_err:
    //
    // return "E00"
    //
    NvTioGdbtCmdString(&cmd, "E00");
    goto reply_send;

reply_ok:
    //
    // return "OK"
    //
    NvTioGdbtCmdString(&cmd, "OK");

reply_send:
    NvTioGdbtEraseCommand(&host->remote);
    (void)DBERR(err);
    err2 = NvTioGdbtCmdSend(&host->remote, &cmd);
    (void)DBERR(err2);
    return err ? err : err2;
}

//===========================================================================
// NvTioGdbtCloseHost()
//===========================================================================
static void NvTioGdbtCloseHost(NvTioHost *host)
{
    NvTioRemoteSetState(&host->remote, NvTioTargetState_RunningEOF);

    // TODO: close down host cleanly when EOF is hit
}

//===========================================================================
// NvTioGdbtFileCommand()
//===========================================================================
static NvError NvTioGdbtFileCommand(
            NvTioHost       *host,
            NvTioGdbtCmd    *cmd)
{
    NvError err;
    NvTioRemoteSetState(&host->remote, NvTioTargetState_FileWait);
    for (;;) {
        err = NvTioGdbtCmdWait(&host->remote, cmd, NV_WAIT_INFINITE);
        if (err) {
            // TODO: clean up host structure when hit EOF?
            NvTioRemoteSetState(&host->remote, NvTioTargetState_Running);
            if (err == NvError_EndOfFile) {
                NvTioGdbtCloseHost(host);
            }
            return DBERR(err);
        }
        cmd = 0;

        //
        // Is this the reply to the F command?
        //
        if (host->remote.buf[0] == 'F') {
            NvTioRemoteSetState(&host->remote, NvTioTargetState_Running);
            return NvSuccess;
        }

        //
        // No, some other command
        //
        NvTioRemoteSetState(&host->remote, NvTioTargetState_FileWaitCmd);
        err = NvTioGdbtHandleCommand(host);
        NvTioRemoteSetState(&host->remote, NvTioTargetState_FileWait);
        NV_ASSERT(host->remote.cmdLen == -1);
        if (err) {
            (void)DBERR(err);
            DB(("Error in HandleCommand() in NvTioGdbtFileCommand()\n"));
            // TODO: How should this error be handled?  For now it is ignored.
        }
    }
}

//===========================================================================
// NvTioBreakpoint() - breakpoint loop
//===========================================================================
void NvTioBreakpoint(void)
{
    // todo: what signal to use?
    NvU32 signal = NV_TI_GDBT_SIGNAL_TRAP;
    char rbuf[10];
    NvTioGdbtCmd  reply;
    NvTioGdbtCmd *cmd = &reply;
    NvError err;
    NvTioHost *host;

    err = NvTioGetHost(&host);
    if (err) {
        DB(("Could not connect to Host - exiting breakpoint"));
        return;
    }

    NV_ASSERT(host);
    NV_ASSERT(host->remote.cmdLen == -1);
    
    //
    // send S reply
    //
    NvTioGdbtCmdBegin(  cmd, rbuf, sizeof(rbuf));
    NvTioGdbtCmdString( cmd, "S");
    NvTioGdbtCmdHexU32( cmd, signal, 2);
    NvTioRemoteSetState(&host->remote, NvTioTargetState_Breakpoint);

    for (;;) {
        err = NvTioGdbtCmdWait(&host->remote, cmd, NV_WAIT_INFINITE);
        if (DBERR(err)) {
            if (err == NvError_EndOfFile) {
                DB(("Got End-of-file - exiting NvTioBreakpoint()\n"));
                NvTioGdbtCloseHost(host);
                return;
            }
            if (err == NvError_Timeout)
                cmd = 0;
            continue;
        }
        cmd = 0;

        // NOTE: command starts at: &host->remote.buf[0]
        //       command ends at:   &host->remote.buf[host->remote.cmdLen]
        //       null terminated:    host->remote.buf[host->remote.cmdLen]==0

        //
        // Is this a 'c' command (continue)?
        //
        if (host->remote.cmdLen == 1 && host->remote.buf[0] == 'c') {
            NvTioGdbtEraseCommand(&host->remote);
            NvTioRemoteSetState(&host->remote, NvTioTargetState_Running);
            return;
        }

        //
        // No, some other command
        //
        NvTioRemoteSetState(&host->remote, NvTioTargetState_BreakpointCmd);
        err = NvTioGdbtHandleCommand(host);
        NvTioRemoteSetState(&host->remote, NvTioTargetState_Breakpoint);
        NV_ASSERT(host->remote.cmdLen == -1);
        if (err) {
            (void)DBERR(err);
            DB(("Error in HandleCommand() in NvTioBreakpoint()\n"));
        }
    }
}

//===========================================================================
// NvTioTargetExit() - exit process
//===========================================================================
static void NvTioTargetExit(int status)
{
    NvError err;
    NvTioHost *host;
    char rbuf[10];
    NvTioGdbtCmd  reply;
    NvTioGdbtCmd *cmd = &reply;

    err = NvTioGetHost(&host);
    if (err) {
        (void)DBERR(err);
        DB(("Could not connect to Host - exiting breakpoint"));
        return;
    }

    NV_ASSERT(host);
    NV_ASSERT(host->remote.cmdLen == -1);

    if (status < 0 || status > 0xff)
        status = 0xff;

    //
    // send W reply
    //
    NvTioGdbtCmdBegin(  cmd, rbuf, sizeof(rbuf));
    NvTioGdbtCmdString( cmd, "W");
    NvTioGdbtCmdHexU32( cmd, status, 2);
    NvTioGdbtCmdSend(&host->remote, cmd);
    NvTioRemoteSetState(&host->remote, NvTioTargetState_Exit);

    //
    // disconnect
    //
    NvTioDisconnectFromHost(host);
}

//===========================================================================
// NvTioGdbtHostOpen() - open file on host
//===========================================================================
static NvError NvTioGdbtHostOpen(
            const char *path,
            NvU32 flags,
            NvTioStreamHandle stream )
{
    NV_ASSERT(!"Not implemented");
    return NvError_FileOperationFailed;
}

//===========================================================================
// NvTioGdbtHostFileOpen() - open file on host
//
// send a fopen command to host and wait for response
// once response is received, finish the stream creation
// and return.
//===========================================================================
static NvError NvTioGdbtHostFileOpen(
            const char *path,
            NvU32 flags,
            NvTioStreamHandle stream )
{
    NvTioHost    *host;
    const char   *stripedPath = (char *)path;
    NvError      err;
    NvTioGdbtCmd cmd;

    err = NvTioGetHost(&host);
    if (err)
        return DBERR(err);
    
    if (!NvOsStrncmp(path,"host:", 5)) // stripped path discriminator
        stripedPath = path + 5;
    NvTioGdbtCmdBegin(  &cmd, s_msgBuf, sizeof(s_msgBuf));
    NvTioGdbtCmdString( &cmd, "Fopen,");
    NvTioGdbtCmdString( &cmd, stripedPath);
    NvTioGdbtCmdString( &cmd, ",");
    NvTioGdbtCmdHexU32( &cmd, (NvU32)flags, 1);
    err = NvTioGdbtFileCommand(host, &cmd);
    if (!err) {
        char *reply  = host->remote.buf;
        size_t replySize = host->remote.cmdLen;
        NV_ASSERT(reply[0] == 'F');
        if (replySize<2 || reply[1]=='-') {
            err = NvError_FileOperationFailed;
        }
        else {
            // the number right after 'F' is file handle.
            NvU8 len = 1;
            while ((reply[len] != ',') && (reply[len] != '#'))
                len++;
            stream->f.fd = NvTioHexToU32(&reply[1], 0, len-1);
        }
        NvTioGdbtEraseCommand(&host->remote);
    }
    NV_ASSERT(host->remote.cmdLen == -1);
    return DBERR(err);
}

#if NV_TIO_GDBT_USE_O_REPLY
//===========================================================================
// NvTioGdbtHostWriteStdout() - write stream to host stdout
//===========================================================================
static NvError NvTioGdbtHostWriteStdout(
            NvTioStreamHandle stream,
            const void *ptr,
            size_t size,
            NvTioHost *host,
            NvTioGdbtCmd *cmd)
{
    NvError err = NvSuccess;
    size_t maxsz = sizeof(s_msgBuf)/2;

    if (maxsz > NV_TIO_TARGET_CONNECTION_BUFFER_SIZE/2-10)
        maxsz = NV_TIO_TARGET_CONNECTION_BUFFER_SIZE/2-10;
    maxsz -= NV_TIO_GDBT_MSG_OVERHEAD + 1;

    while (size) {
        size_t sz = size<maxsz ? size : maxsz;

        NvTioGdbtCmdBegin(   cmd, s_msgBuf, sizeof(s_msgBuf));
        NvTioGdbtCmdChar(    cmd, 'O');
        if (NV_TIO_GDBT_O_REPLY_HEX) {
            NvTioGdbtCmdHexData( cmd, ptr, sz);
        } else {
            NvTioGdbtCmdData(    cmd, ptr, sz);
        }
        NvTioRemoteSetState(&host->remote, NvTioTargetState_FileWait);
        err = NvTioGdbtCmdSend(&host->remote, cmd);
        NvTioRemoteSetState(&host->remote, NvTioTargetState_Running);
        if (err)
            return DBERR(err);
        size -= sz;
        ptr = (const char*)ptr + sz;
    }
    return DBERR(err);
}
#endif

//===========================================================================
// NvTioGdbtHostWrite() - write stream to host
//===========================================================================
static NvError NvTioGdbtHostWrite(
            NvTioStreamHandle stream,
            const void *ptr,
            size_t size )
{
    NvTioHost *host;
    NvError err = NvSuccess;
    NvTioGdbtCmd cmd;
    size_t maxsz;

    err = NvTioGetHost(&host);
    if (err)
        return DBERR(err);
    NV_ASSERT(stream->f.fd >=0);

#if NV_TIO_GDBT_USE_O_REPLY
    if (stream->f.fd == 1) {
        return NvTioGdbtHostWriteStdout(stream, ptr, size, host, &cmd);
    }
#endif

    maxsz = sizeof(s_msgBuf)/2-10;
    maxsz -= NV_TIO_GDBT_MSG_OVERHEAD + 1;


    while (size && !err) {
        size_t sz = size<maxsz ? size : maxsz;

        NvTioGdbtCmdBegin(       &cmd, s_msgBuf, sizeof(s_msgBuf));
        NvTioGdbtCmdString(      &cmd, "Fwrite,");
        NvTioGdbtCmdHexU32Comma( &cmd, (NvU32)stream->f.fd, 1);
        NvTioGdbtCmdHexU32Comma( &cmd, (NvU32)ptr,  1);
        NvTioGdbtCmdHexU32(      &cmd, (NvU32)sz,   1);
        err = NvTioGdbtFileCommand(host, &cmd);
        if (err) {
            NV_ASSERT(host->remote.cmdLen == -1);
            return DBERR(err);
        }

        //
        // NOTE: we now have a reply from the host
        //    reply   starts at: &host->remote.buf[0]
        //    reply   ends at:   &host->remote.buf[host->remote.cmdLen]
        //    null terminated:    host->remote.buf[host->remote.cmdLen]==0
        //
        NV_ASSERT(host->remote.buf[0] == 'F');
        if (host->remote.cmdLen<2 || host->remote.buf[1] == '-') {
            err = NvError_FileWriteFailed;
        }
        NvTioGdbtEraseCommand(&host->remote);   // erase the reply
        size -= sz;
        ptr = (const char*)ptr + sz;
    }
    NV_ASSERT(host->remote.cmdLen == -1);
    return DBERR(err);
}

//===========================================================================
// NvTioGdbtHostRead() - read stream from host
//===========================================================================
static NvError NvTioGdbtHostRead(
            NvTioStreamHandle stream,
            void *ptr,
            size_t size,
            size_t *bytes,
            NvU32 timeout_msec)
{
    NvTioHost *host;
    NvError err;
    NvTioGdbtCmd cmd;
    size_t maxsz;

    err = NvTioGetHost(&host);
    if (err)
        return DBERR(err);
    NV_ASSERT(stream->f.fd >=0);

    // TODO: support timeout_msec
    NV_ASSERT(timeout_msec == NV_WAIT_INFINITE);

    // TODO: find out a better way to enable "X" command
    if (stream->ops == &s_TioGdbtHostFileOps) {
        maxsz = sizeof(s_msgBuf) - NV_TIO_MX_CMD_OVERHEAD;
    } else {
        maxsz = sizeof(s_msgBuf)/2;
        maxsz -= NV_TIO_GDBT_MSG_OVERHEAD + 1;
    }
    if (size > maxsz)
        size = maxsz;

    NvTioGdbtCmdBegin(       &cmd, s_msgBuf, sizeof(s_msgBuf));
    NvTioGdbtCmdString(      &cmd, "Fread,");
    NvTioGdbtCmdHexU32Comma( &cmd, (NvU32)stream->f.fd,   1);
    NvTioGdbtCmdHexU32Comma( &cmd, (NvU32)ptr,  1);
    NvTioGdbtCmdHexU32(      &cmd, (NvU32)size, 1);
    err = NvTioGdbtFileCommand(host, &cmd);
    if (!err) {
        // NOTE: reply   starts at: &host->remote.buf[0]
        //       reply   ends at:   &host->remote.buf[host->remote.cmdLen]
        //       null terminated:    host->remote.buf[host->remote.cmdLen]==0
        char *reply      = host->remote.buf;
        size_t replySize = host->remote.cmdLen;
        char *e;
        size_t len;

        NV_ASSERT(reply[0] == 'F');
        if (replySize < 2 || reply[1] == '-') {
            err = NvError_FileReadFailed;
            if ((replySize >= 4) && (reply[4] == NV_TI_GDBT_ERRNO_EOF[0]))
                err = NvError_EndOfFile;
            goto done;
        }

        len = NvTioHexToU32(reply+1, &e, replySize-1);
        if (e == reply+1 || (*e != ',' && *e != 0)) {
            err = NvError_FileReadFailed;
            goto done;
        }
        *bytes = len;
done:
        NvTioGdbtEraseCommand(&host->remote);   // erase the reply
    }
    NV_ASSERT(host->remote.cmdLen == -1);
    return DBERR(err);
}

static NvError NvTioGdbtHostFileRead(
            NvTioStreamHandle stream,
            void *ptr,
            size_t size,
            size_t *bytes,
            NvU32 timeout_msec)
{
    NvError err;
    size_t  remain = size;
    size_t  cnt;
    const size_t  buf_size = sizeof(s_msgBuf) / 2 - NV_TIO_MX_CMD_OVERHEAD;

    size = 0;
    do
    {
        if (remain >= buf_size)
            cnt = buf_size;
        else
            cnt = remain;
        err = NvTioGdbtHostRead(stream, ptr, cnt, bytes, timeout_msec);

        if (!err) {
            remain -= *bytes;
            ptr = (void *)((size_t)ptr + (*bytes));
            if (*bytes < cnt) {
                // received data is less than requested, we are done
                size += *bytes;
                break;
            }
            size += *bytes;
        }
        else {
            if (err == NvError_EndOfFile) {
                if (!size)
                    return DBERR(err);
                else
                    break;
            }
            return DBERR(err);
        }
    }
    while (remain > 0);

    *bytes = size;
    return NvSuccess;
}

//===========================================================================
// NvTioGdbtHostClose()
//===========================================================================
static void NvTioGdbtHostClose(NvTioStreamHandle stream)
{
    NvTioHost    *host;
    NvError      err;
    NvTioGdbtCmd cmd;

    err = NvTioGetHost(&host);
    if (err)
        return;
    NV_ASSERT(stream->f.fd > 2); // stdin, stdout and stderr can't be closed

    NvTioGdbtCmdBegin(   &cmd, s_msgBuf, sizeof(s_msgBuf));
    NvTioGdbtCmdString(  &cmd, "Fclose,");
    NvTioGdbtCmdHexU32(  &cmd, (NvU32)stream->f.fd, 1);
    err = NvTioGdbtFileCommand(host, &cmd);

    NvTioGdbtEraseCommand(&host->remote);
}

//===========================================================================
// NvTioGdbtTerminateHost() - shut down gdbtarget protocol
//===========================================================================
static void NvTioGdbtTerminateHost(NvTioRemote *remote)
{
    NvTioHost *host = (NvTioHost*)remote;
    int i;

    host->gdbtStream[NvTioHostStream_stdin].magic  = NV_TIO_FREE_MAGIC;
    host->gdbtStream[NvTioHostStream_stdout].magic = NV_TIO_FREE_MAGIC;
    host->gdbtStream[NvTioHostStream_stderr].magic = NV_TIO_FREE_MAGIC;
    host->gdbtStream[NvTioHostStream_stdin].ops    = 0;
    host->gdbtStream[NvTioHostStream_stdout].ops   = 0;
    host->gdbtStream[NvTioHostStream_stderr].ops   = 0;

    //
    // remove gdbtarget streams from list of streams used for stdio
    //
    for (i=0; i<NvTioHostStream_Count; i++) {
        NvU32 j, k;
        NvTioMultiStream *ms = &host->stream[i];
        for (j=k=0; j<NV_ARRAY_SIZE(ms->streams); j++) {
            if (ms->streams[j] != &host->gdbtStream[i])
                ms->streams[k++] = ms->streams[j];
        }
        ms->streamCnt = k;
    }

}

//===========================================================================
// NvTioGdbtInitHost()
//===========================================================================
NvError NvTioGdbtInitHost(NvTioHost *host)
{
    int i;
    static NvTioStreamOps read_ops  = {0};
    static NvTioStreamOps write_ops = {0};

    NV_ASSERT(!host->gdbtStream[NvTioHostStream_stdin].ops);
    NV_ASSERT(!host->gdbtStream[NvTioHostStream_stdout].ops);
    NV_ASSERT(!host->gdbtStream[NvTioHostStream_stderr].ops);

    NvTioGlob.exitFunc = NvTioTargetExit;

    read_ops.sopName    = "GdbtHostReadOps";
    read_ops.sopFopen   = NvTioGdbtHostOpen;
    read_ops.sopFread   = NvTioGdbtHostRead;
    read_ops.sopClose   = NvTioGdbtHostClose;

    write_ops.sopName   = "GdbtHostWriteOps";
    write_ops.sopFopen  = NvTioGdbtHostOpen;
    write_ops.sopFwrite = NvTioGdbtHostWrite;
    write_ops.sopClose  = NvTioGdbtHostClose;

    host->remote.terminate = NvTioGdbtTerminateHost;

    host->gdbtStream[NvTioHostStream_stdin].magic  = NV_TIO_STREAM_MAGIC;
    host->gdbtStream[NvTioHostStream_stdin].ops    = &read_ops;
    host->gdbtStream[NvTioHostStream_stdin].f.fd   = 0;

    host->gdbtStream[NvTioHostStream_stdout].magic = NV_TIO_STREAM_MAGIC;
    host->gdbtStream[NvTioHostStream_stdout].ops   = &write_ops;
    host->gdbtStream[NvTioHostStream_stdout].f.fd  = 1;

    host->gdbtStream[NvTioHostStream_stderr].magic = NV_TIO_STREAM_MAGIC;
    host->gdbtStream[NvTioHostStream_stderr].ops   = &write_ops;
    host->gdbtStream[NvTioHostStream_stderr].f.fd  = 2;

    //
    // add gdbtarget streams to list of streams used for stdio
    //
    for (i=0; i<NvTioHostStream_Count; i++) {
        int j;
        NvTioMultiStream *ms = &host->stream[i];
        for (j=NV_ARRAY_SIZE(ms->streams)-1; j>0; j--) {
            ms->streams[j] = ms->streams[j-1];
        }
        ms->streams[0] = &host->gdbtStream[i];
        ms->streamCnt++;
        NV_ASSERT(ms->streamCnt <= NV_ARRAY_SIZE(ms->streams));
        if (ms->streamCnt > NV_ARRAY_SIZE(ms->streams))
            ms->streamCnt = NV_ARRAY_SIZE(ms->streams);
    }

    return NvSuccess;
}

static NvError NvTioGdbtFileCheckPath(const char *path)
{
    char *p = (char *)path;
    if (NvOsStrlen(path) < 6)
        return NvError_BadParameter;
    if (!NvOsStrncmp(path, "host:", 5))
        return NvSuccess;
    for (;*p;p++)
    {
        if (*p == ':')
            return (NvError_BadValue);
    }

    return NvSuccess;
}

static NvError NvTioGdbtHostFtell(NvTioStreamHandle stream, NvU64 *position)
{
    NvTioHost    *host;
    NvError      err;
    NvTioGdbtCmd cmd;

    err = NvTioGetHost(&host);
    if (err)
        return DBERR(err);

    NvTioGdbtCmdBegin(  &cmd, s_msgBuf, sizeof(s_msgBuf));
    NvTioGdbtCmdString( &cmd, "Ftell,");
    NvTioGdbtCmdHexU32( &cmd, stream->f.fd, 1 );
    err = NvTioGdbtFileCommand( host, &cmd );
    if (!err) {
        NV_ASSERT(host->remote.buf[0] == 'F');
        if (host->remote.buf[1] == '-')
            err = NvError_FileOperationFailed;
        else {
            NvU32 pos;
            err = NvTioGdbtCmdParse(&s_parms, 
                                     host->remote.buf, 
                                     host->remote.cmdLen);
            if (err || 
                (s_parms.parm[1].isHex == 0) || 
                (s_parms.parm[2].isHex == 0))
                goto reply_err;
            pos = s_parms.parm[1].val;
            *position = ((NvU64)pos << 32) + s_parms.parm[2].val;
        }
    }

reply_err:
    NvTioGdbtEraseCommand(&host->remote);

    return DBERR(err);
}

static NvError NvTioGdbtHostFseek(NvTioStreamHandle stream, NvS64 offset, NvU32 whence)
{
    NvTioHost    *host;
    NvError      err;
    NvTioGdbtCmd cmd;

    err = NvTioGetHost(&host);
    if (err)
        return DBERR(err);

    NvTioGdbtCmdBegin(  &cmd, s_msgBuf, sizeof(s_msgBuf));
    NvTioGdbtCmdString( &cmd, "Flseek,");
    NvTioGdbtCmdHexU32Comma( &cmd, stream->f.fd, 1 );
    NvTioGdbtCmdHexU32Comma( &cmd, (NvU32)offset, 1 );
    NvTioGdbtCmdHexU32Comma( &cmd, (NvU32)(offset>>32), 1 );
    NvTioGdbtCmdHexU32( &cmd, whence, 1 );
    err = NvTioGdbtFileCommand( host, &cmd );
    if (!err) {
        NV_ASSERT(host->remote.buf[0] == 'F');
        if (host->remote.buf[1] == '-')
            err = NvError_FileOperationFailed;
    }
    NvTioGdbtEraseCommand(&host->remote);

    return DBERR(err);
}

void NvTioRegisterFile()
{
    s_TioGdbtHostFileOps.sopName      = "GdbtHostFileOps";
    s_TioGdbtHostFileOps.sopFopen     = NvTioGdbtHostFileOpen;
    s_TioGdbtHostFileOps.sopFwrite    = NvTioGdbtHostWrite;
    s_TioGdbtHostFileOps.sopFread     = NvTioGdbtHostFileRead;
    s_TioGdbtHostFileOps.sopClose     = NvTioGdbtHostClose;
    s_TioGdbtHostFileOps.sopFseek     = NvTioGdbtHostFseek;
    s_TioGdbtHostFileOps.sopFtell     = NvTioGdbtHostFtell;
    s_TioGdbtHostFileOps.sopCheckPath = NvTioGdbtFileCheckPath;

    NvTioRegisterOps(&s_TioGdbtHostFileOps);
}

static NvError
NvTioFileHostFopen(const char *path, NvU32 flags, NvOsFileHandle *file)
{
    NvError e = NvSuccess;
    e = NvTioFopen(path, flags, (NvTioStreamHandle *)file);
    return e;
}

static void
NvTioFileHostClose(NvOsFileHandle stream)
{
    NvTioClose((NvTioStreamHandle)stream);
}

static NvError
NvTioFileHostFwrite(NvOsFileHandle stream, const void *ptr, size_t size)
{
    NvError e = NvSuccess;
    e = NvTioFwrite((NvTioStreamHandle)stream, ptr, size);
    return e;
}

static NvError
NvTioFileHostFread(
        NvOsFileHandle stream, 
        void *ptr, 
        size_t size, 
        size_t *bytes,
        NvU32 timeout_msec)
{
    NvError e = NvSuccess;
    e = NvTioFread((NvTioStreamHandle)stream, ptr, size, bytes);
    return e;
}

static NvError
NvTioFileHostFseek(NvOsFileHandle file, NvS64 offset, NvOsSeekEnum whence)
{
    NvError e = NvSuccess;
    e = NvTioFseek((NvTioStreamHandle)file, offset, whence);
    return e;
}

static NvError
NvTioFileHostFtell(NvOsFileHandle file, NvU64 *position)
{
    NvError e = NvSuccess;
    e = NvTioFtell((NvTioStreamHandle)file, position);
    return e;
}

static NvError
NvTioFileHostFstat(NvOsFileHandle file, NvOsStatType *s)
{
    NvError e = NvSuccess;
    e = NvTioFstat((NvTioStreamHandle)file, s);
    return e;
}

static NvError
NvTioFileHostStat(const char *filename, NvOsStatType *stat)
{
    NvError e = NvSuccess;
    return e;
}

static NvError
NvTioFileHostFflush(NvOsFileHandle stream)
{
    NvError e = NvSuccess;
    return e;
}

static NvError
NvTioFileHostFsync(NvOsFileHandle stream)
{
    NvError e = NvSuccess;
    return e;
}

static NvError
NvTioFileHostFlock(NvOsFileHandle stream, NvOsFlockType type)
{
    return NvError_NotSupported;
}

static NvError
NvTioFileHostFtruncate(NvOsFileHandle dir, NvU64  size)
{
    return NvError_NotSupported;
}

static NvError
NvTioFileHostOpendir(const char *path, NvOsDirHandle *dir)
{
    NvError e = NvSuccess;
    return e;
}

static NvError
NvTioFileHostReaddir(NvOsDirHandle dir, char *name, size_t size)
{
    NvError e = NvSuccess;
    return e;
}

static void
NvTioFileHostClosedir(NvOsDirHandle dir)
{
}

static NvError
NvTioFileHostMkdir(char *dirname)
{
    return NvError_NotSupported;
}

void
NvTioSetFileHostHooks(void)
{
    static NvOsFileHooks hostHooks =
    {
        NvTioFileHostFopen,
        NvTioFileHostClose,
        NvTioFileHostFwrite,
        NvTioFileHostFread,
        NvTioFileHostFseek,
        NvTioFileHostFtell,
        NvTioFileHostFstat,
        NvTioFileHostStat,
        NvTioFileHostFflush,
        NvTioFileHostFsync,
        NULL,
        NvTioFileHostFlock,
        NvTioFileHostFtruncate,
        NvTioFileHostOpendir,
        NvTioFileHostReaddir,
        NvTioFileHostClosedir,
        NvTioFileHostMkdir,
    };
    NvOsSetFileHooks(&hostHooks);
}
