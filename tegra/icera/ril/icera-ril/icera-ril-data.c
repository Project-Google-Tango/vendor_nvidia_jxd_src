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
#include <telephony/ril.h>
#include <errno.h>
#include "atchannel.h"
#include "at_tok.h"
#include "misc.h"
#include <cutils/properties.h>

/* For manual IP address setup */
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/sockios.h>
//#include <linux/ipv6_route.h>

#include <linux/route.h>
#include <regex.h>

#include <sys/socket.h>

#include "icera-ril.h"
#include <icera-ril-net.h>
#include "icera-ril-data.h"
#include "icera-ril-services.h"

#include "at_channel_writer.h"

#define LOG_TAG rilLogTag
#include <utils/Log.h>


#define IPDPCFG_PAP 1
#define IPDPCFG_CHAP 2

#define MAX_CID_STRING_LENGTH 4

#define IPV4_NULL_ADDR "0.0.0.0"
/* Different modem report different null ipv6 address in different way */
#define IPV6_NULL_ADDR_1 "0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0"
#define IPV6_NULL_ADDR_2 "::"
#define IPV6_NULL_ADDR_3 "0:0:0:0:0:0:0:0"

/* includes have changed between kernel versions */
#include <netutils/ifc.h>

static void requestOrSendDataCallList(RIL_Token *t);
/*
 * Global variable to be able to send the response once the ip address
 * has been obtained and set
 */

static RIL_Token *SetupDataCall_token = NULL;
static int DataCallSetupOngoing = 0;
static int LastErrorCause = PDP_FAIL_NONE;

static int ExplicitRilRouting = 0;

struct NetworkAdapter {
    char                              *ifname;
    struct NetworkAdapter             *aliasOf;
    RIL_Data_Call_Response_v6         *dataCallResp;
    int                               channelId;     /* %IPDPACT mapping */
};

struct Ipv6Addr {
   char *dns;
   char *gw;
};

struct PdnInfo {
    struct Ipv6Addr ipv6Addr;
    char * defaultApnName;
    int pdnState;
};

/* Provision one extra data call placeholder for default APN */
#define MAX_DATA_CALL (MAX_NETWORK_IF + 1)
static struct NetworkAdapter s_network_adapter[MAX_NETWORK_IF];
/* Provision one extra data call for default APN */
RIL_Data_Call_Response_v6 s_data_call[MAX_DATA_CALL];

/*
 * Storage for info on pdns:
 *      ipv6 address,
 *      defaultApnName,
 *      whether they are a default pdn or not
 */
static struct PdnInfo pdnInfo[MAX_DATA_CALL];

#if RIL_VERSION >= 9
RIL_InitialAttachApn ia_cached_apn;
#endif

static int v_NmbOfConcurentContext = 0;
extern int ipv6_supported;


static int IsIPV6NullAddress(char *AddrV6)
{
    return ( (strcmp(AddrV6, IPV6_NULL_ADDR_1) == 0)
             || (strcmp(AddrV6, IPV6_NULL_ADDR_2) == 0)
             || (strcmp(AddrV6, IPV6_NULL_ADDR_3) == 0) );
}

static int IsIPV4NullAddress(char *Addr)
{
    return (strcmp(Addr, IPV4_NULL_ADDR) == 0);
}

int GetNmbConcurentProfile(void)
{
    return v_NmbOfConcurentContext;
}

/*
 * Only used for test suite to request route handling at RIL level
 * rather than in the framework (which is not used in that configuration
 */
void ConfigureExplicitRilRouting(int Set)
{
    ExplicitRilRouting = Set;
}


static void CompleteDataCallError(int Status, int dataCall, int dcStatus, int retryTime)
{
    int response_size = sizeof(RIL_Data_Call_Response_v6);
    RIL_Data_Call_Response_v6 *response, localDc;

    ALOGD("CompleteDataCallError status: %d dc: %d dcStatus %d retryTime: %d",
                                                  Status, dataCall, dcStatus, retryTime);

    if(SetupDataCall_token == NULL)
    {
        /* No token, this isn't expected... */
        ALOGE("Error: No setupDataCall token!");
        DataCallSetupOngoing = 0;
        return;
    }

    if (dataCall < MAX_DATA_CALL)
    {
        /* Act on actual data call list from RIL - deactivation may be in progress */
        response = &s_data_call[dataCall];
    }
    else
    {
        /* Early failure - no data call allocated */
        memset(&localDc, 0, response_size);
        response         = &localDc;
        /* null cid - deactivated */
        response->cid    = 0;
        response->active = 0;
    }

    /* Populate data call status - store failure cause for later use */
    LastErrorCause               = dcStatus;
    response->status             = LastErrorCause;
    response->suggestedRetryTime = retryTime;

    RIL_onRequestComplete(SetupDataCall_token, Status, response, response_size);
    SetupDataCall_token = NULL;
    DataCallSetupOngoing = 0;
    return;
}

static void CompleteDataCall(int Status, int dataCall)
{
    int response_size=0;
    ALOGD("CompleteDataCall");
    if(SetupDataCall_token == NULL)
    {
        /* No token, this isn't expected... */
        ALOGE("Error: No setupDataCall token!");
        DataCallSetupOngoing = 0;
        return;
    }

    RIL_Data_Call_Response_v6 *response = &s_data_call[dataCall];
    response_size = sizeof(RIL_Data_Call_Response_v6);
    RIL_onRequestComplete(SetupDataCall_token, Status, response, response_size);
    SetupDataCall_token = NULL;
    DataCallSetupOngoing = 0;
    return;
}

/*
 * Helper functions
 */

static struct NetworkAdapter *dataCallToNetworkAdapter(int dataCall)
{
    RIL_Data_Call_Response_v6 *dataCallResp;
    int nwAdapter;

    for (nwAdapter = 0; nwAdapter < GetNmbConcurentProfile(); nwAdapter++)
    {
        dataCallResp = s_network_adapter[nwAdapter].dataCallResp;
        if (dataCallResp == &s_data_call[dataCall])
            return &s_network_adapter[nwAdapter];
    }
    return NULL;
}

static struct NetworkAdapter *allocNetworkAdapter(int dataCall)
{
    int nwAdapter;

    for(nwAdapter=0; nwAdapter < GetNmbConcurentProfile(); nwAdapter++)
    {
        if(s_network_adapter[nwAdapter].dataCallResp == NULL) {
                s_network_adapter[nwAdapter].dataCallResp = &s_data_call[dataCall];
                s_data_call[dataCall].ifname = s_network_adapter[nwAdapter].ifname;
                ALOGD("Allocate adapter %s to cid %d",
                    s_data_call[dataCall].ifname, s_data_call[dataCall].cid);
                return &s_network_adapter[nwAdapter];
        }
    }
    return NULL;
}

static void freeNetworkAdapter(struct NetworkAdapter *nwAdapter)
{
    if((nwAdapter >= &s_network_adapter[0])
        && (nwAdapter <= &s_network_adapter[GetNmbConcurentProfile() - 1]))
    {
        if (nwAdapter->dataCallResp == NULL)
            ALOGE("Network adapter already freed");
        else
            nwAdapter->dataCallResp = NULL;
    }
    else
        ALOGE("Network adapter to be freed - invalid address");
}

static void releaseNetworkAdapter(int dataCall)
{
    struct NetworkAdapter *nwAdapter;
    int status;

    nwAdapter = dataCallToNetworkAdapter(dataCall);
    if (nwAdapter) {
        /*
         * Workaround specific to USB NCM interface
         * Let some time for MEDIA DISCONNECTED status to be drained from modem
         * Otherwise, host will stop polling endpoints too early and will be
         * continuously remote waked-up by device.
         */
        if (strStartsWith(s_data_call[dataCall].ifname, "rmnet")) {
           /*
            * Can't poll interface status for completion, as receipt of MEDIA
            * DISCONNECTED is not reflected on interface state.
            * 10 ms seems enough - use 50 ms as conservative value
            */
            usleep(50*1000);
        }

        status = ifc_down(s_data_call[dataCall].ifname);
        if (status)
            ALOGW("taking down if %s, returned status %d\n", s_data_call[dataCall].ifname, status);

        ALOGD("Freed adapter %s from cid %d", s_data_call[dataCall].ifname,
                                               s_data_call[dataCall].cid);
        freeNetworkAdapter(nwAdapter);
        s_data_call[dataCall].ifname = "none";
    }
}

static int getCurrentApnConf(int this_cid, char** apn, int * authtype, char ** user, char** password, char ** ip_type)
{
    int err;
    ATResponse *p_response   = NULL;
    ATLine *p_cur;
    char *out;
    *apn =NULL;
    *user=NULL;
    *password=NULL;
    *ip_type =NULL;

    ALOGD("getCurrentApnConf()");
    /* First get the APN */
    err = at_send_command_multiline ("AT+CGDCONT?", "+CGDCONT:", &p_response);
    if (err == 0 && p_response->success != 0)
    {
        /* Browse through the contexts */
        for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
            char *line = p_cur->line;
            int cid;
            char *type;
            char *address;

            err = at_tok_start(&line);
            if (err < 0)
                goto error;

            err = at_tok_nextint(&line, &cid);
            if (err < 0)
                goto error;

            /* Focus only on the cId of interest */
            if(cid != this_cid)
            {
                continue;
            }

            /* Type */
            err = at_tok_nextstr(&line, &out);
            if (err < 0)
                goto error;

            asprintf(ip_type, "%s",out);

            /* APN */
            err = at_tok_nextstr(&line, &out);
            if (err < 0)
                goto error;

            asprintf(apn, "%s",out);

            break;
        }
    }
    at_response_free(p_response);
    p_response = NULL;

    /* Same for authentication cfg */
    err = at_send_command_multiline ("AT%IPDPCFG?", "%IPDPCFG:", &p_response);
    if (err == 0 && p_response->success != 0)
    {
        /* Browse through the contexts */
        for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
            char *line = p_cur->line;
            int cid;
            int skip;

            err = at_tok_start(&line);
            if (err < 0)
                goto error;

            err = at_tok_nextint(&line, &cid);
            if (err < 0)
                goto error;

            /* Focus only on the cId of interest */
            if(cid != this_cid)
            {
                continue;
            }

            /* Skip (nbns) */
            err = at_tok_nextint(&line, &skip);
            if (err < 0)
                goto error;

            /*Auth type */
            err = at_tok_nextint(&line, authtype);
            if (err < 0)
                goto error;

            /* user */
            err = at_tok_nextstr(&line, &out);
            if (err < 0)
                goto error;

            asprintf(user, "%s",out);

            /* password */
            err = at_tok_nextstr(&line, &out);
            if (err < 0)
                goto error;

            asprintf(password, "%s",out);

            break;
        }
    }
    at_response_free(p_response);

    error:
    return err;
}

static int getCurrentCallState(int this_cid)
{
    /* Answer 0 if no call listed for the given cid */
    int callState = 0;
    int cid;
    int err;
    ATResponse *p_response   = NULL;
    ATLine *p_cur;
    ALOGD("getCurrentCallState()");

    err = at_send_command_multiline ("AT%IPDPACT?", "%IPDPACT:", &p_response);

    if (err == 0 && p_response->success != 0) {
        for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
            char *line = p_cur->line;
            int cid;

            err = at_tok_start(&line);
            if (err < 0)
                goto error;

            err = at_tok_nextint(&line, &cid);
            if (err < 0)
                goto error;

            /* Focus only on the cId of interrest */
            if(cid != this_cid)
            {
                continue;
            }

            /* Call state */
            err = at_tok_nextint(&line, &callState);
        }
    }
    error:
    return callState;
}

/**
 * RIL_REQUEST_SETUP_DATA_CALL
 */

void requestSetupDataCall(void *data, size_t datalen, RIL_Token t)
{
    const char *apn,*username, *password, *type;
    int confNeedsChecking = 1;
    int authType = 0;
    char *cmd;
    int err, dataCall, defaultApnCid;;
    ATResponse *p_response   = NULL;
    ATLine *p_cur;
    char * currentApn, *currentUsername,*currentPassword, *currentType;
    int currentAuth;
    int error_code = RIL_E_GENERIC_FAILURE;
    struct NetworkAdapter *nwAdapter = NULL;

    /*
     * Check if we're in the middle of processing a connection request
     * We need to complete the current request before handling another one
     */
    if(DataCallSetupOngoing != 0)
    {
        ALOGD("Call request ongoing, delaying request");
        /* Wait 1 second before reprocessing the request */
        void *req = (void *)ril_request_object_create(RIL_REQUEST_SETUP_DATA_CALL, data, datalen, t);
        if(req != NULL)
        {
            postDelayedRequest(req, 1);
        }

        return;
    }

    /* extract request's parameters */
    apn = ((const char **)data)[2];
    username = ((const char **)data)[3];
    password = ((const char **)data)[4];
    type = ((const char **)data)[6];

    /*
     * store the token so we can answer the request later
     * We need to return from this so we can process internal requests in
     * order to obtain the ip address.
     */
    SetupDataCall_token = t;
    DataCallSetupOngoing++;

    /*
     * Check if there is a suitable pre-established context
     * (LTE default APN, or other network-initated PDP)
     */
    for(dataCall=0;dataCall < MAX_DATA_CALL;dataCall++)
    {
        /* Case of alphabetic characters from APN name is not significant */
        if((pdnInfo[dataCall].pdnState == 4) &&
            (pdnInfo[dataCall].defaultApnName != NULL) &&
            (strcasecmp(pdnInfo[dataCall].defaultApnName, apn) == 0))
        {
            confNeedsChecking=0;
            break;
        }
    }

    /* No network-initiated APN pending with the requested APN, look for other context */
    defaultApnCid = s_data_call[DEFAULT_APN_DATA_CALL_OFFSET].cid;
    if(dataCall >= MAX_DATA_CALL)
    {
        for(dataCall=0;dataCall < MAX_DATA_CALL;dataCall++)
        {
            /* Special test for Initial APN cid */
            if (s_data_call[dataCall].cid == defaultApnCid)
            {
                /* Context active - skip */
                if(s_data_call[dataCall].active != 0)
                    continue;
                /* Requested APN is same as Initial APN */
                if (strcasecmp(ia_cached_apn.apn, apn) == 0)
                {
                    ALOGI("Reuse CID from LTE Initial Attach APN");
                    confNeedsChecking=0;
                    break;
                }
                else {
                    continue;
                }
            }
            if(s_data_call[dataCall].active == 0)
                break;
        }
    }

    if(dataCall >= MAX_DATA_CALL)
    {
        ALOGE("Too many concurent calls - no free data call");
#ifdef MULTI_PDP_GB
        error_code = RIL_E_NO_NET_INTERFACE;
#endif
        goto error_no_datacall;
    }

    /* allocate network adapter for this context */
    nwAdapter = allocNetworkAdapter(dataCall);
    if(nwAdapter == NULL)
    {
        ALOGE("Too many concurent calls - no free network adapter");
#ifdef MULTI_PDP_GB
        error_code = RIL_E_NO_NET_INTERFACE;
#endif
        goto error_no_datacall;
    }

    ALOGD("Assigning PDP to data cid %d network adapter %s",
          s_data_call[dataCall].cid, s_data_call[dataCall].ifname);

    /* Reset the error cause */
    s_data_call[dataCall].status = PDP_FAIL_NONE;
    s_data_call[dataCall].suggestedRetryTime = 0;
    LastErrorCause = PDP_FAIL_NONE;

    /*
     * debug socket only gives the apn (in the wrong place as well)
     * need a specific treatment
     */
    if(datalen == sizeof(char*))
    {
        ALOGD("Request from debug socket, best effort attempt only");
        apn = ((const char **)data)[0];
        username = "";
        password = "";
        type = "IP";
        authType = 0;
    }
    else
    {

        /* Check that parameters are there. */
        if(username == NULL)
            username= "";
        if(password== NULL)
            password = "";
        if(apn == NULL)
            apn = "";
        if(type == NULL)
            type = "IP";

        /* Convert requested authentication to the encoding used by our at commands */
        switch (*((const char **)data)[5])
        {
            case '1':
                authType = IPDPCFG_PAP;
                break;
            case '2':
            case '3':
                authType = IPDPCFG_CHAP;
                break;
            default:
                /* No authentication */
                authType = 0;
        }
    }

    /* We need this info to be able to return it to the framework */
    if(s_data_call[dataCall].type == NULL)
    {
        asprintf(&s_data_call[dataCall].type, "%s",type);
    }

    if(confNeedsChecking)
    {
        if(ipv6_supported)
        {
            ALOGD("requesting data connection to APN '%s - IPV6 supported'", apn);
            asprintf(&cmd, "AT+CGDCONT=%d,\"%s\",\"%s\"", s_data_call[dataCall].cid, type, apn);
        }
        else
        {
            ALOGD("requesting data connection to APN '%s - IPV6 not supported'", apn);
            asprintf(&cmd, "AT+CGDCONT=%d,\"IP\",\"%s\"", s_data_call[dataCall].cid, apn);
            type = "IP";
        }

        free(s_data_call[dataCall].type);
        asprintf(&s_data_call[dataCall].type, "%s",type);

        err = at_send_command(cmd, &p_response);
        free(cmd);

        if (err < 0 || p_response->success == 0)
        {
            ALOGE("Problem setting the profile");
            /*
             * although we will abandon this particular attempt,
             * try some recovery steps for next attempt
             */
            asprintf(&cmd,"AT%%IPDPACT=%d,0",s_data_call[dataCall].cid);
            err = at_send_command(cmd, NULL);
            free(cmd);
            goto error;
        }
        at_response_free(p_response);

        if(authType != 0)
        {
            asprintf(&cmd,"AT%%IPDPCFG=%d,0,%d,\"%s\",\"%s\"",
                     s_data_call[dataCall].cid, authType,username,password);
        }
        else
        {
            /*
             * this is needed as this setup is persistent between calls,
             * we couldn't unset it otherwise
             */
            asprintf(&cmd,"AT%%IPDPCFG=%d,0,0",s_data_call[dataCall].cid);
        }
        err = at_send_command(cmd, &p_response);
        free(cmd);

        if (err < 0 || p_response->success == 0)
        {
            goto error;
        }
        at_response_free(p_response);
    }


    /*
     * Icera-specific command to activate PDP context #1
     * The request will conclude when the IP address has been obtained or the call fails.
     */
    asprintf(&cmd,"AT%%IPDPACT=%d,1,\"IP\",%d",s_data_call[dataCall].cid, nwAdapter->channelId);
    err = at_send_command(cmd, &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    at_response_free(p_response);

    return;
error:
    /*
     * Release allocated network adapter - if any
     */
    if (nwAdapter)
        freeNetworkAdapter(nwAdapter);
    /*
     * Give the modem a bit of breathing space: 3 seconds should do,
     * otherwise framework will retry immediately.
     */
    CompleteDataCallError(RIL_E_SUCCESS, dataCall, PDP_FAIL_ERROR_UNSPECIFIED, 3000);

    at_response_free(p_response);
    return;

error_no_datacall:
    /* Fatal error, no data call could be allocated - no retry (INT_MAX) */
    CompleteDataCallError(RIL_E_SUCCESS, MAX_DATA_CALL, PDP_FAIL_ERROR_UNSPECIFIED, INT_MAX);
    at_response_free(p_response);
}

/**
 * RIL_REQUEST_DEACTIVATE_DATA_CALL
 **/
void requestDeactivateDataCall(void *data, size_t datalen, RIL_Token t)
{
    int cid, dataCall, i;
    char *cmd;
    int err;
    ATResponse *p_response = NULL;
    struct NetworkAdapter *nwAdapter;

    cid = atoi(((char **)data)[0]);

    /* Find the related nwIF */
    for(dataCall=0;dataCall < MAX_DATA_CALL;dataCall++)
    {
        if(s_data_call[dataCall].cid == cid)
            break;
    }

    if(dataCall >= MAX_DATA_CALL)
    {
        ALOGE("Unknown data call...");
        goto error;
    }

    /*
     * Deactivate PDP context
     * If this is last PDP context while camped in LTE, modem only deactivates
     * interface (%IPDPACT: x,4) instead of PDP deactivation (%IPDPACT: x,0)
     */
    asprintf(&cmd, "AT%%IPDPACT=%d,0", cid);
    err = at_send_command(cmd, &p_response);
    free(cmd);

    /* Try to free network adapter even if deactivation failed */
    releaseNetworkAdapter(dataCall);

    if (err < 0 || p_response->success == 0) {
        at_response_free(p_response);
        goto error;
    }
    at_response_free(p_response);

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/**
 * RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE
 */
static int getCallFailureCause(void)
{
    char *line = NULL;
    int nwErrRegistration, nwErrAttach, nwErrActivation;
    int err = 0;
    int response = 0;
    ATResponse *p_response;

    err = at_send_command_singleline("AT%IER?", "%IER:", &p_response);
    if (err < 0 || p_response->success == 0)
        goto error;

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0)
        goto error;
    err = at_tok_nextint(&line, &nwErrRegistration);
    if (err < 0)
       goto error;

    err = at_tok_nextint(&line, &nwErrAttach);
    if (err < 0)
        goto error;

    err = at_tok_nextint(&line, &nwErrActivation);
    if (err < 0)
        goto error;

    goto finally;
    error:
    nwErrActivation = -1;
    finally:
    at_response_free(p_response);
    return nwErrActivation;
}

void requestLastDataFailCause(void *data, size_t datalen, RIL_Token t)
{
    int nwErrActivation;
    if(LastErrorCause != PDP_FAIL_NONE)
    {
        nwErrActivation = LastErrorCause;
    }
    else
    {
        nwErrActivation = getCallFailureCause();
    }

    if(nwErrActivation>=0)
    {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &nwErrActivation, sizeof(int));
    }
    else
    {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

void onDataCallListChanged(void *param)
{
    requestOrSendDataCallList(NULL);
}


void requestDataCallList(void *data, size_t datalen, RIL_Token t)
{
    requestOrSendDataCallList(&t);
}

static void requestOrSendDataCallList(RIL_Token *t)
{
    void *data;
    int datalen;

    /* Return the list that is maintained */
    data=(void *)s_data_call;
    datalen = sizeof(RIL_Data_Call_Response_v6) * MAX_DATA_CALL;

    if (t != NULL)
        RIL_onRequestComplete(*t, RIL_E_SUCCESS, data, datalen);
    else
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, data, datalen);
}

void InitDataCallList(int client)
{
    int i, cidRangeStart;

    switch(client)
    {
        case 1:
            cidRangeStart=6;
            break;
        default:
            ALOGE("Unexpected client");
            /* No break */
        case 0:
            cidRangeStart=1;
            break;
    }

    memset(&s_data_call[0], 0, MAX_DATA_CALL*sizeof(RIL_Data_Call_Response_v6));
    for (i = 0; i < MAX_DATA_CALL; i++)
    {
        s_data_call[i].ifname = "none";
        s_data_call[i].cid    = cidRangeStart + i;
        s_data_call[i].active = 0;
        s_data_call[i].status = PDP_FAIL_NONE;
        pdnInfo[i].ipv6Addr.dns  = NULL;
        pdnInfo[i].ipv6Addr.gw   = NULL;
        pdnInfo[i].defaultApnName = NULL;
        pdnInfo[i].pdnState = 0;
    }
}

void AddNetworkInterface(char * NwIF)
{
    char *alias;
    int i;

    if(v_NmbOfConcurentContext >= MAX_NETWORK_IF)
    {
        ALOGE("AddNetworkInterface: Too many network interfaces");
        return;
    }

    /* Initialize the structure */
    s_network_adapter[v_NmbOfConcurentContext].ifname       = NwIF;
    s_network_adapter[v_NmbOfConcurentContext].aliasOf      = NULL;
    s_network_adapter[v_NmbOfConcurentContext].dataCallResp = NULL;
    /* Assumption is that interfaces are declared in modem channel order */
    s_network_adapter[v_NmbOfConcurentContext].channelId    = v_NmbOfConcurentContext;

     /* Check if the interface is an alias (for route configuration)*/
    alias =  strchr(NwIF, ':');
    if(alias!= NULL)
    {
         int name_length = (int) (alias - NwIF);

         // Interface is an alias. Find the parent interface.
         for(i=0; i<v_NmbOfConcurentContext; i++)
         {
             // Compare interface name before alias ID separator ':"
             if(strncmp(NwIF,
                        s_network_adapter[i].ifname,
                        name_length) == 0)
             {
                  // Check parent interface is not an alias itself
                  if(s_network_adapter[i].aliasOf == NULL)
                  {
                      s_network_adapter[v_NmbOfConcurentContext].aliasOf = &s_network_adapter[i];
                  }
             }
         }
         // Ensure the parent interface always exists for an alias.
         if( s_network_adapter[v_NmbOfConcurentContext].aliasOf == NULL)
         {
             ALOGE("AddNetworkInterface: Parent interface must exist before defining its alias");
         }
    }

    v_NmbOfConcurentContext++;
}

void ParseIpdpact(const char* s)
{
    ALOGD("ParseIpdpact");
    char *line = (char*)s;
    int cid;
    int state;
    int err;

    int dataCall;

    /* expected line:
        %IPDPACT: <cid>,<state>,<ndisid>
        where state=0:deactivated,1:activated,2:activating,3:activation failed, 4:LTE mode (%IPDPAUTO)
    */

    err = at_tok_start(&line);
    if (err < 0)
    {
        ALOGE("invalid IPDPACT line %s\n", s);
        goto error;
    }
    err = at_tok_nextint(&line, &cid);
    if (err < 0)
    {
        ALOGE("Error Parsing IPDPACT line %s\n", s);
        goto error;
    }

    err = at_tok_nextint(&line, &state);
    if (err < 0)
    {
        ALOGE("Error Parsing IPDPACT line %s\n", s);
        goto error;
    }

    /*
     * third parameter is not reliable to figure out the nwIf
     * as it is not reported consistently (on call drop it seems to
     * become the error code).
     *
     * Work the data call out from the cId instead.
     */
    for(dataCall=0;dataCall < MAX_DATA_CALL;dataCall++)
    {
        if(s_data_call[dataCall].cid == cid)
            break;
    }

    if(dataCall >= MAX_DATA_CALL)
    {
        ALOGE("Unknown data call...");
        goto error;
    }
    /*
     * we need to move to a suitable command thread for
     * the rest of the processing.
     */
    int data[2];
    data[0] = dataCall;
    data[1] = state;
    ril_request_object_t* req = ril_request_object_create(IRIL_REQUEST_ID_PROCESS_DATACALL_EVENT, data, sizeof(data), NULL);
    if(req != NULL)
    {
        requestPutObject(req);
        return;
    }
    error:
        ALOGE("Error, aborting");
}

static void getDefaultApnDetails(int dataCall)
{
    int err;
    ATResponse *p_response = NULL;
    char * cmd = NULL;
    char * line = NULL;

    ALOGD("getDefaultApnDetails");

    free(pdnInfo[dataCall].defaultApnName);
    pdnInfo[dataCall].defaultApnName = NULL;

    asprintf(&cmd,"AT+CGCONTRDP=%d",s_data_call[dataCall].cid);
    err = at_send_command_singleline(cmd,"+CGCONTRDP:", &p_response);

    free(cmd);
    if((err==0) && (p_response->success != 0))
    {
        int cId,bearerId;
        char * defaultApn;
        regex_t regex;
        regmatch_t regmatch[2];
        int ret;

        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);
        if (err < 0)
            goto error;
        err = at_tok_nextint(&line, &cId);
        if ((err < 0)||(cId != s_data_call[dataCall].cid))
            goto error;

        err = at_tok_nextint(&line, &bearerId);
        if (err < 0)
            goto error;

        err = at_tok_nextstr(&line, &defaultApn);
        if (err < 0)
            goto error;

        /*
         * Trim APN Operator Identifier part: "mnc<MNC>.mcc<MCC>.gprs"
         * On CMW, mnc can (wrongly) be just 2 digits long, relax the regex accordingly
         */
        ret = regcomp(&regex, "(.+)\\.mnc[0-9]{2,3}\\.mcc[0-9]{3}\\.gprs$", REG_EXTENDED|REG_ICASE);
        if (ret == 0)
        {
            ret = regexec(&regex, defaultApn, 2, regmatch, 0);
            if (ret == 0)
            {
                if (regmatch[1].rm_so >= 0)
                    asprintf(&pdnInfo[dataCall].defaultApnName, "%.*s",
                        (int) (regmatch[1].rm_eo - regmatch[1].rm_so), &defaultApn[regmatch[1].rm_so]);
                else
                    ALOGE("Empty APN Network Identifier");
            }
            else
                ALOGE("regexec no match");

            regfree(&regex);
        }
        else
            ALOGE("regcomp error: %d", ret);

        /* In case of error, fall back to raw APN name */
        if (pdnInfo[dataCall].defaultApnName == NULL)
            asprintf(&pdnInfo[dataCall].defaultApnName,"%s",defaultApn);

        ALOGI("Default APN stored: %s", pdnInfo[dataCall].defaultApnName);
    } else
        ALOGE("Could not retrieve APN name from network initiated PDP");

    error:
        at_response_free(p_response);
}

#define IPV6_ADDR_FILE "/proc/net/if_inet6"
#define IPV6_ADDR_LINE_LEN 60
#define IPV6_ADDR_RETRY_INTERVAL 1 /*second*/
#define IPV6_ADDR_RETRY_TIMEOUT 10 /*Usually takes less than 5s. Use 10s as a conservative value.*/

void getIpV6Address(int dataCall, int time)
{
    struct NetworkAdapter *nwAdapter=NULL;
    FILE *f = fopen(IPV6_ADDR_FILE, "r");
    char line[IPV6_ADDR_LINE_LEN];
    char addr[40]; /* 32 chars + 7 separators + ending char */
    int i,j;
    int found = 0;
    char *cmd;

    nwAdapter= dataCallToNetworkAdapter(dataCall);
    if (f && nwAdapter) {
        while (fgets(line, IPV6_ADDR_LINE_LEN, f)) {
            /* Check interface name and search for a non link-local address.*/
            if (strstr(line, nwAdapter->ifname) && (strncasecmp(line,"fe80",4)!=0)) {
                /* Add the separators ':' in the address. */
                for (i=0; i<8; i++ ) {
                    memcpy(&addr[5*i], (char *)&line[4*i], 4);
                    addr[5*i+4] = ':';
                }
                addr[39]='\0'; /*Overwrite last separator since it is not expected here. */

                ALOGD("Global address found for %s: %s", nwAdapter->ifname, addr);
                found = 1;
                break;
           }
       }
       fclose(f);
    }

    /* Increase time spent waiting for this autoconf and compare to timeout.*/
    time = time + IPV6_ADDR_RETRY_INTERVAL;

    if (found) {
        /* If global address is found then report it to Android.*/
        char *temp_addr, *temp_dns, *temp_gw;
        if (strcmp(s_data_call[dataCall].addresses, "") == 0) {
            /* There was no other address (IPV4). */
            asprintf(&temp_addr, "%s", addr);
            asprintf(&temp_dns, "%s", pdnInfo[dataCall].ipv6Addr.dns);
            asprintf(&temp_gw, "%s", pdnInfo[dataCall].ipv6Addr.gw);
        } else {
            /* Add this IPv6 address to the list of dataCall's addesses. */
            asprintf(&temp_addr, "%s %s", s_data_call[dataCall].addresses, addr);
            asprintf(&temp_dns, "%s %s", s_data_call[dataCall].dnses, pdnInfo[dataCall].ipv6Addr.dns);
            asprintf(&temp_gw, "%s %s", s_data_call[dataCall].gateways, pdnInfo[dataCall].ipv6Addr.gw);
        }
        free(s_data_call[dataCall].addresses);
        free(s_data_call[dataCall].dnses);
        free(s_data_call[dataCall].gateways);
        s_data_call[dataCall].addresses = temp_addr;
        s_data_call[dataCall].dnses = temp_dns;
        s_data_call[dataCall].gateways = temp_gw;
        CompleteDataCall(RIL_E_SUCCESS,dataCall);

    } else if (time>IPV6_ADDR_RETRY_TIMEOUT) {
        if (strcmp(s_data_call[dataCall].addresses, "") == 0) {
            /* There was no other address (IPV4). Close the PDP context and report error to Android.*/
            asprintf(&cmd,"AT%%IPDPACT=%d,0",s_data_call[dataCall].cid);
            at_send_command(cmd, NULL);
            free(cmd);

            CompleteDataCallError(RIL_E_SUCCESS, dataCall, PDP_FAIL_ERROR_UNSPECIFIED, -1);
        } else {
            CompleteDataCall(RIL_E_SUCCESS,dataCall);
        }
    } else {
        /* Wait 1 second before checking again for this address */
        int data[2];
        data[0] = dataCall;
        data[1] = time;
        void *req = (void *)ril_request_object_create(IRIL_REQUEST_ID_GET_IPV6_ADDR, data, sizeof(data), NULL);
        if(req != NULL)
        {
            postDelayedRequest(req, IPV6_ADDR_RETRY_INTERVAL);
        }
    }
    return ;
}



static void getIpAddress(int dataCall)
{
    int err;
    ATResponse *p_response = NULL;
    ALOGD("getIpAddress");
    char *devAddr = NULL;
    char *gwProperty = NULL;
    char *gwAddr, *dns1Addr, *dns2Addr, *dns1AddrV4, *dns2AddrV4, *dummy, *netmask;
    char *devAddrV6 = NULL, *gwAddrV6 = NULL, *dns1AddrV6 = NULL, *dns2AddrV6 = NULL;
    char *line, *tmp, *tmp_addr;
    char *cmd;
    int prefix_length;
    struct NetworkAdapter *nwAdapter;
    int iter;

    err = at_send_command_multiline("AT%IPDPADDR?","%IPDPADDR:", &p_response);

    if((err==0) && (p_response->success != 0))
    {
        int cId;
        char *cmd = NULL;
        ATLine *p_cur = p_response->p_intermediates;

        /* Find ip addresses in the response */
        while(p_cur !=NULL)
        {
            line = p_cur->line;

            err = at_tok_start(&line);
            if (err < 0) {
                goto error;
            }
            err = at_tok_nextint(&line, &cId);

            if (err < 0) {
                goto error;
            }

            /* Stop searching if we've got the right call */
            if(cId == s_data_call[dataCall].cid)
                break;

            p_cur = p_cur->p_next;
        }

        if(p_cur == NULL)
        {
            ALOGE("Couldn't find a matching data call");
            goto error;
        }

        // The device address (IPv4)
        err = at_tok_nextstr(&line,&devAddr);
        if(err<0) goto error;

        // The gateway (IPv4)
        err = at_tok_nextstr(&line,&gwAddr);
        if(err<0) goto error;

        /*
         * The 1st & 2nd DNS (IPv4)
         */
        err = at_tok_nextstr(&line,&dns1AddrV4);
        if(err<0) goto error;
        err = at_tok_nextstr(&line,&dns2AddrV4);
        if(err<0) goto error;

        /*
         *  Skip the nbns if they're provided (IPv4)
         */
        err = at_tok_nextstr(&line,&dummy);
        if(err<0) goto error;
        err = at_tok_nextstr(&line,&dummy);
        if(err<0) goto error;

        /*
         *  Check if we have a netmask, use default if not. (IPv4)
         */
        err = at_tok_nextstr(&line,&netmask);

        /*
         * Skip dhcp
         */
        err = at_tok_nextstr(&line,&dummy);
        if(err<0) goto error;
/*
 * Not an LTE limitation, but the CMW gives out
 * very strange IPV6 addresses, let's ignore it
 * for the time being.
 */
#ifndef LTE_TEST

        /* Check if IPV6 addresses are present*/
        if(at_tok_hasmore(&line))
        {
            /*
             * IPV6 device address
             */
            err = at_tok_nextstr(&line,&tmp_addr);
            if(err<0) goto error;
            /* Convert to a suitable format that the framework expect */
            tmp = ConvertIPV6AddressToStandard(tmp_addr);
            devAddrV6 = (tmp!=NULL)?tmp:strdup(tmp_addr);

            /*
             * IPV6 Gateway
             */
            err = at_tok_nextstr(&line,&tmp_addr);
            if(err<0) goto error;
            /* Convert to a suitable format that the framework expect */
            tmp = ConvertIPV6AddressToStandard(tmp_addr);
            gwAddrV6 = (tmp!=NULL)?tmp:strdup(tmp_addr);

            /*
             * IPV6 1st & 2nd DNS
             */
            err = at_tok_nextstr(&line,&tmp_addr);
            if(err<0) goto error;
            /* Convert to a suitable format that the framework expect */
            tmp = ConvertIPV6AddressToStandard(tmp_addr);
            dns1AddrV6 = (tmp!=NULL)?tmp:strdup(tmp_addr);

            err = at_tok_nextstr(&line,&tmp_addr);
            if(err<0) goto error;
            /* Convert to a suitable format that the framework expect */
            tmp = ConvertIPV6AddressToStandard(tmp_addr);
            dns2AddrV6 = (tmp!=NULL)?tmp:strdup(tmp_addr);

            /*
             *  Don't use last parameters
             */

            ALOGD("Addresses device (%s): %s\ngw: %s\ndns1: %s\ndns2: %s\nmask: %s\nipv6 address: %s\nipv6 gw: %s\nipv6 dns1: %s\nipv6 dns2: %s\n",s_data_call[dataCall].ifname, devAddr, gwAddr,dns1AddrV4,dns2AddrV4,netmask,  devAddrV6, gwAddrV6,dns1AddrV6,dns2AddrV6);
        }
        else
#endif /* LTE_TEST */
        {
            /* Default in case there is no IPV6 address provided in the answer.*/
            devAddrV6   = strdup(IPV6_NULL_ADDR_1);

            /* Print only IPV4 addresses */
            ALOGD("Addresses device (%s): %s\ngw: %s\ndns1: %s\ndns2: %s\nmask: %s\n",s_data_call[dataCall].ifname, devAddr, gwAddr,dns1AddrV4,dns2AddrV4,netmask);
        }

        /* Update the data call structure */
        free(s_data_call[dataCall].addresses);
        free(s_data_call[dataCall].dnses);
        free(s_data_call[dataCall].gateways);
        free(pdnInfo[dataCall].ipv6Addr.dns);
        free(pdnInfo[dataCall].ipv6Addr.gw);

        if(IsIPV6NullAddress(devAddrV6))
        {
            asprintf(&s_data_call[dataCall].addresses, "%s",devAddr);
            asprintf(&s_data_call[dataCall].dnses, "%s %s",dns1AddrV4,dns2AddrV4);
            asprintf(&s_data_call[dataCall].gateways,"%s",gwAddr);
            pdnInfo[dataCall].ipv6Addr.dns = NULL;
            pdnInfo[dataCall].ipv6Addr.gw  = NULL;
        }
        else if (IsIPV4NullAddress(devAddr))
        {
            /* Don't write the IPV6 address now since the one reported by the modem is likely
             * a link local address. Will be updated later after autoconf of global address.
             * DNS addresses are the ones provided by the network so record them so that they can be
             * reported later when IPV6 autoconf will complete.
             */
            s_data_call[dataCall].addresses=strdup("");
            s_data_call[dataCall].dnses=strdup("");
            s_data_call[dataCall].gateways=strdup("");

            asprintf(&pdnInfo[dataCall].ipv6Addr.dns, "%s %s",dns1AddrV6,dns2AddrV6);
            asprintf(&pdnInfo[dataCall].ipv6Addr.gw, "%s", gwAddrV6);
        }
        else
        {
            asprintf(&s_data_call[dataCall].addresses, "%s",devAddr);/*See comment above about IPV6 address.*/
            asprintf(&s_data_call[dataCall].dnses, "%s %s",dns1AddrV4,dns2AddrV4);
            asprintf(&s_data_call[dataCall].gateways,"%s",gwAddr);

            asprintf(&pdnInfo[dataCall].ipv6Addr.dns, "%s %s",dns1AddrV6,dns2AddrV6);
            asprintf(&pdnInfo[dataCall].ipv6Addr.gw, "%s", gwAddrV6);
        }
        /* We don't concern ourselves with gateways and masks as we're dealing with point to point connections */

        /*
         * Turning interface UP.
         */
        ALOGI("Starting IF configure");

        /*
         * Mock modem doesn't have any network device, but we still want to
         * simulate the datacall being setup and referenced in the RIL structures
         */
        if(isMockModem()) {
            CompleteDataCall(RIL_E_SUCCESS,dataCall);
            goto end;
        }

        if (ifc_init() != 0) {
            ALOGE("ifc_init failed");
            goto error;
        }

        /* Get associated network adapter */
        nwAdapter= dataCallToNetworkAdapter(dataCall);
        if (nwAdapter == NULL) {
            ALOGE("Unknown network adapter...");
            goto error;
        }

        if(nwAdapter->aliasOf)
        {
            // If interface is an alias. turn up the parent interface instead of the interface itself.
            if(ifc_up(nwAdapter->aliasOf->ifname))
            {
                ALOGE("failed to turn on parent interface %s: %s\n",
                     nwAdapter->aliasOf->ifname, strerror(errno));
                goto error;
            }
        }
        else
        {
            // Interface is not an alias. turn it on.
            if(ifc_up(nwAdapter->ifname)) {
                ALOGE("failed to turn on interface %s: %s\n",
                nwAdapter->ifname, strerror(errno));
                goto error;
            }
        }

        /*
         * Configure IPV4 interface.
         */
        if(!IsIPV4NullAddress(devAddr))
        {
            in_addr_t ip = inet_addr(devAddr);
            in_addr_t mask = inet_addr(netmask);
            in_addr_t gw = inet_addr(gwAddr);

            if (ifc_set_addr(nwAdapter->ifname, ip)) {
                ALOGE("failed to set ipaddr %s: %s\n", devAddr, strerror(errno));
                goto error;
            }

            /* get prefix length from mask*/
            prefix_length = 0;
            while(ntohl(mask) & (0x80000000 >> prefix_length))
            {
                prefix_length++;
            }

            if(ifc_set_prefixLength(nwAdapter->ifname, prefix_length))
            {
                ALOGE("failed to set prefix length %d: %s\n",prefix_length , strerror(errno));
                goto error;
            }

            if(ExplicitRilRouting)
            {
                if (ifc_create_default_route(nwAdapter->ifname, gw)) {
                    ALOGE("failed to set default route %s: %s\n", gwAddr, strerror(errno));
                    goto error;
                }
            }
        }

        /*
         * Configure IPV6 interface.
         */
        if(!IsIPV6NullAddress(devAddrV6)) {

            if (ifc_add_address(nwAdapter->ifname, devAddrV6, 64)){
                ALOGE("failed to set IPV6 address %s: %s\n", devAddrV6, strerror(errno));
                goto error;
            }

            if(ExplicitRilRouting)
            {
                /* Add a default route for IPV6 if explicitely needed. */
                if (ifc_add_route(nwAdapter->ifname, IPV6_NULL_ADDR_1, 0, IPV6_NULL_ADDR_1)){
                    ALOGE("failed to set IPV6 address %s: %s\n", devAddrV6, strerror(errno));
                    goto error;
                }
            }
        }

        ALOGD("Network configuration succedded\n");
    }
    else
    {
        ALOGE("IPDPADDR problem\n");
        goto error;
    }

    if (!IsIPV6NullAddress(devAddrV6))
    {
        /* In case of IPV6 then completion of the datacall is done only when an IPv6 global address is available. */
        getIpV6Address(dataCall, 0);
    }
    else
    {
       CompleteDataCall(RIL_E_SUCCESS,dataCall);
    }
    goto end;

error:
    ALOGE("Network adapter configuration problem");
    /* Could not work out addresses for this PDP - deactivate */
    asprintf(&cmd,"AT%%IPDPACT=%d,0",s_data_call[dataCall].cid);
    at_send_command(cmd, NULL);
    free(cmd);

    CompleteDataCallError(RIL_E_SUCCESS, dataCall, PDP_FAIL_ERROR_UNSPECIFIED, -1);

end:
    at_response_free(p_response);

    free(devAddrV6);
    free(gwAddrV6);
    free(dns1AddrV6);
    free(dns2AddrV6);

    return;
}

void notifyConnectionState(int connected) {
    int notify = 0, i;

    /* From ril.h:
     * 0=inactive, 1=active/physical link down, 2=active/physical link up
     */
    int newState = (connected != 0)? 2: 1;

    /* Iterate through the datacall to look for the current connection state */
    for (i = 0; i < MAX_DATA_CALL; i++)
    {
        /* Call is connected and State is different */
        if((s_data_call[i].active != 0)&&(s_data_call[i].active != newState)) {
            notify++;
            s_data_call[i].active = newState;
        }
    }

    if(notify) {
        onDataCallListChanged(NULL);
    }
}

static void dataTestModePacketForwardUpdate(void)
{
    /*
     * Modem testmode - variant RIL enabled (=2)
     * Monitor data call state
     * - start RNDIS packet forward on first call activation
     * - stop RNDIS packet forward  on last call deactivation
     */

    char prop[PROPERTY_VALUE_MAX];
    int modemtest_mode, rndis_forward;
    int dataCall, dataCallActive;

    property_get("persist.modem.icera.testmode", prop, "0");
    modemtest_mode = strtol(prop, NULL, 10);

    if (modemtest_mode == 2)
    {
        property_get("gsm.modem.icera.testmode.rndis", prop, "0");
        rndis_forward = strtol(prop, NULL, 10);

        dataCallActive = 0;
        for(dataCall=0;dataCall < MAX_DATA_CALL;dataCall++)
        {
            /* Case of alphabetic characters from APN name is not significant */
            if (pdnInfo[dataCall].pdnState == 1)
            {
                dataCallActive = 1;
                break;
            }
        }
        if ((dataCallActive) && (!rndis_forward))
        {
            ALOGI("Start RNDIS forward");
            property_set("gsm.modem.icera.testmode.rndis", "1");
        }
        else if ((!dataCallActive) && (rndis_forward))
        {
            ALOGI("Stop RNDIS forward");
            property_set("gsm.modem.icera.testmode.rndis", "0");
        }
    }
}

void ProcessDataCallEvent(int dataCall, int State)
{
    pdnInfo[dataCall].pdnState = State;
    switch(State)
    {
        case 1:
        {
            /*
             * Call is connected... Get the ip address
             * and acknowledge the request completion.
             */
            ALOGD("PDP Context activation cid %d", s_data_call[dataCall].cid);
            s_data_call[dataCall].active = 2;
            s_data_call[dataCall].status = 0;

            if(dataCallToNetworkAdapter(dataCall) != NULL) {
                getIpAddress(dataCall);
            } else {
                ALOGD("Unreferenced datacall, likely network initiated, ignoring");
            }
            break;
        }
        case 2:
        {
            if(SetupDataCall_token == NULL)
            {
                /*
                 * Must be LTE setup. Block other data call
                 * attempt for the time being as it seems to
                 * affect the modem.
                 */
                DataCallSetupOngoing++;
            }
            break;
        }
        case 4:
        {
            ALOGD("PDP Context cid %d network-activated, network adapter down",
                                                   s_data_call[dataCall].cid);
            s_data_call[dataCall].active = 1;

            /* Need to query the APN supplied by the network */
            getDefaultApnDetails(dataCall);

            /* Allow datacall to be setup from now on */
            DataCallSetupOngoing = 0;

            /* Free associated network adapter */
            releaseNetworkAdapter(dataCall);

            break;
        }

        case 3:
        {
            int cause;

            /*
             * Problem starting the call...
             */
            s_data_call[dataCall].active = 0;

            /*
             * In RIL v6, we now need to find out why it failed
             * If no specific error returned by the modem, put a generic
             * error cause
             */
            cause = getCallFailureCause();
            if(cause  == PDP_FAIL_NONE)
            {
                cause = PDP_FAIL_ERROR_UNSPECIFIED;
            }
            /*
             * Now need to return success in failure cases with failure
             * details in response.
             */
            CompleteDataCallError(RIL_E_SUCCESS, dataCall, cause, -1);
            /*
             * Free associated network adapter
             */
            releaseNetworkAdapter(dataCall);
            break;
        }
        case 0:
        {
            /*
             *  Call was terminated or dropped...
             */
            ALOGD("PDP Context deactivation cid %d",
                                               s_data_call[dataCall].cid);

            /*
             * Free associated network adapter
             */
            releaseNetworkAdapter(dataCall);

            /*
             * Clean up potential default APN
             */
            free(pdnInfo[dataCall].defaultApnName);
            pdnInfo[dataCall].defaultApnName = NULL;

            s_data_call[dataCall].active = 0;
            s_data_call[dataCall].status = PDP_FAIL_NONE;

            /* In RIL v6, we now need to find out why it failed */
            s_data_call[dataCall].status = getCallFailureCause();
            onDataCallListChanged(NULL);

            break;
        }
    }
    dataTestModePacketForwardUpdate();
}

/*
 * Converts from 0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0 to
 * 0:0:0:0:0:0:0:0
 * returns NULL if not the expected format
 * Caller to free the string.
 */
char * ConvertIPV6AddressToStandard(char * src)
{
    int val[8]={0,0,0,0,0,0,0,0};
    int i=0;
    char * ptr = src-1;
    char * dst = NULL;

    do{
        ptr++;
        val[i/2]+= atoi(ptr)<<(8*((i+1)%2));
        i++;
        ptr = strstr(ptr, ".");
    }while((ptr!=NULL)&&(i<=16));

    if(i==16)
        asprintf(&dst,"%x:%x:%x:%x:%x:%x:%x:%x",val[0],val[1],val[2],val[3],val[4],val[5],val[6],val[7]);
    return dst;
}

#if RIL_VERSION >= 9
#define IA_CACHE_FILE   "/data/rfs/ril/ia_cache.txt"
#define IA_CACHE_FILE_SIZE_MAX 256

static void dataIaFreeApn (RIL_InitialAttachApn *ia)
{
    free(ia->apn);
    free(ia->protocol);
    free(ia->username);
    free(ia->password);
}

static int dataIaSetLteIaPdpConfig(RIL_InitialAttachApn *ia)
{
    int err;
    char *cmd, *protocol;
    int  defaultApnCid, authType;
    ATResponse *p_response = NULL;
    char *type;

    defaultApnCid = s_data_call[DEFAULT_APN_DATA_CALL_OFFSET].cid;
    switch(ia->authtype) {
        case 1:
            authType = IPDPCFG_PAP;
            break;
        case 2:
        case 3:
            authType = IPDPCFG_CHAP;
            break;
        default:
            /* No authentication */
            authType = 0;
    }

    if (ipv6_supported)
        protocol = ia->protocol;
    else
        protocol = "IP";

    /* Empty Initial APN (default) */
    if (strlen(ia->apn) == 0) {
        asprintf(&cmd,"AT%%IPDPAUTO=%d,0,0,0", defaultApnCid);
        err = at_send_command(cmd, &p_response);
        free(cmd);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
        at_response_free(p_response);

        /* Clear APN */
        asprintf(&cmd, "AT+CGDCONT=%d,\"%s\",\"\"", defaultApnCid, protocol);
        err = at_send_command(cmd, &p_response);
        free(cmd);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
        at_response_free(p_response);
    /* Explicit Initial APN */
    } else {
        asprintf(&cmd,"AT%%IPDPAUTO=%d,0,0,1", defaultApnCid);
        err = at_send_command(cmd, &p_response);
        free(cmd);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
        at_response_free(p_response);

        /* Clear APN */
        asprintf(&cmd, "AT+CGDCONT=%d,\"%s\",\"%s\"", defaultApnCid, protocol, ia->apn);
        err = at_send_command(cmd, &p_response);
        free(cmd);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
        at_response_free(p_response);
    }

    /* Clear authorization scheme */
    if (authType == 0) {
        asprintf(&cmd,"AT%%IPDPCFG=%d,0,0", defaultApnCid);
        err = at_send_command(cmd, &p_response);
        free(cmd);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
        at_response_free(p_response);
    /* Configure authorization scheme */
    } else {
        asprintf(&cmd,"AT%%IPDPCFG=%d,0,%d,\"%s\",\"%s\"",
                 defaultApnCid, authType, ia->username, ia->password);
        err = at_send_command(cmd, &p_response);
        free(cmd);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
        at_response_free(p_response);
    }
    return 0;
error:
    RLOGW("%s Error during Inital APN configuration", __FUNCTION__);
    at_response_free(p_response);
    return -1;
}

static int dataIaDetach(void)
{
    int err, cfun_state = 1;
    ATResponse *p_response = NULL;
    char *line;

    err = at_send_command_singleline("AT+CFUN?","+CFUN:", &p_response);
    if (err != 0 || p_response->success == 0) {
        goto exit;
    }
    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0)
        goto exit;
    err = at_tok_nextint(&line, &cfun_state);
exit:
    at_response_free(p_response);
    if (cfun_state == 1) {
        changeCfun(4);
        return 1;
    } else
        return 0;
}

static void dataIaReattach(void)
{
    changeCfun(1);;
}


static int dataIaSetLteIa(RIL_InitialAttachApn *ia)
{
    int err, detach, reattach, dataCall, currentTechno;
    char *cmd;
    ATResponse *p_response = NULL;

    detach = 0;
    reattach = 0;
    currentTechno = GetCurrentTechno();
    if ((currentTechno == RADIO_TECH_LTE) || (currentTechno == RADIO_TECH_UNKNOWN))
    {
        /* LTE or unknown state - detach */
        detach = 1;
    }
    else
    {
        /* 2G/3G: Deactivate all active data calls */
        for(dataCall=0; dataCall < MAX_DATA_CALL; dataCall++)  {
            if (s_data_call[dataCall].active != 0) {
                asprintf(&cmd,"AT+CGACT=0,%d", s_data_call[dataCall].cid);
                err = at_send_command(cmd, &p_response);
                free(cmd);
                if ((err < 0) || (p_response->success == 0))
                    detach = 1;
                at_response_free(p_response);
            }
        }
    }

    /* Detach first if required */
    if (detach)
        reattach = dataIaDetach();

    /* Configure Initial APN */
    err = dataIaSetLteIaPdpConfig(ia);
    /* In case of error during config, retry after detach if not already done */
    if ((err < 0) && (!detach)) {
        reattach = dataIaDetach();
        err = dataIaSetLteIaPdpConfig(ia);
    }
    if (reattach)
        dataIaReattach();

    if (err < 0)
        ALOGE("Unrecovered error during Initial APN config procedure");

    return err;
}

/*
 * Inital APN cache file format:
 * "<apn>","<protocol>",<authtype>,"<username>","<password>"
 */

static void dataIaSetCachedApn(RIL_InitialAttachApn *ia)
{
    FILE *fp = NULL;
    char *cache = NULL;
    int err, ret;
    ATResponse *p_response = NULL;

    /* Write IA cache file */
    fp = fopen(IA_CACHE_FILE, "w");
    if (fp == NULL) {
        RLOGE("Could not open %s: %s", IA_CACHE_FILE, strerror(errno));
        goto error;
    }
    asprintf(&cache, "\"%s\",\"%s\",%d,\"%s\",\"%s\"\n",
                     ia->apn, ia->protocol, ia->authtype, ia->username, ia->password);
    ret = fwrite(cache, 1, strlen(cache), fp);
    if (ret != (int) strlen(cache)) {
        RLOGW("Could not write %s: %s", IA_CACHE_FILE, strerror(errno));
        goto error;
    }
    fclose(fp);

    free(cache);
    return;

error:
    RLOGE("Write cached IA: error");
    free(cache);
    if (fp != NULL)
        fclose(fp);
}

static void dataIaGetCachedApn(RIL_InitialAttachApn *ia)
{
    FILE *fp = NULL;
    char cache[IA_CACHE_FILE_SIZE_MAX];
    char *line, *token = NULL;
    ATResponse *p_response = NULL;
    int err;

    memset(ia, 0, sizeof(RIL_InitialAttachApn));

    /* Fetch IA cache file */
    fp = fopen(IA_CACHE_FILE, "r");
    if (fp == NULL) {
        RLOGW("Could not open %s: %s", IA_CACHE_FILE, strerror(errno));
        goto empty;
    }
    err = fread(cache, 1, IA_CACHE_FILE_SIZE_MAX, fp);
    if (!feof(fp)) {
        RLOGE("Did not reach EOF %s: %s", IA_CACHE_FILE, strerror(errno));
        goto empty;
    }

    line = cache;

    err = at_tok_nextstr(&line, &token);
    if (err < 0) {
        RLOGE("Could not parse IA apn from cache");
        goto empty;
    }
    ia->apn = strdup(token);

    err = at_tok_nextstr(&line, &token);
    if (err < 0) {
        RLOGE("Could not parse IA protocol from cache");
        goto empty;
    }
    ia->protocol = strdup(token);

    err = at_tok_nextint(&line, &ia->authtype);
    if (err < 0) {
        RLOGE("Could not parse IA authtype from cache");
        goto empty;
    }

    err = at_tok_nextstr(&line, &token);
    if (err < 0) {
        RLOGE("Could not parse IA username from cache");
        goto empty;
    }
    ia->username = strdup(token);

    err = at_tok_nextstr(&line, &token);
    if (err < 0) {
        RLOGE("Could not parse IA password from cache");
        goto empty;
    }
    ia->password = strdup(token);

    RLOGI("Read cached IA: apn (%s) protocol (%s) authtype (%d) username (%s) password (%s)",
                                ia->apn, ia->protocol, ia->authtype, ia->username, ia->password);
    fclose(fp);
    return;

empty:
    RLOGI("Read cached IA: default/empty APN");
    dataIaFreeApn(ia);
    if (fp != NULL)
        fclose(fp);
    ia->apn = strdup("");
    if (ipv6_supported)
        ia->protocol = strdup("IPV4V6");
    else
        ia->protocol = strdup("IP");
    ia->authtype = 0; // No authentication
    ia->username = strdup("");
    ia->password = strdup("");
    RLOGI("No cached IA - default: apn (%s) protocol (%s) authtype (%d) username (%s) password (%s)",
                                ia->apn, ia->protocol, ia->authtype, ia->username, ia->password);
}

static int dataIaIsEqualInitialAttachApn(RIL_InitialAttachApn *ia1, RIL_InitialAttachApn *ia2)
{
    if ( /* APN name comparison is not case sensitive */
         (strcasecmp(ia1->apn, ia2->apn) == 0)
         && (strcmp(ia1->protocol, ia2->protocol) == 0)
         && (ia1->authtype == ia2->authtype)
         && (strcmp(ia1->username, ia2->username) == 0)
         && (strcmp(ia1->password, ia2->password) == 0)
       )
        return 1;
    else
        return 0;
}

void requestSetInitialAttachApn(void *data, size_t datalen, RIL_Token t)
{
    RIL_InitialAttachApn *_ia = (RIL_InitialAttachApn *) data;
    RIL_InitialAttachApn myia;
    RIL_InitialAttachApn *ia = &myia;

    memset(ia, 0, sizeof(RIL_InitialAttachApn));
    if (_ia->apn != NULL)
        ia->apn = _ia->apn;
    else
        ia->apn = "";
    if (_ia->protocol != NULL)
        ia->protocol = _ia->protocol;
    else {
        if (ipv6_supported)
            ia->protocol = "IPV4V6";
        else
            ia->protocol = "IP";
    }
    if (_ia->authtype >= 0)
        ia->authtype = _ia->authtype;
    else
        ia->authtype = 0;
    if (_ia->username != NULL)
        ia->username = _ia->username;
    else
        ia->username = "";
    if (_ia->password != NULL)
        ia->password = _ia->password;
    else
        ia->password = "";

    /*
     * Ignore if requesting same Inital APN as we currently have in cache
     * hence already configured in the stack
     */
    if (dataIaIsEqualInitialAttachApn(ia, &ia_cached_apn)) {
        RLOGD("Same APN in cache - ignore");
        goto cache;
    }

    /* Activate IA configuration */
    dataIaSetLteIa(ia);

cache:
    /* Cache Inital APN config - update local version */
    dataIaSetCachedApn(ia);
    dataIaFreeApn(&ia_cached_apn);
    dataIaGetCachedApn(&ia_cached_apn);

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}
#endif


void initPdpContext(void )
{
    ATResponse *p_response = NULL;
    ATLine *nextATLine = NULL;
    char *line, *param, *cmd;
    int err;

    RLOGD(__FUNCTION__);

    /*
     * Get the available PDP types to determine if IPV6 is supported by the modem.
     */
    err = at_send_command_multiline ("AT+CGDCONT=?", "+CGDCONT:", &p_response);

    if (err != 0 || p_response->success == 0)
    {
        /* In case of error consider that IPV6 is not supported*/
        ipv6_supported = 0;
    }
    else
    {
        ipv6_supported = 0;

        /* Parse the answer trying to fing if CGDCONT IPV4V6 is supported.*/
        nextATLine = p_response->p_intermediates;

        while ((nextATLine != NULL) && (ipv6_supported==0))
        {
            line = nextATLine->line;

            /* skip prefix */
            err = at_tok_start(&line);
            if (err < 0)
                break;

            /* Skip first parameter*/
            err = at_tok_nextstr(&line,&param);
            if (err < 0)
                break;

            /* Check if IPV4V6 is in this line */
            err = at_tok_nextstr(&line,&param);
            if (err < 0)
                break;

            if(strncmp(param, "IPV4V6",6) == 0)
            {
                  ipv6_supported = 1;
            }

            nextATLine = nextATLine->p_next;
        }
    }

    at_response_free(p_response);

    /*
    * Reserve CID range used by RIL for Android-originated
    * data calls. Modem will not use those when binding CID
    * to network initiated data calls
    */

    asprintf(&cmd,"AT%%IRCID=1,\"%d-%d\"",
             s_data_call[0].cid, s_data_call[MAX_DATA_CALL - 1].cid);
    at_send_command(cmd, NULL);
    free(cmd);

#if RIL_VERSION >= 9
    /*
     * Retrieve cached IA
     * Configure Initial APN before stack is activated
     */
    dataIaFreeApn(&ia_cached_apn);
    dataIaGetCachedApn(&ia_cached_apn);
    dataIaSetLteIa(&ia_cached_apn);
#endif
}
