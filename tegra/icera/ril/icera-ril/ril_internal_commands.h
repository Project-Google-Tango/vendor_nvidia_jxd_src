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
    {0, NULL, NULL},                   //none
    {IRIL_REQUEST_ID_SIM_POLL, dispatchVoid, responseVoid},
    {IRIL_REQUEST_ID_STK_PRO, dispatchInts, responseVoid},
    {IRIL_REQUEST_ID_NETWORK_TIME, dispatchVoid, responseVoid},
    {IRIL_REQUEST_ID_RESTRICTED_STATE, dispatchVoid, responseVoid},
    {IRIL_REQUEST_ID_PROCESS_DATACALL_EVENT, dispatchInts, responseVoid},
    {IRIL_REQUEST_ID_UPDATE_SIM_LOADED, dispatchVoid, responseVoid},
    {IRIL_REQUEST_ID_SMS_ACK_TIMEOUT, dispatchInts, responseVoid},
    {IRIL_REQUEST_ID_FORCE_REATTACH, dispatchVoid, responseVoid},
    {IRIL_REQUEST_ID_ECC_VERIF_COMPUTE, dispatchVoid, responseVoid},
    {IRIL_REQUEST_ID_ECC_VERIF_RESPONSE, dispatchInts, responseVoid},
    {IRIL_REQUEST_ID_SIM_HOTSWAP, dispatchInts, responseVoid},
    {IRIL_REQUEST_ID_RTC_UPDATE, dispatchVoid, responseVoid},
    {IRIL_REQUEST_ID_GET_IPV6_ADDR, dispatchInts, responseVoid},
