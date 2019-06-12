/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.NvwfdTest;

import java.util.ArrayList;
import java.util.List;

import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
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
import android.content.Context;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.Binder;
import android.os.SystemProperties;
import android.content.ServiceConnection;
import android.content.ComponentName;
import android.widget.LinearLayout.LayoutParams;
import android.graphics.Color;
import android.widget.SeekBar;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.CheckBox;
import android.view.View.OnClickListener;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.view.Menu;
import android.os.RemoteException;

import com.nvidia.NvWFDSvc.*;

final public class NvwfdTestActivity extends Activity
                       implements OnClickListener {
    private static final String LOGTAG = "NvwfdTestActivity";
    private static final boolean DBG = true;
    private static final int SINKSFOUND = 1;
    private static final int CONNECTDONE = 2;
    private static final int DISCONNECT = 3;
    private static final int RESETSINKS = 4;
    private static final int HANDLEERROR = 5;
    private static final int DISCOVERYSTATUS = 6;
    private static final int CANCELCONNECTION = 7;
    private ScrollView mScrollView;
    private LinearLayout mLayout;
    private LinearLayout sinkListView;
    private Button mGetFormatsButton;
    private Button mActiveFormatButton;
    private RadioButton m480p60Button;
    private RadioButton m720p30Button;
    private RadioButton m540p30Button;
    private RadioButton m540p60Button;
    private RadioButton m720p60Button;
    private RadioButton m1080p30Button;
    private Button mDisconnectButton;
    private Button mPauseButton;
    private Button mResumeButton;
    private Button sinkClicked;
    private TextView mSinkFormatsText;
    private TextView mActiveFormatText;
    private List<Button> mSinkButtonList;
    private INvWFDRemoteService mServiceMonitor = null;
    private Context mWfdContext;
    private String mSinkSSID;
    private RadioGroup mRadioGroup;
    private CheckBox mForceResolution;
    private SeekBar mPolicySeekBar;
    private boolean bForceResolution = false;
    private static final int CONNECTION_480P_60FPS = 0;
    private static final int CONNECTION_540P_30FPS = 1;
    private static final int CONNECTION_540P_60FPS = 2;
    private static final int CONNECTION_720P_30FPS = 3;
    private static final int CONNECTION_720P_60FPS = 4;
    private static final int CONNECTION_1080P_30FPS = 5;

    private int mResolutionCheckId = CONNECTION_720P_30FPS;
    private AlertDialog mCancelAlertDialog = null;
    private boolean bConnectToOther = false;
    private String connectionModeStr = null;
    private static long mConnectionTs;
    private static long mDisconnectionTs;
    private static long mDiscoveryTs;
    private boolean mRefreshON = false;
    private int mConnectionId = -1;
    private boolean mIsSetupComplete = false;

    //binding with service implementation
    private boolean mIsBound;
    private ServiceConnection mServiceConnection = new ServiceConnection() {
        // On connection established with service, this gets called.
        public void onServiceConnected(ComponentName className, IBinder service) {
            if (DBG) Log.d(LOGTAG, "onServiceConnected+");
            // Get service object to interact with the service.
            mServiceMonitor = INvWFDRemoteService.Stub.asInterface(service);
            if (DBG) Log.d(LOGTAG, "mServiceMonitor: " + mServiceMonitor);

            try {
                mConnectionId = mServiceMonitor.createConnection(listener);
                if (mConnectionId < 0) {
                    throw new Exception("couldn't connect to service !!");
                }
            } catch (Exception ex) {
                ex.printStackTrace();
                Log.e(LOGTAG, "Exception in onServiceConnected");
            }
            if (DBG) Log.d(LOGTAG, "onServiceConnected-");
        }
        // When service is disconnected this gets called.
        public void onServiceDisconnected(ComponentName className) {
            mServiceMonitor = null;
            if (DBG) Log.d(LOGTAG, "onServiceDisconnected");
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mWfdContext = getApplicationContext();
        mScrollView = new ScrollView(this);
        mLayout = new LinearLayout(this);
        sinkListView  = new LinearLayout(this);
        sinkListView.setOrientation(LinearLayout.VERTICAL);
        mSinkButtonList = new ArrayList<Button>();

        mLayout.setOrientation(LinearLayout.VERTICAL);
        mLayout.addView(sinkListView);
        mScrollView.addView(mLayout);
        setContentView(mScrollView);
        if (DBG) Log.d(LOGTAG, "Create Nvwfd and bind service");
    }

    @Override
    public void onStart() {
        if (DBG) Log.d(LOGTAG, "onStart +");
        super.onStart();
        doBindService();
        if (DBG) Log.d(LOGTAG, "onStart -");
    }

    @Override
    public void onStop() {
        if (DBG) Log.d(LOGTAG, "onStop +");
        super.onStop();
        if (mCancelAlertDialog != null) {
            mCancelAlertDialog.cancel();
        }
        if (mServiceMonitor != null) {
            try {
                mServiceMonitor.closeConnection(mConnectionId);
            } catch (Exception ex) {
                if (DBG) Log.d(LOGTAG, ex.getMessage());
            }
       }

        doUnbindService();
        if (DBG) Log.d(LOGTAG, "onStop -");
    }

    void doBindService() {
        // Attach connection with NvwfdService.
        if (DBG) Log.d(LOGTAG, "doBindService+");
        Intent intent = new Intent();
        intent.setClassName("com.nvidia.NvWFDSvc", "com.nvidia.NvWFDSvc.NvwfdService");
        if (DBG) Log.d(LOGTAG, "mServiceConnection " + mServiceConnection);
        mIsBound = mWfdContext.bindService(intent, mServiceConnection, Context.BIND_AUTO_CREATE);
        if (DBG) Log.d(LOGTAG, "doBindService-");
    }

    void doUnbindService() {
        if (mIsBound) {
            if (DBG) Log.d(LOGTAG, "doUnbindService");
            //Detach existing connection with the service.
            if (mServiceMonitor != null ) {
                mWfdContext.unbindService(mServiceConnection);
                mIsBound = false;
                mIsSetupComplete = false;
                mRefreshON = false;
                mServiceMonitor = null;
            }
        }
    }

    private INvWFDServiceListener.Stub listener = new INvWFDServiceListener.Stub() {
        public void onSinksFound(List text) {
            if (DBG) Log.d(LOGTAG, "Sinks found message arrived text:" + text);
            Message msg = new Message();
            msg.what = SINKSFOUND;
            msg.obj = (Object)text;
            msgHandler.sendMessage(msg);
        }

        public void onConnectDone(boolean value) {
            if (DBG) Log.d(LOGTAG, "Connect done message arrived");
            if (mCancelAlertDialog != null) {
                mCancelAlertDialog.cancel();
            }
            if (value == true) {
                if (DBG) Log.d(LOGTAG, "Connection succeeded");
                msgHandler.sendEmptyMessage(CONNECTDONE);
            } else {
                if (DBG) Log.d(LOGTAG, "Connection failed!!!");
                mRefreshON = false;
                msgHandler.sendEmptyMessage(RESETSINKS);
            }
        }

        public void onDisconnectDone(boolean value) {
            if (DBG) Log.d(LOGTAG, "Disconnect call back arrived");
            msgHandler.sendEmptyMessage(DISCONNECT);
        }

        public void onDiscovery(int value) {
            if (DBG) Log.d(LOGTAG, "Discovery status call back arrived value:" + value);
            mRefreshON = false;
            if (value == 0) {
                msgHandler.sendEmptyMessage(RESETSINKS);
            }
        }

        public void onNotifyError(String text) {
            if (DBG) Log.d(LOGTAG, "notifyError arrived text+" + text);
            Message msg = new Message();
            msg.what = HANDLEERROR;
            msg.obj = (Object)text;
            msgHandler.sendMessage(msg);
        }

        public void onCancelConnect(int status) {
            if (DBG) Log.d(LOGTAG,"onCancelConnect "+ status);
            try {
                if (status != -1) {
                    mServiceMonitor.discoverSinks();
                }
            } catch (Exception ex) {
                Log.e(LOGTAG, "Exception in onCancelConnect"+ ex.getMessage());
            }
        }

        public void onSetupComplete(int status) {
            if (DBG) Log.d(LOGTAG,"XXX onSetupComplete "+ status + " mServiceMonitor: " + mServiceMonitor);
            try {
                if (status == 0 && mServiceMonitor != null) {
                    mIsSetupComplete = true;
                    mServiceMonitor.discoverSinks();
                    msgHandler.post(new Runnable() {
                        public void run() {
                            try {
                                doStartActivity();
                            } catch (Exception ex) {
                                ex.printStackTrace();
                            }
                        }
                    });
                }
            } catch (Exception ex) {
                Log.e(LOGTAG, "Exception in onSetupComplete"+ ex.getMessage());
            }
        }
    };

    private Handler msgHandler = new Handler() {

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case SINKSFOUND: {
                if (DBG) Log.d(LOGTAG, "handleMessage - Miracast SINKSFOUND+");
                try {
                    if (mServiceMonitor.getConnectedSinkId() != null && !mServiceMonitor.isConnectionOngoing()) {
                        if (DBG) Log.d(LOGTAG, "Already connected, Refreshing sinks");
                        doStartActivity();
                    } else {
                        List<String> availableSinks = (ArrayList)msg.obj;
                        mLayout.removeAllViews();
                        mSinkButtonList.clear();
                        if (bConnectToOther && mServiceMonitor.getSinkAvailabilityStatus(mSinkSSID)) {
                            mConnectionTs = System.currentTimeMillis();
                            if (DBG) Log.d(LOGTAG, "Connecting to sink in sinksfound list: " + mSinkSSID);
                            bConnectToOther = false;
                            mServiceMonitor.connect(mSinkSSID, connectionModeStr);
                            msgHandler.sendEmptyMessage(CANCELCONNECTION);
                        }
                        if (DBG) Log.d(LOGTAG, "Not connecting to other sinks which are found");
                        for (int i = availableSinks.size() - 1; i >= 0; i--) {
                            String text = availableSinks.get(i);
                            if (mServiceMonitor.getSinkAvailabilityStatus(text) != true) {
                                continue;
                            }
                            String connectedSinkId = mServiceMonitor.getConnectedSinkId();
                            Button b = new Button(NvwfdTestActivity.this);
                            b.setText(text);
                            b.setOnClickListener(mOnConnectClicked);
                            mLayout.addView(b);
                            mSinkButtonList.add(b);
                        }
                    }
                } catch (Exception ex) {
                    ex.printStackTrace();
                    Log.e(LOGTAG, "Exception in handleMessage - Miracast SINKSFOUND-");
                }
                break;
            }
            case CONNECTDONE: {
                if (DBG) Log.d(LOGTAG, "QoS:Time for connection is: "+
                    (System.currentTimeMillis() -mConnectionTs) + " millisecs");
                finish();
                break;
            }
            case RESETSINKS: {
                if (DBG) Log.d(LOGTAG, "Refresh:: Reset SinkButtonList and call doStartActivity");
                try {
                    doStartActivity();
                } catch (Exception ex) {
                    ex.printStackTrace();
                    Log.e(LOGTAG, "Exception in handleMessage - RESETSINKS");
                }
                break;
            }
            case DISCONNECT: {
                if (DBG) Log.d(LOGTAG, "QoS:Time for disconnection is: "+
                    (System.currentTimeMillis()-mDisconnectionTs) + " millisecs");
                if (DBG) Log.d(LOGTAG, "Disconnect: Reset SinkButtonList and call doStartActivity");
                // Clear sinks and start over
                if (!bConnectToOther) {
                    try {
                        doStartActivity();
                    } catch (Exception ex) {
                        ex.printStackTrace();
                        Log.e(LOGTAG, "Exception in handleMessage - DISCONNECT");
                    }
                } else {
                    try {
                        if (DBG) Log.d(LOGTAG, "Connecting to other(DISCONNECT):discover sinks");
                        mServiceMonitor.discoverSinks();
                    } catch (Exception ex) {
                        ex.printStackTrace();
                        Log.e(LOGTAG, "Exception in push mode connect"+ ex.getMessage());
                    }
                }
                break;
            }
            case HANDLEERROR: {
                String err = (String) msg.obj;
                if (DBG) Log.d(LOGTAG, "Activity to handle the error = " + err);
                if (err.equalsIgnoreCase("ServiceDied")) {
                    //remove all the views and perform sink discovery again
                    mLayout.removeAllViews();
                    if (mServiceMonitor != null) {
                        try {
                            mServiceMonitor.disconnect();
                        }catch(Exception e) {
                            e.printStackTrace();
                        }
                    } else {
                        if (DBG) Log.d(LOGTAG, " mNvwfdService is NULL");
                    }
                } else if (err.equalsIgnoreCase("ServiceRestarted")) {
                    mRefreshON = false;
                    RefreshSinkList();
                } else {
                    finish();
                }
                break;
            }
            case CANCELCONNECTION: {
                if (DBG) Log.d(LOGTAG, "Dialog box for cancelling.....");
                NvwfdTestActivity.this.cancelConnection();
                break;
            }
            }
        }
    };

    @Override
    public void onDestroy() {
        if (DBG) Log.d(LOGTAG, "Unbind and destroy");
        super.onDestroy();
    }

    private void doStartActivity() throws RemoteException {
        if (DBG) Log.d(LOGTAG, "doStartActivity Called");
        if (mIsSetupComplete) {
            mLayout.removeAllViews();
            boolean mWifiInit = false;
            if (mServiceMonitor != null) {
                try {
                    mWifiInit = mServiceMonitor.isInitialized();
                } catch (Exception ex) {
                    ex.printStackTrace();
                    Log.e(LOGTAG, "Exception in doStartActivity called mWifiInit =" + mWifiInit);
                }
            }
            if (DBG) Log.d(LOGTAG, "doStartActivity called mWifiInit =" + mWifiInit);

            if (mWifiInit == true) {
                WifiManager wifi = (WifiManager)getSystemService(Context.WIFI_SERVICE);
                if(wifi.isWifiEnabled() == false) {
                    fatalErrorDialog("Wifi not enabled, please use Android Settings to enable.");
                } else if (mServiceMonitor.isConnected() && !mServiceMonitor.isConnectionOngoing()) {
                    if (DBG) Log.d(LOGTAG, "Sink already connected- hence display connection window");

                    List<String> mSinks;
                    mSinks = mServiceMonitor.getSinkList();
                    mSinkButtonList.clear();
                    if (DBG) Log.d(LOGTAG, "sinks list size: " + mSinks.size() + " " + mSinks);
                    for (final String sink : mSinks) {
                        Button b = new Button(NvwfdTestActivity.this);
                        b.setText(sink);
                        b.setOnClickListener(mOnConnectClicked);
                        mSinkButtonList.add(b);
                    }

                    if (DBG) Log.d(LOGTAG, "mSinkButtonList: " + mSinkButtonList);
                    String text = mServiceMonitor.getConnectedSinkId();
                    text = mServiceMonitor.getSinkSSID(text);
                    if (DBG) Log.d(LOGTAG, "SinkConnectedText: " + text);
                    for (final Button SinkButton : mSinkButtonList) {
                        if (text != null && text.equalsIgnoreCase((String)SinkButton.getText())) {
                            if (DBG) Log.d(LOGTAG, "SinkClicked Found");
                            sinkClicked = SinkButton;
                        }
                    }
                    if (DBG) Log.d(LOGTAG, "update connectd state view");
                    if (mSinkButtonList.size() > 0) {
                        viewAfterConnect();
                    }
                } else {
                    RefreshSinkList();
                }
            } else {
                fatalErrorDialog("WFD Protocol init failed!");
            }
        }
    }

    private View.OnClickListener mOnConnectClicked = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            for (final Button SinkButton : mSinkButtonList) {
                if (v == SinkButton) {
                    sinkClicked = SinkButton;
                    // Select Sink
                    int index = mSinkButtonList.indexOf(SinkButton);
                    mSinkSSID = sinkClicked.getText().toString();
                    try {
                        if (mServiceMonitor.getConnectedSinkId() != null && !mServiceMonitor.isConnectionOngoing()) {
                            if (DBG) Log.d(LOGTAG, "Connecting to other(sink clicked)");
                            bConnectToOther = true;
                            mDisconnectionTs = System.currentTimeMillis();
                            NvwfdTestActivity.this.autheticationModeDialog();
                        } else {
                            if (DBG) Log.d(LOGTAG, "Connecting..........");
                            // Get Authetication mode and PIN if needed
                            NvwfdTestActivity.this.autheticationModeDialog();
                        }
                    } catch (Exception ex) {}
                }
            }
        }
    };

    private void viewAfterConnect() throws RemoteException {
        if (DBG) Log.d(LOGTAG, "+viewAfterConnectfunction");

        if (mServiceMonitor.isConnected() && !mServiceMonitor.isConnectionOngoing()) {
            mLayout.removeAllViews();
            mScrollView.removeView(mLayout);

            LinearLayout ll1 = new LinearLayout(this);
            ll1.setOrientation(LinearLayout.HORIZONTAL);

            LinearLayout ll2 = new LinearLayout(this);
            ll2.setOrientation(LinearLayout.HORIZONTAL);

            LinearLayout ll3 = new LinearLayout(this);
            ll3.setOrientation(LinearLayout.VERTICAL);

            sinkClicked.setBackgroundColor(Color.CYAN);
            sinkClicked.setEnabled(false);
            mSinkButtonList.remove(mSinkButtonList.indexOf(sinkClicked));
            ll2.addView(sinkClicked);
            ll2.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT,1.0f));


            // Disconnect
            mDisconnectButton = new Button(this);
            mDisconnectButton.setText("Disconnect");
            mDisconnectButton.setOnClickListener(mOnDisconnectClicked);
            ll3.addView(mDisconnectButton);

            // Pause
            mPauseButton = new Button(this);
            mPauseButton.setText("Pause");
            mPauseButton.setOnClickListener(mOnPauseClicked);
            ll3.addView(mPauseButton);

            // Resume
            mResumeButton = new Button(this);
            mResumeButton.setText("Resume");
            mResumeButton.setOnClickListener(mOnResumeClicked);
            ll3.addView(mResumeButton);

            // Get connection formats
            mGetFormatsButton = new Button(this);
            mGetFormatsButton.setText("Get connection formats");
            mGetFormatsButton.setOnClickListener(mOnGetFormatsClicked);
            ll3.addView(mGetFormatsButton);

            // Get Active connection format
            mActiveFormatButton = new Button(this);
            mActiveFormatButton.setText("Get active connection format");
            mActiveFormatButton.setOnClickListener(mOnGetActiveFormatClicked);
            ll3.addView(mActiveFormatButton);

            // 480p connection
            mRadioGroup = new RadioGroup(this);
            mRadioGroup.setOnClickListener(this);
            m480p60Button = new RadioButton(this);
            m480p60Button.setText("480p60");
            m480p60Button.setId(CONNECTION_480P_60FPS);
            m480p60Button.setOnClickListener(new RadioButton.OnClickListener() {
                public void onClick(View v) {
                    if (bForceResolution) {
                        try {
                            if (true == mServiceMonitor.updateConnectionParameters(CONNECTION_480P_60FPS)) {
                                if (DBG) Log.d(LOGTAG, "480p60 connection succeeded");
                                mRadioGroup.check(CONNECTION_480P_60FPS);
                                mResolutionCheckId = CONNECTION_480P_60FPS;
                            } else {
                                if (DBG) Log.d(LOGTAG, "480p60 resolution is not supported");
                                mRadioGroup.check(mResolutionCheckId);
                            }
                        } catch (Exception ex) {
                            ex.printStackTrace();
                            Log.e(LOGTAG, "Exception in Setting 480p60 resolution.........."+ ex.getMessage());
                        }
                    } else {
                        mRadioGroup.clearCheck();
                    }
                }
            });

            mRadioGroup.addView(m480p60Button);

            m540p30Button = new RadioButton(this);
            m540p30Button.setText("540p30");
            m540p30Button.setId(CONNECTION_540P_30FPS);
            m540p30Button.setOnClickListener(new RadioButton.OnClickListener() {
                public void onClick(View v) {
                    if (bForceResolution) {
                        try {
                            if (true == mServiceMonitor.updateConnectionParameters(CONNECTION_540P_30FPS)) {
                                if (DBG) Log.d(LOGTAG, "540p30 connection succeeded");
                                mRadioGroup.check(CONNECTION_540P_30FPS);
                                mResolutionCheckId = CONNECTION_540P_30FPS;
                            } else {
                                if (DBG) Log.d(LOGTAG, "540p30 resolution is not supported");
                                mRadioGroup.check(mResolutionCheckId);
                            }
                        } catch (Exception ex) {
                            ex.printStackTrace();
                            Log.e(LOGTAG, "Exception in Setting 540p30 resolution.........."+ ex.getMessage());
                        }
                    } else {
                        mRadioGroup.clearCheck();
                    }
                }
            });
            mRadioGroup.addView(m540p30Button);

            m540p60Button = new RadioButton(this);
            m540p60Button.setText("540p60");
            m540p60Button.setId(CONNECTION_540P_60FPS);
            m540p60Button.setOnClickListener(new RadioButton.OnClickListener() {
                public void onClick(View v) {
                    if (bForceResolution) {
                        try {
                            if (true == mServiceMonitor.updateConnectionParameters(CONNECTION_540P_60FPS)) {
                                if (DBG) Log.d(LOGTAG, "540p60 connection succeeded");
                                mRadioGroup.check(CONNECTION_540P_60FPS);
                                mResolutionCheckId = CONNECTION_540P_60FPS;
                            } else {
                                if (DBG) Log.d(LOGTAG, "540p60 resolution is not supported");
                                mRadioGroup.check(mResolutionCheckId);
                            }
                        } catch (Exception ex) {
                            ex.printStackTrace();
                            Log.e(LOGTAG, "Exception in Setting 540p60 resolution.........."+ ex.getMessage());
                        }
                    } else {
                        mRadioGroup.clearCheck();
                    }
                }
            });
            mRadioGroup.addView(m540p60Button);

            // 720p30 connection
            m720p30Button = new RadioButton(this);
            m720p30Button.setText("720p30");
            m720p30Button.setId(CONNECTION_720P_30FPS);
            m720p30Button.setOnClickListener(new RadioButton.OnClickListener() {
                public void onClick(View v) {
                    if (bForceResolution) {
                        try {
                            if (true == mServiceMonitor.updateConnectionParameters(CONNECTION_720P_30FPS)) {
                                if (DBG) Log.d(LOGTAG, "720p30 connection succeeded");
                                mRadioGroup.check(CONNECTION_720P_30FPS);
                                mResolutionCheckId = CONNECTION_720P_30FPS;
                            } else {
                                if (DBG) Log.d(LOGTAG, "720p30 resolution is not supported");
                                mRadioGroup.check(mResolutionCheckId);
                            }
                        } catch (Exception ex) {
                            ex.printStackTrace();
                            Log.e(LOGTAG, "Exception in Setting 720p30 resolution.........."+ ex.getMessage());
                        }
                    } else {
                        mRadioGroup.clearCheck();
                    }
                }
            });
            mRadioGroup.addView(m720p30Button);

            String gamemodeProp;
            gamemodeProp = SystemProperties.get("nvwfd.gamemode");
            if (gamemodeProp != null && gamemodeProp .length() > 0) {
                int gamemode = Integer.parseInt(gamemodeProp);
                if (DBG) Log.d(LOGTAG, "*********** game mode option: " + gamemode);
                if (gamemode == 1) {
                    // 720p60 connection
                    m720p60Button = new RadioButton(this);
                    m720p60Button.setText("720p60");
                    m720p60Button.setId(CONNECTION_720P_60FPS);
                    m720p60Button.setOnClickListener(new RadioButton.OnClickListener() {
                        public void onClick(View v) {
                            if (bForceResolution) {
                                try {
                                    if (true == mServiceMonitor.updateConnectionParameters(CONNECTION_720P_60FPS)) {
                                        if (DBG) Log.d(LOGTAG, "720p60 connection succeeded");
                                        mRadioGroup.check(CONNECTION_720P_60FPS);
                                        mResolutionCheckId = CONNECTION_720P_60FPS;
                                    } else {
                                        if (DBG) Log.d(LOGTAG, "720p60 resolution is not supported");
                                        mRadioGroup.check(mResolutionCheckId);
                                    }
                                } catch (Exception ex) {
                                    ex.printStackTrace();
                                    Log.e(LOGTAG, "Exception in Setting 720p60 resolution.........."+ ex.getMessage());
                                }
                            } else {
                                mRadioGroup.clearCheck();
                            }
                        }
                    });
                    mRadioGroup.addView(m720p60Button);
                }
            }

            String maxResolutionSupportedProp;
            maxResolutionSupportedProp = SystemProperties.get("nvwfd.maxresolution_macroblocks");
            if (maxResolutionSupportedProp != null && maxResolutionSupportedProp .length() > 0) {
                int maxMacroBlocksSupported = Integer.parseInt(maxResolutionSupportedProp);
                if (DBG) Log.d(LOGTAG, "*********** max macro blocks supported: " + maxMacroBlocksSupported);
                if (maxMacroBlocksSupported >= (((1920 + 15) / 16) * ((1080 + 15) / 16))) {
                    // 1080p connection
                    m1080p30Button = new RadioButton(this);
                    m1080p30Button.setText("1080p30");
                    m1080p30Button.setId(CONNECTION_1080P_30FPS);
                    m1080p30Button.setOnClickListener(new RadioButton.OnClickListener() {
                        public void onClick(View v) {
                            if (bForceResolution) {
                                try {
                                    if (true == mServiceMonitor.updateConnectionParameters(CONNECTION_1080P_30FPS)) {
                                        if (DBG) Log.d(LOGTAG, "1080p30 connection succeeded");
                                        mRadioGroup.check(CONNECTION_1080P_30FPS);
                                        mResolutionCheckId = CONNECTION_1080P_30FPS;
                                    } else {
                                        if (DBG) Log.d(LOGTAG, "1080p30 resolution is not supported");
                                        mRadioGroup.check(mResolutionCheckId);
                                    }
                                } catch (Exception ex) {
                                    ex.printStackTrace();
                                    Log.e(LOGTAG, "Exception in Setting 1080p30 resolution.........."+ ex.getMessage());
                                }
                            } else {
                                mRadioGroup.clearCheck();
                            }
                        }
                    });
                    mRadioGroup.addView(m1080p30Button);
                    }
                }

            try {
                mResolutionCheckId = mServiceMonitor.getResolutionCheckId();
            } catch (Exception ex) {
                ex.printStackTrace();
                Log.e(LOGTAG, "Exception in getting the resolution check id.........."+ ex.getMessage());
            }
            try {
                bForceResolution = mServiceMonitor.getForceResolutionValue();
            } catch (Exception ex) {
                ex.printStackTrace();
                Log.e(LOGTAG, "Exception in getting the force resolution value.........."+ ex.getMessage());
            }
            if (bForceResolution) {
                mRadioGroup.check(mResolutionCheckId);
            }

            mForceResolution = new CheckBox(this);
            mForceResolution.setText("Force Resolution");
            mForceResolution.setChecked(bForceResolution);
            mForceResolution.setOnClickListener(new CheckBox.OnClickListener() {
                public void onClick(View v) {
                    if (bForceResolution) {
                        mRadioGroup.clearCheck();
                        mForceResolution.setChecked(false);
                        mRadioGroup.setEnabled(false);
                        for (int i=0;i<mRadioGroup.getChildCount();i++) {
                            ((RadioButton)mRadioGroup.getChildAt(i)).setEnabled(false);
                        }
                        try {
                            mServiceMonitor.forceResolution(false);
                        } catch (Exception ex) {
                            ex.printStackTrace();
                            Log.e(LOGTAG, "Exception in setting the force resolution to false.........."+ ex.getMessage());
                        }
                        bForceResolution = false;
                    } else {
                        mForceResolution.setChecked(true);
                        for (int i = 0; i < mRadioGroup.getChildCount(); i++) {
                            ((RadioButton)mRadioGroup.getChildAt(i)).setEnabled(true);
                        }
                        try {
                            mServiceMonitor.forceResolution(true);
                        } catch (Exception ex) {
                            ex.printStackTrace();
                            Log.e(LOGTAG, "Exception in setting the force resolution to true.........."+ ex.getMessage());
                        }
                        bForceResolution = true;
                        mRadioGroup.check(mResolutionCheckId);
                    }
                }
            });
            ll3.addView(mForceResolution);
            ll3.addView(mRadioGroup);
            ll1.addView(ll2);
            ll1.addView(ll3);

            if (!mForceResolution.isChecked()) {
                for (int i = 0; i < mRadioGroup.getChildCount(); i++) {
                    ((RadioButton)mRadioGroup.getChildAt(i)).setEnabled(false);
                }
            }
            LinearLayout ll4 = new LinearLayout(this);
            ll4.setOrientation(LinearLayout.HORIZONTAL);
            TextView tv_quality = new TextView(this);
            tv_quality.setText("High\nQuality");
            tv_quality.setTextColor(Color.CYAN);
            tv_quality.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
            TextView tv_latency = new TextView(this);
            tv_latency.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
            tv_latency.setText("Low\nLatency");
            tv_latency.setTextColor(Color.CYAN);
            // Policy seek bar
            mPolicySeekBar = new SeekBar(this);
            mPolicySeekBar.setMax(100);
            mPolicySeekBar.setLayoutParams(new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT,1.0f));
            try {
                mPolicySeekBar.setProgress(mServiceMonitor.getPolicySeekBarValue());
            } catch (Exception ex) {
                ex.printStackTrace();
                Log.e(LOGTAG, "Exception in setting the seek bar progress.........."+ ex.getMessage());
            }
            mPolicySeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onStopTrackingTouch(SeekBar arg0) {
                }

                @Override
                public void onStartTrackingTouch(SeekBar arg0) {
                }

                @Override
                public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    try {
                        mServiceMonitor.updatePolicy(progress);
                    } catch (Exception ex) {
                        ex.printStackTrace();
                        Log.e(LOGTAG, "Exception in updating the policy seek bar progress.........."+ ex.getMessage());
                    }
                }
            });
            ll4.addView(tv_latency);
            ll4.addView(mPolicySeekBar);
            ll4.addView(tv_quality);
            ll1.setLayoutParams(
                new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT,1.0f));
            mLayout.addView(ll1,0);
            mLayout.addView(ll4);
            for (final Button SinkButton : mSinkButtonList) {
                if (DBG) Log.d(LOGTAG, "SinkButton" + SinkButton.getText());
                if(sinkClicked != SinkButton) {
                    SinkButton.setLayoutParams(new LinearLayout.LayoutParams(300, LayoutParams.WRAP_CONTENT));
                    mLayout.addView(SinkButton);
                }
            }
            mScrollView.addView(mLayout);
        }
        if (DBG) Log.d(LOGTAG, "-viewAfterConnectfunction");
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.refresh, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle item selection
        switch (item.getItemId()) {
            case R.id.refresh:
                RefreshSinkList();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    private void RefreshSinkList() {
        if (mRefreshON == false) {
            mRefreshON = true;
            if (mServiceMonitor != null && mIsSetupComplete) {
                try {
                    // qos discover time start
                    mDiscoveryTs = System.currentTimeMillis();
                    if (DBG) Log.d(LOGTAG, "DiscoverSinks");
                    mServiceMonitor.discoverSinks();
                } catch (Exception ex) {
                    ex.printStackTrace();
                    Log.e(LOGTAG, "Exception in RefreshSinkList" + ex.getMessage());
                }
            } else {
                if (DBG) Log.d(LOGTAG, "NULL mServiceMonitor");
            }
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
            mSinkFormatsText = new TextView(NvwfdTestActivity.this);
            try {
                List<String> audiofmts = mServiceMonitor.getSupportedAudioFormats();
                List<String> videofmts = mServiceMonitor.getSupportedVideoFormats();

                for (final String videofmt : videofmts) {
                    mSinkFormatsText.setText(mSinkFormatsText.getText() + " | " + videofmt);
                }
                mSinkFormatsText.setText(mSinkFormatsText.getText() + "\n VideoFormatCount =" + videofmts.size() + "\n");

                for (final String audiofmt : audiofmts) {
                    mSinkFormatsText.setText(mSinkFormatsText.getText() + " | " + audiofmt);
                }
                mSinkFormatsText.setText(mSinkFormatsText.getText() + "\n AudioFormatCount =" + audiofmts.size() + "\n");
            } catch (Exception ex) {
                ex.printStackTrace();
                Log.e(LOGTAG, "Exception in getting supported formats");
            }
            mLayout.addView(mSinkFormatsText);
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
            mActiveFormatText = new TextView(NvwfdTestActivity.this);
            try {
                String audiofmt = mServiceMonitor.getActiveAudioFormat();
                String videofmt = mServiceMonitor.getActiveVideoFormat();

                mActiveFormatText.setText(mActiveFormatText.getText() + " | " + videofmt);
                mActiveFormatText.setText(mActiveFormatText.getText() + " | " + audiofmt);
            } catch (Exception ex) {
                ex.printStackTrace();
                Log.e(LOGTAG, "Exception in gettig active format");
            }
            mLayout.addView(mActiveFormatText);
        }
    };

    private View.OnClickListener mOnDisconnectClicked = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            try {
                mDisconnectionTs = System.currentTimeMillis();
                mServiceMonitor.disconnect();
            } catch (Exception e) {
            }
        }
    };

    private View.OnClickListener mOnPauseClicked = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            try {
                if (mServiceMonitor != null) {
                    mServiceMonitor.pause();
                }
            } catch (Exception e) {
            }
        }
    };

    private View.OnClickListener mOnResumeClicked = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            try {
                if (mServiceMonitor != null) {
                    mServiceMonitor.resume();
                }
            } catch (Exception e) {
            }
        }
    };

    private void fatalErrorDialog(String message) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(message);
        builder.setCancelable(false);
        builder.setPositiveButton("Exit", new DialogInterface.OnClickListener() {
           public void onClick(DialogInterface dialog, int id) {
                NvwfdTestActivity.this.finish();
           }});
        AlertDialog alert = builder.create();
        alert.show();
    }

    private void autheticationModeDialog() {
        if (DBG) Log.d(LOGTAG, "autheticationModeDialog ssid: " + mSinkSSID);
        List<String> supportedAuthTypes = null;
        int mode = -1;

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Select Authentication Mode");

        boolean pbcSupport = false;
        boolean pinSupport = false;
        try {
            pbcSupport = mServiceMonitor.getPbcModeSupport(mSinkSSID);
            pinSupport = mServiceMonitor.getPinModeSupport(mSinkSSID);
        } catch (Exception ex) {
            ex.printStackTrace();
            Log.e(LOGTAG, "Exception while querying authentification mode "+ ex.getMessage());
            return;
        }

        if (pbcSupport && pinSupport) {
            if (DBG) Log.d(LOGTAG, "Sink = " + mSinkSSID + "   supports PBC and PIN mode ");
            CharSequence[] itemsPinPbc = {"PBC", "PIN"};
            mode = 2; // sink supports pbc as well as pin mode
            AuthModeDlgListener pinPbcListener = new AuthModeDlgListener(mode);
            builder.setItems(itemsPinPbc, pinPbcListener);
        } else if (pbcSupport) {
            if (DBG) Log.d(LOGTAG, "Sink = " + mSinkSSID + "   supports PBC mode only");
            CharSequence[] itemsPBC = {"PBC"};
            mode = 0; // sink supports pbc mode only
            AuthModeDlgListener pinPbcListener = new AuthModeDlgListener(mode);
            builder.setItems(itemsPBC, pinPbcListener);
        } else if (pinSupport) {
            if (DBG) Log.d(LOGTAG, "Sink = " + mSinkSSID + "   supports PIN mode only");
            CharSequence[] itemsPIN = {"PIN"};
            mode = 1; // sink supports pin mode only
            AuthModeDlgListener pinPbcListener = new AuthModeDlgListener(mode);
            builder.setItems(itemsPIN, pinPbcListener);
        } else {
            if (DBG) Log.d(LOGTAG, "failed to get authentification mode !!!");
        }

        AlertDialog alert = builder.create();
        alert.show();
    }

    private class AuthModeDlgListener implements DialogInterface.OnClickListener {
        private int mMode;
        public AuthModeDlgListener(int mode) {
            mMode = mode;
        }
        public void onClick(DialogInterface dialog, int item) {
            if (DBG) Log.d(LOGTAG, "Mode selected is " + item);
            List<String> mSinks = new ArrayList<String>();
            try {
                mSinks = mServiceMonitor.getSinkList();
            } catch (Exception ex) {
                ex.printStackTrace();
                Log.e(LOGTAG, "Exception in getting the sink list from service.........."+ ex.getMessage());
            }

            if (item == 0) {
                if (mMode == 0 || mMode == 2) {
                    connectToSink(true);
                    connectionModeStr = null;
                } else if (mMode == 1) {
                    connectToSink(false);
                    connectionModeStr = "true";
                }
            } else if (item == 1) {
                if (mMode == 2) {
                    connectToSink(false);
                    connectionModeStr = "true";
                }
            }
            dialog.cancel();
        }
    }

    private void connectToSink(boolean isPbc) {
        try {
            if (bConnectToOther) {
                mServiceMonitor.disconnect();
            } else {
                mConnectionTs = System.currentTimeMillis();
                if (isPbc) {
                    mServiceMonitor.connect(mSinkSSID, null);
                } else {
                    mServiceMonitor.connect(mSinkSSID, "true");
                }
                msgHandler.sendEmptyMessage(CANCELCONNECTION);
            }
        } catch (Exception ex) {
            ex.printStackTrace();
            Log.e(LOGTAG, "Exception in push mode connect"+ ex.getMessage());
        }
    }

    private void cancelConnection() {
        final CharSequence[] items1 = {"CANCEL"};
        AlertDialog.Builder cancel_builder = new AlertDialog.Builder(this);
        cancel_builder.setTitle("Connecting....");
        cancel_builder.setItems(items1, new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int item) {
                if (DBG) Log.d(LOGTAG, "Cancel Connect button pressed");
                if (item == 0) {
                    try {
                        mLayout.removeAllViews();
                        mServiceMonitor.cancelConnect();
                    } catch (Exception e) {
                        e.printStackTrace();
                        Log.e(LOGTAG, "Exception in cancel connection"+ e.getMessage());
                    }
                }
                dialog.cancel();
            }
        });
        mCancelAlertDialog= cancel_builder.create();
        mCancelAlertDialog.show();
    }

    public void onClick(View arg0) {
        try {
            return;
        } catch (Exception ex) {}
    }
}

