/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TESTIO_H
#define INCLUDED_TESTIO_H

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvcommon.h"
#include "nvos.h"
#include <stdarg.h>

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

// default port for listening and connecting over TCP/IP
#define NV_TESTIO_TCP_PORT      9877

// max number of descriptors in a call to NvTioPoll() call
#define NV_TIO_POLL_MAX         30

//
// max sizes of target command arguments
//
// NV_TIO_TARGET_ARG_MAXBYTES - Total characters & nulls for all arguments
// NV_TIO_TARGET_ARG_MAXCOUNT - Total number of arguments
//
#define  NV_TIO_TARGET_ARG_MAXBYTES 512
#define  NV_TIO_TARGET_ARG_MAXCOUNT  64

// TEMPORARY HACK DEFINES
#define NV_TIO_HACK_USE_RELI_BOOT             0
#define NV_TIO_HACK_USE_RELI_DEBUG_CONNECT    0

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

//===========================================================================
// NvTioHostHandle - handle representing a host system (only used on target)
//===========================================================================
typedef struct NvTioHostRec *NvTioHostHandle;

//===========================================================================
// NvTioTargetHandle - handle representing a target system (only used on host)
//===========================================================================
typedef struct NvTioTargetRec *NvTioTargetHandle;

//===========================================================================
// NvTioStreamHandle - handle of a stream or file
//===========================================================================
typedef struct NvTioStreamRec *NvTioStreamHandle;

//===========================================================================
// NvTrnPollEvent - events that can be polled
//===========================================================================
typedef enum {
    /// stream has data that can be read without blocking
    NvTrnPollEvent_In              =   0x00000001,

    NvTrnPollEvent_Force32         =   0x7fffffff
} NvTrnPollEvent;

//===========================================================================
// NvTioTransport - Default transport for target
//===========================================================================
typedef enum
{
    NvTioTransport_Usb,
    NvTioTransport_Uart
}NvTioTransport;

/*=========================================================================*/
/** descriptor for NvTrnPoll()
 */
typedef struct NvTioPollDescRec {
    NvTioStreamHandle   stream;
    NvU32               requestedEvents;    /// from NvGdbhPollEvent enum
    NvU32               returnedEvents;     /// from NvGdbhPollEvent enum
} NvTioPollDesc;

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################


/*-------------------------------------------------------------------------*/
/* NvTioInitialize() - initialize testio
 *
 * Call this before calling any other NvTio functions.
 */
void NvTioInitialize(void);

/*-------------------------------------------------------------------------*/
/* NvTioDeinitialize() - deinitialize testio
 *
 * Call this after finishing to use NvTio functions.
 */
void NvTioDeinitialize(void);

/*-------------------------------------------------------------------------*/
/** NvTiFopen - open a file or stream.
 *
 *  @param path The path to the file or address of the stream
 *  @param flags Or'd flags for the open operation (NVOS_OPEN_*)
 *  @param stream The stream that will be opened, if successful (out param)
 *
 *  Example path names:
 *      "stdin:"     = standard input stream
 *      "stdout:"    = standard output stream
 *      "stderr:"    = standard error stream
 *      "command:"   = command stream
 *      "uart:<p>"   = serial port <p>
 *      "xio:<s>"    = XIO stream <s>
 *      "usb:<s>"    = USB stream <s>
 *      "tcp:<a>:<p> = TCP/IP IP address <a> port <p>
 *      "flash:<f>   = file <f> in flash filesystem
 *      "sd:<f>      = file <f> in sd filesystem
 *      "host:<f>    = file <f> on host
 *      "<f>"        = file <f> on host
 *
 *  If the NVOS_OPEN_CREATE flag is specified, NVOS_OPEN_WRITE must also
 *  be specified.
 *
 *  If NVOS_OPEN_WRITE is specified the file will be opened for write and
 *  will be truncated if it was previously existing.
 *
 *  If NVOS_OPEN_WRITE and NVOS_OPEN_READ is specified the file will not
 *  be truncated.
 */
NvError
NvTioFopen(const char *path, NvU32 flags, NvTioStreamHandle *stream);

/*-------------------------------------------------------------------------*/
/** NvTioMakeReliable - place stream in reliable mode.
 *
 *  Both ends of the stream must be placed in reliable mode before either end
 *  will work.  Once in reliable mode all communication is retried until it
 *  succeeds (or forever).
 *
 *  @param stream - the stream to place into reliable mode
 *  @param timeout_ms - how long to wait for the other end to become reliable
 *                          before retrning NvError_Timeout
 */
NvError NvTioMakeReliable(NvTioStreamHandle stream, NvU32 timeout_ms);

/*-------------------------------------------------------------------------*/
/** NvTioMakeUnreliable - place reliable stream in regular mode.
 *
 *  Both ends of the stream must be in reliable mode first.  Then both must be
 *  placed in unreliable mode.
 *
 *  @param stream - the reliable stream to place into regular mode
 */
NvError NvTioMakeUnreliable(NvTioStreamHandle stream);

/*-------------------------------------------------------------------------*/
/** NvTioClose - close a file, stream, listener, or directory.
 *
 *  @param stream The file or stream to close
 *
 *  Passing in a null handle is ok.
 */
void NvTioClose(NvTioStreamHandle stream);

/*-------------------------------------------------------------------------*/
/** NvTioFwrite - write to a file or stream.
 *
 *  @param stream The file or stream
 *  @param ptr The data to write
 *  @param size The length of the write
 *
 *  NvError_FileWriteFailed is returned on error.
 */
NvError
NvTioFwrite(NvTioStreamHandle stream, const void *ptr, size_t size);

/*-------------------------------------------------------------------------*/
/** NvTioFread - read a file or stream.
 *
 *  @param stream The file or stream
 *  @param ptr The buffer for the read data
 *  @param size The length of the read
 *  @param bytes The number of bytes read (out param) - may be null
 *
 *  This returns an NvError_FileReadFailed if the read encountered any
 *  system errors.
 *
 *  To detect short reads (less that specified amount), pass in 'bytes'
 *  and check its value to the expected value.  The 'bytes' parameter may
 *  be null.
 */
NvError
NvTioFread(
        NvTioStreamHandle stream,
        void *ptr,
        size_t size,
        size_t *bytes);

/*-------------------------------------------------------------------------*/
/** NvTioFreadTimeout - read a file or stream with a timeout.
 *
 *  @param stream The file or stream
 *  @param ptr The buffer for the read data
 *  @param size The length of the read
 *  @param bytes The number of bytes read (out param) - may be null
 *  @param timout_msec number of msec to wait before returning NvError_Timeout
 *
 *  This returns an NvError_FileReadFailed if the read encountered any
 *  system errors.
 *
 *  To detect short reads (less that specified amount), pass in 'bytes'
 *  and check its value to the expected value.  The 'bytes' parameter may
 *  be null.
 */
NvError
NvTioFreadTimeout(
        NvTioStreamHandle stream,
        void *ptr,
        size_t size,
        size_t *bytes,
        NvU32 timeout_msec);

/*-------------------------------------------------------------------------*/
/** NvTioFseek - change the file position pointer.
 *
 *  @param file The file
 *  @param offset The offset from whence
 *  @param whence The starting point for the seek - from NvOsSeekEnum
 *
 *  This return NvError_FileOperationFailed on error.
 */
NvError
NvTioFseek(NvTioStreamHandle file, NvS64 offset, NvOsSeekEnum whence);

/*-------------------------------------------------------------------------*/
/** NvTioFtell - get the current file position pointer.
 *
 *  @param file The file
 *  @param position The file position (out param)
 *
 *  This return NvError_FileOperationFailed on error.
 */
NvError
NvTioFtell(NvTioStreamHandle file, NvU64 *position);

/*-------------------------------------------------------------------------*/
NvError
NvTioFstat(NvTioStreamHandle stream, NvOsStatType *stat);

/*-------------------------------------------------------------------------*/
/** NvTioFflush - flush any pending writes to the file stream.
 *
 *  @param stream The file stream
 */
NvError
NvTioFflush(NvTioStreamHandle stream);

/*-------------------------------------------------------------------------*/
/**
 * Thunk into the device driver implementing this stream (usually a device file)
 * to perform an "IO control" operation.
 *
 * @param stream The stream to perform the IO control operation on
 * @param IoctlCode The IO control code (which operation to perform)
 * @param pBuffer The buffer containing the data for the IO control operation.
 *     This buffer must first consist of InBufferSize bytes of input-only data,
 *     followed by InOutBufferSize bytes of input/output data, and finally
 *     OutBufferSize bytes of output-only data.  Its total size is therefore
 *     InBufferSize + InOutBufferSize + OutBufferSize bytes.
 * @param InBufferSize The number of input-only data bytes in the buffer.
 * @param InOutBufferSize The number of input/output data bytes in the buffer.
 * @param OutBufferSize The number of output-only data bytes in the buffer.
 */
NvError
NvTioIoctl(
    NvTioStreamHandle stream,
    NvU32 IoctlCode,
    void *pBuffer,
    NvU32 InBufferSize,
    NvU32 InOutBufferSize,
    NvU32 OutBufferSize);

/*-------------------------------------------------------------------------*/
/** NvTioOpendir - open a directory.
 *
 *  @param path The path of the directory to open
 *  @param dir The directory that will be opened, if successful (out param)
 *              (call NvTioClose() when done with it)
 *
 *  NvError_DirOperationFailed will be returned upon failure.
 */
NvError
NvTioOpendir(const char *path, NvTioStreamHandle *dir);

/*-------------------------------------------------------------------------*/
/** NvTioReaddir - gets the next entry in the directory.
 *
 *  @param dir The directory pointer
 *  @param name The name of the next file (out param)
 *  @param size The size of the name buffer
 *
 *  This return NvError_EndOfDirList when there are no more entries in the
 *  directory, or NvError_DirOperationFailed if there is a system error.
 */
NvError
NvTioReaddir(NvTioStreamHandle dir, char *name, size_t size);

/*-------------------------------------------------------------------------*/
/** NvTioFprintf - print a string to a file or stream.
 *
 *  @param stream The file or stream to print to
 *  @param format The format string
 */
NvError
NvTioFprintf( NvTioStreamHandle stream, const char *format, ... );

/*-------------------------------------------------------------------------*/
/** NvTioVfprintf - print a string to a file or stream using a va_list.
 *
 *  @param stream The file stream
 *  @param format The format string
 *  @param ap The va_list structure
 */
NvError
NvTioVfprintf(NvTioStreamHandle stream, const char *format, va_list ap);

/*-------------------------------------------------------------------------*/
/** NvTioStreamType - get string indicating type of stream
*
*  @param stream - the stream/file/dir in question
*
*  @returns a static string indicating what type of stream (for debugging)
*/
const char *
NvTioStreamType(NvTioStreamHandle stream);

/*-------------------------------------------------------------------------*/
/** NvTioEnableNvosTransport - enable/disable transport through NVOS
 *
 *  @param enable - 1 to enable, 0 to disable
 *  @returns old value of enable
 */
int NvTioEnableNvosTransport(int enable);

/*-------------------------------------------------------------------------*/
/** NvTioGetConfigU32 - retrives an unsigned integer variable from the
 *      environment.
 *
 *  @param name The name of the variable
 *  @param value The value to write (out param)
 *
 *  This returns NvError_ConfigVarNotFound if the name isn't found in the
 *  environment.  If the configuration variable cannot be converted into
 *  an unsiged integer, then NvError_InvalidConfigVar will be returned.
 */
NvError
NvTioGetConfigU32(const char *name, NvU32 *value);

/*-------------------------------------------------------------------------*/
/** NvTioGetConfigString - retreives a string variable from the
 *      environment.
 *
 *  @param name The name of the variable
 *  @param value The value to write into
 *  @param size The size of the value buffer
 *
 *  This returns NvError_ConfigVarNotFound if the name isn't found in the
 *  environment.
 */
NvError
NvTioGetConfigString(const char *name, char *value, NvU32 size);

/*-------------------------------------------------------------------------*/
/** NvTioDebugf - print debug output
 *
 *  @param format The format string
 */
void
NvTioDebugf(const char *format, ... );

/*-------------------------------------------------------------------------*/
/** NvTioDebugDumpEnable - enable/disable dumping to debug file
 *
 *  @param enable - disable (if 0) or enable (if nonzero) dumping to dump
 *                       file (see #NvTioDebugDumpFile).
 *  @returns old value of enable
 */
int
NvTioDebugDumpEnable(int enable);

/*-------------------------------------------------------------------------*/
/** NvTioDebugDumpFile - set the name of debug dump file
*
*  @param name - name of the file (default is nvtestio_dump.txt file).
*
*  @returns 1 on success, 0 if the old file is already open
*/
int
NvTioDebugDumpFile(const char* name);



//###########################################################################
//############################### TARGET ONLY PROTOTYPES ####################
//###########################################################################

/*-------------------------------------------------------------------------*/
/** NvTioConnectToHost - connect to host over given stream
 *  THIS IS ONLY AVAILABLE FOR TARGET PROGRAMS
 *
 * Only call this from programs that run on the target machine
 *
 *  @param transport - transport to be used
 *  @param connection - an existing stream connection
 *  @param protocol   - for now set this to NULL
 *  @param host       - a handle representing the host is returned here
 *
 */
NvError
NvTioConnectToHost(
            NvTioTransport  transport,
            NvTioStreamHandle connection,
            const char *protocol,
            NvTioHostHandle *host);

/*-------------------------------------------------------------------------*/
/** NvTioGetSdramParams - Get sdram parameters passed from the host.
 *  THIS IS ONLY AVAILABLE FOR TARGET PROGRAMS
 *
 *  @param params - pointer to the data
 */
void
NvTioGetSdramParams(NvU8 **params);

/*-------------------------------------------------------------------------*/
/** NvTioGetBlitPatternParams - Get pattern parameters passed from the host.
 *  THIS IS ONLY AVAILABLE FOR TARGET PROGRAMS
 *
 *  @param params - pointer to the data
 */
void
NvTioGetBlitPatternParams(NvU8 **params);

/*-------------------------------------------------------------------------*/
/** NvTioDisconnectFromHost - disconnect from host
 *  THIS IS ONLY AVAILABLE FOR TARGET PROGRAMS
 *
 *  @param host - handle returned by NvTioConnectToHost
 */
void
NvTioDisconnectFromHost(NvTioHostHandle host);

/*-------------------------------------------------------------------------*/
/** NvTioBreakpoint - hit a breakpoint
 *  THIS IS ONLY AVAILABLE FOR TARGET PROGRAMS
 *
 * Only call this from programs that run on the target machine
 */
void
NvTioBreakpoint(void);

/*-------------------------------------------------------------------------*/
/** NvTioExit - exit a process
 *  THIS IS ONLY AVAILABLE FOR TARGET PROGRAMS
 *
 *  @param status       - exit status of process
 *
 * Only call this from programs that run on the target machine
 */
void
NvTioExit(int status);

//###########################################################################
//############################### HOST ONLY PROTOTYPES ######################
//###########################################################################

/*-------------------------------------------------------------------------*/
/** NvTioPoll - Poll for events
 *  THIS IS ONLY AVAILABLE FOR HOST PROGRAMS
 *
 *  @param list         - list of descriptors
 *  @param cnt          - number of descriptors in list
 *  @param timeout_msec - timeout in milliseconds (NV_WAIT_INFINITE = forever)
 *
 */
NvError
NvTioPoll(NvTioPollDesc *list, NvU32 cnt, NvU32 timeout_msec);

/*-------------------------------------------------------------------------*/
/** NvTioSetUartBaud - set baud rate for future connections
 *
 *  @param baud - baud rate to use for new listen/Fopen calls
 *
 *  @returns old value
 *
 */
NvU32 NvTioSetUartBaud(NvU32 baud);

/*-------------------------------------------------------------------------*/
/** NvTioSetUartPort - set baud rate for future connections
 *
 *  @param port - static string with path of uart device file (e.g.
 *                              "/dev/ttyUSB0")
 *
 *  @returns old value
 *
 */
const char *NvTioSetUartPort(const char *port);

/*-------------------------------------------------------------------------*/
/** NvTiTargetFopen - open a file or stream to a specific target.
 *  THIS IS ONLY AVAILABLE FOR HOST PROGRAMS
 *
 *  @param target - a connected target
 *  @param path   - The name of the stream or file on the target
 *  @param stream - The stream that will be opened, if successful (out param)
 *
 *  Example path names:
 *      "stdin:"     = standard input stream to target
 *      "stdout:"    = standard output stream from target
 *      "stderr:"    = standard error stream from target
 *      "cmd:"       = commands
 *      "reply:"     = replies to commands
 */
NvError
NvTioTargetFopen(
        NvTioTargetHandle target,
        const char *path,
        NvTioStreamHandle *stream);

/*-------------------------------------------------------------------------*/
/** NvTioListen - Create a stream handle for listening for connections.
 *  THIS IS ONLY AVAILABLE FOR HOST PROGRAMS
 *
 *  Call this to listen for connections from UART or TCP.
 *  Call NvTioAccept to accept connections and NvTioClose to close the listening
 *  stream handle.
 *
 *  @param addr the address to listen to
 *  @param listener The listening stream handle is returned here (out param)
 */
NvError
NvTioListen(const char *addr, NvTioStreamHandle *listener);

/*-------------------------------------------------------------------------*/
/** NvTioAcceptTimeout - Accept a connection.
 *  THIS IS ONLY AVAILABLE FOR HOST PROGRAMS
 *
 *  @param listener The listening stream handle (from NvTioListen())
 *  @param stream The connected stream is returned here (out param)
 *  @param timeoutMS The timeout in milliseconds
 *
 */
NvError
NvTioAcceptTimeout(
        NvTioStreamHandle listener,
        NvTioStreamHandle *stream,
        NvU32 timeoutMS);

/*-------------------------------------------------------------------------*/
/** NvTioAccept - Accept a connection.
 *  THIS IS ONLY AVAILABLE FOR HOST PROGRAMS
 *
 *  Same as NvTioAcceptTimeout() with timeoutMS = NV_WAIT_INFINITE.
 *
 *  @param listener The listening stream handle (from NvTioListen())
 *  @param stream The connected stream is returned here (out param)
 */
NvError
NvTioAccept(NvTioStreamHandle listener, NvTioStreamHandle *stream);

/*-------------------------------------------------------------------------*/
/** NvTioConnectToTarget - connect to target over given stream
 *  THIS IS ONLY AVAILABLE FOR HOST PROGRAMS
 *
 *  @param connection   - an existing stream connection
 *  @param protocol     - for now set this to NULL
 *  @param target       - a handle representing the target is returned here
 *
 */
NvError
NvTioConnectToTarget(
            NvTioStreamHandle connection,
            const char *protocol,
            NvTioTargetHandle *target);

/*-------------------------------------------------------------------------*/
/** NvTioDisconnectFromTarget - disconnect from target
 *  THIS IS ONLY AVAILABLE FOR HOST PROGRAMS
 *
 *  @param target - handle returned by NvTioConnectToTarget
 */
void
NvTioDisconnectFromTarget(NvTioTargetHandle target);

/*-------------------------------------------------------------------------*/
/** NvTioSetFileCacheHooks - use file cache in place of nvos file
 *  THIS IS ONLY AVAILABLE FOR TARGET PROGRAMS
 */
void
NvTioSetFileCacheHooks(void);

/*-------------------------------------------------------------------------*/
/** NvTioSetFileHostHooks - use host file I/O in place of nvos file
 *  THIS IS ONLY AVAILABLE FOR TARGET PROGRAMS
 */
void
NvTioSetFileHostHooks(void);

/*-------------------------------------------------------------------------*/
/** NvTioSetParamU32 - set an nvtestio parameter of type NvU32
 *
 *  @param name - right now, only supports "bfmDebug"
 *  @param value - value for the parameter
 *
 *  @returns NvSucess or NvError_BadParameter
 *
 */
NvError
NvTioSetParamU32(const char *name, NvU32 value);
#endif // INCLUDED_TESTIO_H
