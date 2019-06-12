/* Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved. */
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

#include <telephony/ril.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <alloca.h>
#include <time.h>
#include "atchannel.h"
#include "at_tok.h"
#include "misc.h"
#include <getopt.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <cutils/properties.h>
#include <termios.h>
#include <icera-ril.h>
#include <icera-ril-net.h>
#include <icera-ril-data.h>
#include <icera-ril-call.h>
#include <icera-ril-sms.h>
#include <icera-ril-sim.h>
#include <icera-ril-stk.h>
#include <icera-ril-services.h>
#include <icera-ril-utils.h>
#include <icera-ril-power.h>
#include "icera-util.h"
#include "icera-ril-logging.h"
#include "icera-ril-extensions.h"
#define LOG_TAG rilLogTag
#include <utils/Log.h>
#include <icera-ril-utils.h>

#include "at_channel_writer.h"
#include "at_channel_config.h"
/* Modem software version */
#include "version.h"


#define DEFAULT_RIL_DEVICE_NAME "/dev/ttyACM0"

static void onRequest (int request, void *data, size_t datalen, RIL_Token t);
static int onSupports (int requestCode);
static void onCancel (RIL_Token t);
static const char *getVersion(void);
static int isRadioOn(void);
static void unsolNewSmsOnSim(const char *s);

extern char * requestToString(int request);
static char * internalRequestToString(int request);
static const char * rilRequestToString(int request);

static int isRunningOnEmulator( void );
static int isRunningOnLinuxEmu( void );
static void* error_recovery_worker( void* param );
static int error_recovery_handler( void* param );

#ifdef ECC_VERIFICATION
extern int isEmergencyCall;
extern int isEccVerificationOn;
#endif

extern int collateNetworkList;

/*** Static Variables ***/
static const RIL_RadioFunctions s_callbacks = {
    ANNOUNCED_RIL_VERSION,
    onRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion
};

const struct RIL_Env *s_rilenv;

#define MAX_AT_CHANNELS 4 /* default channel plus three additional channel */
static pthread_t s_tid[MAX_AT_CHANNELS];
static struct _at_channel_writer_handle_t* s_h_channel[MAX_AT_CHANNELS];
static struct _rilq_handle_t* s_h_queue[MAX_AT_CHANNELS];
static int s_used_channels = 0;

static RIL_RadioState sState = RADIO_STATE_UNAVAILABLE;

static pthread_mutex_t s_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_state_cond = PTHREAD_COND_INITIALIZER;

static int s_port = -1;
static char s_device_path[PROPERTY_VALUE_MAX];
static int          s_device_socket = 0;

/* trigger change to this with s_state_cond */
static int s_closed = 0;
static int s_baseband_mode = -1;

static const struct timeval TIMEVAL_SIMPOLL = {1,0};
static const struct timeval TIMEVAL_CALLSTATEPOLL = {0,500000};
static const struct timeval TIMEVAL_0 = {0,0};

static ril_request_object_t* StoredQueryCallForwardStatus = NULL;

int ipv6_supported = 0;

/* Can be overridden on command line to force HSPA+ detection */
int UseIcti = 1;

/* LocUpdate supported by default, can be overridden on command line */
static int DefaultLocUpdateEnabled = 1;

/* Logging tags */
char *rilLogTag ="RIL[X]" ;
char *atLogTag = "";
char *atchLogTag = "";
char *atccLogTag = "";
char *atcwLogTag = "";
char *req0LogTag = "";
char *reqqLogTag = "";
char *unsoLogTag = "";
char *oemLogTag  = "";

/* Timeout for fast dormancy
 * Set through the -f option in ril starting parameter,
 * loaded at startup, needs RIL restart when modified.
 *
 */
static int  FDTO_ScreenOn = 0;
static int  FDTO_ScreenOff = 0;

// error recovery flag;
// zero if normal mode, nonzero if error recovery is in progress
static int s_nInErrRecovery = 0;
static pthread_mutex_t s_err_recovery_lock = PTHREAD_MUTEX_INITIALIZER;

void getFDTimeoutValues(int *ScreenOff, int *ScreenOn) {
    if (ScreenOn != NULL) {
        *ScreenOn = FDTO_ScreenOn;
    }
    if(ScreenOff != NULL) {
        *ScreenOff = FDTO_ScreenOff;
    }
}

/** do post- SIM ready initialization
 * EVENT: On Sim Ready
 */
static void onSIMReady(void)
{
    at_send_command_singleline("AT+CSMS=1", "+CSMS:", NULL);
    /*
     * Always send SMS messages directly to the TE
     *
     * mode = 1 // discard when link is reserved (link should never be
     *             reserved)
     * mt = 2   // most messages routed to TE
     * bm = 2   // new cell BM's routed to TE
     * ds = 1   // Status reports routed to TE
     * bfr = 1  // flush buffer
     */
    at_send_command("AT+CNMI=1,2,2,1,0", NULL);

    /* Subscribe to SIM insertion/removal reporting.
     *
     */
    at_send_command("AT*TSIMINS=1", NULL);

    /* In case Stk request was received when SIM not ready so process it now. */
    if(!stk_use_cusat_api)
    {
        StkNotifyServiceIsRunning(0);
    }

    rilExtOnSimReady();
}


/** do post-AT+CFUN=1 initialization
 * EVENT: On radio popwer on
 */
static void onRadioPowerOn(void)
{
    /* Force an arbitrary sim status update*/
    pollSIMState(NULL);

    checkRtcUpdate();

    /* force LTE auto connect notifications on fast channel */
    at_send_command("AT%IPDPACTU=1",NULL);

    /*  Enable unsolicited to detect network orginated detach */
    at_send_command("AT+CGEREP=1", NULL);

    /* Enable unsolicited for RRC state */
    at_send_command("AT%IPSRB=1", NULL);

    if(!stk_use_cusat_api)
    {
        /* Icera specific -- STK needs to be all mannual */
        at_send_command("AT*TSTAUTO=0,0,0,0", NULL);
    }
    else
    {
        /*
         * This being sent in cfun: 0 may not be sufficient
         * Let's make sure all is in place by resending it now,
         */
        at_send_command("AT+CUSATA=1", NULL);
    }

    /*  Network registration events */
    InitializeNetworkUnsols(DefaultLocUpdateEnabled);

    /* Set notification to match current screen state */
    UpdateScreenState(FDTO_ScreenOff, FDTO_ScreenOn);

    /* Icera specific -- enable NITZ unsol notifs */
    at_send_command("AT*TLTS=1", NULL);

    /* use PS and CS */
    at_send_command("AT%INWMODE=2,2", NULL);

    /* Configure 3G SMS to use CS */
    at_send_command("AT+CGSMS=1", NULL);

#ifdef NO_LTE_SUPPORT
    int mode;
    GetCurrentNetworkMode(&mode,NULL);
    switch(mode)
    {
        case PREF_NET_TYPE_LTE_GSM_WCDMA:
        case PREF_NET_TYPE_LTE_ONLY:
            /* Disable LTE BAND only in this case */
            at_send_command("AT%INWMODE=0,UG,1",NULL);
            break;
        default:
            break;
    }
#endif

    /*  Call Waiting notifications */
    at_send_command("AT+CCWA=1", NULL);

    /*  +CSSU unsolicited supp service notifications */
    at_send_command("AT+CSSN=0,1", NULL);

    /*  USSD unsolicited */
    at_send_command("AT+CUSD=1", NULL);

    /* Report current call state */
    at_send_command("AT%ILCC=1", NULL);

    /* Automatic activation of incoming PDP*/
    at_send_command("AT+CGAUTO=1", NULL);

    /* Network restrictions unsolicited and status update */
    at_send_command("AT%INWRESTRICTU=1", NULL);
    at_send_command("AT%INWRESTRICTU", NULL);

    /* Enable signal strength notification*/
    UnsolSignalStrengthInit();

    /*
     * Force a sim check in case we've missed a notification
     */
    pollSIMState(NULL);
}

void requestPutObject(void *param)
{
    rilq_put_object(s_h_queue[0], param, RILQ_OBJECT_TYPE_REQUEST);
}

#define DRIFT_TOLERANCE 10
void checkRtcUpdate(void) {
    struct timeval tv;
    struct timezone tz;
    struct timespec tp;

    static int oldDelta = 0;
    int delta = 0;
    int err = gettimeofday(&tv, &tz);
    int err2 = clock_gettime(CLOCK_MONOTONIC,&tp);

    if(err || err2) {
        ALOGE("Error retrieving timestamps");
        return;
    }
    delta = tp.tv_sec - tv.tv_sec;

    if (abs(delta-oldDelta) < DRIFT_TOLERANCE) {
        return;
    }
    ALOGD("Time changed: %d", delta-oldDelta);

    char * cmd = NULL;

    if(err == 0)
    {
        char timeStringBuffer[100];
        struct tm *timeptr;
        timeptr = localtime(&tv.tv_sec);
        if((timeptr->tm_year < 100)||(timeptr->tm_year > 199)) {
            ALOGE("checkRtcUpdate: current date is invalid");
            return;
        }

        strftime(timeStringBuffer, sizeof(timeStringBuffer), "%y/%m/%d,%H:%M:%S", timeptr);
        /*
         * sprintf format not complete on this libc, so has to do some gymnastics
         * %+02d should suffice, but not supported, so instead we do that...
         * Note that localtime returns minutes west of Greenwich, which run the opposite
         * way to GMT.
         */
        int timeZoneDelta = tz.tz_minuteswest/15;
        asprintf(&cmd, "AT+CCLK=\"%s%s%s%d\"",
            timeStringBuffer,
            (timeZoneDelta>0)?"-":"+",
            (abs(timeZoneDelta)<10)?"0":"",
            abs(timeZoneDelta));

        at_send_command(cmd,NULL);
        free(cmd);
    }
    oldDelta = delta;
}

/**
 * RIL_REQUEST_RADIO_POWER
 *
 * Toggle radio on and off (for "airplane" mode)
 * If the radio is is turned off/on the radio modem subsystem
 * is expected return to an initialized state. For instance,
 * any voice and data calls will be terminated and all associated
 * lists emptied.
 *
 * "data" is int *
 * ((int *)data)[0] is > 0 for "Radio On"
 * ((int *)data)[0] is == 0 for "Radio Off"
 *
 * "response" is NULL
 *
 * Turn radio on if "on" > 0
 * Turn radio off if "on" == 0
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */

static void requestRadioPower(void *data, size_t datalen, RIL_Token t)
{
    int onOff;
    int err;

    assert (datalen >= sizeof(int *));
    onOff = ((int *)data)[0];

    err = rilExtOnRadioPower(onOff);

    /* There was an error in the ext hook */
    if(err < 0) {
        goto error;
    }

    /* Ext hook say we should run the default handler */
    if(err == 0) {
        if (onOff == 0 && sState != RADIO_STATE_OFF) {
            /* Re-use the current STK profiles at next reboot*/
            at_send_command("AT+CUSATD=1,0",NULL);

            err = changeCfun(0);
            if (err < 0) goto error;

            setRadioState(RADIO_STATE_OFF);
        } else if (onOff > 0 && sState == RADIO_STATE_OFF) {
            if (rilEDPSetFullPower() < 0) {
                ALOGW("%s: failed to force full power, ignore\n", __func__);
            } else {
                ALOGD("%s: forced full power\n", __func__);
            }

            err = changeCfun(1);
            if (err < 0) {
                // Some stacks return an error when there is no SIM,
                // but they really turn the RF portion on
                // So, if we get an error, let's check to see if it
                // turned on anyway

                if (isRadioOn() != 1) {
                    goto error;
                }
            }
            setRadioState(RADIO_STATE_SIM_NOT_READY);
        }
    }

    /* If user request power off,
     * and the radio really powered down,
     * we should update emergency call list.
     */
    if (onOff == 0 && sState == RADIO_STATE_OFF) {
        checkAndAmendEmergencyNumbers(2, 1); //Set SIM status to unknown & pb loaded to refresh ril.ecclist
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/**
 * RIL_REQUEST_BASEBAND_VERSION
 *
 * Return string value indicating baseband version, eg
 * response from AT+CGMR
 *
 * "data" is NULL
 * "response" is const char * containing version string for log reporting
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */

static void requestBasebandVersion(void *data, size_t datalen, RIL_Token t)
{
    int err;
    ATResponse *p_response = NULL;
    char * response;
    char * release_name;
    char * source_version;
    ATLine * p_cur;

    err = at_send_command_multiline_no_prefix("AT+CGMR", &p_response);
    if ((err != 0) || (p_response->success==0))
        goto error;

    p_cur = p_response->p_intermediates;

    /* Skip the first line */
    if (p_cur->p_next == NULL) {
        goto error;
    }
    p_cur = p_cur->p_next;

    /* Take the second line (Release name) */
    release_name = p_cur->line;


    /*
     * Take the 3rd line as source code version
     * Multi sim concatenate the 2 versions and tries to fit into a property
     * so concatenated string can't be more than 91 char!
     * */
    if (p_cur->p_next == NULL) {
        asprintf(&response, "%.40s", release_name);
    } else {
        p_cur = p_cur->p_next;
        source_version = p_cur->line;
        asprintf(&response, "%.24s\n%.24s", release_name, source_version);
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    at_response_free(p_response);
    free(response);
    return;

error:
    at_response_free(p_response);
    ALOGE("ERROR: requestBasebandVersion failed\n");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/*
 * RIL_REQUEST_DEVICE_IDENTITY
 *
 * Request the device ESN / MEID / IMEI / IMEISV.
 *
 */
static void requestDeviceIdentity(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response1 = NULL;
    ATResponse *p_response2 = NULL;
    char* response[4];
    int err;

    /* IMEI */
    err = at_send_command_numeric("AT+CGSN", &p_response1);

    if (err < 0 || p_response1->success == 0) {
        goto error;
    }
    asprintf(&response[0], "%s", p_response1->p_intermediates->line);

    /* IMEISV */
    err = at_send_command_numeric("AT%IIMEISV?", &p_response2);

    if (err < 0 || p_response2->success == 0
        || (strlen (p_response2->p_intermediates->line)!=16)) {
        goto error;
    }
    asprintf(&response[1], "%s", &p_response2->p_intermediates->line[14]);

    /* CDMA not supported */
    response[2] = NULL;
    response[3] = NULL;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));

    at_response_free(p_response1);
    at_response_free(p_response2);
    free(response[0]);
    free(response[1]);
    return;

error:
    at_response_free(p_response1);
    at_response_free(p_response2);
    ALOGE("ERROR: requestDeviceIdentity() failed\n");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void countHeldAndWaitingCalls(int *held_count_ptr, int *waiting_count_ptr)
{
    int err;
    int countCalls = 0;
    ATResponse *p_response=NULL;
    ATLine *p_cur;

    assert( held_count_ptr != NULL );
    assert( waiting_count_ptr != NULL );

    *held_count_ptr = 0;
    *waiting_count_ptr = 0;

    err = at_send_command_multiline ("AT+CLCC", "+CLCC:", &p_response);

    if (err != 0 || p_response->success == 0) {
        goto error;
    }

    /* count the waiting/held calls */
    for (countCalls = 0, p_cur = p_response->p_intermediates
            ; p_cur != NULL
            ; p_cur = p_cur->p_next
    ) {
        char *line = p_cur->line;
        int flag;

        /* expected line: +CLCC:<id>,<dir>,<stat>,<mode>,...
           where stat=0: active, 1:held, 2:dialing, 3:alerting, 4:incoming, 5:waiting
        */

        err = at_tok_start(&line);
        if (err < 0)
            goto error;

        /* skip id */
        err = at_tok_nextint(&line, &flag);
        if (err < 0)
            goto error;

        /* skip dir */
        err = at_tok_nextint(&line, &flag);
        if (err < 0)
            goto error;

        /* get stat */
        err = at_tok_nextint(&line, &flag);
        if (err < 0)
            goto error;

        if (flag==1)
        {
            /* held call */
            *held_count_ptr=*held_count_ptr+1;
        }

        if (flag==5)
        {
            /* waiting call */
            *waiting_count_ptr=*waiting_count_ptr+1;
        }
    }
error:
    at_response_free(p_response);
}

/**
 * RIL_REQUEST_GET_IMEI
 *  - DEPRECATED
 *
 * Get the device IMEI, including check digit
 *
 * The request is DEPRECATED, use RIL_REQUEST_DEVICE_IDENTITY
 * Valid when RadioState is not RADIO_STATE_UNAVAILABLE
 *
 * "data" is NULL
 * "response" is a const char * containing the IMEI
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE
 */
static void requestGetIMEI(RIL_Token t)
{
    int err = 0;
    ATResponse *p_response = NULL;
    err = at_send_command_numeric("AT+CGSN", &p_response);

    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS,
                p_response->p_intermediates->line, sizeof(char *));
    }
    at_response_free(p_response);
    return;
}

/**
 * RIL_REQUEST_GET_IMEISV
 *  - DEPRECATED
 *
 * Get the device IMEISV, which should be two decimal digits
 *
 * The request is DEPRECATED, use RIL_REQUEST_DEVICE_IDENTITY
 * Valid when RadioState is not RADIO_STATE_UNAVAILABLE
 *
 * "data" is NULL
 * "response" is a const char * containing the IMEISV
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE
 */
static void requestGetIMEISV(RIL_Token t)
{
    int err = 0;
    ATResponse *p_response = NULL;
    err = at_send_command_numeric("AT%IIMEISV?", &p_response);

    if (err < 0 || p_response->success == 0
        ||(strlen(p_response->p_intermediates->line)!=16)) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS,
                &p_response->p_intermediates->line[14], sizeof(char *));
    }
    at_response_free(p_response);
    return;
}

static const char * rilRequestToString(int request)
{
    const char * requestString = NULL;

    requestString = internalRequestToString(request);

    if(requestString==NULL)
    {
        requestString = rilExtRequestToString(request);
    }

    if(requestString == NULL)
    {
        requestString = requestToString(request);
    }

    /*
     * requestToString in Libril never returns NULL today
     * We return "<unknown request>" if the request hasn't
     * match anything.
     */
    if(requestString == NULL)
    {
        requestString = "<Unknonw request>";
    }

    return requestString;
}
/*
    To regenerate:
    cat ril_internal_commands.h \
    | egrep "^ *{IRIL_" \
    | sed -re 's/\{IRIL_REQUEST_ID_([^,]+),[^,]+,([^}]+).+/case IRIL_REQUEST_ID_\1: return "[INTERNAL]\1";/'
*/
static char * internalRequestToString(int request)
{
    switch(request)
    {
        case IRIL_REQUEST_ID_SIM_POLL: return "[INTERNAL]SIM_POLL";
        case IRIL_REQUEST_ID_STK_PRO: return "[INTERNAL]STK_PRO";
        case IRIL_REQUEST_ID_NETWORK_TIME: return "[INTERNAL]NETWORK_TIME";
        case IRIL_REQUEST_ID_RESTRICTED_STATE: return "[INTERNAL]RESTRICTED_STATE";
        case IRIL_REQUEST_ID_PROCESS_DATACALL_EVENT: return "[INTERNAL]PROCESS_DATACALL_EVENT";
        case IRIL_REQUEST_ID_UPDATE_SIM_LOADED: return "[INTERNAL]UPDATE_SIM_LOADED";
        case IRIL_REQUEST_ID_SMS_ACK_TIMEOUT:   return "[INTERNAL]SMS_ACK_TIMEOUT";
        case IRIL_REQUEST_ID_FORCE_REATTACH:   return "[INTERNAL]FORCE_REATTACH";
#ifdef ECC_VERIFICATION
        case IRIL_REQUEST_ID_ECC_VERIF_COMPUTE: return "[INTERNAL]ECC_VERIF_COMPUTE";
        case IRIL_REQUEST_ID_ECC_VERIF_RESPONSE: return "[INTERNAL]ECC_VERIF_RESPONSE";
#endif
        case IRIL_REQUEST_ID_SIM_HOTSWAP: return "[INTERNAL]SIM_HOTSWAP";
        case IRIL_REQUEST_ID_GET_IPV6_ADDR: return "[INTERNAL]GET_IPV6_GLOBAL_ADDRESS";
        case IRIL_REQUEST_ID_MODEM_RESTART: return "[INTERNAL]MODEM_RESTART";
        default:
            return NULL;
    }
}

/*** Callback methods from the RIL library to us ***/

/**
 * Call from RIL to us to make a RIL_REQUEST
 *
 * Must be completed with a call to RIL_onRequestComplete()
 *
 * RIL_onRequestComplete() may be called from any thread, before or after
 * this function returns.
 *
 * Will always be called from the same thread, so returning here implies
 * that the radio is ready to process another command (whether or not
 * the previous command has completed).
 */
static void onRequest (int request, void *data, size_t datalen, RIL_Token t)
{
    struct _ril_request_object_t* req_obj;

    ALOGD("%x onRequest:[%d] %s", (unsigned int)pthread_self(), (t!=NULL)?*(int*)t:-1 , rilRequestToString(request));

    /*
     * In this state, it is very likely that the queues aren't processed
     * (the channel writer has not completed its init sequence !)
     */
    if (sState == RADIO_STATE_UNAVAILABLE)
    {
        if(request == RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING)
        {
            /* We may receive this notification before being able to process it from the queues.
             */
            if(stk_use_cusat_api)
            {
                StkNotifyServiceIsRunning(1);
            }
            else
            {
                requestSTKReportSTKServiceIsRunning(data, datalen, t);
            }
        }
        ALOGE("Not in a state to process the request, discarding.");
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return;
    }

    req_obj = ril_request_object_create(request, data, datalen, t);
    if ( NULL != req_obj )
    {
        at_channel_id_t at_channel_id = at_channel_config_get_channel_for_request(request);
        rilq_object_type_t request_type = RILQ_OBJECT_TYPE_REQUEST;

        /*
         * These requests can really clog the request queue
         * Deprioritize so we can deal with other request that
         * may be more urgent.
         */
        if((request == RIL_REQUEST_SIM_IO)
         ||(request == RIL_REQUEST_SEND_SMS)
         ||(request == RIL_REQUEST_SEND_SMS_EXPECT_MORE)
         ||(request == RIL_REQUEST_DTMF_START)
         ||(request == RIL_REQUEST_DTMF_STOP))
        {
            request_type = RILQ_OBJECT_TYPE_REQUEST_LOWP;
        }

        ALOGI("sending request %s on channel %d", rilRequestToString(request), at_channel_id);
        (void) rilq_put_object(s_h_queue[at_channel_id], req_obj, request_type );
    }
}

static void onHandleRequest (int request, void *data, size_t datalen, void* t)
{
    ATResponse *p_response = NULL;
    int err;


    ALOGD("%x onHandleRequest:[%d] %s", (unsigned int)pthread_self(), (t!=NULL)?*(int*)t:-1, rilRequestToString(request));

    /*
     * Proprietary RIL requests
     */
    if (rilExtOnRequest(request, data, datalen, t))
    {
        /* Request was dealt with, no need to go on */
        ALOGD("%x proprietary request:[%d] %s", (unsigned int)pthread_self(),
              (t!=NULL)?*(int*)t:-1, rilExtRequestToString(request));
        return;
    }

    /*
     * These commands won't accept RADIO_NOT_AVAILABLE, so we just return
     * GENERIC_FAILURE if we're not in a state to process the request.
     */
    if ((sState == RADIO_STATE_UNAVAILABLE)
        && (request == RIL_REQUEST_SCREEN_STATE)) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    /* Ignore all requests except RIL_REQUEST_GET_SIM_STATUS
     * when RADIO_STATE_UNAVAILABLE.
     */
    if (sState == RADIO_STATE_UNAVAILABLE
        && request != RIL_REQUEST_GET_SIM_STATUS
        && request != RIL_REQUEST_OEM_HOOK_STRINGS
        && request != RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING
        && !((request == RIL_REQUEST_OEM_HOOK_RAW)
             && (((int *)data)[0] == RIL_REQUEST_OEM_HOOK_SHUTDOWN_RADIO_POWER)))
    {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return;
    }

    /* Ignore all non-power requests when RADIO_STATE_OFF
     * (except RIL_REQUEST_GET_SIM_STATUS)
     * We don't always turn the sim off in radio state off,
     * so we should still honor STK and other SIM related requests
     */
    if (sState == RADIO_STATE_OFF
        && !(request == RIL_REQUEST_RADIO_POWER
            || request == RIL_REQUEST_GET_SIM_STATUS
            || request == RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING
            || request == RIL_REQUEST_OEM_HOOK_STRINGS
            || request == RIL_REQUEST_BASEBAND_VERSION
            || request == RIL_REQUEST_GET_IMEI
            || request == RIL_REQUEST_GET_IMEISV
            || request == RIL_REQUEST_STK_GET_PROFILE
            || request == RIL_REQUEST_STK_SET_PROFILE
            || request == RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE
            || request == RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND
            || request == RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM
            || request == RIL_REQUEST_GET_SIM_STATUS
            || request == RIL_REQUEST_ENTER_SIM_PIN
            || request == RIL_REQUEST_ENTER_SIM_PUK
            || request == RIL_REQUEST_ENTER_SIM_PIN2
            || request == RIL_REQUEST_ENTER_SIM_PUK2
            || request == RIL_REQUEST_CHANGE_SIM_PIN
            || request == RIL_REQUEST_CHANGE_SIM_PIN2
            || request == RIL_REQUEST_QUERY_FACILITY_LOCK
            || request == RIL_REQUEST_SET_FACILITY_LOCK
            || request == RIL_REQUEST_SIM_IO
            || request == RIL_REQUEST_SCREEN_STATE
            || request >= IRIL_REQUEST_ID_BASE  /*  White list all internal requests */
            || ((request == RIL_REQUEST_OEM_HOOK_RAW)
                 && (((int *)data)[0] == RIL_REQUEST_OEM_HOOK_SHUTDOWN_RADIO_POWER))))
    {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);

        /* STK terminal request, radio has been switched off
         * make sure that STK session is gracefully terminated
         */
        if (request == RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE
            || request == RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND
            || request == RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS
            || request == RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM)
                  RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END, NULL, 0);
        return;
    }

    switch (request) {
        /* internal requests */
        case IRIL_REQUEST_ID_SIM_POLL:
            pollSIMState(NULL);
            break;
        case IRIL_REQUEST_ID_STK_PRO:
            requestSTKPROReceived(data, datalen, t);
            break;
        case IRIL_REQUEST_ID_PROCESS_DATACALL_EVENT:
            if(datalen != 2*sizeof(int))
            {
                ALOGE("Unexpected datalen for datacall event");
                break;
            }
            int dataCall = ((int*)data)[0];
            int State= ((int*)data)[1];
            ProcessDataCallEvent(dataCall,State);
            break;
        case IRIL_REQUEST_ID_UPDATE_SIM_LOADED:
            /*
             * EVENT: On SIM loaded
             * Configure 3G SMS to use CS in case
             * it failed during the init sequence
             */
            at_send_command("AT+CGSMS=1", NULL);

#ifdef REPORT_CNAP_NAME
            initCnap();
            at_send_command("AT+CNAP=1", NULL);
#endif
            /*
             * Only send this once our internal phonebook access are complete
             * Otherwise there can be interresting race conditions
             */
            rilExtUnsolPhonebookLoaded(NULL);

            /* Re-use the current STK profiles at next SIM restart*/
            at_send_command("AT+CUSATD=1,0",NULL);
            break;
        case IRIL_REQUEST_ID_SMS_ACK_TIMEOUT:
            /*
             * Ack wasn't received within the expected time
             * Modem will have disabled the notification as
             * a consequence. reenable here.
             */
            SmsReenableSupport();

            /* This will release the wakelock,
             * make sure it is done last !
             */
            if(datalen == sizeof(int))
            {
                SmsAckCancelTimeOut(((int*)data)[0]);
            }
            break;
        case IRIL_REQUEST_ID_FORCE_REATTACH:
            {
                /*
                 * EVENT: On NW detach
                 */
                at_send_command("AT+CGATT=1", NULL);
            }
            break;
#ifdef ECC_VERIFICATION
        case IRIL_REQUEST_ID_ECC_VERIF_COMPUTE:
        {
             /*
              * EVENT: Ecc verification procedure
              * #119# in HEX + may need to come from properties eventually
              * So it's easy to OTA...
              */
             char * ussd = "#119#";
             char * ussd_coded;

             if (at_is_channel_UCS2()) {
                 int utf8_len = strlen(ussd);
                 int ucs2_len = UTF8ToUCS2Hex_sizeEstimate(utf8_len);
                 ussd_coded = (char *)calloc(ucs2_len, sizeof(char));
                 UTF8ToUCS2Hex(ussd, ussd_coded, utf8_len, ucs2_len);
             } else {
                 /* encode Hex string to Char string */
                 int len = strlen(ussd);
                 ussd_coded = (char *)malloc(len * 2 + 1);
                 asciiToHex(ussd, ussd_coded, len);
             }

             char * cmd;
             asprintf(&cmd, "AT+CUSD=1,\"%s\"", ussd_coded);
             err = at_send_command(cmd, &p_response);
             free(cmd);
             if (err < 0 || p_response->success == 0)
             {
                 /* Something went wrong, forget about the procedure */
                 isEmergencyCall = 0;
             }
             free(ussd_coded);
             at_response_free(p_response);
        }
        break;
        case IRIL_REQUEST_ID_ECC_VERIF_RESPONSE:
        {
             char * cmd;
             char hexString[7];
             char * UVR = ((char**)data)[0];
             char * UVR_ascii;
             char * coded_string;

             /*Convert UVR to HEX string #xyz# -> 23XXYYZZ23*/
             asprintf(&UVR_ascii, "#%s#", UVR);

             if (at_is_channel_UCS2()) {
                 int utf8_len = strlen(UVR_ascii);
                 int ucs2_len = UTF8ToUCS2Hex_sizeEstimate(utf8_len);
                 coded_string = (char *)calloc(ucs2_len, sizeof(char));
                 UTF8ToUCS2Hex(UVR_ascii, coded_string, utf8_len, ucs2_len);
             } else {
                 /* encode Hex string to Char string */
                 int len = strlen(UVR_ascii);
                 coded_string = (char *)malloc(len * 2 + 1);
                 asciiToHex(UVR_ascii, coded_string, len);
             }
             asciiToHex(UVR, hexString, 3);
             asprintf(&cmd, "AT+CUSD=1,\"%s\"", (char*)coded_string);
             err = at_send_command(cmd, NULL);
             free(UVR);
             free(UVR_ascii);
             free(coded_string);
             free(cmd);

             /* Procedure over, however it went */
             isEmergencyCall = 0;
        }
        break;
#endif
        case IRIL_REQUEST_ID_SIM_HOTSWAP:
        {
            /*
             * EVENT: On sim hotswap
             */
            char * cmd;
            sim_state* simState=((sim_state**)data)[0];
            asprintf(&cmd, "AT%%ISIMDETECT=%d,%d", simState->id, simState->state);
            at_send_command(cmd, NULL);
            free(cmd);
            IceraWakeLockRelease();
            break;
        }
        case IRIL_REQUEST_ID_RTC_UPDATE:
        {
            checkRtcUpdate();
            break;
        }
        case IRIL_REQUEST_ID_GET_IPV6_ADDR:
        {
            if(datalen != 2*sizeof(int))
            {
                ALOGE("Unexpected datalen for get IPV6 address event");
                break;
            }
            int dataCall = ((int*)data)[0];
            int time     = ((int*)data)[1];
            getIpV6Address(dataCall, time);
            break;
        }
        case IRIL_REQUEST_ID_MODEM_RESTART:
           /* Restore last requested CFUN mode. */
           changeCfun(-1);
           break;
        /* Basic Call requests */
        case RIL_REQUEST_DIAL:
            requestDial(data, datalen, t);
            break;
        case RIL_REQUEST_HANGUP:
            requestHangup(data, datalen, t);
            break;
        case RIL_REQUEST_ANSWER:
            requestAnswer(t);
            break;
        case RIL_REQUEST_GET_CURRENT_CALLS:
            requestGetCurrentCalls(data, datalen, t);
            break;
        case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:
            requestLastCallFailCause(data, datalen, t);
            break;

        /* Advanced Call requests */
        case RIL_REQUEST_SET_CALL_WAITING:
            requestSetCallWaiting(data, datalen, t);
            break;
        case RIL_REQUEST_SET_CALL_FORWARD:
            requestSetCallForward(data, t);
            break;
        case RIL_REQUEST_QUERY_CALL_WAITING:
            requestQueryCallWaiting(data, datalen, t);
            break;
        case RIL_REQUEST_QUERY_CALL_FORWARD_STATUS:
            if(PbkIsLoaded())
            {
                ALOGD("pbk is loaded, proceeding");
                requestQueryCallForwardStatus(data, datalen, t);
            }
            else
            {
                /*
                 * Create a copy of the request for when sim phonebook is loaded
                 * Hopefully this won't take longer than the timeout protecting the request framework side
                 */
                ALOGD("pbk is still loading, delaying until completed");
                StoredQueryCallForwardStatus = ril_request_object_create(RIL_REQUEST_QUERY_CALL_FORWARD_STATUS, data, datalen, t);
            }
            break;
        case RIL_REQUEST_UDUB:
            requestUDUB(data, datalen, t);
            break;
        case RIL_REQUEST_GET_MUTE:
            requestGetMute(data, datalen, t);
            break;
        case RIL_REQUEST_SET_MUTE:
            requestSetMute(data, datalen, t);
            break;
        case RIL_REQUEST_DTMF:
            requestDTMF(data, datalen, t);
            break;
        case RIL_REQUEST_DTMF_START:
            requestDTMFStart(data, datalen, t);
            break;
        case RIL_REQUEST_DTMF_STOP:
            requestDTMFStop(data, datalen, t);
            break;
        case RIL_REQUEST_SET_CLIR:
          requestSetCLIR(data, datalen, t);
          break;
        case RIL_REQUEST_GET_CLIR:
            requestGetCLIR(data, datalen, t);
            break;
        case RIL_REQUEST_QUERY_CLIP:
            requestQueryCLIP(data, datalen, t);
            break;
        case RIL_REQUEST_SCREEN_STATE:
            requestScreenState(data, datalen, t, FDTO_ScreenOff, FDTO_ScreenOn);
            break;

        /* Multiparty voice call requests */
        case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
            requestHangupWaitingOrBackground(t);
            break;
        case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
            requestHangupForegroundResumeBackground(t);
            break;
        case RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE:
            requestSwitchWaitingOrHoldingAndActive(t);
            break;
        case RIL_REQUEST_CONFERENCE:
            requestConference(data, datalen, t);
            break;
        case RIL_REQUEST_SEPARATE_CONNECTION:
            requestSeparateConnection(data, datalen, t);
            break;
        case RIL_REQUEST_EXPLICIT_CALL_TRANSFER:
            requestExplicitCallTransfer(data, datalen, t);
            break;

        /* Data call requests */
        case RIL_REQUEST_SETUP_DATA_CALL:
            checkRtcUpdate();
            requestSetupDataCall(data, datalen, t);
            break;
        case RIL_REQUEST_DEACTIVATE_DATA_CALL:
            requestDeactivateDataCall(data, datalen, t);
            break;
        case RIL_REQUEST_DATA_CALL_LIST:
            requestDataCallList(data, datalen, t);
            break;
        case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:
            requestLastDataFailCause(data, datalen, t);
            break;

        /* SMS requests */
        case RIL_REQUEST_SEND_SMS:
            requestSendSMS(data, datalen, t);
            break;
        case RIL_REQUEST_SEND_SMS_EXPECT_MORE:
            requestSendSMSExpectMore(data, datalen, t);
            break;
        case RIL_REQUEST_SMS_ACKNOWLEDGE:
            requestSMSAcknowledge(data, datalen, t);
            break;
        case RIL_REQUEST_WRITE_SMS_TO_SIM:
            requestWriteSmsToSim(data, datalen, t);
            break;
        case RIL_REQUEST_DELETE_SMS_ON_SIM:
            requestDeleteSmsOnSim(data,datalen,t);
            break;
        case RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG:
            requestGSMGetBroadcastSMSConfig(data, datalen, t);
            break;
        case RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG:
            requestGSMSetBroadcastSMSConfig(data, datalen, t);
            break;
        case RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION:
            requestGSMSMSBroadcastActivation(data, datalen, t);
            break;
        case RIL_REQUEST_REPORT_SMS_MEMORY_STATUS:
            requestReportSmsPhoneMemoryStatus(data, datalen, t);
            break;
        case RIL_REQUEST_GET_SMSC_ADDRESS:
            requestGetSmsCentreAddress(data, datalen, t);
            break;
        case RIL_REQUEST_SET_SMSC_ADDRESS:
            requestSetSmsCentreAddress(data, datalen, t);
            break;

        /* SIM requests */
        case RIL_REQUEST_SIM_IO:
            requestSIM_IO(data,datalen,t);
            break;
        case RIL_REQUEST_GET_SIM_STATUS:
            requestGetSimStatus(data,datalen,t);
            break;
        case RIL_REQUEST_ENTER_SIM_PIN:
        case RIL_REQUEST_ENTER_SIM_PUK:
            requestEnterSimPin(data, datalen, t, request);
            break;
        case RIL_REQUEST_ENTER_SIM_PIN2:
        case RIL_REQUEST_ENTER_SIM_PUK2:
            requestEnterSimPin2(data, datalen, t, request);
            break;
        case RIL_REQUEST_QUERY_FACILITY_LOCK:
            requestQueryFacilityLock(data, datalen, t);
            break;
        case RIL_REQUEST_SET_FACILITY_LOCK:
            requestSetFacilityLock(data, datalen, t);
            break;
        case RIL_REQUEST_GET_IMEI:
            requestGetIMEI(t);
            break;
        case RIL_REQUEST_GET_IMEISV:
            requestGetIMEISV(t);
            break;
        case RIL_REQUEST_CHANGE_SIM_PIN:
            requestChangePasswordPin(data, datalen, t);
            break;
        case RIL_REQUEST_CHANGE_SIM_PIN2:
            requestChangePasswordPin2(data, datalen, t);
            break;
        case RIL_REQUEST_CHANGE_BARRING_PASSWORD:
            requestChangeBarringPassword(data, datalen, t);
            break;

        /* USSD requests */
        case RIL_REQUEST_SEND_USSD:
            requestSendUSSD(data, datalen, t);
            break;
        case RIL_REQUEST_CANCEL_USSD:
            requestCancelUSSD(data, datalen, t);
            break;

        /* Network requests */
        case RIL_REQUEST_SET_BAND_MODE:
            requestSetBandMode(data, datalen, t);
            break;
        case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION:
            requestEnterNetworkDepersonalization(data, datalen, t);
            break;
        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:
            requestSetNetworkSelectionAutomatic(data, datalen, t);
            break;
        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:
            requestSetNetworkSelectionManual(data, datalen, t);
            break;
        case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:
            requestQueryAvailableNetworks(data, datalen, t);
            break;
        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
            requestQueryNetworkSelectionMode(data, datalen, t);
            break;
        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:
            requestSetPreferredNetworkType(data, datalen, t);
            break;
        case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:
            requestGetPreferredNetworkType(data, datalen, t);
            break;
        case RIL_REQUEST_VOICE_REGISTRATION_STATE:
        case RIL_REQUEST_DATA_REGISTRATION_STATE:
            requestRegistrationState(request, data, datalen, t);
            break;
        case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE:
            requestQueryAvailableBandMode(data, datalen, t);
            break;
        case RIL_REQUEST_SET_LOCATION_UPDATES:
            requestSetLocationUpdates(data, datalen, t);
            break;
        case RIL_REQUEST_GET_NEIGHBORING_CELL_IDS:
            requestGetNeighboringCellIds(data, datalen, t);
            break;
        case RIL_REQUEST_GET_CELL_INFO_LIST:
            requestGetCellInfoList(data, datalen, t);
            break;
        case RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE:
            requestSetUnsolCellInfoListRate(data, datalen, t);
            break;

        /* OEM requests */
        case RIL_REQUEST_OEM_HOOK_RAW:
            requestOEMHookRaw(data, datalen, t);
            break;
        case RIL_REQUEST_OEM_HOOK_STRINGS:
            requestOEMHookStrings(data, datalen, t);
            break;
        /* Misc requests */
        case RIL_REQUEST_BASEBAND_VERSION:
            requestBasebandVersion(data, datalen, t);
            break;
        case RIL_REQUEST_SIGNAL_STRENGTH:
            requestSignalStrength(data, datalen, t);
            break;
        case RIL_REQUEST_OPERATOR:
            requestOperator(data, datalen, t);
            break;
        case RIL_REQUEST_RADIO_POWER:
            requestRadioPower(data, datalen, t);
            break;
        case RIL_REQUEST_GET_IMSI:
            /*
             *  RIL_REQUEST_GET_IMSI
             */
            err = at_send_command_numeric("AT+CIMI", &p_response);

            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS,
                    p_response->p_intermediates->line, sizeof(char *));
            }
            at_response_free(p_response);
            break;
        case RIL_REQUEST_DEVICE_IDENTITY:
            requestDeviceIdentity(data, datalen, t);
            break;
        case RIL_REQUEST_SET_TTY_MODE:
            requestSetTtyMode(data, datalen, t);
            break;
        case RIL_REQUEST_QUERY_TTY_MODE:
            requestQueryTtyMode(data, datalen, t);
            break;
        case RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION:
            requestSetSuppSVCNotification(data, datalen, t);
            break;

        /* SIM Application Toolkit */
        case RIL_REQUEST_STK_GET_PROFILE:
            requestStkGetProfile(data, datalen, t);
            break;
        case RIL_REQUEST_STK_SET_PROFILE:
            requestSTKSetProfile(data, datalen, t);
            break;
        case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:
            requestSTKSendEnvelopeCommand(data, datalen, t);
            break;
        case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:
            requestSTKSendTerminalResponse(data, datalen, t);
            break;
        case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM:
            requestStkHandleCallSetupRequestedFromSim(data, datalen, t);
            break;
/* temp workaround: %isimemu +  stk profile download -> Modem crash */
#ifndef LTE_TEST
        case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING:
            requestSTKReportSTKServiceIsRunning(data, datalen, t);
            break;
#endif
        case RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS:
            requestSTKSendEnvelopeCommandWithStatus(data, datalen, t);
            break;

#if RIL_VERSION >= 9
        case RIL_REQUEST_SET_INITIAL_ATTACH_APN:
            requestSetInitialAttachApn(data, datalen, t);
            break;

        /*
         * Sole usage by Android of IMS registration state is currently to
         * route ougoing SMS to either legacy CS SEND_SMS interface or to
         * IMS_SEND_SMS interface, in case IMS SMS service is reported.
         * Such arbitration is under control of the modem, so do not report
         * SMS service over IMS for the moment: IMS SMS will go through CS
         * interface.
         */
        case RIL_REQUEST_IMS_REGISTRATION_STATE:
            {
                int reply[2];
                /* 0==unregistered, 1==registered */
                reply[0] = 0;
                /* FORMAT_3GPP(1) vs FORMAT_3GPP2(2) - N/A if unregistered */;
                reply[1] = 0;
                RIL_onRequestComplete(t, RIL_E_SUCCESS, reply, sizeof(reply));
            }
            break;

        case RIL_REQUEST_IMS_SEND_SMS:
            ALOGE("Unexpected usage of IMS_SEND_SMS request");
            RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
            break;
#endif

        case RIL_REQUEST_VOICE_RADIO_TECH:
            requestVoiceRadioTech(data, datalen, t);
            break;

        case RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU:
            /* Called by Android only in case of SMS data-download.
             * Icera modem filters theses messages and handle them by itself.
             * Android never receive such message.
             * So this request should never be called.
             */
            ALOGE("RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU not expected for Icera modem.");
            RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        break;

        case RIL_REQUEST_ISIM_AUTHENTICATION:
            requestSIMAuthentication(data, datalen, t);
            break;

        default:
            ALOGE("Request %d [%s] not supported",request, rilRequestToString(request));
            RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
            break;
    }
}

void SendStoredQueryCallForwardStatus()
{
    if(StoredQueryCallForwardStatus != NULL)
    {
        ALOGD("Sending stored call forward status query");
        requestPutObject(StoredQueryCallForwardStatus);
        StoredQueryCallForwardStatus = NULL;
    }
}

/**
 * Synchronous call from the RIL to us to return current radio state.
 * RADIO_STATE_UNAVAILABLE should be the initial state.
 */
RIL_RadioState
currentState()
{
#if RIL_VERSION < 7
    return sState;
#else
    switch(sState)
    {
        case RADIO_STATE_OFF:
        case RADIO_STATE_UNAVAILABLE:
            return sState;
        default:
            return RADIO_STATE_ON;
    }
#endif
}
/**
 * Call from RIL to us to find out whether a specific request code
 * is supported by this implementation.
 *
 * Return 1 for "supported" and 0 for "unsupported"
 */

static int
onSupports (int requestCode)
{
    //@@@ todo

    return 1;
}

static void onCancel (RIL_Token t)
{
    //@@@todo

}

static const char * getVersion(void)
{
    return "android icera-ril for Icera 1.1";
}

/**
 * SIM ready means any commands that access the SIM will work, including:
 *  AT+CPIN, AT+CSMS, AT+CNMI, AT+CRSM
 *  (all SMS-related commands)
 */


void setRadioState(RIL_RadioState newState)
{
    RIL_RadioState oldState;

    pthread_mutex_lock(&s_state_mutex);

    oldState = sState;

    if (s_closed > 0) {
        // If we're closed, the only reasonable state is
        // RADIO_STATE_UNAVAILABLE
        // This is here because things on the main thread
        // may attempt to change the radio state after the closed
        // event happened in another thread
        newState = RADIO_STATE_UNAVAILABLE;
    }

    if (sState != newState || s_closed > 0) {
        sState = newState;

        pthread_cond_broadcast (&s_state_cond);
    }

    pthread_mutex_unlock(&s_state_mutex);

    /* do these outside of the mutex */
    if (sState != oldState) {
        /* In all cases the SIM state may have changed. */
        RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                        NULL, 0);
        switch(sState) {
            case RADIO_STATE_OFF:
            case RADIO_STATE_UNAVAILABLE:
                RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED,
                                    NULL, 0);
                break;
            case RADIO_STATE_ON:
            case RADIO_STATE_SIM_NOT_READY:
            case RADIO_STATE_SIM_LOCKED_OR_ABSENT:
            case RADIO_STATE_SIM_READY:
                if (oldState == RADIO_STATE_OFF ||
                    oldState == RADIO_STATE_UNAVAILABLE) {
                    RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED,
                                        NULL, 0);
                }
                break;
            default:
                // Do nothing
            break;
        }

        /* FIXME onSimReady() and onRadioPowerOn() cannot be called
         * from the AT reader thread
         * Currently, this doesn't happen, but if that changes then these
         * will need to be dispatched on the request thread
         */
        if (sState == RADIO_STATE_SIM_READY) {
            onSIMReady();
        } else if (sState == RADIO_STATE_SIM_NOT_READY) {
            onRadioPowerOn();
        }
    }
}



/** returns 1 if on, 0 if off, and -1 on error */
static int isRadioOn()
{
    ATResponse *p_response = NULL;
    int err;
    char *line;
    char ret;

    err = at_send_command_singleline("AT+CFUN?", "+CFUN:", &p_response);

    if (err < 0 || p_response->success == 0) {
        // assume radio is off
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextbool(&line, &ret);
    if (err < 0) goto error;

    at_response_free(p_response);

    return (int)ret;

error:

    at_response_free(p_response);
    return -1;
}



/**
 * RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM
 *
 * an unsolicited signal is triggered when a SMS in stored on SIM
 *
 * "data" is a const int
 * indicating the SIM storage index
 */
void unsolNewSmsOnSim(const char *s)
{
    char *line = NULL;
    char *p = NULL;
    int err;
    int index;
    char *skip;

    line = strdup(s);
    p = line;
    err = at_tok_start(&line);
    if (err < 0)
        goto error;

    err = at_tok_nextstr(&line, &skip);
    if (err < 0)
        goto error;

    err = at_tok_nextint(&line, &index);
    if (err < 0)
        goto error;

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM,
                  &index, sizeof(index));
    free(p);
    skip = NULL;
    return;

  error:
    ALOGE("Failed to parse +CMTI.");
    free(p);
    skip = NULL;
    return;
}

/**
 * Get baseband event(s) information thanks to AT%DEBUG=5
 *
 * @return char*
 */
static char *getModemCrashEvent(int *event_len)
{
    ATResponse *p_response = NULL;
    ATLine *p_cur;
    int err, len = 0, offset = 0;
    char *event_msg = NULL, *tmp;

    *event_len = 0;
    err = at_send_command_multiline_no_prefix("AT%DEBUG=5", &p_response);
    if ( (err>=0) && (p_response->success) ) {
        /* Get len of response as sum of all line(s) */
        for (p_cur = p_response->p_intermediates;
              p_cur != NULL;
              p_cur = p_cur->p_next) {
            /* len of a line is strlen + '/0' + '/n' that
               will be "re-added" in event_msg buffer */
            *event_len += strlen(p_cur->line) + 2;
        }
        event_msg = malloc(*event_len);

        /* Copy each line in buffer appended with '\n' */
        for (p_cur = p_response->p_intermediates;
              p_cur != NULL;
              p_cur = p_cur->p_next) {
            snprintf(event_msg + offset, strlen(p_cur->line) + 2, "%s\n", p_cur->line);
            offset += strlen(p_cur->line) + 2;
        }
    }
    at_response_free(p_response);

    return event_msg;
}

/*
 * Send AP timestamps to modem so it gets the offset with its own clock (no drift).
 */
static int synchronizeClocks(void)
{
    int fd = open("/sys/devices/platform/tegra_rtc_sysfs/value", O_RDONLY);
    if ( fd < 0) {
      ALOGE("%s: %s reading tegra RTC value", __func__, strerror(errno));
      return -1;
    }

    int rc = 0;
    int i;
    for (i = 0; i < 5; i++) {
        /* Send initial AT command */
        ATResponse *p_response = NULL;
        bool success = (at_send_command("AT%ITIMESYNCREQ", &p_response) == 0) &&
                       (p_response->success == 1);
        at_response_free(p_response);
        if (! success) {
            rc = -1;
            break;
        }

        /* Get response timestamp */
        char time_buf[32];
        /* We shall assume that reading sysfs takes no time for now */
        ssize_t length = read(fd, time_buf, sizeof(time_buf));
        if (length <= 0) {
            rc = -1;
            break;
        }

        /* The time read is in ms followed by a line feed, remove the latter */
        time_buf[length - 1] = '\0';
        /* Send follow up AT command (times in µs) */
        char response_buf[128];
        snprintf(response_buf, sizeof(response_buf),
                 "AT%%ITIMESYNCREQ=%s000,%s000", time_buf, time_buf);
        at_send_command(response_buf, NULL);
        lseek(fd, 0, SEEK_SET);
        ALOGD("%s: AT command='%s'", __func__, response_buf);
    }

    close(fd);
    return rc;
}

/**
 * EVENT: On init
 * Initialize everything that can be configured while we're still in
 * AT+CFUN=0
 */
static void initializeCallback(void /* *param */)
{
    ATResponse *p_response = NULL;

    char *line;
    int bootcount;

    int err;
    char *event_msg = NULL;
    int event_len;

    InitNetworkCacheState();

    setRadioState (RADIO_STATE_OFF);

    ALOGD("Starting handshake on primary channel.\n");

    err = at_handshake();

    if(err)
    {
        ALOGE("AT Channel not responding - power cycle the modem then invoke recovery handle.\n");
        IceraModemPowerControl(MODEM_POWER_CYCLE, 0);
        sleep(2); /* Let time to the modem for reboot. */
        error_recovery_handler(NULL);
        return;
    }

    ALOGD("Handshake successful on primary channel.\n");

    /* note: we don't check errors here. Everything important will
       be handled in onATTimeout and onATReaderClosed */

    /*  Check modem mode and handle cases when coming from crash */
    err = at_send_command_singleline("AT%MODE?","%MODE:", &p_response);
    if ((err == 0) && (p_response->success == 1)) {
        line = p_response->p_intermediates->line;
        at_tok_start(&line);
        at_tok_nextint(&line, &s_baseband_mode);
        if (s_baseband_mode == 1) {
            int coming_from_crash = 1;
            ALOGD("Device in loader mode.");

            /* Can be in loader mode but not coming from crash:
             * 1: FFS back-up trigger.
             * 2: Previous f/w update failed.
             * In those cases, we don't want to start full coredump reception.
             */
            at_response_free(p_response);
            p_response = NULL;
            err = at_send_command_multiline_no_prefix("AT%DEBUG=3,0", &p_response);
            if((err>=0) && (p_response->success) )
            {
                ATLine *p_cur;
                int n = 0;
                coming_from_crash = 0;
                for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next)
                {
                  n++;
                  if (n > 10)
                  {
                      /* No more than 10 lines in case we're not coming from crash:
                       * a quick way to avoid parsing too long answers... */
                      coming_from_crash = 1;
                      break;
                  }
                }
                if (!coming_from_crash)
                {
                    /* Need to check if it is real loader or loader in modem running:
                     * Criteria to detect loader type is the answer to AT+GMR that differs between
                     * the 2 modes.*/
                    int real_loader = 0;
                    at_response_free(p_response);
                    p_response = NULL;
                    err = at_send_command_multiline_no_prefix("AT+GMR", &p_response);
                    if((err>=0) && (p_response->success) )
                    {
                        for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next)
                        {
                            if (strcasestr(p_cur->line, "Secondary boot digest"))
                            {
                                real_loader = 1;
                                break;
                            }
                        }
                    }
                    if (!real_loader)
                    {
                        /* Loader in modem and not coming from crash: previous f/w update failed,
                         * without switching back in modem mode, let's do it and re-start RIL...*/
                        ALOGD("Loader in Modem");
                        at_send_command("AT%MODE=0", NULL);
                        error_recovery_handler(NULL);
                    }
                    else
                    {
                        /* Real loader but not coming from crash: FFS back-up triggered,
                         * do nothing here and below f/w update will do remaining recovery job.
                         * Simply remove boot indication that may prevent for f/w update trigger */
                        ALOGD("Real Loader");
                        ForceModemFirmwareUpdate();
                    }
                }
            }
            if (coming_from_crash)
            {
                event_msg = getModemCrashEvent(&event_len);
                SetModemLoggingEvent(COREDUMP_LOADER, event_msg, event_len);
                free(event_msg);

                /* RIL will be stopped to allow coredump reception: wait... */
                sleep(5);
            }
        }
        else
        {
            /* Not in loader mode: check if coming from crash */
            err = at_send_command_multiline_no_prefix("AT%DEBUG=3,0", &p_response);
            if ( (err>=0) && (p_response->success) )
            {
                ATLine *nextATLine = p_response->p_intermediates;
                line = nextATLine->p_next->line;
                if (!strStartsWith(line,"No New Crash")) {
                    /* Coming from crash: */
                    char prop[PROPERTY_VALUE_MAX];
                    event_msg = getModemCrashEvent(&event_len);
                    if (!property_get("gsm.modem.fild.args", prop, ""))
                    {
                        /* Dual flash platform: full coredump de-activated on this platform.
                           Will store some AP logs in a dated folder... */
                        ALOGD("Modem crash without full coredump.");
                        SetModemLoggingEvent(MODEM_CRASH_DETECTED_DUAL, event_msg, event_len);
                    }
                    else
                    {
                        /* Single flash platform */
                        SetModemLoggingEvent(MODEM_CRASH_DETECTED_SINGLE, event_msg, event_len);
                    }
                    free(event_msg);
                }
            }
            at_response_free(p_response);
            p_response=NULL;
        }
    }
    at_response_free(p_response);
    p_response = NULL;

    /* Update modem firmware if required... */
    HandleModemFirmwareUpdate();

    /*  atchannel is tolerant of echo but it must */
    /*  have verbose result codes */
    at_send_command("ATE0Q0V1", NULL);

    /* See if first time boot or not */
    line = malloc(PROPERTY_VALUE_MAX+1);
    property_get("ril.bootcount",line,"0");

    bootcount = atoi(line);
    ALOGD("Bootcount: %d",bootcount);

    if(bootcount==0)
    {
        /*
         * modem first boot
         */
        err = at_send_command("AT%ICOLDBOOT", NULL);
    }

    /* Stk needs to know if this is first boot of the RIL to do proper initializations. */
    StkIsFirstBoot(bootcount==0);

    /*
     * Increment the bootcount so we can distinguish modem crash from
     * genuine restart
     */
    bootcount++;
    snprintf(line,PROPERTY_VALUE_MAX,"%d",bootcount);
    property_set("ril.bootcount",line);
    free(line);

    if (IsLoggingInitialised())
    {
        /* Activate warning on the modem side.*/
        err = at_send_command("AT%IWARN=1",&p_response);
        if((err != 0) || (p_response->success == 0))
        {
            ALOGW("Fail to activate modem logging on modem warning.");
        }
        at_response_free(p_response);
        p_response = NULL;

        if (IsDeferredLogging())
        {
            /* Handshake successful and 1st AT cmds answer:
             * logging channel may be ready... */
            StartDeferredLogging();
        }
    }


    /*
     * This is needed because sticks and module may not be configured to
     * start in the right mode.
     */
    err = at_send_command_singleline("AT+CFUN?", "+CFUN:", &p_response);
    if ((err == 0) && (p_response->success == 1))
    {
        int cfunMode=-1;
        line = p_response->p_intermediates->line;
        at_tok_start(&line);
        at_tok_nextint(&line, &cfunMode);
        if(cfunMode!=0)
        {
            changeCfun(0);
        }
    }
    at_response_free(p_response);
    p_response = NULL; /* to avoid problems because p_response is reused later in the function.*/

    /* Log the configuration status */
    at_send_command("AT%ICFG=7", NULL);
    at_send_command("AT%ILIBLIST", NULL);
    at_send_command("AT%ICFG",NULL);
    at_send_command("AT%IPROD",NULL);
    at_send_command("AT%IPLATCFG",NULL);

    /* Log the modem fw details */
    at_send_command("AT%IGMR", NULL);

    /*  No auto-answer */
    at_send_command("ATS0=0", NULL);

    /*  Extended errors */
    at_send_command("AT+CMEE=1", NULL);

    /*  Make ussd decoding into modem. String format depends on CSCS. */
    at_send_command("AT%IUSSDMODE=0", NULL);

    /*  Unsolicited broadcast only on the main channel */
    at_send_command("AT%IDBC=0", NULL);

    /* Synchronize AP and BB clocks */
    synchronizeClocks();

    /* Init LTE context */
    initPdpContext();

#ifdef LTE_TEST
    /* Increase modem mips */
    at_send_command("AT%ILTEMEAS=0;%IAPM=0;%IAPM=5,1435",NULL);
#endif

     /*  Check modem mode. */
    err = at_send_command_singleline("AT%MODE?","%MODE:", &p_response);

    if ((err == 0) && (p_response->success == 1)) {
        line = p_response->p_intermediates->line;
        at_tok_start(&line);
        at_tok_nextint(&line, &s_baseband_mode);
    }

    at_response_free(p_response);
    p_response = NULL;

    /* Also get technology changes notification,
     * This can change while remaining on the same cell
     * (and cell change can be deactivated via
     * requestSetLocationUpdates) -> creg is not enough!
     */
    if(UseIcti)
    {
        at_send_command("AT%ICTI=1",&p_response);
        if((err != 0) || (p_response->success == 0))
        {
            UseIcti = 0;
        }
        at_response_free(p_response);
        p_response = NULL;
    }

    if(UseIcti == 0)
    {
        at_send_command("AT%NWSTATE=3", NULL);
    }

    /*  Alternating voice/data off */
    at_send_command("AT+CMOD=0", NULL);

    /*  Not muted */
    at_send_command("AT+CMUT=0", NULL);

    /*  UCS2 character set */
    at_send_command("AT+CSCS=\"UCS2\"", NULL);

    /*  SMS PDU mode */
    at_send_command("AT+CMGF=0", NULL);

    /*  Enable unsolicited for sim status changed */
    at_send_command("AT%ISIMINIT=1", NULL);

    /* Enable the unsolicited for sim recovery */
    at_send_command("AT%ISIMRECOVER=1", NULL);

    /* Enables unsolicited message for sms storage full notification */
    at_send_command("AT*TUNSOL=\"SM\",1", NULL);

    /* S7 timeout: number of seconds to wait for connection
       completion. Default value is 60s. Add 10 seconds. */
    at_send_command("ATS7=70", NULL);

    /*
     * Register for the CNMA timeout notifications.
     */
    at_send_command("AT%INMA=1", NULL);
    /*
     * 2 different AT interfaces are possible tfor STK.
     * So try the "new"one to check if it can be used otherwise use "old" one by default
     */
    err =  at_send_command("AT+CUSATW=?", &p_response);

    if((err>=0) && (p_response->success) )
    {
        stk_use_cusat_api = 1;
        /* New interface needs the profiles to be set explicitely*/
        StkInitDefaultProfiles();
    }
    else
    {
        stk_use_cusat_api = 0;
        /* Icera specific -- STK needs to be all mannual */
        at_send_command("AT*TSTAUTO=0,0,0,0", NULL);
    }

    /*
     * Register for the IFUNU notifications.
     */
    at_send_command("AT%IFUNU=1", NULL);


    at_response_free(p_response);

    /* EDP initialization */
    rilEDPInit();

    /* Set to the ecclist default value */
    checkAndAmendEmergencyNumbers(2, 1);    // SIM is not checked, read the default ecc list from modem.

    rilExtInitializeCallback();

    /* assume radio is off on error */
    if (isRadioOn() > 0) {
        setRadioState (RADIO_STATE_SIM_NOT_READY);
    }
    /* init is done Wakelock are now managed in the framework */
    IceraWakeLockRelease();
}

static void initializeCallbackEx(void /* *param */)
{

    int err;

    ALOGD("Starting handshake on extended channel.\n");

    err = at_handshake();

    if(err)
    {
        ALOGE("Ext-AT Channel not responding - power cycle the modem then invoke recovery handle.\n");
        IceraModemPowerControl(MODEM_POWER_CYCLE, 0);
        sleep(2); /* Let time to the modem for reboot. */
        error_recovery_handler(NULL);
        return;
    }

    ALOGD("Handshake successful on extended channel.\n");



    /*  atchannel is tolerant of echo but it must */
    /*  have verbose result codes */
    at_send_command("ATE0Q0V1", NULL);

    /*  Extended errors on this channel too */
    at_send_command("AT+CMEE=1", NULL);

    /** TODO: more initialization for additional AT channel here if needed */
    rilExtInitializeCallbackEx();
}

static void waitForClose(void)
{
    pthread_mutex_lock(&s_state_mutex);

    while (s_closed == 0) {
        pthread_cond_wait(&s_state_cond, &s_state_mutex);
    }

    pthread_mutex_unlock(&s_state_mutex);
}

/**
 * RIL just received modem warning.
 * Need to:
 * - start modem logging if not started
 * - dispatch warn event info to higher level
 */
static void unsolWarnModemEvent(const char *s)
{
    SetModemLoggingEvent(MODEM_LOGGING, s, strlen(s));
}

/**
 * Called by atchannel when an unsolicited line appears
 * This is called on atchannel's reader thread. AT commands may
 * not be issued here
 */
static void onUnsolicited (const char *s, const char *sms_pdu)
{
    struct _ril_unsol_object_t* unsol_obj;

    LogKernelTimeStamp();
    ALOGD("%x onUnsolicited: %s %s", (unsigned int)pthread_self(), (s)?s:"null", (sms_pdu)?sms_pdu:"null");

    unsol_obj = ril_unsol_object_create(s, sms_pdu);
    if ( NULL != unsol_obj )
    {
        /* always use default channel for unsolicited response handling */
        (void)rilq_put_object(s_h_queue[0], unsol_obj, RILQ_OBJECT_TYPE_UNSOL );
    }
}

static void onHandleUnsolicited (const char *s, const char *sms_pdu)
{
    char *line = NULL;
    int err;

    ALOGD("%x onHandleUnsolicited: %s %s", (unsigned int)pthread_self(), (s)?s:"null", (sms_pdu)?sms_pdu:"null");

    /* Ignore unsolicited responses until we're initialized.
     * This is OK because the RIL library will poll for initial state
     */
    if (sState == RADIO_STATE_UNAVAILABLE) {
        return;
    }

    /*
     * Proprietary RIL requests
     */

    if (rilExtOnUnsolicited(s, sms_pdu)) {
        /* Unsolicited was dealt with, no need to go on */
        ALOGI("Unsol processing of %s prevented per proprietary request",s);
        return;
    }

    if(strStartsWith(s, "*TLTS:")) {
        unsolNetworkTimeReceived(s);
    } else if (strStartsWith(s, "+CRING:")
                || strStartsWith(s, "RING")) {
            /* Reset the error cause as this is a new call */
            ResetLastCallErrorCause();
            RIL_onUnsolicitedResponse(RIL_UNSOL_CALL_RING,
                NULL, 0);
    } else if (strStartsWith(s,"NO CARRIER")
                || strStartsWith(s,"+CCWA")
    ) {
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
            NULL, 0);
    } else if(strStartsWith(s,"+CREG:")){
        parseCreg(s);
    } else if(strStartsWith(s,"+CGREG:")){
        parseCgreg(s);
    } else if(strStartsWith(s,"+CEREG:")){
        parseCereg(s);
    } else if (strStartsWith(s, "%NWSTATE")) {
        parseNwstate(s);
    } else if (strStartsWith(s, "%ICTI")) {
        parseIcti(s);
    } else if (strStartsWith(s, "%IPSRB: 0")) {
        /* Check if we need to notify the framework */
        notifyConnectionState(0);
    } else if (strStartsWith(s, "%IPSRB: 1")) {
        /* Check if we need to notify the framework */
        notifyConnectionState(1);
    } else if (strStartsWith(s, "+CMT:")) {
        /* 
         * Make sure ACK is received within acceptable time 
         * And only pass on to framework if there is chance to ack
         * garanteed duplicate otherwise
         */
        if(sState != RADIO_STATE_OFF) {
            SmsAckStartTimeOut();
            RIL_onUnsolicitedResponse (
                RIL_UNSOL_RESPONSE_NEW_SMS,
                sms_pdu, strlen(sms_pdu));
        }
    } else if (strStartsWith(s, "+CBM:")) {
        int len = strlen(sms_pdu)/2;
        char * encoded_pdu = alloca(len +1);
        hexToAscii(sms_pdu, encoded_pdu, len);
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS,
            encoded_pdu, len);
    }else if (strStartsWith(s, "+CMTI:")) {
        unsolNewSmsOnSim(s);
    }else if (strStartsWith(s, "%ISIMINIT:")) {
        unsolSimStatusChanged(s);
    }else if(strStartsWith(s, "+CGEV: NW DETACH")){
        /*
         * Let's queue an internal request for that purpose, so it can be sent to
         * a more suitable queue.
         */
        onRequest(IRIL_REQUEST_ID_FORCE_REATTACH, NULL, 0, NULL);
    } else if (strStartsWith(s, "+CDS:")) {
        /* Make sure ACK is received within acceptable time */
        SmsAckStartTimeOut();
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT,
            sms_pdu, strlen(sms_pdu));
    } else if (strStartsWith(s, "*TSMSINFO: 322")) {
        RIL_onUnsolicitedResponse(
            RIL_UNSOL_SIM_SMS_STORAGE_FULL,
            NULL, 0);
    } else if ((strStartsWith(s, "*TSQN"))||
               (strStartsWith(s, "+CSQ"))) {
        unsolSignalStrength(s);
    } else if ((strStartsWith(s, "%ICESQU"))||
               (strStartsWith(s, "+CESQ"))) {
        unsolSignalStrengthExtended(s);
    } else if (strStartsWith(s, "+CUSD:")) {
        onUSSDReceived(s);
    } else if (strStartsWith(s, "*TSIMINS:")
               || strStartsWith(s, "%IUSATREFRESH")) {
        /*%IUSATREFRESH2 also catched here*/
        unsolSIMRefresh(s);
    } else if (strStartsWith(s, "+CSSI:")) {
        onSuppServiceNotification(s, 0);
    } else if (strStartsWith(s, "+CSSU:")) {
        onSuppServiceNotification(s, 1);
    } else if((stk_use_cusat_api && strStartsWith(s, "%IUSATP:"))
              ||(!stk_use_cusat_api && strStartsWith(s, "*TSTUD:"))) {
        unsolStkEventNotify(s);
    } else if((stk_use_cusat_api && strStartsWith(s, "+CUSATP:"))
              ||(stk_use_cusat_api && strStartsWith(s, "%IUSATC:"))
              ||(!stk_use_cusat_api && strStartsWith(s, "*TSTC:"))) {
        unsolicitedSTKPRO(s);
    } else if(strStartsWith(s, "%ILCC:")) {
        unsolicitedCallStateChange(s);
    }
#ifdef REPORT_CNAP_NAME
    else if(strStartsWith(s, "+CNAP:")) {
        unsolicitedCNAP(s);
    }
#endif
    else if (strStartsWith(s, "%IPDPACT:"))
    {
        ParseIpdpact(s);
    }
    else if (strStartsWith(s, "%INMA: 0"))
    {
        /*
         * EVENT: On SMS ack missed
         * only interrested in CNMA ack missed, ignored the rest
         * pending hack should be managed implicitely anyway.
         */
        at_send_command("AT+CNMI=1,2,2,1,0", NULL);
    }
    else if (strStartsWith(s, "%IWARN:"))
    {
        unsolWarnModemEvent(s);
    }
    else if (strStartsWith(s, "%INWRESTRICT:"))
    {
        unsolRestrictedStateChanged(s);
    }
    else if(stk_use_cusat_api && strStartsWith(s, "+CUSATEND")) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END, NULL, 0);
    }
    else if (strStartsWith(s, "%ISIMRECOVER:"))
    {
        /*
         * Arbitrarilly terminate the STK sessions, keep this distinct from the one above
         * so more can be added if needed at a later stage.
         */
        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END, NULL, 0);
    }
    else if (strStartsWith(s, "%IECELLID:")) {
        unsolCellInfo(s);
    }
    else if (strStartsWith(s, "%IEDPU:")) {
        parseIedpu(s);
    }
    else if (strStartsWith(s, "%IFUNU:")) {
        parseIfunu(s);
    }
    else
    {
        /* Not an unsolicited that the ril or OEM ril expects, offer it to higher
         * level (CTS in particular)
         */
        ALOGD("Unsol not processed: %s", s);
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_OEM_HOOK_RAW,
            s, strlen(s));
    }
}

/* Called on command or reader thread */
static void onATReaderClosed(void)
{
    ALOGI("AT channel closed\n");
    error_recovery_handler(NULL);
}

/* Called on command thread */
static void onATTimeout(void)
{
    ALOGI("AT channel timeout; closing\n");
    error_recovery_handler(NULL);
}

static void usage(char *s)
{
#ifdef RIL_SHLIB
    fprintf(stderr, "icera-ril requires: -p <tcp port> or -d /dev/tty_device\n");
#else
    fprintf(stderr, "usage: %s [-p <tcp port>] [-d /dev/tty_device]\n", s);
    exit(-1);
#endif
}

static int deviceOpenCallback( char* device_path, at_channel_device_type_t device_type, int default_channel )
{
    int fd = -1;

    int retry_count = 0;
    int power_cycles = 0;
    int max_retries = 10, ok;
    char modem_property[PROPERTY_VALUE_MAX];

    /* Get the number of retries from the properties */
    ok = property_get("ril.maxretries", modem_property ,"10");
    if(ok)
    {
        max_retries = atoi(modem_property);
    }

    ALOGD("deviceOpenCallback(%s, %d, %d)\n Max_retries: %d", device_path, device_type, default_channel, max_retries);
    while  (fd < 0)
    {
        switch(device_type)
        {
        case ATC_DEV_TYPE_LOOPBACK:
            ALOGD("deviceOpenCallback() trying to open LOOPBACK device %s\n", device_path);
            fd = socket_loopback_client(atoi(device_path), SOCK_STREAM);
            break;

        case ATC_DEV_TYPE_SOCKET:
            ALOGD("deviceOpenCallback() trying to open SOCKET device %s\n", device_path);
            if (!strcmp(device_path, "/dev/socket/qemud"))
            {
                /* Qemu-specific control socket */
                fd = socket_local_client( "qemud",
                                          ANDROID_SOCKET_NAMESPACE_RESERVED,
                                          SOCK_STREAM );
                if (fd >= 0 ) {
                    static int qemu_init_once = 0;
                    if ( 0 == qemu_init_once )
                    {
                       char  answer[2];
                       ALOGD("deviceOpenCallback() writing gsm to device %s\n", device_path);
                       if ( write(fd, "gsm", 3) != 3 ||
                           read(fd, answer, 2) != 2 ||
                           memcmp(answer, "OK", 2) != 0)
                      {
                          close(fd);
                          fd = -1;
                      }
                      ALOGD("deviceOpenCallback() qemu device %s\n", device_path);
                qemu_init_once = 1;
                    }
               }
            }
            else
            {
                fd = socket_local_client(device_path,
                                        ANDROID_SOCKET_NAMESPACE_FILESYSTEM,
                                        SOCK_STREAM );
            }
            break;

        case ATC_DEV_TYPE_TTY:
            fd = open (device_path, O_RDWR);
            if ( fd >= 0 && !memcmp( device_path, "/dev/tty", 8 ) ) {
                /* disable echo on serial ports */
                struct termios  ios;
                tcgetattr( fd, &ios );
                ios.c_lflag = 0;  /* disable ECHO, ICANON, etc... */
                ios.c_cc[VTIME] = 0;
                ios.c_cc[VMIN] = 1;
                tcsetattr( fd, TCSANOW, &ios );
            }
            ALOGD("deviceOpenCallback() open returned fd %d for device %s\n", fd, device_path );
            break;

        case ATC_DEV_TYPE_UNKNOWN:
            default:
            ALOGD("deviceOpenCallback() unknown device type: %d for device %s\n", device_type, device_path);
            break;
        }

        /* Invoke modem power cycle in case of unsuccesful retries*/
        if( (fd < 0) && (retry_count > max_retries) )
        {
            if((default_channel || (s_baseband_mode==0))&&(power_cycles < 30))
            {
                /*
                 * if Power cycling didn't happen (prevented by FILD for example
                 * Reset the amount of attempts to try afresh
                 */
                power_control_status_t status = IceraModemPowerControl(MODEM_POWER_CYCLE, 0);

                switch(status)
                {
                    case OVERRIDDEN:
                    {
                        ALOGD("Power cycle disabled, restarting sequence");
                        power_cycles = 0;
                        break;
                    }
                    case UNCONFIGURED:
                    {
                        /*
                         * Modem power cycling is unconfigured, likely a platform
                         * where it's not implemented by the HW. No point trying further
                         */
                        ALOGE("No modem found, aborting.");
                        return fd;
                    }
                    default:
                    {
                        power_cycles++;
                    }
                }
                retry_count = 0;
            }
            else
            {
                /* if loader mode and not main channel cancel this channel.*/
                break;
            }
        }
        else
        {
            /* Check the state of FW download and reset counter accordingly */
            power_control_status_t status = IceraModemPowerStatus();
            switch(status)
            {
                case OVERRIDDEN:
                {
                    ALOGI("Power cycle disabled, restarting sequence");
                    power_cycles = 0;
                    retry_count = 0;
                    break;
                }
                default:
                {
                }
            }
        }

        if (fd < 0) {
            sleep(1);
            retry_count++;
            /* never returns */
        }
    }

    /*
     * If we're not the default channel:
     * Wait for the default channel to be up
     * so we can apply the final timeout configuration.
     */
    while((!default_channel) && (!at_channel_is_ready(s_h_channel[0])))
    {
        ALOGD("Waiting for main channel to be up");
        sleep(1);
    }

    if ( !default_channel && !isRunningOnEmulator())
    {
        /* if another channel except the default channel is opened, the
            timeout for the default channel can be decreased.
            However on the emulator the timeout of the default channel is never
            re-configured due to quemu issues on timeouts and re-opening */
        long long timeout = 0;
        timeout = at_channel_config_get_at_cmd_timeout(0);
        at_channel_writer_set_at_cmd_timeout( s_h_channel[0], timeout );
    }

    s_closed = 0;
    return fd;
}

static void deviceCloseCallback( void* param )
{
    ALOGD("deviceCloseCallback() %s\n", (char*) param);
}

static void atChannelInit( const char* dev_name, int dev_type )
{
    long long timeout;
    at_channel_callback_funcs_t cb_funcs;

    cb_funcs.callback_dev_open = deviceOpenCallback;
    cb_funcs.callback_dev_close = deviceCloseCallback;
    cb_funcs.callback_unsolicited = onUnsolicited;
    cb_funcs.callback_handle_unsolicited = onHandleUnsolicited;
    cb_funcs.callback_timeout = onATTimeout;
    cb_funcs.callback_closed = onATReaderClosed;
    cb_funcs.callback_handle_request = onHandleRequest;
    cb_funcs.callback_init = initializeCallback;

    timeout = at_channel_config_get_default_at_cmd_timeout(s_used_channels);
    if ( isRunningOnEmulator() )
    {
        timeout = 0; /* overwrite to non-blocking due to qemu issues on re-open */
        ALOGD("atChannelInit() running on emulator, overwriting AT cmd timeout for default channel\n");
    }
    s_h_queue[s_used_channels] = rilq_create();
    s_h_channel[s_used_channels] = at_channel_writer_create( dev_name,
                                                             dev_type,
                                                             &cb_funcs,
                                                             s_h_queue[s_used_channels],
                                                             1,
                                                             timeout );
    s_used_channels++;
}

void postDelayedRequest(void * req, int delayInSeconds)
{
    struct timeval delay = {delayInSeconds,0};

    RIL_requestTimedCallback(requestPutObject, req, &delay);
}

static void atChannelInitExtended( void )
{
    int i;
    at_channel_callback_funcs_t cb_funcs;

    cb_funcs.callback_dev_open = deviceOpenCallback;
    cb_funcs.callback_dev_close = deviceCloseCallback;
    cb_funcs.callback_unsolicited = onUnsolicited;
    cb_funcs.callback_handle_unsolicited = onHandleUnsolicited;
    cb_funcs.callback_timeout = onATTimeout;
    cb_funcs.callback_closed = onATReaderClosed;
    cb_funcs.callback_handle_request = onHandleRequest;
    cb_funcs.callback_init = initializeCallbackEx;

    for ( i=s_used_channels; i <= at_channel_config_get_num_channels() && i < MAX_AT_CHANNELS; ++i )
    {
        long long timeout;

        const char* dev_name = at_channel_config_get_device_name(i);
        timeout = at_channel_config_get_at_cmd_timeout(i);
        s_h_queue[i] = rilq_create();
        s_h_channel[i] = at_channel_writer_create( dev_name,
                                                   ATC_DEV_TYPE_TTY,
                                                   &cb_funcs,
                                                   s_h_queue[i],
                                                   0,
                                                   timeout );
        s_used_channels++;
    }
}


static void atChannelInitExtendedSockets( char* dev_name)
{
    int i;
    at_channel_callback_funcs_t cb_funcs;

    cb_funcs.callback_dev_open = deviceOpenCallback;
    cb_funcs.callback_dev_close = deviceCloseCallback;
    cb_funcs.callback_unsolicited = onUnsolicited;
    cb_funcs.callback_handle_unsolicited = onHandleUnsolicited;
    cb_funcs.callback_timeout = onATTimeout;
    cb_funcs.callback_closed = onATReaderClosed;
    cb_funcs.callback_handle_request = onHandleRequest;
    cb_funcs.callback_init = initializeCallbackEx;

    for ( i=s_used_channels; i <= at_channel_config_get_num_channels() && i < MAX_AT_CHANNELS; ++i )
    {
        long long timeout;
        /* Use same timeout as default channel for testing on the simulated modem. */
        timeout = at_channel_config_get_at_cmd_timeout(i);
        s_h_queue[i] = rilq_create();
        s_h_channel[i] = at_channel_writer_create( dev_name,
                                                   ATC_DEV_TYPE_SOCKET,
                                                   &cb_funcs,
                                                   s_h_queue[i],
                                                   0,
                                                   timeout );
        s_used_channels++;
    }
}
static void atChannelExitAll( void )
{
    int i;

    s_nInErrRecovery = 1; /* if called on shutdown avoid error recovery to be triggered */

    for ( i=0; i <= at_channel_config_get_num_channels() && i < MAX_AT_CHANNELS; ++i )
    {
        if ( NULL != s_h_channel[i] )
        {
            at_channel_writer_destroy( s_h_channel[i] );
            s_h_channel[i] = NULL;
        }
        if ( NULL != s_h_queue[i] )
        {
            rilq_destroy( s_h_queue[i] );
            s_h_queue[i] = NULL;
        }
        s_used_channels--;
    }
}

static void atChannelInitAll( void )
{
    if ( isRunningOnEmulator() )
    {
        if ( isRunningOnLinuxEmu() )
        {
            ALOGD("atChannelInitAll() configuration: Android Emulator running on Linux host\n");
            atChannelInit("/dev/ttyS2", ATC_DEV_TYPE_TTY);
            atChannelInitExtended();
        }
        else
        {
            ALOGD("atChannelInitAll() configuration: Android Emulator running on Windows host\n");
            atChannelInit("/dev/socket/qemud", ATC_DEV_TYPE_SOCKET);
        }
    }
    else
    {
        if(s_device_socket)
        {
           ALOGD("atChannelInitAll() configuration: Android running on target hardware - modem simulated \n");
           atChannelInit(s_device_path, ATC_DEV_TYPE_SOCKET);
           atChannelInitExtendedSockets(s_device_path);
        }
        else
        {
           ALOGD("atChannelInitAll() configuration: Android running on target hardware\n");
           atChannelInit(s_device_path, ATC_DEV_TYPE_TTY);
           atChannelInitExtended();
        }
    }
}

int isRunningTestMode( void )
{
   char ril_testmode[PROPERTY_VALUE_MAX];

    property_get("ril.testmode", ril_testmode ,"0");

    return (ril_testmode[0] != '0');
}

void setTestMode(int newMode) {
    property_set("ril.testmode",(newMode!=0)?"1":"0");
}

static int isRunningOnEmulator( void )
{
    char buffer[1024];
    int    len;
    int ret = 1; /* running on emulator */
    int    fd = open("/proc/cmdline",O_RDONLY);

    if (fd < 0)
    {
        ALOGD("could not open /proc/cmdline:%s", strerror(errno));
        return 0;
    }

    memset(buffer, 0, sizeof(buffer));

    do {
        len = read(fd,buffer,sizeof(buffer));
    }
    while (len == -1 && errno == EINTR);

    if (len < 0)
    {
        ALOGD("could not read /proc/cmdline:%s", strerror(errno));
        close(fd);
        return 0;
    }
    close(fd);

    ALOGI("/proc/cmdline:%s", buffer);

    if (strstr(buffer, "android.qemud=") == NULL)
    {
        ret = 0; /* not running on emulator */
    }

    ALOGD( "isRunningOnEmulator() returns %d\n", ret);
    return ret;
}

/* Init all logging tag, so client can be identified */
static void InitLogging(int clientId)
{
    asprintf(&rilLogTag,    "RIL{%d}",        clientId);
    asprintf(&atLogTag,     "RIL AT{%d}",     clientId);
    asprintf(&atchLogTag,   "RIL_ATCH{%d}",   clientId);
    asprintf(&atccLogTag,   "RIL_ATCC{%d}",   clientId);
    asprintf(&atcwLogTag,   "RIL_ATCW{%d}",   clientId);
    asprintf(&req0LogTag,   "RIL_REQ0{%d}",   clientId);
    asprintf(&reqqLogTag,   "RIL_REQQ{%d}",   clientId);
    asprintf(&unsoLogTag,   "RIL_UNSO{%d}",   clientId);
    asprintf(&oemLogTag,    "RIL_OEM{%d}",    clientId);
}
static int isRunningOnLinuxEmu( void )
{
    int ret = 0;
    int fd = open ("/dev/ttyS3", O_RDWR);
    if ( fd >= 0 ) {
        ret = 1;
        close(fd);
    }
    ALOGD( "isRunningOnLinuxEmu() returns %d\n", ret);
    return ret;
}

static int error_recovery_handler( void* param )
{
    ALOGE("starting Error Recovery handler\n");
    /*
     * Take a wakelock to force continuous execution until the RIL is
     * restarted. We may otherwise enter LP0 too early and be without a
     * modem for a long long time (several hours, potentially!)
     *
     * Note that the count of acquired wakelock will be reset to zero
     * with the ril restart, so this specific acquisition doesn't need
     * to be explicitely released. It will be released implicitely at
     * the end of deviceInitializeCallback as we restart.
     */
    ALOGD("Acquiring wakelock to force smooth restart");
    IceraWakeLockAcquire();

    /*
     * Framework (RILJ) will signal Radio not available and
     * automatically fails any pending requests.
     */
    LogKernelTimeStamp();
    ALOGE("Restarting");
    exit(-1);
}

/*
 * Install a signal handler in the main thread so the exit handlers are called
 * correctly and we get correct code coverage information.
 */
static void sighandler(int signal)
{
    ALOGI("signal received %d, exiting", signal);
    exit(-1);
}

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
    int ret;
    int fd = -1;
    int opt;
    int Client = 0;
    int ok;
    pthread_attr_t attr;
    char modem_property[PROPERTY_VALUE_MAX];
    char socket_name[PROPERTY_VALUE_MAX];
    char* config_file = "/system/etc/ril_atc.config";
    char *device_path = NULL;

    // Signal handler
    struct sigaction action;
    action.sa_handler = sighandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, NULL);

    /* Need to protect the init phase */
    IceraWakeLockRegister(ICERA_RIL_WAKE_LOCK_NAME, true);
    IceraWakeLockAcquire();
    LogKernelTimeStamp();

    ok = property_get("ro.ril.modemcontrol", modem_property ,"");
    if(strcmp(modem_property, "no-access") == 0 )
    {
        /* Modem debug mode - In this case RIL should not execute. */
        while(1)
        {
            ALOGI("RIL disabled by ro.ril.modemcontrol property.\n");
            sleep(0x000000ff);
        }
    }

    s_rilenv = env;

    ALOGD("RIL_Init for Icera\nVersion: \"%s\"", RIL_VERSION_DETAILS);
    ALOGD("%s", RIL_MODIFIED_FILES);

    for (opt=0;opt<argc;opt++)
    {
        ALOGD("RIL_Init argument %d: %s", opt, argv[opt]);
    }


    while ( -1 != (opt = getopt(argc, argv, "e:d:l:c:ns:f:j"))) {
        switch (opt) {
            case 'e':
                AddNetworkInterface(optarg);
                ALOGI("Adding network interface to %s\n", optarg);
            break;
            case 'd':
                device_path = optarg;
                ALOGI("Setting device path to %s\n", s_device_path);
            break;
            case 'l':
                /* Check whether location updates are enabled by default */
                DefaultLocUpdateEnabled = (optarg[0] != '0');
            break;
            case 'n':
                UseIcti = 0;
                break;
            case 'c':
                /* Client id */
                Client = atoi(optarg);
            break;
            case 's':
            {
                // Logical mapping may differ between AP and Modem
                unsigned int simApId;
                unsigned int simModemId = atoi(optarg);

                char* simPath = strstr(optarg,",");
                if (simPath!= NULL) {
                    simApId = atoi(simPath+1);
                } else {
                    simApId = simModemId;
                }

                asprintf(&simPath,"/sys/class/misc/sim/sim%d_inserted",simApId);

                if((simPath!=NULL)&&(simApId<=9 && simModemId<=9))
                {
                    StartHotSwapMonitoring(simModemId,simPath);
                }
                else
                {
                    ALOGE("Sim configuration not supported: %s",optarg);
                }
                break;
            }
            case 'f':
            {
                /*
                 * First Arg is fast dormancy TO in screen off mode,
                 * second parameter is screen on TO, may be omitted
                 */
                FDTO_ScreenOff = atoi(optarg);

                char* param = strstr(optarg,",");
                if (param!= NULL) {
                    FDTO_ScreenOn = atoi(param+1);
                } else {
                    FDTO_ScreenOn = 0;
                }

                ALOGD("Fast dormancy config: Screen off: %d, Screen on: %d",FDTO_ScreenOff, FDTO_ScreenOn);
                break;

            }
            case 'j':
            {
                collateNetworkList = 0;
                break;
            }
            default:
                ALOGD("RIL_Init error: unknown command flag (%c)\n", opt);
            break;
                //usage(argv[0]);
                //return NULL;
        }
    }
    /* Create a dedicated tag to ease log parsing */
    InitLogging(Client);

    /* Log the ril version details only once the client is known */
    ALOGD("%s", RIL_MODIFIED_FILES);

    /* Initialize data call list  accordingly*/
    InitDataCallList(Client);

    /* Init modem logging */
    InitModemLogging();

    if(device_path)
    {
        /* Device name indicated as cmd line argument... */
        /* Either there's a system property either we do not overwrite command line arg */
        property_get("ro.ril.devicename", s_device_path, device_path);
    }
    else
    {
        /* No device name indicated as cmd line argument... */
        /* Either there's a system property, or we use default device name value */
        property_get("ro.ril.devicename", s_device_path, DEFAULT_RIL_DEVICE_NAME);
    }

    property_get("ril.mock_modem.socketname", socket_name, "");

    if (strcmp(socket_name, "") != 0) {
       strcpy(s_device_path, socket_name);
       s_device_socket = 1;
    }

    if(strcmp(modem_property, "no-power-control") != 0)
    {
        ALOGI("Configuring Modem Power Control\n");
        IceraModemPowerControlConfig();
    }
    else
    {
        ALOGI("Modem Power Control disabled by ro.ril.modemcontrol property.\n");
    }

    /* Log the timeout values */
    property_get("ro.ril.wake_lock_timeout", modem_property ,"default");

    ALOGD("Framework ril to: %s, Main channel: %d, Extended channel: %d",
        modem_property,
        (int)at_channel_config_get_default_at_cmd_timeout(0),
        (int)at_channel_config_get_default_at_cmd_timeout(1));

    if ( isRunningOnLinuxEmu() )
    {
        config_file = "/system/etc/ril_atc.emu.config";
        at_channel_config_init(config_file);
    }
    else if ( !isRunningOnEmulator() )
    {
        at_channel_config_init(config_file);
    }
    // else: Windows Emulator or unsupported platform

#ifdef ECC_VERIFICATION
    /* ECC verif procedure can be enabled/disabled via property */
    property_get("ril.ecc_verification", modem_property ,"OFF");

    ALOGD("Ecc verification procedure is: %s",modem_property);

    if(strcmp(modem_property,"ON") == 0)
    {
        isEccVerificationOn=1;
    }
#endif
    atChannelInitAll();

    return &s_callbacks;
}

int useIcti(void)
{
    return UseIcti;
}

int isMockModem(void) {
    return s_device_socket;
}

/*
 * Allow some debug feedback at UI level by simulating an incoming USSD
 * This will result in a pop up with the given message
 */
void debugUserNotif(char * notifString)
{
    int len = strlen(notifString);
    char * unsolString;
    char * encodedString;

    if (at_is_channel_UCS2()) {
        int ucs2_len = UTF8ToUCS2Hex_sizeEstimate(len);
        encodedString = (char *)calloc(ucs2_len, sizeof(char));
        ucs2_len = UTF8ToUCS2Hex(notifString, encodedString, len, ucs2_len);
        if (ucs2_len<0)
            goto error;
    } else {
        encodedString = (char *)malloc(len * 2 + 1);
        asciiToHex(notifString, encodedString, len);
    }

    asprintf(&unsolString,"+CUSD: 0,\"%s\"",encodedString);
    onUnsolicited(unsolString, NULL);
error:
    free(encodedString);
    free(unsolString);
}
