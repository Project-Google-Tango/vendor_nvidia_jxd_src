/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.NvWFDSvc;

import java.util.List;
import java.util.ArrayList;
import java.util.LinkedList;

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
import android.os.SystemProperties;

import com.nvidia.nvwfd.*;

class NvwfdFramerate {
    private static final String LOGTAG = "NvwfdFramerate";
    private static final boolean DBG = false;
    private int mFrameRateNum;
    private int mFrameRateDen;

    NvwfdFramerate(int frameRateNum, int frameRateDen) {
        if (DBG) Log.d(LOGTAG, "frameRateNum: " + frameRateNum + "frameRateDen: " + frameRateDen);
        mFrameRateNum = frameRateNum;
        mFrameRateDen = frameRateDen;
    }

    boolean equals(int frameRateNum1, int frameRateDen1) {
        if ((mFrameRateNum * frameRateDen1) == (frameRateNum1 * mFrameRateDen)) {
            if (DBG) Log.d(LOGTAG, "framerate is matched");
            return true;
        }
        return false;
    }
}
public class NvwfdPolicyManager {
    private static final String LOGTAG = "NvwfdPolicyManager";
    private static final String LOGTAG_RBE   = "RBE:";
    private static final String LOGTAG_RBE_UP = "RBE UP:";
    private static final String LOGTAG_RBE_DN = "RBE DN:";
    private boolean DBG = false;
    private boolean KEEP_ALIVE_ENABLED = false;

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

    private static final int MAXAUDIOBANDWIDTH = (48000 * 2 * 16);
    private static final int APCONNECTIONBANDWIDTH = 3000000;

    private static final int SOURCEVIDEOFORMATCHANGE_EVENT = 0;
    private static final int DISPLAYANDAUDIOSTATUSOFF_EVENT = 1;
    private static final int PROTECTEDCONTENT_EVENT = 2;
    private static final int FRAMEDROP_EVENT = 3;
    private static final int NETWORKSTATUSCHANGE_EVENT = 4;

    private static final int DISPLAYBLANK_AND_AUDIOISOFF_DISCONNECT_EVENT = 5;
    private static final int DISPLAYBLANK_AND_AUDIOISOFF_TIMEOUT = 180000;//3 mns

    private static final int RBE_EVENT = 6;
    private static final int GET_MUXER_RATE_EVENT = 7;
    private static final float MUXER_RATE_VALID_THRESHOLD = 0.80f;
    private static final int REENABLE_BUCKETS_EVENT = 8;
    // Current timeout - 2 minutes
    private static final int REENABLE_BUCKETS_TIMEOUT = 2 * 60 * 1000;

    private static final int HDCP_EVENT = 9;
    private static final int GAME_MODE_REQUEST_EVENT = 10;
    private static final int IDR_REQUEST_EVENT = 11;
    private static final long IDR_REQUEST_TIME_DELAY = 500;
    private static final int GET_HDCP_EVENT = 12;
    private static final int STREAM_STATUS_CHANGED_EVENT = 13;

    public static int mFramedropCount;
    private static long mStartReNegotiationTs;
    private static long[] mFramedropTimeStamps;
    private static int mFramedropIndex = 0;
    private static final int MAX_FRAMEDROP_INDEX = 7;
    private static final int FRAMEDROP_THRESHOLD_TIME = 10;
    private static boolean mAllFramedropIndexesFilled = false;
    private static boolean mVideoIsOn = true;
    private static boolean mAudioIsOn = false;
    private static boolean mEnableGameMode;

    public static final int CONNECTION_480P_60FPS = 0;
    public static final int CONNECTION_540P_30FPS = 1;
    public static final int CONNECTION_540P_60FPS = 2;
    public static final int CONNECTION_720P_30FPS = 3;
    public static final int CONNECTION_720P_60FPS = 4;
    public static final int CONNECTION_1080P_30FPS = 5;

    private static final int MAX_RESOLUTION_SUPPORTED = (1280 * 720);
    private boolean adj[][]; //Adjacency matrix for storing graph
    private ArrayList<Frame> al;

    class BitRateBucket {
        BitRateBucket(int w, int h, int n, int d, int v, boolean a, boolean s) {
            width = w;
            height = h;
            frameRateNum = n;
            frameRateDen = d;
            videoRate = v;
            available = a;
            supported = s;
        }

        int width;
        int height;
        int frameRateNum;
        int frameRateDen;
        int videoRate;
        // This boolean is modified depending on the current frame
        // drop status.
        boolean available;

        // This boolean is set to true only if the sink supports this
        // configuration.
        boolean supported;

        public String toString() {
            return
                "width:" + width +
                " height:" + height +
                " frameRateNum:" + frameRateNum +
                " frameRateDen:" + frameRateDen +
                " videoRate:" + videoRate +
                " available:" + available +
                " supported:" + supported;
        }

        public boolean isValid() {
            return available && supported;
        }
    };

    /*
        Both sets of buckets have as the lowest possible bucket 640x480@60fps
        since that is the only mandatory format and therefore guaranteed to
        be supported by every sink.

        Note that 640x480@60fps should be avoided if possible because it looks
        bad.  The aspect ratio is different and most sinks/TVs don't scale it
        up, resulting in large black borders on all four sides.

        The other values were derived from experiments.
     */

    // Bitrate buckets for regular Miracast
    //   --> only the lowest bucket must use 640x480@60fps
    private BitRateBucket[] mDefaultBuckets = new BitRateBucket[]{
        new BitRateBucket( 640, 480, 60000, 1001,  3000000, true, false),
        new BitRateBucket( 960, 540, 30000, 1001,  3500000, true, false),
        new BitRateBucket( 960, 540, 30000, 1001,  4500000, true, false),
        new BitRateBucket(1280, 720, 30000, 1001,  5500000, true, false),
        new BitRateBucket(1280, 720, 30000, 1001,  7000000, true, false),
        new BitRateBucket(1280, 720, 30000, 1001,  9000000, true, false),
        new BitRateBucket(1280, 720, 30000, 1001, 11000000, true, false)
    };

    // Bitrate buckets for use when gaming mode is on
    //   --> only the lowest bucket must use 640x480@60fps
    private BitRateBucket[] mGamingBuckets = new BitRateBucket[]{
        new BitRateBucket( 640, 480, 60000, 1001,  3000000, true, false),
        new BitRateBucket( 960, 540, 60000, 1001,  3500000, true, false),
        new BitRateBucket( 960, 540, 60000, 1001,  4500000, true, false),
        new BitRateBucket( 960, 540, 60000, 1001,  6000000, true, false),
        new BitRateBucket(1280, 720, 60000, 1001,  8000000, true, false),
        new BitRateBucket(1280, 720, 60000, 1001, 10000000, true, false),
        new BitRateBucket(1280, 720, 60000, 1001, 12000000, true, false),
        new BitRateBucket(1280, 720, 60000, 1001, 14000000, true, false)
    };

    // Bitrate buckets for use when 1080p videos/movies are played
    private BitRateBucket[] mHDVideoPlaybackBuckets = new BitRateBucket[]{
        new BitRateBucket( 640, 480, 60000, 1001,  3000000, true, false),
        new BitRateBucket( 960, 540, 30000, 1001,  3500000, true, false),
        new BitRateBucket( 960, 540, 30000, 1001,  4500000, true, false),
        new BitRateBucket(1280, 720, 30000, 1001,  5500000, true, false),
        new BitRateBucket(1280, 720, 30000, 1001,  7000000, true, false),
        new BitRateBucket(1280, 720, 30000, 1001,  9000000, true, false),
        new BitRateBucket(1920, 1080, 30000, 1001, 11000000, true, false),
        new BitRateBucket(1920, 1080, 30000, 1001, 13000000, true, false),
        new BitRateBucket(1920, 1080, 30000, 1001, 15000000, true, false),
        new BitRateBucket(1920, 1080, 30000, 1001, 17000000, true, false)
    };

    // Default is 1280x720, 30 fps, 7mbps
    private static final int DEFAULT_NON_GAMING_BUCKET = 4;
    // Default is 1280x720, 60 fps, 10mbps
    private static final int DEFAULT_GAMING_BUCKET = 5;
    // Default is 1920x1080, 30 fps, 11mbps
    private static final int DEFAULT_VIDEO_PLAYBACK_BUCKET = 6;

    private BitRateBucket[] mBuckets = mDefaultBuckets;
    private int mBucketIndex = DEFAULT_NON_GAMING_BUCKET;
    private int mMinBucketIndex = 0;
    private int mVideoRate = mBuckets[mBucketIndex].videoRate;
    private int mEstimatedMaxRate;

    // Thresholds for increasing and decreasing.  Consecutive events
    // of x threshold must be seen before action is taken.
    private static final int UP_THRESHOLD = 5;
    private static final int DOWN_THRESHOLD = 2;

    // Current thresholds.  Start down at the limit so that it reacts
    // immediately, if necessary.
    int up_count = 0;
    int dn_count = DOWN_THRESHOLD;

    // current muxer video rate
    int mCurVideoRate = -1;
    int mCurTotalRate = -1;

    // Realtime bandwidth estimator object
    IWLBWClient mRbe;

    private boolean renegotiate(
        int width, int height, int frameRateNum, int frameRateDen,
        int videoRate) {
        String gameMode = mEnableGameMode ? "ON" : "OFF";
        Log.d(LOGTAG, "renegotiate  GameMode: " + gameMode.toString() + "bucketIndex: " + mBucketIndex +
            " sourceWidth: " + width + " sourceHeight: " + height +
            " frameRateNum: " + frameRateNum + " frameRateDen: " + frameRateDen +
            " videoRate: " + videoRate);

        if (DBG) Log.d(LOGTAG, "updated resolution and bitrate: ");

        NvwfdVideoFormat video = new NvwfdVideoFormat(
            width,
            height,
            frameRateNum,
            frameRateDen,
            videoRate,
            calculateTransmissionRate(videoRate),
            NvwfdVideoFormat.NVWFD_CODEC_H_264,
            NvwfdVideoFormat.NVWFD_STEREOSCOPIC_NONE);
        mStartReNegotiationTs = System.currentTimeMillis();
        // rare case where audio can be null called when protocol service crashed
        NvwfdAudioFormat audio = mConnection.getActiveAudioFormat();

        if(video != null && audio != null) {
            return mConnection.renegotiate(video, audio);
        }
        return false;
    }

    private void renegotiateWithBucket(int newBucket) {
        BitRateBucket bucket = mBuckets[newBucket];

        if (DBG) {
            Log.d(LOGTAG,
                  "renegotiateWithBucket: bucket:" + newBucket +
                  " " + bucket);

            // Exception is thrown when renegotiation happens from
            // the updatePolicy call.  Disable this debug for now.
            // Toast.makeText(mContext, "Bucket:" + newBucket, Toast.LENGTH_LONG).show();
        }

        NvwfdVideoFormat activeVideoFormat = mConnection.getActiveVideoFormat();
        NvwfdFramerate mNvwfdFrameRate;
        mNvwfdFrameRate = new NvwfdFramerate(bucket.frameRateNum, bucket.frameRateDen);
        if (activeVideoFormat.getWidth() == bucket.width && activeVideoFormat.getHeight() == bucket.height &&
            mNvwfdFrameRate.equals(activeVideoFormat.getFrameRateNum(), activeVideoFormat.getFrameRateDen())) {
            // Bitrate and Transmission rate updation
            if (renegotiate(bucket.width, bucket.height,
                    bucket.frameRateNum, bucket.frameRateDen, bucket.videoRate)) {
                mBucketIndex = newBucket;
                mVideoRate = bucket.videoRate;
            }
        } else if (changeFormat(bucket.width, bucket.height,
                    bucket.frameRateNum, bucket.frameRateDen, false, bucket.videoRate)) {
            mBucketIndex = newBucket;
            mVideoRate = bucket.videoRate;
            initializeFrameDropStructures();
        }
    }

    private void initializeBuckets() {
        initializeBucketsHelper(mDefaultBuckets);
        initializeBucketsHelper(mGamingBuckets);
        initializeBucketsHelper(mHDVideoPlaybackBuckets);
    }

    private void initializeBucketsHelper(BitRateBucket[] buckets) {
        List<NvwfdVideoFormat> supportedVideofmts = mConnection.getSupportedVideoFormats();

        for (BitRateBucket bucket : buckets) {
            for (NvwfdVideoFormat videoFormat : supportedVideofmts) {
                if (bucket.width == videoFormat.getWidth() &&
                    bucket.height == videoFormat.getHeight() &&
                    bucket.frameRateNum == videoFormat.getFrameRateNum() &&
                    bucket.frameRateDen == videoFormat.getFrameRateDen()) {
                    bucket.supported = true;
                    if (DBG) Log.d(LOGTAG, "bucket supported:" + bucket);
                    break;
                }
            }
        }
    }

    // Invalidate all buckets that have resolution greater
    // than or equal to the current bucket
    private void invalidateBuckets() {
        BitRateBucket curBucket = mBuckets[mBucketIndex];
        int maxPels =
            curBucket.width *
            curBucket.height *
            curBucket.frameRateNum /
            curBucket.frameRateDen;

        for (BitRateBucket bucket : mBuckets) {
            int bucketPels = bucket.width * bucket.height * bucket.frameRateNum / bucket.frameRateDen;

            if (bucketPels >= maxPels) {
                bucket.available = false;
                if (DBG) Log.d(LOGTAG, "invalidate bucket:" + bucket);
            }
        }
    }

    private void makeBucketsAvailable() {
        if (DBG) Log.d(LOGTAG, "enable all buckets");
        for (BitRateBucket bucket : mBuckets) {
            bucket.available = true;
        }
    }

    private int increaseBucket() {
        int newBucket;

        for (newBucket = mBucketIndex + 1; newBucket < mBuckets.length; newBucket++) {
            if (mBuckets[newBucket].isValid()) {
                return newBucket;
            }
        }

        // Increase can is only called when the current bucket is valid,
        // so it is safe to return the current bucket when no higher bucket is found.
        return mBucketIndex;
    }

    private int decreaseBucket() {
        if (mBucketIndex == mMinBucketIndex) {
            return mMinBucketIndex;
        }

        // decreaseBucket can be called after some buckets
        // have been invalidated.  So the loop checks all buckets
        // going down.
        for (int i = mBucketIndex - 1; i >= mMinBucketIndex; i--) {
            if (mBuckets[i].isValid()) {
                return i;
            }
        }

        // Nothing found.  Force lowest
        return mMinBucketIndex;
    }

    // Find lowest bucket supported by sink
    // Return current bucket if no lower bucket is found
    private int lowestBucket() {
        int newBucket = mMinBucketIndex;

        // Only check buckets that are lower than current bucket
        while (!mBuckets[newBucket].isValid() && newBucket < mBucketIndex)
            newBucket++;

        return newBucket;
    }

    private void disconnect() {
        mConnection = null;
        if (mNvwfdAppListener != null) {
            mNvwfdAppListener.onDisconnect();
        }
    }

    // The muxer needs an estimate for the transmission rate.
    // Based on experiments, the transmission rate should be 2x video rate
    // and add in the estimates for audio and the AP connection.
    private int calculateTransmissionRate(int videoRate) {
        return (videoRate * 2) + (MAXAUDIOBANDWIDTH + APCONNECTIONBANDWIDTH);
    }

    // Calculate the estimated max bitrate that should be used
    // for RBE.  The reason this is different from the transmission
    // rate is because RBE requires an estimate of the actual
    // bandwidth utilization.
    private int getEstimatedMaxRbeRate(int videoRate) {
        return videoRate + (MAXAUDIOBANDWIDTH + APCONNECTIONBANDWIDTH);
    }

    private boolean checkFrameRateAndAspectRatio(
        int width, int height, int frameRateNum, int frameRateDen, NvwfdVideoFormat activeVideoFormat) {
        int sourceWidth = activeVideoFormat.getWidth();
        int sourceHeight = activeVideoFormat.getHeight();
        NvwfdFramerate mNvwfdFrameRate;
        mNvwfdFrameRate = new NvwfdFramerate(activeVideoFormat.getFrameRateNum(), activeVideoFormat.getFrameRateDen());

        if ((width * height) <= MAX_RESOLUTION_SUPPORTED &&
            mNvwfdFrameRate.equals(frameRateNum, frameRateDen) &&
            (width * sourceHeight) == (height * sourceWidth)) {
            return true;
        }
        return false;
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
                    if (DBG) Log.d(LOGTAG, "SOURCEVIDEOFORMATCHANGE_EVENT in handler " + msg.what);
                    if (mConnection == null) {
                        return;
                    }
                    // Assume a context change.  Re-enable buckets that may have
                    // been invalidated previously
                    makeBucketsAvailable();

                    if (Integer.parseInt(android.os.SystemProperties.get("nvwfd.hdvideoplayback", "1")) == 1) {
                        NvwfdSourceVideoInfo sourceVideoInfo = (NvwfdSourceVideoInfo)msg.obj;
                        BitRateBucket[] newBuckets = null;
                        int newBucketIndex = -1;
                        BitRateBucket[] prevBuckets = mBuckets;
                        int prevIndex = mBucketIndex;
                        int sourceWidth = sourceVideoInfo.getWidth();
                        int sourceHeight = sourceVideoInfo.getHeight();
                        if (!sourceVideoInfo.isRGB() && (sourceWidth * sourceHeight >= 1920*1080)) {
                            // Find the proper HD video bucket
                            newBuckets = mHDVideoPlaybackBuckets;
                            newBucketIndex = findSupportedBucket(mHDVideoPlaybackBuckets, DEFAULT_VIDEO_PLAYBACK_BUCKET);
                        } else if (mBuckets == mHDVideoPlaybackBuckets) {
                            // switching from 1080p to 720p
                            if (mEnableGameMode) {
                                // Find the proper gaming bucket
                                newBuckets = mGamingBuckets;
                                newBucketIndex = findSupportedBucket(mGamingBuckets, DEFAULT_GAMING_BUCKET);
                            } else {
                                newBuckets = mDefaultBuckets;
                                newBucketIndex = findSupportedBucket(mDefaultBuckets, DEFAULT_NON_GAMING_BUCKET);
                            }
                        }

                        if (newBuckets != null && ((newBuckets != prevBuckets) ||
                            (newBucketIndex != prevIndex))) {
                            // Min bucket index should be 1 if sink supports it to avoid 640x480p60
                            mBuckets = newBuckets;
                            mBucketIndex = newBucketIndex;
                            mMinBucketIndex = (mBuckets.length > 1 && mBuckets[1].available) ? 1 : 0;

                            // Reset RBE counts since bitrates may be different
                            up_count = 0;
                            dn_count = DOWN_THRESHOLD;

                            // Finally do the renegotiation
                            renegotiateWithBucket(mBucketIndex);
                        }
                    }

                    // Remove redundant timer messages for re-enabling.
                    mEventHandler.removeMessages(REENABLE_BUCKETS_EVENT);
                    break;
                }
                case FRAMEDROP_EVENT:
                {
                    if (DBG) Log.d(LOGTAG, "FRAMEDROP_EVENT in handler " + msg.what);
                    if (mConnection == null)
                        break;

                    NvwfdFrameDropInfo info = (NvwfdFrameDropInfo)msg.obj;
                    NvwfdVideoFormat activeVideoFormat = mConnection.getActiveVideoFormat();
                    float frameRate = (activeVideoFormat.getFrameRateNum() / activeVideoFormat.getFrameRateDen());
                    float maxDropThreshold = (frameRate * MAXFRAMEDROPTHRESHOLDPERCENTAGE) / 100;

                    if (DBG) Log.d(LOGTAG, "maxDropThreshold: " + maxDropThreshold);
                    if (!mForceResolution) {
                        if (mBucketIndex != mMinBucketIndex) {
                            // Invalidate the buckets that have pels greater than or equal
                            // to the current bucket.
                            invalidateBuckets();

                            // Find the next lowest bucket
                            int newBucket = decreaseBucket();

                            // Renegotiate
                            renegotiateWithBucket(newBucket);

                            // Anticipate that this framedrop condition is temporary
                            // and enable a timer messages that will re-enable the buckets
                            mEventHandler.sendMessageDelayed(
                                mEventHandler.obtainMessage(REENABLE_BUCKETS_EVENT),
                                REENABLE_BUCKETS_TIMEOUT);

                        } else if (info.getFrameDrops() > ((info.getTime() / 1000.0) * maxDropThreshold)) {
                            Log.e(LOGTAG, "No less resoution is found and framedrops are more than " + MAXFRAMEDROPTHRESHOLDPERCENTAGE + "%");
                            // Disconnect the Miracast connection
                            disconnect();
                        }
                    }
                    break;
                }
                case REENABLE_BUCKETS_EVENT:
                {
                    if (DBG) Log.d(LOGTAG, "REENABLE_BUCKETS_EVENT");
                    if (mConnection == null) {
                        break;
                    }
                    makeBucketsAvailable();
                    break;
                }
                case DISPLAYANDAUDIOSTATUSOFF_EVENT:
                {
                    if (DBG) Log.d(LOGTAG, "DISPLAYANDAUDIOSTATUSOFF_EVENT in handler " + msg.what);
                    // Display is powered off and Audio playback is not active, wait for 3 mns to disconnect the Miracast session
                    msg = new Message();
                    msg.what = DISPLAYBLANK_AND_AUDIOISOFF_DISCONNECT_EVENT;
                    mEventHandler.sendMessageDelayed(msg, DISPLAYBLANK_AND_AUDIOISOFF_TIMEOUT);
                    break;
                }
                case PROTECTEDCONTENT_EVENT:
                {
                    NvwfdProtectionInfo event = (NvwfdProtectionInfo)msg.obj;
                    if (DBG) Log.d(LOGTAG, "PROTECTEDCONTENT_EVENT in handler " + msg.what);

                    // Protected content is playing, inform user about it
                    if ((event != null) && event.isProtected() == true) {
                        Toast.makeText(mContext, "Playing Protected Content...", Toast.LENGTH_LONG).show();
                    }
                    break;
                }
                case NETWORKSTATUSCHANGE_EVENT:
                {
                    NvwfdNetworkInfo info = (NvwfdNetworkInfo)msg.obj;
                    if (DBG) Log.d(LOGTAG, "NETWORKSTATUSCHANGE_EVENT in handler " + msg.what);

                    // Sink device is powered off, disconnect the Miracast connection
                    if ((info != null) && (info.getStatus() == NvwfdNetworkInfo.NVWFD_STATUS_DISCONNECTED)) {
                        disconnect();
                    }
                    break;
                }
                case DISPLAYBLANK_AND_AUDIOISOFF_DISCONNECT_EVENT:
                {
                    // Display is powered off and audio playback is not active for longer duration, disconnect the Miracast session
                    if (DBG) Log.d(LOGTAG, "DISPLAYBLANK_AND_AUDIOISOFF_DISCONNECT_EVENT in handler " + msg.what);

                    if (KEEP_ALIVE_ENABLED) {
                       if (DBG) Log.d(LOGTAG," Keep the Miracast connection alive");
                    } else {
                       disconnect();
                    }
                    break;
                }
                case GET_MUXER_RATE_EVENT:
                {
                    // Update muxer rate by polling
                    if ((mRbe == null) || (mConnection == null)) {
                        break;
                    }
                    NvwfdMuxerStats stats = mConnection.getMuxerStats();
                    if (stats != null) {
                        mCurVideoRate = stats.getVideoBitRate();
                        mCurTotalRate = stats.getTotalStreamRate();
                        if (DBG) Log.d(LOGTAG, "GET_MUXER_RATE_EVENT:v:" + mCurVideoRate + " t:" + mCurTotalRate);

                        // Queue message to poll again
                        Message m = mEventHandler.obtainMessage(GET_MUXER_RATE_EVENT);
                        mEventHandler.sendMessageDelayed(m, 1000);
                    } else {
                        if (DBG) Log.d(LOGTAG,"Muxerstats is null");
                    }
                    break;
                }
                case RBE_EVENT:
                {
                    if ((mRbe == null) || (mConnection == null)) {
                        break;
                    }

                    if (DBG) Log.d(LOGTAG, LOGTAG_RBE + "Current: up:" + up_count + " dn:" + dn_count);

                    switch (msg.arg1) {
                    case IWLBWClient.BANDWIDTH_EVENT_UP:
                    {
                        if (DBG) Log.d(LOGTAG, LOGTAG_RBE_UP);

                        // Always prep for the down message to take affect.
                        dn_count = DOWN_THRESHOLD;

                        // Check muxer rate to see if current outgoing rate is a good sample
                        float vidBps = mVideoRate;
                        int defBucketIndex;
                        if (mBuckets == mHDVideoPlaybackBuckets) {
                            defBucketIndex = DEFAULT_VIDEO_PLAYBACK_BUCKET;
                        } else {
                            defBucketIndex = mEnableGameMode ? DEFAULT_GAMING_BUCKET : DEFAULT_NON_GAMING_BUCKET;
                        }
                        if ((mBucketIndex >= defBucketIndex) && (mCurVideoRate < (vidBps * MUXER_RATE_VALID_THRESHOLD))) {
                            if (DBG) Log.d(LOGTAG, LOGTAG_RBE_UP + "video rate too low:" + mCurVideoRate + " (current setting:" + mVideoRate + ")");
                            // Ignore
                            break;
                        }

                        // Wait for some UP events before increasing
                        up_count++;
                        if (up_count < UP_THRESHOLD) {
                            if (DBG) Log.d(LOGTAG, LOGTAG_RBE_UP + "waiting for more UP events:" + up_count);
                            break;
                        }

                        // UP threshold reached.  Reset ...
                        up_count = 0;

                        // And increase bandwidth.
                        int newBucket = increaseBucket();
                        if (newBucket == mBucketIndex) {
                            if (DBG) Log.d(LOGTAG, LOGTAG_RBE_UP + "keeping bucket:" + mBucketIndex);
                            break;
                        }

                        renegotiateWithBucket(newBucket);
                        break;
                    }

                    case IWLBWClient.BANDWIDTH_EVENT_DOWN:
                    {
                        if (DBG) Log.d(LOGTAG, LOGTAG_RBE_DN);

                        // Reset up counter
                        up_count = 0;

                        // Increase down and check
                        dn_count++;
                        if (dn_count < DOWN_THRESHOLD) {
                            if (DBG) Log.d(LOGTAG, LOGTAG_RBE_DN + "not enough down messages");
                            break;
                        }

                        // Down threshold reached.  Reset threshold
                        dn_count = 0;

                        // And try to decrease bandwidth
                        int newBucket = decreaseBucket();
                        if (newBucket == mBucketIndex) {
                            if (DBG) Log.d(LOGTAG, LOGTAG_RBE_DN + "keeping bucket:" + mBucketIndex);
                            break;
                        }

                        renegotiateWithBucket(newBucket);
                        break;
                    }

                    case IWLBWClient.BANDWIDTH_EVENT_MIN_UNAVAILABLE:
                        // RBE is informing us that the specified minimum bandwidth is not available
                        // Let's go to lowest bucket; most likely we're already in a very low bucket
                        if (DBG) Log.d(LOGTAG, LOGTAG_RBE + "MINIMUM UNAVAILABLE message");

                        if (mBucketIndex != mMinBucketIndex) {
                            if (DBG) Log.d(LOGTAG, LOGTAG_RBE + "Dropping to lowest bucket");

                            // Find lowest bucket supported by sink
                            int newBucket = lowestBucket();
                            renegotiateWithBucket(newBucket);

                            dn_count = 0;
                            up_count = 0;
                        } else {
                            if (DBG) Log.d(LOGTAG, LOGTAG_RBE + "Already at lowest");
                        }
                        break;

                    case IWLBWClient.BANDWIDTH_EVENT_MIN_AVAILABLE:
                        // RBE is informing us that the specified minimum bandwidth is available again
                        // No action is required; we just wait for UP messages to change anything
                        if (DBG) Log.d(LOGTAG, LOGTAG_RBE + "MINIMUM AVAILABLE message");
                        break;

                    default:
                        // RBE is sending an unknown message
                        if (DBG) Log.d(LOGTAG, LOGTAG_RBE + "UNKNOWN message");
                        break;
                    }

                    // Update with curent muxer rate
                    int mbs = mCurTotalRate / 1000000;
                    if (mbs < 1) {
                        mbs = 1;
                    } else if (mbs > mEstimatedMaxRate) {
                        mbs = mEstimatedMaxRate;
                    }
                    if (DBG) Log.d(LOGTAG, LOGTAG_RBE + "update:" + mbs + " (muxer:" + mCurTotalRate + ")");
                    if (mRbe != null) {
                        mRbe.update(mbs);
                    }
                    break;
                }
                case HDCP_EVENT:
                {
                    NvwfdHdcpInfo event = (NvwfdHdcpInfo)msg.obj;
                    if (mNvwfdAppListener != null) {
                        mNvwfdAppListener.onHdcpstatus(event.HdcpAuthenticationStatus());
                    }
                    break;
                }
                case GET_HDCP_EVENT:
                {
                    if (mConnection != null) {
                        NvwfdHdcpInfo mNvwfdHdcpInfo = mConnection.getHdcpStatus();
                        if (mNvwfdAppListener != null && mNvwfdHdcpInfo != null) {
                            mNvwfdAppListener.onHdcpstatus(mNvwfdHdcpInfo.HdcpAuthenticationStatus());
                        }
                    }
                    break;
                }
                case GAME_MODE_REQUEST_EVENT:
                {
                    if (mConnection == null) {
                        return;
                    }

                    BitRateBucket[] newBuckets = null;
                    int newBucketIndex = -1;
                    BitRateBucket[] prevBuckets = mBuckets;
                    int prevIndex = mBucketIndex;

                    if (msg.arg1 == 1) {
                        // Find the proper gaming bucket
                        newBuckets = mGamingBuckets;
                        newBucketIndex = findSupportedBucket(mGamingBuckets, DEFAULT_GAMING_BUCKET);
                    } else {
                        newBuckets = mDefaultBuckets;
                        newBucketIndex = findSupportedBucket(mDefaultBuckets, DEFAULT_NON_GAMING_BUCKET);
                    }

                    // Make sure bucket was found
                    if (newBucketIndex < 0) {
                        Log.e(LOGTAG, "No supported buckets found");
                        break;
                    }

                    if ((newBuckets != prevBuckets) ||
                        (newBucketIndex != prevIndex)) {
                        // Min bucket index should be 1 if sink supports it to avoid 640x480p60
                        mBuckets = newBuckets;
                        mBucketIndex = newBucketIndex;
                        mMinBucketIndex = (mBuckets.length > 1 && mBuckets[1].available) ? 1 : 0;

                        mEnableGameMode = (msg.arg1 == 1) ? true : false;

                        // Mode is being switched so start with all buckets available
                        makeBucketsAvailable();

                        // Reset RBE counts since bitrates may be different
                        up_count = 0;
                        dn_count = DOWN_THRESHOLD;

                        // Finally do the renegotiation
                        renegotiateWithBucket(mBucketIndex);
                    } else {
                        if (DBG) Log.d(LOGTAG, "Renegotiation ignored; no bucket change");
                    }
                    break;
                }
                case IDR_REQUEST_EVENT:
                {
                    if (DBG) Log.d(LOGTAG, "Forcing IDR from policy Manager");
                    if (mConnection != null) {
                        mConnection.forceIDR();
                    } else {
                        if (DBG) Log.d(LOGTAG, "Forcing IDR is ignored because mConnection is null");
                    }
                    break;
                }
                case STREAM_STATUS_CHANGED_EVENT:
                {
                    NvwfdStreamStatusInfo event = (NvwfdStreamStatusInfo)msg.obj;
                    if (mNvwfdAppListener != null) {
                        mNvwfdAppListener.onStreamStatus(event.StreamStatus());
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
        void onHdcpstatus(int status);
        void onStreamStatus(int status);
    }

    public void configure(
        Context context, Nvwfd nvwfd, NvwfdConnection connection) {

        if(Integer.parseInt(android.os.SystemProperties.get("nvwfd.debug", "0")) == 1) {
            DBG = true;
        } else {
            DBG = false;
        }

        if(Integer.parseInt(android.os.SystemProperties.get("nvwfd.keep.alive", "0")) == 1) {
            KEEP_ALIVE_ENABLED = true;
        } else {
            KEEP_ALIVE_ENABLED = false;
        }

        if (DBG) Log.d(LOGTAG, "Configure the PolicyManager");
        mContext = context;
        mNvwfd = nvwfd;
        mConnection = connection;
        mFramedropCount = 0;

        mVideoIsOn = true;
        mAudioIsOn = false;

        up_count = 0;
        dn_count = DOWN_THRESHOLD;

        mRenegotiationResult = false;

        mForceResolution = false;
        mFramedropIndex = 0;

        mCurVideoRate = -1;
        mCurTotalRate = -1;
        mAllFramedropIndexesFilled = false;
    }

    private void initializeFrameDropStructures() {
        for (int i = 0; i < MAX_FRAMEDROP_INDEX; i++) {
            mFramedropTimeStamps[i] = Long.MAX_VALUE;
        }
        mFramedropIndex = 0;
        mAllFramedropIndexesFilled = false;
        if (mEventHandler != null) {
            mEventHandler.removeMessages(FRAMEDROP_EVENT);
        }
    }

    public void start(NvwfdAppListener nvwfdAppListener, boolean enableGameMode) {
        if (DBG) Log.d(LOGTAG, "Start the handler");

        mNvwfdAppListener = nvwfdAppListener;
        mEnableGameMode = enableGameMode;

        mEventHandler = new EventHandler(this, mContext.getMainLooper());

        // Register connection changed listener
        mOnConnectionChangedListener = new NvwfdConnectionListener();
        mConnection.registerConnectionListener(mOnConnectionChangedListener);
        NvwfdNetworkInfo info = mConnection.getNetworkStatus();
        if (info != null && mEventHandler != null &&
            info.getStatus() == NvwfdNetworkInfo.NVWFD_STATUS_DISCONNECTED) {
            if (DBG) Log.d(LOGTAG,"mConnection Status is in disconnected state");
            mEventHandler.obtainMessage(NETWORKSTATUSCHANGE_EVENT, info).sendToTarget();
        }
        // Initialize frame drop structures
        mFramedropTimeStamps = new long[MAX_FRAMEDROP_INDEX];
        initializeFrameDropStructures();

        // Initialize bitrate buckets
        initializeBuckets();

        findBucket();

        // Check if RBE should be started
        if (SystemProperties.getInt("nvwfd.rbe", 1) == 1) {
            mRbe = IWLBWClient.createRbe(new RbeCb());
            if (mRbe != null) {
                // Min is 1 mbs
                // Max depends on the highest indexed bucket, and round up
                int maxRateGaming = getEstimatedMaxRbeRate(mGamingBuckets[mGamingBuckets.length - 1].videoRate);
                int maxRateDefault = getEstimatedMaxRbeRate(mDefaultBuckets[mDefaultBuckets.length - 1].videoRate);

                if (Integer.parseInt(android.os.SystemProperties.get("nvwfd.hdvideoplayback", "1")) == 1) {
                    int maxRateVideoPlayback = getEstimatedMaxRbeRate(mHDVideoPlaybackBuckets[mHDVideoPlaybackBuckets.length - 1].videoRate);
                    if (maxRateGaming > maxRateDefault) {
                        if (maxRateGaming > maxRateVideoPlayback) {
                            mEstimatedMaxRate = maxRateGaming;
                        } else {
                            mEstimatedMaxRate = maxRateVideoPlayback;
                        }
                    } else {
                        if (maxRateDefault > maxRateVideoPlayback) {
                            mEstimatedMaxRate = maxRateDefault;
                        } else {
                            mEstimatedMaxRate = maxRateVideoPlayback;
                        }
                    }
                } else {
                    mEstimatedMaxRate = (maxRateGaming > maxRateDefault) ? maxRateGaming : maxRateDefault;
                }
                mEstimatedMaxRate = (mEstimatedMaxRate + 999999) / 1000000;

                // Add factor of +1 so that messages are always sent
                if (mRbe.start(1, mEstimatedMaxRate + 1)) {
                    // Start polling
                    mEventHandler.obtainMessage(GET_MUXER_RATE_EVENT).sendToTarget();
                } else {
                    // Something went wrong.  Don't enable RBE.
                    mRbe = null;
                }
            }
        }
        if (DBG) Log.d(LOGTAG, "RBE is " + ((mRbe != null) ? "enabled" : "disabled"));

        if (DBG) Log.d(LOGTAG, "Quering for HdcpStatus");
        mEventHandler.removeMessages(HDCP_EVENT);
        mEventHandler.obtainMessage(GET_HDCP_EVENT).sendToTarget();
    }

    public void stop() {
        if (DBG) Log.d(LOGTAG, "Stop");
        if (mEventHandler != null) {
            mEventHandler.removeMessages(SOURCEVIDEOFORMATCHANGE_EVENT);
            mEventHandler.removeMessages(DISPLAYANDAUDIOSTATUSOFF_EVENT);
            mEventHandler.removeMessages(FRAMEDROP_EVENT);
            mEventHandler.removeMessages(PROTECTEDCONTENT_EVENT);
            mEventHandler.removeMessages(NETWORKSTATUSCHANGE_EVENT);
            mEventHandler.removeMessages(DISPLAYBLANK_AND_AUDIOISOFF_DISCONNECT_EVENT);
            mEventHandler.removeMessages(RBE_EVENT);
            mEventHandler.removeMessages(GET_MUXER_RATE_EVENT);
            mEventHandler.removeMessages(REENABLE_BUCKETS_EVENT);
            mEventHandler.removeMessages(HDCP_EVENT);
            mEventHandler.removeMessages(GAME_MODE_REQUEST_EVENT);
            mEventHandler.removeMessages(IDR_REQUEST_EVENT);
            mEventHandler.removeMessages(GET_HDCP_EVENT);
            mEventHandler.removeMessages(STREAM_STATUS_CHANGED_EVENT);
        }
        if (mConnection != null) {
            mConnection.registerConnectionListener(null);
        }
        if (mRbe != null) {
            mRbe.stop(false);
        } else {
            //this case might have been due to NvwfdSvc process getting restarted
            mRbe = IWLBWClient.createRbe(null);
            if (mRbe != null) {
                mRbe.stop(true);
            }
        }
        mRbe = null;
        mNvwfdAppListener = null;
        mConnection = null;
        mEventHandler = null;
    }

    private void findBucket() {
        NvwfdVideoFormat activeVideoFormat = mConnection.getActiveVideoFormat();
        if (DBG) Log.d(LOGTAG, "active connection video format :  " + activeVideoFormat.toString());

        // Min bucket index should be 1 if sink supports it to avoid 640x480p60
        mBuckets = mEnableGameMode ? mGamingBuckets : mDefaultBuckets;
        mBucketIndex = mEnableGameMode ? DEFAULT_GAMING_BUCKET : DEFAULT_NON_GAMING_BUCKET;
        mMinBucketIndex = (mBuckets.length > 1 && mBuckets[1].available) ? 1 : 0;

        BitRateBucket bucket = mBuckets[mBucketIndex];
        mVideoRate = bucket.videoRate;

        if (bucket.width != activeVideoFormat.getWidth() || bucket.height != activeVideoFormat.getHeight() ||
            bucket.frameRateNum*activeVideoFormat.getFrameRateDen() != bucket.frameRateDen*activeVideoFormat.getFrameRateNum()) {
            // couldn't connect with default format. Check for any other supported
            // format for the same mode and renegotiate for the same
            int bucketIdx = findSupportedBucket(mBuckets, mBucketIndex);
            if(bucketIdx >= 0) {
                renegotiateWithBucket(bucketIdx);
            } else {
                Log.e(LOGTAG, "Non-compliant Miracast sink does not support 480p60");
            }
        }
    }

    public void updatePolicy(int value) {
        if (DBG) Log.d(LOGTAG, "updatePolicy: " + value);

        if (value > 100) {
            value = 100;
        } else if (value < 0) {
            value = 0;
        }

        // Translate value (range 0 - 100) into bucket
        int newBucket = (value * mBuckets.length) / 100;

        // Do a quick sanity check and make sure the new bucket is supported.
        // Note that the available flag is not checked since this API
        // assumes the caller wants to force a specific bucket.
        for (; newBucket >= 0; newBucket--) {
            if (mBuckets[newBucket].supported) {
                break;
            }
        }
        if (newBucket < 0) {
            newBucket = 0;
        }

        if (newBucket != mBucketIndex && mConnection != null) {
            renegotiateWithBucket(newBucket);
        }
    }

    public NvwfdVideoFormat getDefaultVideoFormat(boolean isGameMode) {
            BitRateBucket bucket = isGameMode ? mGamingBuckets[DEFAULT_GAMING_BUCKET]
                           : mDefaultBuckets[DEFAULT_NON_GAMING_BUCKET];

            NvwfdVideoFormat video = new NvwfdVideoFormat(
                bucket.width,
                bucket.height,
                bucket.frameRateNum,
                bucket.frameRateDen,
                bucket.videoRate,
                calculateTransmissionRate(bucket.videoRate),
                NvwfdVideoFormat.NVWFD_CODEC_H_264,
                NvwfdVideoFormat.NVWFD_STEREOSCOPIC_NONE);

            return video;
    }

    public void forceResolution(boolean value) {
        mForceResolution = value;
    }

    public boolean changeFormat(
        int dstWidth, int dstHeight, int dstFrameNum, int dstFrameDen, boolean useDefaultBitrate, int newBitRate) {
        if (DBG) Log.d(LOGTAG, "changeFormat useDefaultBitrate: " + useDefaultBitrate);

        if (mConnection == null)
            return false;

        NvwfdVideoFormat activeVideoFormat = mConnection.getActiveVideoFormat();
        int matchedFormatIndex = -1;
        List<NvwfdVideoFormat> supportedVideofmts = mConnection.getSupportedVideoFormats();
        if (supportedVideofmts == null || activeVideoFormat == null)
            return false;

        NvwfdFramerate mNvwfdFrameRate;
        mNvwfdFrameRate = new NvwfdFramerate(dstFrameNum, dstFrameDen);
        for (final NvwfdVideoFormat videoFormat : supportedVideofmts) {
            if ((videoFormat.getWidth() * videoFormat.getHeight()) == (dstWidth* dstHeight) &&
            mNvwfdFrameRate.equals(videoFormat.getFrameRateNum(), videoFormat.getFrameRateDen())) {
                matchedFormatIndex = supportedVideofmts.indexOf(videoFormat);
                break;
            }
        }

        if (matchedFormatIndex != -1) {
            al = new ArrayList<Frame>();
            Frame f;
            Frame src,dst;
            for (final NvwfdVideoFormat videoFormat : supportedVideofmts) {
                f = new Frame();
                f.width = videoFormat.getWidth();
                f.height = videoFormat.getHeight();
                f.num = videoFormat.getFrameRateNum();
                f.den = videoFormat.getFrameRateDen();
                al.add(f);
            }
            createGraph();
            src = new Frame();
            dst = new Frame();

            src.width = activeVideoFormat.getWidth();
            src.height = activeVideoFormat.getHeight();
            src.den = activeVideoFormat.getFrameRateDen();
            src.num = activeVideoFormat.getFrameRateNum();

            dst.width = dstWidth;
            dst.height = dstHeight;
            dst.num = dstFrameNum;
            dst.den = dstFrameDen;

            List<Integer> l = findpath(src,dst);
            //This check is used to handle case where is no path from src to dst
            //This case may arise when src and dst are in different connected components of graph
            if (l.size()==0) return false;
            NvwfdVideoFormat tempVideoFormat;
            //loop started from 1 because 0 has src
            for(int i=1;i<l.size();++i) {
                tempVideoFormat = supportedVideofmts.get(l.get(i).intValue());
                if (useDefaultBitrate) {
                    newBitRate = tempVideoFormat.getBitRate();
                }

                // Note that the video rate used here is the video rate
                // supplied by list of supported formats if it is required otherwise
                // supplied value.
                if (!renegotiate(
                    tempVideoFormat.getWidth(),
                    tempVideoFormat.getHeight(),
                    tempVideoFormat.getFrameRateNum(),
                    tempVideoFormat.getFrameRateDen(),
                    newBitRate)) {
                    return false;
                }
            }
        } else {
            return false;
        }
        return true;
    }

    public boolean renegotiateResolution(int value) {
        if (DBG) Log.d(LOGTAG, "renegotiateResolution: " + value);

        if (mConnection == null)
            return false;

        int dstFrameNum = 0, dstFrameDen = 0, dstWidth = 0, dstHeight = 0;
        if (value == CONNECTION_480P_60FPS) {
            dstWidth = 640;
            dstHeight = 480;
            dstFrameNum = 60000;
            dstFrameDen = 1001;
        } else if (value == CONNECTION_540P_30FPS) {
            dstWidth = 960;
            dstHeight = 540;
            dstFrameNum = 30000;
            dstFrameDen = 1001;
        } else if (value == CONNECTION_540P_60FPS) {
            dstWidth = 960;
            dstHeight = 540;
            dstFrameNum = 60000;
            dstFrameDen = 1001;
        }  else if (value == CONNECTION_720P_30FPS) {
            dstWidth = 1280;
            dstHeight = 720;
            dstFrameNum = 30000;
            dstFrameDen = 1001;
        } else if (value == CONNECTION_1080P_30FPS) {
            dstWidth = 1920;
            dstHeight = 1080;
            dstFrameNum = 30000;
            dstFrameDen = 1001;
        }  else if (value == CONNECTION_720P_60FPS) {
            dstWidth = 1280;
            dstHeight = 720;
            dstFrameNum = 60000;
            dstFrameDen = 1001;
        }

        return changeFormat(dstWidth, dstHeight, dstFrameNum, dstFrameDen, true, 0);
    }

    /*
        Creates a graph with nodes repesenting supported video formats and
        Edge between nodes is present if renogitiation is possible between them
    */
    private void createGraph() {
        int size = al.size();
        adj = new boolean[size][size];
        for (int i=0;i<al.size();++i) {
            for (int j=i+1;j<al.size();++j) {
                if (al.get(i).isConvertable(al.get(j))) {
                    adj[i][j] = true;
                    adj[j][i] = true;
                }
                else {
                    adj[i][j] = false;
                    adj[j][i] = false;
                }
            }
        }
    }

    /*
        Finds shortest path from src to dst using BFS(Breadht First Search) on Graph
        Returns the list of index positions in supported format.in which order renegotiations to be made
    */
    private LinkedList<Integer> findpath(Frame from,Frame to) {
        LinkedList<Integer> rs = new LinkedList<Integer>();
        LinkedList<Integer> queue = new LinkedList<Integer>();
        int size = al.size();
        boolean visited[] = new boolean[size];
        int pred[] = new int[size];
        int src=-1,dest=-1;
        int curr_node;
        boolean found = false;
        for (int i=0;i<size;++i) {
            if (al.get(i).isEquals(from)) {
                src = i;
            }
            if (al.get(i).isEquals(to)) {
                dest = i;
            }
            visited[i] = false;
            pred[i] = -1;
        }
        queue.addLast(new Integer(src));
        visited[src] = true;
        while (!queue.isEmpty()) {
            curr_node = queue.removeFirst().intValue();
            int i;
            for (i=0;i<size;++i) {
                if (adj[curr_node][i] && (!visited[i])) {
                    pred[i] = curr_node;
                    if (i==dest) {
                        found = true;
                        break;
                    }
                    visited[i] = true;
                    queue.addLast(new Integer(i));
                }
            }
            if (found) break;
        }
        if (!found) return rs;
        curr_node = dest;
        rs.addFirst(new Integer(dest));
        while (pred[curr_node]!=-1) {
            rs.addFirst(new Integer(pred[curr_node]));
            curr_node = pred[curr_node];
        }
        return rs;
    }

    //Helper class to check whether renegotiation is possible from one format to another
    private class Frame {
        public int height;
        public int width;
        public int num;
        public int den;

        public boolean isConvertable(Frame f) {
            boolean isreseq = ((this.height == f.height) && (this.width == f.width));
            boolean isfrateeq = ((this.num*f.den)==(this.den*f.num));
            if ((isreseq && !isfrateeq) || (!isreseq && isfrateeq)) {
                return true;
            }
            else {
                return false;
            }
        }

        public boolean isEquals(Frame f) {
            if ((this.height==f.height)&&(this.width==f.width)&&(this.den==f.den)&&(this.num==f.num)) return true;
            return false;
        }
    }

    private class NvwfdConnectionListener implements NvwfdConnection.NvwfdConnectionListener {

        @Override
        public void onNetworkChanged(NvwfdNetworkInfo info) {
            if (DBG) Log.d(LOGTAG, "On Network Changed Listener");
            if (DBG) Log.d(LOGTAG, "PeerAddress: " + info.getPeerAddress() + " Status: " + info.getStatus());

            if (mEventHandler != null) {
                mEventHandler.obtainMessage(NETWORKSTATUSCHANGE_EVENT, info).sendToTarget();
            }
        }

        @Override
        public void onSourceVideoChanged(NvwfdSourceVideoInfo format) {
            if (DBG) Log.d(LOGTAG, "On SOurce Video Parameters Changed");
            if (DBG) Log.d(LOGTAG, "Width: " + format.getWidth() + " Height: " + format.getHeight() +
                " isRGB: " + format.isRGB() + " StereoFormat: " + format.getStereoFormat());

            if (mEventHandler != null) {
                mEventHandler.obtainMessage(SOURCEVIDEOFORMATCHANGE_EVENT, format).sendToTarget();
            }
        }

        @Override
        public void onFrameDrop(NvwfdFrameDropInfo info) {
            if (mConnection != null && mEventHandler != null) {
                Log.e(LOGTAG, "On FrameDrop , Dropped " + info.getFrameDrops() + " Frames in " + info.getTime());

                // Framedrops seen.  If the re-enable timer is active, restart it
                // since frame drops are still occuring.
                if (mEventHandler.hasMessages(REENABLE_BUCKETS_EVENT)) {
                    if (DBG) Log.e(LOGTAG, "resetting REENABLE_BUCKETS_EVENT");
                    mEventHandler.removeMessages(REENABLE_BUCKETS_EVENT);
                    mEventHandler.sendMessageDelayed(
                        mEventHandler.obtainMessage(REENABLE_BUCKETS_EVENT),
                        REENABLE_BUCKETS_TIMEOUT);
                }

                mFramedropCount = mFramedropCount + info.getFrameDrops();
                NvwfdVideoFormat activeVideoFormat = mConnection.getActiveVideoFormat();
                float frameRate = (activeVideoFormat.getFrameRateNum() / activeVideoFormat.getFrameRateDen());
                float minDropThreshold = (frameRate * MINFRAMEDROPTHRESHOLDPERCENTAGE) / 100;

                if (DBG) Log.e(LOGTAG, "minDropThreshold: " + minDropThreshold);
                if (info.getFrameDrops() > ((info.getTime() / 1000.0) * minDropThreshold)) {
                    mFramedropTimeStamps[mFramedropIndex] = System.currentTimeMillis();
                    long newTimeStamp = mFramedropTimeStamps[mFramedropIndex];
                    if (DBG) Log.d(LOGTAG, "framedrops are more than 5%, mFramedropIndex: " + mFramedropIndex + " newTimeStamp: " + newTimeStamp);

                    // check all indexes are filled atleast once, then start taking the action
                    mFramedropIndex = (mFramedropIndex + 1) % MAX_FRAMEDROP_INDEX;
                    if (mAllFramedropIndexesFilled == false) {
                        if (mFramedropIndex == 0) {
                            mAllFramedropIndexesFilled = true;
                        } else {
                            return;
                        }
                    }
                    // check if the time difference between new and old timestamps is less than treshold value,
                    // change the resolution/bitrate to reduce the framedrops
                    long timeDiff = (newTimeStamp - mFramedropTimeStamps[mFramedropIndex]) / 1000;
                    if (DBG) Log.d(LOGTAG, "timeDiff: " + timeDiff + " old time stamp: " + mFramedropTimeStamps[mFramedropIndex]);
                    if (timeDiff != 0 && timeDiff < FRAMEDROP_THRESHOLD_TIME) {
                        mEventHandler.obtainMessage(FRAMEDROP_EVENT, info).sendToTarget();
                    }
                }
            }
        }

        @Override
        public void onDisplayStatusChanged(NvwfdDisplayStatusInfo status) {
            if (mEventHandler != null && status != null) {
                if (DBG) Log.d(LOGTAG, "On DisplayStatus Changed, Display is: " + status.isDisplayOn());
                if (status.isDisplayOn()) {
                    mVideoIsOn = true;
                    // Display is enabled, stop disconnecting Miracast session
                    mEventHandler.removeMessages(DISPLAYANDAUDIOSTATUSOFF_EVENT);
                    mEventHandler.removeMessages(DISPLAYBLANK_AND_AUDIOISOFF_DISCONNECT_EVENT);
                } else {
                    mVideoIsOn = false;
                    if (!mAudioIsOn) {
                        mEventHandler.obtainMessage(DISPLAYANDAUDIOSTATUSOFF_EVENT, status).sendToTarget();
                    }
                }
            }
        }

        @Override
        public void onProtectionEvent(NvwfdProtectionInfo event) {
            if (DBG) Log.d(LOGTAG, "On Protection Event, Status: " + event.isProtected());

            if (mEventHandler != null) {
                mEventHandler.obtainMessage(PROTECTEDCONTENT_EVENT, event).sendToTarget();
            }
        }

        @Override
        public void onRenegotiationResult(boolean result) {
            Log.d(LOGTAG, "onRenegotiationResult, result: " + result);
            mRenegotiationResult = result;
            if (DBG) Log.d(LOGTAG, "QoS:Time for Renegotiation is: "+
                (System.currentTimeMillis() - mStartReNegotiationTs) + " millisecs");
            if (result == true) {
                mEventHandler.removeMessages(IDR_REQUEST_EVENT);
                mEventHandler.sendEmptyMessageDelayed(IDR_REQUEST_EVENT, IDR_REQUEST_TIME_DELAY);
            }
        }

        @Override
        public void onAudioStatusChanged(NvwfdAudioStatusInfo status) {
            if (mEventHandler != null && status != null) {
                if (DBG) Log.d(LOGTAG, "On AudioStatus Changed, Audio is: " + status.isAudioOn());
                if (status.isAudioOn()) {
                    mAudioIsOn = true;
                    // Audio playback is started, stop disconnecting Miracast session
                    mEventHandler.removeMessages(DISPLAYANDAUDIOSTATUSOFF_EVENT);
                    mEventHandler.removeMessages(DISPLAYBLANK_AND_AUDIOISOFF_DISCONNECT_EVENT);
                } else {
                    mAudioIsOn = false;
                    if (!mVideoIsOn) {
                        mEventHandler.obtainMessage(DISPLAYANDAUDIOSTATUSOFF_EVENT, status).sendToTarget();
                    }
                }
            }
        }

        @Override
        public void onHdcpEvent(NvwfdHdcpInfo event) {
            if (mEventHandler != null && event != null) {
                if (DBG) Log.d(LOGTAG, "On HDCP Authentication result: " + event.HdcpAuthenticationStatus());
                mEventHandler.removeMessages(GET_HDCP_EVENT);
                mEventHandler.obtainMessage(HDCP_EVENT, event).sendToTarget();
            }
        }

        @Override
        public void onStreamStatusChanged(NvwfdStreamStatusInfo status) {
            if (mEventHandler != null && status != null) {
                if (DBG) Log.d(LOGTAG, "On Stream Status Changed, Stream is: " + status.StreamStatus());
                mEventHandler.removeMessages(STREAM_STATUS_CHANGED_EVENT);
                mEventHandler.obtainMessage(STREAM_STATUS_CHANGED_EVENT, status).sendToTarget();
            }
        }
    };

    class RbeCb implements IWLBWClient.BandwidthEventCallback {
        public void bandwidthEvent(int event) {
            if (DBG) Log.d(LOGTAG, "bandwidthEvent:" + event);
            if (mEventHandler != null && !mForceResolution) {
                mEventHandler.obtainMessage(RBE_EVENT, event, 0, null).sendToTarget();
            }
        }
    }

    public int findSupportedBucket(BitRateBucket[] buckets, int startIndex) {
        int i;

        for (i = startIndex; i >= 0; i--) {
            if (buckets[i].supported) {
                break;
            }
        }
        return i;
    }

    public void setGamingMode(boolean enable) {
        if (DBG) Log.d(LOGTAG, "setGamingMode:" + enable);

        // Request the change in the handler since most all
        // events that trigger a change in buckets occur
        // in the event handler.
        mEventHandler.obtainMessage(
            GAME_MODE_REQUEST_EVENT,
            enable ? 1 : 0,
            0,
            null).sendToTarget();
    }
};
