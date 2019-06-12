/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
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

#if NVOS_IS_WINDOWS
#include <winsock2.h>
#endif

#if NV_TIO_DEBUG_ENABLE
#undef DB
#define DB(x) NvOsDebugPrintf x
#endif
//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

static NvTioStreamOps *gs_StreamOps = 0;
NvTioGlobal NvTioGlob = {0};

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioDeinitialize() - unregister each transport implementation
//===========================================================================
void NvTioDeinitialize(void)
{
    if (gs_StreamOps) {
        NvTioStreamOps *ops = gs_StreamOps;
        gs_StreamOps = NV_TIO_VFS_LAST_OPS;
        NvTioUnregisterHost();
        while (ops && ops != NV_TIO_VFS_LAST_OPS) {
            NvTioStreamOps *next = ops->sopNext;
            ops->sopNext = NULL;
            ops = next;
        }
        gs_StreamOps = NULL;
    }
}

//===========================================================================
// NvTioInitialize() - register each transport implementation
//===========================================================================
void NvTioInitialize(void)
{
    if (!gs_StreamOps) {
        NvTioGlob.enableNvosTransport = 0;
        NvTioGlob.enableDebugDump     = NV_TIO_STREAM_DUMP_ENABLE;
        NvTioGlob.dumpFileName        = NV_TIO_DEFAULT_DUMP_NAME;
        NvTioGlob.uartBaud            = NV_TIO_DEFAULT_BAUD;

        gs_StreamOps = NV_TIO_VFS_LAST_OPS;
#ifndef TIO_CACHE_SUPPORTED
        NvTioRegisterFile();
#endif
        NvTioRegisterNvos();
#ifndef BUILD_FOR_COSIM
        NvTioRegisterStdio();
#endif
        NvTioRegisterUart();
#ifndef BUILD_FOR_COSIM
        NvTioRegisterUsb();
        NvTioRegisterTcp();
#endif
        NvTioRegisterHost();
#ifdef TIO_CACHE_SUPPORTED
        NvTioRegisterFileCache();
#endif
    }
}

//===========================================================================
// NvTioRegisterOps() - register a filesystem or transport
//===========================================================================
void NvTioRegisterOps(NvTioStreamOps *ops)
{
    NV_ASSERT(ops && ops != NV_TIO_VFS_LAST_OPS);
    NV_ASSERT(ops->sopName);

    // register default filesystems first
    if (!gs_StreamOps)
        NvTioInitialize();

    if (ops->sopNext)
        return;     // already registered this one

    ops->sopNext = gs_StreamOps;
    gs_StreamOps = ops;
}

//===========================================================================
// NvTioStreamOpen() - open a stream/file/dir/listener
//===========================================================================
NvError NvTioStreamOpen(
            const char *path,
            NvU32 flags,
            NvTioStream *stream)
{
    NvError err = NvError_FileOperationFailed;
    NvTioStreamOps *ops;

    if (!path)
        return NvError_BadParameter;

    if (!gs_StreamOps)
        NvTioInitialize();

    for (ops = gs_StreamOps; ops != NV_TIO_VFS_LAST_OPS; ops = ops->sopNext) {
        NV_ASSERT(ops);

        if (ops->sopCheckPath && ops->sopCheckPath(path))
            continue;

        stream->ops = ops;
        DB(("Attempt to open %s with %s\n", path, ops->sopName));
        switch(stream->magic) {
            case NV_TIO_STREAM_MAGIC:
                err = ops->sopFopen ?
                      ops->sopFopen(path, flags, stream) :
                      NvError_FileOperationFailed;
                break;
            case NV_TIO_LISTEN_MAGIC:
                err = ops->sopListen ?
                      ops->sopListen(path, stream) :
                      NvError_FileOperationFailed;
                break;
            case NV_TIO_DIR_MAGIC:
                err = ops->sopOpendir ?
                      ops->sopOpendir(path, stream) :
                      NvError_FileOperationFailed;
                break;
            default:
                NV_ASSERT(!"Bad stream magic");
                break;
        }
        DB(("%15s %s with %s  returned: 0x%08x=%s\n",
            err ? "FAILED to open" : "Success: opened",
            path,
            ops->sopName,
            err,
            NV_TIO_ERROR_STRING(err)));
        if (!err)
            break;
    }

    return DBERR(err);
}

//===========================================================================
// NvTioStreamCreate() - allocate and open a stream/file/dir/listener
//===========================================================================
NvError NvTioStreamCreate(
                const char *path,
                NvU32 flags,
                NvTioStreamHandle *pStream,
                NvU32 magic)
{
    NvError      err = NvError_FileOperationFailed;
    NvTioStream    *stream;

    NV_ASSERT(pStream);

    stream = NvTioOsAllocStream();
    if (!stream)
        return DBERR(NvError_InsufficientMemory);

    stream->magic = magic;

    err = NvTioStreamOpen(path, flags, stream);
    if (err) {
        NvTioOsFreeStream(stream);
        return DBERR(err);
    }
    *pStream = stream;
    return NvSuccess;
}

//===========================================================================
// NvTioFopen() - open a file or stream
//===========================================================================
NvError NvTioFopen(const char *path, NvU32 flags, NvTioStreamHandle *pStream)
{
    return NvTioStreamCreate(path, flags, pStream, NV_TIO_STREAM_MAGIC);
}

//===========================================================================
// NvTioOpendir() - Open a directory for reading
//===========================================================================
NvError NvTioOpendir(const char *path, NvTioStreamHandle *pDir)
{
    return NvTioStreamCreate(path, 0, pDir, NV_TIO_DIR_MAGIC);
}

//===========================================================================
// NvTioClose() - close a file or stream
//===========================================================================
void NvTioClose(NvTioStreamHandle stream)
{
    if (!stream)
        return;
    NV_ASSERT(stream->magic == NV_TIO_STREAM_MAGIC ||
              stream->magic == NV_TIO_DIR_MAGIC ||
              stream->magic == NV_TIO_LISTEN_MAGIC);
    if (stream->ops->sopClose)
        stream->ops->sopClose(stream);
    stream->magic = 0;
    NvTioOsFreeStream(stream);
}

//===========================================================================
// NvTioStreamType() - returns stream ops name
//===========================================================================
const char *NvTioStreamType(NvTioStreamHandle stream)
{
    if (!stream)
        return "NONE";
    NV_ASSERT(stream->magic == NV_TIO_STREAM_MAGIC ||
              stream->magic == NV_TIO_DIR_MAGIC ||
              stream->magic == NV_TIO_LISTEN_MAGIC);
    NV_ASSERT(stream->ops && stream->ops->sopName);
    return stream->ops->sopName;
}

//===========================================================================
// NvTioFwrite() - write bytes to a file or stream
//===========================================================================
NvError NvTioFwrite(NvTioStreamHandle stream, const void *ptr, size_t size)
{
    NV_ASSERT(stream && stream->magic == NV_TIO_STREAM_MAGIC);
    NV_ASSERT(ptr);

    if (!size)
        return NvSuccess;
    if (stream->ops->sopFwrite)
        return stream->ops->sopFwrite(stream, ptr, size);

    return NvError_NotSupported;
}

//===========================================================================
// NvTioFread() - read bytes from a file or stream
//===========================================================================
NvError NvTioFread(
                NvTioStreamHandle stream,
                void *ptr,
                size_t size,
                size_t *bytes)
{
    size_t len;

    if (!stream || !ptr || !size)
        return DBERR(NvError_BadParameter);
    NV_ASSERT(stream->magic == NV_TIO_STREAM_MAGIC);
    NV_ASSERT(ptr);

    bytes = bytes ? bytes : &len;
    if (stream->ops->sopFread) {
        NvError err = stream->ops->sopFread(
                                    stream,
                                    ptr,
                                    size,
                                    bytes,
                                    NV_WAIT_INFINITE);
        return DBERR(err);
    }

    return DBERR(NvError_NotSupported);
}

//===========================================================================
// NvTioFreadTimeout() - read bytes from a file or stream, with timeout
//===========================================================================
NvError NvTioFreadTimeout(
                NvTioStreamHandle stream,
                void *ptr,
                size_t size,
                size_t *bytes,
                NvU32  timeout_msec)
{
    size_t len;
    NV_ASSERT(stream && stream->magic == NV_TIO_STREAM_MAGIC);
    NV_ASSERT(ptr);

    bytes = bytes ? bytes : &len;
    if (!size) {
        *bytes = 0;
        return NvSuccess;
    }
    if (stream->ops->sopFread) {
        NvError err = stream->ops->sopFread(
                                    stream,
                                    ptr,
                                    size,
                                    bytes,
                                    timeout_msec);
        return DBERR(err);
    }

    return DBERR(NvError_NotSupported);
}

//===========================================================================
// NvTioFseek() - jump to a new location in a file
//===========================================================================
NvError NvTioFseek(NvTioStreamHandle stream, NvS64 offset, NvOsSeekEnum whence)
{
    if (!stream)
        return DBERR(NvError_BadParameter);
    NV_ASSERT(stream->magic == NV_TIO_STREAM_MAGIC);
    NV_ASSERT(whence <= 2);
    if (stream->ops->sopFseek)
        return stream->ops->sopFseek(stream, offset, (NvU32)whence);
    return DBERR(NvError_NotSupported);
}

//===========================================================================
// NvTioFtell() - query location in file
//===========================================================================
NvError NvTioFtell(NvTioStreamHandle stream, NvU64 *position)
{
    if (!stream || !position)
        return DBERR(NvError_BadParameter);
    NV_ASSERT(stream && stream->magic == NV_TIO_STREAM_MAGIC);
    NV_ASSERT(position);
    if (stream->ops->sopFseek)
        return stream->ops->sopFtell(stream, position);
    return DBERR(NvError_NotSupported);
}

//===========================================================================
// NvTioFstat() - find the file type and size
//===========================================================================
NvError NvTioFstat(NvTioStreamHandle stream, NvOsStatType *stat)
{
    NvU64 pos;
    NvError err;
    if (!stream)
        return DBERR(NvError_BadParameter);
    NV_ASSERT(stream->magic == NV_TIO_STREAM_MAGIC);

    // since file is already open, at this stage, we only support regular file
    if ((err = NvTioFtell(stream, &pos)) != NvSuccess)
        return err;
    if ((err = NvTioFseek(stream, 0, NvOsSeek_End)) != NvSuccess)
        return err;
    if ((err = NvTioFtell(stream, &pos)) != NvSuccess)
        return err;
    if ((err = NvTioFseek(stream, (NvS64)pos, NvOsSeek_Set)) != NvSuccess)
        return err;
    stat->type = NvOsFileType_File;
    stat->size = pos;
    return NvSuccess;
}


//===========================================================================
// NvTioFflush() - flush output to file or stream
//===========================================================================
NvError NvTioFflush(NvTioStreamHandle stream)
{
    NV_ASSERT(stream && stream->magic == NV_TIO_STREAM_MAGIC);
    if (stream->ops->sopFflush)
        return stream->ops->sopFflush(stream);
    return DBERR(NvError_NotSupported);
}

//===========================================================================
// NvTioIoctl() - ioctl
//===========================================================================
NvError NvTioIoctl(
    NvTioStreamHandle stream,
    NvU32 IoctlCode,
    void *pBuffer,
    NvU32 InBufferSize,
    NvU32 InOutBufferSize,
    NvU32 OutBufferSize)
{
    NV_ASSERT(stream && stream->magic == NV_TIO_STREAM_MAGIC);
    if (stream->ops->sopIoctl)
        return stream->ops->sopIoctl(
                            stream,
                            IoctlCode,
                            pBuffer,
                            InBufferSize,
                            InOutBufferSize,
                            OutBufferSize);
    return DBERR(NvError_NotSupported);
}

//===========================================================================
// NvTioReaddir() - Read next directory entry
//===========================================================================
NvError NvTioReaddir(NvTioStreamHandle dir, char *path, size_t size)
{
    NV_ASSERT(dir && dir->magic == NV_TIO_DIR_MAGIC);
    NV_ASSERT(path);
    if (dir->ops->sopReaddir)
        return dir->ops->sopReaddir(dir, path, size);
    return DBERR(NvError_BadParameter);
}

//===========================================================================
// NvTioVfprintf() - print to a file or stream
//===========================================================================
NvError NvTioVfprintf(NvTioStreamHandle stream, const char *format, va_list ap)
{
    NvError err = NvError_NotImplemented;
    char *buf;
    NvU32 len;
    NV_ASSERT(stream && stream->magic == NV_TIO_STREAM_MAGIC);

    if (stream->ops->sopVfprintf)
        return stream->ops->sopVfprintf(stream, format, ap);

    //
    // create a string and write it out
    //
    err = NvTioOsAllocVsprintf(&buf, &len, format, ap);
    if (!DBERR(err)) {
        err = NvTioFwrite(stream, buf, len);
        NvTioOsFreeVsprintf(buf, len);
    }

    return DBERR(err);
}

//===========================================================================
// NvTioFprintf() - print to a file or stream
//===========================================================================
NvError NvTioFprintf( NvTioStreamHandle stream, const char *format, ... )
{
    NvError err;
    va_list ap;

    va_start( ap, format );
    err = NvTioVfprintf(stream, format, ap);
    va_end( ap );

    return DBERR(err);
}

//===========================================================================
// NvTioGetNullOps() - get null operations struct
//===========================================================================
NvTioStreamOps *NvTioGetNullOps(void)
{
    static NvTioStreamOps ops = {0};
    return &ops;
}

//===========================================================================
// NvTioGetConfigString() - get string env var or registry value
//===========================================================================
NvError NvTioGetConfigString(const char *name, char *value, NvU32 size)
{
    return NvOsGetConfigString(name, value, size);
}

//===========================================================================
// NvTioGetConfigU32() - get integer env var or registry value
//===========================================================================
NvError NvTioGetConfigU32(const char *name, NvU32 *value)
{
    return NvOsGetConfigU32(name, value);
}

//===========================================================================
// NvTioDebugf() - debug printf
//===========================================================================
void NvTioDebugf(const char *fmt, ... )
{
    va_list ap;

    va_start( ap, fmt );
    NvTioNvosDebugVprintf( fmt, ap );
    va_end( ap );
}

//===========================================================================
// NvTioDebugDumpEnable() - enable/disable debug dumping
//===========================================================================
int NvTioDebugDumpEnable(int enable)
{
    int old;
    if (!gs_StreamOps)
        NvTioInitialize();
    old = NvTioGlob.enableDebugDump;
    NvTioGlob.enableDebugDump = enable ? 1 : 0;
    return old;
}

//===========================================================================
// NvTioDebugDumpFile() - set the name of debug dump file
//===========================================================================
int NvTioDebugDumpFile(const char* name)
{
    if (!gs_StreamOps)
        NvTioInitialize();
    NvTioGlob.dumpFileName = name;
    return 0;
}

//===========================================================================
// NvTioEnableNvosTransport() - enable calling NVOS transport functions
//===========================================================================
int NvTioEnableNvosTransport(int enable)
{
    int oldVal;

    if (!gs_StreamOps)
        NvTioInitialize();

    oldVal = NvTioGlob.enableNvosTransport;

    NvTioGlob.enableNvosTransport = enable;
    return oldVal;
}


//===========================================================================
// NvTioSetUartBaud() - set baudrate for future NvTioFopen("uart:") calls
//===========================================================================
NvU32 NvTioSetUartBaud(NvU32 baud)
{
    int oldVal;

    if (!gs_StreamOps)
        NvTioInitialize();

    oldVal = NvTioGlob.uartBaud;
    if (!oldVal)
        oldVal = NV_TIO_DEFAULT_BAUD;
    if (!baud)
        baud   = NV_TIO_DEFAULT_BAUD;
    NvTioGlob.uartBaud = baud;
    return oldVal;
}

//===========================================================================
// NvTioSetUartBaud() - set baudrate for future NvTioFopen("uart:") calls
//===========================================================================
const char *NvTioSetUartPort(const char *port)
{
    const char *oldVal;

    if (!gs_StreamOps)
        NvTioInitialize();

    oldVal = NvTioGlob.uartPort;

    NvTioGlob.uartPort = port;
    return oldVal;
}

//===========================================================================
// NvTioExit() - exit process
//===========================================================================
void NvTioExit(int status)
{
    NV_ASSERT(NvTioGlob.exitFunc);
    if (NvTioGlob.exitFunc)
        NvTioGlob.exitFunc(status);
}

//===========================================================================
// NvTioSetParamU32() - set an nvtestio parameter of type NvU32
//===========================================================================
NvError NvTioSetParamU32(const char *name, NvU32 value)
{
    if(!NvOsStrcmp(name, "bfmDebug")) {
        NvTioGlob.bfmDebug = value;
        return NvSuccess;
    } else if(!NvOsStrcmp(name, "keepRunning")) {
        NvTioGlob.keepRunning = value;
        return NvSuccess;
    } else if(!NvOsStrcmp(name, "readRequestTimeout")) {
        NvTioGlob.readRequestTimeout = value;
        return NvSuccess;
    }
    return NvError_BadParameter;
}
