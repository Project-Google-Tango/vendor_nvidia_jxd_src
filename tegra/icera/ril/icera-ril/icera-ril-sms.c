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

#include <stdio.h>
#include <assert.h>
#include <telephony/ril.h>
#include "atchannel.h"
#include "at_tok.h"
#include "misc.h"
#include "icera-ril.h"
#include "icera-ril-sms.h"
#include "icera-ril-utils.h"
#include "at_channel_writer.h"

#include "icera-util.h"

#define LOG_TAG rilLogTag
#include <utils/Log.h>

/*
 * Tracking whether we're still waiting for an SMS ACK
 * as Timed callback can't be canceled.
 */
static int WaitingForAck = 0;

/**
 * RIL_REQUEST_SEND_SMS
 *
 * Sends an SMS message.
*/
void requestSendSMS(void *data, size_t datalen, RIL_Token t)
{
    int err;
    const char *smsc;
    const char *pdu;
    char *line;
    int tpLayerLength;
    int mr=0;
    int errCode=CMS_SUCCESS;
    char *cmd1, *cmd2;
    RIL_SMS_Response response;
    ATResponse *p_response = NULL;

    smsc = ((const char **)data)[0];
    pdu = ((const char **)data)[1];

    tpLayerLength = strlen(pdu)/2;

    // "NULL for default SMSC"
    if (smsc == NULL) {
        smsc= "00";
    }

    asprintf(&cmd1, "AT+CMGS=%d", tpLayerLength);
    asprintf(&cmd2, "%s%s", smsc, pdu);

    err = at_send_command_sms(cmd1, cmd2, "+CMGS:", &p_response);

    free(cmd1);
    free(cmd2);

    if (err != 0)
        goto error;

    if (p_response->success == 0){

        errCode = at_get_cms_error(p_response);

        /* CMS ERROR occured */
        if (errCode >CMS_SUCCESS){
            response.messageRef =0;
            response.ackPDU = "";
            response.errorCode = errCode;

            switch (errCode) {
                case CMS_ERROR_NETWORK_TIMEOUT:
                case CMS_ERROR_NO_NETWORK_SERVICE:
                    ALOGE("ERROR: requestSendSms failed with RIL_E_SMS_SEND_FAIL_RETRY mapped toCMS_ERROR = %d\n",errCode);
                    RIL_onRequestComplete(t, RIL_E_SMS_SEND_FAIL_RETRY, &response, sizeof(response));
                     break;
                case CMS_ERROR_FDN_FAILURE:
                    ALOGE("ERROR: requestSendSms failed with RIL_E_FDN_CHECK_FAILURE mapped toCMS_ERROR = %d\n",errCode);
                    RIL_onRequestComplete(t, RIL_E_FDN_CHECK_FAILURE, &response, sizeof(response));
                     break;
                default:
                    response.errorCode = CMS_ERROR_UNKNOWN_ERROR;
                    ALOGE("ERROR: requestSendSms failed with RIL_E_GENERIC_FAILURE mapped to CMS_ERROR = %d\n",errCode);
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, &response, sizeof(response));
            }
        at_response_free(p_response);
        return;
        }
    }
    else if (p_response->success > 0){
        line = p_response->p_intermediates->line;

    /* Start parsing in case of successful sending */
        err = at_tok_start(&line);
        if (err < 0)
            goto error;

    /* Extract the <mr> messageRef parameter . */
        err = at_tok_nextint(&line, &mr);
        if (err < 0)
            goto error;

        response.messageRef = mr;
        response.ackPDU = "";
        response.errorCode = CMS_SUCCESS;

        RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));
        at_response_free(p_response);
        return;
    }
    else {
        ALOGE("ERROR: Shall never be reached: p_response->success is negative = %d\n",p_response->success);
    }
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    ALOGE("ERROR: requestSendSms failed\n");
    at_response_free(p_response);
}

void requestSendSMSExpectMore(void *data, size_t datalen, RIL_Token t)
{
  char *cmd = NULL;
  asprintf(&cmd, "AT+CMMS=1");
  at_send_command(cmd, NULL);
  free(cmd);
  requestSendSMS(data, datalen, t);
}

/**
 * RIL_REQUEST_SMS_ACKNOWLEDGE
 *
 * Acknowledge successful or failed receipt of SMS previously indicated
 * via RIL_UNSOL_RESPONSE_NEW_SMS .
*/
void requestSMSAcknowledge(void *data, size_t datalen, RIL_Token t)
{
    int ackSuccess;
    ATResponse *p_response = NULL;
    int err;
    /*
     * Stop the timeout
     */
    SmsAckCancelTimeOut(WaitingForAck);

    ackSuccess = ((int *)data)[0];

    if (ackSuccess == 1) {
        err = at_send_command("AT+CNMA=1", &p_response);
    } else if (ackSuccess == 0)  {
        err = at_send_command("AT+CNMA=2", &p_response);
    } else {
        ALOGE("unsupported arg to RIL_REQUEST_SMS_ACKNOWLEDGE\n");
        goto error;
    }

    /*
     * Something went wrong.
     * Try to mitigate so message can be redelivered
     * Will likely lead to duplicated message, but at least
     * SMS interface is not locked up until next LAU
     */
    if (err < 0 || p_response->success == 0)
    {
        SmsReenableSupport();
        goto error;
    }
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

    return;
error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);

}

/**
 * RIL_REQUEST_WRITE_SMS_TO_SIM
 *
 * Stores a SMS message to SIM memory.
 *
 * "data" is RIL_SMS_WriteArgs *
 *
 * "response" is int *
 * ((const int *)response)[0] is the record index where the message is stored.
 *
 * Valid errors:
 *  SUCCESS
 *  GENERIC_FAILURE
 *
 */
void requestWriteSmsToSim(void *data, size_t datalen, RIL_Token t)
{
    RIL_SMS_WriteArgs *p_args;
    char *cmd1 = NULL, *cmd2 = NULL, *line = NULL;
    int length;
    int err;
    int response = 0;
    ATResponse *p_response = NULL;

    p_args = (RIL_SMS_WriteArgs *)data;

    length = strlen(p_args->pdu)/2;
    asprintf(&cmd1, "AT+CMGW=%d,%d", length, p_args->status);
    asprintf(&cmd2, "%s%s", (p_args->smsc ? p_args->smsc : "00"), p_args->pdu);

    err = at_send_command_sms(cmd1, cmd2, "+CMGW:", &p_response);

    if (err != 0 || p_response->success == 0){
        ALOGD("CMGW error, p_response: %x, cms err: %d", (int)p_response,(p_response?at_get_cms_error(p_response):-1));
        if(p_response &&
          (at_get_cms_error(p_response) == CMS_ERROR_MEMORY_FULL))
        {
            RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);
        }
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0)
        goto error;

    err = at_tok_nextint(&line, &response);
    if (err < 0)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));
    at_response_free(p_response);
    free(cmd1);
    free(cmd2);
    return;
error:
    free(cmd1);
    free(cmd2);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

/**
 * RIL_REQUEST_DELETE_SMS_ON_SIM
 *
 * Deletes a SMS message from SIM memory.
*/
void requestDeleteSmsOnSim(void *data, size_t datalen, RIL_Token t)
{
    char * cmd;
    ATResponse *p_response = NULL;
    int err;

    asprintf(&cmd, "AT+CMGD=%d", ((int *)data)[0]);
    err = at_send_command(cmd, &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
}
/**
 * RIL_REQUEST_GSM_BROADCAST_GET_SMS_CONFIG
 */
void requestGSMGetBroadcastSMSConfig(void *data, size_t datalen, RIL_Token t)
{

    ATResponse *p_response = NULL;
    int err;
    int commasMids = 0;
    int entriesMids = 0;
    int commasDcss = 0;
    int entriesDcss = 0;
    int entriesRequired = 0;
    int i = 0;
    int countQuotes = 0;
    int hasDcss = 0;
    int mode = 0;
    char* line;
    char* p = NULL;
    char* q = NULL;
    char* r = NULL;
    RIL_GSM_BroadcastSmsConfigInfo* configInfo = NULL;
    RIL_GSM_BroadcastSmsConfigInfo** configInfoArray = NULL;

    err = at_send_command_singleline("AT+CSCB?", "+CSCB:", &p_response);

    if (err < 0 || p_response->success == 0)
        goto error;

    line = p_response->p_intermediates->line;

    for (p = line; *p != '\0'; p++){
        if( *p == '"' ){
            countQuotes++;
        }
    }
    /* Count number of commas both in the MIDS and DCSS parts of the response */
    for (p = line; (*p != '\0'); p++){
        if( *p == '"' ){
            p++;
            for (p; *p != '\0'; p++) {
                if (*p == ','){
                    if( i == 0 ){
                        commasMids++;
                    }else if( i == 1 ){
                        commasDcss++;
                    }
                }else if (*p == '"'){
                    i++;
                    break;
                }
            }
        }
    }

    /* Now we know the number of entries */
    entriesMids = commasMids + 1;
    if( countQuotes == 4 ){
        entriesDcss = commasDcss + 1;
        }

    entriesRequired= (entriesMids > entriesDcss) ? entriesMids: entriesDcss;

    /* Allocate array of RIL_GSM_BroadcastSmsConfigInfo, one for each entry. */
    configInfoArray = (RIL_GSM_BroadcastSmsConfigInfo **) alloca(entriesRequired * sizeof(RIL_GSM_BroadcastSmsConfigInfo *));
    configInfo = (RIL_GSM_BroadcastSmsConfigInfo *) alloca(entriesRequired * sizeof(RIL_GSM_BroadcastSmsConfigInfo));
    memset(configInfo, 0, entriesRequired * sizeof(RIL_GSM_BroadcastSmsConfigInfo));

    /* Init the pointer array. */
    for (i = 0; i < entriesRequired; i++) {
        configInfoArray[i] = &(configInfo[i]);
    }

    /* Start parsing */
    err = at_tok_start(&line);
    if (err < 0)
        goto error;

    /* Parse the response string. It should follow the coding scheme: +CSCB: 0,"1,2,3-4,6-7,9-10","0" */

    /* Extract the mode -this is the same for the whole array. */
    err = at_tok_nextint(&line, &mode);
    if (err < 0)
        goto error;

    for( i=0; i<entriesRequired; i++ ){
        configInfoArray[i]->selected = (mode==0)?1:0;
    }

    /* Extract the CBM Message Identifiers */
    err = at_tok_nextstr(&line, &p);
    if (err < 0)
        goto error;

    for( i=0; (i<entriesMids) && (strlen(p) > 0); i++ ){
        q = strsep(&p, ",");

        if( q==NULL){
            /* Only one entry */
            err = at_tok_nextint(&p, &(configInfoArray[i]->fromServiceId));
            if (err < 0) goto error;
            configInfoArray[i]->toServiceId = configInfoArray[i]->fromServiceId;
        }else{
            /* Multiple values set */
            r = strsep(&q, "-");
            if( q==NULL){
                /* Single entry, i.e. no range set */
                err = at_tok_nextint(&r, &(configInfoArray[i]->fromServiceId));
                if (err < 0) goto error;
                configInfoArray[i]->toServiceId = configInfoArray[i]->fromServiceId;
            }else{
                /* Value covers a range */
                err = at_tok_nextint(&r, &(configInfoArray[i]->fromServiceId));
                if (err < 0) goto error;
                err = at_tok_nextint(&q, &(configInfoArray[i]->toServiceId));
                if (err < 0) goto error;
            }
        }
    }

    /* Extract the CBM Data Coding Schemes (if any, at the time of writing this is not supported by the modem)    */
    if( entriesDcss > 0 ){
        err = at_tok_nextstr(&line, &p);
        if (err < 0)
            goto error;

        for( i=0; (i<entriesDcss) && (strlen(p) > 0); i++ ){
            q = strsep(&p, ",");

            if( q==NULL){
                /* Only one entry */
                err = at_tok_nextint(&p, &(configInfoArray[i]->fromCodeScheme));
                if (err < 0) goto error;
                configInfoArray[i]->toCodeScheme = configInfoArray[i]->fromCodeScheme;
            }else{
                /* Multiple values set */
                r = strsep(&q, "-");
                if( q==NULL){
                    /* Single entry, i.e. no range set */
                    err = at_tok_nextint(&r, &(configInfoArray[i]->fromCodeScheme));
                    if (err < 0) goto error;
                    configInfoArray[i]->toCodeScheme = configInfoArray[i]->fromCodeScheme;
                }else{
                    /* Value covers a range */
                    err = at_tok_nextint(&r, &(configInfoArray[i]->fromCodeScheme));
                    if (err < 0) goto error;
                    err = at_tok_nextint(&q, &(configInfoArray[i]->toCodeScheme));
                    if (err < 0) goto error;
                }
            }
        }
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, configInfoArray,
                     entriesRequired *sizeof(RIL_GSM_BroadcastSmsConfigInfo *));
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    return;
}


/**
 * RIL_REQUEST_GSM_BROADCAST_SET_SMS_CONFIG
 */
void requestGSMSetBroadcastSMSConfig(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    RIL_GSM_BroadcastSmsConfigInfo** configInfoArray = (RIL_GSM_BroadcastSmsConfigInfo**)data;
    int err;
    int num = 0;
    int i = 0;
    char *cmd = NULL;

    if (datalen % sizeof(RIL_GSM_BroadcastSmsConfigInfo *) != 0) {
        ALOGE("requestGSMSetBroadcastSMSConfig: invalid response length %d expected multiple of %d",
                (int)datalen, (int)sizeof(RIL_GSM_BroadcastSmsConfigInfo *));
        goto error;
    }

    /* Amount of SMS config sets in this request */
    num = datalen / sizeof(RIL_GSM_BroadcastSmsConfigInfo *);

    for( i=0; i<num; i++ ){
        /* Assemble the AT command parameters as specified in 3GPP 23.041 CBM Message Identifier and */
        /* 3GPP TS 23.038 CBM Data Coding Scheme - format for both serviceId and codeScheme will be  */
        /* a combination of integer ranges, e.g. "0,1,5-7" or "0-3,5"                                */
        RIL_GSM_BroadcastSmsConfigInfo* configInfo = configInfoArray[i];

        if( configInfo != NULL){
            ALOGD("requestGSMSetBroadcastSMSConfig param %d - %d, %d, %d, %d, %d", i,
                                        configInfo->fromServiceId, configInfo->toServiceId,
                                        configInfo->fromCodeScheme, configInfo->toCodeScheme, configInfo->selected);
        }else{
            ALOGE("requestGSMSetBroadcastSMSConfig param %d - NULL pointer!!!", i);
            goto error;
        }

        /*
         * Modem has some limitations: No CS over 15 are supported
         * So bound the range at 15.
         */
        if(configInfo->fromCodeScheme > 15) {
            configInfo->fromCodeScheme = 15;
        }

        if(configInfo->toCodeScheme > 15) {
            configInfo->toCodeScheme = 15;
        }

        /* Careful, mode is inverted logic between ril.h and at command spec ! */
        asprintf(&cmd, "AT+CSCB=%d,\"%d-%d\",\"%d-%d\"", (configInfo->selected==0)?1:0, configInfo->fromServiceId, configInfo->toServiceId, configInfo->fromCodeScheme, configInfo->toCodeScheme);

        err = at_send_command(cmd, &p_response);
        free(cmd);

        if (err < 0 || p_response->success == 0)
            goto error;

        at_response_free(p_response);
        p_response = NULL;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

    return;
error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/**
 * RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION
 */
void requestGSMSMSBroadcastActivation(void *data, size_t datalen, RIL_Token t)
{
    char* cmd;
    ATResponse *p_response = NULL;
    int err;
    int mode = ((int *) data)[0];

    /* No distinction between 2G and 3G so toggle both at the same time */
    asprintf(&cmd, "AT*TCBMS=%d,%d", mode, mode);
    err = at_send_command(cmd, &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0)
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
      else
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

    at_response_free(p_response);
}


/**
 * RIL_REQUEST_REPORT_SMS_MEMORY_STATUS
 *
 * Indicates whether there is storage available for new SMS messages.
 *
 * "data" is int *
 * ((int *)data)[0] is 1 if memory is available for storing new messages
 *                  is 0 if memory capacity is exceeded
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */
void requestReportSmsPhoneMemoryStatus(void *data, size_t datalen, RIL_Token t)
{
    int err = 0;
    char *cmd = NULL;
    int smsfull = ((int *) data)[0];
    ATResponse *p_response = NULL;

    assert(smsfull == 0 || smsfull == 1);

    asprintf(&cmd, "AT%%ISMSFULL=%d", !smsfull);
    err = at_send_command(cmd, &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    at_response_free(p_response);
    ALOGE("ERROR: requestReportSmsPhoneMemoryStatus failed\n");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}


/**
 * RIL_REQUEST_GET_SMSC_ADDRESS
 *
 * Queries the default Short Message Service Center address on the device.
 *
 * "data" is NULL
 *
 * "response" is const char * containing the SMSC address.
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */
void requestGetSmsCentreAddress(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    char *sca = NULL;
    char *hex;
    char *line = NULL;
    int err = 0;
    int len=0;

    err = at_send_command_singleline("AT+CSCA?", "+CSCA:", &p_response);

    if (err < 0 || p_response->success == 0 || p_response->p_intermediates->line == NULL) goto error;

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);

    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &(hex));

    if (err < 0) goto error;

    if (at_is_channel_UCS2()) {
        int ucs2_len = strlen(hex);
        int utf8_len = UCS2HexToUTF8_sizeEstimate(ucs2_len);
        sca = calloc(utf8_len, sizeof(char));
        utf8_len = UCS2HexToUTF8(hex, sca, ucs2_len, utf8_len);
        if (utf8_len < 0)
            goto error;
    } else {
        /* encode Hex string to Char string */
        len = strlen(hex) / 2;
        sca = (char *)malloc(len + 1);

        hexToAscii(hex,sca, len);
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, sca, sizeof(sca));
    at_response_free(p_response);
    free(sca);
    return;

error:
    at_response_free(p_response);
    free(sca);
    ALOGE("ERROR: requestGetSmsCentreAddress failed\n");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}


/**
 * RIL_REQUEST_SET_SMSC_ADDRESS
 *
 * Sets the default Short Message Service Center address on the device.
 *
 * "data" is const char * containing the SMSC address.
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */
void requestSetSmsCentreAddress(void *data, size_t datalen, RIL_Token t)
{
    char *cmd = NULL;
    int err = 0;
    ATResponse *p_response = NULL;
    char *sca = (char *)data;
    int len = strlen(sca);
    int ucs2_len;
    char *scahex = NULL;

    if (at_is_channel_UCS2()) {
        len = strlen(sca);
        ucs2_len = UTF8ToUCS2Hex_sizeEstimate(len);
        scahex = (char *)calloc(ucs2_len, sizeof(char));
        ucs2_len = UTF8ToUCS2Hex(sca, scahex, len, ucs2_len);
        if (ucs2_len<0)
            goto error;
    } else {
        scahex = (char *)malloc(len * 2 + 1);
        asciiToHex(sca,scahex, len);
    }

    asprintf(&cmd, "AT+CSCA=\"%s\"", scahex);
    err = at_send_command(cmd, &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    free(scahex);
    return;

error:
    at_response_free(p_response);
    free(scahex);
    ALOGE("ERROR: requestSetSmsCentreAddress failed\n");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void SmsAckStartTimeOut(void)
{
    /*
     * Start 18 seconds timeout proceeding to make sure ack
     * is received.
     */
    const struct timeval delay = {18,0};
    /* Prevent LP0 to make sure timer are running reliably
     * (and Ack can be process ASAP)
     */
    IceraWakeLockAcquire();
    /*
     * Can't cancel timed callback so need to keep track
     * we can have only one Ack pending at a time, but if Ack
     * is received, it may collide with the timed callback from
     * one of the previous request
     */
    WaitingForAck++;
    RIL_requestTimedCallback(SmsAckElapsedTimeOut,(void*)WaitingForAck, &delay);
}

void SmsAckCancelTimeOut(int Parameter)
{
    /*
     * Obsolete the current request
     * Once we're sure the request is current
     */
    if(WaitingForAck == Parameter)
    {
        WaitingForAck++;
        IceraWakeLockRelease();
    }
}

void SmsAckElapsedTimeOut(void * Parameter)
{
    /*
     * If ACK wasn't received
     * Queue an internal request to re-enable notifications
     */
    int CurrentRequest = (int) Parameter;
    if(WaitingForAck == CurrentRequest)
    {
        void *req = (void *)ril_request_object_create(IRIL_REQUEST_ID_SMS_ACK_TIMEOUT, &CurrentRequest, sizeof(CurrentRequest), NULL);

        if(req != NULL)
        {
            requestPutObject(req);
        }
    }
}
/*
 * EVENT: On SMS ack timeout
 */
void SmsReenableSupport(void)
{
    at_send_command("AT+CNMI=1,2,2,1,0", NULL);
}
