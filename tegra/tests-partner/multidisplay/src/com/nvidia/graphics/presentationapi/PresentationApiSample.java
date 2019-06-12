/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.tests.graphics.presentationapi;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Resources;
import android.media.MediaPlayer;
import android.media.MediaRouter;
import android.media.MediaRouter.RouteInfo;
import android.net.Uri;
import android.os.Bundle;
import android.view.Display;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;

import java.io.IOException;

public class PresentationApiSample extends Activity
    implements View.OnClickListener, DialogInterface.OnDismissListener, SurfaceHolder.Callback
{
    private static final int HDMIMODE_MIRRORING = 1;
    private static final int HDMIMODE_CINEMA = 2;
    private int currentMode = HDMIMODE_MIRRORING;

    private SurfaceView mVideo;

    private CinemaModePresentation mCinemaModePresentation;
    private MediaRouter mMediaRouter;
    private MediaPlayer mMediaPlayer;

    private Button mHDMIModeButton;

    private boolean mSurfaceCreated;

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        mMediaRouter = (MediaRouter)getSystemService(Context.MEDIA_ROUTER_SERVICE);

        setContentView(R.layout.main);

        Button btn = (Button)findViewById(R.id.btn_selectvideo);
        if(btn != null) btn.setOnClickListener(this);
        btn = (Button)findViewById(R.id.btn_startvideo);
        if(btn != null) btn.setOnClickListener(this);
        btn = (Button)findViewById(R.id.btn_pausevideo);
        if(btn != null) btn.setOnClickListener(this);
        btn = (Button)findViewById(R.id.btn_hideui);
        if(btn != null) btn.setOnClickListener(this);
        mHDMIModeButton = (Button)findViewById(R.id.btn_mode);
        if(mHDMIModeButton != null)  {
            mHDMIModeButton.setOnClickListener(this);
            mHDMIModeButton.setText("Switch HDMI Mode, current mode = MIRRORING");
        }

        mVideo = (SurfaceView)findViewById(R.id.myVideo);
        mVideo.getHolder().addCallback(this);
        mSurfaceCreated = false;
    }

    @Override
    protected void onResume() {
        super.onResume();
        mMediaRouter.addCallback(MediaRouter.ROUTE_TYPE_LIVE_VIDEO, mMediaRouterCallback);
        refreshPresentation();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mMediaRouter.removeCallback(mMediaRouterCallback);
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (data != null) {
            // User has requested a new video
            try {
                if (mMediaPlayer != null) {
                    mMediaPlayer.reset();
                    mMediaPlayer = null;
                }
                mMediaPlayer = new MediaPlayer();
                mMediaPlayer.setDataSource(this, data.getData());
                mMediaPlayer.prepare();
                // Our external display surfaceview doesn't get refreshed when we exit the filechooser
                // so we have to refresh it manually here.
                if (currentMode == HDMIMODE_CINEMA && mCinemaModePresentation != null
                    && mCinemaModePresentation.isShowing())
                    mMediaPlayer.setDisplay(mCinemaModePresentation.getSurfaceView().getHolder());
            } catch (IOException ioex) {
            }
        }
    }

    // SurfaceHolder.Callback
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        // Refresh the mediaplayer destination display when either our main activity surfaceview changes
        // or we have a new presentation surfaceview.
        if (mMediaPlayer != null)
            mMediaPlayer.setDisplay(holder);
        if (holder != mVideo.getHolder())
            mVideo.setVisibility(View.INVISIBLE);
        mSurfaceCreated = true;
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        mSurfaceCreated = false;
    }
    // SurfaceHolder.Callback END

    // DialogInterface.OnDismissListener
    @Override
    public void onDismiss(DialogInterface dialog) {
        if (dialog == mCinemaModePresentation) {
            // Our presentation has been dismissed
            mCinemaModePresentation = null;
            if (mMediaPlayer != null)
                mMediaPlayer.setDisplay(mVideo.getHolder());
        }
    }
    // DialogInterface.OnDismissListener END

    // View.OnClickListener
    @Override
    public void onClick(View v) {
        switch(v.getId())
        {
        case R.id.btn_selectvideo:
            Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
            intent.setType("video/*");
            startActivityForResult(intent, 0);
            break;
        case R.id.btn_startvideo:
            startVideo();
            break;
        case R.id.btn_pausevideo:
            pauseVideo();
            break;
        case R.id.btn_hideui:
            hideSystemUi();
            break;
        case R.id.btn_mode:
            refreshPresentation();
            // Toggle between mirroring and cinema mode
            switchHDMIMode(currentMode == HDMIMODE_CINEMA ? HDMIMODE_MIRRORING : HDMIMODE_CINEMA);
            break;
        }

    }
    // View.OnClickListener END

    // MediaRouter.SimpleCallback
    private final MediaRouter.SimpleCallback mMediaRouterCallback =
        new MediaRouter.SimpleCallback() {

            @Override
            public void onRouteSelected(MediaRouter router, int type, RouteInfo info) {
                refreshPresentation();
            }

            @Override
            public void onRouteUnselected(MediaRouter router, int type, RouteInfo info) {
                refreshPresentation();
            }

            @Override
            public void onRoutePresentationDisplayChanged(MediaRouter router, RouteInfo info) {
                if (mMediaPlayer != null && mMediaPlayer.isPlaying()) {
                    mMediaPlayer.setDisplay(null);
                }
                refreshPresentation();
            }
        };
    //  MediaRouter.SimpleCallback END


    private void startVideo() {
        if (mMediaPlayer != null)
            mMediaPlayer.start();
    }

    private void pauseVideo() {
        if (mMediaPlayer != null)
            mMediaPlayer.pause();
    }

    private void hideSystemUi() {
        View myView = findViewById(R.id.main);
        myView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                                     | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                     | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                     | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                     | View.SYSTEM_UI_FLAG_FULLSCREEN);
    }


    private void switchHDMIMode(int newMode) {
        if (newMode == currentMode)
            return;
        refreshPresentation();
        switch (newMode)
        {
        case HDMIMODE_CINEMA:
            if (mCinemaModePresentation != null) {
                mHDMIModeButton.setText("Switch HDMI Mode, current mode = CINEMA");
                currentMode = HDMIMODE_CINEMA;
                try {
                    mCinemaModePresentation.show();
                    mCinemaModePresentation.getSurfaceView().getHolder().addCallback(this);
                } catch (WindowManager.InvalidDisplayException ex) {
                    mCinemaModePresentation = null;
                }
            }
            break;
        case HDMIMODE_MIRRORING:
            mHDMIModeButton.setText("Switch HDMI Mode, current mode = MIRRORING");
            if (mCinemaModePresentation != null)
                mCinemaModePresentation.dismiss();
            mVideo.setVisibility(View.VISIBLE);
            currentMode = HDMIMODE_MIRRORING;
            break;
        }
    }

    private void refreshPresentation() {
        MediaRouter.RouteInfo route = mMediaRouter.getSelectedRoute(MediaRouter.ROUTE_TYPE_LIVE_VIDEO);
        Display mediaDisplay = (route != null ? route.getPresentationDisplay() : null);

        if (mediaDisplay == null) {
            if (mVideo.getVisibility() != View.VISIBLE)
                mVideo.setVisibility(View.VISIBLE);
            else if (mMediaPlayer != null && mSurfaceCreated)
                mMediaPlayer.setDisplay(mVideo.getHolder());
        }
        if (mCinemaModePresentation != null && mCinemaModePresentation.getDisplay() != mediaDisplay) {
            mCinemaModePresentation.dismiss();
        }
        if (mCinemaModePresentation == null && mediaDisplay != null) {
            mCinemaModePresentation = new CinemaModePresentation(this, mediaDisplay);
            mCinemaModePresentation.setOnDismissListener(this);
            if (currentMode == HDMIMODE_CINEMA) {
                try {
                    mCinemaModePresentation.show();
                    mCinemaModePresentation.getSurfaceView().getHolder().addCallback(this);
                } catch (WindowManager.InvalidDisplayException ex) {
                    mCinemaModePresentation = null;
                }
            }
        }
    }

    private final static class CinemaModePresentation extends Presentation {

        private SurfaceView mVideo;

        public CinemaModePresentation(Context context, Display display) {
            super(context, display);
        }

        @Override
        protected void onCreate(Bundle icicle) {
            super.onCreate(icicle);
            Resources r = getContext().getResources();
            setContentView(R.layout.cinemamode);
            mVideo = (SurfaceView)findViewById(R.id.myCinemaModeVideo);
        }

        public SurfaceView getSurfaceView() {
            return mVideo;
        }
    }

}
