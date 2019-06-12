/* Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved. */
/*
** Copyright 2013, Icera Inc.
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
#include "atchannel.h"
#include "at_tok.h"
#include <icera-ril.h>
#include <icera-ril-sim.h>
#include <icera-ril-utils.h>
#include <icera-ril-stk.h>
#include <icera-ril-call.h>
#include <icera-ril-extensions.h>
#include <ril_request_object.h>

#define LOG_TAG rilLogTag
#include <utils/Log.h>


/* SIM-related TLV tags */

#define ICERA_RIL_STK_COMMAND_TAG_MASK               0x7F
#define ICERA_RIL_STK_PROACTIVE_COMMAND_TAG          0xD0
#define ICERA_RIL_STK_COMMAND_DETAILS_TAG            0x81
#define ICERA_RIL_STK_DEVICE_IDENTITIES_TAG          0x82
#define ICERA_RIL_ALPHA_IDENTIFIER_TAG               0x85
#define ICERA_RIL_STK_ITEM_TAG                       0x8F
#define ICERA_RIL_STK_ITEM_ID_TAG                    0x90
#define ICERA_RIL_STK_HELP_REQUEST_TAG               0x95
#define ICERA_RIL_STK_RESULT_TAG                     0x83
#define ICERA_RIL_STK_MENU_SELECTION_TAG             0xD3
#define ICERA_RIL_STK_TEXT_STRING_TAG                0x8D
#define ICERA_RIL_STK_ICON_IDENTIFIER_TAG            0x9E
#define ICERA_RIL_STK_IMMEDIATE_RESPONSE_TAG         0xAB
#define ICERA_RIL_STK_DURATION_TAG                   0x84
#define ICERA_RIL_STK_TEXT_ATTRIBUT_TAG              0xC8
#define ICERA_RIL_STK_RESPONSE_LENGTH_TAG            0x91
#define ICERA_RIL_STK_DEFAULT_TEXT_TAG               0x97
#define ICERA_RIL_STK_TONE_TAG                       0x8E
#define ICERA_RIL_STK_BROWSER_IDENTITY_TAG           0xB0
#define ICERA_RIL_STK_URL_TAG                        0xB1
#define ICERA_RIL_STK_EVENT_LIST_TAG                 0x99
#define ICERA_RIL_STK_FILE_LIST_TAG                  0x92
#define ICERA_RIL_STK_TIMER_IDENTIFIER_TAG           0xA4
#define ICERA_RIL_STK_TIMER_VALUE_TAG                0xA5
#define ICERA_RIL_STK_LANGUAGE_TAG                   0xAD
#define ICERA_RIL_STK_IMEI_TAG                       0x94
#define ICERA_RIL_STK_ESN_TAG                        0xC6
#define ICERA_RIL_STK_DATE_TIME_AND_TIME_ZONE_TAG    0xA6
#define ICERA_RIL_STK_LOCATION_STATUS_TAG            0x9B
#define ICERA_RIL_STK_LOCATION_INFORMATION_TAG       0x93
#define ICERA_RIL_STK_ACCESS_TECHNOLOGY_TAG          0xBF
#define ICERA_RIL_STK_BROWSER_TERMINATION_CAUSE_TAG  0xB4
#define ICERA_RIL_STK_NETWORK_MEASURMENT_RESULTS_TAG 0x96
#define ICERA_RIL_STK_TRANSACTION_ID_TAG             0x9C
#define ICERA_RIL_STK_ADDRESS_TAG                    0x86
#define ICERA_RIL_STK_SUBADDRESS_TAG                 0x88

#define ICERA_RIL_STK_NEXT_ACTION_INDICATOR_TAG      0x18

#define ICERA_RIL_STK_ALPHA_IDENTITY_TAG             0x85
#define ICERA_RIL_STK_SMS_TPDU_TAG                   0x8B
#define ICERA_RIL_STK_USSD_STRING                    0x8A
#define ICERA_RIL_STK_SS_STRING_TAG                  0x89

/**/
#define ICERA_RIL_STK_TIMER_EXPIRATION_TAG           0xD7
#define ICERA_RIL_STK_EVENT_DOWNLOAD_TAG             0xD6

/**/
#define ICERA_RIL_STK_EVENT_MT_CALL                      0x00
#define ICERA_RIL_STK_EVENT_CALL_CONNECTED               0x01
#define ICERA_RIL_STK_EVENT_CALL_DISCONNECTED            0x02
#define ICERA_RIL_STK_EVENT_LOCATION_STATUS              0x03
#define ICERA_RIL_STK_EVENT_USER_ACTIVITY                0x04
#define ICERA_RIL_STK_EVENT_IDLE_SCREEN_AVAILABLE        0x05
#define ICERA_RIL_STK_EVENT_CARD_READER_STATUS           0x06
#define ICERA_RIL_STK_EVENT_LANGUAGE_SELECTION           0x07
#define ICERA_RIL_STK_EVENT_BROWSER_TERMINATION          0x08
#define ICERA_RIL_STK_EVENT_DATA_AVAILABLE               0x09
#define ICERA_RIL_STK_EVENT_CHANNEL_STATUS               0x0A
#define ICERA_RIL_STK_EVENT_ACCESS_TECHNOLOGY_CHANGE     0x0B
#define ICERA_RIL_STK_EVENT_DISPLAY_PARAMETETERS_CHANGED 0x0C
#define ICERA_RIL_STK_EVENT_LOCAL_CONNECTION             0x0D

/**/
#define ICERA_RIL_STK_LOCATION_STATUS_NORMAL_SERVICE     0x00
#define ICERA_RIL_STK_LOCATION_STATUS_LIMITED_SERVICE    0x01
#define ICERA_RIL_STK_LOCATION_STATUS_NO_SERVICE         0x02
/**/
#define ICERA_RIL_STK_COMMAND_SETUP_MENU             0x25
#define ICERA_RIL_STK_COMMAND_SELECT_ITEM            0x24
#define ICERA_RIL_STK_COMMAND_DISPLAY_TEXT           0x21
#define ICERA_RIL_STK_COMMAND_GET_INPUT              0x23
#define ICERA_RIL_STK_COMMAND_GET_INKEY              0x22
#define ICERA_RIL_STK_COMMAND_PLAY_TONE              0x20
#define ICERA_RIL_STK_COMMAND_LAUNCH_BROWSER         0x15
#define ICERA_RIL_STK_COMMAND_SET_UP_IDLE_MODE_TEXT  0x28
#define ICERA_RIL_STK_COMMAND_LANGUAGE_SELECTION     0x35
#define ICERA_RIL_STK_COMMAND_REFRESH                0x01
#define ICERA_RIL_STK_COMMAND_MORE_TIME              0x02
#define ICERA_RIL_STK_COMMAND_SET_UP_EVENT_LIST      0x05
#define ICERA_RIL_STK_COMMAND_TIMER_MANAGEMENT       0x27
#define ICERA_RIL_STK_COMMAND_PROVIDE_LOCAL_INFORMATION  0x26
#define ICERA_RIL_STK_COMMAND_SEND_SHORT_MESSAG      0x13
#define ICERA_RIL_STK_COMMAND_SEND_USSD              0x12
#define ICERA_RIL_STK_COMMAND_SEND_SS                0x11
#define ICERA_RIL_STK_COMMAND_SETUP_CALL             0x10
#define ICERA_RIL_STK_COMMAND_SEND_DTMF              0x14
#define ICERA_RIL_STK_COMMAND_OPEN_CHANNEL           0x40

/**/
#define ICERA_RIL_STK_RESPONSE_SUCCESS                 0x00
#define ICERA_RIL_STK_RESPONSE_PART_COMPREHENSION      0x01
#define ICERA_RIL_STK_RESPONSE_MISSING_INFORMATION     0x02
#define ICERA_RIL_STK_RESPONSE_REFRESH_EES_READ        0x03
#define ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON         0x04
#define ICERA_RIL_STK_RESPONSE_MODIFIED_BY_NAA         0x05
#define ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED         0x06
#define ICERA_RIL_STK_RESPONSE_MODIFICATION            0x07
#define ICERA_RIL_STK_RESPONSE_REFRESH_NAA             0x08
#define ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE         0x09
#define ICERA_RIL_STK_RESPONSE_UICC_TERMINATED         0x10
#define ICERA_RIL_STK_RESPONSE_BACKWARD_UICC           0x11
#define ICERA_RIL_STK_RESPONSE_NO_RESPONSE             0x12
#define ICERA_RIL_STK_RESPONSE_HELP                    0x13
#define ICERA_RIL_STK_RESPONSE_RESERVED_GSM_00         0x14
#define ICERA_RIL_STK_RESPONSE_TERMINAL_COMMAND        0x20
#define ICERA_RIL_STK_RESPONSE_NETWORK_UNABLE          0x21
#define ICERA_RIL_STK_RESPONSE_NOT_ACCEPT              0x22
#define ICERA_RIL_STK_RESPONSE_CLEARED_DOWN            0x23
#define ICERA_RIL_STK_RESPONSE_CONTRADICTION           0x24
#define ICERA_RIL_STK_RESPONSE_PROBLEM_BY_NAA          0x25
#define ICERA_RIL_STK_RESPONSE_BROWSER_ERROR           0x26
#define ICERA_RIL_STK_RESPONSE_MMS_PROBLEM             0x27
#define ICERA_RIL_STK_RESPONSE_NO_CAPABILITIES         0x30
#define ICERA_RIL_STK_RESPONSE_TYPE_INCORRECT          0x31
#define ICERA_RIL_STK_RESPONSE_DATA_INCORRECT          0x32
#define ICERA_RIL_STK_RESPONSE_NUMBER_INCORRECT        0x33
#define ICERA_RIL_STK_RESPONSE_RESERVED_GSM_01         0x34
#define ICERA_RIL_STK_RESPONSE_RESERVED_GSM_02         0x35
#define ICERA_RIL_STK_RESPONSE_ERROR_VALUES_MISS       0x36
#define ICERA_RIL_STK_RESPONSE_RESERVED_GSM_03         0x37
#define ICERA_RIL_STK_RESPONSE_ERROR_MULTIPLECARD      0x38
#define ICERA_RIL_STK_RESPONSE_PROBLEM_CONTROL_BY_NAA  0x39
#define ICERA_RIL_STK_RESPONSE_ERROR_PROTOCOL          0x3A
#define ICERA_RIL_STK_RESPONSE_ACCESS_UNABLE           0x3B
#define ICERA_RIL_STK_RESPONSE_FRAMES_ERROR            0x3C
#define ICERA_RIL_STK_RESPONSE_MMS_ERROR               0x3D

/**/
#define ICERA_RIL_STK_PROVIDE_LOCAL_INFORMATION_LOCATION           0x00
#define ICERA_RIL_STK_PROVIDE_LOCAL_INFORMATION_IMEI               0x01
#define ICERA_RIL_STK_PROVIDE_LOCAL_INFORMATION_NETWORK_MEASURMENT 0x02
#define ICERA_RIL_STK_PROVIDE_LOCAL_INFORMATION_DATE_TIME          0x03
#define ICERA_RIL_STK_PROVIDE_LOCAL_INFORMATION_LANGUAGE           0x04
#define ICERA_RIL_STK_PROVIDE_LOCAL_INFORMATION_ACCESS_TECHNOLOGY  0x06
#define ICERA_RIL_STK_PROVIDE_LOCAL_INFORMATION_ESN                0x07
/**/
#define ICERA_RIL_STK_DEV_ID_KEYPAD                  0x01
#define ICERA_RIL_STK_DEV_ID_DISPLAY                 0x02
#define ICERA_RIL_STK_DEV_ID_EARPIECE                0x03
#define ICERA_RIL_STK_DEV_ID_UICC                    0x81
#define ICERA_RIL_STK_DEV_ID_TERMINAL                0x82
#define ICERA_RIL_STK_DEV_ID_NETWORK                 0x83

#define ICERA_RIL_STK_PRO_CMD_SIZE_MAX               2048

/* Call Control Info */
#define ICERA_RIL_STKCTRLIND_RESULT_ALLOWED                     0x00
#define ICERA_RIL_STKCTRLIND_RESULT_NOT_ALLOWED                 0x01
#define ICERA_RIL_STKCTRLIND_RESULT_ALLOWED_MODIFICATION        0x02
#define ICERA_RIL_STKCTRLIND_RESULT_TOOLKIT_BUSY                0x03

#define ICERA_RIL_STKCTRLIND_TYPE_MO_CALL_CONTROL               0x00
#define ICERA_RIL_STKCTRLIND_TYPE_SS_CALL_CONTROL               0x01
#define ICERA_RIL_STKCTRLIND_TYPE_USSD_CALL_CONTROL             0x02
#define ICERA_RIL_STKCTRLIND_TYPE_MO_SMS_CONTROL                0x03

/*
 * buffer to create TLV PDU for Android STK
 */
static char stk_pro_cmd_buf[ICERA_RIL_STK_PRO_CMD_SIZE_MAX];

/*
 * current buffer pointer
 */
static char *stk_pro_cmd_ptr = stk_pro_cmd_buf;

/*
 * STK command number
 */
static char stk_command_number = 0;

/*
 * Type of STK AT interface to be used
 */
int stk_use_cusat_api=0;

/*
 * Android STK service status
 */
static int s_stkServiceRunning = 0;
static int s_pendingLen=0;
static char* s_pendingProactive = NULL;

/*
 * Phonebook load state
 */
static int PbkLoaded = 0;

/*
 * Finds tag in TLV encoded PDU
 */
static char * tlvFindTag(char *data, size_t datalen, char tag)
{
    char *found = NULL;
    size_t tlvlen;

    while (!found && (datalen > 0)) {
        if ((*data & ICERA_RIL_STK_COMMAND_TAG_MASK) == (tag & ICERA_RIL_STK_COMMAND_TAG_MASK)) {
            found = data;
        } else {
            data++; datalen--;
            tlvlen = (size_t)*data;
            data++; datalen--;
            if (datalen >= tlvlen) {
                data += tlvlen;
                datalen -= tlvlen;
            } else {
                break;
            }
        }
    }

    if (found) {
        /* check if length is valid */
        if (datalen < 2) {
            /* length should be at least 2 - tag and length */
            found = NULL;
        } else {
            /* check if TLV length fits into remaining data length */
            data = found + 1; datalen--;
            tlvlen = (size_t)*data;
            data++; datalen--;
            if (datalen < tlvlen) {
                found = NULL;
            }
        }
    }

    return found;
}

/*
 * Packs a length to TLV format
 */
static char * packLen(char *buf, size_t len)
{
    if (len < 0x80) {
        *buf++ = len;
    }
    else if (len < 0x100) {
        *buf++ = 0x81;
        *buf++ = len;
    }
    else if (len < 0x10000) {
        *buf++ = 0x82;
        *buf++ = len & 0xFF;
        *buf++ = len >> 8;
    }
    else {
        *buf++ = 0x83;
        *buf++ = len & 0xFF;
        *buf++ = (len >> 8) & 0xFF;
        *buf++ = len >> 16;
    }

    return buf;
}

static size_t parseLen(char *buf, char *lenLength)
{
    size_t ret = -1;

    if (lenLength == NULL || buf == NULL)
        return ret;

    if (buf[0] < 0x80) {
        ret = buf[0];
        *lenLength = 1;
    }
    else if (buf[0] == 0x81) {
        ret = buf[1];
        *lenLength = 2;
    }
    else if (buf[0] == 0x82) {
        ret = buf[2] << 8;
        ret |= buf[1] & 0xff;
        *lenLength = 3;
    }
    else if (buf[0] == 0x83) {
        ret = buf[3] << 16;
        ret |= buf[2] << 8;
        ret |= buf[1] & 0xff;
        *lenLength = 4;
    }
    else {
        *lenLength = 0;
    }

    return ret;
}

static int StkGetProfile(char **Data,int *Size)
{
    int err = 0;
    int len = 0;
    ATResponse *p_response = NULL;
    char *response = NULL;
    char *line = NULL;

    err = at_send_command_singleline("AT*TSTPROF?", "*TSTPROF:", &p_response);

    if (err != 0 || p_response->success == 0) goto error;

    line = p_response->p_intermediates->line;

    /* *TSTPROF: 29,"EEFE97FF7F1D00FFBF00001FE2800000836B0000000140000000000000" */

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &len);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &response);

    if (err < 0) goto error;
    asprintf(Data,"%s",response);
    *Size = len;

    at_response_free(p_response);
    return err;

error:
    at_response_free(p_response);
    if (err == 0)
    {
        /* Case of AT sent without error but response not OK*/
        err = 1;
    }

    return err;
}

static int StkSetProfile(char *Data, int Size)
{
    int err = 0;
    ATResponse *p_response = NULL;
    char *profile = Data;
    int length = Size;
    char *cmd = NULL;

    asprintf(&cmd, "AT*TSTPD=%d,%s", length, profile);
    err = at_send_command(cmd, &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) goto error;

    at_response_free(p_response);
    return err;

error:
    at_response_free(p_response);
    if (err == 0)
    {
        /* Case of AT sent without error but response not OK*/
        err = 1;
    }
    return err;
}

int StkNotifyServiceIsRunning(int from_request)
{
    static int allow_processing_from_sim_ready_event = 0;
    int err = 0;
    char *Data = NULL;
    int Size;

	if(!from_request && !allow_processing_from_sim_ready_event)
	    goto error;

    /* This request is just there to tell us that the STK app is ready to
     * receive our notification. Give the go ahead to the modem by triggering the
     * profile download (with the default profile)
     */
    err = StkGetProfile(&Data,&Size);
    if(err!=0)
	{
        /* This is certainly because SIM is still not ready so allow to be processed when SIM will be ready*/
	    allow_processing_from_sim_ready_event = 1;
        goto error;
    }

    err = StkSetProfile(Data,Size);
    if(err!=0)
        goto error;
	/* Processing is successful so do not treat it after */
	allow_processing_from_sim_ready_event=0;
error:
    if(Data)
       free(Data);
    return err;
}

void StkInitDefaultProfiles()
{
    ATResponse *p_response = NULL;
    char *line = NULL;
    char *cmd = NULL;
    char *te_profile = NULL;
    char *te_profile_hex = NULL;
    char *mt_profile = NULL;
    char *mt_profile_hex=NULL;
    char *mt_added_bits_hex=NULL;
    char *conf_profile = NULL;
    int discard, i;
    int err = 0;

    int prof_len = 0;

    /* Reset TE Profile*/
    at_send_command("AT+CUSATW=0,\"\"", NULL);

    /*
     * TE PROFILE
     * use hardcoded profile
     * convert it in hexa for future use.
     */
    te_profile =  RIL_STK_TE_PROFILE;
    te_profile_hex = malloc(strlen(te_profile)+1);
    if(te_profile_hex)
    {
        prof_len =  strlen(te_profile);
        hexToAscii(te_profile, te_profile_hex, prof_len);
    }

    /*
     * CONF PROFILE - Commands waiting for a user confirmation
     * use hardcoded profile
     * convert it in hexa for future use.
     */
    conf_profile =  RIL_STK_CONF_PROFILE;

    /*
     * MT PROFILE
     * - Get default MT profile from modem
     * - remove from it the bits forced in TE profile
     * - add some hardcoded bits that must be present
     * - add commands for which a user confirmation is awaited
     */
    err =  at_send_command_singleline("AT+CUSATR=2", "+CUSATR:", &p_response);
    if(err != 0) goto error;

    if(p_response && p_response->success)
    {
        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);
            if(err != 0) goto error;
        err = at_tok_nextint(&line, &discard);
            if(err != 0) goto error;
        err = at_tok_nextstr(&line, &mt_profile);
            if(err != 0) goto error;

        if((int)strlen(mt_profile) < prof_len) prof_len =  (int)strlen(mt_profile);
        mt_profile_hex = malloc(prof_len+1);
        if(mt_profile_hex)
        {
            hexToAscii(mt_profile, mt_profile_hex, prof_len);
        }
        else
        {
            prof_len = 0;
        }
    }
    at_response_free(p_response);
    mt_profile = NULL;

    /* Get the bits that must be added to MT profile*/
    if((int)strlen(RIL_STK_MT_PROFILE_ADD) < prof_len) prof_len =  (int)strlen(RIL_STK_MT_PROFILE_ADD);
    mt_added_bits_hex = malloc(prof_len/2 + 1);
    if(mt_added_bits_hex)
    {
        hexToAscii(RIL_STK_MT_PROFILE_ADD, mt_added_bits_hex, prof_len/2);
    }

    /* Construct new MT profile and convert it to string*/
    if((prof_len != 0) && (te_profile_hex!=NULL) && (mt_profile_hex!=NULL) && (mt_added_bits_hex!=NULL))
    {
        for(i=0; i<prof_len/2; i++)
        {
             mt_profile_hex[i] &=  ~te_profile_hex[i];
             mt_profile_hex[i] |= mt_added_bits_hex[i];
        }
        mt_profile = malloc(prof_len+1);
        asciiToHex(mt_profile_hex, mt_profile, prof_len/2);
    }

    /*
     *  Write profiles
     */
    if(mt_profile)
    {
        /* MT profile */
        asprintf(&cmd,"AT+CUSATW=1,\"%s\"", mt_profile);
        at_send_command(cmd, NULL);
        free(cmd);

        /* Register for notifications for events handled by MT*/
        asprintf(&cmd,"AT%%IUSATP=1,\"%s\"", mt_profile);
        at_send_command(cmd, NULL);
        free(cmd);

        /* TE profile*/
        asprintf(&cmd,"AT+CUSATW=0,\"%s\"", te_profile);
        at_send_command(cmd, NULL);
        free(cmd);

        /* CONF profile*/
        asprintf(&cmd,"AT%%IUSATC=1,\"%s\"", conf_profile);
        at_send_command(cmd, NULL);
        free(cmd);

        /* Re-use these profiles at next reboot*/
        at_send_command("AT+CUSATD=1,0",NULL);
    }
    else
    {
        /* If an error occured during computation of TE and MT profiles use default*/
        at_send_command("AT+CUSATD=0,0",NULL);
    }

    /* Activate the profiles*/
    at_send_command("AT+CUSATA=1",NULL);

    /* Activate the STK call control info unsolicited message */
    at_send_command("AT%IUSATT=1", NULL);

    /* Activate the STK SIM refresh notifications. */
    at_send_command("AT%IUSATREFRESH=1", NULL);

error:
    /* Reset statics. */
    s_pendingProactive = NULL;
    s_pendingLen = 0;

    free(mt_profile);
    free(te_profile_hex);
    free(mt_profile_hex);
    free(mt_added_bits_hex);
}

/**
 * RIL_REQUEST_STK_GET_PROFILE
 *
 * Requests the profile of SIM tool kit.
 * The profile indicates the SAT/USAT features supported by ME.
 * The SAT/USAT features refer to 3GPP TS 11.14 and 3GPP TS 31.111
 *
 * "data" is NULL
 *
 * "response" is a const char * containing SAT/USAT profile
 * in hexadecimal format string starting with first byte of terminal profile
 *
 * Valid errors:
 *  RIL_E_SUCCESS
 *  RIL_E_RADIO_NOT_AVAILABLE (radio resetting)
 *  RIL_E_GENERIC_FAILURE
 */
void requestStkGetProfile(void *data, size_t datalen, RIL_Token t)
{
    if(stk_use_cusat_api)
    {
        int err = 0;
        char *line = NULL;
        char *profile = NULL;
        int profile_lengh = 0;
        ATResponse *p_response = NULL;
        int discard;

        err = at_send_command_singleline("AT+CUSATR=1", "+CUSATR", &p_response);
        if((err==0) && p_response && p_response->success)
        {
            line = p_response->p_intermediates->line;

            err = at_tok_start(&line);
            if (err!=0) goto error1;

            err = at_tok_nextint(&line, &discard);
            if (err!=0) goto error1;

            err = at_tok_nextstr(&line, &profile);
            if (err!=0) goto error1;

            profile_lengh = strlen(profile);
        }
        RIL_onRequestComplete(t, RIL_E_SUCCESS, profile, profile_lengh);
        at_response_free(p_response);
        return;

error1:
        at_response_free(p_response);
        ALOGE("ERROR: requestStkGetProfile failed\n");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);

    }
    else
    {
        int err = 0;
        char * Data = NULL;
        int Size;
        err = StkGetProfile(&Data,&Size);

        /* Size is reported in bytes - multiply by 2 to get the nb of hex chars.*/
        Size = Size*2;
        if(err!=0)
            goto error;

        RIL_onRequestComplete(t, RIL_E_SUCCESS, Data, Size);
        free(Data);
        return;

error:
        if(Data)
            free(Data);
        ALOGE("ERROR: requestStkGetProfile failed\n");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}


/**
 * RIL_REQUEST_STK_SET_PROFILE
 *
 * Download the STK terminal profile as part of SIM initialization
 * procedure
 *
 * "data" is a const char * containing SAT/USAT profile
 * in hexadecimal format string starting with first byte of terminal profile
 *
 * "response" is NULL
 *
 * Valid errors:
 *  RIL_E_SUCCESS
 *  RIL_E_RADIO_NOT_AVAILABLE (radio resetting)
 *  RIL_E_GENERIC_FAILURE
 */
void requestSTKSetProfile(void * data, size_t datalen, RIL_Token t)
{
    if(stk_use_cusat_api)
    {
        int err = 0;
        char *cmd;
        ATResponse *p_response = NULL;

        asprintf(&cmd, "AT+CUSATW=0,\"%s\"", (char *)data);

        if (!cmd) goto error1;

        err = at_send_command(cmd, &p_response);
        free(cmd);

        if (err < 0 || p_response->success == 0) goto error1;

        at_response_free(p_response);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        return;

error1:
        at_response_free(p_response);
        ALOGE("ERROR: requestSTKSetProfile() failed\n");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    else
    {
        int err = 0;
        int len = strlen(data)/2; /* byte length.*/

        err = StkSetProfile(data, len);
        if(err != 0)
            goto error;

        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        return;

error:
        ALOGE("ERROR: requestSTKSetProfile() failed\n");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}


/**
 * RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND
 *
 * Requests to send a SAT/USAT envelope command to SIM.
 * The SAT/USAT envelope command refers to 3GPP TS 11.14 and 3GPP TS 31.111
 *
 * "data" is a const char * containing SAT/USAT command
 * in hexadecimal format string starting with command tag
 *
 * "response" is a const char * containing SAT/USAT response
 * in hexadecimal format string starting with first byte of response
 * (May be NULL)
 *
 * Valid errors:
 *  RIL_E_SUCCESS
 *  RIL_E_RADIO_NOT_AVAILABLE (radio resetting)
 *  RIL_E_GENERIC_FAILURE
 */
void requestSTKSendEnvelopeCommand(void * data, size_t datalen, RIL_Token t)
{
    if(stk_use_cusat_api)
    {
        int err = 0;
        char *cmd = NULL;
        ATResponse *p_response = NULL;

        asprintf(&cmd, "AT+CUSATE=\"%s\"", (char *)data);

        if (!cmd) goto error1;

        err = at_send_command_singleline(cmd, "+CUSATE:", &p_response);
        free(cmd);

        if (err < 0 || p_response->success == 0) goto error1;

        at_response_free(p_response);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        return;

error1:
        at_response_free(p_response);
        ALOGE("ERROR: requestSTKSendEnvelopeCommand() failed\n");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    else
    {
        int err = 0;
        int len = 0;
        int menu_id, help_requested;
        int timerId, hours, mins, secs, timer;
        char *envdata, *tlvEnvData, *tlvItemId, *tlvHelpRequest, *tlvTimerId, *tlvTimerValue;
        char *tlvEventList, *tlvEventData;
        char *cmd = NULL;
        ATResponse *p_response = NULL;

        if (datalen != sizeof(char *)) goto error;

        /* convert data from hex */
        len = strlen(data);
        if (len % 2 != 0) goto error;

        len = len / 2;
        envdata = (char *)alloca(len);
        if (!envdata) goto error;

        hexToAscii((char *)data, envdata, len);
        if ((tlvEnvData = tlvFindTag(envdata, len, ICERA_RIL_STK_MENU_SELECTION_TAG)) != NULL) {
            tlvItemId = tlvFindTag(tlvEnvData + 2, tlvEnvData[1], ICERA_RIL_STK_ITEM_ID_TAG);
            if (!tlvItemId) {
                ALOGE("TSTMS: Missing item ID tag");
                goto error;
            }

            if (tlvItemId[1] < 1) {
                ALOGE("TSTMS: Wrong item ID tag length");
                goto error;
            }

            menu_id = tlvItemId[2];
            help_requested = 0;
            tlvHelpRequest = tlvFindTag(tlvEnvData + 2, tlvEnvData[1], ICERA_RIL_STK_HELP_REQUEST_TAG);

            if (tlvHelpRequest) {
                help_requested = 1;
            }

            asprintf(&cmd, "AT*TSTMS=%d,%d", menu_id, help_requested);
        } else if ((tlvEnvData = tlvFindTag(envdata, len, ICERA_RIL_STK_TIMER_EXPIRATION_TAG)) != NULL) {
            timerId = 0;
            hours = 0; mins = 0; secs = 0; timer = 0;

            tlvTimerId = tlvFindTag(tlvEnvData + 2, tlvEnvData[1], ICERA_RIL_STK_TIMER_IDENTIFIER_TAG);

            if (tlvTimerId && tlvTimerId[1] > 0) {
                timerId = tlvTimerId[2];
            }

            tlvTimerValue = tlvFindTag(tlvEnvData + 2, tlvEnvData[1], ICERA_RIL_STK_TIMER_VALUE_TAG);

            if (tlvTimerValue && tlvTimerValue[1] >= 3) {
                hours = bcdByteToInt(tlvTimerValue[2]);
                mins  = bcdByteToInt(tlvTimerValue[3]);
                secs  = bcdByteToInt(tlvTimerValue[4]);
                timer = secs + 60 * (mins + 60 * hours);
            }

            if (timer == 0 || timer > 3600) {
                ALOGE("TSTRT: Wrong timer value");
                goto error;
            }

            asprintf(&cmd, "AT*TSTRT=%d", timer);
        } else if ((tlvEnvData = tlvFindTag(envdata, len, ICERA_RIL_STK_EVENT_DOWNLOAD_TAG)) != NULL) {
            if ((tlvEventList = tlvFindTag(tlvEnvData + 2, tlvEnvData[1], ICERA_RIL_STK_EVENT_LIST_TAG)) != NULL &&
                tlvEventList[1] > 0) {

                switch (tlvEventList[2]) {
                    case ICERA_RIL_STK_EVENT_MT_CALL:
                        break;

                    case ICERA_RIL_STK_EVENT_CALL_CONNECTED:
                        break;

                    case ICERA_RIL_STK_EVENT_CALL_DISCONNECTED:
                        break;

                    case ICERA_RIL_STK_EVENT_LOCATION_STATUS:
                        break;

                    case ICERA_RIL_STK_EVENT_USER_ACTIVITY:
                        asprintf(&cmd, "AT*TSTEV=05,\"\"");
                        break;

                    case ICERA_RIL_STK_EVENT_IDLE_SCREEN_AVAILABLE:
                        asprintf(&cmd, "AT*TSTEV=06,\"\"");
                        break;

                    case ICERA_RIL_STK_EVENT_CARD_READER_STATUS:
                        break;

                    case ICERA_RIL_STK_EVENT_LANGUAGE_SELECTION:
                        if ((tlvEventData = tlvFindTag(tlvEnvData + 2,tlvEnvData[1], ICERA_RIL_STK_LANGUAGE_TAG)) != NULL &&
                            tlvEventData[1] > 0) {
                            char *lang = (char*)alloca(tlvEventData[1] + 1);
                            memcpy(lang, &tlvEventData[2], tlvEventData[1]);
                            lang[(int)tlvEventData[1]] = '\0';
                            asprintf(&cmd, "AT*TSTEV=08,\"%s\"", lang);
                        } else {
                            ALOGE("TSTEV: Missing or invalid language");
                            goto error;
                        }
                        break;

                    case ICERA_RIL_STK_EVENT_BROWSER_TERMINATION:
                        if ((tlvEventData = tlvFindTag(tlvEnvData + 2, tlvEnvData[1], ICERA_RIL_STK_BROWSER_TERMINATION_CAUSE_TAG)) != NULL &&
                            tlvEventData[1] > 0) {
                            asprintf(&cmd, "AT*TSTEV=09,\"\"");
                        } else {
                            ALOGE("TSTEV: Missing or invalid browser termination cause");
                            goto error;
                        }
                        break;

                    case ICERA_RIL_STK_EVENT_DATA_AVAILABLE:
                        break;

                    case ICERA_RIL_STK_EVENT_CHANNEL_STATUS:
                        break;

                    case ICERA_RIL_STK_EVENT_ACCESS_TECHNOLOGY_CHANGE:
                        break;

                    case ICERA_RIL_STK_EVENT_DISPLAY_PARAMETETERS_CHANGED:
                        break;

                    case ICERA_RIL_STK_EVENT_LOCAL_CONNECTION:
                        break;

                    default:
                        ALOGE("TSTEV: Unknown event 0x%02x", tlvEventList[2]);
                        goto error;
                        break;
                }
            } else {
                ALOGE("TSTEV: Missing or invalid event list");
                goto error;
            }
        } else {
            ALOGE("TSTEV: Unknown envelope command");
            goto error;
        }

        if (!cmd) goto error;

        err = at_send_command(cmd, &p_response);
        free(cmd);

        if (err < 0 || p_response->success == 0) goto error;

        at_response_free(p_response);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        return;

error:
        at_response_free(p_response);
        ALOGE("ERROR: requestSTKSendEnvelopeCommand() failed\n");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

/**
 * RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS
 *
 * Requests to send a SAT/USAT envelope command to SIM.
 * The SAT/USAT envelope command refers to 3GPP TS 11.14 and 3GPP TS 31.111.
 *
 * This request has one difference from RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:
 * the SW1 and SW2 status bytes from the UICC response are returned along with
 * the response data, using the same structure as RIL_REQUEST_SIM_IO.
 *
 * The RIL implementation shall perform the normal processing of a '91XX'
 * response in SW1/SW2 to retrieve the pending proactive command and send it
 * as an unsolicited response, as RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND does.
 *
 * "data" is a const char * containing the SAT/USAT command
 * in hexadecimal format starting with command tag
 *
 * "response" is a const RIL_SIM_IO_Response *
 *
 * Valid errors:
 *  RIL_E_SUCCESS
 *  RIL_E_RADIO_NOT_AVAILABLE (radio resetting)
 *  RIL_E_GENERIC_FAILURE
 */
void requestSTKSendEnvelopeCommandWithStatus(void * data, size_t datalen, RIL_Token t)
{
    int err = 0, dummy = 0;
    char *cmd = NULL;
    ATResponse *p_response = NULL;
    char *line = NULL;
    RIL_SIM_IO_Response sr;
    ATLine *p_cur;

    if (data == NULL)
        goto error;

    asprintf(&cmd, "AT+CUSATE=\"%s\"", (char *)data);

    if (!cmd) goto error;

    err = at_send_command_multiline(cmd, "+CUSATE", &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) goto error;

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);      // skip prefix +CUSATE:
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &sr.simResponse);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &dummy);
    if (err < 0) goto error;

    if ((p_cur = p_response->p_intermediates->p_next) == NULL) goto error;

    line = p_cur->line;
    err = at_tok_start(&line);      // skip prefix +CUSATE2:
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &sr.sw1);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &sr.sw2);
    if (err < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &sr, sizeof(sr));
    at_response_free(p_response);
    return;

error:
    at_response_free(p_response);
    ALOGE("ERROR: requestSTKSendEnvelopeCommandWithStatus() failed\n");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/**
 * RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE
 *
 * Requests to send a terminal response to SIM for a received
 * proactive command
 *
 * "data" is a const char * containing SAT/USAT response
 * in hexadecimal format string starting with first byte of response data
 *
 * "response" is NULL
 *
 * Valid errors:
 *  RIL_E_SUCCESS
 *  RIL_E_RADIO_NOT_AVAILABLE (radio resetting)
 *  RIL_E_GENERIC_FAILURE
 */
void requestSTKSendTerminalResponse(void * data, size_t datalen, RIL_Token t)
{
    if(stk_use_cusat_api)
    {
        int err = 0;
        char *cmd = NULL;
        ATResponse *p_response = NULL;

        asprintf(&cmd, "AT+CUSATT=\"%s\"", (char *)data);

        if (!cmd) goto error1;

        err = at_send_command(cmd, &p_response);
        free(cmd);

        if (err < 0 || p_response->success == 0) goto error1;

        at_response_free(p_response);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        return;

error1:
        at_response_free(p_response);
        ALOGE("ERROR: requestSTKSendTerminalResponse() failed\n");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    else
    {
        int err = 0;
        int len = 0;
        int lentxt, dcstxt;
        char *hex_string;
        char *trdata, *tlvCommandDetails, *tlvDeviceIdentities, *tlvResult;
        int commandType, commandNumber, commandQualifier, generalResult, additionalResult, itemId;
        char *cmd = NULL;
        ATResponse *p_response = NULL;

        if (datalen != sizeof(char *)) goto error;

        /* convert data from hex */
        len = strlen(data);
        if (len % 2 != 0) goto error;

        len = len / 2;
        trdata = (char *)alloca(len);
        if (!trdata) goto error;

        hexToAscii((char *)data, trdata, len);
        tlvCommandDetails = tlvFindTag(trdata, len, ICERA_RIL_STK_COMMAND_DETAILS_TAG);

        if (!tlvCommandDetails || tlvCommandDetails[1] < 3) {
            ALOGE("TSTCR: Missing command details tag");
            goto error;
        }

        commandNumber = tlvCommandDetails[2];
        commandType = tlvCommandDetails[3];
        commandQualifier = tlvCommandDetails[4];
        tlvDeviceIdentities = tlvFindTag(trdata, len, ICERA_RIL_STK_DEVICE_IDENTITIES_TAG);

        if (!tlvDeviceIdentities || tlvDeviceIdentities[1] != 2) {
            ALOGE("TSTCR: Missing Device Identities tag");
            goto error;
        }

        tlvResult = tlvFindTag(trdata, len, ICERA_RIL_STK_RESULT_TAG);

        if (!tlvResult || tlvResult[1] < 1) {
            ALOGE("TSTCR: Missing result tag");
            goto error;
        }

        generalResult = tlvResult[2];
        additionalResult = 0;

        if (tlvResult[1] > 1) {
            additionalResult = tlvResult[3];
        }

        switch(commandType) {
            case ICERA_RIL_STK_COMMAND_SETUP_MENU:
                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 0; /* Command performed successfully */
                         break;
                    case ICERA_RIL_STK_RESPONSE_HELP:
                         generalResult = 2; /* Help information requested */
                         break;
                    default:
                         generalResult = 3; /* Problem with menu operation */
                         break;
                }
                asprintf(&cmd, "AT*TSTCR=25,%d", generalResult);
                break;

            case ICERA_RIL_STK_COMMAND_SELECT_ITEM:
                itemId = 0;
                tlvResult = tlvFindTag(trdata, len, ICERA_RIL_STK_ITEM_ID_TAG);

                if (tlvResult && tlvResult[1] > 0) {
                    itemId = tlvResult[2];
                }

                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 0; /* Command performed successfully */
                         break;
                    case ICERA_RIL_STK_RESPONSE_UICC_TERMINATED:
                    case ICERA_RIL_STK_RESPONSE_TERMINAL_COMMAND:
                         generalResult = 1; /* Terminate proactive session */
                         break;
                    case ICERA_RIL_STK_RESPONSE_HELP:
                         generalResult = 2; /* Help information requested */
                         break;
                    case ICERA_RIL_STK_RESPONSE_BACKWARD_UICC:
                         generalResult = 3; /* Backward move requested */
                         break;
                    case ICERA_RIL_STK_RESPONSE_NO_RESPONSE:
                    default:
                         generalResult = 4; /* No response from use */
                         break;
                }
                asprintf(&cmd, "AT*TSTCR=24,%d,%d", generalResult, itemId);
                break;

            case ICERA_RIL_STK_COMMAND_DISPLAY_TEXT:
                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 0; /* Command performed successfully */
                         break;
                    case ICERA_RIL_STK_RESPONSE_UICC_TERMINATED:
                    case ICERA_RIL_STK_RESPONSE_TERMINAL_COMMAND:
                         generalResult = 1; /* Terminate proactive session */
                         break;
                    case ICERA_RIL_STK_RESPONSE_CLEARED_DOWN:
                         generalResult = 2; /* User cleared message */
                         break;
                    case ICERA_RIL_STK_RESPONSE_BACKWARD_UICC:
                         generalResult = 4; /* Backward move requested */
                         break;
                    case ICERA_RIL_STK_RESPONSE_NO_RESPONSE:
                         generalResult = 5; /* No response from user */
                         break;
                    default:
                         generalResult = 3; /* Screen is busy */
                         break;
                }
                asprintf(&cmd, "AT*TSTCR=21,%d", generalResult);
                break;

            case ICERA_RIL_STK_COMMAND_GET_INPUT:
                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 0; /* Command performed successfully */
                         break;
                    case ICERA_RIL_STK_RESPONSE_UICC_TERMINATED:
                    case ICERA_RIL_STK_RESPONSE_TERMINAL_COMMAND:
                         generalResult = 1; /* Terminate proactive session */
                         break;
                    case ICERA_RIL_STK_RESPONSE_HELP:
                         generalResult = 2; /* Help information requested */
                         break;
                    case ICERA_RIL_STK_RESPONSE_BACKWARD_UICC:
                         generalResult = 3; /* Backward move requested */
                         break;
                    case ICERA_RIL_STK_RESPONSE_NO_RESPONSE:
                    default:
                         generalResult = 4; /* No response from use */
                         break;
                }

                hex_string = NULL;
                if (generalResult == 0) {
                    tlvResult = tlvFindTag(trdata, len, ICERA_RIL_STK_TEXT_STRING_TAG);
                    if (tlvResult && tlvResult[1] > 0) {
                        lentxt = tlvResult[1] - 1;
                        dcstxt = tlvResult[2];
                        hex_string = (char *)alloca(lentxt * 2 + 1);
                        asciiToHex(&tlvResult[3], hex_string, lentxt);
                    }
                }

                if (hex_string == NULL) {
                    asprintf(&cmd, "AT*TSTCR=23,%d", generalResult);
                }
                else {
                    asprintf(&cmd, "AT*TSTCR=23,%d,%d,%s", generalResult, dcstxt, hex_string);
                }
                break;

            case ICERA_RIL_STK_COMMAND_GET_INKEY:
                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 0; /* Command performed successfully */
                         break;
                    case ICERA_RIL_STK_RESPONSE_UICC_TERMINATED:
                    case ICERA_RIL_STK_RESPONSE_TERMINAL_COMMAND:
                         generalResult = 1; /* Terminate proactive session */
                         break;
                    case ICERA_RIL_STK_RESPONSE_HELP:
                         generalResult = 2; /* Help information requested */
                         break;
                    case ICERA_RIL_STK_RESPONSE_BACKWARD_UICC:
                         generalResult = 3; /* Backward move requested */
                         break;
                    case ICERA_RIL_STK_RESPONSE_NO_RESPONSE:
                    default:
                         generalResult = 4; /* No response from use */
                         break;
                }

                hex_string = NULL;
                if (generalResult == 0) {
                    tlvResult = tlvFindTag(trdata, len, ICERA_RIL_STK_TEXT_STRING_TAG);
                    if (tlvResult && tlvResult[1] > 0) {
                        lentxt = tlvResult[1] - 1;
                        dcstxt = tlvResult[2];
                        hex_string = (char *)alloca(lentxt * 2 + 1);
                        asciiToHex(&tlvResult[3], hex_string, lentxt);
                    }
                }

                if (hex_string == NULL) {
                    asprintf(&cmd, "AT*TSTCR=22,%d", generalResult);
                }
                else {
                    asprintf(&cmd, "AT*TSTCR=22,%d,%d,%s", generalResult, dcstxt, hex_string);
                }
                break;

            case ICERA_RIL_STK_COMMAND_PLAY_TONE:
                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                         generalResult = 0; /* Command performed successfully */
                         break;
                    case ICERA_RIL_STK_RESPONSE_UICC_TERMINATED:
                    case ICERA_RIL_STK_RESPONSE_TERMINAL_COMMAND:
                         generalResult = 1; /* Terminate proactive session */
                         break;
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 2; /* Tone not played */
                         break;
                    default:
                         generalResult = 3; /* Specified tone not supported */
                         break;
                }
                asprintf(&cmd, "AT*TSTCR=20,%d", generalResult);
                break;

            case ICERA_RIL_STK_COMMAND_LAUNCH_BROWSER:
                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 0; /* Command performed successfully */
                         break;
                    case ICERA_RIL_STK_RESPONSE_PART_COMPREHENSION:
                         generalResult = 1; /* Command performed - partial comp */
                         break;
                    case ICERA_RIL_STK_RESPONSE_MISSING_INFORMATION:
                         generalResult = 2; /* Command performed - missing info */
                         break;
                    case ICERA_RIL_STK_RESPONSE_UICC_TERMINATED:
                    case ICERA_RIL_STK_RESPONSE_NOT_ACCEPT:
                         generalResult = 3; /* User rejected launch */
                         break;
                    case ICERA_RIL_STK_RESPONSE_ERROR_PROTOCOL:
                         generalResult = 5; /* Bearer unavailable */
                         break;
                    case ICERA_RIL_STK_RESPONSE_BROWSER_ERROR:
                         generalResult = 6; /* Browser unavailable */
                         break;
                    case ICERA_RIL_STK_RESPONSE_TERMINAL_COMMAND:
                         generalResult = 7; /* ME cannot process command */
                         break;
                    case ICERA_RIL_STK_RESPONSE_NETWORK_UNABLE:
                         generalResult = 8; /* Network cannot process command */
                         break;
                    case ICERA_RIL_STK_RESPONSE_NO_CAPABILITIES:
                         generalResult = 9; /* Command beyond MEs capabilities */
                         break;
                    default:
                         generalResult = 4; /* Error - no specific cause given */
                         break;
                }
                asprintf(&cmd, "AT*TSTCR=15,%d", generalResult);
                break;

            case ICERA_RIL_STK_COMMAND_SET_UP_IDLE_MODE_TEXT:
                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 0; /* Text successfully added/removed */
                         break;
                    default:
                         generalResult = 1; /* Problem performing command */
                         break;
                }
                asprintf(&cmd, "AT*TSTCR=28,%d", generalResult);
                break;

            case ICERA_RIL_STK_COMMAND_SET_UP_EVENT_LIST:
                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 0; /* Command performed successfully */
                         break;
                    default:
                         generalResult = 1; /* Cannot perform command */
                         break;
                }
                asprintf(&cmd, "AT*TSTCR=05,%d", generalResult);
                break;

            case ICERA_RIL_STK_COMMAND_SETUP_CALL:
                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 0; /* user accepted call (conf phase only) */
                         break;
                    case ICERA_RIL_STK_RESPONSE_UICC_TERMINATED:
                    case ICERA_RIL_STK_RESPONSE_TERMINAL_COMMAND:
                         generalResult = 1; /* user rejected call (conf phase only) */
                         break;
                    default:
                         generalResult = 2; /* user cleared call (any phase) */
                         break;
                }
                asprintf(&cmd, "AT*TSTCR=10,%d", generalResult);
                break;

            case ICERA_RIL_STK_COMMAND_SEND_DTMF:
                switch (generalResult) {
                    case ICERA_RIL_STK_RESPONSE_SUCCESS:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_ICON:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_LIMITED:
                    case ICERA_RIL_STK_RESPONSE_SUCCESS_NO_TONE:
                         generalResult = 1; /* DTMF required */
                         break;
                    default:
                         generalResult = 0; /* DTMF not accepted */
                         break;
                }
                asprintf(&cmd, "AT*TSTCR=14,%d", generalResult);
                break;

            case ICERA_RIL_STK_COMMAND_OPEN_CHANNEL:
                asprintf(&cmd, "AT*TSTCR=40,0"); /* this command is not support in Android */
                break;

            default:
                ALOGE("STKTR: Unknown proactive command for response");
                break;
        }

        if (!cmd) goto error;

        err = at_send_command(cmd, &p_response);
        free(cmd);

        if (err < 0 || p_response->success == 0) goto error;

        at_response_free(p_response);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        return;

error:
        at_response_free(p_response);
        ALOGE("ERROR: requestSTKSendTerminalResponse() failed\n");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}


void unsolSimStatusChanged(const char *param)
{
    int sim_state = 0, pbk_state = 0, sms_state = 0, err;
    char *line = (char *)param;

    /* Go and check the general status for the SIM */
    requestPutObject(ril_request_object_create(IRIL_REQUEST_ID_SIM_POLL, NULL, 0, NULL));

    do{
        err = at_tok_start(&line);
        if (err < 0)
        {
            break;
        }
        err = at_tok_nextint(&line, &sim_state);
        if (err < 0)
        {
            break;
        }
        err = at_tok_nextint(&line, &pbk_state);
        if (err < 0)
        {
            break;
        }
        err = at_tok_nextint(&line, &sms_state);
        if (err < 0)
        {
            break;
        }

        /* Allow call forward queries from now on */
        PbkLoaded = pbk_state;

        /* Repost pending call forward request now if applicable */
        if(PbkLoaded)
        {
            SendStoredQueryCallForwardStatus();
        }

        /* Sim and phonebook are loaded (SMS is to make sure we do this
         * only once per sim init) */
        if(((sim_state==1)&&(pbk_state==1))                  /* Sim inserted phonebook loaded */
         ||((sim_state==2)&&(pbk_state==1)&&(sms_state==0))  /* No Sim, en phonebook loaded */
         ||((sim_state==2)&&(pbk_state==0)&&(sms_state==0))) /* Sim pin/puk pending */
        {
            /*
             * Can't send at command as this may be called from unsolicited context
             * we need to queue an internal request instead
             */
            ril_request_object_t* req = ril_request_object_create(IRIL_REQUEST_ID_UPDATE_SIM_LOADED, NULL, 0, NULL);
            if(req != NULL)
            {
                requestPutObject(req);
            }
        }
    }
    while(0);
    // Update Emergency call number when sim_state changed.
    checkAndAmendEmergencyNumbers(sim_state, pbk_state);
    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0);

    return;
}


/**
 * RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM
 *
 * When STK application gets RIL_UNSOL_STK_CALL_SETUP, the call actually has
 * been initialized by ME already. (We could see the call has been in the 'call
 * list') So, STK application needs to accept/reject the call according as user
 * operations.
 *
 * "data" is int *
 * ((int *)data)[0] is > 0 for "accept" the call setup
 * ((int *)data)[0] is == 0 for "reject" the call setup
 *
 * "response" is NULL
 *
 * Valid errors:
 *  RIL_E_SUCCESS
 *  RIL_E_RADIO_NOT_AVAILABLE (radio resetting)
 *  RIL_E_GENERIC_FAILURE
 */
void requestStkHandleCallSetupRequestedFromSim(void * data, size_t datalen, RIL_Token t)
{
    char *cmd = NULL;
    int err = 0;
    int  force_use_legacy_api = 0;
    ATResponse *p_response = NULL;

    /* Firsts firmwares supporting "CUSAT"  interface were not supporting %IUSATC
       So first try this new command.
       And if it is not OK then use the legacy one (wich was still supported by firmware befor supporting %IUSATC)
       */

    if(stk_use_cusat_api)
    {
        asprintf(&cmd, "AT%%IUSATC=4,%d", (((int*)data)[0] > 0)?1:0);

        err = at_send_command(cmd, &p_response);

        if (err < 0 || p_response->success == 0)
        {
             force_use_legacy_api = 1;
        }
        at_response_free(p_response);

        free(cmd);
    }


    if(!stk_use_cusat_api || force_use_legacy_api)
    {
        asprintf(&cmd, "AT*TSTCR=10,%d", (((int*)data)[0] == 0)?1:0);

        err = at_send_command(cmd, NULL);

        free(cmd);
    }


    if (err < 0)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

    error:
    RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
}


/**
 * RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING
 *
 * Indicates that the StkSerivce is running and is
 * ready to receive RIL_UNSOL_STK_XXXXX commands.
 *
 * "data" is NULL
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */
void requestSTKReportSTKServiceIsRunning(void * data, size_t datalen, RIL_Token t)
{
    if(stk_use_cusat_api)
    {
        int err = 0;
        s_stkServiceRunning = 1;

        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

        /* Send Proactive commands that came before this notification*/
        if(s_pendingProactive)
        {
            RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, s_pendingProactive, s_pendingLen);
            free(s_pendingProactive);
            s_pendingProactive = NULL;
            s_pendingLen = 0;
        }

        return;
    }
    else
    {
        int err = 0;

        err =  StkNotifyServiceIsRunning(1);

        if(err!=0)
            goto error;

        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

        return;

error:
        ALOGE("ERROR: requestSTKReportSTKServiceIsRunning() failed\n");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}


/**
 * Wait for RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING only at
 * first RIL boot.
 * This request will not be sent at next RIL restart since it is
 * sent only when STK service itself is restarted.
 */
void StkIsFirstBoot(int first_boot)
{
    ALOGD("StkIsFirsBoot first_boot:%d\n", first_boot);
    if (first_boot)
       s_stkServiceRunning = 0;
    else
       s_stkServiceRunning = 1;
}

/**
 * RIL_UNSOL_STK_EVENT_NOTIFY
 *
 * Indicate when SIM notifies applcations some event happens.
 * Generally, application does not need to have any feedback to
 * SIM but shall be able to indicate appropriate messages to users.
 *
 * "data" is a const char * containing SAT/USAT commands or responses
 * sent by ME to SIM or commands handled by ME, in hexadecimal format string
 * starting with first byte of response data or command tag
 *
 */
void unsolStkEventNotify(const char *s)
{
    if(stk_use_cusat_api)
    {
        char *line = NULL;
        char *tok = NULL;
        int err;

        line = tok = strdup(s);

        char *result;
        err = at_tok_start(&tok);
        if (err < 0) goto end;
        err = at_tok_nextstr(&tok, &result);
        if (err < 0) goto end;

        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_EVENT_NOTIFY, result, strlen(result));
end:
        free(line);
    }
    else
    {
        int err;
        int cmd, cmdlen;
        int type;
        int len, numOfFiles;
        char pkglen[4];
        char *result;
        char *response = NULL;
        char *alpha = NULL;
        char *hex_string;
        char *dialstring;

        response = (char *)alloca(strlen(s) + 1);
        if (!response) goto error;

        strcpy(response, s);

        err = at_tok_start(&response);
        if (err < 0) goto error;

        err = at_tok_nextint(&response, &cmd);
        if (err < 0) goto error;

        stk_pro_cmd_ptr = stk_pro_cmd_buf;

        switch (cmd) {

            case 35: /* LANGUAGE SELECTION */
                /*
                 * Example:
                 *   *TSTUD: 35[,<language>]
                 */
                alpha = NULL;
                if (at_tok_hasmore(&response)) {
                    err = at_tok_nextstr(&response, &alpha);
                    if (err < 0) {
                        alpha = NULL;
                    }
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_LANGUAGE_SELECTION;
                *stk_pro_cmd_ptr++ = 0;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_TERMINAL;


                if (alpha != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_LANGUAGE_TAG;
                    len = strlen(alpha);
                    if (len % 2 != 0) goto error;

                    len /= 2;
                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                    hexToAscii(alpha, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

            case 13: /* SEND SHORT MESSAGE */
                /*
                 * Example:
                 *   *TSTUD:13[,<alphaId>[,<iconId>,<dispMode>]]
                 */
                alpha = NULL;
                if (at_tok_hasmore(&response)) {
                    err = at_tok_nextstr(&response, &alpha);
                    if (err < 0) {
                        alpha = NULL;
                    }
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_SEND_SHORT_MESSAG;
                *stk_pro_cmd_ptr++ = 0;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_NETWORK;

                if (alpha != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ALPHA_IDENTITY_TAG;
                    len = strlen(alpha);
                    if (len % 2 != 0) goto error;

                    len /= 2;
                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                    hexToAscii(alpha, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

            case 12: /* SEND USSD */
                /*
                 * Example:
                 *   *TSTUD:12[,<alphaId>[,<iconId>,<dispMode>]]
                 */
                alpha = NULL;
                if (at_tok_hasmore(&response)) {
                    err = at_tok_nextstr(&response, &alpha);
                    if (err < 0) {
                        alpha = NULL;
                    }
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_SEND_USSD;
                *stk_pro_cmd_ptr++ = 0;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_NETWORK;

                if (alpha != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ALPHA_IDENTITY_TAG;
                    len = strlen(alpha);
                    if (len % 2 != 0) goto error;

                    len /= 2;
                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                    hexToAscii(alpha, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

            case 11: /* SEND SS */
                /*
                 * Example:
                 *   *TSTUD:11[,<alphaId>[,<iconId>,<dispMode>]]
                 */
                alpha = NULL;
                if (at_tok_hasmore(&response)) {
                    err = at_tok_nextstr(&response, &alpha);
                    if (err < 0) {
                        alpha = NULL;
                    }
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_SEND_SS;
                *stk_pro_cmd_ptr++ = 0;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_NETWORK;

                if (alpha != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ALPHA_IDENTITY_TAG;
                    len = strlen(alpha);
                    if (len % 2 != 0) goto error;

                    len /= 2;
                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                    hexToAscii(alpha, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

    #if 0
    /* Commented out for the time being as there are no way to distinguish between
     * a setup call where it is not needed (and will cause a confusing diplay) and
     * the call control process where it may be needed to inform the user that the number was altered.
     */
            case 10: /* Set Up Call */
                /*
                 * Example:
                 *   *TSTUD:10,<alphaId>,<dialstring>,<cps>[,<iconId>,<dispMode>]
                 */
                alpha = NULL;
                (void)at_tok_nextstr(&response, &alpha); /* alphaId */

                dialstring = NULL;
                err = at_tok_nextstr(&response, &dialstring); /* dialstring */
                if (err < 0) goto error;

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_SETUP_CALL;
                *stk_pro_cmd_ptr++ = 0;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_EARPIECE;

                if (alpha != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_ALPHA_IDENTIFIER_TAG;
                    len = strlen(alpha);
                    if (len % 2 != 0) goto error;
                    len /= 2;

                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                    hexToAscii(alpha, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }

                if (dialstring != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ADDRESS_TAG;
                    len = strlen(dialstring);
                    len = (len + 1) / 2;

                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len + 1);
                    *stk_pro_cmd_ptr++ = 0x81;  /* TON and NPI */
                    numericToBCD(dialstring, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;
    #endif

            case 41: /* Close Channel */
                /*
                 * Example:
                 *   *TSTUD:41[,<alphaId>[,<iconId>,<dispMode>]]
                 */
                goto error; /* this command is not support in Android */
                break;

            case 42: /* Receive Data */
                /*
                 * Example:
                 *   *TSTUD:42,<length>[,<alphaId>[,<iconId>,<dispMode>]]
                 */
                goto error; /* this command is not support in Android */
                break;

            case 43: /* Send Data */
                /*
                 * Example:
                 *   *TSTUD:43,<length>,<data>[,<alphaId>[,<iconId>,<dispMode>]]
                 */
                goto error; /* this command is not support in Android */
                break;

            case 34: /* Run AT Command */
                /*
                 * Example:
                 *   *TSTUD:34[,<alphaId>[,<iconId>,<dispMode>]]
                 */
                goto error; /* this command is not support in Android */
                break;

            default:
                goto error;
                break;
        }

        cmdlen = stk_pro_cmd_ptr - stk_pro_cmd_buf;
        result = (char *)alloca(cmdlen * 2 + 8 + 1);
        if (!result) goto error;

        asciiToHex(stk_pro_cmd_buf, result, 1);

        cmdlen--;
        len = (packLen(&pkglen[0], cmdlen) - &pkglen[0]);
        asciiToHex(&pkglen[0], result + 2, len);
        asciiToHex(stk_pro_cmd_buf + 1, result + 2 + len * 2, cmdlen);

        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_EVENT_NOTIFY, result, 0);
        return;

    error:
        ALOGE("Failed to parse *TSTUD, line=%s", s);
    }
}


/**
 * RIL_UNSOL_STK_PROACTIVE_COMMAND
 *
 * Indicate when SIM issue a STK proactive command to applications
 *
 * "data" is a const char * containing SAT/USAT proactive command
 * in hexadecimal format string starting with command tag
 *
 */
void requestSTKPROReceived(void *data, size_t datalen, RIL_Token t)
{
    int err;
    int cmdlen;
    int type, icon_id, disp_mode;
    int len, numOfFiles, total_items;
    int dcs_item, default_dcs;
    int item_id;
    int tone, interval_item;
    int min_rsp_len, max_rsp_len;
    int default_item;
    int qualifier;
    int imm_resp;
    int resp, echo, help, priority, clear,selection, remove_menu;
    char pkglen[4];
    char *len_ptr;
    char *result;
    char *response = NULL;
    char *alpha = NULL;
    char *alpha2 = NULL;
    char *item_text;
    char *hex_string;
    char *ascii_string;
    char *default_text;
    char *dialstring;
    char *url;
    ATLine *p_cur;
    ATResponse *p_response = NULL;
    char *cmds = NULL;
    int cmd = *((int *)data);

    if(cmd == 0)
    {
        ALOGD("No STK application on SIM");
        return;
    }

    asprintf(&cmds, "AT*TSTGC=%d", cmd);
    err = at_send_command_multiline(cmds, "*TSTGC:", &p_response);
    free(cmds);

    if (err < 0 || p_response->success == 0) goto error;

    stk_pro_cmd_ptr = stk_pro_cmd_buf;

    for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next) {
        response = p_cur->line;

        err = at_tok_start(&response);
        if (err < 0) goto error;

        if (p_cur == p_response->p_intermediates) {
            err = at_tok_nextint(&response, &type);
            if ((err < 0) || (type != cmd)) goto error;
        }

        switch (cmd) {
            case 25: /* Set Up Menu */
                /*
                 * Example:
                 *   *TSTGC:25,<numItems>,<selection>,<removemenu>,<helpInfo>,<alphaId>[,<iconId>,<dispMode>]
                 *   *TSTGC:<itemId>,<itemText>[,<iconId>,<dispMode>,<nai>
                 *   *TSTGC:<itemId>,<itemText>[,<iconId>,<dispMode>,<nai>
                 *   ...
                 */
                if (p_cur == p_response->p_intermediates) {
                    qualifier = 0;
                    err = at_tok_nextint(&response, &total_items);
                    if (err < 0) goto error;

                    err = at_tok_nextint(&response, &selection);  /* selection */
                    if (err < 0) goto error;
                    if(selection) qualifier |= 0x01;

                    err = at_tok_nextint(&response, &remove_menu);  /* removemenu */
                    if (err < 0) goto error;

                    err = at_tok_nextint(&response, &help);  /* helpInfo */
                    if (err < 0) goto error;
                    if(help) qualifier |= 0x80;

                    alpha = NULL;
                    (void)at_tok_nextstr(&response, &alpha); /* alphaId */

                    /* first line */
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                    *stk_pro_cmd_ptr++ = 3;
                    *stk_pro_cmd_ptr++ = stk_command_number++;
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_SETUP_MENU;
                    *stk_pro_cmd_ptr++ = qualifier;
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                    *stk_pro_cmd_ptr++ = 2;
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;     /* from UICC */
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_TERMINAL; /* to terminal */

                    if (alpha != NULL) {
                        *stk_pro_cmd_ptr++ = ICERA_RIL_ALPHA_IDENTIFIER_TAG;
                        len = strlen(alpha);
                        if (len % 2 != 0) goto error;

                        len /= 2;
                        stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                        hexToAscii(alpha, stk_pro_cmd_ptr, len);
                        stk_pro_cmd_ptr += len;
                    }
                    if((total_items == 0) || remove_menu)
                    {
                        /* Indicate an item with length null to force removing the menu. */
                        *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ITEM_TAG;
                        *stk_pro_cmd_ptr++ = 0;
                    }
                }
                else {
                    err = at_tok_nextint(&response, &item_id);
                    if (err < 0) goto error;

                    err = at_tok_nextstr(&response, &item_text);
                    if (err < 0) goto error;

                    /* subsequent lines */
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ITEM_TAG;
                    len = strlen(item_text);
                    if (len % 2 != 0) goto error;

                    len /= 2;
                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len + 1);
                    *stk_pro_cmd_ptr++ = item_id;
                    hexToAscii(item_text, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

            case 24: /* Select Item */
                /*
                 * Example:
                 *   *TSTGC:24,<numItems>,<selection>,<helpInfo>,<alphaId>,<iconId>,<dispMode>,<presentationTypeInfoPresent>[,<defaultItemId>]
                 *   *TSTGC:<itemId>,<itemText>[,<iconId>,<dispMode>,<nai>
                 *   *TSTGC:<itemId>,<itemText>[,<iconId>,<dispMode>,<nai>
                 *   ...
                 */
                if (p_cur == p_response->p_intermediates) {
                    qualifier = 0;
                    err = at_tok_nextint(&response, &total_items);
                    if (err < 0) goto error;

                    err = at_tok_nextint(&response, &selection);  /* selection */
                    if (err < 0) goto error;
                    if(selection) qualifier |= 0x04;

                    err = at_tok_nextint(&response, &help);  /* helpInfo */
                    if (err < 0) goto error;
                    if(help) qualifier |= 0x80;

                    alpha = NULL;
                    err = at_tok_nextstr(&response, &alpha); /* alphaId */
                    if (err < 0) goto error;

                    /* Unused parameters. */
                    (void)at_tok_nextint(&response, &icon_id);  /* iconId */
                    (void)at_tok_nextint(&response, &disp_mode);  /* dispMode */

                    err = at_tok_nextint(&response, &type);  /* presentationTypeInfoPresent */
                    /* Error here means this parameter is not present so do not set bit 0x01 of qualifier*/
                    if (err >= 0)
                    {
                        qualifier |= 0x01;             /* Presentation type info is present */
                        if(type) qualifier |= 0x02;    /* Presentation type info */
                    }

                    default_item = 0;
                    if (at_tok_hasmore(&response)) {
                        err = at_tok_nextint(&response, &default_item); /* defaultItemId */
                        if (err < 0) goto error;
                    }

                    /* first line */
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                    *stk_pro_cmd_ptr++ = 3;
                    *stk_pro_cmd_ptr++ = stk_command_number++;
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_SELECT_ITEM;
                    *stk_pro_cmd_ptr++ = qualifier;
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                    *stk_pro_cmd_ptr++ = 2;
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;     /* from UICC */
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_TERMINAL; /* to terminal */

                    if (alpha != NULL) {
                        *stk_pro_cmd_ptr++ = ICERA_RIL_ALPHA_IDENTIFIER_TAG;
                        len = strlen(alpha);
                        if (len % 2 != 0) goto error;

                        len /= 2;
                        stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                        hexToAscii(alpha, stk_pro_cmd_ptr, len);
                        stk_pro_cmd_ptr += len;
                    }

                    if (default_item) {
                        *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ITEM_ID_TAG;
                        *stk_pro_cmd_ptr++ = 1;
                        *stk_pro_cmd_ptr++ = default_item;
                    }
                    if(total_items == 0)
                    {
                        /* Indicate an item with length null */
                        *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ITEM_TAG;
                        *stk_pro_cmd_ptr++ = 0;
                    }
                }
                else {
                    err = at_tok_nextint(&response, &item_id);
                    if (err < 0) goto error;

                    err = at_tok_nextstr(&response, &item_text);
                    if (err < 0) goto error;

                    /* subsequent lines */
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ITEM_TAG;
                    len = strlen(item_text);
                    if (len % 2 != 0) goto error;

                    len /= 2;
                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len + 1);
                    *stk_pro_cmd_ptr++ = item_id;
                    hexToAscii(item_text, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

            case 21: /* Display Text */
                /*
                 * Example:
                 *   *TSTGC:21,<dcs>,<text>,<priority>,<clear>[,<iconId>,<dispMode>[,<response>]]
                 */
                err = at_tok_nextint(&response, &dcs_item);
                if (err < 0) goto error;

                err = at_tok_nextstr(&response, &hex_string);
                if (err < 0) goto error;

                err = at_tok_nextint(&response, &priority);  /* priority */
                if (err < 0) goto error;
                qualifier = 0;
                if(priority)  qualifier |= 1;
                err = at_tok_nextint(&response, &clear);  /* clear */
                if (err < 0) goto error;
                if(clear)  qualifier |= 0x80;

                icon_id = imm_resp = 0;
                if (at_tok_hasmore(&response)) {
                    (void)at_tok_nextint(&response, &icon_id);
                    (void)at_tok_nextint(&response, &type);  /* dispMode */

                    if (at_tok_hasmore(&response)) {
                        (void)at_tok_nextint(&response, &imm_resp);
                    }
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DISPLAY_TEXT;
                *stk_pro_cmd_ptr++ = qualifier;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_DISPLAY;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_TEXT_STRING_TAG;
                len = strlen(hex_string);
                if (len % 2 != 0) goto error;

                len /= 2;
                stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len + 1);
                *stk_pro_cmd_ptr++ = dcs_item;
                hexToAscii(hex_string, stk_pro_cmd_ptr, len);
                stk_pro_cmd_ptr += len;

                if (icon_id) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ICON_IDENTIFIER_TAG;
                    *stk_pro_cmd_ptr++ = 02;
                    *stk_pro_cmd_ptr++ = 01;
                    *stk_pro_cmd_ptr++ = icon_id;
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_IMMEDIATE_RESPONSE_TAG;
                *stk_pro_cmd_ptr++ = 00;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DURATION_TAG;
                *stk_pro_cmd_ptr++ = 02;
                *stk_pro_cmd_ptr++ = 01; //00 - min, 01 - sec, 02 - tenths of seconds
                *stk_pro_cmd_ptr++ = imm_resp; //00 - reserved, 01 - unit, 02 - 2 unit
                break;

            case 23: /* Get Input */
                /*
                 * Example:
                 *   *TSTGC:23,<dcs>,<text>,<response>,<echo>,<helpInfo>,<minLgth>,<maxLgth>[,<dcs>,<default>[,<iconId>,<dispMode>]]
                 */
                err = at_tok_nextint(&response, &dcs_item);
                if (err < 0) goto error;

                err = at_tok_nextstr(&response, &hex_string);
                if (err < 0) goto error;

                err = at_tok_nextint(&response, &resp);  /* response */
                if (err < 0) goto error;
                switch(resp)
                {
                case 1: qualifier = 0; break;    /* SMS  alphabet - unpacked - digits only*/
                case 2: qualifier = 8; break;    /* SMS  alphabet - packed      - digits only*/
                case 3: qualifier = 2; break;    /* UCS2  alphabet                     - digits only*/
                case 4: qualifier = 1; break;    /* SMS  alphabet - unpacked                      */
                case 5: qualifier = 9; break;    /* SMS  alphabet - packed                           */
                case 6: qualifier = 3; break;    /* UCS2  alphabet                                          */
                default: qualifier = 0;
                }

                err = at_tok_nextint(&response, &echo);  /* echo */
                if (err < 0) goto error;
                if(!echo) qualifier |= 0x04;     /* Set echo bit in command qualifier*/

                err = at_tok_nextint(&response, &help);  /* helpInfo */
                if (err < 0) goto error;
                if(help) qualifier |= 0x80;     /*  Set help bit incommand qualifier*/

                err = at_tok_nextint(&response, &min_rsp_len);
                if (err < 0) goto error;

                err = at_tok_nextint(&response, &max_rsp_len);
                if (err < 0) goto error;

                default_text = NULL;
                if (at_tok_hasmore(&response)) {
                    err = at_tok_nextint(&response, &default_dcs);
                    if (err < 0)
                    {
                           default_dcs = 0;
                           default_text = NULL;
                    }
                    else
                    {

                        err = at_tok_nextstr(&response, &default_text);
                        if (err < 0)
                        {
                               default_dcs = 0;
                               default_text = NULL;
                        }
                    }
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_GET_INPUT;
                *stk_pro_cmd_ptr++ = qualifier;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_TERMINAL;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_TEXT_STRING_TAG;
                len = strlen(hex_string);
                ALOGD("STK - len:%d", len);
                if (len % 2 != 0) goto error;

                len /= 2;
                stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len + 1);
                *stk_pro_cmd_ptr++ = dcs_item;
                hexToAscii(hex_string, stk_pro_cmd_ptr, len);
                stk_pro_cmd_ptr += len;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_RESPONSE_LENGTH_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = min_rsp_len;
                *stk_pro_cmd_ptr++ = max_rsp_len;

                if (default_text != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEFAULT_TEXT_TAG;
                    len = strlen(default_text);
                    ALOGD("STK - len:%d", len);
                    if (len % 2 != 0) goto error;

                    len /= 2;
                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len + 1);
                    *stk_pro_cmd_ptr++ = default_dcs;
                    hexToAscii(default_text, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

            case 22: /* Get Inkey */
                /*
                 * Example:
                 *   *TSTGC:22,<dcs>,<text>,<response>,<helpInfo>[,<iconId>,<dispMode>]
                 */
                err = at_tok_nextint(&response, &dcs_item);
                if (err < 0) goto error;

                err = at_tok_nextstr(&response, &hex_string);
                if (err < 0) goto error;

                err = at_tok_nextint(&response, &resp);
                if (err < 0) goto error;
                switch(resp)
                {
                case 0: qualifier = 0; break;    /* digits only          */
                case 1: qualifier = 1; break;    /* SMS  alphabet   */
                case 2: qualifier = 2; break;    /* UCS2  alphabet */
                case 3: qualifier = 4; break;    /* Yes/No                */
                default: qualifier = 0;
                }
                err = at_tok_nextint(&response, &help);  /* helpInfo */
                if (err < 0) goto error;
                if(help)
                {
                     qualifier |= 0x80;     /*  Set help bit incommand qualifier*/
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_GET_INKEY;
                *stk_pro_cmd_ptr++ = qualifier;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_TERMINAL;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_TEXT_STRING_TAG;
                len = strlen(hex_string);
                if (len % 2 != 0) goto error;

                len /= 2;
                stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len + 1);
                *stk_pro_cmd_ptr++ = dcs_item;
                hexToAscii(hex_string, stk_pro_cmd_ptr, len);
                stk_pro_cmd_ptr += len;
                break;

            case 20: /* Play Tone */
                /*
                 * Example:
                 *   *TSTGC:20[,<alphaId>[,<tone>[,<duration>]]]
                 */
                alpha = NULL; tone = 0; interval_item = 0;
                if (at_tok_hasmore(&response)) {
                    (void)at_tok_nextstr(&response, &alpha);

                    if (at_tok_hasmore(&response)) {
                        (void)at_tok_nextint(&response, &tone);

                        if (at_tok_hasmore(&response)) {
                            (void)at_tok_nextint(&response, &interval_item);
                        }
                    }
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_PLAY_TONE;
                *stk_pro_cmd_ptr++ = 0;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_EARPIECE;

                if (alpha != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_ALPHA_IDENTIFIER_TAG;
                    len = strlen(alpha);
                    if (len % 2 != 0) goto error;
                    len /= 2;

                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                    hexToAscii(alpha, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;

                    if (tone) {
                        *stk_pro_cmd_ptr++ = ICERA_RIL_STK_TONE_TAG;
                        *stk_pro_cmd_ptr++ = 1;
                        *stk_pro_cmd_ptr++ = tone;

                        if (interval_item) {
                            *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DURATION_TAG;
                            *stk_pro_cmd_ptr++ = 2;
                            *stk_pro_cmd_ptr++ = 1;
                            *stk_pro_cmd_ptr++ = interval_item;
                        }
                    }
                }
                break;

            case 28: /* Set Up Idle Mode */
                /*
                 * Example:
                 *   *TSTGC:28,<dcs>,<text>[,<iconId>,<dispMode>]
                 */
                err = at_tok_nextint(&response, &dcs_item);
                if (err < 0) goto error;

                err = at_tok_nextstr(&response, &hex_string);
                if (err < 0) goto error;

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_SET_UP_IDLE_MODE_TEXT;
                *stk_pro_cmd_ptr++ = 0;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_TERMINAL;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_TEXT_STRING_TAG;
                len = strlen(hex_string);
                if (len % 2 != 0) goto error;
                len /= 2;

                stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len + 1);
                *stk_pro_cmd_ptr++ = dcs_item;
                hexToAscii(hex_string, stk_pro_cmd_ptr, len);
                stk_pro_cmd_ptr += len;
                break;

            case 15: /* Launch Browser */
                /*
                 * Example:
                 *   *TSTGC:15,<comQual>,<url>[,<browserId>[,<bearer>[,<numFiles>,<provFiles>[,<dcs>,<gateway>[,<alphaId>[,<iconId>,<dispMode>]]]]]]
                 */
                /* Qualifier for Launch browser
                 *  00 - launch browser without making connection, if not already launched
                 *  01 - launch browser making connection, if not already launched
                 *  02 - use existing browser
                 *  03 - close existing browser, launch new browser, making a connection
                 *  04 - close existing browser, launch new browser, using secure session
                 */
                err = at_tok_nextint(&response, &type);  /* comQual */
                if (err < 0) goto error;
                switch(type)
                {
                case 0: qualifier = 0; break;
                case 1: qualifier = 0; break;
                case 2: qualifier = 2; break;
                case 3: qualifier = 3; break;
                case 4: qualifier = 3; break;
                default: 0;
                }

                err = at_tok_nextstr(&response, &url);
                if (err < 0) goto error;

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_LAUNCH_BROWSER;
                *stk_pro_cmd_ptr++ = qualifier;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_TERMINAL;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_BROWSER_IDENTITY_TAG;
                *stk_pro_cmd_ptr++ = 1;
                *stk_pro_cmd_ptr++ = 0;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_URL_TAG;

                if (url == NULL) {
                    *stk_pro_cmd_ptr++ = 0;
                }
                else {
                    len = strlen(url);
                    if (len % 2 != 0) goto error;
                    len /= 2;

                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                    hexToAscii(url, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

            case 05: /* Set Up Event List */
                /*
                 * Example:
                 *   *TSTGC: 05,<eventList>
                 */
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_SET_UP_EVENT_LIST;
                *stk_pro_cmd_ptr++ = 0;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_TERMINAL;

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_EVENT_LIST_TAG;
                len_ptr = stk_pro_cmd_ptr;
                *stk_pro_cmd_ptr++ = 0;

                while (at_tok_hasmore(&response)) {
                    err = at_tok_nexthexint(&response, &type);
                    if (err < 0) goto error;

                    *stk_pro_cmd_ptr++ = type;
                    (*len_ptr)++;
                }
                break;

            case 10: /* Get Acknowledgement For Set Up Call */
                /*
                 * Example:
                 *   *TSTGC:10,<alphaId>,<iconId>,<dispMode>,<commandQualifier>,<dialstring>,<cps>,<callSetupAlphaId>
                 */
                alpha = NULL;
                (void)at_tok_nextstr(&response, &alpha); /* alphaId */
                (void)at_tok_nextint(&response, &type);  /* iconId */
                (void)at_tok_nextint(&response, &type);  /* dispMode */

                qualifier = 0;
                (void)at_tok_nextint(&response, &qualifier);  /* commandQualifier */

                dialstring = NULL;
                err = at_tok_nextstr(&response, &dialstring); /* dialstring */
                if (err < 0) goto error;

                (void)at_tok_nextstr(&response, &default_text); /* cps */

                alpha2 = NULL;
                (void)at_tok_nextstr(&response, &alpha2); /* callSetupAlphaId */

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_SETUP_CALL;
                *stk_pro_cmd_ptr++ = qualifier;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_EARPIECE;

                if (alpha != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_ALPHA_IDENTIFIER_TAG;
                    len = strlen(alpha);
                    if (len % 2 != 0) goto error;
                    len /= 2;

                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                    hexToAscii(alpha, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }

                if (dialstring != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_ADDRESS_TAG;
                    len = strlen(dialstring);
                    len = (len + 1) / 2;

                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len + 1);
                    *stk_pro_cmd_ptr++ = 0x81;  /* TON and NPI */
                    numericToBCD(dialstring, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }

                if (alpha2 != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_ALPHA_IDENTIFIER_TAG;
                    len = strlen(alpha2);
                    if (len % 2 != 0) goto error;
                    len /= 2;

                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                    hexToAscii(alpha2, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

            case 14: /* Send DTMF */
                /*
                 * Example:
                 *   *TSTGC:14[,<alphaId>[,<iconId>,<dispMode>]]
                 */
                alpha = NULL;
                if (at_tok_hasmore(&response)) {
                    (void)at_tok_nextstr(&response, &alpha);
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_SEND_DTMF;
                *stk_pro_cmd_ptr++ = 0;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_EARPIECE;

                if (alpha != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_ALPHA_IDENTIFIER_TAG;
                    len = strlen(alpha);
                    if (len % 2 != 0) goto error;
                    len /= 2;

                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len);
                    hexToAscii(alpha, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

            case 40: /* Open Channel */
                /*
                 * Example:
                 *   *TSTGC:40[,<alphaId>[,<iconId>,<dispMode>]]
                 */
                (void)at_send_command("AT*TSTCR=40,0", NULL); /* this command is not support in Android */
                goto error;
                break;

            case 01: /* REFRESH */
                /*
                 * Example:
                 *   *TSTGC:01,<refMode>[,<numFiles>,<fileList>]
                 */
                err = at_tok_nextint(&response, &type);
                if (err < 0) goto error;

                numOfFiles = 0;
                if (at_tok_hasmore(&response)) {
                    err = at_tok_nextint(&response, &numOfFiles);
                    if (err < 0) {
                        numOfFiles = 0;
                    }
                }

                hex_string = NULL;
                if (at_tok_hasmore(&response)) {
                    err = at_tok_nextstr(&response, &hex_string);
                    if (err < 0) {
                        hex_string = NULL;
                    }
                }

                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_PROACTIVE_COMMAND_TAG;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_DETAILS_TAG;
                *stk_pro_cmd_ptr++ = 3;
                *stk_pro_cmd_ptr++ = stk_command_number++;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_COMMAND_REFRESH;
                *stk_pro_cmd_ptr++ = type;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEVICE_IDENTITIES_TAG;
                *stk_pro_cmd_ptr++ = 2;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_UICC;
                *stk_pro_cmd_ptr++ = ICERA_RIL_STK_DEV_ID_TERMINAL;

                if (hex_string != NULL) {
                    *stk_pro_cmd_ptr++ = ICERA_RIL_STK_FILE_LIST_TAG;
                    len = strlen(hex_string);
                    if (len % 2 != 0) goto error;

                    len /= 2;
                    stk_pro_cmd_ptr = packLen(stk_pro_cmd_ptr, len + 1);
                    *stk_pro_cmd_ptr++ = numOfFiles;
                    hexToAscii(hex_string, stk_pro_cmd_ptr, len);
                    stk_pro_cmd_ptr += len;
                }
                break;

            default:
                goto error;
                break;
        }
    }

    cmdlen = stk_pro_cmd_ptr - stk_pro_cmd_buf;
    result = (char *)alloca(cmdlen * 2 + 8 + 1);
    if (!result) goto error;

    asciiToHex(stk_pro_cmd_buf, result, 1);

    cmdlen--;
    len = (packLen(&pkglen[0], cmdlen) - &pkglen[0]);
    asciiToHex(&pkglen[0], result + 2, len);
    asciiToHex(stk_pro_cmd_buf + 1, result + 2 + len * 2, cmdlen);

    RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, result, sizeof(result));
    return;

error:
    ALOGE("Failed to parse *TSTGC, cmd=%d", cmd);
}


void unsolicitedSTKPRO(const char *s)
{
    if(stk_use_cusat_api)
    {
        char *line = NULL;
        char *tok = NULL;
        char *result;
        int err;
        line = tok = strdup(s);
        err = at_tok_start(&tok);
        if(err<0) goto end;
        err = at_tok_nextstr(&tok, &result);
        if(err<0) goto end;

        if(!s_pendingProactive && !s_stkServiceRunning)
        {
              /*
               * If notification that Android STK service is running has not been yet received
               * we cannot send unsollicited. So memorize the received proactive command.
               * There should not be more than 1.
               */
              s_pendingLen =  strlen(result);
              s_pendingProactive = malloc(s_pendingLen+1);
              memcpy(s_pendingProactive, result, s_pendingLen+1);
        }

        if(s_stkServiceRunning)
        {
            RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, result, strlen(result));
        }
end:
        free(line);
    }
    else
    {
        char *line = NULL;
        char *tok = NULL;
        int cmd;
        int err;

        line = tok = strdup(s);

        err = at_tok_start(&tok);
        if (err < 0) goto error;

        err = at_tok_nextint(&tok, &cmd);

        if (err < 0) goto error;

        /* '81' End of proactive session */
        if (cmd == 81) {
            RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END, NULL, 0);
        }
        else {
            void * req = (void *)ril_request_object_create(IRIL_REQUEST_ID_STK_PRO, &cmd, sizeof(cmd), NULL);

            if(req == NULL)
            {
                goto error;
            }
            /* No need tfor a timed callback here */
            requestPutObject(req);
        }

        free(line);
        return;

error:
        free(line);
        ALOGE("Failed to parse *TSTC.");
    }
}

int PbkIsLoaded(void)
{
    return PbkLoaded;
}

int decodeCallControlInfo(char *input, int call_or_sms, CallControlInfo *output)
{
    char callcontrol_result = 0, val_len = 0, length_len = 0;
    const char tag_len = 1;
    char *p_input = input;
    int index = 0;
    int total_length = strlen (input);
    char *outputByte = NULL;

    int SCA_Parsed = 0;

    // checkout output pointer
    if (output == NULL) {
        ALOGE(LOG_TAG, "output pointer is invalid!");
        return 0;
    }

    memset (output, 0, sizeof(CallControlInfo));

    if (call_or_sms == STK_UNSOL_CCBS) // It is a call
        output->Type = ICERA_RIL_STKCTRLIND_TYPE_MO_CALL_CONTROL;
    else   // It is a SMS
        output->Type = ICERA_RIL_STKCTRLIND_TYPE_MO_SMS_CONTROL;

    outputByte = (char *)alloca(total_length/2+1);
    hexToAscii(input, outputByte, total_length/2);

    if(total_length == 4 && outputByte[0] == 0x90) {
        //special case, only show sw1/sw2, means "allow, no modification"
        output->Result = ICERA_RIL_STKCTRLIND_RESULT_ALLOWED;
        return 1;
    }
    if(outputByte[0] == 0x93) {
         output->Result = ICERA_RIL_STKCTRLIND_RESULT_TOOLKIT_BUSY;
         return 1;
    }

    // Dealing with result value
    output->Result = outputByte[0];

    total_length = parseLen(&outputByte[1], &length_len);
    index = tag_len + length_len;   // tag length + length length

    if(total_length == 0)
        return 1;

    // Parsing TLV blocks
    while(total_length > 0) {
        switch(outputByte[index++] & ICERA_RIL_STK_COMMAND_TAG_MASK) {
            /*  ADDRESS_TAG = Address id[1] + Length[1] + TON_NPI[1] + Dialing Number String*/
            case (ICERA_RIL_STK_ADDRESS_TAG & ICERA_RIL_STK_COMMAND_TAG_MASK):
                val_len = parseLen(&outputByte[index], &length_len);
                index += length_len;

                if(call_or_sms == STK_UNSOL_CCBS) {
                    // Call, USSD, SS
                    //TON_NPI
                    output->Destination_address_type = outputByte[index++];
                    //Dialing Number String
                    int num_len = val_len - 1;  // Need to remove TON_NPI length
                    int DestAddr_Size = (num_len * 2) + 1;  // Need to transfer to Hex string
                    output->Destination_address = (char *)malloc(DestAddr_Size);
                    if (output->Destination_address == NULL) {
                        ALOGE(LOG_TAG, "decodeCallControlInfo, Alloc memory for destination address failed.");
                        return 0;
                    }
                    memset(output->Destination_address, 0, DestAddr_Size);
                    asciiToHexRevByteOrder(outputByte + index, output->Destination_address, num_len);
                    index += num_len;
                    removeInvalidPhoneNum(output->Destination_address);

                    ALOGD("decodeCallControlInfo, Call Destination_address_type=%d, Destination_address: %s", output->Destination_address_type, output->Destination_address);
                }else {
                    // SMS
                    if(SCA_Parsed == 1) {
                        // Parsing destination address type
                        output->Destination_address_type = outputByte[index++];
                        //Dialing Number String
                        int num_len = val_len - 1;  // Need to remove TON_NPI length
                        int DestAddr_Size = (num_len * 2) + 1;  // Need to transfer to Hex string
                        output->Destination_address = (char *)malloc(DestAddr_Size);
                        if (output->Destination_address == NULL) {
                            ALOGE(LOG_TAG, "decodeCallControlInfo, Alloc memory for destination address failed.");
                            return 0;
                        }
                        memset(output->Destination_address, 0, DestAddr_Size);
                        asciiToHexRevByteOrder(outputByte + index, output->Destination_address, num_len);
                        index += num_len;
                        removeInvalidPhoneNum(output->Destination_address);

                        ALOGD("decodeCallControlInfo, SMS Destination_address_type=%d, Destination_address: %s", output->Destination_address_type, output->Destination_address);
                    }else {
                        // Parsing Service Center address
                        SCA_Parsed = 1;
                        output->Service_center_address_type = outputByte[index++];
                        //Dialing Number String
                        int num_len = val_len - 1;  // Need to remove TON_NPI length
                        int SrvCntr_Size = (num_len * 2) + 1;
                        output->Service_center_address = (char *)malloc(SrvCntr_Size);
                        if (output->Service_center_address == NULL) {
                            ALOGE(LOG_TAG, "decodeCallControlInfo, Alloc memory for service center address failed.");
                            return 0;
                        }
                        memset(output->Service_center_address, 0, SrvCntr_Size);
                        asciiToHexRevByteOrder(outputByte + index, output->Service_center_address, num_len);
                        index += num_len;
                        removeInvalidPhoneNum(output->Service_center_address);

                        ALOGD("decodeCallControlInfo, SMS Service_center_address_type=%d, Service_center_address: %s", output->Service_center_address_type, output->Service_center_address);
                    }
                }
                break;

            /*  ALPHA_IDENTITY_TAG = Alpha id + Length + Alpha identifier */
            case (ICERA_RIL_STK_ALPHA_IDENTITY_TAG & ICERA_RIL_STK_COMMAND_TAG_MASK):
                // Length
                val_len = parseLen(&outputByte[index], &length_len);
                index += length_len;

                // Value
                output->Alpha_id = (char *)malloc(val_len + 1);
                if (output->Alpha_id == NULL) {
                    ALOGE(LOG_TAG, "decodeCallControlInfo, Alloc memory for Alpha_id failed");
                    return 0;
                }
                memset(output->Alpha_id, 0, val_len + 1);
                strncpy(output->Alpha_id, &outputByte[index], val_len);
                index += val_len;
                ALOGD("decodeCallControlInfo, Alpha_id: %s", output->Alpha_id);
                break;
            /*  USSD_STRING_TAG = USSD id + Length + DCS + USSD string */
            case (ICERA_RIL_STK_USSD_STRING & ICERA_RIL_STK_COMMAND_TAG_MASK):
                if (output->Type == ICERA_RIL_STKCTRLIND_TYPE_MO_CALL_CONTROL)
                    output->Type = ICERA_RIL_STKCTRLIND_TYPE_USSD_CALL_CONTROL;
                // Length
                val_len = parseLen(&outputByte[index], &length_len);
                index += length_len;

                // DCS
                output->Dcs = outputByte[index++];
                int data_len = ((val_len - 1) * 2) + 1;

                output->Data = (char *)malloc(data_len);
                if (output->Data == NULL) {
                    ALOGE(LOG_TAG, "decodeCallControlInfo, Alloc memory for Data failed");
                    return 0;
                }
                memset(output->Data, 0, data_len);
                memcpy(output->Data, &p_input[index*2], data_len - 1);
                index +=  val_len - 1;
                ALOGD("decodeCallControlInfo, Dcs = %d, Data: %s", output->Dcs, output->Data);
                break;
            /* SS_STRING_TAG = SS_id + length + ton_npi + SS */
            case (ICERA_RIL_STK_SS_STRING_TAG & ICERA_RIL_STK_COMMAND_TAG_MASK):
                if (output->Type == ICERA_RIL_STKCTRLIND_TYPE_MO_CALL_CONTROL)
                    output->Type = ICERA_RIL_STKCTRLIND_TYPE_SS_CALL_CONTROL;
                // Length
                val_len = parseLen(&outputByte[index], &length_len);
                index += length_len;

                // TON NPI, no need, just skip it.
                index ++ ; // skip ton_npi
                int ss_len = ((val_len - 1) * 2) + 1;

                // Data
                output->Data = (char *)malloc(ss_len);
                if (output->Data == NULL) {
                    ALOGE(LOG_TAG, "decodeCallControlInfo, Alloc memory for Data failed");
                    return 0;
                }
                memset(output->Data, 0, ss_len);
                asciiToHexRevByteOrder(&outputByte[index], output->Data, val_len - 1);
                index += val_len;
                ALOGD("decodeCallControlInfo, Data: %s", output->Data);
                break;

            default:
                ALOGD("Non-defined TAG: 0x%X at %d, skipped.", outputByte[index-1], index-1);
                val_len = outputByte[index++];
                index = index + val_len;
                break;
        }
        total_length = total_length - tag_len - length_len - val_len;
    }
    return 1;
}
