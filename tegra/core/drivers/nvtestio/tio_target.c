/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
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

#include "tio_local_target.h"
#include "nvassert.h"
#include "tio_sdram.h"

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

static NvTioHost *gs_Host = 0;

NvTioSdramData NvTioSdramDataFromHost = { 0 };
NvTioBlitPatternData NvTioBlitPatternDataFromHost  = { 0 };
//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioHostOpen() - open stdio stream on target
//===========================================================================
static NvError NvTioHostOpen(
            const char *path,
            NvU32 flags,
            NvTioStreamHandle stream )
{
    if (!NvTioOsStrcmp(path, "stdin:")) {
        stream->f.multi = &gs_Host->stream[NvTioHostStream_stdin];
        return NvSuccess;
    }
    if (!NvTioOsStrcmp(path, "stdout:")) {
        stream->f.multi = &gs_Host->stream[NvTioHostStream_stdout];
        return NvSuccess;
    }
    if (!NvTioOsStrcmp(path, "stderr:")) {
        stream->f.multi = &gs_Host->stream[NvTioHostStream_stderr];
        return NvSuccess;
    }
    if (!NvTioOsStrcmp(path, "hostio:")) {
        NvTioStreamOps *opsSave = stream->ops;
        int port;

        // try to connect on tcp/ip
        for (port=NV_TESTIO_TCP_PORT; port<NV_TESTIO_TCP_PORT+5; port++) {
            char tcpPath[10];
            NvOsSnprintf(tcpPath, sizeof(tcpPath), "tcp:%d", port);
            if (!NvTioStreamOpen(tcpPath, flags, stream))
                return NvSuccess;
        }

        stream->ops = opsSave;
    }
    return NvError_FileOperationFailed;
}

//===========================================================================
// NvTioMultiWrite() - write multi output stream
//===========================================================================
static NvError NvTioMultiWrite(
            NvTioStreamHandle stream,
            const void *ptr,
            size_t size )
{
    NvError err2 = NvSuccess;
    NvError err;
    NvTioMultiStream *ms = stream->f.multi;
    NvU32 i;

    for (i=0; i<ms->streamCnt; i++) {
        err = NvTioFwrite(ms->streams[i], ptr, size);
        err2 = err ? err : err2;
    }
    return DBERR(err2);
}

//===========================================================================
// NvTioMultiRead() - read first stream in multi stream
//===========================================================================
static NvError NvTioMultiRead(
            NvTioStreamHandle stream,
            void *ptr,
            size_t size,
            size_t *bytes,
            NvU32 timeout_msec)
{
    NvError err = NvError_NotSupported;
    NvTioMultiStream *ms = stream->f.multi;

    if (ms->streamCnt > 0)
        err = NvTioFreadTimeout(ms->streams[0], ptr, size, bytes, timeout_msec);
    return DBERR(err);
}

//===========================================================================
// NvTioMultiPollPrep() - prepare to poll first stream in multi-stream
//===========================================================================
static NvError NvTioMultiPollPrep(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc)
{
    NvError err = NvError_NotSupported;
    NvTioMultiStream *ms = tioDesc->stream->f.multi;
    NvTioPollDesc tioDesc2;

    tioDesc2 = *tioDesc;
    tioDesc2.stream = ms->streams[0];
    if (tioDesc2.stream->ops->sopPollPrep &&
        tioDesc2.stream->ops->sopPollCheck) {
        err = tioDesc2.stream->ops->sopPollPrep(&tioDesc2, osDesc);
    }

    return DBERR(err);
}

//===========================================================================
// NvTioMultiPollCheck() - Check result of poll
//===========================================================================
static NvError NvTioMultiPollCheck(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc)
{
    NvError err = NvError_NotSupported;
    NvTioMultiStream *ms = tioDesc->stream->f.multi;
    NvTioPollDesc tioDesc2;

    tioDesc2 = *tioDesc;
    tioDesc2.stream = ms->streams[0];
    if (tioDesc2.stream->ops->sopPollPrep &&
        tioDesc2.stream->ops->sopPollCheck) {
        err = ms->streams[0]->ops->sopPollCheck(&tioDesc2, osDesc);
        if (!err)
            tioDesc->returnedEvents = tioDesc2.returnedEvents;
    }

    return DBERR(err);
}

//===========================================================================
// NvTioHostCheckPath()
//===========================================================================
static NvError NvTioHostCheckPath(const char *path)
{
    if (!NvTioOsStrncmp(path, "stdin:", 6))
        return NvSuccess;
    if (!NvTioOsStrncmp(path, "stdout:", 7))
        return NvSuccess;
    if (!NvTioOsStrncmp(path, "stderr:", 7))
        return NvSuccess;
    if (!NvTioOsStrncmp(path, "hostio:", 7))
        return NvSuccess;
    return DBERR(NvError_BadValue);
}

//===========================================================================
// NvTioUnregisterHost() - unregister IO with host
//===========================================================================
void NvTioUnregisterHost(void)
{
    if (!gs_Host)
        return;

    NvTioClose(gs_Host->stream[NvTioHostStream_stdin].streams[0]);
    NvTioClose(gs_Host->stream[NvTioHostStream_stdout].streams[0]);
    NvTioClose(gs_Host->stream[NvTioHostStream_stderr].streams[0]);

    gs_Host = NULL;
}

//===========================================================================
// NvTioRegisterHost() - register for IO with host
//===========================================================================
void NvTioRegisterHost(void)
{
    static NvTioHost host = {{{0}}};
    static NvTioStreamOps host_ops = {0};

    if (!gs_Host) {
        //
        // stdin from host
        //
        host.stream[NvTioHostStream_stdin].host = &host;
        host.stream[NvTioHostStream_stdin].streamCnt = 0;

        //
        // stdout to host
        //
        host.stream[NvTioHostStream_stdout].host = &host;
        host.stream[NvTioHostStream_stdout].streamCnt = 0;

        //
        // stderr to host
        //
        host.stream[NvTioHostStream_stderr].host = &host;
        host.stream[NvTioHostStream_stderr].streamCnt = 0;

        host_ops.sopName      = "host_ops";
        host_ops.sopFopen     = NvTioHostOpen;
        host_ops.sopCheckPath = NvTioHostCheckPath;
        host_ops.sopFwrite    = NvTioMultiWrite;
        host_ops.sopFread     = NvTioMultiRead;
        host_ops.sopPollPrep  = NvTioMultiPollPrep;
        host_ops.sopPollCheck = NvTioMultiPollCheck;

        NvTioRegisterOps(&host_ops);

        gs_Host = &host;
    }
}

//===========================================================================
// NvTioDisconnectFromHost() - Disconnect from host machine
//===========================================================================
void NvTioDisconnectFromHost(NvTioHostHandle host)
{
    if (host) {
        NV_ASSERT(host == gs_Host);
        NvTioRemoteDisconnect(&host->remote);
        // Close internally created host transport
        NvTioClose(host->ownTransport);
    }
}

//===========================================================================
// NvTioGetHostTransport() - Connect to the host using standard ports
// Use the first connection found.
//===========================================================================
static NvError NvTioGetHostTransport(NvTioTransport transportToUse,
                                            NvTioStreamHandle *pTransport)
{
    NvTioStreamHandle transport;
    NvError err = NvSuccess;
#ifndef BUILD_FOR_COSIM
    int c;
#endif

    NvTioInitialize();
#ifndef BUILD_FOR_COSIM
    if(transportToUse == NvTioTransport_Usb)
    {
        /* Attempt connecting over USB. */
        DB(("Attempting to connect to usb port\n"));
        err = NvTioFopen("usb:",
                         NVOS_OPEN_READ|NVOS_OPEN_WRITE,
                         &transport);
        if (!DBERR(err)) {
          *pTransport = transport;
          return NvSuccess;
        }
        DB(("...Failed to connect to usb port\n"));
    }

    /* Attempt connecting over TCP. */
    for (c='6'; c<='9'; c++) {
      static char tcpAddr[] = "tcp:9876";
      tcpAddr[7] = c;
      DB(("Attempting to connect to '%s'...\n",tcpAddr));
      err = NvTioFopen(
                       tcpAddr,
                       NVOS_OPEN_READ|NVOS_OPEN_WRITE,
                       &transport);
      if (!DBERR(err)) {
        *pTransport = transport;
        return NvSuccess;
      }
    }
    DB(("...Failed to connect to tcp port 9876...9879\n"));
#endif

    /* Attempt connecting over UART. */
    DB(("Attempting to connect to serial port\n"));
    err = NvTioFopen(
                     "uart:",
                     NVOS_OPEN_READ|NVOS_OPEN_WRITE,
                     &transport);

    //
    // The string and the code in NV_TIO_HACK_USE_RELI_DEBUG_CONNECT
    // works for gdbt, but breaks stdio. Removing them works
    // for both gdbt and stdio.
    //
    // err = NvTioFwrite(transport, "C\r\n", 3);

    /*
     * Ensure synchronization with the target by entering & leaving reliable
     * transport mode.
     */
#if NV_TIO_HACK_USE_RELI_DEBUG_CONNECT
    {
      NvU32 reliableTimeout = 2000;

      char buf[1];
      size_t len;
      err = NvTioMakeReliable(transport,reliableTimeout);
      if (!err)
      {
          err = NvTioFwrite(transport, "G", 1);
          err = NvTioFreadTimeout(transport, buf, 1, &len, reliableTimeout);
          if (err) {
             if (err == NvError_Timeout) {
                DB(("...Failed to connect to serial port (Timed out)\n"));
                return DBERR(err);
             }
             if (err != NvError_EndOfFile) {
                DB(("...Failed to connect to serial port (Comm error)\n"));
                return DBERR(err);
             }
          }
          DB(("Established reliable host connection\n"));
      }
      else
          return DBERR(err);
      err = NvTioMakeUnreliable(transport);
      if (err) {
        DB(("...Failed to make connection unreliable\n"));
      } else {
        DB(("Connection is unreliable again.\n"));
      }
    }
#endif

    if (!DBERR(err)) {
      *pTransport = transport;
      return NvSuccess;
    }

    return DBERR(err);
}

//===========================================================================
// NvTioConnectToHost() - Connect to host machine
//===========================================================================
NvError NvTioConnectToHost(
                    NvTioTransport transportToUse,
                    NvTioStreamHandle transport,
                    const char *protocol,
                    NvTioHostHandle *pHost)
{
    static char *buf = 0;
    NvTioHost *host;
    NvError err;

    //
    // protocol may be used later to specify protocols other than GdbTarget
    //
    NV_ASSERT(!protocol);

    if (buf == 0)
        buf = NvOsAlloc(NV_TIO_TARGET_CONNECTION_BUFFER_SIZE);
    if (buf == 0)
        return NvError_InsufficientMemory;

    if (!gs_Host)
        NvTioInitialize();
    host = gs_Host;
    NV_ASSERT(host);
    if (host->remote.state != NvTioTargetState_Disconnected) {
        return DBERR(NvError_AlreadyAllocated);
    }
    host->transportToUse = transportToUse;
    if (!transport) {
        err = NvTioGetHostTransport(transportToUse,&transport);
        if (err)
            return DBERR(err);
        host->ownTransport = transport;
    }

    NV_ASSERT(transport && transport->magic == NV_TIO_STREAM_MAGIC);

    err = NvTioRemoteConnect(
                &host->remote,
                transport,
                buf,
                NV_TIO_TARGET_CONNECTION_BUFFER_SIZE);
    if (err)
        goto fail;

#if NV_TIO_GDBT_SUPPORTED == 1
    err = NvTioGdbtInitHost(host);
#else
    err = NvTioStdioInitHost(host);
#endif //NV_TIO_GDBT_SUPPORTED
    if (err)
        goto fail;

    *pHost = host;
    return NvSuccess;

fail:
    NvTioDisconnectFromHost(host);
    return DBERR(err);
}

//===========================================================================
// NvTioGetHost() - Get host (connect if not connected yet)
//===========================================================================
NvError NvTioGetHost(NvTioHostHandle *pHost)
{
    NvError err = NvSuccess;
    NvTioHostHandle host = gs_Host;

    NV_ASSERT(host);
    if (host->remote.state == NvTioTargetState_Disconnected) {
        err = NvTioConnectToHost(host->transportToUse,0, 0, &host);
    } else if (host->remote.state == NvTioTargetState_RunningEOF) {
        return DBERR(NvError_EndOfFile);
    }
    *pHost = host;
    return DBERR(err);
}

//===========================================================================
// NvTioGetSdramParams() - Get SDRAM parameters from host
//===========================================================================
void
NvTioGetSdramParams(NvU8 **params)
{
    if (params != NULL)
        *params = (NvU8*) &NvTioSdramDataFromHost;
}

//===========================================================================
// NvTioGetBlitPatternParams() - Get Blit Pattern parameters from host
//===========================================================================
void
NvTioGetBlitPatternParams(NvU8 **params)
{
    if (params != NULL)
        *params = (NvU8*) &NvTioBlitPatternDataFromHost;
}
