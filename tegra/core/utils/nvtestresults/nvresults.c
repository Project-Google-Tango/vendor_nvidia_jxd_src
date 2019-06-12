/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvtestio.h"
#include "nvassert.h"
#include "nvutil.h"
#include "nvtest.h"

#if NVOS_IS_WINDOWS || NVOS_IS_LINUX || NVOS_IS_QNX
#include <stdio.h>
#endif

#if NVOS_IS_LINUX
#include <fcntl.h>
#include <unistd.h>
#endif

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

#define DBERR(err) (err)
#define MAX_SUBTEST_NAME_LENGTH 80

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

//===========================================================================
// NvTestAppState - states that an application can be in
//===========================================================================
typedef enum NvTestAppStateEnum {
    NvTestAppState_Uninitialized,
    NvTestAppState_Initialized,
    NvTestAppState_RanTests,
    NvTestAppState_Terminated,

    NvTestAppState_Count,
    NvTestAppState_Force32          =   0x7fffffff
} NvTestAppState;

//===========================================================================
// NvTestSubtest - info about 1 test
//===========================================================================
typedef struct NvTestSubtestRec {
    struct NvTestAppRec *app;
    char                *name;      // name of test
    NvError              result;
    NvU32                index;     // test index
    NvU32                startTime;
    NvBool               skip;
} NvTestSubtest;

//===========================================================================
// NvTestApp - Global information about an application
//===========================================================================
typedef struct NvTestAppRec {
    NvU32           useCount;      // # of times NvTestInitialize() called
    NvTestAppState  state;         // state of the app

    NvU32           testCnt;       // number of tests run so far
    NvU32           failureCnt;    // number of tests that failed so far
    NvU32           filteredCnt;   // Number of tests filtered out
    NvError         err;           // return value of program

    NvU32           tlsIndex;      // per-thread data index (current subtest)
    NvOsMutexHandle mutex;         // mutex for thread accesses to NvTestApp

    NvTestSubtest  *failHead;      // head of failure list
    NvTestSubtest **failTail;      // tail of failure list

    void          (*nvRun)(NvU32); // Points to NvRun() function

    int enableNvosTransport;        // backup of original setting
    NvTioHostHandle   host;         // handle to connected host
    NvTioStreamHandle hostStdin;    // stdin from host
    NvTioStreamHandle hostStdout;   // stdout to host
    char *stdinBuf;                 // buffered stdin characters
    NvU32 stdinBufCnt;              // chars in stdinBuf
    NvU32 stdinBufLen;              // size of stdinBuf

    char *subtestFilter;            // Filter for subtests
    char *subtestList;              // subtest list from sanity harness

    NvBool rest;                    // Whether to print results in REST format
} NvTestApp;

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

static NvTestApp *gs_app=0;

#if NVOS_IS_LINUX
static int marker_fd = -1;
#endif

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTestDisconnectFromHost() - return true if we are booting from UART
//===========================================================================
static NvError NvTestDisconnectFromHost(NvTioHostHandle host)
{
    if (NVOS_IS_WINDOWS)
        return NvError_NotSupported;
    NvTioEnableNvosTransport(gs_app->enableNvosTransport);
    return NvSuccess;
}

//===========================================================================
// NvTestConnectToHost() - return true if we are booting from UART
//===========================================================================
static NvError NvTestConnectToHost(NvTioHostHandle *returned_host,
                                           int argc,
                                           char *argv[])
{
    NvError err;
    int enableNvosTransport,i;
    NvTioTransport transportToUse = NvTioTransport_Usb;
    if (NVOS_IS_WINDOWS)
        return NvError_NotSupported;

    for (i=1; i<argc; i++)
    {
        if ((NvOsStrlen(argv[i]) == 5) && !NvOsStrncmp(argv[i], "#uart", 5)) {
            transportToUse = NvTioTransport_Uart;
        }
    }

    enableNvosTransport = NvTioEnableNvosTransport(0);
    err = NvTioConnectToHost(transportToUse, 0, 0, returned_host);
    if (err) {
        *returned_host = 0;
        NvTioEnableNvosTransport(enableNvosTransport);
    } else {
        gs_app->enableNvosTransport = enableNvosTransport;
        NvTioBreakpoint();
    }
    return err;
}

//===========================================================================
// NvTestGetCurSubtest() - return the current subtest for this thread
//===========================================================================
static NvTestSubtest* NvTestGetCurSubtest(void)
{
    NV_ASSERT(gs_app && gs_app->tlsIndex != NVOS_INVALID_TLS_INDEX);

    return NvOsTlsGet(gs_app->tlsIndex);
}

//===========================================================================
// NvTestSetCurSubtest() - set the current subtest for this thread
//===========================================================================
static void NvTestSetCurSubtest(NvTestSubtest* sub)
{
    NV_ASSERT(gs_app && gs_app->tlsIndex != NVOS_INVALID_TLS_INDEX);

    NvOsTlsSet(gs_app->tlsIndex, sub);
}

#if NVOS_IS_LINUX

//===========================================================================
// NvTestOpenFTrace() - Open ftrace for writing.
//===========================================================================
static void NvTestOpenFTrace(void)
{
    if(marker_fd >= 0)
        return;

    marker_fd = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
}

//===========================================================================
// NvTestCloseFTrace() - Close ftrace
//===========================================================================
static void NvTestCloseFTrace(void)
{
    if(marker_fd < 0)
        return;

    close(marker_fd);
    marker_fd = -1;
}

//===========================================================================
// NvTestWriteFTrace() - Write a string into ftrace
//===========================================================================
void NvTestWriteFTrace(const char *str)
{
    if(marker_fd < 0)
        return;

    write(marker_fd, str, NvOsStrlen(str));
}

#else /* NVOS_IS_LINUX */

static void NvTestOpenFTrace(void) { }
static void NvTestCloseFTrace(void) { }
void NvTestWriteFTrace(const char *str) { }

#endif /* NVOS_IS_LINUX */

//===========================================================================
// NvTestUseTestIO() - Check if NVTestIO should be used
//===========================================================================
static NvBool NvTestUseTestIO(int argc, char *argv[])
{
    int i;

    for (i=1; i<argc; i++)
    {
        if (!NvOsStrncmp(argv[i], "#testio", sizeof("#testio"))) {
            return NV_TRUE;
        }
    }
    return NV_FALSE;
}

//===========================================================================
// NvTestInitialize() - must called before any other NvTest* functions
//===========================================================================
void NvTestInitialize(
            int                          *argc,
            char                        *argv[],
            NvTestApplication           *pApp)
{
    static NvTestApp app[1] = {{0}};
    NvU32  err;

    (void)err; // prevent warnings
    NV_ASSERT(!gs_app || gs_app == app);

    if (!gs_app) {
        NV_ASSERT(app->state == NvTestAppState_Uninitialized);
        NV_ASSERT(!app->useCount);
        app->state = NvTestAppState_Initialized;
        app->useCount = 1;
        app->failTail = &app->failHead;
        app->enableNvosTransport = 0;
        app->host       = 0;
        app->hostStdin  = 0;
        app->hostStdout = 0;
        app->testCnt    = 0;
        app->failureCnt = 0;
        app->subtestFilter = NULL;
        app->subtestList = NULL;
        app->rest = NV_FALSE;
        app->tlsIndex = NvOsTlsAlloc();
        err = NvOsMutexCreate(&app->mutex);
        NV_ASSERT(!err);
        gs_app = app;

        NvTestOpenFTrace();

        if (argc && NvTestUseTestIO(*argc, argv)) {
            //
            // Note: ignoring errors.  Errors are OK.
            //
            if( argc )
            {
                NvTestConnectToHost(&app->host,*argc,argv);
            }
            err = NvTioFopen("stdin:",  NVOS_OPEN_READ,  &app->hostStdin);
            NV_ASSERT(!err);
            err = NvTioFopen("stdout:", NVOS_OPEN_WRITE, &app->hostStdout);
            NV_ASSERT(!err);
        }

    } else {
        NV_ASSERT(app->state == NvTestAppState_Initialized);

        NvOsMutexLock(app->mutex);
        NV_ASSERT(app->useCount);
        app->useCount++;
        NvOsMutexUnlock(app->mutex);
    }

    if (pApp)
        *pApp= app;
}

//===========================================================================
// NvTestSetRunFunction() - set the NvRun() function.
//===========================================================================
void NvTestSetRunFunction(NvTestApplication app, void (*func)(NvU32))
{
    app = app?app:gs_app;
    NV_ASSERT(gs_app == app);
    NvOsMutexLock(app->mutex);
    // if called more than once, use the function from the first call.
    app->nvRun = app->nvRun ? app->nvRun : func;
    NvOsMutexUnlock(app->mutex);
}

//===========================================================================
// NvTestSubtestEnd() - called after each test
//===========================================================================
static void NvTestSubtestEnd(NvTestSubtestHandle sub)
{
    char* result = "pass";

    NV_ASSERT(sub->app == gs_app);
    // We could lift the restriction of one active subtest per thread, but that
    // wouldn't work with helper macros in nvtest.h.
    NV_ASSERT(NvTestGetCurSubtest() == sub);

    if (sub->result) {
        NvOsMutexLock(sub->app->mutex);
        sub->app->failureCnt++;
        NvOsMutexUnlock(sub->app->mutex);
        result = "fail";
    }

    if (sub->skip) {
        NvOsMutexLock(sub->app->mutex);
        sub->app->testCnt--;
        sub->app->filteredCnt++;
        NvOsMutexUnlock(sub->app->mutex);
    } else {
        if (sub->app->rest)
            NvTestPrintf("[REST:, test_case=%s, disposition=%s, duration=%d]\n", sub->name, result, NvOsGetTimeMS() - sub->startTime);
        else
            NvTestPrintf("[%s: %s]\n", result, sub->name);
    }

    NvOsFree(sub->name);
    sub->name = NULL;

    NvTestSetCurSubtest(NULL);
    NvOsFree(sub);
}

//===========================================================================
// NvTestTerminate() - called at end of all tests to report results
//===========================================================================
NvError NvTestTerminate(NvTestApplication app)
{
    NvError err;

    app = app?app:gs_app;
    NV_ASSERT(gs_app == app);

    NvOsMutexLock(app->mutex);

    NV_ASSERT(app->state == NvTestAppState_Initialized ||
                 app->state == NvTestAppState_RanTests);
    NV_ASSERT(app->useCount);

    if (NvTestGetCurSubtest()) {
        NvTestSubtestEnd(NvTestGetCurSubtest());
    }

    if (app->failureCnt && !app->err)
        app->err = NvError_TestApplicationFailed;

    err = app->err;
    if (--app->useCount == 0) {

        NvTestCloseFTrace();

        NvOsMutexUnlock(app->mutex);
        NvOsMutexDestroy(app->mutex);

        app->state = NvTestAppState_Terminated;

        NvTestPrintf( "---------------\n" );
        NvTestPrintf("total subtests: %d\n", app->testCnt);
        NvTestPrintf("total failures: %d\n", app->failureCnt);
        if (app->filteredCnt > 0)
            NvTestPrintf("total skipped:  %d\n", app->filteredCnt);

        NvTestPrintf("\n\n");

        if (app->err &&
            (app->err != NvError_TestApplicationFailed ||
             !app->failureCnt)) {
            NvTestPrintf("NvTestMain() returned error.\n\n");
        }

        if (app->host) {
            NvTioBreakpoint();
            NvTioExit(app->err ? 1 : 0);
            NvTioClose(app->hostStdin);
            NvTioClose(app->hostStdout);
            app->hostStdin  = 0;
            app->hostStdout = 0;

            NvTestDisconnectFromHost(app->host);
            app->host = 0;
        }

        if (app->subtestFilter)
            NvOsFree(app->subtestFilter);

        if (app->subtestList)
            NvOsFree(app->subtestList);

        NvOsTlsFree(app->tlsIndex);

        NvOsMemset(app, 0, sizeof(*app));
        gs_app = 0;
    } else {
        NvOsMutexUnlock(app->mutex);
    }
    return err;
}

//===========================================================================
// NvTestError() - indicate that the test should return an error
//===========================================================================
void NvTestError(NvTestApplication app, NvError err)
{
    app = app?app:gs_app;
    NV_ASSERT(gs_app == app);

    NvOsMutexLock(app->mutex);

    NV_ASSERT(app->state == NvTestAppState_Initialized ||
                 app->state == NvTestAppState_RanTests);
    NV_ASSERT(app->useCount);

    if (err) {
        app->err = err;
    } else if (!app->err) {
        app->err = NvError_TestApplicationFailed;
    }

    NvOsMutexUnlock(app->mutex);
}

//===========================================================================
//
//===========================================================================
static void ReadInputFile(NvTestApplication app, char* filename)
{
    NvOsFileHandle file = NULL;
    NvOsStatType stat;
    char* data = NULL;
    char* tmp;
    size_t read;
    NvBool inName = NV_TRUE;
    unsigned int i;

    NvError err = NvOsFopen(filename, NVOS_OPEN_READ, &file);
    if (err)
        goto fail;

    err = NvOsFstat(file, &stat);
    if (err)
        goto fail;

    data = (char*)NvOsAlloc((size_t)stat.size);
    if (!data)
        goto fail;

    err = NvOsFread(file, data, (size_t)stat.size, &read);
    if (err)
        goto fail;

    app->subtestList = (char*)NvOsAlloc((size_t)stat.size);
    if (!app->subtestList)
        goto fail;

    tmp = app->subtestList;
    for (i = 0; i < read; i++)
    {
        if (inName)
            *(tmp++) = data[i];


        if (data[i] == ',' || data[i] == '\n')
            inName = !inName;
    }
    *tmp = '\0';

    NvTestPrintf("Subtest list set to: %s\n", app->subtestList);

fail:
    if (data)
        NvOsFree(data);

    if (file)
        NvOsFclose(file);
}

//===========================================================================
//
//===========================================================================
static NvBool ShouldRunSubtest(NvTestSubtest* sub)
{
    if (sub->app->subtestFilter)
    {
        NvU32 filterLen = NvOsStrlen(sub->app->subtestFilter);
        const char *filter = sub->app->subtestFilter;

        if (!((NvOsStrncmp(sub->name, filter, filterLen) == 0 &&
              filterLen == NvOsStrlen(sub->name)) ||
              (filter[filterLen-1] == '*' &&
               NvOsStrncmp(sub->name, filter, filterLen-1) == 0)))
            return NV_FALSE;
    }

    if (sub->app->subtestList)
    {
        char tmpName[MAX_SUBTEST_NAME_LENGTH];

        NvOsSnprintf(tmpName, MAX_SUBTEST_NAME_LENGTH, "%s,", sub->name);

        if (!NvUStrstr(sub->app->subtestList, tmpName))
            return NV_FALSE;
    }

    return NV_TRUE;
}

//===========================================================================
// NvTestSubtestBegin() - call to determine whether a subtest should be run
//===========================================================================
NvBool NvTestSubtestBegin(
            NvTestApplication app,
            NvTestSubtestHandle *pSubHandle,
            const char *testNameFormat, ...)
{
    char tmpName[MAX_SUBTEST_NAME_LENGTH];
    va_list valist;
    NvTestSubtest* sub;

    app = app?app:gs_app;
    NV_ASSERT(gs_app == app);

    NvOsMutexLock(app->mutex);

    NV_ASSERT(app->state == NvTestAppState_Initialized ||
                 app->state == NvTestAppState_RanTests);

    sub = (NvTestSubtest*)NvOsAlloc(sizeof(NvTestSubtest));
    if (!sub)
        goto fail;

    NvOsMemset(sub, 0, sizeof(NvTestSubtest));

    if (NvTestGetCurSubtest()) {
        NvTestSubtestEnd(NvTestGetCurSubtest());
    }

    // Keep our own copy of subtest name, cause we have no guarantees about what
    // the app does with the string between calling this and NvTestSubtestEnd
    if (testNameFormat)
    {
        NvU32 len;
        NvS32 success;
        va_start(valist, testNameFormat);
        success = NvOsVsnprintf(
            tmpName,
            MAX_SUBTEST_NAME_LENGTH,
            testNameFormat,
            valist);
        va_end(valist);
        if (success > 0)
        {
            len = NvOsStrlen(tmpName);
            sub->name = NvOsAlloc(len + 1);
            if (!sub->name)
                goto fail;
            NvOsStrncpy(sub->name, tmpName, len);
        }
        else
        {
            // NvOsVsnprintf failed
            len = NvOsStrlen(testNameFormat);
            sub->name = NvOsAlloc(len + 1);
            if (!sub->name)
                goto fail;
            NvOsStrncpy(sub->name, testNameFormat, len);
        }
        sub->name[len] = '\0';
    }
    else
    {
        sub->name = NvOsAlloc(1);
        sub->name[0] = '\0';
    }

    sub->result  = NvSuccess;
    sub->skip = NV_FALSE;
    sub->app = app;

    if (!ShouldRunSubtest(sub))
    {
        // The filter does not match, skip test.
        app->filteredCnt++;
        // Free name if filtered out, because SubtestEnd is *not*
        // called for filtered-out tests.
        if (sub->name)
            NvOsFree(sub->name);
        sub->name = NULL;
        NvOsFree(sub);
        NvOsMutexUnlock(app->mutex);
        return NV_FALSE;
    }

    sub->startTime = NvOsGetTimeMS();
    sub->index = app->testCnt++;
    NvTestSetCurSubtest(sub);

    if (pSubHandle)
        *pSubHandle = sub;

    // NvRun() function is for setting a breakpoint before the subtest begins.
    if (app->nvRun)
        app->nvRun(app->testCnt - 1);   // This calls NvRun()

    NvOsMutexUnlock(app->mutex);
    return NV_TRUE;   // yes, do run the test

fail:
    NvTestPrintf("Failed to allocate memory for subtest\n");
    if (sub)
        NvOsFree(sub);
    NvOsMutexUnlock(app->mutex);
    return NV_FALSE;
}

//===========================================================================
// NvTestSubtestSkip() - Call if a subtest should be skipped after starting
//===========================================================================
void NvTestSubtestSkip(
            NvTestApplication app,
            NvTestSubtestHandle sub,
            const char *reason)
{
    app = app ? app : gs_app;
    if (!sub && !NvTestGetCurSubtest())
        NvTestSubtestBegin(app, 0, "unknown");

    sub = sub ? sub : NvTestGetCurSubtest();
    NV_ASSERT(sub->app == app);
    NV_ASSERT(NvTestGetCurSubtest() == sub);

    sub->skip = NV_TRUE;
    NvTestPrintf( "[skip: %s] %s\n",
            sub->name,
            reason?reason:" " );
}

//===========================================================================
// NvTestSubtestFail() - Call if a subtest fails
//===========================================================================
void NvTestSubtestFail(
            NvTestApplication app,
            NvTestSubtestHandle sub,
            const char *reason,
            const char *file,
            int line)
{
    app = app ? app : gs_app;
    if (!sub && !NvTestGetCurSubtest())
        NvTestSubtestBegin(app, 0, "unknown");

    sub = sub ? sub : NvTestGetCurSubtest();
    NV_ASSERT(sub->app == app);
    NV_ASSERT(NvTestGetCurSubtest() == sub);

    sub->result = NvError_TestApplicationFailed;
    NvTestPrintf( "[fail: %s  at %s:%d] %s\n",
            sub->name,
            file,
            line,
            reason?reason:" " );
}

//===========================================================================
// NvTestGetFailureCount() - returns number of failures so far
//===========================================================================
NvU32 NvTestGetFailureCount(NvTestApplication app)
{
    NV_ASSERT(gs_app == app);
    NV_ASSERT(app->state == NvTestAppState_RanTests);
    return app->failureCnt;
}

//===========================================================================
// NvTestReadline() - printf to host
//===========================================================================
NvError NvTestReadline(
                const char *prompt,
                char *buffer,
                size_t bufferLength,
                size_t *count)
{
    NvError err;
    size_t i, cnt, srcCnt;
    int c;
    char *src;
    char *dst = buffer;
    NV_ASSERT(gs_app);

    if (!gs_app->host)
        return NvError_NotSupported;

    NvOsMutexLock(gs_app->mutex);

    // make count point to something if NULL was passed in
    count = count ? count : &cnt;
    *count = 0;
    buffer[0] = 0;

    // Signal out of memory error if we lost chars last time
    if (gs_app->stdinBufCnt == 0xFFFFFFFF) {
        gs_app->stdinBufCnt = 0;
        NvOsMutexUnlock(gs_app->mutex);
        return DBERR(NvError_InsufficientMemory);
    }

    if (prompt) {
        NvTestPrintf("%s%s\n",
            prompt,
            gs_app->stdinBufCnt ? gs_app->stdinBuf : "");
    }

    while (1) {

        //
        // chars left over from last read?
        //
        if (gs_app->stdinBufCnt) {
            srcCnt = gs_app->stdinBufCnt;
            src    = gs_app->stdinBuf;
        } else {
            err = NvTioFread(
                        gs_app->hostStdin,
                        dst,
                        bufferLength - (dst - buffer) - 1,
                        &srcCnt);
            if (err) {
                buffer[0] = 0;
                *count = 0;
                NvOsMutexUnlock(gs_app->mutex);
                return DBERR(err);
            }
            src = dst;
        }
        cnt = srcCnt < bufferLength-1 ?
              srcCnt : bufferLength-1;
        c = 0;
        for (i=0; i<cnt; ) {
            c = src[i++];
            if (c=='\r' || c=='\n') {
                *(dst++) = '\n';
                break;
            }
            *(dst++) = c;
        }
        if (c=='\n' || c=='\r' || i == bufferLength-1) {

            //
            // collapse "\r\n" or "\n\r" into "\n"
            //
            if (i<srcCnt) {
                if ((src[i]=='\n' && c=='\r') ||
                    (src[i]=='\r' && c=='\n')) {
                    i++;
                }
            }

            if (srcCnt == i) {
                //
                // no characters left over - free stdinBuf
                //
                NvOsFree(gs_app->stdinBuf);
                gs_app->stdinBuf = 0;
                gs_app->stdinBufCnt = 0;
                gs_app->stdinBufLen = 0;
            } else {

                //
                // allocate gs_app->stdinBuf to save extra chars
                //
                gs_app->stdinBufCnt = srcCnt - i;
                if (gs_app->stdinBufCnt+1 > gs_app->stdinBufLen) {
                    NvOsFree(gs_app->stdinBuf);
                    gs_app->stdinBuf = NvOsAlloc(gs_app->stdinBufCnt);
                    gs_app->stdinBufLen = gs_app->stdinBufCnt+1;
                }
                if (gs_app->stdinBuf) {
                    //
                    // copy extra chars to stdinBuf for next time
                    //
                    src = src + i;
                    for (i=0; i<gs_app->stdinBufCnt; i++)
                        gs_app->stdinBuf[i] = src[i];
                    gs_app->stdinBuf[i] = 0;

                } else {

                    //
                    // Got a complete line, so no error here.
                    // But next read will be an error.
                    //
                    (void)DBERR(NvError_InsufficientMemory);
                    gs_app->stdinBufCnt = 0;
                    gs_app->stdinBufLen = -1;   // signal error next time
                }
            }
            *count = dst - buffer;
            *dst = 0;
            NvOsMutexUnlock(gs_app->mutex);
            return NvSuccess;
        }
        gs_app->stdinBufCnt = 0;
    }
}

//===========================================================================
// NvTestPrintf() - printf to host
//===========================================================================
void NvTestPrintf(const char *format, ...)
{
    va_list ap;
    va_start( ap, format );
    NvTestVprintf(format, ap);
    va_end( ap );
}

//===========================================================================
// NvTestVprintf() - vprintf to host
//===========================================================================
void NvTestVprintf(const char *format, va_list ap)
{
    NV_ASSERT(gs_app);
    if (gs_app->hostStdout) {
        NvTioVfprintf(gs_app->hostStdout, format, ap);
    } else {
#if NVOS_IS_QNX
        //This is required because QNX NvOsDebugVprintf
        //always and only prints to system log.
        vprintf(format, ap);
#else
        NvOsDebugVprintf(format, ap);
#endif
    }
}

//===========================================================================
// NvTestWrite() - vprintf to host
//===========================================================================
void NvTestWrite(const void *data, size_t len)
{
    NV_ASSERT(gs_app);
    if (gs_app->hostStdout) {
        NvTioFwrite(gs_app->hostStdout, data, len);
    } else {
        size_t i;
        for (i=0; i<len; i++) {
#if NVOS_IS_QNX
            //This is required because QNX NvOsDebugPrintf
            //always and only prints to system log.
            printf("%c", ((unsigned char *)data)[len]);
#else
            NvOsDebugPrintf("%c", ((unsigned char *)data)[len]);
#endif
        }
    }
}

//===========================================================================
// NvTestWriteString() - vprintf to host
//===========================================================================
void NvTestWriteString(const char *str)
{
    NvTestWrite(str,NvOsStrlen(str));
}

//===========================================================================
// NvTestSetSubtestFilter()
//===========================================================================
void NvTestSetSubtestFilter(NvTestApplication app, const char* Filter)
{
    int len = NvOsStrlen(Filter);
    char* copy = NvOsAlloc(len + 1);
    if (!copy)
    {
        NvTestPrintf("Failed to set subtest filter.\n");
        return;
    }
    NvOsStrncpy(copy, Filter, len + 1);

    NvOsMutexLock(app->mutex);

    if (app->subtestFilter)
        NvOsFree(app->subtestFilter);
    app->subtestFilter = copy;

    NvTestPrintf("Test filter set to: '%s'\n", app->subtestFilter);

    NvOsMutexUnlock(app->mutex);
}

//===========================================================================
// NvTestResultsForREST()
//===========================================================================
void NvTestResultsForREST(NvTestApplication app, char* filename)
{
    NV_ASSERT(filename);

    NvOsMutexLock(app->mutex);
    app->rest = NV_TRUE;
    ReadInputFile(app, filename);
    NvOsMutexUnlock(app->mutex);
}

//===========================================================================
// NvTestGetHwStrings() - get strings indicating the HW environment
//===========================================================================
void NvTestGetHwStrings(const char **pChip, const char **pSim)
{
    static char *chip = 0;
    static char *sim  = 0;

    if (!chip)
    {
        // TODO: come up with a better way to detect chip & simulator type
        NvError err;
        char buf[100];
        chip = "unk";
        sim  = "unk";
        buf[0] = 0;
        err = NvOsGetConfigString("NV_CFG_CHIPLIB", buf, sizeof(buf));
        if (!err && buf[0])
        {
            sim = "csim";
            if (!NvOsStrncmp(buf,"t30_",4))
                chip = "t30";
            else if (!NvOsStrncmp(buf,"t40_",4))
                chip = "t40";
        }
        buf[0] = 0;
        err = NvOsGetConfigString("NV_CFG_CHIPLIB_ARGS", buf, sizeof(buf));
        if (!err && buf[0])
        {
            int i;
            sim = "csim";
            for (i=0; buf[i]; i++)
            {
                if (!NvOsStrncmp(&buf[i], "csim", 4))
                {
                    sim="csim";
                    break;
                }
                if (!NvOsStrncmp(&buf[i], "asim", 4))
                {
                    sim="asim";
                    break;
                }
            }
        }
    }

    if (pChip)
        *pChip = chip;
    if (pSim)
        *pSim  = sim;
}
