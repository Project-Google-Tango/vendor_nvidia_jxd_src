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

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.SurfaceTexture;
import android.view.TextureView;

import com.nvidia.graphics.NvBlit;

public class NvBlitBasicActivity extends NvBlitTestActivity implements TextureView.SurfaceTextureListener {

    private TextureView mTextureView;
    private int activityResult = -1;

    protected void onSetResultAndExit(NvBlitTestResult testResult)
    {
        Bitmap bm = mTextureView.getBitmap();
        testResult.setResult(bm.getPixel(0, 0) != Color.MAGENTA ? NvBlitTestResult.RESULT_FAIL : NvBlitTestResult.RESULT_PASS);
    }

    @Override
    protected void onCreate(Bundle icicle) {

        super.onCreate(icicle);
        NvBlit.open();
        mTextureView = new TextureView(this);
        mTextureView.setSurfaceTextureListener(this);
        setContentView(mTextureView);

    }

    public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {

        if (NvBlit.frameBegin(surface))
        {
            NvBlit.State state = new NvBlit.State();
            state.clear();
            state.setSrcColor(Color.MAGENTA);

            NvBlit.blit(state);

            NvBlit.frameEnd();
        }
    }

    public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
    }

    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        return true;
    }

    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
    }
}
