/* Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved. */
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

#ifndef ICERA_RIL_SMS_H
#define ICERA_RIL_SMS_H 1

void requestSendSMS(void *data, size_t datalen, RIL_Token t);
void requestSendSMSExpectMore(void *data, size_t datalen, RIL_Token t);
void requestSMSAcknowledge(void *data, size_t datalen, RIL_Token t);
void requestWriteSmsToSim(void *data, size_t datalen, RIL_Token t);
void requestDeleteSmsOnSim(void *data, size_t datalen, RIL_Token t);
void onNewSmsOnSIM(const char *s);
void unsolSimSmsFull(const char *s);
void requestGSMGetBroadcastSMSConfig(void *data, size_t datalen, RIL_Token t);
void requestGSMSetBroadcastSMSConfig(void *data, size_t datalen, RIL_Token t);
void requestGSMSMSBroadcastActivation(void *data, size_t datalen, RIL_Token t);
void requestReportSmsPhoneMemoryStatus(void *data, size_t datalen, RIL_Token t);
void requestGetSmsCentreAddress(void *data, size_t datalen, RIL_Token t);
void requestSetSmsCentreAddress(void *data, size_t datalen, RIL_Token t);

void SmsAckCancelTimeOut(int Parameter);
void SmsAckElapsedTimeOut(void * Dummy);
void SmsAckStartTimeOut(void);
void SmsReenableSupport(void);
#endif
