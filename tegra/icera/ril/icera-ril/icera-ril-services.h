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

#ifndef ICERA_RIL_SERVICES_H
#define ICERA_RIL_SERVICES_H 1

void onUSSDReceived(const char *s);
void onSuppServiceNotification(const char *s, int type);
void requestQueryClip(void *data, size_t datalen, RIL_Token t);
void requestCancelUSSD(void *data, size_t datalen, RIL_Token t);
void requestSendUSSD(void *data, size_t datalen, RIL_Token t);
void requestGetCLIR(void *data, size_t datalen, RIL_Token t);
void requestSetCLIR(void *data, size_t datalen, RIL_Token t);
void requestQueryCallForwardStatus(void *data, size_t datalen, RIL_Token t);
void requestQueryCLIP(void *data, size_t datalen, RIL_Token t);
void requestSetTtyMode(void *data, size_t datalen, RIL_Token t);
void requestQueryTtyMode(void *data, size_t datalen, RIL_Token t);
void requestSetSuppSVCNotification(void *data, size_t datalen, RIL_Token t);
void requestScreenState(void *data, size_t datalen, RIL_Token t, int FDTO_screenOff, int FDTO_screenOn);
void requestOEMHookStrings(void *data, size_t datalen, RIL_Token t);
void requestOEMHookRaw(void *data, size_t datalen, RIL_Token t);
void onSuppServiceNotification(const char *s, int type);
void requestShutdownRadioPower(void *data, size_t datalen, RIL_Token t);
void requestAcousticProfileCmd(void *data, size_t datalen, RIL_Token t);

int UpdateScreenState(int FDTO_screenOff, int FDTO_screenOn);
int changeCfun(int cfun);
void parseIfunu(const char *s);

#endif
