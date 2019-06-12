/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.NvWFDSvc;

/**
 * @hide
 */
interface INvWFDServiceListener {
    void onSinksFound(in List sinks);
    void onConnectDone(boolean status);
    void onDiscovery(int status);
    void onDisconnectDone(boolean status);
    void onNotifyError(String text);
    void onCancelConnect(int status);
    void onSetupComplete(int status);
}
