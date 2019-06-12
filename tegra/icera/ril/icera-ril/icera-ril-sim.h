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
#ifndef ICERA_RIL_SIM_H
#define ICERA_RIL_SIM_H 1

typedef enum {
    SIM_ABSENT = 0,
    SIM_NOT_READY = 1,
    SIM_READY = 2,         /* SIM_READY means the radio state is RADIO_STATE_SIM_READY. */
    SIM_PIN = 3,
    SIM_PUK = 4,
    SIM_NETWORK_PERSONALIZATION = 5,
    SIM_NETWORK_PERSONALIZATION_BRICKED = 6,
    SIM_PIN2 = 7,
    SIM_PUK2 = 8,
    SIM_ERROR = 9,
    SIM_PERM_BLOCKED = 10
} SIM_Status;

typedef struct pinRetries_{
    int pin1Retries;
    int pin2Retries;
    int puk1Retries;
    int puk2Retries;
} pinRetriesStruct;

void unsolSIMRefresh(const char *s);
void requestGetSimStatus(void *data, size_t datalen, RIL_Token t);
void requestSIM_IO(void *data, size_t datalen, RIL_Token t);
void requestEnterSimPin(void *data, size_t datalen, RIL_Token t, int request);
void requestEnterSimPin2(void *data, size_t  datalen, RIL_Token  t, int request);
void requestChangePasswordPin(void *data, size_t datalen, RIL_Token t);
void requestChangePasswordPin2(void *data, size_t datalen, RIL_Token t);
void requestSetFacilityLock(void *data, size_t datalen, RIL_Token t);
void requestQueryFacilityLock(void *data, size_t datalen, RIL_Token t);
void requestChangeBarringPassword(void *data, size_t datalen, RIL_Token t);
int getNumOfRetries(pinRetriesStruct * pinRetries);
void requestSIMAuthentication(void *data, size_t datalen, RIL_Token t);

void pollSIMState(void *param);
SIM_Status getSIMStatus(void);

void StartHotSwapMonitoring(int sim_id, char * sim_path);
typedef struct sim_state{
    int id;
    int state;
    char * path;
} sim_state;
#endif
