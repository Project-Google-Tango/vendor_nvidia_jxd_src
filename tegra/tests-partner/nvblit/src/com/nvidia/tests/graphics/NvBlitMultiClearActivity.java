/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.tests.graphics.nvblit;

import java.util.Random;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.view.TextureView;

import com.nvidia.graphics.NvBlit;

public class NvBlitMultiClearActivity extends NvBlitTestActivity implements TextureView.SurfaceTextureListener {

    private class Particle {
        float x;
        float y;
        float w;
        float h;
        float dx;
        float dy;
        int c;
    };

    private int mWidth;
    private int mHeight;
    private Random mRandom;
    private Particle[] mParticles;
    private TextureView mTextureView;
    private SurfaceTexture mSurfaceTexture;
    private Handler mHandler = new Handler();
    private Runnable mUpdateTask = new Runnable() {
        public void run() {
            update();
            blit();
            mHandler.postDelayed(this, 16);
        }
    };

    protected void onSetResultAndExit(NvBlitTestResult testResult) {
        Bitmap bm = mTextureView.getBitmap();
        Particle lastParticle = mParticles[mParticles.length-1];
        int c = bm.getPixel((int)lastParticle.x, (int)lastParticle.y);
        testResult.setResult(c == lastParticle.c ?
                             NvBlitTestResult.RESULT_PASS :
                             NvBlitTestResult.RESULT_FAIL);
    }

    @Override
    protected void onCreate(Bundle icicle) {

        super.onCreate(icicle);

        NvBlit.open();

        mRandom = new Random(0);
        mParticles = new Particle[500];
        mTextureView = new TextureView(this);

        mTextureView.setSurfaceTextureListener(this);

        setContentView(mTextureView);
    }

    @Override
    protected void onPause() {
        super.onPause();
        mHandler.removeCallbacks(mUpdateTask);
    }

    public float RandomFloat(float min, float max)
    {
        return (mRandom.nextFloat() * (max - min)) + min;
    }

    public float RandomInt(int min, int max)
    {
        return mRandom.nextInt(max - min) + min;
    }

    public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {

        mHandler.removeCallbacks(mUpdateTask);
        mWidth = width;
        mHeight = height;
        mSurfaceTexture = surface;

        for (int i=0; i<mParticles.length; ++i)
        {
            mParticles[i] = new Particle();
            mParticles[i].x  = RandomFloat(0.0f, mWidth);
            mParticles[i].y  = RandomFloat(0.0f, mHeight);
            mParticles[i].w  = RandomInt(2, 20);
            mParticles[i].h  = RandomInt(2, 20);
            mParticles[i].dx = RandomFloat(-10.0f, 10.0f);
            mParticles[i].dy = RandomFloat(-10.0f, 10.0f);
            mParticles[i].c = mRandom.nextInt();
            // Force the last particle to be opaque so we can do our result check
            if (i == mParticles.length-1)
                mParticles[i].c |= 0xFF000000;
        }

        mHandler.postDelayed(mUpdateTask, 0);
    }

    public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {

    }

    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {

        return true;
    }

    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
    }

    public void blit() {

        if (NvBlit.frameBegin(mSurfaceTexture))
        {
            RectF rect = new RectF();
            NvBlit.State state = new NvBlit.State();

            state.clear();
            state.setSrcColor(Color.BLACK);
            NvBlit.blit(state);

            for (int i=0; i<mParticles.length; ++i)
            {
                rect.left   = mParticles[i].x - mParticles[i].w;
                rect.top    = mParticles[i].y - mParticles[i].h;
                rect.right  = mParticles[i].x + mParticles[i].w;
                rect.bottom = mParticles[i].y + mParticles[i].h;

                if (rect.left < 0.0f)
                    rect.left = 0.0f;

                if (rect.top < 0.0f)
                    rect.top = 0.0f;

                if (rect.right > mWidth)
                    rect.right = mWidth;

                if (rect.bottom > mHeight)
                    rect.bottom = mHeight;

                state.clear();
                state.setDstRect(rect);
                state.setSrcColor(mParticles[i].c);

                NvBlit.blit(state);
            }

            NvBlit.frameEnd();
        }
    }

    public void update() {

        for (int i=0; i<mParticles.length; ++i)
        {
            mParticles[i].x += mParticles[i].dx;
            mParticles[i].y += mParticles[i].dy;

            if (mParticles[i].x < 0.0f)
                mParticles[i].x = mWidth;
            else if (mParticles[i].x > mWidth)
                mParticles[i].x = 0.0f;

            if (mParticles[i].y < 0.0f)
                mParticles[i].y = mHeight;
            else if (mParticles[i].y > mHeight)
                mParticles[i].y = 0.0f;
        }
    }
}
