/* Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved. */
/*
** Copyright 2010, Icera Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** You may not use this file except in compliance with the License.
** You may obtain a copy of the License at:
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** imitations under the License.
*/
/*
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "atchannel.h"
#include "at_tok.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define LOG_NDEBUG 0
#define LOG_TAG atLogTag
#include <utils/Log.h>

#include "misc.h"

#include "at_channel_handle.h"
#include "atchannel.h"
#include "icera-ril.h"

#ifdef HAVE_ANDROID_OS
#define USE_NP 1
#endif /* HAVE_ANDROID_OS */


#define NUM_ELEMS(x) (sizeof(x)/sizeof(x[0]))

#define HANDSHAKE_RETRY_COUNT 10
#define HANDSHAKE_TIMEOUT_MSEC 1000

static int s_ackPowerIoctl; /* true if TTY has android byte-count
                                handshake for low power*/

#if AT_DEBUG
void  AT_DUMP(const char*  prefix, const char*  buff, int  len)
{
    if (len < 0)
        len = strlen(buff);
    ALOGD("%.*s", len, buff);
}
#endif

static void onReaderClosed(void);
static int writeCtrlZ (const char *s);
static int writeline (const char *s, const char *s_to_log);

#ifndef USE_NP
static void setTimespecRelative(struct timespec *p_ts, long long msec)
{
    struct timeval tv;

    gettimeofday(&tv, (struct timezone *) NULL);

    /* what's really funny about this is that I know
       pthread_cond_timedwait just turns around and makes this
       a relative time again */
    p_ts->tv_sec = tv.tv_sec + (msec / 1000);
    p_ts->tv_nsec = (tv.tv_usec + (msec % 1000) * 1000L ) * 1000L;
}
#endif /*USE_NP*/

static void sleepMsec(long long msec)
{
    struct timespec ts;
    int err;

    ts.tv_sec = (msec / 1000);
    ts.tv_nsec = (msec % 1000) * 1000 * 1000;

    do {
        err = nanosleep (&ts, &ts);
    } while (err < 0 && errno == EINTR);
}



/** add an intermediate response to sp_response*/
static void addIntermediate(const char *line)
{
    ATLine *p_new;
    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("addIntermediate() AT channel handle not found");
        return;
    }

    p_new = (ATLine  *) malloc(sizeof(ATLine));

    p_new->line = strdup(line);

    /* note: this adds to the head of the list, so the list
       will be in reverse order of lines received. the order is flipped
       again before passing on to the command issuer */
    p_new->p_next = handle->response->p_intermediates;
    handle->response->p_intermediates = p_new;
}

static const char* s_at_cmds_no_carrier_is_final_result[] = {
    "ATD"
  , "ATA"
  , "ATO"
  , "AT+CGACT"
  , NULL
};

static void atCmdSet( const char* cmd )
{
    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("at_cmd_set() AT channel handle not found");
        return;
    }

    if ( cmd )
    {
        /* store the command */
        char* p = handle->at_cmd;
        const char* c = cmd;

        if ( 0 != handle->at_cmd[0] )
        {
           ALOGW("at_cmd_set() There is already an AT command stored!!");
        }

        while ( *c && ((size_t)(c-cmd)) < sizeof(handle->at_cmd) ) {
           *p++ = *c++;
        }
    }
    else
    {
        /* if called with NULL, clear AT cmd buffer */
        memset( handle->at_cmd, 0, sizeof( handle->at_cmd ));
    }
}

static char* atCmdGet( void )
{
    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("at_cmd_set() AT channel handle not found");
        return "";
    }

    return handle->at_cmd;
}

static int isNoCarrierFinalResult( const char* cmd )
{
    char** p;
    for ( p = (char**)s_at_cmds_no_carrier_is_final_result; *p; ++p )
    {
        if ( !strncmp( *p, cmd, strlen( *p ) ) )
        {
            return 1;
        }
    }

    return 0;
}

/**
 * returns 1 if line is a final response indicating error
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static const char * s_finalResponsesError[] = {
    "+CMS ERROR:",
    "+CME ERROR:",
    "NO CARRIER", /* sometimes! */
    "NO ANSWER",
    "NO DIALTONE",
    "+CCHC ERROR"
};
static int isFinalResponseError(const char *line)
{
    size_t i;

    if (strStartsWith(line, s_finalResponsesError[2]) && /* NO Carrier */
        !isNoCarrierFinalResult(atCmdGet())            ) {
       return 0;
    }

    /* We want exactly ERROR as now at%debug response can contain ERROR: xxxx */
    if(strcmp(line, "ERROR") == 0)
    {
        return 1;
    }

    /* Skip the first entry in the array */
    for (i = 0 ; i < NUM_ELEMS(s_finalResponsesError) ; i++) {
        if (strStartsWith(line, s_finalResponsesError[i])) {
            return 1;
        }
    }

    return 0;
}

/**
 * returns 1 if line is a final response indicating success
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static const char * s_finalResponsesSuccess[] = {
    "OK",
    "CONNECT"       /* some stacks start up data on another channel */
};
static int isFinalResponseSuccess(const char *line)
{
    /* As "OK" can start a crashdump line (AT%DEBUG=0), we have to compare the all line */
    if (strcmp(line, s_finalResponsesSuccess[0]) == 0) {
        return 1;
    }

    /* In case of "CONNECT", we can also have the speed displayed, so we have to compare only the beginning of the line*/
    if (strStartsWith(line, s_finalResponsesSuccess[1])) {
        return 1;
    }

    return 0;
}

/**
 * returns 1 if line is a final response, either  error or success
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static int isFinalResponse(const char *line)
{
    return isFinalResponseSuccess(line) || isFinalResponseError(line);
}


/**
 * returns 1 if line is the first line in (what will be) a two-line
 * SMS unsolicited response
 */
static const char * s_smsUnsoliciteds[] = {
    "+CMT:",
    "+CDS:",
    "+CBM:"
};
static int isSMSUnsolicited(const char *line)
{
    size_t i;

    for (i = 0 ; i < NUM_ELEMS(s_smsUnsoliciteds) ; i++) {
        if (strStartsWith(line, s_smsUnsoliciteds[i])) {
            return 1;
        }
    }

    return 0;
}


/** assumes s_commandmutex is held */
static void handleFinalResponse(const char *line)
{
    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("handleFinalResponse() AT channel handle not found");
        return;
    }

    handle->response->finalResponse = strdup(line);
    atCmdSet(NULL);

    pthread_cond_signal(&handle->commandcond);
}

static void handleUnsolicited(const char *line)
{
    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("handleUnsolicited() AT channel handle not found");
        return;
    }

    /* Only process unsolicited if on the default channel. */
    if (handle->unsolHandler != NULL && handle->isDefaultChannel) {
        handle->unsolHandler(line, NULL);
    }
}

static void processLine(const char *line)
{
    at_channel_handle_t* handle = at_channel_handle_get();
    if ( NULL == handle )
    {
        ALOGE("processLine() AT channel handle not found");
        return;
    }

    pthread_mutex_lock(&handle->commandmutex);

    if (handle->response == NULL) {
        /* no command pending */
        handleUnsolicited(line);
    } else if (isFinalResponseSuccess(line)) {
        handle->response->success = 1;
        handleFinalResponse(line);
    } else if (isFinalResponseError(line)) {
        handle->response->success = 0;
        handleFinalResponse(line);
    } else if (handle->smsPDU != NULL && 0 == strcmp(line, "> ")) {
        // See eg. TS 27.005 4.3
        // Commands like AT+CMGS have a "> " prompt
        writeCtrlZ(handle->smsPDU);
        handle->smsPDU = NULL;
    } else switch (handle->type) {
        case NO_RESULT:
            handleUnsolicited(line);
            break;
        case NUMERIC:
            if (handle->response->p_intermediates == NULL
                && isdigit(line[0])
            ) {
                addIntermediate(line);
            } else {
                /* either we already have an intermediate response or
                   the line doesn't begin with a digit */
                handleUnsolicited(line);
            }
            break;
        case SINGLELINE:
            if (handle->response->p_intermediates == NULL
                && strStartsWith (line, handle->responsePrefix)
            ) {
                addIntermediate(line);
            } else {
                /* we already have an intermediate response */
                handleUnsolicited(line);
            }
            break;
        case MULTILINE:
            if (strStartsWith (line, handle->responsePrefix)) {
                addIntermediate(line);
            } else {
                handleUnsolicited(line);
            }
            break;
        case MULTILINE_NO_PREFIX:
            /* the line will be scanned for leading characters of all handled unsolicited commands */
            if ( !strStartsWith (line, "+C") &&
               !strStartsWith (line, "*T") &&
               !strStartsWith (line, "%I") &&
               !strStartsWith (line, "RING") &&
               !strStartsWith (line, "NO CARRIER")&&
               !strStartsWith (line, "%NWSTATE")&&
               !strStartsWith (line, "%ICTI"))
               {
                addIntermediate(line);
            } else {
                handleUnsolicited(line);
            }
            break;
        case TOFILE:
            /* the line will be scanned for leading characters of all handled unsolicited commands */
            if(handle->filename != NULL)
            {
                if( handle->fileout == NULL) {
                    /* Cannot open file */
                    ALOGE("TOFILE: unable to open output file (%s)", handle->filename);
                    handle->filename = NULL;
                    }
                else {
                    fputs(line,handle->fileout);
                    fputc('\n',handle->fileout);
                    }
                }
            else {
                ALOGE("TOFILE: filename not specified");
            }
            break;
        default: /* this should never be reached */
            ALOGE("Unsupported AT command type %d\n", handle->type);
            handleUnsolicited(line);
        break;
    }

    pthread_mutex_unlock(&handle->commandmutex);
}


/**
 * Returns a pointer to the end of the next line
 * special-cases the "> " SMS prompt
 *
 * returns NULL if there is no complete line
 */
static char * findNextEOL(char *cur)
{
    if (cur[0] == '>' && cur[1] == ' ' && cur[2] == '\0') {
        /* SMS prompt character...not \r terminated */
        return cur+2;
    }

    // Find next newline
    while (*cur != '\0' && *cur != '\r' && *cur != '\n') cur++;

    return *cur == '\0' ? NULL : cur;
}


/**
 * Reads a line from the AT channel, returns NULL on timeout.
 * Assumes it has exclusive read access to the FD
 *
 * This line is valid only until the next call to readline
 *
 * This function exists because as of writing, android libc does not
 * have buffered stdio.
 */

static const char *readline(void)
{
    ssize_t count;

    char *p_read = NULL;
    char *p_eol = NULL;
    char *ret;

    at_channel_handle_t* handle = at_channel_handle_get();
    if ( NULL == handle )
    {
        ALOGE("readline() AT channel handle not found");
        return NULL;
    }

    /* this is a little odd. I use *handle->ATBufferCur == 0 to
     * mean "buffer consumed completely". If it points to a character, than
     * the buffer continues until a \0
     */
    if (*handle->ATBufferCur == '\0') {
        /* empty buffer */
        handle->ATBufferCur = handle->ATBuffer;
        *handle->ATBufferCur = '\0';
        p_read = handle->ATBuffer;
    } else {   /* *handle->ATBufferCur != '\0' */
        /* there's data in the buffer from the last read */

        // skip over leading newlines
        while (*handle->ATBufferCur == '\r' || *handle->ATBufferCur == '\n')
            handle->ATBufferCur++;

        p_eol = findNextEOL(handle->ATBufferCur);

        if (p_eol == NULL) {
            /* a partial line. move it up and prepare to read more */
            size_t len;

            len = strlen(handle->ATBufferCur);

            memmove(handle->ATBuffer, handle->ATBufferCur, len + 1);
            p_read = handle->ATBuffer + len;
            handle->ATBufferCur = handle->ATBuffer;
        }
        /* Otherwise, (p_eol !- NULL) there is a complete line  */
        /* that will be returned the while () loop below        */
    }

    while (p_eol == NULL) {
        if (0 == MAX_AT_RESPONSE - (p_read - handle->ATBuffer)) {
            ALOGE("ERROR: Input line exceeded buffer\n");
            /* ditch buffer and start over again */
            handle->ATBufferCur = handle->ATBuffer;
            *handle->ATBufferCur = '\0';
            p_read = handle->ATBuffer;
        }

        do {
            count = read(handle->fd, p_read,
                            MAX_AT_RESPONSE - (p_read - handle->ATBuffer));
        } while ( count < 0 && (errno == EINTR || errno == EAGAIN ) );

        if (count > 0) {
            AT_DUMP( "<< ", p_read, count );
            handle->readCount += count;

            p_read[count] = '\0';
            /*
             * Null termination is not managed properly for SMS prompt
             * This fixes it... (the buffer management expect the remainder
             * of the command response to be present after p_read[count],
             * usually some carriage return, null terminated. This is not
             * the case for the sms prompt.)
             */
            p_read[count+1] = '\0';

            // skip over leading newlines
            while (*handle->ATBufferCur == '\r' || *handle->ATBufferCur == '\n')
                handle->ATBufferCur++;

            p_eol = findNextEOL(handle->ATBufferCur);
            p_read += count;
        } else if (count <= 0) {
            /* read error encountered or EOF reached */
            if(count == 0) {
                ALOGE("atchannel(%d): EOF reached", handle->fd);
            } else {
                ALOGE("atchannel(%d): read error %d: %s\n",
                        handle->fd, errno, strerror(errno));
            }
            return NULL;
        }
    }

    /* a full line in the buffer. Place a \0 over the \r and return */

    ret = handle->ATBufferCur;
    *p_eol = '\0';
    handle->ATBufferCur = p_eol + 1; /* this will always be <= p_read,    */
                              /* and there will be a \0 at *p_read */

    if(handle->type != (ATCommandType)TOFILE)
    {
        ALOGD("AT(%d)< %s\n", handle->fd, ret);
    }
    return ret;
}


static void onReaderClosed(void)
{
    at_channel_handle_t* handle = at_channel_handle_get();
    if ( NULL == handle )
    {
        ALOGE("onReaderClosed() AT channel handle not found");
        return;
    }

    if (handle->onReaderClosed != NULL && handle->readerClosed == 0) {

        pthread_mutex_lock(&handle->commandmutex);

        handle->readerClosed = 1;

        pthread_cond_signal(&handle->commandcond);

        pthread_mutex_unlock(&handle->commandmutex);

        handle->onReaderClosed();
    }
}


static void *readerLoop(void *arg)
{
    if ( 0 == at_channel_handle_set((at_channel_handle_t*)arg) )
    {
        at_channel_handle_t* handle = at_channel_handle_get();
        if ( NULL == handle )
        {
            ALOGE("readerLoop() AT channel handle not found");
            return NULL;
        }

        ALOGD("readerLoop() AT channel handle %p (fd: %d)\n", handle, handle->fd );

        for (;;)
        {
          const char * line;

          line = readline();

          if (line == NULL) {
              break;
          }

          if(handle->isDefaultChannel && isSMSUnsolicited(line)) {
              char *line1;
              const char *line2;

              // The scope of string returned by 'readline()' is valid only
              // till next call to 'readline()' hence making a copy of line
              // before calling readline again.
              line1 = strdup(line);
              line2 = readline();

              if (line2 == NULL) {
                  break;
              }

              if (handle->unsolHandler != NULL) {
                  handle->unsolHandler (line1, line2);
              }
              free(line1);
          } else {
              processLine(line);
          }
      }
      onReaderClosed();
    }

    return NULL;
}

/**
 * Sends string s to the radio with a \r appended.
 * Returns AT_ERROR_* on error, 0 on success
 *
 * This function exists because as of writing, android libc does not
 * have buffered stdio.
 */
static int writeline (const char *s, const char *s_to_log)
{
    size_t cur = 0;
    size_t len = strlen(s);
    ssize_t written;

    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("writeline() AT channel handle not found");
        return AT_ERROR_CHANNEL_CLOSED;
    }

    if (handle->fd < 0 || handle->readerClosed > 0) {
        return AT_ERROR_CHANNEL_CLOSED;
    }

    if(s_to_log)
    {
        /* Some AT commands may contain information that we don't want to display in the log
          * such as pin code.
          * In that case provide a way to log a " modified"  string instead of the real command
          * if s_to_log is not provided then display the original string itself.
          */
         ALOGD("AT(%d)> %s\n", handle->fd, s_to_log);
         AT_DUMP( ">> ", s_to_log, strlen(s_to_log) );
    }
    else
    {
        ALOGD("AT(%d)> %s\n", handle->fd, s);
        AT_DUMP( ">> ", s, strlen(s) );
    }

    /* the main string */
    while (cur < len) {
        do {
            written = write (handle->fd, s + cur, len - cur);
        } while (written < 0 && errno == EINTR);

        if (written < 0) {
            return AT_ERROR_GENERIC;
        }

        cur += written;
    }

    /* the \r  */

    do {
        written = write (handle->fd, "\r" , 1);
    } while ((written < 0 && errno == EINTR) || (written == 0));

    if (written < 0) {
        return AT_ERROR_GENERIC;
    }

    return 0;
}
static int writeCtrlZ (const char *s)
{
    size_t cur = 0;
    size_t len = strlen(s);
    ssize_t written;

    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("writeCtrlZ() AT channel handle not found");
        return AT_ERROR_CHANNEL_CLOSED;
    }

    if (handle->fd < 0 || handle->readerClosed > 0) {
        return AT_ERROR_CHANNEL_CLOSED;
    }

    ALOGD("AT> %s^Z\n", s);

    AT_DUMP( ">* ", s, strlen(s) );

    /* the main string */
    while (cur < len) {
        do {
            written = write (handle->fd, s + cur, len - cur);
        } while (written < 0 && errno == EINTR);

        if (written < 0) {
            return AT_ERROR_GENERIC;
        }

        cur += written;
    }

    /* the ^Z  */

    do {
        written = write (handle->fd, "\032" , 1);
    } while ((written < 0 && errno == EINTR) || (written == 0));

    if (written < 0) {
        return AT_ERROR_GENERIC;
    }

    return 0;
}

static void clearPendingCommand(void)
{
    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("clearPendingCommand() AT channel handle not found");
        return;
    }

    if (handle->response != NULL) {
        at_response_free(handle->response);
    }

    handle->response = NULL;
    handle->responsePrefix = NULL;
    handle->smsPDU = NULL;
}

/**
 * Starts AT handler on stream "fd'
 * returns 0 on success, -1 on error
 */
int at_open(int fd, ATUnsolHandler h, int default_channel, long long timeout)
{
    int ret;
    pthread_attr_t attr;

    at_channel_handle_t* handle = at_channel_handle_create();

    ALOGD("created AT channel handle %p for fd %d with timeout %lld\n", handle, fd, timeout );

    handle->fd = fd;
    handle->unsolHandler = h;
    handle->readerClosed = 0;
    handle->isDefaultChannel = default_channel;
    handle->responsePrefix = NULL;
    handle->smsPDU = NULL;
    handle->response = NULL;
    handle->ATCmdTimeout = timeout;
    handle->isInErrorRecovery = 0;

    ALOGD("created AT channel handle %p for fd %d with timeout %lld, err rec: %d\n",
         handle, handle->fd, handle->ATCmdTimeout,  handle->isInErrorRecovery );

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&handle->tid_reader, &attr, readerLoop, (void*) handle);

    if (ret < 0) {
        perror ("pthread_create");
        return -1;
    }


    return 0;
}

/* FIXME is it ok to call this from the reader and the command thread? */
void at_close()
{
    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("at_close() AT channel handle not found");
        return;
    }

    if (handle->fd >= 0) {
        close(handle->fd);
    }
    handle->fd = -1;

    pthread_mutex_lock(&handle->commandmutex);

    handle->readerClosed = 1;

    pthread_cond_signal(&handle->commandcond);

    pthread_mutex_unlock(&handle->commandmutex);

    ALOGE("at_close() destroying handle\n");
    at_channel_handle_set((at_channel_handle_t*)NULL); /* unregister handle from channel */
    at_channel_handle_destroy(handle); /* destroy handle */

    /* the reader thread should eventually die */
}

static ATResponse * at_response_new(void)
{
    return (ATResponse *) calloc(1, sizeof(ATResponse));
}

void at_response_free(ATResponse *p_response)
{
    ATLine *p_line;

    if (p_response == NULL) return;

    p_line = p_response->p_intermediates;

    while (p_line != NULL) {
        ATLine *p_toFree;

        p_toFree = p_line;
        p_line = p_line->p_next;

        free(p_toFree->line);
        free(p_toFree);
    }

    free (p_response->finalResponse);
    free (p_response);
}

/**
 * The line reader places the intermediate responses in reverse order
 * here we flip them back
 */
static void reverseIntermediates(ATResponse *p_response)
{
    ATLine *pcur,*pnext;

    pcur = p_response->p_intermediates;
    p_response->p_intermediates = NULL;

    while (pcur != NULL) {
        pnext = pcur->p_next;
        pcur->p_next = p_response->p_intermediates;
        p_response->p_intermediates = pcur;
        pcur = pnext;
    }
}

/**
 * Internal send_command implementation
 * Doesn't lock or call the timeout callback
 *
 * timeoutMsec == 0 means infinite timeout
 */

static int at_send_command_full_nolock (const char *command, ATCommandType type,
                    const char *responsePrefix, const char *smspdu,
                    long long timeoutMsec, ATResponse **pp_outResponse, const char *command_to_log)
{
    int err = 0;
#ifndef USE_NP
    struct timespec ts;
#endif /*USE_NP*/

    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("at_send_command_full_nolock() handle not found");
        err = AT_ERROR_COMMAND_PENDING;
        goto error;
    }

    if(handle->response != NULL) {
        err = AT_ERROR_COMMAND_PENDING;
        goto error;
    }

    atCmdSet(command);
    err = writeline (command, command_to_log);

    if (err < 0) {
        goto error;
    }

    handle->type = type;
    handle->responsePrefix = responsePrefix;
    handle->smsPDU = smspdu;
    handle->response = at_response_new();

    if(handle->response == NULL) {
        ALOGE("at_send_command_full_nolock() unable to allocate memory for response of cmd %s", command);
        err = AT_ERROR_GENERIC;
        goto error;
    }

#ifndef USE_NP
    if (timeoutMsec != 0) {
        setTimespecRelative(&ts, timeoutMsec);
    }
#endif /*USE_NP*/

    while (handle->response->finalResponse == NULL && handle->readerClosed == 0) {
        if (timeoutMsec != 0) {
#ifdef USE_NP
            err = pthread_cond_timeout_np(&handle->commandcond, &handle->commandmutex, timeoutMsec);
#else
            err = pthread_cond_timedwait(&handle->commandcond, &handle->commandmutex, &ts);
#endif /*USE_NP*/
        } else {
            err = pthread_cond_wait(&handle->commandcond, &handle->commandmutex);
        }

        if (err == ETIMEDOUT) {
            /* Clear pending command. */
            atCmdSet(NULL);
            err = AT_ERROR_TIMEOUT;
            goto error;
        }
    }

    if (pp_outResponse == NULL) {
        at_response_free(handle->response);
    } else {
        /* line reader stores intermediate responses in reverse order */
        reverseIntermediates(handle->response);
        *pp_outResponse = handle->response;
    }

    handle->response = NULL;

    if(handle->readerClosed > 0) {
        err = AT_ERROR_CHANNEL_CLOSED;
        goto error;
    }

    err = 0;
error:
    clearPendingCommand();

    return err;
}

/**
 * Internal send_command implementation
 *
 * timeoutMsec == 0 means infinite timeout
 */
static int at_send_command_full (const char *command, ATCommandType type,
                    const char *responsePrefix, const char *smspdu,
                    long long timeoutMsec, ATResponse **pp_outResponse, const char *command_to_log)
{
    int err;

    /* Null pointer is tested in some case as a success status for the operation */
    if ( pp_outResponse != NULL )
    {
        *pp_outResponse = NULL;
    }

    at_channel_handle_t* handle = at_channel_handle_get();
    if ( NULL == handle )
    {
        ALOGE("at_send_command_full() handle not found for cmd %s", command);
        return AT_ERROR_GENERIC;
    }

    if (0 != pthread_equal(handle->tid_reader, pthread_self())) {
        /* cannot be called from reader thread */
        ALOGE("at_send_command_full() cannot be called from reader thread\n");
        return AT_ERROR_INVALID_THREAD;
    }

    if ( 0 != handle->isInErrorRecovery )
    {
        ALOGE("at_send_command_full(%d) error recovery mode %d\n",
            handle->fd, handle->isInErrorRecovery);
        return handle->isInErrorRecovery;
    }

    if ( 0 == timeoutMsec )
    {
        /* only block if nothing else is configured for the channel */
        timeoutMsec = handle->ATCmdTimeout;
    }

    pthread_mutex_lock(&handle->commandmutex);

    err = at_send_command_full_nolock(command, type,
                    responsePrefix, smspdu,
                    timeoutMsec, pp_outResponse, command_to_log);

    pthread_mutex_unlock(&handle->commandmutex);

    if (err == AT_ERROR_TIMEOUT && handle->onTimeout != NULL) {
        ALOGE("at_send_command_full(%d) timeout error occured\n", handle->fd);
        handle->isInErrorRecovery = AT_ERROR_TIMEOUT;
        handle->onTimeout();
    }

    return err;
}


/**
 * Issue a single normal AT command with no intermediate response expected
 *
 * "command" should not include \r
 * pp_outResponse can be NULL
 *
 * if non-NULL, the resulting ATResponse * must be eventually freed with
 * at_response_free
 */
int at_send_command (const char *command, ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, NO_RESULT, NULL,
                                    NULL, 0, pp_outResponse,NULL);

    return err;
}


int at_send_command_secure (const char *command, const char *command_to_log, ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, NO_RESULT, NULL,
                                    NULL, 0, pp_outResponse,command_to_log);

    return err;
}


int at_send_command_singleline (const char *command,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, SINGLELINE, responsePrefix,
                                    NULL, 0, pp_outResponse,0);

    if (err == 0 && pp_outResponse != NULL
        && (*pp_outResponse)->success > 0
        && (*pp_outResponse)->p_intermediates == NULL
    ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


int at_send_command_singleline_secure (const char *command,
                                const char *command_to_log,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, SINGLELINE, responsePrefix,
                                    NULL, 0, pp_outResponse,command_to_log);

    if (err == 0 && pp_outResponse != NULL
        && (*pp_outResponse)->success > 0
        && (*pp_outResponse)->p_intermediates == NULL
    ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}



int at_send_command_numeric (const char *command,
                                 ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, NUMERIC, NULL,
                                    NULL, 0, pp_outResponse,NULL);

    if (err == 0 && pp_outResponse != NULL
        && (*pp_outResponse)->success > 0
        && (*pp_outResponse)->p_intermediates == NULL
    ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


int at_send_command_sms (const char *command,
                                const char *pdu,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, SINGLELINE, responsePrefix,
                                    pdu, 0, pp_outResponse,NULL);

    if (err == 0 && pp_outResponse != NULL
        && (*pp_outResponse)->success > 0
        && (*pp_outResponse)->p_intermediates == NULL
    ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


int at_send_command_multiline (const char *command,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, MULTILINE, responsePrefix,
                                    NULL, 0, pp_outResponse,NULL);

    return err;
}

int at_send_command_multiline_no_prefix (const char *command,
                                   ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, MULTILINE_NO_PREFIX, NULL,
                                    NULL, 0, pp_outResponse,NULL);

    return err;
}

/** This callback is invoked on the command thread */
void at_set_on_timeout(void (*onTimeout)(void))
{
    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("at_set_on_timeout() handle not found");
        return;
    }

    handle->onTimeout = onTimeout;
}

/**
 * Issue a an AT command and pipe the output to a file
 *
 * "command" should not include \r
 * file must be a path to a file to be created
 */
int at_send_command_tofile (const char *command, const char *file)
{
    int err;
    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("at_send_command_tofile() handle not found for cmd %s", command);
        return AT_ERROR_GENERIC;
    }

    handle->filename = file;
    handle->fileout = fopen(file,"w+");

    if(handle->fileout  == NULL)
    {
        /* Do not invoke the AT command if unable to open destination file*/
        ALOGE("TOFILE: unable to open output file (%s)", handle->filename);
        err = 1;
    }
    else
    {
        err = at_send_command_full (command, TOFILE, NULL,
                                    NULL, 0, NULL, NULL);
        fclose(handle->fileout);
    }

    handle->filename = NULL;
    handle->fileout = NULL;
    return err;
}
/**
 *  This callback is invoked on the reader thread (like ATUnsolHandler)
 *  when the input stream closes before you call at_close
 *  (not when you call at_close())
 *  You should still call at_close()
 */

void at_set_on_reader_closed(void (*onClose)(void))
{
    at_channel_handle_t* handle = at_channel_handle_get();

    if ( NULL == handle )
    {
        ALOGE("at_set_on_reader_closed() handle not found");
        return;
    }

    handle->onReaderClosed = onClose;
}


/**
 * Periodically issue an AT command and wait for a response.
 * Used to ensure channel has start up and is active
 */

int at_handshake()
{
    int i;
    int err = 0;

    at_channel_handle_t* handle = at_channel_handle_get();

    if(handle == NULL)
    {
        ALOGE("at_handshake() handle not found");
        return AT_ERROR_GENERIC;
    }

    if (0 != pthread_equal(handle->tid_reader, pthread_self())) {
        /* cannot be called from reader thread */
        return AT_ERROR_INVALID_THREAD;
    }

    pthread_mutex_lock(&handle->commandmutex);

    for (i = 0 ; i < HANDSHAKE_RETRY_COUNT ; i++) {
        /* some stacks start with verbose off */
        err = at_send_command_full_nolock ("ATE0Q0V1", NO_RESULT,
                    NULL, NULL, HANDSHAKE_TIMEOUT_MSEC, NULL, NULL);

        if (err == 0) {
            break;
        }
    }

    pthread_mutex_unlock(&handle->commandmutex);

    if (err == 0) {
        /* pause for a bit to let the input buffer drain any unmatched OK's
           (they will appear as extraneous unsolicited responses) */

        sleepMsec(HANDSHAKE_TIMEOUT_MSEC);
    }

    return err;
}

/**
 * Returns error code from response
 * Assumes AT+CMEE=1 (numeric) mode
 */
AT_CME_Error at_get_cme_error(const ATResponse *p_response)
{
    int ret;
    int err;
    char *p_cur;

    if (p_response->success > 0) {
        return CME_SUCCESS;
    }

    if (p_response->finalResponse == NULL
        || !(strStartsWith(p_response->finalResponse, "+CME ERROR:") || strStartsWith(p_response->finalResponse, "+CCHC ERROR:"))
    ) {
        return CME_ERROR_NON_CME;
    }

    p_cur = p_response->finalResponse;
    err = at_tok_start(&p_cur);

    if (err < 0) {
        return CME_ERROR_NON_CME;
    }

    err = at_tok_nextint(&p_cur, &ret);

    if (err < 0) {
        return CME_ERROR_NON_CME;
    }

    return (AT_CME_Error) ret;
}

/**
 * Returns CMS error code from response
 * Assumes AT+CMEE=1 (numeric) mode
 */
AT_CMS_Error at_get_cms_error(const ATResponse *p_response)
{
    int ret;
    int err=CMS_SUCCESS;
    char *p_cur;

    if (p_response->success > 0) {
        return CMS_SUCCESS;
    }

    if (p_response->finalResponse == NULL
        || !strStartsWith(p_response->finalResponse, "+CMS ERROR:")
    ) {
        return CMS_ERROR_NON_CMS;
    }

    p_cur = p_response->finalResponse;
    err = at_tok_start(&p_cur);

    if (err < 0) {
        return CMS_ERROR_NON_CMS;
    }

    err = at_tok_nextint(&p_cur, &ret);

    if (err < 0) {
        return CMS_ERROR_NON_CMS;
    }
    if (ret != CMS_ERROR_NETWORK_TIMEOUT &&
        ret != CMS_ERROR_NO_NETWORK_SERVICE &&
        ret != CMS_ERROR_FDN_FAILURE &&
        ret != CMS_ERROR_MEMORY_FULL)
        {
        ret = CMS_ERROR_UNKNOWN_ERROR;
        }

    return (AT_CMS_Error) ret;
}

/*
 * Check if the current channel has been configured for UCS2.
 **/
int at_is_channel_UCS2(void) {
    ATResponse *p_response = NULL;
    int err;
    char *line;
    char *format;
    int res = 0; /* By default consider this is not UCS2. */

    err = at_send_command_singleline("AT+CSCS?", "+CSCS:", &p_response);
    if((err<0)||(p_response->success== 0)||(p_response->p_intermediates->line == NULL))
        goto done;

    line =   p_response->p_intermediates->line;
    err = at_tok_start (&line);
    if(err<0)
        goto done;

    err = at_tok_nextstr(&line, &format);
    if(err<0)
        goto done;

    if (strstr(format, "UCS2")) {
        res = 1;
    }

done:
    at_response_free(p_response);
    return res;
}
