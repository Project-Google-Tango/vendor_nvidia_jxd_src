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

#ifndef ATCHANNEL_H
#define ATCHANNEL_H 1

#ifdef __cplusplus
extern "C" {
#endif

/* define AT_DEBUG to send AT traffic to /tmp/radio-at.log" */
#define AT_DEBUG  0

#if AT_DEBUG
extern void  AT_DUMP(const char* prefix, const char*  buff, int  len);
#else
#define  AT_DUMP(prefix,buff,len)  do{}while(0)
#endif

#define AT_ERROR_GENERIC -1
#define AT_ERROR_COMMAND_PENDING -2
#define AT_ERROR_CHANNEL_CLOSED -3
#define AT_ERROR_TIMEOUT -4
#define AT_ERROR_INVALID_THREAD -5 /* AT commands may not be issued from
                                       reader thread (or unsolicited response
                                       callback */
#define AT_ERROR_INVALID_RESPONSE -6 /* eg an at_send_command_singleline that
                                        did not get back an intermediate
                                        response */

/** a singly-lined list of intermediate responses */
typedef struct ATLine  {
    struct ATLine *p_next;
    char *line;
} ATLine;

/** Free this with at_response_free() */
typedef struct {
    int success;              /* true if final response indicates
                                    success (eg "OK") */
    char *finalResponse;      /* eg OK, ERROR */
    ATLine  *p_intermediates; /* any intermediate responses */
} ATResponse;

/**
 * a user-provided unsolicited response handler function
 * this will be called from the reader thread, so do not block
 * "s" is the line, and "sms_pdu" is either NULL or the PDU response
 * for multi-line TS 27.005 SMS PDU responses (eg +CMT:)
 */
typedef void (*ATUnsolHandler)(const char *s, const char *sms_pdu);

int at_open(int fd, ATUnsolHandler h, int default_channel, long long timeout);
void at_close(void);

/* This callback is invoked on the command thread.
   You should reset or handshake here to avoid getting out of sync */
void at_set_on_timeout(void (*onTimeout)(void));
/* This callback is invoked on the reader thread (like ATUnsolHandler)
   when the input stream closes before you call at_close
   (not when you call at_close())
   You should still call at_close()
   It may also be invoked immediately from the current thread if the read
   channel is already closed */
void at_set_on_reader_closed(void (*onClose)(void));

int at_send_command_singleline (const char *command,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse);

int at_send_command_singleline_secure (const char *command,
                                const char *command_to_log,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse);

int at_send_command_numeric (const char *command,
                                 ATResponse **pp_outResponse);

int at_send_command_multiline (const char *command,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse);

int at_send_command_multiline_no_prefix (const char *command,
                                 ATResponse **pp_outResponse);

int at_handshake(void);

int at_send_command (const char *command, ATResponse **pp_outResponse);

int at_send_command_secure (const char *command, const char *command_to_log, ATResponse **pp_outResponse);

int at_send_command_sms (const char *command, const char *pdu,
                            const char *responsePrefix,
                            ATResponse **pp_outResponse);

void at_response_free(ATResponse *p_response);

int at_send_command_tofile (const char *command, const char *file);

typedef enum {
    CME_ERROR_NON_CME = -1,
    CME_SUCCESS = 0,
    CME_SIM_NOT_INSERTED = 10,
    CME_ERROR_PUK_REQUIRED = 12,
    CME_ERROR_SIM_FAILURE = 13,
    CME_ERROR_SIM_BUSY = 14,
    CME_ERROR_INCORRECT_PASSWORD = 16,
    CME_ERROR_PIN2_REQUIRED = 17,
    CME_ERROR_PUK2_REQUIRED = 18,
    CME_ERROR_ADN_PHONEBOOK_FULL = 20,
    CME_ERROR_ILLEGAL_MS = 103,
    CME_ERROR_ILLEGAL_ME = 106,
    CME_ERROR_GPRS_SERVICES_NOT_ALLOWED = 107,
    CME_ERROR_GPRS_SERVICE_OPTION_NOT_SUPPORTED = 132,
    CME_ERROR_GPRS_REQUESTED_SERVICE_OPTION_NOT_SUBSCRIBED = 133,
    CME_ERROR_GPRS_SERVICE_OPTION_TEMPORARILY_NOT_AVAILABLE = 134,
    CME_ERROR_GPRS_ACTIVATION_REJECT = 577,
    CME_ERROR_GPRS_UNSPECIFIED_ACTIVATION_REJECTION = 578,
    CME_ERROR_GPRS_INSUFFICIENT_RESOURCES = 592,
    CME_ERROR_GPRS_MISSING_OR_UNKNOWN_APN = 607,
    CME_ERROR_GPRS_UNSPECIFIED_PROTOCOL_ERROR = 631,
    CME_ERROR_GPRS_UNKNOWN_PDP_ADDRESS_OR_TYPE = 643,
    CME_ERROR_GPRS_USER_AUTHORISATION_FAILED = 645,
    CME_ERROR_INVALID_INPUT_VALUE = 765,
    CME_ERROR_SIM_INVALID = 770,
    CME_ERROR_SIM_POWERED_DOWN = 772,
    CME_ERROR_ME_DEPERSONALIZATION_BLOCKED = 774,
    CME_ERROR_SIM_PUK_BLOCKED = 775,
    CME_ERROR_GRP_PHONEBOOK_FULL = 780,
    CME_ERROR_ANR_PHONEBOOK_FULL = 781,
    CME_ERROR_SNE_PHONEBOOK_FULL = 782,
    CME_ERROR_EMAIL_PHONEBOOK_FULL = 783,
    CME_ERROR_AAS_PHONEBOOK_FULL = 785,
    CME_ERROR_EXT1_PHONEBOOK_FULL = 787,
    CME_ERROR_TEMPERATURE_EXCEED = 805,
    CME_ERROR_INVALID_SESSION_ID = 810,
    CME_ERROR_CCHC_INVALID_SESSION_ID = 813,
    CME_ERROR_CCHC_INVALID_INPUT_VALUE = 814,

    CME_ERROR_SS_FDN_FAILURE = 886
} AT_CME_Error;

typedef enum {
    CMS_ERROR_NON_CMS = -1,
    CMS_SUCCESS = 0,
    CMS_ERROR_MEMORY_FULL = 322,
    CMS_ERROR_NO_NETWORK_SERVICE = 331,
    CMS_ERROR_NETWORK_TIMEOUT = 332,
    CMS_ERROR_UNKNOWN_ERROR = 500,
    CMS_ERROR_FDN_FAILURE = 520
} AT_CMS_Error;

typedef enum {
    CCHC_SUCCESS = 0,
} AT_CCHC_Error;

AT_CME_Error at_get_cme_error(const ATResponse *p_response);
AT_CMS_Error at_get_cms_error(const ATResponse *p_response);
int at_is_channel_UCS2(void);

#ifdef __cplusplus
}
#endif

#endif /*ATCHANNEL_H*/
