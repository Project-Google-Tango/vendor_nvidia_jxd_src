/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
package com.nvidia.NvWFDSvc;

import android.util.Log;

public class IWLBWClient {
    static final String LOG_TAG = "IWLBWClient";

    public static final int BANDWIDTH_EVENT_UP = 0;
    public static final int BANDWIDTH_EVENT_DOWN = 1;
    public static final int BANDWIDTH_EVENT_MIN_UNAVAILABLE = 2;
    public static final int BANDWIDTH_EVENT_MIN_AVAILABLE = 3;
    public static boolean libraryFound = false;

    public static IWLBWClient createRbe(BandwidthEventCallback callback) {
        // The static initializer to load the library will be run first.
        if (libraryFound) {
            return new IWLBWClient(callback);
        } else {
            Log.e(LOG_TAG, "native library not found");
            return null;
        }
    }

    private BandwidthEventCallback mBandwidthEventCallback;

    private IWLBWClient(BandwidthEventCallback callback) {
        mBandwidthEventCallback = callback;
    }

    public interface BandwidthEventCallback {
        public void bandwidthEvent(int event);
    }

    public boolean start(int minMBps, int maxMBps) {
        boolean ret = true;
        try {
            Log.d(LOG_TAG, "native_setup");
            native_setup(this);
            Log.d(LOG_TAG, "native_start");
            native_start(minMBps, maxMBps);
            Log.d(LOG_TAG, "update");
            native_bwUpdated(minMBps);
        } catch (Exception e) {
            Log.e(LOG_TAG, "Exception:start:" + e);
            ret = false;
        }
        return ret;
    }

    // Notify BWE of current bandwidth usage
    public int update(int mbps) {
        native_bwUpdated(mbps);
        return 0;
    }

    public void stop(boolean forcestop) {
        native_stop(forcestop);
        native_finalize();
    }

    // Send event back through the callback
    void sendEvent(int event) {
        if (mBandwidthEventCallback != null) {
            mBandwidthEventCallback.bandwidthEvent(event);
        }
    }

    // Load JNI library
    static {
        try {
            System.load("/system/lib/libwlbwjni.so");
            Log.i(LOG_TAG, "libwlbwjni.so successful");
            libraryFound = true;
        } catch (UnsatisfiedLinkError e) {
            Log.e(LOG_TAG, "Could not load libwlbwjni.so");
        }
    }

    /* Below are the fields/methods required the JNI interface */
    public int mNativeContext;
    public int mListenerContext;

    public native void native_setup(Object thiz);
    public native void native_finalize();
    public native int native_start(int minMBps, int maxMBps);
    public native int native_stop(boolean forcestop);
    public native int native_bwUpdated(int mbps);

    // Callback from JNI.  Forward to the client instance
    static void postEventFromNative(
        Object class_ref,
        int event,
        int ext,
        int zero,
        Object returnString) {
        IWLBWClient client = (IWLBWClient)class_ref;
        if (client != null) {
            client.sendEvent(event);
        }
    }
};
