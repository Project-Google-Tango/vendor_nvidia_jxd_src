/*
 * Copyright (c) 2009 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

package com.nvidia.display;

import android.view.Display;
import android.util.Log;
import android.graphics.PixelFormat;
import com.nvidia.display.DisplayMode;

/**
 * @hide
 */

public class MutableDisplay { // TODO: put in android.view to allow Display to be extended
    static {
        System.loadLibrary("nvidia_display_jni");
    };
    static final String TAG = "MutableDisplay";
    
    public static MutableDisplay getPrimary() {
        return new MutableDisplay(0);
    }
    
    public static MutableDisplay[] getAll(boolean connectedOnly) {
        int ids[] = getDisplayIdsNative(connectedOnly);
        MutableDisplay[] result = new MutableDisplay[ids.length];
        for (int i = 0; i<ids.length; ++i)
            result[i] = new MutableDisplay(ids[i]);
        return result;
    }
    
    MutableDisplay(int displayId) {
        mDisplayId = displayId;
    }
    
    // get display mode
    public DisplayMode getDisplayMode() {
        int buffer[] = getDisplayModeNative(mDisplayId);
        return convertFromNative(
            buffer[0], 
            buffer[1], 
            buffer[2], 
            buffer[3]
        );
    }
    
    // set display mode
    public void setDisplayMode(DisplayMode mode) {
        // convert display mode into format expected by native interfaces (see convertFromNative)
        PixelFormat format = new PixelFormat();
        PixelFormat.getPixelFormatInfo(mode.pixelFormat, format);
        setDisplayModeNative(mDisplayId, mode.width, mode.height, format.bitsPerPixel, (int)(mode.refreshRate*(1<<16)+0.5));
    }
    
    // query supported modes
    public DisplayMode[] getSupportedDisplayModes() {
        int buffer[] = getSupportedDisplayModesNative(mDisplayId);
        DisplayMode[] result = new DisplayMode[buffer.length/4];
        for (int i = 0; i<result.length; ++i)
            result[i] = convertFromNative(
                buffer[4*i+0], 
                buffer[4*i+1], 
                buffer[4*i+2], 
                buffer[4*i+3]
            );
        return result;
    }
    
    public native static void setRouteAudioToHdmi(boolean routeAudioToHdmi);
    public native static boolean getRouteAudioToHdmi();
    
    // in the native interfaces, a display mode is described by 4 ints:
    //      width, height, bitsPerPixel, refreshRate*2^16
    private DisplayMode convertFromNative(int width, int height, int bitsPerPixel, int refreshRateFx16) {
        DisplayMode result = new DisplayMode();
        result.width = width; 
        result.height = height;
        result.pixelFormat = (bitsPerPixel<24? PixelFormat.RGB_565: PixelFormat.RGB_888);
        result.refreshRate = refreshRateFx16/(float)(1<<16);
        return result;
    }
    
    private native static int[] getDisplayModeNative(int id);
    private native static void setDisplayModeNative(int id, int width, int height, int bitsPerPixel, int refreshRateFx16);
    private native static int[] getSupportedDisplayModesNative(int id);
    private native static int[] getDisplayIdsNative(boolean connectedOnly);

    private int mDisplayId;
}
