/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.NvWFDSvc;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.UserHandle;
import android.util.Log;
import android.os.ServiceManager;
import android.os.IBinder;

public class NvwfdStartServiceReceiver extends BroadcastReceiver {
    private final static String LOG_TAG = "NvwfdStartServiceReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        try {
            IBinder mBinder = ServiceManager.getService("com.nvidia.NvWFDSvc.NvwfdService");
            if (mBinder == null && UserHandle.myUserId() == UserHandle.USER_OWNER) {
                Intent service = new Intent(context, NvwfdService.class);
                context.startService(service);
            } else {
                Log.e(LOG_TAG,"guest user:" + UserHandle.myUserId() +" has to access NvWFDSvc through Android getService(),mBinder = " + mBinder);
            }
        } catch (Exception ex) {
            Log.e(LOG_TAG,"NvwfdService failed to run with exception:" + ex.toString());
        }
  }
}
