/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
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
#include "tio_stdio.h"
#include "nvassert.h"
#include "nvos.h"
#include <stdio.h>
#include "nvapputil.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

static NvU32 expectedSn = 0; // expected read request serial number

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioStdioHandleReadReply() - handle a Fread reply to the target
//===========================================================================
static NvError NvTioStdioHandleReadReply(NvTioTarget *target)
{
    NvError err = NvSuccess;
    NvU32 cnt;
    NvU32 maxcnt;
    NvTioStreamBuffer *buf = target->readStream;
    NvTioRemote *remote = &(target->remote);
    NvTioStdioPkt pkt;
    static NvU8 cbuf[NV_TIO_STDIO_MAX_PKT]; // pkt buffer
    NvU32 pktSize = 0;

    NV_ASSERT(buf);
    NV_ASSERT(buf->send.bufCnt);
    NV_ASSERT(target->readLen > 0);
    NV_ASSERT(target->remote.cmdLen == -1);
    NV_ASSERT(target->remote.state == NvTioTargetState_FileWait);

    maxcnt = sizeof(cbuf) - sizeof(NvTioStdioPktHeader);
    cnt = buf->send.bufCnt;
    if (cnt > target->readLen)
        cnt = target->readLen;
    if (cnt > maxcnt)
        cnt = maxcnt;

    //
    // We found through experiments that
    // if host sends to target a USB packet
    // of size that is a multiple of 512, two USB status
    // registers ENDPTSTATUS and ENDPTCOMPLETE
    // are not updated properly by USB controller
    // though it's verified through trace debugger
    // that the packets are recieved by target.
    //
    // For packets of size that's not a multiple of 512,
    // those registers are updated shortly
    // with the following values:
    // ENDPTPRIM = 0, ENDPTSTATUS = 0, ENDPTCOMPLETE = 2
    // But for the problematic packets,
    // those registers keep the values below
    // ENDPTPRIM = 0, ENDPTSTATUS = 2, ENDPTCOMPLETE = 0
    //
    // Avoid sending those packets until the
    // root cause is found and resolved.
    //
    pktSize = cnt + sizeof(NvTioStdioPktHeader);
    if ( (pktSize % 512) == 0)
    {
#if NVTIO_STDIO_DEBUG
        NvTioDebugf("Avoid sending a packet of size %d\n",
                    pktSize);
#endif
        cnt = cnt - 4;
    }

    NvTioStdioInitPkt(&pkt, cbuf);
    pkt.pPktHeader->fd     = target->readAddr;
    pkt.pPktHeader->cmd    = NvTioStdioCmd_fread_reply;
    NvTioStdioPktAddData(&pkt, buf->send.buf, cnt);
    err = NvTioStdioPktSend(remote, &pkt);
#if NVTIO_STDIO_DEBUG
    NvTioDebugf("Sending Command:\n");
    NvTioShowBytes((const NvU8*) cbuf, cnt + sizeof(NvTioStdioPktHeader));
#endif
    NvTioBufShrink(&buf->send, cnt);
    NvTioRemoteSetState(&target->remote, NvTioTargetState_Running);
    target->readStream = 0;
    target->readAddr = -1;
    target->readLen = 0;
    expectedSn++;

    return err;
}


/*
 * NvTioStdioPropagateReply()
 *
 * Propagate the reply text to the higher-level code.
 * All state changes and command-specific error checking should happen
 * elsewhere - this is a common utility function for the command processing
 * code.
 */
static NvError
NvTioStdioPropagateReply(NvTioTarget *target)
{
    NvError            e   = NvSuccess;
    NvTioStreamBuffer *buf = &target->buf[NvTioTargetStream_reply];
    NvTioRemote *remote = &(target->remote);
    NvTioStdioPktHeader *header = (NvTioStdioPktHeader*)remote->buf;
    NvU8              *dst;
    size_t            size;

#if NVTIO_STDIO_DEBUG
    if(remote->cmdLen > 0) {
        NvTioDebugf("Got Command:\n");
        NvTioShowBytes((const NvU8*)target->remote.buf, target->remote.cmdLen);
    }
#endif
    //
    // Already have a reply pending?
    //
    if (buf->recv.bufCnt)
    {
        NvTioDebugf("Ignoring extra reply: ");
        NvTioShowBytes(target->remote.buf, target->remote.cmdLen);

        NvTioDebugf("Existing reply:       ");
        NvTioShowBytes(buf->recv.buf, buf->recv.bufCnt);
    }
    else
    {
        //
        // add reply to reply buffer
        //
        if(header->cmd == NvTioStdioCmd_exit) {
            NvTioRemoteSetState(remote, NvTioTargetState_Exit);
        }
        size = target->remote.cmdLen - sizeof(NvTioStdioPktHeader);
        e = NvTioBufAlloc(&buf->recv, size, &dst);
        if (e) {
            DB(("Buffer overflow in reply buffer()!!\n"));
            return DBERR(e);
        } else {
            NvOsMemcpy(dst, target->remote.buf + sizeof(NvTioStdioPktHeader), size);
            NvTioBufCommit(&buf->recv, size, dst);
        }
    }

    if (target->remote.cmdLen >= 0)
        NvTioStdioEraseCommand(&target->remote);

    return DBERR(e);
}

//===========================================================================
// NvTioStdioHandleStdoutRequest() - Handle stdout/stderr
//===========================================================================
static NvError
NvTioStdioHandleStdoutRequest(NvTioTarget *target)
{
    NvError            err   = NvSuccess;
    NvTioRemote *remote = &(target->remote);
    NvTioStdioPktHeader *header = (NvTioStdioPktHeader*)remote->buf;
    NvTioStreamBuffer *buf;
    char              *p   = target->remote.buf + sizeof(NvTioStdioPktHeader);
    NvU32              cnt = header->payload;
    NvU8              *dst;
    buf = &target->buf[header->fd];
    // !!To avoid overflow, only read data from the input buffer when there is enough space
    if(buf->recv.bufLen >= buf->recv.bufCnt + cnt){
    #if NVTIO_STDIO_DEBUG
        if(remote->cmdLen > 0) {
            NvTioDebugf("Got Command:\n");
            NvTioShowBytes((const NvU8*)target->remote.buf, target->remote.cmdLen);
        }
    #endif
        if(
        #if NVTIO_STDIO_SENDBACK == 0
            header->fd == NvTioStdioStream_stderr ||
        #endif
            header->fd == NvTioStdioStream_stdout
        ) {
            NvTioBufAlloc(&buf->recv, cnt, &dst);
            NvOsMemcpy(dst, p, cnt);
            NvTioBufCommit(&buf->recv, cnt, dst);
        }
        NvTioRemoteSetState(&target->remote, NvTioTargetState_Running);
        NvTioStdioEraseCommand(&target->remote);
    } else {
        // Don't read or change the remote buf and return timeout
        NvTioDebugf("Warning @%d, %s: Timeout becuase buffer is full\n", __LINE__, __FILE__);
        err = NvError_Timeout;
    }
    return DBERR(err);
}


//===========================================================================
// NvTioStdioHandleFileWriteReply() - send ack or nack for a file write
//                                    request
//===========================================================================
static void
NvTioStdioHandleFileWriteReply(NvTioTarget *target, NvBool sendAck)
{
    NvTioRemote *remote = &(target->remote);
    NvTioStreamBuffer *buf = &(target->buf[NvTioStdioStream_file]);
    NvTioStdioPkt pkt;

    NV_ASSERT(target->remote.cmdLen == -1);
    NV_ASSERT(target->remote.state == NvTioTargetState_FileWait);

    NvTioStdioInitPkt(&pkt, buf->send.buf);
    pkt.pPktHeader->fd     = NvTioStdioStream_file;
    if(sendAck) {
        pkt.pPktHeader->cmd    = NvTioStdioCmd_ack;
    } else {
        pkt.pPktHeader->cmd    = NvTioStdioCmd_nack;
    }
    NvTioStdioPktSend(remote, &pkt);
#if NVTIO_STDIO_DEBUG
    NvTioDebugf("Sending Command:\n");
    NvTioShowBytes((const NvU8*) buf->send.buf,
        sizeof(NvTioStdioPktHeader));
#endif
    NvTioRemoteSetState(remote, NvTioTargetState_Running);
}

//===========================================================================
// NvTioStdioHandleFileOpenReply() - Handle file open reply of the file
// stream
//===========================================================================
static NvError
NvTioStdioHandleFileOpenReply(NvTioTarget *target, NvOsFileHandle fh)
{
    NvError err = NvSuccess;
    NvTioRemote *remote = &(target->remote);
    NvTioStreamBuffer *buf = &(target->buf[NvTioStdioStream_file]);
    NvTioStdioPkt pkt;

    NV_ASSERT(target->remote.cmdLen == -1);
    NV_ASSERT(target->remote.state == NvTioTargetState_FileWait);

    NvTioStdioInitPkt(&pkt, buf->send.buf);
    pkt.pPktHeader->fd     = NvTioStdioStream_file;
    pkt.pPktHeader->cmd    = NvTioStdioCmd_fopen_reply;
    NvTioStdioPktAddData(&pkt, &fh, sizeof(NvOsFileHandle));
    err = NvTioStdioPktSend(remote, &pkt);
#if NVTIO_STDIO_DEBUG
    NvTioDebugf("Sending Command:\n");
    NvTioShowBytes((const NvU8*) buf->send.buf,
        sizeof(fh) + sizeof(NvTioStdioPktHeader));
#endif
    NvTioRemoteSetState(remote, NvTioTargetState_Running);
    return err;
}

//===========================================================================
// NvTioStdioHandleFileRequest() - Handle file request of the file
// stream and stdin
//===========================================================================
static NvError
NvTioStdioHandleFileRequest(NvTioTarget *target)
{
    NvError err = NvSuccess;
    NvTioRemote *remote = &(target->remote);
    NvTioStdioPktHeader *header = (NvTioStdioPktHeader*)remote->buf;
    NvTioStreamBuffer *buf;
    NvU8*   pHeader = (NvU8 *) header;
    NvU32*  pData   = (NvU32 *) (pHeader + sizeof(NvTioStdioPktHeader));

#if NVTIO_STDIO_DEBUG
    if(target->remote.cmdLen > 0)
    {
        NvTioDebugf("Got Command:\n");
        NvTioShowBytes((const NvU8*)target->remote.buf, target->remote.cmdLen);
    }
#endif

    NvTioRemoteSetState(&target->remote, NvTioTargetState_FileWait);
    NvTioStdioEraseCommand(&target->remote);

    if( (header->cmd == NvTioStdioCmd_fread) &&
       (header->fd == NvTioStdioStream_stdin) )
    {
        NvU32 cnt = 0;
        NvU32 sn = 0;

        // in a read packet the first data in payload is
        // the size of data to read;
        // the second data is the serial number of the read
        // request packet
        cnt = *pData;
        sn  = *(pData + 1);

        // readAddr in stdio now represents the fd of the readstream
        target->readAddr    = header->fd;
        buf                 = &target->buf[header->fd];
        target->readStream  = buf;
        target->readLen     = cnt;

    #if NVTIO_STDIO_DEBUG
        if(sn < expectedSn) {
            NvTioDebugf("Warning@%d, %s: a redundant pakcet#%d "
                "is received. Expecting %d, So ignore it.\n",
                __LINE__, __FILE__, sn, expectedSn);
        }
    #endif

        if(sn > expectedSn) {
            // Pakcet might have been lost...
            NvTioDebugf("Error@%d, %s: Expecting read request "
                "packet #%d, received packet #%d. \n",
                __LINE__, __FILE__, expectedSn, sn);
            err = NvError_FileOperationFailed;
        }

        if (sn == expectedSn) {
            if (buf->send.bufCnt > 0 )
            {
                return NvTioStdioHandleReadReply(target);
            }
        }
    }
    else if (header->fd == NvTioStdioStream_file)
    {
        NvOsFileHandle  fh;

        if (header->cmd == NvTioStdioCmd_fopen) {
            // handle a fopen request
            // the packet includes flags, and the file path
            // return the file handle
            NvU32 flags = 0;
            char  *path = 0;

            flags = *pData;
            pData = pData + 1;
            path = (char*) pData;
            err = NvOsFopen(path, flags, &fh);
            if (!err) {
                err = NvTioStdioHandleFileOpenReply(target, fh);
            } else {
                NvTioDebugf("Error (%s): opening file %s with "
                    "flags 0x%08x failed @ %d of %s\n",
                    NvAuErrorName(err), path, flags,
                    __LINE__, __FILE__);
            }
        }
        else if (header->cmd == NvTioStdioCmd_fclose) {
            // handle a fclose request
            // packet contains the file handle
            // no reply is needed
            fh = (NvOsFileHandle)(*pData);
            NvOsFclose(fh);
            NvTioRemoteSetState(&target->remote, NvTioTargetState_Running);
        }
        else if (header->cmd == NvTioStdioCmd_fwrite) {
            // hanle a fwrite request
            // packet incluces the file handle and the data to write
            // no reply packet is sent to target
            NvU32 cnt = header->payload - sizeof(NvU32);
            fh = (NvOsFileHandle)(*pData);
            pData = pData + 1;
            err = NvOsFwrite(fh, pData, cnt);
            if(!err) {
                // send back an ACK
                NvTioStdioHandleFileWriteReply(target, NV_TRUE);
            } else {
                // send back an NACK
                NvTioStdioHandleFileWriteReply(target, NV_FALSE);
            }
        }
        else {
            goto cmd_unknown;
        }
    } else {
cmd_unknown:
            NvTioDebugf("Error: Unknown command 0x%08x "
                "from stream 0x%08x @%d of %s\n",
                header->cmd, header->fd, __LINE__, __FILE__);
            err = NvError_FileOperationFailed;
            return err;
    }

    return err;
}
//===========================================================================
// NvTioStdioHandleReply() - handle a reply from the target
//===========================================================================
static NvError
NvTioStdioHandleReply(NvTioTarget *target)
{
    NvError      e      = NvSuccess;
    NvTioRemote *remote = &(target->remote);
    NvTioStdioPktHeader *header = (NvTioStdioPktHeader*)remote->buf;

    NV_ASSERT(remote->cmdLen >= 0);

    switch (remote->state)
    {
        case NvTioTargetState_Running:
        case NvTioTargetState_FileWait:
            /* The target is running, so only expect a few commands. */

            if (
                    header->fd == NvTioStdioStream_stdin ||
                    header->fd == NvTioStdioStream_file
               )
            {
                return NvTioStdioHandleFileRequest(target);
            }
            else if (
                        header->fd == NvTioStdioStream_stdout ||
                        header->fd == NvTioStdioStream_stderr
                    )
            {
                return NvTioStdioHandleStdoutRequest(target);
            }
            else if (header->fd == NvTioStdioStream_reply)
            {
                return NvTioStdioPropagateReply(target);
            }
            else
            {
                /* Unexpected message. */
            #if NVTIO_STDIO_DEBUG
                NvTioDebugf("Error @%d, %s: Unexpected command in "
                    "state Running in NvTioStdioHandleReply()!!\n",
                    __LINE__, __FILE__);

                NvTioShowBytes((const NvU8*)target->remote.buf, target->remote.cmdLen);
            #endif
                e = NvError_InvalidState;
                goto reply_none;
            }
            break;

        case NvTioTargetState_BreakpointCmd:
        case NvTioTargetState_Breakpoint:
            /*
             * The target is waiting for a message from the host.
             * No messages should arrive from the target in this state.
             */
            #if NVTIO_STDIO_DEBUG
                NvTioDebugf("Error@%d, %s: Unexpected command in state %d in NvTioStdioHandleReply()!!\n", __LINE__, __FILE__, remote->state);
                NvTioShowBytes((const NvU8*)target->remote.buf, target->remote.cmdLen);
            #endif
                e = NvError_InvalidState;
                goto reply_none;

        case NvTioTargetState_FileWaitCmd:
            /* The host is waiting for a reply from the target. */
        case NvTioTargetState_RunningEOF:        // running; got disconnected
        case NvTioTargetState_Exit:              // NvTioExit() called
        case NvTioTargetState_Unknown:
        case NvTioTargetState_Disconnected:
        default:
            #if NVTIO_STDIO_DEBUG
                NvTioDebugf("TODO: Determine what to do with command in state %d in NvTioStdioFwriteFile()!!\n",
                            remote->state);
                NvTioShowBytes((const NvU8*)target->remote.buf, target->remote.cmdLen);
            #endif
            /* TODO: Determine what is appropriate here! */
            e = NvError_InvalidState;
            goto reply_none;
    }

reply_none:
    if (remote->cmdLen >= 0)
        NvTioStdioEraseCommand(remote);
    return DBERR(e);
}

//===========================================================================
// NvTioStdioTargetRead() - read bytes from target & handle commands
//===========================================================================
// returns retval of NvTioStdioHandleReply() if command was processed
// returns NvError_Timeout if no command was processed.
static NvError
NvTioStdioTargetRead(NvTioTarget *target, NvU32 timeout_msec)
{
    NvError err;
    NvTioRemote *remote = &target->remote;
        //wait to get a pkt?
    err = NvTioStdioPktWait(remote, timeout_msec);
    if (!err) {
        return NvTioStdioHandleReply(target);
    }
    return DBERR(err);
}

//===========================================================================
// NvTioStdioFwriteFile() - write to a stdio file descriptor
//===========================================================================
static NvError NvTioStdioFwriteFile(
                        NvTioStreamHandle stream,
                        const void *ptr,
                        size_t size)
{
    NvError err;
    NvU8 *dst;
    NvTioStreamBuffer *buf = stream->f.buf;
    NvTioTarget *target = (NvTioTarget*)buf->remote;

    err = NvTioBufAlloc(&buf->send, size, &dst);
    if (err) {
        NvTioDebugf("Buffer overflow in NvTioStdioFwriteFile()!!\n");
        return DBERR(err);
    }

    NvOsMemcpy(dst, ptr, size);
    NvTioBufCommit(&buf->send, size, dst);

    if (target->readStream == buf)
        return NvTioStdioHandleReadReply(target);

    return NvSuccess;
}

//===========================================================================
// NvTioStdioFread() - read a stdio file descriptor
//===========================================================================
static NvError NvTioStdioFread(
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

     *bytes = 0;

    if (!size) {
        return NvSuccess;
    }

    if (timeout_msec && timeout_msec != NV_WAIT_INFINITE)
        startTime = NvTioOsGetTimeMS();
    while(!buf->recv.bufCnt) {
        err = NvTioStdioTargetRead(target, timeLeft);
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
// NvTioStdioFwriteCommand() - write a stdio command
//===========================================================================
static NvError NvTioStdioFwriteCommand(
                        NvTioStreamHandle stream,
                        const void *ptr,
                        size_t size)
{
    NvTioDebugf("Write command is not supported!\n");
    return NvError_NotSupported;
}

//===========================================================================
// NvTioStdioPollPrep() - prepare to poll
//===========================================================================
static NvError NvTioStdioPollPrep(
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
// NvTioStdioPollCheck() - Check result of poll
//===========================================================================
static NvError NvTioStdioPollCheck(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc)
{
    NvError err;
    NvTioStreamBuffer *buf = tioDesc->stream->f.buf;
    NvTioTarget *target = (NvTioTarget*)buf->remote;
    NvTioPollDesc tioDesc2;
    int doRead = 1;
    NvU32   timeoutMS = NV_WAIT_INFINITE;

    if (buf->recv.bufCnt || target->remote.hup) {
        tioDesc->returnedEvents = NvTrnPollEvent_In;
        return NvSuccess;
    }

    tioDesc->returnedEvents = 0;

    // Only check a packet once a round to avoid unnecessary waiting time
    if(buf == &target->buf[NvTioStdioStream_stdout]) {
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
        // read a pkt if the connection stream is readable
        //
        doRead = tioDesc2.returnedEvents & NvTrnPollEvent_In ? 1 : 0;
        if(doRead) {
            err = NvTioStdioTargetRead(target, timeoutMS);
            if (buf->recv.bufCnt || target->remote.hup) {
                tioDesc->returnedEvents = NvTrnPollEvent_In;
                return NvSuccess;
            }
            return err == NvError_Timeout ? NvSuccess : DBERR(err);
        }
    }
    return NvSuccess;
}

//===========================================================================
// NvTioStdioClose() - close a virtual stream
//===========================================================================
static void NvTioStdioClose(NvTioStreamHandle stream)
{
    stream->f.buf->stream = 0;
    stream->f.buf = 0;
}

//===========================================================================
// NvTioStdioTerminateTarget() - shut down stdio protocol
//===========================================================================
static void NvTioStdioTerminateTarget(NvTioRemote *remote)
{
    NvTioTarget *target = (NvTioTarget*)remote;
    NvTioStreamBuffer *buf;
    int i;

    buf = target->buf;
    for (i=0; i<NvTioStdioStream_Count; i++) {
        NvOsFree(target->buf[i].send.buf);
        NvOsFree(target->buf[i].recv.buf);
        buf->send.buf = 0;
        buf->recv.buf = 0;
    }
}

//===========================================================================
// NvTioStdioInitTarget() - set up target for stdio protocol
//===========================================================================
NvError NvTioStdioInitTarget(NvTioTarget *target)
{
    static NvTioStreamOps cmd_ops = {0};
    static NvTioStreamOps read_ops = {0};
    static NvTioStreamOps write_ops = {0};
    int i;

    if (!cmd_ops.sopClose) {
        cmd_ops.sopName       = "NvTioStdio_target_cmd_ops";
        cmd_ops.sopClose      = NvTioStdioClose;
        cmd_ops.sopFwrite     = NvTioStdioFwriteCommand;

        read_ops.sopName      = "NvTioStdio_target_read_ops";
        read_ops.sopClose     = NvTioStdioClose;
        read_ops.sopFread     = NvTioStdioFread;
        read_ops.sopPollPrep  = NvTioStdioPollPrep;
        read_ops.sopPollCheck = NvTioStdioPollCheck;

        write_ops.sopName     = "NvTioStdio_target_write_ops";
        write_ops.sopClose    = NvTioStdioClose;
        write_ops.sopFwrite   = NvTioStdioFwriteFile;
    }

    target->remote.terminate = NvTioStdioTerminateTarget;

    for (i=0; i<NvTioStdioStream_Count; i++) {
        target->buf[i].remote      = &target->remote;

        if (i == NvTioStdioStream_stdin) {
            target->buf[i].send.bufLen = NV_TIO_HOST_CONNECTION_BUFFER_SIZE;
            target->buf[i].send.buf    = NvOsAlloc(target->buf[i].send.bufLen);
            if (!target->buf[i].send.buf)
                goto fail;
        }
        if (i == NvTioStdioStream_stdout ||
            i == NvTioStdioStream_stderr ||
            i == NvTioStdioStream_reply) {

            target->buf[i].recv.bufLen = NV_TIO_TARGET_CONNECTION_BUFFER_SIZE;
            target->buf[i].recv.buf    = NvOsAlloc(target->buf[i].recv.bufLen);
            if (!target->buf[i].recv.buf)
                goto fail;
        }
        if (i > NvTioStdioStream_reply) {
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

            // file stream is not supposed to
            // be opened directly by NvTioOpen()
            // or closed by NvTioClose
            target->buf[i].ops = 0;
        }
    }

    target->buf[NvTioStdioStream_stdin].name = "stdin:";
    target->buf[NvTioStdioStream_cmd].name   = "cmd:";
    target->buf[NvTioStdioStream_stdout].name= "stdout:";
    target->buf[NvTioStdioStream_stderr].name= "stderr:";
    target->buf[NvTioStdioStream_reply].name = "reply:";

    target->buf[NvTioStdioStream_stdin].ops   = &write_ops;
    target->buf[NvTioStdioStream_cmd].ops     = &cmd_ops;
    target->buf[NvTioStdioStream_stdout].ops  = &read_ops;
    target->buf[NvTioStdioStream_stderr].ops  = &read_ops;
    target->buf[NvTioStdioStream_reply].ops   = &read_ops;

    return NvSuccess;

fail:
    NvTioStdioTerminateTarget(&target->remote);
    return NvError_InsufficientMemory;
}
