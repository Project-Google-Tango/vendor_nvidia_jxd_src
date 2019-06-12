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

#include "tio_stdio.h"
#include "tio_local_target.h"
#include "tio_sdram.h"
#include "nvassert.h"
#include "nvutil.h"
#include "nvapputil.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################


//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

static NvU8           s_msgBuf[NV_TIO_STDIO_MAX_PKT];  // send && receive buffer

static NvTioStreamOps s_TioStdioHostFileOps = {0};
//###########################################################################
//############################### CODE ######################################
//###########################################################################

void NvTioStdioTargetDebugf(const char *format, ... )
{
    static char     errMsgBuf[128]; // formated error msg to send back to host
    NvTioHost *host;
    NvTioRemote *remote;
    NvTioStdioPkt pkt;
    va_list ap;
    size_t len;

    NvTioGetHost(&host);
    remote = &host->remote;
    NvTioStdioEraseCommand(remote);   // erase existing reply

    va_start(ap, format);
    len = NvOsVsnprintf(errMsgBuf, sizeof(errMsgBuf), format, ap);
    va_end(ap);

    NvTioStdioInitPkt(&pkt, s_msgBuf);
    pkt.pPktHeader->fd  = NvTioStdioStream_stderr;
    pkt.pPktHeader->cmd = NvTioStdioCmd_fwrite;
    NvTioStdioPktAddData(&pkt, errMsgBuf, len);
    NvTioStdioPktSend(remote, &pkt);
}

//===========================================================================
// NvTioStdioTargetExit() - exit process
//===========================================================================
static void NvTioStdioTargetExit(int status)
{
    NvError err;
    NvTioHost *host;
    NvTioRemote *remote;
    NvTioStdioPkt pkt;
    char cbuf[9]; // store status hex string
                  // it's the way nvhost works. Don't ask me why
    size_t size;

    err = NvTioGetHost(&host);
    if (err) {
        (void)DBERR(err);
        DB(("Could not connect to Host"));
        return;
    }

    NV_ASSERT(host);
    NV_ASSERT(host->remote.cmdLen == -1);
    remote = &host->remote;

    NvTioRemoteSetState(remote, NvTioTargetState_Exit);

    NvTioStdioInitPkt(&pkt, s_msgBuf);

    pkt.pPktHeader->fd = NvTioStdioStream_reply;
    pkt.pPktHeader->cmd = NvTioStdioCmd_exit;
    NvTioStdioPktAddChar(&pkt, 'W');
    size = NvOsSnprintf(cbuf, sizeof(cbuf), "%08x", status);
    NvTioStdioPktAddData(&pkt, cbuf, size);
    NvTioStdioPktSend(remote, &pkt);
    NvTioRemoteSetState(remote, NvTioTargetState_Exit);
    //
    // disconnect
    //
    NvTioDisconnectFromHost(host);
}


//===========================================================================
// NvTioStdioHostWrite() - write stream to host
//===========================================================================
static NvError NvTioStdioHostWrite(
            NvTioStreamHandle stream,
            const void *ptr,
            size_t size )
{
    NvTioHost *host;
    NvTioRemote *remote;
    size_t maxsz;
    NvTioStdioPkt pkt;
    NvError err = NvSuccess;

    err = NvTioGetHost(&host);

    NV_ASSERT(stream->f.fd > NvTioStdioStream_stdin);

    remote = &host->remote;
    maxsz = sizeof(s_msgBuf)-sizeof(NvTioStdioPktHeader);

    NvTioRemoteSetState(&host->remote, NvTioTargetState_FileWait);

    while (size && !err) {
        size_t sz = (size < maxsz) ? size : maxsz;
        NvTioStdioInitPkt(&pkt, s_msgBuf);
        pkt.pPktHeader->fd = (NvTioStdioStreamFd) stream->f.fd;
        pkt.pPktHeader->cmd = NvTioStdioCmd_fwrite;
        NvTioStdioPktAddData(&pkt, ptr, sz);

        err = NvTioStdioPktSend(remote, &pkt);
        if (err) {
            break;
        }
        size -= sz;
        ptr = (const char*)ptr + sz;
    }
    NV_ASSERT(host->remote.cmdLen == -1);
    NvTioRemoteSetState(&host->remote, NvTioTargetState_Running);

    return DBERR(err);
}

//===========================================================================
// NvTioStdioHostRead() - read stream from host
//===========================================================================
static NvError NvTioStdioHostRead(
            NvTioStreamHandle stream,
            void *ptr,
            size_t size,
            size_t *bytes,
            NvU32 timeout_msec)
{
    //Warning: in some rare scenarios, a read request pakcet gets lost.
    //The current strategy is to resent the lost packet.
    static NvU32 zero_padding[2] = {0};
    static NvU32 sn = 0;  // serial number for each read request packet.
    NvTioHost *host;
    NvError err = NvSuccess;
    NvTioRemote *remote;
    NvTioStdioPktHeader* header;
    size_t maxsz;
    NvTioStdioPkt pkt;
    size_t replySize;
    NvU32  resendCnt = 2;  // in case a packet is lost, resend it.
    NvU8*  pData;
    NvU32  requestSize;
    NvU32  timeout = 10000; // wait until a packet is received. If not resend it

    //
    // In BFM debug mode, wait for 60 minutes before
    // resending a packet.
    //
    if(NvTioGlob.bfmDebug || NvTioGlob.keepRunning) {
        timeout = 60*60*1000; // 60 minuets
        resendCnt = 100;
    } else if (NvTioGlob.readRequestTimeout) {
        timeout = NvTioGlob.readRequestTimeout;
    }

    *bytes = 0;
    err = NvTioGetHost(&host);
    if (err)
        goto done;
    // only allow stdin to read the stream
    NV_ASSERT(stream->f.fd == NvTioStdioStream_stdin);

    maxsz = sizeof(s_msgBuf) - sizeof(NvTioStdioPktHeader);

    if (size > maxsz)
        size = maxsz;

resend:
    remote = &host->remote;
    requestSize = size;
    NvTioStdioInitPkt(&pkt, s_msgBuf);
    pkt.pPktHeader->fd  = (NvTioStdioStreamFd) stream->f.fd;
    pkt.pPktHeader->cmd = NvTioStdioCmd_fread;
    NvTioStdioPktAddData(&pkt, &requestSize, sizeof(requestSize));
    NvTioStdioPktAddData(&pkt, &sn, sizeof(sn));

    // zero padding to reduce the risk of losing a packet
    NvTioStdioPktAddData(&pkt, &zero_padding, sizeof(zero_padding));

    // send the read request
    err = NvTioStdioPktSend(remote, &pkt);

    if (!err) {
        err = NvTioStdioPktWait(remote, timeout);

        header = (NvTioStdioPktHeader*) remote->buf;
        replySize = header->payload;
        pData =  ((NvU8*) remote->buf) + sizeof(NvTioStdioPktHeader);
        if(err) {
            if (err == NvError_Timeout) {
                if (resendCnt--) {
                    NvTioStdioTargetDebugf("Warning @%d, %s: a read request packet might have been lost. resending ...\n",
                                        __LINE__, __FILE__);
                    goto resend;
                } else {
                    err = NvError_FileReadFailed;
                    goto fail;
                }
            } else {
                goto fail;
            }
        }

        // A response packet has been received
    #if NVTIO_STDIO_SENDBACK
        // send back what received
        NvTioStdioInitPkt(&pkt, s_msgBuf);
        NvOsMemcpy(s_msgBuf, remote->buf, replySize + sizeof(NvTioStdioPktHeader));
        pkt.pPktHeader->fd  = NvTioStdioStream_stderr;
        pkt.pPktHeader->cmd = NvTioStdioCmd_fwrite;
        NvTioStdioEraseCommand(&host->remote);   // erase the reply
        NvTioStdioPktSend(remote, &pkt);
    #endif
        if (header->cmd == NvTioStdioCmd_fread_reply &&
            header->fd == NvTioStdioStream_stdin)
        {
            sn++;
            NvOsMemcpy(ptr, pData, replySize);
            *bytes = replySize;
            goto done;
        } else {
            err = NvError_FileReadFailed;
        }
    }

fail:
    NvTioStdioTargetDebugf("Error @%d, %s:  %d (%s)\n", __LINE__, __FILE__, err, NvAuErrorName(err));

done:
    NvTioStdioEraseCommand(&host->remote);   // erase the reply

    return DBERR(err);
}

//===========================================================================
// NvTioStdioTerminateHost() - shut down stdio protocol
//===========================================================================
static void NvTioStdioTerminateHost(NvTioRemote *remote)
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
    // remove stdio streams from list of streams used for stdio
    //
    for (i=0; i<NvTioHostStream_Count; i++) {
        int j, k;
        NvTioMultiStream *ms = &host->stream[i];
        for (j=k=0; j<NV_ARRAY_SIZE(ms->streams); j++) {
            if (ms->streams[j] != &host->gdbtStream[i])
                ms->streams[k++] = ms->streams[j];
        }
        ms->streamCnt = k;
    }

}

//===========================================================================
// NvTioStdioInitHost()  -- Register stdio ops
//                       -- it's ugly, RegisterStdio would have been a better choice
//===========================================================================
NvError NvTioStdioInitHost(NvTioHost *host)
{
    int i;
    static NvTioStreamOps read_ops  = {0};
    static NvTioStreamOps write_ops = {0};

    NV_ASSERT(!host->gdbtStream[NvTioHostStream_stdin].ops);
    NV_ASSERT(!host->gdbtStream[NvTioHostStream_stdout].ops);
    NV_ASSERT(!host->gdbtStream[NvTioHostStream_stderr].ops);

    NvTioGlob.exitFunc = NvTioStdioTargetExit;

    read_ops.sopName    = "StdioHostReadOps";
    read_ops.sopFread   = NvTioStdioHostRead;

    write_ops.sopName   = "StdioHostWriteOps";
    write_ops.sopFwrite = NvTioStdioHostWrite;

    host->remote.terminate = NvTioStdioTerminateHost;

    host->gdbtStream[NvTioHostStream_stdin].magic  = NV_TIO_STREAM_MAGIC;
    host->gdbtStream[NvTioHostStream_stdin].ops    = &read_ops;
    host->gdbtStream[NvTioHostStream_stdin].f.fd   = NvTioStdioStream_stdin;

    host->gdbtStream[NvTioHostStream_stdout].magic = NV_TIO_STREAM_MAGIC;
    host->gdbtStream[NvTioHostStream_stdout].ops   = &write_ops;
    host->gdbtStream[NvTioHostStream_stdout].f.fd  = NvTioStdioStream_stdout;

    host->gdbtStream[NvTioHostStream_stderr].magic = NV_TIO_STREAM_MAGIC;
    host->gdbtStream[NvTioHostStream_stderr].ops   = &write_ops;
    host->gdbtStream[NvTioHostStream_stderr].f.fd  = NvTioStdioStream_stderr;

    for (i=0; i<NvTioHostStream_Count; i++) {
        NvTioMultiStream *ms = &host->stream[i];
        ms->streams[0] = &host->gdbtStream[i];
        ms->streamCnt=1;  // Use one stream only !
    }

    return NvSuccess;
}

//===========================================================================
// NvTioStdioHostFopen - send fopen request to host
//===========================================================================
static NvError
NvTioStdioHostFopen(const char *path, NvU32 flags, NvTioStreamHandle stream)
{
    NvError err = NvSuccess;
    NvU32 zero_padding = 0;
    NvTioHost    *host;
    const char   *stripedPath = (char *)path;
    NvTioStdioPkt pkt;
    NvU32   fh;
    NvTioRemote *remote;
    NvU32   replySize;
    NvTioStdioPktHeader* header;
    NvU8*  pData;

    // wait until a respones packet is received.
    NvU32  timeout = 5000;

    err = NvTioGetHost(&host);
    if (err)
        return DBERR(err);

    // stripped path discriminator
    if (!NvOsStrncmp(path,"host:", 5))
        stripedPath = path + 5;

    remote = &host->remote;
    NvTioStdioInitPkt(&pkt, s_msgBuf);
    pkt.pPktHeader->fd  = NvTioStdioStream_file;
    pkt.pPktHeader->cmd = NvTioStdioCmd_fopen;
    NvTioStdioPktAddData(&pkt, &flags, sizeof(flags));
    NvTioStdioPktAddData(&pkt, stripedPath, NvOsStrlen(stripedPath));

    // zero padding in case that the path doesn't end with 0
    NvTioStdioPktAddData(&pkt, &zero_padding, sizeof(zero_padding));

    // send the read request
    err = NvTioStdioPktSend(remote, &pkt);

    if(err) {
        NvTioStdioTargetDebugf("Error(%s) @%d, %s when "
            "sending the fopen request packet\n",
            NvAuErrorName(err), __LINE__, __FILE__);
        goto done;
    }

    err = NvTioStdioPktWait(remote, timeout);

    if(err) {
        NvTioStdioTargetDebugf("Error %s @%d of %s occured"
            " while waiting for the fopen response packet\n",
            NvAuErrorName(err), __LINE__, __FILE__);
        goto done;
    }

    // A response packet has been received
    header = (NvTioStdioPktHeader*) remote->buf;
    replySize = header->payload;
    pData =  ((NvU8*) remote->buf) + sizeof(NvTioStdioPktHeader);

#if NVTIO_STDIO_SENDBACK
    // send back what received
    NvTioStdioInitPkt(&pkt, s_msgBuf);
    NvOsMemcpy(s_msgBuf, remote->buf,
        replySize + sizeof(NvTioStdioPktHeader));

    pkt.pPktHeader->fd  = NvTioStdioStream_stderr;
    pkt.pPktHeader->cmd = NvTioStdioCmd_fwrite;
    NvTioStdioEraseCommand(remote);   // erase the reply
    NvTioStdioPktSend(remote, &pkt);
#endif
    if (header->cmd == NvTioStdioCmd_fopen_reply &&
        header->fd == NvTioStdioStream_file)
    {
        fh = *((NvU32*)pData);
    } else {
        NvTioStdioTargetDebugf("Error @%d, %s: "
            "Unexpected command received\n",
             __LINE__, __FILE__);
        err = NvError_FileReadFailed;
        goto done;
    }

    stream->f.fd = (int) fh;

done:
    NvTioStdioEraseCommand(remote);   // erase the reply
    return err;
}

//===========================================================================
// NvTioStdioHostFclose - send fclose request to host
//===========================================================================
static void
NvTioStdioHostFclose(NvTioStreamHandle stream)
{
    NvError err = NvSuccess;
    NvTioHost    *host;
    NvTioStdioPkt pkt;
    NvU32   fh;
    NvTioRemote *remote;

    err = NvTioGetHost(&host);
    if(err)
        goto fail;

    fh = (NvU32) stream->f.fd;
    remote = &host->remote;
    NvTioStdioInitPkt(&pkt, s_msgBuf);
    pkt.pPktHeader->fd  = NvTioStdioStream_file;
    pkt.pPktHeader->cmd = NvTioStdioCmd_fclose;
    NvTioStdioPktAddData(&pkt, &fh, sizeof(fh));

    // send the read request
    err = NvTioStdioPktSend(remote, &pkt);

fail:
    if(err) {
        NvTioStdioTargetDebugf("Error(%s) @%d, %s: "
            "sending the fclose request packet failed\n",
            NvAuErrorName(err), __LINE__, __FILE__);
    }
}

//===========================================================================
// NvTioStdioHostFwrite - send fwrite request to host
//===========================================================================
static NvError NvTioStdioHostFwrite(
            NvTioStreamHandle stream,
            const void *ptr,
            size_t size )
{
    NvError err = NvSuccess;
    size_t maxsz;
    NvTioHost    *host;
    NvTioStdioPkt pkt;
    NvU32   fh;
    NvTioRemote *remote;
    NvU32 timeout = 5000; // 5 seconds
    NvU32 replySize;
    NvU8*  pData;
    NvTioStdioPktHeader *header;

    err = NvTioGetHost(&host);
    if (err)
        return DBERR(err);

    fh = (NvU32) stream->f.fd;

    remote = &host->remote;

    maxsz = sizeof(s_msgBuf) - sizeof(NvTioStdioPktHeader)
            - sizeof(fh);

    while (size) {
        size_t sz = (size < maxsz) ? size : maxsz;
        NvTioStdioInitPkt(&pkt, s_msgBuf);
        pkt.pPktHeader->fd = NvTioStdioStream_file;
        pkt.pPktHeader->cmd = NvTioStdioCmd_fwrite;
        NvTioStdioPktAddData(&pkt, &fh, sizeof(fh));
        NvTioStdioPktAddData(&pkt, ptr, sz);

        err = NvTioStdioPktSend(remote, &pkt);
        if (err) {
            goto done;
        }

        // expect an ACK or NACK
        err = NvTioStdioPktWait(remote, timeout);

        header = (NvTioStdioPktHeader*) remote->buf;
        replySize = header->payload;
        pData =  ((NvU8*) remote->buf) + sizeof(NvTioStdioPktHeader);

        if(err)
        {
            NvTioStdioTargetDebugf("Error @%d, %s: failed"
                "to receive an ACK for fwrite()\n",
                 __LINE__, __FILE__);
            goto done;
        }

        // A response packet has been received
    #if NVTIO_STDIO_SENDBACK
        // send back what received
        NvTioStdioInitPkt(&pkt, s_msgBuf);
        NvOsMemcpy(s_msgBuf, remote->buf, replySize + sizeof(NvTioStdioPktHeader));
        pkt.pPktHeader->fd  = NvTioStdioStream_stderr;
        pkt.pPktHeader->cmd = NvTioStdioCmd_fwrite;
        NvTioStdioEraseCommand(&host->remote);   // erase the reply
        NvTioStdioPktSend(remote, &pkt);
    #endif

        NvTioStdioEraseCommand(&host->remote);   // erase the reply
        if (header->cmd ==  NvTioStdioCmd_ack &&
            header->fd == NvTioStdioStream_file)
        {

            size -= sz;
            ptr = (const char*)ptr + sz;
        }
        else if ( header->cmd == NvTioStdioCmd_nack &&
                  header->fd == NvTioStdioStream_file)
        {
            err = NvError_FileWriteFailed;
            NvTioStdioTargetDebugf("Error @%d, %s: NACK received.\n",
                 __LINE__, __FILE__);
            return err;
        } else {
            err = NvError_FileWriteFailed;
            NvTioStdioTargetDebugf("Error @%d, %s: unexpected command\n",
                 __LINE__, __FILE__);
            return err;
        }
    }

done:
    if(err) {
        NvTioStdioTargetDebugf("Error @%d, %s: "
            "sending fwrite packet failed for %s\n",
             __LINE__, __FILE__, NvAuErrorName(err));
    }

    return err;
}

//===========================================================================
// NvTioStdioHostCheckPath - check path
//===========================================================================
static NvError
NvTioStdioHostCheckPath(const char* path)
{
    if (NvOsStrlen(path) < 6)
        return NvError_BadParameter;
    if (!NvOsStrncmp(path, "host:", 5))
        return NvSuccess;
    return NvError_BadParameter;
}

//===========================================================================
// NvTioRegisterFile() - regiter file ops
//===========================================================================
void NvTioRegisterFile()
{
    // Only fopen, fclose, and fwrite are supported
    s_TioStdioHostFileOps.sopName      = "StdioHostFileOps";
    s_TioStdioHostFileOps.sopFopen     = NvTioStdioHostFopen;
    s_TioStdioHostFileOps.sopFwrite    = NvTioStdioHostFwrite;
    s_TioStdioHostFileOps.sopClose     = NvTioStdioHostFclose;
    s_TioStdioHostFileOps.sopCheckPath = NvTioStdioHostCheckPath;

    NvTioRegisterOps(&s_TioStdioHostFileOps);
}

//===========================================================================
// NvTioBreakpoint() - breakpoint loop
//===========================================================================
void NvTioBreakpoint(void)
{ }
