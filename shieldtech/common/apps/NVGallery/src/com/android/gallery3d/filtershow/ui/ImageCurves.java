/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.gallery3d.filtershow.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.os.AsyncTask;
import android.util.AttributeSet;
import android.view.MotionEvent;

import com.android.gallery3d.R;
import com.android.gallery3d.filtershow.filters.ImageFilterCurves;
import com.android.gallery3d.filtershow.imageshow.ImageSlave;
import com.android.gallery3d.filtershow.presets.ImagePreset;

public class ImageCurves extends ImageSlave {

    private static final String LOGTAG = "ImageCurves";
    Paint gPaint = new Paint();
    Path gPathSpline = new Path();

    private int mCurrentCurveIndex = Spline.RGB;
    private boolean mDidAddPoint = false;
    private boolean mDidDelete = false;
    private ControlPoint mCurrentControlPoint = null;
    private ImagePreset mLastPreset = null;
    int[] redHistogram = new int[256];
    int[] greenHistogram = new int[256];
    int[] blueHistogram = new int[256];
    Path gHistoPath = new Path();

    boolean mDoingTouchMove = false;

    public ImageCurves(Context context) {
        super(context);
        resetCurve();
    }

    public ImageCurves(Context context, AttributeSet attrs) {
        super(context, attrs);
        resetCurve();
    }

    public void nextChannel() {
        mCurrentCurveIndex = ((mCurrentCurveIndex + 1) % 4);
        invalidate();
    }

    @Override
    public boolean showTitle() {
        return false;
    }

    private ImageFilterCurves curves() {
        if (getMaster() != null) {
            String filterName = getFilterName();
            return (ImageFilterCurves) getImagePreset().getFilter(filterName);
        }
        return null;
    }

    private Spline getSpline(int index) {
        return curves().getSpline(index);
    }

    @Override
    public void resetParameter() {
        super.resetParameter();
        resetCurve();
        mLastPreset = null;
        invalidate();
    }

    public void resetCurve() {
        if (getMaster() != null && curves() != null) {
            curves().reset();
            updateCachedImage();
        }
    }

    @Override
    public void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        gPaint.setAntiAlias(true);

        if (getImagePreset() != mLastPreset && getFilteredImage() != null) {
            new ComputeHistogramTask().execute(getFilteredImage());
            mLastPreset = getImagePreset();
        }

        if (curves() == null) {
            return;
        }

        if (mCurrentCurveIndex == Spline.RGB || mCurrentCurveIndex == Spline.RED) {
            drawHistogram(canvas, redHistogram, Color.RED, PorterDuff.Mode.SCREEN);
        }
        if (mCurrentCurveIndex == Spline.RGB || mCurrentCurveIndex == Spline.GREEN) {
            drawHistogram(canvas, greenHistogram, Color.GREEN, PorterDuff.Mode.SCREEN);
        }
        if (mCurrentCurveIndex == Spline.RGB || mCurrentCurveIndex == Spline.BLUE) {
            drawHistogram(canvas, blueHistogram, Color.BLUE, PorterDuff.Mode.SCREEN);
        }
        // We only display the other channels curves when showing the RGB curve
        if (mCurrentCurveIndex == Spline.RGB) {
            for (int i = 0; i < 4; i++) {
                Spline spline = getSpline(i);
                if (i != mCurrentCurveIndex && !spline.isOriginal()) {
                    // And we only display a curve if it has more than two
                    // points
                    spline.draw(canvas, Spline.colorForCurve(i), getWidth(),
                            getHeight(), false, mDoingTouchMove);
                }
            }
        }
        // ...but we always display the current curve.
        getSpline(mCurrentCurveIndex)
                .draw(canvas, Spline.colorForCurve(mCurrentCurveIndex), getWidth(), getHeight(),
                        true, mDoingTouchMove);
        drawToast(canvas);

    }

    private int pickControlPoint(float x, float y) {
        int pick = 0;
        Spline spline = getSpline(mCurrentCurveIndex);
        float px = spline.getPoint(0).x;
        float py = spline.getPoint(0).y;
        double delta = Math.sqrt((px - x) * (px - x) + (py - y) * (py - y));
        for (int i = 1; i < spline.getNbPoints(); i++) {
            px = spline.getPoint(i).x;
            py = spline.getPoint(i).y;
            double currentDelta = Math.sqrt((px - x) * (px - x) + (py - y)
                    * (py - y));
            if (currentDelta < delta) {
                delta = currentDelta;
                pick = i;
            }
        }

        if (!mDidAddPoint && (delta * getWidth() > 100)
                && (spline.getNbPoints() < 10)) {
            return -1;
        }

        return pick;
    }

    private String getFilterName() {
        return "Curves";
    }

    @Override
    public synchronized boolean onTouchEvent(MotionEvent e) {
        float posX = e.getX() / getWidth();
        float posY = e.getY();
        float margin = Spline.curveHandleSize() / 2;
        if (posY < margin) {
            posY = margin;
        }
        if (posY > getHeight() - margin) {
            posY = getHeight() - margin;
        }
        posY = (posY - margin) / (getHeight() - 2 * margin);

        if (e.getActionMasked() == MotionEvent.ACTION_UP) {
            mCurrentControlPoint = null;
            updateCachedImage();
            mDidAddPoint = false;
            if (mDidDelete) {
                mDidDelete = false;
            }
            mDoingTouchMove = false;
            return true;
        }
        mDoingTouchMove = true;

        if (mDidDelete) {
            return true;
        }

        if (curves() == null) {
            return true;
        }

        Spline spline = getSpline(mCurrentCurveIndex);
        int pick = pickControlPoint(posX, posY);
        if (mCurrentControlPoint == null) {
            if (pick == -1) {
                mCurrentControlPoint = new ControlPoint(posX, posY);
                pick = spline.addPoint(mCurrentControlPoint);
                mDidAddPoint = true;
            } else {
                mCurrentControlPoint = spline.getPoint(pick);
            }
        }

        if (spline.isPointContained(posX, pick)) {
            spline.didMovePoint(mCurrentControlPoint);
            spline.movePoint(pick, posX, posY);
        } else if (pick != -1 && spline.getNbPoints() > 2) {
            spline.deletePoint(pick);
            mDidDelete = true;
        }
        updateCachedImage();
        invalidate();
        return true;
    }

    public synchronized void updateCachedImage() {
        // update image
        if (getImagePreset() != null) {
            resetImageCaches(this);
            invalidate();
        }
    }

    class ComputeHistogramTask extends AsyncTask<Bitmap, Void, int[]> {
        @Override
        protected int[] doInBackground(Bitmap... params) {
            int[] histo = new int[256 * 3];
            Bitmap bitmap = params[0];
            int w = bitmap.getWidth();
            int h = bitmap.getHeight();
            int[] pixels = new int[w * h];
            bitmap.getPixels(pixels, 0, w, 0, 0, w, h);
            for (int i = 0; i < w; i++) {
                for (int j = 0; j < h; j++) {
                    int index = j * w + i;
                    int r = Color.red(pixels[index]);
                    int g = Color.green(pixels[index]);
                    int b = Color.blue(pixels[index]);
                    histo[r]++;
                    histo[256 + g]++;
                    histo[512 + b]++;
                }
            }
            return histo;
        }

        @Override
        protected void onPostExecute(int[] result) {
            System.arraycopy(result, 0, redHistogram, 0, 256);
            System.arraycopy(result, 256, greenHistogram, 0, 256);
            System.arraycopy(result, 512, blueHistogram, 0, 256);
            invalidate();
        }
    }

    private void drawHistogram(Canvas canvas, int[] histogram, int color, PorterDuff.Mode mode) {
        int max = 0;
        for (int i = 0; i < histogram.length; i++) {
            if (histogram[i] > max) {
                max = histogram[i];
            }
        }
        float w = getWidth();
        float h = getHeight();
        float wl = w / histogram.length;
        float wh = (0.3f * h) / max;
        Paint paint = new Paint();
        paint.setARGB(100, 255, 255, 255);
        paint.setStrokeWidth((int) Math.ceil(wl));

        Paint paint2 = new Paint();
        paint2.setColor(color);
        paint2.setStrokeWidth(6);
        paint2.setXfermode(new PorterDuffXfermode(mode));
        gHistoPath.reset();
        gHistoPath.moveTo(0, h);
        boolean firstPointEncountered = false;
        float prev = 0;
        float last = 0;
        for (int i = 0; i < histogram.length; i++) {
            float x = i * wl;
            float l = histogram[i] * wh;
            if (l != 0) {
                float v = h - (l + prev) / 2.0f;
                if (!firstPointEncountered) {
                    gHistoPath.lineTo(x, h);
                    firstPointEncountered = true;
                }
                gHistoPath.lineTo(x, v);
                prev = l;
                last = x;
            }
        }
        gHistoPath.lineTo(last, h);
        gHistoPath.lineTo(w, h);
        gHistoPath.close();
        canvas.drawPath(gHistoPath, paint2);
        paint2.setStrokeWidth(2);
        paint2.setStyle(Paint.Style.STROKE);
        paint2.setARGB(255, 200, 200, 200);
        canvas.drawPath(gHistoPath, paint2);
    }

    public void setChannel(int itemId) {
        switch (itemId) {
            case R.id.curve_menu_rgb: {
                mCurrentCurveIndex = Spline.RGB;
                break;
            }
            case R.id.curve_menu_red: {
                mCurrentCurveIndex = Spline.RED;
                break;
            }
            case R.id.curve_menu_green: {
                mCurrentCurveIndex = Spline.GREEN;
                break;
            }
            case R.id.curve_menu_blue: {
                mCurrentCurveIndex = Spline.BLUE;
                break;
            }
        }
        invalidate();
    }
}
