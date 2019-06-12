/*
 * Copyright (c) 2007-2012 NVIDIA Corporation.  All Rights Reserved.
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

#include "tio_local_host.h"
#include "tio_gdbt.h"
#include "nvassert.h"
#include "nvos.h"

//###########################################################################
//############################### NOTES #####################################
//###########################################################################

/*
 * Notes on tio_gdbt_host.c:
 *
 * This file contains the host side of the nvtestio communication with the
 * target using the gdb remote debugging protocol.
 *
 * At any point, the system is in one of the following states:
 * - The target is stopped (in a breakpoint).  The flow of commands is from
 *   the host to the target, and replies are returned by the target to the
 *   host.
 * - The target is running.  The flow of commands is from the target to the
 *   host, and replies are returned by the host to the target.
 * - In transition between these states.
 * - The target hasn't started.
 * - The target has exited.
 *
 * Because these states affect the intepretation of messages, this code
 * tracks the state of the target.  When the target is stopped, all commands
 * originate with the calling program (for example, nvhost), so all responses
 * from the target are sent back to the calling program, minus the gdb
 * remote debugging protocol overhead.  When the target is running, all
 * commands are requests from the target for services on the host, such as
 * file I/O.  These are transparent to a program like nvhost, so the commands
 * are processed within this layer.  Any such commands which cause transitions
 * between the states mentioned above are also made visible to the calling
 * program.
 *
 * What this boils down to is that the code for processing messages will
 * update the data tracking the state of the target, completely handle
 * some messages internally, and propagate the text of other messages back
 * to the calling program.
 *
 * The state machine that tracks the target's state and guides the behavior
 * of the gdbt_host code looks like the following:
 *
 * [TODO: Turn this into a proper dot description for the benefit of
 *        doxygen runs.]
 * [TODO: Fill in the missing pieces.]
 *
 * Unknown -> ?? [??]
 * Running -> Breakpoint [target sends Sxx]
 * Running -> Running [target sends O...]
 * Running -> Exiting [target sends W]
 * Running -> ?? [target sends F]
 * Breakpoint -> BreakpointCmd [host sends non-c to target]
 * Breakpoint -> Running [host sends c]
 * BreakpointCmd -> Breakpoint [target sends response]
 * Exiting -> ?? [ ?? ]
 * FileWait -> ?? [ ?? ]
 * FileWaitCmd -> ?? [ ?? ]
 * FunningEOF -> ?? [ ?? ]
 *
 *
 * A comparison of this state machine with that contained within nvhost shows
 * that the latter doesn't see state transitions in which this machine handles
 * service requests from the target (like the F command).
 *
 * Remaining tasks:
 * - Clean up what's here.
 * - Determine if the two sets of response handlers can be merged into one
 *   (taking advantage of better state tracking).
 */

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

#define NV_TIO_MAX_COMMAND  (NV_TIO_HOST_CONNECTION_BUFFER_SIZE/2)

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioGdbtHandleFileOpenRequest
//
// when target sent a command "$fopen,<file_path>,<flags>", host needs to 
// open a file in a new stream and return the file descriptor to target.
//
// arguments:
//
// return:
//   NvS32 ==> file descriptor to be returned to target.
//===========================================================================
static NvS32 NvTioGdbtHandleFileOpenRequest(NvTioTarget *target, NvOsFileHandle fp)
{
    NvU32 i;

    for (i=NvTioTargetStream_reply+1; i<NvTioTargetStream_Count; i++)
    {
        if (!target->buf[i].stream)
            break;
    }
    if (i == NvTioTargetStream_Count)
        return -1;


    target->buf[i].stream = NvOsAlloc(sizeof(NvTioStream));
    if (!target->buf[i].stream) {
        return -1;
    }
    target->buf[i].stream->magic = NV_TIO_STREAM_MAGIC;
    target->buf[i].stream->f.fp = fp;

    return i;
}

//===========================================================================
// NvTioGdbtHandleReadReply() - handle a Fread reply to the target
//===========================================================================
// NOTE: command starts at: &target->remote.buf[0]
//       command ends at:   &target->remote.buf[target->remote.cmdLen]
//       null terminated:    target->remote.buf[target->remote.cmdLen]==0
static NvError NvTioGdbtHandleReadReply(NvU32 isBin, NvTioTarget *target)
{
    char *fErrno = NV_TI_GDBT_ERRNO_EUNKNOWN;
    NvError err;
    NvError err2;
    NvU32 cnt;
    NvTioStreamBuffer *buf = target->readStream;
    static NvU8 cbuf[NV_TIO_MAX_COMMAND];
    NvTioGdbtCmd cmd;
    NvU32 maxcnt;
    if (isBin)
        maxcnt = (sizeof(cbuf) - 19)  & ~3UL; // TODO: is 19 the sufficient overhead?
    else
        maxcnt = ((sizeof(cbuf) - 4) / 2) & ~3UL;

    NV_ASSERT(buf);
    NV_ASSERT(buf->send.bufCnt);
    NV_ASSERT(target->readLen > 0);
    NV_ASSERT(target->remote.state == NvTioTargetState_FileWait);
    NV_ASSERT(target->remote.cmdLen == -1);

    DB(("TODO: Determine what to do with message %s in state %d in NvTioHandleReadReply()!!\n",
        target->remote.buf, target->remote.state));

    cnt = buf->send.bufCnt;
    if (cnt > target->readLen)
        cnt = target->readLen;
    if (cnt > maxcnt)
        cnt = maxcnt;

    target->readStream = 0;

    NvTioGdbtCmdBegin(       &cmd, cbuf, sizeof(cbuf));
    if (isBin)
        NvTioGdbtCmdString(      &cmd, "X");
    else
        NvTioGdbtCmdString(      &cmd, "M");
    NvTioGdbtCmdHexU32Comma( &cmd, target->readAddr, 1);
    NvTioGdbtCmdHexU32(      &cmd, cnt,  1);
    NvTioGdbtCmdChar(        &cmd, ':');
    if (isBin) {
        NvTioGdbtCmdData(&cmd, buf->send.buf, cnt);
    }
    else
        NvTioGdbtCmdHexData(     &cmd, buf->send.buf, cnt);
    NvTioBufShrink(&buf->send, cnt);
    NvTioRemoteSetState(&target->remote, NvTioTargetState_FileWaitCmd);
    err = NvTioGdbtCmdWait(&target->remote, &cmd, NV_WAIT_INFINITE);
    (void)DBERR(err);
    NvTioRemoteSetState(&target->remote, NvTioTargetState_FileWait);

    //
    // return file reply
    //
    NvTioGdbtCmdBegin(&cmd, cbuf, sizeof(cbuf));
    NvTioGdbtCmdChar(&cmd, 'F');

    if (!err &&
        target->remote.cmdLen == 2 &&
        target->remote.buf[0] == 'O' &&
        target->remote.buf[1] == 'K') {
        NvTioGdbtCmdHexU32(&cmd, cnt, 1);
    } else {
        NvTioGdbtCmdString(&cmd, "-1,");
        NvTioGdbtCmdString(&cmd, fErrno);
    }

    // discard the OK reply
    NvTioGdbtEraseCommand(&target->remote);

    NvTioRemoteSetState(&target->remote, NvTioTargetState_Running);
    err2 = NvTioGdbtCmdSend(&target->remote, &cmd);
    (void)DBERR(err2);

    return err ? err : err2;
}

static NvError NvTioGdbtHandleOpenReply(NvTioTarget *target, NvU32 fd)
{
    NvTioGdbtCmd       cmd;
    NvError            err;
    NvU8               cbuf[40];

    // prepare "open" command reply
    NvTioGdbtCmdBegin      (&cmd, cbuf, sizeof(cbuf));
    NvTioGdbtCmdString     (&cmd, "F");
    if (fd != 0xFFFFFFFF)
        NvTioGdbtCmdHexU32Comma(&cmd, fd, 1);
    NvTioRemoteSetState(
        &target->remote,
        NvTioTargetState_FileWait);
    err = NvTioGdbtCmdSend(&target->remote,&cmd);
    NvTioRemoteSetState(&target->remote,NvTioTargetState_Running);
    if (fd == 0xFFFFFFFF)
        err = NvError_FileOperationFailed;
    if (DBERR(err))
        goto done;
    return NvSuccess;

done:
    if (target->remote.cmdLen >= 0)
        NvTioGdbtEraseCommand(&target->remote);
    return DBERR(err);
}

/*
 * NvTioGdbtPropagateReply()
 *
 * Propagate the reply text to the higher-level code.
 * All state changes and command-specific error checking should happen
 * elsewhere - this is a common utility function for the command processing
 * code.
 */
static NvError
NvTioGdbtPropagateReply(NvTioTarget *target)
{
    NvError            e   = NvSuccess;
    NvTioStreamBuffer *buf = &target->buf[NvTioTargetStream_reply];
    NvU8              *dst;

    //
    // Already have a reply pending?
    //
#define NV_TIO_GDBT_ONLY_1_REPLY 1
#if NV_TIO_GDBT_ONLY_1_REPLY
    if (buf->recv.bufCnt)
    {
        int i;
        NvU32 j;
        NvTioDebugf("Ignoring extra reply: ");
        for (i=0; i<target->remote.cmdLen; i++)
        {
            int c = target->remote.buf[i];
            NvTioDebugf("%c",c<' '||c>=0x7f?'.':c);
        }
        NvTioDebugf("Existing reply:       ");
        for (j=0; j<buf->recv.bufCnt; j++)
        {
            int c = buf->recv.buf[j];
            NvTioDebugf("%c",c<' '||c>=0x7f?'.':c);
        }
    }
    else
#endif
    {
        //
        // add reply to reply buffer
        //
        e = NvTioBufAlloc(&buf->recv, target->remote.cmdLen, &dst);
        if (e) {
            DB(("Buffer overflow in reply buffer()!!\n"));
            return DBERR(e);
        } else {
            // TODO: make it so each reply is returned separately
            NvOsMemcpy(dst, target->remote.buf, target->remote.cmdLen);
            NvTioBufCommit(&buf->recv, target->remote.cmdLen, dst);
        }
    }

    if (target->remote.cmdLen >= 0)
        NvTioGdbtEraseCommand(&target->remote);

    return DBERR(e);
}

/* Handle an 'F' request from the target */
static NvError
NvTioGdbtHandleFileRequest(NvTioTarget *target)
{
    NvError err = NvSuccess;
    char *fErrno = NV_TI_GDBT_ERRNO_EUNKNOWN;
    NvU8 cbuf[40];
    NvTioGdbtCmd cmd;
    NvTioGdbtParms parms;
    NvTioStreamBuffer *buf;
    int rv = -1;
    NvTioTargetState oldState = target->remote.state;

    NV_ASSERT(target->remote.cmdLen >= 0);

    NvTioRemoteSetState(&target->remote, NvTioTargetState_FileWait);
    err = NvTioGdbtCmdParse(
        &parms, 
        target->remote.buf, 
        target->remote.cmdLen);
    if (err)
        goto reply_file;
    if (parms.parmCnt < 1)
        goto reply_file;

    if (!NvTioOsStrcmp(parms.parm[0].str, "write")) {
        int fd     = parms.parm[1].val;
        NvU32 addr = parms.parm[2].val;
        size_t cntTotal = parms.parm[3].val;
        size_t cntLeft;
        size_t cntChunk;
        NvU8 *dst;
        NvU8 *dstChunk;
        const size_t maxChunk = ((NV_TIO_MAX_COMMAND/2) - 2) & ~3UL;

        if (parms.parmCnt != 4 ||
            parms.parm[0].separator != ',' ||
            parms.parm[1].isHex     == 0   ||
            parms.parm[1].separator != ',' ||
            parms.parm[2].isHex     == 0   ||
            parms.parm[2].separator != ',' ||
            parms.parm[3].isHex     == 0   ||
            parms.parm[3].separator != 0)
            goto reply_file;

        // TODO: support other file descriptors
        if (fd == 1) {
            buf = &target->buf[NvTioTargetStream_stdout];
        } else if (fd == 2) {
            buf = &target->buf[NvTioTargetStream_stderr];
        } else if ((fd > NvTioTargetStream_reply) && 
                  (fd < NvTioTargetStream_Count)) {
            buf = &target->buf[fd];
        } else {
            fErrno = NV_TI_GDBT_ERRNO_EBADF;
            goto reply_file;
        }

        err = NvTioBufAlloc(&buf->recv, cntTotal, &dst);
        if (err)
            goto reply_file;

        // DO NOT USE parms AFTER THIS POINT
        // (they are destroyed by NvTioGdbtEraseCommand() function)

        cntLeft = cntTotal;
        dstChunk = dst;
        while (cntLeft) {

            NvTioGdbtEraseCommand(&target->remote);

            cntChunk = cntLeft < maxChunk ? cntLeft : maxChunk;

            NvTioGdbtCmdBegin(       &cmd, cbuf, sizeof(cbuf));
            NvTioGdbtCmdString(      &cmd, "m");
            NvTioGdbtCmdHexU32Comma( &cmd, addr, 1);
            NvTioGdbtCmdHexU32(      &cmd, cntChunk,  1);
            NvTioRemoteSetState(
                &target->remote, 
                NvTioTargetState_FileWaitCmd);
            err = NvTioGdbtCmdWait(&target->remote, &cmd, NV_WAIT_INFINITE);
            NvTioRemoteSetState(&target->remote, NvTioTargetState_FileWait);
            if (DBERR(err))
                goto reply_file;

            if (target->remote.buf[0] == 'E' || 
                target->remote.cmdLen != (int)cntChunk*2)
                goto reply_file;

            err = NvTioHexToMem(dstChunk, target->remote.buf, cntChunk);
            if (DBERR(err))
                goto reply_file;

            cntLeft  -= cntChunk;
            dstChunk += cntChunk;
            addr     += cntChunk;
        }

        NvTioBufCommit(&buf->recv, cntTotal, dst);

        if (fd > NvTioTargetStream_reply) {
            // if fd is larger than ..._Count, it would have exited already.
            // if fd is less than ..._reply, it should fall through
            err = NvOsFwrite(buf->stream->f.fp,buf->recv.buf,buf->recv.bufCnt);
            buf->recv.bufCnt = 0;
            if (err)
                fErrno = NV_TI_GDBT_ERRNO_EBADF;
        }
        //
        // send the F reply
        //
        rv = cntTotal;
        goto reply_file;
    }

    if (!NvTioOsStrcmp(parms.parm[0].str, "read")) {
        int fd     = parms.parm[1].val;
        NvU32 addr = parms.parm[2].val;
        size_t cnt = parms.parm[3].val;
        NvU32 isBin = 0;

        if (parms.parmCnt != 4 ||
            parms.parm[0].separator != ',' ||
            parms.parm[1].isHex     == 0   ||
            parms.parm[1].separator != ',' ||
            parms.parm[2].isHex     == 0   ||
            parms.parm[2].separator != ',' ||
            parms.parm[3].isHex     == 0   ||
            parms.parm[3].separator != 0)
            goto reply_file;

        // TODO: support other file descriptors
        if (fd == 0) {
            buf = &target->buf[NvTioTargetStream_stdin];
        } else {
            NvU8 *dst;
            isBin = 1; // binary transfer to save bandwidth
            if ((fd >= NvTioTargetStream_Count) || 
                (!target->buf[fd].stream) ||
                (target->buf[fd].stream->f.fp == (void *)-1)) {
                fErrno = NV_TI_GDBT_ERRNO_EOF;
                err = NvError_FileOperationFailed;
                goto reply_file;
            }
            buf = &target->buf[fd];
            err = NvTioBufAlloc(&buf->send, cnt, &dst);
            if (err) {
                fErrno = NV_TI_GDBT_ERRNO_EBADF;
                goto reply_file;
            }
            err = NvOsFread(buf->stream->f.fp, buf->send.buf, cnt, &cnt);
            if (err) {
                fErrno = NV_TI_GDBT_ERRNO_EBADF;
                if (err == NvError_EndOfFile)
                    fErrno = NV_TI_GDBT_ERRNO_EOF;
                goto reply_file;
            }
            NvTioBufCommit(&buf->send, cnt, dst);
        }

        NV_ASSERT(!target->readStream);
        target->readStream = buf;
        target->readAddr   = addr;
        target->readLen    = cnt;

        NvTioGdbtEraseCommand(&target->remote);
        if (buf->send.bufCnt)
            return NvTioGdbtHandleReadReply(isBin, target);
        return NvSuccess;
    }

    if (!NvTioOsStrcmp(parms.parm[0].str,"open")) {
        /* syntax: open,pathname,flags. mode is not supported here. */
        NvOsFileHandle  fp;
        NvU32           fd;
        err = NvOsFopen(parms.parm[1].str, parms.parm[2].val, &fp);

        NvTioGdbtEraseCommand(&target->remote);
        // create a new stream 
        if (!err) {
            fd = NvTioGdbtHandleFileOpenRequest(target,fp);
            //fd = fp->fd;
        }
        else
            fd = -1;
        // send response (file handle) back to target
        return NvTioGdbtHandleOpenReply(target,fd);
    }
        
    if (!NvTioOsStrcmp(parms.parm[0].str,"close")) {
        int fd = parms.parm[1].val;

        if ((fd <= NvTioTargetStream_reply) ||
            (fd >= NvTioTargetStream_Count) ||
            (!target->buf[fd].stream))
        {
            err = NvError_FileOperationFailed;
            goto reply_file;
        }
                // close file
        NvOsFclose((NvOsFileHandle)target->buf[fd].stream->f.fp);
        NvOsFree(target->buf[fd].stream);
        target->buf[fd].stream = 0;
        target->buf[fd].send.bufCnt = target->buf[fd].recv.bufCnt = 0;
    }

    if (!NvTioOsStrcmp(parms.parm[0].str,"tell")) {
        int     fd = parms.parm[1].val;
        NvU64   pos;
        NvError err;
        if ((fd <= NvTioTargetStream_reply) ||
            (fd >= NvTioTargetStream_Count) ||
            (!target->buf[fd].stream))
        {
            err = NvError_FileOperationFailed;
        }
        else
        {
            err = NvOsFtell(target->buf[fd].stream->f.fp, &pos);
            NvTioGdbtEraseCommand(&target->remote);
            if (!err) {
                rv = 0;
                NvTioGdbtCmdBegin( &cmd,cbuf,sizeof(cbuf));
                NvTioGdbtCmdString(&cmd,"F");
                NvTioGdbtCmdHexU32Comma(&cmd, fd, 1);
                NvTioGdbtCmdHexU32Comma(&cmd, (NvU32)(pos >> 32), 1);
                NvTioGdbtCmdHexU32Comma(&cmd, (NvU32)pos, 1);
                NvTioRemoteSetState(
                    &target->remote,
                    NvTioTargetState_FileWait);
                err = NvTioGdbtCmdSend(&target->remote, &cmd);
                NvTioRemoteSetState(
                    &target->remote,
                    NvTioTargetState_Running);
                if (DBERR(err))
                    rv = -1;
                goto reply_none;
            }
            else
                rv = -1;
        }
    }
        
    if (!NvTioOsStrcmp(parms.parm[0].str,"lseek")) {

        NvU64 offset;
        int fd = parms.parm[1].val;
        NvOsSeekEnum whence = (NvOsSeekEnum)parms.parm[4].val;

        offset = parms.parm[3].val;
        offset <<= 32;
        offset |= (parms.parm[2].val);

        if ((fd <= NvTioTargetStream_reply) ||
            (fd >= NvTioTargetStream_Count) ||
            (!target->buf[fd].stream))
        {
            err = NvError_FileOperationFailed;
        }
        else
        {
            err = NvOsFseek(target->buf[fd].stream->f.fp, (NvS64)offset, whence);
            if (!err)
                rv = 0;
            else
                rv = -1;
        }
    }
        
    DB(("Unrecognized F command"));
    goto reply_state;


reply_state:
    // use state to decide what reply to use
    if (oldState == NvTioTargetState_Breakpoint ||
        oldState == NvTioTargetState_BreakpointCmd)
        goto reply_none;

reply_file:

    if (target->remote.cmdLen >= 0)
        NvTioGdbtEraseCommand(&target->remote);

    //
    // return file reply
    //
    NvTioGdbtCmdBegin(&cmd, cbuf, sizeof(cbuf));
    NvTioGdbtCmdChar(&cmd, 'F');
    if (rv == -1) {
        NvTioGdbtCmdString(&cmd, "-1,");
        NvTioGdbtCmdString(&cmd, fErrno);
    } else {
        NvTioGdbtCmdHexU32(&cmd, (NvU32)rv, 1);
    }
    err = NvTioGdbtCmdSend(&target->remote, &cmd);

    NvTioRemoteSetState(&target->remote, NvTioTargetState_Running);

reply_none:
    if (target->remote.cmdLen >= 0)
        NvTioGdbtEraseCommand(&target->remote);

    return DBERR(err);
}

//===========================================================================
// NvTioGdbtHandleStdoutRequest() - Handle an 'O' request from the target
//===========================================================================
static NvError
NvTioGdbtHandleStdoutRequest(NvTioTarget *target)
{
    NvError            e   = NvSuccess;
    NvTioStreamBuffer *buf;
    char              *p   = target->remote.buf + 1;
    NvU32              cnt = target->remote.cmdLen - 1;
    NvU8              *dst;

#if NV_TIO_DEBUG_ENABLE
    NvTioDebugf("Got Command:\n");
    NvTioShowBytes((const NvU8*)target->remote.buf, target->remote.cmdLen);
#endif

    //
    // add output to stdout buffer
    //
    buf = &target->buf[NvTioTargetStream_stdout];
#if NV_TIO_GDBT_O_REPLY_HEX
    if (cnt & 1)
        goto fail;
    NV_CHECK_ERROR_CLEANUP(NvTioBufAlloc(&buf->recv, cnt/2, &dst));
    NV_CHECK_ERROR_CLEANUP(NvTioHexToMem(dst, p, cnt/2));
    NvTioBufCommit(&buf->recv, cnt/2, dst);
#else
    NV_CHECK_ERROR_CLEANUP(NvTioBufAlloc(&buf->recv, cnt, &dst));
    NvOsMemcpy(dst, p, cnt);
    NvTioBufCommit(&buf->recv, cnt, dst);
#endif


 fail:
    NvTioRemoteSetState(&target->remote, NvTioTargetState_Running);

    if (target->remote.cmdLen >= 0)
        NvTioGdbtEraseCommand(&target->remote);
    return DBERR(e);
}


//===========================================================================
// NvTioGdbtHandleReply() - handle a reply (or F command) from the target
//===========================================================================
// NOTE: reply   starts at: &target->remote.buf[0]
//       reply   ends at:   &target->remote.buf[target->remote.cmdLen]
//       null terminated:    target->remote.buf[target->remote.cmdLen]==0
static NvError
NvTioGdbtHandleReply(NvTioTarget *target)
{
    NvError      e      = NvSuccess;
    NvTioRemote *remote = &(target->remote);

    NV_ASSERT(remote->cmdLen >= 0);

#if NV_TIO_DEBUG_ENABLE
    NvTioDebugf("Got Command in state %d:\n", remote->state);
    NvTioShowBytes((const NvU8*)remote->buf, remote->cmdLen);
#endif

    switch (remote->state)
    {
        case NvTioTargetState_Running:
            /* The target is running, so only expect a few commands. */

            if (remote->buf[0] == 'F')
            {
                /* F - request a file */
                return NvTioGdbtHandleFileRequest(target);
            }
            else if (remote->buf[0] == 'O')
            {
                /* O - provide text to stdout */
                NvTioRemoteSetState(remote, NvTioTargetState_FileWait);
                return NvTioGdbtHandleStdoutRequest(target);
            }
            else if (remote->buf[0] == 'S')
            {
                /* S - target signals a breakpoint */
                NvTioRemoteSetState(remote, NvTioTargetState_Breakpoint);
                return NvTioGdbtPropagateReply(target);
            }
            else if (remote->buf[0] == 'W')
            {
                /* W - target exiting */
                NvTioRemoteSetState(remote, NvTioTargetState_Exit);
                return NvTioGdbtPropagateReply(target);
            }
            else
            {
                /* Unexpected message. */
                DB(("Unexpected command %s in state Running in NvTioGdbtHandleReply()!!\n",
                    remote->buf));
                e = NvError_InvalidState;
                goto reply_none;
            }            
            break;

        case NvTioTargetState_BreakpointCmd:
            /* Transition back to Breakpoint and send the text onwards. */
            NvTioRemoteSetState(remote, NvTioTargetState_Breakpoint);
            return NvTioGdbtPropagateReply(target);

        case NvTioTargetState_Breakpoint:
        case NvTioTargetState_FileWait:
            /*
             * The target is waiting for a message from the host.
             * No messages should arrive from the target in this state.
             */
            DB(("Unexpected command %s in state %d inNvTioGdbtFwriteFile()!!\n",
                remote->buf, remote->state));
            e = NvError_InvalidState;
            goto reply_none;

        case NvTioTargetState_FileWaitCmd:
            /* The host is waiting for a reply from the target. */
        case NvTioTargetState_RunningEOF:        // running; got disconnected
        case NvTioTargetState_Exit:              // NvTioExit() called
        case NvTioTargetState_Unknown:
        case NvTioTargetState_Disconnected:
        default:
            /* TODO: Determine what is appropriate here! */
            DB(("TODO: Determine what to do with command %s in state %d inNvTioGdbtFwriteFile()!!\n",
                remote->buf, remote->state));
            e = NvError_InvalidState;
            goto reply_none;
    }

reply_none:
    if (remote->cmdLen >= 0)
        NvTioGdbtEraseCommand(remote);
    return DBERR(e);
}

//===========================================================================
// NvTioGdbtTargetRead() - read bytes from target & handle commands
//===========================================================================
// returns retval of NvTioGdbtHandleReply() if command was processed
// returns NvError_Timeout if no command was processed.
static NvError
NvTioGdbtTargetRead(NvTioTarget *target, NvU32 timeout_msec, int doRead)
{
    NvError err;
    NvU32 startTime = NvTioOsGetTimeMS();
    NvU32 timeLeft = timeout_msec;

    //
    // handle any buffered replies
    //
    for (;;) {
        // command ready?
        err = NvTioGdbtCmdCheck(&target->remote);
        if (!err) {
            return NvTioGdbtHandleReply(target);
        }

        //
        // Error parsing commands?
        //
        if (err != NvError_Timeout)
            return DBERR(err);

        //
        // Timeout == no command in buffer. Read more characters (only once).
        //
        if (!doRead)
            return NvError_Timeout;
        doRead = 0;

        err = NvTioGdbtGetChars(&target->remote, timeLeft);
        if (err) {
            if (err == NvError_EndOfFile)
                target->remote.hup = 1;
            return DBERR(err);
        }
        timeLeft = NvTioCalcTimeout(timeout_msec, startTime, 0);
    }
}

#if 0
//===========================================================================
// NvTioGdbtHaltTarget() - cause a target to halt (enter a breakpoint)
//===========================================================================
NvError NvTioGdbtFwriteCommand(NvTioTargetHandle target)
{
    //
    // Is target currently trying to read stdin?
    // (if so tell it a Ctrl-C occurred)
    //
    if (target->readStream) {
        NvU8 cbuf[NV_TIO_MAX_COMMAND];
        NvTioGdbtCmd cmd;
        NvTioTarget *target = (NvTioTarget*)remote;
        NV_ASSERT(remote->state == NvTioTargetState_FileWait);

        //
        // Abort Ffread command
        //
        target->readStream = 0;

        //
        // return file reply
        //
        NvTioGdbtCmdBegin(&cmd, cbuf, sizeof(cbuf));

        //
        // Ctrl-C response to Ffread  gdbtarget command
        //     F-1,4,C
        //     | | | |
        //     | | | +--- C  = ctrl-C occurred
        //     | | +----- 4  = EINTR
        //     | +------- -1 = error
        //     +--------- F  = file command response
        //
        NvTioGdbtCmdString(&cmd, "F-1,4,C");

        NvTioRemoteSetState(&target->remote, NvTioTargetState_Running);
        err = NvTioGdbtCmdSend(&target->remote, &cmd);
        if (err)
            return DBERR(err);
    }

    if (remote->state == NvTioTargetState_Running) {
        XXXXXXXXX TODO: send ctrl-C
    }

    while (remote->state != NvTioTargetState_Breakpoint) {
        XXXXXXXX TODO: wait for target to halt
        //NvTioRemoteSetState(&target->remote, NvTioTargetState_Breakpoint);
    }

    return NvSuccess;
}
#endif

//===========================================================================
// NvTioGdbtCmdSimple() - send a gdbtarget command and wait for ack
//===========================================================================
static NvError NvTioGdbtCmdSimple(
                            NvTioRemote *remote,
                            const void *ptr,
                            size_t size)
{
    size_t cmdSize = size + 6;
    NvError err;
    char *cbuf;
    NvTioGdbtCmd cmd;

    cbuf = NvOsAlloc(cmdSize);

    NvTioGdbtCmdBegin(    &cmd, cbuf, cmdSize);
    NvTioGdbtCmdData(     &cmd, ptr, size);
    err = NvTioGdbtCmdSend(remote, &cmd);
    NvOsFree(cbuf);
    return DBERR(err);
}

//===========================================================================
// NvTioGdbtFwriteCommand() - write a gdbtarget command, encoded
//===========================================================================
static NvError NvTioGdbtFwriteCommand(
                        NvTioStreamHandle stream, 
                        const void *ptr, 
                        size_t size)
{
    NvU8        *cmd    = (NvU8*)ptr;
    NvTioRemote *remote = stream->f.buf->remote;
    NvError      e;

    /* TODO: If more logic is not needed, eliminate the switch. */
    switch (remote->state)
    {
        case NvTioTargetState_Breakpoint:
            if (cmd[0] == 'c')
            {
                /* Target is transitioning to running. */
                remote->isRunCmd = 1;
                NvTioRemoteSetState(remote, NvTioTargetState_Running);
            }
            else
            {
                remote->isRunCmd = 0;
                NvTioRemoteSetState(remote, NvTioTargetState_BreakpointCmd);
            }
            e = NvTioGdbtCmdSimple(remote, ptr, size);
            break;

        case NvTioTargetState_Running:
        case NvTioTargetState_BreakpointCmd:
        case NvTioTargetState_FileWait:
        case NvTioTargetState_FileWaitCmd:
        case NvTioTargetState_RunningEOF:
        case NvTioTargetState_Exit:
        case NvTioTargetState_Unknown:
        case NvTioTargetState_Disconnected:
        default:
            /* TODO: Determine what is appropriate here! */
            DB(("TODO: Determine what to do with command %s in state %d inNvTioGdbtFwriteCommand()!!\n",
                cmd, remote->state));
            e = NvError_InvalidState;
            break;
    }

    return DBERR(e);
}

//===========================================================================
// NvTioGdbtFwriteFile() - write to a gdbtarget file descriptor
//===========================================================================
static NvError NvTioGdbtFwriteFile(
                        NvTioStreamHandle stream, 
                        const void *ptr, 
                        size_t size)
{
    NvError err;
    NvU8 *dst;
    NvTioStreamBuffer *buf = stream->f.buf;
    NvTioTarget *target = (NvTioTarget*)buf->remote;

    NV_ASSERT(size > 0);

    err = NvTioBufAlloc(&buf->send, size, &dst);
    if (err) {
        DB(("Buffer overflow in NvTioGdbtFwriteFile()!!\n"));
        return DBERR(err);
    }

    NvOsMemcpy(dst, ptr, size);
    NvTioBufCommit(&buf->send, size, dst);

    if (target->readStream == buf)
        return NvTioGdbtHandleReadReply(0, target);

    return NvSuccess;
}

//===========================================================================
// NvTioGdbtFread() - read a gdbtarget file descriptor
//===========================================================================
static NvError NvTioGdbtFread(
                        NvTioStreamHandle stream, 
                        void *ptr, 
                        size_t size,
                        size_t *bytes,
                        NvU32 timeout_msec)
{
    NvError err;
    NvTioStreamBuffer *buf = stream->f.buf;
    NvTioTarget *target = (NvTioTarget*)buf->remote;
    NvU32 startTime = 0;
    NvU32 timeLeft = timeout_msec;
    
    if (!size) {
        *bytes = 0;
        return NvSuccess;
    }

    // TODO: FIXME: fix it so only complete replies are returned

    if (timeout_msec && timeout_msec != NV_WAIT_INFINITE)
        startTime = NvTioOsGetTimeMS();
    while(!buf->recv.bufCnt) {
        err = NvTioGdbtTargetRead(target, timeLeft, 1);
        if (err)
            return DBERR(err);
        if (buf->recv.bufCnt)
            break;
        timeLeft = NvTioCalcTimeout(timeout_msec, startTime, 0);
    }

    if (size > buf->recv.bufCnt)
        size = buf->recv.bufCnt;

    NvOsMemcpy(ptr, buf->recv.buf, size);
    NvTioBufShrink(&buf->recv, size);

    *bytes = size;

    return NvSuccess;
}

//===========================================================================
// NvTioGdbtPollPrep() - prepare to poll
//===========================================================================
static NvError NvTioGdbtPollPrep(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc)
{
    NvTioStreamBuffer *buf = tioDesc->stream->f.buf;
    NvTioPollDesc tioDesc2;
    tioDesc2 = *tioDesc;
    tioDesc2.stream = &buf->remote->stream;
    if (!tioDesc2.stream->ops->sopPollPrep)
        return NvError_NotSupported;

    //
    // May have become readable while checking one of the other buffers that
    // shares the same remote sub stream.
    //
    if (buf->recv.bufCnt)
        tioDesc->returnedEvents |= NvTrnPollEvent_In & tioDesc->requestedEvents;

    return tioDesc2.stream->ops->sopPollPrep(&tioDesc2, osDesc);
}

//===========================================================================
// NvTioGdbtLinuxPollCheck() - Check result of poll
//===========================================================================
static NvError NvTioGdbtPollCheck(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc)
{
    NvError err;
    NvTioStreamBuffer *buf = tioDesc->stream->f.buf;
    NvTioTarget *target = (NvTioTarget*)buf->remote;
    NvTioPollDesc tioDesc2;
    int doRead = 1;

    if (buf->recv.bufCnt || target->remote.hup) {
        tioDesc->returnedEvents = NvTrnPollEvent_In;
        return NvSuccess;
    }

    tioDesc->returnedEvents = 0;

    tioDesc2 = *tioDesc;
    tioDesc2.stream = &target->remote.stream;

    if (!tioDesc2.stream->ops->sopPollCheck)
        return NvError_NotSupported;

    //
    // check for readability
    //
    err = tioDesc2.stream->ops->sopPollCheck(&tioDesc2, osDesc);
    if (err)
        return DBERR(err);

    //
    // read more chars if the connection stream is readable
    //
    doRead = tioDesc2.returnedEvents & NvTrnPollEvent_In ? 1 : 0;
    for (;;) {
        err = NvTioGdbtTargetRead(target, 0, doRead);
        if (buf->recv.bufCnt || target->remote.hup) {
            tioDesc->returnedEvents = NvTrnPollEvent_In;
            return NvSuccess;
        }
        if (err)
            break;
        if (doRead)
            doRead = 0;
        else
            return NvSuccess;
    }
    return err == NvError_Timeout ? NvSuccess : DBERR(err);
}

//===========================================================================
// NvTioGdbtClose() - close a virtual stream
//===========================================================================
static void NvTioGdbtClose(NvTioStreamHandle stream)
{
    stream->f.buf->stream = 0;
    stream->f.buf = 0;
}

//===========================================================================
// NvTioGdbtTerminateTarget() - shut down gdbtarget protocol
//===========================================================================
static void NvTioGdbtTerminateTarget(NvTioRemote *remote)
{
    NvTioTarget *target = (NvTioTarget*)remote;
    NvTioStreamBuffer *buf;
    int i;

    buf = target->buf;
    for (i=0; i<NvTioTargetStream_Count; i++) {
        NvOsFree(target->buf[i].send.buf);
        NvOsFree(target->buf[i].recv.buf);
        buf->send.buf = 0;
        buf->recv.buf = 0;
    }
}

//===========================================================================
// NvTioGdbtInitTarget() - set up target for gdbtarget protocol
//===========================================================================
NvError NvTioGdbtInitTarget(NvTioTarget *target)
{
    static NvTioStreamOps cmd_ops = {0};
    static NvTioStreamOps read_ops = {0};
    static NvTioStreamOps write_ops = {0};
    static NvTioStreamOps remote_file_ops = {0};
    int i;

    if (!cmd_ops.sopClose) {
        cmd_ops.sopName       = "GdbTarget_target_cmd_ops";
        cmd_ops.sopClose      = NvTioGdbtClose;
        cmd_ops.sopFwrite     = NvTioGdbtFwriteCommand;
                              
        read_ops.sopName      = "GdbTarget_target_read_ops";
        read_ops.sopClose     = NvTioGdbtClose;
        read_ops.sopFread     = NvTioGdbtFread;
        read_ops.sopPollPrep  = NvTioGdbtPollPrep;
        read_ops.sopPollCheck = NvTioGdbtPollCheck;

        write_ops.sopName     = "GdbTarget_target_write_ops";
        write_ops.sopClose    = NvTioGdbtClose;
        write_ops.sopFwrite   = NvTioGdbtFwriteFile;

        remote_file_ops.sopName = "GdbTarget_target_remote_file_ops";
        remote_file_ops.sopClose    = NvTioGdbtClose;
        remote_file_ops.sopFwrite   = NvTioGdbtFwriteFile;
        remote_file_ops.sopFread    = NvTioGdbtFread;
        remote_file_ops.sopPollPrep = NvTioGdbtPollPrep;
        remote_file_ops.sopPollCheck= NvTioGdbtPollCheck;
    }

    target->remote.terminate = NvTioGdbtTerminateTarget;

    for (i=0; i<NvTioTargetStream_Count; i++) {
        target->buf[i].remote      = &target->remote;

        if (i==NvTioTargetStream_stdin ||
            i==NvTioTargetStream_reply) {

            target->buf[i].send.bufLen = NV_TIO_TARGET_CONNECTION_BUFFER_SIZE;
            target->buf[i].send.buf    = NvOsAlloc(target->buf[i].send.bufLen);
            if (!target->buf[i].send.buf)
                goto fail;
        }
        if (i==NvTioTargetStream_stdout ||
            i==NvTioTargetStream_stderr ||
            i==NvTioTargetStream_reply) {

            target->buf[i].recv.bufLen = NV_TIO_TARGET_CONNECTION_BUFFER_SIZE;
            target->buf[i].recv.buf    = NvOsAlloc(target->buf[i].recv.bufLen);
            if (!target->buf[i].recv.buf)
                goto fail;
        }
        if (i>NvTioTargetStream_reply) {
            target->buf[i].send.bufLen = NV_TIO_TARGET_CONNECTION_BUFFER_SIZE;
            target->buf[i].recv.bufLen = NV_TIO_TARGET_CONNECTION_BUFFER_SIZE;
            target->buf[i].send.bufCnt = target->buf[i].recv.bufCnt = 0;
            target->buf[i].send.buf    = NvOsAlloc(target->buf[i].send.bufLen);
            if (!target->buf[i].send.buf)
                goto fail;
            target->buf[i].recv.buf    = NvOsAlloc(target->buf[i].recv.bufLen);
            if (!target->buf[i].recv.buf)
                goto fail;
            target->buf[i].stream = 0;
            target->buf[i].ops = &remote_file_ops;
        }
    }

    target->buf[NvTioTargetStream_stdin].name = "stdin:";
    target->buf[NvTioTargetStream_cmd].name   = "cmd:";
    target->buf[NvTioTargetStream_stdout].name= "stdout:";
    target->buf[NvTioTargetStream_stderr].name= "stderr:";
    target->buf[NvTioTargetStream_reply].name = "reply:";

    target->buf[NvTioTargetStream_stdin].ops   = &write_ops;
    target->buf[NvTioTargetStream_cmd].ops     = &cmd_ops;
    target->buf[NvTioTargetStream_stdout].ops  = &read_ops;
    target->buf[NvTioTargetStream_stderr].ops  = &read_ops;
    target->buf[NvTioTargetStream_reply].ops   = &read_ops;
    
    return NvSuccess;

fail:
    NvTioGdbtTerminateTarget(&target->remote);
    return NvError_InsufficientMemory;
}
