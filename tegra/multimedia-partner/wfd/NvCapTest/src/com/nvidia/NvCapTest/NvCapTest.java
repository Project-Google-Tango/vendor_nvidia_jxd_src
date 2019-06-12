/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

package com.nvidia.NvCapTest;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;

public class NvCapTest extends Service {

    /** JNI Function Prototypes */
    private native void startTestUsingJNI();
    private native void endTestUsingJNI();
    private native void runtimeUpdateUsingJNI();

    /** Load JNI library */
    static {
        System.loadLibrary("nvcaptest_jni");
    }

    /** Debug Tags */
    private static final String TAG = "NvCapTest";

    /** Client will not need to bind to the service, so we dont care about this method */
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        handleIntent(intent);

        // We want this service to continue running until it is explicitly
        // stopped, so return sticky.
        return START_STICKY;
    }

    private void handleIntent(Intent intent) {

        Log.v(TAG, "handleIntent(): START");

        if (intent == null) {
            Log.v(TAG, "handleIntent(): intent is null. Do nothing.");
            return;
        }

        String ctrlVal = intent.getStringExtra("com.nvidia.NvCapTest.NvCapControl");
        if (ctrlVal == null) {
            Log.v(TAG, "handleIntent(): NvCapControl is not a part of the provided intent");
            return;
        }
        Log.v(TAG, "handleIntent(): NvCapControl value specified in the intent is " + ctrlVal);

        if (ctrlVal.equals("start")) { // Start the NvCap Test
            Log.v(TAG, "handleIntent(): Start NvCap Test.");
            startTestUsingJNI();
        } else if (ctrlVal.equals("end")) { // End NvCap Test
            Log.v(TAG, "handleIntent(): End NvCap Test.");
            endTestUsingJNI();
        } else if (ctrlVal.equals("runtime")) { // Runtime updates
            Log.v(TAG, "handleIntent(): Runtime updates.");
            runtimeUpdateUsingJNI();
        } else {
            Log.v(TAG, "handleIntent(): Unknown extra argument passed with the Intent.");
        }

        Log.v(TAG, "handleIntent(): END");

    }
}

