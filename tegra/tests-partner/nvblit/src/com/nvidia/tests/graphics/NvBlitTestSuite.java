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
import android.app.ActivityManager;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.PackageInfo;
import android.content.pm.ActivityInfo;

import android.os.Bundle;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;
import android.graphics.Point;

import java.io.PrintStream;
import java.util.Enumeration;
import java.util.Vector;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

public class NvBlitTestSuite extends Activity implements View.OnClickListener, View.OnTouchListener
{
    // Names for Intent extra parameters passed between tests and testsuite
    public static final String EXTRA_TEST_RESULT = "com.nvidia.tests.graphics.nvblit.test_result";
    public static final String EXTRA_TEST_TIMEOUT = "com.nvidia.tests.graphics.nvblit.test_timeout";

    private Vector<NvBlitTestCase> testCases = new Vector();
    private BlockingQueue testQueue;
    private TestRunner testRunner;
    private boolean isTestRunning = false;

    private int mDragAnchorX, mDragAnchorY;
    private Point mDispSize = new Point();
    private Point mWindowSize = new Point();;

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setContentView(R.layout.main);
        getWindowManager().getDefaultDisplay().getSize(mDispSize);
        WindowManager.LayoutParams lp = this.getWindow().getAttributes();
        lp.flags |= WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH |
            WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
        this.getWindow().setAttributes(lp);
        this.getWindow().getDecorView().setOnTouchListener(this);

        Button btn = (Button)findViewById(R.id.btn_runselected);
        if(btn != null) btn.setOnClickListener(this);
        btn = (Button)findViewById(R.id.btn_selectall);
        if(btn != null) btn.setOnClickListener(this);
        btn = (Button)findViewById(R.id.btn_selectnone);
        if(btn != null) btn.setOnClickListener(this);
        btn = (Button)findViewById(R.id.btn_runall);
        if(btn != null) btn.setOnClickListener(this);

        LinearLayout myLayout = (LinearLayout)findViewById(R.id.mainmenu);
        if(myLayout != null)
        {
            PackageManager pm = getPackageManager();
            PackageInfo pi = null;
            try {
                pi = pm.getPackageInfo(getPackageName(), 1);
            } catch(PackageManager.NameNotFoundException nnfe) {
                // This should never happen as it's our own package
                System.out.println("Package not found: " + nnfe.toString());
            }
            if(pi != null) {
                ActivityInfo tests[] = pi.activities;
                testQueue = new ArrayBlockingQueue(tests.length - 1);
                for(int i = 0, ii = 0; i < tests.length; i++)
                {
                    // For each test, dynamically add: "[] TestLabel <Preview> Result:"
                    String className = tests[i].name.substring(tests[i].name.lastIndexOf("."));
                    // Don't include our testsuite
                    if(!className.equals(getComponentName().getShortClassName()))
                    {
                        LinearLayout ll = new LinearLayout(this);
                        ll.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));
                        myLayout.addView(ll);
                        CheckBox cb = new CheckBox(this);
                        cb.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT, 0.33f));
                        cb.setText(tests[i].loadLabel(pm));
                        ll.addView(cb);
                        Button b = new Button(this);
                        b.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
                        b.setGravity(Gravity.LEFT);
                        b.setText("Preview Test");
                        ll.addView(b);
                        TextView tv = new TextView(this);
                        tv.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT, 0.33f));
                        tv.setGravity(Gravity.RIGHT);
                        tv.setText("Result: N/A");
                        ll.addView(tv);
                        testCases.add(new NvBlitTestCase(ii, b, cb, tv, tests[i].packageName, className));
                        ii++;
                    }
                }

            }
        }

        // Start our testQueue consumer
        testRunner = new TestRunner(testQueue);
        new Thread(testRunner).start();
    }

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
    public void onClick(View v) {
        Enumeration<NvBlitTestCase> e = testCases.elements();
        switch(v.getId())
        {
        case R.id.btn_selectall:
            while(e.hasMoreElements())
                e.nextElement().cb.setChecked(true);
            break;

        case R.id.btn_selectnone:
            while(e.hasMoreElements())
                e.nextElement().cb.setChecked(false);
            for(; e.hasMoreElements(); ((NvBlitTestCase)e.nextElement()).cb.setChecked(false));
            break;

        case R.id.btn_runall:
            while(e.hasMoreElements())
                testQueue.add(e.nextElement());
            break;

        case R.id.btn_runselected:
            NvBlitTestCase tc;
            while (e.hasMoreElements()) {
                tc = e.nextElement();
                if (tc.cb.isChecked())
                    testQueue.add(tc);
            }
            break;
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        synchronized(NvBlitTestSuite.class)
        {
            isTestRunning = false;
        }
        NvBlitTestCase testCase = (NvBlitTestCase)testCases.get(requestCode);
        if(testCase != null && data != null) {
            switch(data.getIntExtra(EXTRA_TEST_RESULT, NvBlitTestResult.RESULT_UNKNOWN))
            {
            case NvBlitTestResult.RESULT_PASS:
                testCase.tv.setText("Result: PASS");
                testCase.tv.setTextColor(0xff00ff00);
                break;

            case NvBlitTestResult.RESULT_FAIL:
                testCase.tv.setText("Result: FAIL");
                testCase.tv.setTextColor(0xffff0000);
                break;

            case NvBlitTestResult.RESULT_UNKNOWN:
                testCase.tv.setText("Result: N/A");
                testCase.tv.setTextColor(0xffffffff);
                break;
            }
        }
    }

    // Class consuming tests added to queue
    private class TestRunner implements Runnable
    {
        private final BlockingQueue queue;

        TestRunner(BlockingQueue q) {
            queue = q;
        }

        @Override
        public void run() {
            try {
                while (true) {
                    NvBlitTestCase tc = (NvBlitTestCase)queue.take();
                    long timeout = NvBlitTestActivity.TEST_TIMEOUT_FOREVER;
                    EditText timeoutView = (EditText)findViewById(R.id.timeout);
                    if(timeoutView != null) {
                        try
                        {
                            timeout = Long.parseLong(timeoutView.getText().toString());
                        }
                        catch (NumberFormatException nfe) {}
                        tc.run(timeout);
                        while(isTestRunning) { };
                    }
                }
            } catch (InterruptedException ex) {
                return;
            }
        }
    }

    // TestCase class
    private class NvBlitTestCase implements View.OnClickListener
    {
        public int id;             // To recognize case returning in onActivityResult()
        public CheckBox cb;        // To see wether to run this test
        public String packageName; // Needed to start the activity
        public String className;   // Needed to start the activity
        public Button btn;         // To register onClickListener for Preview
        public TextView tv;        // Result text

        public NvBlitTestCase(int id, Button btn, CheckBox cb, TextView tv, String packageName, String className) {
            this.id = id;
            this.btn = btn;
            this.btn.setOnClickListener(this);
            this.cb = cb;
            this.tv = tv;
            this.packageName = packageName;
            this.className = className;
        }

        public void run(long timeout) {
            synchronized(NvBlitTestSuite.class)
            {
                // Make sure that only 1 test runs at a time
                isTestRunning = true;
            }
            Intent intent = new Intent();
            intent.setAction("android.intent.action.RUN");
            intent.setComponent(new ComponentName(packageName, packageName + className));
            intent.putExtra(EXTRA_TEST_TIMEOUT, timeout);
            startActivityForResult(intent, id);
        }

        public void onClick(View v) {
            // Preview, run until user exits the testactivity
            run(NvBlitTestActivity.TEST_TIMEOUT_FOREVER);
        }
    }
}
