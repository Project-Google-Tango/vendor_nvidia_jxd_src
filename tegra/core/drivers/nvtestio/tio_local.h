/*
 * Copyright (c) 2005 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_TIO_LOCAL_H
#define INCLUDED_TIO_LOCAL_H

#ifdef __cplusplus
extern "C" {
#endif

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvtestio.h"
#include "nvtestio_ops.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

//
// max protocol (gdbt) command & reply size
//
// NV_TIO_*_CONNECTION_BUFFER_SIZE - size of buffer for accumulating commands
// NV_TIO_MAX_PARMS                - largest # of params in single command
//
#define NV_TIO_TARGET_CONNECTION_BUFFER_SIZE    8192
#define NV_TIO_HOST_CONNECTION_BUFFER_SIZE      100000
#define NV_TIO_MAX_PARMS                        10
#define NV_TIO_CONNECTION_BUFFER_SIZE   8192
#define NV_TIO_MAX_DATA_BUFFER_SIZE     (NV_TIO_TARGET_CONNECTION_BUFFER_SIZE/2 - NV_TIO_MAX_PARMS * 10)

//
// NV_TIO_UART_HW_FLOWCTRL      - enable uart HW flow control pins
// NV_TIO_INFINITE_TIMEOUTS     - wait forever for response (useful in debug)
// NV_TIO_DEBUG_SHOW_TIMEOUT    - set to 0 to suppress printing Timeout errors
// NV_TIO_DEBUG_ENABLE          - print extra debug messages
// NV_TIO_USE_NVRM              - use nvrm features if set
// NV_TIO_GDBT_TIMEOUT          - timeout (ms) before retrying command
// NV_TIO_UART_CONNECT_SEND_CR  - make uart NvTioFopen() send CR char
// NV_TIO_UART_ACCEPT_WAIT_CHAR - make uart NvTioAccept() wait for a char
// NV_TIO_TWO_STOP_BITS         - uart sends 2 stop bits (more reliable)
// NV_TIO_CHAR_DELAY_US         - delay between characters (more reliable)
// NV_TIO_STREAM_DUMP_ENABLE    - dump stream debug info (overridden by
//                                                NvTioDebugDumpEnable());
// NV_TIO_DUMP_CT_ENABLE        - enable dumping code
//
#define NV_TIO_UART_HW_FLOWCTRL         0
#define NV_TIO_INFINITE_TIMEOUTS        1
#define NV_TIO_DEBUG_SHOW_TIMEOUT       0

#define NV_TIO_DEBUG_ENABLE             0
#define NV_TIO_UART_CONNECT_SEND_CR     0   // switch to send "\r\n" at UART open
#define NV_TIO_UART_ACCEPT_WAIT_CHAR    0   // should be 0
#define NV_TIO_USE_NVRM                 0   // set to 1 for pinmux calls

#if 1
#define NV_TIO_TWO_STOP_BITS            0   // for reliability
#define NV_TIO_CHAR_DELAY_US            0   // use 20-80 for slow but reliable
#else
#define NV_TIO_TWO_STOP_BITS            1   // for reliability
#define NV_TIO_CHAR_DELAY_US            80  // use 20-80 for slow but reliable
#endif


#define NV_TIO_STREAM_DUMP_ENABLE       0

#if NVOS_IS_LINUX
#define NV_TIO_DUMP_CT_ENABLE           NV_DEBUG
#elif NVOS_IS_WINDOWS
#define NV_TIO_DUMP_CT_ENABLE           1
#else
#define NV_TIO_DUMP_CT_ENABLE           0
#endif

#define NV_TIO_TCP_LISTEN_BACKLOG       3
#define NV_TIO_DEFAULT_DUMP_NAME        "nvtestio_dump.txt"
#define NV_TIO_DEFAULT_BAUD             57600
#define NV_TIO_GDBT_TIMEOUT             4000
#define NV_TIO_USB_USE_HEADERS          0
#define NV_TIO_USB_MAX_SEGMENT_SIZE     4096


#if NV_TIO_DEBUG_ENABLE
#include "nvapputil.h"
#define NV_TIO_ERROR_STRING(err)    NvAuErrorName(err)
#define DB(x)       NvTioDebugf x
#else
#define DB(x)
#define NV_TIO_ERROR_STRING(err)    "???"
#endif

#if NV_TIO_DUMP_CT_ENABLE
#define DUMPON()    (NvTioGlob.enableDebugDump)
#define DBD(x)      do { if (DUMPON()) { NvTioDebugDumpf x ; }} while(0)
#define DBB(x)      do { DB x ; DBD x ; } while(0)

#define DB_STR(str) \
        do { if (DUMPON()) { \
            NvTioStreamDebug(-4, 0, str, 0); \
        }} while(0)
#define NV_TIO_DBR(fd,ptr,len) \
        do { if (DUMPON()) { \
            NvTioStreamDebug(0,fd,ptr,len); \
        }} while(0)
#define NV_TIO_DBW(fd,ptr,len) \
        do { if (DUMPON()) { \
            NvTioStreamDebug(1,fd,ptr,len); \
        }} while(0)
#define NV_TIO_DBO(fd,name)    \
        do { if (DUMPON()) { \
            NvTioStreamDebug(-1,fd,name,0); \
        }} while(0)
#define NV_TIO_DBP(fd,events)  \
        do { if (DUMPON()) { \
            NvTioStreamDebug(-2,fd,0,events); \
        }} while(0)

#define NvTioRemoteSetState(remote,state) \
        NvTioRemoteSetStateDebug(remote,state,__FILE__,__LINE__)
#else
#define DUMPON()    (0)
#define DBD(x)
#define DBB(x)      DB x

#define DB_STR(str)
#define NV_TIO_DBR(fd,ptr,len)
#define NV_TIO_DBW(fd,ptr,len)
#define NV_TIO_DBO(fd,name)
#define NV_TIO_DBP(fd,events)

#define NvTioRemoteSetState(remote,state) \
        NvTioRemoteSetStateDebug(remote,state,0,0)
#endif

#if NV_TIO_DUMP_CT_ENABLE||NV_TIO_DEBUG_ENABLE
#define DBERR(err)  ((err)?NvTioShowError(err,__FILE__,__LINE__):NvSuccess)
#else
#define DBERR(err)  (err)
#endif

#define NV_TIO_VFS_LAST_OPS    ((NvTioStreamOps*)1)


//
// If enabled, LLDB will send a string back from target to host that you
// can see in nvtestio_dump.txt
//
#define NV_TIO_ENABLE_LLDB 0
#if NV_TIO_ENABLE_LLDB && !NVOS_IS_LINUX
#define LLDB(remote,str)    \
        do { \
            NvTioFwrite(&(remote)->stream, (str), NvTioOsStrlen(str)); \
        } while(0)
#else
#define LLDB(remote,str)
#endif

#ifndef NV_TIO_GDBT_SUPPORTED
#define NV_TIO_GDBT_SUPPORTED 1
#endif
//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

//===========================================================================
// NvTioTcpAddrInfo - virtual stream
//===========================================================================
typedef struct NvTioTcpAddrInfoRec {
    char    addrStr[32];
    char    portStr[16];
    NvU8    addrVal[4];
    NvU16   portVal;
    NvU8    any;
    NvU8    defaultAddr;
    NvU8    defaultPort;
    NvU8    pad[3];
} NvTioTcpAddrInfo;

//===========================================================================
// NvTioStream - virtual stream
//===========================================================================
typedef struct NvTioStreamRec {

    #define NV_TIO_STREAM_MAGIC  0xbeefee89     // file or stream
    #define NV_TIO_DIR_MAGIC     0xbeefee8b     // for directory
    #define NV_TIO_LISTEN_MAGIC  0xbeefee8d     // for listener
    #define NV_TIO_ALLOC_MAGIC   0xbeefee8f     // allocated but unused
    #define NV_TIO_FREE_MAGIC    0              // free (unused)
    NvU32            magic; // set to one of the above NV_TIO_*_MAGIC values

    union {
        unsigned char volatile      *regs;
        void                        *fp;
        int                          fd;
        struct NvTioUsbRec          *usb;
        int                          uartIndex;
        struct NvTioStreamBufferRec *buf;
        struct NvTioRemoteRec       *remote;
        struct NvTioMultiStreamRec  *multi;
        struct NvTioReliRec         *reli;
    } f;

    NvTioStreamOps     *ops;
} NvTioStream;

//===========================================================================
// NvTioTargetState - states that the target can be in
//===========================================================================
typedef enum NvTioTargetStateEnum {
    NvTioTargetState_Disconnected,

    NvTioTargetState_Unknown,
    NvTioTargetState_Running,
    NvTioTargetState_Breakpoint,        // target waiting for command from host
    NvTioTargetState_BreakpointCmd,     // host waiting for reply from target
    NvTioTargetState_FileWait,          // target waiting for reply from host
    NvTioTargetState_FileWaitCmd,       // host waiting for reply from target

    NvTioTargetState_RunningEOF,        // running; got disconnected

    NvTioTargetState_Exit,              // NvTioExit() called

    NvTioTargetState_Count,
    NvTioTargetState_Force32        =   0x7fffffff
} NvTioTargetState;

//===========================================================================
// NvTioRemote - info about a remote host or target
//===========================================================================
typedef struct NvTioRemoteRec {
    NvTioStream    stream;          // internal copy of original stream
    NvTioStream   *oldStream;       // original stream, disabled

    //
    // state of the target/host communication
    //
    NvTioTargetState    state;

    //
    // At any point in time there may be 0 or more "parsed" chars in the
    // input buffer and 0 or more "unparsed" chars in the input buffer.
    //
    // Parsed chars are always part of a command, and if any exist they begin
    // at the start of the buffer, buf[0].  src-1 is the index of the
    // last parsed char. src is -1 if no parsed chars are in the buffer.  If
    // src==0 it means there are no parsed chars, but the next char is
    // part of a command (i.e. we already parsed the $ at the start of the
    // command).
    //
    // Unparsed chars begin at src and end at cnt.
    //
    // NvTioGdbtGetChars()
    // -------------------
    // The NvTioGdbtGetChars() function reads new characters into the buffer.
    // New chars are placed at buf[cnt++].
    //
    // NvTioGdbtCmdCheck()
    // -------------------
    // The NvTioGdbtCmdCheck() function parses characters in the buffer and
    // returns NvSuccess when a complete command (or reply) is available.  In
    // the case that it returns true it has also already sent the '+' ack.  In
    // this case cmdLen will be set to the length of the entire command (not
    // including the $, #, or checksum).  The entire command will begin at
    // buf[0].  (It is an error (assert) to call NvTioGdbtCmdCheck() when
    // cmdLen != -1.
    //
    char        *buf;           // input buffer
    int          cmdLen;        // if ==-1: no command/reply available yet
                                // if >= 0: # of chars in complete command/reply
                                //          (command/reply starts at buf[0])
    int          bufLen;        // space in buffer
    int          cnt;           // # characters currently in buffer
    int          src;           // next unparsed char index
    int          dst;           // bytes in message (so far) (-1 if none)
    NvU8         sum;           // checksum of parsed chars
    NvU8         esc;           // last char was esc char
    NvU8         hup;           // stream hung up
    NvU8         isRunCmd;      // set by NvTioGdbtCmdSend() if cmd is 'c'

    //
    // Call this when destroying the remote connection
    //
    void (*terminate)(struct NvTioRemoteRec *remote);
} NvTioRemote;


//===========================================================================
// NvTioGlobal - global info common to host and target
//===========================================================================
typedef struct NvTioGlobalRec {
    int         isHost;     // 1 if this is the host, 0 if this is a target
    int         enableNvosTransport;
    int         enableDebugDump;
    const char *dumpFileName;
    NvU32       uartBaud;
    const char *uartPort;
    void        (*exitFunc)(int status);
    NvU32       bfmDebug;
    NvU32       keepRunning;
    NvU32       readRequestTimeout; // in ms
} NvTioGlobal;

//===========================================================================
// NvTioUsbMsgHeader
//===========================================================================
#if NV_TIO_USB_USE_HEADERS
#if 1   // allow varying segment sizes
#define NV_TIO_USB_SMALL_SEGMENT_SIZE    64
#define NV_TIO_USB_LARGE_SEGMENT_SIZE    NV_TIO_USB_MAX_SEGMENT_SIZE
#else
#define NV_TIO_USB_SMALL_SEGMENT_SIZE    64
#define NV_TIO_USB_LARGE_SEGMENT_SIZE    64
#endif

typedef struct NvTioUsbMsgHeaderRec
{
    NvU8    isDebug;            // Does the packed hold debug message? The
                                // message must be zero terminated string
    NvU8    isNextSegmentSmall;
    NvU16   payload;            // Current payload length (can be shorter then
                                // segment size)
} NvTioUsbMsgHeader;

#define NV_TIO_USB_SMALL_SEGMENT_PAYLOAD  \
    (NV_TIO_USB_SMALL_SEGMENT_SIZE-sizeof(NvTioUsbMsgHeader))
#define NV_TIO_USB_LARGE_SEGMENT_PAYLOAD  \
    (NV_TIO_USB_LARGE_SEGMENT_SIZE-sizeof(NvTioUsbMsgHeader))

#endif // NV_TIO_USB_USE_HEADERS

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

//===========================================================================
// Generic remote connection handling
//===========================================================================
NvError NvTioRemoteConnect(
                    NvTioRemote *remote,
                    NvTioStream *connection,
                    void        *buf,
                    NvU32        bufSize);
void NvTioRemoteDisconnect(NvTioRemote *remote);

//===========================================================================
// functions to register/unregister various transports and filesystems
//===========================================================================
void NvTioRegisterNvos(void);
void NvTioRegisterStdio(void);
void NvTioRegisterUart(void);
void NvTioRegisterUsb(void);
void NvTioRegisterTcp(void);
void NvTioRegisterKitl(void);
void NvTioRegisterHost(void);
void NvTioRegisterFile(void);
void NvTioRegisterFileCache(void);

void NvTioUnregisterHost(void);

// get null operations struct
NvTioStreamOps *NvTioGetNullOps(void);

//===========================================================================
// vfs stuff
//===========================================================================

// open a stream/file/dir/listener
NvError NvTioStreamOpen(
            const char *path,
            NvU32 flags,
            NvTioStream *stream);

// create and open a stream/file/dir/listener
NvError NvTioStreamCreate(
            const char *path,
            NvU32 flags,
            NvTioStreamHandle *pStream,
            NvU32 magic);

//===========================================================================
// Utility functions
//===========================================================================
NvU32 NvTioCalcTimeout(NvU32 timeout, NvU32 startTime, NvU32 max);
NvError NvTioParseIpAddr(const char *name, int any, NvTioTcpAddrInfo *info);

//===========================================================================
// Debug functions
//===========================================================================

// show bytes which may or may not be ASCII
void NvTioShowBytes(const NvU8 *ptr, NvU32 len);

// Show error if debug enabled.  Returns err. Do nothing if err==0
NvError NvTioShowError(
                    NvError err,
                    const char *file,
                    int line);

// Debugging
void NvTioStreamDebug(
                    int isWrite,
                    int fd,
                    const void *ptr,
                    size_t len);
void NvTioRemoteSetStateDebug(
            NvTioRemote *remote,
            NvTioTargetState state,
            const char *file,
            int line);
void NvTioDebugDumpf(const char *fmt, ...);

//===========================================================================
// functions supplied by tio_nvos.c or tio_null_nvos.c
//===========================================================================

// Allocate a zeroed NvTioStream structure
NvTioStream *NvTioOsAllocStream(void);

// Free a NvTioStream structure
void         NvTioOsFreeStream(NvTioStream *stream);

// snprintf
NvS32 NvTioOsSnprintf(
                    char  *buf,
                    size_t len,
                    const char *format,
                    ...);

// vsnprintf
NvS32 NvTioOsVsnprintf(
                    char  *buf,
                    size_t len,
                    const char *format,
                    va_list ap);

// strlen
size_t NvTioOsStrlen(const char *s);

// sprintf to an allocated buffer
NvError      NvTioOsAllocVsprintf(
                    char  **buf,
                    NvU32  *len,
                    const char *format,
                    va_list ap);

// free buffer from NvTioOsAllocVsprintf()
void         NvTioOsFreeVsprintf(
                    char  *buf,
                    NvU32  len);

NvError NvTioNvosDebugVprintf(
                const char *format,
                va_list ap);

// strncmp
int          NvTioOsStrcmp(const void *s1, const void *s2);
int          NvTioOsStrncmp(const void *s1, const void *s2, size_t size);

// get env var or registry value
NvError NvTioNvosGetConfigString(const char *name, char *value, NvU32 size);
NvError NvTioNvosGetConfigU32(const char *name, NvU32 *value);

NvU32 NvTioOsGetTimeMS(void);


//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

extern NvTioGlobal NvTioGlob;

#ifdef __cplusplus
}
#endif

#endif // INCLUDED_TIO_LOCAL_H
