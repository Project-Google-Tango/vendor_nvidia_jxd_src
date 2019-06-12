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
import android.os.Bundle;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.SweepGradient;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.TextureView;

import com.nvidia.graphics.NvBlit;

public class NvBlitTransformActivity extends NvBlitTestActivity implements TextureView.SurfaceTextureListener {

    private TextureView mOriginalView;
    private TextureView mViews[] = new TextureView[7];

    private int mTransforms[] = {NvBlit.Transform_Rotate90,
                                 NvBlit.Transform_Rotate180,
                                 NvBlit.Transform_Rotate270,
                                 NvBlit.Transform_FlipHorizontal,
                                 NvBlit.Transform_InvTranspose,
                                 NvBlit.Transform_FlipVertical,
                                 NvBlit.Transform_Transpose};

    protected void onSetResultAndExit(NvBlitTestResult testResult) {
        boolean result = true;
        Bitmap original = mOriginalView.getBitmap();
        for (int i = 0; i < 7 && result == true; i++) {
            Bitmap transformed = mViews[i].getBitmap();
            int ow = original.getWidth() - 1;
            int oh = original.getHeight() - 1;
            int tw = transformed.getWidth() - 1;
            int th = transformed.getHeight() - 1;
            switch(mTransforms[i])
            {
            case NvBlit.Transform_Rotate90:
                if (original.getPixel(0, 0) == transformed.getPixel(0, th) &&
                    original.getPixel(ow, 0) == transformed.getPixel(0, 0) &&
                    original.getPixel(ow, oh) == transformed.getPixel(tw, 0) &&
                    original.getPixel(0, oh) == transformed.getPixel(tw, th))
                    result &= true;
                else
                    result &= false;
                break;
            case NvBlit.Transform_Rotate180:
                if (original.getPixel(0, 0) == transformed.getPixel(tw, th) &&
                    original.getPixel(ow, 0) == transformed.getPixel(0, th) &&
                    original.getPixel(ow, oh) == transformed.getPixel(0, 0) &&
                    original.getPixel(0, oh) == transformed.getPixel(tw, 0))
                    result &= true;
                else
                    result &= false;
                break;
            case NvBlit.Transform_Rotate270:
                if (original.getPixel(0, 0) == transformed.getPixel(tw, 0) &&
                    original.getPixel(ow, 0) == transformed.getPixel(tw, th) &&
                    original.getPixel(ow, oh) == transformed.getPixel(0, th) &&
                    original.getPixel(0, oh) == transformed.getPixel(0, 0))
                    result &= true;
                else
                    result &= false;
                break;
            case NvBlit.Transform_FlipHorizontal:
                if (original.getPixel(0, 0) == transformed.getPixel(tw, 0) &&
                    original.getPixel(ow, 0) == transformed.getPixel(0, 0) &&
                    original.getPixel(ow, oh) == transformed.getPixel(0, th) &&
                    original.getPixel(0, oh) == transformed.getPixel(tw, th))
                    result &= true;
                else
                    result &= false;
                break;
            case NvBlit.Transform_InvTranspose:
                if (original.getPixel(0, 0) == transformed.getPixel(tw, th) &&
                    original.getPixel(ow, 0) == transformed.getPixel(tw, 0) &&
                    original.getPixel(ow, oh) == transformed.getPixel(0, 0) &&
                    original.getPixel(0, oh) == transformed.getPixel(0, th))
                    result &= true;
                else
                    result &= false;
                break;
            case NvBlit.Transform_FlipVertical:
                if (original.getPixel(0, 0) == transformed.getPixel(0, th) &&
                    original.getPixel(ow, 0) == transformed.getPixel(tw, th) &&
                    original.getPixel(ow, oh) == transformed.getPixel(tw, 0) &&
                    original.getPixel(0, oh) == transformed.getPixel(0, 0))
                    result &= true;
                else
                    result &= false;
                break;
            case NvBlit.Transform_Transpose:
                if (original.getPixel(0, 0) == transformed.getPixel(0, 0) &&
                    original.getPixel(ow, 0) == transformed.getPixel(0, th) &&
                    original.getPixel(ow, oh) == transformed.getPixel(tw, th) &&
                    original.getPixel(0, oh) == transformed.getPixel(tw, 0))
                    result &= true;
                else
                    result &= false;
                break;
            }
        }
        testResult.setResult(result ?
                             NvBlitTestResult.RESULT_PASS :
                             NvBlitTestResult.RESULT_FAIL);
    }

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setContentView(R.layout.transform);

        NvBlit.open();
        mOriginalView= (TextureView)findViewById(R.id.view1);
        mOriginalView.setSurfaceTextureListener(this);
        int i = 0;
        mViews[i] = (TextureView)findViewById(R.id.view2);
        mViews[++i] = (TextureView)findViewById(R.id.view3);
        mViews[++i] = (TextureView)findViewById(R.id.view4);
        mViews[++i] = (TextureView)findViewById(R.id.view5);
        mViews[++i] = (TextureView)findViewById(R.id.view6);
        mViews[++i] = (TextureView)findViewById(R.id.view7);
        mViews[++i] = (TextureView)findViewById(R.id.view8);
    }

    public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
        Surface surf = new Surface(surface);
        try {
            int[] colors = new int[] {Color.RED, Color.YELLOW, Color.GREEN, Color.CYAN, Color.BLUE, Color.MAGENTA, Color.RED};
            SweepGradient g = new SweepGradient(width/2, height/2, colors, null);
            Paint p = new Paint();
            p.setShader(g);
            Canvas canvas = surf.lockCanvas(new Rect(0, 0, width, height));
            canvas.drawPaint(p);
            surf.unlockCanvasAndPost(canvas);
        } catch (Exception e) {
        }
    }

    public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
    }

    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        return true;
    }

    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
            NvBlit.State state = new NvBlit.State();
            state.clear();
            state.setSrcSurface(surface);
            for (int i = 0; i < 7; i++) {
                SurfaceTexture dst = mViews[i].getSurfaceTexture();
                if (dst != null && NvBlit.frameBegin(dst))
                {
                    state.setTransform(mTransforms[i]);
                    NvBlit.blit(state);
                    NvBlit.frameEnd();
                }
            }
    }
}
