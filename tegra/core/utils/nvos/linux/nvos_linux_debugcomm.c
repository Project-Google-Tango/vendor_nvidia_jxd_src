/*
 * Copyright 2009 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 * Stub implementation that does nothing for now.
 */

#include "nvos_debugcomm.h"

#if NVOS_RESTRACKER_COMPILED

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "nvassert.h"

#define FIFO_BUFFER_SIZE 512

#ifdef ANDROID
#define FIFO_FILE_PATH "/data/local/tmp/"
#else
#define FIFO_FILE_PATH "/tmp/"
#endif

#define DISABLE_PRINTS 1
#if DISABLE_PRINTS
    #define NvOsDebugPrintf(...) ;
#endif

/*
 * Internal data structures and global variables.
 */

typedef struct DebugConfigurationRec
{
    NvDebugConfigVar    config[NvDebugConfigVar_Last];  /* Actual configuration values. */
    NvBool              is_set[NvDebugConfigVar_Last];  /* Whether the slot has been written to. */
} DebugConfiguration;

typedef struct CommandLineTokenRec {
    const char* name;
    NvU32       token;
    NvU32       argc;
} CommandLineToken;

/* We want to have our tokens based on NvDebugCommAction and
 * NvDebugConfigVar but also make them unique. */
#define ACTION 0x80000000
#define CONFIG 0x40000000

static CommandLineToken s_tokens[] =
{
    { "none",           ACTION | NvDebugCommAction_None,                    0 },
    { "checkpoint",     ACTION | NvDebugCommAction_Checkpoint,              0 },
    { "dump",           ACTION | NvDebugCommAction_Dump,                    0 },
    { "dumptofile",     ACTION | NvDebugCommAction_DumpToFile,              0 },
    { "__kill__",       ACTION | NvDebugCommAction_KillDebugcommListener,   0 },

    { "stack",          CONFIG | NvDebugConfigVar_Stack,                    1 },
    { "breakstackid",   CONFIG | NvDebugConfigVar_BreakStackId,             1 },
    { "breakid",        CONFIG | NvDebugConfigVar_BreakId,                  1 },
    { "stackfilterpid", CONFIG | NvDebugConfigVar_StackFilterPid,           1 },
    { "disabledump",    CONFIG | NvDebugConfigVar_DisableDump,              1 },
    { NULL }
};

static NvConfigCallback         s_configCallback    = NULL;
static NvActionCallback         s_actionCallback    = NULL;

static NvOsSemaphoreHandle      s_debugcomm_sem     = NULL;
static NvOsThreadHandle         s_listenerThread    = NULL;

static int                      s_listenerPID       = -1;
static int                      s_mainPID           = -1;
static DebugConfiguration       s_config            = { { 0 }, { NV_FALSE } };

/*
 * Static functions.
 */

NV_INLINE void                  skip_ws             (const char** s);
NV_INLINE NvU32                 get_next_word       (const char** s, const char** start, NvU32* length);
static NvBool                   word_to_token       (const char* word, NvU32 token_length, NvU32* rv, NvU32* argc);
static NvBool                   token_to_word       (NvU32 token, const char** word);
static NvError                  parse_cmdline       (const char* cmdline, int out_fd);

static const char*              fifo_name           (int pid);
static NvError                  get_fifo            (const char* path, NvBool to_write, int* rv);
static NvError                  send_cmd            (const char* cmdline, NvU32 pid);
static void                     fifo_listener       (void* args);

/*
 * A simple word-parsing library to turn command lines into actual
 * configurations.
 */

void skip_ws (const char** s)
{
    while (isspace(**s))
        (*s)++;
}

NvU32 get_next_word (const char** s, const char** start, NvU32* length)
{
    skip_ws(s);
    *start = *s;                /* Save start of the word. */

    while (**s && !isspace(**s))
        (*s)++;

    *length = *s - *start;     /* Save length of the word. */
    return *length;
}

NvBool word_to_token (const char* word, NvU32 token_length, NvU32* rv, NvU32* argc)
{
    int i;

    for (i = 0; s_tokens[i].name; i++)
    {
        if (strncasecmp(s_tokens[i].name, word, token_length) == 0)
        {
            *rv     = s_tokens[i].token;
            *argc   = s_tokens[i].argc;
            return NV_TRUE;
        }
    }

    return NV_FALSE;
}

NvBool token_to_word (NvU32 token, const char** word)
{
    int i;

    for (i = 0; s_tokens[i].name; i++)
    {
        if (s_tokens[i].token == token)
        {
            *word = s_tokens[i].name;
            return NV_TRUE;
        }
    }

    return NV_FALSE;
}

NvError parse_cmdline (const char* cmdline, int out_fd)
{
    const char* word;
    NvU32       len;

    /* Process commands until end-of-data. */
    while (get_next_word(&cmdline, &word, &len) > 0)
    {
        NvU32       cmd;
        NvU32       argc;
        long int    val = 0;

        /* Read action/config request. */
        if (!word_to_token(word, len, &cmd, &argc))
            return NvError_BadParameter;

        /* Read config value, if needed. */
        if (argc > 0)
        {
            char* endptr;

            if (get_next_word(&cmdline, &word, &len) == 0)
                return NvError_BadParameter;

            errno = 0;
            val = strtol(word, &endptr, 0);
            if (errno != 0 || endptr == word || (endptr - word) > len)
                return NvError_BadParameter;
        }

        /* Pass control to the appropriate hooks. */
        NV_ASSERT(cmd & (CONFIG | ACTION));

        if (cmd & ACTION)
        {
            /* Intercept kill request. */
            if ((cmd & ~ACTION) == NvDebugCommAction_KillDebugcommListener)
                s_listenerPID = -1;

            /* Otherwise pass to action callback. */
            else if (s_actionCallback)
                s_actionCallback(cmd & ~ACTION);
        }
        else if (cmd & CONFIG)
        {
            if (s_configCallback)
                s_configCallback(cmd & ~CONFIG, val);
        }
    }

    return NvSuccess;
}

/*
 * Debugcomm listener thread and FIFO management.
 */

const char* fifo_name (int pid)
{
    static char buffer[sizeof(FIFO_FILE_PATH) + 15];
    sprintf(buffer, FIFO_FILE_PATH "nvos-%d", pid);
    return buffer;
}

NvError get_fifo (const char* path, NvBool to_write, int* rv)
{
    struct stat filestat;

    /* Protect from race condition: if the fifo doesn't exist but someone
     * creates it before our mkfifo() call, make sure it still won't exist
     * until bailing out. */
    if (lstat(path, &filestat) == -1 &&
        mkfifo(path, 0664) == -1 && 
        lstat(path, &filestat) == -1)
    {
        NvOsDebugPrintf("[NVOS] PID %d: fifo %s doesn't exist and can't create one.\n", s_mainPID, path);
        return NvError_FileOperationFailed;
    }

    *rv = open(path, to_write ? O_WRONLY : O_RDONLY);

    return (*rv != -1) ? NvSuccess : NvError_FileOperationFailed;
}

NvError send_cmd (const char* cmdline, NvU32 pid)
{
    NvError err;
    int     fd;

    err = get_fifo(fifo_name(pid), NV_TRUE, &fd);

    if (err == NvSuccess)
    {
        write(fd, cmdline, strlen(cmdline));
        close(fd);
    }

    return err;
}

static void fifo_listener (void* args)
{
    /* Standard malloc won't show up in the process leak reports. */
    char* fifobuffer = malloc(FIFO_BUFFER_SIZE);

    if (fifobuffer)
    {
        /* Record listener pid and signal "go" to parent. */
        s_listenerPID = getpid();
        NvOsSemaphoreSignal(s_debugcomm_sem);

        /* Main loop for FIFO listener: get_fifo() creates the fifo and opens
         * it which blocks until something is written to it. The command line
         * is read and processed, fifo closed, and then we loop back to the
         * start.
         *
         * By the time of reading, the writer has probably closed the fifo
         * already which breaks the connection, causing read() to just return
         * immediately and return 0 bytes without error. So, we close the old
         * fd, loop back and reopen the fifo. Reopening it doesn't lose any
         * data because any writing blocks until we open the fifo for reading
         * again.
         */
        while (s_listenerPID != -1)
        {
            size_t  bytes_read;
            int     fifofd;

            if (NvSuccess == get_fifo(fifo_name(s_mainPID), NV_FALSE, &fifofd))
            {
                errno = 0;
                bytes_read = read(fifofd, fifobuffer, FIFO_BUFFER_SIZE - 1);

                if (errno == 0 && bytes_read > 0 && fifobuffer[0])
                {
                    fifobuffer[bytes_read] = '\0';

                    if (NvSuccess != parse_cmdline(fifobuffer, fifofd))
                    {
                        NvOsDebugPrintf("[NVOS] PID %d: received erroneous debugcomm message: \"%s\"\n",
                                        s_mainPID,
                                        fifobuffer);
                    }
                }

                close(fifofd);
            }
            else
            {
                NvOsDebugPrintf("[NVOS] PID %d: unable to open debugcomm fifo \"%s\"; retrying in 1s...\n",
                                s_mainPID,
                                fifo_name(s_mainPID));
                sleep(1);
            }
        }

        free(fifobuffer);
    }

    NvOsSemaphoreSignal(s_debugcomm_sem);
}

/*
 * NvDebugcomm API.
 */

NvError NvReadConfigInteger (NvDebugConfigVar name, NvU32 *value)
{
    /* This API function is effectively client-only: the listener should
     * never really wants to call us. */
    NV_ASSERT(getpid() != s_listenerPID);

    if (0 < name && name < NvDebugConfigVar_Last)
    {
        if (!s_config.is_set[name])
            return NvError_InvalidState;

        *value = s_config.config[name];
        return NvSuccess;
    }

    return NvError_BadParameter;
}

NvError NvWriteConfigInteger (NvDebugConfigVar name, NvU32 value)
{
    /* This API function is effectively client-only: the listener should
     * never really wants to call us. */
    NV_ASSERT(getpid() != s_listenerPID);

    if (0 < name && name < NvDebugConfigVar_Last)
    {
        s_config.config[name] = value;
        s_config.is_set[name] = NV_TRUE;

        return NvSuccess;
    }

    return NvError_BadParameter;
}

NvError NvSendAction (NvDebugCommAction action, NvU32 targetPid)
{
    NvError err = NvError_InsufficientMemory;
    char*   buffer;

    /* This API function is effectively client-only: the listener should
     * never really wants to call us. */
    NV_ASSERT(getpid() != s_listenerPID);

    buffer = NvOsAlloc(FIFO_BUFFER_SIZE);

    /* Build a command line based on the given action and any
     * configuration variables set earlier, then send it off to the
     * control pipe of the appropriate target process. */
    if (buffer)
    {
        NvBool      ok;
        char        tmp[32];
        const char* name;
        int         i;

        memset(buffer, 0, FIFO_BUFFER_SIZE);

        /* Purge config changes first. */
        for (i = 0; i < NvDebugConfigVar_Last; i++)
        {
            if (s_config.is_set[i])
            {
                ok = token_to_word(CONFIG | i, &name);
                NV_ASSERT(ok);

                sprintf(tmp, "%s 0x%x ", name, s_config.config[i]);
                strncat(buffer, tmp, FIFO_BUFFER_SIZE);
            }
        }

        /* Append action. */
        ok = token_to_word(ACTION | action, &name);
        NV_ASSERT(ok);

        sprintf(tmp, "%s", name);
        strncat(buffer, tmp, FIFO_BUFFER_SIZE);

        /* Signal target. */
        err = send_cmd(buffer, targetPid);

        NvOsFree(buffer);
    }

    return err;
}

NvError NvDebugCommInit (NvConfigCallback config, NvActionCallback action)
{
    NvError rv;

    /* For main thread--listener communication. */
    rv = NvOsSemaphoreCreate(&s_debugcomm_sem, 0);
    if (rv != NvSuccess)
        return rv;

    /* Store PID of main process. */
    s_mainPID = getpid();

    /* Create and sync the listener. */
    rv = NvOsThreadCreate(fifo_listener, NULL, &s_listenerThread);
    if (rv == NvSuccess)
    {
        NvOsSemaphoreWait(s_debugcomm_sem); /* Wait for listener initialization. */

        if (s_listenerPID == -1)
            rv = NvError_InvalidState;
    }

    s_actionCallback = action;
    s_configCallback = config;

    return rv;
}

void NvDebugCommDeinit(void)
{
    if (s_debugcomm_sem)
    {
        if (s_listenerPID != -1)
        {
            /* The listener is generally blocked by the fifo: try to awake it. */
            if (NvSuccess == send_cmd("__kill__", s_mainPID))
            {
                /* Wait for exit. */
                NvOsThreadJoin(s_listenerThread);
            }
            else
            {
                /* Something must be wrong: fall back to killing the thread brutally. */
                kill(s_listenerPID, SIGKILL);
            }
        }

        NvOsSemaphoreDestroy(s_debugcomm_sem);

        unlink(fifo_name(s_mainPID));
    }
}

const char* NvGetDumpFilePath (void)
{
#ifdef ANDROID
    return "/data/local/tmp/nvos.dump";
#else
    return "/tmp/nvos.dump";
#endif
}

#endif // NVOS_RESTRACKER_COMPILED
