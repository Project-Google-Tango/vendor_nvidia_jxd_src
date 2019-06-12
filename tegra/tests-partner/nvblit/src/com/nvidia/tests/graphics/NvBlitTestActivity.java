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
import android.os.Handler;

import android.app.ActivityManager;
import android.graphics.Point;
import android.view.WindowManager;
import android.view.View;
import android.view.MotionEvent;

public abstract class NvBlitTestActivity extends Activity implements View.OnTouchListener
{
    public static final int TEST_TIMEOUT_FOREVER = -1;

    private Thread timerThread;
    private Handler testHandler;
    private long timeout;
    private NvBlitTestResult testResult;

    private int mDragAnchorX, mDragAnchorY;
    private Point mDispSize = new Point();
    private Point mWindowSize = new Point();

    private Runnable testStopper = new Runnable() {
            @Override
            public void run() {
                onSetResultAndExit(testResult);
                exitTest();
            }
        };

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        switch (event.getAction())
        {
        case MotionEvent.ACTION_DOWN:
            // Make sure our window size is correct so we can anchor our drag correctly
            mWindowSize.set(this.getWindow().getDecorView().getWidth(), this.getWindow().getDecorView().getHeight());
            // Grab focus if we don't have it
            ActivityManager am = (ActivityManager) this.getSystemService(ACTIVITY_SERVICE);
            am.moveTaskToFront(this.getTaskId(),0);
            mDragAnchorX = (int) event.getX();
            mDragAnchorY = (int) event.getY();
            break;
        case MotionEvent.ACTION_MOVE:
            int x = (int)event.getRawX();
            int y = (int)event.getRawY();
            WindowManager.LayoutParams lp = this.getWindow().getAttributes();
            lp.x = x - (mDispSize.x / 2) + ((mWindowSize.x / 2) - mDragAnchorX);
            lp.y = y - (mDispSize.y / 2) + ((mWindowSize.y / 2) - mDragAnchorY);
            this.getWindow().setAttributes(lp);
            break;
        }
        return true;
    }

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        getWindowManager().getDefaultDisplay().getSize(mDispSize);
        WindowManager.LayoutParams lp = this.getWindow().getAttributes();
        lp.flags |= WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH |
            WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
        this.getWindow().setAttributes(lp);
        this.getWindow().getDecorView().setOnTouchListener(this);

        testHandler = new Handler();
        testResult = new NvBlitTestResult();
        Intent intent = getIntent();
        if(intent.getAction().equals("android.intent.action.RUN"))
        {
            timeout = intent.getLongExtra(NvBlitTestSuite.EXTRA_TEST_TIMEOUT, TEST_TIMEOUT_FOREVER);
            if(timeout > TEST_TIMEOUT_FOREVER)
                testHandler.postDelayed(testStopper, timeout);
        }
    }

    abstract void onSetResultAndExit(NvBlitTestResult result);

    private void exitTest() {
        Intent intent = getIntent();
        intent.putExtra(NvBlitTestSuite.EXTRA_TEST_RESULT, testResult.getResult());
        setResult(RESULT_OK, intent);
        finish();
    }
}
