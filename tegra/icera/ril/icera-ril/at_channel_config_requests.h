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

/**
* @{
*/

/**
* @file at_channel_config_requests.h Configuration which RIL Requests will be mapped to which
*     additional AT channel
*
*/
#ifndef __AT_CHANNEL_CONFIG_REQUESTS_H_
#define __AT_CHANNEL_CONFIG_REQUESTS_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <telephony/ril.h> /* RIL Request definitions */
#include <icera-ril.h>

/** List of RIL Requersts mapped to the first additional AT channel */
static int s_chan_01_ril_request_list[] =
{
  RIL_REQUEST_QUERY_AVAILABLE_NETWORKS,
  RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL,
  RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC,
  IRIL_REQUEST_ID_FORCE_REATTACH,
  RIL_REQUEST_OEM_HOOK_RAW,
/*, RIL_REQUEST_xxx   */
/** TODO: add more RIl requests here */
  0  /* always terminate this kind of list with zero */
};


static int s_chan_02_ril_request_list[] =
{
    RIL_REQUEST_SEND_SMS_EXPECT_MORE,
    RIL_REQUEST_SEND_SMS,
    RIL_REQUEST_SMS_ACKNOWLEDGE,
    0  /* always terminate this kind of list with zero */
};

/** array of RIL request lists for the different additional AT channels */
static int* s_chan_list[] =
{
    NULL  /** default channel has no RIL request list */
  , s_chan_01_ril_request_list
  , s_chan_02_ril_request_list
/*  enable this for an additonal channel , s_chan_02_ril_request_list */
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __AT_CHANNEL_CONFIG_REQUESTS_H_ */

/** @} END OF FILE */
