/**
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.wfd.nvwfdtest;

import java.util.ArrayList;
import java.util.List;

import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.text.Editable;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.SeekBar;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.CheckBox;
import android.view.View.OnClickListener;

import com.nvidia.nvwfd.*;

final public class NvwfdActivity extends Activity
    implements OnClickListener {

    private static final String LOGTAG = "NVWFDAPP";
    private static final int SINKSFOUND = 1;
    private static final int CONNECTDONE = 2;
    private static final int DISCONNECT = 3;
    private Nvwfd mNvwfd;
    private boolean mInit = false;
    private List<NvwfdSinkInfo> mSinks;
    private NvwfdSinkInfo mConnectedSink;
    private NvwfdConnection mConnection;

    private ScrollView mScrollView;
    private LinearLayout mLayout;
    private Button mDiscoverSinksButton;
    private Button mGetFormatsButton;
    private Button mDisconnectButton;
    private Button mActiveFormatButton;
    private RadioButton m480pButton;
    private RadioButton m720pButton;
    private RadioButton m1080pButton;
    private TextView mSinkFormatsText;
    private TextView mActiveFormatText;
    private List<Button> mSinkButtonList;
    private SeekBar mPolicySeekBar;
    private RadioGroup mRadioGroup;
    private CheckBox mForceResolution;
    private NvwfdPolicyManager mNvwfdPolicyManager;
    private boolean bForceResolution = false;
    private int mResolutionCheckId = 1;
    private static final int CONNECTION_480P = 0;
    private static final int CONNECTION_720P = 1;
    private static final int CONNECTION_1080P = 2;
    private static Object mSinksLock;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mScrollView = new ScrollView(this);
        mLayout = new LinearLayout(this);
        mDiscoverSinksButton = new Button(this);
        mSinkButtonList = new ArrayList<Button>();

        mLayout.setOrientation(LinearLayout.VERTICAL);
        mScrollView.addView(mLayout);
        setContentView(mScrollView);

        Log.d(LOGTAG, "Create Nvwfd");
        mConnection = null;
        mNvwfd = new Nvwfd();
        mInit = mNvwfd.init(this, new NvwfdListener());
        mSinks = new ArrayList<NvwfdSinkInfo>();
        mSinksLock = new Object();
        Begin();
    }

    @Override
    public void onDestroy() {
        Log.d(LOGTAG, "Disconnect and destroy");
        if(mInit && (mConnection != null)) {
            mOnDisconnectClicked.onClick(mDisconnectButton);
        }
        mNvwfd.deinit();
        super.onDestroy();
    }

    private void Begin() {
        mLayout.removeAllViews();
        mSinks.clear();
        mConnectedSink = null;

        if(mInit == true) {
            WifiManager wifi = (WifiManager)getSystemService(Context.WIFI_SERVICE);
            if(wifi.isWifiEnabled() == false) {
                FatalErrorDialog("Wifi not enabled, please use Android Settings to enable.");
            }
            else {
                mDiscoverSinksButton.setText("Discover Sinks");
                mDiscoverSinksButton.setOnClickListener(mOnDiscoverSinksClicked);
                mDiscoverSinksButton.setEnabled(true);
                mLayout.addView(mDiscoverSinksButton);
            }
        } else {
            FatalErrorDialog("WFD Protocol init failed!");
        }
    }

    private View.OnClickListener mOnDiscoverSinksClicked = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            for(final Button b : mSinkButtonList) {
                mLayout.removeView(b);
            }
            mSinkButtonList.clear();
            mSinks.clear();
            mNvwfd.discoverSinks();
        }
    };

    private void SinksFound() {
        for(final Button b : mSinkButtonList) {
            mLayout.removeView(b);
        }
        mSinkButtonList.clear();
        synchronized(mSinksLock) {
            for (final NvwfdSinkInfo nvwfdSink : mSinks) {
                String text = nvwfdSink.getSsid();
                Button b = new Button(NvwfdActivity.this);
                b.setText("Connect to " + text);
                b.setOnClickListener(mOnConnectClicked);
                mLayout.addView(b);
                mSinkButtonList.add(b);
            }
        }
    }

    private View.OnClickListener mOnConnectClicked = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            for(final Button SinkButton : mSinkButtonList) {
                // Disable all sink buttons until connect finishes
                mDiscoverSinksButton.setEnabled(false);
                SinkButton.setEnabled(false);

                if(v == SinkButton) {
                    int index;
                    // Select Sink
                    index = mSinkButtonList.indexOf(SinkButton);
                    synchronized(mSinksLock) {
                        mConnectedSink = mSinks.get(index);
                    }
                    NvwfdActivity.this.AutheticationModeDialog();
                }
            }
        }
    };

    private void ConnectDone() {
        Log.d(LOGTAG, "Connect done message arrived");

        if(mConnection != null) {
            mNvwfdPolicyManager = new NvwfdPolicyManager();
            if (mNvwfdPolicyManager != null) {
                mNvwfdPolicyManager.configure(this, mNvwfd, mConnection);
                // Register connection changed listener
                mNvwfdPolicyManager.start(new NvwfdAppListener());
            }

            Log.d(LOGTAG, "registerDisconnectionListener from service");
            mConnection.registerDisconnectListener(new NvwfdDisconnectListener());
            // Get Active connection format
            mActiveFormatButton = new Button(this);
            mActiveFormatButton.setText("Get active connection format");
            mActiveFormatButton.setOnClickListener(mOnGetActiveFormatClicked);
            mLayout.addView(mActiveFormatButton);

            // Get connection formats
            mGetFormatsButton = new Button(this);
            mGetFormatsButton.setText("Get connection formats");
            mGetFormatsButton.setOnClickListener(mOnGetFormatsClicked);
            mLayout.addView(mGetFormatsButton);

            // Disconnect
            mDisconnectButton = new Button(this);
            mDisconnectButton.setText("Disconnect");
            mDisconnectButton.setOnClickListener(mOnDisconnectClicked);
            mLayout.addView(mDisconnectButton);

            // 480p connection
            mRadioGroup = new RadioGroup(this);
            mRadioGroup.setOnClickListener(this);
            m480pButton = new RadioButton(this);
            m480pButton.setText("480p");
            m480pButton.setId(CONNECTION_480P);
            m480pButton.setOnClickListener(new RadioButton.OnClickListener() {
                public void onClick(View v) {
                    if (mNvwfdPolicyManager != null &&
                    true == mNvwfdPolicyManager.renegotiateResolution(CONNECTION_480P)) {
                        Log.d(LOGTAG, "480p connection succeeded");
                        mRadioGroup.check(CONNECTION_480P);
                        mResolutionCheckId = CONNECTION_480P;
                    } else {
                        Log.d(LOGTAG, "480p resolution is not supported");
                        mRadioGroup.check(mResolutionCheckId);
                    }
                }
            });
            mRadioGroup.addView(m480pButton);
            // 720p connection
            m720pButton = new RadioButton(this);
            m720pButton.setText("720p");
            m720pButton.setId(CONNECTION_720P);
            m720pButton.setOnClickListener(new RadioButton.OnClickListener() {
                public void onClick(View v) {
                    if (mNvwfdPolicyManager != null &&
                    true == mNvwfdPolicyManager.renegotiateResolution(CONNECTION_720P)) {
                        Log.d(LOGTAG, "720p connection succeeded");
                        mRadioGroup.check(CONNECTION_720P);
                        mResolutionCheckId = CONNECTION_720P;
                    } else {
                        Log.d(LOGTAG, "720p resolution is not supported");
                        mRadioGroup.check(mResolutionCheckId);
                    }
                }
            });
            mRadioGroup.addView(m720pButton);
            // 1080p connection
            m1080pButton = new RadioButton(this);
            m1080pButton.setText("1080p");
            m1080pButton.setId(CONNECTION_1080P);
            m1080pButton.setOnClickListener(new RadioButton.OnClickListener() {
                public void onClick(View v) {
                    if (mNvwfdPolicyManager != null &&
                    true == mNvwfdPolicyManager.renegotiateResolution(CONNECTION_1080P)) {
                        Log.d(LOGTAG, "1080p connection succeeded");
                        mRadioGroup.check(CONNECTION_1080P);
                        mResolutionCheckId = CONNECTION_1080P;
                    } else {
                        Log.d(LOGTAG, "1080p resolution is not supported");
                        mRadioGroup.check(mResolutionCheckId);
                    }
                }
            });
            mRadioGroup.addView(m1080pButton);
            mRadioGroup.check(1);
            mLayout.addView(mRadioGroup);

            mForceResolution = new CheckBox(this);
            mForceResolution.setText("Force Resolution");
            mForceResolution.setChecked(false);
            mForceResolution.setOnClickListener(new CheckBox.OnClickListener() {
                public void onClick(View v) {
                    if (bForceResolution) {
                        mForceResolution.setChecked(false);
                        if (mNvwfdPolicyManager != null) {
                            mNvwfdPolicyManager.forceResolution(false);
                        }
                        bForceResolution = false;
                    } else {
                        mForceResolution.setChecked(true);
                        if (mNvwfdPolicyManager != null) {
                            mNvwfdPolicyManager.forceResolution(true);
                            mNvwfdPolicyManager.renegotiateResolution(mResolutionCheckId);
                        }
                        bForceResolution = true;
                    }
                }
            });
            mLayout.addView(mForceResolution);

            // Policy seek bar
            mPolicySeekBar = new SeekBar(this);
            mPolicySeekBar.setMax(100);
            mPolicySeekBar.setProgress(50);
            mPolicySeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onStopTrackingTouch(SeekBar arg0) {
                }

                @Override
                public void onStartTrackingTouch(SeekBar arg0) {
                }

                @Override
                public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    if (mNvwfdPolicyManager != null) {
                        mNvwfdPolicyManager.updatePolicy(progress);
                    }
                }
            });

            mLayout.addView(mPolicySeekBar);
        } else {
            // Clear sinks and start over
            mSinkButtonList.clear();
            Begin();
        }
    }

    private View.OnClickListener mOnGetFormatsClicked = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            if (mActiveFormatText != null) {
                mLayout.removeView(mActiveFormatText);
            }
            if (mSinkFormatsText != null) {
                mLayout.removeView(mSinkFormatsText);
            }
            mSinkFormatsText = new TextView(NvwfdActivity.this);
            if (mConnection != null) {
                List<NvwfdAudioFormat> audiofmts = mConnection.getSupportedAudioFormats();
                List<NvwfdVideoFormat> videofmts = mConnection.getSupportedVideoFormats();
                for (final NvwfdVideoFormat videofmt : videofmts) {
                    mSinkFormatsText.setText(mSinkFormatsText.getText() + " | " + videofmt.toString());
                }
                mSinkFormatsText.setText(mSinkFormatsText.getText() + "\n VideoFormatCount =" + videofmts.size() + "\n");
                for (final NvwfdAudioFormat audiofmt : audiofmts) {
                    mSinkFormatsText.setText(mSinkFormatsText.getText() + " | " + audiofmt.toString());
                }
                mSinkFormatsText.setText(mSinkFormatsText.getText() + "\n AudioFormatCount =" + audiofmts.size() + "\n");
                mLayout.addView(mSinkFormatsText);
            }
        }
    };

    private View.OnClickListener mOnGetActiveFormatClicked = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            if (mSinkFormatsText != null) {
                mLayout.removeView(mSinkFormatsText);
            }
            if (mActiveFormatText != null) {
                mLayout.removeView(mActiveFormatText);
            }
            mActiveFormatText = new TextView(NvwfdActivity.this);
            if (mConnection != null) {
                NvwfdAudioFormat audiofmt = mConnection.getActiveAudioFormat();
                NvwfdVideoFormat videofmt = mConnection.getActiveVideoFormat();

                mActiveFormatText.setText(mActiveFormatText.getText() + " | " + videofmt.toString());
                mActiveFormatText.setText(mActiveFormatText.getText() + " | " + audiofmt.toString());
                mLayout.addView(mActiveFormatText);
            }
        }
    };

    private View.OnClickListener mOnDisconnectClicked = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            if (mNvwfdPolicyManager != null) {
                mNvwfdPolicyManager.stop();
            }
            if (mConnection != null) {
                mConnection.disconnect();
                mConnection = null;
            }
            Begin();
        }
    };

    private class NvwfdListener implements Nvwfd.NvwfdListener {
        @Override
        public void onDiscoverSinks(List<NvwfdSinkInfo> sinkList) {
            synchronized(mSinksLock) {
                mSinks.clear();
                for (final NvwfdSinkInfo nvwfdSink : sinkList) {
                    mSinks.add(nvwfdSink);
                }
            }
            Message msg = new Message();
            msg.what = SINKSFOUND;
            msgHandler.sendMessage(msg);
        }
        @Override
        public void onConnect(NvwfdConnection connection) {
            if(connection == null) {
                Log.d(LOGTAG, "Connection failed");
            }
            else {
                Log.d(LOGTAG, "Connection succeeded");
            }
            mConnection = connection;
            msgHandler.sendEmptyMessage(CONNECTDONE);
        }
        @Override
        public void discoverSinksResult(boolean result) {
            Log.d(LOGTAG, "discoverSinksResult " + result);
        }
    }

    private Handler msgHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {

            switch(msg.what) {
                case SINKSFOUND:
                {
                    SinksFound();
                    break;
                }
                case CONNECTDONE:
                {
                    ConnectDone();
                    break;
                }
                case DISCONNECT:
                {
                    if (mConnection != null) {
                        mConnection.disconnect();
                    }
                    Begin();
                    break;
                }
            }
        }
    };

    private void FatalErrorDialog(String message) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(message);
        builder.setCancelable(false);
        builder.setPositiveButton("Exit", new DialogInterface.OnClickListener() {
           public void onClick(DialogInterface dialog, int id) {
                NvwfdActivity.this.finish();
           }});
        AlertDialog alert = builder.create();
        alert.show();
    }

    private void AutheticationModeDialog() {
        final CharSequence[] items = {"Push Button", "PIN"};

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Select Authentication Mode");
        builder.setItems(items, new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int item) {
                Log.d(LOGTAG, "Mode selected is " + item);
                // Connect in PUSH buttom mode
                synchronized(mSinksLock) {
                    if(item == 0) {
                        if (mConnectedSink != null  && mSinks.contains(mConnectedSink)) {
                            // Attempt connection
                            Log.d(LOGTAG, "Attempting connect to " + mConnectedSink.getSsid());
                            mNvwfd.connect(mConnectedSink, false);
                            Toast.makeText(NvwfdActivity.this, "Connecting, please wait...", Toast.LENGTH_LONG).show();
                        } else {
                            FatalErrorDialog("Sink not available. Discover sinks again.");
                        }
                    }
                    // Connect in PIN buttom mode
                    else if(item == 1) {
                        if (mConnectedSink != null  && mSinks.contains(mConnectedSink)) {
                            // Attempt connection
                            Log.d(LOGTAG, "Attempting connect to " + mConnectedSink.getSsid());
                            mNvwfd.connect(mConnectedSink, true);
                        } else {
                            FatalErrorDialog("Sink not available. Discover sinks again.");
                        }
                    }
                }
                dialog.cancel();
            }
        });

        AlertDialog alert = builder.create();
        alert.show();
    }

    private class NvwfdAppListener implements NvwfdPolicyManager.NvwfdAppListener {
        @Override
        public void onDisconnect() {
            Log.d(LOGTAG, "onDisconnect ");
            Message msg = new Message();
            msg.what = DISCONNECT;
            msg.obj = null;
            msgHandler.sendMessage(msg);
        }
    }

    private class NvwfdDisconnectListener implements NvwfdConnection.NvwfdDisconnectListener {
        @Override
        public void onDisconnectResult(boolean result) {
            Log.d(LOGTAG, "onDisconnectResult " + result);
        }
    }

    public void onClick(View arg0) {
        try {
            return;
        } catch (Exception e) {
        }
    }
}

