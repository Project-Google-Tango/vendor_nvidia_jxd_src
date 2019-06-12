/**
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.wfd.nvwfdtest;

import java.util.List;
import java.util.ArrayList;

import android.os.Handler;
import android.os.Message;
import android.content.Context;
import android.util.Log;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.Looper;
import android.widget.Toast;

import com.nvidia.nvwfd.*;

class NvwfdFramerate {
    private static final String LOGTAG = "NvwfdFramerate";
    private int mFrameRateNum;
    private int mFrameRateDen;

    NvwfdFramerate(int frameRateNum, int frameRateDen) {
        Log.e(LOGTAG, "frameRateNum: " + frameRateNum + "frameRateDen: " + frameRateDen);
        mFrameRateNum = frameRateNum;
        mFrameRateDen = frameRateDen;
    }

    boolean equals(int frameRateNum1, int frameRateDen1) {
        if ((mFrameRateNum * frameRateDen1) == (frameRateNum1 * mFrameRateDen)) {
            Log.e(LOGTAG, "framerate is matched");
            return true;
        }
        return false;
    }
}

public class NvwfdPolicyManager {
    private static final String LOGTAG = "NvwfdPolicyManager";
    private Nvwfd mNvwfd;
    private NvwfdConnection mConnection;
    private Context mContext;
    private EventHandler mEventHandler;
    private NvwfdConnectionListener mOnConnectionChangedListener;
    private NvwfdAppListener mNvwfdAppListener;
    private boolean mRenegotiationResult = false;
    private int mValue = 50;
    private boolean mForceResolution = false;
    private static final int MINFRAMEDROPTHRESHOLDPERCENTAGE = 5;
    private static final int MAXFRAMEDROPTHRESHOLDPERCENTAGE = 30;
    private static final int MINVIDEONETWORKBANDWIDTHPERCENTAGE = 30;
    private static final int MAXVIDEONETWORKBANDWIDTHPERCENTAGE = 80;
    private static final int AUDIONETWORKBANDWIDTH = (48000 * 2 * 16);
    private static final int AVAILABLENETWORKBANDWIDTH = 15000000;
    private static final int CONNECTIONBANDWIDTH = 3000000;

    private static final int SOURCEVIDEOFORMATCHANGE_EVENT = 0;
    private static final int DISPLAYSTATUSCHANGE_EVENT = 1;
    private static final int PROTECTEDCONTENT_EVENT = 2;
    private static final int FRAMEDROP_EVENT = 3;
    private static final int NETWORKSTATUSCHANGE_EVENT = 4;

    private static final int CONNECTION_480P = 0;
    private static final int CONNECTION_720P = 1;
    private static final int CONNECTION_1080P = 2;
    private static final int MAX_RESOLUTION_SUPPORTED = (1280 * 720);

    private void renegotiate(
        int width, int height, int frameRateNum, int frameRateDen,
        int bitRate, int transmissionRate) {
        Log.d(LOGTAG, "sourceWidth: " + width + " sourceHeight: " + height +
            " frameRateNum: " + frameRateNum + " frameRateDen: " + frameRateDen +
            " bitRate: " + bitRate + " TransmissionRate:" + transmissionRate);

        Log.e(LOGTAG, "updated resolution and bitrate: ");
        NvwfdVideoFormat video = new NvwfdVideoFormat(
            width,
            height,
            frameRateNum,
            frameRateDen,
            bitRate,
            transmissionRate,
            NvwfdVideoFormat.NVWFD_CODEC_H_264,
            NvwfdVideoFormat.NVWFD_STEREOSCOPIC_NONE);
        NvwfdAudioFormat audio = new NvwfdAudioFormat(
            48000, NvwfdAudioFormat.NVWFD_CODEC_LPCM, 16, 2);
        mConnection.renegotiate(video, audio);
    }

    private void disconnect() {
        mConnection = null;
        if (mNvwfdAppListener != null) {
            mNvwfdAppListener.onDisconnect();
        }
    }

    private int getBitRate(int bandWidth) {
        // get the avaliable network bandwidth for video
        int availableVideoBandwidth = bandWidth -
            (AUDIONETWORKBANDWIDTH + CONNECTIONBANDWIDTH);
        int maxBitRate = (MAXVIDEONETWORKBANDWIDTHPERCENTAGE * availableVideoBandwidth) / 100;
        int minBitRate = (MINVIDEONETWORKBANDWIDTHPERCENTAGE * availableVideoBandwidth) / 100;
        int bitRate = (maxBitRate - minBitRate);
        int offset = (mValue * bitRate) / 100;
        bitRate = minBitRate + offset;

        if (bitRate < 1000000) {
            bitRate = 1000000;
        }
        return bitRate;
    }

    private class EventHandler extends Handler {
        private NvwfdPolicyManager mNvwfdPolicyManager;

        public EventHandler(NvwfdPolicyManager nvwfdPolicyManager, Looper looper) {
            super(looper);
            mNvwfdPolicyManager = nvwfdPolicyManager;
        }

        @Override
        public void handleMessage(Message msg) {
            switch(msg.what) {
                case SOURCEVIDEOFORMATCHANGE_EVENT:
                {
                    Log.e(LOGTAG, "SOURCEVIDEOFORMATCHANGE_EVENT in handler " + msg.what);
                    NvwfdSourceVideoInfo sourceVideoFormat = (NvwfdSourceVideoInfo)msg.obj;
                    NvwfdVideoFormat activeVideoFormat = mConnection.getActiveVideoFormat();
                    Log.d(LOGTAG, "sourceWidth: " + sourceVideoFormat.getWidth() + " sourceHeight: " + sourceVideoFormat.getHeight() +
                        " activeWidth: " + activeVideoFormat.getWidth() + " activeHeight: " + activeVideoFormat.getHeight());

                    int sourceWidth = activeVideoFormat.getWidth();
                    int sourceHeight = activeVideoFormat.getHeight();
                    int frameRateNum = activeVideoFormat.getFrameRateNum();
                    int frameRateDen = activeVideoFormat.getFrameRateDen();
                    int bitRate = getBitRate(AVAILABLENETWORKBANDWIDTH);

                    Log.d(LOGTAG, "new BitRate " + bitRate);
                    int matchedFormatIndex = -1;
                    if (!mForceResolution && AVAILABLENETWORKBANDWIDTH < 15000000) {
                        NvwfdFramerate mNvwfdFrameRate;
                        mNvwfdFrameRate = new NvwfdFramerate(activeVideoFormat.getFrameRateNum(), activeVideoFormat.getFrameRateDen());
                        List<NvwfdVideoFormat> supportedVideofmts = mConnection.getSupportedVideoFormats();
                        for (final NvwfdVideoFormat videoFormat : supportedVideofmts) {
                            if ((videoFormat.getWidth() * videoFormat.getHeight()) < (sourceWidth * sourceHeight) &&
                            ((videoFormat.getWidth() * videoFormat.getHeight()) <= MAX_RESOLUTION_SUPPORTED) &&
                            mNvwfdFrameRate.equals(videoFormat.getFrameRateNum(), videoFormat.getFrameRateDen())) {
                                matchedFormatIndex = supportedVideofmts.indexOf(videoFormat);
                                sourceWidth = videoFormat.getWidth();
                                sourceHeight = videoFormat.getHeight();
                            }
                        }

                        if (matchedFormatIndex != -1) {
                            NvwfdVideoFormat tempVideoFormat = supportedVideofmts.get(matchedFormatIndex);
                            sourceWidth = tempVideoFormat.getWidth();
                            sourceHeight = tempVideoFormat.getHeight();
                            frameRateNum = tempVideoFormat.getFrameRateNum();
                            frameRateDen = tempVideoFormat.getFrameRateDen();
                            Log.d(LOGTAG, "Lowest supported format width " + sourceWidth + " height " + sourceHeight);
                        }
                    }

                    if (matchedFormatIndex != -1 ||
                    bitRate != activeVideoFormat.getBitRate() ||
                    AVAILABLENETWORKBANDWIDTH != activeVideoFormat.getTransmissionRate()) {
                        renegotiate(sourceWidth, sourceHeight, frameRateNum, frameRateDen, bitRate, AVAILABLENETWORKBANDWIDTH);
                    }
                    break;
                }
                case FRAMEDROP_EVENT:
                {
                    Log.e(LOGTAG, "FRAMEDROP_EVENT in handler " + msg.what);
                    NvwfdFrameDropInfo info = (NvwfdFrameDropInfo)msg.obj;
                    NvwfdVideoFormat activeVideoFormat = mConnection.getActiveVideoFormat();
                    float frameRate = (activeVideoFormat.getFrameRateNum() / activeVideoFormat.getFrameRateDen());
                    float minDropThreshold = (frameRate * MINFRAMEDROPTHRESHOLDPERCENTAGE) / 100;
                    float maxDropThreshold = (frameRate * MAXFRAMEDROPTHRESHOLDPERCENTAGE) / 100;

                    Log.e(LOGTAG, "minDropThreshold: " +minDropThreshold + " maxDropThreshold: " + maxDropThreshold);
                    if (!mForceResolution &&
                    (info != null) && (info.getFrameDrops() > ((info.getTime() / 1000.0) * minDropThreshold))) {
                        Log.d(LOGTAG, "framedrops are more than 5%");
                        List<NvwfdVideoFormat> supportedVideofmts = mConnection.getSupportedVideoFormats();
                        int width = 0;
                        int height = 0;
                        int matchedIndex = -1;
                        NvwfdFramerate mNvwfdFrameRate;
                        mNvwfdFrameRate = new NvwfdFramerate(activeVideoFormat.getFrameRateNum(), activeVideoFormat.getFrameRateDen());
                        // Lower resolution than active resolution
                        for (final NvwfdVideoFormat videoFormat : supportedVideofmts) {
                            if (((videoFormat.getWidth() * videoFormat.getHeight()) <
                            (activeVideoFormat.getWidth() * activeVideoFormat.getHeight())) &&
                            ((videoFormat.getWidth() * videoFormat.getHeight()) <= MAX_RESOLUTION_SUPPORTED) &&
                            mNvwfdFrameRate.equals(videoFormat.getFrameRateNum(), videoFormat.getFrameRateDen())) {
                                if ((width * height) < (videoFormat.getWidth() * videoFormat.getHeight())) {
                                    width = videoFormat.getWidth();
                                    height = videoFormat.getHeight();
                                    matchedIndex = supportedVideofmts.indexOf(videoFormat);
                                    Log.d(LOGTAG, "Framedrop matchindex is : " + matchedIndex);
                                }
                            }
                        }
                        if (matchedIndex != -1) {
                            NvwfdVideoFormat tempVideoFormat = supportedVideofmts.get(matchedIndex);
                            int bitRate = getBitRate(AVAILABLENETWORKBANDWIDTH);

                            renegotiate(tempVideoFormat.getWidth(), tempVideoFormat.getHeight(),
                                tempVideoFormat.getFrameRateNum(), tempVideoFormat.getFrameRateDen(),
                                bitRate, AVAILABLENETWORKBANDWIDTH);
                        } else if (info.getFrameDrops() > ((info.getTime() / 1000.0) * maxDropThreshold)) {
                            Log.d(LOGTAG, "No less resoution is found and framedrops are more than 30%");
                            // Disconnect the Miracast connection
                            disconnect();
                        }
                    }
                    break;
                }
                case DISPLAYSTATUSCHANGE_EVENT:
                {
                    NvwfdDisplayStatusInfo status = (NvwfdDisplayStatusInfo)msg.obj;
                    Log.e(LOGTAG, "DISPLAYSTATUSCHANGE_EVENT in handler " + msg.what);

                    // Display is powered off, disconnect the Miracast connection
                    if ((status != null) && status.isDisplayOn() == false) {
                        disconnect();
                    }
                    break;
                }
                case PROTECTEDCONTENT_EVENT:
                {
                    NvwfdProtectionInfo event = (NvwfdProtectionInfo)msg.obj;
                    Log.e(LOGTAG, "PROTECTEDCONTENT_EVENT in handler " + msg.what);

                    // Protected content is playing, inform user about it
                    if ((event != null) && event.isProtected() == true) {
                        Toast.makeText(mContext, "Playing Protected Content...", Toast.LENGTH_LONG).show();
                    }
                    break;
                }
                case NETWORKSTATUSCHANGE_EVENT:
                {
                    NvwfdNetworkInfo info = (NvwfdNetworkInfo)msg.obj;
                    Log.e(LOGTAG, "NETWORKSTATUSCHANGE_EVENT in handler " + msg.what);

                    // Sink device is powered off, disconnect the Miracast connection
                    if ((info != null) && (info.getStatus() == NvwfdNetworkInfo.NVWFD_STATUS_DISCONNECTED)) {
                        disconnect();
                    }
                    break;
                }
                default:
                {
                    Log.e(LOGTAG, "Unknown message type " + msg.what);
                    break;
                }
            }
        }
    }

    public static interface NvwfdAppListener {
        void onDisconnect();
    }

    public void configure(
        Context context, Nvwfd nvwfd, NvwfdConnection connection) {

        Log.d(LOGTAG, "Configure the PolicyManager");
        mContext = context;
        mNvwfd = nvwfd;
        mConnection = connection;
    }
    public void start(NvwfdAppListener nvwfdAppListener) {
        Log.d(LOGTAG, "Start the handler");
        mNvwfdAppListener = nvwfdAppListener;
        if(mConnection != null) {
            mEventHandler = new EventHandler(this, mContext.getMainLooper());

            // Register connection changed listener
            mOnConnectionChangedListener = new NvwfdConnectionListener();
            mConnection.registerConnectionListener(mOnConnectionChangedListener);
        }
    }

    public void stop() {
        Log.d(LOGTAG, "Stop");
        mEventHandler.removeMessages(SOURCEVIDEOFORMATCHANGE_EVENT);
        mEventHandler.removeMessages(DISPLAYSTATUSCHANGE_EVENT);
        mEventHandler.removeMessages(FRAMEDROP_EVENT);
        mEventHandler.removeMessages(PROTECTEDCONTENT_EVENT);
        mEventHandler.removeMessages(NETWORKSTATUSCHANGE_EVENT);
        mNvwfdAppListener = null;
        mConnection = null;
        mEventHandler = null;
    }

    public void updatePolicy(int value) {
        Log.d(LOGTAG, "updatePolicy: " + value);
        value = ((value + 5) / 10) * 10;
        if (value != mValue) {
            mValue = value;
            int bitRate = getBitRate(AVAILABLENETWORKBANDWIDTH);
            NvwfdVideoFormat activeVideoFormat = mConnection.getActiveVideoFormat();
            int sourceWidth = activeVideoFormat.getWidth();
            int sourceHeight = activeVideoFormat.getHeight();
            int frameRateNum = activeVideoFormat.getFrameRateNum();
            int frameRateDen = activeVideoFormat.getFrameRateDen();

            Log.d(LOGTAG, "new BitRate " + bitRate);
            if (!mForceResolution && AVAILABLENETWORKBANDWIDTH < 15000000) {
                int matchedFormatIndex = -1;
                NvwfdFramerate mNvwfdFrameRate;
                mNvwfdFrameRate = new NvwfdFramerate(activeVideoFormat.getFrameRateNum(), activeVideoFormat.getFrameRateDen());

                List<NvwfdVideoFormat> supportedVideofmts = mConnection.getSupportedVideoFormats();
                for (final NvwfdVideoFormat videoFormat : supportedVideofmts) {
                    if ((videoFormat.getWidth() * videoFormat.getHeight()) < (sourceWidth * sourceHeight) &&
                    ((videoFormat.getWidth() * videoFormat.getHeight()) <= MAX_RESOLUTION_SUPPORTED) &&
                    mNvwfdFrameRate.equals(videoFormat.getFrameRateNum(), videoFormat.getFrameRateDen())) {
                        matchedFormatIndex = supportedVideofmts.indexOf(videoFormat);
                        sourceWidth = videoFormat.getWidth();
                        sourceHeight = videoFormat.getHeight();
                    }
                }
                if (matchedFormatIndex != -1) {
                    NvwfdVideoFormat tempVideoFormat = supportedVideofmts.get(matchedFormatIndex);
                    sourceWidth = tempVideoFormat.getWidth();
                    sourceHeight = tempVideoFormat.getHeight();
                    frameRateNum = tempVideoFormat.getFrameRateNum();
                    frameRateDen = tempVideoFormat.getFrameRateDen();
                    Log.d(LOGTAG, "Lowest supported format width " + sourceWidth + " height " + sourceHeight);
                }
            }
            renegotiate(sourceWidth, sourceHeight,
                frameRateNum, frameRateDen, bitRate, AVAILABLENETWORKBANDWIDTH);
        }
    }

    public void forceResolution(boolean value) {
        mForceResolution = value;
    }

    public boolean renegotiateResolution(int value) {
        Log.d(LOGTAG, "renegotiateResolution: " + value);
        int bitRate = getBitRate(AVAILABLENETWORKBANDWIDTH);
        NvwfdVideoFormat activeVideoFormat = mConnection.getActiveVideoFormat();
        int frameRateNum = activeVideoFormat.getFrameRateNum();
        int frameRateDen = activeVideoFormat.getFrameRateDen();
        int sourceWidth = activeVideoFormat.getWidth();
        int sourceHeight = activeVideoFormat.getHeight();

        if (value == CONNECTION_480P) {
            sourceWidth = 640;
            sourceHeight = 480;
        } else if (value == CONNECTION_720P) {
            sourceWidth = 1280;
            sourceHeight = 720;
        } else if (value == CONNECTION_1080P) {
            sourceWidth = 1920;
            sourceHeight = 1080;
        }

        int matchedFormatIndex = -1;
        List<NvwfdVideoFormat> supportedVideofmts = mConnection.getSupportedVideoFormats();
        NvwfdFramerate mNvwfdFrameRate;
        mNvwfdFrameRate = new NvwfdFramerate(activeVideoFormat.getFrameRateNum(), activeVideoFormat.getFrameRateDen());
        for (final NvwfdVideoFormat videoFormat : supportedVideofmts) {
            if ((videoFormat.getWidth() * videoFormat.getHeight()) ==
            (sourceWidth * sourceHeight) &&
            mNvwfdFrameRate.equals(videoFormat.getFrameRateNum(), videoFormat.getFrameRateDen())) {
                matchedFormatIndex = supportedVideofmts.indexOf(videoFormat);
                break;
            }
        }

        if (matchedFormatIndex != -1) {
            NvwfdVideoFormat tempVideoFormat = supportedVideofmts.get(matchedFormatIndex);
            sourceWidth = tempVideoFormat.getWidth();
            sourceHeight = tempVideoFormat.getHeight();
            frameRateNum = tempVideoFormat.getFrameRateNum();
            frameRateDen = tempVideoFormat.getFrameRateDen();
            renegotiate(sourceWidth, sourceHeight,
                frameRateNum, frameRateDen, bitRate, AVAILABLENETWORKBANDWIDTH);
        } else {
            Toast.makeText(mContext, "Format is not Supported...", Toast.LENGTH_SHORT).show();
            return false;
        }
        return true;
    }

    private class NvwfdConnectionListener implements NvwfdConnection.NvwfdConnectionListener {

        @Override
        public void onNetworkChanged(NvwfdNetworkInfo info) {
            Log.d(LOGTAG, "On Network Changed Listener");
            Log.d(LOGTAG, "PeerAddress: " + info.getPeerAddress() + " Status: " + info.getStatus());

            if (mEventHandler != null) {
                mEventHandler.obtainMessage(NETWORKSTATUSCHANGE_EVENT, info).sendToTarget();
            }
        }

        @Override
        public void onSourceVideoChanged(NvwfdSourceVideoInfo format) {
            Log.d(LOGTAG, "On SOurce Video Parameters Changed");
            Log.d(LOGTAG, "Width: " + format.getWidth() + " Height: " + format.getHeight() +
                " isRGB: " + format.isRGB() + " StereoFormat: " + format.getStereoFormat());

            if (mEventHandler != null) {
                mEventHandler.obtainMessage(SOURCEVIDEOFORMATCHANGE_EVENT, format).sendToTarget();
            }
        }

        @Override
        public void onFrameDrop(NvwfdFrameDropInfo info) {
            Log.d(LOGTAG, "On FrameDrop , Dropped " + info.getFrameDrops() + " Frames in " + info.getTime());

            if (mEventHandler != null) {
                mEventHandler.obtainMessage(FRAMEDROP_EVENT, info).sendToTarget();
            }
        }

        @Override
        public void onDisplayStatusChanged(NvwfdDisplayStatusInfo status) {
            Log.d(LOGTAG, "On DisplayStatus Changed, Display is: " + status.isDisplayOn());

            if (mEventHandler != null) {
                mEventHandler.obtainMessage(DISPLAYSTATUSCHANGE_EVENT, status).sendToTarget();
            }
        }

        @Override
        public void onProtectionEvent(NvwfdProtectionInfo event) {
            Log.d(LOGTAG, "On Protection Event, Status: " + event.isProtected());

            if (mEventHandler != null) {
                mEventHandler.obtainMessage(PROTECTEDCONTENT_EVENT, event).sendToTarget();
            }
        }

        @Override
        public void onRenegotiationResult(boolean result) {
            Log.d(LOGTAG, "onRenegotiationResult, result: " + result);
            if (result == true) {
                Toast.makeText(mContext, "Format change is succeeded...", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(mContext, "Format change is failed...", Toast.LENGTH_SHORT).show();
            }
            mRenegotiationResult = result;
        }
    };
}
