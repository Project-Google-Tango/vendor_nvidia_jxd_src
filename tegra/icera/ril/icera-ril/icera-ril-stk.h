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
#ifndef ICERA_RIL_STK_H
#define ICERA_RIL_STK_H 1

void requestStkGetProfile(void *data, size_t datalen, RIL_Token t);
void requestSTKSetProfile(void * data, size_t datalen, RIL_Token t);
void requestSTKSendEnvelopeCommand(void * data, size_t datalen, RIL_Token t);
void requestSTKSendTerminalResponse(void * data, size_t datalen, RIL_Token t);
void requestStkHandleCallSetupRequestedFromSim(void * data, size_t datalen, RIL_Token t);
void requestSTKReportSTKServiceIsRunning(void * data, size_t datalen, RIL_Token t);
void requestSTKPROReceived(void *data, size_t datalen, RIL_Token t);
void unsolSimStatusChanged(const char *s);
void unsolStkEventNotify(const char *s);
void unsolicitedSTKPRO(const char *s);
int StkNotifyServiceIsRunning(int from_request);
void StkInitDefaultProfiles(void);
void requestSTKSendEnvelopeCommandWithStatus(void * data, size_t datalen, RIL_Token t);
int PbkIsLoaded(void);
void StkIsFirstBoot(int first_boot);

typedef struct {
    int Type;
    int Result;
    char * Alpha_id;
    int Dcs;
    char * Data;
    char * Destination_address;
    int Destination_address_type;
    char * Service_center_address;
    int Service_center_address_type;
} CallControlInfo;

#define STK_UNSOL_CCBS 0
#define STK_UNSOL_MOSMSC 1

int decodeCallControlInfo(char *input, int call_or_sms, CallControlInfo *output);

extern int stk_use_cusat_api;

/* STK profiles*/
#define RIL_STK_TE_PROFILE     "080017210100001CC900000000000000000000000000000000000000000000"
#define RIL_STK_MT_PROFILE_ADD "0000001E610000000000000000000000000000000000000000000000000000"
#define RIL_STK_CONF_PROFILE   "00000010000000000000000100000000000000000000000000000000000000"

#endif
