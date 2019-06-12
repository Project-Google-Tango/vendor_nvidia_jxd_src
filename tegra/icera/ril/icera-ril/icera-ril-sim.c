/* Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved. */
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
#include <assert.h>
#include <stdio.h>
#include <poll.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "atchannel.h"
#include "at_tok.h"
#include <icera-ril.h>
#include <icera-ril-sim.h>
#include <icera-ril-utils.h>
#include "icera-util.h"

#define LOG_TAG rilLogTag
#include <utils/Log.h>
#include "at_channel_writer.h"
#include "misc.h"
#include "icera-ril-utils.h"

#define CRSM_CHUNK_SIZE 255
#define READ_BINARY 176

static int getCardStatus(RIL_CardStatus_v6 **pp_card_status);
static void freeCardStatus(RIL_CardStatus_v6 *p_card_status);

void pollSIMState(void *param)
{
    /*
     * Do not change the system state because of a sim event
     * if the current state is RADIO_STATE_OFF
     */
    if(currentState() != RADIO_STATE_OFF)
    {
        switch(getSIMStatus()) {
            case SIM_ABSENT:
            case SIM_PIN:
            case SIM_PUK:
            case SIM_NETWORK_PERSONALIZATION:
            case SIM_PERM_BLOCKED:
            default:
                setRadioState(RADIO_STATE_SIM_LOCKED_OR_ABSENT);
            return;

            case SIM_NOT_READY:
            case SIM_ERROR:
                /* Error on SIM - reset the state before restarting to poll.*/
                setRadioState(RADIO_STATE_SIM_NOT_READY);
            return;

            case SIM_READY:
            case SIM_PIN2:
            case SIM_PUK2:
                setRadioState(RADIO_STATE_SIM_READY);
            return;
        }
    }
}

/**
 * RIL_REQUEST_GET_SIM_STATUS
 *
 * Requests status of the SIM interface and the SIM card.
 *
 * Valid errors:
 *  Must never fail.
 */
void requestGetSimStatus(void *data, size_t datalen, RIL_Token t)
{
    RIL_CardStatus_v6 *p_card_status;
    char *p_buffer;
    int buffer_size;
    int result = getCardStatus(&p_card_status);

    if (result == RIL_E_SUCCESS) {
        p_buffer = (char *)p_card_status;
        buffer_size = sizeof(*p_card_status);
    } else {
        p_buffer = NULL;
        buffer_size = 0;
    }

    RIL_onRequestComplete(t, result, p_buffer, buffer_size);
    freeCardStatus(p_card_status);
    return;
}


/**
 * RIL_REQUEST_SIM_IO
 *
 * Request SIM I/O operation.
 * This is similar to the TS 27.007 "restricted SIM" operation
 * where it assumes all of the EF selection will be done by the
 * callee.
 *
 * "data" is a const RIL_SIM_IO_v6 *
 * Please note that RIL_SIM_IO_v6 has a "PIN2" field which may be NULL,
 * or may specify a PIN2 for operations that require a PIN2 (eg
 * updating FDN records)
 *
 * "response" is a const RIL_SIM_IO_Response *
 *
 * Arguments and responses that are unused for certain
 * values of "command" should be ignored or set to NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *  SIM_PIN2
 *  SIM_PUK2
 */
void requestSIM_IO(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    RIL_SIM_IO_Response sr;
    int err;
    int chunks_num = 1;
    char *cmd = NULL;
    RIL_SIM_IO_v6 *p_args;
    char *line;
    int p1, p2, p3;
    int i;

    memset(&sr, 0, sizeof(sr));
    sr.simResponse = (char *)malloc(1);
    if (!sr.simResponse) goto error;
    *sr.simResponse = 0; // empty string

    p_args = (RIL_SIM_IO_v6 *)data;

    if (p_args->pin2 != NULL) {
        asprintf(&cmd, "AT+CPWD=\"P2\",\"%s\",\"%s\"", p_args->pin2, p_args->pin2);

        err = at_send_command(cmd, &p_response);
        free(cmd);

        if (err < 0 || p_response->success == 0) goto error;

        at_response_free(p_response);
        p_response = NULL;
    }

    // The condition for READ_BINARY allow to read huge files on SIM
    p1 = p_args->p1;
    p2 = p_args->p2;
    p3 = p_args->p3;

    if (p_args->command == READ_BINARY) {
        chunks_num = ((p3 - 1) / CRSM_CHUNK_SIZE) + 1;
    }

    for (i = 0; i < chunks_num; i++) {
        if (p_args->data == NULL) {
            // The condition for READ_BINARY allow to read huge files on SIM
            if (p_args->command == READ_BINARY) {
                p1 = ((i * CRSM_CHUNK_SIZE + p_args->p2) >> 8) + p_args->p1;
                p2 = (i * CRSM_CHUNK_SIZE + p_args->p2) & 0xFF;
                p3 = (i == chunks_num - 1 ? (p_args->p3 - 1) % CRSM_CHUNK_SIZE + 1 : CRSM_CHUNK_SIZE);
            }

            asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,,\"%s\"",
                        p_args->command, p_args->fileid,
                        p1, p2, p3,p_args->path);
        } else {
            asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,\"%s\",\"%s\"",
                        p_args->command, p_args->fileid,
                        p1, p2, p3, p_args->data,p_args->path);
        }

        err = at_send_command_singleline(cmd, "+CRSM:", &p_response);
        free(cmd);

        if (err < 0 || p_response->success == 0) goto error;



        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &(sr.sw1));
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &(sr.sw2));
        if (err < 0) goto error;

        if (at_tok_hasmore(&line)) {
            char *str;

            err = at_tok_nextstr(&line, &str);
            if (err < 0) goto error;

            sr.simResponse = (char *)realloc(sr.simResponse, strlen(sr.simResponse) + strlen(str) + 1);
            if (!sr.simResponse) goto error;

            strcat(sr.simResponse, str);
        }
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &sr, sizeof(sr));
    at_response_free(p_response);
    free(sr.simResponse);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    free(sr.simResponse);
}


/**
 * RIL_REQUEST_ENTER_SIM_PIN
 *
 * Supplies SIM PIN. Only called if RIL_CardStatus_v6 has RIL_APPSTATE_PIN state
 *
 * "data" is const char **
 * ((const char **)data)[0] is PIN value
 *
 * "response" is int *
 * ((int *)response)[0] is the number of retries remaining, or -1 if unknown
 *
 * Valid errors:
 *
 * SUCCESS
 * RADIO_NOT_AVAILABLE (radio resetting)
 * GENERIC_FAILURE
 * PASSWORD_INCORRECT
 */
void requestEnterSimPin(void *data, size_t  datalen, RIL_Token  t, int request)
{
    ATResponse   *p_response = NULL;
    int           err;
    int           numberOfRetries = -1;
    char         *cmd = NULL;
    char         *cmd_secure = NULL;
    const char  **strings = (const char **)data;
    RIL_Errno     failure = RIL_E_GENERIC_FAILURE;
    pinRetriesStruct pinRetries;


    switch(datalen / sizeof(char*))
    {
        case 2: /* Pin entry*/
            asprintf(&cmd, "AT+CPIN=\"%s\"", strings[0]);
            asprintf(&cmd_secure, "AT+CPIN=\"%s\"", "****");
            break;
        case 3: /* Puk entry */
            asprintf(&cmd, "AT+CPIN=\"%s\",\"%s\"", strings[0],strings[1]);
            asprintf(&cmd_secure, "AT+CPIN=\"%s\",\"%s\"", "****","****");

            break;
        default:
            goto end;
    }
    err = at_send_command_secure(cmd, cmd_secure, &p_response);
    free(cmd);
    free(cmd_secure);

    getNumOfRetries(&pinRetries);

    switch(request)
    {
        case RIL_REQUEST_ENTER_SIM_PIN:
            numberOfRetries = pinRetries.pin1Retries;
            break;
        case RIL_REQUEST_ENTER_SIM_PUK:
            numberOfRetries = pinRetries.puk1Retries;
            break;
        default:
            numberOfRetries = -1;
    }

    ALOGE("requestEnterSimPin: number of retries left: %d", numberOfRetries);

    if (err < 0)
        goto end;

    if (p_response->success == 0) {
        if ((at_get_cme_error(p_response) == CME_ERROR_INCORRECT_PASSWORD) ||
            (at_get_cme_error(p_response) == CME_ERROR_PUK_REQUIRED)) {
            failure = RIL_E_PASSWORD_INCORRECT;
        }
    } else {
        failure = RIL_E_SUCCESS;
        /* Notify that SIM is ready */
        setRadioState(RADIO_STATE_SIM_READY);
    }

end:
    RIL_onRequestComplete(t, failure, (void *)&numberOfRetries, sizeof(numberOfRetries));
    at_response_free(p_response);
}

/**
 * RIL_REQUEST_ENTER_SIM_PIN2
 *
 * Supplies SIM PIN. Only called if RIL_CardStatus_v6 has RIL_APPSTATE_PIN state
 *
 * "data" is const char **
 * ((const char **)data)[0] is PIN value
 *
 * "response" is int *
 * ((int *)response)[0] is the number of retries remaining, or -1 if unknown
 *
 * Valid errors:
 *
 * SUCCESS
 * RADIO_NOT_AVAILABLE (radio resetting)
 * GENERIC_FAILURE
 * PASSWORD_INCORRECT
 */
void requestEnterSimPin2(void *data, size_t  datalen, RIL_Token  t, int request)
{
    ATResponse   *p_response = NULL;
    int           err;
    int           numberOfRetries = -1;
    char         *cmd = NULL;
    char         *cmd_secure = NULL;
    const char  **strings = (const char **)data;
    RIL_Errno     failure = RIL_E_GENERIC_FAILURE;
    pinRetriesStruct pinRetries;


    switch(datalen / sizeof(char*))
    {
        case 2: /* Pin2 entry: this will force pin2 check at sim level */
            asprintf(&cmd, "AT+CPWD=\"P2\",\"%s\",\"%s\"", strings[0], strings[0]);
            asprintf(&cmd_secure, "AT+CPWD=\"P2\",\"%s\",\"%s\"", "****", "****");
            break;
        case 3: /* Puk2 entry, need to first check that PUK2 is needed */
        {
            char * cpinResult, * line;

            err = at_send_command_singleline("AT+CPIN?", "+CPIN:", &p_response);
            if (err < 0 || p_response->success == 0) {
                goto end;
            }

            line = p_response->p_intermediates->line;

            err = at_tok_start(&line);

            if (err < 0) {
                goto end;
            }

            err = at_tok_nextstr(&line, &cpinResult);

            if(strcmp (cpinResult, "SIM PUK2")) {
                /*
                 * Cpin is not expecting PUK2,
                 * Only way to enter it is via SS
                 */
                asprintf(&cmd, "ATD**052*%s*%s*%s#", strings[0],strings[1],strings[1]);
                asprintf(&cmd_secure, "ATD**052*%s*%s*%s#", "****","****","****");
            }
            else
            {
                asprintf(&cmd, "AT+CPIN=\"%s\",\"%s\"", strings[0],strings[1]);
                asprintf(&cmd_secure, "AT+CPIN=\"%s\",\"%s\"", "****","****");
            }
            at_response_free(p_response);
            break;
        }
        default:
            goto end;
    }
    err = at_send_command_secure(cmd, cmd_secure, &p_response);
    free(cmd);
    free(cmd_secure);

    getNumOfRetries(&pinRetries);
    switch(request)
    {
        case RIL_REQUEST_ENTER_SIM_PIN2:
            numberOfRetries = pinRetries.pin2Retries;
            break;
        case RIL_REQUEST_ENTER_SIM_PUK2:
            numberOfRetries = pinRetries.puk2Retries;
            break;
        default:
            numberOfRetries = -1;
            break;
    }

    ALOGE("requestEnterSimPin2: number of retries left: %d", numberOfRetries);

    if (err < 0)
        goto end;

    if(p_response->success == 0) {
        if ((at_get_cme_error(p_response) == CME_ERROR_INCORRECT_PASSWORD) ||
            (at_get_cme_error(p_response) == CME_ERROR_PUK2_REQUIRED)) {
            failure = RIL_E_PASSWORD_INCORRECT;
        }
    } else {
        failure = RIL_E_SUCCESS;
        /* Notify that SIM is ready */
        setRadioState(RADIO_STATE_SIM_READY);
    }

end:
    RIL_onRequestComplete(t, failure, (void *)&numberOfRetries, sizeof(numberOfRetries));
    at_response_free(p_response);
}


/**
 * RIL_REQUEST_QUERY_FACILITY_LOCK
 *
 * Query the status of a facility lock state
 *
 * "data" is const char **
 * ((const char **)data)[0] is the facility string code from TS 27.007 7.4
 *                      (eg "AO" for BAOC, "SC" for SIM lock)
 * ((const char **)data)[1] is the password, or "" if not required
 * ((const char **)data)[2] is the TS 27.007 service class bit vector of
 *                           services to query
 *
 * "response" is an int *
 * ((const int *)response) 0 is the TS 27.007 service class bit vector of
 *                           services for which the specified barring facility
 *                           is active. "0" means "disabled for all"
 *
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */
void requestQueryFacilityLock(void *data, size_t datalen, RIL_Token t)
{
    int err, rat, response;
    ATResponse *p_response = NULL;
    char *cmd = NULL;
    char *cmd_secure = NULL;
    char *line = NULL;
    char *facility_string = NULL;
    char *facility_password = NULL;
    char *facility_class = NULL;

    ALOGD("FACILITY");
    assert (datalen >= (3 * sizeof(char **)));

    facility_string   = ((char **)data)[0];
    facility_password = ((char **)data)[1];
    facility_class    = ((char **)data)[2];

    if((facility_password==NULL)||(strlen(facility_password)==0)) {
        /* class=0 may be invalid */
        if(strcmp(facility_class, "0") == 0) {
            asprintf(&cmd, "AT+CLCK=\"%s\",2", facility_string);
        } else {
            asprintf(&cmd, "AT+CLCK=\"%s\",2,,%s", facility_string, facility_class);
        }
        err = at_send_command_singleline(cmd, "+CLCK:", &p_response);
    } else {
        /* class=0 may be invalid */
        if(strcmp(facility_class, "0") == 0) {
            asprintf(&cmd, "AT+CLCK=\"%s\",2,\"%s\"", facility_string, facility_password);
            asprintf(&cmd_secure, "AT+CLCK=\"%s\",2,\"%s\"", facility_string, "****");
        }
        else {
            asprintf(&cmd, "AT+CLCK=\"%s\",2,\"%s\",%s", facility_string, facility_password, facility_class);
            asprintf(&cmd_secure, "AT+CLCK=\"%s\",2,\"%s\",%s", facility_string, "****", facility_class);
        }
        err = at_send_command_singleline_secure(cmd, cmd_secure, "+CLCK:", &p_response);
        free(cmd_secure);
    }

    free(cmd);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);

    if (err < 0) {
        goto error;
    }

    err = at_tok_nextint(&line, &response);

    if (err < 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
    at_response_free(p_response);
    return;

error:
    ALOGE("ERROR: requestQueryFacilityLock() failed\n");
    if ((p_response) && (at_get_cme_error(p_response) == CME_ERROR_SS_FDN_FAILURE)) {
        RIL_onRequestComplete(t, RIL_E_FDN_CHECK_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
}


/**
 * RIL_REQUEST_SET_FACILITY_LOCK
 *
 * Enable/disable one facility lock
 *
 * "data" is const char **
 *
 * ((const char **)data)[0] = facility string code from TS 27.007 7.4
 * (eg "AO" for BAOC)
 * ((const char **)data)[1] = "0" for "unlock" and "1" for "lock"
 * ((const char **)data)[2] = password
 * ((const char **)data)[3] = string representation of decimal TS 27.007
 *                            service class bit vector. Eg, the string
 *                            "1" means "set this facility for voice services"
 *
 * "response" is int *
 * ((int *)response)[0] is the number of retries remaining, or -1 if unknown
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *  PASSWORD_INCORRECT
 *
 */
void requestSetFacilityLock(void *data, size_t datalen, RIL_Token t)
{
    /* It must be tested if the Lock for a particular class can be set without
     * modifing the values of the other class. If not, first must call
     * requestQueryFacilityLock to obtain the previous value
     */
    int err = 0;
    int numberOfRetries = -1;
    char *cmd = NULL;
    char *cmd_secure = NULL;
    char *code = NULL;
    char *lock = NULL;
    char *password = NULL;
    char *class = NULL;
    ATResponse *p_response = NULL;
    RIL_Errno failure = RIL_E_GENERIC_FAILURE;
    pinRetriesStruct pinRetries;

    assert (datalen >= (4 * sizeof(char **)));

    code = ((char **)data)[0];
    lock = ((char **)data)[1];
    password = ((char **)data)[2];
    class = ((char **)data)[3];

    /* class=0 may be invalid */
    if( strcmp(class, "0") == 0) {
        asprintf(&cmd, "AT+CLCK=\"%s\",%s,\"%s\"", code, lock, password);
        asprintf(&cmd_secure, "AT+CLCK=\"%s\",%s,\"%s\"", code, lock, "****");
    } else {
        asprintf(&cmd, "AT+CLCK=\"%s\",%s,\"%s\",%s", code, lock, password, class);
        asprintf(&cmd_secure, "AT+CLCK=\"%s\",%s,\"%s\",%s", code, lock, "****", class);
    }

    err = at_send_command_secure(cmd, cmd_secure, &p_response);
    free(cmd);
    free(cmd_secure);

    getNumOfRetries(&pinRetries);

    if (0 == strcmp(code,"FD")) {
        numberOfRetries = pinRetries.pin2Retries;
    } else if (0 == strcmp(code,"SC")) {
        numberOfRetries = pinRetries.pin1Retries;
    }

    if(numberOfRetries!= -1)
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0);
    }

    ALOGE("requestSetFacilityLock: number of retries left: %d", numberOfRetries);

    if (err < 0)
        goto end;

    if(p_response->success == 0) {
        if (at_get_cme_error(p_response) == CME_ERROR_PIN2_REQUIRED) {
            failure = RIL_E_SIM_PIN2;
        } else if (at_get_cme_error(p_response) == CME_ERROR_PUK2_REQUIRED) {
            failure = RIL_E_SIM_PUK2;
        } else if (at_get_cme_error(p_response) == CME_ERROR_SS_FDN_FAILURE) {
            failure = RIL_E_FDN_CHECK_FAILURE;
            numberOfRetries = -1;
        } else {
            failure = RIL_E_PASSWORD_INCORRECT;
        }
    } else {
        failure = RIL_E_SUCCESS;
    }
end:
    RIL_onRequestComplete(t, failure, (void *)&numberOfRetries, sizeof(numberOfRetries));
    at_response_free(p_response);
}


/**
 * RIL_REQUEST_CHANGE_BARRING_PASSWORD
 *
 * Change call barring facility password
 *
 * "data" is const char **
 *
 * ((const char **)data)[0] = facility string code from TS 27.007 7.4
 * (eg "AO" for BAOC)
 * ((const char **)data)[1] = old password
 * ((const char **)data)[2] = new password
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */
void requestChangeBarringPassword(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err = 0;
    char *cmd = NULL;
    char *cmd_secure = NULL;
    char *string = NULL;
    char *old_password = NULL;
    char *new_password = NULL;

    assert (datalen >= (3 * sizeof(char **)));

    string       = ((char **)data)[0];
    old_password = ((char **)data)[1];
    new_password = ((char **)data)[2];

    asprintf(&cmd, "AT+CPWD=\"%s\",\"%s\",\"%s\"", string, old_password, new_password);
    asprintf(&cmd_secure, "AT+CPWD=\"%s\",\"%s\",\"%s\"", string, "****", "****");
    err = at_send_command_secure(cmd, cmd_secure, &p_response);
    free(cmd);
    free(cmd_secure);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    if ((p_response) && (at_get_cme_error(p_response) == CME_ERROR_SS_FDN_FAILURE)) {
        RIL_onRequestComplete(t, RIL_E_FDN_CHECK_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
    ALOGE("ERROR: requestChangeBarringPassword() failed, err = %d\n", err);
}

#define SIM_NOT_READY_TRY_COUNT 20

/** Returns SIM_NOT_READY on error */
SIM_Status getSIMStatus()
{
    ATResponse *p_response = NULL;
    int err;
    int ret,iter=0, cme_error;
    char *cpinLine;
    char *cpinResult;

    if (currentState() == RADIO_STATE_UNAVAILABLE) {
        ret = SIM_NOT_READY;
        goto done;
    }

    do
    {
        err = at_send_command_singleline("AT+CPIN?", "+CPIN:", &p_response);

        if ((err != 0) ||(p_response == NULL)){
            ret = SIM_NOT_READY;
            goto done;
        }
        cme_error = at_get_cme_error(p_response);
        switch (cme_error) {
            case CME_SUCCESS:
                break;

            case CME_SIM_NOT_INSERTED:
                ret = SIM_ABSENT;
                goto done;

            case CME_ERROR_SIM_FAILURE:
                ret = SIM_ERROR;
                goto done;

            case CME_ERROR_ME_DEPERSONALIZATION_BLOCKED:
                ret = SIM_NETWORK_PERSONALIZATION_BRICKED;
                goto done;

            case CME_ERROR_SIM_PUK_BLOCKED:
                ret = SIM_PERM_BLOCKED;
                goto done;

            case CME_ERROR_SIM_BUSY:
                iter++;
                if(iter > SIM_NOT_READY_TRY_COUNT)
                {
                    ret = SIM_NOT_READY;
                    goto done;
                }

                /* give the modem some time to progress */
                usleep(300*1000);

                /* get ready to resend the command */
                at_response_free(p_response);
                p_response = NULL;
                break;

            default:
                ret = SIM_NOT_READY;
                goto done;
        }
    }
    while(cme_error == CME_ERROR_SIM_BUSY);
    /* CPIN? has succeeded, now look at the result */

    cpinLine = p_response->p_intermediates->line;
    err = at_tok_start (&cpinLine);

    if (err < 0) {
        ret = SIM_NOT_READY;
        goto done;
    }

    err = at_tok_nextstr(&cpinLine, &cpinResult);

    if (err < 0) {
        ret = SIM_NOT_READY;
        goto done;
    }

    if (0 == strcmp (cpinResult, "SIM PIN")) {
        ret = SIM_PIN;
        goto done;
    } else if (0 == strcmp (cpinResult, "SIM PUK")) {
        ret = SIM_PUK;
        goto done;
    } else
    if (0 == strcmp (cpinResult, "SIM PIN2")) {
        ret = SIM_PIN2;
        goto done;
    } else if (0 == strcmp (cpinResult, "SIM PUK2")) {
        ret = SIM_PUK2;
        goto done;
    } else if (0 == strcmp (cpinResult, "PH-NET PIN")) {
        ret = SIM_NETWORK_PERSONALIZATION;
        goto done;
    } else if (0 != strcmp (cpinResult, "READY")) {
        /* we're treating unsupported lock types as "sim absent" */
        ret = SIM_ABSENT;
        goto done;
    }

    ret = SIM_READY;

done:
    at_response_free(p_response);
    return ret;
}

static RIL_AppType getCardType(void)
{
    ATResponse *p_response = NULL;
    int err;
    RIL_AppType ret =  RIL_APPTYPE_UNKNOWN;
    char *line;
    int sim;

    err = at_send_command_singleline("AT*TSIMINS?", "*TSIMINS:", &p_response);
    if((err<0)||(p_response->success== 0))      goto done;

    line =   p_response->p_intermediates->line;
    err = at_tok_start (&line);
    if(err<0)        goto done;

    err = at_tok_nextint(&line, &sim);
    if(err<0)        goto done;
    /*  1: SIM Present 0: SIM not present */
    err = at_tok_nextint(&line, &sim);
    if((err<0) || (sim==0))        goto done;

    err = at_tok_nextint(&line, &sim);
    if(err<0)        goto done;

   switch(sim)
   {
       case  0:
           ret =   RIL_APPTYPE_SIM;
           break;
       case 1:
           ret = RIL_APPTYPE_USIM;
           break;
       default:
            ret =   RIL_APPTYPE_UNKNOWN;
            break;
   }

done:
    at_response_free(p_response);
    return ret;

}


/**
 * Get the current card status.
 *
 * This must be freed using freeCardStatus.
 * @return: On success returns RIL_E_SUCCESS
 */
static int getCardStatus(RIL_CardStatus_v6 **pp_card_status)
{
    RIL_AppStatus app_status_array[] = {
        // SIM_ABSENT = 0
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // SIM_NOT_READY = 1
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_DETECTED, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // SIM_READY = 2
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // SIM_PIN = 3
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_PIN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
        // SIM_PUK = 4
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN },
        // SIM_NETWORK_PERSONALIZATION = 5
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
        // SIM_NETWORK_PERSONALIZATION_BRICKED = 6
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_PERM_BLOCKED, RIL_PINSTATE_UNKNOWN },
        // SIM_PIN2 = 7
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_ENABLED_NOT_VERIFIED},
        // SIM PUK2 = 8
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_ENABLED_BLOCKED},
        // SIM ERROR = 9
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN},
        // SIM PERM BLOCKED = 10
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_PERM_BLOCKED, RIL_PINSTATE_ENABLED_PERM_BLOCKED}
    };
    RIL_CardState card_state;
    int num_apps;
    int sim_status = getSIMStatus();
    int i;
    pinRetriesStruct pinRetries;

    switch(sim_status)
    {
        case SIM_ABSENT:
            card_state = RIL_CARDSTATE_ABSENT;
            num_apps = 0;
            break;
        case SIM_ERROR:
            card_state = RIL_CARDSTATE_ERROR;
            num_apps = 0;
            break;
        default:
            card_state = RIL_CARDSTATE_PRESENT;
            num_apps = 1;
            break;
    }

    // Allocate and initialize base card status.
    RIL_CardStatus_v6 *p_card_status = malloc(sizeof(RIL_CardStatus_v6));
    p_card_status->card_state = card_state;
    p_card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
    p_card_status->gsm_umts_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->cdma_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->ims_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->num_applications = num_apps;

    // Initialize application status
    for (i = 0; i < RIL_CARD_MAX_APPS; i++) {
        p_card_status->applications[i] = app_status_array[SIM_ABSENT];
    }

    // Pickup the appropriate application status
    // that reflects sim_status for gsm.
    if (num_apps != 0) {
        int err = -1;
        // Only support one app, gsm
        p_card_status->num_applications = 1;
        p_card_status->gsm_umts_subscription_app_index = 0;

        // Get the correct app status
        p_card_status->applications[0] = app_status_array[sim_status];
        p_card_status->applications[0].app_type = getCardType();

        /* Compute the pin state */
        err = getNumOfRetries(&pinRetries);

        if(err == 0)
        {
            if(pinRetries.puk1Retries == 0)
            {
                p_card_status->applications[0].pin1 = RIL_PINSTATE_ENABLED_PERM_BLOCKED;
            }
            else if(pinRetries.pin1Retries == 0)
            {
                p_card_status->applications[0].pin1 = RIL_PINSTATE_ENABLED_BLOCKED;
            }

            if(pinRetries.puk2Retries == 0)
            {
                p_card_status->applications[0].pin2 = RIL_PINSTATE_ENABLED_PERM_BLOCKED;
            }
            else if(pinRetries.pin2Retries == 0)
            {
                p_card_status->applications[0].pin2 = RIL_PINSTATE_ENABLED_BLOCKED;
            }
        }
    }

    *pp_card_status = p_card_status;
    return RIL_E_SUCCESS;
}

/**
 * Free the card status returned by getCardStatus
 */
static void freeCardStatus(RIL_CardStatus_v6 *p_card_status)
{
    free(p_card_status);
}

/*
 * return a file ID as int from its path on the SIM.
 * e.g. for "7FFF6F3B" this will return 0x6F3B i.e. 28475
 * or -1 in case of error
 */
static int getFileIdFromPath(const char *path) {
    int result = -1;
    int n = strlen(path);
    if (n>=4) {
       result  = hexToChar(path[n-1]);
       result |= hexToChar(path[n-2]) << 4;
       result |= hexToChar(path[n-3]) << 8;
       result |= hexToChar(path[n-4]) << 12;
    }
    return result;
}

/**
 * RIL_UNSOL_SIM_REFRESH
 *
 * Indicates that file(s) on the SIM have been updated, or the SIM
 * has been reinitialized.
 *
 * In the case where RIL is version 6 or older:
 * "data" is an int *
 * ((int *)data)[0] is a RIL_SimRefreshResult.
 * ((int *)data)[1] is the EFID of the updated file if the result is
 * SIM_FILE_UPDATE or NULL for any other result.
 *
 * In the case where RIL is version 7:
 * "data" is a RIL_SimRefreshResponse_v7 *
 *
 * Note: If the SIM state changes as a result of the SIM refresh (eg,
 * SIM_READY -> SIM_LOCKED_OR_ABSENT), RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED
 * should be sent.
 */

void unsolSIMRefresh(const char *s)
{
    static char *onGoingStkSimRefresh = NULL;
    RIL_SimRefreshResponse_v7 response;
    int n, inserted=0, err;
    char *line = NULL;
    char *p = NULL;
    char *files_id = NULL;
    char *file_path = NULL;


    if (!onGoingStkSimRefresh && strStartsWith(s, "*TSIMINS:")) {
        /*
         * If no refresh details were previously provided by the modem then this means that the SIM insersion was
         * not triggered by STK. Then report SIM initialized.  All files should be re-read.
         */
        line = strdup(s);
        p = line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &n);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &inserted);
        if (err < 0) goto error;

        if(inserted == 1) {
            response.result = SIM_INIT;
            response.ef_id  = 0;
            response.aid    = NULL;
            RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_REFRESH,
                                  &response, sizeof(response));
        }
    } else if (strStartsWith(s, "%IUSATREFRESH:")) {
        free(onGoingStkSimRefresh);
        onGoingStkSimRefresh = strdup(s);
    } else if (strStartsWith(s, "%IUSATREFRESH2")) {
        /*
         * Parse the SIM refresh details initiated by STK received earlier by:
         * %IUSATREFRESH: <type>,"[<aid>]","[<file1>,[<file2>,...]]"
         */
        line = strdup(onGoingStkSimRefresh);
        free(onGoingStkSimRefresh);
        onGoingStkSimRefresh=NULL;
        p = line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &n);
        if (err < 0) goto error;
        switch (n) {
            case 0x00: /* SIM_INIT_FULL_FILE_CHA */
            case 0x02: /* SIM_INIT_FILE_CHA_NOT */
            case 0x03: /* SIM_INIT */
                response.result = SIM_INIT;
                break;
            case 0x01: /* FILE_CHA_NOTIFICATION */
                response.result = SIM_FILE_UPDATE;
                break;
            case 0x04: /* SIM_RESET */
            case 0x05: /* APPLICATION_RESET */
            case 0x06: /* SESSION_RESET */
                response.result = SIM_RESET;
                break;
            default:
                goto error;
        }

        err = at_tok_nextstr(&line, &response.aid);
        if (err < 0) goto error;
        if (response.result == SIM_RESET) {
            /* Do not report aid for reset. */
            response.aid = NULL;
        }

        /* If not a file update message no need to parse file ID.
         * send the response now.
         */
        if (response.result != SIM_FILE_UPDATE) {
            /* No need of ef_id if not SIM_FILE_UPDATE.*/
            response.ef_id = 0;
            RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_REFRESH,
                                      &response, sizeof(response));
        }

        if (n == 0x02) {
            /* Special case of SIM_INIT + FILE CHANGE we need now to send SIM_REFRESH
             * with status = SIM_FILE_UPDATE in addition to the SIM_REFRESH with
             * status = SIM_INIT sent above.
             */
            response.result = SIM_FILE_UPDATE;
        }

        if (response.result == SIM_FILE_UPDATE) {
            /* File update - need to parse file ID to place them in response. */
            err = at_tok_nextstr(&line, &files_id);

            if (err < 0) goto error;
            while ( at_tok_nextstr(&files_id, &file_path) == 0) {
                /*Unsolicited from modem may contain several file ids. So send several messages to Android.*/
                n = getFileIdFromPath(file_path);
                if (n>=0) {
                    response.ef_id = n;
                    RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_REFRESH,
                                              &response, sizeof(response));
                }
            }
        }
    }

error:
    free(p);
}

/**
 * RIL_REQUEST_CHANGE_SIM_PIN
 *
 * Supplies old SIM PIN and new PIN.
 *
 * "data" is const char **
 * ((const char **)data)[0] is old PIN value
 * ((const char **)data)[1] is new PIN value
 *
 * "response" is int *
 * ((int *)response)[0] is the number of retries remaining, or -1 if unknown
 *
 * Valid errors:
 *
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE
 *  PASSWORD_INCORRECT
 *     (old PIN is invalid)
 *
 */
void requestChangePasswordPin(void *data, size_t datalen, RIL_Token t)
{
    int err = 0;
    int numberOfRetries = -1;
    int simCheckEnabled = 0;
    char *line;
    char *cmd = NULL;
    char *cmd_secure = NULL;
    char *old_password = NULL;
    char *new_password = NULL;
    ATResponse *p_response = NULL;
    RIL_Errno failure = RIL_E_GENERIC_FAILURE;
    pinRetriesStruct pinRetries;


    if (datalen != 3 * sizeof(char *))
        goto end;

    old_password = ((char **)data)[0];
    new_password = ((char **)data)[1];

    asprintf(&cmd, "AT+CLCK=\"%s\",2", "SC");
    err = at_send_command_singleline(cmd,"+CLCK:", &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0)
    {
        goto end;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);

    if (err < 0)
    {
        goto end;
    }

    err = at_tok_nextint(&line, &simCheckEnabled);

    if (err < 0)
    {
        goto end;
    }
    at_response_free(p_response);

    if(simCheckEnabled == 0)
    {
        asprintf(&cmd, "AT+CLCK=\"%s\",1,\"%s\"", "SC", old_password);
        asprintf(&cmd_secure, "AT+CLCK=\"%s\",1,\"%s\"", "SC", "****");
        err = at_send_command_secure(cmd, cmd_secure, &p_response);
        free(cmd);
        free(cmd_secure);

        if (err < 0)
            goto end;

        if(p_response->success == 0)
        {
            if ((at_get_cme_error(p_response) == CME_ERROR_INCORRECT_PASSWORD) ||
            (at_get_cme_error(p_response) == CME_ERROR_PUK_REQUIRED))
            {
                failure = RIL_E_PASSWORD_INCORRECT;
            }
            goto end;
        }
       at_response_free(p_response);
    }

    asprintf(&cmd, "AT+CPWD=\"%s\",\"%s\",\"%s\"", "SC", old_password,new_password);
    asprintf(&cmd_secure, "AT+CPWD=\"%s\",\"%s\",\"%s\"", "SC", "****","****");
    err = at_send_command_secure(cmd, cmd_secure, &p_response);
    free(cmd);
    free(cmd_secure);

    if (err < 0)
        goto end;

    if(p_response->success == 0)
    {
        if ((at_get_cme_error(p_response) == CME_ERROR_INCORRECT_PASSWORD) ||
            (at_get_cme_error(p_response) == CME_ERROR_PUK_REQUIRED))
        {
            failure = RIL_E_PASSWORD_INCORRECT;
        }
        goto end;
    }
    else
    {
        failure = RIL_E_SUCCESS;
    }

    if(simCheckEnabled == 0)
    {
        at_response_free(p_response);
        asprintf(&cmd, "AT+CLCK=\"%s\",0,\"%s\"", "SC", new_password);
        asprintf(&cmd_secure, "AT+CLCK=\"%s\",0,\"%s\"", "SC", "****");
        err = at_send_command_secure(cmd, cmd_secure, &p_response);
        free(cmd);
        free(cmd_secure);

        if (err < 0)
        {
            failure = RIL_E_GENERIC_FAILURE;
            goto end;
        }

        if(p_response->success == 0)
        {
            if ((at_get_cme_error(p_response) == CME_ERROR_INCORRECT_PASSWORD) ||
            (at_get_cme_error(p_response) == CME_ERROR_PUK_REQUIRED))
            {
                failure = RIL_E_PASSWORD_INCORRECT;
            }
            else
                failure = RIL_E_GENERIC_FAILURE;
            goto end;
        }
    }


end:
    getNumOfRetries(&pinRetries);

    numberOfRetries = pinRetries.pin1Retries;
    ALOGE("requestChangePassword: number of retries left: %d", numberOfRetries);
    RIL_onRequestComplete(t, failure, (void *)&numberOfRetries, sizeof(numberOfRetries));
    at_response_free(p_response);
    return;
}

/**
 * RIL_REQUEST_CHANGE_SIM_PIN2
 *
 * Supplies old SIM PIN2 and new PIN2.
 *
 * "data" is const char **
 * ((const char **)data)[0] is old PIN2 value
 * ((const char **)data)[1] is new PIN2 value
 *
 * "response" is int *
 * ((int *)response)[0] is the number of retries remaining, or -1 if unknown
 *
 * Valid errors:
 *
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE
 *  PASSWORD_INCORRECT
 *     (old PIN2 is invalid)
 *
 */
void requestChangePasswordPin2(void *data, size_t datalen, RIL_Token t)
{
    int err = 0;
    int numberOfRetries = -1;
    int simCheckEnabled = 0;
    char *line;
    char *cmd = NULL;
    char *cmd_secure = NULL;
    char *old_password = NULL;
    char *new_password = NULL;
    ATResponse *p_response = NULL;
    RIL_Errno failure = RIL_E_GENERIC_FAILURE;
    pinRetriesStruct pinRetries;


    if (datalen != 3 * sizeof(char *))
        goto end;

    old_password = ((char **)data)[0];
    new_password = ((char **)data)[1];

    asprintf(&cmd, "AT+CPWD=\"%s\",\"%s\",\"%s\"", "P2", old_password,new_password);
    asprintf(&cmd_secure, "AT+CPWD=\"%s\",\"%s\",\"%s\"", "P2", "****","****");
    err = at_send_command_secure(cmd, cmd_secure, &p_response);
    free(cmd);
    free(cmd_secure);

    if (err < 0)
        goto end;

    if(p_response->success == 0)
    {
        if (at_get_cme_error(p_response) == CME_ERROR_INCORRECT_PASSWORD)
        {
            failure = RIL_E_SIM_PIN2;
        } else if (at_get_cme_error(p_response) == CME_ERROR_PUK2_REQUIRED) {
            failure = RIL_E_SIM_PUK2;
        }
        goto end;
    }
    else
    {
        failure = RIL_E_SUCCESS;
    }

end:

    getNumOfRetries(&pinRetries);
    numberOfRetries = pinRetries.pin2Retries;
    ALOGE("requestChangePassword: number of retries left: %d", numberOfRetries);

    RIL_onRequestComplete(t, failure, (void *)&numberOfRetries, sizeof(numberOfRetries));
    at_response_free(p_response);
    return;
}
/**
 * Get the number of retries left for pin functions
 */
int getNumOfRetries(pinRetriesStruct *pinRetries)
{
    ATResponse *p_response = NULL;
    int err = 0;
    char *line;
    int num_of_retries = -1;

    pinRetries->pin1Retries = -1;
    pinRetries->puk1Retries = -1;
    pinRetries->pin2Retries = -1;
    pinRetries->puk2Retries = -1;

    err = at_send_command_singleline("AT%PINNUM?", "%PINNUM:", &p_response);

    if (err < 0 || p_response->success == 0)
        goto end;

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0)
        goto end;
    err = at_tok_nextint(&line, &num_of_retries);
    if (err < 0)
       goto end;
    pinRetries->pin1Retries = num_of_retries;

    err = at_tok_nextint(&line, &num_of_retries);
    if (err < 0)
        goto end;
    pinRetries->puk1Retries = num_of_retries;

    err = at_tok_nextint(&line, &num_of_retries);
    if (err < 0)
        goto end;
    pinRetries->pin2Retries = num_of_retries;

    err = at_tok_nextint(&line, &num_of_retries);
    if (err < 0)
        goto end;
    pinRetries->puk2Retries = num_of_retries;

end:
    at_response_free(p_response);
    return err;
}

static void* SimHotSwapMonitor(void* param)
{
    int fd;
    sim_state * simState=(sim_state *)param;
    char buf[15];
    struct pollfd fdset[1];
    int timeout;
    int len;

    ALOGD("SimHotSwapMonitor starting on sim id %d (%s)",simState->id,simState->path);

    fd = open(simState->path, O_RDONLY );

    if ( fd < 0 ) {
        ALOGE("error opening sim sysfs");
        return NULL;
    }

    read(fd, buf, sizeof(buf));
    ALOGD("Original Sim state: %s",buf);

    timeout = -1;
    fdset[0].fd = fd;
    fdset[0].events = POLLPRI;
    fdset[0].revents = 0;

    while(1)
    {
        poll(fdset, 1, timeout);
        /*
         * Quite a few spurious POLLERR
         * Need to filter out
         */
        if (fdset[0].revents & POLLPRI)
        {
            /*
             * Rewind from previous check
             */
            lseek(fdset[0].fd, 0,SEEK_SET);

            len = read(fdset[0].fd, buf, sizeof(buf));

            if(len<=0)
            {
                ALOGE("SimHotSwapMonitor on sim id %d had a problem",simState->id);

                /*
                 * Deliberetly not taking a wakelock
                 * Will run again when it is useful, wait at least 10 minutes
                 * to avoid trashing the logs.
                 */
                sleep(1000*60*10);
                continue;
            }

            /* Make sure we have time to send the notifications to the modem */
            IceraWakeLockAcquire();

            simState->state = (buf[0]='1')?1:0;

            /*
             * Send internal requests with the details
             */
            requestPutObject(ril_request_object_create(IRIL_REQUEST_ID_SIM_HOTSWAP, &simState, sizeof(sim_state*), NULL));
        }
    }

    return NULL;
}

void StartHotSwapMonitoring(int sim_id, char * sim_path)
{
    pthread_t thread_id;
    sim_state *simState = malloc(sizeof(sim_state));
    simState->id = sim_id;
    simState->path = sim_path;

    int err = pthread_create(&thread_id, NULL, SimHotSwapMonitor,simState);
}

/**
 * RIL_REQUEST_ISIM_AUTHENTICATION
 *
 * Request the ISIM application on the UICC to perform AKA
 * challenge/response algorithm for IMS authentication
 *
 * "data" is a const char * containing the challenge string in Base64 format
 * "response" is a const char * containing the response in Base64 format
 *
 * Valid errors:
 * SUCCESS
 * RADIO_NOT_AVAILABLE
 * GENERIC_FAILURE
 */
void requestSIMAuthentication(void *data, size_t datalen, RIL_Token t) {
    uint8_t *ascii_nonce = NULL;
    int ascii_nonce_len = 0;
    ATResponse *p_response = NULL;
    char *b64_nonce = (char *)data;

    char *cmd = NULL, *line = NULL;
    char hex_rand[33] = {0}, hex_autn[33] = {0}, *hex_sync = NULL;
    char *res = NULL, *b64_rst = NULL;
    char *hex_res = NULL, *hex_ck = NULL, *hex_ik = NULL;
    int err, res_len = 0, b64_rst_len = 0;
    int auth_rst = 0;
    int ril_result = RIL_E_GENERIC_FAILURE;

    // decode base64
    ascii_nonce_len = decode_Base64(b64_nonce, &ascii_nonce);
    if (ascii_nonce_len != 32) {
        ALOGE("nonce length is not correct : %d", ascii_nonce_len);
        goto error;
    }

    // Convert from ascii to HEX string, and divide it into rand & autn
    asciiToHex((char *)ascii_nonce, hex_rand, 16);      // first 16 bytes.
    asciiToHex((char *)ascii_nonce + 16, hex_autn, 16); // end 16 bytes.
    free(ascii_nonce);
    ascii_nonce = NULL;
    asprintf(&cmd, "AT%%IIMSAUTH=0,\"%s\",\"%s\"", hex_rand, hex_autn);

    err = at_send_command_singleline(cmd, "%IIMSAUTH:", &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        ALOGE("at_send_command_singleline failure");
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &auth_rst);
    if (err < 0) goto error;

    if (auth_rst == 0) {
        // IMS AUTH failed
        err = at_tok_nextstr(&line, &hex_sync);
        if (err < 0) goto error;

        ril_result = RIL_E_SUCCESS;
        b64_rst = NULL;
        b64_rst_len = 0;
        goto final;

    } else if (auth_rst == 1) {
        // IMS AUTH passed
        err = at_tok_nextstr(&line, &hex_res);
        if (err < 0) goto error;

        err = at_tok_nextstr(&line, &hex_ck);
        if (err < 0) goto error;

        err = at_tok_nextstr(&line, &hex_ik);
        if (err < 0) goto error;

        res_len = strlen(hex_res)/2;
        res = (char *)calloc(res_len + 1, sizeof(char));
        if (res == NULL) goto error;
        hexToAscii(hex_res, res, res_len);

    } else {
        ALOGE("response is not in expected. auth_rst = %d", auth_rst);
        goto error;
    }

    ALOGI("total size = %d", res_len);

    b64_rst_len = encode_Base64(res, &b64_rst, res_len);
    free(res);
    ril_result = RIL_E_SUCCESS;
    goto final;
error:
    free(ascii_nonce);
    ril_result = RIL_E_GENERIC_FAILURE;
    b64_rst = NULL;
    b64_rst_len = 0;
final:
    RIL_onRequestComplete(t, ril_result, b64_rst, b64_rst_len);
    at_response_free(p_response);
    free(b64_rst);
}
