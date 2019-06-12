/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "linux_common.h"

static XAboolean s_GstInitDone = XA_BOOLEAN_FALSE;

void InitializeFramework(void)
{
    XA_ENTER_INTERFACE_VOID;

    if (!s_GstInitDone)
    {
        gst_init(NULL, NULL);
        if (!g_thread_supported())
            g_thread_init(NULL);

        g_type_init();
        s_GstInitDone = XA_BOOLEAN_TRUE;
    }

    XA_LEAVE_INTERFACE_VOID;
}

XAresult
ChangeGraphState(
    GstElement *pGraph,
    GstState newState)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;
    GstState state;
    XA_ENTER_INTERFACE;

    AL_CHK_ARG(pGraph);
    ret = gst_element_set_state(pGraph, newState);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        XA_LOGE("\n Graph State Change failed! ret = 0x%08x\n", ret);
        result = XA_RESULT_INTERNAL_ERROR;
    }
    else if (ret == GST_STATE_CHANGE_ASYNC)
    {
        XA_LOGD(" waiting for Graph to reach new state: %d \n", newState);
        ret = gst_element_get_state(
            pGraph, &state, NULL, 5 * GST_SECOND); //Worst case 5 second Timeout
        if ((ret != GST_STATE_CHANGE_SUCCESS) ||
            (state != newState))
        {
            result = XA_RESULT_INTERNAL_ERROR;
        }
    }

    XA_LEAVE_INTERFACE;
}

