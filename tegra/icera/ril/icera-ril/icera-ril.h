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

#ifndef ICERA_RIL_H
#define ICERA_RIL_H 1
#include <telephony/ril.h>

/* Logcat tags */
extern char *rilLogTag;
extern char *atLogTag;
extern char *atchLogTag;
extern char *atccLogTag;
extern char *atcwLogTag;
extern char *req0LogTag;
extern char *reqqLogTag;
extern char *unsoLogTag;

/**
 * Name of the wake lock we will request
 */
#define ICERA_RIL_WAKE_LOCK_NAME ("icera-libril")

/**
 *  Various features that can be enabled
 */

/**
 * Enable enhanced caller id management: +CLIP and +CNAP
 * Should be left enabled by default
 */
#define REPORT_CNAP_NAME

/**
 * Will update a property (gsm.modem.ril.init) to make the modem status
 * available in all modes. If unset, then property is only available
 * in test mode
 */
//#define ALWAYS_SET_MODEM_STATE_PROPERTY

/**
 *  Will enable support for emergency call verification. (Specific to
 * operators that support it.)
 * Also need ril.ecc_verification to be set (for quick enable/disable)
 */
//#define ECC_VERIFICATION

#define RIL_REQUEST_OEM_HOOK_SHUTDOWN_RADIO_POWER  0
#define RIL_REQUEST_OEM_HOOK_ACOUSTIC_PROFILE      1
#define RIL_REQUEST_OEM_HOOK_EXPLICIT_ROUTE_BY_RIL 2
#define RIL_REQUEST_OEM_HOOK_EXPLICIT_DATA_ABORT   3
#define RIL_REQUEST_OEM_HOOK_TOGGLE_TEST_MODE      4

#define MAX_NETWORK_IF 5

#if (RIL_VERSION > 10) || (RIL_VERSION < 6)
    #error Version not supported
#elif RIL_VERSION >= 6
    #define ANNOUNCED_RIL_VERSION 6
#endif

#ifndef IRIL_REQUEST_ID_BASE
    #define IRIL_REQUEST_ID_BASE               5000 /* internal communication e.g. for unsolicited events */
#endif
#define IRIL_REQUEST_ID_SIM_POLL           (IRIL_REQUEST_ID_BASE + 1) /* request SIM polling in AT Writer thread */
#define IRIL_REQUEST_ID_STK_PRO            (IRIL_REQUEST_ID_BASE + 2) /* request STK PRO commands in AT Writer thread */
#define IRIL_REQUEST_ID_NETWORK_TIME       (IRIL_REQUEST_ID_BASE + 3) /* DEPRECATED - request Network Time command in AT Writer thread */
#define IRIL_REQUEST_ID_RESTRICTED_STATE   (IRIL_REQUEST_ID_BASE + 4) /* request Restricted State Change in AT Writer thread */
#define IRIL_REQUEST_ID_PROCESS_DATACALL_EVENT    (IRIL_REQUEST_ID_BASE + 5) /* Process event during the datacall progress */
#define IRIL_REQUEST_ID_UPDATE_SIM_LOADED    (IRIL_REQUEST_ID_BASE + 6) /* Sim is fully loaded,  ie we're ready to update the ECC list for the framework */
#define IRIL_REQUEST_ID_SMS_ACK_TIMEOUT     (IRIL_REQUEST_ID_BASE + 7) /* SMS ack timeout */
#define IRIL_REQUEST_ID_FORCE_REATTACH   (IRIL_REQUEST_ID_BASE + 8) /* Reattach attempt */
#define IRIL_REQUEST_ID_ECC_VERIF_COMPUTE   (IRIL_REQUEST_ID_BASE + 9) /* Emergency call start verification procedure */
#define IRIL_REQUEST_ID_ECC_VERIF_RESPONSE  (IRIL_REQUEST_ID_BASE + 10) /* Emergency call verification response */
#define IRIL_REQUEST_ID_SIM_HOTSWAP         (IRIL_REQUEST_ID_BASE + 11) /* Sim hotswap */
#define IRIL_REQUEST_ID_RTC_UPDATE          (IRIL_REQUEST_ID_BASE + 12) /* Will update the RTC of the modem on reception of the request */
#define IRIL_REQUEST_ID_GET_IPV6_ADDR       (IRIL_REQUEST_ID_BASE + 13) /* Used to wait for the IPV6 global address autoconf. */
#define IRIL_REQUEST_ID_MODEM_RESTART       (IRIL_REQUEST_ID_BASE + 14) /* Used to schedule a restart of the modem (e.g. after a thermal shutdown). */

#ifdef RIL_SHLIB
extern const struct RIL_Env *s_rilenv;

#define RIL_onRequestComplete(t, e, response, responselen) s_rilenv->OnRequestComplete(t,e, response, responselen)
#define RIL_onUnsolicitedResponse(a,b,c) s_rilenv->OnUnsolicitedResponse(a,b,c)
#define RIL_requestTimedCallback(a,b,c) s_rilenv->RequestTimedCallback(a,b,c)
#endif

RIL_RadioState currentState(void);
void setRadioState(RIL_RadioState newState);
void requestPutObject(void *param);
void postDelayedRequest(void * req, int delayInSeconds);

const char * iceraRequestToString(int request);

void SendStoredQueryCallForwardStatus(void);

void HandleModemFirmwareUpdate(void);
void ForceModemFirmwareUpdate(void);

int isRunningTestMode( void );
void setTestMode(int newMode);
int isMockModem(void);

int useIcti(void);
void getFDTimeoutValues(int *ScreenOff, int *ScreenOn);
void debugUserNotif(char *);

/* The following defines can be overridden by OEMs */
#ifndef RIL_NOT_USED
    #define RIL_NOT_USED (-1)
#endif

#ifndef CFUN_STATE_ON_RADIO_POWER_OFF
    #define CFUN_STATE_ON_RADIO_POWER_OFF 0
#endif

void checkRtcUpdate(void);

#endif //ICERA_RIL_H

