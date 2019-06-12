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


#ifndef ICERA_RIL_CALL_H
#define ICERA_RIL_CALL_H 1

void requestHangupWaitingOrBackground(RIL_Token t);
void requestHangupForegroundResumeBackground(RIL_Token t);
void requestSwitchWaitingOrHoldingAndActive(RIL_Token t);
void requestConference(void *data, size_t datalen, RIL_Token t);
void requestSeparateConnection(void *data, size_t datalen, RIL_Token t);
void requestExplicitCallTransfer(void *data, size_t datalen, RIL_Token t);
void requestUDUB(void *data, size_t datalen, RIL_Token t);
void requestSetMute(void *data, size_t datalen, RIL_Token t);
void requestGetMute(void *data, size_t datalen, RIL_Token t);
void requestGetCurrentCalls(void *data, size_t datalen, RIL_Token t);
void requestDial(void *data, size_t datalen, RIL_Token t);
void requestAnswer(RIL_Token t);
void requestHangup(void *data, size_t datalen, RIL_Token t);
void requestDTMF(void *data, size_t datalen, RIL_Token t);
void requestDTMFStart(void *data, size_t datalen, RIL_Token t);
void requestDTMFStop(void *data, size_t datalen, RIL_Token t);
void requestLastCallFailCause(void *data, size_t datalen, RIL_Token t);

void requestSetCallForward(void *data, RIL_Token t);
void requestQueryCallWaiting(void *data, size_t datalen, RIL_Token t);
void requestSetCallWaiting(void *data, size_t datalen, RIL_Token t);

void unsolicitedCallStateChange(const char *s);

void checkAndAmendEmergencyNumbers(int simInserted, int pbLoaded);

void ResetLastCallErrorCause(void);

#ifdef REPORT_CNAP_NAME
void initCnap(void);
void unsolicitedCNAP(const char *s);
#endif
#endif
