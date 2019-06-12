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


#include <stdio.h>
#include <string.h>
#include <telephony/ril.h>
#include <assert.h>
#include <ctype.h>
#include "atchannel.h"
#include "at_tok.h"
#include "misc.h"
#include <icera-ril.h>
#include <icera-ril-net.h>
#include "icera-util.h"
#include "icera-ril-logging.h"
#include <icera-ril-extensions.h>
#include <ril_request_object.h>

/*for GetNmbConcurentProfile */
#include <icera-ril-data.h>

#define LOG_TAG rilLogTag
#include <utils/Log.h>
#include <telephony/librilutils.h>
#include <icera-ril-utils.h>

#define countryCodeLength 3
#define networkCodeMinLength 2
#define networkCodeMaxLength 3

static int GetCurrentNetworkSelectionMode(void);
static void resetCachedOperator(void);

int CurrentTechnoCS = RADIO_TECH_UNKNOWN;
int CurrentTechnoPS = RADIO_TECH_UNKNOWN;

int collateNetworkList = 1;

static RIL_SignalStrength_v8 signalStrengthData;
static RIL_SignalStrength_v8 *signalStrength=&signalStrengthData;
static int ResponseSize = sizeof(RIL_SignalStrength_v8);

static int ActToTechno(int Act)
{
    switch(Act)
    {
        case 0:
        case 1:
        case 3:
            return TECH_2G;
            break;
        case 2:
        case 4:
        case 5:
        case 6:
            return TECH_3G;
            break;
        case 7:
            return TECH_4G;
            break;
        default:
            return 0;
    }
}

static char * TechnoToString(int Techno)
{
    switch(Techno)
    {
        case TECH_2G:
            return "2G";
        case TECH_3G:
            return "3G";
        case TECH_4G:
            return "4G";
        case TECH_2G|TECH_3G:
            return "2G/3G";
        case TECH_2G|TECH_4G:
            return "2G/4G";
        case TECH_3G|TECH_4G:
            return "3G/4G";
        case TECH_2G|TECH_3G|TECH_4G:
            return "2G/3G/4G";
        default:
            return "";
    }
}

typedef struct _copsstate{
    int valid;
    char* networkOp[3];
}opstate;

/* Cached network details */
nwstate NwState;
regstate Creg;
regstate Cgreg;
regstate Cereg;

static int cacheInvalid;
static opstate CopsState;

/* Global state variable */
static int LocUpdateEnabled = 1;
static int LteSupported;
static int NetworkSelectionMode;
static int FDAutoTimerEnabled = 1;


void setNetworkSelectionMode(int mode)
{
    NetworkSelectionMode = mode;
}

/**
 * RIL_REQUEST_SET_BAND_MODE
 *
 * Assign a specified band for RF configuration.
 *
 * "data" is int *
 * ((int *)data)[0] is == 0 for "unspecified" (selected by baseband automatically)
 * ((int *)data)[0] is == 1 for "EURO band" (GSM-900 / DCS-1800 / WCDMA-IMT-2000)
 * ((int *)data)[0] is == 2 for "US band" (GSM-850 / PCS-1900 / WCDMA-850 / WCDMA-PCS-1900)
 * ((int *)data)[0] is == 3 for "JPN band" (WCDMA-800 / WCDMA-IMT-2000)
 * ((int *)data)[0] is == 4 for "AUS band" (GSM-900 / DCS-1800 / WCDMA-850 / WCDMA-IMT-2000)
 * ((int *)data)[0] is == 5 for "AUS band 2" (GSM-900 / DCS-1800 / WCDMA-850)
 * ((int *)data)[0] is == 6 for "Cellular (800-MHz Band)"
 * ((int *)data)[0] is == 7 for "PCS (1900-MHz Band)"
 * ((int *)data)[0] is == 8 for "Band Class 3 (JTACS Band)"
 * ((int *)data)[0] is == 9 for "Band Class 4 (Korean PCS Band)"
 * ((int *)data)[0] is == 10 for "Band Class 5 (450-MHz Band)"
 * ((int *)data)[0] is == 11 for "Band Class 6 (2-GMHz IMT2000 Band)"
 * ((int *)data)[0] is == 12 for "Band Class 7 (Upper 700-MHz Band)"
 * ((int *)data)[0] is == 13 for "Band Class 8 (1800-MHz Band)"
 * ((int *)data)[0] is == 14 for "Band Class 9 (900-MHz Band)"
 * ((int *)data)[0] is == 15 for "Band Class 10 (Secondary 800-MHz Band)"
 * ((int *)data)[0] is == 16 for "Band Class 11 (400-MHz European PAMR Band)"
 * ((int *)data)[0] is == 17 for "Band Class 15 (AWS Band)"
 * ((int *)data)[0] is == 18 for "Band Class 16 (US 2.5-GHz Band)"
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */
void requestSetBandMode(void *data, size_t datalen, RIL_Token t)
{
    int bandMode = ((int *)data)[0];
    ATResponse *p_response = NULL;
    int err = 0;

    switch (bandMode) {

        /* "unspecified" (selected by baseband automatically) */
        case 0:
            /* All bands are activated except FDD BAND III (1700 UMTS-FDD band),
             * i.e. GSM 900, GSM 1800, GSM 1900, GSM 850, 2100 UMTS-FDD,
             * 1900 UMTS-FDD, 850 UMTS-FDD, 900 UMTS-FDD bans.
             */
            err = at_send_command("AT%IPBM=\"ANY\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;
            break;

        /* "EURO band" (GSM-900 / DCS-1800 / WCDMA-IMT-2000) */
        case 1:
            /* EGSM (GSM 900) and FDD BAND I (2100 UMTS-FDD/WCDMA-IMT-2000) set ON */
            err = at_send_command("AT%IPBM=\"ANY\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 1800 (DCS-1800) set ON */
            err = at_send_command("AT%IPBM=\"DCS\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;
            break;

        /* "US band" (GSM-850 / PCS-1900 / WCDMA-850 / WCDMA-PCS-1900) */
        case 2:
            /* EGSM (GSM 900) and FDD BAND I (2100 UMTS-FDD/WCDMA-IMT-2000) set ON */
            err = at_send_command("AT%IPBM=\"ANY\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 850 set ON */
            err = at_send_command("AT%IPBM=\"G850\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 1900 (PCS-1900) set ON */
            err = at_send_command("AT%IPBM=\"PCS\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* 850 UMTS-FDD (WCDMA-850) set ON */
            err = at_send_command("AT%IPBM=\"FDD_BAND_V\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* 1900 UMTS-FDD (WCDMA-PCS-1900) set ON */
            err = at_send_command("AT%IPBM=\"FDD_BAND_II\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 900 set OFF */
            err = at_send_command("AT%IPBM=\"EGSM\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* 2100 UMTS-FDD (WCDMA-IMT-2000) set OFF */
            err = at_send_command("AT%IPBM=\"FDD_BAND_I\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;
            break;

        /* "JPN band" (WCDMA-800 / WCDMA-IMT-2000) */
        case 3:
            /* ATTENTION: WCDMA-800 is not supported by ICERA modem */

            /* EGSM (GSM 900) and FDD BAND I (2100 UMTS-FDD/WCDMA-IMT-2000) set ON */
            err = at_send_command("AT%IPBM=\"ANY\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 900 set OFF */
            err = at_send_command("AT%IPBM=\"EGSM\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;
            break;

        /* "AUS band" (GSM-900 / DCS-1800 / WCDMA-850 / WCDMA-IMT-2000) */
        case 4:
            /* EGSM (GSM 900) and FDD BAND I (2100 UMTS-FDD/WCDMA-IMT-2000) set ON */
            err = at_send_command("AT%IPBM=\"ANY\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 1800 (DCS-1800) set ON */
            err = at_send_command("AT%IPBM=\"DCS\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* 850 UMTS-FDD (WCDMA-850) set ON */
            err = at_send_command("AT%IPBM=\"FDD_BAND_V\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;
            break;

        /* "AUS band 2" (GSM-900 / DCS-1800 / WCDMA-850) */
        case 5:
            /* EGSM (GSM 900) and FDD BAND I (2100 UMTS-FDD/WCDMA-IMT-2000) set ON */
            err = at_send_command("AT%IPBM=\"ANY\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 1800 (DCS-1800) set ON */
            err = at_send_command("AT%IPBM=\"DCS\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* 850 UMTS-FDD (WCDMA-850) set ON */
            err = at_send_command("AT%IPBM=\"FDD_BAND_V\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* 2100 UMTS-FDD (WCDMA-IMT-2000) set OFF */
            err = at_send_command("AT%IPBM=\"FDD_BAND_I\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;
            break;

        /* Cellular (800-MHz Band) */
        case 6:
            /* It is not supported by ICERA modem */
            goto not_supported;

        /* PCS (1900-MHz Band) */
        case 7:
            /* EGSM (GSM 900) and FDD BAND I (2100 UMTS-FDD/WCDMA-IMT-2000) set ON */
            err = at_send_command("AT%IPBM=\"ANY\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 1900 (PCS-1900) set ON */
            err = at_send_command("AT%IPBM=\"PCS\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 900 set OFF */
            err = at_send_command("AT%IPBM=\"EGSM\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* 2100 UMTS-FDD (WCDMA-IMT-2000) set OFF */
            err = at_send_command("AT%IPBM=\"FDD_BAND_I\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;
            break;

        /* Band Class 3 (JTACS Band) */
        case 8:
            /* It is not supported by ICERA modem */
            goto not_supported;

        /* Band Class 4 (Korean PCS Band) */
        case 9:
            /* It is not supported by ICERA modem */
            goto not_supported;

        /* Band Class 5 (450-MHz Band) */
        case 10:
            /* It is not supported by ICERA modem */
            goto not_supported;

        /* Band Class 6 (2-GMHz IMT2000 Band) */
        case 11:
            /* EGSM (GSM 900) and FDD BAND I (2100 UMTS-FDD/WCDMA-IMT-2000) set ON */
            err = at_send_command("AT%IPBM=\"ANY\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 900 set OFF */
            err = at_send_command("AT%IPBM=\"EGSM\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;
            break;

        /* Band Class 7 (Upper 700-MHz Band) */
        case 12:
            /* It is not supported by ICERA modem */
            goto not_supported;

        /* Band Class 8 (1800-MHz Band) */
        case 13:
            /* EGSM (GSM 900) and FDD BAND I (2100 UMTS-FDD/WCDMA-IMT-2000) set ON */
            err = at_send_command("AT%IPBM=\"ANY\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 1800 (DCS-1800) set ON */
            err = at_send_command("AT%IPBM=\"DCS\",1", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* GSM 900 set OFF */
            err = at_send_command("AT%IPBM=\"EGSM\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* 2100 UMTS-FDD (WCDMA-IMT-2000) set OFF */
            err = at_send_command("AT%IPBM=\"FDD_BAND_I\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;
            break;

        /* Band Class 9 (900-MHz Band) */
        case 14:
            /* EGSM (GSM 900) and FDD BAND I (2100 UMTS-FDD/WCDMA-IMT-2000) set ON */
            err = at_send_command("AT%IPBM=\"ANY\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;

            at_response_free(p_response);
            p_response = NULL;

            /* 2100 UMTS-FDD (WCDMA-IMT-2000) set OFF */
            err = at_send_command("AT%IPBM=\"FDD_BAND_I\",0", &p_response);
            if (err < 0 || p_response->success == 0) goto error;
            break;

        /* Band Class 10 (Secondary 800-MHz Band) */
        case 15:
            /* It is not supported by ICERA modem */
            goto not_supported;

        /* Band Class 11 (400-MHz European PAMR Band) */
        case 16:
            /* It is not supported by ICERA modem */
            goto not_supported;

        /* Band Class 15 (AWS Band) */
        case 17:
            /* It is not supported by ICERA modem */
            goto not_supported;

        /* Band Class 16 (US 2.5-GHz Band) */
        case 18:
            /* It is not supported by ICERA modem */
            goto not_supported;

        default:
            goto error;
    }

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

not_supported:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_MODE_NOT_SUPPORTED, NULL, 0);
    return;

error:
    at_response_free(p_response);
    ALOGE("ERROR: requestSetBandMode() failed\n");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/**
 * RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE
 *
 * Query the list of band mode supported by RF.
 *
 * See also: RIL_REQUEST_SET_BAND_MODE
 */
void requestQueryAvailableBandMode(void *data, size_t datalen, RIL_Token t)
{
    int err = 0;
    int i;
    int value[12];
    char *cmd = NULL;
    char *line, *c_skip;
    ATLine *nextATLine = NULL;
    ATResponse *p_response = NULL;

    // the stack  has to be powered down by the framework before the band mode is requested.

    err = at_send_command_multiline_no_prefix("AT%IPBM?", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    nextATLine = p_response->p_intermediates;
    line = nextATLine->line;

    for (i = 0 ; i < 12 ; i++ )
    {
        err = at_tok_nextColon(&line);
        if (err < 0)
            goto error;
        err = at_tok_nextint(&line, &value[i]);
        if (err < 0)
            goto error;
        if(nextATLine->p_next!=NULL)
        {
            nextATLine = nextATLine->p_next;
            line = nextATLine->line;
        }
    }

    if(value[0]==1) //Any Band
    {
        int response[2];

        response[0] = 2;
        response[1] = ANY; //Any Band supported
        RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
        goto IF_END;
    }else if((value[3]==1)||(value[4]==1)||(value[6]==1)||(value[9]==1))  //US Band
    {
        if((value[1]==1)||(value[2]==1)||(value[5]==1))  //EURO Band + US Band
        {
            int response[6];

            response[0] = 6;
            response[1] = EURO; //EURO Band supported
            response[2] = US;   //US Band supported
            response[3] = AUS;  //AUS band supported
            response[4] = AUS2; //AUS-2 band supported
            response[5] = JPN;  //JPN band supported
            RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
             goto IF_END;
        }else  //No EURO Band
        {
            if((value[5]==1)||(value[8]==1)) //JPN Band  + US Band
            {
                int response[5];

                response[0] = 5;
                response[1] = US;   //US Band supported
                response[2] = AUS;  //AUS band supported
                response[3] = AUS2; //AUS-2 band supported
                response[4] = JPN;  //JPN band supported
                RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
                goto IF_END;
            }else  //No JPN Band
            {
                int response[4];

                response[0] = 4;
                response[1] = US;   //US Band supported
                response[2] = AUS;  //AUS band supported
                response[3] = AUS2; //AUS-2 band supported
                RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
                goto IF_END;
            }
        }
    }else if((value[1]==1)||(value[2]==1)||(value[5]==1))
    { //No US Support at all
        int response[6];//check

        response[0] = 6;
        response[1] = EURO; //EURO Band supported
        response[3] = AUS;    //AUS band supported
        response[4] = AUS2; //AUS-2 band supported
        response[5] = JPN;    //JPN band supported
        RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    }
IF_END:
    // the stack  has to be powered up by the framework after the band mode was requested.

      at_response_free(p_response);
    return;

    error:
    at_response_free(p_response);
    ALOGE("requestQueryAvailableBandMode must never return error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return;
}

/**
 * RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC
 *
 * Specify that the network should be selected automatically
 *
 * "data" is NULL
 * "response" is NULL
 *
 * This request must not respond until the new operator is selected
 * and registered
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  ILLEGAL_SIM_OR_ME
 *  GENERIC_FAILURE
 *
 * Note: Returns ILLEGAL_SIM_OR_ME when the failure is permanent and
 *       no retries needed, such as illegal SIM or ME.
 *       Returns GENERIC_FAILURE for all other causes that might be
 *       fixed by retries.
 *
 */
void requestSetNetworkSelectionAutomatic(void *data, size_t datalen, RIL_Token t)
{
    int err = 0;

    /*
     * First query the current network selection mode
     * If already in aurtomatic, then re-applying automatic
     * will make the phone offline for some time
     */
    int currentSelMode = GetCurrentNetworkSelectionMode();

    if(currentSelMode!=0)
    {
        err = at_send_command("AT+COPS=0", NULL);
    }
    else
    {
        ALOGD("Network selection mode already automatic, ignoring. ");
    }

    if(err < 0)
    {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        /*
         * Not sure where we stand, will fetch it from the
         * modem on a per need basis
         */
        NetworkSelectionMode = -1;
    }
    else
    {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        NetworkSelectionMode = 0;
    }
    return;
}

/**
 * RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL
 *
 * Manually select a specified network.
 *
 * "data" is const char * specifying MCCMNC of network to select (eg "310170")
 * "response" is NULL
 *
 * This request must not respond until the new operator is selected
 * and registered
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  ILLEGAL_SIM_OR_ME
 *  GENERIC_FAILURE
 *
 * Note: Returns ILLEGAL_SIM_OR_ME when the failure is permanent and
 *       no retries needed, such as illegal SIM or ME.
 *       Returns GENERIC_FAILURE for all other causes that might be
 *       fixed by retries.
 *
 */
void requestSetNetworkSelectionManual(void *data, size_t datalen, RIL_Token t)
{
    int err = 0;
    int response = -1;
    char *cmd = NULL;
    const char *oper = (const char *) data;
    ATResponse *p_response = NULL;
    AT_CME_Error cme_error_num = -1;

    char *ptr = strstr(oper,"_");

    if(ptr != NULL) {
        *ptr = 0;
        ptr++;
    }

    //check oper length if valid
    if((strlen(oper) > (countryCodeLength + networkCodeMaxLength))
            ||(strlen(oper) < (countryCodeLength + networkCodeMinLength))){
        ALOGE("requestSetNetworkSelectionManual oper length invalid");
        goto error;
    }

    if(ptr) {
        asprintf(&cmd, "AT+COPS=1,2,\"%s\",%s", oper,ptr);
    } else {
        asprintf(&cmd, "AT+COPS=1,2,\"%s\"", oper);
    }

    err = at_send_command(cmd, &p_response);

    if (err < 0 || p_response->success == 0) goto error;

    free(cmd);
    NetworkSelectionMode = 1;
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    /* Not sure where we stand anymore, ask the modem again */
    NetworkSelectionMode = -1;
    // check CME Error for handling different error response
    if(p_response)
    {
        cme_error_num = at_get_cme_error(p_response);
        switch (cme_error_num)
        {
            case CME_SIM_NOT_INSERTED:
            case CME_ERROR_SIM_INVALID:
            case CME_ERROR_SIM_POWERED_DOWN:
            case CME_ERROR_ILLEGAL_MS:
            case CME_ERROR_ILLEGAL_ME:
                RIL_onRequestComplete(t, RIL_E_ILLEGAL_SIM_OR_ME, NULL, 0);
            break;

            default:
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        }
        rilExtOnNetworkRegistrationError(cme_error_num);
    }
    else
    {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }

    ALOGE("ERROR - requestSetNetworkSelectionManual() failed");

    free(cmd);
    at_response_free(p_response);
}


void requestQueryNetworkSelectionMode(
                void *data, size_t datalen, RIL_Token t)
{
    int response = GetCurrentNetworkSelectionMode();

    if(response >= 0)
    {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
    }
    else
    {
        ALOGE("requestQueryNetworkSelectionMode must never return error when radio is on");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

/**
 * RIL_REQUEST_QUERY_AVAILABLE_NETWORKS
 *
 * Scans for available networks
 *
 * "data" is NULL
 * "response" is const char ** that should be an array of n*4 strings, where
 *    n is the number of available networks
 * For each available network:
 *
 * ((const char **)response)[n+0] is long alpha ONS or EONS
 * ((const char **)response)[n+1] is short alpha ONS or EONS
 * ((const char **)response)[n+2] is 5 or 6 digit numeric code (MCC + MNC)
 * ((const char **)response)[n+3] is a string value of the status:
 *           "unknown"
 *           "available"
 *           "current"
 *           "forbidden"
 *
 * This request must not respond until the new operator is selected
 * and registered
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */
void requestQueryAvailableNetworks(void *data, size_t datalen, RIL_Token t)
{
    /* We expect an answer on the following form:
       +COPS: (2,"AT&T","AT&T","310410",0),(1,"T-Mobile ","TMO","310260",0)
     */

    int err, operators, i, skip, status;
    ATResponse *p_response = NULL;
    char * c_skip, *line, *p = NULL;
    char ** response = NULL;
    int AcT, *AccessTechno;
    char * LongAlpha, *ShortAlpha, * MccMnc, *StatusString;
    int j, found = 0, effectiveOperators = 0;

    err = at_send_command_singleline("AT+COPS=?", "+COPS:", &p_response);

    if (err != 0) goto error;

    if (at_get_cme_error(p_response) != CME_SUCCESS) goto error;

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    /* Count number of '(' in the +COPS response to get number of operators*/
    operators = 0;
    for (p = line ; *p != '\0' ;p++) {
        if (*p == '(') operators++;
    }

    response = (char **)alloca(operators * 4 * sizeof(char *));
    AccessTechno = (int*)alloca(operators * sizeof(int));

    for (i = 0 ; i < operators ; i++ )
    {
        err = at_tok_nextstr(&line, &c_skip);
        if (err < 0) goto error;
        if (strcmp(c_skip,"") == 0)
        {
            operators = i;
            continue;
        }
        status = atoi(&c_skip[1]);
        StatusString = (char*)networkStatusToRilString(status);

        err = at_tok_nextstr(&line, &LongAlpha);
        if (err < 0) goto error;

        err = at_tok_nextstr(&line, &ShortAlpha);
        if (err < 0) goto error;

        err = at_tok_nextstr(&line, &MccMnc);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &AcT);
        if (err < 0) goto error;

        if(collateNetworkList) {
            /*
             * Check if this operator is already in the list for another AcT
             * to avoid confusing doublons
             */
            for(j=0,found=0; (j<effectiveOperators) && (found == 0); j++)
            {

                if(strcmp(response[j*4+2],MccMnc) == 0)
                {
                    found++;
                    AccessTechno[j] |= ActToTechno(AcT);
                }
            }
        }

        if(!found)
        {
            response[effectiveOperators*4] = LongAlpha;
            response[effectiveOperators*4+1] = ShortAlpha;
            response[effectiveOperators*4+2] = MccMnc;
            response[effectiveOperators*4+3] = StatusString;
            AccessTechno[effectiveOperators] = ActToTechno(AcT);
            effectiveOperators++;
        }
    }

    /* Rewrite the Long Alpha to mention the techno */
    for(j=0; j<effectiveOperators; j++)
    {
        asprintf(&response[j*4], "%s (%s)",response[j*4], TechnoToString(AccessTechno[j]));
        if(collateNetworkList==0) {
            /* Identify the RAT in the numeric name too */
            asprintf(&response[j*4+2],"%s_%d",response[j*4+2],AccessTechno[j]);
        }
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, (effectiveOperators * 4 * sizeof(char *)));

    for(j=0; j<effectiveOperators; j++)
    {
        free(response[j*4]);
        if(collateNetworkList==0) {
            free(response[j*4+2]);
        }
    }

    at_response_free(p_response);
    return;

error:
    ALOGE("ERROR - requestQueryAvailableNetworks() failed");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    if (p_response)
        rilExtOnNetworkRegistrationError(at_get_cme_error(p_response));
    at_response_free(p_response);
}


/**
 * RIL_REQUEST_VOICE_REGISTRATION_STATE
 *
 * Request current registration state
 *
 * "data" is NULL
 * "response" is a "char **"
 * ((const char **)response)[0] is registration state 0-6,
 *              0 - Not registered, MT is not currently searching
 *                  a new operator to register
 *              1 - Registered, home network
 *              2 - Not registered, but MT is currently searching
 *                  a new operator to register
 *              3 - Registration denied
 *              4 - Unknown
 *              5 - Registered, roaming
 *             10 - Same as 0, but indicates that emergency calls
 *                  are enabled.
 *             12 - Same as 2, but indicates that emergency calls
 *                  are enabled.
 *             13 - Same as 3, but indicates that emergency calls
 *                  are enabled.
 *             14 - Same as 4, but indicates that emergency calls
 *                  are enabled.
 *
 * ((const char **)response)[1] is LAC if registered on a GSM/WCDMA system or
 *                              NULL if not.Valid LAC are 0x0000 - 0xffff
 * ((const char **)response)[2] is CID if registered on a * GSM/WCDMA or
 *                              NULL if not.
 *                                 Valid CID are 0x00000000 - 0xffffffff
 *                                    In GSM, CID is Cell ID (see TS 27.007)
 *                                            in 16 bits
 *                                    In UMTS, CID is UMTS Cell Identity
 *                                             (see TS 25.331) in 28 bits
 * ((const char **)response)[3] indicates the available voice radio technology,
 *                              valid values as defined by RIL_RadioTechnology.
 * ((const char **)response)[4] is Base Station ID if registered on a CDMA
 *                              system or NULL if not.  Base Station ID in
 *                              decimal format
 * ((const char **)response)[5] is Base Station latitude if registered on a
 *                              CDMA system or NULL if not. Base Station
 *                              latitude is a decimal number as specified in
 *                              3GPP2 C.S0005-A v6.0. It is represented in
 *                              units of 0.25 seconds and ranges from -1296000
 *                              to 1296000, both values inclusive (corresponding
 *                              to a range of -90 to +90 degrees).
 * ((const char **)response)[6] is Base Station longitude if registered on a
 *                              CDMA system or NULL if not. Base Station
 *                              longitude is a decimal number as specified in
 *                              3GPP2 C.S0005-A v6.0. It is represented in
 *                              units of 0.25 seconds and ranges from -2592000
 *                              to 2592000, both values inclusive (corresponding
 *                              to a range of -180 to +180 degrees).
 * ((const char **)response)[7] is concurrent services support indicator if
 *                              registered on a CDMA system 0-1.
 *                                   0 - Concurrent services not supported,
 *                                   1 - Concurrent services supported
 * ((const char **)response)[8] is System ID if registered on a CDMA system or
 *                              NULL if not. Valid System ID are 0 - 32767
 * ((const char **)response)[9] is Network ID if registered on a CDMA system or
 *                              NULL if not. Valid System ID are 0 - 65535
 * ((const char **)response)[10] is the TSB-58 Roaming Indicator if registered
 *                               on a CDMA or EVDO system or NULL if not. Valid values
 *                               are 0-255.
 * ((const char **)response)[11] indicates whether the current system is in the
 *                               PRL if registered on a CDMA or EVDO system or NULL if
 *                               not. 0=not in the PRL, 1=in the PRL
 * ((const char **)response)[12] is the default Roaming Indicator from the PRL,
 *                               if registered on a CDMA or EVDO system or NULL if not.
 *                               Valid values are 0-255.
 * ((const char **)response)[13] if registration state is 3 (Registration
 *                               denied) this is an enumerated reason why
 *                               registration was denied.  See 3GPP TS 24.008,
 *                               10.5.3.6 and Annex G.
 *                                 0 - General
 *                                 1 - Authentication Failure
 *                                 2 - IMSI unknown in HLR
 *                                 3 - Illegal MS
 *                                 4 - Illegal ME
 *                                 5 - PLMN not allowed
 *                                 6 - Location area not allowed
 *                                 7 - Roaming not allowed
 *                                 8 - No Suitable Cells in this Location Area
 *                                 9 - Network failure
 *                                10 - Persistent location update reject
 *                                11 - PLMN not allowed
 *                                12 - Location area not allowed
 *                                13 - Roaming not allowed in this Location Area
 *                                15 - No Suitable Cells in this Location Area
 *                                17 - Network Failure
 *                                20 - MAC Failure
 *                                21 - Sync Failure
 *                                22 - Congestion
 *                                23 - GSM Authentication unacceptable
 *                                25 - Not Authorized for this CSG
 *                                32 - Service option not supported
 *                                33 - Requested service option not subscribed
 *                                34 - Service option temporarily out of order
 *                                38 - Call cannot be identified
 *                                48-63 - Retry upon entry into a new cell
 *                                95 - Semantically incorrect message
 *                                96 - Invalid mandatory information
 *                                97 - Message type non-existent or not implemented
 *                                98 - Message not compatible with protocol state
 *                                99 - Information element non-existent or not implemented
 *                               100 - Conditional IE error
 *                               101 - Message not compatible with protocol state
 *                               111 - Protocol error, unspecified
 * ((const char **)response)[14] is the Primary Scrambling Code of the current
 *                               cell as described in TS 25.331, in hexadecimal
 *                               format, or NULL if unknown or not registered
 *                               to a UMTS network.
 *
 * Please note that registration state 4 ("unknown") is treated
 * as "out of service" in the Android telephony system
 *
 * Registration state 3 can be returned if Location Update Reject
 * (with cause 17 - Network Failure) is received repeatedly from the network,
 * to facilitate "managed roaming"
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */
/**
 * RIL_REQUEST_DATA_REGISTRATION_STATE
 *
 * Request current DATA registration state
 *
 * "data" is NULL
 * "response" is a "char **"
 * ((const char **)response)[0] is registration state 0-5 from TS 27.007 10.1.20 AT+CGREG
 * ((const char **)response)[1] is LAC if registered or NULL if not
 * ((const char **)response)[2] is CID if registered or NULL if not
 * ((const char **)response)[3] indicates the available data radio technology,
 *                              valid values as defined by RIL_RadioTechnology.
 * ((const char **)response)[4] if registration state is 3 (Registration
 *                               denied) this is an enumerated reason why
 *                               registration was denied.  See 3GPP TS 24.008,
 *                               Annex G.6 "Additonal cause codes for GMM".
 *      7 == GPRS services not allowed
 *      8 == GPRS services and non-GPRS services not allowed
 *      9 == MS identity cannot be derived by the network
 *      10 == Implicitly detached
 *      14 == GPRS services not allowed in this PLMN
 *      16 == MSC temporarily not reachable
 *      40 == No PDP context activated
 * ((const char **)response)[5] The maximum number of simultaneous Data Calls that can be
 *                              established using RIL_REQUEST_SETUP_DATA_CALL.
 *
 * The values at offsets 6..10 are optional LTE location information in decimal.
 * If a value is unknown that value may be NULL. If all values are NULL,
 * none need to be present.
 *  ((const char **)response)[6] is TAC, a 16-bit Tracking Area Code.
 *  ((const char **)response)[7] is CID, a 0-503 Physical Cell Identifier.
 *  ((const char **)response)[8] is ECI, a 28-bit E-UTRAN Cell Identifier.
 *  ((const char **)response)[9] is CSGID, a 27-bit Closed Subscriber Group Identity.
 *  ((const char **)response)[10] is TADV, a 6-bit timing advance value.
 *
 * LAC and CID are in hexadecimal format.
 * valid LAC are 0x0000 - 0xffff
 * valid CID are 0x00000000 - 0x0fffffff
 *
 * Please note that registration state 4 ("unknown") is treated
 * as "out of service" in the Android telephony system
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */
void requestRegistrationState(int request, void *data,
                                        size_t datalen, RIL_Token t)
{
    int err;
    unsigned int i;
    int Reg, Lac, Cid;
    char * responseStr[15];
    ATResponse *p_response = NULL;
    char *line, *p;
    int ConnectionState, CurrentTechno;

    memset(responseStr,(int)NULL,sizeof(responseStr));

    /* Size of the response, in char * */
    int ResponseSize = 4;

    if (request == RIL_REQUEST_VOICE_REGISTRATION_STATE) {
        Reg = Creg.Reg;
        Lac = Creg.Lac;
        Cid = Creg.Cid;
    } else if (request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        Reg = Cgreg.Reg;
        Lac = Cgreg.Lac;
        Cid = Cgreg.Cid;
    } else {
        assert(0);
        goto error;
    }

    if(Reg == -1)
        goto error;

    /*
     * Reports service on LTE radio technology
     * CS services (voice, SMS...) may be available on LTE via IMS
     * If no IMS service, those will be reported as restricted
     * PS services are available on LTE
     */
    if ((Reg!=1) && (Reg!=5))
    {
        /*
         * Return the information only if more
         * valuable than the current one
         */
        if((Cereg.Reg != 0) && (Cereg.Reg != 2))
        {
            Reg = Cereg.Reg;
            Cid = Cereg.Cid;
            Lac = Cereg.Lac;
        }
    }

    /*
     * Only fill the following 2 null initialized fields
     * if there is something relevant to report
     */
    if(Lac != -1)
    {
        asprintf(&responseStr[1], "%x", Lac);
    }

    if(Cid != -1)
    {
        asprintf(&responseStr[2], "%x", Cid);
    }

    /*
     * Workout if emergency calls are possible:
     * Rat indication but not registered
     */
    if((request == RIL_REQUEST_VOICE_REGISTRATION_STATE)&&
     ((strcmp(NwState.rat,"0")!=0)&&(IsRegistered()==0)))
    {
        /* Need to indicate that emergency calls are available */
        Reg+=REG_STATE_EMERGENCY;
    }
    asprintf(&responseStr[0], "%d", Reg);

    /* See if we can find an HPSA+ bearer */
    ConnectionState = accessTechToRat(NwState.connState, request);
    if (request == RIL_REQUEST_VOICE_REGISTRATION_STATE)
        CurrentTechno = CurrentTechnoCS;
    else
        CurrentTechno = CurrentTechnoPS;
    /*
     * Rat type are sorted, so this test is sufficient to report
     * the best supported RAT
     */
    if(ConnectionState > CurrentTechno)
        CurrentTechno = ConnectionState;

    asprintf(&responseStr[3], "%d", (int)CurrentTechno);

    /*
     * Ril v6 takes registration failure cause but the info is located
     * in different places depending on the request
     */

    if((Reg == REG_STATE_REJECTED)
     ||(Reg == (REG_STATE_REJECTED+REG_STATE_EMERGENCY)))
    {
        int NwRejCause=0, index;
        if(request == RIL_REQUEST_DATA_REGISTRATION_STATE)
        {
            err = at_send_command_singleline("AT%IER?", "%IER", &p_response);
        }
        else
        {
            err = at_send_command_singleline("AT%IVCER?", "%IVCER:", &p_response);
            ResponseSize = 14;
        }
        if((err < 0)||(p_response->success == 0))
            goto error;
        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &NwRejCause);

        /* No rejection cause given for CS, see if PS can supply one and return that */
        if((NwRejCause == 0) && (request != RIL_REQUEST_DATA_REGISTRATION_STATE)) {
            at_response_free(p_response);
            p_response = NULL;

            err = at_send_command_singleline("AT%IER?", "%IER", &p_response);

            if((err < 0)||(p_response->success == 0))
                goto error;

            line = p_response->p_intermediates->line;

            err = at_tok_start(&line);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &NwRejCause);
            if (err < 0) goto error;

            /* If there is still no usefull error, try to use the attach error */
            if(NwRejCause == 0) {
                err = at_tok_nextint(&line, &NwRejCause);
                if (err < 0) goto error;
            }
        }

        /*
         *  Framework is expecting 24.008 coded cause, so we can transmit as is,
         * note that the info is located in different places in each request
         */
        index = (request == RIL_REQUEST_DATA_REGISTRATION_STATE)?4:13;
        asprintf(&responseStr[index],"%d",NwRejCause);
    }

    /* Ril v6 takes #number of concurent profiles */
    if(request == RIL_REQUEST_DATA_REGISTRATION_STATE)
    {
        ResponseSize = 6;
        asprintf(&responseStr[5],"%d",GetNmbConcurentProfile());
    }

    /* RIL v8 LTE-specific data registration state additions (if available) */
    if ((request == RIL_REQUEST_DATA_REGISTRATION_STATE)
        && ((Cereg.Reg == 1) || (Cereg.Reg == 5))
        && ((Cereg.Lac != -1) || (Cereg.Cid != -1))
    )
    {
        ResponseSize = 11;
        /* [6] TAC */
        if (Cereg.Lac != -1)
            asprintf(&responseStr[6],"%d",Cereg.Lac);
        else
            responseStr[6] = NULL;
        /* [7] Physical Cell Identifier */
        responseStr[7] = NULL;
        /* [8] EUTRAN Cell Identifier */
        if (Cereg.Cid != -1)
            asprintf(&responseStr[8],"%d",Cereg.Cid);
        else
            responseStr[8] = NULL;
        /* [9] Closed Subscriber Group Identity */
        responseStr[9] = NULL;
        /* [10] Timing advance */
        responseStr[10] = NULL;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, ResponseSize*sizeof(char*));

    goto finally;
error:
    ALOGE("requestRegistrationState must never return an error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);

finally:
    /* Generic clean up */
    at_response_free(p_response);

    for(i =0; i<(sizeof(responseStr)/sizeof(char*)); i++)
    {
        free(responseStr[i]);
    }
}

/*
 * Returns the latest technology in use
 */

int GetCurrentTechno(void)
{
    if (CurrentTechnoPS != RADIO_TECH_UNKNOWN)
        return CurrentTechnoPS;
    else
        return CurrentTechnoCS;
}

static int available_modes[]={/*0*/ -1, /* Not supported */
                 /*1*/ PREF_NET_TYPE_GSM_ONLY,
                 /*2*/ PREF_NET_TYPE_WCDMA,
                 /*3*/ PREF_NET_TYPE_GSM_WCDMA_AUTO,
                 /*4*/ PREF_NET_TYPE_LTE_ONLY,
                 /*5*/ -1, /* Not supported 2G+4G*/
#if RIL_VERSION >= 9
                 /*6*/ PREF_NET_TYPE_LTE_WCDMA,
#else
                 /*6*/ -1,
#endif
                 /*7*/ PREF_NET_TYPE_LTE_GSM_WCDMA};


static int getModeFromRat(unsigned int ratBitmap) {
    if(ratBitmap > sizeof(available_modes)/sizeof(available_modes[0])) {
        return -1;
    }
    return available_modes[ratBitmap];
}

static int mode_to_rat[]={ /*PREF_NET_TYPE_GSM_WCDMA                */           TECH_3G | TECH_2G,
                           /*PREF_NET_TYPE_GSM_ONLY                 */                     TECH_2G,
                           /*PREF_NET_TYPE_WCDMA                    */           TECH_3G          ,
                           /*PREF_NET_TYPE_GSM_WCDMA_AUTO           */           TECH_3G | TECH_2G,
                           /*PREF_NET_TYPE_CDMA_EVDO_AUTO           */    /*Not supported*/     -1,
                           /*PREF_NET_TYPE_CDMA_ONLY                */    /*Not supported*/     -1,
                           /*PREF_NET_TYPE_EVDO_ONLY                */    /*Not supported*/     -1,
                           /*PREF_NET_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO */           TECH_3G | TECH_2G,
                           /*PREF_NET_TYPE_LTE_CDMA_EVDO            */ TECH_4G                    ,
                           /*PREF_NET_TYPE_LTE_GSM_WCDMA            */ TECH_4G | TECH_3G | TECH_2G,
                           /*PREF_NET_TYPE_LTE_CMDA_EVDO_GSM_WCDMA  */ TECH_4G | TECH_3G | TECH_2G,
                           /*PREF_NET_TYPE_LTE_ONLY                 */ TECH_4G                    ,
                           /*PREF_NET_TYPE_LTE_WCDMA                */ TECH_4G | TECH_3G          ,
};

static int getRatFromMode(int mode) {
    if(mode < 0 || mode > (int)(sizeof(mode_to_rat)/sizeof(mode_to_rat[0]))) {
        return -1;
    }
    return mode_to_rat[mode];
}

/**
 * RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE
 *
 * Query the preferred network type (CS/PS domain, RAT, and operation mode)
 * for searching and registering
 *
 * "data" is NULL
 *
 * "response" is int *
 * ((int *)reponse)[0] is == RIL_PreferredNetworkType
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 * See also: RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE
 */
/*
 * Returns the current rat mode (RIL encoding) after querying it from the modem.
 * returns -1 if it can't figure it out
 */
void GetCurrentNetworkMode(int* mode, int * supportedRat)
{
    int err, nwmode_option, prio;
    unsigned int supported_tech;
    ATResponse *p_response = NULL;
    char * line,*current_mode;

    *mode = -1;

    /*
     * Get the current mode
     */
    err = at_send_command_multiline ("AT%INWMODE?", "%INWMODE:", &p_response);

    if (err != 0 || p_response->success == 0) {
        ALOGE("Looks like %%nwmode is not supported by modem FW!");
        goto error;
    }
    /* only interrested in the first line. */
    if(p_response->p_intermediates == NULL)
    {
        ALOGE("Looks like %%nwmode has returned no information!");
        goto error;
    }
    line = p_response->p_intermediates->line;

    /* expected line: %INWMODE:0,<domain>,<prio> */

    /* skip prefix */
    err = at_tok_start(&line);
    if (err < 0)
        goto error;

    /* get current mode, has to be 0 on the first line */
    err = at_tok_nextint(&line, &nwmode_option);
    if ((err < 0)||(nwmode_option != 0))
        goto error;

    /* Get the RAT */
    err = at_tok_nextstr(&line, &current_mode);
    if (err < 0)
        goto error;
   /* get current prio */
    err = at_tok_nextint(&line, &prio);
    if (err < 0)
        goto error;

    supported_tech = ((strstr(current_mode,"G")!=NULL)?TECH_2G:0) +
                     ((strstr(current_mode,"U")!=NULL)?TECH_3G:0) +
                     ((strstr(current_mode,"E")!=NULL)?TECH_4G:0);

        *mode = getModeFromRat(supported_tech);
        if(*mode == -1)
            goto error;

        /* Check for the one case where we may have a priority set */
        if((*mode==PREF_NET_TYPE_GSM_WCDMA_AUTO)&&(prio!=0))
        {
            *mode = PREF_NET_TYPE_GSM_WCDMA;
        }

    /* Check the RAT with configured bands */
    if((supportedRat!=NULL)&&(p_response->p_intermediates->p_next!=NULL)) {
        *supportedRat = -1;
        line = p_response->p_intermediates->p_next->line;

        /* skip prefix */
        err = at_tok_start(&line);
        if (err < 0)
            goto error;

        /* Next line should be the available bands */
        err = at_tok_nextint(&line, &nwmode_option);
        if ((err < 0)||(nwmode_option != 1))
            goto error;

        /* Get the bandconfig */
        err = at_tok_nextstr(&line, &current_mode);
        if (err < 0)
            goto error;

        /* A RAT can be used if it has at least one of its bands enabled */
        *supportedRat = ((strstr(current_mode,"G")!=NULL)?TECH_2G:0) +
                         ((strstr(current_mode,"U")!=NULL)?TECH_3G:0) +
                         ((strstr(current_mode,"E")!=NULL)?TECH_4G:0);
    }
    error:
    at_response_free(p_response);
}

/**
 * RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE
 *
 * Query current network selectin mode
 *
 * "data" is NULL
 *
 * "response" is int *
 * ((const int *)response)[0] is
 *     0 for automatic selection
 *     1 for manual selection
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */
/*
 * Obtain the current network selection mode
 * return -1 if it can't be obtained.
 */
static int GetCurrentNetworkSelectionMode(void)
{
    int err;
    ATResponse *p_response = NULL;
    int response = -1;
    char *line;

    if(NetworkSelectionMode == -1)
    {

        err = at_send_command_singleline("AT+COPS?", "+COPS:", &p_response);

        if (err < 0 || p_response->success == 0) {
            goto error;
        }

        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);

        if (err < 0) {
            goto error;
        }

        err = at_tok_nextint(&line, &NetworkSelectionMode);
    }
    else
    {
        ALOGD("Providing cached network selection details");
    }


    response = NetworkSelectionMode;

error:
    if (p_response)
        rilExtOnNetworkRegistrationError(at_get_cme_error(p_response));
    at_response_free(p_response);
    return response;
}

/**
 * RIL_REQUEST_OPERATOR
 *
 * Request current operator ONS or EONS
 *
 * "data" is NULL
 * "response" is a "const char **"
 * ((const char **)response)[0] is long alpha ONS or EONS
 *                                  or NULL if unregistered
 *
 * ((const char **)response)[1] is short alpha ONS or EONS
 *                                  or NULL if unregistered
 * ((const char **)response)[2] is 5 or 6 digit numeric code (MCC + MNC)
 *                                  or NULL if unregistered
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */
void requestOperator(void *data, size_t datalen, RIL_Token t)
{
    int err;
    int i;
    int skip;
    ATLine *p_cur;
    char *response[3];
    ATResponse *p_response = NULL;

    memset(response, 0, sizeof(response));

    if(CopsState.valid == 0)
    {
        char* value = NULL;


        err = at_send_command_multiline(
            "AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?",
            "+COPS:", &p_response);

        /* we expect 3 lines here:
         * +COPS: 0,0,"T - Mobile"
         * +COPS: 0,1,"TMO"
         * +COPS: 0,2,"310170"
         */

        if ((err != 0) ||(p_response->success==0)) goto error;

        for (i = 0, p_cur = p_response->p_intermediates
                ; p_cur != NULL
                ; p_cur = p_cur->p_next, i++) {
            char *line = p_cur->line;

            err = at_tok_start(&line);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;

            // If we're unregistered, we may just get
            // a "+COPS: 0" response
            if (!at_tok_hasmore(&line)) {
                CopsState.networkOp[i] = NULL;
                continue;
            }

            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;

            // a "+COPS: 0, n" response is also possible
            if (!at_tok_hasmore(&line)) {
                CopsState.networkOp[i] = NULL;
                continue;
            }

            err = at_tok_nextstr(&line, &value);
            if (err < 0) goto error;

            CopsState.networkOp[i] = strdup(value);
        }

        if (i != 3) {
            /* expect 3 lines exactly */
            goto error;
        }
        CopsState.valid = 1;
    }
    else
    {
        ALOGD("Providing cached Operator details");
    }

    for(i=0;i<3;i++)
    {
        response[i] = CopsState.networkOp[i];
    }
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

    return;
error:
    ALOGE("requestOperator must not return error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    if (p_response)
        rilExtOnNetworkRegistrationError(at_get_cme_error(p_response));
    at_response_free(p_response);
}

/**
 * RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE
 *
 * Query the preferred network type (CS/PS domain, RAT, and operation mode)
 * for searching and registering
 *
 * "data" is NULL
 *
 * "response" is int *
 * ((int *)reponse)[0] is == RIL_PreferredNetworkType
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */
void requestSetPreferredNetworkType(void *data, size_t datalen, RIL_Token t)
{
    int err, requested_prio = 0, current_mode, supported_rat, rat_from_mode,
        Result = RIL_E_GENERIC_FAILURE, cfun_state = 0;
    char *line = NULL;
    char * cmd = NULL;
    char *requested_rat;
    ATResponse *p_response = NULL;

    assert (datalen >= sizeof(int *));
    int requested_mode = ((int *)data)[0];

    /* CDMA/EVDO modes aren't supported, return an error immediately */
    if((requested_mode >3) && (requested_mode < 7))
    {
        ALOGE("requestSetPreferredNetworkType: unsupported mode (%d)", requested_mode);
        Result = RIL_E_MODE_NOT_SUPPORTED;
        goto error;
    }

    /* Get the current mode from the modem to apply the change only if necessary */
    GetCurrentNetworkMode(&current_mode, &supported_rat);

    /*
     * Check if the modem can support the request, if not we need to transform the
     * request to something sensible.
     */
    rat_from_mode = getRatFromMode(requested_mode);
    if((rat_from_mode & supported_rat) != rat_from_mode) {
        /* Do a bitwise AND between the supported RAT and the requested ones */
        rat_from_mode = getModeFromRat(rat_from_mode & supported_rat);
    }

    if (current_mode != requested_mode)
    {
        switch (requested_mode)
        {
            case PREF_NET_TYPE_GSM_WCDMA:
                requested_rat = "UG";
                requested_prio = 1;
                break;
            case PREF_NET_TYPE_GSM_ONLY:
                requested_rat = "G";
                break;
            case PREF_NET_TYPE_WCDMA:
                requested_rat = "U";
                break;
            case PREF_NET_TYPE_GSM_WCDMA_AUTO:
            case PREF_NET_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO:
                requested_rat = "UG";
                break;
            case PREF_NET_TYPE_LTE_CDMA_EVDO:
            case PREF_NET_TYPE_LTE_ONLY:
                requested_rat = "E";
                break;
            case PREF_NET_TYPE_LTE_CMDA_EVDO_GSM_WCDMA:
            case PREF_NET_TYPE_LTE_GSM_WCDMA:
                requested_rat = "EUG";
                break;
#if RIL_VERSION >= 9
            case PREF_NET_TYPE_LTE_WCDMA:
                requested_rat = "EU";
                break;
#endif
            default: goto error; break; /* Should never happen */
        }

        asprintf(&cmd, "AT%%INWMODE=0,%s,%d", requested_rat, requested_prio);
        err = at_send_command(cmd, &p_response);
        free(cmd);

        if ((err != 0) ||(p_response->success==0))
            goto error;

        at_response_free(p_response);
    }
    else
    {
        ALOGD("Already in requested mode: No change");
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    ALOGE("ERROR: requestSetPreferredNetworkType() failed\n");
    at_response_free(p_response);
    RIL_onRequestComplete(t, Result, NULL, 0);
}

void requestGetPreferredNetworkType(void *data, size_t datalen, RIL_Token t)
{
    int current_mode;
    GetCurrentNetworkMode(&current_mode, NULL);
    if(current_mode != -1)
    {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &current_mode, sizeof(int));
        return;
    }
    else
    {
        ALOGE("ERROR: requestGetPreferredNetworkType() failed\n");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

/**
 * RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION
 *
 * Requests that network personlization be deactivated
 *
 * "data" is const char **
 * ((const char **)(data))[0]] is network depersonlization code
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
 *     (code is invalid)
 */
void requestEnterNetworkDepersonalization(void *data, size_t datalen,
                                          RIL_Token t)
{
    /*
     * AT+CLCK=<fac>,<mode>[,<passwd>[,<class>]]
     *     <fac>    = "PN" = Network Personalization (refer 3GPP TS 22.022)
     *     <mode>   = 0 = Unlock
     *     <passwd> = inparam from upper layer
     */

    int err = 0;
    char *cmd = NULL;
    ATResponse *p_response = NULL;
    const char *passwd = ((const char **) data)[0];
    RIL_Errno rilerr = RIL_E_GENERIC_FAILURE;
    int num_retries = -1;

    /* Check inparameter. */
    if (passwd == NULL) {
        goto error;
    }
    /* Build and send command. */
    asprintf(&cmd, "AT+CLCK=\"PN\",0,\"%s\"", passwd);
    err = at_send_command(cmd, &p_response);

    free(cmd);

    if (err < 0 || p_response->success == 0)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &num_retries, sizeof(int));
    at_response_free(p_response);
    return;

error:
    if (p_response && at_get_cme_error(p_response) == CME_ERROR_INCORRECT_PASSWORD) {
        char *line;
        rilerr = RIL_E_PASSWORD_INCORRECT;

        /* Get the number of remaining attempts */
        at_response_free(p_response);
        p_response = NULL;
        err = at_send_command_singleline("AT%ILOCKNUM=\"PN\",2","%ILOCKNUM:", &p_response);

        if (!(err < 0 || p_response->success == 0))
        {
            line = p_response->p_intermediates->line;
            err = at_tok_start(&line);
            if (err >= 0)
                err = at_tok_nextint(&line, &num_retries);
            if(err <0)
                rilerr = RIL_E_GENERIC_FAILURE;
        }
    } else {
        ALOGE("ERROR: requestEnterNetworkDepersonalization failed\n");
    }
    at_response_free(p_response);
    RIL_onRequestComplete(t,
                                                     rilerr,
                                                     (rilerr==RIL_E_PASSWORD_INCORRECT)?&num_retries:NULL,
                                                     (rilerr==RIL_E_PASSWORD_INCORRECT)?sizeof(int):0);
}

/**
 * RIL_REQUEST_SET_LOCATION_UPDATES
 *
 * Enables/disables network state change notifications due to changes in
 * LAC and/or CID (for GSM) or BID/SID/NID/latitude/longitude (for CDMA).
 * Basically +CREG=2 vs. +CREG=1 (TS 27.007).
 *
 * Note:  The RIL implementation should default to "updates enabled"
 * when the screen is on and "updates disabled" when the screen is off.
 *
 * "data" is int *
 * ((int *)data)[0] is == 1 for updates enabled (+CREG=2)
 * ((int *)data)[0] is == 0 for updates disabled (+CREG=1)
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 * See also: RIL_REQUEST_SCREEN_STATE, RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED
 */
void requestSetLocationUpdates(void *data, size_t datalen, RIL_Token t)
{
    int err = 0;
    char *cmd = NULL;
    ATResponse *p_response = NULL;

    /* Update the global for future screen_state transitions */
    LocUpdateEnabled = ((int *)data)[0];

    if(LocUpdateEnabled)
    {
        at_send_command(LteSupported?"AT+CREG=2;+CGREG=2;+CEREG=2":"AT+CREG=2;+CGREG=2",&p_response);
    }
    else
    {
        at_send_command(LteSupported?"AT+CREG=1;+CGREG=1;+CEREG=1":"AT+CREG=1;+CGREG=1",&p_response);
    }

    if(err < 0 || p_response->success == 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    ALOGE("ERROR: requestSetLocationUpdates failed\n");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    if (p_response)
        rilExtOnNetworkRegistrationError(at_get_cme_error(p_response));
    at_response_free(p_response);
}

/**
 * RIL_REQUEST_GET_NEIGHBORING_CELL_IDS
 *
 * Request neighboring cell id in GSM network
 *
 * "data" is NULL
 * "response" must be a " const RIL_NeighboringCell** "
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */
void requestGetNeighboringCellIds(void *data, size_t datalen, RIL_Token t)
{
    int err = 0;
    ATResponse *p_response = NULL;
    ATLine *p_cur;
    RIL_NeighboringCell **all_pp_cells = NULL;
    RIL_NeighboringCell **pp_cells;
    RIL_NeighboringCell *p_cells;
    char *line;
    int type;
    int all_cells_number = 0;
    int cells_number;
    int cell_id;
    int i;

    err = at_send_command_multiline("AT%INCELL", "%INCELL:", &p_response);

    if (err != 0 || p_response->success == 0) goto error;

    /* %INCELL: <type>,<nbCells>,<cellId1>,<SignalLevel1>,...,<cellIdN>,<SignalLevelN> */

    for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next) {
        line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &type);
        if (err < 0) goto error;

        if (type == 2) continue; /* to ignore interRat GSM cells */

        err = at_tok_nextint(&line, &cells_number);
        if (err < 0) goto error;

        if (cells_number > 0) {
            p_cells = (RIL_NeighboringCell *)alloca(cells_number * sizeof(RIL_NeighboringCell));
            pp_cells = (RIL_NeighboringCell **)alloca((all_cells_number + cells_number) * sizeof(RIL_NeighboringCell *));

            if (all_cells_number > 0) {
                memcpy(pp_cells, all_pp_cells, all_cells_number * sizeof(RIL_NeighboringCell *));
            }

            for (i = 0; i < cells_number; i++) {
                err = at_tok_nextint(&line, &cell_id);
                if (err < 0) goto error;

                asprintf(&p_cells[i].cid, "%x", cell_id);

                err = at_tok_nextint(&line, &p_cells[i].rssi);
                if (err < 0) goto error;

                pp_cells[all_cells_number + i] = &p_cells[i];
            }

            all_cells_number += cells_number;
            all_pp_cells = pp_cells;
        }
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, all_pp_cells, all_cells_number * sizeof(RIL_NeighboringCell *));
    goto finally;

error:

    ALOGE("ERROR: requestGetNeighboringCellIds failed\n");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);

finally:
    at_response_free(p_response);
    while (all_cells_number>0)
    {
        all_cells_number --;
        free(all_pp_cells[all_cells_number]->cid);
    }
}

/**
 * RIL_UNSOL_RESTRICTED_STATE_CHANGED
 *
 * Indicates a restricted state change (eg, for Domain Specific
 * Access Control).
 *
 * Radio need send this msg after radio off/on cycle no matter
 * it is changed or not.
 *
 * "data" is an int *
 * ((int *)data)[0] contains a bitmask of RIL_RESTRICTED_STATE_*
 * values.
 */
void unsolRestrictedStateChanged(const char *s)
{
    char *p, *line;
    int response = 0, err;

    line = strdup(s);
    p = line;

    /* %INWRESTRICT: <restriction state (hex)> */
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nexthexint(&line, &response);
    if (err < 0) goto error;

    /* Report no restriction if result is invalid/unknown */
    if ((unsigned int) response == 0xFFFFFFFF)
        response = 0;

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESTRICTED_STATE_CHANGED,
                                             &response, sizeof(response));
error:
    free(p);
}


/**
 * RIL_UNSOL_NITZ_TIME_RECEIVED
 *
 * Called when radio has received a NITZ time message.
 *
 * "data" is const char * pointing to NITZ time string
 * in the form "yy/mm/dd,hh:mm:ss(+/-)tz,dt"
*/
void unsolNetworkTimeReceived(const char *data)
{
    int err;
    ATResponse *p_response = NULL;
    char *line = (char *)data;

    /* expected line: *TLTS: "yy/mm/dd,hh:mm:ss+/-tz", without "dt" */

    /* skip prefix */
    err = at_tok_start(&line);
    if (err < 0)
        goto error;

    /* Remove spaces and " around the useful string*/
    err =  at_tok_nextstr(&line, &line);
    if (err < 0)
        goto error;

    /* Skip the report to Android if the string is empty (meaning that the modem has no NITZ info available)*/
    if(strcmp(line,"") != 0)
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_NITZ_TIME_RECEIVED,
                                  line, strlen(line));
    }

    /* Queue an update of the modem time in a little while */
    void *req = (void *)ril_request_object_create(IRIL_REQUEST_ID_RTC_UPDATE, NULL, 0, 0);
    if(req != NULL)
    {
        postDelayedRequest(req, 1);
    }

    return;
error:
    ALOGE("ERROR: unsolNetworkTimeReceived - invalid time string\n");
    return;
}

void requestSignalStrength(void *data, size_t datalen, RIL_Token t)
{
    if(t)
    {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, signalStrength, ResponseSize);
    }
    else
    {
        RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, signalStrength, ResponseSize);
    }
}

void unsolSignalStrength(const char *s)
{
    char * p;
    int err;
    int rssi, ber;
    char *line;
    /*
     * Cache the values so they don't need to be requested from the modem again
     */
    line = strdup(s);
    p = line;
    memset(signalStrength,-1,ResponseSize);

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &rssi);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &ber);
    if (err < 0) goto error;

    switch(GetCurrentTechno())
    {
#ifdef LTE_SPECIFIC_SIGNAL_REPORT
        /*
         * As of JB MR1, LTE signal level reporting is broken in the framework
         * unless you can report rsnr or rsrp. LTE rrsi is ignored. Defaulting
         * to GSM level should work just fine.
         */
        case RADIO_TECH_LTE:
            signalStrength->GW_SignalStrength.signalStrength = 99;
            signalStrength->GW_SignalStrength.bitErrorRate = 0;
            signalStrength->LTE_SignalStrength.cqi = 0;
            signalStrength->LTE_SignalStrength.rsrp = 0;
            signalStrength->LTE_SignalStrength.rsrq = 0;
            signalStrength->LTE_SignalStrength.rssnr = 0;
            signalStrength->LTE_SignalStrength.signalStrength = rssi;
            break;
#endif
        default:
            signalStrength->GW_SignalStrength.signalStrength = rssi;
            signalStrength->GW_SignalStrength.bitErrorRate = ber;
            signalStrength->LTE_SignalStrength.cqi = 0;
            signalStrength->LTE_SignalStrength.rsrp = -140;
            signalStrength->LTE_SignalStrength.rsrq = 20;
            signalStrength->LTE_SignalStrength.rssnr = -200;
            signalStrength->LTE_SignalStrength.signalStrength = 99;
            break;
    }

    /* Send unsolicited to framework */
    requestSignalStrength(NULL, 0, 0);

    error:
    free(p);
}


void unsolSignalStrengthExtended(const char *s)
{
    char * p;
    int err;
    int rxlev, ber, rscp, ecno, rsrq, rsrp, rssi, rssnr;
    char *line;

    /*
     * Cache the values so they don't need to be requested from the modem again
     */
    line = strdup(s);
    p = line;
    /* Invalidate CDMA and EVDO: valid values are positive integer */
    memset(signalStrength, INT_MIN,ResponseSize);
    /* Invalidate GSM/WCDMA */
    signalStrength->GW_SignalStrength.signalStrength  = 99;
    signalStrength->GW_SignalStrength.bitErrorRate    = 99;
    /* Invalidate LTE */
    signalStrength->LTE_SignalStrength.signalStrength = 99;
    signalStrength->LTE_SignalStrength.rsrp           = INT_MAX;
    signalStrength->LTE_SignalStrength.rsrq           = INT_MAX;
    signalStrength->LTE_SignalStrength.rssnr          = INT_MAX;
    signalStrength->LTE_SignalStrength.cqi            = INT_MAX;
    signalStrength->LTE_SignalStrength.timingAdvance  = INT_MAX;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &rxlev);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &ber);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &rscp);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &ecno);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &rsrq);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &rsrp);
    if (err < 0) goto error;

    if (rxlev != 99)
    {
        /*
         * 2G - report rxlev as signal strength
         * signalStrength: 2-dB step, relative to -113 dBm [0..63]
         * bitErrorRate: RXQUAL [0..7]
         * CESQ rxlev: dB step, relative to -110 dBm
         * CESQ ber: RXQUAL [0..7]
         */

        /*
         * rxlev in dB unit, relative to -110 dBm
         * convert to rssi: 2dB unit, relative to -113 dBm
         */
        rssi = (rxlev + (113 - 110))/2;
        if (rssi > 63)
        {
            signalStrength->GW_SignalStrength.signalStrength = 63;
        }
        else
        {
            signalStrength->GW_SignalStrength.signalStrength = rssi;
        }

        if (ber <= 7)
        {
            signalStrength->GW_SignalStrength.bitErrorRate = ber;
        }
    }
    else if (rscp != 255)
    {
        /*
         * 3G - report rscp as signal strength
         * signalStrength: 2 dB step, relative to -113 dBm [0..63]
         * CESQ rscp: 1 dB step, relative to -120 dBm [0..96]
         * CESQ ecno: half dB step relative to -24 dB [0..49]
         */

        /*
         * rscp in dB unit, relative to -120 dBm
         * convert to rssi: 2dB unit, relative to -113 dBm
         */
        rssi = (rscp - (120 - 113))/2;
        if (rssi < 0)
        {
            rssi = 0;
        }
        else if (rssi > 63)
        {
            rssi = 63;
        }
        signalStrength->GW_SignalStrength.signalStrength = rssi;
    }
    else if (rsrp != 255)
    {
        ATResponse *p_response;
        ATLine *p_cur;

        /*
         * 4G - report rsrp as signal strength
         * RSSI ~ RSRP
         * signalStrength: 2 dB step, relative to -113 dBm [0..63]
         * rsrp: Reference Signal Receive Power in dBm multipled by -1
         *       44 to 140 dBm
         * rsrq: Reference Signal Receive Quality in dB multiplied by -1
         *       Range: 20 to 3 dB
         * rssnr: reference signal signal-to-noise ratio in 0.1 dB units
         *       Range: -200 to +300 (-200 = -20.0 dB, +300 = 30dB)
         * cqi: Not available
         * CESQ rsrq : half-dB step relative to -19.5 dB [0..34]
         * CESQ rsrp: dB step, relative to -140 dBm [0..97]
         * ICSQ rssnr: signed value, 0.1 dB unit
         */

        /*
         * rsrp in dB unit, relative to -140 dBm
         * convert to rssi: 2dB unit, relative to -113 dBm
         */
        rssi = (rsrp - (140 - 113))/2;
        if (rssi < 0)
        {
            rssi = 0;
        }
        else if (rssi > 63)
        {
            rssi = 63;
        }
        signalStrength->LTE_SignalStrength.signalStrength = rssi;

        /*
         * rsrp dB unit, relative to -140 dBm
         * convert -44 to 140 dBm range to to [44..140]
         */
        rsrp = (140 - rsrp);
        if (rsrp < 0)
        {
            rsrp = 0;
        }
        else if (rsrp > 140)
        {
            rsrp = 140;
        }
        signalStrength->LTE_SignalStrength.rsrp = rsrp;

        /*
         * rsrq 1/2 dB unit, relative to -19.5 dB
         * convert -3 to 20 dB range to [3..20]
         */
        if (rsrq <= 34)
        {
            signalStrength->LTE_SignalStrength.rsrq = (40 - rsrq)/2;
        }

        /*
         * rssnr may be available from %ICSQ
         * %ICSQ rssnr is signed value in cB
         * Matches unit and format of reported rssnr, range [-200..300]
         */

        rssnr = INT_MAX;
        p_response = NULL;
        {
            /* Query %ICSQ - search for "%ICSQ: LTE-RSSNR= <rssnr> cB" */
            err = at_send_command_multiline ("AT%ICSQ", "%ICSQ:", &p_response);
            if (err == 0 && p_response->success != 0) {
                for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next) {
                    char *line = p_cur->line;
                    char *token;

                    err = at_tok_start(&line);
                    if (err < 0)
                        goto rssnr_end;

                    while ((*line != '\0') && isspace(*line)) {
                        line++;
                    }
                    token = strsep(&line, " ");
                    if (token == NULL)
                        goto rssnr_end;

                    if (strcmp(token, "LTE-RSSNR=") != 0)
                        continue;

                    while ((*line != '\0') && isspace(*line)) {
                        line++;
                    }
                    token = strsep(&line, " ");
                    if (token == NULL)
                        goto rssnr_end;

                    rssnr = strtol(token, NULL, 10);

                    if (rssnr > 300)
                    {
                        rssnr = 300;
                    }
                    else if (rssnr < -200)
                    {
                        rssnr = -200;
                    }
                }
            }
        }
rssnr_end:
        at_response_free(p_response);
        signalStrength->LTE_SignalStrength.rssnr = rssnr;
    }
    else
    {
        ALOGI("Invalid/empty measurement report");
    }
    /* Send unsolicited to framework */
    requestSignalStrength(NULL, 0, 0);

error:
    free(p);
}


const char* networkStatusToRilString(int state)
{
    switch(state){
        case 0: return("unknown");   break;
        case 1: return("available"); break;
        case 2: return("current");   break;
        case 3: return("forbidden"); break;
        default: return NULL;
    }
}

void parsePlusCxREG(char *line, int *p_RegState, int* p_Lac,int * p_CId, int *p_Rac)
{
    int Lac= -1, CId =-1, RegState = -1, Rac = -1, skip, err, commas, unsol;
    char * p;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    /*
     * The solicited version of the CREG response is
     * +CREG: <n>, <stat>[,<lac>,<ci>[,<AcT>]]
     * and the unsolicited version is
     * +CREG: <stat>[,<lac>,<ci>[,<AcT>]]
     * The <n> parameter is basically "is unsolicited creg on?"
     * which it should always be
     *
     * Also since the LAC and CID are only reported when registered,
     * we can have 1 to 5 arguments here
     *
     * a +CGREG: answer may have a sixth value that corresponds
     * to the RAC:
     *  +CGREG: <stat>[,<lac>,<ci>[,<AcT>,<rac>]]
     *
     *  +CEREG is same as +CREG, though <tac> is used in place of <lac>
     *
     *  <Act>  network type is not taken from the +CxREG response, but taken
     *  from the AT%NWSTATE
     */

    /* count number of commas
     * and determine if it is sollicited or not
     * lac parameter comes surrounded by " " - Check its position
     * after 1st comma means unsollicited otherwise it is a sollicited format
     */

    commas = 0;
    unsol = 0;
    for (p = line ; *p != '\0' ;p++) {
        if (*p == ',') commas++;
        if((commas == 1)&&(*p=='"')) unsol=1;
    }

    switch (commas) {
        /* Unsolicited, no locupdate */
        case 0: /* +CxREG: <stat> */
            err = at_tok_nextint(&line, &RegState);
            if (err < 0) goto error;
        break;

        /* Solicited */
        case 1: /* +CxREG: <n>, <stat> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &RegState);
            if (err < 0) goto error;
        break;

        /*
         * Long format response, solicited has heading <n>
         *  +CREG: [<n>],<stat>[,<lac>,<ci>[,<AcT>]]
         * +CGREG: [<n>],<stat>[,<lac>,<ci>[,<AcT>,<rac>]]
         * +CEREG: [<n>],<stat>[,<tac>,<ci>[,<AcT>]]
         */
        default:
            if(!unsol)
            {
                err = at_tok_nextint(&line, &skip);
                if (err < 0) goto error;
            }
            err = at_tok_nextint(&line, &RegState);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &Lac);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &CId);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &Rac);
            if (err < 0) goto error;
        break;
    }

    error:
    if(p_RegState!=NULL)
        *p_RegState = RegState;
    if(p_Lac!=NULL)
        *p_Lac = Lac;
    if(p_CId != NULL)
        *p_CId = CId;
    if(p_Rac != NULL)
        *p_Rac = Rac;
}

int accessTechToRat(char *rat, int request)
{
    unsigned int i;
    typedef struct _RatDesc{
        const char * Rat;
        const int PsRadioTech;
        const int CsRadioTech;
    }RatDesc;

    const RatDesc RatDescList[]=
    /* Rat,             PS eq,              CS eq */
    {
    {"0",               RADIO_TECH_UNKNOWN, RADIO_TECH_UNKNOWN},
    {"2g-gprs",         RADIO_TECH_UNKNOWN, RADIO_TECH_GPRS},
    {"2g-edge",         RADIO_TECH_UNKNOWN, RADIO_TECH_EDGE},
    {"(2G-GPRS)",       RADIO_TECH_GPRS,    RADIO_TECH_UNKNOWN},
    {"(2G-EDGE)",       RADIO_TECH_EDGE,    RADIO_TECH_UNKNOWN},
    {"2G-GPRS",         RADIO_TECH_GPRS,    RADIO_TECH_GPRS},
    {"GPRS",            RADIO_TECH_GPRS,    RADIO_TECH_GPRS},
    {"2G-EDGE",         RADIO_TECH_EDGE,    RADIO_TECH_EDGE},
    {"EDGE",            RADIO_TECH_EDGE,    RADIO_TECH_EDGE},
    {"3g",              RADIO_TECH_UNKNOWN, RADIO_TECH_UMTS},
    {"3g-hsdpa",        RADIO_TECH_UNKNOWN, RADIO_TECH_HSDPA},
    {"3g-hsupa",        RADIO_TECH_UNKNOWN, RADIO_TECH_HSUPA},
    {"3g-hsdpa-hsupa",  RADIO_TECH_UNKNOWN, RADIO_TECH_HSPA},
    {"(3G)",            RADIO_TECH_UMTS,    RADIO_TECH_UNKNOWN},
    {"(3G-HSDPA)",      RADIO_TECH_HSDPA,   RADIO_TECH_UNKNOWN},
    {"(3G-HSUPA)",      RADIO_TECH_HSUPA,   RADIO_TECH_UNKNOWN},
    {"(3G-HSDPA-HSUPA)",RADIO_TECH_HSPA,    RADIO_TECH_UNKNOWN},
    {"3G",              RADIO_TECH_UMTS,    RADIO_TECH_UMTS},
    {"3G-HSDPA",        RADIO_TECH_HSDPA,   RADIO_TECH_HSDPA},
    {"HSDPA",           RADIO_TECH_HSDPA,   RADIO_TECH_HSDPA},
    {"3G-HSUPA",        RADIO_TECH_HSUPA,   RADIO_TECH_HSUPA},
    {"HSUPA",           RADIO_TECH_HSUPA,   RADIO_TECH_HSUPA},
    {"3G-HSDPA-HSUPA",  RADIO_TECH_HSPA,    RADIO_TECH_HSPA},
    {"HSDPA-HSUPA",     RADIO_TECH_HSPA,    RADIO_TECH_HSPA},
    {"lte",             RADIO_TECH_UNKNOWN, RADIO_TECH_LTE},
    {"LTE",             RADIO_TECH_LTE,     RADIO_TECH_LTE},
    {"2g",              RADIO_TECH_UNKNOWN, RADIO_TECH_GSM},
    {"HSDPA-HSUPA-HSPA+",RADIO_TECH_HSPAP,  RADIO_TECH_HSPAP},
    {"DC-HSDPA",        RADIO_TECH_HSPAP,   RADIO_TECH_HSPAP},
    };

    for(i=0; i<sizeof(RatDescList)/sizeof(RatDesc); i++)
    {
        if(strcmp(rat,RatDescList[i].Rat)==0)
        {
            return (request==RIL_REQUEST_VOICE_REGISTRATION_STATE)?
                RatDescList[i].CsRadioTech
                :RatDescList[i].PsRadioTech;
        }
    }
    return RADIO_TECH_UNKNOWN;
}

void parseCreg(const char *s)
{
    int Reg, Lac, Cid, OldReg;
    char * line = (char*) s;

    parsePlusCxREG(line, &Reg, &Lac, &Cid, NULL);

    OldReg = Creg.Reg;

    if((Reg != Creg.Reg)||(Lac != Creg.Lac)||(Cid != Creg.Cid))
    {
        Creg.Reg = Reg;
        Creg.Lac = Lac;
        Creg.Cid = Cid;

        /* Notify framework */
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
            NULL, 0);
        /* We'll refresh the operator next time we're asked for it */
        resetCachedOperator();

        /*
         * if network status switch from registered to non-registered, indicate network lost
         * but filter out the network lost report due to radio being turned off.
         * This is only affecting logging and debugging tools.
         */
        if((currentState()!=RADIO_STATE_OFF)&&(Reg != 1)&&(Reg != 5)&&
          ((OldReg == 1)||(OldReg == 5)))
        {
            const char* str="Network lost\n";
            ALOGE("Network lost\n");
            SetModemLoggingEvent(MODEM_LOGGING, str, strlen(str));
        }
    }
}

void parseCgreg(const char *s)
{
    int Reg, Lac, Cid, Rac, OldReg;
    char * line = (char*) s;

    parsePlusCxREG(line, &Reg, &Lac, &Cid, &Rac);

    OldReg = Cgreg.Reg;

    if((Reg != Cgreg.Reg)||(Lac != Cgreg.Lac)||(Cid != Cgreg.Cid)||(Rac != Cgreg.Rac))
    {
        Cgreg.Reg = Reg;
        Cgreg.Lac = Lac;
        Cgreg.Cid = Cid;
        Cgreg.Rac = Rac;

        /* We'll refresh the operator next time we're asked for it */
        resetCachedOperator();

        /* If not searching, update the restricted state */
        if((Reg != OldReg)&&(Reg!=2))
        {
            /* Notify framework */
            RIL_onUnsolicitedResponse (
                RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
                NULL, 0);
        }
    }
}

void parseCereg(const char *s)
{
    int Reg, Lac, Cid, OldReg;
    char * line = (char*) s;

    parsePlusCxREG(line, &Reg, &Lac, &Cid, NULL);

    OldReg = Cereg.Reg;

    if((Reg != Cereg.Reg)||(Lac != Cereg.Lac)||(Cid != Cereg.Cid))
    {
        Cereg.Reg = Reg;
        Cereg.Lac = Lac;
        Cereg.Cid = Cid;

        /* We'll refresh the operator next time we're asked for it */
        resetCachedOperator();

        /* If not searching, update the restricted state */
        if((Reg != OldReg)&&(Reg!=2))
        {
            /* Notify framework */
            RIL_onUnsolicitedResponse (
                RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
                NULL, 0);
        }
    }
}

void parseNwstate(const char *s)
{
    int err;
    int emergencyOrSearching, mccMnc;
    char *rat, *connState;
    char * line = strdup((char*)s);
    char *p = line;
    err = at_tok_start(&line);

    err = at_tok_nextint(&line, &emergencyOrSearching);
    if (err < 0) goto error;
    err = at_tok_nextint(&line, &mccMnc);
    if (err < 0) goto error;
    err = at_tok_nextstr(&line, &rat);
    if (err < 0) goto error;
    err = at_tok_nextstr(&line, &connState);
    if (err < 0) goto error;

    if((emergencyOrSearching != NwState.emergencyOrSearching)||
      (mccMnc != NwState.mccMnc)||
      (strcmp(rat, NwState.rat))||
      (strcmp(connState,NwState.connState)))
    {
        /* Notify a change of network status */
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
            NULL, 0);

        /* Cache the info, Framework will request them later */
        free(NwState.connState);
        free(NwState.rat);

        NwState.emergencyOrSearching = emergencyOrSearching;
        NwState.mccMnc = mccMnc;
        NwState.rat = strdup(rat);
        NwState.connState = strdup(connState);

        CurrentTechnoCS = accessTechToRat(NwState.rat, RIL_REQUEST_VOICE_REGISTRATION_STATE);
        CurrentTechnoPS = accessTechToRat(NwState.rat, RIL_REQUEST_DATA_REGISTRATION_STATE);

    }
    free(p);
    return;
    error:
        free(p);
        ALOGE("Error parsing nwstate");
}

void parseIcti(const char *s)
{
    char * line = strdup((char*)s);
    char *p = line;
    char *rat;
    int err;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &rat);
    if (err < 0) goto error;

    if(strcmp(rat, NwState.rat))
    {
        /* Notify a change of network status */
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
            NULL, 0);

        free(NwState.rat);
        NwState.rat = strdup(rat);

        CurrentTechnoCS = accessTechToRat(NwState.rat, RIL_REQUEST_VOICE_REGISTRATION_STATE);
        CurrentTechnoPS = accessTechToRat(NwState.rat, RIL_REQUEST_DATA_REGISTRATION_STATE);
    }
    error:
    free(p);
}
void InitNetworkCacheState(void)
{
    int i;

    NwState.connState = strdup("");
    NwState.rat = strdup("");
    NwState.emergencyOrSearching = 0;
    NwState.mccMnc = -1;
    NetworkSelectionMode = -1;

    Creg.Cid = -1;
    Creg.Lac = -1;
    Creg.Reg = -1;
    Creg.Rac = -1;

    Cgreg.Cid = -1;
    Cgreg.Lac = -1;
    Cgreg.Reg = -1;
    Cgreg.Rac = -1;

    Cereg.Cid = -1;
    Cereg.Lac = -1;
    Cereg.Reg = -1;
    Cereg.Rac = -1;

    CopsState.valid = 0;
    for(i=0;i<3;i++)
    {
        CopsState.networkOp[i] = NULL;
    }
}

static void resetCachedOperator(void)
{
    int i;
    CopsState.valid = 0;
    for(i = 0;i<3;i++)
    {
        free(CopsState.networkOp[i]);
        CopsState.networkOp[i] = NULL;
    }
}

int IsRegistered(void)
{
    return  (Cereg.Reg == 1)||(Cereg.Reg == 5)||
            (Creg.Reg == 1)||(Creg.Reg == 5)||
            (Cgreg.Reg == 1)||(Cgreg.Reg == 5);
}

/*
 * EVENT: On radio popwer on
 */
void InitializeNetworkUnsols(int defaultLocUp)
{
    int err;
    ATResponse *p_response = NULL;

    /*  check if LTE is supported */
    err = at_send_command("AT+CEREG?", &p_response);

    /* Check if LTE supporting FW */
    if (err != 0 || p_response->success == 0)
    {
        LteSupported = 0;
        /* State will always be not searching for LTE */
        Cereg.Reg = 0;
    }
    else
    {
        LteSupported = 1;
    }
    at_response_free(p_response);

    LocUpdateEnabled = defaultLocUp;

    /* Initialize unsolicited and force first update with solicited request */
    if(defaultLocUp==0)
    {
        at_send_command(LteSupported?"AT+CREG=1;+CGREG=1;+CEREG=1;+CREG?;+CGREG?;+CEREG?":"AT+CREG=1;+CGREG=1;+CREG?;+CGREG?",NULL);
    }
    else
    {
        at_send_command(LteSupported?"AT+CREG=2;+CGREG=2;+CEREG=2;+CREG?;+CGREG?;+CEREG?":"AT+CREG=2;+CGREG=2;+CREG?;+CGREG?",NULL);
    }
}

int GetFDTimerEnabled(void) {
    return FDAutoTimerEnabled;
}

void SetFDTimerEnabled(int enabled) {
    FDAutoTimerEnabled = enabled;
}

int GetLocupdateState(void)
{
    return LocUpdateEnabled;
}

int GetLteSupported(void)
{
    return LteSupported;
}

void requestVoiceRadioTech(void *data, size_t datalen, RIL_Token t) {
    int voiceRadioTech = RADIO_TECH_UMTS;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &voiceRadioTech, sizeof(int));
}

int UnsolSignalStrengthExt = -1;

void SetUnsolSignalStrength(int enabled)
{
    if (UnsolSignalStrengthExt == -1)
    {
        return;
    }

    if (currentState() == RADIO_STATE_OFF ||
        currentState() == RADIO_STATE_UNAVAILABLE)
    {
        return;
    }

    if (enabled)
    {
        if (UnsolSignalStrengthExt)
        {
            /*
             * %ICESQU filtering
             *    5 sec minimum time before update
             *    6 dB rxlev, rscp, rsrp update threshold
             *    12 1/2 dB ecno, rsrq update threshold
             *    1 ber RXQUAL update threshold
             * %ICESQU=<unsol>,<timerPeriod>,<rssiThr>,<berThr>,<rscpThr>,<ecnoThr>,<rsrqThr>,<rscpThr>
             */
            if(rilExtOnConfigureSignalLevelFiltering(UnsolSignalStrengthExt)) {
                at_send_command("AT%ICESQU=1,5,6,1,6,12,12,6", NULL);
            }

            /* Update signal strength values via unsolicited */
            at_send_command("AT+CESQ",NULL);
        }
        else
        {
            if(rilExtOnConfigureSignalLevelFiltering(UnsolSignalStrengthExt)) {
                /* Average over 500ms to avoid intempestive notifications */
                at_send_command("AT*TSQF=5,500,3", NULL);
            }
            /* Enable *TSQN notifications */
            at_send_command("AT*TUNSOL=\"SQ\",1", NULL);
            /* Update signal strength values via unsolicited */
            at_send_command("AT+CSQ",NULL);
        }
    }
    else
    {
        if (UnsolSignalStrengthExt)
        {
            at_send_command("AT%ICESQU=0", NULL);
        }
        else
        {
            at_send_command("AT*TUNSOL=\"SQ\",0", NULL);
        }
    }
}

void UnsolSignalStrengthInit(void)
{
    int err;
    ATResponse *p_response = NULL;

    /*  Check if AT%IUCESQU is supported */
    err = at_send_command_singleline("AT%ICESQU?", "%ICESQU:", &p_response);
    if ((err == 0) && (p_response->success == 1))
    {
        UnsolSignalStrengthExt = 1;
    }
    else
    {
        UnsolSignalStrengthExt = 0;
    }
    at_response_free(p_response);

    /* Enable Unsolicited at start up */
    SetUnsolSignalStrength(1);
}

static int clipSignalStrength(int ss) {
    // Clipping the rssi range in [0..31]
    if (ss > 31)
        return 31;
    else if (ss < 0)
        return 0;
    return ss;
}

/**
 * return value: -1 as error, 0 as executed normally, 1 as get all necessary infomation, 2 as totally endded.
 */
static int parseCellID ( char *line, CellInfoBuf *cib, signalStrengthBuf *ssb, int cell_idx) {
    nameValuePair rst;
    char *szValue = NULL;
    int err;

    if (cib == NULL || ssb == NULL)
        return -1;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &(rst.name));
    if (err < 0) goto error;

    if (strcmp("END", rst.name) == 0) {
        // END doesn't contain value
        rst.value = NULL;
        return 2;   //END
    }

    err = at_tok_nextstr(&line, &(rst.value));
    if (err < 0) goto error;

    if (cell_idx == 0)
        cib->registered = 1;
    else
        cib->registered = 0;

    if (cell_idx > 0 && cib->rat == RIL_CELL_INFO_TYPE_GSM)
        return 0;

    if (strcmp("RAT", rst.name) == 0) {                                                             /* Common fields */
        if (strcmp("GSM", rst.value) == 0)
            cib->rat = RIL_CELL_INFO_TYPE_GSM;
        else if (strcmp("UTRA FDD", rst.value) == 0)
            cib->rat = RIL_CELL_INFO_TYPE_WCDMA;
        else if (strcmp("LTE", rst.value) == 0)
            cib->rat = RIL_CELL_INFO_TYPE_LTE;
        else
            cib->rat = 0;
    } else if (strcmp("MCC", rst.name) == 0) {
        cib->mcc = atoi(rst.value);
    } else if (strcmp("MNC", rst.name) == 0) {
        cib->mnc = atoi(rst.value);
    } else if (cib->rat == RIL_CELL_INFO_TYPE_GSM && strcmp("LAC", rst.name) == 0) {              /* GSM fields */
        cib->lac = atoi(rst.value);
    } else if (cib->rat == RIL_CELL_INFO_TYPE_GSM && strcmp("CellId", rst.name) == 0) {
        cib->cid = atoi(rst.value);
        ALOGD("gsm serving cell info ready");
        return 1;   // GSM Cell parsing finished, need to get signal strength from +CSQ
    } else if ((cib->rat == RIL_CELL_INFO_TYPE_GSM || cib->rat == RIL_CELL_INFO_TYPE_WCDMA) &&
                strcmp("LAC", rst.name) == 0) {
        cib->lac = atoi(rst.value);
    } else if (cib->rat == RIL_CELL_INFO_TYPE_WCDMA && strcmp("CPICH_RSCP", rst.name) == 0) {      /* WCDMA fields */
        // Need to convert the rscp [0..91] & [123..127] to rssi [0..31]
        int rscp = atoi(rst.value);
        if (rscp <= 91 && rscp >= 0) {
            ssb->signalStrength = clipSignalStrength((rscp - (115 - 113))/2);
        } else {
            // from 123 to 127, it always lower than -113dbm
            ssb->signalStrength = 0;
        }
    } else if (cib->rat == RIL_CELL_INFO_TYPE_WCDMA && strcmp("UC", rst.name) == 0) {
        cib->cid = atoi(rst.value);
    } else if (cib->rat == RIL_CELL_INFO_TYPE_WCDMA &&
                (strcmp("Intra Freq", rst.name) == 0 || strcmp("Inter Freq", rst.name) == 0)) {
        ALOGD("wcdma cell info ready");
        return 1; // WCDMA cell info ready.
    } else if (cib->rat == RIL_CELL_INFO_TYPE_WCDMA && strcmp("Primary Scrambling Code", rst.name) == 0) {
        cib->psc = atoi(rst.value);
    } else if ((cib->rat == RIL_CELL_INFO_TYPE_LTE) && strcmp("CellId", rst.name) == 0) {           /* LTE fields */
        cib->ci = strtoul(rst.value, NULL, 10);
    } else if (cib->rat == RIL_CELL_INFO_TYPE_LTE && strcmp("PhysicalCellId", rst.name) == 0) {
        cib->pci = atoi(rst.value);
    } else if (cib->rat == RIL_CELL_INFO_TYPE_LTE && strcmp("Tac", rst.name) == 0) {
        cib->tac = atoi(rst.value);
    } else if (cib->rat == RIL_CELL_INFO_TYPE_LTE && strcmp("RSRP", rst.name) == 0) {
        // ssb->rsrp is [44..140]
        ssb->rsrp = 140 - atoi(rst.value);
        if (ssb->rsrp < 44)
            ssb->rsrp = 44;
        // Convert rsrp [0..97] to signalstrength [0..31]
        ssb->signalStrength = clipSignalStrength((atoi(rst.value) - (140 - 113))/2);
    } else if (cib->rat == RIL_CELL_INFO_TYPE_LTE && strcmp("RSRQ", rst.name) == 0) {
        // RSRQ's range in [0..34], need to convert to ssb->rsrq's range in [3..20]
        ssb->rsrq = (40 - atoi(rst.value))/2;
        if (ssb->rsrq > 20)
            ssb->rsrq = 20;
        else if (ssb->rsrq < 3)
            ssb->rsrq = 3;
    } else if (cib->rat == RIL_CELL_INFO_TYPE_LTE && strcmp("Timing Advance", rst.name) == 0) {
        /*
         * tA INTEGER(0..1282) OPTIONAL, -- Timing Currently used Timing Advance value
         * (NTA/16 as per [3GPP TS 36.213]).
         * Ta * 16 * 1000000 / (15000 * 2048), Ref: [3GPP 36.211]
         */
        ssb->timingAdvance = atoi(rst.value) * 16 * 1000000 / (15000 * 2048);
    } else if (cib->rat == RIL_CELL_INFO_TYPE_LTE && strcmp("MeasResultEUTRA", rst.name) == 0) {
        ALOGD("lte cell info ready");
        return 1; // LTE cell info ready.
    } else {
        // Ignore useless tag
    }

    return 0;
error:
    ALOGE("parseCellID failure.");

    return -1;
}

/* Check if any information has been left in the cib */
static int isAvailableCell(CellInfoBuf * const cib) {
    if (cib->lac == INT_MAX &&
        cib->cid == INT_MAX &&
        cib->psc == INT_MAX &&
        cib->ci  == INT_MAX &&
        cib->pci == INT_MAX &&
        cib->tac == INT_MAX)
        return 0;
    return 1;
}

static int initCellinfoBuf(CellInfoBuf * const cib, int keepCommonFields) {
    if (cib == NULL)
        return -1;

    if (!keepCommonFields) {
        cib->rat = INT_MAX;
        cib->mcc = INT_MAX;
        cib->mnc = INT_MAX;
    }
    cib->lac = INT_MAX;
    cib->cid = INT_MAX;
    cib->psc = INT_MAX;
    cib->ci = INT_MAX;
    cib->pci = INT_MAX;
    cib->tac = INT_MAX;
    cib->registered = 0;    // no registered as default.
    return 1;
}

static int initSSBuf(signalStrengthBuf * const ssb) {
    if (ssb == NULL)
        return -1;
    ssb->signalStrength = 99;
    ssb->bitErrorRate = 99;
    ssb->rsrp = INT_MAX;
    ssb->rsrq = INT_MAX;
    ssb->timingAdvance = INT_MAX;
    ssb->cqi = INT_MAX;
    ssb->rssnr = INT_MAX;
    return 1;
}

static void setResponsFromCibSsb(RIL_CellInfo * const ptr, const CellInfoBuf *cib, const signalStrengthBuf *ssb) {
    if (ptr == NULL)
        return;

    // Fill the data
    ptr->registered = cib->registered;
    ptr->timeStampType = RIL_TIMESTAMP_TYPE_OEM_RIL;
    ptr->timeStamp = ril_nano_time();
    ptr->cellInfoType = cib->rat;
    switch(ptr->cellInfoType) {
        case RIL_CELL_INFO_TYPE_GSM:
            ptr->CellInfo.gsm.cellIdentityGsm.mcc = cib->mcc;
            ptr->CellInfo.gsm.cellIdentityGsm.mnc = cib->mnc;
            ptr->CellInfo.gsm.cellIdentityGsm.lac = cib->lac;
            ptr->CellInfo.gsm.cellIdentityGsm.cid = cib->cid;
            ptr->CellInfo.gsm.signalStrengthGsm.signalStrength = ssb->signalStrength;
            ptr->CellInfo.gsm.signalStrengthGsm.bitErrorRate = 99;  /* Keep the value as default */
            break;
        case RIL_CELL_INFO_TYPE_WCDMA:
            ptr->CellInfo.wcdma.cellIdentityWcdma.mcc = cib->mcc;
            ptr->CellInfo.wcdma.cellIdentityWcdma.mnc = cib->mnc;
            ptr->CellInfo.wcdma.cellIdentityWcdma.lac = cib->lac;
            ptr->CellInfo.wcdma.cellIdentityWcdma.cid = cib->cid;
            ptr->CellInfo.wcdma.cellIdentityWcdma.psc = cib->psc;
            ptr->CellInfo.wcdma.signalStrengthWcdma.signalStrength = ssb->signalStrength;
            ptr->CellInfo.wcdma.signalStrengthWcdma.bitErrorRate = 99; /* Not used in WCDMA, keep it as default */
            break;
        case RIL_CELL_INFO_TYPE_LTE:
            ptr->CellInfo.lte.cellIdentityLte.mcc = cib->mcc;
            ptr->CellInfo.lte.cellIdentityLte.mnc = cib->mnc;
            ptr->CellInfo.lte.cellIdentityLte.ci = cib->ci;
            ptr->CellInfo.lte.cellIdentityLte.pci = cib->pci;
            ptr->CellInfo.lte.cellIdentityLte.tac = cib->tac;
            ptr->CellInfo.lte.signalStrengthLte.signalStrength = ssb->signalStrength;
            ptr->CellInfo.lte.signalStrengthLte.rsrp = ssb->rsrp;
            ptr->CellInfo.lte.signalStrengthLte.rsrq = ssb->rsrq;
            ptr->CellInfo.lte.signalStrengthLte.rssnr = ssb->rssnr;
            ptr->CellInfo.lte.signalStrengthLte.cqi = INT_MAX;
            ptr->CellInfo.lte.signalStrengthLte.timingAdvance = ssb->timingAdvance;
            break;
        default:
            ALOGE("Get an unknown cell type [%d] in [%03d%02d]", cib->rat, cib->mcc, cib->mnc);
            ptr->cellInfoType = 0;
    }
}

/**
* RIL_REQUEST_GET_CELL_INFO_LIST
*
* Request all of the current cell information known to the radio. The radio
* must a list of all current cells, including the neighboring cells. If for a particular
* cell information isn't known then the appropriate unknown value will be returned.
* This does not cause or change the rate of RIL_UNSOL_CELL_INFO_LIST.
*
* "data" is NULL
*
* "response" is an array of RIL_CellInfo.
*/
static RIL_Token cilRequestToken;

void requestGetCellInfoList(void *data, size_t datalen, RIL_Token t) {
    int err;
    ATResponse *p_response = NULL;
    int pending;

    if (cilRequestToken == NULL) {

        err = at_send_command ("AT%IECELLID=1", &p_response);

        if (err != 0 || p_response->success == 0) {
            pending = 0;
        } else {
            pending = 1;
        }

        at_response_free(p_response);

        if (pending == 1) {
            cilRequestToken = t;
            return;
        }
    } else {
        ALOGE("CELL_INFO_LIST request is still running, skip it");
    }
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    ALOGE("requestGetCellInfoList failure");
    return;
}

/**
 * RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE
 *
 * Sets the minimum time between when RIL_UNSOL_CELL_INFO_LIST should be invoked.
 * A value of 0, means invoke RIL_UNSOL_CELL_INFO_LIST when any of the reported
 * information changes. Setting the value to INT_MAX(0x7fffffff) means never issue
 * a RIL_UNSOL_CELL_INFO_LIST.
 *
 * "data" is int *
 * ((int *)data)[0] is minimum time in milliseconds
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */

void requestSetUnsolCellInfoListRate(void *data, size_t datalen, RIL_Token t)
{
    // data[0] is minimum time in milliseconds
    int durationMSec = ((int *)data)[0];
    int mode, setDurationSec;
    char * cmd = NULL;
    int err;
    ATResponse *p_response = NULL;

    if (durationMSec != INT_MAX)
    {
        mode = ICERA_CELLID_TIMER_ON;
        setDurationSec = durationMSec/1000;
        if (setDurationSec > 600) {
            ALOGE("Duration is exceeding the max timer, set to 600 seconds.");
            setDurationSec = 600;
        } else if (setDurationSec <= 0) {
            /* If user requires to send unsol when cell state changed, we send it every 60s */
            setDurationSec = 60;
        } else if (setDurationSec == 1) {
            /* One second is not enough for us to query %INCELL and parsing the response, so the floor should be 2 seconds */
            setDurationSec = 2;
        }
        asprintf(&cmd, "AT%%IECELLID=%d,%d", mode, setDurationSec);
    }
    else if (durationMSec == INT_MAX)
    {
        //Setting to INT_MAX(0x7fffffff) means never issue
        mode = ICERA_CELLID_TIMER_OFF;
        asprintf(&cmd, "AT%%IECELLID=%d", mode);
    } else {
        ALOGE("duration is incorrect!");
        goto error;
    }

    err = at_send_command(cmd, &p_response);
    free(cmd);
    if (err != 0 && p_response->success == 0)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    at_response_free(p_response);
    ALOGE("ERROR: requestSetUnsolCellInfoListRate failed\n");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static int getGSMSCSS(RIL_CellInfo **data, int *p_cellNum) {
    int err;
    ATResponse *p_response = NULL;
    char *line;
    int rssi, ber;
    RIL_CellInfo *response = *data;
    int cellNum = *p_cellNum;

    if (cellNum == 0 || response == NULL)
        goto error;

    err = at_send_command_singleline("AT+CSQ",  "+CSQ:", &p_response);

    if (err != 0 || p_response->success == 0)
        goto error;

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &rssi);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &ber);
    if (err < 0) goto error;

    response[0].CellInfo.gsm.signalStrengthGsm.signalStrength = rssi;
    response[0].CellInfo.gsm.signalStrengthGsm.bitErrorRate = ber;

    at_response_free(p_response);
    return 0;
error:
    at_response_free(p_response);
    return -1;
}

/*
 * Return vaule: -1 mean should return error if it is a request.
 * 1 means it is good, should return response.
 */
static int getGSMNCInfo(RIL_CellInfo **data, int *p_cellNum ) {
    int err = 0;
    ATResponse *p_response = NULL;
    ATLine *p_cur;
    char *line;
    int type;
    int cells_number;
    int cell_id;
    int rssi;
    int i,j;
    CellInfoBuf cib;
    signalStrengthBuf ssb;
    RIL_CellInfo *response = *data;
    int cellNum = *p_cellNum;

    if (cellNum == 0 || response == NULL)
        goto error;

    err = at_send_command_multiline("AT%INCELL", "%INCELL:", &p_response);

    if (err != 0 || p_response->success == 0) goto error;

    /* %INCELL: <type>,<nbCells>,<cellId1>,<SignalLevel1>,...,<cellIdN>,<SignalLevelN> */

    for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next) {
        line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &type);
        if (err < 0) goto error;

        if (type != 3) continue; /* Only parsing 2G cells which is 3 */

        err = at_tok_nextint(&line, &cells_number);
        if (err < 0) goto error;

        // Initial common fields
        cib.rat = response[0].cellInfoType;
        cib.mcc = response[0].CellInfo.gsm.cellIdentityGsm.mcc;
        cib.mnc = response[0].CellInfo.gsm.cellIdentityGsm.mnc;

        for (i = 0; i < cells_number; i++) {
            err = at_tok_nextint(&line, &cell_id);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &rssi);
            if (err < 0) goto error;

            cellNum ++ ;
            initCellinfoBuf(&cib, 1);  // Keep the common fields.
            initSSBuf(&ssb);
            *data = (RIL_CellInfo *) realloc (*data, sizeof(RIL_CellInfo) * cellNum);
            response = *data;
            *p_cellNum = cellNum;
            // Fill up the NC information
            cib.lac = (cell_id >> 16) & 0xFFFF;
            cib.cid = cell_id & 0xFFFF;
            ssb.signalStrength = clipSignalStrength((rssi - (110 - 113)) / 2);
            setResponsFromCibSsb(&response[cellNum-1], &cib, &ssb);
        }
    }
    at_response_free(p_response);
    return 1;
error:
    at_response_free(p_response);
    return -1;
}

void unsolCellInfo(const char *s) {
    static CellInfoBuf cib;
    static signalStrengthBuf ssb;
    static RIL_CellInfo *response;
    static int processing;
    static int CellNum;
    int err = 0;

    if (processing == 0) {
        // It is the first unsol of IECELLID, initialize necesary variables.
        processing = 1;
        initCellinfoBuf(&cib, 0);
        initSSBuf(&ssb);
        response = NULL;
        CellNum = 0;
    }

    err = parseCellID((char*)s, &cib, &ssb, CellNum);
    if (err == 1) {
        // Cell information is ready.
        CellNum ++;
        response = realloc(response, sizeof(RIL_CellInfo) * CellNum);
        setResponsFromCibSsb(&response[CellNum-1], &cib, &ssb);
        initCellinfoBuf(&cib, 1);
        initSSBuf(&ssb);
    } else if (err == 2) {
        // END tag received, we should store the last cell id if we have.
        if (isAvailableCell(&cib)) {
            CellNum ++;
            response = realloc(response, sizeof(RIL_CellInfo) * CellNum);
            setResponsFromCibSsb(&response[CellNum-1], &cib, &ssb);
            initCellinfoBuf(&cib, 1);
            initSSBuf(&ssb);
        }
        processing = 0; // All cells done, next time will be a new response or unsolicited
        if (response == NULL) {
            // no neighbor cells or serving cell are reported, return NULL to framework
            if (cilRequestToken) {
                RIL_onRequestComplete(cilRequestToken, RIL_E_SUCCESS, NULL, 0);
                cilRequestToken = NULL;
            }
            return;
        }

        if (response[0].cellInfoType == RIL_CELL_INFO_TYPE_GSM) {
            // Need to get SC signal level from +CSQ.
            getGSMSCSS( &response, &CellNum);
            // Need to get NC info from %INCELL.
            getGSMNCInfo( &response, &CellNum );
        }
        // All cases will need to be sent by unsol or response.
        if (cilRequestToken == NULL)
        {
            RIL_onUnsolicitedResponse(RIL_UNSOL_CELL_INFO_LIST, response, sizeof(RIL_CellInfo) * CellNum);
        }
        else
        {
            RIL_onRequestComplete(cilRequestToken, RIL_E_SUCCESS, response, sizeof(RIL_CellInfo) * CellNum);
            cilRequestToken = NULL;
        }
        free(response);
        response = NULL;
    } else if (err == -1) {
        ALOGE("parsing error");
        if (cilRequestToken != NULL) {
            RIL_onRequestComplete(cilRequestToken, RIL_E_GENERIC_FAILURE, NULL, 0);
            cilRequestToken = NULL;
        }
        free(response);
        response = NULL;
        processing = 0;
    } else {
        // Get a parameter, keep waiting for the "END" tag comming.
    }
}
